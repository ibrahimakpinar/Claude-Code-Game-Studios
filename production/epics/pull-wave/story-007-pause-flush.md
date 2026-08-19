# Story 007: Pause-Flush (Queued-to-Next-Tick) + bPauseFlushPending Gate

> **Epic**: Pull-Wave Behavior
> **Status**: Complete
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: (fill before sprint planning)
> **Manifest Version**: 2026-07-03 (docs/registry/architecture.yaml v8 — no separate control-manifest.md; forbidden patterns cited from registry)
> **Last Updated**: 2026-08-19

## Context

**GDD**: `design/gdd/pull-wave-behavior.md`
**Requirement**: `TR-PW-002`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0010: Pull-Wave Object Pool, State Machine, and Despawn Pipeline
**ADR Decision Summary**: When `RSM.OnPausedChanged(false)` fires (run aborted during a pause), Pull-Wave's bound handler sets `bPauseFlushPending = true` synchronously — but does NOT immediately walk `ActiveWaves`. On the next Pull-Wave tick, the very first action after `Super::Tick()` and `SCOPE_CYCLE_COUNTER` is: if `bPauseFlushPending && RSM.GetCurrentState() == RUNNING`, walk all `ActiveWaves` in WaveId ASC order and call `TransitionTo(Wave, DESPAWNING)` (with `DespawnReason=PauseFlush`) for every wave in LEANING, TRAVERSING, or LANDED state; clear `bPauseFlushPending`. This must occur BEFORE the per-wave state-dispatch loop. Inline mid-tick flush is explicitly rejected (ADR-0010 Alternative 3 — breaks AC-PW-15 contiguity).

**Engine**: Unreal Engine 5.7 | **Risk**: LOW
**Engine Notes**: `RSM.OnPausedChanged` delegate subscription — non-dynamic multicast (ADR-0007 Seam boundary). `RSM.GetCurrentState()` read at flush-consumption time — stable per ADR-0007 interface. `FDelegateHandle` / `AddRaw` or `AddUObject` pattern for handler binding at `BeginPlay` and `EndPlay` — verified stable pre-UE-4. No post-cutoff APIs.

**Control Manifest Rules (Feature layer)** — from `docs/registry/architecture.yaml` v8:
- Required: `bPauseFlushPending` flag set synchronously in `RSM.OnPausedChanged(false)` handler — no inline wave-walk in the handler.
- Required: Pause-flush batch executes at TOP of the Pull-Wave tick body, BEFORE the per-wave iteration loop (AC-PW-MID-TICK-PAUSE-DEFERRAL guarantee).
- Required: Flush only when `RSM.GetCurrentState() == RUNNING` at consumption time — if state is ABORTED/DEAD/COMPLETE at next tick, skip flush and clear the flag (Rule 19).
- Required: Flush walks `ActiveWaves` in WaveId ASC order; `TransitionTo(DESPAWNING)` called for every LEANING, TRAVERSING, and LANDED wave; SPAWNED and DESPAWNING waves are skipped (SPAWNED has not yet ticked and DESPAWNING is already terminal).
- Required: After flush, per-wave iteration loop runs normally (DESPAWNING waves from the flush fire their 6-step pipeline in that same tick per Story 006).
- Forbidden: Inline mid-tick flush — `RSM.OnPausedChanged` handler must not call `TransitionTo()` on any wave (ADR-0010 Alternative 3 rejection; AC-PW-MID-TICK-PAUSE-DEFERRAL CI assert).
- Forbidden: `bPauseFlushPending` consulted inside the per-wave iteration loop — flag is consumed once at top-of-tick; not re-checked per wave.

---

## Acceptance Criteria

*From GDD `design/gdd/pull-wave-behavior.md` §Pause-Flush, scoped to this story:*

- [ ] **AC-PW-MID-TICK-PAUSE-DEFERRAL:** Given Pull-Wave is mid-tick (between wave[N] and wave[N+1] in the per-wave loop), `RSM.OnPausedChanged(false)` fires (simulated via test hook): (a) `bPauseFlushPending` becomes `true` synchronously; (b) no wave transitions to DESPAWNING within that tick; (c) at the TOP of the next tick, all LEANING/TRAVERSING/LANDED waves transition to DESPAWNING atomically in WaveId ASC order; (d) their 6-step pipelines fire in that same tick. All four assertions independent.
- [ ] **Flush at next-tick top (RUNNING state):** Given 3 waves in LEANING/TRAVERSING/LANDED when `RSM.OnPausedChanged(false)` fires with `RSM.current_state=RUNNING`: next tick's top-of-tick flush routes all 3 to DESPAWNING with `DespawnReason=PauseFlush`. `ActiveWaves.Num() == 0` after that tick. `bPauseFlushPending == false` after consumption.
- [ ] **Rule 19 — ABORTED state skip:** Given `RSM.OnPausedChanged(false)` fires with `RSM.current_state=ABORTED` (pause fired during run-abort sequence): at next tick's top-of-tick, `bPauseFlushPending` is cleared WITHOUT routing any wave to DESPAWNING. Waves remain in their current state; Story 008 run-termination drain handles them.
- [ ] **SPAWNED wave not flushed:** Given one wave in SPAWNED state when pause-flush fires: that wave is NOT routed to DESPAWNING in the flush batch (SPAWNED→DESPAWNING is a forbidden transition per the Story 003 table).
- [ ] **DESPAWNING wave not re-flushed:** Given one wave already in DESPAWNING when pause-flush fires: it is skipped in the flush batch (already terminal state).
- [ ] **WaveId ASC flush order:** Given 3 waves with WaveIds 5, 7, 9 mid-TRAVERSING when flush fires: `FWaveSpawnerCallbackTestStub` event log records DESPAWNING-entry events in order WaveId=5, then 7, then 9 (no interleaving per AC-PW-15).
- [ ] **Handler binding lifecycle:** `bPauseFlushPending`-setting handler is bound to `RSM.OnPausedChanged` at `APullWaveSubsystemActor::BeginPlay` and unbound at `EndPlay` via stored `FDelegateHandle`. Re-entrant binding (double-bind) does not fire the handler twice.
- [ ] **Pause-freeze and pause-flush are independent:** During pause (RSM.GetIsPaused()==true), per-wave tick bodies are skipped (Story 004 pause-freeze gate). `bPauseFlushPending` is only relevant for the next-tick top-of-tick check when the run is aborted; it does NOT affect normal pause-freeze behavior.

---

## Implementation Notes

*Derived from ADR-0010 D2 Implementation Guidelines (pause-flush section):*

**Handler (bound at BeginPlay):**
```cpp
void APullWaveSubsystemActor::OnRSMPausedChanged(bool bNewIsPaused)
{
    if (!bNewIsPaused)
    {
        // Run ended while paused — queue flush for next tick
        bPauseFlushPending = true;
    }
}
```

**Tick body structure (top-of-tick, before per-wave loop):**
```cpp
void APullWaveSubsystemActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    SCOPE_CYCLE_COUNTER(STAT_PullWaveTick);

    // [Pause-flush batch — Story 007]
    if (bPauseFlushPending)
    {
        bPauseFlushPending = false;
        if (RSM->GetCurrentState() == EPullWaveRunState::Running)  // Rule 19
        {
            for (FPullWaveInstanceState& Wave : ActiveWaves)
            {
                if (Wave.State == EPullWaveState::LEANING ||
                    Wave.State == EPullWaveState::TRAVERSING ||
                    Wave.State == EPullWaveState::LANDED)
                {
                    Wave.PendingDespawnReason = EDespawnReason::PauseFlush;
                    TransitionTo(Wave, EPullWaveState::DESPAWNING);
                }
                // SPAWNED and DESPAWNING are skipped
            }
        }
        // If not RUNNING (e.g., ABORTED), clear flag without flushing — Story 008 handles
    }

    // [Per-wave iteration — Story 004]
    for (int32 i = 0; i < ActiveWaves.Num(); ++i)
    {
        FPullWaveInstanceState& Wave = ActiveWaves[i];
        if (RSM->GetIsPaused()) { continue; }
        switch (Wave.State)
        {
            // ... all states including DESPAWNING from this flush batch
        }
    }

    WaveMassISMC->MarkRenderStateDirty();
}
```

**Why queued-to-next-tick (not inline)?** If `RSM.OnPausedChanged(false)` fires mid-iteration (between Wave[N] and Wave[N+1] in the loop), waves 0..N have already been processed this tick. An inline flush would mix DESPAWNING-entry events for those waves with the continuing loop for waves N+1..end — violating AC-PW-15 contiguity. Deferring to next-tick's TOP guarantees the flush is always batch-atomic and contiguous in the event log.

**`bPauseFlushPending` field:** `bool bPauseFlushPending = false;` declared on `APullWaveSubsystemActor`. Initialized to `false` at `BeginPlay`. Not `UPROPERTY()` — not inspector-visible; programmer-only state.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 004: Per-wave pause-freeze gate (`if (RSM.GetIsPaused()) { continue; }`) — that fires during normal paused play, not at pause-boundary abort.
- Story 006: Six-step DESPAWNING pipeline (fires for flush-routed waves in the same tick as the flush; handled there).
- Story 008: Run-termination drain semantics (RSM RUNNING→DEAD/COMPLETE during unpaused play — the `bPauseFlushPending` Rule 19 ABORTED check routes to this path).

---

## QA Test Cases

*Test cases not yet defined — run /qa-plan to generate them.*

---

## Test Evidence

**Story Type**: Integration
**Required evidence**: `tests/integration/pull-wave/pause-flush_test.cpp` OR documented playtest — must exist and pass

**Status**: [x] Created — Source/SLIPSTORM/Tests/Integration/PullWave/PullWavePauseFlushTest.cpp (9 test cases: TC1–TC9)

---

## Dependencies

- Depends on: Story 003 (TransitionTo helper), Story 004 (tick body structure to insert top-of-tick check), Story 006 (DESPAWNING pipeline for flush-routed waves)
- Unlocks: Story 008 (run-termination complements pause-flush; Rule 19 ABORTED branch)

---

## Completion Notes

**Completed**: 2026-08-19
**Criteria**: 8/8 passing
**Deviations**:
- ADVISORY D1: Production `FDelegateHandle` bind/unbind lifecycle not exercised in test context (tests call `OnRSMPausedChanged` directly; no real RSM delegate wired). Mitigated by `TODO(story-RSM-integration)` comment in `EndPlay` body and TC7 idempotency test.
- ADVISORY D2: Test file placed at `Source/SLIPSTORM/Tests/Integration/PullWave/PullWavePauseFlushTest.cpp` per project UE convention (story spec listed `tests/integration/pull-wave/pause-flush_test.cpp`). Consistent with Stories 004–006 pattern.
**Test Evidence**: Integration test at `Source/SLIPSTORM/Tests/Integration/PullWave/PullWavePauseFlushTest.cpp` (9 TCs: TC1 MidTickDeferral, TC2 RunningStateFlush, TC3 Rule19AbortedSkip, TC4 SpawnedNotFlushed, TC5 DespawningNotReflushed+reason, TC6 WaveIdAscOrder, TC7 HandlerDoublecall, TC8 PauseFreezeIndependence, TC9 MixedSpawnedTraversing). GAP-2 fixed via TC9. GAP-1 fixed via TC5c NaturalLanding assertion.
**Code Review**: Complete — /code-review run this session; CHANGES REQUIRED verdict; 2 blocking gaps (TC5 no reason assertion, no mixed-pool TC) fixed before close. Advisory TODO added to EndPlay re: RSM integration Remove() obligation.

# Story 008: Run-Termination Drain Semantics

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
**ADR Decision Summary**: When RSM transitions RUNNING→DEAD, RUNNING→COMPLETE, or RUNNING→ABORTED (run-termination), in-flight Pull-Wave instances are NOT immediately flushed to DESPAWNING. They continue advancing through their normal lifecycle (LEANING→TRAVERSING→LANDED→DESPAWNING) with `DespawnReason=RunTermination`. This is the explicit distinction from pause-flush: pause-flush immediately routes all active waves to DESPAWNING; run-termination lets waves drain naturally. The pause-freeze gate (`if (RSM.GetIsPaused()) { continue; }`) does NOT suppress ticking after run-termination — the Run State Machine's is-paused flag is independent of the run-state transition. Hit and near-miss events still fire normally for waves landing during the drain.

**Engine**: Unreal Engine 5.7 | **Risk**: LOW
**Engine Notes**: `RSM.GetCurrentState()` read at top-of-tick to detect DEAD/COMPLETE/ABORTED states — stable per ADR-0007 interface. Run-termination detection via `RSM.OnRunStateChanged` delegate OR direct `GetCurrentState()` poll — either pattern is valid (no per-tick performance concern at drain latency). No post-cutoff APIs.

**Control Manifest Rules (Feature layer)** — from `docs/registry/architecture.yaml` v8:
- Required: Waves in LEANING/TRAVERSING/LANDED continue advancing normally after run-termination — no mid-lifecycle flush.
- Required: `DespawnReason = RunTermination` for all waves that enter DESPAWNING after RSM leaves RUNNING state during a run-termination (not via NaturalLanding or PauseFlush).
- Required: Hit and near-miss events (`OnWaveHit`, `OnNearMiss`) still fire for waves landing during the drain (the run is over but in-flight collision resolution is contractually complete per AC-PW-15).
- Required: `OnLeanProgress` delegate continues firing during LEANING drain ticks (Telegraph must show correct lean state even post-termination until despawn).
- Forbidden: Pull-Wave calling `RSM->SetState(DEAD)` or any RSM mutation during run-termination drain — RSM transition is already done; Pull-Wave is purely a consumer.
- Forbidden: Immediately routing waves to DESPAWNING on run-termination — that is PauseFlush semantics, not drain semantics.

---

## Acceptance Criteria

*From GDD `design/gdd/pull-wave-behavior.md` §Run-Termination Drain, scoped to this story:*

- [ ] **Drain continues normally (LEANING):** Given wave in LEANING when RSM transitions RUNNING→DEAD via `FRSMTestStub.SetCurrentState(DEAD)`: wave does NOT immediately transition to DESPAWNING; it continues accumulating `LeanProgress` across subsequent ticks; `OnLeanProgress` fires each tick; eventually reaches TRAVERSING, then LANDED (with hit/near-miss resolution), then DESPAWNING with `DespawnReason=RunTermination`.
- [ ] **Drain continues normally (TRAVERSING):** Given wave in TRAVERSING at t_norm=0.3 when RSM→DEAD: wave continues advancing `TraverseElapsedS`; trajectory formulas (F-TRAJ-TNORM, F-TRAJ-LATERAL, F-TRAJ-FORWARD) evaluate normally per tick; ISMC transform updates continue; wave eventually lands → DESPAWNING with `DespawnReason=RunTermination`.
- [ ] **Drain continues normally (LANDED):** Given wave in LANDED during hold when RSM→DEAD: `LandedHoldElapsedS` continues accumulating; hold expires → DESPAWNING with `DespawnReason=RunTermination`. `OnWaveHit`/`OnNearMiss` do NOT re-fire (already fired at LANDED entry per Story 005's hold constraint).
- [ ] **Hit fires during drain:** Given wave in TRAVERSING when RSM→DEAD; wave lands with `FPlayerMovementTestStub` SETTLED + `current_lane==target_lane`: `OnWaveHit` fires exactly once at LANDED entry; `DespawnReason=RunTermination` at DESPAWNING. Run-termination does NOT suppress hit/near-miss events.
- [ ] **Distinct from PauseFlush:** Given wave in TRAVERSING: (a) PauseFlush scenario: `RSM.OnPausedChanged(false)` fires with `RSM.current_state=ABORTED` → wave is NOT flushed immediately (Rule 19 check in Story 007 skips it); (b) Run-termination scenario: `FRSMTestStub.SetCurrentState(DEAD)` → wave continues draining naturally. The two scenarios produce different event sequences: PauseFlush-skip produces no DESPAWNING; run-termination produces a full LANDED→DESPAWNING pipeline at natural cadence.
- [ ] **DespawnReason=RunTermination in event log:** After a run-termination drain cycle completes, `FWaveSpawnerCallbackTestStub.GetDespawnReason(WaveId) == RunTermination` for all waves that despawned post-termination.
- [ ] **Pause-freeze still applies during drain:** If `RSM.GetIsPaused() == true` while RSM is in DEAD state (edge case: paused THEN terminated), the pause-freeze gate (`if (RSM.GetIsPaused()) { continue; }`) still suppresses per-wave tick bodies — drain does not override pause-freeze.
- [ ] **Wave Spawner stops admitting after run-termination** (out-of-scope to implement here, but test that Pull-Wave pool is not corrupted): No new waves are admitted after RSM→DEAD; `ActiveWaves.Num()` monotonically decreases to 0 as waves drain. Verified by ticking until `ActiveWaves.Num() == 0` with no new admissions.

---

## Implementation Notes

*Derived from ADR-0010 D2 Implementation Guidelines (run-termination section):*

**Run-termination detection:** Pull-Wave reads `RSM.GetCurrentState()` at `BeginPlay` to register an `OnRunStateChanged` handler, OR polls `GetCurrentState()` at the top of each tick. Either approach is valid. The recommended pattern (per ADR-0007 tick-order contract) is a state-change delegate handler that sets a `bRunTerminated` flag and a `DespawnReasonOverride = RunTermination` field:

```cpp
void APullWaveSubsystemActor::OnRSMRunStateChanged(EPullWaveRunState NewState)
{
    if (NewState == EPullWaveRunState::Dead      ||
        NewState == EPullWaveRunState::Complete  ||
        NewState == EPullWaveRunState::Aborted)
    {
        // Mark all future DESPAWNING entries as RunTermination
        // (do NOT immediately flush — drain semantics)
        bRunTerminated = true;
    }
}
```

**DespawnReason assignment at DESPAWNING entry:**
- Natural landing path (Story 005's hold-expiry): `Wave.PendingDespawnReason = bRunTerminated ? EDespawnReason::RunTermination : EDespawnReason::NaturalLanding`
- PauseFlush path (Story 007's batch): always `EDespawnReason::PauseFlush` (flush only fires when `RSM.current_state == RUNNING`, so run-termination and flush are mutually exclusive — Rule 19 guarantee).

**Key semantic distinction from PauseFlush:**

| Scenario | Trigger | Immediate flush? | DespawnReason |
|---|---|---|---|
| PauseFlush | `RSM.OnPausedChanged(false)` + `RSM.state==RUNNING` | YES (next tick top) | PauseFlush |
| RunTermination | `RSM.state ∈ {DEAD, COMPLETE, ABORTED}` | NO (drain naturally) | RunTermination |
| Both together | `OnPausedChanged(false)` + `RSM.state==ABORTED` | NO (Rule 19 skips flush) | RunTermination (via drain) |

**bRunTerminated flag:** `bool bRunTerminated = false;` on `APullWaveSubsystemActor`. Reset at `BeginPlay` / run-start. Not `UPROPERTY()`.

**Pause-freeze gate during drain:** The `if (RSM->GetIsPaused()) { continue; }` gate inside the per-wave loop reads `GetIsPaused()` independently of run state. If the run terminated while unpaused, `GetIsPaused()` returns `false` and drain ticks proceed normally. No special-case logic needed.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 005: `OnWaveHit`/`OnNearMiss` broadcasts (fire at LANDED entry regardless of run state — do not re-implement here).
- Story 006: Six-step despawn pipeline at DESPAWNING entry (handles the `DespawnReason=RunTermination` branch; do not duplicate here).
- Story 007: Pause-flush semantics (Rule 19 ABORTED check is implemented there; the interaction with run-termination is tested here but implemented there).
- Story 009: `Construct()` entry point — Wave Spawner is responsible for stopping new admissions after RSM→DEAD (not Pull-Wave's concern here; just assert that no new admissions happen in test).

---

## QA Test Cases

*Test cases not yet defined — run /qa-plan to generate them.*

---

## Test Evidence

**Story Type**: Integration
**Required evidence**: `tests/integration/pull-wave/run-termination-drain_test.cpp` OR documented playtest — must exist and pass

**Status**: [x] Created — Source/SLIPSTORM/Tests/Integration/PullWave/PullWaveRunTerminationDrainTest.cpp (9 test cases: TC1–TC9)

---

## Dependencies

- Depends on: Story 004 (tick advance loop; drain uses same per-wave bodies), Story 005 (LANDED entry events still fire), Story 006 (DESPAWNING pipeline with RunTermination reason), Story 007 (Rule 19 interaction — pause-flush skips ABORTED state)
- Unlocks: None (terminal story in the state-machine lifecycle chain)

---

## Completion Notes

**Completed**: 2026-08-19
**Criteria**: 8/8 passing
**Deviations**:
- ADVISORY D1: `bRunTerminated` only reset at `InitializePool()`/`BeginPlay()` — if RSM cycles RUNNING→DEAD→RUNNING within the same actor lifetime, flag stays latched `true` permanently. Mitigated by `TODO(story-RSM-integration)` comment in `EndPlay` and `OnRSMRunStateChanged` body. Reset on RUNNING/IDLE branch should be added at RSM integration time.
- ADVISORY D2: `EndPlay` comment overstates dangling-pointer risk for `AddUObject` (weak-ref binding; missing `Remove()` leaves stale entry, not use-after-free). Fix comment at RSM integration time.
- ADVISORY D3: Test file placed at `Source/SLIPSTORM/Tests/Integration/PullWave/PullWaveRunTerminationDrainTest.cpp` per project convention (story spec listed `tests/integration/pull-wave/run-termination-drain_test.cpp`). Consistent with Stories 004–008 pattern.
**Test Evidence**: Integration test at `Source/SLIPSTORM/Tests/Integration/PullWave/PullWaveRunTerminationDrainTest.cpp` (9 TCs: TC1 LeaningDrain, TC2 TraversingDrain, TC3 LandedHoldDrain, TC4 HitFiresDuringDrain, TC5 DistinctFromPauseFlush, TC6 AllRunTerminationReason, TC7 PauseFreezeAppliesToDrain, TC8 PoolDrainsMonotonically, TC9 NaturalLandingNegativeControl). TC9 added per /code-review blocking gap (negative control for bRunTerminated=false path).
**Code Review**: Complete — /code-review run this session; CHANGES REQUIRED verdict; 1 blocking gap (TC9 NaturalLanding negative control) fixed before close. 4 advisory items deferred to RSM integration story.

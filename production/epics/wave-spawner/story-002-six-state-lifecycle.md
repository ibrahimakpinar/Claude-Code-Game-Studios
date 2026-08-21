# Story 002: Six-State Lifecycle, Phase Drain, and Atomic Pool Swap

> **Epic**: Wave Spawner Pattern Library
> **Status**: Complete
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: M (~3–4h)
> **Manifest Version**: (none — docs/architecture/control-manifest.md not found; run /create-control-manifest)
> **Last Updated**: 2026-08-19

## Context

**GDD**: `design/gdd/wave-spawner-pattern-library.md`
**Requirement**: `TR-WS-011`, `TR-WS-020`, `TR-WS-021`, `TR-WS-022`, `TR-WS-023`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0011: Wave Spawner Pattern Library — D3 Six-State Lifecycle (primary); ADR-0005: Wave Spawner Subsystem Hosting (secondary — provides class skeleton)
**ADR Decision Summary**: The spawner has a six-state lifecycle (`Cold → Active → Holding ↔ Flushing → Idle`) governed jointly by RSM state events and DPC frame snapshots. `IsTickable()` returns false in Cold and Idle states (no idle CPU cost). Phase pool swap on OPENER→MID and MID→PEAK transitions is pointer-atomic: the active draw pool pointer is reassigned without copying pattern data. The Rule 9 drain window (`max_pattern_length_s`) prevents new pattern starts within the drain window of a phase boundary. In-flight wave `telegraph_window_s` snapshots are immutable post-admission — phase transition does NOT re-parametrize in-flight waves.

**Engine**: Unreal Engine 5.7 | **Risk**: HIGH
**Engine Notes**: `ETickableTickType::Conditional` combined with `IsTickable()` state gating — UE 5.7 game-thread guarantee on mobile must be verified (ADR-0005 Verification Required §3). Atomic pointer swap for pool reference: use `FPlatformAtomics` or a simple boolean flag if pool swap is game-thread-only (verify thread context). `FTickableGameObject::Tick()` does NOT receive a guaranteed game thread on editor targets — ensure `IsTickable()` additionally gates on `GetWorld()->IsGameWorld()` or equivalent.

**Control Manifest Rules (Feature layer)**:
- No control manifest found at `docs/architecture/control-manifest.md` — run `/create-control-manifest`.

---

## Acceptance Criteria

*From GDD `design/gdd/wave-spawner-pattern-library.md`, scoped to this story:*

- [x] **AC-WS-15a (BLOCKING — TR-WS-011/TR-WS-020)**: `EWaveSpawnerLifecycleState` enum declares exactly 6 states (`Cold`, `Active`, `Holding`, `Flushing`, `Idle`). All 9 valid transitions are enforced by `TransitionTo()` per ADR-0011 D3 (authoritative): `Cold→Active`, `Active→Holding`, `Active→Flushing`, `Holding→Flushing`, `Holding→Active`, `Holding→Idle`, `Flushing→Cold`, `Flushing→Active`, `Idle→Active`. Any other transition (e.g. `Cold→Holding`, `Flushing→Idle`, `Idle→Cold`) triggers `check(false)` in non-Shipping builds (after UE_LOG) and returns without state change. `IsTickable()` returns `false` in `Cold` and `Idle` states; returns `true` in `Active`, `Holding`, and `Flushing` states. *(Note: `TestOnly_IsValidTransition()` seam used in tests to verify forbidden pairs without invoking the process-aborting `check(false)`.)*

- [x] **AC-WS-15b (BLOCKING — TR-WS-021)**: Phase pool swap fires atomically on `Active→Holding` entry: `ActiveDrawPool` pointer is reassigned from `&OpenerPool` to `&MidPool` (on first `Holding` entry), then from `&MidPool` to `&PeakPool` (on second `Holding` entry). Pattern data is NEVER copied — only the raw pointer changes. In-flight wave `telegraph_window_s` snapshots are NOT re-parametrized by the swap — no `FWaveInFlightState` fields are mutated during pointer reassignment (TR-WS-022). `GetTickableGameObjectWorld()` returns `GetGameInstance()->GetWorld()` (VR-7: associates tick with the correct game world per UE 5.7 tick-world binding).

- [x] **AC-WS-15c (BLOCKING — TR-WS-022/TR-WS-023)**: Rule 9 drain window: on `Active→Holding`, `bDrainWindowActive` is set `true`; when `CountInFlightWaves() == 0`, `TransitionTo(Active)` fires automatically, resuming admissions under the next pool pointer. `Cold→Active` initialises `ActiveDrawPool = &OpenerPool` and `ActivePhase = ERunPhase::Opener`. `Flushing→Cold` (run-termination path) resets `ActiveDrawPool = nullptr`, `ActivePhase = ERunPhase::Opener`, and `bDrainWindowActive = false` for the next run. `Holding→Idle` (long-pause path) clears `bDrainWindowActive`; `Idle→Active` resumes admissions without a Cold reset.

---

## Implementation Notes

*Derived from ADR-0011 D3 Implementation Guidelines:*

**State enum and transition table:**
```cpp
UENUM()
enum class EWaveSpawnerLifecycleState : uint8
{
    Cold,       // Pre-run: pool allocated, not ticking
    Active,     // Run live: OPENER or MID phase admitting patterns
    Holding,    // Phase drain window: no new admissions; in-flight waves completing
    Flushing,   // Pause flush in progress: draining in-flight in wave_id ASC
    Idle        // Post-run or post-flush: all waves despawned, not ticking
};
```

**State machine — valid transition pairs per ADR-0011 D3 (9 valid transitions):**
```
Cold      → Active     (RSM RUNNING event + RunSeed capture)
Active    → Holding    (OPENER→MID or MID→PEAK drain window entry)
Active    → Flushing   (RSM OnPausedChanged(true) while not in Holding)
Holding   → Flushing   (RSM OnPausedChanged(true) while draining)
Holding   → Active     (drain window clears: all in-flight complete)
Holding   → Idle       (long-pause path: drain completes while paused)
Flushing  → Cold       (run-termination path: all in-flight waves despawned)
Flushing  → Active     (RSM OnPausedChanged(false) — resume after flush)
Idle      → Active     (RSM OnPausedChanged(false) — resume from long pause)
```

Note: `Flushing→Idle`, `Flushing→Holding`, `Idle→Cold` are NOT valid — see AC-WS-15a.

**Transition enforcement (VR-9 + VR-10 applied):**
```cpp
void UWaveSpawnerSubsystem::TransitionTo(EWaveSpawnerLifecycleState NewState)
{
    const bool bValid = IsValidTransition(LifecycleState, NewState);
    if (!bValid)
    {
        // VR-9: UEnum::GetValueAsString() — LexToString() not available for UENUMs in UE 5.7
        UE_LOG(LogWaveSpawner, Error,
            TEXT("WaveSpawner: forbidden lifecycle transition %s -> %s (ignored). "
                 "See ADR-0011 D3 for the 9 valid transitions."),
            *UEnum::GetValueAsString(LifecycleState),
            *UEnum::GetValueAsString(NewState));
        check(false);  // VR-10: aborts in Dev/Test; no-op in Shipping
        return;        // reached only in Shipping
    }
    const EWaveSpawnerLifecycleState PrevState = LifecycleState;
    LifecycleState = NewState;
    OnLifecycleTransition(NewState);  // hook for pool swap, drain window, cold reset
    UE_LOG(LogWaveSpawner, Log,
        TEXT("WaveSpawner lifecycle: %s -> %s"),
        *UEnum::GetValueAsString(PrevState),
        *UEnum::GetValueAsString(NewState));
}
```

**IsTickable() state gate:**
```cpp
bool UWaveSpawnerSubsystem::IsTickable() const
{
    return LifecycleState != EWaveSpawnerLifecycleState::Cold
        && LifecycleState != EWaveSpawnerLifecycleState::Idle;
}
```

**Phase pool swap (atomic, pointer reassignment only — game-thread-safe as-is):**
```cpp
void UWaveSpawnerSubsystem::OnLifecycleTransition(EWaveSpawnerLifecycleState NewState)
{
    if (NewState == EWaveSpawnerLifecycleState::Holding)
    {
        bDrainWindowActive = true;
        // Advance active pool pointer: Opener→Mid or Mid→Peak
        if (ActivePhase == ERunPhase::Mid)         { ActiveDrawPool = &PeakPool; ActivePhase = ERunPhase::Peak; }
        else if (ActivePhase == ERunPhase::Opener) { ActiveDrawPool = &MidPool;  ActivePhase = ERunPhase::Mid;  }
        // Peak stays at Peak. Pattern pool data is NEVER copied — only raw pointer swapped.
    }
    else if (NewState == EWaveSpawnerLifecycleState::Active)
    {
        bDrainWindowActive = false;
        if (ActiveDrawPool == nullptr)
        {
            // First Cold→Active: initialise Opener pool
            ActiveDrawPool = &OpenerPool;
            ActivePhase = ERunPhase::Opener;
        }
        // Holding→Active: no pool swap (swap fired on Holding entry); just clear drain window.
        // Idle→Active: resumes without Cold reset; pool pointer already set from prior phase.
    }
    else if (NewState == EWaveSpawnerLifecycleState::Cold)
    {
        // Flushing→Cold (run-termination): full reset for next run.
        ActiveDrawPool = nullptr;
        ActivePhase = ERunPhase::Opener;
        bDrainWindowActive = false;
    }
    else if (NewState == EWaveSpawnerLifecycleState::Idle)
    {
        bDrainWindowActive = false;
    }
}
```

**Rule 9 drain window (Phase Boundary):**
- On `Active→Holding`, set `bDrainWindowActive = true`.
- On each tick: if drain window active and `CountInFlightWaves() == 0`, call `TransitionTo(Active)` (drain complete, resume admissions under next pool).
- `max_pattern_length_s` is the maximum expected pattern duration — if no explicit timer is needed, relying on `CountInFlightWaves() == 0` is sufficient per ADR-0011 D3.

**Snapshot immutability (TR-WS-022):**
- `telegraph_window_s` captured per-wave at admission time into `FWaveInFlightState.TelegraphWindowS`.
- Phase pool swap MUST NOT modify any `FWaveInFlightState` field.
- Verified by AC-WS-29 (Story 007).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 001: Class skeleton, pool pre-allocation, `FPatternPool` data structure.
- Story 003: Rule 1 admission gate and cadence gate (tick body admission logic — the state machine here gates whether ticking is ON, but the actual admission pipeline is Story 003).
- Story 007: RSM/DPC delegate subscriptions that trigger transitions (wiring `OnPausedChanged` to `TransitionTo(Flushing)`).

---

## QA Test Cases

*Test cases not yet defined — run /qa-plan to generate them.*

---

## Test Evidence

**Story Type**: Logic
**Required evidence**: `Source/SLIPSTORM/Tests/Unit/WaveSpawner/WaveSpawnerLifecycleTest.cpp` — must exist and pass

**Status**: [x] Created — `Source/SLIPSTORM/Tests/Unit/WaveSpawner/WaveSpawnerLifecycleTest.cpp` (7 test commands: Lifecycle.ValidTransitions, Lifecycle.ForbiddenTransitions, Lifecycle.PoolSwapOpenerToMid, Lifecycle.PoolSwapMidToPeak, Lifecycle.InFlightImmutability, Lifecycle.DrainWindowClearance, Lifecycle.ColdResetOnTermination)

---

## Dependencies

- Depends on: Story 001 (class skeleton + pool structure required for state machine to reference)
- Unlocks: Story 003 (admission gate requires active lifecycle state gating), Story 007 (RSM integration fires the lifecycle transitions)

---

## Completion Notes

**Completed**: 2026-08-19
**Criteria**: 3/3 passing (AC-WS-15a ✅ AC-WS-15b ✅ AC-WS-15c ✅)
**Deviations**:
- ADR-0011 D3 supersedes original Story ACs on 3 transition labels: `Flushing→Cold` (not `Flushing→Idle`), `Holding→Idle` (long-pause path), `Idle→Active` (not `Idle→Cold`). Implementation follows ADR. Story ACs reconciled 2026-08-19.
- `OnLifecycleTransition()` is 82 lines (exceeds 40-line standard). Refactor to per-state private methods deferred to Story 007 when Flushing case gains full flush logic.
- Three "will be reconciled" deviation notes in `.cpp` and test file are now stale — update suggested in a follow-up.
**Test Evidence**: Logic — `Source/SLIPSTORM/Tests/Unit/WaveSpawner/WaveSpawnerLifecycleTest.cpp` (7 commands, all ACs covered)
**Code Review**: Complete — APPROVED WITH SUGGESTIONS (advisory only; no required changes)

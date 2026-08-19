# Story 006: Rule 13 Six-Step Despawn Pipeline

> **Epic**: Pull-Wave Behavior
> **Status**: Complete
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: (fill before sprint planning)
> **Manifest Version**: 2026-07-03 (docs/registry/architecture.yaml v8 — no separate control-manifest.md; forbidden patterns cited from registry)
> **Last Updated**: 2026-08-19 (Complete)

## Context

**GDD**: `design/gdd/pull-wave-behavior.md`
**Requirement**: `TR-PW-016`, `TR-PW-017`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0010: Pull-Wave Object Pool, State Machine, and Despawn Pipeline
**ADR Decision Summary**: At DESPAWNING entry (reached via LANDED hold-expiry, pause-flush, or run-termination), Pull-Wave executes Rule 13's six-step pipeline exactly once per wave in fixed order: (1) Coll unregister, (2) Tel unregister, (3) `OnWaveDespawned` broadcast, (4) ISMC hide via `PerInstanceCustomData[2]=1.0`, (5) state clear (zero all fields), (6) `ActiveWaves.RemoveAt(SlotIndex)`. Each step fires exactly once per WaveId. When multiple waves despawn in the same tick (e.g., pause-flush batch), their per-wave pipelines run sequentially in WaveId ASC order with no interleaving — AC-PW-15 unified event log contiguity.

**Engine**: Unreal Engine 5.7 | **Risk**: LOW
**Engine Notes**: `UInstancedStaticMeshComponent::SetCustomDataValue(InstanceIndex, DataIndex, Value, bMarkDirty)` — sets `PerInstanceCustomData[2]` for per-instance dissolve signal; verified stable in ADR-0006. `TArray::RemoveAt(Index)` — O(N) shift; at N=23 max ~4 KB memcpy, sub-microsecond on mid-tier mobile. Non-dynamic multicast delegate broadcast (`OnWaveDespawned.Broadcast`) — stable pre-UE-4. No post-cutoff APIs.

**Control Manifest Rules (Feature layer)** — from `docs/registry/architecture.yaml` v8:
- Required: Six-step pipeline executes in the fixed order specified by Rule 13 — no reordering, no step skipping.
- Required: Step 3 (`OnWaveDespawned`) carries `EDespawnReason ∈ {NaturalLanding, PauseFlush, RunTermination}` — subscribers use this to distinguish lifecycle paths.
- Required: Step 4 hides the ISMC instance via `PerInstanceCustomData[2] = 1.0` (not `UpdateInstanceTransform` to origin — the dissolve shader reads index 2 to trigger hide).
- Required: Step 6 uses `TArray::RemoveAt(SlotIndex)` only — `RemoveAtSwap` forbidden (AC-PW-22b pattern 9).
- Required: Per-wave despawn pipeline is contiguous in the unified event log — no events from a second wave's pipeline may appear between steps 1–6 of a first wave's pipeline (AC-PW-15).
- Required: Seam 13 `SetOnDespawnedUserCallback()` slot is provided on `APullWaveSubsystemActor` (non-Shipping testability — R10c).
- Forbidden: `ActiveWaves.RemoveAtSwap` — breaks WaveId ASC invariant.
- Forbidden: Re-entering DESPAWNING from DESPAWNING (forbidden-transition table enforced by `TransitionTo()`).

---

## Acceptance Criteria

*From GDD `design/gdd/pull-wave-behavior.md` §Rule 13 / §Despawn Pipeline, scoped to this story:*

- [ ] **AC-PW-15 (unified event log contiguity):** Given 3 waves (WaveIds N, N+1, N+2) simultaneously entering DESPAWNING (e.g., via pause-flush batch in the same tick): `FWaveSpawnerCallbackTestStub` event log records each wave's 6-step pipeline completely before the next wave's pipeline begins. Step ordering per wave: Coll-unregister(WaveId=N) → Tel-unregister(N) → OnWaveDespawned(N) → ISMC-hide(N) → state-clear(N) → RemoveAt(N) — then N+1, then N+2. No interleaving across WaveIds.
- [ ] **Six-step ordering (single wave, NaturalLanding):** Given wave in LANDED→DESPAWNING transition (NaturalLanding reason): steps fire in the order Coll-unregister → Tel-unregister → `OnWaveDespawned(WaveId, DespawnReason=NaturalLanding)` → ISMC `PerInstanceCustomData[2]=1.0` → state-clear → `RemoveAt`. All 6 steps on the same tick as DESPAWNING entry.
- [ ] **Six-step ordering (PauseFlush reason):** Given wave mid-TRAVERSING when pause-flush fires: DESPAWNING entry with `DespawnReason=PauseFlush`; all 6 steps fire; `FWaveSpawnerCallbackTestStub.GetDespawnReason(WaveId) == PauseFlush`.
- [ ] **Six-step ordering (RunTermination reason):** Given wave mid-LEANING when RSM transitions RUNNING→DEAD: wave continues (not immediately flushed — that is Story 008's contract), reaches DESPAWNING naturally or via Story 008 drain; `DespawnReason=RunTermination`. Stubbed via `FRSMTestStub.SetCurrentState(DEAD)`.
- [ ] **ISMC hide via PerInstanceCustomData[2]:** After step 4, `FISMCTestStub.GetCustomDataValue(ISMCInstanceIndex, DataIndex=2) == 1.0f`. The instance is NOT removed from the ISMC — only hidden (dissolve signal). Instance remains in ISMC until pool reset.
- [ ] **State clear (step 5):** After step 5, `FPullWaveInstanceState` fields `WaveId`, `TargetLane`, `SourceLane`, `TraverseElapsedS`, `LandedHoldElapsedS`, `CollisionOutcome`, `LeanProgress`, `ForwardVelocityMs`, `State` are reset to zero/default. Wave is inert — even if ticked again before `RemoveAt`, no events fire.
- [ ] **RemoveAt (step 6):** After step 6, `ActiveWaves.Num()` decreases by 1. Remaining waves retain WaveId ASC ordering (verified by iterating post-removal).
- [ ] **Seam 13 `SetOnDespawnedUserCallback()`:** `APullWaveSubsystemActor` exposes `SetOnDespawnedUserCallback(TFunction<void(int32 WaveId, EDespawnReason)> Callback)` in non-Shipping builds. Setting this callback routes step 3's broadcast to the stub's capture slot. Test harness can assert `GetDespawnCallbackFireCount() == 1` after a single despawn.
- [ ] **`OnWaveDespawned` delegate:** `DECLARE_MULTICAST_DELEGATE_TwoParams(FOnWaveDespawned, int32 /*WaveId*/, EDespawnReason /*Reason*/)` declared; `OnWaveDespawned.Broadcast(Wave.WaveId, Reason)` fires exactly ONCE per wave per lifetime.
- [ ] `OnLeanProgress` delegate is NOT fired during DESPAWNING entry (LEANING is over; no lean progress during despawn).

---

## Implementation Notes

*Derived from ADR-0010 D4 Implementation Guidelines:*

**DESPAWNING entry body (fires once, on the tick where LANDED→DESPAWNING, PauseFlush→DESPAWNING, or RunTermination→DESPAWNING transition occurs):**

```
void APullWaveSubsystemActor::AdvanceDespawning(FPullWaveInstanceState& Wave, EDespawnReason Reason, int32 SlotIndex)
{
    // Step 1: Collision system unregister
    CollisionSubsystem->UnregisterWave(Wave.WaveId);

    // Step 2: Telegraph system unregister
    TelegraphSubsystem->UnregisterWave(Wave.WaveId);

    // Step 3: OnWaveDespawned broadcast
    OnWaveDespawned.Broadcast(Wave.WaveId, Reason);
    if (OnDespawnedUserCallback) { OnDespawnedUserCallback(Wave.WaveId, Reason); }  // Seam 13 test slot

    // Step 4: ISMC hide (dissolve signal — shader reads PerInstanceCustomData[2])
    WaveMassISMC->SetCustomDataValue(Wave.ISMCInstanceIndex, 2, 1.0f, false);

    // Step 5: State clear
    Wave = FPullWaveInstanceState{};  // zero-initialize

    // Step 6: Pool slot return
    ActiveWaves.RemoveAt(SlotIndex);
    // NOTE: After RemoveAt, 'Wave' reference is invalid — no further access.
}
```

**Caller pattern (in Tick body's per-wave dispatch):**
```
case DESPAWNING:
    AdvanceDespawning(Wave, Wave.PendingDespawnReason, i);
    --i;  // adjust loop index after RemoveAt
    break;
```

`Wave.PendingDespawnReason` (field on `FPullWaveInstanceState`) is set at the `TransitionTo(DESPAWNING)` call site by whichever path triggered the despawn (Story 005 sets `NaturalLanding`; Story 007 sets `PauseFlush`; Story 008 sets `RunTermination`).

**`EDespawnReason` enum:**
```cpp
UENUM()
enum class EDespawnReason : uint8
{
    NaturalLanding,
    PauseFlush,
    RunTermination
};
```

**Seam 13 slot (non-Shipping only):**
```cpp
#if !UE_BUILD_SHIPPING
TFunction<void(int32 /*WaveId*/, EDespawnReason)> OnDespawnedUserCallback;
void SetOnDespawnedUserCallback(TFunction<void(int32, EDespawnReason)> Callback)
    { OnDespawnedUserCallback = MoveTemp(Callback); }
#endif
```

**AC-PW-15 contiguity guaranteed by:** running the full 6-step pipeline inline for each wave before advancing the loop index to the next wave. No deferred step or cross-wave interleaving is possible as long as `AdvanceDespawning` runs to completion synchronously.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 005: LANDED hold → `TransitionTo(DESPAWNING)` trigger; `PendingDespawnReason = NaturalLanding` set there.
- Story 007: Pause-flush batch routing (sets `PendingDespawnReason = PauseFlush`; calls `TransitionTo(DESPAWNING)` for affected waves).
- Story 008: Run-termination path (sets `PendingDespawnReason = RunTermination`; each wave drains naturally into DESPAWNING).
- Story 009: ISMC add-instance at `Construct()` — the initial `ISMCInstanceIndex` assignment that step 4 references.

---

## QA Test Cases

*Test cases not yet defined — run /qa-plan to generate them.*

---

## Test Evidence

**Story Type**: Integration
**Required evidence**: `Source/SLIPSTORM/Tests/Integration/PullWave/PullWaveDespawnPipelineTest.cpp` OR documented playtest — must exist and pass

**Status**: [x] Created — Source/SLIPSTORM/Tests/Integration/PullWave/PullWaveDespawnPipelineTest.cpp (11 test commands)

---

## Dependencies

- Depends on: Story 003 (TransitionTo helper), Story 004 (TRAVERSING dispatch loop that hosts DESPAWNING case), Story 005 (LANDED hold sets DespawnReason=NaturalLanding)
- Unlocks: Story 007 (pause-flush needs DESPAWNING entry to fire correctly), Story 008 (run-termination drain ends in DESPAWNING), Story 009 (Construct() sets ISMCInstanceIndex used by step 4)

---

## Completion Notes

**Completed**: 2026-08-19  
**Criteria**: 10/10 passing  
**Deviations**: ADVISORY — ADR-0010 D4 step 5 specifies "except WaveId which remains for last-tick observation window"; implementation uses `Wave = FPullWaveInstanceState{}` (zeroes WaveId). WaveId captured in `const int32` before step 5; `RemoveAt` fires immediately in step 6. No observable impact; diverges from spec letter.  
**Test Evidence**: Integration test at `Source/SLIPSTORM/Tests/Integration/PullWave/PullWaveDespawnPipelineTest.cpp` (11 test commands). Post-review additions: `OnStateClearOverride` seam (step 5 field-level assertions in TC5), `ensure(ActiveWaves.GetSlack() > 0)` guard before step 3 broadcast, fire-count assertions in TC1/TC2/TC3.  
**Code Review**: Complete — `/code-review` run this session; 3 blocking items identified and fixed before close.

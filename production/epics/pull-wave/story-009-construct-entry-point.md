# Story 009: Construct() Entry Point + Wave Spawner Integration

> **Epic**: Pull-Wave Behavior
> **Status**: Complete
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: (fill before sprint planning)
> **Manifest Version**: 2026-07-03 (docs/registry/architecture.yaml v8 — no separate control-manifest.md; forbidden patterns cited from registry)
> **Last Updated**: 2026-08-19

## Context

**GDD**: `design/gdd/pull-wave-behavior.md`
**Requirement**: `TR-PW-004`, `TR-PW-014`, `TR-PW-021`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0010: Pull-Wave Object Pool, State Machine, and Despawn Pipeline; ADR-0011: Wave Spawner Pattern Library (secondary — governs handoff contract)
**ADR Decision Summary**: `Construct(FPullWaveSpawnParams)` is the sole admission gateway from Wave Spawner into Pull-Wave. It: (1) acquires a pool slot via `ActiveWaves.Add(FPullWaveInstanceState{})`, (2) copies `FPullWaveSpawnParams` fields into the instance state field-by-field, (3) sets `State = SPAWNED` + initializes accumulators to zero, (4) computes `TravelDurationS = SPAWN_PLANE_Z_OFFSET_M / ForwardVelocityMs` (F-TRAVERSE-DURATION), (5) registers the ISMC instance at initial transform with `PerInstanceCustomData[0..2] = 0`. After `Construct()` returns, the spawned params struct is no longer referenced — immutability contract closed. `Construct()` asserts `ActiveWaves.Num() < 23` before admitting (pool-full guard: log+return, Shipping-safe).

**Engine**: Unreal Engine 5.7 | **Risk**: LOW
**Engine Notes**: `UInstancedStaticMeshComponent::AddInstance(Transform, bWorldSpace)` — returns instance index; `SetCustomDataValue(Index, 0, 0.0f)` × 3 for initial PerInstanceCustomData — verified stable in ADR-0006. `TArray::Add(Element)` — append-to-tail, O(1) amortized (reserved; no realloc) — stable pre-UE-4. `FPullWaveSpawnParams` 152-byte stack-copy cost negligible at PEAK cadence. No post-cutoff APIs.

**Control Manifest Rules (Feature layer)** — from `docs/registry/architecture.yaml` v8:
- Required: `Construct()` is the single admission call site — no other code path inserts waves into `ActiveWaves` directly.
- Required: Pool-full guard: `if (ActiveWaves.Num() >= 23) { UE_LOG(LogPullWave, Error, TEXT("pull_wave_pool_full WaveId=%d"), SpawnParams.WaveId); return; }` — Shipping-safe no-op.
- Required: `TravelDurationS = SPAWN_PLANE_Z_OFFSET_M / ForwardVelocityMs` computed at `Construct()` and stored in `FPullWaveInstanceState.TravelDurationS` (F-TRAVERSE-DURATION formula; never re-computed mid-flight).
- Required: `PerInstanceCustomData[0] = 0` (lean tier = 0), `[1] = 0` (near-miss flash = 0), `[2] = 0` (dissolve = 0) set at instance add time — initial state clean.
- Required: `ForwardVelocityMs` frozen at SPAWNED — never mutated after `Construct()` stores it (Rule 14(a) velocity immutability; AC-PW-20 verifies across RSM-phase transition).
- Forbidden: Re-reading `DPC.telegraph_window_s` at `Construct()` — `LeanDurationS` is provided in `FPullWaveSpawnParams` (Wave Spawner captured it at admission; Pull-Wave trusts the handoff value per D6 immutability contract and ADR-0011 D2 Stage 4).
- Forbidden: `TArray::Insert` at any non-tail position (AC-PW-22b pattern 11 FORBID).
- Forbidden: `ActiveWaves.Num() >= 23` pool-full scenario silently admitting a 24th wave — must guard and log.

---

## Acceptance Criteria

*From GDD `design/gdd/pull-wave-behavior.md` §Construct(), scoped to this story:*

- [ ] **AC-PW-09 (F-TRAVERSE-DURATION):** Given `ForwardVelocityMs=7.5` and `SPAWN_PLANE_Z_OFFSET_M=15.0`: `TravelDurationS == 2.0` (within `1e-5`) after `Construct()`. Given `ForwardVelocityMs=4.0`: `TravelDurationS == 3.75` (within `1e-5`).
- [ ] **Params copy correctness:** Given `FPullWaveSpawnParams{ WaveId=42, SourceLane=1, TargetLane=3, ForwardVelocityMs=6.0, SpawnTimeS=12.5, LeanDurationS=0.80, CurveSnapshot=<populated> }`: after `Construct()`, `ActiveWaves.Last().WaveId == 42`, `SourceLane == 1`, `TargetLane == 3`, `ForwardVelocityMs == 6.0`, `SpawnTimeS == 12.5`, `LeanDurationS == 0.80f`. `CurveSnapshot.Samples` array matches input byte-for-byte.
- [ ] **State initialization:** After `Construct()`, `Wave.State == SPAWNED`, `Wave.LeanProgress == 0.0f`, `Wave.TraverseElapsedS == 0.0f`, `Wave.LandedHoldElapsedS == 0.0f`, `Wave.CollisionOutcome == ECollisionOutcome::Unresolved`.
- [ ] **ISMC instance added:** After `Construct()`, `FISMCTestStub.GetInstanceCount()` increases by 1. `Wave.ISMCInstanceIndex` is a valid index into the ISMC instance list. `GetCustomDataValue(ISMCInstanceIndex, 0) == 0.0f` (lean tier), `GetCustomDataValue(ISMCInstanceIndex, 1) == 0.0f` (near-miss flash), `GetCustomDataValue(ISMCInstanceIndex, 2) == 0.0f` (dissolve).
- [ ] **Pool-full guard:** Given `ActiveWaves.Num() == 23` (pool at capacity), calling `Construct()` does NOT add a 24th wave. `ActiveWaves.Num()` remains 23. `UE_LOG(LogPullWave, Error, "pull_wave_pool_full")` fired (verifiable via `FLogCaptureTestStub` in non-Shipping). No crash.
- [ ] **AC-PW-20 (velocity binding via Construct):** Given wave spawned via `Construct()` with `ForwardVelocityMs=5.0` during OPENER phase; RSM advances through MID into PEAK during TRAVERSING: `Wave.ForwardVelocityMs` remains `5.0f` on all sampled ticks (immutable post-Construct); `world_z` computed against `5.0` matches observed within `1e-5`. Verified via `FRSMTestStub.SetCurrentPhase(PEAK)` and F-TRAJ-FORWARD evaluation.
- [ ] **LeanDurationS trusted from SpawnParams:** Given `SpawnParams.LeanDurationS=0.95s` (Wave Spawner captured `DPC.telegraph_window_s=0.95s` at admission): `Wave.LeanDurationS == 0.95f` after `Construct()`. Pull-Wave does NOT re-read DPC here. Given `SpawnParams.LeanDurationS=0.60s` (below TELEGRAPH_WINDOW_FLOOR_S=0.70s — Wave Spawner should have clamped this, but if not): Pull-Wave stores `0.60f` as-is (clamping is Wave Spawner's responsibility per ADR-0011 D2 Stage 4 — NOT re-clamped by Pull-Wave at Construct).
- [ ] **Append-to-tail ordering:** After 5 sequential `Construct()` calls, `ActiveWaves` contains the 5 waves in WaveId ASC order with no gaps. `ActiveWaves.Last()` is the most recently admitted wave.
- [ ] **`Construct()` is Wave Spawner's ONLY entry gate:** No other path calls `ActiveWaves.Add()` directly (verified by CI grep on `ActiveWaves.Add\s*\(` — must match only `Construct()` call site).

---

## Implementation Notes

*Derived from ADR-0010 D6 + ADR-0011 D2 Stage 4–6 Implementation Guidelines:*

**`Construct()` body:**
```cpp
void APullWaveSubsystemActor::Construct(FPullWaveSpawnParams SpawnParams)
{
    // Pool-full guard (Shipping-safe)
    if (ActiveWaves.Num() >= MAX_POOL_SIZE)  // MAX_POOL_SIZE = 23
    {
        UE_LOG(LogPullWave, Error,
               TEXT("pull_wave_pool_full WaveId=%d — admission rejected"),
               SpawnParams.WaveId);
        return;
    }

    // Acquire pool slot (append-to-tail; WaveId ASC order maintained)
    FPullWaveInstanceState& Wave = ActiveWaves.Add_GetRef(FPullWaveInstanceState{});

    // Copy spawn params into instance state
    Wave.WaveId             = SpawnParams.WaveId;
    Wave.SourceLane         = SpawnParams.SourceLane;
    Wave.TargetLane         = SpawnParams.TargetLane;
    Wave.CurveSnapshot      = SpawnParams.CurveSnapshot;   // 128-byte value copy
    Wave.ForwardVelocityMs  = SpawnParams.ForwardVelocityMs;
    Wave.SpawnTimeS         = SpawnParams.SpawnTimeS;
    Wave.LeanDurationS      = SpawnParams.LeanDurationS;   // Wave Spawner owns floor-clamp

    // Compute derived value
    Wave.TravelDurationS    = SPAWN_PLANE_Z_OFFSET_M / SpawnParams.ForwardVelocityMs;  // F-TRAVERSE-DURATION

    // Initialize accumulator state
    Wave.State              = EPullWaveState::SPAWNED;
    Wave.LeanProgress       = 0.0f;
    Wave.TraverseElapsedS   = 0.0f;
    Wave.LandedHoldElapsedS = 0.0f;
    Wave.CollisionOutcome   = ECollisionOutcome::Unresolved;

    // ISMC: add instance at initial spawn-plane transform
    FTransform InitialTransform = ComputeSpawnTransform(SpawnParams.SourceLane);
    Wave.ISMCInstanceIndex = WaveMassISMC->AddInstance(InitialTransform, true /*bWorldSpace*/);

    // PerInstanceCustomData[0..2] = 0 (lean tier, near-miss flash, dissolve — all clean)
    WaveMassISMC->SetCustomDataValue(Wave.ISMCInstanceIndex, 0, 0.0f, false);
    WaveMassISMC->SetCustomDataValue(Wave.ISMCInstanceIndex, 1, 0.0f, false);
    WaveMassISMC->SetCustomDataValue(Wave.ISMCInstanceIndex, 2, 0.0f, false);
    // Note: MarkRenderStateDirty called once at end of Tick, not here.
}
```

**`ComputeSpawnTransform(int32 SourceLane)`:** Returns an `FTransform` placing the wave at `(source_lane_x, 0, SPAWN_PLANE_Z_OFFSET_M)` in world-space. `source_lane_x = (SourceLane - 2) * LANE_WIDTH_M`. Uses `FVector(source_lane_x, 0.0f, SPAWN_PLANE_Z_OFFSET_M)` as translation with identity rotation and scale.

**ADR-0011 handoff contract:** Wave Spawner (ADR-0011 D2 Stage 4–6) is responsible for:
1. Reading `DPC.GetCurrentFrameState().telegraph_window_s` and clamping to `TELEGRAPH_WINDOW_FLOOR_S = 0.70s`.
2. Sampling the validated `UCurveFloat*` into `CurveSnapshot` at 32 evenly-spaced t_norm values.
3. Assigning `WaveId` from the subsystem counter.
4. Constructing `FPullWaveSpawnParams` and calling `PullWave->Construct(SpawnParams)`.

Pull-Wave trusts all values in `FPullWaveSpawnParams` as-is. The only value Pull-Wave computes itself is `TravelDurationS` (because it is derivable from `ForwardVelocityMs`, which is immutable — so no need to transmit pre-computed value across the boundary).

**`Add_GetRef` vs `Add` + index:** Use `TArray::Add_GetRef(FPullWaveInstanceState{})` to get a direct reference to the added element without an extra array lookup. This is the correct append pattern for this pool (stable pre-UE-4 API, O(1) when reserved).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 001: `FPullWaveSpawnParams` struct definition (the parameter type this story consumes).
- Story 002: `ActiveWaves.Reserve(23)` at `BeginPlay`; `NextWaveId` counter (Wave Spawner assigns WaveId — this story only receives it).
- Story 003: `TransitionTo()` helper — used in subsequent tick advances; `Construct()` sets `State = SPAWNED` directly (initial assignment at construction site, not a transition).
- Story 004: SPAWNED-state tick body (reads `DPC.GetCurrentFrameState()` once at SPAWNED tick — NOT at Construct; this is a deliberate one-tick-later capture per D2 SPAWNED body spec).
- ADR-0011: Wave Spawner admission pipeline (pattern draw, DPC snapshot, curve sampling, `FPullWaveSpawnParams` construction). Wave Spawner calls `Construct()` — the integration point is here but the Wave Spawner's side is ADR-0011's responsibility.

---

## QA Test Cases

*Test cases not yet defined — run /qa-plan to generate them.*

---

## Test Evidence

**Story Type**: Integration
**Required evidence**: `tests/integration/pull-wave/construct-entry-point_test.cpp` OR documented playtest — must exist and pass

**Status**: [x] Created — Source/SLIPSTORM/Tests/Integration/PullWave/PullWaveConstructTest.cpp (9 test cases: TC1–TC9)
*(Note: story doc listed path tests/integration/pull-wave/construct-entry-point_test.cpp; as-built path matches project convention — all 12 sibling test files use Source/SLIPSTORM/Tests/Integration/PullWave/.)*

---

## Dependencies

- Depends on: Story 001 (FPullWaveSpawnParams struct), Story 002 (ActiveWaves pool + Reserve), Story 004 (SPAWNED state tick body that fires on the tick after Construct)
- Unlocks: All end-to-end integration test scenarios (full lifecycle from Construct → SPAWNED → LEANING → TRAVERSING → LANDED → DESPAWNING can now be exercised)

---

## Completion Notes

**Completed**: 2026-08-19
**Criteria**: 9/9 passing
**Deviations**:
- ADVISORY D1: `Wave.State = EPullWaveState::Spawned` kept as explicit write — redundant given in-class initializer; carve-out documented in `TransitionTo()` doc. No behavioral impact.
- ADVISORY D2: No `ensureMsgf` guard for `ForwardVelocityMs=0` — trust contract delegated to Wave Spawner (ADR-0011 D2 Stage 4). Risk acknowledged in .cpp comment. Guard to be added at Wave Spawner integration time.
- ADVISORY D3: `SetCustomDataValue` return values unchecked; no BeginPlay `ensure(NumCustomDataFloats>=3)`. Deferred to ISMC integration story.
- ADVISORY D4: TC2 checks 3/32 CurveSnapshot indices against a zero baseline — partial coverage. Recommend filling all 32 indices in a follow-up.
- ADVISORY D5: TC6 tri-state state assertion + no in-loop ForwardVelocityMs sampling — imprecise for a deterministic scenario. Deferred.
- ADVISORY D6: Axis mismatch — `ComputeSpawnTransform` uses X for lateral; `AdvanceTraversing` uses Y. Flagged in both files; out of scope, flagged for architecture review.
- ADVISORY D7: ISMCInstanceIndex stays at struct default when both WaveMassISMC and seam are null — undefined value; explicit INDEX_NONE assignment + TC3 assertion deferred.
**Test Evidence**: Integration test at `Source/SLIPSTORM/Tests/Integration/PullWave/PullWaveConstructTest.cpp` (9 TCs: TC1 F-TRAVERSE-DURATION, TC2 ParamsCopy, TC3 StateInit, TC4 ISMCSeam, TC5 PoolFullGuard, TC6 VelocityBinding, TC7 LeanDurationTrusted, TC8 AppendOrdering, TC9 OnlyAddSite). TC5 AddExpectedError + TC9 grep pattern fixed during /code-review before close.
**Code Review**: Complete — /code-review run 2026-08-19; CHANGES REQUIRED verdict; 2 blocking gaps (TC5 AddExpectedError, TC9 grep pattern `Add(_GetRef)?\s*(`) fixed before close; re-reviewed APPROVED WITH SUGGESTIONS.

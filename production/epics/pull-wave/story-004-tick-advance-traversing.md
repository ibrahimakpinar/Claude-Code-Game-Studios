# Story 004: Per-Tick Advance (SPAWNED→TRAVERSING) + Pause-Freeze Gate

> **Epic**: Pull-Wave Behavior
> **Status**: Complete
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: 3–5 hours
> **Manifest Version**: 2026-07-03 (docs/registry/architecture.yaml v8 — no separate control-manifest.md; forbidden patterns cited from registry)
> **Last Updated**: 2026-08-18

## Context

**GDD**: `design/gdd/pull-wave-behavior.md`
**Requirement**: `TR-PW-003`, `TR-PW-015`, `TR-PW-021`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0010: Pull-Wave Object Pool, State Machine, and Despawn Pipeline
**ADR Decision Summary**: The `APullWaveSubsystemActor::Tick()` body iterates `ActiveWaves` in WaveId ASC order. For each wave, a pause-freeze guard fires first (`if (RSM.GetIsPaused()) { return; }`). Then the per-wave state dispatch executes SPAWNED/LEANING/TRAVERSING advance bodies. SPAWNED is one-tick init (reads DPC snapshot once). LEANING accumulates `LeanProgress`. TRAVERSING accumulates `TraverseElapsedS` and evaluates trajectory formulas to produce ISMC transform updates. `TraverseElapsedS` is the canonical time source — `GameTime` is never subtracted from `SpawnTimeS` in trajectory evaluation.

**Engine**: Unreal Engine 5.7 | **Risk**: LOW
**Engine Notes**: `APullWaveSubsystemActor` tick subscribes to the UE tick group chain (ADR-0007/0008 prerequisite mechanism ensures RSM→DPC→PM tick before Pull-Wave). `UInstancedStaticMeshComponent::UpdateInstanceTransform(Index, Transform, bWorldSpace, bMarkRenderStateDirty)` — verified stable in ADR-0006. `MarkRenderStateDirty()` called once per frame (not per wave) for batched dirty. Read `RSM.GetIsPaused()` via ADR-0007 interface. Read `DPC.GetCurrentFrameState()` via ADR-0008 interface. Post-cutoff: none.

**Control Manifest Rules (Feature layer)** — from `docs/registry/architecture.yaml` v8:
- Required: `SCOPE_CYCLE_COUNTER(STAT_PullWaveTick)` as FIRST statement inside `APullWaveSubsystemActor::Tick()` after `Super::Tick()` (AC-PW-22b pattern 2).
- Required: Pause-freeze guard `if (RSM.GetIsPaused()) { return; }` at TOP of per-wave tick body — BEFORE state dispatch (D2 pause-freeze semantic).
- Required: `DPC.GetCurrentFrameState().telegraph_window_s` read ONCE per wave at SPAWNED entry only; stored into `LeanDurationS = FMath::Max(ReadValue, TELEGRAPH_WINDOW_FLOOR_S)`. Never re-read mid-lean.
- Required: `TraverseElapsedS += DeltaTime` (accumulator-based); NEVER `world_z`-from-`GameTime - SpawnTimeS` (wall-clock form prohibited by AC-PW-22b pattern 5a FORBID).
- Forbidden: `GameTime - SpawnTimeS` or `SpawnTimeS - GameTime` in trajectory evaluation source (AC-PW-22b pattern 5a CI grep — must return 0 matches).
- Forbidden: Pull-Wave reading `DPC.current_phase`, `DPC.wave_spawn_interval_s`, or `DPC.max_concurrent_waves` (D5 explicit forbidden reads).
- Forbidden: Pull-Wave subscribing to `PM.OnSlipMidpoint` delegate (D5 explicit forbidden reads per Rule 11 R1 RC-A).
- Forbidden: Direct assignment of lane or trajectory fields after SPAWNED entry (Rule 4 param immutability).

---

## Acceptance Criteria

*From GDD `design/gdd/pull-wave-behavior.md` §Formula Correctness + §State Machine Transitions, scoped to this story:*

- [ ] **AC-PW-01 (F-TRAJ-TNORM):** Given `ForwardVelocityMs=7.5`, `SPAWN_PLANE_Z_OFFSET_M=15.0` (→ `TravelDurationS=2.0`), accumulator `TraverseElapsedS=1.2s` delivered via Seam 7/CF-A2 injected delta, when F-TRAJ-TNORM evaluates: `|t_norm − 0.6| < 1e-5`.
- [ ] **AC-PW-04 (F-TRAJ-LATERAL):** Given `SourceLane=4`, `TargetLane=1`, `LANE_WIDTH_M=1.0`, `t_norm=0.6`, `FCurveProviderTestStub` returns `0.7` at `t_norm=0.6`: `|world_x − (−0.1)| < 1e-5` (source_lane_x=2.0; lateral_offset=0.7×(1-4)×1.0=−2.1; world_x=−0.1).
- [ ] **AC-PW-05 (tier-0 straight-forward):** Given `SourceLane==TargetLane=2`, at `t_norm ∈ {0.0, 0.5, 1.0}`: `|world_x − 0.0| < 1e-5` on all three.
- [ ] **AC-PW-06:** Given `t_norm=0.0` and canonical curve snapshot (`Samples[0]=0.0`): `|world_x − source_lane_x| < 1e-5`.
- [ ] **AC-PW-07:** Given `t_norm=1.0`, `SourceLane=0`, `TargetLane=4`, `LANE_WIDTH_M=1.0`: `|world_x − 2.0| < 1e-5`.
- [ ] **AC-PW-08 (F-TRAJ-FORWARD):** Given `SpawnPlaneZ=15.0`, `t_norm=0.6`: `|world_z − 6.0| < 1e-5`. At `t_norm=0.0`: `world_z=15.0` (exact). At `t_norm=1.0`: `world_z=0.0` (algebraic identity, exact).
- [ ] **AC-PW-11 (SPAWNED→LEANING):** After `Construct()` sets `State=SPAWNED`, one tick fires; state advances to `LEANING`. `target_lane`, `source_lane`, `lean_duration_s`, `forward_velocity_ms`, and `CurveSnapshot` are frozen — late mutation attempts produce no observable change.
- [ ] **AC-PW-12 (LEANING→TRAVERSING):** Given `lean_duration_s=0.70s` (R10d FLOOR), when `lean_progress >= 1.0`, state → TRAVERSING on that exact tick with no gap frame. `TraverseElapsedS` initialized to `0.0f` at TRAVERSING entry. F-TRAJ-TNORM reads `0.0` (within `1e-5`) at TRAVERSING entry tick (read-before-increment contract).
- [ ] **AC-PW-16 (pause-freeze):** Given `RSM.SetIsPaused(true)` during TRAVERSING, when 20 ticks fire with injected `DeltaTime=0.016` (CF-A2): (a) `bWaveFound==true`; (b) `State==TRAVERSING`; (c) `ElapsedS` unchanged (within `1e-5`); (d) `t_norm` unchanged; (e) `world_z` unchanged. All five asserted independently.
- [ ] **AC-PW-20 (velocity binding):** Given wave spawns with `ForwardVelocityMs=5.0` during OPENER; RSM advances through MID into PEAK during TRAVERSING: `forward_velocity_ms` remains `5.0` on all sampled ticks; `world_z` computed against `5.0` matches observed within `1e-5`.
- [ ] **AC-PW-TIER0-LEANING:** Given `SourceLane==TargetLane==2` (tier-0), `LeanDurationS=0.70s`, ticked across full LEANING duration: each tick `lean_magnitude_tier==0`, `lean_angle_deg==0.0°` (within `1e-5`), `LeanChargeIntensity_scalar` ramps 1.0→`LEAN_BRIGHTNESS_PEAK_RATIO` via eased curve. Wave transitions LEANING→TRAVERSING normally at `lean_progress>=1.0`.
- [ ] AC-PW-22b pattern 2 (SCOPE_CYCLE_COUNTER) and pattern 5 (time-source regression guard) pass.
- [ ] `DECLARE_STATS_GROUP` for `STATGROUP_PullWave` present (AC-PW-22b pattern 1).

---

## Implementation Notes

*Derived from ADR-0010 D2 + D5 Implementation Guidelines:*

**Tick body structure (D2):**
```
APullWaveSubsystemActor::Tick(DeltaTime):
  SCOPE_CYCLE_COUNTER(STAT_PullWaveTick)
  [pause-flush top-of-tick batch — Story 007]
  for each Wave in ActiveWaves (WaveId ASC):
    if (RSM.GetIsPaused()) { continue; }  // pause-freeze
    switch (Wave.State):
      SPAWNED  → AdvanceSpawned(Wave, DeltaTime)
      LEANING  → AdvanceLeaning(Wave, DeltaTime)
      TRAVERSING → AdvanceTraversing(Wave, DeltaTime)
      LANDED   → [Story 005]
      DESPAWNING → [Story 006]
  [ISMC MarkRenderStateDirty once per frame]
```

**SPAWNED body (one-tick init):**
- Read `DPC.GetCurrentFrameState().is_active` — assert `true` (safety check, Shipping-safe log+return on false — handled in Story 009 `Construct()`).
- Read `DPC.GetCurrentFrameState().telegraph_window_s` → `Wave.LeanDurationS = FMath::Max(raw, TELEGRAPH_WINDOW_FLOOR_S)`. ONCE — never re-read.
- `TransitionTo(Wave, LEANING)` (unconditional next tick entry — SPAWNED is one-tick only).

**LEANING body:**
- `Wave.LeanProgress += DeltaTime / Wave.LeanDurationS`; clamp to [0, 1].
- Fire `OnLeanProgress(Wave.WaveId, Wave.LeanProgress)` each tick (Telegraph reads this).
- If `Wave.LeanProgress >= 1.0`: `TransitionTo(Wave, TRAVERSING)`; `Wave.TraverseElapsedS = 0.0f`.

**TRAVERSING body (F-TRAJ-* evaluation):**
- `Wave.TraverseElapsedS += DeltaTime` (accumulator; pause-freeze guard above already skipped if paused).
- `float t_norm = FMath::Clamp(Wave.TraverseElapsedS / Wave.TravelDurationS, 0.0f, 1.0f)` — F-TRAJ-TNORM.
- `float source_lane_x = (Wave.SourceLane - 2) * LANE_WIDTH_M`.
- `float lateral_offset = Wave.CurveSnapshot.EvaluateAt(t_norm) * (Wave.TargetLane - Wave.SourceLane) * LANE_WIDTH_M`.
- `float world_x = source_lane_x + lateral_offset` — F-TRAJ-LATERAL.
- `float world_z = SPAWN_PLANE_Z * (1.0f - t_norm)` — F-TRAJ-FORWARD (or equivalently `SPAWN_PLANE_Z - t_norm * SPAWN_PLANE_Z_OFFSET_M`).
- If `world_z <= PLAYER_PLANE_Z`: `TransitionTo(Wave, LANDED)` (threshold-cross — Story 005 takes over at LANDED entry).
- Update ISMC instance transform: `WaveMassISMC->UpdateInstanceTransform(Wave.ISMCInstanceIndex, NewTransform, true, false)` (bWorldSpace=true, defer dirty — dirty called once per frame after loop).

**Tick ordering:** RSM → DPC → PM → Pull-Wave → Telegraph → Collision. `APullWaveSubsystemActor` tick must register AFTER the DPC/PM ticks per the prerequisite chain (ADR-0007/0008 tick hosting; established at BeginPlay via `AddTickPrerequisiteActor` or equivalent).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 005: LANDED entry body (hit/near-miss resolution, OnWaveHit/OnNearMiss broadcasts).
- Story 006: DESPAWNING entry body (six-step despawn pipeline).
- Story 007: Pause-flush batch at top-of-tick (bPauseFlushPending consumption).
- Story 008: Run-termination drain detection and semantics.
- Story 009: `Construct()` entry point (fills `Wave.LeanDurationS` from DPC at SPAWNED).

---

## QA Test Cases

*Test cases not yet defined — run /qa-plan to generate them.*

---

## Test Evidence

**Story Type**: Integration
**Required evidence**: `Source/SLIPSTORM/Tests/Integration/PullWave/PullWaveTickAdvanceTraversingTest.cpp` — must exist and pass

**Status**: [x] Created — `Source/SLIPSTORM/Tests/Integration/PullWave/PullWaveTickAdvanceTraversingTest.cpp` (12 test commands)

---

## Dependencies

- Depends on: Story 001 (structs), Story 002 (pool + WaveId), Story 003 (state machine + TransitionTo)
- Unlocks: Story 005 (LANDED body), Story 006 (DESPAWNING body), Story 007 (pause-flush), Story 008 (run-termination), Story 009 (Construct entry point)

---

## Completion Notes

**Completed**: 2026-08-18
**Criteria**: 12/12 passing
**Deviations**: None blocking. D6 clarification: AdvanceSpawned() does not re-read DPC.telegraph_window_s — LeanDurationS is captured at Wave Spawner admission per ADR-0010 D6. Story Implementation Notes incorrectly described this read; D6 is authoritative. Documented in PullWaveSubsystemActor.cpp with a D6-clarification comment.
**Test Evidence**: Integration test at `Source/SLIPSTORM/Tests/Integration/PullWave/PullWaveTickAdvanceTraversingTest.cpp` (12 test commands covering all ACs)
**Code Review**: Complete — verdict APPROVED (after fixing BLOCKING-1 AC-PW-11 field-freeze assertions, WARNING-2 AddToRoot/RemoveFromRoot GC guards, VERIFY-1 UpdateInstanceTransform explicit bTeleport=false, ADVISORY-2 AC-PW-20(c) tautology replaced with TravelDurationS field assertion)

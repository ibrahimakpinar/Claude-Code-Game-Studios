# Story 005: LANDED Entry + CollisionOutcome + Hit/Near-Miss Broadcasts

> **Epic**: Pull-Wave Behavior
> **Status**: Complete
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: (fill before sprint planning)
> **Manifest Version**: 2026-07-03 (docs/registry/architecture.yaml v8 — no separate control-manifest.md; forbidden patterns cited from registry)
> **Last Updated**: 2026-08-19

## Context

**GDD**: `design/gdd/pull-wave-behavior.md`
**Requirement**: `TR-PW-025`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0010: Pull-Wave Object Pool, State Machine, and Despawn Pipeline
**ADR Decision Summary**: At LANDED entry, Pull-Wave reads `PM.current_lane` via Seam 12 (`IPlayerMovementProvider::GetCurrentLane()`) and determines `CollisionOutcome ∈ {Hit, NearMiss, CleanMiss}`. Hit and NearMiss multicast delegates are broadcast ONCE at LANDED entry. Near-miss additionally calls `PM.TriggerNearMissBeat()` directly. The wave holds at LANDED for `WAVE_DESPAWN_HOLD_S = 0.15s` before transitioning to DESPAWNING. No hit/near-miss events re-fire during the hold. Pull-Wave does NOT call RSM on hit — Collision GDD subscribes to `OnWaveHit` and is responsible for the RSM→DEAD transition.

**Engine**: Unreal Engine 5.7 | **Risk**: LOW
**Engine Notes**: `IPlayerMovementProvider::GetCurrentLane()` (Seam 12) — interface read stable (ADR-0009). Non-dynamic `DECLARE_MULTICAST_DELEGATE_*` — stable pre-UE-4. `PM.TriggerNearMissBeat()` direct call (PM public interface per ADR-0009). No post-cutoff APIs.

**Control Manifest Rules (Feature layer)** — from `docs/registry/architecture.yaml` v8:
- Required: `CollisionOutcome` set exactly ONCE at LANDED entry; never updated during `WAVE_DESPAWN_HOLD_S` hold.
- Required: `OnWaveHit` and `OnNearMiss` broadcast at LANDED ENTRY only — AC-PW-14 explicitly states no re-fires during hold.
- Required: `PM.TriggerNearMissBeat()` called BEFORE broadcasting `OnNearMiss` (call order: TriggerNearMissBeat() → broadcast).
- Forbidden: Pull-Wave calling `RSM->SetState(DEAD)` or equivalent — RSM transition is Collision GDD's responsibility after `OnWaveHit` subscription.
- Forbidden: Pull-Wave subscribing to `PM.OnSlipMidpoint` — direct `PM.current_lane` read at LANDED is the sole near-miss detection mechanism (D5 Rule 11 R1 RC-A).
- Forbidden: Re-reading `PM.current_lane` during the `WAVE_DESPAWN_HOLD_S` hold — the outcome is fixed at LANDED entry.

---

## Acceptance Criteria

*From GDD `design/gdd/pull-wave-behavior.md` §Hit / Near-Miss Resolution, scoped to this story:*

- [ ] **AC-PW-13 (TRAVERSING→LANDED threshold-cross):** Given TRAVERSING wave where single-tick `DeltaTime` overshoot takes `world_z` from `+0.2` to `−0.5`, state → LANDED on that same tick; `collision_outcome` set in that same tick. Threshold-cross detects `world_z <= player_plane_z`; not deferred to next tick.
- [ ] **AC-PW-14 (LANDED→DESPAWNING after hold):** Given wave in LANDED with `WAVE_DESPAWN_HOLD_S=0.15s`, when 0.15s elapses (RUNNING + unpaused, injected DeltaTime via CF-A2), state → DESPAWNING at hold-expiry tick. `OnWaveHit` and `OnNearMiss` do NOT re-fire during the hold.
- [ ] **AC-PW-21a (hit — SETTLED on target lane):** Given wave with `target_lane=2` in LANDED, `FPlayerMovementTestStub` SETTLED + `current_lane=2`: `OnWaveHit(wave_id, target_lane=2, ...)` published; `OnNearMiss` NOT published. `FWaveSpawnerCallbackTestStub.GetWaveHitCount()==1` AND `GetNearMissCount()==0`.
- [ ] **AC-PW-21b (hit — SLIPPING toward target lane):** Given wave `target_lane=2`, stub SLIPPING + `target_lane=2`: `OnWaveHit` published; `OnNearMiss` NOT. Hit count 1, near-miss count 0.
- [ ] **AC-PW-21c (clean miss):** Given wave `target_lane=2`, stub SETTLED + `current_lane=4`: neither `OnWaveHit` nor `OnNearMiss` fires. `CollisionOutcome==CleanMiss`. Hit count 0, near-miss count 0.
- [ ] **AC-PW-22 (near-miss — direct read of `current_lane`):** Given wave `target_lane=3`, stub SLIPPING + `current_lane=3` + `target_lane=4` (player mid-slip from lane 3→4; `current_lane` returns SOURCE per PM Rules 4+7): `OnNearMiss(wave_id, target_lane=3, slipped_from_lane=3)` published AND `FPlayerMovementTestStub.GetNearMissBeatCount()==1`. `OnWaveHit` NOT published. No `PM.OnSlipMidpoint` subscription.
- [ ] **AC-PW-28 (EC-CONCURRENT-LANDING-SAME-LANE):** Two waves with identical `target_lane=2`, identical velocities, same-tick LANDED, stub SETTLED + `current_lane=2`: both independently publish `OnWaveHit` (two separate events, same tick, distinct `wave_id`). `FWaveSpawnerCallbackTestStub.GetWaveHitCount()==2`. No deduplication.
- [ ] **AC-PW-31 (EC-LANDING-OVERSHOOT-FORWARD):** TRAVERSING wave at `world_z=+0.3` at tick N; simulated hitch jumps `world_z` to `−0.8` at tick N+1: LANDED triggers at tick N+1 (not deferred); `collision_outcome` evaluated at tick-N+1 position.
- [ ] `OnLeanProgress` delegate is NOT fired during LANDED state (LEANING is over; no lean progress during hold).

---

## Implementation Notes

*Derived from ADR-0010 D2 + D5 Implementation Guidelines:*

**LANDED entry body (fires once, at the tick where `world_z <= player_plane_z`):**
1. `Wave.TraverseElapsedS = FMath::Min(Wave.TraverseElapsedS, Wave.TravelDurationS)` — clamp accumulator.
2. Read `int32 CurrentLane = PM.GetCurrentLane()` via Seam 12.
3. Read `ERunSlipState MovementState = PM.GetMovementState()` via Seam 12.
4. Determine `CollisionOutcome`:
   - **Hit**: `(SETTLED AND CurrentLane == Wave.TargetLane)` OR `(SLIPPING AND PM.GetTargetLane() == Wave.TargetLane)`.
   - **NearMiss**: `SLIPPING AND CurrentLane == Wave.TargetLane` (PM.current_lane returns SOURCE lane throughout SLIPPING per PM Rules 4+7).
   - **CleanMiss**: all other cases.
5. `Wave.CollisionOutcome = CollisionOutcome`.
6. If Hit: broadcast `OnWaveHit.Broadcast(Wave.WaveId, Wave.TargetLane, Wave.SourceLane, Wave.SpawnTimeS)`.
7. If NearMiss: call `PM.TriggerNearMissBeat()` FIRST; then broadcast `OnNearMiss.Broadcast(Wave.WaveId, Wave.TargetLane, CurrentLane)`.

**LANDED hold tick body (subsequent ticks during hold):**
- `Wave.LandedHoldElapsedS += DeltaTime` (only if `!RSM.GetIsPaused()` — pause-freeze applies here too per Story 004's pause gate).
- When `Wave.LandedHoldElapsedS >= WAVE_DESPAWN_HOLD_S`: `TransitionTo(Wave, DESPAWNING)`.
- No hit/near-miss re-broadcasts during hold.

**`OnWaveHit` and `OnNearMiss` delegate declarations:**
- `DECLARE_MULTICAST_DELEGATE_FourParams(FOnWaveHit, int32 /*WaveId*/, int32 /*TargetLane*/, int32 /*SourceLane*/, float /*SpawnTimeS*/)` — non-dynamic.
- `DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnNearMiss, int32 /*WaveId*/, int32 /*TargetLane*/, int32 /*SlippedFromLane*/)` — non-dynamic.

**Seam 12 (`IPlayerMovementProvider`):** Required methods for this story — `GetCurrentLane()`, `GetTargetLane()`, `GetMovementState()`, `TriggerNearMissBeat()`. The stub (`FPlayerMovementTestStub`) provides `SetCurrentLane()`, `SetTargetLane()`, `SetMovementState()`, `GetNearMissBeatCount()`.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 004: TRAVERSING→LANDED transition trigger (threshold-cross detection; `world_z <= player_plane_z`).
- Story 006: DESPAWNING entry and six-step despawn pipeline (fires after LANDED hold expires).
- Story 007: Pause-flush path (waves in LANDED are included in flush batch; handled there).
- Story 008: Run-termination drain (waves in LANDED continue through; LANDED events still fire).

---

## QA Test Cases

*Test cases not yet defined — run /qa-plan to generate them.*

---

## Test Evidence

**Story Type**: Integration
**Required evidence**: `Source/SLIPSTORM/Tests/Integration/PullWave/PullWaveLandedCollisionOutcomeTest.cpp` — must exist and pass
*(Note: story originally listed `tests/integration/pull-wave/landed-collision-outcome_test.cpp` — corrected to actual UE project path per Story 001/004 precedent)*

**Status**: [x] Created — `Source/SLIPSTORM/Tests/Integration/PullWave/PullWaveLandedCollisionOutcomeTest.cpp` (9 test commands)

---

## Dependencies

- Depends on: Story 004 (LANDED state reached via tick advance; `TransitionTo(LANDED)` already in Story 004's TRAVERSING body)
- Unlocks: Story 006 (despawn pipeline fires after LANDED→DESPAWNING transition), Story 007 (pause-flush includes LANDED waves), Story 008 (run-termination includes LANDED events)

## Completion Notes
**Completed**: 2026-08-19
**Criteria**: 9/9 passing
**Deviations**: None — all ADR-0010 control manifest rules satisfied; no forbidden patterns present
**Test Evidence**: Integration — `Source/SLIPSTORM/Tests/Integration/PullWave/PullWaveLandedCollisionOutcomeTest.cpp` (9 test commands, 9/9 AC covered)
**Code Review**: Complete — APPROVED after W1/W2 fix pass (ensure guards + teardown contract docs added 2026-08-19)
**Advisory test gaps (not blocking)**:
  - GAP-1: No TC for null PMProvider path (→ CleanMiss default)
  - GAP-2: No TC for pause-freeze during LANDED hold (SetRSMProvider unused in all 9 TCs)
  - GAP-3: No TC verifying CollisionOutcome immutability across hold ticks

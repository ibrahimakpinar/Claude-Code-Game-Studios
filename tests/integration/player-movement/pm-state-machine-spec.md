# PM State Machine Integration Test Spec

**Story**: `production/epics/player-movement/story-003-state-machine-tick-body.md`
**C++ file**: `Source/SLIPSTORM/Tests/Integration/PlayerMovement/PMStateMachineTest.cpp`
**Category**: `SLIPSTORM.PlayerMovement.StateMachine`
**Runner**: UE Automation Framework, headless (`-nullrhi -nosound -unattended`)
**Flags**: `ClientContext | ProductFilter`

## Test Cases

### TC1 — ac01_baseline_slip
**AC**: AC-01 (baseline slip)
**Arrange**: PM at Center SETTLED; RSM not required (direct helper path).
**Act**: `IsSlipValidFromLane(Center, Right, out)` → `CompleteTween()`.
**Assert**: `current_lane == Right` post-CompleteTween; `slip_complete_count == 1`; `tween_progress == 1.0f`; `movement_state == SETTLED`.
**Sub-cases**: (a) Center+Left→Left; (b) Left+Right→Center; (c) Right+Right→FarRight; edge: FarRight+Right returns false with OutTargetLane unchanged; FarLeft+Left returns false with OutTargetLane unchanged.

### TC2 — ac08_non_running_discard
**AC**: AC-08 (non-RUNNING discard)
**Arrange**: PM at Center SETTLED in a test world; RSM stub returns IDLE.
**Act**: `HandleSlipTransition(Right)`.
**Assert**: `movement_state == SETTLED`; `current_lane == Center`; `tween_progress == 0.0f`; `target_lane == Center` (no mutation).

### TC3 — ac09_paused_discard
**AC**: AC-09 (paused discard — null-RSM path for stub)
**Arrange**: PM via NewObject (no world); RSMSubsystem == null.
**Act**: `HandleSlipTransition(Right)`.
**Assert**: `movement_state == SETTLED`; `current_lane == Center`; `tween_progress == 0.0f`.
**Note**: Full paused-path (IsPaused==true) deferred to Story 009 test suite.

### TC4 — ac10_grace_discard
**AC**: AC-10 (resume-grace discard — null-RSM path for stub)
**Arrange**: PM via NewObject; RSMSubsystem == null.
**Act**: `HandleSlipTransition(Right)`.
**Assert**: `movement_state == SETTLED`; `current_lane == Left` (unchanged).
**Note**: Full grace-path (IsResumeGrace==true) deferred to Story 009 test suite.

### TC5 — ac23_settled_to_slipping_sync
**AC**: AC-23 (SETTLED→SLIPPING synchronous)
**Arrange**: Pawn in test world at Center SETTLED. Transition fields applied manually (bypassing Rule 5 gate).
**Act**: `IsSlipValidFromLane(Center, Right, out)` → `SetActorLocation(+100, 0, 0, bSweep=false)` → set fields.
**Assert** (all synchronous, same event call):
  - (a) pawn root X == +100.0f (LaneWorldX(Right))
  - (b) `movement_state == SLIPPING`
  - (c) `target_lane == Right`
  - (d) `tween_progress == 0.0f`
  - (e) `current_lane == Center` (source-lane semantic — NOT target)

### TC6 — ac24_midpoint_broadcast
**AC**: AC-24 (OnSlipMidpoint broadcast)
**Arrange**: PM via NewObject; SLIPPING Left→Right; `OnSlipMidpoint` subscriber lambda counts deliveries.
**Act**: `AdvanceTweenProgress` to cross TP=0.5; simulate midpoint check inline.
**Assert**:
  - Positive path: broadcast fires exactly once with (Center, Right).
  - Edge (a): TP = 0.5 exactly — fires.
  - Edge (b): TP 0.4→0.6 in one hitch — fires exactly once.
  - Edge (c): prev_tp already >= 0.5 — does NOT fire.

### TC7 — ac34_source_lane_semantic
**AC**: AC-34 (source-lane semantic during SLIPPING)
**Arrange**: PM via NewObject; Left→Center tween.
**Act**: Sample `current_lane` at TP ∈ {0.0, 0.1, 0.5, 0.99}; then call `CompleteTween()`.
**Assert**: `current_lane == Left` at every sample; `current_lane == Center` exactly after CompleteTween; `movement_state == SETTLED`.

### TC8 — force_tick_now_prologue
**AC**: ForceTickNow prologue invariant (ADR-0009 IG-1)
**Arrange**: Pawn in test world; `RSM->TestOnly_ForceTickNowCallCount` reset to 0.
**Act**: Call `RSM->ForceTickNow()` directly; then call `PM->TickComponent(...)`.
**Assert**:
  - Counter increments on explicit `ForceTickNow()` call.
  - Counter >= 1 after `TickComponent` (confirming it was called before gate reads).

## Out of Scope

- AC-34b (buffer-flush source-lane): deferred to Story 005 per JC-4.
- Paused-path / grace-path with stub injected: deferred to Story 009.
- COUNTDOWN/DEAD/COMPLETE/ABORTED RSM-state paths: deferred to Story 008.
- F-6 edge-absorb TriggerEdgeAbsorb behavior: deferred to Story 007.
- TriggerCommitmentTell counter: deferred to Story 010.

## Grep Gate

`PlayerMovement_TickComponent_without_prior_ForceTickNow` grep on `Source/SLIPSTORM/**/PlayerMovement*.cpp` must return 0 hits.

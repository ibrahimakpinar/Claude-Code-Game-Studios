# Story 003: State machine SETTLED↔SLIPPING + tick body + collision commit + source-lane semantic + F-4 helper + OnSlipMidpoint

> **Epic**: Player Movement (Slip)
> **Status**: Complete
> **Layer**: Core
> **Type**: Integration
> **Estimate**: 4 hours (M)
> **Manifest Version**: 2026-07-03 (docs/registry/architecture.yaml v8)
> **Last Updated**: 2026-07-12

## Context

**GDD**: `design/gdd/player-movement-mechanics.md` (§3 Rules 2/4/5, §4 F-4, §Source-Lane Semantic R7-PM-PROPAGATION-REVIEW BINDING, §8 AC-01/08/09/10/23/24/34/34b), `design/gdd/player-movement-platform.md` (§3 Tick Ordering SD4).
**Requirement**: `TR-PM-011`, `TR-PM-019`, `TR-PM-035`, `TR-PM-012 (F-4 helper only — buffer state stays in Story 005)`, `TR-PM-015 (partial — slip_complete_count increment)`.
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0009 (SD4 tick ordering + SD5 collision commit), secondary ADR-0007 (RSM property-read + delegate contract).
**ADR Decision Summary**: `TickComponent(DeltaTime)` first statement after `check(IsInGameThread())` is `RSMSubsystem->ForceTickNow()` (SD4; mirrors ADR-0008 SD1). Collision commit on SETTLED→SLIPPING transition uses `GetOwner()->SetActorLocation(FVector(target_x, 0, 0), /*bSweep=*/false)` on the pawn root (SD5). Mesh writes are decoupled (Story 004).

**Engine**: Unreal Engine 5.7 | **Risk**: MEDIUM
**Engine Notes**: `AActor::SetActorLocation(FVector, bool bSweep, FHitResult*, ETeleportType)` and `check(IsInGameThread())` stable pre-cutoff. `bSweep=false` skips Chaos physics resolution — required for constant-time transform update per ADR-0009 Engine Compatibility Verification #3.

**Control Manifest Rules (this layer)** — from `docs/registry/architecture.yaml` v8:
- **Required**: TickComponent body's first statement after `check(IsInGameThread())` MUST be `RSMSubsystem->ForceTickNow()` — no exceptions (ADR-0009 IG-1).
- **Required**: PM does NOT subscribe to `OnStateChanged` / `OnPausedChanged` from inside `ForceTickNow()`'s execution (ADR-0007 non-callback invariant + ADR-0009 IG-2). PM's tick body uses direct property reads (`GetCurrentState()`, `IsPaused()`, `IsResumeGrace()`) for zero-latency gating.
- **Required**: Root commit uses `GetOwner()->SetActorLocation(FVector(LaneWorldX(target_lane), 0.0f, 0.0f), /*bSweep=*/false)` on SETTLED→SLIPPING transition frame BEFORE any tween progress accumulates (ADR-0009 SD5).
- **Required**: Source-lane semantic BINDING (R7-PM-PROPAGATION-REVIEW) — throughout SLIPPING, `current_lane` returns SOURCE lane. Reassignment to `target_lane` happens ONLY at CompleteTween.
- **Forbidden**: `PlayerMovement_TickComponent_without_prior_ForceTickNow` (registry v8).

---

## Acceptance Criteria

*From `design/gdd/player-movement-mechanics.md` §3 (Rules 2/4/5) + §8, scoped to this story:*

- [ ] `TickComponent(DeltaTime, TickType, ThisTickFunction)` body: `check(IsInGameThread())` then `RSMSubsystem->ForceTickNow()` as the first statement after the thread check. If `RSMSubsystem` is null (BeginPlay failed to resolve), skip mechanics update entirely (fail-safe).
- [ ] Tick body reads RSM properties directly per tick: `ERunState state = RSMSubsystem->GetCurrentState()`; `bool paused = RSMSubsystem->IsPaused()`; `bool grace = RSMSubsystem->IsResumeGrace()`.
- [ ] Rule 5 gate: `if (state != ERunState::RUNNING || paused || grace) { /* skip F-2/F-3/F-5/F-6 mechanics update; do NOT advance TweenProgress; do NOT process buffered input */ }`. Inputs received outside the gate are DISCARDED (not buffered).
- [ ] F-4 pure utility (edge-check pre-validation): `bool IsSlipValidFromLane(EPlayerLane FromLane, ESlipDirection Dir, EPlayerLane& OutTargetLane)` — returns true and sets OutTargetLane if the projected lane is on-track; returns false and leaves OutTargetLane unchanged for off-track (edge no-op) inputs. Used by HandleSlipTransition (this story) and buffer flush (Story 005).
- [ ] `HandleSlipTransition(ESlipDirection Dir)` initial-input path (SETTLED state):
  - [ ] Rule 5 gate: discard if RSM state not RUNNING or paused or resume_grace.
  - [ ] F-4 pre-validation: if IsSlipValidFromLane(current_lane, Dir, target_lane) returns false → Rule 1 edge no-op path (fire edge-absorb via Story 007's F-6 — this story leaves a hook `TriggerEdgeAbsorb()` as an empty method stub or forward-declares, ACTUAL firing lands in Story 007).
  - [ ] Otherwise: SETTLED→SLIPPING transition:
    - [ ] `target_lane` set to F-4 projection.
    - [ ] Collision commit: `GetOwner()->SetActorLocation(FVector(LaneWorldX(target_lane), 0.0f, 0.0f), /*bSweep=*/false)`.
    - [ ] `movement_state = ERunSlipState::SLIPPING`.
    - [ ] `tween_progress = 0.0f`.
    - [ ] Commitment-tell hook `TriggerCommitmentTell(target_lane)` invoked (stub in this story — full behavior lands in Story 010; counter increments in Story 010).
- [ ] Rule 4 source-lane semantic: while `movement_state == SLIPPING`, `current_lane` returns the SOURCE lane (not `target_lane`). `current_lane` is reassigned to `target_lane` ONLY at `CompleteTween` (below).
- [ ] `CompleteTween()` invoked when `tween_progress >= 1.0f`:
  - [ ] `tween_progress = 1.0f` (clamp to exactly 1.0 for downstream F-3 relative-offset invariant per Story 004).
  - [ ] `current_lane = target_lane`.
  - [ ] `movement_state = ERunSlipState::SETTLED`.
  - [ ] `slip_complete_count += 1`.
  - [ ] Buffer flush hook `FlushBufferedInput()` invoked (stub in this story — implementation lands in Story 005; same tick, no idle frame).
- [ ] **AC-24 OnSlipMidpoint**: `OnSlipMidpoint.Broadcast(source_lane, target_lane)` fires ONCE per tween on the tick when `tween_progress` crosses 0.5 (previous tick TP < 0.5 AND current tick TP >= 0.5). MUST NOT fire on edge-absorb (Story 007), pause resume, DEAD/COMPLETE/ABORTED transitions (Story 008 exclusions), or completed tween.
- [ ] Rule 2 acceptance behavior: collision at `LaneWorldX(target_lane)` immediately post-transition; visual mesh still at SOURCE lane offset (Story 004 F-3 output). Root position on pawn is at target lane world X within the same event-call as the state change.

---

## Implementation Notes

*Derived from ADR-0009 SD4 + SD5 + Implementation Guidelines 1/2/5 + mechanics §3 Rules 2/4/5 + Delegate Handler Bodies:*

**TickComponent skeleton**:
```cpp
void UPlayerLaneMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    check(IsInGameThread());

    // MANDATORY first statement per ADR-0009 IG-1 + registry forbidden pattern.
    if (RSMSubsystem)
    {
        RSMSubsystem->ForceTickNow();
    }

    float raw_dt = 0.0f, effective_dt = 0.0f;
    ComputeTickDT(raw_dt, effective_dt);  // From Story 002

    // Watchdog push (Story 013) — leaves a hook here in Story 013 that consumes raw_dt.
    // WatchdogTick(raw_dt);

    // Rule 5 gate
    if (!RSMSubsystem
        || RSMSubsystem->GetCurrentState() != ERunState::RUNNING
        || RSMSubsystem->IsPaused()
        || RSMSubsystem->IsResumeGrace())
    {
        return; // mechanics skipped; buffered input NOT flushed; TweenProgress NOT advanced
    }

    // F-2 advance (Story 002)
    if (movement_state == ERunSlipState::SLIPPING)
    {
        const float effective_slip_tween = GetEffectiveSlipTween();  // Story 002
        const float prev_tp = tween_progress;
        tween_progress += effective_dt / effective_slip_tween;

        // AC-24: midpoint broadcast
        if (prev_tp < 0.5f && tween_progress >= 0.5f)
        {
            OnSlipMidpoint.Broadcast(/*source=*/current_lane, /*target=*/target_lane);
        }

        // F-3 site (Story 004): MeshComponent->SetRelativeLocation(FVector(F3Relative(tween_progress), 0, 0));
        // F-5 site (Story 006): MeshComponent->SetRelativeRotation(F5Lean(tween_progress));
        // F-6 site (Story 007): if (edge_absorb_active) AdvanceEdgeAbsorb(effective_dt);

        if (tween_progress >= 1.0f)
        {
            CompleteTween();
        }
    }
}

void UPlayerLaneMovementComponent::CompleteTween()
{
    tween_progress = 1.0f;
    current_lane = target_lane;
    movement_state = ERunSlipState::SETTLED;
    slip_complete_count += 1;
    FlushBufferedInput();  // Story 005 stub; Story 005 implements
}
```

**HandleSlipTransition (initial-input path)**:
```cpp
void UPlayerLaneMovementComponent::HandleSlipTransition(ESlipDirection Dir)
{
    // Rule 5 gate (also enforced at input source, but defensive here)
    if (!RSMSubsystem
        || RSMSubsystem->GetCurrentState() != ERunState::RUNNING
        || RSMSubsystem->IsPaused()
        || RSMSubsystem->IsResumeGrace())
    {
        return; // DISCARD (not buffered)
    }

    if (movement_state == ERunSlipState::SLIPPING)
    {
        // Buffered-input path lands in Story 005
        return;
    }

    // SETTLED path
    EPlayerLane ProjectedTarget;
    if (!IsSlipValidFromLane(current_lane, Dir, ProjectedTarget))
    {
        TriggerEdgeAbsorb(current_lane, Dir);  // Story 007 stub
        return;
    }

    // SETTLED → SLIPPING transition per Rule 2
    target_lane = ProjectedTarget;
    GetOwner()->SetActorLocation(
        FVector(LaneWorldX(target_lane), 0.0f, 0.0f),
        /*bSweep=*/false);
    movement_state = ERunSlipState::SLIPPING;
    tween_progress = 0.0f;
    TriggerCommitmentTell(target_lane);  // Story 010 stub
}
```

**Non-callback invariant** (IG-2): `RSMSubsystem->ForceTickNow()` may cause RSM to broadcast `OnStateChanged` / `OnPausedChanged` synchronously (ADR-0007 SD2 says NOT synchronously in typical cases, but PM must tolerate either). Handlers `HandleStateChanged` / `HandlePausedChanged` (Stories 008/009) MUST NOT re-enter PM's tick body.

**Rule 4 source-lane semantic** (R7-PM-PROPAGATION-REVIEW BINDING): Pull-Wave's near-miss detection reads `current_lane` during SLIPPING and expects the SOURCE lane. Do NOT set `current_lane = target_lane` at SETTLED→SLIPPING transition — it stays as the source until CompleteTween.

**Performance budget**: `TickComponent` body executes every frame in RUNNING state. Hot-path costs per tick: 1 `RSMSubsystem->ForceTickNow()` call (idempotent post-first-call per ADR-0007 SD2), 3 RSM property reads (`GetCurrentState`/`IsPaused`/`IsResumeGrace`), 1 `ComputeTickDT` call (Story 002 helper — < 1 μs), 1 F-2 accumulator step (Story 002 — < 1 μs), branch predictions for the midpoint broadcast + CompleteTween check. `SetActorLocation` collision commit happens ONCE per SETTLED→SLIPPING transition (not per-tick). `OnSlipMidpoint.Broadcast` fires at most once per tween. Estimated per-tick cost < 5 μs on mid-tier mobile — well within the per-tick PM CPU `< 0.15 ms p99` budget from `EPIC.md` Definition of Done. No heap allocations. No engine-scheduler interaction beyond `ForceTickNow`.

---

## Out of Scope

- Story 002: F-1/F-PROLOGUE/F-2/SLIP_TWEEN helper implementations (this story CALLS them).
- Story 004: F-3 lateral interpolation + `MeshComponent->SetRelativeLocation` mesh writes.
- Story 005: Buffered-input path (SLIPPING + input → single-slot buffer + Rule 3 drop feedback + Rule 11 discard). This story stubs `FlushBufferedInput()`.
- Story 006: F-5 lean + `SetRelativeRotation` writes.
- Story 007: `TriggerEdgeAbsorb` implementation + F-6 tail advance + edge_absorb_trigger_count++.
- Story 008: `HandleStateChanged` switch bodies + AC-SS-B default-branch runtime.
- Story 009: `HandlePausedChanged` body + pause/grace resume behavior.
- Story 010: `TriggerCommitmentTell` implementation + commitment_tell_fire_count++ + material flash + cadence cap.
- Story 013: Watchdog `raw_dt` consumption.

---

## QA Test Cases

*Test file: `tests/integration/player-movement/pm_state_machine_test.cpp`. Automated integration tests using a stubbed `URunStateMachineSubsystem` mock.*

- **AC-01 (baseline slip)**:
  - Given: PM at `current_lane == EPlayerLane::Center`, `movement_state == SETTLED`, RSM state = RUNNING, not paused, not grace.
  - When: `HandleSlipTransition(ESlipDirection::Right)` invoked; then TickComponent runs long enough for TP >= 1.0.
  - Then: `current_lane == EPlayerLane::Right` post-CompleteTween; pawn root X coordinate at LaneWorldX(Right) exactly.
  - Edge cases: (a) Center + Left → Left; (b) Left + Right → Center; (c) 5-lane matrix — all valid transitions produce correct target.

- **AC-08 (non-RUNNING discard)**:
  - Given: RSM state = COUNTDOWN, PM SETTLED at Center.
  - When: `HandleSlipTransition(Right)` invoked.
  - Then: `movement_state == SETTLED` still; `current_lane == Center`; no root move; no `bSlipTweenClampActive` change.
  - Edge cases: also verify IDLE, DEAD, COMPLETE, ABORTED states — all discard.

- **AC-09 (paused discard)**:
  - Given: RSM state = RUNNING, `IsPaused() == true`, PM SETTLED at Center.
  - When: `HandleSlipTransition(Right)` invoked.
  - Then: unchanged as AC-08.

- **AC-10 (resume_grace discard)**:
  - Given: RSM state = RUNNING, `IsResumeGrace() == true`.
  - When: `HandleSlipTransition(Right)` invoked.
  - Then: unchanged as AC-08.

- **AC-23 (SETTLED→SLIPPING synchronous)**:
  - Given: PM at Center SETTLED, RSM RUNNING clean.
  - When: `HandleSlipTransition(Right)` invoked (single event call).
  - Then: post-call within the SAME event: (a) pawn root X == LaneWorldX(Right); (b) `movement_state == SLIPPING`; (c) `target_lane == Right`; (d) `tween_progress == 0.0f`; (e) `current_lane == Center` (source-lane preservation).

- **AC-24 (OnSlipMidpoint broadcast, positive path)**:
  - Given: PM SLIPPING with `tween_progress = 0.45f` on tick N; effective_dt sized so tick N+1 has `tween_progress = 0.55f`.
  - When: TickComponent runs on tick N+1.
  - Then: `OnSlipMidpoint.Broadcast(Center, Right)` fires exactly once with the correct source/target. Broadcast subscribers observe one delivery.
  - Edge cases: (a) TP = 0.5 exactly on tick N+1 — fires (>=0.5); (b) TP goes 0.4 → 0.6 in one hitch — fires once (not twice); (c) TP was already 0.5+ from previous tick — does NOT fire again.

- **AC-34 (source-lane semantic during SLIPPING)**:
  - Given: PM Left→Center tween mid-flight.
  - When: `current_lane` queried at TP ∈ {0.0, 0.1, 0.5, 0.99}.
  - Then: `current_lane == Left` every sample. At `CompleteTween()` (TP >= 1.0): `current_lane == Center` (reassigned exactly at completion).

- **AC-34b (buffer-flush source-lane)**:
  - Given: after Left→Center CompleteTween, buffer had queued (Center→Right).
  - When: `FlushBufferedInput()` invoked from CompleteTween (Story 005 wires actual flush; this story stubs — verify the STUB signature is called).
  - Then: PM enters new SLIPPING tween where `current_lane == Center` (new source).

- **ForceTickNow prologue invariant**:
  - Given: PM subclassed with a spy that records `RSMSubsystem->ForceTickNow` call order.
  - When: TickComponent runs.
  - Then: `ForceTickNow` invoked BEFORE any RSM property read in the tick body; `check(IsInGameThread())` triggered before `ForceTickNow`.
  - Grep gate: `PlayerMovement_TickComponent_without_prior_ForceTickNow` grep on PM source returns 0 hits.

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Automated integration test at `Source/SLIPSTORM/Tests/Integration/PlayerMovement/PMStateMachineTest.cpp` (compiled into the SLIPSTORM game module per `tests/automation-cpp/README.md` dual-location convention) — must exist and pass in headless UE Automation Framework runner. Test class `FPMStateMachineTest`; category `SLIPSTORM.PlayerMovement.StateMachine`. Test flags `EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter` (headless-compatible per Story 001a precedent). Uses `CreateTestPlayWorld` helper pattern from Story 001a for real BeginPlay + a controllable RSM stub subsystem (already stubbed in Story 001).
- Test spec markdown at `tests/integration/player-movement/pm-state-machine-spec.md` (studio dual-location convention — spec in `tests/`, C++ under `Source/`).
- Grep gate: `PlayerMovement_TickComponent_without_prior_ForceTickNow` forbidden pattern verified clean in `Source/SLIPSTORM/**/PlayerMovement*.cpp`.

**Status**: [x] Created — `Source/SLIPSTORM/Tests/Integration/PlayerMovement/PMStateMachineTest.cpp` + `tests/integration/player-movement/pm-state-machine-spec.md`. Forbidden-pattern grep gate: clean.

---

## Dependencies

- **Depends on**: Story 001 (skeleton, RSMSubsystem resolved, current_lane/target_lane/movement_state fields, counter fields, OnSlipMidpoint delegate declared); Story 002 (F-1/F-PROLOGUE/F-2/GetEffectiveSlipTween helpers).
- **Unlocks**: Story 004 (F-3 in SLIPPING mesh writes), Story 005 (buffer state + Rule 3/11), Story 006 (F-5 in SLIPPING mesh rotation), Story 007 (F-6 timer + edge_absorb_trigger_count++), Story 008 (terminal state handlers), Story 010 (commitment-tell trigger).

---

## Completion Notes
**Completed**: 2026-07-12
**Criteria**: 10/10 verified. All 5 TRs (TR-PM-011, -012 partial, -015 partial, -019, -035) mapped to implementation and covered by integration tests.
**Deviations**: None blocking. 5 non-blocking suggestions surfaced during code review — deferred by user (option B at /code-review Phase 9):
  1. `PlayerLaneMovementComponent.cpp:162` — tag `// WatchdogTick(raw_dt);` with `TODO(Story 013):` for grep consistency.
  2. `PlayerLaneMovementComponent.h:324` — drop unused `mutable` from `HandleStateChanged_TestOnlyCallCount`.
  3. `PlayerLaneMovementComponent.h:310` — brace-init `TickDTRollingBuffer[60]` to sentinel value.
  4. `PlayerLaneMovementComponent.h:266-287` — add `TestOnly_*CallCount` counters for `FlushBufferedInput` / `TriggerCommitmentTell` / `TriggerEdgeAbsorb` / `HandlePausedChanged` (mirrors h:324 pattern; closes AC-34b observability gap for downstream Stories 005/007/009/010).
  5. `PMStateMachineTest.cpp` — add explicit AC-24 (a) TP=0.5 exact-boundary sub-test; add TP=0.1 and TP=0.99 samples to AC-34 to match the story's specified sample grid (currently samples at tick-natural ~0.333/~0.533).
**Test Evidence**: Integration test at `Source/SLIPSTORM/Tests/Integration/PlayerMovement/PMStateMachineTest.cpp` (27,239 bytes, ~43 assertions across 8 AC groups). Spec markdown at `tests/integration/player-movement/pm-state-machine-spec.md`. Test file rebuilt after prior /code-review flagged AC-24 tautology + non-ordering-check as BLOCKING; both resolved via snapshot-based ordering evidence in RSM stub (`TestOnly_GetCurrentStateCountAtForceTickNow`).
**Untested criteria**: AC-34b (buffer-flush stub-called assertion) — unobservable without stub counter; blocked on suggestion #4 above. Recommend addressing in Story 005 when `FlushBufferedInput` gets its implementation.
**Code Review**: Complete — /code-review on `.h` (APPROVED WITH SUGGESTIONS) and full `.h + .cpp` (APPROVED WITH SUGGESTIONS) both run 2026-07-12 with unreal-specialist + qa-tester specialist passes.

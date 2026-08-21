# Story 009: HandlePausedChanged + Rule 6 pause/resume/grace freeze + buffer preservation

> **Epic**: Player Movement (Slip)
> **Status**: Complete
> **Layer**: Core
> **Type**: Integration
> **Estimate**: 3–4 hours
> **Manifest Version**: 2026-07-03 (docs/registry/architecture.yaml v8)
> **Last Updated**: 2026-08-02

## Context

**GDD**: `design/gdd/player-movement-mechanics.md` (§3 Rules 5/6, Delegate Handler Bodies HandlePausedChanged, §8 AC-11/12/13, AC-COUNTER-PAUSE-RESUME).
**Requirement**: `TR-PM-011` (pause branch of Rule 5).
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0009 (SD4 per-tick RSM property reads gate F-2 advance), secondary ADR-0007 (`OnPausedChanged` delegate contract).
**ADR Decision Summary**: F-2 freezes during pause because Story 003's tick body Rule 5 gate skips mechanics when `IsPaused() == true` OR `IsResumeGrace() == true`. Buffer is PRESERVED across pause (unlike terminal transitions). `HandlePausedChanged(bool bNewPaused)` body is logging-only per mechanics §3 — no state mutation (the freeze is the tick-body gate's responsibility).

**Engine**: Unreal Engine 5.7 | **Risk**: LOW.
**Engine Notes**: None.

**Control Manifest Rules (this layer)** — from `docs/registry/architecture.yaml` v8:
- **Required**: Buffer PRESERVED across pause (do NOT invoke `DiscardBuffer()` from `HandlePausedChanged` — this differs from terminal transitions in Story 008).
- **Required**: F-2 freeze is realized by Story 003's Rule 5 gate — this story verifies the gate correctly handles pause + resume_grace as freeze conditions; it does NOT re-implement the gate.
- **Required**: On grace expiry (resume_grace transition true→false, RUNNING with pause == false), the tick body naturally resumes F-2 advance AND `FlushBufferedInput()` (Story 005 hook — flushes at CompleteTween, but if buffer was queued mid-tween-mid-pause, it flushes at grace-expiry-frame's next CompleteTween). AC-13 specifies: grace expiry during pause+buffer → tween resumes, buffered slip fires SAME TICK. Verify the state machine achieves this without special-casing.

---

## Acceptance Criteria

*From `design/gdd/player-movement-mechanics.md` §3 Rule 6 + Delegate Handler Bodies HandlePausedChanged + §8, scoped to this story:*

- [ ] `HandlePausedChanged(bool bNewPaused)` body: logging-only per mechanics §3 pseudo-code (lines 311–326). Log `UE_LOG(LogPlayerMovement, Verbose, TEXT("PM: paused=%d"), bNewPaused ? 1 : 0)`. No state mutation.
- [ ] Confirm Story 003's Rule 5 gate correctly interprets `IsPaused() == true` OR `IsResumeGrace() == true` as freeze conditions. This story is primarily a verification pass on that gate.
- [ ] **AC-11 (mid-tween pause freezes TweenProgress)**: SLIPPING at TP=0.4; RSM broadcasts pause=true; ticks advance; TP remains 0.4.
- [ ] **AC-12 (resume_grace freezes TweenProgress)**: SLIPPING at TP=0.4; RSM state transitions to RUNNING but `IsResumeGrace() == true`; ticks advance; TP remains 0.4.
- [ ] **AC-13 (grace expiry with buffer flush)**: SLIPPING at TP=0.7 → pause → buffer queued (via HandleSlipTransition, which — during pause — Rule 5 discards! see edge case) → resume_grace → grace expires → tween resumes from TP=0.7 → CompleteTween → buffered slip fires SAME TICK.
- [ ] **AC-COUNTER-PAUSE-RESUME**: counters preserved across pause/grace; resume-completion increments `slip_complete_count` on the flushed tween.

**Edge case call-out for AC-13**: Rule 5 discards inputs during pause. So a slip attempted DURING pause would not queue. AC-13's semantic must be reinterpreted from the spec: buffer was queued PRIOR to pause; pause holds; grace expires; F-2 resumes; CompleteTween fires; buffered slip flushes.

- [ ] **AC-24 exclusion (OnSlipMidpoint deferred past pause window)**: if TP would cross 0.5 during a pause frame, no broadcast (gate skips). Broadcast fires on the tick AFTER resume when TP crosses 0.5.

---

## Implementation Notes

*Derived from mechanics §3 Delegate Handler Bodies + Rules 5/6:*

**HandlePausedChanged body** (logging-only):
```cpp
void UPlayerLaneMovementComponent::HandlePausedChanged(bool bNewPaused)
{
    UE_LOG(LogPlayerMovement, Verbose, TEXT("HandlePausedChanged(bNewPaused=%d)"),
           bNewPaused ? 1 : 0);
    // Intentional no-op: pause freeze is enforced by TickComponent's Rule 5 gate
    // reading RSMSubsystem->IsPaused() per tick. Do NOT DiscardBuffer here — Rule 6
    // preserves the buffer across pause.
}
```

**Rule 5 gate verification** (Story 003 site):
```cpp
if (RSMSubsystem->GetCurrentState() != ERSMState::RUNNING
    || RSMSubsystem->IsPaused()
    || RSMSubsystem->IsResumeGrace())
{
    return; // TP not advanced, buffer preserved, edge-absorb timer NOT advanced
}
```

**AC-13 mechanism** (Rule 6 + Rule 3 buffer flush from Story 005): the sequence is:
1. Pre-pause: PM SLIPPING at TP=0.7 with buffer already queued from a mid-tween user input.
2. Pause: RSM broadcasts pause. Rule 5 gate freezes advance.
3. Grace: RSM enters resume_grace state. Gate still freezes.
4. Grace expiry: `IsResumeGrace() == false` next tick.
5. Tick body advances F-2 → TP crosses 1.0 → CompleteTween → FlushBufferedInput (Story 005) → new SLIPPING → all within the same tick.

**No pause hook for F-6**: F-6 tail advance is inside the Rule 5 gate — pause freezes F-6 progress the same way it freezes F-2. Verify in tests.

**IMPORTANT — Actual signature (Story 001 binding)**:

The pseudocode above shows `HandlePausedChanged(bool bNewPaused)` (1 param). This is aspirational shorthand — do NOT change the actual signature. The signature bound in Story 001 (`PlayerLaneMovementComponent.h:256`) is:

```cpp
void HandlePausedChanged(bool bIsPaused, double Timestamp);
```

Follow this signature. `Timestamp` is unused by Story 009 — cast via `(void)Timestamp;` to suppress unused-parameter warnings. Story 013 (watchdog) may consume `Timestamp` for pause-window telemetry.

Same recurring drift pattern as Stories 006 (FRotator axis), 007 (sign convention), and 008 (HandleStateChanged 4-param). All four story-spec drifts are queued for a consolidated `/architecture-decision` amendment pass.

---

## Out of Scope

- Story 003: Rule 5 gate implementation (already in TickComponent skeleton).
- Story 005: `FlushBufferedInput()` implementation (this story exercises the flush at grace-expiry as an integration test).
- Story 008: terminal-state buffer discard (pause does NOT discard).

---

## QA Test Cases

*Test file: `tests/integration/player-movement/pm_pause_grace_test.cpp`. Automated integration tests using stubbed RSM with pause + resume_grace flag control.*

- **AC-11 (mid-tween pause freezes TweenProgress)**:
  - Given: PM SLIPPING Left→Center at `tween_progress = 0.4`.
  - When: RSM broadcasts `HandlePausedChanged(true)`; simulate 10 ticks.
  - Then: `tween_progress` remains 0.4 (bit-exact); pawn root at LaneWorldX(Center) (already committed at transition); mesh relative unchanged; F-6 state unchanged.

- **AC-12 (resume_grace freezes TweenProgress)**:
  - Given: PM SLIPPING at TP=0.4; RSM state RUNNING but `IsResumeGrace() == true`.
  - When: simulate 10 ticks.
  - Then: `tween_progress` remains 0.4.

- **AC-13 (grace expiry with buffer flush)**:
  - Given: PM SLIPPING Center→Right at TP=0.7 with `has_queued_input = true, queued_input_direction = Right`; RSM broadcasts pause=true; then resume_grace=true; then resume_grace=false (grace expiry).
  - When: TickComponent runs on the grace-expiry tick with effective_dt large enough for TP to cross 1.0.
  - Then: within that single tick: (a) CompleteTween fires (Center→Right complete); (b) `slip_complete_count += 1`; (c) FlushBufferedInput invokes HandleSlipTransition(Right); (d) new SLIPPING starts (Right→FarRight); (e) `has_queued_input == false` post-flush.

- **AC-COUNTER-PAUSE-RESUME**:
  - Given: PM with `slip_complete_count = 3`, `edge_absorb_trigger_count = 1`, `commitment_tell_fire_count = 5`.
  - When: pause and resume_grace and grace expiry cycle.
  - Then: all three counters preserved across the cycle. Resume completion (if the tween completed during resume) increments `slip_complete_count` normally.

- **Buffer preservation across pause (Rule 6)**:
  - Given: buffer queued before pause.
  - When: pause + resume.
  - Then: `has_queued_input == true` throughout; `queued_input_direction` unchanged.

- **F-6 tail freeze during pause**:
  - Given: F-6 tail active at `edge_absorb_progress = 0.3`.
  - When: pause broadcast; 10 ticks.
  - Then: `edge_absorb_progress` remains 0.3; `edge_absorb_local_timer_s` unchanged.

- **AC-24 exclusion during pause**:
  - Given: TP=0.45 SLIPPING; pause broadcast; ticks advance without unpausing.
  - When: verify OnSlipMidpoint spy.
  - Then: no broadcast. When resume happens and TP crosses 0.5, broadcast fires exactly once (not retroactively for the pause frames).

- **Slip input during pause discarded (Rule 5)**:
  - Given: PM SLIPPING + paused.
  - When: `HandleSlipTransition(Left)` invoked.
  - Then: input discarded (Rule 5 gate hits); buffer NOT queued; feedback NOT fired.

- **HandlePausedChanged is logging-only**:
  - Given: PM SLIPPING with mid-tween state.
  - When: `HandlePausedChanged(true)` invoked directly (not via TickComponent gate).
  - Then: no state field changes; only log entry appears.

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Automated integration test at `tests/integration/player-movement/pm_pause_grace_test.cpp` — must exist and pass.

**Status**: [ ] Not yet created

---

## Dependencies

- **Depends on**: Story 001 (HandlePausedChanged bound at BeginPlay + log category); Story 003 (Rule 5 gate reads IsPaused/IsResumeGrace); Story 005 (FlushBufferedInput hook exists for AC-13 verification); Story 008 (contrast with terminal DiscardBuffer semantics).
- **Unlocks**: No downstream stories — this is the last state-machine integration story.

---

## Completion Notes

**Completed**: 2026-08-02
**Criteria**: 7/7 covered by 9 integration test commands. All acceptance criteria have direct or transitive coverage.

**Deviations**:

- **ADVISORY — Signature convention correction (recurring pattern across 4 stories this session)**: Story text §Implementation Notes shows `HandlePausedChanged(bool bNewPaused)` (1 param — aspirational shorthand). Actual signature bound in Story 001 at `PlayerLaneMovementComponent.h:256` is `HandlePausedChanged(bool bIsPaused, double Timestamp)` (2 params). Implementation followed the actual signature; `Timestamp` `(void)`-cast (Story 013 watchdog will consume it later). **Follow-up**: same consolidated `/architecture-decision` amendment pass that addresses Story 006 FRotator axis + Story 007 sign convention + Story 008 HandleStateChanged signature should also update story-009 markdown line 53.

- **ADVISORY — Test path drift (recurring across all 6 stories this session)**: Story states `tests/integration/player-movement/pm_pause_grace_test.cpp`. Actual `Source/SLIPSTORM/Tests/Integration/PlayerMovement/PMPauseGraceTest.cpp` (UE convention). Same recurring pattern across Stories 004-009 — needs project-wide story-template fix.

**Test Evidence**:
- `Source/SLIPSTORM/Tests/Integration/PlayerMovement/PMPauseGraceTest.cpp` — 9 integration test commands:
  - TC1 `ac11_mid_tween_pause_freezes_tp` — AC-11 bit-exact TP preservation across 10 paused ticks
  - TC2 `ac12_resume_grace_freezes_tp` — AC-12 grace-state freeze
  - TC3 `ac13_grace_expiry_with_buffer_flush` — AC-13 4-phase end-to-end: SLIPPING+buffer → pause → grace → grace-expiry-tick → CompleteTween → FlushBufferedInput → new SLIPPING Right→FarRight (5 assertions verify state transitions across all phases)
  - TC4 `ac_counter_pause_resume_preserved` — AC-COUNTER-PAUSE-RESUME (3 counters across full cycle)
  - TC5 `buffer_preserved_across_pause` — Rule 6 buffer preservation contract (contrasts with Story 008 discard)
  - TC6 `f6_tail_freezes_during_pause` — F-6 progress + timer + sign unchanged across 10 paused ticks (sign assertion added via code-review fix)
  - TC7 `ac24_exclusion_during_pause` — OnSlipMidpoint delegate spy: count=0 during 5 paused ticks, count=1 exactly-once post-resume when TP crosses 0.5
  - TC8 `slip_input_during_pause_discarded` — Rule 5 gate discards slip input during pause; IHapticDispatch spy confirms zero haptic dispatch
  - TC9 `handle_paused_changed_is_logging_only` — 16 state fields snapshotted; verifies HandlePausedChanged direct invocation increments test counter but mutates no other state (edge_absorb_sign seed+assert added via code-review fix)

**Build verification**: `Result: Succeeded` (11.32 s incremental post-fix; 0 errors, 0 warnings).

**Code Review**: Complete — `/code-review` with `unreal-specialist + qa-tester` in parallel.

- **unreal-specialist verdict: CLEAN** — cleanest review of the session. ADR-0007 SD2 non-callback invariant satisfied (no ForceTickNow, no broadcasts, no RSM state mutation). Rule 6 buffer preservation grep gate PASS (DiscardBuffer only appears in comment as "deliberately NOT calling"). AC-13 4-phase sequence verified against CompleteTween→FlushBufferedInput→HandleSlipTransition chain.
- **qa-tester verdict: GAPS** — 2 minor + 1 nit (all field-completeness gaps, not correctness):
  1. **TC6 `edge_absorb_sign` assertion added** — closes seed-without-assert gap
  2. **TC9 `edge_absorb_sign` seed + assert added** — completes the "EVERY state field unchanged" contract
  3. **TC7 dt=0.1 comment clarified** — notes it applies only to post-unpause tick, paused ticks use 0.016s

All 3 suggestions applied inline. Post-fix build: `Result: Succeeded`. No deferred items.

**Notable**: This was the ONLY story this session where the code-review didn't catch a correctness bug. Stories 006 (FRotator axis), 007 (TriggerEdgeAbsorb stub + sign inversion), 008 (DEAD case reset) all had blocking bugs caught in review. Story 009's trivial scope (logging-only body, verification-only tests) meant no complex helpers to stall on and no design pseudocode to invert. **Workflow lesson reinforced**: complex helper-body stories reliably benefit from code review; trivial verification-only stories still benefit but the review adds fewer suggestions.

**Files touched**:
- `Source/SLIPSTORM/Player/PlayerLaneMovementComponent.h`: `HandlePausedChanged_TestOnlyCallCount int32` field at h:417, `friend class FPMPauseGraceTest` at h:363, doc-comment updates
- `Source/SLIPSTORM/Player/PlayerLaneMovementComponent.cpp:492-510`: replaced Story 001 TODO stub with logging-only body (`(void)Timestamp`, test counter increment under `WITH_DEV_AUTOMATION_TESTS`, `UE_LOG(Verbose)` line, explicit "NOT calling DiscardBuffer" comment per Rule 6)
- `Source/SLIPSTORM/Tests/Integration/PlayerMovement/PMPauseGraceTest.cpp` (new, ~700 lines post-fixes): 9 integration test commands with AAA labels


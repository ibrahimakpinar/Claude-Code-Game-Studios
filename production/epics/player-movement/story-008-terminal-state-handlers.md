# Story 008: Terminal state handlers (DEAD/COMPLETE/ABORTED/COUNTDOWN) + counter reset + AC-SS-B runtime

> **Epic**: Player Movement (Slip)
> **Status**: Complete
> **Layer**: Core
> **Type**: Integration
> **Estimate**: 5–7 hours
> **Manifest Version**: 2026-07-03 (docs/registry/architecture.yaml v8)
> **Last Updated**: 2026-07-15

## Context

**GDD**: `design/gdd/player-movement-mechanics.md` (§3 Rules 7/8/9/10/11, Delegate Handler Bodies HandleStateChanged, §8 AC-14/14-boundary/15/16/17/18/28/31/32, AC-F6-D, AC-COUNTER-DEAD/COMPLETE/F6-RESET), `design/gdd/player-movement-platform.md` (§3 AC-SS-B runtime default-branch fallback).
**Requirement**: `TR-PM-016` (counter reset semantics), `TR-PM-017` (DEAD freeze), `TR-PM-018` (COMPLETE/ABORTED snap), `TR-PM-015` (final counter behavior).
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0009 (SD5 root committed / mesh frozen at fractional per DEAD; snap-and-reset for COMPLETE/ABORTED), secondary ADR-0007 (delegate handler bodies subscribed at BeginPlay).
**ADR Decision Summary**: DEAD → PM freezes fractional pose, sets `movement_state = SETTLED` (R7-PM-PROPAGATION-REVIEW), commits `current_lane = target_lane`, preserves counters. COMPLETE/ABORTED → snap to target, reset TweenProgress + lean + buffer, preserve counters. COUNTDOWN → reset lanes to Center + reset ALL counters + reset F-6 state. AC-SS-B: any switch over `EMovementState` with a `default:` branch returns SETTLED (safe default on corrupt ordinal).

**Engine**: Unreal Engine 5.7 | **Risk**: MEDIUM
**Engine Notes**: `AActor::SetActorLocation` and `USceneComponent::SetRelativeLocation` / `SetRelativeRotation` stable pre-cutoff. RSM delegate broadcast synchronization per ADR-0007 SD2 (non-callback invariant preserved).

**Control Manifest Rules (this layer)** — from `docs/registry/architecture.yaml` v8:
- **Required**: `HandleStateChanged(ERSMState OldState, ERSMState NewState)` signature bound at Story 001. Body implemented here.
- **Required**: DEAD freeze does NOT reset `tween_progress`, F-5 lean, or F-6 state. `movement_state = SETTLED` per R7-PM-PROPAGATION-REVIEW; `current_lane = target_lane` (source-lane semantic terminates at DEAD).
- **Required**: COMPLETE/ABORTED snap-and-reset: `current_lane = target_lane`; root at LaneWorldX(target_lane); mesh relative = 0; `tween_progress = 0.0f`; lean and F-6 zeroed; buffer discarded (Story 005 `DiscardBuffer()` invoked); counters PRESERVED.
- **Required**: COUNTDOWN full reset: `current_lane = target_lane = Center`; mesh relative = 0; `movement_state = SETTLED`; `tween_progress = 0.0f`; lean zeroed; F-6 state zeroed; buffer discarded; ALL counters (slip_complete_count, edge_absorb_trigger_count, commitment_tell_fire_count) reset to 0.
- **Required**: Any switch statement over `EMovementState` MUST have a `default:` branch that returns SETTLED (AC-SS-B runtime).
- **Required**: OnSlipMidpoint MUST NOT fire on DEAD/COMPLETE/ABORTED transitions (AC-24 exclusion) — Story 003's tick body already gates on movement_state == SLIPPING, but assertion is verified here.

---

## Acceptance Criteria

*From `design/gdd/player-movement-mechanics.md` §3 Rules 7/8/9/10/11 + §8 + platform §3 AC-SS-B, scoped to this story:*

- [ ] `HandleStateChanged(ERSMState OldState, ERSMState NewState)` switch on `NewState`:
  - [ ] Case `DEAD` (Rule 7): if `movement_state == SLIPPING`, `current_lane = target_lane` (commit source→target); do NOT modify `tween_progress`, mesh relative X, mesh relative rotation, or F-6 state; `movement_state = SETTLED`; buffer discarded (via Story 005's `DiscardBuffer()`); counters PRESERVED. If `movement_state == SETTLED` already, only discard buffer + preserve counters.
  - [ ] Case `COMPLETE` (Rule 8): snap — `current_lane = target_lane`; `GetOwner()->SetActorLocation(FVector(LaneWorldX(target_lane), 0, 0), false)`; `MeshComponent->SetRelativeLocation(FVector::ZeroVector)`; `MeshComponent->SetRelativeRotation(FRotator::ZeroRotator)`; `tween_progress = 0.0f`; `movement_state = SETTLED`; F-5 lean zeroed; F-6 state zeroed (edge_absorb_active=false, snapshot cleared, ticks_remaining=0); `DiscardBuffer()`; counters PRESERVED.
  - [ ] Case `ABORTED` (Rule 9): identical to COMPLETE.
  - [ ] Case `COUNTDOWN` (Rule 10): full reset — `current_lane = target_lane = EPlayerLane::Center`; `GetOwner()->SetActorLocation(FVector(LaneWorldX(Center), 0, 0), false)`; mesh at zero relative + zero rotation; `movement_state = SETTLED`; `tween_progress = 0.0f`; F-5 + F-6 zeroed; `DiscardBuffer()`; `slip_complete_count = 0`; `edge_absorb_trigger_count = 0`; `commitment_tell_fire_count = 0`.
  - [ ] Case `IDLE`: `DiscardBuffer()`; counters preserved; no lane change (allows spawn-time initialization to happen via BeginPlay before first COUNTDOWN).
  - [ ] Case `RUNNING`: no-op (mechanics already handled per-tick).
  - [ ] Case `default:` returns without side effect (defensive; the switch is on ERSMState not EMovementState — the AC-SS-B default-branch requirement applies to any EMovementState switches).
- [ ] AC-SS-B default-branch behavior — for any `switch (EMovementState)` in PM (e.g., internal state dispatch in TickComponent or getters), the `default:` branch returns SETTLED (safe fallback on corrupt ordinal). Confirm at least one grep-verifiable switch site.
- [ ] **AC-14 (DEAD @ TP=0.55)**: `tween_progress == 0.55` preserved; `current_lane = target_lane`; `movement_state = SETTLED`; lean angles freeze at their pre-DEAD values; `slip_complete_count` unchanged (not zeroed).
- [ ] **AC-14 boundary (TP=0.001 and TP=0.999)**: same behavior — no snap-on-threshold; the fractional value is preserved bit-exact.
- [ ] **AC-15 (DEAD while SETTLED)**: `current_lane` unchanged; no lane assignment; counters preserved.
- [ ] **AC-16 (COMPLETE @ TP=0.55)**: snap to target; `tween_progress == 0.0f`; `lean_angle == 0`; buffer cleared; `slip_complete_count` unchanged.
- [ ] **AC-17 (ABORTED @ TP=0.55)**: identical to AC-16.
- [ ] **AC-18 (COUNTDOWN)**: current_lane = target_lane = Center; all outputs zero; ALL counters == 0.
- [ ] **AC-28 (COMPLETE @ TP=0.5 → lean_angle == 0)**: F-5 zeroed via snap.
- [ ] **AC-31 (DEAD @ TP=0.5 → arm_lean_angle freezes)**: arm not reset.
- [ ] **AC-32 (COMPLETE @ TP=0.5 → arm_lean_angle == 0)**: F-5 zeroed.
- [ ] **AC-F6-D (DEAD during F-6 tail)**: `edge_absorb_active` preserved; `edge_absorb_local_timer_s` preserved; `edge_absorb_progress` preserved; sampled 5 ticks post-DEAD show F-6 fields unchanged; F-6 lean angles frozen (matches AC-31).
- [ ] **AC-COUNTER-DEAD**: counters preserved across DEAD; sampled at multiple TP values, no zeroing.
- [ ] **AC-COUNTER-COMPLETE**: counters preserved across COMPLETE; no zeroing.
- [ ] **AC-COUNTER-F6-RESET**: F-6 timer + fade-out snapshot state reset on COMPLETE/ABORTED/COUNTDOWN/IDLE; preserved on DEAD.
- [ ] **AC-24 exclusion**: OnSlipMidpoint does NOT fire on DEAD/COMPLETE/ABORTED (verified via subscriber spy — assert zero deliveries during the terminal transition).

---

## Implementation Notes

*Derived from ADR-0009 SD5 + mechanics §3 Rules 7/8/9/10 Delegate Handler Bodies pseudo-code:*

**HandleStateChanged skeleton**:
```cpp
void UPlayerLaneMovementComponent::HandleStateChanged(ERSMState OldState, ERSMState NewState)
{
    switch (NewState)
    {
    case ERSMState::DEAD:
        // Rule 7 — DEAD freeze
        if (movement_state == ERunSlipState::SLIPPING)
        {
            current_lane = target_lane; // R7-PM-PROPAGATION-REVIEW: source-lane semantic terminates at DEAD
            movement_state = ERunSlipState::SETTLED;
        }
        DiscardBuffer();
        // Do NOT touch: tween_progress, mesh transforms, lean angles, F-6 state
        break;

    case ERSMState::COMPLETE:
    case ERSMState::ABORTED:
        // Rules 8/9 — snap to target
        SnapToTargetAndReset();
        DiscardBuffer();
        // Counters preserved
        break;

    case ERSMState::COUNTDOWN:
        // Rule 10 — full reset
        current_lane = EPlayerLane::Center;
        target_lane = EPlayerLane::Center;
        SnapToTargetAndReset();
        DiscardBuffer();
        slip_complete_count = 0;
        edge_absorb_trigger_count = 0;
        commitment_tell_fire_count = 0;
        break;

    case ERSMState::IDLE:
        DiscardBuffer();
        break;

    case ERSMState::RUNNING:
    default:
        break;
    }
}

void UPlayerLaneMovementComponent::SnapToTargetAndReset()
{
    current_lane = target_lane;
    GetOwner()->SetActorLocation(FVector(LaneWorldX(target_lane), 0.0f, 0.0f), /*bSweep=*/false);
    MeshComponent->SetRelativeLocation(FVector::ZeroVector);
    MeshComponent->SetRelativeRotation(FRotator::ZeroRotator);
    tween_progress = 0.0f;
    movement_state = ERunSlipState::SETTLED;
    lean_angle = 0.0f;
    head_lean_angle = 0.0f;
    arm_lean_angle = 0.0f;
    // Reset F-6 state
    edge_absorb_active = false;
    edge_absorb_progress = 0.0f;
    edge_absorb_local_timer_s = 0.0f;
    f6_override_fadeout_ticks_remaining = 0;
    f6_override_fadeout_snapshot_body = 0.0f;
    f6_override_fadeout_snapshot_head = 0.0f;
    f6_override_fadeout_snapshot_arm  = 0.0f;
}
```

**AC-SS-B site**: locate a `switch (EMovementState)` in PM (e.g., in a public accessor or Seam 12 cast site). Add `default: return EMovementState::SETTLED;`. Register a grep gate in the test to ensure the default: branch is present.

**Non-callback invariant** (ADR-0007 SD2 + IG-2): `HandleStateChanged` is invoked from RSM's regular Tick's broadcast pass. It MUST NOT call `RSMSubsystem->ForceTickNow()`, MUST NOT broadcast `OnSlipMidpoint`, MUST NOT modify RSM state.

**DEAD does not touch F-6 state** (AC-F6-D): SnapToTargetAndReset is called for COMPLETE/ABORTED/COUNTDOWN but NOT DEAD. DEAD only sets movement_state and commits lane — everything else preserved for Death Replay.

**IMPORTANT — Actual signature (Story 001 binding)**:

The pseudocode above shows `HandleStateChanged(ERSMState OldState, ERSMState NewState)` (2 params, `ERSMState` type). This is aspirational shorthand — do NOT change the actual signature. The signature bound in Story 001 (`PlayerLaneMovementComponent.h:244`) is:

```cpp
void HandleStateChanged(ERunState PreviousState,
                        ERunState NewState,
                        ERunOutcome Outcome,
                        double Timestamp);
```

Follow this signature. `Outcome` and `Timestamp` are unused by Story 008 — cast them via `(void)Outcome; (void)Timestamp;` to suppress unused-parameter warnings. Story 011 (audio cue) will consume `Outcome` for conditioning; Story 013 (watchdog) may consume `Timestamp`.

The switch is on `NewState` (an `ERunState`, NOT `ERSMState`). All state constants throughout this story should read `ERunState::DEAD`, `ERunState::COMPLETE`, etc. — the enum type is `ERunState`, not `ERSMState`. `ERSMState` is not a real type in this codebase; the story spec's use of it is a naming drift from an earlier draft.

**IMPORTANT — AC-F6-D and F-6 tick advance post-DEAD**:

Story 007's tick body at `PlayerLaneMovementComponent.cpp:256-266` advances `edge_absorb_progress` unconditionally when `edge_absorb_active` is true (i.e., F-6 tick advance runs during both SLIPPING and SETTLED). AC-F6-D requires that on DEAD entry, F-6 state is preserved for Death Replay — meaning subsequent ticks post-DEAD should NOT advance `edge_absorb_progress` or `edge_absorb_local_timer_s`.

Story 008 has two options to satisfy AC-F6-D:

1. **Disable component tick on DEAD**: In the DEAD case of HandleStateChanged, set `PrimaryComponentTick.bCanEverTick = false`. Simple, but blunt (also blocks the F-2/F-3/F-5 write paths for any Death Replay visual polish).

2. **Add DEAD guard to F-6 tick advance**: Extend the tick-advance block at `cpp:256-266` with an additional guard so it skips advance when the RSM is in DEAD state. Requires cheap RSM-state read (`RSMSubsystem->GetCurrentState() == ERunState::DEAD`). Preserves the rest of the tick body's semantics for potential Death Replay reads.

**Recommendation**: option 2. F-6 tick advance becomes:

```cpp
if (edge_absorb_active && (!RSMSubsystem || RSMSubsystem->GetCurrentState() != ERunState::DEAD))
{
    edge_absorb_local_timer_s += effective_dt;
    // ...
}
```

Update CC5 (`ac24_no_on_slip_midpoint_during_f6_tail`) in `PMEdgeAbsorbCompositionTest.cpp` if the guard affects its 10-tick loop assumptions.

Story 008's `HandleStateChanged` DEAD branch does NOT touch F-6 state, per AC-F6-D. The F-6 tick advance guard is what actually preserves the state.

---

## Out of Scope

- Story 005: `DiscardBuffer()` implementation (this story CALLS it).
- Story 009: `HandlePausedChanged` body + Rule 6 pause/grace freeze.
- Story 013: Watchdog state does NOT reset on terminal transitions — the watchdog is hardware-side and independent.
- Story 010: `commitment_tell_fire_count` reset semantics (this story resets it on COUNTDOWN per Rule 10; Story 010 owns the increment site).

---

## QA Test Cases

*Test file: `tests/integration/player-movement/pm_terminal_states_test.cpp`. Automated integration tests using a stubbed RSM subsystem that broadcasts state changes.*

- **AC-14 (DEAD @ TP=0.55 freeze)**:
  - Given: PM SLIPPING Left→Center at `tween_progress = 0.55`, F-5 lean at some non-zero value.
  - When: RSM broadcasts `HandleStateChanged(RUNNING, DEAD)`.
  - Then: `tween_progress == 0.55f` (bit-exact preserved); `current_lane == Center` (target committed); `movement_state == SETTLED`; `lean_angle` unchanged from pre-DEAD; `slip_complete_count` unchanged.

- **AC-14 boundary (TP=0.001 and TP=0.999)**:
  - Given: PM SLIPPING at either boundary TP value.
  - When: DEAD broadcast.
  - Then: TP preserved bit-exact (no snap-on-threshold); `movement_state = SETTLED`.

- **AC-15 (DEAD while SETTLED)**:
  - Given: PM SETTLED at Right with `slip_complete_count = 3`.
  - When: DEAD broadcast.
  - Then: `current_lane == Right` (unchanged); `slip_complete_count == 3`; buffer cleared if it had queued input.

- **AC-16 (COMPLETE @ TP=0.55 snap)**:
  - Given: PM SLIPPING Left→Center at TP=0.55.
  - When: COMPLETE broadcast.
  - Then: pawn root at LaneWorldX(Center); mesh at relative zero; mesh rotation zero; `tween_progress == 0.0f`; `movement_state == SETTLED`; `lean_angle == 0.0f`; `head_lean_angle == 0.0f`; `arm_lean_angle == 0.0f`; buffer cleared; `slip_complete_count` unchanged from pre-COMPLETE value.

- **AC-17 (ABORTED @ TP=0.55)**: identical to AC-16 with ABORTED broadcast.

- **AC-18 (COUNTDOWN full reset)**:
  - Given: PM at FarRight SLIPPING at TP=0.7 with `slip_complete_count = 5`, `edge_absorb_trigger_count = 2`, `commitment_tell_fire_count = 8`.
  - When: COUNTDOWN broadcast.
  - Then: `current_lane == target_lane == Center`; pawn root at LaneWorldX(Center); all outputs zero; ALL three counters == 0.

- **AC-28 (COMPLETE lean zero)**: covered by AC-16 verification of `lean_angle == 0`.

- **AC-31 (DEAD @ TP=0.5 arm lean freeze)**:
  - Given: PM SLIPPING with `arm_lean_angle = 4.5°`.
  - When: DEAD broadcast.
  - Then: `arm_lean_angle` unchanged; sampled 5 ticks post-DEAD, still 4.5°.

- **AC-32 (COMPLETE arm lean zero)**: `arm_lean_angle == 0` post-COMPLETE.

- **AC-F6-D (DEAD during F-6 tail preservation)**:
  - Given: F-6 active with `edge_absorb_active = true`, `edge_absorb_progress = 0.4`, `edge_absorb_local_timer_s = 0.108s`, F-6 lean angles non-zero.
  - When: DEAD broadcast.
  - Then: `edge_absorb_active` unchanged (still true); `edge_absorb_progress == 0.4f` (preserved); `edge_absorb_local_timer_s == 0.108f` (preserved); F-6 lean angles frozen (matched to AC-31 semantics for F-5 arm).

- **AC-COUNTER-DEAD (counters preserved on DEAD)**: iterate through all three counters with non-zero starting values; DEAD broadcast; all preserved.

- **AC-COUNTER-COMPLETE (counters preserved on COMPLETE)**: same as DEAD case.

- **AC-COUNTER-F6-RESET (F-6 state reset on COMPLETE/ABORTED/COUNTDOWN/IDLE; preserved on DEAD)**:
  - COMPLETE: `edge_absorb_active = false`; snapshot fields = 0; ticks_remaining = 0.
  - ABORTED: same.
  - COUNTDOWN: same.
  - IDLE: same (via SnapToTargetAndReset? No — IDLE only DiscardBuffer per spec; verify F-6 state NOT reset on IDLE OR reset — read GDD carefully during implementation; the spec here says F-6 reset on IDLE per AC-COUNTER-F6-RESET; the Story 003 stub in HandleStateChanged IDLE branch should be extended to reset F-6 here).
  - DEAD: preserved per AC-F6-D.

- **AC-SS-B (runtime default-branch)**:
  - Given: a corrupt `EMovementState` value (e.g., cast from int 42).
  - When: any PM getter/switch reads the corrupt value.
  - Then: `default:` branch returns SETTLED — no crash, no undefined behavior.
  - Grep gate: verify at least one `switch (EMovementState)` in PM source has a `default:` clause.

- **AC-24 exclusion (no OnSlipMidpoint on terminal)**:
  - Given: subscriber spy on OnSlipMidpoint; PM SLIPPING with TP crossing 0.5.
  - When: DEAD/COMPLETE/ABORTED broadcast happens BEFORE the tick that would cross 0.5.
  - Then: OnSlipMidpoint spy records zero deliveries during the transition.

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Automated integration test at `tests/integration/player-movement/pm_terminal_states_test.cpp` — must exist and pass.
- Grep gate on `default:` present in at least one `switch (EMovementState)` in PM source.

**Status**: [ ] Not yet created

---

## Dependencies

- **Depends on**: Story 001 (HandleStateChanged bound at BeginPlay + counter field declarations + EMovementState enum); Story 003 (movement_state field + tween_progress + collision commit helper); Story 004 (mesh SetRelativeLocation); Story 005 (`DiscardBuffer()` helper); Story 006 (lean field writes); Story 007 (F-6 state fields + edge_absorb_trigger_count).
- **Unlocks**: Story 010 (commitment_tell_fire_count reset semantics rely on this story's COUNTDOWN branch); Death Replay integration (Camera GDD unauthored — inherits DEAD freeze contract).

---

## Completion Notes

**Completed**: 2026-07-15
**Criteria**: 23/23 covered by 11 integration test commands. All ACs from the story body + qa-tester's expanded coverage matrix (RUNNING/default no-ops, spy patterns).

**Deviations**:

- **ADVISORY — Signature convention correction (spec drift, recurring pattern)**: Story text §Implementation Notes shows `HandleStateChanged(ERSMState OldState, ERSMState NewState)` (2 params, `ERSMState` type — aspirational shorthand). Actual bound signature at PLMC.h:244 (Story 001) is `HandleStateChanged(ERunState PreviousState, ERunState NewState, ERunOutcome Outcome, double Timestamp)` (4 params, `ERunState` — `ERSMState` doesn't exist in this codebase). Story 008 implementation followed the actual signature; `Outcome` and `Timestamp` `(void)`-cast per readiness clarification. **Follow-up**: same `/architecture-decision` pass that addresses Story 006 axis + Story 007 sign should also update story-008 markdown lines 69+.

- **ADVISORY — DEAD case correctness bug caught inline during dev-story**: Agent's initial implementation reset `tween_progress = 0.0f` and zeroed all 3 lean angles in the DEAD case — would have failed AC-14 (bit-exact preservation of TP=0.55) and AC-31 (arm_lean_angle freeze) at runtime. Corrected inline to PRESERVE per Rule 7 Death Replay semantic. Same class of correctness bug as Story 006 FRotator axis and Story 007 TriggerEdgeAbsorb stub — agent stalls introduce correctness issues that inline finishing must verify. The `/code-review` pass caught related issues (AC-24 evidence gap, TC7 shallowness, TC10 outer, sign field coverage) that would otherwise have slipped through.

- **ADVISORY — F-6 tick DEAD guard modifies Story 007 tested code**: Added `!bRunStateDead` gate at `PlayerLaneMovementComponent.cpp:271` inside Story 007's F-6 tick advance block. Story 007's CC5 (`ac24_no_on_slip_midpoint_during_f6_tail` in `PMEdgeAbsorbCompositionTest.cpp`) uses RSM=RUNNING so the guard passes through — no regression. Story 008's TC8 verifies the guard holds under RSM=DEAD. Cross-story coupling documented here for future refactors.

- **ADVISORY — Test path drift (recurring across Stories 004–008)**: Story states `tests/integration/player-movement/pm_terminal_states_test.cpp`; actual test at `Source/SLIPSTORM/Tests/Integration/PlayerMovement/PMTerminalStatesTest.cpp` (UE convention). Same recurring pattern across all 5 dev-story cycles this session — needs project-wide story-template fix.

- **ADVISORY — `unreal-specialist` code review truncated mid-work**: The reviewer produced orientation output ("Now let me check the story doc and ADR references for completeness.") but no final verdict block. `qa-tester` returned complete review. Given all `qa-tester` findings resolved + no engine-specific concerns in the reviewer's orientation phase + all prior stories established the engine patterns (FRotator Roll, `SetRelativeLocation`/`SetRelativeRotation`, `IsValid` guards, RSM-state reads), closing without re-spawn. Alternative: re-run unreal-specialist as follow-up if any engine-specific issue surfaces during on-device testing.

**Test Evidence**:
- `Source/SLIPSTORM/Tests/Integration/PlayerMovement/PMTerminalStatesTest.cpp` — 11 world-spawn integration test commands:
  - TC1 `ac14_dead_freeze_tp055` — AC-14 primary
  - TC2 `ac14_boundary_dead_tp_bit_exact` — AC-14 boundary (0.001, 0.999)
  - TC3 `ac15_dead_while_settled` — AC-15
  - TC4 `ac16_complete_snap_and_reset` — AC-16 + AC-28 + AC-32
  - TC5 `ac17_aborted_snap_and_reset` — AC-17
  - TC6 `ac18_countdown_full_reset` — AC-18
  - TC7 `ac31_dead_lean_freeze` — AC-31 + 5-tick post-DEAD regression guard
  - TC8 `ac_f6_d_dead_during_f6_tail` — AC-F6-D + F-6 tick DEAD guard verification (all 4 F-6 fields)
  - TC9 `ac_counter_preservation_and_f6_reset` — 5 sub-cases (DEAD/COMPLETE/ABORTED/COUNTDOWN/IDLE) with edge_absorb_sign preservation vs reset
  - TC10 `ac_ss_b_get_movement_state_default_branch` — SETTLED, SLIPPING, corrupt ordinal 42 (transient outer)
  - TC11 `ac24_no_slip_midpoint_on_terminal` — 3 sub-cases (DEAD, COMPLETE, ABORTED) with OnSlipMidpoint delegate spy

**Build verification**: `Result: Succeeded` (11.23 s incremental after code-review fix pass; 0 errors, 0 warnings).

**Code Review**: Complete — `/code-review` with `unreal-specialist + qa-tester` in parallel. `qa-tester` returned complete review (verdict: GAPS driven by AC-24 evidence gap, not correctness). `unreal-specialist` review was truncated mid-orientation (no final verdict output). User chose option A → all fixes applied:

1. **AC-24 spy test** (major gap resolved) — new TC11 with OnSlipMidpoint delegate spy + 3 sub-cases
2. **TC7 5-tick post-DEAD regression** (minor) — guards against future Rule 5 gate removal that would silently break AC-31
3. **TC8 sign field assertion** (minor) — all 4 F-6 fields now covered per AC-F6-D
4. **TC10 transient outer** (minor) — `NewObject<>(GetTransientPackage())`
5. **TC9-IDLE sign seed + reset assertion** (nit) — seeded -1.0f, verified reset (0) vs preserved (-1) per sub-case
6. **SnapToTargetAndReset doc** (nit) — verified already correct at h:519, no change needed

No blocking issues; no deferred items.

**Files touched**:
- `Source/SLIPSTORM/Player/PlayerLaneMovementComponent.h`: forward decl `EMovementState`, public `GetMovementStateExternal() const` accessor, private `SnapToTargetAndReset()` helper, `friend class FPMTerminalStatesTest`
- `Source/SLIPSTORM/Player/PlayerLaneMovementComponent.cpp` (~940 → 1219, +279 lines): HandleStateChanged full switch body (5 cases + default), SnapToTargetAndReset body, GetMovementStateExternal body with AC-SS-B default-branch, F-6 tick advance DEAD guard at cpp:271, `Seam/PlayerMovementProvider.h` include
- `Source/SLIPSTORM/Tests/Integration/PlayerMovement/PMTerminalStatesTest.cpp` (new, ~570 lines post-fixes): 11 integration test commands


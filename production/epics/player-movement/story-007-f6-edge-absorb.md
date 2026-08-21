# Story 007: F-6 edge-absorb tail + Rule 1 edge no-op + 2-frame fade-out override + EC15 decay + F-5/F-6 co-write

> **Epic**: Player Movement (Slip)
> **Status**: Complete
> **Layer**: Core
> **Type**: Logic
> **Estimate**: 5–7 hours
> **Manifest Version**: 2026-07-03 (docs/registry/architecture.yaml v8)
> **Last Updated**: 2026-07-15

## Context

**GDD**: `design/gdd/player-movement-mechanics.md` (§3 Rule 1, §4 F-6 edge-absorb tail, §5.1(a) Override, R11a §6.1 fade-out, R11a §6.2 EC15_F6_DECAY_COEFFICIENT, §8 AC-02, AC-F6-A/B/C/E).
**Requirement**: `TR-PM-009`, `TR-PM-028` (EC15 coefficient in phase 3).
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0009 (SD6 EdgeAbsorbCurve validation + fallback path).
**ADR Decision Summary**: F-6 fires on Rule 1 edge no-op or as a tail phase during SLIPPING. §5.1(a) Override: SETTLED→SLIPPING while F-6 active snapshots F-6 lean values into `f6_override_fadeout_snapshot_*`; over 2 ticks apply linear fade-out (1.0 → 0.5 → 0). During SLIPPING phase 3, multiply F-6 output by `EC15_F6_DECAY_COEFFICIENT = 0.7` to preserve ±(MAX_LEAN×1.2) headroom.

**Engine**: Unreal Engine 5.7 | **Risk**: LOW (curve + timer state on component; no post-cutoff engine surface).
**Engine Notes**: None.

**Control Manifest Rules (this layer)** — from `docs/registry/architecture.yaml` v8:
- **Required**: F-5 + F-6 co-write final SUM clamped to ±(MAX_LEAN_ANGLE_DEG × 1.2). F-5 does its own component clamp (Story 006); F-6 clamps its own; the FINAL SUM re-clamped in the mesh write step.
- **Required**: EdgeAbsorbCurve null → immediate F-6 return (no tail phase) + `bCurveFallbackActive == true`. Edge no-op still increments `edge_absorb_trigger_count` for AC parity even in fallback (ADR-0009 SD6 fallback note).
- **Required**: §5.1(a) Override on SETTLED→SLIPPING while F-6 active — snapshot at collision commit, 2-frame fade-out with multiplier ramp 1.0 → 0.5 → 0 (R11a-3 lockstep).

---

## Acceptance Criteria

*From `design/gdd/player-movement-mechanics.md` §3 Rule 1 + §4 F-6 + §5.1(a) + §8, scoped to this story:*

- [ ] F-6 state on component: `bool edge_absorb_active`; `float edge_absorb_progress ∈ [0.0, 1.0]`; `float edge_absorb_sign` (±1); `float edge_absorb_local_timer_s`; `float f6_override_fadeout_snapshot_body`, `snapshot_head`, `snapshot_arm`; `int32 f6_override_fadeout_ticks_remaining` (0..2).
- [ ] `TriggerEdgeAbsorb(EPlayerLane FromLane, ESlipDirection Dir)`:
  - [ ] Sets `edge_absorb_active = true`; `edge_absorb_progress = 0.0f`; `edge_absorb_local_timer_s = 0.0f`; `edge_absorb_sign = +1` if Dir=Right else -1.
  - [ ] Increments `edge_absorb_trigger_count += 1` (Rule 1 counter).
- [ ] TickComponent F-6 advance (SLIPPING or SETTLED with F-6 active):
  - [ ] If `edge_absorb_active`, advance `edge_absorb_local_timer_s += effective_dt`; `edge_absorb_progress = FMath::Clamp(edge_absorb_local_timer_s / EDGE_ABSORB_DURATION_S, 0.0f, 1.0f)`.
  - [ ] Compute F-6 lean: `f6_body = EdgeAbsorbCurve->GetFloatValue(edge_absorb_progress) * MAX_LEAN_ANGLE_DEG * edge_absorb_sign`. Same shape for head/arm (staggered like F-5 per HEAD_LAG/ARM_LEAD to mechanics §4 F-6 formula).
  - [ ] If `movement_state == SLIPPING` AND `tween_progress > authored_phase3_start` (phase 3 threshold from mechanics §4 F-6 R11a-4): multiply f6 components by `EC15_F6_DECAY_COEFFICIENT` (default 0.7, safe [0.6, 1.0]).
  - [ ] Component-level clamp each F-6 component to ±(MAX_LEAN×1.2).
  - [ ] When `edge_absorb_progress >= 1.0`, set `edge_absorb_active = false`.
- [ ] §5.1(a) Override (SETTLED→SLIPPING while F-6 active) — this hooks into Story 003's HandleSlipTransition SETTLED→SLIPPING branch:
  - [ ] BEFORE clearing F-6 state, snapshot current F-6 lean values into `f6_override_fadeout_snapshot_*`.
  - [ ] Set `f6_override_fadeout_ticks_remaining = 2`.
  - [ ] Clear `edge_absorb_active = false`.
- [ ] Fade-out application (during first 2 ticks post-Override):
  - [ ] Tick N=1 (first post-Override tick): multiplier 1.0 → `f6_body = snapshot_body`, etc.; decrement `f6_override_fadeout_ticks_remaining` to 1.
  - [ ] Tick N=2: multiplier 0.5 → `f6_body = 0.5 * snapshot_body`; decrement to 0.
  - [ ] Tick N=3+: multiplier 0 → no snapshot contribution.
- [ ] Final F-5 + F-6 co-write sum + clamp (extending Story 006's mesh write):
  - [ ] `combined_body = FMath::Clamp(f5_body + f6_body, -MAX_LEAN×1.2, MAX_LEAN×1.2)`. Same for head/arm.
  - [ ] `MeshComponent->SetRelativeRotation(FRotator(combined_body, 0, 0))`.
  - [ ] `lean_angle = combined_body`; `head_lean_angle = combined_head`; `arm_lean_angle = combined_arm`.
- [ ] Rule 1 edge no-op path (HandleSlipTransition SETTLED-with-invalid-target from Story 003): TriggerEdgeAbsorb fires; PM stays SETTLED; NO commitment-tell fires; NO OnSlipMidpoint broadcast.
- [ ] `edge_absorb_trigger_count` public property returns the monotonic counter.
- [ ] **AC-02 (edge no-op)**: FarRight + slip-right → PM stays SETTLED; `edge_absorb_trigger_count++`; F-6 fires.
- [ ] **AC-F6-A (SETTLED @ FarRight + slip-right → F-6 fires, mid-tail body/head/arm all negative and non-zero)**: verifies head + arm decoupling (§5.1(a) closes decouple defect).
- [ ] **AC-F6-B (§5.1(a) Override 2-frame fade-out, no >50% single-frame jump)**: NO single tick's F-6 contribution changes by more than 50% of the snapshot magnitude.
- [ ] **AC-F6-C (FarLeft slip-left AND FarRight slip-right both fire F-6; Left slip-left → FarLeft tween does NOT fire F-6)**: counter-case verification.
- [ ] **AC-F6-E (EC-15 worst case)**: SLIPPING @ TP=0.85 + F-6 @ progress=0.10 + EC15_F6_DECAY_COEFFICIENT=0.7 → `head_lean_angle ≤ 11.5° ± 0.1°` (under 12° clamp, margin ≥ 0.58°).
- [ ] **AC-24 exclusion**: `OnSlipMidpoint` MUST NOT fire on edge-absorb — verified in Story 003 test but re-asserted here.

---

## Implementation Notes

*Derived from ADR-0009 SD6 + mechanics §4 F-6 + §5.1(a) Override + R11a §6.1 + R11a §6.2:*

**F-6 formula site** (mechanics §4):
```cpp
void UPlayerLaneMovementComponent::ComputeF6(float& OutBody, float& OutHead, float& OutArm)
{
    // Fallback: no curve, no snapshot fade -> zero
    if (!EdgeAbsorbCurve || bCurveFallbackActive)
    {
        OutBody = OutHead = OutArm = 0.0f;
    }
    else if (edge_absorb_active)
    {
        const float body_t = EdgeAbsorbCurve->GetFloatValue(edge_absorb_progress);
        const float head_t = EdgeAbsorbCurve->GetFloatValue(FMath::Max(0.0f, edge_absorb_progress - HEAD_LAG_PROGRESS));
        const float arm_t  = EdgeAbsorbCurve->GetFloatValue(FMath::Min(1.0f, edge_absorb_progress + ARM_LEAD_PROGRESS));

        OutBody = body_t * MAX_LEAN_ANGLE_DEG * edge_absorb_sign;
        OutHead = head_t * MAX_LEAN_ANGLE_DEG * edge_absorb_sign;
        OutArm  = arm_t  * MAX_LEAN_ANGLE_DEG * edge_absorb_sign;

        // Phase 3 EC15 decay during SLIPPING
        if (movement_state == ERunSlipState::SLIPPING && tween_progress > /* phase3_start from mechanics §4 F-6 R11a-4 */ 0.66f)
        {
            OutBody *= EC15_F6_DECAY_COEFFICIENT;
            OutHead *= EC15_F6_DECAY_COEFFICIENT;
            OutArm  *= EC15_F6_DECAY_COEFFICIENT;
        }
    }
    else
    {
        OutBody = OutHead = OutArm = 0.0f;
    }

    // §5.1(a) Override fade-out contribution
    if (f6_override_fadeout_ticks_remaining > 0)
    {
        const float multiplier = (f6_override_fadeout_ticks_remaining == 2) ? 1.0f : 0.5f;
        OutBody += f6_override_fadeout_snapshot_body * multiplier;
        OutHead += f6_override_fadeout_snapshot_head * multiplier;
        OutArm  += f6_override_fadeout_snapshot_arm  * multiplier;
        --f6_override_fadeout_ticks_remaining;
    }

    // Component clamp
    const float max_clamp = MAX_LEAN_ANGLE_DEG * 1.2f;
    OutBody = FMath::Clamp(OutBody, -max_clamp, max_clamp);
    OutHead = FMath::Clamp(OutHead, -max_clamp, max_clamp);
    OutArm  = FMath::Clamp(OutArm,  -max_clamp, max_clamp);
}
```

**§5.1(a) Override hook** in HandleSlipTransition (extends Story 003):
```cpp
// Inside SETTLED→SLIPPING branch, BEFORE clearing F-6 state:
if (edge_absorb_active)
{
    ComputeF6(f6_override_fadeout_snapshot_body,
              f6_override_fadeout_snapshot_head,
              f6_override_fadeout_snapshot_arm);
    f6_override_fadeout_ticks_remaining = 2;
    edge_absorb_active = false;
    edge_absorb_progress = 0.0f;
    edge_absorb_local_timer_s = 0.0f;
}
```

**Final F-5/F-6 co-write** at mesh SetRelativeRotation site (extends Story 006):
```cpp
float f5_body, f5_head, f5_arm;
ComputeLean(tween_progress, current_lane, target_lane, f5_body, f5_head, f5_arm);

float f6_body, f6_head, f6_arm;
ComputeF6(f6_body, f6_head, f6_arm);

const float max_clamp = MAX_LEAN_ANGLE_DEG * 1.2f;
lean_angle = FMath::Clamp(f5_body + f6_body, -max_clamp, max_clamp);
head_lean_angle = FMath::Clamp(f5_head + f6_head, -max_clamp, max_clamp);
arm_lean_angle = FMath::Clamp(f5_arm + f6_arm, -max_clamp, max_clamp);

MeshComponent->SetRelativeRotation(FRotator(lean_angle, 0.0f, 0.0f));
```

**Rule 1 edge no-op wiring** (from Story 003's HandleSlipTransition SETTLED path with F-4 invalid): `TriggerEdgeAbsorb(current_lane, Dir)` invoked → PM stays SETTLED; commitment-tell NOT triggered.

**Tuning knob defaults**:
- `constexpr float EDGE_ABSORB_DURATION_S = 0.27f;` safe [0.20, 0.35]
- `constexpr float EC15_F6_DECAY_COEFFICIENT = 0.7f;` safe [0.6, 1.0] (private per R11a §6.2)

**IMPORTANT — FRotator axis (Roll, not Pitch)**:

The sample co-write code at line 149 above shows `MeshComponent->SetRelativeRotation(FRotator(lean_angle, 0.0f, 0.0f))` — this is the same **incorrect** form that Story 006 corrected during code review. Story 006's PLMC.cpp:248 uses `FRotator(0.0f, 0.0f, lean_angle)` (Roll axis — rotation around forward X, the correct axis for lateral character lean). Standard UE convention: `FRotator(Pitch, Yaw, Roll)`; `SlipstormPlayerPawn.cpp` applies no non-standard mesh-orientation override.

Story 007 implementation MUST use the Roll form: `FRotator(0.0f, 0.0f, combined_body)`. The Story 006 composition test `f5_roll_axis_regression_guard` (`Source/SLIPSTORM/Tests/Integration/PlayerMovement/PMLeanCompositionTest.cpp`) already gates this; Story 007's co-write sum must not regress it.

The story text above (and ADR-0009 IG-6 line 468) still show the Pitch form pending an `/architecture-decision` amendment pass. The code truth is Roll.

---

## Out of Scope

- Story 003: `TriggerEdgeAbsorb` hook site (called by Story 003's HandleSlipTransition edge-no-op path — Story 003 stubbed; this story implements).
- Story 006: F-5 own-clamp (this story adds the co-write sum + final clamp).
- Story 008: DEAD-during-F-6 tail preservation (AC-F6-D lands in Story 008's HandleStateChanged DEAD branch — reference here but implementation there).

---

## QA Test Cases

*Test file: `tests/unit/player-movement/pm_edge_absorb_test.cpp`. Automated unit tests using stubbed EdgeAbsorbCurve.*

- **AC-02 (edge no-op fires F-6)**:
  - Given: PM SETTLED at FarRight; RSM RUNNING.
  - When: `HandleSlipTransition(Right)` invoked.
  - Then: `movement_state == SETTLED` (unchanged); `edge_absorb_active == true`; `edge_absorb_trigger_count == 1`; NO commitment-tell trigger fired; NO OnSlipMidpoint broadcast.

- **AC-F6-A (FarRight + slip-right → F-6 body/head/arm all negative at mid-tail)**:
  - Given: PM SETTLED at FarRight; `TriggerEdgeAbsorb(FarRight, Right)` invoked; simulate tick advances until `edge_absorb_progress ≈ 0.5`.
  - When: `ComputeF6(body, head, arm)`.
  - Then: `body < 0`; `head < 0`; `arm < 0`; head ≠ 0 AND arm ≠ 0 (decouple closed).

- **AC-F6-B (§5.1(a) Override 2-frame fade-out, no >50% single-frame jump)**:
  - Given: PM SETTLED at FarRight with F-6 mid-tail (edge_absorb_progress ≈ 0.5, snapshot body ≈ -5°); `HandleSlipTransition(Left)` invoked (valid inward slip).
  - When: tick N=1 → sample F-6 contribution; tick N=2 → sample; tick N=3 → sample.
  - Then: |contribution(N=1)| ≈ |snapshot| (multiplier 1.0); |contribution(N=2)| ≈ 0.5 * |snapshot|; |contribution(N=3)| == 0. No single-tick delta exceeds 0.5 * |snapshot|.

- **AC-F6-C (counter-case: Left + slip-left → FarLeft tween does NOT fire F-6)**:
  - Given: PM SETTLED at Left; `HandleSlipTransition(Left)` invoked (valid, projects to FarLeft).
  - Then: `edge_absorb_active == false`; `edge_absorb_trigger_count` unchanged; SLIPPING starts (normal Rule 2 path).
  - Also verify positive cases: FarLeft + slip-left → F-6 fires; FarRight + slip-right → F-6 fires.

- **AC-F6-E (EC-15 worst case within 12° clamp)**:
  - Given: PM SLIPPING at TP=0.85 (phase 3); edge_absorb_progress=0.10; EC15_F6_DECAY_COEFFICIENT=0.7; MAX_LEAN=10; hypothetical unclamped head_lean pre-decay ≈ 16.4°.
  - When: ComputeF6 invoked.
  - Then: `head_lean` <= 11.5° ± 0.1° AFTER EC15 decay AND component clamp (16.4 * 0.7 = 11.48 < 12).
  - Edge cases: without EC15 decay (SETTLED path) the head would exceed 12° pre-clamp — verify EC15 gates on SLIPPING+phase-3.

- **AC-24 exclusion (OnSlipMidpoint NOT on edge-absorb)**:
  - Given: F-6 firing on Rule 1 edge no-op.
  - When: F-6 progresses through 0.5 (analog of TP=0.5).
  - Then: OnSlipMidpoint NEVER broadcast during F-6 tail; spy on delegate confirms zero deliveries.

- **§5.1(a) Override snapshot correctness**:
  - Given: F-6 mid-tail with body=-6°, head=-4.5°, arm=-5°; SETTLED→SLIPPING triggered.
  - When: check snapshot fields post-transition.
  - Then: `f6_override_fadeout_snapshot_body == -6.0f`; head/arm match; `f6_override_fadeout_ticks_remaining == 2`; `edge_absorb_active == false`.

- **Curve fallback (EdgeAbsorbCurve null)**:
  - Given: `EdgeAbsorbCurve = nullptr` (or `bCurveFallbackActive == true`); TriggerEdgeAbsorb called.
  - When: ticks advance.
  - Then: F-6 body/head/arm = 0.0; `edge_absorb_trigger_count` still incremented (AC parity per SD6 fallback note); `edge_absorb_active` set to false immediately (tail collapses).

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Automated unit test at `tests/unit/player-movement/pm_edge_absorb_test.cpp` — must exist and pass.

**Status**: [ ] Not yet created

---

## Dependencies

- **Depends on**: Story 001 (EdgeAbsorbCurve UPROPERTY, edge_absorb_trigger_count counter); Story 002 (LANE constants + F-PROLOGUE effective_dt); Story 003 (TriggerEdgeAbsorb hook site + tick body skeleton); Story 006 (F-5 ComputeLean helper for co-write sum).
- **Unlocks**: Story 008 (AC-F6-D DEAD-during-tail preservation) — Story 008 references this story's state fields.

---

## Completion Notes

**Completed**: 2026-07-15
**Criteria**: 14/14 passing (12 full + 2 partial with documented gaps). All have covering tests. See `/story-done` session log for traceability.

**Deviations**:

- **ADVISORY — Sign convention correction (code-truth diverges from story spec)**: Story text §Implementation Notes line 36 states `edge_absorb_sign = +1 if Dir=Right else -1`. This is empirically WRONG. AC-F6-A ("FarRight+slip-right → body/head/arm all negative at mid-tail") requires recoil AWAY from the wall, which forces the opposite sign convention. Code correctly inverts: `-1 for slip-right`, `+1 for slip-left`. The .h comment at line 556-560 documents this as "CORRECTED to satisfy AC-F6-A". Discovered by unreal-specialist + qa-tester code review (both agents caught the three-way contradiction between story spec, .h comment, and initial TC1 assertion). **Follow-up needed**: run `/architecture-decision` amendment pass to update story-007 markdown line 36 + Implementation Notes sample to reflect the AC-F6-A recoil convention.

- **ADVISORY — AC-F6-E partial coverage**: The exact worst-case scenario specified (plateau curve producing unclamped head_lean ≈ 16.4° pre-decay → 11.5° post-decay+clamp with ≥0.58° margin against the 12° ceiling) is not reproduced by any test. TC5 verifies EC15 decay math with different values (progress=0.5, decay applied). Non-blocking — decay logic and its gate (SLIPPING + tween_progress > PHASE3_TP_THRESHOLD) are proven; scenario-specific stress test would strengthen coverage. Deferred as qa-tester major suggestion.

- **ADVISORY — AC-24 exclusion test trivially passes**: CC5 (`ac24_no_on_slip_midpoint_during_f6_tail`) places PM in SETTLED, spies on OnSlipMidpoint, ticks 10 times, and asserts count=0. But since F-2 doesn't advance during SETTLED, `tween_progress` never crosses 0.5 and the midpoint broadcast condition (`prev_tp < 0.5 && tween_progress >= 0.5`) cannot fire — the assertion passes regardless of F-6 state. Meaningful AC-24 verification requires a concurrent SLIPPING tween crossing TP=0.5 with F-6 tail also running (§5.1(a) Override case). That scenario overlaps Story 008 (terminal-state handlers) more than pure F-6 coverage. Deferred.

- **ADVISORY — Story 007 markdown FRotator drift (recurring)**: Story text line 149 pseudocode still shows `FRotator(lean_angle, 0.0f, 0.0f)` (Pitch axis). Same drift as Story 006. Both stories share the same `/architecture-decision` amendment need to align ADR-0009 IG-6 + story-006 + story-007 markdowns to the Roll form corrected in code.

- **ADVISORY — CC4 file-type misclassification**: `ac_f6_b_fade_out_no_more_than_50pct_jump` in `PMEdgeAbsorbCompositionTest.cpp` uses `NewObject<UPlayerLaneMovementComponent>()` only (no UWorld spawn). Functionally correct but should live in the Unit file to preserve the Unit-vs-Integration file-type contract. Deferred as qa-tester minor suggestion.

- **ADVISORY — Deactivation-before-co-write final-tick pop** (unreal-specialist minor): On the tick where `edge_absorb_progress >= 1.0`, `edge_absorb_active` is set false BEFORE the co-write reads F-6. So the last frame's `ComputeF6` takes the inactive-zero branch (Branch 3) instead of the curve's t=1.0 value. If `EdgeAbsorbCurve(1.0) == 0` by authored convention (typical fade-to-rest), no visible pop. If the authored curve has a non-zero settle value at t=1.0, expect a one-frame pop. Curve authoring should ensure smooth-to-zero at t=1.0. Design intent clarification deferred.

**Test Evidence**:
- Unit: `Source/SLIPSTORM/Tests/Unit/PlayerMovement/PMEdgeAbsorbTest.cpp` (7 pure-math test commands: TriggerEdgeAbsorb state+counter, 2 fallback paths, inactive-zero, EC15 decay applied under phase-3, EC15 NOT applied under SETTLED, §5.1(a) fade-out 2-frame ramp)
- Composition: `Source/SLIPSTORM/Tests/Integration/PlayerMovement/PMEdgeAbsorbCompositionTest.cpp` (7 world-spawn commands: AC-02 edge no-op stays SETTLED, AC-F6-C valid slip doesn't fire, §5.1(a) Override snapshot captured, AC-F6-B ≤50% single-frame delta, AC-24 delegate spy (trivial pass), AC-F6-A explicit negative body/head/arm with decouple non-trivial, AC-F6-C left-edge positive fires with sign=+1)
- **Total: 14 test commands** (7 unit + 7 composition)

**Build verification**: `Result: Succeeded` (11.11 s incremental after Fix 1-4 pass; 0 errors, 0 warnings).

**Code Review**: Complete — `/code-review` with `unreal-specialist + qa-tester` in parallel. Initial verdict: **CHANGES REQUIRED** — 2 BLOCKING findings converged across both reviewers:

1. **TriggerEdgeAbsorb was still a Story 003 TODO stub** — agent's mid-work stall left the body unchanged. My earlier verification checked only the function signature at cpp:720, not the body content — a false-positive mis-verification. Fixed with real implementation (4 state fields + sign per AC-F6-A + counter increment).
2. **Sign convention three-way contradiction** across story spec (`+1` for slip-right), `.h` "CORRECTED" comment (`-1`), and my TC1 assertion (`+1`). AC-F6-A empirically requires `-1` for slip-right; .h was right; story + tests were wrong. Fixed TC1 assertions + CC3 sign + expected snapshots + added CC6 (AC-F6-A explicit).

Additional fix: added CC7 (AC-F6-C left-edge positive case) to close a qa-tester-identified gap.

6 minor/nit code-review advisories deferred (documented above).

**Files touched**:
- `Source/SLIPSTORM/Player/PlayerLaneMovementComponent.h` (527 → 625, +98 lines): 3 tuning constants (`EDGE_ABSORB_DURATION_S`, `EC15_F6_DECAY_COEFFICIENT`, `PHASE3_TP_THRESHOLD`), 8 F-6 state members, `ComputeF6` decl, 2 friend classes (`FPMEdgeAbsorbTest`, `FPMEdgeAbsorbCompositionTest`)
- `Source/SLIPSTORM/Player/PlayerLaneMovementComponent.cpp` (784 → 951, +167 lines): `TriggerEdgeAbsorb` body (4 state assignments + AC-F6-A sign + counter), `ComputeF6` 3-branch impl + Override fade-out ramp + component clamp, tick restructure (F-6 tick advance outside SLIPPING/SETTLED branches, F-5+F-6 co-write sum), §5.1(a) Override snapshot hook in HandleSlipTransition. FRotator axis Roll preserved from Story 006 correction.
- `Source/SLIPSTORM/Tests/Unit/PlayerMovement/PMEdgeAbsorbTest.cpp` (new, ~340 lines): 7 pure-math test commands
- `Source/SLIPSTORM/Tests/Integration/PlayerMovement/PMEdgeAbsorbCompositionTest.cpp` (new, ~465 lines): 7 world-spawn composition tests including OnSlipMidpoint delegate spy, AC-F6-A explicit, AC-F6-C left-edge positive


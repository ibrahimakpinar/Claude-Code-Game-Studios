# Story 006: F-5 body/head/arm lean + LeanCurve + staggered offsets + SetRelativeRotation clamp

> **Epic**: Player Movement (Slip)
> **Status**: Complete
> **Layer**: Core
> **Type**: Logic
> **Estimate**: 3–4 hours
> **Manifest Version**: 2026-07-03 (docs/registry/architecture.yaml v8)
> **Last Updated**: 2026-07-13

## Context

**GDD**: `design/gdd/player-movement-mechanics.md` (§4 F-5 rotation output, §Authored Asset Contracts LeanCurve, §8 AC-26/27/30).
**Requirement**: `TR-PM-008`, `TR-PM-029` (HEAD_LAG / ARM_LEAD staggered offsets), `TR-PM-033` (±(MAX_LEAN×1.2) clamp).
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0009 (SD5 lean via `SetRelativeRotation` on mesh + SD6 curve fallback path).
**ADR Decision Summary**: Body/head/arm lean is computed from LeanCurve at staggered progress offsets (`HEAD_LAG_PROGRESS = 0.10`, `ARM_LEAD_PROGRESS = 0.05`). Applied via `MeshComponent->SetRelativeRotation(FRotator(lean_angle, 0, 0))`. Pawn root rotation stays at identity — rotating the root is FORBIDDEN (breaks Rule 2 collision commitment).

**Engine**: Unreal Engine 5.7 | **Risk**: LOW (`SetRelativeRotation` stable pre-cutoff; `UCurveFloat::GetFloatValue` stable pre-cutoff)
**Engine Notes**: None post-cutoff.

**Control Manifest Rules (this layer)** — from `docs/registry/architecture.yaml` v8:
- **FORBIDDEN**: `PlayerMovement_SetActorRotation_for_lean` — lean applied via `MeshComponent->SetRelativeRotation` ONLY. Pawn root stays at identity rotation (ADR-0009 IG-6 + Rule 2 collision-commit invariant).
- **Required**: Final F-5 lean values clamped to ±(MAX_LEAN_ANGLE_DEG × 1.2) after F-5+F-6 co-write summation (Story 007 finishes the sum; this story clamps its own F-5 output component into the co-write pipeline).
- **Required**: LeanCurve null → zero-lean fallback + `bCurveFallbackActive == true` (already set by Story 001 BeginPlay if the curve was null).

---

## Acceptance Criteria

*From `design/gdd/player-movement-mechanics.md` §4 F-5 + §8, scoped to this story:*

- [ ] F-5 helper: `void ComputeLean(float TweenProgress, EPlayerLane FromLane, EPlayerLane ToLane, float& OutBodyLean, float& OutHeadLean, float& OutArmLean)`.
- [ ] Body lean: `body_curve_t = LeanCurve->GetFloatValue(TweenProgress)`; `body_lean_deg = body_curve_t * MAX_LEAN_ANGLE_DEG * DirectionSign(FromLane, ToLane)` where `DirectionSign` is +1 if ToLane index > FromLane, else -1 (rightward slip → positive lean).
- [ ] Head staggered: `head_curve_t = LeanCurve->GetFloatValue(FMath::Max(0.0f, TweenProgress - HEAD_LAG_PROGRESS))`; `head_lean_deg = head_curve_t * MAX_LEAN_ANGLE_DEG * DirectionSign(...)`. `HEAD_LAG_PROGRESS = 0.10` default (safe [0.08, 0.12]).
- [ ] Arm staggered: `arm_curve_t = LeanCurve->GetFloatValue(FMath::Min(1.0f, TweenProgress + ARM_LEAD_PROGRESS))`; `arm_lean_deg = arm_curve_t * MAX_LEAN_ANGLE_DEG * DirectionSign(...)`. `ARM_LEAD_PROGRESS = 0.05` default (safe [0.03, 0.08]).
- [ ] Fallback (LeanCurve null OR `bCurveFallbackActive`): `body_lean = head_lean = arm_lean = 0.0f`.
- [ ] Final component-level clamp: `body_lean = FMath::Clamp(body_lean, -MAX_LEAN_ANGLE_DEG * 1.2f, MAX_LEAN_ANGLE_DEG * 1.2f)`. Same for head/arm. (F-6 co-write in Story 007 adds its contribution, then re-clamps the final sum — this story clamps its OWN F-5 contribution before writing.)
- [ ] Publish `lean_angle`, `head_lean_angle`, `arm_lean_angle` public properties.
- [ ] Write: `MeshComponent->SetRelativeRotation(FRotator(body_lean, 0, 0))` per SLIPPING tick (extending Story 003's TickComponent skeleton after F-3's SetRelativeLocation).
- [ ] **AC-26 (SETTLED + no F-6 → zero lean)**: `movement_state == SETTLED` AND edge_absorb inactive → `lean_angle ≤ 0.01f` (F-6 co-write may produce non-zero while SETTLED via F-6 — but WITHOUT F-6 active, F-5 alone returns zero).
- [ ] **AC-27 (Tween @ TP=0.20 with LeanCurve=1.0)**: `|body_lean| == MAX_LEAN_ANGLE_DEG ± 0.1°`; head lag → `head_lean` sampled at TP=0.10 which returns curve value at 0.10; arm lead → `arm_lean` sampled at TP=0.25 which returns curve value at 0.25.
- [ ] **AC-30 (SETTLED + F-6 inactive → arm_lean_angle ≤ 0.01°)**: independent verification for arm.

---

## Implementation Notes

*Derived from ADR-0009 SD5 + SD6 + IG-6 + mechanics §4 F-5:*

**F-5 formula site** (mechanics §4):
```cpp
float DirectionSign(EPlayerLane From, EPlayerLane To)
{
    const int32 delta = static_cast<int32>(To) - static_cast<int32>(From);
    return (delta > 0) ? 1.0f : -1.0f;
}

void UPlayerLaneMovementComponent::ComputeLean(float TP, EPlayerLane FromLane, EPlayerLane ToLane,
                                                float& OutBody, float& OutHead, float& OutArm)
{
    if (!LeanCurve || bCurveFallbackActive)
    {
        OutBody = OutHead = OutArm = 0.0f;
        return;
    }

    const float sign = DirectionSign(FromLane, ToLane);
    const float body_t = LeanCurve->GetFloatValue(TP);
    const float head_t = LeanCurve->GetFloatValue(FMath::Max(0.0f, TP - HEAD_LAG_PROGRESS));
    const float arm_t  = LeanCurve->GetFloatValue(FMath::Min(1.0f, TP + ARM_LEAD_PROGRESS));

    const float max_clamp = MAX_LEAN_ANGLE_DEG * 1.2f;
    OutBody = FMath::Clamp(body_t * MAX_LEAN_ANGLE_DEG * sign, -max_clamp, max_clamp);
    OutHead = FMath::Clamp(head_t * MAX_LEAN_ANGLE_DEG * sign, -max_clamp, max_clamp);
    OutArm  = FMath::Clamp(arm_t  * MAX_LEAN_ANGLE_DEG * sign, -max_clamp, max_clamp);
}
```

**Tick-body write site** (extends Story 003):
```cpp
// After F-3's SetRelativeLocation, before F-6:
float body_lean, head_lean, arm_lean;
ComputeLean(tween_progress, current_lane, target_lane, body_lean, head_lean, arm_lean);

// F-6 co-write (Story 007) adds edge_absorb contribution here.
// Final SetRelativeRotation applied after F-6 sum. In this story, apply body_lean directly:
MeshComponent->SetRelativeRotation(FRotator(body_lean, 0.0f, 0.0f));
lean_angle = body_lean;
head_lean_angle = head_lean;
arm_lean_angle = arm_lean;
```

**Head/arm rotation write** (mechanics §3): the body_lean drives the mesh root; head_lean and arm_lean are exposed as public properties for skeletal animation Blueprint to consume (SetRelativeRotation on nested sub-components is out of scope — the mesh has anim blueprint responsibility for skeletal control per Art Bible).

**Tuning knob defaults** (mechanics §7):
- `constexpr float MAX_LEAN_ANGLE_DEG = 10.0f;` safe [8, 12]
- `constexpr float HEAD_LAG_PROGRESS = 0.10f;` safe [0.08, 0.12]
- `constexpr float ARM_LEAD_PROGRESS = 0.05f;` safe [0.03, 0.08]

---

## Out of Scope

- Story 004: F-3 lateral interpolation (called earlier in the tick body).
- Story 007: F-6 edge-absorb tail + F-5/F-6 co-write final sum + fade-out override.
- Skeletal-animation Blueprint consumption of head_lean_angle / arm_lean_angle (Art / anim BP concern).

---

## QA Test Cases

*Test file: `tests/unit/player-movement/pm_lean_test.cpp`. Automated pure-math unit tests.*

- **AC-26 (SETTLED + no F-6 → body lean == 0)**:
  - Given: PM SETTLED at Center; F-6 inactive; last write was CompleteTween's snap (Story 008 will confirm — but F-5 not invoked in SETTLED per Story 003 gate).
  - When: verify `lean_angle` public read.
  - Then: `lean_angle` ≈ 0.0 ± 0.01°.
  - Edge cases: also verify `head_lean_angle` and `arm_lean_angle` == 0.

- **AC-27 (Tween @ TP=0.20 with LeanCurve returning 1.0)**:
  - Given: SLIPPING Left→Center (source=Left index 1, target=Center index 2 → sign = +1); LeanCurve returning 1.0 at TP=0.20; MAX_LEAN_ANGLE_DEG = 10.0.
  - When: `ComputeLean(0.20, Left, Center, body, head, arm)`.
  - Then: `body ≈ +10.0° ± 0.1°`; `head` = LeanCurve(0.10) * 10 * sign; `arm` = LeanCurve(0.25) * 10 * sign.
  - Edge cases: (a) same with Center→Left (sign = -1) → body ≈ -10.0°; (b) LeanCurve returning 1.05 at TP=0.20 → body clamped to +12.0° (= 10 * 1.2).

- **AC-30 (arm_lean_angle == 0 when SETTLED + F-6 inactive)**:
  - Given: SETTLED at Right, no F-6.
  - When: read `arm_lean_angle`.
  - Then: ≈ 0.0 ± 0.01°.

- **HEAD_LAG staggered offset**:
  - Given: TP=0.15, HEAD_LAG_PROGRESS=0.10, LeanCurve returning identity `y=x`.
  - When: `ComputeLean(0.15, Left, Center, body, head, arm)` invoked.
  - Then: `head_curve_t == max(0, 0.15 - 0.10) == 0.05`; head_lean = 0.05 * 10.0 = 0.5° ± 0.05°.
  - Edge cases: TP=0.05 → head sampled at max(0, -0.05) = 0.0 → head_lean = 0.

- **ARM_LEAD staggered offset**:
  - Given: TP=0.80, ARM_LEAD_PROGRESS=0.05, LeanCurve identity.
  - When: `ComputeLean(0.80, ...)`.
  - Then: `arm_curve_t == min(1.0, 0.80 + 0.05) == 0.85`; arm_lean ≈ 8.5° ± 0.05° sign-corrected.
  - Edge cases: TP=0.98 → arm sampled at min(1.0, 1.03) = 1.0 → arm_lean = ±MAX_LEAN = ±10.0°.

- **±(MAX_LEAN×1.2) final clamp**:
  - Given: LeanCurve returning 1.5 (out of authored range).
  - When: ComputeLean invoked.
  - Then: body, head, arm all clamped to ±12.0° exactly.

- **Curve fallback (LeanCurve null)**:
  - Given: `LeanCurve = nullptr` or `bCurveFallbackActive == true`.
  - When: ComputeLean invoked at TP=0.5.
  - Then: OutBody = OutHead = OutArm = 0.0.

- **`SetActorRotation` grep gate**:
  - Grep gate: `PlayerMovement_SetActorRotation_for_lean` grep across `Source/SLIPSTORM/**/PlayerMovement*.cpp` returns 0 hits.

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Automated unit test at `tests/unit/player-movement/pm_lean_test.cpp` — must exist and pass.
- Grep gate on `SetActorRotation` forbidden pattern.

**Status**: [ ] Not yet created

---

## Dependencies

- **Depends on**: Story 001 (LeanCurve UPROPERTY, MeshComponent reference, bCurveFallbackActive); Story 002 (LANE constants); Story 003 (TickComponent skeleton — this story adds the F-5 site into it).
- **Unlocks**: Story 007 (F-6 co-writes with F-5; final sum + clamp behavior); anim-BP consumption of head/arm angles (out of epic).

---

## Completion Notes

**Completed**: 2026-07-13
**Criteria**: 11/11 passing — all COVERED by automated tests (9 pure-math unit + 3 world-spawn composition). See `/story-done` session log for traceability table.

**Deviations**:

- **ADVISORY — FRotator axis drift (code corrected; spec/ADR pending amendment)**: The implementation at `PlayerLaneMovementComponent.cpp:248` uses `SetRelativeRotation(FRotator(0.0f, 0.0f, body_lean_deg))` — body_lean applied as **Roll** (rotation around forward X axis, standard UE convention for left/right lean). The story text (§Implementation Notes lines 18, 41, 89) and ADR-0009 IG-6 line 468 both document the incorrect `FRotator(body_lean, 0, 0)` form — that applies body_lean as Pitch (nose-forward/back nod), which is wrong for a lateral slip animation. Standard UE convention is authoritative: `FRotator(Pitch, Yaw, Roll)`; `SlipstormPlayerPawn.cpp` applies no non-standard mesh-orientation override; forward=+X, right=+Y, up=+Z is in effect. The bug was discovered by unreal-specialist during `/code-review` and would only be visible via playtest (all 9 unit tests inspect float outputs of `ComputeLean`, none observed the resulting `FRotator` on the mesh). Fix applied to code with explanatory comment at PLMC.cpp:249-256. **Follow-up needed**: run `/architecture-decision` to amend ADR-0009 IG-6 line 468 from Pitch form to Roll form; update story-006 markdown lines 18, 41, 89 accordingly. New composition test `f5_roll_axis_regression_guard` (CC3) prevents future regression.

- **ADVISORY — Test constants (Fix 4 deferred)**: qa-tester recommended replacing hard-coded literals (`10.0f`, `12.0f`, `0.10f`, `0.05f`) in `PMLeanTest.cpp` assertions with `UPlayerLaneMovementComponent::MAX_LEAN_ANGLE_DEG`, `HEAD_LAG_PROGRESS`, `ARM_LEAD_PROGRESS` references (accessible via friend). Provides compile-time coupling between tuning changes and test assertions. Deferred as minor tech debt — swap is broad (~20 sites) and non-blocking. Recommend addressing in a future consolidated test-hygiene pass.

- **ADVISORY — Test path drift (recurring)**: Story's `## Test Evidence` section names `tests/unit/player-movement/pm_lean_test.cpp` (aspirational repo-root path). Actual test files live at `Source/SLIPSTORM/Tests/Unit/PlayerMovement/PMLeanTest.cpp` (pure-math) and `Source/SLIPSTORM/Tests/Integration/PlayerMovement/PMLeanCompositionTest.cpp` (composition — added via Fix 2). Same recurring pattern as Stories 004 and 005 — needs project-wide story-template fix.

**Test Evidence**:
- Unit: `Source/SLIPSTORM/Tests/Unit/PlayerMovement/PMLeanTest.cpp` (9 test commands: `lean_tp02_leancurve_1_body_10deg`, `lean_tp02_center_to_left_negative_sign`, `lean_max_clamp_1_5_curve`, `head_lag_staggered_offset`, `head_lag_clamped_to_zero`, `arm_lead_staggered_offset`, `arm_lead_clamped_to_one`, `fallback_null_curve`, `direction_sign_all_transitions`)
- Composition: `Source/SLIPSTORM/Tests/Integration/PlayerMovement/PMLeanCompositionTest.cpp` (3 test commands: `ac26_settled_zero_lean_after_tick`, `ac30_settled_arm_lean_zero_after_tick`, `f5_roll_axis_regression_guard`)

**Build verification**: `Result: Succeeded` (9.83 s incremental; 0 errors, 0 warnings) — Story 006 code compiles and links cleanly in the SLIPSTORM Editor target.

**Code Review**: Complete — `/code-review` with `unreal-specialist + qa-tester` in parallel. Initial verdict: **CHANGES REQUIRED** (2 BLOCKING findings). User chose option A → Fixes 1, 2, 3 applied inline:
1. FRotator axis corrected from Pitch to Roll (`PLMC.cpp:248` + 8-line explanatory comment noting spec/ADR drift)
2. Composition test file created (`PMLeanCompositionTest.cpp` — 3 commands closing AC-26 / AC-30 coverage gap + Roll-axis regression guard)
3. Body input clamped to `[0, 1]` for hitch symmetry with head/arm inputs (`PLMC.cpp:775`)

Fix 4 (test-constant symbol references) deferred as minor. ADR/story spec amendment deferred per user's option C. Re-code-review not run.

**Files touched**:
- `Source/SLIPSTORM/Player/PlayerLaneMovementComponent.h` (461 → 527, +66): 3 tuning constants (`MAX_LEAN_ANGLE_DEG`, `HEAD_LAG_PROGRESS`, `ARM_LEAD_PROGRESS`), `ComputeLean` + `DirectionSign` decls, `friend class FPMLeanTest`
- `Source/SLIPSTORM/Player/PlayerLaneMovementComponent.cpp` (617 → 784+, +167 for Story 006 + code-review fixes): F-5 tick-site write with Roll-axis correction, SETTLED zero-lean, `DirectionSign` + `ComputeLean` impls with body-input clamp
- `Source/SLIPSTORM/Tests/Unit/PlayerMovement/PMLeanTest.cpp` (new, ~21 KB): 9 pure-math test commands
- `Source/SLIPSTORM/Tests/Integration/PlayerMovement/PMLeanCompositionTest.cpp` (new, ~230 lines): 3 world-spawn composition tests


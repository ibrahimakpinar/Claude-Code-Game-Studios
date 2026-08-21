---
story: production/epics/player-movement/story-004-f3-lateral-interpolation.md
tr_ids: [TR-PM-007, TR-PM-026, TR-PM-034]
adrs:
  - docs/architecture/adr-0009-player-movement-hosting.md
cpp_test: Source/SLIPSTORM/Tests/Unit/PlayerMovement/PMLateralInterpolationTest.cpp
cpp_composition_test: Source/SLIPSTORM/Tests/Integration/PlayerMovement/PMLateralInterpolationCompositionTest.cpp
gdd_refs:
  - design/gdd/player-movement-mechanics.md §4 F-3 + §8 AC-03 + §Authored Asset Contracts (SlipCurve)
test_category: SLIPSTORM.PlayerMovement.LateralInterpolation
composition_test_category: SLIPSTORM.PlayerMovement.LateralInterpolationComposition
guard: "#if WITH_DEV_AUTOMATION_TESTS (unit — no WITH_EDITOR); #if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS (composition)"
---

# PM Lateral Interpolation — Test Spec (Story 004)

## Overview

Story 004 implements F-3 lateral interpolation: `UPlayerLaneMovementComponent`
now drives the player mesh position during a lane-slip tween by calling
`CachedMeshComponent->SetRelativeLocation(FVector(F3RelativeOffset(tween_progress), 0, 0))`
on every tick while `movement_state == SLIPPING`.

The public `lateral_world_position` field is written on every SLIPPING tick
(= `GetOwner()->GetActorLocation().X + rel_x`) and on every SETTLED tick
(= `LaneWorldX(current_lane)`) so Camera and Pull-Wave systems always have a
fresh lateral reference even when no tween is active.

Key moving parts:

- **`F3RelativeOffset(float TweenProgress) const`** — private pure-math helper.
  Returns `FMath::Lerp(delta, 0.0f, curve_t)` where `delta = LaneWorldX(current_lane) - LaneWorldX(target_lane)`.
  `curve_t` is read from `SlipCurve->GetFloatValue(TweenProgress)` (clamped [0,1]),
  or falls back to `FMath::Clamp(TweenProgress, 0, 1)` when `SlipCurve` is null
  or `bCurveFallbackActive == true` (ADR-0009 SD6).
- **`CachedMeshComponent`** — `TObjectPtr<UStaticMeshComponent>` resolved at `BeginPlay`.
  Tick site is null-safe: skips the mesh write if unresolved.
- **`lateral_world_position`** — public `float` written by F-3 and by the SETTLED
  else-branch. Consumers (Camera, Pull-Wave) read it as the authoritative lateral position.
- **Curve fallback path** — both conditions (`SlipCurve == nullptr` OR
  `bCurveFallbackActive == true`) divert to linear `curve_t`. This preserves the
  TP≥1.0 zero-relative invariant across all paths.

---

## TC1 — F-3 Lerp, identity curve, Left→Center (`f3_lerp_identity_curve`)

**Given**: `current_lane = Left` (X=-100), `target_lane = Center` (X=0).
`SlipCurve` = identity curve with keys (0,0),(1,1); `bCurveFallbackActive = false`.
delta = -100 − 0 = -100.

**When**: `F3RelativeOffset(TP)` called at TP = 0.0, 0.25, 0.5, 1.0.

**Then** (`FMath::Lerp(-100, 0, curve_t)`):

| TP   | curve_t | Expected rel_x | Tolerance |
|------|---------|----------------|-----------|
| 0.0  | 0.0     | -100.0         | ±0.001    |
| 0.25 | 0.25    | -75.0          | ±0.001    |
| 0.5  | 0.5     | -50.0          | ±0.001    |
| 1.0  | 1.0     |   0.0          | exact     |

**Edge cases**: TP=1.0 asserts exact equality (zero-relative invariant; see Design Invariant section).

---

## TC2 — F-3 Lerp, authored curve (`f3_lerp_authored_curve`)

**Given**: `current_lane = Left`, `target_lane = Center`. Authored `SlipCurve` with keys
(0, 0), (0.5, 0.35), (1, 1). `bCurveFallbackActive = false`.

**When**: `F3RelativeOffset(0.5f)` called.

**Then**: `curve_t ≈ 0.35` → `FMath::Lerp(-100, 0, 0.35) = -65.0`. Expected: -65.0 ± 1.0.

**Edge cases**: tolerance ±1.0 accommodates segment interpolation rounding inside
`FRichCurve::Eval` across the (0,0)-(0.5,0.35) Hermite segment.

---

## TC3 — TP=1.0 zero-relative invariant (`f3_tp1_zero_relative_invariant`)

**Given**: identity `SlipCurve`; `bCurveFallbackActive = false`.
All 5×5 = 25 lane transitions; 20 non-self pairs (source ≠ target).

**When**: `F3RelativeOffset(1.0f)` called for each of the 20 non-self transitions.

**Then**: return value == 0.0f for every pair.

**Design invariant proof**: at TP=1.0, identity curve yields `GetFloatValue(1.0) = 1.0`,
clamped to 1.0. `FMath::Lerp(delta, 0.0f, 1.0f)` returns exactly 0.0f by IEEE 754:
result = `delta*(1−1) + 0.0f*1 = 0`. No absolute-position clamp is needed.

**Grep gate note** (in test file):
> `F3RelativeOffset` must contain no absolute-position clamp — only `FMath::Clamp` on `curve_t`. See ADR-0009 IG-5.

---

## TC4 — Fallback path, null SlipCurve (`f3_fallback_null_curve`)

**Given**: `current_lane = Left`, `target_lane = Center`.
`SlipCurve = nullptr`; `bCurveFallbackActive = true`.

**When**: `F3RelativeOffset(0.5f)` called.

**Then**: `curve_t = FMath::Clamp(0.5, 0, 1) = 0.5` → result = -50.0 ± 0.001.

**Also assert**: `bCurveFallbackActive` is unchanged after the call (`F3RelativeOffset` is a pure reader — it writes nothing to `*this`).

---

## TC5 — Fallback path, flag overrides valid curve (`f3_fallback_active_flag`)

**Given**: `current_lane = Left`, `target_lane = Center`.
`SlipCurve` = valid identity curve; `bCurveFallbackActive = true`.

**When**: `F3RelativeOffset(0.5f)` called.

**Then**: despite `SlipCurve` being non-null, the flag routes to fallback:
`curve_t = FMath::Clamp(0.5, 0, 1) = 0.5` → result = -50.0 ± 0.001.

**Why separate from TC4**: TC4 exercises the `SlipCurve == nullptr` guard;
TC5 exercises the `!bCurveFallbackActive` guard — two distinct branch conditions
in `if (SlipCurve && !bCurveFallbackActive)`.

---

## TC6 — F-3 `rel_x` during SLIPPING at TP=0.5 (`f3_rel_x_slipping_tp05`)

**Given**: `current_lane = Left`, `target_lane = Center`. Identity `SlipCurve`;
`bCurveFallbackActive = false`. `tween_progress = 0.5f`.

**When**: `F3RelativeOffset(0.5f)` called (the pure-math half of the F-3 tick site).

**Then**: `rel_x == -50.0f ± 0.001`.

**Note**: the full tick-path composition — actor location contribution AND the
public `lateral_world_position` write — is covered end-to-end by **CC3
(`lateral_world_position_slipping_write`)** in
`PMLateralInterpolationCompositionTest.cpp`. TC6 covers the pure-math half only.

---

## CC1 — F-3 composition invariant (`f3_composition_invariant`)

**Guard**: `WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS`.

**Given**: spawn `ASlipstormPlayerPawn` in a test world; `BeginPlay` runs →
`CachedMeshComponent` resolved. RSM = RUNNING. Identity `SlipCurve`.
`current_lane = Left`, `target_lane = Center`, `movement_state = SLIPPING`,
`tween_progress = 0.5f`.

**When**:
1. Assert `F3RelativeOffset(0.5f) == -50.0f ± 0.001` (pure-math step).
2. Call `CachedMeshComponent->SetRelativeLocation(FVector(-50, 0, 0))` directly.
3. Read `MeshWorldX = CachedMeshComponent->GetComponentLocation().X` and
   `RootWorldX = Pawn->GetActorLocation().X`.

**Then**: `|MeshWorldX − (RootWorldX + (−50))| < 0.01`.

**ADR reference**: ADR-0009 Engine Compat Verification #4 — proves that
`UStaticMeshComponent::SetRelativeLocation` adds the offset to the actor root
in the UE 5.7 scene graph as expected.

---

## CC2 — `lateral_world_position` when SETTLED (`lateral_world_position_settled`)

**Guard**: `WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS`.

**Given**: spawn pawn; `BeginPlay` runs. RSM = RUNNING.
`movement_state = SETTLED`, `current_lane = Right`, `lateral_world_position = 0.0f` (sentinel).

**When**: `TickComponent(0.016f, ...)` called once (Rule 5 gate passes; SETTLED else-branch runs).

**Then**: `lateral_world_position == 100.0f` (= `LaneWorldX(Right)` = `(3−2)*100`).

**Why composition (not unit)**: the SETTLED else-branch lives inside the Rule 5 gate in
`TickComponent`, which requires a real BeginPlay-initialised PM with a valid `RSMSubsystem`
to exercise the full code path without stubbing the gate.

---

## CC3 — `lateral_world_position` end-to-end when SLIPPING (`lateral_world_position_slipping_write`)

**Guard**: `WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS`.

**Given**: spawn pawn; `BeginPlay` runs → `CachedMeshComponent` resolved (asserted).
RSM = RUNNING. Identity `SlipCurve`; `bCurveFallbackActive = false`.
`current_lane = Left`, `target_lane = Center`, `movement_state = SLIPPING`,
`tween_progress = 0.4f` (pre-advance), `lateral_world_position = 0.0f` (sentinel).

**When**: `FApp::SetDeltaTime(0.016)` then `TickComponent(0.016f, ...)` called once.
F-2 advances `tween_progress`; F-3 writes mesh relative offset + `lateral_world_position`.

**Then**:
- `PM->lateral_world_position ≈ RootWorldX + PM->F3RelativeOffset(PM->tween_progress)` within 0.01 cm
  (uses the impl's own `F3RelativeOffset` to avoid duplicating F-3 math; proves the composition).
- `PM->lateral_world_position` is NOT ≈ 0.0f (sentinel-overwrite guard — catches the case
  where `CachedMeshComponent` was null and the F-3 branch at cpp:214 was skipped silently).

**Purpose**: closes the AC6 SLIPPING end-to-end assertion gap flagged by qa-tester
Story 004 code review. TC6 covers the pure-math `rel_x`; CC3 covers the tick-path
write to the public `lateral_world_position` property.

---

## Design Invariant — TP=1.0 zero-relative (pinned)

`F3RelativeOffset(1.0f)` must return exactly `0.0f` for every lane transition.
This is the **mesh-snaps-to-root guarantee**: at the end of a tween, the mesh's
relative offset is zero, so the mesh world position exactly matches the pawn root
world position (which was snapped to `LaneWorldX(target_lane)` during the
`SETTLED→SLIPPING` collision commit in `HandleSlipTransition`).

**IEEE 754 proof**: `FMath::Lerp(delta, 0.0f, 1.0f)` expands to
`delta * (1.0f − 1.0f) + 0.0f * 1.0f = delta * 0.0f + 0.0f`.
By IEEE 754, `x * 0.0f = 0.0f` for all finite `x` (including subnormals).
Result: `0.0f + 0.0f = 0.0f`. Exact — no tolerance needed.

This invariant is why ADR-0009 IG-5 forbids clamping the absolute component
position: the relative-form naturally converges to zero without it.

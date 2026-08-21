# Story 004: F-3 lateral interpolation + mesh SetRelativeLocation + curve fallback

> **Epic**: Player Movement (Slip)
> **Status**: Complete
> **Layer**: Core
> **Type**: Logic
> **Estimate**: 3–4 hours
> **Manifest Version**: 2026-07-03 (docs/registry/architecture.yaml v8)
> **Last Updated**: 2026-07-12

## Context

**GDD**: `design/gdd/player-movement-mechanics.md` (§4 F-3 lateral_world_position, §8 AC-03; §Authored Asset Contracts for SlipCurve).
**Requirement**: `TR-PM-007`, `TR-PM-026 (partial — curve nulls fallback)`, `TR-PM-034 (partial — SlipCurve validation cross-reference)`.
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0009 (SD5 collision-decouple invariant + SD6 curve fallback path).
**ADR Decision Summary**: F-3 output is a RELATIVE offset applied via `MeshComponent->SetRelativeLocation`. Pawn root is committed to `LaneWorldX(target_lane)` (Story 003); mesh WORLD X = root world X + F-3 relative offset. At `TweenProgress = 1.0`, F-3 relative = 0.0 exactly, so mesh world = target lane exactly. Fallback (curve invalid): linear interpolation (no curve shaping).

**Engine**: Unreal Engine 5.7 | **Risk**: MEDIUM
**Engine Notes**: `USceneComponent::SetRelativeLocation` stable pre-cutoff. Composition invariant (mesh world = root world + relative) is UE 5.7 Engine Compatibility Verification #4 in ADR-0009 — must be confirmed on target hardware. `UCurveFloat::GetFloatValue()` stable pre-cutoff.

**Control Manifest Rules (this layer)** — from `docs/registry/architecture.yaml` v8:
- **Required**: F-3's clamp applies to the RELATIVE offset (not the absolute mesh world). At `TweenProgress = 1.0`, relative X == 0.0 exactly (ADR-0009 IG-5).
- **Required**: SlipCurve null → linear F-3 (no curve shaping) + `bCurveFallbackActive == true` — Shipping-safe path per ADR-0009 SD6 (already set by Story 001's BeginPlay if the curve was null; this story handles the tick-time fallback if a fault develops).
- **Forbidden**: Applying F-3 clamp to the absolute mesh world position — see ADR-0009 Risk 1 mitigation.

---

## Acceptance Criteria

*From `design/gdd/player-movement-mechanics.md` §4 F-3 + §8, scoped to this story:*

- [ ] `F3RelativeOffset(float TweenProgress) -> float` computes the relative-space delta for the mesh: given SlipCurve valid, `curve_t = FMath::Clamp(SlipCurve->GetFloatValue(TweenProgress), 0.0f, 1.0f)`; then relative offset = `FMath::Lerp(source_x - target_x, 0.0f, curve_t)` where `source_x = LaneWorldX(current_lane)`, `target_x = LaneWorldX(target_lane)`.
- [ ] Curve fallback (SlipCurve null OR `bCurveFallbackActive`): `curve_t = TweenProgress` (linear); no crash; behavior gracefully degrades.
- [ ] F-3 invocation site in TickComponent (added to Story 003's TickComponent skeleton): after F-2 advance, before F-5/F-6:
  ```cpp
  const float rel_x = F3RelativeOffset(tween_progress);
  MeshComponent->SetRelativeLocation(FVector(rel_x, 0.0f, 0.0f));
  ```
- [ ] **AC-03 (F-3 Lerp @ TP=0.5)**: given Left→Center tween (source_x = -100 cm, target_x = 0 cm), SlipCurve output at TP=0.5 = 0.5, relative offset = Lerp(-100 - 0, 0, 0.5) = -50; mesh world X = 0 + (-50) = -50 cm ± 1 cm.
- [ ] **Risk 1 mitigation invariant**: at `TweenProgress = 1.0`, F-3 relative offset = 0.0 exactly. Mesh world X == pawn root world X == LaneWorldX(target_lane). Verified across all 5×5 lane transition combinations.
- [ ] Public `lateral_world_position` property returns `pawn_root_world_x + rel_x` per tick during SLIPPING; returns `LaneWorldX(current_lane)` when SETTLED.

---

## Implementation Notes

*Derived from ADR-0009 SD5 + SD6 + Risk 1 mitigation + mechanics §4 F-3:*

**F-3 semantic** (SD5): the interpolation delta `(source_x - target_x)` is in TRACK-SPACE units (SD3 stationary-player convention). At TP=0, mesh relative X starts at `(source_x - target_x)` (an offset from the committed root); at TP=1, relative X collapses to 0. The formula uses `Lerp(from, to, curve_t)` where `from = source_x - target_x` and `to = 0.0f`. Do NOT clamp the absolute mesh world position — the relative form naturally lands at 0 without an extra clamp.

**Fallback path** (SD6): if `SlipCurve == nullptr` OR `bCurveFallbackActive` was set by BeginPlay (Story 001), the linear fallback applies: `curve_t = TweenProgress`. No panic — the mesh still tracks source→target linearly.

**Invocation site** (mechanics §4): after F-2 has advanced `tween_progress` on the current tick, F-3 writes the mesh relative location. F-5 (Story 006) writes rotation right after F-3.

**`lateral_world_position` public property** (mechanics §3 Public Interface): consumed by Camera + Pull-Wave targeting. Returns absolute world X of the mesh (root world X + relative), NOT the raw relative offset.

**Performance**: F-3 is O(1) per SLIPPING tick — one `UCurveFloat::GetFloatValue` lookup, one `FMath::Lerp`, one `SetRelativeLocation` write. No allocations. Well within the 16.6 ms mobile frame budget per `.claude/docs/technical-preferences.md`. No impact expected when SETTLED (F-3 not invoked — gated by Rule 5 in Story 003's TickComponent skeleton).

---

## Out of Scope

- Story 002: F-PROLOGUE + F-2 + SLIP_TWEEN clamp (called upstream in TickComponent).
- Story 003: TickComponent skeleton + Rule 5 gate + collision commit + CompleteTween (this story adds the F-3 write site into that skeleton).
- Story 006: F-5 lean → `SetRelativeRotation` writes.
- Story 007: F-6 tail phase.

---

## QA Test Cases

*Test file: `tests/unit/player-movement/pm_lateral_interpolation_test.cpp`. Automated pure-math + component-level tests.*

- **AC-03 (F-3 Lerp @ TP=0.5, curve = identity)**:
  - Given: Left→Center tween; `source_x = -100.0f`, `target_x = 0.0f`; SlipCurve returns identity (curve_t = TweenProgress).
  - When: `F3RelativeOffset(0.5f)` invoked.
  - Then: returns `-50.0f ± 0.01f`.
  - Edge cases: (a) TP = 0.0 → -100.0f; (b) TP = 1.0 → 0.0f exactly; (c) TP = 0.25 → -75.0f.

- **AC-03 (F-3 Lerp with authored curve)**:
  - Given: SlipCurve with ease-in-out shape returning 0.35 at TP=0.5 (representative Phase 1 shelf); Left→Center same as above.
  - When: `F3RelativeOffset(0.5f)`.
  - Then: relative offset = Lerp(-100, 0, 0.35) = -65 ± 1cm.

- **Risk 1 mitigation (TP=1.0 zero-relative invariant)**:
  - Given: any of the 20 non-self lane transitions (5-lane × 4 non-self target).
  - When: `F3RelativeOffset(1.0f)` invoked.
  - Then: relative offset = 0.0f exactly (no epsilon needed — Lerp(x, 0, 1) == 0 in IEEE 754).
  - Grep gate: search `F3RelativeOffset` for any absolute-position clamp — must not exist.

- **Mesh composition invariant** (Engine Compatibility Verification #4):
  - Given: PM SLIPPING mid-tween with mesh relative X set to -50; pawn root at LaneWorldX(Center) = 0.
  - When: read `MeshComponent->GetComponentLocation().X` (world) and `GetRelativeLocation().X`.
  - Then: `world.X == root_world.X + relative.X` within 0.01 cm.
  - Edge cases: transitions where target lane != Center, verifying root world + relative == source lane exactly at TP=0.0.

- **Curve fallback (SlipCurve null at tick)**:
  - Given: `SlipCurve = nullptr` (or `bCurveFallbackActive == true`); Left→Center tween at TP=0.5.
  - When: `F3RelativeOffset(0.5f)`.
  - Then: relative offset = Lerp(-100, 0, 0.5) = -50 (linear fallback engaged); no crash; `bCurveFallbackActive` remains true.

- **`lateral_world_position` public property**:
  - Given: PM SLIPPING at TP=0.5 with rel_x=-50 (Left→Center), root at 0.
  - When: read `lateral_world_position`.
  - Then: returns -50.0f (root + relative).
  - Edge cases: SETTLED at Right → returns LaneWorldX(Right) = 100.0f.

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Automated unit test at `tests/unit/player-movement/pm_lateral_interpolation_test.cpp` — must exist and pass.

**Status**: [ ] Not yet created

---

## Dependencies

- **Depends on**: Story 001 (SlipCurve UPROPERTY, bCurveFallbackActive flag, MeshComponent reference on pawn); Story 002 (LANE_WIDTH_CM constant + LaneWorldX); Story 003 (TickComponent skeleton + CompleteTween + current_lane/target_lane state).
- **Unlocks**: Camera GDD (unauthored — inherits `lateral_world_position` public read surface); Pull-Wave targeting (out-of-epic).

---

## Completion Notes

**Completed**: 2026-07-12
**Criteria**: 6/6 passing — all COVERED by automated tests (see traceability in `/story-done` session log)
**Deviations**:
- **ADVISORY**: Fallback path adds `FMath::Clamp(TweenProgress, 0.0f, 1.0f)` symmetrically with the SlipCurve path (not in original story text). Added per Q3 decision during `/dev-story` to preserve IG-5 TP≥1.0 zero-relative invariant on hitch (F-2 accumulator can reach 1.667 per Story 002 TC3). Correctness improvement over story text. See `PlayerLaneMovementComponent.cpp:607-614`.
- **ADVISORY**: The `## Test Evidence` section above names `tests/unit/player-movement/pm_lateral_interpolation_test.cpp` (aspirational). Actual test files follow UE convention:
  - `Source/SLIPSTORM/Tests/Unit/PlayerMovement/PMLateralInterpolationTest.cpp` (6 test commands)
  - `Source/SLIPSTORM/Tests/Integration/PlayerMovement/PMLateralInterpolationCompositionTest.cpp` (3 test commands — CC1 composition invariant, CC2 SETTLED, CC3 SLIPPING end-to-end)
  - Spec doc: `tests/unit/player-movement/pm-lateral-interpolation-spec.md`
**Test Evidence**: Logic — automated unit tests at project convention path (see above). Evidence gate: **PASS**.
**Code Review**: Complete — `/code-review` in same session (unreal-specialist + qa-tester in parallel) → APPROVED WITH SUGGESTIONS. 3 minor suggestions applied inline (TC3 strict `!=` for TP=1.0 invariant; TC6 renamed to `f3_rel_x_slipping_tp05`; CC3 added to close AC6 SLIPPING end-to-end gap; `OnSlipMidpoint.Broadcast` ordering comment at `cpp:208`). 3 nits deferred (non-blocking):
- Use `IsValid(CachedMeshComponent)` vs raw `if (CachedMeshComponent)` for consistency (cosmetic)
- Test-command naming style (`f3_lerp_identity_curve` omits `_[expected]` suffix — matches PMLaneAndTweenTest precedent; escalate to qa-lead for canonical Story 005+ ruling)
- Grep-gate ("no absolute-position clamp in `F3RelativeOffset`") should be added as a checklist item to future Done criteria — enforced by code review at cpp:587-589 for this story
**Files touched**:
- `Source/SLIPSTORM/Player/PlayerLaneMovementComponent.h` (426 → 461 lines)
- `Source/SLIPSTORM/Player/PlayerLaneMovementComponent.cpp` (530 → 617+ lines, includes ordering comment addition)
- `Source/SLIPSTORM/Tests/Unit/PlayerMovement/PMLateralInterpolationTest.cpp` (new — 6 commands)
- `Source/SLIPSTORM/Tests/Integration/PlayerMovement/PMLateralInterpolationCompositionTest.cpp` (new — 3 commands)
- `tests/unit/player-movement/pm-lateral-interpolation-spec.md` (new spec doc)


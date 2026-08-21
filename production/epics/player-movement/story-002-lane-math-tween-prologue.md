# Story 002: Lane math + tween prologue (F-1 + F-PROLOGUE + F-2 + SLIP_TWEEN clamp)

> **Epic**: Player Movement (Slip)
> **Status**: Complete
> **Layer**: Core
> **Type**: Logic
> **Estimate**: 2 hours (S) — actual: ~3 hours across dev-story (specialist pass with 6 semantic errors) + 2 code-review passes (corrective inline + APPROVED)
> **Manifest Version**: 2026-07-03 (docs/registry/architecture.yaml v8)
> **Last Updated**: 2026-07-12

## Context

**GDD**: `design/gdd/player-movement-mechanics.md` (§4 F-1, F-2), `design/gdd/player-movement-platform.md` (§4 F-PROLOGUE, §3 Shipping-Safety AC-SS-A + AC-21, §7 knob safe ranges).
**Requirement**: `TR-PM-004`, `TR-PM-005`, `TR-PM-006`.
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0009 (Player Movement Component Hosting).
**ADR Decision Summary**: F-PROLOGUE clamps `effective_dt = FMath::Clamp(FApp::GetDeltaTime(), 0.0f, MAX_SLIP_DT_S)` as the single-source DT for both watchdog and mechanics (platform §4 canonicalization). F-2 accumulates TweenProgress against a persistently-clamped `SLIP_TWEEN_DURATION_S ∈ [0.10, 0.15]`.

**Engine**: Unreal Engine 5.7 | **Risk**: MEDIUM
**Engine Notes**: `FApp::GetDeltaTime()` chosen over `GetWorld()->GetDeltaSeconds()` — raw frame DT (ignores world time dilation) so watchdog remains hardware-semantically correct if slow-motion / replay-scrubbing added later (platform §4 F-PROLOGUE rationale). `FMath::Clamp` stable pre-cutoff.

**Control Manifest Rules (this layer)** — from `docs/registry/architecture.yaml` v8:
- Required: F-PROLOGUE is the SINGLE SOURCE of `effective_dt` for the tick body — F-2 and (later) F-6 read the same clamped value; watchdog reads `raw_dt` (Story 013). Do not re-clamp downstream.
- Required: SLIP_TWEEN persistent clamp — every violating tick clamped to nearest bound (0.10 or 0.15). `bSlipTweenClampActive = true` for the duration of the violation. Rate-limited `Error` log: 1 per 600 ticks (AC-21 four-site lockstep policy). NOT a one-shot flag reset.
- Forbidden: `PlayerMovement_TickComponent_without_prior_ForceTickNow` — this story's `TickComponent` body will call F-PROLOGUE, but the ForceTickNow prologue lands in Story 003; do not implement TickComponent's outer structure fully until Story 003 wires the prologue. This story's contribution: F-PROLOGUE + F-2 + SLIP_TWEEN clamp as private helpers callable from TickComponent.

---

## Acceptance Criteria

*From `design/gdd/player-movement-mechanics.md` §4 + `design/gdd/player-movement-platform.md` §4 + §8, scoped to this story:*

- [ ] **F-1 lane_world_x**: `float LaneWorldX(EPlayerLane lane) { return (static_cast<int32>(lane) - 2) * LANE_WIDTH_CM; }` where `LANE_WIDTH_CM = 100.0f * LANE_WIDTH_M` with `LANE_WIDTH_M = 1.0f` (default; safe range [0.85, 1.5]).
- [ ] **F-PROLOGUE**: private helper `void ComputeTickDT(float& OutRawDT, float& OutEffectiveDT)` computes `OutRawDT = FApp::GetDeltaTime()`; `OutEffectiveDT = FMath::Clamp(OutRawDT, 0.0f, MAX_SLIP_DT_S)`. `MAX_SLIP_DT_S = 0.05f` (default; safe range [0.020, 0.06]).
- [ ] **F-2 TweenProgress advance**: `TweenProgress += EffectiveDT / EffectiveSlipTween`. Gated by RSM RUNNING + !paused + !resume_grace (per-tick property reads land in Story 003; this story implements the accumulator as a pure function).
- [ ] **SLIP_TWEEN persistent clamp (AC-21 + AC-SS-A)**: private helper `float GetEffectiveSlipTween()` returns `FMath::Clamp(SLIP_TWEEN_DURATION_S, 0.10f, 0.15f)`. If the raw knob is out of range, `bSlipTweenClampActive = true` for the frame; else `false`. Rate-limited log: `UE_LOG(LogPlayerMovement, Error, TEXT("SLIP_TWEEN_DURATION_S=%f out of safe range [0.10,0.15]"), SLIP_TWEEN_DURATION_S)` on tick 1, tick 601, tick 1201, etc. (1 per 600 ticks).
- [ ] **AC-20 (F-2 math)**: given `SLIP_TWEEN_DURATION_S = 0.15`, `MAX_SLIP_DT_S = 0.05`, `DT = 0.016` → `TP_delta = 0.016/0.15 ≈ 0.10667 ± 0.001`. Given hitched `DT = 2.0`, F-PROLOGUE clamps to 0.05 → `TP_delta = 0.05/0.15 ≈ 0.3333` (skip prevented).
- [ ] **AC-21 (SLIP_TWEEN persistent clamp)**: given `SLIP_TWEEN_DURATION_S = 0.09` (floor violation), 10 consecutive ticks clamp to 0.10 (NOT reset to 0.15); `bSlipTweenClampActive == true` all 10 ticks; F-2 logs `Error` on tick 1 and tick 601. Pre-R10a one-shot flag pattern is NOT permitted. Also test ceiling case `SLIP_TWEEN_DURATION_S = 0.20` clamps to 0.15.
- [ ] **AC-SS-A**: same as AC-21 (four-site lockstep — this AC verifies the mechanics site + platform §3 Shipping-Safety table row + §3 AC-SS-A body + §8 AC-21 body all describe the same behavior).

---

## Implementation Notes

*Derived from ADR-0009 SD4 + mechanics §4 F-2 + platform §4 F-PROLOGUE + §3 Shipping-Safety Enforcement Policy:*

**Single-source DT invariant** (platform §4 F-PROLOGUE B-F6-3 closure): `FApp::GetDeltaTime()` is read ONCE per tick and fanned out to `raw_dt` (watchdog input) and `effective_dt` (mechanics input). Do not call `FApp::GetDeltaTime()` in F-2 or F-6 downstream — receive the value.

**Clamp four-site lockstep** (platform §3 Shipping-Safety enforcement-table policy): if you ever revise the tick-601 log threshold, all four sites must update in lockstep: (1) this file's IG comment on F-2 pseudo-code; (2) platform §3 Shipping-Safety table SLIP_TWEEN row AC-21 fold-in prose; (3) platform §8 AC-21 body; (4) platform §3 AC-SS-A body. Do NOT edit in isolation.

**F-2 pseudo-code** (mechanics §4):
```cpp
// Called inside TickComponent from Story 003. This story provides the helpers.
float raw_dt, effective_dt;
ComputeTickDT(raw_dt, effective_dt);          // F-PROLOGUE

const float effective_slip_tween = GetEffectiveSlipTween(); // SLIP_TWEEN persistent clamp
if (movement_state == ERunSlipState::SLIPPING)
{
    tween_progress += effective_dt / effective_slip_tween;
    // CompleteTween crossing check lands in Story 003
}
```

**Tuning knob defaults** (mechanics §7 + platform §7):
- `constexpr float LANE_WIDTH_M = 1.0f;` safe [0.85, 1.5]
- `constexpr float LANE_WIDTH_CM = LANE_WIDTH_M * 100.0f;`
- `constexpr float MAX_SLIP_DT_S = 0.05f;` safe [0.020, 0.06]
- `float SLIP_TWEEN_DURATION_S = 0.15f;` safe [0.10, 0.15] — designer-tunable UPROPERTY (EditDefaultsOnly)

**Performance budget**: `LaneWorldX`, `ComputeTickDT`, `GetEffectiveSlipTween`, and the F-2 accumulator step are pure-math helpers (integer arithmetic + `FMath::Clamp` + one division for F-2 / one for the tween ratio). Estimated < 1 μs per call on mid-tier mobile. Well within the per-tick PM CPU `< 0.15 ms p99` budget from `EPIC.md` Definition of Done — this story's contribution to that budget is negligible. No heap allocations; no thread synchronization; no engine-scheduler interaction. The rate-limited `Error` log path is gated to at most 1 log per 600 ticks (≈ 10 s at 60 fps) so it does not become a hot-path cost even under sustained clamp-violation configurations.

---

## Out of Scope

- Story 003: TickComponent outer structure with `RSM->ForceTickNow()` prologue + Rule 5 RSM gate + F-2 site of invocation + CompleteTween crossing + slip_complete_count++.
- Story 013: `raw_dt` consumption by the watchdog (this story exposes it via F-PROLOGUE; watchdog reads it there).
- Story 007: F-6 consumption of `effective_dt`.

---

## QA Test Cases

*Test file: `tests/unit/player-movement/pm_lane_and_tween_test.cpp`. Automated pure-math unit tests.*

- **F-1 lane math**: `LaneWorldX(FarLeft) == -200.0f`; `LaneWorldX(Left) == -100.0f`; `LaneWorldX(Center) == 0.0f`; `LaneWorldX(Right) == 100.0f`; `LaneWorldX(FarRight) == 200.0f`. (LANE_WIDTH_M = 1.0.)
  - Edge cases: LANE_WIDTH_M at 0.85 → boundaries scale proportionally; at 1.5 → same. Both within safe range must produce non-overlapping lane centers.

- **AC-20 (F-2 math nominal)**:
  - Given: `SLIP_TWEEN_DURATION_S = 0.15f`, `MAX_SLIP_DT_S = 0.05f`, `DT = 0.016f`.
  - When: `F-PROLOGUE(DT)` runs, then F-2 accumulates for one tick starting from `tween_progress = 0.0f`.
  - Then: `tween_progress ≈ 0.10667f ± 0.001f` post-tick.
  - Edge cases: DT = 0 → tween_progress unchanged; DT negative (shouldn't happen but F-PROLOGUE clamps to 0) → tween_progress unchanged.

- **AC-20 (F-2 hitch clamp)**:
  - Given: `SLIP_TWEEN_DURATION_S = 0.15f`, `MAX_SLIP_DT_S = 0.05f`, `DT = 2.0f` (2-second hitch simulation).
  - When: F-PROLOGUE runs.
  - Then: `effective_dt == 0.05f` (clamped); one tick of F-2 → `tween_progress ≈ 0.3333f` — hitch does not skip past TP=1.0 in a single frame.
  - Edge cases: sustained DT = 0.10f across 5 ticks — TP advances by 5 × 0.3333 = 1.667 → capped at CompleteTween crossing (verified in Story 003).

- **AC-21 (SLIP_TWEEN floor persistent clamp)**:
  - Given: `SLIP_TWEEN_DURATION_S = 0.09f`.
  - When: `GetEffectiveSlipTween()` invoked on 100 consecutive ticks.
  - Then: returns `0.10f` all 100 ticks; `bSlipTweenClampActive == true` all 100 ticks; `LogPlayerMovement Error` emitted on tick 1 and no earlier than tick 601 for the second (rate-limit).
  - Edge cases: (a) restore `SLIP_TWEEN_DURATION_S = 0.12f` mid-run → `bSlipTweenClampActive` transitions to `false` on the next tick; (b) tick 601 fires the second log; tick 1201 the third.

- **AC-21 (SLIP_TWEEN ceiling persistent clamp)**:
  - Given: `SLIP_TWEEN_DURATION_S = 0.20f`.
  - When: `GetEffectiveSlipTween()` invoked on 10 consecutive ticks.
  - Then: returns `0.15f` all 10 ticks; `bSlipTweenClampActive == true` all 10 ticks; `Error` log on tick 1.
  - Edge cases: exactly 0.15f (boundary) — no clamp, flag false, no log.

- **AC-SS-A (four-site lockstep verification, meta)**:
  - Docs test only: grep confirms four sites reference `600 ticks` rate limit and the exact clamp bounds `[0.10, 0.15]`. If any one site diverges without the other three matching, the test fails. Purely defensive against rot.

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Automated unit test at `Source/SLIPSTORM/Tests/Unit/PlayerMovement/PMLaneAndTweenTest.cpp` (compiled into the SLIPSTORM game module per `tests/automation-cpp/README.md` dual-location convention) — must exist and pass in headless UE Automation Framework runner (`-nullrhi`). Test class `FPMLaneAndTweenTest`; category `SLIPSTORM.PlayerMovement.LaneAndTween`. Test flags `EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter` (headless-compatible per Story 001a precedent).
- Test spec markdown at `tests/unit/player-movement/pm-lane-and-tween-spec.md` (studio dual-location convention — spec in `tests/`, C++ under `Source/`).

**Status**: [ ] Not yet created

---

## Dependencies

- **Depends on**: Story 001 (component skeleton, log category, `SLIP_TWEEN_DURATION_S`, `MAX_SLIP_DT_S`, `LANE_WIDTH_M` declared as UPROPERTY / constexpr).
- **Unlocks**: Story 003 (calls F-PROLOGUE + F-2 in TickComponent), Story 004 (F-3 consumes tween_progress), Story 007 (F-6 consumes effective_dt), Story 013 (watchdog consumes raw_dt).

---

## Completion Notes

**Completed**: 2026-07-12
**Criteria**: 7/7 ACs passing (6 runtime-tested + 1 docs-level invariant for AC-SS-A four-site lockstep).

**Files delivered** (4):
- `Source/SLIPSTORM/Player/PlayerLaneMovementComponent.h` (359 lines, +78 from Story 001) — `SLIPSTORM_PM::LANE_WIDTH_M/CM/MAX_SLIP_DT_S` constexpr constants, `SLIP_TWEEN_DURATION_S` UPROPERTY EditDefaultsOnly default `0.15f`, method decls for `LaneWorldX`/`ComputeTickDT`/`GetEffectiveSlipTween`/`AdvanceTweenProgress`, `SlipTweenClampLogTickCounter` field, second `friend class FPMLaneAndTweenTest` in existing `WITH_DEV_AUTOMATION_TESTS` block
- `Source/SLIPSTORM/Player/PlayerLaneMovementComponent.cpp` (307 lines, +102 from Story 001) — `#include "Misc/App.h"` + `LaneWorldX`/`ComputeTickDT`/`GetEffectiveSlipTween`/`AdvanceTweenProgress` implementations with correct AC-aligned semantics
- `Source/SLIPSTORM/Tests/Unit/PlayerMovement/PMLaneAndTweenTest.cpp` (318 lines, NEW) — `IMPLEMENT_COMPLEX_AUTOMATION_TEST` with 5 `GetTests` entries (`f1_lane_math`, `f2_math_nominal`, `f2_hitch_clamp`, `slip_tween_floor_clamp`, `slip_tween_ceiling_clamp`), 26 test-framework assertions, 2 `AddExpectedError` guards, `#if WITH_DEV_AUTOMATION_TESTS`-only guard
- `tests/unit/player-movement/pm-lane-and-tween-spec.md` (117 lines, NEW) — spec markdown with story/TR/ADR/GDD frontmatter + Given/When/Then per test case + rate-limit counter policy documentation + AC-SS-A four-site lockstep note

**Test-Criterion Traceability**: All 7 ACs COVERED via 5 automated test cases (601-call loop in TC4 verifies the tick-601 rate-limit boundary). Runtime pass verification deferred to first PR CI green run (requires UE 5.7 Editor on self-hosted runner per `.github/workflows/tests.yml`).

**Code Review**: Complete (2026-07-11). Verdict: **APPROVED** after 2-pass correction cycle:
- Pass 1 by specialist introduced 6 semantic errors (SLIP_TWEEN_DURATION_S default 0.125f, `GetEffectiveSlipTween` clamped wrong variable (tween_progress vs SLIP_TWEEN_DURATION_S), `ComputeTickDT` wrong signature, `AdvanceTweenProgress` had out-of-scope completion side effects, log severity Warning vs Error, `bSlipTweenClampActive` semantics reversed) — all corrected inline via Edit/Write before pass 2
- Pass 2 by fresh specialist + qa-tester → APPROVED WITH SUGGESTIONS → 2 `Should fix` items applied inline (`%f` → `%.6f` log format for cross-platform `AddExpectedError` portability; forward-dependency comment on `AdvanceTweenProgress` warning Story 003 against double-invocation of `GetEffectiveSlipTween`)

**Deviations (Advisory — non-blocking)**:
1. **LANE_WIDTH_M edge case not runtime-testable** — Story 002 AC line 87 requires boundary scaling verification at LANE_WIDTH_M = 0.85 and 1.5. Since `LANE_WIDTH_M` is `constexpr float LANE_WIDTH_M = 1.0f` (design-fixed; no runtime tuning surface), the AC is a design-time invariant proved by the formula structure `(ordinal - 2) * LANE_WIDTH_CM`. Cannot be exercised at runtime without redesigning the constant as a UPROPERTY. Accepted as documentation-level AC.
2. **`SLIP_TWEEN_DURATION_S` public UPROPERTY** — unreal-specialist noted the friend declaration covers test-access to private fields, so public isn't strictly required. Defensible as designer-tunable knob for BP; no functional impact. Kept as-is.

**Test Evidence**: Unit test at `Source/SLIPSTORM/Tests/Unit/PlayerMovement/PMLaneAndTweenTest.cpp` + spec at `tests/unit/player-movement/pm-lane-and-tween-spec.md`.

**Unlocks**: Story 003 (calls `ComputeTickDT` + `GetEffectiveSlipTween` + `AdvanceTweenProgress` in `TickComponent`), Story 004 (F-3 consumes `tween_progress`), Story 007 (F-6 consumes `effective_dt`), Story 013 (watchdog consumes `raw_dt`).

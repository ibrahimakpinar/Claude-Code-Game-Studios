---
story: production/epics/player-movement/story-002-lane-math-tween-prologue.md
tr_ids: [TR-PM-004, TR-PM-005, TR-PM-006]
adrs:
  - docs/architecture/adr-0009-player-movement-hosting.md
cpp_test: Source/SLIPSTORM/Tests/Unit/PlayerMovement/PMLaneAndTweenTest.cpp
gdd_refs:
  - design/gdd/player-movement-mechanics.md §4 F-1, F-2 + §7 Tuning Knobs
  - design/gdd/player-movement-platform.md §4 F-PROLOGUE + §3 Shipping-Safety AC-SS-A + §8 AC-20 + AC-21
test_category: SLIPSTORM.PlayerMovement.LaneAndTween
guard: "#if WITH_DEV_AUTOMATION_TESTS (no WITH_EDITOR — no world spawn)"
---

# PM Lane and Tween — Unit Test Spec (Story 002)

## Overview

Pure-math unit tests covering Story 002's four helpers on `UPlayerLaneMovementComponent`:

- **F-1 `LaneWorldX(EPlayerLane)`** — static lane-ordinal-to-world-X mapping
- **F-PROLOGUE `ComputeTickDT(float& OutRawDT, float& OutEffectiveDT)`** — reads `FApp::GetDeltaTime()` once and computes the effective DT (clamped to `MAX_SLIP_DT_S`)
- **F-2 `AdvanceTweenProgress(float EffectiveDT)`** — pure accumulator: `tween_progress += EffectiveDT / GetEffectiveSlipTween()`
- **AC-21 / AC-SS-A `GetEffectiveSlipTween()`** — persistent SLIP_TWEEN_DURATION_S clamp to `[0.10, 0.15]` with per-call `bSlipTweenClampActive` flag + rate-limited Error log (1 per 600 consecutive violating calls)

No `UWorld` spawn, no `BeginPlay` invocation. All private members accessed via the `friend class FPMLaneAndTweenTest` declaration in `PlayerLaneMovementComponent.h` (guarded by `#if WITH_DEV_AUTOMATION_TESTS`).

---

## TC1 — F-1 lane math (`f1_lane_math`)

**Given**: LANE_WIDTH_M = 1.0f, LANE_WIDTH_CM = 100.0f (design defaults).
**When**: `LaneWorldX(Lane)` called for each of the 5 `EPlayerLane` ordinals.
**Then**:

| Lane      | Ordinal | Expected X (cm) |
|-----------|---------|-----------------|
| FarLeft   | 0       | -200.0f         |
| Left      | 1       | -100.0f         |
| Center    | 2       |    0.0f         |
| Right     | 3       | +100.0f         |
| FarRight  | 4       | +200.0f         |

Tolerance: exact equality (integer multiply — no FP error).

---

## TC2 — AC-20 F-2 math nominal (`f2_math_nominal`)

**Given**: `SLIP_TWEEN_DURATION_S = 0.15f`, `MAX_SLIP_DT_S = 0.05f`, `tween_progress = 0.0f`.
**When**: `AdvanceTweenProgress(0.016f)` invoked once.
**Then**: `tween_progress ≈ 0.10667f ± 0.001f` (equals `0.016 / 0.15`).

**Edge case**: `AdvanceTweenProgress(0.0f)` leaves `tween_progress` unchanged.

---

## TC3 — AC-20 F-2 hitch clamp (`f2_hitch_clamp`)

**Given**: `SLIP_TWEEN_DURATION_S = 0.15f`, `MAX_SLIP_DT_S = 0.05f`, `tween_progress = 0.0f`.
**When**: `ComputeTickDT(OutRawDT, OutEffectiveDT)` invoked.
**Then**: `OutEffectiveDT ≤ MAX_SLIP_DT_S + KINDA_SMALL_NUMBER`; `OutEffectiveDT >= 0.0f` (invariants — the raw side depends on the global `FApp` clock and is not directly assertable in unit scope).

**Simulated hitch application**: caller (Story 003 TickComponent glue) passes the clamped-DT ceiling value `0.05f` into `AdvanceTweenProgress`:
- Single tick: `tween_progress ≈ 0.3333f ± 0.001f` (equals `0.05 / 0.15`) — proves a single hitch does not skip past `TP = 1.0f`.
- 5 consecutive ticks: `tween_progress ≈ 1.667f ± 0.01f` — proves the accumulator does NOT clip at 1.0. Completion side effects (snap to target + `movement_state = SETTLED`) are Story 003's `CompleteTween()` scope.

---

## TC4 — AC-21 SLIP_TWEEN floor persistent clamp (`slip_tween_floor_clamp`)

**Given**: `SLIP_TWEEN_DURATION_S = 0.09f` (floor violation, strictly `< 0.10f`).
**When**: `GetEffectiveSlipTween()` invoked 601 times consecutively.
**Then**:
- All 601 return values equal `0.10f` (nearest bound; persistent — not one-shot).
- `bSlipTweenClampActive == true` throughout.
- Exactly 2 `LogPlayerMovement Error` entries fire — one at counter=0 (first call), one at counter=600 (601st call). Message contains `"SLIP_TWEEN_DURATION_S=0.090000 out of safe range"`. Guarded via `AddExpectedError(..., Contains, 2)`.

**Edge case (restore)**: after restoring `SLIP_TWEEN_DURATION_S = 0.12f`:
- Next call returns `0.12f` (in-range — no clamp).
- `bSlipTweenClampActive == false`.
- `SlipTweenClampLogTickCounter == 0` (reset on non-violating call → resumed violation logs immediately).

---

## TC5 — AC-21 SLIP_TWEEN ceiling persistent clamp (`slip_tween_ceiling_clamp`)

**Given**: `SLIP_TWEEN_DURATION_S = 0.20f` (ceiling violation, strictly `> 0.15f`).
**When**: `GetEffectiveSlipTween()` invoked 10 times consecutively.
**Then**:
- All 10 return values equal `0.15f` (nearest bound).
- `bSlipTweenClampActive == true` throughout.
- Exactly 1 `LogPlayerMovement Error` entry fires (tick 1 only; tick 601 not reached). Guarded via `AddExpectedError(..., Contains, 1)`.

**Boundary edge cases** (proves strict `<` / `>` — exact bounds are NOT violations):
- `SLIP_TWEEN_DURATION_S = 0.15f` → returns `0.15f`, `bSlipTweenClampActive == false`.
- `SLIP_TWEEN_DURATION_S = 0.10f` → returns `0.10f`, `bSlipTweenClampActive == false`.

---

## AC-SS-A four-site lockstep (meta-note, not a runtime test)

The AC-21 four-site lockstep policy (platform §3 Shipping-Safety Enforcement Policy) requires that any revision of the tick-601 log threshold update ALL four sites in lockstep:

1. `GetEffectiveSlipTween` method body comment in `PlayerLaneMovementComponent.cpp`
2. `design/gdd/player-movement-platform.md` §3 Shipping-Safety table SLIP_TWEEN row AC-21 fold-in prose
3. `design/gdd/player-movement-platform.md` §8 AC-21 body
4. `design/gdd/player-movement-platform.md` §3 AC-SS-A body

This is a documentation-hygiene invariant, not a runtime assertion. If the tick-601 log threshold changes without lockstep updates elsewhere, the four sites will diverge — code review is the enforcement mechanism.

---

## Rate-limit counter policy (pinned)

`SlipTweenClampLogTickCounter` counts consecutive violating calls to `GetEffectiveSlipTween`:

- On a violating call: log iff `SlipTweenClampLogTickCounter % 600 == 0`, then increment.
- On a non-violating call: reset counter to `0`.

Semantics:
- Tick 1 of a sustained violation: counter is `0`, `0 % 600 == 0` → **log fires**, counter becomes `1`.
- Ticks 2–600: counter is `1`–`599` → **no log**.
- Tick 601: counter is `600`, `600 % 600 == 0` → **log fires**, counter becomes `601`.
- A valid call between violations resets counter → next violation logs immediately (does not silently wait for the next 600-tick boundary).

Boundary values `0.10f` and `0.15f` exactly are NOT violations (strict `<` / `>` comparison).

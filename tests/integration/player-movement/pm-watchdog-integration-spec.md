# PM Watchdog Integration — Sequential Setup B → Setup D Release Latency

**Story**: `production/sprints/sprint-1.md` S1-09
**Test file**: `Source/SLIPSTORM/Tests/Integration/PlayerMovement/PMWatchdogIntegrationTest.cpp`
**Category**: `SLIPSTORM.PlayerMovement.WatchdogIntegration`
**Type**: Integration

## Scope

Validates the sequential Setup B → Setup D DT-watchdog release-latency chain
that `PMWatchdogTest.cpp` (unit) explicitly defers to integration testing per
its comment at line 335-338 ("The sequential path is intrinsic to the design
and would be exercised in an integration test with real gameplay; this
Logic-type story validates the release condition in isolation.").

Unit test coverage: Setup B (entry) and Setup D (release) validated in
**isolation** via `SEED_*` macros that decouple release testing from entry
sequence. This integration test validates the **sequential** path — Setup D
immediately following Setup B on the same live PM component — where the
buffer must flush breach samples before the hysteresis accumulator can start.

## Test Cases

### TC1 — `sequential_b_to_d_release_latency`

**Given**: fresh `UPlayerLaneMovementComponent` on a spawned
`ASlipstormPlayerPawn` in a real `EWorldType::Game` world (via
`FTestWorldWrapper`) with sentinel-filled DT rolling buffer.

**When**:
1. 30 × `WatchdogTick(0.020f)` drives Setup B (sustained sub-55 breach entry).
2. `WatchdogTick(0.01667f)` called repeatedly until release fires (capped at 300).

**Then** (design invariants, not implementation counts):

- Breach broadcast fires exactly once during entry phase.
- Release broadcast fires exactly once during release phase.
- Subscriber sees exactly `[true, false]` in order; no duplicates.
- Total release-phase tick count is bounded in `[180, 260]`.
- `is_hw_performance_degraded` transitions `true → false` correctly.

## Expected Release-Phase Latency

Code-derived expected: 60 buffer-flush ticks (to displace 30 breach samples
past all 30 sentinel slots and back to breach positions) + 180 hysteresis-
accumulate ticks (`3.0 s / 0.01667 s`) = **240 ticks**.

The `[180, 260]` accepted range accommodates buffer-semantics variance
without ossifying an exact count into the test. Prior comment at
`PMWatchdogTest.cpp:335-338` estimated `~30 flush + 180 accumulate = ~210`;
the actual flush cost per `PlayerLaneMovementComponent.cpp:961-994` release-
condition logic (`count_gt_1667 == 0` required before accumulator advances)
appears closer to 60. The range absorbs both estimates.

## References

- `design/gdd/player-movement-platform.md` §3 Hardware Contract + §8 AC-HW-A Setup B/D
- `docs/architecture/adr-0009-player-movement-hosting.md`
- `Source/SLIPSTORM/Tests/Unit/PlayerMovement/PMWatchdogTest.cpp` (companion unit test — TC2 Setup B entry, TC5 Setup D release-in-isolation)
- `docs/tests-headless.md` §2 (flag-registration rule) + §3 (FTestWorldWrapper pattern)

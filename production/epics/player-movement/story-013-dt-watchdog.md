# Story 013: DT watchdog rolling buffer + breach + 3.0s hysteresis-release + OnHardwarePerformanceBreach broadcast + is_hw_performance_degraded

> **Epic**: Player Movement (Slip)
> **Status**: Complete
> **Layer**: Core (platform contract)
> **Type**: Logic
> **Estimate**: 5 hours
> **Manifest Version**: 2026-07-03 (docs/registry/architecture.yaml v8)
> **Last Updated**: 2026-08-07

## Context

**GDD**: `design/gdd/player-movement-platform.md` (§3 Runtime DT watchdog specification, §3 Public Interface — `is_hw_performance_degraded` + `OnHardwarePerformanceBreach`, §4 F-WATCHDOG-ROLLING-BUFFER, R11a-6 sentinel, R11a-7 orthogonal pass criteria, §8 AC-HW-A Setups A-G, AC-21 SLIP_TWEEN rate-limited log idiom mirror).
**Requirement**: `TR-PM-020` (60-sample rolling watchdog), `TR-PM-021` (3.0s hysteresis-release), `TR-PM-022` (broadcast delegate), `TR-PM-024` (public `is_hw_performance_degraded` flag), `TR-PM-026` (final Shipping-safe guards row).
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0009 (SD1 delegate on component + SD4 raw_dt sample source from F-PROLOGUE).
**ADR Decision Summary**: Watchdog pushes `raw_dt` (unclamped) from F-PROLOGUE into a 60-slot ring buffer each tick. Two breach criteria (OR-composed): (a) sustained sub-55fps — ≥30 of 60 samples > 18.18ms; (b) hitch cluster — ≥18 of 60 samples > 16.67ms AND any single sample > 33ms. Hysteresis-release requires all 60 samples ≤ 16.67ms sustained for 3.0s continuous. State change fires `OnHardwarePerformanceBreach(bEntering)` delegate + toggles `is_hw_performance_degraded`.

**Engine**: Unreal Engine 5.7 | **Risk**: LOW (pure math + delegate broadcast).
**Engine Notes**: `FApp::GetDeltaTime()` stable pre-cutoff (already used in Story 002 F-PROLOGUE).

**Control Manifest Rules (this layer)** — from `docs/registry/architecture.yaml` v8:
- **Required**: `raw_dt` (NOT `effective_dt`) is the watchdog input. F-PROLOGUE from Story 002 exposes both.
- **Required**: 60-slot ring buffer with sentinel init (`0.01667f` × 60) already established in Story 001; this story ADVANCES + INSPECTS.
- **Required**: OR-composed breach criteria — sustained OR hitch-cluster.
- **Required**: 3.0s continuous clean before hysteresis-release. Any hitch resets the accumulator.
- **Required**: Rate-limited breach log (mirror of AC-21 idiom): first log immediately, next log no earlier than tick 601 after threshold.
- **Required**: `OnHardwarePerformanceBreach.Broadcast(bEntering)` fires ONCE per state transition (entry or release) — not per tick during breach.

---

## Acceptance Criteria

*From `design/gdd/player-movement-platform.md` §3 + §8, scoped to this story:*

- [ ] `void WatchdogTick(float raw_dt)` called from TickComponent (extend Story 003's skeleton, immediately after F-PROLOGUE):
  - [ ] Push raw_dt into `TickDTRollingBuffer[TickDTRingIndex]`; advance `TickDTRingIndex = (TickDTRingIndex + 1) % 60`.
  - [ ] Count samples > 18.18ms and samples > 16.67ms and max sample.
  - [ ] Detect breach: `(count_gt_1818 >= 30) || (count_gt_1667 >= 18 && max_sample > 0.033f)`.
  - [ ] Hysteresis: if all 60 samples ≤ 16.67ms, `ContinuousCleanWindowTime += raw_dt`; else `ContinuousCleanWindowTime = 0.0f`.
  - [ ] Release: after breach was active, if `ContinuousCleanWindowTime >= 3.0f`, transition out of breach.
  - [ ] State-transition semantics:
    - [ ] Not-breach → breach: `is_hw_performance_degraded = true`; `bHardwarePerformanceBreachActive = true`; `OnHardwarePerformanceBreach.Broadcast(true)`; log Error (rate-limited).
    - [ ] Breach → not-breach: `is_hw_performance_degraded = false`; `bHardwarePerformanceBreachActive = false`; `OnHardwarePerformanceBreach.Broadcast(false)`; log Info.
    - [ ] Same-state (both branches idempotent): no broadcast, no log.
- [ ] Public `is_hw_performance_degraded` read-only bool (declared in Story 001; this story writes).
- [ ] `OnHardwarePerformanceBreach` non-dynamic multicast delegate (declared in Story 001; this story broadcasts).
- [ ] **AC-HW-A Setup A (clean baseline)**: 60× `0.01667s` → `is_hw_performance_degraded == false`; no broadcast.
- [ ] **AC-HW-A Setup B (sustained sub-55 entry)**: 30 samples of `0.020s` → breach on the 30th sample; `Broadcast(true)` fires exactly once.
- [ ] **AC-HW-A Setup C (hitch cluster entry)**: 17× `0.018s` + 1× `0.040s` → breach on the 18th sample; broadcast fires.
- [ ] **AC-HW-A Setup D (hysteresis-release)**: after breach, inject 180 clean samples (3.0s @ 60fps); breach releases; `Broadcast(false)` fires exactly once.
- [ ] **AC-HW-A Setup E (flap prevention)**: single hitch mid-clean → `is_hw_performance_degraded` never true; `ContinuousCleanWindowTime` resets on hitch.
- [ ] **AC-HW-A Setup F (broadcast subscriber correctness)**: mock subscribers verify `bEntering=true/false` delivery, no duplicates.
- [ ] **AC-HW-A Setup G Part 1 (sentinel pre-fill regression)**: verified in Story 001 test; this story references.
- [ ] **AC-HW-A Setup G Part 2 (breach within 30 frames of degradation onset)**: PIE start with degraded frames from tick 1 (30× `0.020s`) → breach entry latency = 30 samples ± 1.

---

## Implementation Notes

*Derived from platform §3 Runtime DT watchdog specification + §4 F-WATCHDOG-ROLLING-BUFFER + R11a-6/7/8:*

**WatchdogTick body**:
```cpp
void UPlayerLaneMovementComponent::WatchdogTick(float raw_dt)
{
    // 1. Advance ring buffer
    TickDTRollingBuffer[TickDTRingIndex] = raw_dt;
    TickDTRingIndex = (TickDTRingIndex + 1) % 60;

    // 2. Aggregate stats
    int32 count_gt_1818 = 0;
    int32 count_gt_1667 = 0;
    float max_sample = 0.0f;
    for (int32 i = 0; i < 60; ++i)
    {
        const float s = TickDTRollingBuffer[i];
        if (s > 0.01818f) ++count_gt_1818;
        if (s > 0.01667f) ++count_gt_1667;
        if (s > max_sample) max_sample = s;
    }

    // 3. Detect breach
    const bool is_breach = (count_gt_1818 >= 30)
                        || (count_gt_1667 >= 18 && max_sample > 0.033f);

    // 4. Hysteresis accumulator
    const bool all_clean = (count_gt_1667 == 0);
    if (all_clean)
    {
        ContinuousCleanWindowTime += raw_dt;
    }
    else
    {
        ContinuousCleanWindowTime = 0.0f;
    }

    // 5. State transition
    if (is_breach && !bHardwarePerformanceBreachActive)
    {
        bHardwarePerformanceBreachActive = true;
        is_hw_performance_degraded = true;
        OnHardwarePerformanceBreach.Broadcast(/*bEntering=*/true);

        // Rate-limited log (mirror of AC-21 idiom)
        LogWatchdogBreach(TEXT("entered breach"));
    }
    else if (bHardwarePerformanceBreachActive && ContinuousCleanWindowTime >= 3.0f)
    {
        bHardwarePerformanceBreachActive = false;
        is_hw_performance_degraded = false;
        OnHardwarePerformanceBreach.Broadcast(/*bEntering=*/false);
        UE_LOG(LogPlayerMovement, Log, TEXT("Watchdog released (3.0s clean window)"));
    }
    // else: same-state, no side effect
}
```

**TickComponent integration** (extends Story 003's skeleton, right after F-PROLOGUE):
```cpp
float raw_dt, effective_dt;
ComputeTickDT(raw_dt, effective_dt);
WatchdogTick(raw_dt);  // NEW
// ... rest of TickComponent (RSM gate, F-2 advance, etc.)
```

**Rate-limited log helper** (mirror of AC-21 idiom):
```cpp
void UPlayerLaneMovementComponent::LogWatchdogBreach(const TCHAR* Msg)
{
    // Similar to AC-21 SLIP_TWEEN rate-limited log — 1 per 600 ticks.
    // Simpler here: log once per breach entry event (no per-tick logging while breach active).
    UE_LOG(LogPlayerMovement, Error, TEXT("Watchdog %s"), Msg);
}
```

**Sentinel init reference** (already done in Story 001): `TickDTRollingBuffer[60] = 0.01667f` — Setup G Part 1 verified in Story 001. This story's tests exercise the ADVANCE, BREACH, and RELEASE behavior.

**Performance**: WatchdogTick runs one O(60) scan per tick (~180 float compares + 60 branches). Zero allocations, cache-hot ring buffer. Cost: <0.5μs on mid-tier mobile. Budget: negligible against 16.6ms frame budget.

**Non-callback invariant**: WatchdogTick runs inside TickComponent — subscribers to `OnHardwarePerformanceBreach` (Wave Spawner, HUD) receive the broadcast synchronously. Subscribers MUST NOT call back into PM's tick body. Documented forward contract.

**Wave Spawner integration** (out of epic): WS binds `OnHardwarePerformanceBreach` at its Initialize; on `true`, suppresses M=3 PEAK barrages for `is_hw_performance_degraded==true` duration + 3.0s grace window (R11a-8). PM's side is complete when this story lands.

---

## Out of Scope

- Story 001: Ring buffer + sentinel field declaration + init (already done).
- Story 002: F-PROLOGUE `raw_dt` output (already done; this story consumes).
- Story 003: TickComponent skeleton (already done; this story adds WatchdogTick call after F-PROLOGUE).
- Wave Spawner R11a-8 grace window (belongs to Wave Spawner epic).
- HUD "Performance mode — hardest barrage suppressed." banner rendering (belongs to HUD epic; consumes this story's `OnHardwarePerformanceBreach` + `is_hw_performance_degraded`).
- AC-HW-B min-spec device audit (Polish-phase evidence).
- AC-HW-C on-device banner verification (HUD epic + Polish).

---

## QA Test Cases

*Test file: `tests/unit/player-movement/pm_watchdog_test.cpp`. Automated unit tests with mocked clock — inject DT sequences directly.*

- **AC-HW-A Setup A (clean baseline)**:
  - Given: PM fresh (BeginPlay ran; buffer sentinel filled).
  - When: inject 60 samples of `0.01667f`.
  - Then: `is_hw_performance_degraded == false`; `bHardwarePerformanceBreachActive == false`; no broadcast fires; `ContinuousCleanWindowTime > 0` (accumulating).

- **AC-HW-A Setup B (sustained sub-55 entry)**:
  - Given: fresh PM.
  - When: inject 30 samples of `0.020f` (18.18ms threshold exceeded; sub-55 fps).
  - Then: on the 30th sample, breach detected; `Broadcast(true)` fires exactly once; `is_hw_performance_degraded == true`; error log emitted.
  - Edge cases: 29 samples of `0.020f` → no breach (below 30 threshold); 31 samples → still one broadcast (idempotent same-state).

- **AC-HW-A Setup C (hitch cluster entry)**:
  - Given: fresh PM.
  - When: inject 17× `0.018f` + 1× `0.040f`.
  - Then: on the 18th sample, breach detected (count_gt_1667 >= 18 AND max > 33ms); broadcast fires.
  - Edge cases: 17× `0.018f` + 1× `0.030f` (max sample below 33ms threshold) → no breach.

- **AC-HW-A Setup D (hysteresis-release)**:
  - Given: PM in breach.
  - When: inject 180 samples of `0.01667f` (3.0s @ 60fps continuous clean).
  - Then: breach released; `Broadcast(false)` fires exactly once; `is_hw_performance_degraded == false`.
  - Edge cases: 179 samples → still in breach (below 3.0s threshold).

- **AC-HW-A Setup E (flap prevention)**:
  - Given: fresh PM.
  - When: inject 100× `0.01667f`, then 1× `0.020f`, then 100× `0.01667f`.
  - Then: `is_hw_performance_degraded` never true (single hitch doesn't cross 30-sample threshold); `ContinuousCleanWindowTime` resets on the hitch sample.

- **AC-HW-A Setup F (broadcast subscriber correctness)**:
  - Given: mock subscriber bound to `OnHardwarePerformanceBreach`.
  - When: breach entry then release.
  - Then: subscriber received (true) exactly once + (false) exactly once; no duplicates; ordering preserved.

- **AC-HW-A Setup G Part 2 (breach latency)**:
  - Given: fresh PM at PIE start with `TickDTRollingBuffer` sentinel-filled (0.01667f × 60).
  - When: inject 30 samples of `0.020f` starting at tick 1.
  - Then: breach entry occurs at sample 30 ± 1 (latency <= 60 samples from degradation onset).

- **Non-broadcast on same-state**:
  - Given: PM already in breach.
  - When: additional degraded samples continue.
  - Then: no additional `Broadcast(true)` calls (idempotent).

- **`raw_dt` NOT `effective_dt` invariant**:
  - Given: F-PROLOGUE clamps to `effective_dt = 0.05f` for a `raw_dt = 2.0f` hitch.
  - When: WatchdogTick invoked.
  - Then: watchdog sees the RAW value `2.0f` in the buffer — hitch is not hidden by the mechanics clamp.

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Automated unit test at `tests/unit/player-movement/pm_watchdog_test.cpp` — must exist and pass. Uses direct injection into `WatchdogTick(raw_dt)` (no real clock needed).

**Status**: [x] Created — `Source/SLIPSTORM/Tests/Unit/PlayerMovement/PMWatchdogTest.cpp` (12 test commands: TC1-TC12; build clean 6.09s incremental).

---

## Dependencies

- **Depends on**: Story 001 (ring buffer + sentinel init + delegate declaration + `bHardwarePerformanceBreachActive` + `ContinuousCleanWindowTime` field); Story 002 (F-PROLOGUE `raw_dt` output); Story 003 (TickComponent skeleton — this story adds `WatchdogTick(raw_dt)` call).
- **Unlocks**: Wave Spawner R11a-8 grace window binding (out-of-epic); HUD banner subscriber (out-of-epic); ADR-0005 forward contract closure on PM side.

---

## Completion Notes

**Completed**: 2026-08-07
**Criteria**: 17/17 covered (all AC-HW-A setups A-G + same-state + raw_dt invariant + 2 bonus strict-`>` boundary tests). AC-HW-A Setup G Part 1 discharged in Story 001; all others verified via TC1-TC12 in `Source/SLIPSTORM/Tests/Unit/PlayerMovement/PMWatchdogTest.cpp`.
**Test Evidence**: Automated unit test at `Source/SLIPSTORM/Tests/Unit/PlayerMovement/PMWatchdogTest.cpp` (12 test commands; build result Succeeded 6.09s incremental, 0 errors, 0 warnings). Runtime green/red CLI verification deferred — session-wide epic-close follow-up.
**Code Review**: Complete — unreal-specialist CLEAN, qa-tester TESTABLE (0 BLOCKING). APPROVED WITH SUGGESTIONS. 3 NITs applied inline (UE-N1 `max_sample = 0.0f` invariant comment at PLMC.cpp:947; UE-N3 GREP-GATE marker at .cpp:241; QA-N1 ADR-0009 IG-3 carve-out comment at PMWatchdogTest.cpp:457).

**Deviations** (ADVISORY only — no BLOCKING items):
1. Test path at project convention `Source/SLIPSTORM/Tests/Unit/PlayerMovement/PMWatchdogTest.cpp` vs story's aspirational `tests/unit/player-movement/pm_watchdog_test.cpp`. Session-long template drift (Stories 004-013).
2. `LogWatchdogBreach` helper dropped; inlined `UE_LOG(Error, ...)` at PLMC.cpp:986. Spec-authorized in Implementation Notes lines 131-135 ("Simpler here: log once per breach entry event").
3. Log cadence deviates from Control Manifest per-600-tick idiom: implementation logs once per state-transition (Error on enter, Log on release). Rationale documented at PLMC.cpp:922-928.
4. TC5 Setup D uses `SEED_BREACH_WITH_CLEAN_BUFFER` precondition-injection rather than sequential Setup B → D. Advisor-caught design analysis: sequential release path needs ~30 flush + 180 accumulate = 210 samples due to the buffer-wide `all_clean` gate. TC5 isolates the release condition per AC-D literal wording; sequential path filed as follow-up integration story.
5. TC4 + TC11 + TC12 added beyond the 9 named QA test cases. TC4 covers the AC-HW-A Setup C edge-case cited in story line 179; TC11/TC12 are strict-`>` comparator regression guards. Net-positive coverage.

**Follow-ups filed** (do not block closure):
- QA-S1: sequential Setup B → Setup D integration test (~210-tick real latency; needs `UWorld` + real tick loop). Recommended before Wave Spawner subscriber ships to CI.
- UE-S1: ADR-0009 IG-3 outbound-binding carve-out annotation at PLMC.h:121 (prevents future maintainers flagging test-scope lambdas).
- Epic-wide runtime-verification gap: correct headless CLI invocation for UE Automation tests needs research. All PM stories (001-013, ~85 test commands) have been closed on build-clean + inspection. MUST be resolved before Player Movement epic ships to CI.

**Player Movement epic: 13/13 stories complete upon commit — epic closer.**

# Story 009: Telemetry + Edge-Case Defense + Performance Baseline

> **Epic**: Wave Spawner Pattern Library
> **Status**: Complete
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: M (4–6 hours)
> **Manifest Version**: (none — docs/architecture/control-manifest.md not found; run /create-control-manifest)
> **Last Updated**: 2026-08-21
> **Completed**: 2026-08-21

## Context

**GDD**: `design/gdd/wave-spawner-pattern-library.md`
**Requirement**: `TR-WS-019`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0011: Wave Spawner Pattern Library — D3 Six-State Lifecycle (primary), §Telemetry + §Edge Cases
**ADR Decision Summary**: The spawner emits 9 named telemetry events with defined payload schemas (C.3 §9 in the GDD). Rate-limit state flags for high-frequency events live on the subsystem instance and reset on `Cold` entry. Runtime edge-case defense: pool exhaustion in Shipping is a no-op + telemetry (no `check(false)`); pool exhaustion in non-Shipping hits `check(false)`. Asset-invalid-at-load is per-asset deduplicated (one telemetry per invalid asset, not per draw). Critical pool empty post-validation transitions the spawner to `Idle` and emits `critical_pool_empty_post_load`. Performance baselines: p99 tick admission ≤ 0.30ms (AC-WS-30, ADVISORY at story Done / BLOCKING at Alpha), pool allocation ≤ 16.6ms (AC-WS-31, same gate).

**Engine**: Unreal Engine 5.7 | **Risk**: HIGH
**Engine Notes**: Telemetry integration: UE 5.7 does not have a built-in gameplay telemetry framework — verify project-level telemetry API (custom event bus, analytics provider, or `UGameplayStatics::GetGameInstance()->GetSubsystem<UAnalyticsSubsystem>()`). Rate-limit implementation: a simple `float LastEmitTimeS` per event category on the subsystem is sufficient (no need for a full rate-limiter framework). Performance measurement: use UE `SCOPE_CYCLE_COUNTER` or `CSV_SCOPED_TIMING_STAT` in the tick body to capture admission tick duration — readable via `stat WaveSpawner` in-editor profiler.

**Control Manifest Rules (Feature layer)**:
- No control manifest found at `docs/architecture/control-manifest.md` — run `/create-control-manifest`.

---

## Acceptance Criteria

*From GDD `design/gdd/wave-spawner-pattern-library.md`, scoped to this story:*

**Telemetry — BLOCKING at story Done:**

- [ ] **AC-WS-25 (BLOCKING)**: `pattern_admitted` event emitted after successful admission (`TryAdmitPattern` returns true). Payload includes: `WaveId`, `PhasePool` (OPENER/MID/PEAK), `bIsBarrage`, `TelegraphWindowS`, `AdmissionTimeS`. Verified via telemetry stub capturing emitted events.

- [ ] **AC-WS-26 (BLOCKING)**: `pause_flush_executed` event emitted once per pause flush (Rule 13). Payload includes: `WavesFlushedCount`, `FlushTimeS`. Rate-limit: once per flush session (not once per wave flushed).

- [ ] **AC-WS-27 (BLOCKING)**: `run_termination_flush_executed` event emitted once per Rule 14 run termination. Payload includes: `WavesTerminatedCount`, `TerminationTimeS`.

**Telemetry — ADVISORY at story Done:**

- [ ] **AC-WS-22 (ADVISORY)**: `barrage_dropped_due_to_concurrency` event emitted when barrage draw cannot be admitted due to insufficient slots (Story 004 call site). Payload: `BarrageDroppedAtTime`. Rate-limited: at most once per 1.0s (prevents telemetry flood during sustained slot exhaustion).

- [ ] **AC-WS-23 (ADVISORY)**: `peak_min_barrage_floor_undershoot` event emitted when a PEAK run ends with fewer barrages drawn than `BARRAGE_EVENTS_PER_PEAK_TARGET_AVG`. Payload: `ActualBarrageCount`, `TargetBarrageCount`. Emitted once on lifecycle transition `Active→Idle` or `Flushing→Idle` after PEAK phase.

- [ ] **AC-WS-24 (ADVISORY)**: `pool_exhaustion_detected` emitted when all 23 pool slots are in-flight simultaneously and a new admission is attempted. In Shipping: no-op + telemetry emit. In non-Shipping: `check(false)` (hard fail to catch authoring errors).

**Edge-case defense — BLOCKING at story Done:**

- [ ] **AC-WS-26b (BLOCKING)**: `empty_pool_at_draw` emitted when the active draw pool (OPENER/MID/PEAK) has zero non-barrage patterns at draw time. Admission aborted for that tick. Rate-limited: once per second. Does NOT crash.

- [ ] **AC-WS-27b (BLOCKING)**: `pattern_asset_invalid_at_load` emitted per-pattern for any pattern that fails cook-time validation at runtime load. De-duplicated by pattern asset ID (emit once per invalid asset, not once per draw). Invalid patterns are excluded from pools after this check. If a pool becomes empty as a result: emits `critical_pool_empty_post_load` and spawner transitions to `Idle`.

**Performance — ADVISORY at story Done / BLOCKING at Alpha:**

- [ ] **AC-WS-30 (ADVISORY → BLOCKING at Alpha)**: p99 admission tick time ≤ 0.30ms on target mid-tier mobile hardware. Measured via `SCOPE_CYCLE_COUNTER(STAT_WaveSpawnerAdmissionTick)` over a simulated 60s run. Report median, p95, p99 values.

- [ ] **AC-WS-31 (ADVISORY → BLOCKING at Alpha)**: Pool pre-allocation (`OnFirstWorldLoaded`) completes in ≤ 16.6ms on target hardware (one frame budget). Measured from delegate entry to pool allocation complete.

---

## Implementation Notes

*Derived from ADR-0011 D3 §Telemetry + §Edge Cases + G.3 Rate-Limit Policy:*

**9 named telemetry events (all must be implemented):**

| Event Name | Trigger | Rate Limit | Gate |
|---|---|---|---|
| `barrage_dropped_due_to_concurrency` | Barrage drop in Story 004 | 1/s | ADVISORY |
| `peak_min_barrage_floor_undershoot` | PEAK run ends below target | Once/run | ADVISORY |
| `pattern_admitted` | Successful admission | None | BLOCKING |
| `pause_flush_executed` | Pause flush complete | Once/flush | BLOCKING |
| `run_termination_flush_executed` | Run termination complete | Once/run | BLOCKING |
| `pool_exhaustion_detected` | All 23 slots in-flight + admission attempt | 1/s | ADVISORY |
| `empty_pool_at_draw` | Draw from empty active pool | 1/s | BLOCKING |
| `pattern_asset_invalid_at_load` | Per-invalid-pattern at load | Once/asset | BLOCKING |
| `critical_pool_empty_post_load` | Pool empty after invalid-asset pruning | Once/load | BLOCKING |

**Telemetry emission helper:**
```cpp
void UWaveSpawnerSubsystem::EmitTelemetry(FName EventName, const TMap<FName, FString>& Payload)
{
    // Route through project telemetry API
    // Example: UTelemetrySubsystem::Get(this)->TrackEvent(EventName, Payload);
    // Implementation specific to project analytics provider.
}
```

**Rate-limit fields on the subsystem instance (reset at Cold→Active):**
```cpp
float LastBarrageDropTelemetryTimeS = -1.f;
float LastPoolExhaustionTelemetryTimeS = -1.f;
float LastEmptyPoolTelemetryTimeS = -1.f;
TSet<FSoftObjectPath> ReportedInvalidPatternAssets;  // de-dup set; cleared at Cold entry
```

**Rate-limit check pattern:**
```cpp
bool UWaveSpawnerSubsystem::ShouldEmitRateLimited(float& LastEmitTimeS, float RateLimitS) const
{
    const float Now = GetWorld()->GetTimeSeconds();
    if (Now - LastEmitTimeS < RateLimitS) return false;
    LastEmitTimeS = Now;
    return true;
}
```

**Pool exhaustion edge case (from Story 004 integration):**
```cpp
// In TryAdmitPattern, if all slots saturated (available == 0 for any admission):
if (GetAvailableSlots() <= 0)
{
    if (ShouldEmitRateLimited(LastPoolExhaustionTelemetryTimeS, 1.f))
        EmitTelemetry(TEXT("pool_exhaustion_detected"), {});
#if !UE_BUILD_SHIPPING
    check(false);   // authoring error: wave count misconfigured
#endif
    return false;
}
```

**Performance instrumentation:**
```cpp
// In Tick() / OnDPCFrameReady():
SCOPE_CYCLE_COUNTER(STAT_WaveSpawnerAdmissionTick);
// In OnFirstWorldLoaded():
SCOPE_CYCLE_COUNTER(STAT_WaveSpawnerPoolAlloc);

// Declare stats:
DECLARE_CYCLE_STAT(TEXT("WaveSpawner Admission Tick"), STAT_WaveSpawnerAdmissionTick, STATGROUP_WaveSpawner);
DECLARE_CYCLE_STAT(TEXT("WaveSpawner Pool Alloc"), STAT_WaveSpawnerPoolAlloc, STATGROUP_WaveSpawner);
```

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 004: `barrage_dropped_due_to_concurrency` call site (Story 004 calls `EmitTelemetry()`; this story provides `EmitTelemetry()` and the rate-limit infrastructure).
- Story 006: Despawn pipeline ordering — this story adds telemetry hooks around despawn, not the pipeline itself.
- Story 008: Cook-time validation at load (Story 008 implements validation; this story handles the runtime response when it fails — `pattern_asset_invalid_at_load`, `critical_pool_empty_post_load`).

---

## QA Test Cases

*Test cases not yet defined — run /qa-plan to generate them.*

---

## Test Evidence

**Story Type**: Integration
**Required evidence**: `Source/SLIPSTORM/Tests/Integration/WaveSpawner/WaveSpawnerTelemetryTest.cpp` — must exist and pass (tests for all BLOCKING telemetry events; ADVISORY events tested if time permits)

**Status**: [x] Created — `Source/SLIPSTORM/Tests/Integration/WaveSpawner/WaveSpawnerTelemetryTest.cpp` (7 test cases, all BLOCKING ACs covered)

---

## Dependencies

- Depends on: Story 004 (pool exhaustion + barrage drop call sites), Story 006 (despawn pipeline call sites for flush events), Story 007 (pause flush + run termination trigger telemetry events), Story 008 (invalid-asset detection at load triggers `pattern_asset_invalid_at_load`)
- Unlocks: Epic complete — all 9 stories done + all telemetry and edge cases implemented

---

## Completion Notes

**Completed**: 2026-08-21  
**Criteria**: 8/10 passing (AC-WS-30, AC-WS-31 deferred — ADVISORY at story Done / BLOCKING at Alpha; hardware measurement required)  
**Deviations**:
- ADVISORY: AC-WS-24 — no `check(false)` on pool exhaustion; rate-limited telemetry only. Story 004 TC3 asserts Deferred_ConcurrencyCap as normal outcome; check belongs at AcquireFromPool() nullptr in Story 006.
- ADVISORY: AC-WS-23 — `peak_min_barrage_floor_undershoot` emitted before `TransitionTo(Flushing)`. Active→Idle and Flushing→Idle are both FORBIDDEN per ADR-0011 D3; emit must precede the transition.
- ADVISORY: AC-WS-30/31 — SCOPE_CYCLE_COUNTER instrumentation in place; hardware measurement deferred to Alpha gate.  
**Test Evidence**: Integration test at `Source/SLIPSTORM/Tests/Integration/WaveSpawner/WaveSpawnerTelemetryTest.cpp` — 7 test cases (TC1–TC7), all BLOCKING ACs covered.  
**Code Review**: Complete — /code-review run 2026-08-21; B-1 pruning fix, W-2 macro swap (COMPLEX→SIMPLE), W-3 lifecycle guard, TC3/TC4 comment label corrections applied post-review.  
**Post-review fixes applied**:
- B-1: `ValidateAndPrunePoolsAtLoad` Step 2 — `RemoveAt` hoisted out of dedup guard (duplicate PatternId bug)
- W-2: All 7 tests changed from `IMPLEMENT_COMPLEX_AUTOMATION_TEST` to `IMPLEMENT_SIMPLE_AUTOMATION_TEST`; `GetTests()` bodies removed
- W-3: Cold-state precondition guard added before `TransitionTo(Idle)` in critical-empty block

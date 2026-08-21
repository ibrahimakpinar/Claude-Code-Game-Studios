# Story 008: Cook-Time Validator (14 Binding Rule 15 Checks)

> **Epic**: Wave Spawner Pattern Library
> **Status**: Complete
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: (fill before sprint planning)
> **Manifest Version**: (none — docs/architecture/control-manifest.md not found; run /create-control-manifest)
> **Last Updated**: 2026-08-21

## Context

**GDD**: `design/gdd/wave-spawner-pattern-library.md`
**Requirement**: `TR-WS-001`, `TR-WS-002`, `TR-WS-003`, `TR-WS-004`, `TR-WS-005`, `TR-WS-006`, `TR-WS-007`, `TR-WS-031`, `TR-WS-032`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0011: Wave Spawner Pattern Library — D4 Cook-Time Contract (primary)
**ADR Decision Summary**: Rule 15 defines 14 binding cook-time checks that must pass before pools are considered valid for runtime use. The validator runs at cook time (UE data validation, Commandlet, or CI test) and returns a list of errors. A pool with any Rule 15 violation MUST NOT reach runtime — the spawner is permitted to `check(false)` or log critical-and-disable if violations are detected. The 14 checks are: OPENER_NO_BARRAGE, MID_NO_BARRAGE, PEAK_SURVIVING_TRIPLETS, PEAK_BARRAGE_MIN_TIER, BARRAGE_W_SPAN, NON_BARRAGE_STAGGER, PRIMER_PATTERN, PILLAR_1_VERB_SLIP, POOL_NON_EMPTY, PEAK_BASE_W_RANGE, PEAK_BASE_W_BELOW_CEILING, MIN_BARRAGE_PATTERN_COUNT_PER_TRIPLET, BARRAGE_UNIFORM_TIER, BARRAGE_DISTINCT_SOURCE_LANES.

**Engine**: Unreal Engine 5.7 | **Risk**: HIGH
**Engine Notes**: Cook-time validation in UE 5.7 can be implemented as: (a) a `UObject::IsDataValid()` override on the pool data asset, returning `EDataValidationResult`; (b) a UE Commandlet (`UCommandlet` subclass) invoked in CI headless build; (c) a dedicated UE Automation test (`IMPLEMENT_SIMPLE_AUTOMATION_TEST`) run pre-cook. Prefer option (c) for testability — automation tests can be run headlessly with `-nullrhi`. The choice of integration mechanism is left to the implementer; the 14 checks themselves are the binding contract regardless of mechanism. All checks work on `FPatternPool` structs that are populated at data-asset load time — validator must not mutate pool data.

**Control Manifest Rules (Feature layer)**:
- No control manifest found at `docs/architecture/control-manifest.md` — run `/create-control-manifest`.

---

## Acceptance Criteria

*From GDD `design/gdd/wave-spawner-pattern-library.md`, scoped to this story:*

- [ ] **AC-WS-01 (BLOCKING)**: OPENER_NO_BARRAGE — OPENER pool contains no barrage patterns. Given an OPENER pool with one barrage pattern: validator returns error `OPENER_NO_BARRAGE`. Given an OPENER pool with only non-barrage patterns: passes.

- [ ] **AC-WS-02 (BLOCKING)**: MID_NO_BARRAGE — MID pool contains no barrage patterns. Same logic as AC-WS-01 for MID pool.

- [ ] **AC-WS-03 (BLOCKING)**: PEAK_SURVIVING_TRIPLETS — PEAK barrage sub-pool contains patterns covering exactly the 7 target lane triplets: `{0,1,3},{0,1,4},{0,2,3},{0,2,4},{0,3,4},{1,2,4},{1,3,4}`. Missing or extra triplets fail.

- [ ] **AC-WS-04 (BLOCKING)**: PEAK_BARRAGE_MIN_TIER — Every PEAK barrage pattern has tier ≥ 2 (tier-1 barrages forbidden). Given any barrage pattern with tier < 2: validator returns `PEAK_BARRAGE_MIN_TIER`.

- [ ] **AC-WS-05 (BLOCKING)**: BARRAGE_W_SPAN — Barrage patterns cluster all telegraph onsets within `TELEGRAPH_WINDOW_FLOOR / 2 = 0.35s`. Given a barrage pattern with onset spread > 0.35s: validator returns `BARRAGE_W_SPAN`.

- [ ] **AC-WS-06 (BLOCKING)**: NON_BARRAGE_STAGGER — Non-barrage patterns stagger consecutive onsets ≥ `TELEGRAPH_WINDOW_FLOOR_S = 0.70s`. Given a non-barrage pattern with consecutive onsets < 0.70s apart: validator returns `NON_BARRAGE_STAGGER`.

- [ ] **AC-WS-07 (BLOCKING)**: PRIMER_PATTERN — At least one non-barrage pattern in the OPENER pool is tagged as primer-eligible. Given OPENER pool with no primer-eligible pattern: validator returns `PRIMER_PATTERN`.

- [ ] **AC-WS-08 (BLOCKING)**: PILLAR_1_VERB_SLIP — At least one pattern in each non-empty pool allows lane transitions (has `bAllowsSlip = true`). Given a pool with all patterns having `bAllowsSlip = false`: validator returns `PILLAR_1_VERB_SLIP`.

- [ ] **AC-WS-09 (BLOCKING)**: POOL_NON_EMPTY — Each of the three pools (OPENER, MID, PEAK) has at least one pattern (non-barrage or barrage as appropriate). Given any pool with zero patterns: validator returns `POOL_NON_EMPTY`.

- [ ] **AC-WS-32 (BLOCKING)**: PEAK_BASE_W_RANGE — `base_w` (unweighted barrage fraction, F-3 tuning knob) is in `[0.15, 0.40]`. Given `base_w = 0.10`: validator returns `PEAK_BASE_W_RANGE`. Given `base_w = 0.45`: same. Given `base_w = 0.30`: passes.

- [ ] **AC-WS-33 (BLOCKING)**: PEAK_BASE_W_BELOW_CEILING — `base_w < W_CEILING`. Given `base_w ≥ W_CEILING`: validator returns `PEAK_BASE_W_BELOW_CEILING` (prevents F-3 proportional branch silent saturation).

- [ ] **AC-WS-34 (BLOCKING)**: MIN_BARRAGE_PATTERN_COUNT_PER_TRIPLET — Each of the 7 target triplets has ≥ 1 pattern in the PEAK barrage sub-pool. Given a valid triplet set but one triplet with zero patterns: validator returns `MIN_BARRAGE_PATTERN_COUNT_PER_TRIPLET`.

- [ ] **AC-WS-35 (BLOCKING)**: BARRAGE_UNIFORM_TIER — All PEAK barrage patterns share the same tier value (uniform tier across the barrage sub-pool). Given mixed tiers (e.g., tier 2 and tier 3 in the same pool): validator returns `BARRAGE_UNIFORM_TIER`.

- [ ] **AC-WS-36 (BLOCKING)**: BARRAGE_DISTINCT_SOURCE_LANES — No two patterns in the PEAK barrage sub-pool share the same source lane set (the 3 source lanes in a triplet must be distinct from all other patterns' source lane sets). Given duplicate triplet (same 3 lanes): validator returns `BARRAGE_DISTINCT_SOURCE_LANES`.

> **AC-WS-37 / AC-WS-38 note**: ACs above map to the 14 named checks in ADR-0011 D4. Read the GDD AC section H.1 for the authoritative AC numbering and any additional edge conditions per check.

---

## Implementation Notes

*Derived from ADR-0011 D4 Implementation Guidelines:*

**Validator structure (UE Automation Test preferred):**
```cpp
// Source/SLIPSTORM/Tests/Unit/WaveSpawner/WaveSpawnerCookTimeValidatorTest.cpp
// One IMPLEMENT_SIMPLE_AUTOMATION_TEST per Rule 15 check for isolation.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWaveSpawnerValidatorOpenerNoBarrage,
    "SLIPSTORM.WaveSpawner.CookTime.OpenerNoBarrage",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FWaveSpawnerValidatorOpenerNoBarrage::RunTest(const FString& Parameters)
{
    FPatternPool Pool;
    Pool.BarragePatterns.Add(MakeBarragePattern());
    TArray<FString> Errors;
    UWaveSpawnerCookTimeValidator::Validate(Pool, EPhasePool::OPENER, Errors);
    TestTrue("OPENER_NO_BARRAGE error returned", Errors.Contains(TEXT("OPENER_NO_BARRAGE")));
    return true;
}
```

**Validator entry point:**
```cpp
// Source/SLIPSTORM/WaveSpawner/WaveSpawnerCookTimeValidator.h
class SLIPSTORM_API UWaveSpawnerCookTimeValidator
{
public:
    /**
     * Run all 14 Rule 15 binding checks on the provided pools and knob values.
     * Returns a list of error codes (empty = all checks pass).
     * MUST NOT mutate pool data.
     */
    static TArray<FString> Validate(
        const FPatternPool& OpenerPool,
        const FPatternPool& MidPool,
        const FPatternPool& PeakPool,
        float BaseW,
        float WCeiling);
};
```

**Per-check implementation pattern (example — BARRAGE_W_SPAN):**
```cpp
static void CheckBarrageWSpan(const FPatternPool& Pool, TArray<FString>& OutErrors)
{
    constexpr float MaxSpan = TELEGRAPH_WINDOW_FLOOR_S / 2.f;  // 0.35s
    for (const FPatternDefinition& Pat : Pool.BarragePatterns)
    {
        if (Pat.OnsetTimes.Num() < 2) continue;
        float MinT = Pat.OnsetTimes[0], MaxT = Pat.OnsetTimes[0];
        for (float T : Pat.OnsetTimes) { MinT = FMath::Min(MinT, T); MaxT = FMath::Max(MaxT, T); }
        if ((MaxT - MinT) > MaxSpan)
        {
            OutErrors.Add(FString::Printf(TEXT("BARRAGE_W_SPAN: pattern '%s' span=%.3fs > %.3fs"),
                *Pat.Name.ToString(), MaxT - MinT, MaxSpan));
        }
    }
}
```

**Integration with runtime pool load:**
- At `OnFirstWorldLoaded` (Story 001), after loading pool data assets, call `UWaveSpawnerCookTimeValidator::Validate(...)`.
- If errors returned: emit `pattern_asset_invalid_at_load` telemetry per invalid pattern; if all pools empty post-validation, emit `critical_pool_empty_post_load` and transition to `Idle` (Story 009).
- In non-Shipping builds: `check(Errors.Num() == 0)` to catch authoring errors immediately.
- In Shipping builds: log critical errors and skip invalid patterns.

**All 14 check IDs (error code strings):**
`OPENER_NO_BARRAGE`, `MID_NO_BARRAGE`, `PEAK_SURVIVING_TRIPLETS`, `PEAK_BARRAGE_MIN_TIER`, `BARRAGE_W_SPAN`, `NON_BARRAGE_STAGGER`, `PRIMER_PATTERN`, `PILLAR_1_VERB_SLIP`, `POOL_NON_EMPTY`, `PEAK_BASE_W_RANGE`, `PEAK_BASE_W_BELOW_CEILING`, `MIN_BARRAGE_PATTERN_COUNT_PER_TRIPLET`, `BARRAGE_UNIFORM_TIER`, `BARRAGE_DISTINCT_SOURCE_LANES`.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 001: Pool data loading (validator runs after pools are loaded; does not load them).
- Story 009: Telemetry for invalid assets at runtime (`pattern_asset_invalid_at_load`, `critical_pool_empty_post_load`).

---

## QA Test Cases

*Test cases not yet defined — run /qa-plan to generate them.*

---

## Test Evidence

**Story Type**: Logic
**Required evidence**: `Source/SLIPSTORM/Tests/Unit/WaveSpawner/WaveSpawnerCookTimeValidatorTest.cpp` — must exist and pass (14 tests minimum, one per Rule 15 check)

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: Story 001 (`FPatternPool` and `FPatternDefinition` struct definitions)
- Unlocks: Pool authoring can be validated before Story 007's RSM integration — validator can run in CI independently

## Completion Notes
**Completed**: 2026-08-21
**Criteria**: 14/14 passing (all BLOCKING)
**Deviations**:
- Story implementation note used `U` prefix; corrected to `FWaveSpawnerCookTimeValidator` per UE naming convention (F prefix for plain C++ types)
- `WaveSpawnerTypes.h` extended with 5 new cook-time fields (out of stated scope, valid dependency — Story 008 is the cook-time consumer)
- ADR-0011 D4 table amended: replaced `PEAK_NO_ADJACENT_CLUSTER`/`POOL_SIZE_DERIVATION_MATCH` with `MID_NO_BARRAGE`/`BARRAGE_W_SPAN`; corrected ≥ 2 → ≥ 1 per triplet in D1 and D4
- Test coverage gaps GAP-1 through GAP-8 (pool-dispatch arms, multi-onset stagger passing case): advisory, recommend addressing in follow-up
**Test Evidence**: `Source/SLIPSTORM/Tests/Unit/WaveSpawner/WaveSpawnerCookTimeValidatorTest.cpp` — 15 test commands (TC1–TC15)
**Code Review**: Complete — APPROVED after 4 required fixes (rename U→F, remove unused include, remove redundant static, fix TC4 duplicate assertion)

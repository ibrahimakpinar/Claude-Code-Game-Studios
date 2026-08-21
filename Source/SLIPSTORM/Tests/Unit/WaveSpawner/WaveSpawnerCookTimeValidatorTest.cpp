// Copyright (c) Game Studio. All rights reserved.
// SLIPSTORM — 60-second voxel endless runner, mobile, Unreal Engine 5.7
//
// WaveSpawnerCookTimeValidatorTest.cpp — Unit tests for the 14 Rule 15 binding
// cook-time checks implemented in FWaveSpawnerCookTimeValidator (ADR-0011 D4).
//
// TEST COMMANDS (15 total, one per Rule 15 check + one all-pass):
//   TC1  SLIPSTORM.WaveSpawner.CookTime.OpenerNoBarrage
//   TC2  SLIPSTORM.WaveSpawner.CookTime.MidNoBarrage
//   TC3  SLIPSTORM.WaveSpawner.CookTime.PeakSurvivingTriplets
//   TC4  SLIPSTORM.WaveSpawner.CookTime.PeakBarrageMinTier
//   TC5  SLIPSTORM.WaveSpawner.CookTime.BarrageWSpan
//   TC6  SLIPSTORM.WaveSpawner.CookTime.NonBarrageStagger
//   TC7  SLIPSTORM.WaveSpawner.CookTime.PrimerPattern
//   TC8  SLIPSTORM.WaveSpawner.CookTime.Pillar1VerbSlip
//   TC9  SLIPSTORM.WaveSpawner.CookTime.PoolNonEmpty
//   TC10 SLIPSTORM.WaveSpawner.CookTime.PeakBaseWRange
//   TC11 SLIPSTORM.WaveSpawner.CookTime.PeakBaseWBelowCeiling
//   TC12 SLIPSTORM.WaveSpawner.CookTime.MinBarragePatternCountPerTriplet
//   TC13 SLIPSTORM.WaveSpawner.CookTime.BarrageUniformTier
//   TC14 SLIPSTORM.WaveSpawner.CookTime.BarrageDistinctSourceLanes
//   TC15 SLIPSTORM.WaveSpawner.CookTime.AllChecksPass
//
// Story: production/epics/wave-spawner/story-008-cook-time-validator.md
// ADR:   docs/architecture/adr-0011-wave-spawner-pattern-library.md (D4)

#include "WaveSpawner/WaveSpawnerCookTimeValidator.h"
#include "WaveSpawner/WaveSpawnerTypes.h"
#include "Misc/AutomationTest.h"

// ---------------------------------------------------------------------------
// Test factory helpers (anonymous namespace — test-only, not exported)
// ---------------------------------------------------------------------------

namespace
{
    /**
     * Build a barrage FPatternDefinition for the given 3-lane triplet.
     * Defaults: tier=2, onset spread=0.28s (< 0.35s max), bAllowsSlip=true.
     */
    static FPatternDefinition MakeBarragePat(
        int32 L0, int32 L1, int32 L2,
        int32 Tier = 2,
        float OnsetSpread = 0.28f)
    {
        FPatternDefinition P;
        P.bIsBarrage = true;
        P.SourceLanes = { L0, L1, L2 };
        P.LeanMagnitudeTier = Tier;
        // Cluster 3 onsets within [0, OnsetSpread]
        P.OnsetTimes = { 0.0f, OnsetSpread * 0.4f, OnsetSpread };
        P.bAllowsSlip = true;
        return P;
    }

    /**
     * Build a non-barrage FPatternDefinition.
     * Defaults: single onset at 0.0f (trivially passes stagger check),
     *           bIsPrimerEligible=false, bAllowsSlip=true.
     */
    static FPatternDefinition MakeNonBarragePat(bool bPrimer = false, bool bSlip = true)
    {
        FPatternDefinition P;
        P.bIsBarrage = false;
        P.OnsetTimes = { 0.0f };  // single onset — no consecutive pair to check
        P.bIsPrimerEligible = bPrimer;
        P.bAllowsSlip = bSlip;
        return P;
    }

    /**
     * Build a valid minimal PEAK pool:
     *   BarragePatterns: exactly the 7 required triplets, each with tier=2 and
     *                    bAllowsSlip=true, onset spread=0.28s.
     *   NonBarragePatterns: 1 pattern with bAllowsSlip=true.
     * Passes all PEAK-related Rule 15 checks when paired with BaseW=0.25/WCeiling=0.60.
     */
    static FPatternPool MakeValidPeakPool()
    {
        FPatternPool Pool;
        Pool.BarragePatterns.Add(MakeBarragePat(0, 1, 3));
        Pool.BarragePatterns.Add(MakeBarragePat(0, 1, 4));
        Pool.BarragePatterns.Add(MakeBarragePat(0, 2, 3));
        Pool.BarragePatterns.Add(MakeBarragePat(0, 2, 4));
        Pool.BarragePatterns.Add(MakeBarragePat(0, 3, 4));
        Pool.BarragePatterns.Add(MakeBarragePat(1, 2, 4));
        Pool.BarragePatterns.Add(MakeBarragePat(1, 3, 4));
        Pool.NonBarragePatterns.Add(MakeNonBarragePat(false, true));
        return Pool;
    }

    /**
     * Build a valid minimal OPENER pool:
     *   NonBarragePatterns: 1 primer-eligible pattern with bAllowsSlip=true.
     * Passes all OPENER-related Rule 15 checks.
     */
    static FPatternPool MakeValidOpenerPool()
    {
        FPatternPool Pool;
        Pool.NonBarragePatterns.Add(MakeNonBarragePat(/*bPrimer=*/true, /*bSlip=*/true));
        return Pool;
    }

    /**
     * Build a valid minimal MID pool:
     *   NonBarragePatterns: 1 pattern with bAllowsSlip=true.
     * Passes all MID-related Rule 15 checks.
     */
    static FPatternPool MakeValidMidPool()
    {
        FPatternPool Pool;
        Pool.NonBarragePatterns.Add(MakeNonBarragePat(/*bPrimer=*/false, /*bSlip=*/true));
        return Pool;
    }

    /**
     * Returns true if any string in Errors contains the given Substring.
     * Used to test that a specific Rule 15 check ID appears in the error list.
     */
    static bool ErrorsContain(const TArray<FString>& Errors, const FString& Substring)
    {
        for (const FString& E : Errors)
        {
            if (E.Contains(Substring))
            {
                return true;
            }
        }
        return false;
    }

    // Default valid BaseW / WCeiling knob values (pass AC-WS-32 and AC-WS-33)
    static constexpr float kValidBaseW    = 0.25f;  // in [0.15, 0.40]
    static constexpr float kValidWCeiling = 0.60f;  // > kValidBaseW

}  // namespace

// ===========================================================================
// TC1: OPENER_NO_BARRAGE (AC-WS-01)
//   Fail: OPENER pool contains a barrage pattern → error "OPENER_NO_BARRAGE".
// ===========================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWaveSpawnerCookTimeOpenerNoBarrage,
    "SLIPSTORM.WaveSpawner.CookTime.OpenerNoBarrage",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FWaveSpawnerCookTimeOpenerNoBarrage::RunTest(const FString& Parameters)
{
    // Arrange: OPENER pool with one barrage pattern.
    FPatternPool Opener;
    Opener.BarragePatterns.Add(MakeBarragePat(0, 1, 3));
    Opener.NonBarragePatterns.Add(MakeNonBarragePat(true, true));  // pass other checks

    FPatternPool Mid  = MakeValidMidPool();
    FPatternPool Peak = MakeValidPeakPool();

    // Act
    TArray<FString> Errors = FWaveSpawnerCookTimeValidator::Validate(Opener, Mid, Peak, kValidBaseW, kValidWCeiling);

    // Assert: OPENER_NO_BARRAGE error present.
    TestTrue("OPENER_NO_BARRAGE error returned when OPENER pool contains a barrage pattern",
             ErrorsContain(Errors, TEXT("OPENER_NO_BARRAGE")));

    // Negative: a clean OPENER pool must produce no OPENER_NO_BARRAGE error.
    FPatternPool CleanOpener = MakeValidOpenerPool();
    TArray<FString> CleanErrors = FWaveSpawnerCookTimeValidator::Validate(CleanOpener, Mid, Peak, kValidBaseW, kValidWCeiling);
    TestFalse("No OPENER_NO_BARRAGE error when OPENER pool has no barrage patterns",
              ErrorsContain(CleanErrors, TEXT("OPENER_NO_BARRAGE")));

    return true;
}

// ===========================================================================
// TC2: MID_NO_BARRAGE (AC-WS-02)
//   Fail: MID pool contains a barrage pattern → error "MID_NO_BARRAGE".
// ===========================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWaveSpawnerCookTimeMidNoBarrage,
    "SLIPSTORM.WaveSpawner.CookTime.MidNoBarrage",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FWaveSpawnerCookTimeMidNoBarrage::RunTest(const FString& Parameters)
{
    // Arrange: MID pool with one barrage pattern.
    FPatternPool Mid;
    Mid.BarragePatterns.Add(MakeBarragePat(0, 2, 4));
    Mid.NonBarragePatterns.Add(MakeNonBarragePat(false, true));

    FPatternPool Opener = MakeValidOpenerPool();
    FPatternPool Peak   = MakeValidPeakPool();

    // Act
    TArray<FString> Errors = FWaveSpawnerCookTimeValidator::Validate(Opener, Mid, Peak, kValidBaseW, kValidWCeiling);

    // Assert
    TestTrue("MID_NO_BARRAGE error returned when MID pool contains a barrage pattern",
             ErrorsContain(Errors, TEXT("MID_NO_BARRAGE")));

    // Negative
    TArray<FString> CleanErrors = FWaveSpawnerCookTimeValidator::Validate(Opener, MakeValidMidPool(), Peak, kValidBaseW, kValidWCeiling);
    TestFalse("No MID_NO_BARRAGE error when MID pool has no barrage patterns",
              ErrorsContain(CleanErrors, TEXT("MID_NO_BARRAGE")));

    return true;
}

// ===========================================================================
// TC3: PEAK_SURVIVING_TRIPLETS (AC-WS-03)
//   Fail: PEAK barrage sub-pool is missing one of the 7 required triplets.
// ===========================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWaveSpawnerCookTimePeakSurvivingTriplets,
    "SLIPSTORM.WaveSpawner.CookTime.PeakSurvivingTriplets",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FWaveSpawnerCookTimePeakSurvivingTriplets::RunTest(const FString& Parameters)
{
    FPatternPool Opener = MakeValidOpenerPool();
    FPatternPool Mid    = MakeValidMidPool();

    // Arrange (fail): PEAK barrage pool with only 6 of the 7 required triplets — missing {1,3,4}.
    FPatternPool PeakMissing = MakeValidPeakPool();
    // Remove the last pattern ({1,3,4}) to leave only 6 distinct triplets.
    PeakMissing.BarragePatterns.RemoveAt(PeakMissing.BarragePatterns.Num() - 1);

    // Act
    TArray<FString> Errors = FWaveSpawnerCookTimeValidator::Validate(Opener, Mid, PeakMissing, kValidBaseW, kValidWCeiling);

    // Assert: error for the missing required triplet.
    TestTrue("PEAK_SURVIVING_TRIPLETS error when a valid triplet is absent from PEAK barrage pool",
             ErrorsContain(Errors, TEXT("PEAK_SURVIVING_TRIPLETS")));

    // Negative: full 7-triplet pool passes.
    TArray<FString> CleanErrors = FWaveSpawnerCookTimeValidator::Validate(Opener, Mid, MakeValidPeakPool(), kValidBaseW, kValidWCeiling);
    TestFalse("No PEAK_SURVIVING_TRIPLETS error with all 7 required triplets present",
              ErrorsContain(CleanErrors, TEXT("PEAK_SURVIVING_TRIPLETS")));

    return true;
}

// ===========================================================================
// TC4: PEAK_BARRAGE_MIN_TIER (AC-WS-04)
//   Fail: a PEAK barrage pattern has LeanMagnitudeTier = 1 (tier 1 is banned).
// ===========================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWaveSpawnerCookTimePeakBarrageMinTier,
    "SLIPSTORM.WaveSpawner.CookTime.PeakBarrageMinTier",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FWaveSpawnerCookTimePeakBarrageMinTier::RunTest(const FString& Parameters)
{
    FPatternPool Opener = MakeValidOpenerPool();
    FPatternPool Mid    = MakeValidMidPool();

    // Arrange (fail): PEAK pool with a tier-1 barrage pattern.
    FPatternPool PeakLowTier = MakeValidPeakPool();
    // Overwrite the first barrage pattern with tier=1.
    PeakLowTier.BarragePatterns[0] = MakeBarragePat(0, 1, 3, /*Tier=*/1);

    TArray<FString> Errors = FWaveSpawnerCookTimeValidator::Validate(Opener, Mid, PeakLowTier, kValidBaseW, kValidWCeiling);

    TestTrue("PEAK_BARRAGE_MIN_TIER error returned when a PEAK barrage pattern has tier=1",
             ErrorsContain(Errors, TEXT("PEAK_BARRAGE_MIN_TIER")));

    // BARRAGE_UNIFORM_TIER also fires: tier=1 differs from the remaining 6 patterns (tier=2).
    // Both check IDs are independently present in the accumulated error list.
    TestTrue("BARRAGE_UNIFORM_TIER also fires when the tier=1 pattern is inconsistent with others",
             ErrorsContain(Errors, TEXT("BARRAGE_UNIFORM_TIER")));

    // Negative: all tier-2 patterns pass.
    TArray<FString> CleanErrors = FWaveSpawnerCookTimeValidator::Validate(Opener, Mid, MakeValidPeakPool(), kValidBaseW, kValidWCeiling);
    TestFalse("No PEAK_BARRAGE_MIN_TIER error when all PEAK barrage patterns have tier >= 2",
              ErrorsContain(CleanErrors, TEXT("PEAK_BARRAGE_MIN_TIER")));

    return true;
}

// ===========================================================================
// TC5: BARRAGE_W_SPAN (AC-WS-05)
//   Fail: a PEAK barrage pattern has onset spread > 0.35s.
// ===========================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWaveSpawnerCookTimeBarrageWSpan,
    "SLIPSTORM.WaveSpawner.CookTime.BarrageWSpan",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FWaveSpawnerCookTimeBarrageWSpan::RunTest(const FString& Parameters)
{
    FPatternPool Opener = MakeValidOpenerPool();
    FPatternPool Mid    = MakeValidMidPool();

    // Arrange (fail): one barrage pattern with onset spread = 0.50s > 0.35s.
    FPatternPool PeakWide = MakeValidPeakPool();
    PeakWide.BarragePatterns[0].OnsetTimes = { 0.0f, 0.25f, 0.50f };  // span = 0.50s

    TArray<FString> Errors = FWaveSpawnerCookTimeValidator::Validate(Opener, Mid, PeakWide, kValidBaseW, kValidWCeiling);

    TestTrue("BARRAGE_W_SPAN error when barrage onset spread exceeds 0.35s",
             ErrorsContain(Errors, TEXT("BARRAGE_W_SPAN")));

    // Negative: spread = 0.28s passes.
    TArray<FString> CleanErrors = FWaveSpawnerCookTimeValidator::Validate(Opener, Mid, MakeValidPeakPool(), kValidBaseW, kValidWCeiling);
    TestFalse("No BARRAGE_W_SPAN error when barrage onset spread is within 0.35s",
              ErrorsContain(CleanErrors, TEXT("BARRAGE_W_SPAN")));

    return true;
}

// ===========================================================================
// TC6: NON_BARRAGE_STAGGER (AC-WS-06)
//   Fail: a non-barrage pattern has two consecutive onsets < 0.70s apart.
// ===========================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWaveSpawnerCookTimeNonBarrageStagger,
    "SLIPSTORM.WaveSpawner.CookTime.NonBarrageStagger",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FWaveSpawnerCookTimeNonBarrageStagger::RunTest(const FString& Parameters)
{
    FPatternPool Mid  = MakeValidMidPool();
    FPatternPool Peak = MakeValidPeakPool();

    // Arrange (fail): OPENER non-barrage pattern with onset gap = 0.50s < 0.70s.
    FPatternPool OpenerBadStagger = MakeValidOpenerPool();
    OpenerBadStagger.NonBarragePatterns[0].OnsetTimes = { 0.0f, 0.50f };  // gap = 0.50s

    TArray<FString> Errors = FWaveSpawnerCookTimeValidator::Validate(
        OpenerBadStagger, Mid, Peak, kValidBaseW, kValidWCeiling);

    TestTrue("NON_BARRAGE_STAGGER error when consecutive onsets are < 0.70s apart",
             ErrorsContain(Errors, TEXT("NON_BARRAGE_STAGGER")));

    // Negative: single onset (no consecutive pair) passes.
    TArray<FString> CleanErrors = FWaveSpawnerCookTimeValidator::Validate(
        MakeValidOpenerPool(), Mid, Peak, kValidBaseW, kValidWCeiling);
    TestFalse("No NON_BARRAGE_STAGGER error when onsets are adequately staggered (single onset)",
              ErrorsContain(CleanErrors, TEXT("NON_BARRAGE_STAGGER")));

    return true;
}

// ===========================================================================
// TC7: PRIMER_PATTERN (AC-WS-07)
//   Fail: OPENER pool has no primer-eligible non-barrage pattern.
// ===========================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWaveSpawnerCookTimePrimerPattern,
    "SLIPSTORM.WaveSpawner.CookTime.PrimerPattern",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FWaveSpawnerCookTimePrimerPattern::RunTest(const FString& Parameters)
{
    FPatternPool Mid  = MakeValidMidPool();
    FPatternPool Peak = MakeValidPeakPool();

    // Arrange (fail): OPENER pool with bIsPrimerEligible=false.
    FPatternPool OpenerNoPrimer;
    OpenerNoPrimer.NonBarragePatterns.Add(MakeNonBarragePat(/*bPrimer=*/false, /*bSlip=*/true));

    TArray<FString> Errors = FWaveSpawnerCookTimeValidator::Validate(
        OpenerNoPrimer, Mid, Peak, kValidBaseW, kValidWCeiling);

    TestTrue("PRIMER_PATTERN error when OPENER pool has no primer-eligible pattern",
             ErrorsContain(Errors, TEXT("PRIMER_PATTERN")));

    // Negative
    TArray<FString> CleanErrors = FWaveSpawnerCookTimeValidator::Validate(
        MakeValidOpenerPool(), Mid, Peak, kValidBaseW, kValidWCeiling);
    TestFalse("No PRIMER_PATTERN error when OPENER has a primer-eligible pattern",
              ErrorsContain(CleanErrors, TEXT("PRIMER_PATTERN")));

    return true;
}

// ===========================================================================
// TC8: PILLAR_1_VERB_SLIP (AC-WS-08)
//   Fail: MID pool has patterns but none has bAllowsSlip=true.
// ===========================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWaveSpawnerCookTimePillar1VerbSlip,
    "SLIPSTORM.WaveSpawner.CookTime.Pillar1VerbSlip",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FWaveSpawnerCookTimePillar1VerbSlip::RunTest(const FString& Parameters)
{
    FPatternPool Opener = MakeValidOpenerPool();
    FPatternPool Peak   = MakeValidPeakPool();

    // Arrange (fail): MID pool with one non-slip pattern.
    FPatternPool MidNoSlip;
    MidNoSlip.NonBarragePatterns.Add(MakeNonBarragePat(false, /*bSlip=*/false));

    TArray<FString> Errors = FWaveSpawnerCookTimeValidator::Validate(
        Opener, MidNoSlip, Peak, kValidBaseW, kValidWCeiling);

    TestTrue("PILLAR_1_VERB_SLIP error when MID pool has no slip-capable pattern",
             ErrorsContain(Errors, TEXT("PILLAR_1_VERB_SLIP")));

    // Negative
    TArray<FString> CleanErrors = FWaveSpawnerCookTimeValidator::Validate(
        Opener, MakeValidMidPool(), Peak, kValidBaseW, kValidWCeiling);
    TestFalse("No PILLAR_1_VERB_SLIP error when every pool has at least one slip-capable pattern",
              ErrorsContain(CleanErrors, TEXT("PILLAR_1_VERB_SLIP")));

    return true;
}

// ===========================================================================
// TC9: POOL_NON_EMPTY (AC-WS-09)
//   Fail: OPENER pool has zero patterns.
// ===========================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWaveSpawnerCookTimePoolNonEmpty,
    "SLIPSTORM.WaveSpawner.CookTime.PoolNonEmpty",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FWaveSpawnerCookTimePoolNonEmpty::RunTest(const FString& Parameters)
{
    FPatternPool Mid  = MakeValidMidPool();
    FPatternPool Peak = MakeValidPeakPool();

    // Arrange (fail): empty OPENER pool.
    FPatternPool EmptyOpener;

    TArray<FString> Errors = FWaveSpawnerCookTimeValidator::Validate(
        EmptyOpener, Mid, Peak, kValidBaseW, kValidWCeiling);

    TestTrue("POOL_NON_EMPTY error returned for empty OPENER pool",
             ErrorsContain(Errors, TEXT("POOL_NON_EMPTY")));

    // Negative: all three pools non-empty.
    TArray<FString> CleanErrors = FWaveSpawnerCookTimeValidator::Validate(
        MakeValidOpenerPool(), Mid, Peak, kValidBaseW, kValidWCeiling);
    TestFalse("No POOL_NON_EMPTY error when all three pools have at least one pattern",
              ErrorsContain(CleanErrors, TEXT("POOL_NON_EMPTY")));

    return true;
}

// ===========================================================================
// TC10: PEAK_BASE_W_RANGE (AC-WS-32)
//   Fail A: BaseW = 0.10, below the minimum 0.15.
//   Fail B: BaseW = 0.45, above the maximum 0.40.
//   Pass:   BaseW = 0.30, within range.
// ===========================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWaveSpawnerCookTimePeakBaseWRange,
    "SLIPSTORM.WaveSpawner.CookTime.PeakBaseWRange",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FWaveSpawnerCookTimePeakBaseWRange::RunTest(const FString& Parameters)
{
    FPatternPool Opener = MakeValidOpenerPool();
    FPatternPool Mid    = MakeValidMidPool();
    FPatternPool Peak   = MakeValidPeakPool();

    // Fail A: BaseW below minimum.
    TArray<FString> ErrorsLow = FWaveSpawnerCookTimeValidator::Validate(Opener, Mid, Peak, 0.10f, kValidWCeiling);
    TestTrue("PEAK_BASE_W_RANGE error when BaseW=0.10 (below minimum 0.15)",
             ErrorsContain(ErrorsLow, TEXT("PEAK_BASE_W_RANGE")));

    // Fail B: BaseW above maximum.
    TArray<FString> ErrorsHigh = FWaveSpawnerCookTimeValidator::Validate(Opener, Mid, Peak, 0.45f, kValidWCeiling);
    TestTrue("PEAK_BASE_W_RANGE error when BaseW=0.45 (above maximum 0.40)",
             ErrorsContain(ErrorsHigh, TEXT("PEAK_BASE_W_RANGE")));

    // Pass: BaseW = 0.30 is in [0.15, 0.40].
    TArray<FString> PassErrors = FWaveSpawnerCookTimeValidator::Validate(Opener, Mid, Peak, 0.30f, kValidWCeiling);
    TestFalse("No PEAK_BASE_W_RANGE error when BaseW=0.30 (within [0.15, 0.40])",
              ErrorsContain(PassErrors, TEXT("PEAK_BASE_W_RANGE")));

    return true;
}

// ===========================================================================
// TC11: PEAK_BASE_W_BELOW_CEILING (AC-WS-33)
//   Fail: BaseW == WCeiling (must be strictly below).
//   Pass: BaseW = 0.25, WCeiling = 0.60.
// ===========================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWaveSpawnerCookTimePeakBaseWBelowCeiling,
    "SLIPSTORM.WaveSpawner.CookTime.PeakBaseWBelowCeiling",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FWaveSpawnerCookTimePeakBaseWBelowCeiling::RunTest(const FString& Parameters)
{
    FPatternPool Opener = MakeValidOpenerPool();
    FPatternPool Mid    = MakeValidMidPool();
    FPatternPool Peak   = MakeValidPeakPool();

    // Fail: BaseW = WCeiling (equality violates strict inequality).
    constexpr float Ceiling = 0.40f;
    TArray<FString> Errors = FWaveSpawnerCookTimeValidator::Validate(Opener, Mid, Peak, Ceiling, Ceiling);
    TestTrue("PEAK_BASE_W_BELOW_CEILING error when BaseW equals WCeiling",
             ErrorsContain(Errors, TEXT("PEAK_BASE_W_BELOW_CEILING")));

    // Pass: BaseW < WCeiling.
    TArray<FString> PassErrors = FWaveSpawnerCookTimeValidator::Validate(Opener, Mid, Peak, kValidBaseW, kValidWCeiling);
    TestFalse("No PEAK_BASE_W_BELOW_CEILING error when BaseW < WCeiling",
              ErrorsContain(PassErrors, TEXT("PEAK_BASE_W_BELOW_CEILING")));

    return true;
}

// ===========================================================================
// TC12: MIN_BARRAGE_PATTERN_COUNT_PER_TRIPLET (AC-WS-34)
//   Fail: one of the 7 target triplets has zero patterns in the PEAK barrage pool.
// ===========================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWaveSpawnerCookTimeMinBarragePatternCountPerTriplet,
    "SLIPSTORM.WaveSpawner.CookTime.MinBarragePatternCountPerTriplet",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FWaveSpawnerCookTimeMinBarragePatternCountPerTriplet::RunTest(const FString& Parameters)
{
    FPatternPool Opener = MakeValidOpenerPool();
    FPatternPool Mid    = MakeValidMidPool();

    // Arrange (fail): 6-pattern pool — remove the {1,3,4} triplet pattern.
    // This means triplet {1,3,4} has 0 patterns, which should trigger this check.
    FPatternPool PeakMissing = MakeValidPeakPool();
    PeakMissing.BarragePatterns.RemoveAt(PeakMissing.BarragePatterns.Num() - 1);

    TArray<FString> Errors = FWaveSpawnerCookTimeValidator::Validate(
        Opener, Mid, PeakMissing, kValidBaseW, kValidWCeiling);

    TestTrue("MIN_BARRAGE_PATTERN_COUNT_PER_TRIPLET error when a triplet has zero patterns",
             ErrorsContain(Errors, TEXT("MIN_BARRAGE_PATTERN_COUNT_PER_TRIPLET")));

    // Negative: all 7 triplets present (each with 1 pattern).
    TArray<FString> CleanErrors = FWaveSpawnerCookTimeValidator::Validate(
        Opener, Mid, MakeValidPeakPool(), kValidBaseW, kValidWCeiling);
    TestFalse("No MIN_BARRAGE_PATTERN_COUNT_PER_TRIPLET error when all 7 triplets have >= 1 pattern",
              ErrorsContain(CleanErrors, TEXT("MIN_BARRAGE_PATTERN_COUNT_PER_TRIPLET")));

    return true;
}

// ===========================================================================
// TC13: BARRAGE_UNIFORM_TIER (AC-WS-35)
//   Fail: PEAK barrage pool has patterns with different tier values (tier 2 + tier 3).
// ===========================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWaveSpawnerCookTimeBarrageUniformTier,
    "SLIPSTORM.WaveSpawner.CookTime.BarrageUniformTier",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FWaveSpawnerCookTimeBarrageUniformTier::RunTest(const FString& Parameters)
{
    FPatternPool Opener = MakeValidOpenerPool();
    FPatternPool Mid    = MakeValidMidPool();

    // Arrange (fail): first pattern tier=2, second pattern tier=3.
    FPatternPool PeakMixedTier = MakeValidPeakPool();
    PeakMixedTier.BarragePatterns[1].LeanMagnitudeTier = 3;  // violates uniform tier

    TArray<FString> Errors = FWaveSpawnerCookTimeValidator::Validate(
        Opener, Mid, PeakMixedTier, kValidBaseW, kValidWCeiling);

    TestTrue("BARRAGE_UNIFORM_TIER error when PEAK barrage pool has mixed tier values",
             ErrorsContain(Errors, TEXT("BARRAGE_UNIFORM_TIER")));

    // Negative: all patterns share tier=2.
    TArray<FString> CleanErrors = FWaveSpawnerCookTimeValidator::Validate(
        Opener, Mid, MakeValidPeakPool(), kValidBaseW, kValidWCeiling);
    TestFalse("No BARRAGE_UNIFORM_TIER error when all PEAK barrage patterns share the same tier",
              ErrorsContain(CleanErrors, TEXT("BARRAGE_UNIFORM_TIER")));

    return true;
}

// ===========================================================================
// TC14: BARRAGE_DISTINCT_SOURCE_LANES (AC-WS-36)
//   Fail: two PEAK barrage patterns share the same source lane set (duplicate triplet).
//   The pool still covers all 7 distinct triplets so PEAK_SURVIVING_TRIPLETS passes,
//   isolating this check specifically.
// ===========================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWaveSpawnerCookTimeBarrageDistinctSourceLanes,
    "SLIPSTORM.WaveSpawner.CookTime.BarrageDistinctSourceLanes",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FWaveSpawnerCookTimeBarrageDistinctSourceLanes::RunTest(const FString& Parameters)
{
    FPatternPool Opener = MakeValidOpenerPool();
    FPatternPool Mid    = MakeValidMidPool();

    // Arrange (fail): valid 7-triplet pool PLUS one extra pattern with {0,1,3} (a duplicate).
    // The 7 distinct triplets are still all covered, so PEAK_SURVIVING_TRIPLETS passes.
    FPatternPool PeakDuplicate = MakeValidPeakPool();
    PeakDuplicate.BarragePatterns.Add(MakeBarragePat(0, 1, 3));  // duplicate of pattern[0]

    TArray<FString> Errors = FWaveSpawnerCookTimeValidator::Validate(
        Opener, Mid, PeakDuplicate, kValidBaseW, kValidWCeiling);

    TestTrue("BARRAGE_DISTINCT_SOURCE_LANES error when two PEAK barrage patterns share a source lane set",
             ErrorsContain(Errors, TEXT("BARRAGE_DISTINCT_SOURCE_LANES")));

    TestFalse("PEAK_SURVIVING_TRIPLETS not fired — 7 distinct triplets are still covered",
              ErrorsContain(Errors, TEXT("PEAK_SURVIVING_TRIPLETS")));

    // Negative: 7 distinct triplets (1 each).
    TArray<FString> CleanErrors = FWaveSpawnerCookTimeValidator::Validate(
        Opener, Mid, MakeValidPeakPool(), kValidBaseW, kValidWCeiling);
    TestFalse("No BARRAGE_DISTINCT_SOURCE_LANES error when all PEAK barrage patterns have distinct source lane sets",
              ErrorsContain(CleanErrors, TEXT("BARRAGE_DISTINCT_SOURCE_LANES")));

    return true;
}

// ===========================================================================
// TC15: AllChecksPass
//   Verifies that the minimal valid pool set (as constructed by helper functions)
//   produces ZERO errors — confirming the validator accepts a correctly-authored pool.
// ===========================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWaveSpawnerCookTimeAllChecksPass,
    "SLIPSTORM.WaveSpawner.CookTime.AllChecksPass",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FWaveSpawnerCookTimeAllChecksPass::RunTest(const FString& Parameters)
{
    // Arrange: minimal valid pool set.
    FPatternPool Opener = MakeValidOpenerPool();
    FPatternPool Mid    = MakeValidMidPool();
    FPatternPool Peak   = MakeValidPeakPool();

    // Act
    TArray<FString> Errors = FWaveSpawnerCookTimeValidator::Validate(
        Opener, Mid, Peak, kValidBaseW, kValidWCeiling);

    // Assert: no errors at all.
    if (Errors.Num() > 0)
    {
        for (const FString& E : Errors)
        {
            AddError(FString::Printf(TEXT("Unexpected validation error: %s"), *E));
        }
    }
    TestEqual("All 14 Rule 15 checks pass for the minimal valid pool set", Errors.Num(), 0);

    return true;
}

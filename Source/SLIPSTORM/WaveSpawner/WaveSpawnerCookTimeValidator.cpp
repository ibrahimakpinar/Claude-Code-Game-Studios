// Copyright (c) Game Studio. All rights reserved.
// SLIPSTORM — 60-second voxel endless runner, mobile, Unreal Engine 5.7
//
// WaveSpawnerCookTimeValidator.cpp — Implementation of all 14 Rule 15 binding
// cook-time checks for the Wave Spawner pattern pool library (ADR-0011 D4).
//
// Each check function appends to OutErrors and never clears it, so callers
// always see the full accumulated error list after Validate() returns.
//
// Story: production/epics/wave-spawner/story-008-cook-time-validator.md
// ADR:   docs/architecture/adr-0011-wave-spawner-pattern-library.md (D4)

#include "WaveSpawner/WaveSpawnerCookTimeValidator.h"

// ---------------------------------------------------------------------------
// Static data: the 7 valid PEAK barrage source-lane triplets (Rule 4, AC-WS-03).
// Each inner array is in sorted ascending order.
// {0,1,3},{0,1,4},{0,2,3},{0,2,4},{0,3,4},{1,2,4},{1,3,4}
// ---------------------------------------------------------------------------
const TArray<TArray<int32>> FWaveSpawnerCookTimeValidator::ValidPeakTriplets =
{
    { 0, 1, 3 },
    { 0, 1, 4 },
    { 0, 2, 3 },
    { 0, 2, 4 },
    { 0, 3, 4 },
    { 1, 2, 4 },
    { 1, 3, 4 },
};

// ---------------------------------------------------------------------------
// Public entry point
// ---------------------------------------------------------------------------

TArray<FString> FWaveSpawnerCookTimeValidator::Validate(
    const FPatternPool& OpenerPool,
    const FPatternPool& MidPool,
    const FPatternPool& PeakPool,
    float BaseW,
    float WCeiling)
{
    TArray<FString> Errors;

    // Run all 14 Rule 15 binding checks in the order they appear in ADR-0011 D4.
    // Each check appends to Errors; earlier failures do NOT short-circuit later ones.

    CheckOpenerNoBarrage(OpenerPool, Errors);                              // AC-WS-01
    CheckMidNoBarrage(MidPool, Errors);                                    // AC-WS-02
    CheckPeakSurvivingTriplets(PeakPool, Errors);                          // AC-WS-03
    CheckPeakBarrageMinTier(PeakPool, Errors);                             // AC-WS-04
    CheckBarrageWSpan(PeakPool, Errors);                                   // AC-WS-05
    CheckNonBarrageStagger(OpenerPool, MidPool, PeakPool, Errors);         // AC-WS-06
    CheckPrimerPattern(OpenerPool, Errors);                                // AC-WS-07
    CheckPillar1VerbSlip(OpenerPool, MidPool, PeakPool, Errors);           // AC-WS-08
    CheckPoolNonEmpty(OpenerPool, MidPool, PeakPool, Errors);              // AC-WS-09
    CheckPeakBaseWRange(BaseW, Errors);                                    // AC-WS-32
    CheckPeakBaseWBelowCeiling(BaseW, WCeiling, Errors);                  // AC-WS-33
    CheckMinBarragePatternCountPerTriplet(PeakPool, Errors);               // AC-WS-34
    CheckBarrageUniformTier(PeakPool, Errors);                             // AC-WS-35
    CheckBarrageDistinctSourceLanes(PeakPool, Errors);                     // AC-WS-36

    return Errors;
}

// ---------------------------------------------------------------------------
// AC-WS-01: OPENER_NO_BARRAGE
// ---------------------------------------------------------------------------

void FWaveSpawnerCookTimeValidator::CheckOpenerNoBarrage(
    const FPatternPool& OpenerPool,
    TArray<FString>& OutErrors)
{
    if (OpenerPool.BarragePatterns.Num() > 0)
    {
        OutErrors.Add(FString::Printf(
            TEXT("OPENER_NO_BARRAGE: OPENER pool contains %d barrage pattern(s); must be 0"
                 " (AC-WS-01, Rule 15, ADR-0011 D4)"),
            OpenerPool.BarragePatterns.Num()));
    }
}

// ---------------------------------------------------------------------------
// AC-WS-02: MID_NO_BARRAGE
// ---------------------------------------------------------------------------

void FWaveSpawnerCookTimeValidator::CheckMidNoBarrage(
    const FPatternPool& MidPool,
    TArray<FString>& OutErrors)
{
    if (MidPool.BarragePatterns.Num() > 0)
    {
        OutErrors.Add(FString::Printf(
            TEXT("MID_NO_BARRAGE: MID pool contains %d barrage pattern(s); must be 0"
                 " (AC-WS-02, Rule 15, ADR-0011 D4)"),
            MidPool.BarragePatterns.Num()));
    }
}

// ---------------------------------------------------------------------------
// AC-WS-03: PEAK_SURVIVING_TRIPLETS
// ---------------------------------------------------------------------------

void FWaveSpawnerCookTimeValidator::CheckPeakSurvivingTriplets(
    const FPatternPool& PeakPool,
    TArray<FString>& OutErrors)
{
    // Build the set of distinct (sorted) triplets present in the barrage sub-pool.
    TArray<TArray<int32>> Present;
    for (const FPatternDefinition& Pat : PeakPool.BarragePatterns)
    {
        TArray<int32> Norm = NormalizeTriplet(Pat.SourceLanes);
        bool bAlreadyPresent = false;
        for (const TArray<int32>& Existing : Present)
        {
            if (TripletsEqual(Existing, Norm))
            {
                bAlreadyPresent = true;
                break;
            }
        }
        if (!bAlreadyPresent)
        {
            Present.Add(Norm);
        }
    }

    // Flag any required triplet that is absent from the sub-pool.
    for (const TArray<int32>& Required : ValidPeakTriplets)
    {
        bool bFound = false;
        for (const TArray<int32>& P : Present)
        {
            if (TripletsEqual(Required, P))
            {
                bFound = true;
                break;
            }
        }
        if (!bFound)
        {
            OutErrors.Add(FString::Printf(
                TEXT("PEAK_SURVIVING_TRIPLETS: required triplet %s is absent from the PEAK"
                     " barrage sub-pool (AC-WS-03, Rule 15, ADR-0011 D4)"),
                *TripletToString(Required)));
        }
    }

    // Flag any triplet present in the sub-pool that is NOT in the valid 7.
    for (const TArray<int32>& P : Present)
    {
        if (!IsValidPeakTriplet(P))
        {
            OutErrors.Add(FString::Printf(
                TEXT("PEAK_SURVIVING_TRIPLETS: extra triplet %s is not one of the 7 valid"
                     " PEAK barrage triplets (AC-WS-03, Rule 15, ADR-0011 D4)"),
                *TripletToString(P)));
        }
    }
}

// ---------------------------------------------------------------------------
// AC-WS-04: PEAK_BARRAGE_MIN_TIER
// ---------------------------------------------------------------------------

void FWaveSpawnerCookTimeValidator::CheckPeakBarrageMinTier(
    const FPatternPool& PeakPool,
    TArray<FString>& OutErrors)
{
    for (const FPatternDefinition& Pat : PeakPool.BarragePatterns)
    {
        if (Pat.LeanMagnitudeTier < 2)
        {
            OutErrors.Add(FString::Printf(
                TEXT("PEAK_BARRAGE_MIN_TIER: barrage pattern '%s' has LeanMagnitudeTier=%d;"
                     " PEAK barrage patterns require tier >= 2 (AC-WS-04, Rule 15, ADR-0011 D4)"),
                *Pat.PatternId.ToString(), Pat.LeanMagnitudeTier));
        }
    }
}

// ---------------------------------------------------------------------------
// AC-WS-05: BARRAGE_W_SPAN
// ---------------------------------------------------------------------------

void FWaveSpawnerCookTimeValidator::CheckBarrageWSpan(
    const FPatternPool& PeakPool,
    TArray<FString>& OutErrors)
{
    for (const FPatternDefinition& Pat : PeakPool.BarragePatterns)
    {
        if (Pat.OnsetTimes.Num() < 2)
        {
            // Single-onset barrage: trivially within any span limit.
            continue;
        }

        float MinT = Pat.OnsetTimes[0];
        float MaxT = Pat.OnsetTimes[0];
        for (float T : Pat.OnsetTimes)
        {
            MinT = FMath::Min(MinT, T);
            MaxT = FMath::Max(MaxT, T);
        }

        const float Span = MaxT - MinT;
        if (Span > kBarrageWSpanMaxS)
        {
            OutErrors.Add(FString::Printf(
                TEXT("BARRAGE_W_SPAN: barrage pattern '%s' onset span=%.3fs exceeds"
                     " max=%.3fs (TELEGRAPH_WINDOW_FLOOR_S/2)"
                     " (AC-WS-05, Rule 15, ADR-0011 D4)"),
                *Pat.PatternId.ToString(), Span, kBarrageWSpanMaxS));
        }
    }
}

// ---------------------------------------------------------------------------
// AC-WS-06: NON_BARRAGE_STAGGER — checked across all three pools
// ---------------------------------------------------------------------------

namespace
{
    /** Check a single pool's non-barrage patterns for the stagger constraint. */
    void CheckNonBarrageStaggerInPool(
        const FPatternPool& Pool,
        const FString& PoolName,
        float FloorS,
        TArray<FString>& OutErrors)
    {
        for (const FPatternDefinition& Pat : Pool.NonBarragePatterns)
        {
            for (int32 i = 0; i < Pat.OnsetTimes.Num() - 1; ++i)
            {
                const float Delta = Pat.OnsetTimes[i + 1] - Pat.OnsetTimes[i];
                if (Delta < FloorS)
                {
                    OutErrors.Add(FString::Printf(
                        TEXT("NON_BARRAGE_STAGGER: %s pool pattern '%s'"
                             " onset[%d→%d] gap=%.3fs < min=%.3fs"
                             " (AC-WS-06, Rule 15, ADR-0011 D4)"),
                        *PoolName, *Pat.PatternId.ToString(),
                        i, i + 1, Delta, FloorS));
                }
            }
        }
    }
}  // namespace

void FWaveSpawnerCookTimeValidator::CheckNonBarrageStagger(
    const FPatternPool& OpenerPool,
    const FPatternPool& MidPool,
    const FPatternPool& PeakPool,
    TArray<FString>& OutErrors)
{
    CheckNonBarrageStaggerInPool(OpenerPool, TEXT("OPENER"), kTelegraphWindowFloorS, OutErrors);
    CheckNonBarrageStaggerInPool(MidPool,    TEXT("MID"),    kTelegraphWindowFloorS, OutErrors);
    CheckNonBarrageStaggerInPool(PeakPool,   TEXT("PEAK"),   kTelegraphWindowFloorS, OutErrors);
}

// ---------------------------------------------------------------------------
// AC-WS-07: PRIMER_PATTERN
// ---------------------------------------------------------------------------

void FWaveSpawnerCookTimeValidator::CheckPrimerPattern(
    const FPatternPool& OpenerPool,
    TArray<FString>& OutErrors)
{
    bool bFoundPrimer = false;
    for (const FPatternDefinition& Pat : OpenerPool.NonBarragePatterns)
    {
        if (Pat.bIsPrimerEligible)
        {
            bFoundPrimer = true;
            break;
        }
    }

    if (!bFoundPrimer)
    {
        OutErrors.Add(
            TEXT("PRIMER_PATTERN: OPENER pool has no primer-eligible non-barrage pattern"
                 " (AC-WS-07, Rule 15, ADR-0011 D4)"));
    }
}

// ---------------------------------------------------------------------------
// AC-WS-08: PILLAR_1_VERB_SLIP
// ---------------------------------------------------------------------------

bool FWaveSpawnerCookTimeValidator::IsPoolNonEmpty(const FPatternPool& Pool)
{
    return Pool.BarragePatterns.Num() > 0 || Pool.NonBarragePatterns.Num() > 0;
}

bool FWaveSpawnerCookTimeValidator::HasSlipPattern(const FPatternPool& Pool)
{
    for (const FPatternDefinition& P : Pool.BarragePatterns)
    {
        if (P.bAllowsSlip) return true;
    }
    for (const FPatternDefinition& P : Pool.NonBarragePatterns)
    {
        if (P.bAllowsSlip) return true;
    }
    return false;
}

void FWaveSpawnerCookTimeValidator::CheckPillar1VerbSlip(
    const FPatternPool& OpenerPool,
    const FPatternPool& MidPool,
    const FPatternPool& PeakPool,
    TArray<FString>& OutErrors)
{
    if (IsPoolNonEmpty(OpenerPool) && !HasSlipPattern(OpenerPool))
    {
        OutErrors.Add(
            TEXT("PILLAR_1_VERB_SLIP: OPENER pool has no pattern with bAllowsSlip=true"
                 " (AC-WS-08, Rule 15, ADR-0011 D4)"));
    }
    if (IsPoolNonEmpty(MidPool) && !HasSlipPattern(MidPool))
    {
        OutErrors.Add(
            TEXT("PILLAR_1_VERB_SLIP: MID pool has no pattern with bAllowsSlip=true"
                 " (AC-WS-08, Rule 15, ADR-0011 D4)"));
    }
    if (IsPoolNonEmpty(PeakPool) && !HasSlipPattern(PeakPool))
    {
        OutErrors.Add(
            TEXT("PILLAR_1_VERB_SLIP: PEAK pool has no pattern with bAllowsSlip=true"
                 " (AC-WS-08, Rule 15, ADR-0011 D4)"));
    }
}

// ---------------------------------------------------------------------------
// AC-WS-09: POOL_NON_EMPTY
// ---------------------------------------------------------------------------

void FWaveSpawnerCookTimeValidator::CheckPoolNonEmpty(
    const FPatternPool& OpenerPool,
    const FPatternPool& MidPool,
    const FPatternPool& PeakPool,
    TArray<FString>& OutErrors)
{
    if (!IsPoolNonEmpty(OpenerPool))
    {
        OutErrors.Add(
            TEXT("POOL_NON_EMPTY: OPENER pool has no patterns (barrage or non-barrage)"
                 " (AC-WS-09, Rule 15, ADR-0011 D4)"));
    }
    if (!IsPoolNonEmpty(MidPool))
    {
        OutErrors.Add(
            TEXT("POOL_NON_EMPTY: MID pool has no patterns (barrage or non-barrage)"
                 " (AC-WS-09, Rule 15, ADR-0011 D4)"));
    }
    if (!IsPoolNonEmpty(PeakPool))
    {
        OutErrors.Add(
            TEXT("POOL_NON_EMPTY: PEAK pool has no patterns (barrage or non-barrage)"
                 " (AC-WS-09, Rule 15, ADR-0011 D4)"));
    }
}

// ---------------------------------------------------------------------------
// AC-WS-32: PEAK_BASE_W_RANGE
// ---------------------------------------------------------------------------

void FWaveSpawnerCookTimeValidator::CheckPeakBaseWRange(float BaseW, TArray<FString>& OutErrors)
{
    if (BaseW < kPeakBaseWMin || BaseW > kPeakBaseWMax)
    {
        OutErrors.Add(FString::Printf(
            TEXT("PEAK_BASE_W_RANGE: BaseW=%.4f is outside valid range [%.2f, %.2f]"
                 " (AC-WS-32, Rule 15, ADR-0011 D4)"),
            BaseW, kPeakBaseWMin, kPeakBaseWMax));
    }
}

// ---------------------------------------------------------------------------
// AC-WS-33: PEAK_BASE_W_BELOW_CEILING
// ---------------------------------------------------------------------------

void FWaveSpawnerCookTimeValidator::CheckPeakBaseWBelowCeiling(
    float BaseW,
    float WCeiling,
    TArray<FString>& OutErrors)
{
    if (BaseW >= WCeiling)
    {
        OutErrors.Add(FString::Printf(
            TEXT("PEAK_BASE_W_BELOW_CEILING: BaseW=%.4f >= WCeiling=%.4f;"
                 " prevents F-3 proportional governor silent saturation"
                 " (AC-WS-33, Rule 15, ADR-0011 D4)"),
            BaseW, WCeiling));
    }
}

// ---------------------------------------------------------------------------
// AC-WS-34: MIN_BARRAGE_PATTERN_COUNT_PER_TRIPLET
// ---------------------------------------------------------------------------

void FWaveSpawnerCookTimeValidator::CheckMinBarragePatternCountPerTriplet(
    const FPatternPool& PeakPool,
    TArray<FString>& OutErrors)
{
    for (const TArray<int32>& Required : ValidPeakTriplets)
    {
        int32 Count = 0;
        for (const FPatternDefinition& Pat : PeakPool.BarragePatterns)
        {
            if (TripletsEqual(NormalizeTriplet(Pat.SourceLanes), Required))
            {
                ++Count;
            }
        }

        if (Count < 1)
        {
            OutErrors.Add(FString::Printf(
                TEXT("MIN_BARRAGE_PATTERN_COUNT_PER_TRIPLET: triplet %s has %d pattern(s);"
                     " requires >= 1 (AC-WS-34, Rule 15, ADR-0011 D4)"),
                *TripletToString(Required), Count));
        }
    }
}

// ---------------------------------------------------------------------------
// AC-WS-35: BARRAGE_UNIFORM_TIER
// ---------------------------------------------------------------------------

void FWaveSpawnerCookTimeValidator::CheckBarrageUniformTier(
    const FPatternPool& PeakPool,
    TArray<FString>& OutErrors)
{
    if (PeakPool.BarragePatterns.Num() < 2)
    {
        // 0 or 1 patterns: trivially uniform.
        return;
    }

    const int32 ExpectedTier = PeakPool.BarragePatterns[0].LeanMagnitudeTier;
    for (int32 i = 1; i < PeakPool.BarragePatterns.Num(); ++i)
    {
        const int32 ActualTier = PeakPool.BarragePatterns[i].LeanMagnitudeTier;
        if (ActualTier != ExpectedTier)
        {
            OutErrors.Add(FString::Printf(
                TEXT("BARRAGE_UNIFORM_TIER: PEAK barrage pattern '%s' has tier=%d;"
                     " expected tier=%d (all PEAK barrage patterns must share the same tier)"
                     " (AC-WS-35, Rule 15, ADR-0011 D4)"),
                *PeakPool.BarragePatterns[i].PatternId.ToString(),
                ActualTier, ExpectedTier));
        }
    }
}

// ---------------------------------------------------------------------------
// AC-WS-36: BARRAGE_DISTINCT_SOURCE_LANES
// ---------------------------------------------------------------------------

void FWaveSpawnerCookTimeValidator::CheckBarrageDistinctSourceLanes(
    const FPatternPool& PeakPool,
    TArray<FString>& OutErrors)
{
    const int32 N = PeakPool.BarragePatterns.Num();
    for (int32 i = 0; i < N; ++i)
    {
        const TArray<int32> NormI = NormalizeTriplet(PeakPool.BarragePatterns[i].SourceLanes);
        for (int32 j = i + 1; j < N; ++j)
        {
            const TArray<int32> NormJ = NormalizeTriplet(PeakPool.BarragePatterns[j].SourceLanes);
            if (TripletsEqual(NormI, NormJ))
            {
                OutErrors.Add(FString::Printf(
                    TEXT("BARRAGE_DISTINCT_SOURCE_LANES: patterns '%s' and '%s' share"
                         " the same source lane set %s; each triplet must appear at most once"
                         " (AC-WS-36, Rule 15, ADR-0011 D4)"),
                    *PeakPool.BarragePatterns[i].PatternId.ToString(),
                    *PeakPool.BarragePatterns[j].PatternId.ToString(),
                    *TripletToString(NormI)));
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Utility helpers
// ---------------------------------------------------------------------------

TArray<int32> FWaveSpawnerCookTimeValidator::NormalizeTriplet(const TArray<int32>& Lanes)
{
    TArray<int32> Sorted = Lanes;
    Sorted.Sort();
    return Sorted;
}

FString FWaveSpawnerCookTimeValidator::TripletToString(const TArray<int32>& SortedLanes)
{
    if (SortedLanes.Num() == 0)
    {
        return TEXT("{}");
    }

    FString Result = TEXT("{");
    for (int32 i = 0; i < SortedLanes.Num(); ++i)
    {
        if (i > 0) Result += TEXT(",");
        Result += FString::FromInt(SortedLanes[i]);
    }
    Result += TEXT("}");
    return Result;
}

bool FWaveSpawnerCookTimeValidator::TripletsEqual(const TArray<int32>& A, const TArray<int32>& B)
{
    if (A.Num() != B.Num()) return false;
    for (int32 i = 0; i < A.Num(); ++i)
    {
        if (A[i] != B[i]) return false;
    }
    return true;
}

bool FWaveSpawnerCookTimeValidator::IsValidPeakTriplet(const TArray<int32>& SortedLanes)
{
    for (const TArray<int32>& Valid : ValidPeakTriplets)
    {
        if (TripletsEqual(Valid, SortedLanes)) return true;
    }
    return false;
}

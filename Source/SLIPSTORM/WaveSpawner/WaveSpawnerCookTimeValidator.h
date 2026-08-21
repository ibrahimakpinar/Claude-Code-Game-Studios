// Copyright (c) Game Studio. All rights reserved.
// SLIPSTORM — 60-second voxel endless runner, mobile, Unreal Engine 5.7
//
// WaveSpawnerCookTimeValidator.h — Cook-time validation of the Wave Spawner pattern
// pool data against the 14 binding Rule 15 checks (ADR-0011 D4).
//
// USAGE
//   Call Validate() after loading pool data assets (e.g. at OnFirstWorldLoaded,
//   after populating OpenerPool/MidPool/PeakPool from data assets).
//
//   TArray<FString> Errors = FWaveSpawnerCookTimeValidator::Validate(
//       OpenerPool, MidPool, PeakPool, BaseW, WCeiling);
//   // Non-Shipping: immediate authoring error catch
//   check(Errors.Num() == 0);
//   // Shipping: log critical errors and skip invalid patterns
//
// CONTRACT
//   - Returns empty array if all 14 checks pass.
//   - Each error string begins with its check ID (e.g. "OPENER_NO_BARRAGE: ...").
//   - MUST NOT mutate pool data — all arguments are const refs.
//   - A pool set with any Rule 15 violation MUST NOT reach runtime.
//
// Story: production/epics/wave-spawner/story-008-cook-time-validator.md
// ADR:   docs/architecture/adr-0011-wave-spawner-pattern-library.md (D4)
// TRs:   TR-WS-001, TR-WS-002, TR-WS-003, TR-WS-004, TR-WS-005,
//        TR-WS-006, TR-WS-007, TR-WS-031, TR-WS-032

#pragma once

#include "CoreMinimal.h"
#include "WaveSpawnerTypes.h"

/**
 * Static cook-time validator for the Wave Spawner pattern pool library.
 * Implements all 14 binding Rule 15 checks from ADR-0011 D4.
 *
 * This is a plain C++ utility class (not a UObject). The SLIPSTORM_API export
 * macro is provided for module-boundary linking only. All methods are static.
 * Named with the F prefix per UE convention for non-UObject plain C++ types.
 */
class SLIPSTORM_API FWaveSpawnerCookTimeValidator
{
public:
    /**
     * Run all 14 Rule 15 binding checks (ADR-0011 D4) on the provided pools and knob values.
     * Returns a list of error strings; empty array means all checks passed.
     * MUST NOT mutate pool data.
     *
     * @param OpenerPool  OPENER phase pool — must contain no barrage patterns (AC-WS-01)
     * @param MidPool     MID phase pool   — must contain no barrage patterns (AC-WS-02)
     * @param PeakPool    PEAK phase pool  — partitioned into barrage + non-barrage sub-pools
     * @param BaseW       Unweighted barrage fraction tuning knob; must be in [0.15, 0.40] (AC-WS-32)
     * @param WCeiling    Cadence governor ceiling; BaseW must be strictly below this value (AC-WS-33)
     * @return            Array of error strings, each beginning with its Rule 15 check ID.
     *                    Empty = all 14 checks passed.
     */
    static TArray<FString> Validate(
        const FPatternPool& OpenerPool,
        const FPatternPool& MidPool,
        const FPatternPool& PeakPool,
        float BaseW,
        float WCeiling);

    /**
     * The 7 valid PEAK barrage source-lane triplets per Rule 4.
     * Each inner array is in sorted ascending order.
     * {0,1,3},{0,1,4},{0,2,3},{0,2,4},{0,3,4},{1,2,4},{1,3,4}
     */
    static const TArray<TArray<int32>> ValidPeakTriplets;

private:
    // -------------------------------------------------------------------
    // Timing constants (Rule 15 / ADR-0011 D4)
    // -------------------------------------------------------------------

    /** Minimum gap between consecutive non-barrage onsets (AC-WS-06). */
    static constexpr float kTelegraphWindowFloorS = 0.70f;

    /** Maximum spread of barrage onset cluster = kTelegraphWindowFloorS / 2 (AC-WS-05). */
    static constexpr float kBarrageWSpanMaxS = kTelegraphWindowFloorS / 2.0f;  // 0.35s

    // -------------------------------------------------------------------
    // BaseW range constants (Rule 15 / AC-WS-32)
    // -------------------------------------------------------------------
    static constexpr float kPeakBaseWMin = 0.15f;
    static constexpr float kPeakBaseWMax = 0.40f;

    // -------------------------------------------------------------------
    // Per-check helper implementations
    // Each appends error strings to OutErrors and never clears it.
    // Error strings always begin with the Rule 15 check ID.
    // -------------------------------------------------------------------

    /** AC-WS-01: OPENER pool must contain no barrage patterns. */
    static void CheckOpenerNoBarrage(const FPatternPool& OpenerPool, TArray<FString>& OutErrors);

    /** AC-WS-02: MID pool must contain no barrage patterns. */
    static void CheckMidNoBarrage(const FPatternPool& MidPool, TArray<FString>& OutErrors);

    /**
     * AC-WS-03: The set of distinct (sorted) source-lane triplets in PeakPool.BarragePatterns
     * must equal exactly {0,1,3},{0,1,4},{0,2,3},{0,2,4},{0,3,4},{1,2,4},{1,3,4}.
     * Missing triplets and extra triplets each produce a separate error entry.
     */
    static void CheckPeakSurvivingTriplets(const FPatternPool& PeakPool, TArray<FString>& OutErrors);

    /** AC-WS-04: Every pattern in PeakPool.BarragePatterns has LeanMagnitudeTier >= 2. */
    static void CheckPeakBarrageMinTier(const FPatternPool& PeakPool, TArray<FString>& OutErrors);

    /**
     * AC-WS-05: Every barrage pattern in PeakPool clusters all OnsetTimes within
     * kBarrageWSpanMaxS = 0.35s (max(OnsetTimes) - min(OnsetTimes) <= 0.35s).
     * Patterns with fewer than 2 onset times are skipped (trivially in range).
     */
    static void CheckBarrageWSpan(const FPatternPool& PeakPool, TArray<FString>& OutErrors);

    /**
     * AC-WS-06: Every non-barrage pattern across all three pools has consecutive OnsetTimes
     * at least kTelegraphWindowFloorS = 0.70s apart.
     * Patterns with fewer than 2 onset times are skipped (no consecutive pairs).
     */
    static void CheckNonBarrageStagger(
        const FPatternPool& OpenerPool,
        const FPatternPool& MidPool,
        const FPatternPool& PeakPool,
        TArray<FString>& OutErrors);

    /**
     * AC-WS-07: At least one pattern in OpenerPool.NonBarragePatterns has
     * bIsPrimerEligible = true.
     */
    static void CheckPrimerPattern(const FPatternPool& OpenerPool, TArray<FString>& OutErrors);

    /**
     * AC-WS-08: At least one pattern in each non-empty pool has bAllowsSlip = true.
     * Checked per-pool: OPENER, MID, PEAK (combined barrage + non-barrage sub-pools).
     * A pool is "non-empty" if it has at least one pattern of any kind.
     */
    static void CheckPillar1VerbSlip(
        const FPatternPool& OpenerPool,
        const FPatternPool& MidPool,
        const FPatternPool& PeakPool,
        TArray<FString>& OutErrors);

    /**
     * AC-WS-09: Each of the three pools (OPENER, MID, PEAK) has at least one pattern
     * (barrage or non-barrage).
     */
    static void CheckPoolNonEmpty(
        const FPatternPool& OpenerPool,
        const FPatternPool& MidPool,
        const FPatternPool& PeakPool,
        TArray<FString>& OutErrors);

    /** AC-WS-32: BaseW is in [kPeakBaseWMin, kPeakBaseWMax] = [0.15, 0.40]. */
    static void CheckPeakBaseWRange(float BaseW, TArray<FString>& OutErrors);

    /**
     * AC-WS-33: BaseW < WCeiling.
     * Prevents the F-3 proportional governor from silently saturating at the ceiling.
     */
    static void CheckPeakBaseWBelowCeiling(float BaseW, float WCeiling, TArray<FString>& OutErrors);

    /**
     * AC-WS-34: Each of the 7 valid triplets has at least one pattern in
     * PeakPool.BarragePatterns.
     *
     * Note: PEAK_SURVIVING_TRIPLETS (AC-WS-03) checks that the DISTINCT triplet SET
     * equals exactly the 7 valid triplets. This check cares about per-triplet count
     * (>= 1 each). Both will fire when a triplet is absent; they are separate error IDs.
     */
    static void CheckMinBarragePatternCountPerTriplet(const FPatternPool& PeakPool, TArray<FString>& OutErrors);

    /**
     * AC-WS-35: All patterns in PeakPool.BarragePatterns share the same LeanMagnitudeTier.
     * Mixed-tier barrage sub-pools are forbidden.
     * Skipped if the barrage sub-pool has fewer than 2 patterns (trivially uniform).
     */
    static void CheckBarrageUniformTier(const FPatternPool& PeakPool, TArray<FString>& OutErrors);

    /**
     * AC-WS-36: No two patterns in PeakPool.BarragePatterns share the same sorted
     * source-lane set. Duplicate triplets in the PEAK barrage sub-pool are forbidden.
     */
    static void CheckBarrageDistinctSourceLanes(const FPatternPool& PeakPool, TArray<FString>& OutErrors);

    // -------------------------------------------------------------------
    // Utility helpers
    // -------------------------------------------------------------------

    /** Returns a copy of Lanes sorted ascending. Does not modify the input. */
    static TArray<int32> NormalizeTriplet(const TArray<int32>& Lanes);

    /** Returns a "{L0,L1,L2}" string from a sorted triplet for use in error messages. */
    static FString TripletToString(const TArray<int32>& SortedLanes);

    /** Returns true when two TArray<int32> triplets are element-wise equal. */
    static bool TripletsEqual(const TArray<int32>& A, const TArray<int32>& B);

    /** Returns true when SortedLanes matches any entry in ValidPeakTriplets. */
    static bool IsValidPeakTriplet(const TArray<int32>& SortedLanes);

    /** Returns true when Pool has at least one pattern (barrage or non-barrage). */
    static bool IsPoolNonEmpty(const FPatternPool& Pool);

    /** Returns true when Pool has at least one pattern with bAllowsSlip = true. */
    static bool HasSlipPattern(const FPatternPool& Pool);
};

// Copyright (c) Game Studio. All rights reserved.
// SLIPSTORM — 60-second voxel endless runner, mobile, Unreal Engine 5.7
//
// PMLateralInterpolationTest.cpp — Story 004 unit tests for F-3 lateral
// interpolation (pure math).
//
// Spec:  tests/unit/player-movement/pm-lateral-interpolation-spec.md
// GDD:   design/gdd/player-movement-mechanics.md §4 F-3, §8 AC-03
// ADR:   docs/architecture/adr-0009-player-movement-hosting.md (SD5, SD6, IG-5)
// Story: production/epics/player-movement/story-004-f3-lateral-interpolation.md
//
// Test category: SLIPSTORM.PlayerMovement.LateralInterpolation
// Runner: UE Automation Framework, headless (-nullrhi -nosound -unattended)
//         ClientContext | ProductFilter — matches Story 001a precedent.
//
// Pure-math tests — no UWorld or SpawnActor.  All helpers are exercised via
// NewObject<UPlayerLaneMovementComponent>() + direct field mutation through the
// FPMLateralInterpolationTest friend declaration in PlayerLaneMovementComponent.h.
//
// Guard: WITH_DEV_AUTOMATION_TESTS only.  No WITH_EDITOR — no editor APIs used.

#if WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Curves/CurveFloat.h"

#include "Player/PlayerLaneMovementComponent.h"
#include "Player/EPlayerLane.h"
#include "Player/ERunSlipState.h"

// ---------------------------------------------------------------------------
// IMPLEMENT_COMPLEX_AUTOMATION_TEST
// 6 test commands per Story 004 AC list (lateral_world_position_settled moved
// to composition test per Q3 decision).
// ---------------------------------------------------------------------------

IMPLEMENT_COMPLEX_AUTOMATION_TEST(
    FPMLateralInterpolationTest,
    "SLIPSTORM.PlayerMovement.LateralInterpolation",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

void FPMLateralInterpolationTest::GetTests(
    TArray<FString>& OutBeautifiedNames,
    TArray<FString>& OutTestCommands) const
{
    OutBeautifiedNames.Add(TEXT("F-3 Lerp — identity curve — Left→Center at TP=0.0/0.25/0.5/1.0"));
    OutTestCommands.Add(TEXT("f3_lerp_identity_curve"));

    OutBeautifiedNames.Add(TEXT("F-3 Lerp — authored curve (0.35 at TP=0.5) — Left→Center ≈ -65"));
    OutTestCommands.Add(TEXT("f3_lerp_authored_curve"));

    OutBeautifiedNames.Add(TEXT("F-3 TP=1.0 zero-relative invariant — all 20 non-self lane transitions"));
    OutTestCommands.Add(TEXT("f3_tp1_zero_relative_invariant"));

    OutBeautifiedNames.Add(TEXT("F-3 fallback — null SlipCurve + bCurveFallbackActive=true — linear"));
    OutTestCommands.Add(TEXT("f3_fallback_null_curve"));

    OutBeautifiedNames.Add(TEXT("F-3 fallback — valid curve but bCurveFallbackActive=true — linear"));
    OutTestCommands.Add(TEXT("f3_fallback_active_flag"));

    OutBeautifiedNames.Add(TEXT("F-3 rel_x SLIPPING — F3RelativeOffset(0.5) Left→Center == -50 (formula only; end-to-end covered by CC3)"));
    OutTestCommands.Add(TEXT("f3_rel_x_slipping_tp05"));
}

bool FPMLateralInterpolationTest::RunTest(const FString& Parameters)
{
    // -----------------------------------------------------------------------
    // TC1 (f3_lerp_identity_curve) — identity SlipCurve, Left→Center,
    // sampled at TweenProgress = 0.0 / 0.25 / 0.5 / 1.0.
    //
    // Setup:
    //   current_lane = Left  (X = -100)
    //   target_lane  = Center (X =   0)
    //   delta        = -100 - 0 = -100
    //   IdentityCurve: keys (0,0) and (1,1) — curve_t = TweenProgress.
    //
    // Expected (FMath::Lerp(-100, 0, t)):
    //   TP=0.0  → -100.0  (at source, no movement)
    //   TP=0.25 →  -75.0  (quarter of the way)
    //   TP=0.5  →  -50.0  (midpoint)
    //   TP=1.0  →    0.0  (at target; IG-5 zero-relative invariant)
    //
    // Tolerance: ±0.001f for interior values; exact equality for TP=1.0.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("f3_lerp_identity_curve"))
    {
        UPlayerLaneMovementComponent* PM = NewObject<UPlayerLaneMovementComponent>();
        TestNotNull(TEXT("TC1: NewObject succeeded"), PM);
        if (!PM) { return false; }

        // Build an identity UCurveFloat in-memory (outer = PM for lifetime safety).
        UCurveFloat* IdentityCurve = NewObject<UCurveFloat>(PM);
        IdentityCurve->FloatCurve.AddKey(0.0f, 0.0f);
        IdentityCurve->FloatCurve.AddKey(1.0f, 1.0f);

        PM->current_lane        = EPlayerLane::Left;
        PM->target_lane         = EPlayerLane::Center;
        PM->SlipCurve           = IdentityCurve;
        PM->bCurveFallbackActive = false;

        // TP = 0.0 — mesh still at source offset.
        TestTrue(
            TEXT("TC1: F3RelativeOffset(0.0) == -100.0f ± 0.001"),
            FMath::IsNearlyEqual(PM->F3RelativeOffset(0.0f), -100.0f, 0.001f));

        // TP = 0.25 — quarter of the way.
        TestTrue(
            TEXT("TC1: F3RelativeOffset(0.25) == -75.0f ± 0.001"),
            FMath::IsNearlyEqual(PM->F3RelativeOffset(0.25f), -75.0f, 0.001f));

        // TP = 0.5 — midpoint.
        TestTrue(
            TEXT("TC1: F3RelativeOffset(0.5) == -50.0f ± 0.001"),
            FMath::IsNearlyEqual(PM->F3RelativeOffset(0.5f), -50.0f, 0.001f));

        // TP = 1.0 — at target; IG-5 zero-relative invariant: exact 0.
        TestEqual(
            TEXT("TC1: F3RelativeOffset(1.0) == 0.0f exactly (IG-5 invariant)"),
            PM->F3RelativeOffset(1.0f),
            0.0f);

        return true;
    }

    // -----------------------------------------------------------------------
    // TC2 (f3_lerp_authored_curve) — authored curve returns 0.35 at TP=0.5.
    // Left→Center. Tests that the curve shaping is applied to curve_t, not
    // TweenProgress directly.
    //
    // Authored curve keys: (0, 0), (0.5, 0.35), (1, 1).
    // At TP=0.5: curve_t = 0.35 → FMath::Lerp(-100, 0, 0.35) = -65.0.
    //
    // Tolerance: ±1.0f (interpolation rounding from the curve asset's segment).
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("f3_lerp_authored_curve"))
    {
        UPlayerLaneMovementComponent* PM = NewObject<UPlayerLaneMovementComponent>();
        TestNotNull(TEXT("TC2: NewObject succeeded"), PM);
        if (!PM) { return false; }

        UCurveFloat* AuthoredCurve = NewObject<UCurveFloat>(PM);
        AuthoredCurve->FloatCurve.AddKey(0.0f,  0.0f);
        AuthoredCurve->FloatCurve.AddKey(0.5f,  0.35f);
        AuthoredCurve->FloatCurve.AddKey(1.0f,  1.0f);

        PM->current_lane         = EPlayerLane::Left;
        PM->target_lane          = EPlayerLane::Center;
        PM->SlipCurve            = AuthoredCurve;
        PM->bCurveFallbackActive = false;

        const float Result = PM->F3RelativeOffset(0.5f);
        TestTrue(
            TEXT("TC2: F3RelativeOffset(0.5) ≈ -65.0 ± 1.0 (authored curve shapes curve_t to 0.35)"),
            FMath::Abs(Result - (-65.0f)) <= 1.0f);

        return true;
    }

    // -----------------------------------------------------------------------
    // TC3 (f3_tp1_zero_relative_invariant) — for every non-self lane transition
    // among the 5 lanes (20 pairs), F3RelativeOffset(1.0f) must return exactly 0.
    //
    // Proof: at TP=1.0, FMath::Clamp(curve.GetFloatValue(1.0), 0, 1) = 1.0
    // (identity curve endpoint); FMath::Lerp(delta, 0.0, 1.0) = 0.0 exactly
    // by IEEE 754 FMA/multiply: result = delta*(1-1)+0*1 = 0*delta+0 = 0.
    //
    // Grep gate: F3RelativeOffset must contain no absolute-position clamp —
    // only FMath::Clamp on curve_t. See ADR-0009 IG-5.
    //
    // Uses identity curve for all 20 transitions.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("f3_tp1_zero_relative_invariant"))
    {
        UPlayerLaneMovementComponent* PM = NewObject<UPlayerLaneMovementComponent>();
        TestNotNull(TEXT("TC3: NewObject succeeded"), PM);
        if (!PM) { return false; }

        UCurveFloat* IdentityCurve = NewObject<UCurveFloat>(PM);
        IdentityCurve->FloatCurve.AddKey(0.0f, 0.0f);
        IdentityCurve->FloatCurve.AddKey(1.0f, 1.0f);
        PM->SlipCurve            = IdentityCurve;
        PM->bCurveFallbackActive = false;

        const EPlayerLane AllLanes[] = {
            EPlayerLane::FarLeft,
            EPlayerLane::Left,
            EPlayerLane::Center,
            EPlayerLane::Right,
            EPlayerLane::FarRight,
        };

        bool bAllPassed = true;
        for (EPlayerLane From : AllLanes)
        {
            for (EPlayerLane To : AllLanes)
            {
                if (From == To) { continue; } // skip self-transitions (delta==0 anyway)

                PM->current_lane = From;
                PM->target_lane  = To;

                const float Result = PM->F3RelativeOffset(1.0f);
                // IG-5 contract: at TP=1.0, F3RelativeOffset MUST return 0.0f
                // exactly (IEEE 754 Lerp(x, 0, 1) == 0 by algebraic identity).
                // No epsilon — strict inequality catches any future drift.
                if (Result != 0.0f)
                {
                    AddError(FString::Printf(
                        TEXT("TC3: F3RelativeOffset(1.0) == %f (expected 0.0 exact) for From=%d To=%d"),
                        Result,
                        static_cast<int32>(From),
                        static_cast<int32>(To)));
                    bAllPassed = false;
                }
            }
        }

        TestTrue(
            TEXT("TC3: All 20 non-self lane transitions return 0.0 at TP=1.0 (IG-5 zero-relative invariant)"),
            bAllPassed);

        return bAllPassed;
    }

    // -----------------------------------------------------------------------
    // TC4 (f3_fallback_null_curve) — SlipCurve == nullptr + bCurveFallbackActive == true.
    // Exercises the fallback linear path: curve_t = FMath::Clamp(TweenProgress, 0, 1).
    //
    // Left→Center, TP=0.5 → curve_t = 0.5 → FMath::Lerp(-100, 0, 0.5) = -50.
    //
    // Also asserts bCurveFallbackActive is unchanged after the call (F3RelativeOffset
    // is a pure reader — it does not modify state).
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("f3_fallback_null_curve"))
    {
        UPlayerLaneMovementComponent* PM = NewObject<UPlayerLaneMovementComponent>();
        TestNotNull(TEXT("TC4: NewObject succeeded"), PM);
        if (!PM) { return false; }

        PM->current_lane         = EPlayerLane::Left;
        PM->target_lane          = EPlayerLane::Center;
        PM->SlipCurve            = nullptr;
        PM->bCurveFallbackActive = true;

        const float Result = PM->F3RelativeOffset(0.5f);
        TestTrue(
            TEXT("TC4: F3RelativeOffset(0.5) == -50.0f ± 0.001 (null SlipCurve fallback, linear curve_t)"),
            FMath::IsNearlyEqual(Result, -50.0f, 0.001f));

        // bCurveFallbackActive unchanged — F3RelativeOffset must not write state.
        TestTrue(
            TEXT("TC4: bCurveFallbackActive unchanged after F3RelativeOffset call"),
            PM->bCurveFallbackActive);

        return true;
    }

    // -----------------------------------------------------------------------
    // TC5 (f3_fallback_active_flag) — SlipCurve is a valid identity curve but
    // bCurveFallbackActive == true (flag engages fallback regardless of curve validity).
    //
    // Branch condition: `if (SlipCurve && !bCurveFallbackActive)` → false when
    // bCurveFallbackActive is true even if SlipCurve is non-null.
    // Fallback path: curve_t = FMath::Clamp(0.5, 0, 1) = 0.5 → -50.0f.
    //
    // Isolates the "valid curve, flag overrides" path (cf. TC4 which also sets
    // SlipCurve to null — two distinct failure modes).
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("f3_fallback_active_flag"))
    {
        UPlayerLaneMovementComponent* PM = NewObject<UPlayerLaneMovementComponent>();
        TestNotNull(TEXT("TC5: NewObject succeeded"), PM);
        if (!PM) { return false; }

        UCurveFloat* IdentityCurve = NewObject<UCurveFloat>(PM);
        IdentityCurve->FloatCurve.AddKey(0.0f, 0.0f);
        IdentityCurve->FloatCurve.AddKey(1.0f, 1.0f);

        PM->current_lane         = EPlayerLane::Left;
        PM->target_lane          = EPlayerLane::Center;
        PM->SlipCurve            = IdentityCurve; // valid — but flag overrides
        PM->bCurveFallbackActive = true;           // flag engaged

        const float Result = PM->F3RelativeOffset(0.5f);
        TestTrue(
            TEXT("TC5: F3RelativeOffset(0.5) == -50.0f ± 0.001 (bCurveFallbackActive=true overrides valid curve)"),
            FMath::IsNearlyEqual(Result, -50.0f, 0.001f));

        return true;
    }

    // -----------------------------------------------------------------------
    // TC6 (f3_rel_x_slipping_tp05) — tests the rel_x value the tick
    // site writes during SLIPPING: F3RelativeOffset(0.5f) with Left→Center,
    // identity curve, no fallback.
    //
    // Expected rel_x: -50.0f ± 0.001f.
    //
    // Note: this test exercises F3RelativeOffset directly (the pure-math half
    // of the tick site). The full tick-path integration — actor location
    // contribution AND the public lateral_world_position write — is covered by
    // CC3 (lateral_world_position_slipping_write) in the composition test:
    //   Source/SLIPSTORM/Tests/Integration/PlayerMovement/PMLateralInterpolationCompositionTest.cpp
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("f3_rel_x_slipping_tp05"))
    {
        UPlayerLaneMovementComponent* PM = NewObject<UPlayerLaneMovementComponent>();
        TestNotNull(TEXT("TC6: NewObject succeeded"), PM);
        if (!PM) { return false; }

        UCurveFloat* IdentityCurve = NewObject<UCurveFloat>(PM);
        IdentityCurve->FloatCurve.AddKey(0.0f, 0.0f);
        IdentityCurve->FloatCurve.AddKey(1.0f, 1.0f);

        PM->current_lane         = EPlayerLane::Left;
        PM->target_lane          = EPlayerLane::Center;
        PM->SlipCurve            = IdentityCurve;
        PM->bCurveFallbackActive = false;

        const float rel_x = PM->F3RelativeOffset(0.5f);
        TestTrue(
            TEXT("TC6: rel_x == -50.0f ± 0.001 (Left→Center TP=0.5, identity curve)"),
            FMath::IsNearlyEqual(rel_x, -50.0f, 0.001f));

        // Full lateral_world_position tick-path integration lives in
        // PMLateralInterpolationCompositionTest.cpp (lateral_world_position_settled
        // and f3_composition_invariant).

        return true;
    }

    // Unknown parameter — fail explicitly rather than silently returning true.
    AddError(FString::Printf(
        TEXT("FPMLateralInterpolationTest::RunTest — unknown Parameters value: '%s'"),
        *Parameters));
    return false;
}

#endif // WITH_DEV_AUTOMATION_TESTS

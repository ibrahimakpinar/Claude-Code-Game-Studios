// Copyright (c) Game Studio. All rights reserved.
// SLIPSTORM — 60-second voxel endless runner, mobile, Unreal Engine 5.7
//
// PMLeanTest.cpp — Story 006 unit tests for F-5 body/head/arm lean
// (pure math — ComputeLean + DirectionSign).
//
// Spec:  tests/unit/player-movement/pm-lean-spec.md (pending)
// GDD:   design/gdd/player-movement-mechanics.md §4 F-5, §7 Tuning Knobs, §8 AC-26/27/30
// ADR:   docs/architecture/adr-0009-player-movement-hosting.md (SD5, SD6, IG-6)
// Story: production/epics/player-movement/story-006-f5-lean.md
// TR:    TR-PM-008 (F-5 rotation output via LeanCurve + staggered offsets)
//        TR-PM-029 (HEAD_LAG=0.10 / ARM_LEAD=0.05 staggered offsets)
//        TR-PM-033 (±(MAX_LEAN×1.2) final clamp)
//
// Test category: SLIPSTORM.PlayerMovement.Lean
// Runner: UE Automation Framework, headless (-nullrhi -nosound -unattended)
//         ClientContext | ProductFilter — matches existing story precedent.
//
// Pure-math tests — no UWorld or SpawnActor.  All helpers exercised via
// NewObject<UPlayerLaneMovementComponent>() + direct field/method access
// through the FPMLeanTest friend declaration in PlayerLaneMovementComponent.h.
//
// Guard: WITH_DEV_AUTOMATION_TESTS only.  No WITH_EDITOR — no editor APIs used.
//
// Grep gate (manual / code-review): SetActorRotation must NOT appear in any
// PlayerMovement*.cpp file.  This is a code-review check, not an automated
// assertion.  The forbidden pattern is registered in docs/registry/architecture.yaml
// as PlayerMovement_SetActorRotation_for_lean (ADR-0009 IG-6).

#if WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Curves/CurveFloat.h"

#include "Player/PlayerLaneMovementComponent.h"
#include "Player/EPlayerLane.h"
#include "Player/ERunSlipState.h"

// ---------------------------------------------------------------------------
// IMPLEMENT_COMPLEX_AUTOMATION_TEST
// 9 test commands covering AC-26/27/30, staggered offsets, clamp, fallback,
// and direction sign exhaustive transitions.
// ---------------------------------------------------------------------------

IMPLEMENT_COMPLEX_AUTOMATION_TEST(
    FPMLeanTest,
    "SLIPSTORM.PlayerMovement.Lean",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

void FPMLeanTest::GetTests(
    TArray<FString>& OutBeautifiedNames,
    TArray<FString>& OutTestCommands) const
{
    OutBeautifiedNames.Add(TEXT("F-5 body lean — LeanCurve=1.0 at TP=0.20 — Left→Center — body ≈ +10.0°"));
    OutTestCommands.Add(TEXT("lean_tp02_leancurve_1_body_10deg"));

    OutBeautifiedNames.Add(TEXT("F-5 body lean — LeanCurve=1.0 at TP=0.20 — Center→Left — body ≈ -10.0° (negative sign)"));
    OutTestCommands.Add(TEXT("lean_tp02_center_to_left_negative_sign"));

    OutBeautifiedNames.Add(TEXT("F-5 clamp — LeanCurve=1.5 (out of range) — body/head/arm all clamp to ±12.0°"));
    OutTestCommands.Add(TEXT("lean_max_clamp_1_5_curve"));

    OutBeautifiedNames.Add(TEXT("F-5 head lag — identity curve — TP=0.15, HEAD_LAG=0.10 → head sampled at 0.05 → 0.5°"));
    OutTestCommands.Add(TEXT("head_lag_staggered_offset"));

    OutBeautifiedNames.Add(TEXT("F-5 head lag — identity curve — TP=0.05, HEAD_LAG=0.10 → clamped to 0.0 → head 0°"));
    OutTestCommands.Add(TEXT("head_lag_clamped_to_zero"));

    OutBeautifiedNames.Add(TEXT("F-5 arm lead — identity curve — TP=0.80, ARM_LEAD=0.05 → arm sampled at 0.85 → 8.5°"));
    OutTestCommands.Add(TEXT("arm_lead_staggered_offset"));

    OutBeautifiedNames.Add(TEXT("F-5 arm lead — identity curve — TP=0.98, ARM_LEAD=0.05 → clamped to 1.0 → arm ±10.0°"));
    OutTestCommands.Add(TEXT("arm_lead_clamped_to_one"));

    OutBeautifiedNames.Add(TEXT("F-5 fallback — null LeanCurve or bCurveFallbackActive=true → body/head/arm == 0"));
    OutTestCommands.Add(TEXT("fallback_null_curve"));

    OutBeautifiedNames.Add(TEXT("DirectionSign — all 20 non-self lane transitions — +1 rightward, -1 leftward"));
    OutTestCommands.Add(TEXT("direction_sign_all_transitions"));
}

bool FPMLeanTest::RunTest(const FString& Parameters)
{
    // -----------------------------------------------------------------------
    // TC1 (lean_tp02_leancurve_1_body_10deg)
    //
    // Left→Center slip (sign = +1), LeanCurve returning 1.0 at TP=0.20.
    // Curve keys: (0,0), (0.19, 1.0), (0.21, 1.0), (1.0, 1.0) — flat plateau
    // around 0.20 ensures GetFloatValue(0.20) returns exactly 1.0.
    // ComputeLean(0.20, Left, Center, ...):
    //   body_t = 1.0 → body_lean = 1.0 * 10.0 * (+1) = +10.0°
    //   head_t = curve(max(0, 0.10)) = curve(0.10) → 0.0 (below plateau) * 10 = 0°
    //   arm_t  = curve(min(1.0, 0.25)) = curve(0.25) → 1.0 (above plateau) * 10 = +10°
    // This test only asserts body_lean ≈ +10.0° per AC-27 spec focus.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("lean_tp02_leancurve_1_body_10deg"))
    {
        // Arrange
        UPlayerLaneMovementComponent* PM = NewObject<UPlayerLaneMovementComponent>();
        TestNotNull(TEXT("TC1: NewObject succeeded"), PM);
        if (!PM) { return false; }

        // Flat-plateau curve returns 1.0 for TweenProgress in [0.19, 1.0].
        UCurveFloat* PlateauCurve = NewObject<UCurveFloat>(PM);
        PlateauCurve->FloatCurve.AddKey(0.0f,  0.0f);
        PlateauCurve->FloatCurve.AddKey(0.19f, 1.0f);
        PlateauCurve->FloatCurve.AddKey(0.21f, 1.0f);
        PlateauCurve->FloatCurve.AddKey(1.0f,  1.0f);

        PM->LeanCurve           = PlateauCurve;
        PM->bCurveFallbackActive = false;

        // Act
        float body_lean = 0.0f;
        float head_lean = 0.0f;
        float arm_lean  = 0.0f;
        PM->ComputeLean(0.20f, EPlayerLane::Left, EPlayerLane::Center,
                        body_lean, head_lean, arm_lean);

        // Assert
        TestTrue(
            TEXT("TC1: body_lean ≈ +10.0° ± 0.1° (Left→Center at TP=0.20, curve=1.0, sign=+1)"),
            FMath::Abs(body_lean - 10.0f) <= 0.1f);

        return true;
    }

    // -----------------------------------------------------------------------
    // TC2 (lean_tp02_center_to_left_negative_sign)
    //
    // Center→Left slip (sign = -1), same plateau curve (returns 1.0 at TP=0.20).
    // body_lean = 1.0 * 10.0 * (-1) = -10.0°.
    // AC-27 (b): verifies DirectionSign polarity for leftward slips.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("lean_tp02_center_to_left_negative_sign"))
    {
        // Arrange
        UPlayerLaneMovementComponent* PM = NewObject<UPlayerLaneMovementComponent>();
        TestNotNull(TEXT("TC2: NewObject succeeded"), PM);
        if (!PM) { return false; }

        UCurveFloat* PlateauCurve = NewObject<UCurveFloat>(PM);
        PlateauCurve->FloatCurve.AddKey(0.0f,  0.0f);
        PlateauCurve->FloatCurve.AddKey(0.19f, 1.0f);
        PlateauCurve->FloatCurve.AddKey(0.21f, 1.0f);
        PlateauCurve->FloatCurve.AddKey(1.0f,  1.0f);

        PM->LeanCurve            = PlateauCurve;
        PM->bCurveFallbackActive = false;

        // Act
        float body_lean = 0.0f;
        float head_lean = 0.0f;
        float arm_lean  = 0.0f;
        PM->ComputeLean(0.20f, EPlayerLane::Center, EPlayerLane::Left,
                        body_lean, head_lean, arm_lean);

        // Assert
        TestTrue(
            TEXT("TC2: body_lean ≈ -10.0° ± 0.1° (Center→Left at TP=0.20, curve=1.0, sign=-1)"),
            FMath::Abs(body_lean - (-10.0f)) <= 0.1f);

        return true;
    }

    // -----------------------------------------------------------------------
    // TC3 (lean_max_clamp_1_5_curve)
    //
    // LeanCurve returns 1.5 everywhere (flat above authored range).
    // max_clamp = 10.0 * 1.2 = 12.0.
    // raw = 1.5 * 10.0 * sign = ±15.0 → clamped to ±12.0 exactly.
    // All three outputs (body, head, arm) hit the clamp because the curve
    // returns 1.5 at all sample points.
    // AC-27 (b) extended; TR-PM-033. TestEqual used — clamp is exact.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("lean_max_clamp_1_5_curve"))
    {
        // Arrange
        UPlayerLaneMovementComponent* PM = NewObject<UPlayerLaneMovementComponent>();
        TestNotNull(TEXT("TC3: NewObject succeeded"), PM);
        if (!PM) { return false; }

        // Flat curve returning 1.5 across the entire domain.
        UCurveFloat* OverRangeCurve = NewObject<UCurveFloat>(PM);
        OverRangeCurve->FloatCurve.AddKey(0.0f, 1.5f);
        OverRangeCurve->FloatCurve.AddKey(1.0f, 1.5f);

        PM->LeanCurve            = OverRangeCurve;
        PM->bCurveFallbackActive = false;

        // Act
        float body_lean = 0.0f;
        float head_lean = 0.0f;
        float arm_lean  = 0.0f;
        // Left→Center: sign = +1 → expected clamp = +12.0
        PM->ComputeLean(0.20f, EPlayerLane::Left, EPlayerLane::Center,
                        body_lean, head_lean, arm_lean);

        // Assert — FMath::Clamp at the bound produces exact values
        TestEqual(
            TEXT("TC3: body_lean == +12.0° exactly (1.5 * 10 * 1 = 15 → clamped to +12)"),
            body_lean, 12.0f);
        TestEqual(
            TEXT("TC3: head_lean == +12.0° exactly (1.5 * 10 * 1 = 15 → clamped to +12)"),
            head_lean, 12.0f);
        TestEqual(
            TEXT("TC3: arm_lean == +12.0° exactly (1.5 * 10 * 1 = 15 → clamped to +12)"),
            arm_lean, 12.0f);

        return true;
    }

    // -----------------------------------------------------------------------
    // TC4 (head_lag_staggered_offset)
    //
    // Identity LeanCurve (keys (0,0), (1,1)), TP=0.15, HEAD_LAG=0.10.
    // head sampled at max(0, 0.15 - 0.10) = 0.05 → curve(0.05) = 0.05.
    // head_lean = 0.05 * 10.0 * (+1) = +0.5°.
    // body sampled at 0.15 → 0.15 * 10 = +1.5°.
    // arm sampled at min(1.0, 0.20) = 0.20 → 0.20 * 10 = +2.0°.
    // TR-PM-029. Tests that HEAD_LAG_PROGRESS shifts the sample point correctly.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("head_lag_staggered_offset"))
    {
        // Arrange
        UPlayerLaneMovementComponent* PM = NewObject<UPlayerLaneMovementComponent>();
        TestNotNull(TEXT("TC4: NewObject succeeded"), PM);
        if (!PM) { return false; }

        UCurveFloat* IdentityCurve = NewObject<UCurveFloat>(PM);
        IdentityCurve->FloatCurve.AddKey(0.0f, 0.0f);
        IdentityCurve->FloatCurve.AddKey(1.0f, 1.0f);

        PM->LeanCurve            = IdentityCurve;
        PM->bCurveFallbackActive = false;

        // Act
        float body_lean = 0.0f;
        float head_lean = 0.0f;
        float arm_lean  = 0.0f;
        // Left→Center: sign = +1
        PM->ComputeLean(0.15f, EPlayerLane::Left, EPlayerLane::Center,
                        body_lean, head_lean, arm_lean);

        // Assert
        // head_curve_t = max(0, 0.15 - 0.10) = 0.05 → head_lean = 0.05 * 10 * 1 = 0.5°
        TestTrue(
            TEXT("TC4: head_lean ≈ +0.5° ± 0.05° (TP=0.15, HEAD_LAG=0.10 → sampled at 0.05)"),
            FMath::Abs(head_lean - 0.5f) <= 0.05f);

        return true;
    }

    // -----------------------------------------------------------------------
    // TC5 (head_lag_clamped_to_zero)
    //
    // Identity curve, TP=0.05, HEAD_LAG=0.10.
    // head sampled at max(0, 0.05 - 0.10) = max(0, -0.05) = 0.0.
    // curve(0.0) = 0.0 → head_lean = 0.0 * 10 * sign = 0.0°.
    // TR-PM-029. Tests the lower-bound clamp on the head offset.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("head_lag_clamped_to_zero"))
    {
        // Arrange
        UPlayerLaneMovementComponent* PM = NewObject<UPlayerLaneMovementComponent>();
        TestNotNull(TEXT("TC5: NewObject succeeded"), PM);
        if (!PM) { return false; }

        UCurveFloat* IdentityCurve = NewObject<UCurveFloat>(PM);
        IdentityCurve->FloatCurve.AddKey(0.0f, 0.0f);
        IdentityCurve->FloatCurve.AddKey(1.0f, 1.0f);

        PM->LeanCurve            = IdentityCurve;
        PM->bCurveFallbackActive = false;

        // Act
        float body_lean = 0.0f;
        float head_lean = 0.0f;
        float arm_lean  = 0.0f;
        PM->ComputeLean(0.05f, EPlayerLane::Left, EPlayerLane::Center,
                        body_lean, head_lean, arm_lean);

        // Assert
        // head sampled at max(0, -0.05) = 0.0 → curve(0.0) = 0 → head_lean = 0°
        TestEqual(
            TEXT("TC5: head_lean == 0.0° exactly (TP=0.05 - HEAD_LAG=0.10 → clamped to 0.0)"),
            head_lean, 0.0f);

        return true;
    }

    // -----------------------------------------------------------------------
    // TC6 (arm_lead_staggered_offset)
    //
    // Identity LeanCurve, TP=0.80, ARM_LEAD=0.05.
    // arm sampled at min(1.0, 0.80 + 0.05) = 0.85 → curve(0.85) = 0.85.
    // arm_lean = 0.85 * 10.0 * (+1) = +8.5°.
    // TR-PM-029. Tests that ARM_LEAD_PROGRESS shifts the sample point correctly.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("arm_lead_staggered_offset"))
    {
        // Arrange
        UPlayerLaneMovementComponent* PM = NewObject<UPlayerLaneMovementComponent>();
        TestNotNull(TEXT("TC6: NewObject succeeded"), PM);
        if (!PM) { return false; }

        UCurveFloat* IdentityCurve = NewObject<UCurveFloat>(PM);
        IdentityCurve->FloatCurve.AddKey(0.0f, 0.0f);
        IdentityCurve->FloatCurve.AddKey(1.0f, 1.0f);

        PM->LeanCurve            = IdentityCurve;
        PM->bCurveFallbackActive = false;

        // Act
        float body_lean = 0.0f;
        float head_lean = 0.0f;
        float arm_lean  = 0.0f;
        // Left→Center: sign = +1
        PM->ComputeLean(0.80f, EPlayerLane::Left, EPlayerLane::Center,
                        body_lean, head_lean, arm_lean);

        // Assert
        // arm_curve_t = min(1.0, 0.80 + 0.05) = 0.85 → arm_lean = 0.85 * 10 * 1 = 8.5°
        TestTrue(
            TEXT("TC6: arm_lean ≈ +8.5° ± 0.05° (TP=0.80, ARM_LEAD=0.05 → sampled at 0.85)"),
            FMath::Abs(arm_lean - 8.5f) <= 0.05f);

        return true;
    }

    // -----------------------------------------------------------------------
    // TC7 (arm_lead_clamped_to_one)
    //
    // Identity curve, TP=0.98, ARM_LEAD=0.05.
    // arm sampled at min(1.0, 0.98 + 0.05) = min(1.0, 1.03) = 1.0.
    // curve(1.0) = 1.0 → arm_lean = 1.0 * 10.0 * (+1) = ±10.0° = MAX_LEAN_ANGLE_DEG.
    // TR-PM-029. Tests the upper-bound clamp on the arm offset.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("arm_lead_clamped_to_one"))
    {
        // Arrange
        UPlayerLaneMovementComponent* PM = NewObject<UPlayerLaneMovementComponent>();
        TestNotNull(TEXT("TC7: NewObject succeeded"), PM);
        if (!PM) { return false; }

        UCurveFloat* IdentityCurve = NewObject<UCurveFloat>(PM);
        IdentityCurve->FloatCurve.AddKey(0.0f, 0.0f);
        IdentityCurve->FloatCurve.AddKey(1.0f, 1.0f);

        PM->LeanCurve            = IdentityCurve;
        PM->bCurveFallbackActive = false;

        // Act
        float body_lean = 0.0f;
        float head_lean = 0.0f;
        float arm_lean  = 0.0f;
        // Left→Center: sign = +1
        PM->ComputeLean(0.98f, EPlayerLane::Left, EPlayerLane::Center,
                        body_lean, head_lean, arm_lean);

        // Assert
        // arm sampled at min(1.0, 1.03) = 1.0 → arm_lean = 1.0 * 10 * 1 = 10.0°
        TestTrue(
            TEXT("TC7: arm_lean ≈ +10.0° ± 0.05° (TP=0.98, ARM_LEAD=0.05 → clamped to 1.0 → MAX_LEAN)"),
            FMath::Abs(arm_lean - 10.0f) <= 0.05f);

        return true;
    }

    // -----------------------------------------------------------------------
    // TC8 (fallback_null_curve)
    //
    // Two fallback triggers:
    //   (a) LeanCurve = nullptr, bCurveFallbackActive = false.
    //   (b) LeanCurve = valid curve, bCurveFallbackActive = true.
    // Both must produce body=head=arm=0.0f.
    // ADR-0009 SD6; TR-PM-008. Mirrors F3RelativeOffset's fallback TC pattern.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("fallback_null_curve"))
    {
        // Arrange — path (a): null curve
        {
            UPlayerLaneMovementComponent* PM = NewObject<UPlayerLaneMovementComponent>();
            TestNotNull(TEXT("TC8a: NewObject succeeded"), PM);
            if (!PM) { return false; }

            PM->LeanCurve            = nullptr;
            PM->bCurveFallbackActive = false;

            // Act
            float body_lean = 99.0f;
            float head_lean = 99.0f;
            float arm_lean  = 99.0f;
            PM->ComputeLean(0.5f, EPlayerLane::Left, EPlayerLane::Center,
                            body_lean, head_lean, arm_lean);

            // Assert
            TestEqual(TEXT("TC8a: body_lean == 0.0f (null LeanCurve fallback)"), body_lean, 0.0f);
            TestEqual(TEXT("TC8a: head_lean == 0.0f (null LeanCurve fallback)"), head_lean, 0.0f);
            TestEqual(TEXT("TC8a: arm_lean == 0.0f (null LeanCurve fallback)"),  arm_lean,  0.0f);
        }

        // Arrange — path (b): valid curve, fallback flag set
        {
            UPlayerLaneMovementComponent* PM = NewObject<UPlayerLaneMovementComponent>();
            TestNotNull(TEXT("TC8b: NewObject succeeded"), PM);
            if (!PM) { return false; }

            UCurveFloat* ValidCurve = NewObject<UCurveFloat>(PM);
            ValidCurve->FloatCurve.AddKey(0.0f, 0.0f);
            ValidCurve->FloatCurve.AddKey(1.0f, 1.0f);

            PM->LeanCurve            = ValidCurve; // valid — but flag overrides
            PM->bCurveFallbackActive = true;

            // Act
            float body_lean = 99.0f;
            float head_lean = 99.0f;
            float arm_lean  = 99.0f;
            PM->ComputeLean(0.5f, EPlayerLane::Left, EPlayerLane::Center,
                            body_lean, head_lean, arm_lean);

            // Assert
            TestEqual(TEXT("TC8b: body_lean == 0.0f (bCurveFallbackActive=true overrides valid curve)"), body_lean, 0.0f);
            TestEqual(TEXT("TC8b: head_lean == 0.0f (bCurveFallbackActive=true overrides valid curve)"), head_lean, 0.0f);
            TestEqual(TEXT("TC8b: arm_lean == 0.0f (bCurveFallbackActive=true overrides valid curve)"),  arm_lean,  0.0f);
        }

        return true;
    }

    // -----------------------------------------------------------------------
    // TC9 (direction_sign_all_transitions)
    //
    // Exhaustive check across all 20 non-self lane-pair transitions.
    // DirectionSign(From, To) must return:
    //   +1.0f when static_cast<int32>(To) > static_cast<int32>(From)  (rightward)
    //   -1.0f when static_cast<int32>(To) < static_cast<int32>(From)  (leftward)
    // Ordinals: FarLeft=0, Left=1, Center=2, Right=3, FarRight=4.
    // Static method — called via PM instance through friend access.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("direction_sign_all_transitions"))
    {
        // Arrange — PM instance used only for GC scoping; DirectionSign is static
        UPlayerLaneMovementComponent* PM = NewObject<UPlayerLaneMovementComponent>();
        TestNotNull(TEXT("TC9: NewObject succeeded"), PM);
        if (!PM) { return false; }

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
                if (From == To) { continue; } // skip self-transitions

                // Act
                const float sign = UPlayerLaneMovementComponent::DirectionSign(From, To);

                // Assert
                const int32 from_ord = static_cast<int32>(From);
                const int32 to_ord   = static_cast<int32>(To);
                const float expected = (to_ord > from_ord) ? 1.0f : -1.0f;

                if (sign != expected)
                {
                    AddError(FString::Printf(
                        TEXT("TC9: DirectionSign(From=%d, To=%d) == %f, expected %f"),
                        from_ord, to_ord, sign, expected));
                    bAllPassed = false;
                }
            }
        }

        TestTrue(
            TEXT("TC9: All 20 non-self transitions return correct DirectionSign (+1 rightward, -1 leftward)"),
            bAllPassed);

        return bAllPassed;
    }

    // Unknown parameter — fail explicitly rather than silently returning true.
    AddError(FString::Printf(
        TEXT("FPMLeanTest::RunTest — unknown Parameters value: '%s'"),
        *Parameters));
    return false;
}

#endif // WITH_DEV_AUTOMATION_TESTS

// Copyright (c) Game Studio. All rights reserved.
// SLIPSTORM — 60-second voxel endless runner, mobile, Unreal Engine 5.7
//
// PMEdgeAbsorbTest.cpp — Story 007 pure-math unit tests for F-6 edge-absorb.
//
// Spec:   production/epics/player-movement/story-007-f6-edge-absorb.md
// GDD:    design/gdd/player-movement-mechanics.md §3 Rule 1, §4 F-6, §5.1(a)
//         Override, R11a §6.2 EC15 decay, §8 AC-02, AC-F6-A/B/C/E
// ADR:    docs/architecture/adr-0009-player-movement-hosting.md (SD6)
// TR:     TR-PM-009 (F-6 tail + Override), TR-PM-028 (EC15 decay)
//
// Test category: SLIPSTORM.PlayerMovement.EdgeAbsorb
// Runner: UE Automation Framework, ClientContext | ProductFilter.
//
// Pure-math tests via NewObject<UPlayerLaneMovementComponent>() + direct field
// mutation through the FPMEdgeAbsorbTest friend declaration. No UWorld spawn.
// Composition tests (Rule 5 gate + OnSlipMidpoint exclusion + AC-F6-B fade-out
// across ticks) live in:
//   Source/SLIPSTORM/Tests/Integration/PlayerMovement/PMEdgeAbsorbCompositionTest.cpp
//
// AAA (Arrange / Act / Assert) labels present per test-standards.md convention.
//
// Build guard: WITH_DEV_AUTOMATION_TESTS.

#if WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#include "Player/PlayerLaneMovementComponent.h"
#include "Player/EPlayerLane.h"
#include "Player/ERunSlipState.h"
#include "Player/ESlipDirection.h"

// ---------------------------------------------------------------------------
// Helper: identity UCurveFloat with keys (0, 0) and (1, 1).
// Anchored to InOuter to survive GC within the test scope.
// ---------------------------------------------------------------------------

static UCurveFloat* MakeIdentityCurve_EA(UObject* InOuter)
{
    UCurveFloat* C = NewObject<UCurveFloat>(InOuter);
    C->FloatCurve.AddKey(0.0f, 0.0f);
    C->FloatCurve.AddKey(1.0f, 1.0f);
    return C;
}

// ---------------------------------------------------------------------------
// Helper: unit-value UCurveFloat that returns 1.0f at all inputs (plateau).
// Used for AC-F6-E worst-case EC15 test to make body/head/arm hit MAX_LEAN
// before decay + clamp.
// ---------------------------------------------------------------------------

static UCurveFloat* MakePlateauCurve_EA(UObject* InOuter, float PlateauValue = 1.0f)
{
    UCurveFloat* C = NewObject<UCurveFloat>(InOuter);
    C->FloatCurve.AddKey(0.0f, PlateauValue);
    C->FloatCurve.AddKey(1.0f, PlateauValue);
    return C;
}

// ---------------------------------------------------------------------------
// IMPLEMENT_COMPLEX_AUTOMATION_TEST — 7 pure-math test commands.
// ---------------------------------------------------------------------------

IMPLEMENT_COMPLEX_AUTOMATION_TEST(
    FPMEdgeAbsorbTest,
    "SLIPSTORM.PlayerMovement.EdgeAbsorb",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

void FPMEdgeAbsorbTest::GetTests(
    TArray<FString>& OutBeautifiedNames,
    TArray<FString>& OutTestCommands) const
{
    OutBeautifiedNames.Add(TEXT("TriggerEdgeAbsorb sets edge_absorb_* state + increments counter (Rule 1)"));
    OutTestCommands.Add(TEXT("trigger_edge_absorb_state_and_counter"));

    OutBeautifiedNames.Add(TEXT("ComputeF6 fallback (EdgeAbsorbCurve null) → zero + collapse edge_absorb_active"));
    OutTestCommands.Add(TEXT("computef6_fallback_null_curve_collapses_active"));

    OutBeautifiedNames.Add(TEXT("ComputeF6 fallback (bCurveFallbackActive=true) → zero even with valid curve"));
    OutTestCommands.Add(TEXT("computef6_fallback_active_flag"));

    OutBeautifiedNames.Add(TEXT("ComputeF6 inactive branch → zero outputs (no snapshot)"));
    OutTestCommands.Add(TEXT("computef6_inactive_branch_zero"));

    OutBeautifiedNames.Add(TEXT("ComputeF6 active + SLIPPING + TP > PHASE3 → EC15 decay applied"));
    OutTestCommands.Add(TEXT("computef6_ec15_decay_applied_when_phase3"));

    OutBeautifiedNames.Add(TEXT("ComputeF6 active + SETTLED → NO EC15 decay (edge no-op case)"));
    OutTestCommands.Add(TEXT("computef6_no_ec15_decay_when_settled"));

    OutBeautifiedNames.Add(TEXT("§5.1(a) Override snapshot fade-out — 2-frame ramp 1.0 → 0.5 → 0"));
    OutTestCommands.Add(TEXT("override_fadeout_two_frame_ramp"));
}

bool FPMEdgeAbsorbTest::RunTest(const FString& Parameters)
{
    // -----------------------------------------------------------------------
    // TC1 — TriggerEdgeAbsorb sets state and increments counter.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("trigger_edge_absorb_state_and_counter"))
    {
        // Arrange
        UPlayerLaneMovementComponent* PM = NewObject<UPlayerLaneMovementComponent>();
        TestNotNull(TEXT("TC1: NewObject succeeded"), PM);
        if (!PM) { return false; }
        const int32 CounterBefore = PM->edge_absorb_trigger_count;

        // Act — Right at FarRight edge (typical AC-02 shape).
        PM->TriggerEdgeAbsorb(EPlayerLane::FarRight, ESlipDirection::Right);

        // Assert
        TestTrue(TEXT("TC1: edge_absorb_active == true after Trigger"), PM->edge_absorb_active);
        TestEqual(TEXT("TC1: edge_absorb_progress == 0.0 at start"), PM->edge_absorb_progress, 0.0f);
        TestEqual(TEXT("TC1: edge_absorb_local_timer_s == 0.0 at start"), PM->edge_absorb_local_timer_s, 0.0f);
        // Sign convention per AC-F6-A: slip-right at right edge → leftward recoil → sign = -1.
        // Story spec §Implementation Notes pseudocode had this inverted; the code correction
        // makes AC-F6-A ("FarRight + slip-right → body/head/arm all negative at mid-tail") pass.
        TestEqual(TEXT("TC1: edge_absorb_sign == -1 for slip-right (recoil away from wall, AC-F6-A)"),
                  PM->edge_absorb_sign, -1.0f);
        TestEqual(TEXT("TC1: edge_absorb_trigger_count += 1 (Rule 1 counter)"),
                  PM->edge_absorb_trigger_count, CounterBefore + 1);

        // Edge — slip-left at FarLeft edge yields sign = +1 (rightward recoil).
        UPlayerLaneMovementComponent* PM2 = NewObject<UPlayerLaneMovementComponent>();
        PM2->TriggerEdgeAbsorb(EPlayerLane::FarLeft, ESlipDirection::Left);
        TestEqual(TEXT("TC1: edge_absorb_sign == +1 for slip-left (rightward recoil, AC-F6-A)"),
                  PM2->edge_absorb_sign, +1.0f);

        return true;
    }

    // -----------------------------------------------------------------------
    // TC2 — Fallback (EdgeAbsorbCurve null) → zero + collapse edge_absorb_active.
    //
    // SD6 fallback semantic: counter still increments (in TriggerEdgeAbsorb),
    // but the tail collapses immediately — no visual F-6 output.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("computef6_fallback_null_curve_collapses_active"))
    {
        // Arrange
        UPlayerLaneMovementComponent* PM = NewObject<UPlayerLaneMovementComponent>();
        PM->EdgeAbsorbCurve         = nullptr;
        PM->bCurveFallbackActive    = false;
        PM->edge_absorb_active      = true;
        PM->edge_absorb_progress    = 0.3f;
        PM->edge_absorb_sign        = 1.0f;
        float body = 99.0f, head = 99.0f, arm = 99.0f; // sentinel

        // Act
        PM->ComputeF6(body, head, arm);

        // Assert
        TestEqual(TEXT("TC2: body == 0 (fallback)"), body, 0.0f);
        TestEqual(TEXT("TC2: head == 0 (fallback)"), head, 0.0f);
        TestEqual(TEXT("TC2: arm == 0 (fallback)"),  arm,  0.0f);
        TestFalse(TEXT("TC2: edge_absorb_active collapsed to false"), PM->edge_absorb_active);
        return true;
    }

    // -----------------------------------------------------------------------
    // TC3 — Fallback via bCurveFallbackActive flag (curve present but flagged).
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("computef6_fallback_active_flag"))
    {
        // Arrange
        UPlayerLaneMovementComponent* PM = NewObject<UPlayerLaneMovementComponent>();
        PM->EdgeAbsorbCurve         = MakeIdentityCurve_EA(PM);
        PM->bCurveFallbackActive    = true; // valid curve, but flagged
        PM->edge_absorb_active      = true;
        PM->edge_absorb_progress    = 0.5f;
        PM->edge_absorb_sign        = 1.0f;
        float body = 99.0f, head = 99.0f, arm = 99.0f;

        // Act
        PM->ComputeF6(body, head, arm);

        // Assert
        TestEqual(TEXT("TC3: body == 0 (flag fallback)"), body, 0.0f);
        TestEqual(TEXT("TC3: head == 0 (flag fallback)"), head, 0.0f);
        TestEqual(TEXT("TC3: arm == 0 (flag fallback)"),  arm,  0.0f);
        TestFalse(TEXT("TC3: edge_absorb_active collapsed"), PM->edge_absorb_active);
        return true;
    }

    // -----------------------------------------------------------------------
    // TC4 — Inactive edge-absorb → zero outputs.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("computef6_inactive_branch_zero"))
    {
        // Arrange
        UPlayerLaneMovementComponent* PM = NewObject<UPlayerLaneMovementComponent>();
        PM->EdgeAbsorbCurve         = MakeIdentityCurve_EA(PM);
        PM->bCurveFallbackActive    = false;
        PM->edge_absorb_active      = false; // inactive
        PM->f6_override_fadeout_ticks_remaining = 0; // no snapshot either
        float body = 99.0f, head = 99.0f, arm = 99.0f;

        // Act
        PM->ComputeF6(body, head, arm);

        // Assert
        TestEqual(TEXT("TC4: body == 0 (inactive)"), body, 0.0f);
        TestEqual(TEXT("TC4: head == 0 (inactive)"), head, 0.0f);
        TestEqual(TEXT("TC4: arm == 0 (inactive)"),  arm,  0.0f);
        return true;
    }

    // -----------------------------------------------------------------------
    // TC5 — EC15 decay applied when SLIPPING and tween_progress > PHASE3_TP_THRESHOLD.
    //
    // Setup: identity curve; edge_absorb_progress = 0.5 (body_t=0.5 exact via identity);
    //        sign = +1; MAX_LEAN=10; expected pre-decay body = 0.5 * 10 * 1 = 5.0°.
    //        With EC15 = 0.7 → decayed body = 5.0 * 0.7 = 3.5°.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("computef6_ec15_decay_applied_when_phase3"))
    {
        // Arrange — SLIPPING, tween_progress past phase-3 threshold (0.66).
        UPlayerLaneMovementComponent* PM = NewObject<UPlayerLaneMovementComponent>();
        PM->EdgeAbsorbCurve       = MakeIdentityCurve_EA(PM);
        PM->bCurveFallbackActive  = false;
        PM->edge_absorb_active    = true;
        PM->edge_absorb_progress  = 0.5f;
        PM->edge_absorb_sign      = 1.0f;
        PM->movement_state        = ERunSlipState::SLIPPING;
        PM->tween_progress        = 0.85f; // > PHASE3_TP_THRESHOLD (0.66)
        PM->f6_override_fadeout_ticks_remaining = 0;
        float body = 0.0f, head = 0.0f, arm = 0.0f;

        // Act
        PM->ComputeF6(body, head, arm);

        // Assert — decayed body = 5.0 * 0.7 = 3.5°.
        TestTrue(TEXT("TC5: body ≈ 3.5° ± 0.05 (EC15 decay 0.5 * 10 * 0.7 = 3.5)"),
                 FMath::Abs(body - 3.5f) <= 0.05f);
        // Head at TP-HEAD_LAG = 0.5 - 0.10 = 0.4 → curve(0.4) = 0.4 → 0.4 * 10 * 0.7 = 2.8°.
        TestTrue(TEXT("TC5: head ≈ 2.8° ± 0.05 (staggered + decay)"),
                 FMath::Abs(head - 2.8f) <= 0.05f);
        // Arm at TP+ARM_LEAD = 0.5 + 0.05 = 0.55 → 0.55 * 10 * 0.7 = 3.85°.
        TestTrue(TEXT("TC5: arm ≈ 3.85° ± 0.05 (staggered + decay)"),
                 FMath::Abs(arm - 3.85f) <= 0.05f);
        return true;
    }

    // -----------------------------------------------------------------------
    // TC6 — NO EC15 decay when SETTLED (edge no-op case).
    //
    // The decay is a phase-3 headroom concern for the SLIPPING F-5+F-6 sum.
    // In the Rule 1 edge no-op case PM stays SETTLED and F-5 contribution is 0,
    // so the decay must NOT apply — F-6 alone gets full MAX_LEAN authority.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("computef6_no_ec15_decay_when_settled"))
    {
        // Arrange
        UPlayerLaneMovementComponent* PM = NewObject<UPlayerLaneMovementComponent>();
        PM->EdgeAbsorbCurve       = MakeIdentityCurve_EA(PM);
        PM->bCurveFallbackActive  = false;
        PM->edge_absorb_active    = true;
        PM->edge_absorb_progress  = 0.5f;
        PM->edge_absorb_sign      = 1.0f;
        PM->movement_state        = ERunSlipState::SETTLED; // key: NOT SLIPPING
        PM->tween_progress        = 0.85f; // would trigger decay if state were SLIPPING
        PM->f6_override_fadeout_ticks_remaining = 0;
        float body = 0.0f;
        float head = 0.0f;
        float arm  = 0.0f;

        // Act
        PM->ComputeF6(body, head, arm);

        // Assert — full value: 0.5 * 10 * 1.0 (no decay) = 5.0°.
        TestTrue(TEXT("TC6: body ≈ 5.0° ± 0.05 (no decay under SETTLED)"),
                 FMath::Abs(body - 5.0f) <= 0.05f);
        return true;
    }

    // -----------------------------------------------------------------------
    // TC7 — §5.1(a) Override fade-out 2-frame ramp (1.0 → 0.5 → 0).
    //
    // Arrange snapshot values with edge_absorb_active=false (so base branch is
    // the inactive-zero path). Set ticks_remaining=2. Call ComputeF6 three times;
    // outputs should equal snapshot × 1.0, snapshot × 0.5, snapshot × 0.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("override_fadeout_two_frame_ramp"))
    {
        // Arrange — snapshot = -6.0 (body), -4.5 (head), -5.0 (arm).
        UPlayerLaneMovementComponent* PM = NewObject<UPlayerLaneMovementComponent>();
        PM->EdgeAbsorbCurve       = MakeIdentityCurve_EA(PM);
        PM->bCurveFallbackActive  = false;
        PM->edge_absorb_active    = false; // base branch = inactive-zero
        PM->f6_override_fadeout_snapshot_body   = -6.0f;
        PM->f6_override_fadeout_snapshot_head   = -4.5f;
        PM->f6_override_fadeout_snapshot_arm    = -5.0f;
        PM->f6_override_fadeout_ticks_remaining = 2;

        // Act tick 1 — expect multiplier 1.0.
        float body1 = 0.0f, head1 = 0.0f, arm1 = 0.0f;
        PM->ComputeF6(body1, head1, arm1);
        // Assert tick 1
        TestTrue(TEXT("TC7: tick 1 body ≈ -6.0 (mult 1.0)"), FMath::Abs(body1 - (-6.0f)) <= 0.01f);
        TestTrue(TEXT("TC7: tick 1 head ≈ -4.5"),           FMath::Abs(head1 - (-4.5f)) <= 0.01f);
        TestTrue(TEXT("TC7: tick 1 arm  ≈ -5.0"),           FMath::Abs(arm1  - (-5.0f)) <= 0.01f);
        TestEqual(TEXT("TC7: tick 1 ticks_remaining decremented to 1"),
                  PM->f6_override_fadeout_ticks_remaining, 1);

        // Act tick 2 — expect multiplier 0.5.
        float body2 = 0.0f, head2 = 0.0f, arm2 = 0.0f;
        PM->ComputeF6(body2, head2, arm2);
        // Assert tick 2
        TestTrue(TEXT("TC7: tick 2 body ≈ -3.0 (mult 0.5)"), FMath::Abs(body2 - (-3.0f)) <= 0.01f);
        TestTrue(TEXT("TC7: tick 2 head ≈ -2.25"),          FMath::Abs(head2 - (-2.25f)) <= 0.01f);
        TestTrue(TEXT("TC7: tick 2 arm  ≈ -2.5"),           FMath::Abs(arm2  - (-2.5f)) <= 0.01f);
        TestEqual(TEXT("TC7: tick 2 ticks_remaining decremented to 0"),
                  PM->f6_override_fadeout_ticks_remaining, 0);

        // Act tick 3 — expect no snapshot contribution (multiplier 0 effectively).
        float body3 = 0.0f, head3 = 0.0f, arm3 = 0.0f;
        PM->ComputeF6(body3, head3, arm3);
        // Assert tick 3
        TestEqual(TEXT("TC7: tick 3 body == 0 (fade-out complete)"), body3, 0.0f);
        TestEqual(TEXT("TC7: tick 3 head == 0 (fade-out complete)"), head3, 0.0f);
        TestEqual(TEXT("TC7: tick 3 arm  == 0 (fade-out complete)"), arm3,  0.0f);
        return true;
    }

    AddError(FString::Printf(
        TEXT("FPMEdgeAbsorbTest::RunTest — unknown Parameters: '%s'"),
        *Parameters));
    return false;
}

#endif // WITH_DEV_AUTOMATION_TESTS

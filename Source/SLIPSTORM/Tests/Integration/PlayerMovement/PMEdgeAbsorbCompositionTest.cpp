// Copyright (c) Game Studio. All rights reserved.
// SLIPSTORM — 60-second voxel endless runner, mobile, Unreal Engine 5.7
//
// PMEdgeAbsorbCompositionTest.cpp — Story 007 composition/integration tests.
//
// Location: Integration/ because tests require UWorld spawn + tick to exercise
// the HandleSlipTransition edge-no-op path (AC-02), §5.1(a) Override snapshot
// on SETTLED→SLIPPING transition, and the OnSlipMidpoint delegate exclusion
// (AC-24) — none reachable from pure-math NewObject tests.
//
// Pure-math ComputeF6 branches live in:
//   Source/SLIPSTORM/Tests/Unit/PlayerMovement/PMEdgeAbsorbTest.cpp
//
// Spec:   production/epics/player-movement/story-007-f6-edge-absorb.md
// GDD:    design/gdd/player-movement-mechanics.md §3 Rule 1, §4 F-6, §5.1(a),
//         §8 AC-02 + AC-F6-A/B/C/E + AC-24
// ADR:    docs/architecture/adr-0009-player-movement-hosting.md (SD6)
// TR:     TR-PM-009 (F-6 tail), TR-PM-028 (EC15 decay)
//
// Test category: SLIPSTORM.PlayerMovement.EdgeAbsorbComposition
// Runner: UE Automation Framework, ClientContext | ProductFilter.
//
// Helpers `_EAC`-suffixed to avoid ODR collisions with prior composition tests.
// AAA labels present.
//
// Build guard: WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS.

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Curves/CurveFloat.h"
#include "Misc/App.h"

#include "Player/PlayerLaneMovementComponent.h"
#include "Player/SlipstormPlayerPawn.h"
#include "Player/EPlayerLane.h"
#include "Player/ERunSlipState.h"
#include "Player/ESlipDirection.h"
#include "RunStateMachine/RunStateMachineSubsystem.h"
#include "RunStateMachine/ERunState.h"

// ---------------------------------------------------------------------------
// Helpers (duplicated from PMStateMachineTest pattern).
// ---------------------------------------------------------------------------

static UCurveFloat* MakeIdentityCurve_EAC(UObject* InOuter)
{
    UCurveFloat* C = NewObject<UCurveFloat>(InOuter);
    C->FloatCurve.AddKey(0.0f, 0.0f);
    C->FloatCurve.AddKey(1.0f, 1.0f);
    return C;
}

static UCurveFloat* MakePlateauCurve_EAC(UObject* InOuter, float PlateauValue)
{
    UCurveFloat* C = NewObject<UCurveFloat>(InOuter);
    C->FloatCurve.AddKey(0.0f, PlateauValue);
    C->FloatCurve.AddKey(1.0f, PlateauValue);
    return C;
}

static UWorld* CreateTestPlayWorld_EAC(FAutomationTestBase* T, const TCHAR* Label)
{
    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    if (!World)
    {
        T->AddError(FString::Printf(TEXT("%s: CreateNewMap returned null"), Label));
        return nullptr;
    }
    World->InitializeActorsForPlay(FURL(nullptr));
    World->BeginPlay();
    return World;
}

static ASlipstormPlayerPawn* SpawnPawnWithCurves_EAC(
    FAutomationTestBase* T,
    UWorld* World,
    const TCHAR* Label)
{
    ASlipstormPlayerPawn* Pawn = World->SpawnActorDeferred<ASlipstormPlayerPawn>(
        ASlipstormPlayerPawn::StaticClass(), FTransform::Identity);
    if (!T->TestNotNull(FString::Printf(TEXT("%s: SpawnActorDeferred returned null"), Label), Pawn))
    {
        return nullptr;
    }
    Pawn->MovementComponent->SlipCurve       = MakeIdentityCurve_EAC(World);
    Pawn->MovementComponent->LeanCurve       = MakeIdentityCurve_EAC(World);
    Pawn->MovementComponent->EdgeAbsorbCurve = MakeIdentityCurve_EAC(World);
    UGameplayStatics::FinishSpawningActor(Pawn, FTransform::Identity);
    return Pawn;
}

static URunStateMachineSubsystem* GetRSM_EAC(
    FAutomationTestBase* T,
    UWorld* World,
    const TCHAR* Label)
{
    UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
    URunStateMachineSubsystem* RSM =
        GI ? GI->GetSubsystem<URunStateMachineSubsystem>() : nullptr;
    if (!T->TestNotNull(FString::Printf(TEXT("%s: RSMSubsystem"), Label), RSM))
    {
        return nullptr;
    }
    return RSM;
}

static void SetRSMRunning_EAC(URunStateMachineSubsystem* RSM)
{
    RSM->TestOnly_CurrentState = ERunState::RUNNING;
    RSM->TestOnly_bPaused      = false;
    RSM->TestOnly_bResumeGrace = false;
}

// ---------------------------------------------------------------------------
// IMPLEMENT_COMPLEX_AUTOMATION_TEST — 5 composition test commands.
// ---------------------------------------------------------------------------

IMPLEMENT_COMPLEX_AUTOMATION_TEST(
    FPMEdgeAbsorbCompositionTest,
    "SLIPSTORM.PlayerMovement.EdgeAbsorbComposition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

void FPMEdgeAbsorbCompositionTest::GetTests(
    TArray<FString>& OutBeautifiedNames,
    TArray<FString>& OutTestCommands) const
{
    OutBeautifiedNames.Add(TEXT("AC-02 edge no-op — SETTLED @ FarRight + slip-right → stays SETTLED, F-6 fires, counter=1"));
    OutTestCommands.Add(TEXT("ac02_edge_noop_stays_settled_fires_f6"));

    OutBeautifiedNames.Add(TEXT("AC-F6-C counter-case — SETTLED @ Left + slip-left → valid SLIPPING (not F-6)"));
    OutTestCommands.Add(TEXT("ac_f6_c_valid_slip_does_not_fire_f6"));

    OutBeautifiedNames.Add(TEXT("§5.1(a) Override snapshot — SETTLED→SLIPPING while F-6 active captures f6 into snapshot fields"));
    OutTestCommands.Add(TEXT("override_snapshot_on_settled_to_slipping"));

    OutBeautifiedNames.Add(TEXT("AC-F6-B — 2-frame fade-out delta ≤ 50% of snapshot magnitude across ticks"));
    OutTestCommands.Add(TEXT("ac_f6_b_fade_out_no_more_than_50pct_jump"));

    OutBeautifiedNames.Add(TEXT("AC-24 exclusion — OnSlipMidpoint NEVER broadcast during F-6 tail"));
    OutTestCommands.Add(TEXT("ac24_no_on_slip_midpoint_during_f6_tail"));

    OutBeautifiedNames.Add(TEXT("AC-F6-A — FarRight+slip-right mid-tail: body/head/arm all NEGATIVE (recoil left)"));
    OutTestCommands.Add(TEXT("ac_f6_a_farright_slipright_mid_tail_all_negative"));

    OutBeautifiedNames.Add(TEXT("AC-F6-C+ — FarLeft+slip-left → F-6 fires + rightward recoil sign"));
    OutTestCommands.Add(TEXT("ac_f6_c_farleft_slipleft_fires_f6"));
}

bool FPMEdgeAbsorbCompositionTest::RunTest(const FString& Parameters)
{
    // -----------------------------------------------------------------------
    // CC1 (ac02_edge_noop_stays_settled_fires_f6) — AC-02.
    //
    // Setup: RSM = RUNNING. SETTLED at FarRight. HandleSlipTransition(Right).
    // Verify: PM stays SETTLED; edge_absorb_active=true; counter=1; no OnSlipMidpoint.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("ac02_edge_noop_stays_settled_fires_f6"))
    {
        // Arrange
        UWorld* TestWorld = CreateTestPlayWorld_EAC(this, TEXT("CC1"));
        if (!TestWorld) { return false; }
        ASlipstormPlayerPawn* Pawn = SpawnPawnWithCurves_EAC(this, TestWorld, TEXT("CC1"));
        if (!Pawn) { return false; }
        URunStateMachineSubsystem* RSM = GetRSM_EAC(this, TestWorld, TEXT("CC1"));
        if (!RSM) { return false; }

        UPlayerLaneMovementComponent* PM = Pawn->MovementComponent.Get();
        SetRSMRunning_EAC(RSM);

        PM->movement_state = ERunSlipState::SETTLED;
        PM->current_lane   = EPlayerLane::FarRight;
        PM->target_lane    = EPlayerLane::FarRight;
        const int32 CounterBefore = PM->edge_absorb_trigger_count;

        // OnSlipMidpoint spy: bind a lambda that increments a counter.
        int32 MidpointBroadcasts = 0;
        FDelegateHandle SpyHandle = PM->OnSlipMidpoint.AddLambda(
            [&MidpointBroadcasts](EPlayerLane, EPlayerLane) { ++MidpointBroadcasts; });

        // Act — slip-right at FarRight is off-track → edge no-op path.
        PM->HandleSlipTransition(ESlipDirection::Right);

        // Assert
        TestEqual(TEXT("CC1: movement_state stays SETTLED"),
                  static_cast<uint8>(PM->movement_state), static_cast<uint8>(ERunSlipState::SETTLED));
        TestTrue(TEXT("CC1: edge_absorb_active == true (F-6 fired)"), PM->edge_absorb_active);
        TestEqual(TEXT("CC1: edge_absorb_trigger_count += 1"),
                  PM->edge_absorb_trigger_count, CounterBefore + 1);
        TestEqual(TEXT("CC1: OnSlipMidpoint NOT broadcast on edge no-op"),
                  MidpointBroadcasts, 0);

        PM->OnSlipMidpoint.Remove(SpyHandle);
        TestWorld->DestroyActor(Pawn);
        return true;
    }

    // -----------------------------------------------------------------------
    // CC2 (ac_f6_c_valid_slip_does_not_fire_f6) — AC-F6-C counter-case.
    //
    // SETTLED at Left + slip-left → projected FarLeft is valid; SLIPPING starts;
    // F-6 must NOT fire; counter unchanged.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("ac_f6_c_valid_slip_does_not_fire_f6"))
    {
        // Arrange
        UWorld* TestWorld = CreateTestPlayWorld_EAC(this, TEXT("CC2"));
        if (!TestWorld) { return false; }
        ASlipstormPlayerPawn* Pawn = SpawnPawnWithCurves_EAC(this, TestWorld, TEXT("CC2"));
        if (!Pawn) { return false; }
        URunStateMachineSubsystem* RSM = GetRSM_EAC(this, TestWorld, TEXT("CC2"));
        if (!RSM) { return false; }

        UPlayerLaneMovementComponent* PM = Pawn->MovementComponent.Get();
        SetRSMRunning_EAC(RSM);

        PM->movement_state = ERunSlipState::SETTLED;
        PM->current_lane   = EPlayerLane::Left;
        PM->target_lane    = EPlayerLane::Left;
        const int32 CounterBefore = PM->edge_absorb_trigger_count;

        // Act — slip-left from Left projects to FarLeft (valid). SLIPPING starts.
        PM->HandleSlipTransition(ESlipDirection::Left);

        // Assert
        TestEqual(TEXT("CC2: movement_state == SLIPPING (valid slip)"),
                  static_cast<uint8>(PM->movement_state), static_cast<uint8>(ERunSlipState::SLIPPING));
        TestEqual(TEXT("CC2: target_lane == FarLeft (projected)"),
                  static_cast<uint8>(PM->target_lane), static_cast<uint8>(EPlayerLane::FarLeft));
        TestFalse(TEXT("CC2: edge_absorb_active == false (F-6 did NOT fire)"), PM->edge_absorb_active);
        TestEqual(TEXT("CC2: edge_absorb_trigger_count unchanged"),
                  PM->edge_absorb_trigger_count, CounterBefore);

        TestWorld->DestroyActor(Pawn);
        return true;
    }

    // -----------------------------------------------------------------------
    // CC3 (override_snapshot_on_settled_to_slipping) — §5.1(a) Override capture.
    //
    // Setup: PM SETTLED at FarRight with F-6 mid-tail (progress=0.5, sign=+1).
    // Then HandleSlipTransition(Left) — valid inward slip → SETTLED→SLIPPING.
    // Verify: f6_override_fadeout_snapshot_* populated with pre-transition F-6 values;
    //         f6_override_fadeout_ticks_remaining == 2;
    //         edge_absorb_active cleared.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("override_snapshot_on_settled_to_slipping"))
    {
        // Arrange
        UWorld* TestWorld = CreateTestPlayWorld_EAC(this, TEXT("CC3"));
        if (!TestWorld) { return false; }
        ASlipstormPlayerPawn* Pawn = SpawnPawnWithCurves_EAC(this, TestWorld, TEXT("CC3"));
        if (!Pawn) { return false; }
        URunStateMachineSubsystem* RSM = GetRSM_EAC(this, TestWorld, TEXT("CC3"));
        if (!RSM) { return false; }

        UPlayerLaneMovementComponent* PM = Pawn->MovementComponent.Get();
        SetRSMRunning_EAC(RSM);

        // Put PM in SETTLED with F-6 mid-tail (identity curve, progress=0.5).
        // Sign convention per AC-F6-A: FarRight+slip-right → sign = -1 (recoil leftward).
        // With identity curve at progress=0.5 → body_t=0.5 → 0.5 * 10 * (-1) = -5.0° body.
        PM->movement_state       = ERunSlipState::SETTLED;
        PM->current_lane         = EPlayerLane::FarRight;
        PM->target_lane          = EPlayerLane::FarRight;
        PM->edge_absorb_active   = true;
        PM->edge_absorb_progress = 0.5f;
        PM->edge_absorb_sign     = -1.0f; // AC-F6-A: right-edge recoil = leftward = -1
        PM->f6_override_fadeout_ticks_remaining = 0;

        // Act — slip-left from FarRight projects to Right (valid). SETTLED→SLIPPING.
        PM->HandleSlipTransition(ESlipDirection::Left);

        // Assert — snapshot captured; active cleared; ticks_remaining=2.
        // Identity curve at progress=0.5 → body_t=0.5 → 0.5 * 10 * (-1) = -5.0° body.
        // Head at 0.4 → 0.4 * 10 * (-1) = -4.0°. Arm at 0.55 → -5.5°.
        TestTrue(TEXT("CC3: snapshot body ≈ -5.0° captured (AC-F6-A recoil)"),
                 FMath::Abs(PM->f6_override_fadeout_snapshot_body - (-5.0f)) <= 0.05f);
        TestTrue(TEXT("CC3: snapshot head ≈ -4.0° captured (staggered, recoil)"),
                 FMath::Abs(PM->f6_override_fadeout_snapshot_head - (-4.0f)) <= 0.05f);
        TestTrue(TEXT("CC3: snapshot arm ≈ -5.5° captured (staggered, recoil)"),
                 FMath::Abs(PM->f6_override_fadeout_snapshot_arm - (-5.5f)) <= 0.05f);
        TestEqual(TEXT("CC3: ticks_remaining == 2 (2-frame fade-out armed)"),
                  PM->f6_override_fadeout_ticks_remaining, 2);
        TestFalse(TEXT("CC3: edge_absorb_active cleared"), PM->edge_absorb_active);
        TestEqual(TEXT("CC3: movement_state == SLIPPING after Override"),
                  static_cast<uint8>(PM->movement_state), static_cast<uint8>(ERunSlipState::SLIPPING));

        TestWorld->DestroyActor(Pawn);
        return true;
    }

    // -----------------------------------------------------------------------
    // CC4 (ac_f6_b_fade_out_no_more_than_50pct_jump) — AC-F6-B.
    //
    // No single tick's F-6 contribution changes by more than 50% of |snapshot|
    // across the 2-frame fade-out ramp.
    //
    // Multiplier ramp: 1.0 → 0.5 → 0. Deltas:
    //   |contribution(tick 1) - contribution(tick 2)| = |1.0 - 0.5| * |snapshot| = 0.5 * |snapshot|
    //   |contribution(tick 2) - contribution(tick 3)| = |0.5 - 0.0| * |snapshot| = 0.5 * |snapshot|
    // Both should be <= 0.5 * |snapshot| (equality is the AC bound).
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("ac_f6_b_fade_out_no_more_than_50pct_jump"))
    {
        // Arrange
        UPlayerLaneMovementComponent* PM = NewObject<UPlayerLaneMovementComponent>();
        PM->EdgeAbsorbCurve       = MakeIdentityCurve_EAC(PM);
        PM->bCurveFallbackActive  = false;
        PM->edge_absorb_active    = false; // base branch = inactive; only fade-out contributes
        PM->f6_override_fadeout_snapshot_body   = -6.0f; // representative mid-tail values
        PM->f6_override_fadeout_snapshot_head   = -4.5f;
        PM->f6_override_fadeout_snapshot_arm    = -5.0f;
        PM->f6_override_fadeout_ticks_remaining = 2;

        const float snap_body_mag = FMath::Abs(PM->f6_override_fadeout_snapshot_body);
        const float snap_head_mag = FMath::Abs(PM->f6_override_fadeout_snapshot_head);
        const float snap_arm_mag  = FMath::Abs(PM->f6_override_fadeout_snapshot_arm);
        const float tolerance     = 0.01f;

        // Act tick 1
        float b1 = 0, h1 = 0, a1 = 0;
        PM->ComputeF6(b1, h1, a1);

        // Act tick 2
        float b2 = 0, h2 = 0, a2 = 0;
        PM->ComputeF6(b2, h2, a2);

        // Act tick 3
        float b3 = 0, h3 = 0, a3 = 0;
        PM->ComputeF6(b3, h3, a3);

        // Assert — deltas across ticks each ≤ 0.5 * |snapshot| + tolerance.
        TestTrue(TEXT("CC4: |b1 - b2| ≤ 0.5 * |snapshot_body|"),
                 FMath::Abs(b1 - b2) <= (0.5f * snap_body_mag) + tolerance);
        TestTrue(TEXT("CC4: |b2 - b3| ≤ 0.5 * |snapshot_body|"),
                 FMath::Abs(b2 - b3) <= (0.5f * snap_body_mag) + tolerance);
        TestTrue(TEXT("CC4: |h1 - h2| ≤ 0.5 * |snapshot_head|"),
                 FMath::Abs(h1 - h2) <= (0.5f * snap_head_mag) + tolerance);
        TestTrue(TEXT("CC4: |h2 - h3| ≤ 0.5 * |snapshot_head|"),
                 FMath::Abs(h2 - h3) <= (0.5f * snap_head_mag) + tolerance);
        TestTrue(TEXT("CC4: |a1 - a2| ≤ 0.5 * |snapshot_arm|"),
                 FMath::Abs(a1 - a2) <= (0.5f * snap_arm_mag) + tolerance);
        TestTrue(TEXT("CC4: |a2 - a3| ≤ 0.5 * |snapshot_arm|"),
                 FMath::Abs(a2 - a3) <= (0.5f * snap_arm_mag) + tolerance);

        return true;
    }

    // -----------------------------------------------------------------------
    // CC5 (ac24_no_on_slip_midpoint_during_f6_tail) — AC-24 exclusion.
    //
    // OnSlipMidpoint fires only on SLIPPING TP crossing 0.5. The F-6 tail runs
    // during SETTLED (edge no-op) or as an additive during SLIPPING. In either
    // case OnSlipMidpoint must NOT fire because of F-6's own progress crossing 0.5.
    //
    // Setup: PM SETTLED at FarRight; trigger F-6; simulate ticks that would
    //        take F-6 progress past 0.5. Spy on OnSlipMidpoint.
    // Assert: Broadcast count == 0.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("ac24_no_on_slip_midpoint_during_f6_tail"))
    {
        // Arrange
        UWorld* TestWorld = CreateTestPlayWorld_EAC(this, TEXT("CC5"));
        if (!TestWorld) { return false; }
        ASlipstormPlayerPawn* Pawn = SpawnPawnWithCurves_EAC(this, TestWorld, TEXT("CC5"));
        if (!Pawn) { return false; }
        URunStateMachineSubsystem* RSM = GetRSM_EAC(this, TestWorld, TEXT("CC5"));
        if (!RSM) { return false; }

        UPlayerLaneMovementComponent* PM = Pawn->MovementComponent.Get();
        SetRSMRunning_EAC(RSM);

        // OnSlipMidpoint spy.
        int32 MidpointBroadcasts = 0;
        FDelegateHandle SpyHandle = PM->OnSlipMidpoint.AddLambda(
            [&MidpointBroadcasts](EPlayerLane, EPlayerLane) { ++MidpointBroadcasts; });

        // Trigger edge no-op (SETTLED at FarRight + slip-right → F-6 fires).
        PM->movement_state = ERunSlipState::SETTLED;
        PM->current_lane   = EPlayerLane::FarRight;
        PM->target_lane    = EPlayerLane::FarRight;
        PM->HandleSlipTransition(ESlipDirection::Right);

        // Act — advance ticks so F-6 progress goes 0 → past 0.5 → 1.0.
        // EDGE_ABSORB_DURATION_S = 0.27s; each 0.016s tick advances progress by ~0.06.
        // 10 ticks × 0.016 = 0.16s → progress ≈ 0.59 (past 0.5).
        FApp::SetDeltaTime(0.016);
        for (int32 i = 0; i < 10; ++i)
        {
            PM->TickComponent(0.016f, ELevelTick::LEVELTICK_All, nullptr);
        }

        // Assert — OnSlipMidpoint never fired.
        TestEqual(TEXT("CC5: OnSlipMidpoint NEVER broadcast during F-6 tail (AC-24 exclusion)"),
                  MidpointBroadcasts, 0);

        PM->OnSlipMidpoint.Remove(SpyHandle);
        TestWorld->DestroyActor(Pawn);
        return true;
    }

    // -----------------------------------------------------------------------
    // CC6 (ac_f6_a_farright_slipright_mid_tail_all_negative) — AC-F6-A explicit.
    //
    // Story text: "SETTLED @ FarRight + slip-right → F-6 fires, mid-tail
    // body/head/arm all negative and non-zero (verifies head + arm decoupling,
    // §5.1(a) closes decouple defect)."
    //
    // Setup: SETTLED at FarRight; HandleSlipTransition(Right) → edge no-op fires
    // F-6 with sign=-1 (per AC-F6-A recoil convention). Advance F-6 progress via
    // multiple TickComponent calls until edge_absorb_progress ≈ 0.5 (mid-tail).
    // Call ComputeF6 and assert body/head/arm all strictly less than 0.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("ac_f6_a_farright_slipright_mid_tail_all_negative"))
    {
        // Arrange
        UWorld* TestWorld = CreateTestPlayWorld_EAC(this, TEXT("CC6"));
        if (!TestWorld) { return false; }
        ASlipstormPlayerPawn* Pawn = SpawnPawnWithCurves_EAC(this, TestWorld, TEXT("CC6"));
        if (!Pawn) { return false; }
        URunStateMachineSubsystem* RSM = GetRSM_EAC(this, TestWorld, TEXT("CC6"));
        if (!RSM) { return false; }

        UPlayerLaneMovementComponent* PM = Pawn->MovementComponent.Get();
        SetRSMRunning_EAC(RSM);

        PM->movement_state = ERunSlipState::SETTLED;
        PM->current_lane   = EPlayerLane::FarRight;
        PM->target_lane    = EPlayerLane::FarRight;

        // Act 1 — fire F-6 via HandleSlipTransition (edge no-op path).
        PM->HandleSlipTransition(ESlipDirection::Right);
        TestTrue(TEXT("CC6: edge_absorb_active after trigger"), PM->edge_absorb_active);
        TestEqual(TEXT("CC6: sign == -1 (AC-F6-A recoil)"), PM->edge_absorb_sign, -1.0f);

        // Act 2 — advance to mid-tail. EDGE_ABSORB_DURATION_S = 0.27s.
        // Target: progress ≈ 0.5 → time ≈ 0.135s → 8 ticks at 0.016s = 0.128s → progress ≈ 0.474.
        FApp::SetDeltaTime(0.016);
        for (int32 i = 0; i < 8; ++i)
        {
            PM->TickComponent(0.016f, ELevelTick::LEVELTICK_All, nullptr);
        }
        TestTrue(TEXT("CC6: edge_absorb_progress ≈ 0.474 (mid-tail range)"),
                 PM->edge_absorb_progress > 0.4f && PM->edge_absorb_progress < 0.55f);
        TestTrue(TEXT("CC6: edge_absorb_active still true at mid-tail"), PM->edge_absorb_active);

        // Act 3 — direct ComputeF6 call to inspect body/head/arm signs.
        // Note: TickComponent's co-write has already called ComputeF6 this frame,
        // but ComputeF6 is a query (no state mutation for the active branch).
        // Calling again with the same progress yields the same outputs.
        float body = 0.0f, head = 0.0f, arm = 0.0f;
        PM->ComputeF6(body, head, arm);

        // Assert — all three components strictly negative (recoil leftward from right wall).
        // With identity curve and sign=-1, body ≈ -progress * 10; head/arm staggered.
        TestTrue(TEXT("CC6: body < 0 (recoil left, AC-F6-A)"), body < 0.0f);
        TestTrue(TEXT("CC6: head < 0 (staggered, non-zero decouple)"), head < 0.0f);
        TestTrue(TEXT("CC6: arm < 0 (staggered, non-zero decouple)"),  arm  < 0.0f);
        // Head + arm decouple non-trivial: not the same as body, verifying stagger works.
        TestTrue(TEXT("CC6: head ≠ body (stagger active — decouple closed)"),
                 !FMath::IsNearlyEqual(head, body, 0.01f));
        TestTrue(TEXT("CC6: arm ≠ body (stagger active — decouple closed)"),
                 !FMath::IsNearlyEqual(arm, body, 0.01f));

        TestWorld->DestroyActor(Pawn);
        return true;
    }

    // -----------------------------------------------------------------------
    // CC7 (ac_f6_c_farleft_slipleft_fires_f6) — AC-F6-C positive case for left edge.
    //
    // Verifies that FarLeft + slip-left also fires F-6 (symmetric to CC1 for
    // the left boundary), with sign=+1 (recoil rightward AWAY from left wall).
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("ac_f6_c_farleft_slipleft_fires_f6"))
    {
        // Arrange
        UWorld* TestWorld = CreateTestPlayWorld_EAC(this, TEXT("CC7"));
        if (!TestWorld) { return false; }
        ASlipstormPlayerPawn* Pawn = SpawnPawnWithCurves_EAC(this, TestWorld, TEXT("CC7"));
        if (!Pawn) { return false; }
        URunStateMachineSubsystem* RSM = GetRSM_EAC(this, TestWorld, TEXT("CC7"));
        if (!RSM) { return false; }

        UPlayerLaneMovementComponent* PM = Pawn->MovementComponent.Get();
        SetRSMRunning_EAC(RSM);

        PM->movement_state = ERunSlipState::SETTLED;
        PM->current_lane   = EPlayerLane::FarLeft;
        PM->target_lane    = EPlayerLane::FarLeft;
        const int32 CounterBefore = PM->edge_absorb_trigger_count;

        // Act — slip-left at FarLeft is off-track → edge no-op path fires F-6.
        PM->HandleSlipTransition(ESlipDirection::Left);

        // Assert
        TestEqual(TEXT("CC7: movement_state stays SETTLED"),
                  static_cast<uint8>(PM->movement_state), static_cast<uint8>(ERunSlipState::SETTLED));
        TestTrue(TEXT("CC7: edge_absorb_active == true (F-6 fired at left edge)"),
                 PM->edge_absorb_active);
        TestEqual(TEXT("CC7: edge_absorb_sign == +1 (recoil rightward AWAY from left wall)"),
                  PM->edge_absorb_sign, +1.0f);
        TestEqual(TEXT("CC7: edge_absorb_trigger_count += 1"),
                  PM->edge_absorb_trigger_count, CounterBefore + 1);

        TestWorld->DestroyActor(Pawn);
        return true;
    }

    AddError(FString::Printf(
        TEXT("FPMEdgeAbsorbCompositionTest::RunTest — unknown Parameters: '%s'"),
        *Parameters));
    return false;
}

#endif // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

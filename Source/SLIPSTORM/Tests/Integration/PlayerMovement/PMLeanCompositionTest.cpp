// Copyright (c) Game Studio. All rights reserved.
// SLIPSTORM — 60-second voxel endless runner, mobile, Unreal Engine 5.7
//
// PMLeanCompositionTest.cpp — Story 006 composition/integration tests for F-5.
//
// Location: Integration/ rather than Unit/ because the tests require a real
// UWorld + BeginPlay to exercise the TickComponent SETTLED else-branch write
// (AC-26 / AC-30) and the SLIPPING F-5 tick-site write (Roll-axis regression
// guard). Pure-math ComputeLean tests live in:
//   Source/SLIPSTORM/Tests/Unit/PlayerMovement/PMLeanTest.cpp
//
// Spec:   production/epics/player-movement/story-006-f5-lean.md
// GDD:    design/gdd/player-movement-mechanics.md §4 F-5, §8 AC-26 / AC-27 / AC-30
// ADR:    docs/architecture/adr-0009-player-movement-hosting.md (SD5, SD6, IG-6)
// TR:     TR-PM-008, TR-PM-029, TR-PM-033
//
// Test category: SLIPSTORM.PlayerMovement.LeanComposition
// Runner: UE Automation Framework, ClientContext | ProductFilter.
//
// Helpers `_LC`-suffixed to avoid ODR collisions with PMStateMachineTest
// / PMLateralInterpolationCompositionTest / PMInputBufferTest helpers.
//
// AAA (Arrange / Act / Assert) labels present per test-standards.md convention
// established by Story 005 qa-tester review.
//
// Build guard: WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS.

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/AutomationCommon.h"        // FTestWorldWrapper (S1-04 harness fix)
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Curves/CurveFloat.h"
#include "Misc/App.h"
#include "Components/StaticMeshComponent.h"

#include "Player/PlayerLaneMovementComponent.h"
#include "Player/SlipstormPlayerPawn.h"
#include "Player/EPlayerLane.h"
#include "Player/ERunSlipState.h"
#include "RunStateMachine/RunStateMachineSubsystem.h"
#include "RunStateMachine/ERunState.h"

// ---------------------------------------------------------------------------
// Helpers (duplicated from PMStateMachineTest.cpp pattern — Story 006 scope
// consistent with Stories 004 / 005 helpers).
// ---------------------------------------------------------------------------

static UCurveFloat* MakeValidCurve_LC(UObject* InOuter)
{
    UCurveFloat* C = NewObject<UCurveFloat>(InOuter);
    C->FloatCurve.AddKey(0.0f, 0.0f);
    C->FloatCurve.AddKey(1.0f, 1.0f);
    return C;
}

// S1-04 harness fix: FTestWorldWrapper canonical UE pattern (Engine/Source/Runtime/Engine/Public/Tests/AutomationCommon.h).
// See PMStateMachineTest.cpp CreateTestPlayWorld_SM for full rationale + engine citation.
// Wrapper is stack-allocated per TC; destructor handles all teardown (GI Shutdown + DestroyWorldContext).
static UWorld* CreateTestPlayWorld_LC(FAutomationTestBase* T, FTestWorldWrapper& WorldWrapper, const TCHAR* Label)
{
    if (!WorldWrapper.CreateTestWorld(EWorldType::Game))
    {
        T->AddError(FString::Printf(TEXT("%s: FTestWorldWrapper::CreateTestWorld failed"), Label));
        return nullptr;
    }
    if (!WorldWrapper.BeginPlayInTestWorld())
    {
        T->AddError(FString::Printf(TEXT("%s: FTestWorldWrapper::BeginPlayInTestWorld failed"), Label));
        return nullptr;
    }
    return WorldWrapper.GetTestWorld();
}

static ASlipstormPlayerPawn* SpawnPawnWithCurves_LC(
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
    Pawn->MovementComponent->SlipCurve       = MakeValidCurve_LC(World);
    Pawn->MovementComponent->LeanCurve       = MakeValidCurve_LC(World);
    Pawn->MovementComponent->EdgeAbsorbCurve = MakeValidCurve_LC(World);
    UGameplayStatics::FinishSpawningActor(Pawn, FTransform::Identity);
    return Pawn;
}

static URunStateMachineSubsystem* GetRSM_LC(
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

static void SetRSMRunning_LC(URunStateMachineSubsystem* RSM)
{
    RSM->TestOnly_CurrentState = ERunState::RUNNING;
    RSM->TestOnly_bPaused      = false;
    RSM->TestOnly_bResumeGrace = false;
}

// ---------------------------------------------------------------------------
// IMPLEMENT_COMPLEX_AUTOMATION_TEST — 3 composition test commands.
// ---------------------------------------------------------------------------

IMPLEMENT_COMPLEX_AUTOMATION_TEST(
    FPMLeanCompositionTest,
    "SLIPSTORM.PlayerMovement.LeanComposition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

void FPMLeanCompositionTest::GetTests(
    TArray<FString>& OutBeautifiedNames,
    TArray<FString>& OutTestCommands) const
{
    OutBeautifiedNames.Add(
        TEXT("AC-26 SETTLED + no F-6 → lean_angle / head_lean_angle / arm_lean_angle all zero after tick"));
    OutTestCommands.Add(TEXT("ac26_settled_zero_lean_after_tick"));

    OutBeautifiedNames.Add(
        TEXT("AC-30 SETTLED at Right + F-6 inactive → arm_lean_angle == 0 after tick (explicit)"));
    OutTestCommands.Add(TEXT("ac30_settled_arm_lean_zero_after_tick"));

    OutBeautifiedNames.Add(
        TEXT("F-5 Roll-axis regression guard — SetRelativeRotation writes ROLL (not Pitch/Yaw) during SLIPPING"));
    OutTestCommands.Add(TEXT("f5_roll_axis_regression_guard"));
}

bool FPMLeanCompositionTest::RunTest(const FString& Parameters)
{
    // -----------------------------------------------------------------------
    // CC1 (ac26_settled_zero_lean_after_tick) — AC-26 gate.
    //
    // Setup: RSM = RUNNING. movement_state = SETTLED, current_lane = Center,
    //        target_lane = Center. Sentinel-pre-fill lean_angle / head_lean_angle
    //        / arm_lean_angle with 99.0f so a silent no-write bug is caught.
    // Action: TickComponent once (Rule 5 gate passes; SETTLED else-branch runs).
    // Assert: all three public lean angles ≤ 0.01f after tick.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("ac26_settled_zero_lean_after_tick"))
    {
        // Arrange
        FTestWorldWrapper WorldWrapper;
        UWorld* TestWorld = CreateTestPlayWorld_LC(this, WorldWrapper, TEXT("CC1"));
        if (!TestWorld) { return false; }
        ASlipstormPlayerPawn* Pawn = SpawnPawnWithCurves_LC(this, TestWorld, TEXT("CC1"));
        if (!Pawn) { return false; }
        URunStateMachineSubsystem* RSM = GetRSM_LC(this, TestWorld, TEXT("CC1"));
        if (!RSM) { return false; }

        UPlayerLaneMovementComponent* PM = Pawn->MovementComponent.Get();
        SetRSMRunning_LC(RSM);

        PM->movement_state    = ERunSlipState::SETTLED;
        PM->current_lane      = EPlayerLane::Center;
        PM->target_lane       = EPlayerLane::Center;
        PM->tween_progress    = 0.0f;
        // Sentinel: any residual would leak past the SETTLED zero write.
        PM->lean_angle        = 99.0f;
        PM->head_lean_angle   = 99.0f;
        PM->arm_lean_angle    = 99.0f;

        // Act
        FApp::SetDeltaTime(0.016);
        PM->TickComponent(0.016f, ELevelTick::LEVELTICK_All, nullptr);

        // Assert — AC-26 threshold is ≤ 0.01° per story text.
        TestTrue(TEXT("CC1: lean_angle ≤ 0.01° after SETTLED tick (AC-26)"),
                 FMath::Abs(PM->lean_angle) <= 0.01f);
        TestTrue(TEXT("CC1: head_lean_angle ≤ 0.01° after SETTLED tick (AC-26)"),
                 FMath::Abs(PM->head_lean_angle) <= 0.01f);
        TestTrue(TEXT("CC1: arm_lean_angle ≤ 0.01° after SETTLED tick (AC-26)"),
                 FMath::Abs(PM->arm_lean_angle) <= 0.01f);

        // Sentinel guard: prove the tick actually wrote (not that 99.0 leaked as 0.0).
        TestTrue(TEXT("CC1: SETTLED write did overwrite sentinel (not silent skip)"),
                 !FMath::IsNearlyEqual(PM->lean_angle, 99.0f, KINDA_SMALL_NUMBER));

        TestWorld->DestroyActor(Pawn);
        return true;
    }

    // -----------------------------------------------------------------------
    // CC2 (ac30_settled_arm_lean_zero_after_tick) — AC-30 explicit arm gate.
    //
    // Story text: "AC-30 (SETTLED + F-6 inactive → arm_lean_angle ≤ 0.01°):
    //              independent verification for arm."
    // Same shape as CC1 but with current_lane = Right and focused arm assertion.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("ac30_settled_arm_lean_zero_after_tick"))
    {
        // Arrange
        FTestWorldWrapper WorldWrapper;
        UWorld* TestWorld = CreateTestPlayWorld_LC(this, WorldWrapper, TEXT("CC2"));
        if (!TestWorld) { return false; }
        ASlipstormPlayerPawn* Pawn = SpawnPawnWithCurves_LC(this, TestWorld, TEXT("CC2"));
        if (!Pawn) { return false; }
        URunStateMachineSubsystem* RSM = GetRSM_LC(this, TestWorld, TEXT("CC2"));
        if (!RSM) { return false; }

        UPlayerLaneMovementComponent* PM = Pawn->MovementComponent.Get();
        SetRSMRunning_LC(RSM);

        PM->movement_state  = ERunSlipState::SETTLED;
        PM->current_lane    = EPlayerLane::Right;
        PM->target_lane     = EPlayerLane::Right;
        PM->tween_progress  = 0.0f;
        PM->arm_lean_angle  = 99.0f; // sentinel

        // Act
        FApp::SetDeltaTime(0.016);
        PM->TickComponent(0.016f, ELevelTick::LEVELTICK_All, nullptr);

        // Assert — AC-30 threshold ≤ 0.01°.
        TestTrue(TEXT("CC2: arm_lean_angle ≤ 0.01° after SETTLED tick at Right (AC-30)"),
                 FMath::Abs(PM->arm_lean_angle) <= 0.01f);
        TestTrue(TEXT("CC2: SETTLED write did overwrite arm sentinel"),
                 !FMath::IsNearlyEqual(PM->arm_lean_angle, 99.0f, KINDA_SMALL_NUMBER));

        TestWorld->DestroyActor(Pawn);
        return true;
    }

    // -----------------------------------------------------------------------
    // CC3 (f5_roll_axis_regression_guard) — Story 006 /code-review found the
    // FRotator axis was originally Pitch instead of Roll. This test asserts
    // that a SLIPPING tick with a curve returning body_lean > 0 writes ROLL
    // (not Pitch or Yaw) on the mesh component.
    //
    // Without this test, a future refactor could silently re-introduce the
    // Pitch-vs-Roll bug and the unit-only test suite would not catch it.
    //
    // Setup: identity SlipCurve/LeanCurve for reasonable interior values.
    //        SLIPPING Left→Center (sign = +1), tween_progress = 0.5.
    //        Expected body_lean ≈ 0.5 * 10 * (+1) = +5.0° (Roll).
    // Assert: MeshComponent->GetRelativeRotation() has
    //         Roll ≈ body_lean, Pitch ≈ 0, Yaw ≈ 0.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("f5_roll_axis_regression_guard"))
    {
        // Arrange
        FTestWorldWrapper WorldWrapper;
        UWorld* TestWorld = CreateTestPlayWorld_LC(this, WorldWrapper, TEXT("CC3"));
        if (!TestWorld) { return false; }
        ASlipstormPlayerPawn* Pawn = SpawnPawnWithCurves_LC(this, TestWorld, TEXT("CC3"));
        if (!Pawn) { return false; }
        URunStateMachineSubsystem* RSM = GetRSM_LC(this, TestWorld, TEXT("CC3"));
        if (!RSM) { return false; }

        UPlayerLaneMovementComponent* PM = Pawn->MovementComponent.Get();
        SetRSMRunning_LC(RSM);

        PM->current_lane      = EPlayerLane::Left;    // source
        PM->target_lane       = EPlayerLane::Center;  // destination (sign = +1)
        PM->movement_state    = ERunSlipState::SLIPPING;
        PM->tween_progress    = 0.5f;
        PM->bCurveFallbackActive = false;

        // Act
        FApp::SetDeltaTime(0.0); // dt=0 → F-2 leaves tween_progress unchanged; F-5 runs at 0.5.
        PM->TickComponent(0.0f, ELevelTick::LEVELTICK_All, nullptr);

        // Assert — Roll carries the body lean; Pitch and Yaw are zero.
        const FRotator MeshRot = Pawn->MeshComponent->GetRelativeRotation();
        TestTrue(TEXT("CC3: mesh Pitch ≈ 0° (lean is NOT Pitch — regression guard)"),
                 FMath::Abs(MeshRot.Pitch) < 0.01f);
        TestTrue(TEXT("CC3: mesh Yaw ≈ 0° (lean is NOT Yaw)"),
                 FMath::Abs(MeshRot.Yaw) < 0.01f);
        // Roll should be non-zero and match the published lean_angle.
        TestTrue(TEXT("CC3: mesh Roll matches PM->lean_angle (Roll carries the F-5 write)"),
                 FMath::IsNearlyEqual(MeshRot.Roll, PM->lean_angle, 0.01f));
        TestTrue(TEXT("CC3: PM->lean_angle > 0 for Left→Center at TP=0.5 (positive sign, identity curve)"),
                 PM->lean_angle > 0.0f);

        TestWorld->DestroyActor(Pawn);
        return true;
    }

    AddError(FString::Printf(
        TEXT("FPMLeanCompositionTest::RunTest — unknown Parameters: '%s'"),
        *Parameters));
    return false;
}

#endif // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

// Copyright (c) Game Studio. All rights reserved.
// SLIPSTORM — 60-second voxel endless runner, mobile, Unreal Engine 5.7
//
// PMLateralInterpolationCompositionTest.cpp — Story 004 composition/integration
// tests for F-3 lateral interpolation.
//
// Tests here require a real UWorld with BeginPlay invoked so that
// CachedMeshComponent is resolved and RSM subsystem bindings are live.
// Pure-math F3RelativeOffset tests live in:
//   Source/SLIPSTORM/Tests/Unit/PlayerMovement/PMLateralInterpolationTest.cpp
//
// Spec:   tests/unit/player-movement/pm-lateral-interpolation-spec.md
// GDD:    design/gdd/player-movement-mechanics.md §4 F-3, §8 AC-03
// ADR:    docs/architecture/adr-0009-player-movement-hosting.md (SD5, SD6, IG-5,
//         Engine Compat Verification #4)
// Story:  production/epics/player-movement/story-004-f3-lateral-interpolation.md
//
// Test category: SLIPSTORM.PlayerMovement.LateralInterpolationComposition
// Runner: UE Automation Framework, ClientContext | ProductFilter.
//
// Uses CreateTestPlayWorld / SpawnPawnWithCurves / SetRSMRunning helpers
// duplicated from PMStateMachineTest.cpp (not refactored into a shared header
// per Story 004 scope constraint).
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
#include "Player/ESlipDirection.h"
#include "RunStateMachine/RunStateMachineSubsystem.h"
#include "RunStateMachine/ERunState.h"
#include "RunStateMachine/ERunOutcome.h"

// ---------------------------------------------------------------------------
// Local helpers (duplicated from PMStateMachineTest.cpp — Story 004 scope
// constraint: no shared-header refactor).
// ---------------------------------------------------------------------------

static UCurveFloat* MakeValidCurve_LI(UObject* InOuter)
{
    UCurveFloat* C = NewObject<UCurveFloat>(InOuter);
    C->FloatCurve.AddKey(0.0f, 0.0f);
    C->FloatCurve.AddKey(1.0f, 1.0f);
    return C;
}

// S1-04 harness fix: FTestWorldWrapper canonical UE pattern (Engine/Source/Runtime/Engine/Public/Tests/AutomationCommon.h).
// See PMStateMachineTest.cpp CreateTestPlayWorld_SM for full rationale + engine citation.
// Wrapper is stack-allocated per TC; destructor handles all teardown (GI Shutdown + DestroyWorldContext).
static UWorld* CreateTestPlayWorld_LI(FAutomationTestBase* T, FTestWorldWrapper& WorldWrapper, const TCHAR* Label)
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

static ASlipstormPlayerPawn* SpawnPawnWithCurves_LI(
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
    // All three curve assets required to pass BeginPlay validation and avoid
    // the fallback path for the SlipCurve-dependent test cases.
    Pawn->MovementComponent->SlipCurve       = MakeValidCurve_LI(World);
    Pawn->MovementComponent->LeanCurve       = MakeValidCurve_LI(World);
    Pawn->MovementComponent->EdgeAbsorbCurve = MakeValidCurve_LI(World);
    UGameplayStatics::FinishSpawningActor(Pawn, FTransform::Identity);
    return Pawn;
}

static URunStateMachineSubsystem* GetRSM_LI(
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

static void SetRSMRunning_LI(URunStateMachineSubsystem* RSM)
{
    RSM->TestOnly_CurrentState                       = ERunState::RUNNING;
    RSM->TestOnly_bPaused                            = false;
    RSM->TestOnly_bResumeGrace                       = false;
    RSM->TestOnly_ForceTickNowCallCount              = 0;
    RSM->TestOnly_GetCurrentStateCallCount           = 0;
    RSM->TestOnly_GetCurrentStateCountAtForceTickNow = -1;
}

// ---------------------------------------------------------------------------
// IMPLEMENT_COMPLEX_AUTOMATION_TEST — 2 composition test commands.
// ---------------------------------------------------------------------------

IMPLEMENT_COMPLEX_AUTOMATION_TEST(
    FPMLateralInterpolationCompositionTest,
    "SLIPSTORM.PlayerMovement.LateralInterpolationComposition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

void FPMLateralInterpolationCompositionTest::GetTests(
    TArray<FString>& OutBeautifiedNames,
    TArray<FString>& OutTestCommands) const
{
    OutBeautifiedNames.Add(
        TEXT("F-3 composition invariant — mesh world X == root world X + rel_x (ADR-0009 Engine Compat #4)"));
    OutTestCommands.Add(TEXT("f3_composition_invariant"));

    OutBeautifiedNames.Add(
        TEXT("lateral_world_position SETTLED — TickComponent writes LaneWorldX(current_lane)"));
    OutTestCommands.Add(TEXT("lateral_world_position_settled"));

    OutBeautifiedNames.Add(
        TEXT("lateral_world_position SLIPPING — TickComponent writes root.X + rel_x (AC-06 end-to-end)"));
    OutTestCommands.Add(TEXT("lateral_world_position_slipping_write"));
}

bool FPMLateralInterpolationCompositionTest::RunTest(const FString& Parameters)
{
    // -----------------------------------------------------------------------
    // CC1 (f3_composition_invariant) — proves ADR-0009 Engine Compat
    // Verification #4: mesh world position == root world position + relative offset.
    //
    // Setup:
    //   Spawn pawn (BeginPlay runs → CachedMeshComponent resolved).
    //   RSM = RUNNING.
    //   current_lane = Left  → LaneWorldX(Left)   = -100
    //   target_lane  = Center → LaneWorldX(Center) =    0
    //   movement_state = SLIPPING, tween_progress = 0.5
    //   SlipCurve = identity → rel_x = F3RelativeOffset(0.5) = -50.
    //
    // Direct verification (bypasses full TickComponent for isolation):
    //   1. Assert F3RelativeOffset(0.5) == -50 via friend access.
    //   2. Call CachedMeshComponent->SetRelativeLocation(FVector(-50, 0, 0))
    //      directly to confirm the invariant without depending on
    //      TickComponent dispatching (which is covered by AC-01 in SM test).
    //   3. Read MeshWorldX = CachedMeshComponent->GetComponentLocation().X
    //      and RootWorldX = Pawn->GetActorLocation().X.
    //   4. Assert |MeshWorldX - (RootWorldX + (-50))| < 0.01.
    //
    // Why direct SetRelativeLocation? TickComponent's F-3 site is
    // already proven correct by the unit test; this composition test proves
    // the MESH WORLD POSITION formula (root + relative = world) is correct in
    // the actual UE scene graph — i.e., CachedMeshComponent->SetRelativeLocation
    // does what we expect relative to the pawn root (Engine Compat #4).
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("f3_composition_invariant"))
    {
        FTestWorldWrapper WorldWrapper;
        UWorld* TestWorld = CreateTestPlayWorld_LI(this, WorldWrapper, TEXT("CC1"));
        if (!TestWorld) { return false; }
        ASlipstormPlayerPawn* Pawn = SpawnPawnWithCurves_LI(this, TestWorld, TEXT("CC1"));
        if (!Pawn) { return false; }
        URunStateMachineSubsystem* RSM = GetRSM_LI(this, TestWorld, TEXT("CC1"));
        if (!RSM) { return false; }

        UPlayerLaneMovementComponent* PM = Pawn->MovementComponent.Get();
        SetRSMRunning_LI(RSM);

        // Verify CachedMeshComponent was resolved during BeginPlay.
        if (!TestNotNull(TEXT("CC1: CachedMeshComponent resolved at BeginPlay"), PM->CachedMeshComponent.Get()))
        {
            TestWorld->DestroyActor(Pawn);
            return false;
        }

        // Arrange SLIPPING state.
        PM->current_lane         = EPlayerLane::Left;
        PM->target_lane          = EPlayerLane::Center;
        PM->movement_state       = ERunSlipState::SLIPPING;
        PM->tween_progress       = 0.5f;
        PM->bCurveFallbackActive = false;
        // SlipCurve already set to identity by SpawnPawnWithCurves_LI.

        // Step 1: verify F3RelativeOffset(0.5) == -50 (pure-math half).
        const float rel_x = PM->F3RelativeOffset(0.5f);
        TestTrue(
            TEXT("CC1: F3RelativeOffset(0.5) == -50.0f ± 0.001 (Left→Center, identity curve)"),
            FMath::IsNearlyEqual(rel_x, -50.0f, 0.001f));

        // Step 2: apply the relative offset directly to isolate the scene-graph invariant.
        PM->CachedMeshComponent->SetRelativeLocation(FVector(rel_x, 0.0f, 0.0f));

        // Step 3: read world positions.
        const float MeshWorldX = PM->CachedMeshComponent->GetComponentLocation().X;
        const float RootWorldX = Pawn->GetActorLocation().X;

        // Step 4: assert ADR-0009 Engine Compat Verification #4 invariant.
        // mesh world X == root world X + rel_x (within floating-point tolerance).
        const float Expected = RootWorldX + rel_x;
        TestTrue(
            TEXT("CC1: |MeshWorldX - (RootWorldX + rel_x)| < 0.01 (Engine Compat #4: root+relative==world)"),
            FMath::Abs(MeshWorldX - Expected) < 0.01f);

        TestWorld->DestroyActor(Pawn);
        return true;
    }

    // -----------------------------------------------------------------------
    // CC2 (lateral_world_position_settled) — when movement_state == SETTLED,
    // a single TickComponent call (passing the Rule 5 gate) writes
    //   lateral_world_position = LaneWorldX(current_lane)
    // which for current_lane = Right is +100.0f.
    //
    // Setup:
    //   RSM = RUNNING (Rule 5 gate passes).
    //   movement_state = SETTLED.
    //   current_lane   = Right → LaneWorldX(Right) = +100.
    //
    // Verify: after TickComponent, lateral_world_position == 100.0f.
    //
    // This test lives in the composition file (not unit) per Q3 decision:
    // it requires a real BeginPlay-initialised PM to test the SETTLED else-branch
    // that lives inside the Rule 5 gate in TickComponent.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("lateral_world_position_settled"))
    {
        FTestWorldWrapper WorldWrapper;
        UWorld* TestWorld = CreateTestPlayWorld_LI(this, WorldWrapper, TEXT("CC2"));
        if (!TestWorld) { return false; }
        ASlipstormPlayerPawn* Pawn = SpawnPawnWithCurves_LI(this, TestWorld, TEXT("CC2"));
        if (!Pawn) { return false; }
        URunStateMachineSubsystem* RSM = GetRSM_LI(this, TestWorld, TEXT("CC2"));
        if (!RSM) { return false; }

        UPlayerLaneMovementComponent* PM = Pawn->MovementComponent.Get();
        SetRSMRunning_LI(RSM);

        // Arrange SETTLED state at Right lane.
        PM->movement_state       = ERunSlipState::SETTLED;
        PM->current_lane         = EPlayerLane::Right;
        PM->target_lane          = EPlayerLane::Right; // target == current for SETTLED
        PM->tween_progress       = 0.0f;
        PM->lateral_world_position = 0.0f; // explicit sentinel to confirm the write

        // Tick once with a nominal frame time. Set FApp::GetDeltaTime() first to
        // avoid stale-clock nondeterminism (same pattern as PMStateMachineTest).
        FApp::SetDeltaTime(0.016);
        PM->TickComponent(0.016f, ELevelTick::LEVELTICK_All, nullptr);

        // LaneWorldX(Right) == (3-2)*100 == +100.
        TestEqual(
            TEXT("CC2: lateral_world_position == 100.0f when SETTLED at Right lane"),
            PM->lateral_world_position,
            100.0f);

        TestWorld->DestroyActor(Pawn);
        return true;
    }

    // -----------------------------------------------------------------------
    // CC3 (lateral_world_position_slipping_write) — closes the AC6 SLIPPING
    // end-to-end assertion gap identified by qa-tester Story 004 code review.
    //
    // Ticks once with SLIPPING state; asserts that after the tick,
    //   PM->lateral_world_position == RootWorldX + F3RelativeOffset(TP_post)
    // where TP_post is the post-tick tween_progress (F-2 advanced by dt).
    //
    // Uses the impl's own F3RelativeOffset to compute expected — proves the
    // composition without duplicating F-3 math in the test. A sentinel guard
    // catches the case where TickComponent skipped the F-3 write (e.g., if
    // CachedMeshComponent was null and the branch at cpp:214 didn't run).
    //
    // Setup:
    //   RSM = RUNNING (Rule 5 gate passes).
    //   movement_state = SLIPPING.
    //   current_lane = Left, target_lane = Center.
    //   tween_progress = 0.4 (pre-advance; F-2 will push it forward).
    //   SlipCurve = identity (from SpawnPawnWithCurves_LI), bCurveFallbackActive = false.
    //   lateral_world_position = 0.0 (sentinel).
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("lateral_world_position_slipping_write"))
    {
        FTestWorldWrapper WorldWrapper;
        UWorld* TestWorld = CreateTestPlayWorld_LI(this, WorldWrapper, TEXT("CC3"));
        if (!TestWorld) { return false; }
        ASlipstormPlayerPawn* Pawn = SpawnPawnWithCurves_LI(this, TestWorld, TEXT("CC3"));
        if (!Pawn) { return false; }
        URunStateMachineSubsystem* RSM = GetRSM_LI(this, TestWorld, TEXT("CC3"));
        if (!RSM) { return false; }

        UPlayerLaneMovementComponent* PM = Pawn->MovementComponent.Get();
        SetRSMRunning_LI(RSM);

        // Verify CachedMeshComponent was resolved during BeginPlay — otherwise
        // the F-3 write path (cpp:214-219) is skipped and the assertion is meaningless.
        if (!TestNotNull(TEXT("CC3: CachedMeshComponent resolved at BeginPlay"), PM->CachedMeshComponent.Get()))
        {
            TestWorld->DestroyActor(Pawn);
            return false;
        }

        // Arrange SLIPPING state — Left→Center at TP=0.4 (interior; will advance).
        PM->current_lane           = EPlayerLane::Left;
        PM->target_lane            = EPlayerLane::Center;
        PM->movement_state         = ERunSlipState::SLIPPING;
        PM->tween_progress         = 0.4f;
        PM->bCurveFallbackActive   = false;
        PM->lateral_world_position = 0.0f; // sentinel — tick must overwrite

        // Tick with a nominal DT (matches PMStateMachineTest / CC2 pattern).
        FApp::SetDeltaTime(0.016);
        PM->TickComponent(0.016f, ELevelTick::LEVELTICK_All, nullptr);

        // Expected: lateral_world_position == RootWorldX + F3RelativeOffset(TP_post).
        // Use the impl's own F3RelativeOffset to avoid duplicating F-3 math.
        const float RootWorldX  = Pawn->GetActorLocation().X;
        const float ExpectedRel = PM->F3RelativeOffset(PM->tween_progress);
        const float Expected    = RootWorldX + ExpectedRel;

        TestTrue(
            TEXT("CC3: lateral_world_position == RootWorldX + F3RelativeOffset(tween_progress) after SLIPPING tick"),
            FMath::IsNearlyEqual(PM->lateral_world_position, Expected, 0.01f));

        // Sentinel guard: confirm the tick actually WROTE the field (not left at 0).
        // At TP≈0.5 Left→Center, expected rel_x ≈ -50, so lateral_world_position
        // must have moved off the 0.0 sentinel unless the F-3 branch was skipped.
        TestTrue(
            TEXT("CC3: lateral_world_position was written by TickComponent (sentinel 0.0 overwritten)"),
            !FMath::IsNearlyEqual(PM->lateral_world_position, 0.0f, KINDA_SMALL_NUMBER));

        TestWorld->DestroyActor(Pawn);
        return true;
    }

    AddError(FString::Printf(
        TEXT("FPMLateralInterpolationCompositionTest::RunTest — unknown Parameters: '%s'"),
        *Parameters));
    return false;
}

#endif // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

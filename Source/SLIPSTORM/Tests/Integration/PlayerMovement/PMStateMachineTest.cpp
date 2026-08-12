// Copyright (c) Game Studio. All rights reserved.
// SLIPSTORM — 60-second voxel endless runner, mobile, Unreal Engine 5.7
//
// PMStateMachineTest.cpp — Story 003 integration tests for:
//   SETTLED↔SLIPPING state machine, TickComponent tick body, F-4 edge
//   pre-validation, OnSlipMidpoint broadcast, source-lane semantic, and
//   CompleteTween slip_complete_count increment.
//
// Spec: tests/integration/player-movement/pm-state-machine-spec.md
// GDD: design/gdd/player-movement-mechanics.md §3 Rules 2/4/5, §4 F-4,
//      §8 AC-01/08/09/10/23/24/34
// ADR: docs/architecture/adr-0009-player-movement-hosting.md (SD4, SD5, IG-1)
//      docs/architecture/adr-0007-run-state-machine-hosting.md
// Story: production/epics/player-movement/story-003-state-machine-tick-body.md
//
// Test category: SLIPSTORM.PlayerMovement.StateMachine
// Runner: UE Automation Framework, headless ClientContext | ProductFilter.
//
// Uses CreateTestPlayWorld helper pattern from Story 001a for real BeginPlay.
// Uses RSM stub TestOnly_CurrentState / TestOnly_bPaused / TestOnly_bResumeGrace
// state-control seams (added after /code-review 2026-07-12 flagged missing seams
// as BLOCKING) to drive the Rule 5 gate through real code paths.
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

#include "Player/PlayerLaneMovementComponent.h"
#include "Player/SlipstormPlayerPawn.h"
#include "Player/EPlayerLane.h"
#include "Player/ERunSlipState.h"
#include "Player/ESlipDirection.h"
#include "RunStateMachine/RunStateMachineSubsystem.h"
#include "RunStateMachine/ERunState.h"
#include "RunStateMachine/ERunOutcome.h"

// ---------------------------------------------------------------------------
// Helper: construct a valid UCurveFloat with 2 keys spanning [0, 1].
// ---------------------------------------------------------------------------

static UCurveFloat* MakeValidCurve_SM(UObject* InOuter)
{
    UCurveFloat* C = NewObject<UCurveFloat>(InOuter);
    C->FloatCurve.AddKey(0.0f, 0.0f);
    C->FloatCurve.AddKey(1.0f, 1.0f);
    return C;
}

// ---------------------------------------------------------------------------
// Helper: CreateTestPlayWorld — uses FTestWorldWrapper (ENGINE_API) to create
// a dedicated EWorldType::Game world with a proper UGameInstance + registered
// UGameInstanceSubsystems. This is the canonical UE-authored pattern from
// Engine/Source/Runtime/Engine/Public/Tests/AutomationCommon.h line 30-72.
//
// Root cause of prior test-harness failures (S1-04):
//   FAutomationEditorCommonUtils::CreateNewMap() returns an EWorldType::Editor
//   world with no GameInstance. UGameInstanceSubsystem registration is gated
//   on Game-type worlds with a properly-installed WorldContext. Manual
//   NewObject<UGameInstance> + SetGameInstance + Init on the editor's shared
//   WorldContext does NOT trigger subsystem instantiation for the pawn's
//   BeginPlay lookup path (proven by 2026-08-08 pointer diagnostic:
//   PM->RSMSubsystem == 0x0 despite World/GameInstance matching test side).
//
// FTestWorldWrapper solves this by:
//   1. Creating a fresh EWorldType::Game world via UWorld::CreateWorld
//   2. Creating a dedicated WorldContext via GEngine->CreateNewWorldContext
//   3. Wiring GameInstance + Init in the correct order
//   4. Destructor calls DestroyTestWorld → GI->Shutdown → DestroyWorldContext,
//      preventing GI/subsystem contamination between TCs.
//
// The FTestWorldWrapper is stack-allocated per TC — its destructor handles
// all teardown automatically, so no manual DestroyTestWorld or ON_SCOPE_EXIT
// is needed for world lifecycle.
// ---------------------------------------------------------------------------

static UWorld* CreateTestPlayWorld_SM(FAutomationTestBase* T, FTestWorldWrapper& WorldWrapper, const TCHAR* Label)
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

// ---------------------------------------------------------------------------
// Helper: spawn a pawn with valid curves and return the pawn pointer.
// ---------------------------------------------------------------------------

static ASlipstormPlayerPawn* SpawnPawnWithCurves(
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
    Pawn->MovementComponent->SlipCurve       = MakeValidCurve_SM(World);
    Pawn->MovementComponent->LeanCurve       = MakeValidCurve_SM(World);
    Pawn->MovementComponent->EdgeAbsorbCurve = MakeValidCurve_SM(World);
    UGameplayStatics::FinishSpawningActor(Pawn, FTransform::Identity);
    return Pawn;
}

// ---------------------------------------------------------------------------
// Helper: retrieve the RSM stub subsystem from the test world.
// ---------------------------------------------------------------------------

static URunStateMachineSubsystem* GetRSM_SM(FAutomationTestBase* T, UWorld* World, const TCHAR* Label)
{
    UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
    URunStateMachineSubsystem* RSM = GI ? GI->GetSubsystem<URunStateMachineSubsystem>() : nullptr;
    if (!T->TestNotNull(FString::Printf(TEXT("%s: RSMSubsystem"), Label), RSM))
    {
        return nullptr;
    }
    return RSM;
}

// ---------------------------------------------------------------------------
// Helper: reset RSM stub TestOnly state to RUNNING (default entry point).
// ---------------------------------------------------------------------------

static void SetRSMRunning(URunStateMachineSubsystem* RSM)
{
    RSM->TestOnly_CurrentState                       = ERunState::RUNNING;
    RSM->TestOnly_bPaused                            = false;
    RSM->TestOnly_bResumeGrace                       = false;
    RSM->TestOnly_ForceTickNowCallCount              = 0;
    RSM->TestOnly_GetCurrentStateCallCount           = 0;
    RSM->TestOnly_GetCurrentStateCountAtForceTickNow = -1;
}

// ---------------------------------------------------------------------------
// IMPLEMENT_COMPLEX_AUTOMATION_TEST — 8 test cases per Story 003 AC list.
// ---------------------------------------------------------------------------

IMPLEMENT_COMPLEX_AUTOMATION_TEST(
    FPMStateMachineTest,
    "SLIPSTORM.PlayerMovement.StateMachine",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

void FPMStateMachineTest::GetTests(
    TArray<FString>& OutBeautifiedNames,
    TArray<FString>& OutTestCommands) const
{
    OutBeautifiedNames.Add(TEXT("AC-01 baseline slip — Center+Right→Right via HandleSlipTransition+Tick"));
    OutTestCommands.Add(TEXT("ac01_baseline_slip"));

    OutBeautifiedNames.Add(TEXT("AC-08 non-RUNNING discard — IDLE/COUNTDOWN/DEAD/COMPLETE/ABORTED/RESOLVING"));
    OutTestCommands.Add(TEXT("ac08_non_running_discard"));

    OutBeautifiedNames.Add(TEXT("AC-09 paused discard — RUNNING+paused"));
    OutTestCommands.Add(TEXT("ac09_paused_discard"));

    OutBeautifiedNames.Add(TEXT("AC-10 resume_grace discard — RUNNING+grace"));
    OutTestCommands.Add(TEXT("ac10_grace_discard"));

    OutBeautifiedNames.Add(TEXT("AC-23 SETTLED→SLIPPING synchronous — via HandleSlipTransition"));
    OutTestCommands.Add(TEXT("ac23_settled_to_slipping_sync"));

    OutBeautifiedNames.Add(TEXT("AC-24 OnSlipMidpoint broadcast — driven through TickComponent"));
    OutTestCommands.Add(TEXT("ac24_midpoint_broadcast"));

    OutBeautifiedNames.Add(TEXT("AC-34 source-lane semantic during SLIPPING via ticks"));
    OutTestCommands.Add(TEXT("ac34_source_lane_semantic"));

    OutBeautifiedNames.Add(TEXT("ForceTickNow prologue invariant — snapshot-based ordering check"));
    OutTestCommands.Add(TEXT("force_tick_now_prologue"));
}

bool FPMStateMachineTest::RunTest(const FString& Parameters)
{
    // -----------------------------------------------------------------------
    // AC-01 — Baseline slip via HandleSlipTransition + TickComponent.
    // Drives the full integration flow: Rule 5 gate → F-4 → SETTLED→SLIPPING
    // → tick advances → CompleteTween commits current_lane = target_lane.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("ac01_baseline_slip"))
    {
        FTestWorldWrapper WorldWrapper;
        UWorld* TestWorld = CreateTestPlayWorld_SM(this, WorldWrapper, TEXT("AC-01"));
        if (!TestWorld) return false;
        ASlipstormPlayerPawn* Pawn = SpawnPawnWithCurves(this, TestWorld, TEXT("AC-01"));
        if (!Pawn) return false;
        URunStateMachineSubsystem* RSM = GetRSM_SM(this, TestWorld, TEXT("AC-01"));
        if (!RSM) return false;

        UPlayerLaneMovementComponent* PM = Pawn->MovementComponent.Get();
        SetRSMRunning(RSM);

        // Initial state: SETTLED at Center.
        PM->current_lane   = EPlayerLane::Center;
        PM->target_lane    = EPlayerLane::Center;
        PM->movement_state = ERunSlipState::SETTLED;
        PM->tween_progress = 0.0f;
        PM->slip_complete_count = 0;

        // Drive the slip through HandleSlipTransition (the public entry point).
        PM->HandleSlipTransition(ESlipDirection::Right);

        // Post-transition (synchronous — Rule 2 collision commit):
        TestEqual(TEXT("AC-01: target_lane == Right after HandleSlipTransition"),
                  PM->target_lane, EPlayerLane::Right);
        TestEqual(TEXT("AC-01: movement_state == SLIPPING"),
                  PM->movement_state, ERunSlipState::SLIPPING);
        TestEqual(TEXT("AC-01: current_lane still Center (source-lane semantic)"),
                  PM->current_lane, EPlayerLane::Center);
        TestTrue(TEXT("AC-01: pawn root X == LaneWorldX(Right) after collision commit"),
                 FMath::IsNearlyEqual(Pawn->GetActorLocation().X, 100.0f, KINDA_SMALL_NUMBER));

        // Drive TickComponent enough ticks to complete the tween. With
        // SLIP_TWEEN_DURATION_S = 0.15 and MAX_SLIP_DT_S = 0.05, each tick
        // advances TP by 0.05/0.15 ≈ 0.333. Four ticks reach TP ≈ 1.333 → CompleteTween.
        for (int32 i = 0; i < 4; ++i)
        {
            FApp::SetDeltaTime(0.05);
            PM->TickComponent(0.05f, ELevelTick::LEVELTICK_All, nullptr);
        }

        TestEqual(TEXT("AC-01: current_lane == Right post-CompleteTween"),
                  PM->current_lane, EPlayerLane::Right);
        TestEqual(TEXT("AC-01: movement_state == SETTLED post-CompleteTween"),
                  PM->movement_state, ERunSlipState::SETTLED);
        TestEqual(TEXT("AC-01: slip_complete_count incremented to 1"),
                  PM->slip_complete_count, 1);

        TestWorld->DestroyActor(Pawn);
        return true;
    }

    // -----------------------------------------------------------------------
    // AC-08 — Non-RUNNING state discard. HandleSlipTransition must return
    // without side effects when RSM is in any state other than RUNNING.
    // Tests each of the 6 non-RUNNING ERunState values.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("ac08_non_running_discard"))
    {
        const ERunState States[] = {
            ERunState::IDLE, ERunState::COUNTDOWN, ERunState::DEAD,
            ERunState::COMPLETE, ERunState::RESOLVING, ERunState::ABORTED
        };
        const TCHAR* Names[] = {
            TEXT("IDLE"), TEXT("COUNTDOWN"), TEXT("DEAD"),
            TEXT("COMPLETE"), TEXT("RESOLVING"), TEXT("ABORTED")
        };

        for (int32 i = 0; i < 6; ++i)
        {
            FTestWorldWrapper WorldWrapper;
            UWorld* TestWorld = CreateTestPlayWorld_SM(this, WorldWrapper, Names[i]);
            if (!TestWorld) return false;
            ASlipstormPlayerPawn* Pawn = SpawnPawnWithCurves(this, TestWorld, Names[i]);
            if (!Pawn) return false;
            URunStateMachineSubsystem* RSM = GetRSM_SM(this, TestWorld, Names[i]);
            if (!RSM) return false;

            UPlayerLaneMovementComponent* PM = Pawn->MovementComponent.Get();
            RSM->TestOnly_CurrentState = States[i];
            RSM->TestOnly_bPaused = false;
            RSM->TestOnly_bResumeGrace = false;

            PM->current_lane   = EPlayerLane::Center;
            PM->target_lane    = EPlayerLane::Center;
            PM->movement_state = ERunSlipState::SETTLED;

            PM->HandleSlipTransition(ESlipDirection::Right);

            TestEqual(FString::Printf(TEXT("AC-08 %s: current_lane unchanged"), Names[i]),
                      PM->current_lane, EPlayerLane::Center);
            TestEqual(FString::Printf(TEXT("AC-08 %s: target_lane unchanged"), Names[i]),
                      PM->target_lane, EPlayerLane::Center);
            TestEqual(FString::Printf(TEXT("AC-08 %s: movement_state stays SETTLED"), Names[i]),
                      PM->movement_state, ERunSlipState::SETTLED);

            TestWorld->DestroyActor(Pawn);
        }
        return true;
    }

    // -----------------------------------------------------------------------
    // AC-09 — RUNNING + paused = discard. Verifies the Rule 5 gate's IsPaused()
    // branch actually fires (as opposed to only testing the null-RSM path).
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("ac09_paused_discard"))
    {
        FTestWorldWrapper WorldWrapper;
        UWorld* TestWorld = CreateTestPlayWorld_SM(this, WorldWrapper, TEXT("AC-09"));
        if (!TestWorld) return false;
        ASlipstormPlayerPawn* Pawn = SpawnPawnWithCurves(this, TestWorld, TEXT("AC-09"));
        if (!Pawn) return false;
        URunStateMachineSubsystem* RSM = GetRSM_SM(this, TestWorld, TEXT("AC-09"));
        if (!RSM) return false;

        UPlayerLaneMovementComponent* PM = Pawn->MovementComponent.Get();
        RSM->TestOnly_CurrentState = ERunState::RUNNING;
        RSM->TestOnly_bPaused      = true;
        RSM->TestOnly_bResumeGrace = false;

        PM->current_lane   = EPlayerLane::Center;
        PM->target_lane    = EPlayerLane::Center;
        PM->movement_state = ERunSlipState::SETTLED;

        PM->HandleSlipTransition(ESlipDirection::Right);

        TestEqual(TEXT("AC-09: current_lane unchanged when RUNNING+paused"),
                  PM->current_lane, EPlayerLane::Center);
        TestEqual(TEXT("AC-09: movement_state stays SETTLED"),
                  PM->movement_state, ERunSlipState::SETTLED);
        TestEqual(TEXT("AC-09: target_lane unchanged"),
                  PM->target_lane, EPlayerLane::Center);

        TestWorld->DestroyActor(Pawn);
        return true;
    }

    // -----------------------------------------------------------------------
    // AC-10 — RUNNING + resume_grace = discard.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("ac10_grace_discard"))
    {
        FTestWorldWrapper WorldWrapper;
        UWorld* TestWorld = CreateTestPlayWorld_SM(this, WorldWrapper, TEXT("AC-10"));
        if (!TestWorld) return false;
        ASlipstormPlayerPawn* Pawn = SpawnPawnWithCurves(this, TestWorld, TEXT("AC-10"));
        if (!Pawn) return false;
        URunStateMachineSubsystem* RSM = GetRSM_SM(this, TestWorld, TEXT("AC-10"));
        if (!RSM) return false;

        UPlayerLaneMovementComponent* PM = Pawn->MovementComponent.Get();
        RSM->TestOnly_CurrentState = ERunState::RUNNING;
        RSM->TestOnly_bPaused      = false;
        RSM->TestOnly_bResumeGrace = true;

        PM->current_lane   = EPlayerLane::Center;
        PM->target_lane    = EPlayerLane::Center;
        PM->movement_state = ERunSlipState::SETTLED;

        PM->HandleSlipTransition(ESlipDirection::Right);

        TestEqual(TEXT("AC-10: current_lane unchanged when RUNNING+grace"),
                  PM->current_lane, EPlayerLane::Center);
        TestEqual(TEXT("AC-10: movement_state stays SETTLED"),
                  PM->movement_state, ERunSlipState::SETTLED);
        TestEqual(TEXT("AC-10: target_lane unchanged"),
                  PM->target_lane, EPlayerLane::Center);

        TestWorld->DestroyActor(Pawn);
        return true;
    }

    // -----------------------------------------------------------------------
    // AC-23 — SETTLED→SLIPPING synchronous. All 5 fields observable within
    // the single HandleSlipTransition call (Rule 2 collision commit + state).
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("ac23_settled_to_slipping_sync"))
    {
        FTestWorldWrapper WorldWrapper;
        UWorld* TestWorld = CreateTestPlayWorld_SM(this, WorldWrapper, TEXT("AC-23"));
        if (!TestWorld) return false;
        ASlipstormPlayerPawn* Pawn = SpawnPawnWithCurves(this, TestWorld, TEXT("AC-23"));
        if (!Pawn) return false;
        URunStateMachineSubsystem* RSM = GetRSM_SM(this, TestWorld, TEXT("AC-23"));
        if (!RSM) return false;

        UPlayerLaneMovementComponent* PM = Pawn->MovementComponent.Get();
        SetRSMRunning(RSM);

        PM->current_lane   = EPlayerLane::Center;
        PM->target_lane    = EPlayerLane::Center;
        PM->movement_state = ERunSlipState::SETTLED;
        PM->tween_progress = 0.0f;

        // Single event call — Rule 2 requires all 5 field updates synchronous.
        PM->HandleSlipTransition(ESlipDirection::Right);

        TestTrue(TEXT("AC-23-(a): pawn root X == LaneWorldX(Right) post-transition"),
                 FMath::IsNearlyEqual(Pawn->GetActorLocation().X, 100.0f, KINDA_SMALL_NUMBER));
        TestEqual(TEXT("AC-23-(b): movement_state == SLIPPING"),
                  PM->movement_state, ERunSlipState::SLIPPING);
        TestEqual(TEXT("AC-23-(c): target_lane == Right"),
                  PM->target_lane, EPlayerLane::Right);
        TestTrue(TEXT("AC-23-(d): tween_progress == 0.0f (fresh tween)"),
                 FMath::IsNearlyEqual(PM->tween_progress, 0.0f, KINDA_SMALL_NUMBER));
        TestEqual(TEXT("AC-23-(e): current_lane == Center (SOURCE preserved)"),
                  PM->current_lane, EPlayerLane::Center);

        // Also verify pawn root Y and Z stayed at 0 (Rule 2 only writes X).
        TestTrue(TEXT("AC-23: pawn root Y == 0"),
                 FMath::IsNearlyEqual(Pawn->GetActorLocation().Y, 0.0f, KINDA_SMALL_NUMBER));
        TestTrue(TEXT("AC-23: pawn root Z == 0"),
                 FMath::IsNearlyEqual(Pawn->GetActorLocation().Z, 0.0f, KINDA_SMALL_NUMBER));

        TestWorld->DestroyActor(Pawn);
        return true;
    }

    // -----------------------------------------------------------------------
    // AC-24 — OnSlipMidpoint broadcast, driven through TickComponent.
    //
    // Fires ONCE on the tick when tween_progress crosses 0.5 (prev < 0.5 AND
    // current >= 0.5). Verified by counting subscriber deliveries — NOT by
    // re-implementing the check inline. If TickComponent's midpoint check is
    // deleted or made buggy, THIS test must fail.
    //
    // Edge cases:
    //   (a) TP already >= 0.5 from prior tick — no fire on subsequent ticks
    //   (b) Hitch causing 0.4 → 0.6 crossing in one tick — fires exactly once
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("ac24_midpoint_broadcast"))
    {
        FTestWorldWrapper WorldWrapper;
        UWorld* TestWorld = CreateTestPlayWorld_SM(this, WorldWrapper, TEXT("AC-24"));
        if (!TestWorld) return false;
        ASlipstormPlayerPawn* Pawn = SpawnPawnWithCurves(this, TestWorld, TEXT("AC-24"));
        if (!Pawn) return false;
        URunStateMachineSubsystem* RSM = GetRSM_SM(this, TestWorld, TEXT("AC-24"));
        if (!RSM) return false;

        UPlayerLaneMovementComponent* PM = Pawn->MovementComponent.Get();
        SetRSMRunning(RSM);

        // Put PM into SLIPPING (Center→Right) with TP just below 0.5.
        PM->current_lane   = EPlayerLane::Center;
        PM->target_lane    = EPlayerLane::Right;
        PM->movement_state = ERunSlipState::SLIPPING;
        PM->tween_progress = 0.45f;

        int32 BroadcastCount = 0;
        EPlayerLane LastFrom = EPlayerLane::FarLeft;
        EPlayerLane LastTo   = EPlayerLane::FarLeft;
        PM->OnSlipMidpoint.AddLambda([&](EPlayerLane From, EPlayerLane To)
        {
            ++BroadcastCount;
            LastFrom = From;
            LastTo   = To;
        });

        // DT chosen so AdvanceTweenProgress adds ~0.10 → TP 0.45 → 0.55.
        // 0.015 / 0.15 = 0.10.
        FApp::SetDeltaTime(0.015);
        PM->TickComponent(0.015f, ELevelTick::LEVELTICK_All, nullptr);

        TestTrue(TEXT("AC-24: tween_progress >= 0.5 after crossing tick"),
                 PM->tween_progress >= 0.5f);
        TestEqual(TEXT("AC-24: TickComponent broadcast OnSlipMidpoint exactly once"),
                  BroadcastCount, 1);
        TestEqual(TEXT("AC-24: broadcast From == Center (source-lane)"),
                  LastFrom, EPlayerLane::Center);
        TestEqual(TEXT("AC-24: broadcast To == Right (target-lane)"),
                  LastTo, EPlayerLane::Right);

        // Edge case (a): second tick — TP now >= 0.5 for prev too, MUST NOT fire again.
        FApp::SetDeltaTime(0.015);
        PM->TickComponent(0.015f, ELevelTick::LEVELTICK_All, nullptr);
        TestEqual(TEXT("AC-24-(a): no second broadcast when both prev and current >= 0.5"),
                  BroadcastCount, 1);

        // Edge case (b): fresh PM, one big hitch causes TP 0.4 → 0.6 → fires once.
        {
            ASlipstormPlayerPawn* Pawn2 = SpawnPawnWithCurves(this, TestWorld, TEXT("AC-24-b"));
            if (!Pawn2) return false;
            UPlayerLaneMovementComponent* PM2 = Pawn2->MovementComponent.Get();
            SetRSMRunning(RSM);

            PM2->current_lane   = EPlayerLane::Center;
            PM2->target_lane    = EPlayerLane::Right;
            PM2->movement_state = ERunSlipState::SLIPPING;
            PM2->tween_progress = 0.4f;

            int32 HitchCount = 0;
            PM2->OnSlipMidpoint.AddLambda([&](EPlayerLane, EPlayerLane) { ++HitchCount; });

            // DT causing a big jump (0.03 / 0.15 = 0.20 → 0.4 → 0.6).
            FApp::SetDeltaTime(0.03);
            PM2->TickComponent(0.03f, ELevelTick::LEVELTICK_All, nullptr);

            TestEqual(TEXT("AC-24-(b): 0.4→0.6 hitch fires exactly once (not twice)"),
                      HitchCount, 1);

            TestWorld->DestroyActor(Pawn2);
        }

        TestWorld->DestroyActor(Pawn);
        return true;
    }

    // -----------------------------------------------------------------------
    // AC-34 — Source-lane semantic BINDING (R7-PM-PROPAGATION-REVIEW).
    // Throughout SLIPPING, current_lane MUST return the SOURCE lane.
    // Reassignment to target_lane happens ONLY at CompleteTween.
    //
    // Test: drive Left→Center tween via HandleSlipTransition + Tick, sample
    // current_lane at multiple TP values, verify always == Left. Then complete
    // the tween via ticks, verify current_lane == Center post-CompleteTween.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("ac34_source_lane_semantic"))
    {
        FTestWorldWrapper WorldWrapper;
        UWorld* TestWorld = CreateTestPlayWorld_SM(this, WorldWrapper, TEXT("AC-34"));
        if (!TestWorld) return false;
        ASlipstormPlayerPawn* Pawn = SpawnPawnWithCurves(this, TestWorld, TEXT("AC-34"));
        if (!Pawn) return false;
        URunStateMachineSubsystem* RSM = GetRSM_SM(this, TestWorld, TEXT("AC-34"));
        if (!RSM) return false;

        UPlayerLaneMovementComponent* PM = Pawn->MovementComponent.Get();
        SetRSMRunning(RSM);

        // Start SETTLED at Left.
        PM->current_lane   = EPlayerLane::Left;
        PM->target_lane    = EPlayerLane::Left;
        PM->movement_state = ERunSlipState::SETTLED;
        PM->tween_progress = 0.0f;

        // Trigger Left→Center via HandleSlipTransition.
        PM->HandleSlipTransition(ESlipDirection::Right);

        TestEqual(TEXT("AC-34: target_lane == Center after Left+Right slip"),
                  PM->target_lane, EPlayerLane::Center);
        TestEqual(TEXT("AC-34: current_lane still Left (source) at TP=0.0"),
                  PM->current_lane, EPlayerLane::Left);
        TestEqual(TEXT("AC-34: movement_state == SLIPPING"),
                  PM->movement_state, ERunSlipState::SLIPPING);

        // Sample current_lane at intermediate TP values (each tick advances by ~0.333).
        FApp::SetDeltaTime(0.015);
        PM->TickComponent(0.015f, ELevelTick::LEVELTICK_All, nullptr);
        TestEqual(TEXT("AC-34: current_lane == Left after 1 tick (mid-SLIPPING)"),
                  PM->current_lane, EPlayerLane::Left);

        FApp::SetDeltaTime(0.03);
        PM->TickComponent(0.03f, ELevelTick::LEVELTICK_All, nullptr);
        TestEqual(TEXT("AC-34: current_lane == Left after 2 ticks"),
                  PM->current_lane, EPlayerLane::Left);

        // Complete the tween.
        for (int32 i = 0; i < 5; ++i)
        {
            FApp::SetDeltaTime(0.05);
            PM->TickComponent(0.05f, ELevelTick::LEVELTICK_All, nullptr);
        }

        TestEqual(TEXT("AC-34: current_lane == Center post-CompleteTween (target reassigned)"),
                  PM->current_lane, EPlayerLane::Center);
        TestEqual(TEXT("AC-34: movement_state == SETTLED post-CompleteTween"),
                  PM->movement_state, ERunSlipState::SETTLED);

        TestWorld->DestroyActor(Pawn);
        return true;
    }

    // -----------------------------------------------------------------------
    // force_tick_now_prologue — ADR-0009 IG-1 ordering invariant.
    //
    // Uses the snapshot mechanism: RSMSubsystem's ForceTickNow captures
    // TestOnly_GetCurrentStateCallCount BEFORE incrementing its own counter.
    // After TickComponent runs:
    //   - TestOnly_ForceTickNowCallCount >= 1 (proves ForceTickNow was called)
    //   - TestOnly_GetCurrentStateCallCount >= 1 (proves gate reads happened)
    //   - TestOnly_GetCurrentStateCountAtForceTickNow == 0 (proves ForceTickNow
    //     ran BEFORE any GetCurrentState — the ordering invariant)
    //
    // If the ForceTickNow call is moved to AFTER the Rule 5 gate reads,
    // the snapshot would be >= 1 → this test fails. This is a real ordering
    // check, not a "called somewhere during Tick" existence check.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("force_tick_now_prologue"))
    {
        FTestWorldWrapper WorldWrapper;
        UWorld* TestWorld = CreateTestPlayWorld_SM(this, WorldWrapper, TEXT("prologue"));
        if (!TestWorld) return false;
        ASlipstormPlayerPawn* Pawn = SpawnPawnWithCurves(this, TestWorld, TEXT("prologue"));
        if (!Pawn) return false;
        URunStateMachineSubsystem* RSM = GetRSM_SM(this, TestWorld, TEXT("prologue"));
        if (!RSM) return false;

        UPlayerLaneMovementComponent* PM = Pawn->MovementComponent.Get();
        SetRSMRunning(RSM);

        // Baseline: all counters reset.
        TestEqual(TEXT("prologue: ForceTickNowCallCount starts 0"),
                  RSM->TestOnly_ForceTickNowCallCount, 0);
        TestEqual(TEXT("prologue: GetCurrentStateCallCount starts 0"),
                  RSM->TestOnly_GetCurrentStateCallCount, 0);
        TestEqual(TEXT("prologue: snapshot sentinel == -1 before tick"),
                  RSM->TestOnly_GetCurrentStateCountAtForceTickNow, -1);

        FApp::SetDeltaTime(0.016);
        PM->TickComponent(0.016f, ELevelTick::LEVELTICK_All, nullptr);

        TestTrue(TEXT("prologue: ForceTickNowCallCount >= 1 after tick"),
                 RSM->TestOnly_ForceTickNowCallCount >= 1);
        TestTrue(TEXT("prologue: GetCurrentStateCallCount >= 1 after tick"),
                 RSM->TestOnly_GetCurrentStateCallCount >= 1);
        TestEqual(TEXT("prologue: snapshot == 0 proves ForceTickNow ran BEFORE GetCurrentState (ADR-0009 IG-1)"),
                  RSM->TestOnly_GetCurrentStateCountAtForceTickNow, 0);

        TestWorld->DestroyActor(Pawn);
        return true;
    }

    AddError(FString::Printf(
        TEXT("FPMStateMachineTest::RunTest — unknown Parameters: '%s'"), *Parameters));
    return false;
}

#endif // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

// Copyright (c) Game Studio. All rights reserved.
// SLIPSTORM — 60-second voxel endless runner, mobile, Unreal Engine 5.7
//
// PMTerminalStatesTest.cpp — Story 008 integration tests for terminal-state
// handlers (DEAD/COMPLETE/ABORTED/COUNTDOWN/IDLE) + AC-F6-D DEAD F-6
// preservation + counter reset semantics + AC-SS-B default-branch fallback.
//
// Story Type: Integration (first in the epic).
// Tests directly invoke HandleStateChanged via the FPMTerminalStatesTest
// friend access, following the ADR-0007 non-callback invariant. No RSM
// broadcast simulation required — the state-change is asserted at the
// handler-body level.
//
// Spec:   production/epics/player-movement/story-008-terminal-state-handlers.md
// GDD:    design/gdd/player-movement-mechanics.md §3 Rules 7/8/9/10/11 +
//         §8 AC-14/15/16/17/18/28/31/32/F6-D/COUNTER-DEAD/COUNTER-COMPLETE/
//         COUNTER-F6-RESET + platform §3 AC-SS-B
// ADR:    docs/architecture/adr-0009-player-movement-hosting.md (SD5),
//         docs/architecture/adr-0007-run-state-machine-hosting.md (SD2 non-callback)
// TR:     TR-PM-015 (counters), TR-PM-016 (COUNTDOWN reset), TR-PM-017 (DEAD freeze),
//         TR-PM-018 (COMPLETE/ABORTED snap)
//
// Test category: SLIPSTORM.PlayerMovement.TerminalStates
// Runner: UE Automation Framework, ClientContext | ProductFilter.
//
// Helpers _TS-suffixed to avoid ODR collisions with prior composition tests.
// AAA (Arrange / Act / Assert) labels present per test-standards.md.
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
#include "Components/StaticMeshComponent.h"

#include "Player/PlayerLaneMovementComponent.h"
#include "Player/SlipstormPlayerPawn.h"
#include "Player/EPlayerLane.h"
#include "Player/ERunSlipState.h"
#include "RunStateMachine/RunStateMachineSubsystem.h"
#include "RunStateMachine/ERunState.h"
#include "RunStateMachine/ERunOutcome.h"
#include "Seam/PlayerMovementProvider.h" // EMovementState for AC-SS-B accessor

// ---------------------------------------------------------------------------
// Helpers (duplicated from prior composition test files with _TS suffix).
// ---------------------------------------------------------------------------

static UCurveFloat* MakeIdentityCurve_TS(UObject* InOuter)
{
    UCurveFloat* C = NewObject<UCurveFloat>(InOuter);
    C->FloatCurve.AddKey(0.0f, 0.0f);
    C->FloatCurve.AddKey(1.0f, 1.0f);
    return C;
}

static UWorld* CreateTestPlayWorld_TS(FAutomationTestBase* T, const TCHAR* Label)
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

static ASlipstormPlayerPawn* SpawnPawnWithCurves_TS(
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
    Pawn->MovementComponent->SlipCurve       = MakeIdentityCurve_TS(World);
    Pawn->MovementComponent->LeanCurve       = MakeIdentityCurve_TS(World);
    Pawn->MovementComponent->EdgeAbsorbCurve = MakeIdentityCurve_TS(World);
    UGameplayStatics::FinishSpawningActor(Pawn, FTransform::Identity);
    return Pawn;
}

// Convenience wrapper for HandleStateChanged with default unused args.
static void BroadcastState_TS(UPlayerLaneMovementComponent* PM, ERunState OldState, ERunState NewState)
{
    PM->HandleStateChanged(OldState, NewState, ERunOutcome::NONE, 0.0);
}

// ---------------------------------------------------------------------------
// IMPLEMENT_COMPLEX_AUTOMATION_TEST — 10 test commands consolidating 16 ACs.
// ---------------------------------------------------------------------------

IMPLEMENT_COMPLEX_AUTOMATION_TEST(
    FPMTerminalStatesTest,
    "SLIPSTORM.PlayerMovement.TerminalStates",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

void FPMTerminalStatesTest::GetTests(
    TArray<FString>& OutBeautifiedNames,
    TArray<FString>& OutTestCommands) const
{
    OutBeautifiedNames.Add(TEXT("AC-14 DEAD @ TP=0.55 — freeze tween_progress + lane commit + SETTLED"));
    OutTestCommands.Add(TEXT("ac14_dead_freeze_tp055"));

    OutBeautifiedNames.Add(TEXT("AC-14 boundary — DEAD @ TP=0.001 and 0.999 preserved bit-exact"));
    OutTestCommands.Add(TEXT("ac14_boundary_dead_tp_bit_exact"));

    OutBeautifiedNames.Add(TEXT("AC-15 DEAD while SETTLED — no lane change, counters preserved"));
    OutTestCommands.Add(TEXT("ac15_dead_while_settled"));

    OutBeautifiedNames.Add(TEXT("AC-16 + AC-28 + AC-32 COMPLETE snap — root, mesh, TP, lean all zero"));
    OutTestCommands.Add(TEXT("ac16_complete_snap_and_reset"));

    OutBeautifiedNames.Add(TEXT("AC-17 ABORTED snap — identical to AC-16"));
    OutTestCommands.Add(TEXT("ac17_aborted_snap_and_reset"));

    OutBeautifiedNames.Add(TEXT("AC-18 COUNTDOWN full reset — Center + all counters zero"));
    OutTestCommands.Add(TEXT("ac18_countdown_full_reset"));

    OutBeautifiedNames.Add(TEXT("AC-31 DEAD @ TP=0.5 — F-5 lean angles freeze (preserved)"));
    OutTestCommands.Add(TEXT("ac31_dead_lean_freeze"));

    OutBeautifiedNames.Add(TEXT("AC-F6-D DEAD during F-6 tail — F-6 state preserved + not advanced"));
    OutTestCommands.Add(TEXT("ac_f6_d_dead_during_f6_tail"));

    OutBeautifiedNames.Add(TEXT("AC-COUNTER-DEAD/COMPLETE preserved; AC-COUNTER-F6-RESET on non-DEAD terminals"));
    OutTestCommands.Add(TEXT("ac_counter_preservation_and_f6_reset"));

    OutBeautifiedNames.Add(TEXT("AC-SS-B GetMovementStateExternal default: → SETTLED on corrupt ordinal"));
    OutTestCommands.Add(TEXT("ac_ss_b_get_movement_state_default_branch"));

    OutBeautifiedNames.Add(TEXT("AC-24 exclusion — OnSlipMidpoint NEVER broadcast on DEAD/COMPLETE/ABORTED"));
    OutTestCommands.Add(TEXT("ac24_no_slip_midpoint_on_terminal"));
}

bool FPMTerminalStatesTest::RunTest(const FString& Parameters)
{
    // -----------------------------------------------------------------------
    // TC1 (ac14_dead_freeze_tp055) — AC-14 DEAD freeze at TP=0.55.
    //
    // SLIPPING Left→Center with tween_progress = 0.55, non-zero lean values.
    // Broadcast DEAD. Assert: tween_progress == 0.55 (bit-exact), current_lane
    // committed to target_lane (Center), movement_state = SETTLED, lean values
    // preserved (F-5 lean is frozen for Death Replay), slip_complete_count
    // unchanged.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("ac14_dead_freeze_tp055"))
    {
        // Arrange
        UWorld* TestWorld = CreateTestPlayWorld_TS(this, TEXT("TC1"));
        if (!TestWorld) { return false; }
        ASlipstormPlayerPawn* Pawn = SpawnPawnWithCurves_TS(this, TestWorld, TEXT("TC1"));
        if (!Pawn) { return false; }
        UPlayerLaneMovementComponent* PM = Pawn->MovementComponent.Get();

        PM->current_lane        = EPlayerLane::Left;
        PM->target_lane         = EPlayerLane::Center;
        PM->movement_state      = ERunSlipState::SLIPPING;
        PM->tween_progress      = 0.55f;
        PM->lean_angle          = 3.2f;
        PM->head_lean_angle     = 2.1f;
        PM->arm_lean_angle      = 3.8f;
        PM->slip_complete_count = 5;

        // Act
        BroadcastState_TS(PM, ERunState::RUNNING, ERunState::DEAD);

        // Assert — freeze semantics: TP preserved, lean preserved, lane committed.
        TestEqual(TEXT("TC1: tween_progress == 0.55 bit-exact (AC-14)"), PM->tween_progress, 0.55f);
        TestEqual(TEXT("TC1: current_lane == Center (source-lane commit at DEAD)"),
                  static_cast<uint8>(PM->current_lane), static_cast<uint8>(EPlayerLane::Center));
        TestEqual(TEXT("TC1: movement_state == SETTLED after DEAD"),
                  static_cast<uint8>(PM->movement_state), static_cast<uint8>(ERunSlipState::SETTLED));
        TestEqual(TEXT("TC1: lean_angle preserved == 3.2°"), PM->lean_angle, 3.2f);
        TestEqual(TEXT("TC1: head_lean_angle preserved == 2.1°"), PM->head_lean_angle, 2.1f);
        TestEqual(TEXT("TC1: arm_lean_angle preserved == 3.8°"), PM->arm_lean_angle, 3.8f);
        TestEqual(TEXT("TC1: slip_complete_count preserved == 5"), PM->slip_complete_count, 5);

        TestWorld->DestroyActor(Pawn);
        return true;
    }

    // -----------------------------------------------------------------------
    // TC2 (ac14_boundary_dead_tp_bit_exact) — AC-14 boundary TP=0.001 and 0.999.
    // Verifies no snap-on-threshold; DEAD preserves fractional value exactly.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("ac14_boundary_dead_tp_bit_exact"))
    {
        // Boundary TP=0.001
        {
            UWorld* TestWorld = CreateTestPlayWorld_TS(this, TEXT("TC2-lo"));
            if (!TestWorld) { return false; }
            ASlipstormPlayerPawn* Pawn = SpawnPawnWithCurves_TS(this, TestWorld, TEXT("TC2-lo"));
            if (!Pawn) { return false; }
            UPlayerLaneMovementComponent* PM = Pawn->MovementComponent.Get();

            // Arrange
            PM->current_lane   = EPlayerLane::Left;
            PM->target_lane    = EPlayerLane::Center;
            PM->movement_state = ERunSlipState::SLIPPING;
            PM->tween_progress = 0.001f;

            // Act
            BroadcastState_TS(PM, ERunState::RUNNING, ERunState::DEAD);

            // Assert
            TestEqual(TEXT("TC2-lo: TP=0.001 preserved bit-exact"), PM->tween_progress, 0.001f);

            TestWorld->DestroyActor(Pawn);
        }
        // Boundary TP=0.999
        {
            UWorld* TestWorld = CreateTestPlayWorld_TS(this, TEXT("TC2-hi"));
            if (!TestWorld) { return false; }
            ASlipstormPlayerPawn* Pawn = SpawnPawnWithCurves_TS(this, TestWorld, TEXT("TC2-hi"));
            if (!Pawn) { return false; }
            UPlayerLaneMovementComponent* PM = Pawn->MovementComponent.Get();

            // Arrange
            PM->current_lane   = EPlayerLane::Left;
            PM->target_lane    = EPlayerLane::Center;
            PM->movement_state = ERunSlipState::SLIPPING;
            PM->tween_progress = 0.999f;

            // Act
            BroadcastState_TS(PM, ERunState::RUNNING, ERunState::DEAD);

            // Assert
            TestEqual(TEXT("TC2-hi: TP=0.999 preserved bit-exact"), PM->tween_progress, 0.999f);

            TestWorld->DestroyActor(Pawn);
        }
        return true;
    }

    // -----------------------------------------------------------------------
    // TC3 (ac15_dead_while_settled) — AC-15.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("ac15_dead_while_settled"))
    {
        // Arrange
        UWorld* TestWorld = CreateTestPlayWorld_TS(this, TEXT("TC3"));
        if (!TestWorld) { return false; }
        ASlipstormPlayerPawn* Pawn = SpawnPawnWithCurves_TS(this, TestWorld, TEXT("TC3"));
        if (!Pawn) { return false; }
        UPlayerLaneMovementComponent* PM = Pawn->MovementComponent.Get();

        PM->current_lane        = EPlayerLane::Right;
        PM->target_lane         = EPlayerLane::Right;
        PM->movement_state      = ERunSlipState::SETTLED;
        PM->slip_complete_count = 3;
        PM->has_queued_input    = true;
        PM->queued_input_direction = ESlipDirection::Left;

        // Act
        BroadcastState_TS(PM, ERunState::RUNNING, ERunState::DEAD);

        // Assert
        TestEqual(TEXT("TC3: current_lane unchanged == Right"),
                  static_cast<uint8>(PM->current_lane), static_cast<uint8>(EPlayerLane::Right));
        TestEqual(TEXT("TC3: slip_complete_count preserved == 3"), PM->slip_complete_count, 3);
        TestFalse(TEXT("TC3: buffer cleared post-DEAD"), PM->has_queued_input);

        TestWorld->DestroyActor(Pawn);
        return true;
    }

    // -----------------------------------------------------------------------
    // TC4 (ac16_complete_snap_and_reset) — AC-16 + AC-28 + AC-32.
    // COMPLETE snaps root to LaneWorldX(target), zeroes mesh transforms, zeroes
    // F-5 lean (AC-28 body, AC-32 arm), TP=0, movement_state=SETTLED, buffer
    // cleared, counters preserved.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("ac16_complete_snap_and_reset"))
    {
        // Arrange
        UWorld* TestWorld = CreateTestPlayWorld_TS(this, TEXT("TC4"));
        if (!TestWorld) { return false; }
        ASlipstormPlayerPawn* Pawn = SpawnPawnWithCurves_TS(this, TestWorld, TEXT("TC4"));
        if (!Pawn) { return false; }
        UPlayerLaneMovementComponent* PM = Pawn->MovementComponent.Get();

        PM->current_lane        = EPlayerLane::Left;
        PM->target_lane         = EPlayerLane::Center;
        PM->movement_state      = ERunSlipState::SLIPPING;
        PM->tween_progress      = 0.55f;
        PM->lean_angle          = 3.2f;
        PM->head_lean_angle     = 2.1f;
        PM->arm_lean_angle      = 3.8f;
        PM->slip_complete_count = 5;
        PM->has_queued_input    = true;

        // Act
        BroadcastState_TS(PM, ERunState::RUNNING, ERunState::COMPLETE);

        // Assert
        TestEqual(TEXT("TC4: tween_progress == 0 after snap"), PM->tween_progress, 0.0f);
        TestEqual(TEXT("TC4: movement_state == SETTLED"),
                  static_cast<uint8>(PM->movement_state), static_cast<uint8>(ERunSlipState::SETTLED));
        TestEqual(TEXT("TC4: current_lane == target_lane == Center"),
                  static_cast<uint8>(PM->current_lane), static_cast<uint8>(EPlayerLane::Center));
        TestEqual(TEXT("TC4: lean_angle == 0 (AC-28)"), PM->lean_angle, 0.0f);
        TestEqual(TEXT("TC4: head_lean_angle == 0"),    PM->head_lean_angle, 0.0f);
        TestEqual(TEXT("TC4: arm_lean_angle == 0 (AC-32)"), PM->arm_lean_angle, 0.0f);
        TestFalse(TEXT("TC4: buffer cleared"), PM->has_queued_input);
        TestEqual(TEXT("TC4: slip_complete_count preserved == 5"), PM->slip_complete_count, 5);
        // Root at LaneWorldX(Center) = 0.
        TestTrue(TEXT("TC4: pawn root X ≈ 0 (LaneWorldX Center)"),
                 FMath::Abs(Pawn->GetActorLocation().X) < 0.01f);
        // Mesh at zero relative (both location + rotation).
        const FVector MeshRelLoc = Pawn->MeshComponent->GetRelativeLocation();
        const FRotator MeshRelRot = Pawn->MeshComponent->GetRelativeRotation();
        TestTrue(TEXT("TC4: mesh relative loc == 0"), MeshRelLoc.IsNearlyZero(0.01f));
        TestTrue(TEXT("TC4: mesh relative rot == 0"), MeshRelRot.IsNearlyZero(0.01f));

        TestWorld->DestroyActor(Pawn);
        return true;
    }

    // -----------------------------------------------------------------------
    // TC5 (ac17_aborted_snap_and_reset) — AC-17 (identical to AC-16 with ABORTED).
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("ac17_aborted_snap_and_reset"))
    {
        // Arrange
        UWorld* TestWorld = CreateTestPlayWorld_TS(this, TEXT("TC5"));
        if (!TestWorld) { return false; }
        ASlipstormPlayerPawn* Pawn = SpawnPawnWithCurves_TS(this, TestWorld, TEXT("TC5"));
        if (!Pawn) { return false; }
        UPlayerLaneMovementComponent* PM = Pawn->MovementComponent.Get();

        PM->current_lane   = EPlayerLane::Left;
        PM->target_lane    = EPlayerLane::Center;
        PM->movement_state = ERunSlipState::SLIPPING;
        PM->tween_progress = 0.55f;
        PM->lean_angle     = 3.2f;

        // Act
        BroadcastState_TS(PM, ERunState::RUNNING, ERunState::ABORTED);

        // Assert
        TestEqual(TEXT("TC5: TP == 0 after ABORTED"), PM->tween_progress, 0.0f);
        TestEqual(TEXT("TC5: movement_state == SETTLED"),
                  static_cast<uint8>(PM->movement_state), static_cast<uint8>(ERunSlipState::SETTLED));
        TestEqual(TEXT("TC5: current_lane == Center"),
                  static_cast<uint8>(PM->current_lane), static_cast<uint8>(EPlayerLane::Center));
        TestEqual(TEXT("TC5: lean_angle == 0"), PM->lean_angle, 0.0f);

        TestWorld->DestroyActor(Pawn);
        return true;
    }

    // -----------------------------------------------------------------------
    // TC6 (ac18_countdown_full_reset) — AC-18.
    // COUNTDOWN sets lanes to Center + all counters to zero + F-6 state cleared.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("ac18_countdown_full_reset"))
    {
        // Arrange
        UWorld* TestWorld = CreateTestPlayWorld_TS(this, TEXT("TC6"));
        if (!TestWorld) { return false; }
        ASlipstormPlayerPawn* Pawn = SpawnPawnWithCurves_TS(this, TestWorld, TEXT("TC6"));
        if (!Pawn) { return false; }
        UPlayerLaneMovementComponent* PM = Pawn->MovementComponent.Get();

        PM->current_lane                = EPlayerLane::FarRight;
        PM->target_lane                 = EPlayerLane::FarRight;
        PM->movement_state              = ERunSlipState::SLIPPING;
        PM->tween_progress              = 0.7f;
        PM->slip_complete_count         = 5;
        PM->edge_absorb_trigger_count   = 2;
        PM->commitment_tell_fire_count  = 8;

        // Act
        BroadcastState_TS(PM, ERunState::COMPLETE, ERunState::COUNTDOWN);

        // Assert
        TestEqual(TEXT("TC6: current_lane == Center"),
                  static_cast<uint8>(PM->current_lane), static_cast<uint8>(EPlayerLane::Center));
        TestEqual(TEXT("TC6: target_lane == Center"),
                  static_cast<uint8>(PM->target_lane), static_cast<uint8>(EPlayerLane::Center));
        TestEqual(TEXT("TC6: slip_complete_count == 0"),         PM->slip_complete_count, 0);
        TestEqual(TEXT("TC6: edge_absorb_trigger_count == 0"),   PM->edge_absorb_trigger_count, 0);
        TestEqual(TEXT("TC6: commitment_tell_fire_count == 0"),  PM->commitment_tell_fire_count, 0);
        TestEqual(TEXT("TC6: tween_progress == 0"),              PM->tween_progress, 0.0f);
        TestEqual(TEXT("TC6: movement_state == SETTLED"),
                  static_cast<uint8>(PM->movement_state), static_cast<uint8>(ERunSlipState::SETTLED));

        TestWorld->DestroyActor(Pawn);
        return true;
    }

    // -----------------------------------------------------------------------
    // TC7 (ac31_dead_lean_freeze) — AC-31.
    // DEAD preserves all 3 F-5 lean angles at their pre-DEAD values.
    // Includes a 5-tick post-DEAD loop to regression-guard against future
    // removal of the Rule 5 RUNNING gate (which currently prevents the F-5/F-6
    // co-write from overwriting frozen lean values on post-DEAD ticks).
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("ac31_dead_lean_freeze"))
    {
        // Arrange
        UWorld* TestWorld = CreateTestPlayWorld_TS(this, TEXT("TC7"));
        if (!TestWorld) { return false; }
        ASlipstormPlayerPawn* Pawn = SpawnPawnWithCurves_TS(this, TestWorld, TEXT("TC7"));
        if (!Pawn) { return false; }
        UPlayerLaneMovementComponent* PM = Pawn->MovementComponent.Get();

        UGameInstance* GI = TestWorld->GetGameInstance();
        URunStateMachineSubsystem* RSM =
            GI ? GI->GetSubsystem<URunStateMachineSubsystem>() : nullptr;
        if (!TestNotNull(TEXT("TC7: RSM available"), RSM)) { TestWorld->DestroyActor(Pawn); return false; }

        PM->current_lane    = EPlayerLane::Left;
        PM->target_lane     = EPlayerLane::Center;
        PM->movement_state  = ERunSlipState::SLIPPING;
        PM->tween_progress  = 0.5f;
        PM->lean_angle      = 5.0f;
        PM->head_lean_angle = 4.2f;
        PM->arm_lean_angle  = 4.5f;

        // Act 1 — broadcast DEAD.
        BroadcastState_TS(PM, ERunState::RUNNING, ERunState::DEAD);

        // Assert — all 3 lean angles preserved immediately post-broadcast.
        TestEqual(TEXT("TC7: lean_angle preserved == 5.0° post-DEAD"),      PM->lean_angle, 5.0f);
        TestEqual(TEXT("TC7: head_lean_angle preserved == 4.2° post-DEAD"), PM->head_lean_angle, 4.2f);
        TestEqual(TEXT("TC7: arm_lean_angle preserved == 4.5° post-DEAD"),  PM->arm_lean_angle, 4.5f);

        // Act 2 — set RSM to DEAD and tick 5 times. Rule 5 gate at cpp:209-215
        // returns before the F-5/F-6 co-write when RSM != RUNNING; if a future
        // refactor removes that gate this test will catch the regression.
        RSM->TestOnly_CurrentState = ERunState::DEAD;
        RSM->TestOnly_bPaused      = false;
        RSM->TestOnly_bResumeGrace = false;
        FApp::SetDeltaTime(0.016);
        for (int32 i = 0; i < 5; ++i)
        {
            PM->TickComponent(0.016f, ELevelTick::LEVELTICK_All, nullptr);
        }

        // Assert — lean values STILL preserved after 5 ticks under DEAD.
        TestEqual(TEXT("TC7: lean_angle still 5.0° after 5 post-DEAD ticks (Rule 5 gate held)"),
                  PM->lean_angle, 5.0f);
        TestEqual(TEXT("TC7: head_lean_angle still 4.2° after 5 post-DEAD ticks"),
                  PM->head_lean_angle, 4.2f);
        TestEqual(TEXT("TC7: arm_lean_angle still 4.5° after 5 post-DEAD ticks"),
                  PM->arm_lean_angle, 4.5f);

        TestWorld->DestroyActor(Pawn);
        return true;
    }

    // -----------------------------------------------------------------------
    // TC8 (ac_f6_d_dead_during_f6_tail) — AC-F6-D.
    // F-6 active mid-tail; DEAD; verify F-6 state fields preserved bit-exact.
    // Then tick the component 5 more times; verify F-6 state STILL preserved
    // (F-6 tick advance guard at cpp:~271 blocks progression when RSM is DEAD).
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("ac_f6_d_dead_during_f6_tail"))
    {
        // Arrange
        UWorld* TestWorld = CreateTestPlayWorld_TS(this, TEXT("TC8"));
        if (!TestWorld) { return false; }
        ASlipstormPlayerPawn* Pawn = SpawnPawnWithCurves_TS(this, TestWorld, TEXT("TC8"));
        if (!Pawn) { return false; }
        UPlayerLaneMovementComponent* PM = Pawn->MovementComponent.Get();

        // Get RSM subsystem for setting DEAD state (F-6 tick guard reads via RSMSubsystem).
        UGameInstance* GI = TestWorld->GetGameInstance();
        URunStateMachineSubsystem* RSM =
            GI ? GI->GetSubsystem<URunStateMachineSubsystem>() : nullptr;
        if (!TestNotNull(TEXT("TC8: RSM available"), RSM)) { TestWorld->DestroyActor(Pawn); return false; }

        PM->movement_state          = ERunSlipState::SETTLED;
        PM->current_lane            = EPlayerLane::FarRight;
        PM->target_lane             = EPlayerLane::FarRight;
        PM->edge_absorb_active      = true;
        PM->edge_absorb_progress    = 0.4f;
        PM->edge_absorb_local_timer_s = 0.108f;
        PM->edge_absorb_sign        = -1.0f;

        // Act 1 — broadcast DEAD.
        BroadcastState_TS(PM, ERunState::RUNNING, ERunState::DEAD);

        // Assert — F-6 state preserved bit-exact (all 4 fields).
        TestTrue(TEXT("TC8: edge_absorb_active preserved (true)"),      PM->edge_absorb_active);
        TestEqual(TEXT("TC8: edge_absorb_progress == 0.4 preserved"),   PM->edge_absorb_progress, 0.4f);
        TestEqual(TEXT("TC8: edge_absorb_local_timer_s == 0.108 preserved"),
                  PM->edge_absorb_local_timer_s, 0.108f);
        TestEqual(TEXT("TC8: edge_absorb_sign == -1.0 preserved (AC-F6-D all 4 fields)"),
                  PM->edge_absorb_sign, -1.0f);

        // Act 2 — set RSM to DEAD (so F-6 tick guard reads DEAD) and tick 5 times.
        // If the guard fails, edge_absorb_progress will advance beyond 0.4.
        RSM->TestOnly_CurrentState = ERunState::DEAD;
        RSM->TestOnly_bPaused      = false;
        RSM->TestOnly_bResumeGrace = false;
        FApp::SetDeltaTime(0.016);
        for (int32 i = 0; i < 5; ++i)
        {
            PM->TickComponent(0.016f, ELevelTick::LEVELTICK_All, nullptr);
        }

        // Assert — F-6 state STILL preserved after 5 ticks under DEAD.
        TestTrue(TEXT("TC8: edge_absorb_active still true after 5 ticks under DEAD"),
                 PM->edge_absorb_active);
        TestEqual(TEXT("TC8: edge_absorb_progress still 0.4 (F-6 tick guard held)"),
                  PM->edge_absorb_progress, 0.4f);
        TestEqual(TEXT("TC8: edge_absorb_local_timer_s still 0.108 (guard held)"),
                  PM->edge_absorb_local_timer_s, 0.108f);

        TestWorld->DestroyActor(Pawn);
        return true;
    }

    // -----------------------------------------------------------------------
    // TC9 (ac_counter_preservation_and_f6_reset) — AC-COUNTER-DEAD, AC-COUNTER-COMPLETE,
    // AC-COUNTER-F6-RESET consolidated.
    //
    // Verifies:
    //   - DEAD preserves all 3 counters + F-6 state.
    //   - COMPLETE preserves all 3 counters BUT resets F-6 state.
    //   - COUNTDOWN resets all counters AND F-6 state.
    //   - IDLE preserves counters, resets F-6 state.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("ac_counter_preservation_and_f6_reset"))
    {
        // Helper lambda: set up PM with non-zero counters + active F-6, then broadcast NewState.
        auto RunSubCase = [this](ERunState NewState,
                                 int32 ExpectedSlipCount,
                                 int32 ExpectedEdgeCount,
                                 int32 ExpectedCommitCount,
                                 bool  ExpectF6Preserved,
                                 const TCHAR* Label)
        {
            UWorld* TestWorld = CreateTestPlayWorld_TS(this, Label);
            if (!TestWorld) { return; }
            ASlipstormPlayerPawn* Pawn = SpawnPawnWithCurves_TS(this, TestWorld, Label);
            if (!Pawn) { return; }
            UPlayerLaneMovementComponent* PM = Pawn->MovementComponent.Get();

            PM->slip_complete_count         = 5;
            PM->edge_absorb_trigger_count   = 2;
            PM->commitment_tell_fire_count  = 8;
            PM->edge_absorb_active          = true;
            PM->edge_absorb_progress        = 0.4f;
            PM->edge_absorb_local_timer_s   = 0.108f;
            PM->edge_absorb_sign            = -1.0f; // seed non-zero to verify reset paths

            BroadcastState_TS(PM, ERunState::RUNNING, NewState);

            TestEqual(FString::Printf(TEXT("%s: slip_complete_count"), Label),
                      PM->slip_complete_count, ExpectedSlipCount);
            TestEqual(FString::Printf(TEXT("%s: edge_absorb_trigger_count"), Label),
                      PM->edge_absorb_trigger_count, ExpectedEdgeCount);
            TestEqual(FString::Printf(TEXT("%s: commitment_tell_fire_count"), Label),
                      PM->commitment_tell_fire_count, ExpectedCommitCount);
            TestEqual(FString::Printf(TEXT("%s: edge_absorb_active %s"),
                                       Label, ExpectF6Preserved ? TEXT("preserved") : TEXT("reset")),
                      PM->edge_absorb_active, ExpectF6Preserved);
            if (!ExpectF6Preserved)
            {
                TestEqual(FString::Printf(TEXT("%s: edge_absorb_progress == 0 (F-6 reset)"), Label),
                          PM->edge_absorb_progress, 0.0f);
                TestEqual(FString::Printf(TEXT("%s: edge_absorb_sign == 0 (F-6 reset)"), Label),
                          PM->edge_absorb_sign, 0.0f);
            }
            else
            {
                // DEAD path: sign preserved at -1.0f (AC-F6-D all 4 fields).
                TestEqual(FString::Printf(TEXT("%s: edge_absorb_sign == -1.0 preserved (DEAD)"), Label),
                          PM->edge_absorb_sign, -1.0f);
            }

            TestWorld->DestroyActor(Pawn);
        };

        // DEAD: counters preserved, F-6 preserved.
        RunSubCase(ERunState::DEAD,      /*slip*/5, /*edge*/2, /*commit*/8, /*f6Preserved*/true,  TEXT("TC9-DEAD"));
        // COMPLETE: counters preserved, F-6 reset.
        RunSubCase(ERunState::COMPLETE,  /*slip*/5, /*edge*/2, /*commit*/8, /*f6Preserved*/false, TEXT("TC9-COMPLETE"));
        // ABORTED: same as COMPLETE.
        RunSubCase(ERunState::ABORTED,   /*slip*/5, /*edge*/2, /*commit*/8, /*f6Preserved*/false, TEXT("TC9-ABORTED"));
        // COUNTDOWN: all counters reset to 0, F-6 reset.
        RunSubCase(ERunState::COUNTDOWN, /*slip*/0, /*edge*/0, /*commit*/0, /*f6Preserved*/false, TEXT("TC9-COUNTDOWN"));
        // IDLE: counters preserved, F-6 reset.
        RunSubCase(ERunState::IDLE,      /*slip*/5, /*edge*/2, /*commit*/8, /*f6Preserved*/false, TEXT("TC9-IDLE"));

        return true;
    }

    // -----------------------------------------------------------------------
    // TC10 (ac_ss_b_get_movement_state_default_branch) — AC-SS-B.
    //
    // Verifies GetMovementStateExternal returns SETTLED for both valid states
    // AND for a corrupt ERunSlipState ordinal (cast from uint8 42 via
    // reinterpret bytes — a common corruption pattern).
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("ac_ss_b_get_movement_state_default_branch"))
    {
        // Arrange — NewObject with transient outer to satisfy UE's outer assertion
        // on some UObject construction paths (qa-tester minor).
        UPlayerLaneMovementComponent* PM =
            NewObject<UPlayerLaneMovementComponent>(GetTransientPackage());
        TestNotNull(TEXT("TC10: NewObject succeeded"), PM);
        if (!PM) { return false; }

        // Valid SETTLED
        PM->movement_state = ERunSlipState::SETTLED;
        TestEqual(TEXT("TC10: SETTLED → EMovementState::SETTLED"),
                  static_cast<uint8>(PM->GetMovementStateExternal()),
                  static_cast<uint8>(EMovementState::SETTLED));

        // Valid SLIPPING
        PM->movement_state = ERunSlipState::SLIPPING;
        TestEqual(TEXT("TC10: SLIPPING → EMovementState::SLIPPING"),
                  static_cast<uint8>(PM->GetMovementStateExternal()),
                  static_cast<uint8>(EMovementState::SLIPPING));

        // Corrupt ordinal — AC-SS-B safe fallback. Cast from arbitrary uint8.
        PM->movement_state = static_cast<ERunSlipState>(static_cast<uint8>(42));
        TestEqual(TEXT("TC10: corrupt ordinal (42) → EMovementState::SETTLED (AC-SS-B fallback)"),
                  static_cast<uint8>(PM->GetMovementStateExternal()),
                  static_cast<uint8>(EMovementState::SETTLED));

        return true;
    }

    // -----------------------------------------------------------------------
    // TC11 (ac24_no_slip_midpoint_on_terminal) — AC-24 exclusion.
    //
    // Story spec item 23: "OnSlipMidpoint MUST NOT fire on DEAD/COMPLETE/
    // ABORTED transitions — verified via subscriber spy — assert zero
    // deliveries during the terminal transition."
    //
    // The invariant holds via two mechanisms:
    //   1. HandleStateChanged does not call OnSlipMidpoint.Broadcast (ADR-0007 SD2).
    //   2. Rule 5 gate at cpp:209-215 returns before the co-write / broadcast
    //      section of TickComponent when RSM is not RUNNING.
    //
    // This test binds a lambda to OnSlipMidpoint, drives PM into SLIPPING
    // with TP < 0.5 (about to cross), broadcasts each terminal state, and
    // asserts zero deliveries. Repeats for DEAD, COMPLETE, ABORTED.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("ac24_no_slip_midpoint_on_terminal"))
    {
        auto RunTerminalCase = [this](ERunState TerminalState, const TCHAR* Label)
        {
            // Arrange
            UWorld* TestWorld = CreateTestPlayWorld_TS(this, Label);
            if (!TestWorld) { return; }
            ASlipstormPlayerPawn* Pawn = SpawnPawnWithCurves_TS(this, TestWorld, Label);
            if (!Pawn) { return; }
            UPlayerLaneMovementComponent* PM = Pawn->MovementComponent.Get();

            // Bind spy on OnSlipMidpoint (test-only AddLambda pattern).
            int32 BroadcastCount = 0;
            FDelegateHandle Handle = PM->OnSlipMidpoint.AddLambda(
                [&BroadcastCount](EPlayerLane, EPlayerLane) { ++BroadcastCount; });

            // Arrange SLIPPING at TP=0.3 (below the 0.5 midpoint threshold).
            PM->current_lane   = EPlayerLane::Left;
            PM->target_lane    = EPlayerLane::Center;
            PM->movement_state = ERunSlipState::SLIPPING;
            PM->tween_progress = 0.3f;

            // Act — broadcast the terminal state.
            BroadcastState_TS(PM, ERunState::RUNNING, TerminalState);

            // Assert — zero deliveries during the terminal transition itself.
            TestEqual(FString::Printf(TEXT("%s: OnSlipMidpoint NOT broadcast during terminal"), Label),
                      BroadcastCount, 0);

            // Cleanup spy binding before pawn destruction.
            PM->OnSlipMidpoint.Remove(Handle);
            TestWorld->DestroyActor(Pawn);
        };

        RunTerminalCase(ERunState::DEAD,     TEXT("TC11-DEAD"));
        RunTerminalCase(ERunState::COMPLETE, TEXT("TC11-COMPLETE"));
        RunTerminalCase(ERunState::ABORTED,  TEXT("TC11-ABORTED"));

        return true;
    }

    AddError(FString::Printf(
        TEXT("FPMTerminalStatesTest::RunTest — unknown Parameters: '%s'"),
        *Parameters));
    return false;
}

#endif // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

// Copyright (c) Game Studio. All rights reserved.
// SLIPSTORM — 60-second voxel endless runner, mobile, Unreal Engine 5.7
//
// PMPauseGraceTest.cpp — Story 009 integration tests for HandlePausedChanged
// (logging-only body) + Rule 6 pause/resume/grace freeze + buffer preservation.
//
// Story Type: Integration.
// Tests drive RSM TestOnly_* flags directly (no RSM broadcast simulation
// required — freeze semantics are verified at the tick-body gate level).
//
// Spec:    production/epics/player-movement/story-009-pause-grace.md
// GDD:     design/gdd/player-movement-mechanics.md §3 Rules 5/6,
//          Delegate Handler Bodies HandlePausedChanged, §8 AC-11/12/13/24,
//          AC-COUNTER-PAUSE-RESUME
// ADR:     docs/architecture/adr-0009-player-movement-hosting.md (SD4, SD5)
//          docs/architecture/adr-0007-run-state-machine-hosting.md (SD2)
// TR:      TR-PM-011 (pause branch of Rule 5)
//
// Test category: SLIPSTORM.PlayerMovement.PauseGrace
// Runner: UE Automation Framework, ClientContext | ProductFilter.
//
// Helpers _PG-suffixed to avoid ODR collisions with prior integration tests.
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
#include "Player/ESlipDirection.h"
#include "RunStateMachine/RunStateMachineSubsystem.h"
#include "RunStateMachine/ERunState.h"
#include "RunStateMachine/ERunOutcome.h"
#include "Seam/IHapticDispatch.h"

// ---------------------------------------------------------------------------
// Helpers (_PG suffix to avoid ODR collisions)
// ---------------------------------------------------------------------------

static UCurveFloat* MakeIdentityCurve_PG(UObject* InOuter)
{
    UCurveFloat* C = NewObject<UCurveFloat>(InOuter);
    C->FloatCurve.AddKey(0.0f, 0.0f);
    C->FloatCurve.AddKey(1.0f, 1.0f);
    return C;
}

static UWorld* CreateTestPlayWorld_PG(FAutomationTestBase* T, const TCHAR* Label)
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

static ASlipstormPlayerPawn* SpawnPawnWithCurves_PG(
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
    Pawn->MovementComponent->SlipCurve       = MakeIdentityCurve_PG(World);
    Pawn->MovementComponent->LeanCurve       = MakeIdentityCurve_PG(World);
    Pawn->MovementComponent->EdgeAbsorbCurve = MakeIdentityCurve_PG(World);
    UGameplayStatics::FinishSpawningActor(Pawn, FTransform::Identity);
    return Pawn;
}

static URunStateMachineSubsystem* GetRSM_PG(
    FAutomationTestBase* T,
    UWorld* World,
    const TCHAR* Label)
{
    UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
    URunStateMachineSubsystem* RSM =
        GI ? GI->GetSubsystem<URunStateMachineSubsystem>() : nullptr;
    T->TestNotNull(FString::Printf(TEXT("%s: RSM available"), Label), RSM);
    return RSM;
}

/** Puts RSM into the RUNNING/unpaused state required for PM to accept ticks
 *  and process inputs. */
static void SetRSMRunning_PG(URunStateMachineSubsystem* RSM)
{
    RSM->TestOnly_CurrentState = ERunState::RUNNING;
    RSM->TestOnly_bPaused      = false;
    RSM->TestOnly_bResumeGrace = false;
}

// File-scope haptic spy state for test 8 (plain function pointer — no capture).
static int32 g_HapticFireCount_PG = 0;
static void SpyFire_PG(EHapticEvent) { ++g_HapticFireCount_PG; }
static bool SpyIsEnabled_PG() { return true; }

// ---------------------------------------------------------------------------
// IMPLEMENT_COMPLEX_AUTOMATION_TEST — 9 test commands covering all 9 QA cases.
// ---------------------------------------------------------------------------

IMPLEMENT_COMPLEX_AUTOMATION_TEST(
    FPMPauseGraceTest,
    "SLIPSTORM.PlayerMovement.PauseGrace",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

void FPMPauseGraceTest::GetTests(
    TArray<FString>& OutBeautifiedNames,
    TArray<FString>& OutTestCommands) const
{
    OutBeautifiedNames.Add(TEXT("AC-11 mid-tween pause — TP frozen at 0.4 for 10 ticks"));
    OutTestCommands.Add(TEXT("ac11_mid_tween_pause_freezes_tp"));

    OutBeautifiedNames.Add(TEXT("AC-12 resume_grace — TP frozen at 0.4 for 10 ticks (RSM RUNNING)"));
    OutTestCommands.Add(TEXT("ac12_resume_grace_freezes_tp"));

    OutBeautifiedNames.Add(TEXT("AC-13 grace expiry with buffer flush — tween resumes + queued slip fires same tick"));
    OutTestCommands.Add(TEXT("ac13_grace_expiry_with_buffer_flush"));

    OutBeautifiedNames.Add(TEXT("AC-COUNTER-PAUSE-RESUME — all 3 counters preserved across pause/grace/resume cycle"));
    OutTestCommands.Add(TEXT("ac_counter_pause_resume_preserved"));

    OutBeautifiedNames.Add(TEXT("Rule 6 — buffer preserved across pause (has_queued_input stays true)"));
    OutTestCommands.Add(TEXT("buffer_preserved_across_pause"));

    OutBeautifiedNames.Add(TEXT("AC-F6 tail freeze during pause — edge_absorb_progress + timer unchanged for 10 ticks"));
    OutTestCommands.Add(TEXT("f6_tail_freezes_during_pause"));

    OutBeautifiedNames.Add(TEXT("AC-24 exclusion — OnSlipMidpoint not broadcast during pause; fires once post-resume"));
    OutTestCommands.Add(TEXT("ac24_exclusion_during_pause"));

    OutBeautifiedNames.Add(TEXT("Slip input during pause discarded — buffer not queued, no haptic dispatch"));
    OutTestCommands.Add(TEXT("slip_input_during_pause_discarded"));

    OutBeautifiedNames.Add(TEXT("HandlePausedChanged is logging-only — no state field changes, counter incremented"));
    OutTestCommands.Add(TEXT("handle_paused_changed_is_logging_only"));
}

bool FPMPauseGraceTest::RunTest(const FString& Parameters)
{
    // -----------------------------------------------------------------------
    // TC1 (ac11_mid_tween_pause_freezes_tp) — AC-11.
    //
    // PM SLIPPING Left→Center at TP=0.4. RSM bPaused=true (RUNNING).
    // Tick 10 times at 0.016s. Assert tween_progress == 0.4f bit-exact.
    // Rule 5 gate at cpp:209-215 returns early when IsPaused()==true.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("ac11_mid_tween_pause_freezes_tp"))
    {
        // Arrange
        UWorld* TestWorld = CreateTestPlayWorld_PG(this, TEXT("TC1"));
        if (!TestWorld) { return false; }
        ASlipstormPlayerPawn* Pawn = SpawnPawnWithCurves_PG(this, TestWorld, TEXT("TC1"));
        if (!Pawn) { return false; }
        UPlayerLaneMovementComponent* PM = Pawn->MovementComponent.Get();
        URunStateMachineSubsystem* RSM = GetRSM_PG(this, TestWorld, TEXT("TC1"));
        if (!RSM) { TestWorld->DestroyActor(Pawn); return false; }

        PM->current_lane   = EPlayerLane::Left;
        PM->target_lane    = EPlayerLane::Center;
        PM->movement_state = ERunSlipState::SLIPPING;
        PM->tween_progress = 0.4f;

        RSM->TestOnly_CurrentState = ERunState::RUNNING;
        RSM->TestOnly_bPaused      = true;
        RSM->TestOnly_bResumeGrace = false;
        FApp::SetDeltaTime(0.016);

        // Act — 10 ticks under pause
        for (int32 i = 0; i < 10; ++i)
        {
            PM->TickComponent(0.016f, ELevelTick::LEVELTICK_All, nullptr);
        }

        // Assert
        TestEqual(TEXT("TC1: tween_progress == 0.4 bit-exact after 10 paused ticks (AC-11)"),
                  PM->tween_progress, 0.4f);

        TestWorld->DestroyActor(Pawn);
        return true;
    }

    // -----------------------------------------------------------------------
    // TC2 (ac12_resume_grace_freezes_tp) — AC-12.
    //
    // PM SLIPPING at TP=0.4. RSM state=RUNNING, bPaused=false,
    // bResumeGrace=true. Tick 10 times. Assert TP == 0.4f.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("ac12_resume_grace_freezes_tp"))
    {
        // Arrange
        UWorld* TestWorld = CreateTestPlayWorld_PG(this, TEXT("TC2"));
        if (!TestWorld) { return false; }
        ASlipstormPlayerPawn* Pawn = SpawnPawnWithCurves_PG(this, TestWorld, TEXT("TC2"));
        if (!Pawn) { return false; }
        UPlayerLaneMovementComponent* PM = Pawn->MovementComponent.Get();
        URunStateMachineSubsystem* RSM = GetRSM_PG(this, TestWorld, TEXT("TC2"));
        if (!RSM) { TestWorld->DestroyActor(Pawn); return false; }

        PM->current_lane   = EPlayerLane::Left;
        PM->target_lane    = EPlayerLane::Center;
        PM->movement_state = ERunSlipState::SLIPPING;
        PM->tween_progress = 0.4f;

        RSM->TestOnly_CurrentState = ERunState::RUNNING;
        RSM->TestOnly_bPaused      = false;
        RSM->TestOnly_bResumeGrace = true;
        FApp::SetDeltaTime(0.016);

        // Act — 10 ticks under resume_grace
        for (int32 i = 0; i < 10; ++i)
        {
            PM->TickComponent(0.016f, ELevelTick::LEVELTICK_All, nullptr);
        }

        // Assert
        TestEqual(TEXT("TC2: tween_progress == 0.4 bit-exact after 10 grace ticks (AC-12)"),
                  PM->tween_progress, 0.4f);

        TestWorld->DestroyActor(Pawn);
        return true;
    }

    // -----------------------------------------------------------------------
    // TC3 (ac13_grace_expiry_with_buffer_flush) — AC-13.
    //
    // Pre-pause: PM SLIPPING Center→Right at TP=0.7 with has_queued_input=true,
    // queued_input_direction=Right (buffered before pause).
    //
    // Pause: bPaused=true → 3 freeze ticks.
    // ResumeGrace: bResumeGrace=true → 3 more freeze ticks.
    // Grace expiry: bPaused=false, bResumeGrace=false, RUNNING.
    //
    // Tick with dt=0.1 (effective_dt clamped to MAX_SLIP_DT_S=0.05s).
    // At SLIP_TWEEN=0.15s: advance = 0.05/0.15 ≈ 0.333 → TP 0.7+0.333 = 1.033 > 1.0.
    // CompleteTween fires → slip_complete_count += 1 → FlushBufferedInput →
    // queued Right input fires HandleSlipTransition(Right) → new SLIPPING Center→FarRight.
    //
    // Assert: (a) slip_complete_count == initial+1, (b) movement_state == SLIPPING
    //         (new tween started), (c) has_queued_input == false (buffer consumed),
    //         (d) current_lane == Center (CompleteTween committed Center→Right,
    //              then FlushBufferedInput starts Right→FarRight with current=Center...
    //              wait — CompleteTween sets current_lane=target_lane=Right THEN
    //              FlushBufferedInput calls HandleSlipTransition(Right) from SETTLED.
    //              HandleSlipTransition in SETTLED does F-4: from Right dir=Right →
    //              target_lane=FarRight. current_lane stays Right until next CompleteTween.)
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("ac13_grace_expiry_with_buffer_flush"))
    {
        // Arrange
        UWorld* TestWorld = CreateTestPlayWorld_PG(this, TEXT("TC3"));
        if (!TestWorld) { return false; }
        ASlipstormPlayerPawn* Pawn = SpawnPawnWithCurves_PG(this, TestWorld, TEXT("TC3"));
        if (!Pawn) { return false; }
        UPlayerLaneMovementComponent* PM = Pawn->MovementComponent.Get();
        URunStateMachineSubsystem* RSM = GetRSM_PG(this, TestWorld, TEXT("TC3"));
        if (!RSM) { TestWorld->DestroyActor(Pawn); return false; }

        PM->current_lane           = EPlayerLane::Center;
        PM->target_lane            = EPlayerLane::Right;
        PM->movement_state         = ERunSlipState::SLIPPING;
        PM->tween_progress         = 0.7f;
        PM->has_queued_input       = true;
        PM->queued_input_direction = ESlipDirection::Right;
        PM->slip_complete_count    = 2;

        // Pause phase — freeze 3 ticks
        RSM->TestOnly_CurrentState = ERunState::RUNNING;
        RSM->TestOnly_bPaused      = true;
        RSM->TestOnly_bResumeGrace = false;
        FApp::SetDeltaTime(0.016);
        for (int32 i = 0; i < 3; ++i)
        {
            PM->TickComponent(0.016f, ELevelTick::LEVELTICK_All, nullptr);
        }
        TestEqual(TEXT("TC3: TP still 0.7 after pause ticks"), PM->tween_progress, 0.7f);
        TestTrue(TEXT("TC3: buffer preserved during pause"), PM->has_queued_input);

        // ResumeGrace phase — freeze 3 more ticks
        RSM->TestOnly_bPaused      = false;
        RSM->TestOnly_bResumeGrace = true;
        for (int32 i = 0; i < 3; ++i)
        {
            PM->TickComponent(0.016f, ELevelTick::LEVELTICK_All, nullptr);
        }
        TestEqual(TEXT("TC3: TP still 0.7 after grace ticks"), PM->tween_progress, 0.7f);
        TestTrue(TEXT("TC3: buffer preserved during grace"), PM->has_queued_input);

        // Grace expiry — single tick with large dt (effective_dt clamped to 0.05s)
        // 0.7 + 0.05/0.15 = 0.7 + 0.333 = 1.033 → crosses 1.0 → CompleteTween
        RSM->TestOnly_bPaused      = false;
        RSM->TestOnly_bResumeGrace = false;
        FApp::SetDeltaTime(0.1);

        // Act — single grace-expiry tick
        PM->TickComponent(0.1f, ELevelTick::LEVELTICK_All, nullptr);

        // Assert
        TestEqual(TEXT("TC3: slip_complete_count == 3 (initial 2 + 1 CompleteTween) (AC-13a)"),
                  PM->slip_complete_count, 3);
        TestEqual(TEXT("TC3: movement_state == SLIPPING (flushed Right→FarRight started) (AC-13d)"),
                  static_cast<uint8>(PM->movement_state),
                  static_cast<uint8>(ERunSlipState::SLIPPING));
        TestFalse(TEXT("TC3: has_queued_input == false (buffer consumed by FlushBufferedInput) (AC-13e)"),
                  PM->has_queued_input);
        // After CompleteTween: current_lane=Right, target_lane=Right.
        // After FlushBufferedInput→HandleSlipTransition(Right): target_lane=FarRight.
        TestEqual(TEXT("TC3: current_lane == Right (committed by CompleteTween)"),
                  static_cast<uint8>(PM->current_lane),
                  static_cast<uint8>(EPlayerLane::Right));
        TestEqual(TEXT("TC3: target_lane == FarRight (new tween started by flush)"),
                  static_cast<uint8>(PM->target_lane),
                  static_cast<uint8>(EPlayerLane::FarRight));

        TestWorld->DestroyActor(Pawn);
        return true;
    }

    // -----------------------------------------------------------------------
    // TC4 (ac_counter_pause_resume_preserved) — AC-COUNTER-PAUSE-RESUME.
    //
    // Set 3 counters non-zero. Drive pause → grace → resume cycle (no tween
    // completion). Verify all 3 counters unchanged throughout.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("ac_counter_pause_resume_preserved"))
    {
        // Arrange
        UWorld* TestWorld = CreateTestPlayWorld_PG(this, TEXT("TC4"));
        if (!TestWorld) { return false; }
        ASlipstormPlayerPawn* Pawn = SpawnPawnWithCurves_PG(this, TestWorld, TEXT("TC4"));
        if (!Pawn) { return false; }
        UPlayerLaneMovementComponent* PM = Pawn->MovementComponent.Get();
        URunStateMachineSubsystem* RSM = GetRSM_PG(this, TestWorld, TEXT("TC4"));
        if (!RSM) { TestWorld->DestroyActor(Pawn); return false; }

        PM->slip_complete_count        = 7;
        PM->edge_absorb_trigger_count  = 3;
        PM->commitment_tell_fire_count = 12;
        PM->movement_state             = ERunSlipState::SETTLED;

        // Act — pause cycle (no tween completion occurs)
        RSM->TestOnly_CurrentState = ERunState::RUNNING;
        RSM->TestOnly_bPaused      = true;
        RSM->TestOnly_bResumeGrace = false;
        FApp::SetDeltaTime(0.016);
        for (int32 i = 0; i < 5; ++i)
        {
            PM->TickComponent(0.016f, ELevelTick::LEVELTICK_All, nullptr);
        }

        RSM->TestOnly_bPaused      = false;
        RSM->TestOnly_bResumeGrace = true;
        for (int32 i = 0; i < 5; ++i)
        {
            PM->TickComponent(0.016f, ELevelTick::LEVELTICK_All, nullptr);
        }

        RSM->TestOnly_bPaused      = false;
        RSM->TestOnly_bResumeGrace = false;
        for (int32 i = 0; i < 3; ++i)
        {
            PM->TickComponent(0.016f, ELevelTick::LEVELTICK_All, nullptr);
        }

        // Assert
        TestEqual(TEXT("TC4: slip_complete_count == 7 preserved across pause cycle"),
                  PM->slip_complete_count, 7);
        TestEqual(TEXT("TC4: edge_absorb_trigger_count == 3 preserved across pause cycle"),
                  PM->edge_absorb_trigger_count, 3);
        TestEqual(TEXT("TC4: commitment_tell_fire_count == 12 preserved across pause cycle"),
                  PM->commitment_tell_fire_count, 12);

        TestWorld->DestroyActor(Pawn);
        return true;
    }

    // -----------------------------------------------------------------------
    // TC5 (buffer_preserved_across_pause) — Rule 6 buffer preservation.
    //
    // Buffer queued before pause. Tick 5× while paused. Resume.
    // Assert has_queued_input == true throughout; direction unchanged.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("buffer_preserved_across_pause"))
    {
        // Arrange
        UWorld* TestWorld = CreateTestPlayWorld_PG(this, TEXT("TC5"));
        if (!TestWorld) { return false; }
        ASlipstormPlayerPawn* Pawn = SpawnPawnWithCurves_PG(this, TestWorld, TEXT("TC5"));
        if (!Pawn) { return false; }
        UPlayerLaneMovementComponent* PM = Pawn->MovementComponent.Get();
        URunStateMachineSubsystem* RSM = GetRSM_PG(this, TestWorld, TEXT("TC5"));
        if (!RSM) { TestWorld->DestroyActor(Pawn); return false; }

        // Pre-pause: establish buffered input on a mid-tween PM (TP=0.3 — won't complete)
        PM->current_lane           = EPlayerLane::Left;
        PM->target_lane            = EPlayerLane::Center;
        PM->movement_state         = ERunSlipState::SLIPPING;
        PM->tween_progress         = 0.3f;
        PM->has_queued_input       = true;
        PM->queued_input_direction = ESlipDirection::Right;

        // Act — 5 ticks while paused (TP=0.3 won't cross 1.0 even if gate were open)
        RSM->TestOnly_CurrentState = ERunState::RUNNING;
        RSM->TestOnly_bPaused      = true;
        RSM->TestOnly_bResumeGrace = false;
        FApp::SetDeltaTime(0.016);
        for (int32 i = 0; i < 5; ++i)
        {
            PM->TickComponent(0.016f, ELevelTick::LEVELTICK_All, nullptr);
        }

        // Assert mid-pause
        TestTrue(TEXT("TC5: has_queued_input true after 5 paused ticks"),
                 PM->has_queued_input);
        TestEqual(TEXT("TC5: queued_input_direction == Right after 5 paused ticks"),
                  static_cast<uint8>(PM->queued_input_direction),
                  static_cast<uint8>(ESlipDirection::Right));

        // Resume (still SLIPPING at TP=0.3 — won't complete in 1 tick at 0.016s)
        RSM->TestOnly_bPaused = false;
        PM->TickComponent(0.016f, ELevelTick::LEVELTICK_All, nullptr);

        // Assert post-resume (TP < 1.0, so buffer still held)
        TestTrue(TEXT("TC5: has_queued_input still true after resume tick (TP < 1.0)"),
                 PM->has_queued_input);
        TestEqual(TEXT("TC5: queued_input_direction == Right after resume tick"),
                  static_cast<uint8>(PM->queued_input_direction),
                  static_cast<uint8>(ESlipDirection::Right));

        TestWorld->DestroyActor(Pawn);
        return true;
    }

    // -----------------------------------------------------------------------
    // TC6 (f6_tail_freezes_during_pause) — F-6 freeze under pause.
    //
    // F-6 active at edge_absorb_progress=0.3, edge_absorb_local_timer_s=0.081.
    // Pause. Tick 10×. Assert both fields unchanged (Rule 5 gate blocks F-6
    // tick advance which lives inside the gate at cpp:270+ per Story 007/008).
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("f6_tail_freezes_during_pause"))
    {
        // Arrange
        UWorld* TestWorld = CreateTestPlayWorld_PG(this, TEXT("TC6"));
        if (!TestWorld) { return false; }
        ASlipstormPlayerPawn* Pawn = SpawnPawnWithCurves_PG(this, TestWorld, TEXT("TC6"));
        if (!Pawn) { return false; }
        UPlayerLaneMovementComponent* PM = Pawn->MovementComponent.Get();
        URunStateMachineSubsystem* RSM = GetRSM_PG(this, TestWorld, TEXT("TC6"));
        if (!RSM) { TestWorld->DestroyActor(Pawn); return false; }

        PM->movement_state             = ERunSlipState::SETTLED;
        PM->current_lane               = EPlayerLane::FarLeft;
        PM->target_lane                = EPlayerLane::FarLeft;
        PM->edge_absorb_active         = true;
        PM->edge_absorb_progress       = 0.3f;
        PM->edge_absorb_local_timer_s  = 0.081f;
        PM->edge_absorb_sign           = 1.0f;

        // Act — 10 ticks while paused
        RSM->TestOnly_CurrentState = ERunState::RUNNING;
        RSM->TestOnly_bPaused      = true;
        RSM->TestOnly_bResumeGrace = false;
        FApp::SetDeltaTime(0.016);
        for (int32 i = 0; i < 10; ++i)
        {
            PM->TickComponent(0.016f, ELevelTick::LEVELTICK_All, nullptr);
        }

        // Assert
        TestEqual(TEXT("TC6: edge_absorb_progress == 0.3 unchanged after 10 paused ticks"),
                  PM->edge_absorb_progress, 0.3f);
        TestEqual(TEXT("TC6: edge_absorb_local_timer_s == 0.081 unchanged after 10 paused ticks"),
                  PM->edge_absorb_local_timer_s, 0.081f);

        TestWorld->DestroyActor(Pawn);
        return true;
    }

    // -----------------------------------------------------------------------
    // TC7 (ac24_exclusion_during_pause) — AC-24 exclusion during pause.
    //
    // PM SLIPPING at TP=0.45 (below midpoint). Pause. Tick 5× at dt that would
    // push TP past 0.5 if unpaused. Assert OnSlipMidpoint spy count == 0.
    // Then unpause and tick once with dt to push TP past 0.5. Assert count == 1
    // (broadcast fires exactly once post-resume, NOT retroactively).
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("ac24_exclusion_during_pause"))
    {
        // Arrange
        UWorld* TestWorld = CreateTestPlayWorld_PG(this, TEXT("TC7"));
        if (!TestWorld) { return false; }
        ASlipstormPlayerPawn* Pawn = SpawnPawnWithCurves_PG(this, TestWorld, TEXT("TC7"));
        if (!Pawn) { return false; }
        UPlayerLaneMovementComponent* PM = Pawn->MovementComponent.Get();
        URunStateMachineSubsystem* RSM = GetRSM_PG(this, TestWorld, TEXT("TC7"));
        if (!RSM) { TestWorld->DestroyActor(Pawn); return false; }

        int32 MidpointBroadcastCount = 0;
        FDelegateHandle Handle = PM->OnSlipMidpoint.AddLambda(
            [&MidpointBroadcastCount](EPlayerLane, EPlayerLane) { ++MidpointBroadcastCount; });

        PM->current_lane   = EPlayerLane::Left;
        PM->target_lane    = EPlayerLane::Center;
        PM->movement_state = ERunSlipState::SLIPPING;
        PM->tween_progress = 0.45f;

        // Act — 5 ticks while paused (would push past 0.5 unpaused)
        RSM->TestOnly_CurrentState = ERunState::RUNNING;
        RSM->TestOnly_bPaused      = true;
        RSM->TestOnly_bResumeGrace = false;
        // At SLIP_TWEEN=0.15, effective_dt clamped to 0.05s: advance = 0.05/0.15 ≈ 0.333
        // Un-paused, first tick alone would push TP from 0.45 to 0.783. But gated out.
        FApp::SetDeltaTime(0.016);
        for (int32 i = 0; i < 5; ++i)
        {
            PM->TickComponent(0.016f, ELevelTick::LEVELTICK_All, nullptr);
        }

        // Assert — no broadcast while paused
        TestEqual(TEXT("TC7: OnSlipMidpoint NOT broadcast during 5 paused ticks (AC-24)"),
                  MidpointBroadcastCount, 0);
        TestEqual(TEXT("TC7: TP still 0.45 (frozen while paused)"),
                  PM->tween_progress, 0.45f);

        // Unpause. dt=0.1s applies ONLY to this single post-unpause tick
        // (the 5 paused ticks above used dt=0.016s). effective_dt clamped to
        // MAX_SLIP_DT_S=0.05 → advance=0.05/0.15≈0.333 → TP crosses from 0.45 to ≈0.783.
        RSM->TestOnly_bPaused = false;
        FApp::SetDeltaTime(0.1);
        PM->TickComponent(0.1f, ELevelTick::LEVELTICK_All, nullptr);

        // Assert — exactly 1 broadcast post-resume when TP crosses 0.5
        TestEqual(TEXT("TC7: OnSlipMidpoint fires exactly once post-resume when TP crosses 0.5"),
                  MidpointBroadcastCount, 1);

        PM->OnSlipMidpoint.Remove(Handle);
        TestWorld->DestroyActor(Pawn);
        return true;
    }

    // -----------------------------------------------------------------------
    // TC8 (slip_input_during_pause_discarded) — Rule 5 discard during pause.
    //
    // PM SLIPPING + RSM bPaused=true. Call HandleSlipTransition(Left).
    // Assert has_queued_input == false (not queued) and no haptic dispatch.
    // HandleSlipTransition's Rule 5 gate checks IsPaused() and discards.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("slip_input_during_pause_discarded"))
    {
        // Arrange
        UWorld* TestWorld = CreateTestPlayWorld_PG(this, TEXT("TC8"));
        if (!TestWorld) { return false; }
        ASlipstormPlayerPawn* Pawn = SpawnPawnWithCurves_PG(this, TestWorld, TEXT("TC8"));
        if (!Pawn) { return false; }
        UPlayerLaneMovementComponent* PM = Pawn->MovementComponent.Get();
        URunStateMachineSubsystem* RSM = GetRSM_PG(this, TestWorld, TEXT("TC8"));
        if (!RSM) { TestWorld->DestroyActor(Pawn); return false; }

        PM->current_lane   = EPlayerLane::Center;
        PM->target_lane    = EPlayerLane::Right;
        PM->movement_state = ERunSlipState::SLIPPING;
        PM->tween_progress = 0.5f;
        PM->has_queued_input = false;

        RSM->TestOnly_CurrentState = ERunState::RUNNING;
        RSM->TestOnly_bPaused      = true;
        RSM->TestOnly_bResumeGrace = false;

        // Install haptic spy (file-scope counter + plain function pointer)
        g_HapticFireCount_PG = 0;
        IHapticDispatch::TestOnly_SetFireFn(SpyFire_PG);
        IHapticDispatch::TestOnly_SetIsEnabledFn(SpyIsEnabled_PG);
        ON_SCOPE_EXIT { IHapticDispatch::TestOnly_Reset(); };

        // Act
        PM->HandleSlipTransition(ESlipDirection::Left);

        // Assert
        TestFalse(TEXT("TC8: has_queued_input == false (Rule 5 discards during pause)"),
                  PM->has_queued_input);
        TestEqual(TEXT("TC8: haptic dispatch count == 0 (Rule 5 gate fires before haptic path)"),
                  g_HapticFireCount_PG, 0);
        TestEqual(TEXT("TC8: movement_state unchanged == SLIPPING"),
                  static_cast<uint8>(PM->movement_state),
                  static_cast<uint8>(ERunSlipState::SLIPPING));

        TestWorld->DestroyActor(Pawn);
        return true;
    }

    // -----------------------------------------------------------------------
    // TC9 (handle_paused_changed_is_logging_only) — logging-only body.
    //
    // Snapshot ALL critical state fields. Call HandlePausedChanged(true, 0.0)
    // directly via friend access. Verify:
    //   (a) HandlePausedChanged_TestOnlyCallCount incremented by 1.
    //   (b) Every other state field is unchanged.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("handle_paused_changed_is_logging_only"))
    {
        // Arrange
        UWorld* TestWorld = CreateTestPlayWorld_PG(this, TEXT("TC9"));
        if (!TestWorld) { return false; }
        ASlipstormPlayerPawn* Pawn = SpawnPawnWithCurves_PG(this, TestWorld, TEXT("TC9"));
        if (!Pawn) { return false; }
        UPlayerLaneMovementComponent* PM = Pawn->MovementComponent.Get();

        // Seed non-zero/non-default state across all critical fields
        PM->current_lane               = EPlayerLane::Left;
        PM->target_lane                = EPlayerLane::Center;
        PM->movement_state             = ERunSlipState::SLIPPING;
        PM->tween_progress             = 0.6f;
        PM->lean_angle                 = 4.5f;
        PM->head_lean_angle            = 3.2f;
        PM->arm_lean_angle             = 4.8f;
        PM->has_queued_input           = true;
        PM->queued_input_direction     = ESlipDirection::Right;
        PM->slip_complete_count        = 5;
        PM->edge_absorb_trigger_count  = 2;
        PM->commitment_tell_fire_count = 9;
        PM->edge_absorb_active         = true;
        PM->edge_absorb_progress       = 0.35f;
        PM->edge_absorb_local_timer_s  = 0.095f;
        PM->edge_absorb_sign           = -1.0f; // seed non-default value for logging-only proof

        const int32 CounterBefore = PM->HandlePausedChanged_TestOnlyCallCount;

        // Act — invoke logging-only body directly
        PM->HandlePausedChanged(true, 0.0);

        // Assert (a) — counter incremented
        TestEqual(TEXT("TC9: HandlePausedChanged_TestOnlyCallCount incremented by 1"),
                  PM->HandlePausedChanged_TestOnlyCallCount, CounterBefore + 1);

        // Assert (b) — no state field mutation
        TestEqual(TEXT("TC9: current_lane unchanged"),
                  static_cast<uint8>(PM->current_lane),
                  static_cast<uint8>(EPlayerLane::Left));
        TestEqual(TEXT("TC9: target_lane unchanged"),
                  static_cast<uint8>(PM->target_lane),
                  static_cast<uint8>(EPlayerLane::Center));
        TestEqual(TEXT("TC9: movement_state unchanged == SLIPPING"),
                  static_cast<uint8>(PM->movement_state),
                  static_cast<uint8>(ERunSlipState::SLIPPING));
        TestEqual(TEXT("TC9: tween_progress unchanged == 0.6"),
                  PM->tween_progress, 0.6f);
        TestEqual(TEXT("TC9: lean_angle unchanged == 4.5"), PM->lean_angle, 4.5f);
        TestEqual(TEXT("TC9: head_lean_angle unchanged == 3.2"), PM->head_lean_angle, 3.2f);
        TestEqual(TEXT("TC9: arm_lean_angle unchanged == 4.8"), PM->arm_lean_angle, 4.8f);
        TestTrue(TEXT("TC9: has_queued_input unchanged == true"), PM->has_queued_input);
        TestEqual(TEXT("TC9: queued_input_direction unchanged == Right"),
                  static_cast<uint8>(PM->queued_input_direction),
                  static_cast<uint8>(ESlipDirection::Right));
        TestEqual(TEXT("TC9: slip_complete_count unchanged == 5"),
                  PM->slip_complete_count, 5);
        TestEqual(TEXT("TC9: edge_absorb_trigger_count unchanged == 2"),
                  PM->edge_absorb_trigger_count, 2);
        TestEqual(TEXT("TC9: commitment_tell_fire_count unchanged == 9"),
                  PM->commitment_tell_fire_count, 9);
        TestTrue(TEXT("TC9: edge_absorb_active unchanged == true"), PM->edge_absorb_active);
        TestEqual(TEXT("TC9: edge_absorb_progress unchanged == 0.35"),
                  PM->edge_absorb_progress, 0.35f);
        TestEqual(TEXT("TC9: edge_absorb_local_timer_s unchanged == 0.095"),
                  PM->edge_absorb_local_timer_s, 0.095f);
        // Complete the "EVERY state field unchanged" contract per qa-tester review.
        TestEqual(TEXT("TC9: edge_absorb_sign unchanged == -1.0 (logging-only body)"),
                  PM->edge_absorb_sign, -1.0f);

        TestWorld->DestroyActor(Pawn);
        return true;
    }

    AddError(FString::Printf(
        TEXT("FPMPauseGraceTest::RunTest — unknown Parameters: '%s'"),
        *Parameters));
    return false;
}

#endif // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

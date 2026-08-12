// Copyright (c) Game Studio. All rights reserved.
// SLIPSTORM — 60-second voxel endless runner, mobile, Unreal Engine 5.7
//
// PMInputBufferTest.cpp — Story 005 integration tests for the single-slot
// input buffer + Rule 3 buffer-drop feedback + Rule 11 discard.
//
// Location: Integration/ rather than Unit/ because HandleSlipTransition's Rule 5
// gate reads RSMSubsystem->GetCurrentState() — obtaining an RSMSubsystem instance
// without a UWorld is not practical, and the deviation from the story's stated
// aspirational unit path is documented in Story 005 Completion Notes.
//
// Spec:   production/epics/player-movement/story-005-input-buffer.md
// GDD:    design/gdd/player-movement-mechanics.md §3 Rules 3/4/11, §4 F-4, §8 AC-04/05/06/07/19/25
// ADR:    docs/architecture/adr-0009-player-movement-hosting.md (SD1)
//         docs/architecture/adr-0002-haptic-platform-bridge.md (INT-002-amended)
// TR:     TR-PM-012
//
// Test category: SLIPSTORM.PlayerMovement.InputBuffer
// Runner: UE Automation Framework, ClientContext | ProductFilter.
//
// Uses CreateTestPlayWorld / SpawnPawnWithCurves / SetRSMRunning helpers
// duplicated from PMStateMachineTest.cpp (not refactored to a shared header
// per prior story-scope discipline).
//
// Spy pattern: IHapticDispatch::TestOnly_SetFireFn / TestOnly_SetIsEnabledFn
// install lambdas that record dispatched events + gate state to a module-scope
// TArray + bool. ON_SCOPE_EXIT restores the null default in teardown.
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
#include "Seam/IHapticDispatch.h"

// ---------------------------------------------------------------------------
// Spy state — module-scope so the C-linkage function-pointer spy can write to it.
// Reset explicitly at the start of every test branch to avoid cross-test leakage.
// ---------------------------------------------------------------------------

static TArray<EHapticEvent> GSpyDispatchLog;
static bool                 GSpyHapticsEnabled = true;

static void SpyFireFn(EHapticEvent Event)
{
    GSpyDispatchLog.Add(Event);
}

static bool SpyIsEnabledFn()
{
    return GSpyHapticsEnabled;
}

static void ResetSpy()
{
    GSpyDispatchLog.Reset();
    GSpyHapticsEnabled = true;
}

// ---------------------------------------------------------------------------
// Helpers (duplicated from PMStateMachineTest.cpp — scope-consistent with
// Story 004 CC1-CC3 helpers). _IB suffix to avoid ODR collisions.
// ---------------------------------------------------------------------------

static UCurveFloat* MakeValidCurve_IB(UObject* InOuter)
{
    UCurveFloat* C = NewObject<UCurveFloat>(InOuter);
    C->FloatCurve.AddKey(0.0f, 0.0f);
    C->FloatCurve.AddKey(1.0f, 1.0f);
    return C;
}

// S1-04 harness fix: FTestWorldWrapper canonical UE pattern (Engine/Source/Runtime/Engine/Public/Tests/AutomationCommon.h).
// See PMStateMachineTest.cpp CreateTestPlayWorld_SM for full rationale + engine citation.
// Wrapper is stack-allocated per TC; destructor handles all teardown (GI Shutdown + DestroyWorldContext).
static UWorld* CreateTestPlayWorld_IB(FAutomationTestBase* T, FTestWorldWrapper& WorldWrapper, const TCHAR* Label)
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

static ASlipstormPlayerPawn* SpawnPawnWithCurves_IB(
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
    Pawn->MovementComponent->SlipCurve       = MakeValidCurve_IB(World);
    Pawn->MovementComponent->LeanCurve       = MakeValidCurve_IB(World);
    Pawn->MovementComponent->EdgeAbsorbCurve = MakeValidCurve_IB(World);
    UGameplayStatics::FinishSpawningActor(Pawn, FTransform::Identity);
    return Pawn;
}

static URunStateMachineSubsystem* GetRSM_IB(
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

static void SetRSMRunning_IB(URunStateMachineSubsystem* RSM)
{
    RSM->TestOnly_CurrentState = ERunState::RUNNING;
    RSM->TestOnly_bPaused      = false;
    RSM->TestOnly_bResumeGrace = false;
}

// ---------------------------------------------------------------------------
// IMPLEMENT_COMPLEX_AUTOMATION_TEST — 7 test commands per Story 005 QA Test Cases.
// ---------------------------------------------------------------------------

IMPLEMENT_COMPLEX_AUTOMATION_TEST(
    FPMInputBufferTest,
    "SLIPSTORM.PlayerMovement.InputBuffer",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

void FPMInputBufferTest::GetTests(
    TArray<FString>& OutBeautifiedNames,
    TArray<FString>& OutTestCommands) const
{
    OutBeautifiedNames.Add(TEXT("AC-04 buffer flush on completion — same tick, no idle frame"));
    OutTestCommands.Add(TEXT("ac04_buffer_flush_on_completion"));

    OutBeautifiedNames.Add(TEXT("AC-05 second input dropped when buffer full"));
    OutTestCommands.Add(TEXT("ac05_second_input_dropped_when_full"));

    OutBeautifiedNames.Add(TEXT("AC-06 edge-check discard on buffered input"));
    OutTestCommands.Add(TEXT("ac06_edge_check_discard_on_buffered_input"));

    OutBeautifiedNames.Add(TEXT("AC-07 slip_complete_count increments across flushed tween"));
    OutTestCommands.Add(TEXT("ac07_counter_on_flush"));

    OutBeautifiedNames.Add(TEXT("AC-19 DiscardBuffer clears has_queued_input"));
    OutTestCommands.Add(TEXT("ac19_buffer_discard_on_non_running_state_entry"));

    OutBeautifiedNames.Add(TEXT("AC-25 drop feedback synchronous within event call"));
    OutTestCommands.Add(TEXT("ac25_drop_feedback_synchronous"));

    OutBeautifiedNames.Add(TEXT("Rule 3 idempotence — three drops leave queued value unchanged"));
    OutTestCommands.Add(TEXT("rule3_idempotence"));

    OutBeautifiedNames.Add(TEXT("Rule 5 gate blocks buffer path when RSM not RUNNING (paused)"));
    OutTestCommands.Add(TEXT("rule5_gate_blocks_buffer_when_not_running"));
}

bool FPMInputBufferTest::RunTest(const FString& Parameters)
{
    // -----------------------------------------------------------------------
    // AC-04 (ac04_buffer_flush_on_completion) — buffered slip fires SAME TICK
    // on CompleteTween; no idle frame; new tween begins immediately.
    //
    // Sequence:
    //   1. SETTLED at Center. HandleSlipTransition(Right) → SLIPPING Center→Right.
    //   2. Second HandleSlipTransition(Right) WHILE SLIPPING → buffered.
    //   3. Force tween_progress to 1.0 and call CompleteTween manually — its tail
    //      calls FlushBufferedInput, which re-enters HandleSlipTransition on the
    //      buffered direction. The re-entry hits the SETTLED path (F-4 valid,
    //      collision commit, SLIPPING).
    //   Result: same-tick flush produces a second SLIPPING transition; state ends
    //   with movement_state == SLIPPING, current_lane == Right, target_lane ==
    //   FarRight, has_queued_input == false.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("ac04_buffer_flush_on_completion"))
    {
        FTestWorldWrapper WorldWrapper;
        UWorld* TestWorld = CreateTestPlayWorld_IB(this, WorldWrapper, TEXT("AC-04"));
        if (!TestWorld) { return false; }
        ASlipstormPlayerPawn* Pawn = SpawnPawnWithCurves_IB(this, TestWorld, TEXT("AC-04"));
        if (!Pawn) { return false; }
        URunStateMachineSubsystem* RSM = GetRSM_IB(this, TestWorld, TEXT("AC-04"));
        if (!RSM) { return false; }

        UPlayerLaneMovementComponent* PM = Pawn->MovementComponent.Get();
        SetRSMRunning_IB(RSM);

        // 1. Initial slip Center → Right.
        PM->HandleSlipTransition(ESlipDirection::Right);
        TestEqual(TEXT("AC-04.1: movement_state == SLIPPING after initial slip"),
                  static_cast<uint8>(PM->movement_state), static_cast<uint8>(ERunSlipState::SLIPPING));
        TestEqual(TEXT("AC-04.1: target_lane == Right"),
                  static_cast<uint8>(PM->target_lane), static_cast<uint8>(EPlayerLane::Right));

        // 2. Second Right while SLIPPING → buffered.
        PM->HandleSlipTransition(ESlipDirection::Right);
        TestTrue(TEXT("AC-04.2: has_queued_input after buffered second Right"), PM->has_queued_input);
        TestEqual(TEXT("AC-04.2: queued direction == Right"),
                  static_cast<uint8>(PM->queued_input_direction), static_cast<uint8>(ESlipDirection::Right));

        // 3. Force tween completion + flush (same tick).
        PM->tween_progress = 1.0f;
        PM->CompleteTween();

        // 4. Assertions post-flush.
        TestFalse(TEXT("AC-04.4: has_queued_input cleared after flush"), PM->has_queued_input);
        TestEqual(TEXT("AC-04.4: movement_state == SLIPPING (buffered slip started same tick)"),
                  static_cast<uint8>(PM->movement_state), static_cast<uint8>(ERunSlipState::SLIPPING));
        TestEqual(TEXT("AC-04.4: current_lane == Right (source of buffered slip)"),
                  static_cast<uint8>(PM->current_lane), static_cast<uint8>(EPlayerLane::Right));
        TestEqual(TEXT("AC-04.4: target_lane == FarRight (buffered slip destination)"),
                  static_cast<uint8>(PM->target_lane), static_cast<uint8>(EPlayerLane::FarRight));

        TestWorld->DestroyActor(Pawn);
        return true;
    }

    // -----------------------------------------------------------------------
    // AC-05 (ac05_second_input_dropped_when_full) — SLIPPING + has_queued_input=true.
    // Second HandleSlipTransition leaves queue unchanged; haptic + audio dispatched
    // exactly once.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("ac05_second_input_dropped_when_full"))
    {
        FTestWorldWrapper WorldWrapper;
        UWorld* TestWorld = CreateTestPlayWorld_IB(this, WorldWrapper, TEXT("AC-05"));
        if (!TestWorld) { return false; }
        ASlipstormPlayerPawn* Pawn = SpawnPawnWithCurves_IB(this, TestWorld, TEXT("AC-05"));
        if (!Pawn) { return false; }
        URunStateMachineSubsystem* RSM = GetRSM_IB(this, TestWorld, TEXT("AC-05"));
        if (!RSM) { return false; }

        UPlayerLaneMovementComponent* PM = Pawn->MovementComponent.Get();
        SetRSMRunning_IB(RSM);

        // Install haptic spy.
        ResetSpy();
        IHapticDispatch::TestOnly_SetFireFn(&SpyFireFn);
        IHapticDispatch::TestOnly_SetIsEnabledFn(&SpyIsEnabledFn);
        ON_SCOPE_EXIT { IHapticDispatch::TestOnly_Reset(); };

        // Arrange SLIPPING with buffer full (Right queued).
        PM->HandleSlipTransition(ESlipDirection::Right); // Center → Right, sets SLIPPING
        PM->HandleSlipTransition(ESlipDirection::Right); // buffered
        TestTrue(TEXT("AC-05 pre: buffer set"), PM->has_queued_input);

        // Second Left → Rule 3 drop; queue unchanged; haptic + audio fire once.
        const int32 AudioStingCountBefore = PM->BufferDropAudioSting_TestOnlyCallCount;
        PM->HandleSlipTransition(ESlipDirection::Left);

        TestTrue(TEXT("AC-05.1: buffer still full after drop"), PM->has_queued_input);
        TestEqual(TEXT("AC-05.1: queued direction UNCHANGED (still Right)"),
                  static_cast<uint8>(PM->queued_input_direction), static_cast<uint8>(ESlipDirection::Right));

        TestEqual(TEXT("AC-05.2: SpyLog contains exactly one BufferDrop"),
                  GSpyDispatchLog.Num(), 1);
        if (GSpyDispatchLog.Num() >= 1)
        {
            TestEqual(TEXT("AC-05.2: SpyLog[0] == BufferDrop"),
                      static_cast<uint8>(GSpyDispatchLog[0]), static_cast<uint8>(EHapticEvent::BufferDrop));
        }
        TestEqual(TEXT("AC-05.3: audio sting counter incremented exactly once"),
                  PM->BufferDropAudioSting_TestOnlyCallCount, AudioStingCountBefore + 1);

        // Edge case: haptics disabled system-wide → haptic skipped, audio still fires.
        ResetSpy();
        GSpyHapticsEnabled = false;
        const int32 AudioStingCountBeforeGate = PM->BufferDropAudioSting_TestOnlyCallCount;
        PM->HandleSlipTransition(ESlipDirection::Left);
        TestEqual(TEXT("AC-05.4: no haptic dispatch when IsSystemHapticsEnabled=false"),
                  GSpyDispatchLog.Num(), 0);
        TestEqual(TEXT("AC-05.4: audio sting still fires when haptics gated off"),
                  PM->BufferDropAudioSting_TestOnlyCallCount, AudioStingCountBeforeGate + 1);

        TestWorld->DestroyActor(Pawn);
        return true;
    }

    // -----------------------------------------------------------------------
    // AC-06 (ac06_edge_check_discard_on_buffered_input) — SLIPPING Left→FarLeft
    // (target_lane = FarLeft). Buffered slip-Left projects FarLeft going Left =
    // off-track. F-4 rejects. TriggerEdgeAbsorb fires. Buffer NOT set. NO haptic
    // BufferDrop (edge no-op is a separate feedback path — Story 007).
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("ac06_edge_check_discard_on_buffered_input"))
    {
        FTestWorldWrapper WorldWrapper;
        UWorld* TestWorld = CreateTestPlayWorld_IB(this, WorldWrapper, TEXT("AC-06"));
        if (!TestWorld) { return false; }
        ASlipstormPlayerPawn* Pawn = SpawnPawnWithCurves_IB(this, TestWorld, TEXT("AC-06"));
        if (!Pawn) { return false; }
        URunStateMachineSubsystem* RSM = GetRSM_IB(this, TestWorld, TEXT("AC-06"));
        if (!RSM) { return false; }

        UPlayerLaneMovementComponent* PM = Pawn->MovementComponent.Get();
        SetRSMRunning_IB(RSM);

        ResetSpy();
        IHapticDispatch::TestOnly_SetFireFn(&SpyFireFn);
        IHapticDispatch::TestOnly_SetIsEnabledFn(&SpyIsEnabledFn);
        ON_SCOPE_EXIT { IHapticDispatch::TestOnly_Reset(); };

        // Arrange SLIPPING Left→FarLeft (target_lane == FarLeft).
        PM->current_lane   = EPlayerLane::Left;
        PM->target_lane    = EPlayerLane::FarLeft;
        PM->movement_state = ERunSlipState::SLIPPING;
        PM->tween_progress = 0.3f;
        PM->has_queued_input = false;
        const int32 EdgeAbsorbBefore  = PM->edge_absorb_trigger_count;
        const int32 AudioStingBefore  = PM->BufferDropAudioSting_TestOnlyCallCount;

        // Buffered Left → F-4 sees FarLeft-going-Left as off-track (would leave the lane range).
        PM->HandleSlipTransition(ESlipDirection::Left);

        TestFalse(TEXT("AC-06.1: buffer NOT set (F-4 discard)"), PM->has_queued_input);
        TestEqual(TEXT("AC-06.2: NO haptic BufferDrop (edge no-op, not buffer-drop)"),
                  GSpyDispatchLog.Num(), 0);
        TestEqual(TEXT("AC-06.2: NO audio BufferDrop sting"),
                  PM->BufferDropAudioSting_TestOnlyCallCount, AudioStingBefore);
        // Story 007's TriggerEdgeAbsorb currently a stub — it does NOT yet increment
        // edge_absorb_trigger_count. Verify the counter unchanged to guard against
        // an accidental Story 005 side-effect.
        TestEqual(TEXT("AC-06.3: edge_absorb_trigger_count unchanged (Story 007 stub)"),
                  PM->edge_absorb_trigger_count, EdgeAbsorbBefore);

        TestWorld->DestroyActor(Pawn);
        return true;
    }

    // -----------------------------------------------------------------------
    // AC-07 (ac07_counter_on_flush) — sequence: initial slip Right + buffered Right
    // → after both tweens complete, slip_complete_count == 2, current_lane == FarRight.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("ac07_counter_on_flush"))
    {
        FTestWorldWrapper WorldWrapper;
        UWorld* TestWorld = CreateTestPlayWorld_IB(this, WorldWrapper, TEXT("AC-07"));
        if (!TestWorld) { return false; }
        ASlipstormPlayerPawn* Pawn = SpawnPawnWithCurves_IB(this, TestWorld, TEXT("AC-07"));
        if (!Pawn) { return false; }
        URunStateMachineSubsystem* RSM = GetRSM_IB(this, TestWorld, TEXT("AC-07"));
        if (!RSM) { return false; }

        UPlayerLaneMovementComponent* PM = Pawn->MovementComponent.Get();
        SetRSMRunning_IB(RSM);

        // Reset counters (BeginPlay defaults them, but explicit for the record).
        PM->slip_complete_count = 0;

        // 1. Initial slip Center → Right. Completes first tween.
        PM->HandleSlipTransition(ESlipDirection::Right);
        // 2. Second slip Right → buffered (will become Right→FarRight after flush).
        PM->HandleSlipTransition(ESlipDirection::Right);
        // 3. Complete first tween → flush fires → SLIPPING Right→FarRight.
        PM->tween_progress = 1.0f;
        PM->CompleteTween();
        // 4. Complete the buffered tween.
        PM->tween_progress = 1.0f;
        PM->CompleteTween();

        TestEqual(TEXT("AC-07: slip_complete_count == 2"), PM->slip_complete_count, 2);
        TestEqual(TEXT("AC-07: current_lane == FarRight after both tweens complete"),
                  static_cast<uint8>(PM->current_lane), static_cast<uint8>(EPlayerLane::FarRight));
        TestEqual(TEXT("AC-07: target_lane == FarRight (== current when SETTLED)"),
                  static_cast<uint8>(PM->target_lane), static_cast<uint8>(EPlayerLane::FarRight));
        TestEqual(TEXT("AC-07: movement_state == SETTLED after final CompleteTween"),
                  static_cast<uint8>(PM->movement_state), static_cast<uint8>(ERunSlipState::SETTLED));

        TestWorld->DestroyActor(Pawn);
        return true;
    }

    // -----------------------------------------------------------------------
    // AC-19 (ac19_buffer_discard_on_non_running_state_entry) — DiscardBuffer()
    // clears has_queued_input. Integration with HandleStateChanged is Story 008's
    // scope; this test verifies the helper contract in isolation.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("ac19_buffer_discard_on_non_running_state_entry"))
    {
        UPlayerLaneMovementComponent* PM = NewObject<UPlayerLaneMovementComponent>();
        TestNotNull(TEXT("AC-19: NewObject succeeded"), PM);
        if (!PM) { return false; }

        PM->has_queued_input       = true;
        PM->queued_input_direction = ESlipDirection::Left;

        PM->DiscardBuffer();

        TestFalse(TEXT("AC-19: has_queued_input == false after DiscardBuffer"), PM->has_queued_input);
        // queued_input_direction is not required to reset — the story spec leaves it
        // undefined when has_queued_input is false. Do not assert on it.

        return true;
    }

    // -----------------------------------------------------------------------
    // AC-25 (ac25_drop_feedback_synchronous) — within the same HandleSlipTransition
    // event call, BOTH the haptic dispatch and the audio sting are invoked BEFORE
    // the function returns. Verified by capturing counts immediately after return.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("ac25_drop_feedback_synchronous"))
    {
        FTestWorldWrapper WorldWrapper;
        UWorld* TestWorld = CreateTestPlayWorld_IB(this, WorldWrapper, TEXT("AC-25"));
        if (!TestWorld) { return false; }
        ASlipstormPlayerPawn* Pawn = SpawnPawnWithCurves_IB(this, TestWorld, TEXT("AC-25"));
        if (!Pawn) { return false; }
        URunStateMachineSubsystem* RSM = GetRSM_IB(this, TestWorld, TEXT("AC-25"));
        if (!RSM) { return false; }

        UPlayerLaneMovementComponent* PM = Pawn->MovementComponent.Get();
        SetRSMRunning_IB(RSM);

        ResetSpy();
        IHapticDispatch::TestOnly_SetFireFn(&SpyFireFn);
        IHapticDispatch::TestOnly_SetIsEnabledFn(&SpyIsEnabledFn);
        ON_SCOPE_EXIT { IHapticDispatch::TestOnly_Reset(); };

        // Arrange SLIPPING + buffer full.
        PM->HandleSlipTransition(ESlipDirection::Right);
        PM->HandleSlipTransition(ESlipDirection::Right);

        const int32 AudioBefore = PM->BufferDropAudioSting_TestOnlyCallCount;
        const int32 SpyBefore   = GSpyDispatchLog.Num();

        // Trigger the drop path (queue full → Rule 3).
        PM->HandleSlipTransition(ESlipDirection::Left);

        // If the dispatches were asynchronous / deferred, the counts would still be
        // at Before-values. Sync guarantee: both must be observable now.
        TestEqual(TEXT("AC-25.1: haptic BufferDrop dispatched synchronously (before return)"),
                  GSpyDispatchLog.Num(), SpyBefore + 1);
        TestEqual(TEXT("AC-25.2: audio sting dispatched synchronously (before return)"),
                  PM->BufferDropAudioSting_TestOnlyCallCount, AudioBefore + 1);

        TestWorld->DestroyActor(Pawn);
        return true;
    }

    // -----------------------------------------------------------------------
    // rule3_idempotence — three consecutive HandleSlipTransition(Left) calls with
    // SLIPPING + buffer full produce exactly three BufferDrop dispatches; queued
    // direction never mutates.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("rule3_idempotence"))
    {
        FTestWorldWrapper WorldWrapper;
        UWorld* TestWorld = CreateTestPlayWorld_IB(this, WorldWrapper, TEXT("Rule3-idem"));
        if (!TestWorld) { return false; }
        ASlipstormPlayerPawn* Pawn = SpawnPawnWithCurves_IB(this, TestWorld, TEXT("Rule3-idem"));
        if (!Pawn) { return false; }
        URunStateMachineSubsystem* RSM = GetRSM_IB(this, TestWorld, TEXT("Rule3-idem"));
        if (!RSM) { return false; }

        UPlayerLaneMovementComponent* PM = Pawn->MovementComponent.Get();
        SetRSMRunning_IB(RSM);

        ResetSpy();
        IHapticDispatch::TestOnly_SetFireFn(&SpyFireFn);
        IHapticDispatch::TestOnly_SetIsEnabledFn(&SpyIsEnabledFn);
        ON_SCOPE_EXIT { IHapticDispatch::TestOnly_Reset(); };

        // SLIPPING + Right buffered.
        PM->HandleSlipTransition(ESlipDirection::Right); // Center → Right, SLIPPING
        PM->HandleSlipTransition(ESlipDirection::Right); // buffered
        ResetSpy(); // don't count the arrange-phase (none should have fired anyway)
        const int32 AudioBefore = PM->BufferDropAudioSting_TestOnlyCallCount;

        // Fire three drops.
        PM->HandleSlipTransition(ESlipDirection::Left);
        PM->HandleSlipTransition(ESlipDirection::Left);
        PM->HandleSlipTransition(ESlipDirection::Left);

        TestEqual(TEXT("Rule3-idem: exactly 3 BufferDrop dispatches"),
                  GSpyDispatchLog.Num(), 3);
        for (int32 i = 0; i < GSpyDispatchLog.Num(); ++i)
        {
            TestEqual(FString::Printf(TEXT("Rule3-idem: SpyLog[%d] == BufferDrop"), i),
                      static_cast<uint8>(GSpyDispatchLog[i]), static_cast<uint8>(EHapticEvent::BufferDrop));
        }
        TestEqual(TEXT("Rule3-idem: audio sting counter += 3"),
                  PM->BufferDropAudioSting_TestOnlyCallCount, AudioBefore + 3);
        TestTrue(TEXT("Rule3-idem: buffer still full"), PM->has_queued_input);
        TestEqual(TEXT("Rule3-idem: queued direction UNCHANGED (still Right)"),
                  static_cast<uint8>(PM->queued_input_direction), static_cast<uint8>(ESlipDirection::Right));

        TestWorld->DestroyActor(Pawn);
        return true;
    }

    // -----------------------------------------------------------------------
    // rule5_gate_blocks_buffer_when_not_running — regression guard against a
    // future refactor that might hoist the buffer write above the Rule 5 gate.
    //
    // The Rule 5 gate (HandleSlipTransition:294-300) discards inputs when the
    // RSM is NOT RUNNING (paused, resume-grace, or non-RUNNING state). Story 005
    // introduced the SLIPPING buffer branch DOWNSTREAM of that gate. A refactor
    // that inverts the order would let inputs during PAUSED silently commit to
    // the buffer — a real bug the buffer's semantics don't survive.
    //
    // This test uses AAA labels to demonstrate the pattern qa-tester recommends
    // adopting from Story 006+ onward for new test authoring.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("rule5_gate_blocks_buffer_when_not_running"))
    {
        // ARRANGE ------------------------------------------------------------
        FTestWorldWrapper WorldWrapper;
        UWorld* TestWorld = CreateTestPlayWorld_IB(this, WorldWrapper, TEXT("Rule5-gate"));
        if (!TestWorld) { return false; }
        ASlipstormPlayerPawn* Pawn = SpawnPawnWithCurves_IB(this, TestWorld, TEXT("Rule5-gate"));
        if (!Pawn) { return false; }
        URunStateMachineSubsystem* RSM = GetRSM_IB(this, TestWorld, TEXT("Rule5-gate"));
        if (!RSM) { return false; }

        UPlayerLaneMovementComponent* PM = Pawn->MovementComponent.Get();

        // Install haptic spy — if the buffer path executed incorrectly, the
        // Rule 3 drop feedback would fire on a second call. We want to prove
        // NOTHING fires under a paused gate.
        ResetSpy();
        IHapticDispatch::TestOnly_SetFireFn(&SpyFireFn);
        IHapticDispatch::TestOnly_SetIsEnabledFn(&SpyIsEnabledFn);
        ON_SCOPE_EXIT { IHapticDispatch::TestOnly_Reset(); };

        // First, put PM into SLIPPING with a buffer full — under a valid RSM.
        SetRSMRunning_IB(RSM);
        PM->HandleSlipTransition(ESlipDirection::Right); // Center → Right, SLIPPING
        PM->HandleSlipTransition(ESlipDirection::Right); // buffered
        TestTrue(TEXT("Rule5-gate arrange: buffer set under RUNNING"), PM->has_queued_input);
        ResetSpy(); // arrange complete; ignore any pre-existing spy state

        // Snapshot state that MUST NOT change during the gated call.
        const bool                     PreBuffer     = PM->has_queued_input;
        const ESlipDirection           PreDirection  = PM->queued_input_direction;
        const int32                    PreAudioCount = PM->BufferDropAudioSting_TestOnlyCallCount;

        // Now flip RSM to PAUSED — Rule 5 gate must reject subsequent inputs.
        RSM->TestOnly_bPaused = true;

        // ACT ----------------------------------------------------------------
        // A slip attempt during PAUSED: gate rejects at line 294; SLIPPING branch
        // and Rule 3 drop must NOT execute.
        PM->HandleSlipTransition(ESlipDirection::Left);

        // ASSERT -------------------------------------------------------------
        // Buffer state unchanged (Rule 5 discard, NOT Rule 3 drop-with-feedback).
        TestEqual(TEXT("Rule5-gate: has_queued_input unchanged under PAUSED"),
                  PM->has_queued_input, PreBuffer);
        TestEqual(TEXT("Rule5-gate: queued_input_direction unchanged under PAUSED"),
                  static_cast<uint8>(PM->queued_input_direction), static_cast<uint8>(PreDirection));

        // No haptic BufferDrop fired — gate rejected before Rule 3 drop path.
        TestEqual(TEXT("Rule5-gate: no BufferDrop haptic dispatched under PAUSED"),
                  GSpyDispatchLog.Num(), 0);

        // No audio sting fired either.
        TestEqual(TEXT("Rule5-gate: audio sting counter unchanged under PAUSED"),
                  PM->BufferDropAudioSting_TestOnlyCallCount, PreAudioCount);

        // Additional coverage: same expectation under RESUME_GRACE.
        RSM->TestOnly_bPaused      = false;
        RSM->TestOnly_bResumeGrace = true;
        PM->HandleSlipTransition(ESlipDirection::Left);
        TestEqual(TEXT("Rule5-gate: no dispatch under RESUME_GRACE either"),
                  GSpyDispatchLog.Num(), 0);
        TestEqual(TEXT("Rule5-gate: audio sting still unchanged under RESUME_GRACE"),
                  PM->BufferDropAudioSting_TestOnlyCallCount, PreAudioCount);

        // Additional coverage: same expectation under non-RUNNING state (IDLE).
        RSM->TestOnly_bResumeGrace = false;
        RSM->TestOnly_CurrentState = ERunState::IDLE;
        PM->HandleSlipTransition(ESlipDirection::Left);
        TestEqual(TEXT("Rule5-gate: no dispatch under CurrentState != RUNNING"),
                  GSpyDispatchLog.Num(), 0);
        TestEqual(TEXT("Rule5-gate: audio sting still unchanged under CurrentState != RUNNING"),
                  PM->BufferDropAudioSting_TestOnlyCallCount, PreAudioCount);

        TestWorld->DestroyActor(Pawn);
        return true;
    }

    AddError(FString::Printf(
        TEXT("FPMInputBufferTest::RunTest — unknown Parameters: '%s'"),
        *Parameters));
    return false;
}

#endif // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

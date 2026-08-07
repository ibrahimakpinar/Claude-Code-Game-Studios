// Copyright (c) Game Studio. All rights reserved.
// SLIPSTORM — 60-second voxel endless runner, mobile, Unreal Engine 5.7
//
// PMCommitmentTellTest.cpp — Story 010 integration tests for the
// commitment-tell 80% flash + 2-frame hold + 50ms decay + 200ms cadence cap +
// commitment_tell_fire_count. TR-PM-013 / TR-PM-014 / TR-PM-015.
//
// Story Type: Visual/Feel — PEAT/Harding FPA formal gate deferred to Polish
// per presentation §8. This automated file covers the cadence cap arithmetic,
// counter parity (always increments), lifecycle timing (hold/decay), sign
// direction, and reset semantics. Manual evidence for peak amplitude visual
// verification lives at
// production/qa/evidence/story-010-commitment-tell-evidence.md.
//
// Spec:   production/epics/player-movement/story-010-commitment-tell.md
// GDD:    design/gdd/player-movement-presentation.md §3 Commitment-Tell +
//         R11a-11 PEAT + §4 F-COMMIT-CADENCE-CAP + §8 AC-29/CADENCE/ENABLED
// ADR:    docs/architecture/adr-0009-player-movement-hosting.md (SD5)
// TR:     TR-PM-013 (80% flash + hold + decay), TR-PM-014 (200ms cadence),
//         TR-PM-015 (commitment_tell_fire_count)
//
// Test category: SLIPSTORM.PlayerMovement.CommitmentTell
// Runner: UE Automation Framework, ClientContext | ProductFilter.
//
// Helpers _CT-suffixed to avoid ODR collisions.
// AAA labels present per test-standards.md.
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
#include "Materials/MaterialInstanceDynamic.h"

#include "Player/PlayerLaneMovementComponent.h"
#include "Player/SlipstormPlayerPawn.h"
#include "Player/EPlayerLane.h"
#include "Player/ERunSlipState.h"
#include "Player/ESlipDirection.h"
#include "RunStateMachine/RunStateMachineSubsystem.h"
#include "RunStateMachine/ERunState.h"
#include "RunStateMachine/ERunOutcome.h"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static UCurveFloat* MakeIdentityCurve_CT(UObject* InOuter)
{
    UCurveFloat* C = NewObject<UCurveFloat>(InOuter);
    C->FloatCurve.AddKey(0.0f, 0.0f);
    C->FloatCurve.AddKey(1.0f, 1.0f);
    return C;
}

static UWorld* CreateTestPlayWorld_CT(FAutomationTestBase* T, const TCHAR* Label)
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

static ASlipstormPlayerPawn* SpawnPawnWithCurves_CT(
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
    Pawn->MovementComponent->SlipCurve       = MakeIdentityCurve_CT(World);
    Pawn->MovementComponent->LeanCurve       = MakeIdentityCurve_CT(World);
    Pawn->MovementComponent->EdgeAbsorbCurve = MakeIdentityCurve_CT(World);
    UGameplayStatics::FinishSpawningActor(Pawn, FTransform::Identity);
    // Note: MeshMaterialDynamic is null after BeginPlay in headless (no slot-0
    // material asset on the pawn's MeshComponent). Tests that need the material
    // write path to fire must inject a MID via friend access INSIDE the test
    // body (RunTest is friends with UPlayerLaneMovementComponent via
    // FPMCommitmentTellTest — free helpers like this one are NOT friends).
    return Pawn;
}

// Advance the world time directly. Story 010's cadence gate reads
// World->GetTimeSeconds(); manipulating TimeSeconds simulates elapsed play time
// without requiring a full tick pump. Safe because we're not asserting on any
// UE subsystem that depends on time continuity in these tests.
static void SetWorldTime_CT(UWorld* World, double NewTime)
{
    if (World)
    {
        World->TimeSeconds = NewTime;
    }
}

// ---------------------------------------------------------------------------
// IMPLEMENT_COMPLEX_AUTOMATION_TEST — 7 integration test commands.
// ---------------------------------------------------------------------------

IMPLEMENT_COMPLEX_AUTOMATION_TEST(
    FPMCommitmentTellTest,
    "SLIPSTORM.PlayerMovement.CommitmentTell",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

void FPMCommitmentTellTest::GetTests(
    TArray<FString>& OutBeautifiedNames,
    TArray<FString>& OutTestCommands) const
{
    OutBeautifiedNames.Add(TEXT("AC-29 — peak amplitude ±0.80 + 2-frame hold + 50ms decay lifecycle"));
    OutTestCommands.Add(TEXT("ac29_amplitude_and_lifecycle_timing"));

    OutBeautifiedNames.Add(TEXT("Direction sign — Left→Center = +0.80, Center→Left = -0.80"));
    OutTestCommands.Add(TEXT("direction_sign_left_and_right"));

    OutBeautifiedNames.Add(TEXT("Setup A — counter always increments, cadence cap suppresses visual"));
    OutTestCommands.Add(TEXT("counter_always_increments_setup_a"));

    OutBeautifiedNames.Add(TEXT("Cadence cap suppression — 2nd fire within 200ms of prior zero"));
    OutTestCommands.Add(TEXT("cadence_cap_suppression"));

    OutBeautifiedNames.Add(TEXT("Cadence cap release — 2nd fire outside 200ms window renders"));
    OutTestCommands.Add(TEXT("cadence_cap_release"));

    OutBeautifiedNames.Add(TEXT("Setup B — 5 isolated slips ≥500ms apart all render"));
    OutTestCommands.Add(TEXT("setup_b_isolated_slips"));

    OutBeautifiedNames.Add(TEXT("SnapToTargetAndReset clears flash lifecycle state"));
    OutTestCommands.Add(TEXT("snap_reset_clears_flash_state"));

    OutBeautifiedNames.Add(TEXT("Null MID null-guard — counter increments, material write no-ops"));
    OutTestCommands.Add(TEXT("null_mid_null_guard_no_ops_write"));
}

bool FPMCommitmentTellTest::RunTest(const FString& Parameters)
{
    // -----------------------------------------------------------------------
    // TC1 — AC-29 peak amplitude + 2-frame hold + 50ms decay lifecycle.
    //
    // TriggerCommitmentTell fires → material param written to ±0.80 → 2 ticks
    // hold at ±0.80 (no re-writes during hold) → then decay ramp writes each
    // tick with param decreasing linearly to 0 over 50ms → at fade-to-zero
    // time_last_flash_zero_s is updated to World time.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("ac29_amplitude_and_lifecycle_timing"))
    {
        // Arrange
        UWorld* TestWorld = CreateTestPlayWorld_CT(this, TEXT("TC1"));
        if (!TestWorld) { return false; }
        ASlipstormPlayerPawn* Pawn = SpawnPawnWithCurves_CT(this, TestWorld, TEXT("TC1"));
        if (!Pawn) { return false; }
        UPlayerLaneMovementComponent* PM = Pawn->MovementComponent.Get();

        UGameInstance* GI = TestWorld->GetGameInstance();
        URunStateMachineSubsystem* RSM =
            GI ? GI->GetSubsystem<URunStateMachineSubsystem>() : nullptr;
        if (!TestNotNull(TEXT("TC1: RSM available"), RSM)) { TestWorld->DestroyActor(Pawn); return false; }

        RSM->TestOnly_CurrentState = ERunState::RUNNING;
        RSM->TestOnly_bPaused      = false;
        RSM->TestOnly_bResumeGrace = false;

        // Inject MID via friend access — BeginPlay's CreateAndSetMaterialInstanceDynamic
        // returned null (no slot-0 material on headless mesh). This TC needs the
        // material write path to fire so the write-count assertions hold.
        PM->MeshMaterialDynamic = NewObject<UMaterialInstanceDynamic>(PM);

        PM->current_lane = EPlayerLane::Left;

        const int32 WriteCountBefore = PM->CommitmentTellFlashWrite_TestOnlyCallCount;

        // Act 1 — fire commitment-tell.
        PM->TriggerCommitmentTell(EPlayerLane::Center);

        // Assert 1 — peak write occurred once with +0.80 sign captured.
        TestEqual(TEXT("TC1: sign_of_current_flash == +1.0 (Left→Center)"),
                  PM->sign_of_current_flash, 1.0f);
        TestEqual(TEXT("TC1: commitment_tell_fire_count += 1"),
                  PM->commitment_tell_fire_count, 1);
        TestEqual(TEXT("TC1: flash_hold_ticks_remaining == 2 after peak"),
                  PM->flash_hold_ticks_remaining, 2);
        TestFalse(TEXT("TC1: flash_decay_active still false during hold"), PM->flash_decay_active);
        TestEqual(TEXT("TC1: exactly 1 material write at peak"),
                  PM->CommitmentTellFlashWrite_TestOnlyCallCount, WriteCountBefore + 1);

        // Act 2 — 2 ticks of hold at 0.016s each (2-frame hold at 60fps).
        FApp::SetDeltaTime(0.016);
        PM->TickComponent(0.016f, ELevelTick::LEVELTICK_All, nullptr);
        TestEqual(TEXT("TC1: after tick 1, hold_ticks_remaining == 1"), PM->flash_hold_ticks_remaining, 1);
        TestFalse(TEXT("TC1: decay not yet active after tick 1"), PM->flash_decay_active);

        PM->TickComponent(0.016f, ELevelTick::LEVELTICK_All, nullptr);
        TestEqual(TEXT("TC1: after tick 2, hold_ticks_remaining == 0"), PM->flash_hold_ticks_remaining, 0);
        TestTrue(TEXT("TC1: decay active after hold completes"), PM->flash_decay_active);
        TestEqual(TEXT("TC1: flash_decay_time_s == 0.050 at decay start"),
                  PM->flash_decay_time_s, 0.050f);

        // Act 3 — advance decay by 5 ticks of 0.010s (total 50ms → decay complete).
        FApp::SetDeltaTime(0.010);
        for (int32 i = 0; i < 5; ++i)
        {
            PM->TickComponent(0.010f, ELevelTick::LEVELTICK_All, nullptr);
        }

        // Assert 3 — decay complete, fade-to-zero moment recorded.
        TestFalse(TEXT("TC1: flash_decay_active == false after decay complete"),
                  PM->flash_decay_active);
        TestTrue(TEXT("TC1: flash_decay_time_s <= 0 after decay complete"),
                 PM->flash_decay_time_s <= KINDA_SMALL_NUMBER);
        TestTrue(TEXT("TC1: time_last_flash_zero_s updated to World time (> initial -1000)"),
                 PM->time_last_flash_zero_s > -999.0f);
        // Additional decay writes: 5 tick decays produced 5 more material writes.
        TestEqual(TEXT("TC1: total material writes == peak + 5 decay ramps"),
                  PM->CommitmentTellFlashWrite_TestOnlyCallCount, WriteCountBefore + 1 + 5);

        TestWorld->DestroyActor(Pawn);
        return true;
    }

    // -----------------------------------------------------------------------
    // TC2 — direction sign captured based on lane ordinal comparison.
    // Left→Center (1 < 2 → +1.0). Center→Left (2 > 1 → -1.0).
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("direction_sign_left_and_right"))
    {
        UWorld* TestWorld = CreateTestPlayWorld_CT(this, TEXT("TC2"));
        if (!TestWorld) { return false; }
        ASlipstormPlayerPawn* Pawn = SpawnPawnWithCurves_CT(this, TestWorld, TEXT("TC2"));
        if (!Pawn) { return false; }
        UPlayerLaneMovementComponent* PM = Pawn->MovementComponent.Get();

        // Arrange 1 — Left→Center is rightward → sign +1.
        PM->current_lane = EPlayerLane::Left;
        SetWorldTime_CT(TestWorld, 100.0); // clear of any -1000 initial cadence
        PM->time_last_flash_zero_s = -1000.0f; // ensure gate passes

        // Act 1
        PM->TriggerCommitmentTell(EPlayerLane::Center);

        // Assert 1
        TestEqual(TEXT("TC2: Left→Center → sign +1.0 (rightward slip)"),
                  PM->sign_of_current_flash, 1.0f);

        // Arrange 2 — Center→Left is leftward → sign -1.
        PM->current_lane = EPlayerLane::Center;
        SetWorldTime_CT(TestWorld, 1000.0); // advance past cadence window
        PM->time_last_flash_zero_s = 0.0f;  // simulate prior fade-to-zero at t=0

        // Act 2
        PM->TriggerCommitmentTell(EPlayerLane::Left);

        // Assert 2
        TestEqual(TEXT("TC2: Center→Left → sign -1.0 (leftward slip)"),
                  PM->sign_of_current_flash, -1.0f);

        TestWorld->DestroyActor(Pawn);
        return true;
    }

    // -----------------------------------------------------------------------
    // TC3 — Setup A: counter increments on ALL fires, cadence cap suppresses
    // most visuals. 5 rapid fires at same world time → 5 counter increments
    // but only 1 material write (first fire only).
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("counter_always_increments_setup_a"))
    {
        UWorld* TestWorld = CreateTestPlayWorld_CT(this, TEXT("TC3"));
        if (!TestWorld) { return false; }
        ASlipstormPlayerPawn* Pawn = SpawnPawnWithCurves_CT(this, TestWorld, TEXT("TC3"));
        if (!Pawn) { return false; }
        UPlayerLaneMovementComponent* PM = Pawn->MovementComponent.Get();

        // Arrange
        PM->current_lane = EPlayerLane::Left;
        SetWorldTime_CT(TestWorld, 5.0);       // t=5 seconds
        PM->time_last_flash_zero_s = 4.9f;      // prior zero at t=4.9 → 100ms ago → within 200ms window
        const int32 WriteCountBefore = PM->CommitmentTellFlashWrite_TestOnlyCallCount;

        // Act — 5 rapid fires at the same world time.
        for (int32 i = 0; i < 5; ++i)
        {
            PM->TriggerCommitmentTell(EPlayerLane::Center);
        }

        // Assert
        TestEqual(TEXT("TC3: counter == 5 (all fires counted)"),
                  PM->commitment_tell_fire_count, 5);
        // Cadence gate blocks all 5 since time_last_flash_zero_s stays at 4.9 (no decay ran).
        TestEqual(TEXT("TC3: material writes == 0 (all suppressed by cadence cap)"),
                  PM->CommitmentTellFlashWrite_TestOnlyCallCount, WriteCountBefore);

        TestWorld->DestroyActor(Pawn);
        return true;
    }

    // -----------------------------------------------------------------------
    // TC4 — cadence cap suppression: 2nd fire within 200ms window is suppressed.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("cadence_cap_suppression"))
    {
        UWorld* TestWorld = CreateTestPlayWorld_CT(this, TEXT("TC4"));
        if (!TestWorld) { return false; }
        ASlipstormPlayerPawn* Pawn = SpawnPawnWithCurves_CT(this, TestWorld, TEXT("TC4"));
        if (!Pawn) { return false; }
        UPlayerLaneMovementComponent* PM = Pawn->MovementComponent.Get();

        // Arrange
        PM->current_lane = EPlayerLane::Left;
        SetWorldTime_CT(TestWorld, 10.0);
        PM->time_last_flash_zero_s = 9.9f;  // 100ms ago — within 200ms window
        const int32 WriteCountBefore = PM->CommitmentTellFlashWrite_TestOnlyCallCount;

        // Act — one fire that should be suppressed.
        PM->TriggerCommitmentTell(EPlayerLane::Center);

        // Assert
        TestEqual(TEXT("TC4: counter incremented"), PM->commitment_tell_fire_count, 1);
        TestEqual(TEXT("TC4: material write suppressed (0 new writes)"),
                  PM->CommitmentTellFlashWrite_TestOnlyCallCount, WriteCountBefore);
        TestEqual(TEXT("TC4: flash_hold_ticks_remaining unchanged (no schedule)"),
                  PM->flash_hold_ticks_remaining, 0);

        TestWorld->DestroyActor(Pawn);
        return true;
    }

    // -----------------------------------------------------------------------
    // TC5 — cadence cap release: 2nd fire outside 200ms window renders.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("cadence_cap_release"))
    {
        UWorld* TestWorld = CreateTestPlayWorld_CT(this, TEXT("TC5"));
        if (!TestWorld) { return false; }
        ASlipstormPlayerPawn* Pawn = SpawnPawnWithCurves_CT(this, TestWorld, TEXT("TC5"));
        if (!Pawn) { return false; }
        UPlayerLaneMovementComponent* PM = Pawn->MovementComponent.Get();

        // Arrange — inject MID (headless BeginPlay resolved null; see TC1 note).
        PM->MeshMaterialDynamic = NewObject<UMaterialInstanceDynamic>(PM);
        PM->current_lane = EPlayerLane::Left;
        SetWorldTime_CT(TestWorld, 10.0);
        PM->time_last_flash_zero_s = 9.5f;  // 500ms ago — outside 200ms window
        const int32 WriteCountBefore = PM->CommitmentTellFlashWrite_TestOnlyCallCount;

        // Act
        PM->TriggerCommitmentTell(EPlayerLane::Center);

        // Assert
        TestEqual(TEXT("TC5: counter incremented"), PM->commitment_tell_fire_count, 1);
        TestEqual(TEXT("TC5: material write occurred (+1)"),
                  PM->CommitmentTellFlashWrite_TestOnlyCallCount, WriteCountBefore + 1);
        TestEqual(TEXT("TC5: flash_hold_ticks_remaining == 2 (lifecycle armed)"),
                  PM->flash_hold_ticks_remaining, 2);

        TestWorld->DestroyActor(Pawn);
        return true;
    }

    // -----------------------------------------------------------------------
    // TC6 — Setup B: 5 isolated slips ≥500ms apart all render at peak.
    // Between fires, simulate the full hold + decay so time_last_flash_zero_s
    // advances past the cadence window before the next fire.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("setup_b_isolated_slips"))
    {
        UWorld* TestWorld = CreateTestPlayWorld_CT(this, TEXT("TC6"));
        if (!TestWorld) { return false; }
        ASlipstormPlayerPawn* Pawn = SpawnPawnWithCurves_CT(this, TestWorld, TEXT("TC6"));
        if (!Pawn) { return false; }
        UPlayerLaneMovementComponent* PM = Pawn->MovementComponent.Get();

        // Inject MID (headless BeginPlay resolved null; see TC1 note).
        PM->MeshMaterialDynamic = NewObject<UMaterialInstanceDynamic>(PM);
        PM->current_lane = EPlayerLane::Left;
        const int32 WriteCountBefore = PM->CommitmentTellFlashWrite_TestOnlyCallCount;

        // 5 iterations: each fire, advance time_last_flash_zero_s to 500ms ago
        // so cadence gate always passes. This simulates the "≥500ms apart" spec.
        for (int32 i = 0; i < 5; ++i)
        {
            const double CurrentTime = 10.0 + (i * 1.0); // 10s, 11s, 12s, 13s, 14s
            SetWorldTime_CT(TestWorld, CurrentTime);
            PM->time_last_flash_zero_s = static_cast<float>(CurrentTime - 0.5); // 500ms ago

            PM->TriggerCommitmentTell(EPlayerLane::Center);
        }

        TestEqual(TEXT("TC6: counter == 5 (all 5 fires counted)"),
                  PM->commitment_tell_fire_count, 5);
        TestEqual(TEXT("TC6: material writes == 5 (all rendered — cadence never blocked)"),
                  PM->CommitmentTellFlashWrite_TestOnlyCallCount, WriteCountBefore + 5);

        TestWorld->DestroyActor(Pawn);
        return true;
    }

    // -----------------------------------------------------------------------
    // TC7 — SnapToTargetAndReset clears flash lifecycle state.
    // Verifies that Story 010's extension of Story 008's SnapToTargetAndReset
    // resets all 4 flash lifecycle fields + zeros the material param.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("snap_reset_clears_flash_state"))
    {
        UWorld* TestWorld = CreateTestPlayWorld_CT(this, TEXT("TC7"));
        if (!TestWorld) { return false; }
        ASlipstormPlayerPawn* Pawn = SpawnPawnWithCurves_CT(this, TestWorld, TEXT("TC7"));
        if (!Pawn) { return false; }
        UPlayerLaneMovementComponent* PM = Pawn->MovementComponent.Get();

        // Arrange — seed non-zero flash lifecycle state.
        PM->flash_hold_ticks_remaining = 1;
        PM->flash_decay_active         = true;
        PM->flash_decay_time_s         = 0.025f;
        PM->sign_of_current_flash      = 1.0f;
        PM->time_last_flash_zero_s     = 5.0f;

        // Act
        PM->SnapToTargetAndReset();

        // Assert
        TestEqual(TEXT("TC7: flash_hold_ticks_remaining reset to 0"),
                  PM->flash_hold_ticks_remaining, 0);
        TestFalse(TEXT("TC7: flash_decay_active reset to false"), PM->flash_decay_active);
        TestEqual(TEXT("TC7: flash_decay_time_s reset to 0"), PM->flash_decay_time_s, 0.0f);
        TestEqual(TEXT("TC7: sign_of_current_flash reset to 0"), PM->sign_of_current_flash, 0.0f);
        // Sentinel value per PLMC.cpp:1285 — resets to -1000.0f so the next run's
        // first commitment-tell fire always passes the cadence gate. Matches the
        // field's in-class initializer at h:695.
        TestEqual(TEXT("TC7: time_last_flash_zero_s reset to sentinel -1000.0f"),
                  PM->time_last_flash_zero_s, -1000.0f);

        TestWorld->DestroyActor(Pawn);
        return true;
    }

    // -----------------------------------------------------------------------
    // TC8 — null MID null-guard behavior.
    //
    // Explicitly asserts the design contract: when MeshMaterialDynamic is null
    // (as would happen if BeginPlay MID resolution failed on a pawn with no
    // slot-0 material), TriggerCommitmentTell still increments the counter but
    // the material write no-ops. Both the peak-write and per-tick decay writes
    // must gracefully skip on null MID.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("null_mid_null_guard_no_ops_write"))
    {
        // Arrange
        UWorld* TestWorld = CreateTestPlayWorld_CT(this, TEXT("TC8"));
        if (!TestWorld) { return false; }
        ASlipstormPlayerPawn* Pawn = SpawnPawnWithCurves_CT(this, TestWorld, TEXT("TC8"));
        if (!Pawn) { return false; }
        UPlayerLaneMovementComponent* PM = Pawn->MovementComponent.Get();

        // Explicitly clear the MID injected by SpawnPawnWithCurves_CT — simulates
        // the "no slot-0 material" headless case that BeginPlay warn-and-continues.
        PM->MeshMaterialDynamic = nullptr;

        PM->current_lane = EPlayerLane::Left;
        SetWorldTime_CT(TestWorld, 10.0);
        PM->time_last_flash_zero_s = -1000.0f; // ensure cadence gate passes
        const int32 WriteCountBefore   = PM->CommitmentTellFlashWrite_TestOnlyCallCount;
        const int32 CounterBefore      = PM->commitment_tell_fire_count;

        // Act
        PM->TriggerCommitmentTell(EPlayerLane::Center);

        // Assert
        TestEqual(TEXT("TC8: counter incremented despite null MID"),
                  PM->commitment_tell_fire_count, CounterBefore + 1);
        TestEqual(TEXT("TC8: material write NOT counted (null MID → SetScalarParameterValue skipped)"),
                  PM->CommitmentTellFlashWrite_TestOnlyCallCount, WriteCountBefore);
        TestEqual(TEXT("TC8: flash_hold_ticks_remaining == 2 (schedule armed even without MID)"),
                  PM->flash_hold_ticks_remaining, 2);

        TestWorld->DestroyActor(Pawn);
        return true;
    }

    AddError(FString::Printf(
        TEXT("FPMCommitmentTellTest::RunTest — unknown Parameters: '%s'"),
        *Parameters));
    return false;
}

#endif // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

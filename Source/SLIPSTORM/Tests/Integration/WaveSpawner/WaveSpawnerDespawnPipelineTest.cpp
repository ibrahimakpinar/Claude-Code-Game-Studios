// Copyright (c) Game Studio. All rights reserved.
// SLIPSTORM — 60-second voxel endless runner, mobile, Unreal Engine 5.7
//
// WaveSpawnerDespawnPipelineTest.cpp — Story 006 integration tests for the
// Rule 12 despawn pipeline, IWaveSpawnerCallback, and Seam 13 contract.
//
// Story: production/epics/wave-spawner/story-006-despawn-pipeline-and-seam-13.md
// ADR:   docs/architecture/adr-0011-wave-spawner-pattern-library.md (D3, Rule 12, Seam 13)
// TR:    TR-WS-026
//
// Test category: SLIPSTORM.Integration.WaveSpawner.DespawnPipeline
// Runner:        UE Automation Framework
//                Flags: ApplicationContextMask | ProductFilter
//
// All 5 test commands are fully headless (no UWorld / no SpawnActor required).
// NewObject<UWaveSpawnerSubsystem>(GetTransientPackage()) creates the subsystem.
// TestOnly_SetLiveCount(1) injects WaveId -1001 into LiveSlots as the test target.
// FWaveSpawnerCallbackTestStub (Seam 13) records pipeline events for assertion.
//
// Note on ReleaseToPool warning: DespawnWave() calls ReleaseToPool(WaveId) as the
// final pipeline step. In headless tests the AWave pool is not populated (no
// SpawnActor), so ReleaseToPool logs a Warning that WaveId is not found. This is
// expected behavior in the test environment and does not indicate a pipeline defect.
// The pipeline assertions (EventLog order and contents) are unaffected.
//
// Guard: WITH_DEV_AUTOMATION_TESTS only.
// Stub:  FWaveSpawnerCallbackTestStub is !UE_BUILD_SHIPPING only — compatible,
//        since WITH_DEV_AUTOMATION_TESTS implies non-Shipping builds.

#if WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#include "WaveSpawner/WaveSpawnerSubsystem.h"
#include "WaveSpawner/WaveSpawnerTypes.h"
#include "Seam/WaveSpawnerCallbackTestStub.h"

// ============================================================================
// IMPLEMENT_COMPLEX_AUTOMATION_TEST
// 5 test commands covering AC-WS-16, AC-WS-19, AC-WS-20.
// Pattern matches WaveSpawnerLifecycleTest.cpp (Story 002 precedent).
// ============================================================================

IMPLEMENT_COMPLEX_AUTOMATION_TEST(
    FWaveSpawnerDespawnPipelineTest,
    "SLIPSTORM.Integration.WaveSpawner.DespawnPipeline",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

void FWaveSpawnerDespawnPipelineTest::GetTests(
    TArray<FString>& OutBeautifiedNames,
    TArray<FString>& OutTestCommands) const
{
    OutBeautifiedNames.Add(
        TEXT("Pipeline fires CollisionUnregistered -> TelegraphUnregistered -> WaveDespawned"
             " for NaturalLanding (AC-WS-16, TR-WS-026)"));
    OutTestCommands.Add(TEXT("DespawnPipeline.PipelineOrder.NaturalLanding"));

    OutBeautifiedNames.Add(
        TEXT("Pipeline fires CollisionUnregistered -> TelegraphUnregistered -> WaveDespawned"
             " for RunTermination (AC-WS-16, TR-WS-026)"));
    OutTestCommands.Add(TEXT("DespawnPipeline.PipelineOrder.RunTermination"));

    OutBeautifiedNames.Add(
        TEXT("Pipeline fires CollisionUnregistered -> TelegraphUnregistered -> WaveDespawned"
             " for PauseFlush (AC-WS-16, TR-WS-026)"));
    OutTestCommands.Add(TEXT("DespawnPipeline.PipelineOrder.PauseFlush"));

    OutBeautifiedNames.Add(
        TEXT("SetOnDespawnedUserCallback fires AFTER OnWaveDespawned is recorded in EventLog"
             " (AC-WS-19, Seam 13)"));
    OutTestCommands.Add(TEXT("DespawnPipeline.Seam13.ReentrantCallbackAfterLog"));

    OutBeautifiedNames.Add(
        TEXT("LiveSlots decremented between OnTelegraphUnregistered and OnWaveDespawned;"
             " TestOnly_GetLiveCount() == 0 at callback time (AC-WS-20)"));
    OutTestCommands.Add(TEXT("DespawnPipeline.Seam13.LiveSlotReleasedBeforeWaveDespawned"));
}

bool FWaveSpawnerDespawnPipelineTest::RunTest(const FString& Parameters)
{
    // -------------------------------------------------------------------------
    // Shared constants.
    //
    // kTestWaveId -1001: injected into LiveSlots by TestOnly_SetLiveCount(1).
    // This value is in the range reserved for TestOnly_SetLiveCount injections
    // (range: -1001 to -(1023)) and cannot collide with production NextWaveIdCounter
    // output (starts at 0 and increments). See WaveSpawnerSubsystem.h TestOnly_SetLiveCount.
    // -------------------------------------------------------------------------
    static constexpr int32 kTestWaveId = -1001;

    // -------------------------------------------------------------------------
    // TC1 — AC-WS-16 (TR-WS-026): Pipeline order for NaturalLanding.
    //
    // Arrange: fresh subsystem, Stub wired as callback, 1 live slot injected.
    // Act:     DespawnWave(kTestWaveId, NaturalLanding).
    // Assert:  EventLog has exactly 3 entries in mandatory Rule 12 order.
    //          Entry 0: CollisionUnregistered  (WaveId matches, Reason == None)
    //          Entry 1: TelegraphUnregistered  (WaveId matches, Reason == None)
    //          Entry 2: WaveDespawned          (WaveId matches, Reason == NaturalLanding)
    // -------------------------------------------------------------------------
    if (Parameters == TEXT("DespawnPipeline.PipelineOrder.NaturalLanding"))
    {
        // Arrange
        UWaveSpawnerSubsystem* Sub =
            NewObject<UWaveSpawnerSubsystem>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC1: NewObject<UWaveSpawnerSubsystem> succeeded"), Sub))
        {
            return false;
        }
        FWaveSpawnerCallbackTestStub Stub;
        Sub->SetDespawnCallback(&Stub);
        Sub->TestOnly_SetLiveCount(1);  // injects WaveId -1001 into LiveSlots

        // Act — ReleaseToPool warning expected (pool not populated in headless tests)
        Sub->DespawnWave(kTestWaveId, EWaveDespawnReason::NaturalLanding);

        // Assert — pipeline order
        const TArray<FDespawnEvent>& Log = Stub.GetEventLog();
        TestEqual(TEXT("TC1: EventLog has exactly 3 entries"), Log.Num(), 3);
        if (Log.Num() == 3)
        {
            TestTrue(TEXT("TC1: Entry 0 WaveId matches"),
                Log[0].WaveId == kTestWaveId);
            TestTrue(TEXT("TC1: Entry 0 is CollisionUnregistered"),
                Log[0].EventName == FName(TEXT("CollisionUnregistered")));
            TestTrue(TEXT("TC1: Entry 0 Reason is None (not a WaveDespawned event)"),
                Log[0].Reason == EWaveDespawnReason::None);

            TestTrue(TEXT("TC1: Entry 1 WaveId matches"),
                Log[1].WaveId == kTestWaveId);
            TestTrue(TEXT("TC1: Entry 1 is TelegraphUnregistered"),
                Log[1].EventName == FName(TEXT("TelegraphUnregistered")));
            TestTrue(TEXT("TC1: Entry 1 Reason is None (not a WaveDespawned event)"),
                Log[1].Reason == EWaveDespawnReason::None);

            TestTrue(TEXT("TC1: Entry 2 WaveId matches"),
                Log[2].WaveId == kTestWaveId);
            TestTrue(TEXT("TC1: Entry 2 is WaveDespawned"),
                Log[2].EventName == FName(TEXT("WaveDespawned")));
            TestTrue(TEXT("TC1: Entry 2 Reason is NaturalLanding"),
                Log[2].Reason == EWaveDespawnReason::NaturalLanding);
        }

        Sub->SetDespawnCallback(nullptr);
        return true;
    }

    // -------------------------------------------------------------------------
    // TC2 — AC-WS-16 (TR-WS-026): Pipeline order for RunTermination.
    //
    // Same structural assertions as TC1; only the Reason value differs.
    // Verifies the mandatory ordering holds for the run-termination path (Story 007).
    // -------------------------------------------------------------------------
    if (Parameters == TEXT("DespawnPipeline.PipelineOrder.RunTermination"))
    {
        // Arrange
        UWaveSpawnerSubsystem* Sub =
            NewObject<UWaveSpawnerSubsystem>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC2: NewObject<UWaveSpawnerSubsystem> succeeded"), Sub))
        {
            return false;
        }
        FWaveSpawnerCallbackTestStub Stub;
        Sub->SetDespawnCallback(&Stub);
        Sub->TestOnly_SetLiveCount(1);

        // Act
        Sub->DespawnWave(kTestWaveId, EWaveDespawnReason::RunTermination);

        // Assert — pipeline order
        const TArray<FDespawnEvent>& Log = Stub.GetEventLog();
        TestEqual(TEXT("TC2: EventLog has exactly 3 entries"), Log.Num(), 3);
        if (Log.Num() == 3)
        {
            TestTrue(TEXT("TC2: Entry 0 is CollisionUnregistered"),
                Log[0].EventName == FName(TEXT("CollisionUnregistered")));
            TestTrue(TEXT("TC2: Entry 1 is TelegraphUnregistered"),
                Log[1].EventName == FName(TEXT("TelegraphUnregistered")));
            TestTrue(TEXT("TC2: Entry 2 is WaveDespawned with RunTermination"),
                Log[2].EventName == FName(TEXT("WaveDespawned"))
                && Log[2].Reason == EWaveDespawnReason::RunTermination);
        }

        Sub->SetDespawnCallback(nullptr);
        return true;
    }

    // -------------------------------------------------------------------------
    // TC3 — AC-WS-16 (TR-WS-026): Pipeline order for PauseFlush.
    //
    // Same structural assertions; verifies ordering holds for the pause-flush path.
    // -------------------------------------------------------------------------
    if (Parameters == TEXT("DespawnPipeline.PipelineOrder.PauseFlush"))
    {
        // Arrange
        UWaveSpawnerSubsystem* Sub =
            NewObject<UWaveSpawnerSubsystem>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC3: NewObject<UWaveSpawnerSubsystem> succeeded"), Sub))
        {
            return false;
        }
        FWaveSpawnerCallbackTestStub Stub;
        Sub->SetDespawnCallback(&Stub);
        Sub->TestOnly_SetLiveCount(1);

        // Act
        Sub->DespawnWave(kTestWaveId, EWaveDespawnReason::PauseFlush);

        // Assert — pipeline order
        const TArray<FDespawnEvent>& Log = Stub.GetEventLog();
        TestEqual(TEXT("TC3: EventLog has exactly 3 entries"), Log.Num(), 3);
        if (Log.Num() == 3)
        {
            TestTrue(TEXT("TC3: Entry 0 is CollisionUnregistered"),
                Log[0].EventName == FName(TEXT("CollisionUnregistered")));
            TestTrue(TEXT("TC3: Entry 1 is TelegraphUnregistered"),
                Log[1].EventName == FName(TEXT("TelegraphUnregistered")));
            TestTrue(TEXT("TC3: Entry 2 is WaveDespawned with PauseFlush"),
                Log[2].EventName == FName(TEXT("WaveDespawned"))
                && Log[2].Reason == EWaveDespawnReason::PauseFlush);
        }

        Sub->SetDespawnCallback(nullptr);
        return true;
    }

    // -------------------------------------------------------------------------
    // TC4 — AC-WS-19 (Seam 13): SetOnDespawnedUserCallback fires AFTER
    // OnWaveDespawned is recorded in EventLog.
    //
    // The re-entrant callback fires inside OnWaveDespawned, after the stub appends
    // the WaveDespawned entry. At callback time, EventLog.Num() must already be 3.
    //
    // Arrange: fresh subsystem, Stub wired, re-entrant callback captures log count.
    // Act:     DespawnWave(kTestWaveId, NaturalLanding).
    // Assert:  Captured count == 3 (WaveDespawned entry exists at callback time).
    // -------------------------------------------------------------------------
    if (Parameters == TEXT("DespawnPipeline.Seam13.ReentrantCallbackAfterLog"))
    {
        // Arrange
        UWaveSpawnerSubsystem* Sub =
            NewObject<UWaveSpawnerSubsystem>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC4: NewObject<UWaveSpawnerSubsystem> succeeded"), Sub))
        {
            return false;
        }
        FWaveSpawnerCallbackTestStub Stub;
        Sub->SetDespawnCallback(&Stub);
        Sub->TestOnly_SetLiveCount(1);

        // Capture log size from inside the re-entrant callback.
        int32 LogCountAtCallback = -1;  // sentinel; -1 means callback never fired
        Stub.SetOnDespawnedUserCallback(
            [&Stub, &LogCountAtCallback](int32 /*WaveId*/, EWaveDespawnReason /*Reason*/)
            {
                LogCountAtCallback = Stub.GetEventLog().Num();
            });

        // Act
        Sub->DespawnWave(kTestWaveId, EWaveDespawnReason::NaturalLanding);

        // Assert — re-entrant callback fired (not sentinel) and WaveDespawned already logged
        TestTrue(TEXT("TC4: Re-entrant callback fired (LogCountAtCallback != -1)"),
            LogCountAtCallback != -1);
        TestEqual(
            TEXT("TC4: EventLog.Num() == 3 at callback time (WaveDespawned already recorded, AC-WS-19)"),
            LogCountAtCallback, 3);

        Sub->SetDespawnCallback(nullptr);
        return true;
    }

    // -------------------------------------------------------------------------
    // TC5 — AC-WS-20: Live slot released BETWEEN OnTelegraphUnregistered and
    // OnWaveDespawned (i.e., before the re-entrant callback fires).
    //
    // LiveSlots.Remove(WaveId) executes after OnTelegraphUnregistered and before
    // OnWaveDespawned. The re-entrant callback (which fires inside OnWaveDespawned,
    // after the log entry) therefore sees the post-release LiveSlot count.
    //
    // Arrange: 1 live slot injected (TestOnly_GetLiveCount() == 1 before despawn).
    // Act:     DespawnWave — pipeline executes: Unregister×2 → Remove → WaveDespawned.
    // Assert:  TestOnly_GetLiveCount() == 0 at callback time (slot already released).
    // -------------------------------------------------------------------------
    if (Parameters == TEXT("DespawnPipeline.Seam13.LiveSlotReleasedBeforeWaveDespawned"))
    {
        // Arrange
        UWaveSpawnerSubsystem* Sub =
            NewObject<UWaveSpawnerSubsystem>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC5: NewObject<UWaveSpawnerSubsystem> succeeded"), Sub))
        {
            return false;
        }
        FWaveSpawnerCallbackTestStub Stub;
        Sub->SetDespawnCallback(&Stub);
        Sub->TestOnly_SetLiveCount(1);  // LiveSlots = {-1001}; GetLiveCount() == 1

        // Verify precondition
        TestEqual(TEXT("TC5 precondition: LiveCount == 1 before despawn"),
            Sub->TestOnly_GetLiveCount(), 1);

        // Capture live count from inside the re-entrant callback.
        int32 LiveCountAtCallback = -1;  // sentinel; -1 means callback never fired
        Stub.SetOnDespawnedUserCallback(
            [Sub, &LiveCountAtCallback](int32 /*WaveId*/, EWaveDespawnReason /*Reason*/)
            {
                LiveCountAtCallback = Sub->TestOnly_GetLiveCount();
            });

        // Act
        Sub->DespawnWave(kTestWaveId, EWaveDespawnReason::NaturalLanding);

        // Assert — slot already released at callback time
        TestTrue(TEXT("TC5: Re-entrant callback fired (LiveCountAtCallback != -1)"),
            LiveCountAtCallback != -1);
        TestEqual(
            TEXT("TC5: TestOnly_GetLiveCount() == 0 at callback time "
                 "(slot released before OnWaveDespawned fires, AC-WS-20)"),
            LiveCountAtCallback, 0);

        // Verify final post-despawn count as well
        TestEqual(TEXT("TC5: TestOnly_GetLiveCount() == 0 after DespawnWave completes"),
            Sub->TestOnly_GetLiveCount(), 0);

        Sub->SetDespawnCallback(nullptr);
        return true;
    }

    // Unrecognized test command — fail with diagnostic
    AddError(FString::Printf(
        TEXT("WaveSpawnerDespawnPipelineTest: unrecognized Parameters='%s'"),
        *Parameters));
    return false;
}

#endif  // WITH_DEV_AUTOMATION_TESTS

// Copyright (c) Game Studio. All rights reserved.
// SLIPSTORM — 60-second voxel endless runner, mobile, Unreal Engine 5.7
//
// WaveSpawnerRSMDPCIntegrationTest.cpp — Story 007 integration tests for
// RSM/DPC delegate wiring: pause-flush, run-termination, snapshot immutability,
// and resume-grace window.
//
// Story: production/epics/wave-spawner/story-007-rsm-dpc-integration.md
// ADR:   docs/architecture/adr-0011-wave-spawner-pattern-library.md (D3 Rules 13, 14)
// ADR:   docs/architecture/adr-0007-run-state-machine-hosting.md
// TRs:   TR-WS-013, TR-WS-022, TR-WS-025, TR-WS-027, TR-WS-028
//
// Test category: SLIPSTORM.Integration.WaveSpawner.RSMDPCIntegration
// Runner:        UE Automation Framework
//                Flags: ApplicationContextMask | ProductFilter
//
// All 5 test commands are fully headless (no UWorld / no SpawnActor required).
// NewObject<UWaveSpawnerSubsystem>(GetTransientPackage()) creates the subsystem.
// Initialize() is NOT called — handlers are invoked via TestOnly_Fire* seams.
// TestOnly_SetLifecycleState(Active) is required in Arrange for all handler tests
// because: (a) the default state is Cold, and (b) handlers guard on lifecycle state
// to prevent spurious broadcast processing (check(false) protection, Story 007).
//
// Note on ReleaseToPool warning: DespawnWave() calls ReleaseToPool(WaveId) as the
// final pipeline step. In headless tests the AWave pool is not populated (no SpawnActor),
// so ReleaseToPool logs a Warning that WaveId is not found. This is expected behavior
// in the test environment and does not indicate a pipeline defect.
//
// DEVIATION NOTE (AC-WS-18 / TC3): AC-WS-18 specifies lifecycle → Idle after run
// termination. ADR-0011 D3 is authoritative (coordinator-approved): Flushing→Idle is
// FORBIDDEN; run-termination path is Active → Flushing → Cold. TC3 therefore asserts
// Cold, not Idle. AC-WS-18 text should be reconciled by the story owner.
//
// Guard: WITH_DEV_AUTOMATION_TESTS only.
// Stub:  FWaveSpawnerCallbackTestStub is !UE_BUILD_SHIPPING only — compatible,
//        since WITH_DEV_AUTOMATION_TESTS implies non-Shipping builds.

#if WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#include "WaveSpawner/WaveSpawnerSubsystem.h"
#include "WaveSpawner/WaveSpawnerTypes.h"
#include "DPC/DPCSubsystem.h"
#include "RunStateMachine/ERunState.h"
#include "RunStateMachine/ERunOutcome.h"
#include "Seam/WaveSpawnerCallbackTestStub.h"

// ============================================================================
// IMPLEMENT_COMPLEX_AUTOMATION_TEST
// 5 test commands covering AC-WS-17, AC-WS-18, AC-WS-21, AC-WS-28, AC-WS-29.
// Pattern matches WaveSpawnerDespawnPipelineTest.cpp (Story 006 precedent).
// ============================================================================

IMPLEMENT_COMPLEX_AUTOMATION_TEST(
    FWaveSpawnerRSMDPCIntegrationTest,
    "SLIPSTORM.Integration.WaveSpawner.RSMDPCIntegration",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

void FWaveSpawnerRSMDPCIntegrationTest::GetTests(
    TArray<FString>& OutBeautifiedNames,
    TArray<FString>& OutTestCommands) const
{
    OutBeautifiedNames.Add(
        TEXT("Rule 13 Pause Flush: LiveSlots drained in WaveId ASC order with PauseFlush reason;"
             " Live == 0 after flush (AC-WS-17, TR-WS-025)"));
    OutTestCommands.Add(TEXT("RSMDPCIntegration.PauseFlush.LiveWavesDespawnedASCOrder"));

    OutBeautifiedNames.Add(
        TEXT("Rule 13 Pause Flush: LastSpawnTimeS NOT reset after pause flush (AC-WS-17, TR-WS-027)"));
    OutTestCommands.Add(TEXT("RSMDPCIntegration.PauseFlush.LastSpawnTimeNotReset"));

    OutBeautifiedNames.Add(
        TEXT("Rule 14 Run Termination: all 5 in-flight waves despawned with RunTermination;"
             " bBarrageOwed cleared; lifecycle → Cold (AC-WS-18; DEVIATION: ADR-0011 D3 = Cold not Idle)"));
    OutTestCommands.Add(TEXT("RSMDPCIntegration.RunTermination.AllWavesDespawnedBarrageOwedCleared"));

    OutBeautifiedNames.Add(
        TEXT("Snapshot Immutability: admitted wave TelegraphWindowS unchanged after DPC publishes"
             " new frame AND after MID→PEAK pool swap (AC-WS-21, AC-WS-29, TR-WS-022)"));
    OutTestCommands.Add(TEXT("RSMDPCIntegration.SnapshotImmutability.TelegraphWindowPreservedAfterDPCUpdateAndPhaseBoundary"));

    OutBeautifiedNames.Add(
        TEXT("Resume Grace: OnPausedChanged(false) sets grace window; Rule 1 gate blocks admissions"
             " within window; admissions resume after expiry (AC-WS-28, TR-WS-012)"));
    OutTestCommands.Add(TEXT("RSMDPCIntegration.ResumeGrace.BlocksAdmissionsDuringWindow"));

    // ---- TC6-TC9: Added by code-review 2026-08-21 (W-S2, W-S3, W-S4) ----

    OutBeautifiedNames.Add(
        TEXT("Rule 14 Run Termination: COMPLETE state drains all waves and resets lifecycle → Cold"
             " (AC-WS-18 COMPLETE branch; S2 coverage)"));
    OutTestCommands.Add(TEXT("RSMDPCIntegration.RunTermination.CompleteStateTerminates"));

    OutBeautifiedNames.Add(
        TEXT("Rule 14 Run Termination: ABORTED state drains all waves and resets lifecycle → Cold"
             " (AC-WS-18 ABORTED branch; S2 coverage)"));
    OutTestCommands.Add(TEXT("RSMDPCIntegration.RunTermination.AbortedStateTerminates"));

    OutBeautifiedNames.Add(
        TEXT("Resume Grace: HandlePausedChanged(false) causal path sets grace window; next DPC frame"
             " blocked; first post-grace frame admitted (AC-WS-28 causal path; S3 coverage)"));
    OutTestCommands.Add(TEXT("RSMDPCIntegration.ResumeGrace.PausedChangedFalseSetsGrace"));

    OutBeautifiedNames.Add(
        TEXT("Rule 13 Pause Flush: flush from Holding lifecycle state drains live waves and"
             " transitions to Flushing (AC-WS-17 Holding branch; S4 coverage)"));
    OutTestCommands.Add(TEXT("RSMDPCIntegration.PauseFlush.FromHoldingLifecycleState"));
}

bool FWaveSpawnerRSMDPCIntegrationTest::RunTest(const FString& Parameters)
{
    // =========================================================================
    // TC1 — AC-WS-17 (TR-WS-025): Rule 13 Pause Flush — ASC order + Live == 0.
    //
    // Arrange: 3 live waves (WaveIds -1001, -1002, -1003 from TestOnly_SetLiveCount).
    //          Lifecycle set to Active (HandlePausedChanged guards against Cold).
    // Act:     TestOnly_FirePausedChanged(true).
    // Assert:
    //   — EventLog has exactly 9 entries (3 waves × 3 Rule 12 events each).
    //   — Each block of 3: [Collision, Telegraph, WaveDespawned(PauseFlush)] in order.
    //   — WaveIds in each block are equal (each wave's 3 events share the same WaveId).
    //   — WaveDespawned WaveIds are in ascending numeric order (ASC flush order).
    //   — LiveSlots.Num() == 0 after flush.
    // =========================================================================
    if (Parameters == TEXT("RSMDPCIntegration.PauseFlush.LiveWavesDespawnedASCOrder"))
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
        // Lifecycle guard: HandlePausedChanged guards on Active|Holding; Cold is the default.
        Sub->TestOnly_SetLifecycleState(EWaveSpawnerLifecycleState::Active);
        // Inject 3 live waves. TestOnly_SetLiveCount(3) adds WaveIds -1001, -1002, -1003.
        // GetInFlightWaveIdsSorted() returns them sorted ASC: [-1003, -1002, -1001].
        Sub->TestOnly_SetLiveCount(3);

        // Precondition
        TestEqual(TEXT("TC1 precondition: LiveCount == 3 before flush"),
            Sub->TestOnly_GetLiveCount(), 3);

        // Act — ReleaseToPool warnings expected (pool not populated in headless tests).
        Sub->TestOnly_FirePausedChanged(true, 0.0);

        // Assert — event count
        const TArray<FDespawnEvent>& Log = Stub.GetEventLog();
        TestEqual(TEXT("TC1: EventLog has exactly 9 entries (3 waves × 3 events)"), Log.Num(), 9);

        if (Log.Num() == 9)
        {
            // Assert grouped pipeline order for each wave block.
            // Block 0 (wave -1003): entries 0, 1, 2
            TestTrue(TEXT("TC1: Block0[0] is CollisionUnregistered"),
                Log[0].EventName == FName(TEXT("CollisionUnregistered")));
            TestTrue(TEXT("TC1: Block0[1] is TelegraphUnregistered"),
                Log[1].EventName == FName(TEXT("TelegraphUnregistered")));
            TestTrue(TEXT("TC1: Block0[2] is WaveDespawned"),
                Log[2].EventName == FName(TEXT("WaveDespawned")));
            TestTrue(TEXT("TC1: Block0[2] Reason is PauseFlush"),
                Log[2].Reason == EWaveDespawnReason::PauseFlush);
            // WaveIds within a block must match.
            TestTrue(TEXT("TC1: Block0 WaveIds consistent"),
                Log[0].WaveId == Log[1].WaveId && Log[1].WaveId == Log[2].WaveId);

            // Block 1 (wave -1002): entries 3, 4, 5
            TestTrue(TEXT("TC1: Block1[0] is CollisionUnregistered"),
                Log[3].EventName == FName(TEXT("CollisionUnregistered")));
            TestTrue(TEXT("TC1: Block1[1] is TelegraphUnregistered"),
                Log[4].EventName == FName(TEXT("TelegraphUnregistered")));
            TestTrue(TEXT("TC1: Block1[2] is WaveDespawned with PauseFlush"),
                Log[5].EventName == FName(TEXT("WaveDespawned"))
                && Log[5].Reason == EWaveDespawnReason::PauseFlush);
            TestTrue(TEXT("TC1: Block1 WaveIds consistent"),
                Log[3].WaveId == Log[4].WaveId && Log[4].WaveId == Log[5].WaveId);

            // Block 2 (wave -1001): entries 6, 7, 8
            TestTrue(TEXT("TC1: Block2[0] is CollisionUnregistered"),
                Log[6].EventName == FName(TEXT("CollisionUnregistered")));
            TestTrue(TEXT("TC1: Block2[1] is TelegraphUnregistered"),
                Log[7].EventName == FName(TEXT("TelegraphUnregistered")));
            TestTrue(TEXT("TC1: Block2[2] is WaveDespawned with PauseFlush"),
                Log[8].EventName == FName(TEXT("WaveDespawned"))
                && Log[8].Reason == EWaveDespawnReason::PauseFlush);
            TestTrue(TEXT("TC1: Block2 WaveIds consistent"),
                Log[6].WaveId == Log[7].WaveId && Log[7].WaveId == Log[8].WaveId);

            // Assert WaveId ASC flush order across blocks.
            // GetInFlightWaveIdsSorted() sorts [-1001,-1002,-1003] → [-1003,-1002,-1001] ASC.
            TestTrue(TEXT("TC1: WaveDespawned entries in WaveId ASC order (block0 < block1 < block2)"),
                Log[2].WaveId < Log[5].WaveId && Log[5].WaveId < Log[8].WaveId);
        }

        // Assert LiveSlots empty after flush (DespawnWave calls LiveSlots.Remove per wave).
        TestEqual(TEXT("TC1: LiveSlots.Num() == 0 after pause flush"),
            Sub->TestOnly_GetLiveCount(), 0);

        Sub->SetDespawnCallback(nullptr);
        return true;
    }

    // =========================================================================
    // TC2 — AC-WS-17 (TR-WS-027): LastSpawnTimeS NOT reset during pause flush.
    //
    // Arrange: inject known LastSpawnTimeS = 7.5f before pause flush.
    // Act:     TestOnly_FirePausedChanged(true).
    // Assert:  LastSpawnTimeS unchanged (still 7.5f) after flush completes.
    // =========================================================================
    if (Parameters == TEXT("RSMDPCIntegration.PauseFlush.LastSpawnTimeNotReset"))
    {
        // Arrange
        UWaveSpawnerSubsystem* Sub =
            NewObject<UWaveSpawnerSubsystem>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC2: NewObject<UWaveSpawnerSubsystem> succeeded"), Sub))
        {
            return false;
        }

        Sub->TestOnly_SetLifecycleState(EWaveSpawnerLifecycleState::Active);
        Sub->TestOnly_SetLastSpawnTimeS(7.5f);  // inject known value
        Sub->TestOnly_SetLiveCount(1);           // one live wave for the flush to process

        // Precondition
        TestEqual(TEXT("TC2 precondition: LastSpawnTimeS == 7.5f before flush"),
            Sub->TestOnly_GetLastSpawnTimeS(), 7.5f);

        // Act — ReleaseToPool warning expected.
        Sub->TestOnly_FirePausedChanged(true, 0.0);

        // Assert — LastSpawnTimeS preserved (TR-WS-027).
        TestEqual(
            TEXT("TC2: LastSpawnTimeS still 7.5f after pause flush (TR-WS-027 — NOT reset)"),
            Sub->TestOnly_GetLastSpawnTimeS(), 7.5f);

        return true;
    }

    // =========================================================================
    // TC3 — AC-WS-18: Rule 14 Run Termination — all 5 waves despawned, bBarrageOwed
    // cleared, lifecycle → Cold (ADR-0011 D3; DEVIATION from AC-WS-18 text → Idle).
    //
    // Arrange: 5 live waves, bBarrageOwed = true, lifecycle = Active.
    // Act:     TestOnly_FireRunStateChanged(RUNNING, DEAD).
    // Assert:
    //   — EventLog has exactly 15 entries (5 waves × 3 events each).
    //   — All WaveDespawned entries carry RunTermination reason.
    //   — bBarrageOwed == false after termination.
    //   — Lifecycle == Cold (ADR-0011 D3; not Idle per AC-WS-18 text — see DEVIATION NOTE).
    //   — LiveSlots.Num() == 0, ScheduledSlots.Num() == 0.
    // =========================================================================
    if (Parameters == TEXT("RSMDPCIntegration.RunTermination.AllWavesDespawnedBarrageOwedCleared"))
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
        Sub->TestOnly_SetLifecycleState(EWaveSpawnerLifecycleState::Active);
        Sub->TestOnly_SetLiveCount(5);          // WaveIds -1001..-1005; sorted ASC: -1005..-1001
        Sub->TestOnly_SetScheduledCount(3);     // S1 fix: inject non-empty ScheduledSlots so the
                                                // ScheduledCount==0 assertion is non-trivial.
                                                // WaveIds -1,-2,-3 (no collision with live -1001 range).
                                                // Verifies HandleRunStateChanged calls ScheduledSlots.Reset().
        Sub->TestOnly_SetBarrageOwed(true); // must be cleared by termination (AC-WS-18)

        // Preconditions
        TestEqual(TEXT("TC3 precondition: LiveCount == 5 before termination"),
            Sub->TestOnly_GetLiveCount(), 5);
        TestTrue(TEXT("TC3 precondition: bBarrageOwed == true before termination"),
            Sub->TestOnly_GetBarrageOwed());

        // Act — ReleaseToPool warnings expected (5 warns, one per wave).
        Sub->TestOnly_FireRunStateChanged(ERunState::RUNNING, ERunState::DEAD);

        // Assert — event count
        const TArray<FDespawnEvent>& Log = Stub.GetEventLog();
        TestEqual(TEXT("TC3: EventLog has exactly 15 entries (5 waves × 3 events)"), Log.Num(), 15);

        if (Log.Num() == 15)
        {
            // Verify all WaveDespawned entries carry RunTermination reason.
            bool bAllRunTermination = true;
            for (int32 i = 2; i < 15; i += 3)  // entries 2, 5, 8, 11, 14
            {
                if (Log[i].EventName != FName(TEXT("WaveDespawned"))
                 || Log[i].Reason    != EWaveDespawnReason::RunTermination)
                {
                    bAllRunTermination = false;
                    break;
                }
            }
            TestTrue(TEXT("TC3: All 5 WaveDespawned entries carry RunTermination reason (AC-WS-18)"),
                bAllRunTermination);

            // Verify WaveId ASC flush order across the 5 blocks.
            TestTrue(TEXT("TC3: WaveIds in ASC flush order across all 5 blocks"),
                Log[2].WaveId  < Log[5].WaveId  &&
                Log[5].WaveId  < Log[8].WaveId  &&
                Log[8].WaveId  < Log[11].WaveId &&
                Log[11].WaveId < Log[14].WaveId);
        }

        // Assert post-termination state.
        TestFalse(TEXT("TC3: bBarrageOwed == false after run termination (AC-WS-18)"),
            Sub->TestOnly_GetBarrageOwed());

        // DEVIATION NOTE (AC-WS-18): ADR-0011 D3 mandates Flushing→Cold for run termination.
        // This assertion verifies Cold, not Idle (see file-level DEVIATION NOTE and handler comment).
        TestTrue(
            TEXT("TC3: lifecycle == Cold after run termination "
                 "(ADR-0011 D3 authoritative; AC-WS-18 text says Idle — reconcile AC text)"),
            Sub->TestOnly_GetLifecycleState() == EWaveSpawnerLifecycleState::Cold);

        TestEqual(TEXT("TC3: LiveSlots.Num() == 0 after termination"),
            Sub->TestOnly_GetLiveCount(), 0);
        TestEqual(TEXT("TC3: ScheduledSlots.Num() == 0 after termination"),
            Sub->TestOnly_GetScheduledCount(), 0);

        Sub->SetDespawnCallback(nullptr);
        return true;
    }

    // =========================================================================
    // TC4 — AC-WS-21 + AC-WS-29: Snapshot immutability under DPC update and
    // phase-boundary pool swap.
    //
    // Phase A (AC-WS-21 — DPC update):
    //   Admit wave 0 at TelegraphWindowS=0.80. Publish new DPC frame at 0.65.
    //   Assert wave 0's stored TelegraphWindowS is still 0.80.
    //
    // Phase B (AC-WS-29 — phase boundary):
    //   Admit wave 1 in MID phase at TelegraphWindowS=0.82.
    //   Fire Active→Holding transition (MID→PEAK pool swap).
    //   Assert wave 1's stored TelegraphWindowS is still 0.82 after pool swap.
    //
    // Notes:
    //   — Initialize() not called; ActiveDrawPool may be null. TryAdmitPattern still
    //     returns Admitted and populates InFlightWaves before calling DrawNonBarragePattern.
    //     DrawNonBarragePattern logs a null-pool warning — expected in headless tests.
    //   — WaveId 0 is the first minted ID (NextWaveIdCounter starts at 0).
    //   — WaveSpawnIntervalS=0.0f ensures cadence gate passes at any non-negative Now.
    //   — ShouldDrawBarrage() returns false for non-PEAK phases (no FRand() consumed).
    // =========================================================================
    if (Parameters == TEXT("RSMDPCIntegration.SnapshotImmutability.TelegraphWindowPreservedAfterDPCUpdateAndPhaseBoundary"))
    {
        // Arrange
        UWaveSpawnerSubsystem* Sub =
            NewObject<UWaveSpawnerSubsystem>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC4: NewObject<UWaveSpawnerSubsystem> succeeded"), Sub))
        {
            return false;
        }

        Sub->TestOnly_SetLifecycleState(EWaveSpawnerLifecycleState::Active);
        Sub->TestOnly_SetCurrentTimeOverride(10.f);  // Now=10; LastSpawnTimeS=0; interval=0 → gate passes

        // ---------------------
        // Phase A: DPC update does not mutate in-flight snapshot (AC-WS-21)
        // ---------------------

        // Frame 1 — admit wave at TelegraphWindowS=0.80
        FDPCFrameState Frame1;
        Frame1.bIsActive         = true;
        Frame1.TelegraphWindowS  = 0.80f;
        Frame1.WaveSpawnIntervalS = 0.0f;  // cadence gate: (10-0) >= 0 → passes
        Sub->TestOnly_TriggerDPCFrameReady(Frame1);

        TestTrue(TEXT("TC4-A: Frame1 was admitted (LastAdmissionResult == Admitted)"),
            Sub->TestOnly_GetLastAdmissionResult() == EAdmissionResult::Admitted);

        // WaveId 0 minted for the first non-barrage admission.
        const float StoredWindow0_AfterFrame1 = Sub->TestOnly_GetInFlightTelegraphWindow(0);
        TestEqual(TEXT("TC4-A: InFlight WaveId=0 TelegraphWindowS == 0.80 after Frame1"),
            StoredWindow0_AfterFrame1, 0.80f);

        // Frame 2 — publish new DPC frame with a different TelegraphWindowS=0.65.
        // This admits WaveId=1 at 0.65; WaveId=0's stored value must be unchanged.
        FDPCFrameState Frame2;
        Frame2.bIsActive         = true;
        Frame2.TelegraphWindowS  = 0.65f;
        Frame2.WaveSpawnIntervalS = 0.0f;  // (10-10)=0 >= 0 → cadence passes again
        Sub->TestOnly_TriggerDPCFrameReady(Frame2);

        TestEqual(
            TEXT("TC4-A: InFlight WaveId=0 TelegraphWindowS still 0.80 after Frame2 (DPC update immutability, AC-WS-21)"),
            Sub->TestOnly_GetInFlightTelegraphWindow(0), 0.80f);
        TestEqual(
            TEXT("TC4-A: InFlight WaveId=1 TelegraphWindowS == 0.65 (captures Frame2 value)"),
            Sub->TestOnly_GetInFlightTelegraphWindow(1), 0.65f);

        // ---------------------
        // Phase B: phase-boundary pool swap does not mutate in-flight snapshot (AC-WS-29)
        // ---------------------

        // Reset to a clean subsystem for Phase B.
        UWaveSpawnerSubsystem* Sub2 =
            NewObject<UWaveSpawnerSubsystem>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC4-B: NewObject<UWaveSpawnerSubsystem> for Phase B succeeded"), Sub2))
        {
            return false;
        }

        Sub2->TestOnly_SetLifecycleState(EWaveSpawnerLifecycleState::Active);
        Sub2->TestOnly_SetActivePhase(ERunPhase::Mid);   // MID phase for phase-boundary test
        Sub2->TestOnly_SetCurrentTimeOverride(10.f);

        // Admit wave in MID phase at TelegraphWindowS=0.82.
        FDPCFrameState MidFrame;
        MidFrame.bIsActive         = true;
        MidFrame.TelegraphWindowS  = 0.82f;
        MidFrame.WaveSpawnIntervalS = 0.0f;
        Sub2->TestOnly_TriggerDPCFrameReady(MidFrame);

        TestTrue(TEXT("TC4-B: MidFrame was admitted"),
            Sub2->TestOnly_GetLastAdmissionResult() == EAdmissionResult::Admitted);

        const float StoredWindowMid = Sub2->TestOnly_GetInFlightTelegraphWindow(0);
        TestEqual(TEXT("TC4-B: InFlight WaveId=0 TelegraphWindowS == 0.82 before pool swap"),
            StoredWindowMid, 0.82f);

        // Fire Active→Holding: triggers MID→PEAK pool-pointer swap in OnLifecycleTransition.
        // TransitionTo is public — no TestOnly seam needed.
        Sub2->TransitionTo(EWaveSpawnerLifecycleState::Holding);

        TestTrue(TEXT("TC4-B: lifecycle is now Holding after pool swap"),
            Sub2->TestOnly_GetLifecycleState() == EWaveSpawnerLifecycleState::Holding);
        TestEqual(
            TEXT("TC4-B: InFlight WaveId=0 TelegraphWindowS still 0.82 after MID→PEAK pool swap (AC-WS-29)"),
            Sub2->TestOnly_GetInFlightTelegraphWindow(0), 0.82f);

        return true;
    }

    // =========================================================================
    // TC5 — AC-WS-28: Resume grace blocks admissions within window;
    // admissions resume after grace window expires.
    //
    // Arrange: lifecycle=Active; resume grace set far in the future (EndTimeS=999.f).
    // Act A:   Fire DPC frame with bIsActive=true — grace blocks admission.
    // Assert A: LastAdmissionResult unchanged from PoolExhausted sentinel (TryAdmitPattern
    //           never reached; Rule 1 grace check returns early).
    // Act B:   Advance Now past grace EndTimeS (1000.f > 999.f). Fire DPC frame.
    // Assert B: Grace cleared lazily; LastAdmissionResult == Admitted.
    //
    // Notes on bIsActive: FDPCFrameState::bIsActive defaults to false. Gate (1a) in
    // OnDPCFrameReady returns early on !bIsActive — must set true to reach grace check.
    // Notes on cadence gate (After grace clears):
    //   Now=1000; LastSpawnTimeS=0; WaveSpawnIntervalS=0 → (1000-0)>=0 → gate passes.
    //   ShouldDrawBarrage returns false (Opener phase, not PEAK). Admission fires.
    // =========================================================================
    if (Parameters == TEXT("RSMDPCIntegration.ResumeGrace.BlocksAdmissionsDuringWindow"))
    {
        // Arrange
        UWaveSpawnerSubsystem* Sub =
            NewObject<UWaveSpawnerSubsystem>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC5: NewObject<UWaveSpawnerSubsystem> succeeded"), Sub))
        {
            return false;
        }

        Sub->TestOnly_SetLifecycleState(EWaveSpawnerLifecycleState::Active);
        // Inject grace: bInResumeGrace=true, ResumeGraceEndTimeS=999.f (far in future).
        Sub->TestOnly_SetResumeGrace(true, 999.f);
        Sub->TestOnly_SetCurrentTimeOverride(5.f);  // Now=5 < 999 → grace active

        // Precondition: LastAdmissionResult is PoolExhausted sentinel (initialized value,
        // proves TryAdmitPattern was never called if it stays at PoolExhausted after the frame).
        TestTrue(TEXT("TC5 precondition: LastAdmissionResult == PoolExhausted (sentinel)"),
            Sub->TestOnly_GetLastAdmissionResult() == EAdmissionResult::PoolExhausted);

        // Act A — frame within grace window; bIsActive=true to reach the grace check.
        FDPCFrameState GraceFrame;
        GraceFrame.bIsActive         = true;
        GraceFrame.TelegraphWindowS  = 0.94f;
        GraceFrame.WaveSpawnIntervalS = 0.0f;
        Sub->TestOnly_TriggerDPCFrameReady(GraceFrame);

        // Assert A — grace blocked admission; sentinel value unchanged.
        TestTrue(
            TEXT("TC5-A: LastAdmissionResult still PoolExhausted (TryAdmitPattern never called — "
                 "grace blocked Rule 1 gate, AC-WS-28)"),
            Sub->TestOnly_GetLastAdmissionResult() == EAdmissionResult::PoolExhausted);

        // Act B — advance time past grace expiry; fire another DPC frame.
        Sub->TestOnly_SetCurrentTimeOverride(1000.f);  // Now=1000 > ResumeGraceEndTimeS=999
        Sub->TestOnly_TriggerDPCFrameReady(GraceFrame);

        // Assert B — grace cleared lazily; admission fires.
        TestTrue(
            TEXT("TC5-B: LastAdmissionResult == Admitted after grace window expires (AC-WS-28)"),
            Sub->TestOnly_GetLastAdmissionResult() == EAdmissionResult::Admitted);

        return true;
    }

    // =========================================================================
    // TC6 — S2 coverage: Rule 14 COMPLETE state terminates correctly.
    //
    // Arrange: 1 live wave, lifecycle = Active.
    // Act:     TestOnly_FireRunStateChanged(RUNNING, COMPLETE).
    // Assert:  LiveCount == 0, lifecycle == Cold.
    // =========================================================================
    if (Parameters == TEXT("RSMDPCIntegration.RunTermination.CompleteStateTerminates"))
    {
        UWaveSpawnerSubsystem* Sub =
            NewObject<UWaveSpawnerSubsystem>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC6: NewObject<UWaveSpawnerSubsystem> succeeded"), Sub))
        {
            return false;
        }

        Sub->TestOnly_SetLifecycleState(EWaveSpawnerLifecycleState::Active);
        Sub->TestOnly_SetLiveCount(1);

        // ReleaseToPool warning expected (pool not populated in headless tests).
        Sub->TestOnly_FireRunStateChanged(ERunState::RUNNING, ERunState::COMPLETE);

        TestEqual(TEXT("TC6: LiveCount == 0 after COMPLETE termination"),
            Sub->TestOnly_GetLiveCount(), 0);
        TestTrue(
            TEXT("TC6: lifecycle == Cold after COMPLETE termination (ADR-0011 D3, AC-WS-18)"),
            Sub->TestOnly_GetLifecycleState() == EWaveSpawnerLifecycleState::Cold);

        return true;
    }

    // =========================================================================
    // TC7 — S2 coverage: Rule 14 ABORTED state terminates correctly.
    //
    // Arrange: 1 live wave, lifecycle = Active.
    // Act:     TestOnly_FireRunStateChanged(RUNNING, ABORTED).
    // Assert:  LiveCount == 0, lifecycle == Cold.
    // =========================================================================
    if (Parameters == TEXT("RSMDPCIntegration.RunTermination.AbortedStateTerminates"))
    {
        UWaveSpawnerSubsystem* Sub =
            NewObject<UWaveSpawnerSubsystem>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC7: NewObject<UWaveSpawnerSubsystem> succeeded"), Sub))
        {
            return false;
        }

        Sub->TestOnly_SetLifecycleState(EWaveSpawnerLifecycleState::Active);
        Sub->TestOnly_SetLiveCount(1);

        // ReleaseToPool warning expected (pool not populated in headless tests).
        Sub->TestOnly_FireRunStateChanged(ERunState::RUNNING, ERunState::ABORTED);

        TestEqual(TEXT("TC7: LiveCount == 0 after ABORTED termination"),
            Sub->TestOnly_GetLiveCount(), 0);
        TestTrue(
            TEXT("TC7: lifecycle == Cold after ABORTED termination (ADR-0011 D3, AC-WS-18)"),
            Sub->TestOnly_GetLifecycleState() == EWaveSpawnerLifecycleState::Cold);

        return true;
    }

    // =========================================================================
    // TC8 — S3 coverage: HandlePausedChanged(false) causal path.
    //
    // Verifies that the PRODUCTION code path HandlePausedChanged(false) →
    // OnResumeFromPause() correctly sets bInResumeGrace and ResumeGraceEndTimeS.
    // This complements TC5 (which injects grace via TestOnly_SetResumeGrace directly).
    //
    // Arrange: lifecycle = Flushing (valid resume source), Now = 5.f.
    //          LastAdmissionResult is PoolExhausted sentinel (TryAdmitPattern not yet called).
    // Act A:   TestOnly_FirePausedChanged(false, 5.0) — production resume path.
    //          This calls OnResumeFromPause() → sets grace EndTimeS = 5 + 1.5 = 6.5.
    //          Then transitions Flushing → Active.
    // Act B:   Fire DPC frame at Now = 5.f (within grace 6.5) — admission must be blocked.
    // Assert B: LastAdmissionResult still PoolExhausted (sentinel preserved — grace blocked).
    // Act C:   Advance Now = 10.f (past grace 6.5). Fire another DPC frame.
    // Assert C: LastAdmissionResult == Admitted (grace expired; admission fires).
    // =========================================================================
    if (Parameters == TEXT("RSMDPCIntegration.ResumeGrace.PausedChangedFalseSetsGrace"))
    {
        UWaveSpawnerSubsystem* Sub =
            NewObject<UWaveSpawnerSubsystem>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC8: NewObject<UWaveSpawnerSubsystem> succeeded"), Sub))
        {
            return false;
        }

        // Arrange: Flushing is the expected lifecycle after a pause flush (HandlePausedChanged(true)).
        Sub->TestOnly_SetLifecycleState(EWaveSpawnerLifecycleState::Flushing);
        Sub->TestOnly_SetCurrentTimeOverride(5.f);

        // Precondition: sentinel value proves TryAdmitPattern was not yet called.
        TestTrue(TEXT("TC8 precondition: LastAdmissionResult == PoolExhausted (sentinel)"),
            Sub->TestOnly_GetLastAdmissionResult() == EAdmissionResult::PoolExhausted);

        // Act A — fire the production resume path. HandlePausedChanged(false) calls
        // OnResumeFromPause() which sets bInResumeGrace=true and ResumeGraceEndTimeS=5+1.5=6.5.
        // Then TransitionTo(Active) fires (Flushing → Active).
        Sub->TestOnly_FirePausedChanged(false, 5.0);

        // Verify lifecycle transitioned correctly from the resume path.
        TestTrue(TEXT("TC8-A: lifecycle == Active after HandlePausedChanged(false)"),
            Sub->TestOnly_GetLifecycleState() == EWaveSpawnerLifecycleState::Active);

        // Act B — DPC frame at Now=5.f (within grace window [5, 6.5)).
        FDPCFrameState GraceFrame;
        GraceFrame.bIsActive          = true;
        GraceFrame.TelegraphWindowS   = 0.9f;
        GraceFrame.WaveSpawnIntervalS = 0.0f;
        Sub->TestOnly_TriggerDPCFrameReady(GraceFrame);

        // Assert B — grace blocked admission; PoolExhausted sentinel unchanged.
        TestTrue(
            TEXT("TC8-B: grace blocks DPC admission at Now=5 (ResumeGraceEndTimeS=6.5); "
                 "LastAdmissionResult still PoolExhausted (AC-WS-28 causal path verified)"),
            Sub->TestOnly_GetLastAdmissionResult() == EAdmissionResult::PoolExhausted);

        // Act C — advance past grace expiry.
        Sub->TestOnly_SetCurrentTimeOverride(10.f);  // 10 > 6.5 — grace expired
        Sub->TestOnly_TriggerDPCFrameReady(GraceFrame);

        // Assert C — grace cleared lazily; admission fires.
        TestTrue(
            TEXT("TC8-C: admission fires after grace expires at Now=10 (AC-WS-28 causal path)"),
            Sub->TestOnly_GetLastAdmissionResult() == EAdmissionResult::Admitted);

        return true;
    }

    // =========================================================================
    // TC9 — S4 coverage: Pause flush from Holding lifecycle state.
    //
    // Verifies that HandlePausedChanged(true) correctly flushes live waves and
    // transitions to Flushing when lifecycle is Holding (not just Active).
    //
    // Arrange: 2 live waves, lifecycle = Holding.
    // Act:     TestOnly_FirePausedChanged(true).
    // Assert:  EventLog has 6 entries (2 waves × 3 Rule 12 events each).
    //          LiveCount == 0 after flush.
    //          lifecycle == Flushing (Active|Holding → Flushing per Rule 13 / DEVIATION NOTE).
    // =========================================================================
    if (Parameters == TEXT("RSMDPCIntegration.PauseFlush.FromHoldingLifecycleState"))
    {
        UWaveSpawnerSubsystem* Sub =
            NewObject<UWaveSpawnerSubsystem>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC9: NewObject<UWaveSpawnerSubsystem> succeeded"), Sub))
        {
            return false;
        }

        FWaveSpawnerCallbackTestStub Stub;
        Sub->SetDespawnCallback(&Stub);
        // Holding is reached during the phase-boundary drain window (Active → Holding).
        // Pause can fire during Holding — the handler must handle this case (S4 coverage).
        Sub->TestOnly_SetLifecycleState(EWaveSpawnerLifecycleState::Holding);
        Sub->TestOnly_SetLiveCount(2);  // WaveIds -1001, -1002

        // Preconditions
        TestEqual(TEXT("TC9 precondition: LiveCount == 2 before flush"),
            Sub->TestOnly_GetLiveCount(), 2);

        // Act — ReleaseToPool warnings expected (2 warns, one per wave).
        Sub->TestOnly_FirePausedChanged(true, 0.0);

        // Assert — event count
        const TArray<FDespawnEvent>& Log = Stub.GetEventLog();
        TestEqual(TEXT("TC9: EventLog has exactly 6 entries (2 waves × 3 events)"), Log.Num(), 6);

        if (Log.Num() == 6)
        {
            // Verify PauseFlush reason on both WaveDespawned entries.
            TestTrue(TEXT("TC9: Block0[2] WaveDespawned with PauseFlush reason"),
                Log[2].EventName == FName(TEXT("WaveDespawned"))
                && Log[2].Reason == EWaveDespawnReason::PauseFlush);
            TestTrue(TEXT("TC9: Block1[2] WaveDespawned with PauseFlush reason"),
                Log[5].EventName == FName(TEXT("WaveDespawned"))
                && Log[5].Reason == EWaveDespawnReason::PauseFlush);
            // WaveId ASC flush order.
            TestTrue(TEXT("TC9: WaveDespawned entries in WaveId ASC order"),
                Log[2].WaveId < Log[5].WaveId);
        }

        TestEqual(TEXT("TC9: LiveSlots.Num() == 0 after pause flush from Holding"),
            Sub->TestOnly_GetLiveCount(), 0);
        TestTrue(TEXT("TC9: lifecycle == Flushing after pause flush from Holding (Rule 13 / DEVIATION NOTE)"),
            Sub->TestOnly_GetLifecycleState() == EWaveSpawnerLifecycleState::Flushing);

        Sub->SetDespawnCallback(nullptr);
        return true;
    }

    // Unrecognized test command — fail with diagnostic.
    AddError(FString::Printf(
        TEXT("WaveSpawnerRSMDPCIntegrationTest: unrecognized Parameters='%s'"),
        *Parameters));
    return false;
}

#endif  // WITH_DEV_AUTOMATION_TESTS

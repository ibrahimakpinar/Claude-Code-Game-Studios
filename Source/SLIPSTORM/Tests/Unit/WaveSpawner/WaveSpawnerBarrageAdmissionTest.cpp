// Copyright (c) Game Studio. All rights reserved.
// SLIPSTORM — 60-second voxel endless runner, mobile, Unreal Engine 5.7
//
// WaveSpawnerBarrageAdmissionTest.cpp — Story 004 unit tests for Rule 7 two-layer slot
// pre-commitment in UWaveSpawnerSubsystem::TryAdmitPattern.
//
// Story: production/epics/wave-spawner/story-004-barrage-atomic-admission.md
// ADR:   docs/architecture/adr-0011-wave-spawner-pattern-library.md (D2 Stage 2)
// TRs:   TR-WS-017 (barrage requires 3 free slots atomically; non-barrage requires 1)
//         TR-WS-018 (dropped barrages set bBarrageOwed; next >=3 window reserved)
//
// Test category: SLIPSTORM.WaveSpawner.BarrageAdmission
// Runner:        UE Automation Framework
//                Flags: ApplicationContextMask | ProductFilter
//
// All 9 test commands are fully headless (no UWorld needed).
// NewObject<UWaveSpawnerSubsystem>(GetTransientPackage()) + TestOnly_* seams throughout.
//
// TC4 (BarrageAdmitted3Slots) and TC7 (ReservationFulfilled) exercise the same mechanical
// scenario (bBarrageOwed=true, available>=3 → barrage admitted). They are kept separate
// because they assert different invariants: TC4 verifies the +3 slot pre-commitment
// (AC-WS-12b), TC7 verifies bBarrageOwed is cleared (AC-WS-12c).
//
// ShouldDrawBarrage() is always false in this story (Story 005 stub). The barrage path is
// entered only when bBarrageOwed=true, injected via TestOnly_SetBarrageOwed().
//
// Deferred_BarrageOwed is unreachable in Story 004 — reservation is fulfilled rather than
// used to veto non-barrage draws. Story 005 scope.
//
// Test isolation: each TC constructs a fresh UWaveSpawnerSubsystem via NewObject<> and
// sets up its own preconditions. No state carries across RunTest dispatches
// (IMPLEMENT_COMPLEX_AUTOMATION_TEST issues a separate RunTest call per command).
//
// Guard: WITH_DEV_AUTOMATION_TESTS only.

#if WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#include "WaveSpawner/WaveSpawnerSubsystem.h"
#include "WaveSpawner/WaveSpawnerTypes.h"
#include "DPC/DPCSubsystem.h"

// ============================================================================
// IMPLEMENT_COMPLEX_AUTOMATION_TEST
// 9 test commands covering Story 004 acceptance criteria.
// Pattern matches WaveSpawnerAdmissionGateTest.cpp (Story 003).
// ============================================================================

IMPLEMENT_COMPLEX_AUTOMATION_TEST(
    FWaveSpawnerBarrageAdmissionTest,
    "SLIPSTORM.WaveSpawner.BarrageAdmission",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

void FWaveSpawnerBarrageAdmissionTest::GetTests(
    TArray<FString>& OutBeautifiedNames,
    TArray<FString>& OutTestCommands) const
{
    OutBeautifiedNames.Add(
        TEXT("Slot formula: inject Scheduled=5 Live=3 → available=15 (AC-WS-12a, TR-WS-017)"));
    OutTestCommands.Add(TEXT("Barrage.SlotFormulaCorrect"));

    OutBeautifiedNames.Add(
        TEXT("Non-barrage: available=1 → admitted, ScheduledCount becomes 23 (AC-WS-12a, TR-WS-017)"));
    OutTestCommands.Add(TEXT("Barrage.NonBarrageAdmitted"));

    OutBeautifiedNames.Add(
        TEXT("Non-barrage: available=0 → Deferred_ConcurrencyCap, count unchanged (AC-WS-12a, TR-WS-017)"));
    OutTestCommands.Add(TEXT("Barrage.NonBarrageConcurrencyVeto"));

    OutBeautifiedNames.Add(
        TEXT("Barrage: bBarrageOwed=true, available=5 → 3 slots pre-committed (AC-WS-12b, TR-WS-017)"));
    OutTestCommands.Add(TEXT("Barrage.BarrageAdmitted3Slots"));

    OutBeautifiedNames.Add(
        TEXT("Barrage dropped: available=2 → bBarrageOwed=true, non-barrage fallback admitted (AC-WS-12b, TR-WS-018)"));
    OutTestCommands.Add(TEXT("Barrage.BarrageDroppedFallback"));

    OutBeautifiedNames.Add(
        TEXT("Barrage dropped: available=0 → bBarrageOwed=true, Deferred_SlotAtomic, no fallback (AC-WS-12b, TR-WS-018)"));
    OutTestCommands.Add(TEXT("Barrage.BarrageDroppedPoolFull"));

    OutBeautifiedNames.Add(
        TEXT("Reservation fulfilled: bBarrageOwed=true, available=5 → admitted, bBarrageOwed cleared (AC-WS-12c, TR-WS-018)"));
    OutTestCommands.Add(TEXT("Barrage.ReservationFulfilled"));

    OutBeautifiedNames.Add(
        TEXT("Reservation persists across pause-flush: bBarrageOwed NOT cleared by Cold->Active->Holding->Flushing->Active (AC-WS-12c, TR-WS-018)"));
    OutTestCommands.Add(TEXT("Barrage.ReservationPersistsPausedFlush"));

    OutBeautifiedNames.Add(
        TEXT("Barrage admitted at exact 3-slot boundary: threshold is >= 3, not > 3 (AC-WS-12b, TR-WS-017)"));
    OutTestCommands.Add(TEXT("Barrage.BarrageAdmittedAtExactBoundary"));
}

bool FWaveSpawnerBarrageAdmissionTest::RunTest(const FString& Parameters)
{
    // -------------------------------------------------------------------------
    // TC1 — AC-WS-12a (TR-WS-017): Slot formula correctness.
    //
    // Verifies: available = KMaxConcurrentWavesStub − (ScheduledSlots.Num() + LiveSlots.Num())
    //
    // Given:  inject Scheduled=5, Live=3 via test seams.
    // When:   TestOnly_GetAvailableSlots().
    // Then:   available == 15  (23 − (5+3) = 15).
    //
    // Does not call TriggerDPCFrameReady — tests the formula directly.
    // -------------------------------------------------------------------------
    if (Parameters == TEXT("Barrage.SlotFormulaCorrect"))
    {
        UWaveSpawnerSubsystem* Sub =
            NewObject<UWaveSpawnerSubsystem>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC1: NewObject succeeded"), Sub))
        {
            return false;
        }

        // Arrange
        Sub->TestOnly_SetScheduledCount(5);
        Sub->TestOnly_SetLiveCount(3);

        // Act + Assert
        TestEqual(
            TEXT("TC1: available == 15  (23 − (5+3) = 15; slot formula AC-WS-12a, TR-WS-017)"),
            Sub->TestOnly_GetAvailableSlots(),
            15);
        TestEqual(
            TEXT("TC1: ScheduledSlots.Num() == 5 — injection sanity check"),
            Sub->TestOnly_GetScheduledCount(),
            5);
        TestEqual(
            TEXT("TC1: LiveSlots.Num() == 3 — injection sanity check"),
            Sub->TestOnly_GetLiveCount(),
            3);

        return true;
    }

    // -------------------------------------------------------------------------
    // TC2 — AC-WS-12a (TR-WS-017): Non-barrage draw admitted at pool edge.
    //
    // Given:  Scheduled=22, Live=0 (available=1); Active state; primer pending=true;
    //         bIsActive=true. bBarrageOwed=false (default).
    // When:   TestOnly_TriggerDPCFrameReady.
    // Then:   LastAdmissionResult == Admitted.
    //         ScheduledCount == 23 (+1 stub WaveId added at NextWaveIdCounter=0).
    // -------------------------------------------------------------------------
    if (Parameters == TEXT("Barrage.NonBarrageAdmitted"))
    {
        UWaveSpawnerSubsystem* Sub =
            NewObject<UWaveSpawnerSubsystem>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC2: NewObject succeeded"), Sub))
        {
            return false;
        }

        // Arrange
        Sub->TestOnly_SetLifecycleState(EWaveSpawnerLifecycleState::Active);
        Sub->TestOnly_SetPrimerPending(true);   // routes to TryAdmitPattern via primer path
        Sub->TestOnly_SetScheduledCount(22);    // available = 23 − 22 = 1
        Sub->TestOnly_SetCurrentTimeOverride(1.f);

        FDPCFrameState FrameState;
        FrameState.bIsActive          = true;
        FrameState.WaveSpawnIntervalS = 2.5f;

        // Act
        Sub->TestOnly_TriggerDPCFrameReady(FrameState);

        // Assert
        TestEqual(
            TEXT("TC2: LastAdmissionResult == Admitted — non-barrage path, available=1 (AC-WS-12a)"),
            Sub->TestOnly_GetLastAdmissionResult(),
            EAdmissionResult::Admitted);
        TestEqual(
            TEXT("TC2: ScheduledCount == 23 — one slot pre-committed (TR-WS-017)"),
            Sub->TestOnly_GetScheduledCount(),
            23);

        return true;
    }

    // -------------------------------------------------------------------------
    // TC3 — AC-WS-12a (TR-WS-017): Non-barrage veto at zero available slots.
    //
    // Given:  Scheduled=23, Live=0 (available=0); Active state; primer pending=true;
    //         bIsActive=true. bBarrageOwed=false (default).
    // When:   TestOnly_TriggerDPCFrameReady.
    // Then:   LastAdmissionResult == Deferred_ConcurrencyCap.
    //         ScheduledCount == 23 (unchanged — nothing added).
    //
    // LastAdmissionResult is initialized to PoolExhausted; asserting Deferred_ConcurrencyCap
    // proves TryAdmitPattern executed (not just the initial value).
    // -------------------------------------------------------------------------
    if (Parameters == TEXT("Barrage.NonBarrageConcurrencyVeto"))
    {
        UWaveSpawnerSubsystem* Sub =
            NewObject<UWaveSpawnerSubsystem>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC3: NewObject succeeded"), Sub))
        {
            return false;
        }

        // Arrange
        Sub->TestOnly_SetLifecycleState(EWaveSpawnerLifecycleState::Active);
        Sub->TestOnly_SetPrimerPending(true);
        Sub->TestOnly_SetScheduledCount(23);    // available = 23 − 23 = 0
        Sub->TestOnly_SetCurrentTimeOverride(1.f);

        FDPCFrameState FrameState;
        FrameState.bIsActive          = true;
        FrameState.WaveSpawnIntervalS = 2.5f;

        // Act
        Sub->TestOnly_TriggerDPCFrameReady(FrameState);

        // Assert
        TestEqual(
            TEXT("TC3: LastAdmissionResult == Deferred_ConcurrencyCap — available=0, "
                 "non-barrage path (AC-WS-12a, TR-WS-017); PoolExhausted initial value disproves no-op"),
            Sub->TestOnly_GetLastAdmissionResult(),
            EAdmissionResult::Deferred_ConcurrencyCap);
        TestEqual(
            TEXT("TC3: ScheduledCount == 23 — unchanged; nothing admitted (TR-WS-017)"),
            Sub->TestOnly_GetScheduledCount(),
            23);

        return true;
    }

    // -------------------------------------------------------------------------
    // TC4 — AC-WS-12b (TR-WS-017): Barrage admitted with 3-slot pre-commitment.
    //
    // Given:  Scheduled=18, Live=0 (available=5); bBarrageOwed=true;
    //         Active state; primer pending=true; bIsActive=true.
    // When:   TestOnly_TriggerDPCFrameReady.
    // Then:   LastAdmissionResult == Admitted.
    //         ScheduledCount == 21 (18 injected + 3 barrage slots).
    //
    // NOTE: same mechanical scenario as TC7 (ReservationFulfilled); asserts +3 slot count
    // rather than flag clearance. Both TCs are kept separate per design direction.
    // -------------------------------------------------------------------------
    if (Parameters == TEXT("Barrage.BarrageAdmitted3Slots"))
    {
        UWaveSpawnerSubsystem* Sub =
            NewObject<UWaveSpawnerSubsystem>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC4: NewObject succeeded"), Sub))
        {
            return false;
        }

        // Arrange
        Sub->TestOnly_SetLifecycleState(EWaveSpawnerLifecycleState::Active);
        Sub->TestOnly_SetPrimerPending(true);
        Sub->TestOnly_SetScheduledCount(18);    // available = 23 − 18 = 5
        Sub->TestOnly_SetBarrageOwed(true);     // enters barrage path (ShouldDrawBarrage stub=false)
        Sub->TestOnly_SetCurrentTimeOverride(1.f);

        // Precondition: confirm available == 5 before acting.
        // If kMaxConcurrentWavesStub changes (Story 005), this assertion signals
        // the test needs a new Scheduled injection rather than failing with confusing arithmetic.
        TestEqual(
            TEXT("TC4 precondition: available == 5 (18 scheduled, kMaxConcurrentWavesStub=23)"),
            Sub->TestOnly_GetAvailableSlots(),
            5);

        FDPCFrameState FrameState;
        FrameState.bIsActive          = true;
        FrameState.WaveSpawnIntervalS = 2.5f;

        // Act
        Sub->TestOnly_TriggerDPCFrameReady(FrameState);

        // Assert
        TestEqual(
            TEXT("TC4: LastAdmissionResult == Admitted — barrage path, available=5 >= 3 (AC-WS-12b)"),
            Sub->TestOnly_GetLastAdmissionResult(),
            EAdmissionResult::Admitted);
        TestEqual(
            TEXT("TC4: ScheduledCount == 21 — 3 barrage slots pre-committed (18 + 3; TR-WS-017)"),
            Sub->TestOnly_GetScheduledCount(),
            21);

        return true;
    }

    // -------------------------------------------------------------------------
    // TC5 — AC-WS-12b (TR-WS-018): Barrage dropped; non-barrage fallback admitted.
    //
    // Given:  Scheduled=21, Live=0 (available=2 < 3); bBarrageOwed=true;
    //         Active state; primer pending=true; bIsActive=true.
    // When:   TestOnly_TriggerDPCFrameReady.
    // Then:   LastAdmissionResult == Admitted (non-barrage fallback admitted).
    //         bBarrageOwed == true (reservation kept — only cleared on successful barrage draw).
    //         ScheduledCount == 22 (21 + 1 fallback slot).
    // -------------------------------------------------------------------------
    if (Parameters == TEXT("Barrage.BarrageDroppedFallback"))
    {
        UWaveSpawnerSubsystem* Sub =
            NewObject<UWaveSpawnerSubsystem>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC5: NewObject succeeded"), Sub))
        {
            return false;
        }

        // Arrange
        Sub->TestOnly_SetLifecycleState(EWaveSpawnerLifecycleState::Active);
        Sub->TestOnly_SetPrimerPending(true);
        Sub->TestOnly_SetScheduledCount(21);    // available = 23 − 21 = 2  (< 3)
        Sub->TestOnly_SetBarrageOwed(true);
        Sub->TestOnly_SetCurrentTimeOverride(1.f);

        // Precondition: confirm available == 2 before acting.
        // If kMaxConcurrentWavesStub changes (Story 005), this assertion signals
        // the test needs a new Scheduled injection rather than failing with confusing arithmetic.
        TestEqual(
            TEXT("TC5 precondition: available == 2 (21 scheduled, kMaxConcurrentWavesStub=23)"),
            Sub->TestOnly_GetAvailableSlots(),
            2);

        FDPCFrameState FrameState;
        FrameState.bIsActive          = true;
        FrameState.WaveSpawnIntervalS = 2.5f;

        // Act
        Sub->TestOnly_TriggerDPCFrameReady(FrameState);

        // Assert
        TestEqual(
            TEXT("TC5: LastAdmissionResult == Admitted — non-barrage fallback took 1 slot (AC-WS-12b)"),
            Sub->TestOnly_GetLastAdmissionResult(),
            EAdmissionResult::Admitted);
        TestTrue(
            TEXT("TC5: bBarrageOwed == true — reservation preserved; not cleared on fallback (TR-WS-018)"),
            Sub->TestOnly_GetBarrageOwed());
        TestEqual(
            TEXT("TC5: ScheduledCount == 22 — 21 injected + 1 fallback slot (TR-WS-017)"),
            Sub->TestOnly_GetScheduledCount(),
            22);

        return true;
    }

    // -------------------------------------------------------------------------
    // TC6 — AC-WS-12b (TR-WS-018): Barrage dropped; pool full — no fallback.
    //
    // Given:  Scheduled=23, Live=0 (available=0); bBarrageOwed=true;
    //         Active state; primer pending=true; bIsActive=true.
    // When:   TestOnly_TriggerDPCFrameReady.
    // Then:   LastAdmissionResult == Deferred_SlotAtomic (barrage + fallback both failed).
    //         bBarrageOwed == true (reservation kept).
    //         ScheduledCount == 23 (unchanged — nothing admitted).
    // -------------------------------------------------------------------------
    if (Parameters == TEXT("Barrage.BarrageDroppedPoolFull"))
    {
        UWaveSpawnerSubsystem* Sub =
            NewObject<UWaveSpawnerSubsystem>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC6: NewObject succeeded"), Sub))
        {
            return false;
        }

        // Arrange
        Sub->TestOnly_SetLifecycleState(EWaveSpawnerLifecycleState::Active);
        Sub->TestOnly_SetPrimerPending(true);
        Sub->TestOnly_SetScheduledCount(23);    // available = 23 − 23 = 0
        Sub->TestOnly_SetBarrageOwed(true);
        Sub->TestOnly_SetCurrentTimeOverride(1.f);

        FDPCFrameState FrameState;
        FrameState.bIsActive          = true;
        FrameState.WaveSpawnIntervalS = 2.5f;

        // Act
        Sub->TestOnly_TriggerDPCFrameReady(FrameState);

        // Assert
        TestEqual(
            TEXT("TC6: LastAdmissionResult == Deferred_SlotAtomic — barrage owed, available=0, "
                 "no fallback possible (AC-WS-12b, TR-WS-017)"),
            Sub->TestOnly_GetLastAdmissionResult(),
            EAdmissionResult::Deferred_SlotAtomic);
        TestTrue(
            TEXT("TC6: bBarrageOwed == true — reservation kept on full-drop (TR-WS-018)"),
            Sub->TestOnly_GetBarrageOwed());
        TestEqual(
            TEXT("TC6: ScheduledCount == 23 — unchanged; no admission (TR-WS-017)"),
            Sub->TestOnly_GetScheduledCount(),
            23);

        return true;
    }

    // -------------------------------------------------------------------------
    // TC7 — AC-WS-12c (TR-WS-018): Reservation fulfilled; bBarrageOwed cleared.
    //
    // Given:  Scheduled=18, Live=0 (available=5 >= 3); bBarrageOwed=true;
    //         Active state; primer pending=true; bIsActive=true.
    // When:   TestOnly_TriggerDPCFrameReady.
    // Then:   LastAdmissionResult == Admitted.
    //         bBarrageOwed == false (reservation consumed on successful barrage draw).
    //
    // NOTE: same mechanical scenario as TC4 (BarrageAdmitted3Slots); asserts flag
    // clearance rather than slot count. Both TCs are kept separate per design direction.
    // -------------------------------------------------------------------------
    if (Parameters == TEXT("Barrage.ReservationFulfilled"))
    {
        UWaveSpawnerSubsystem* Sub =
            NewObject<UWaveSpawnerSubsystem>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC7: NewObject succeeded"), Sub))
        {
            return false;
        }

        // Arrange
        Sub->TestOnly_SetLifecycleState(EWaveSpawnerLifecycleState::Active);
        Sub->TestOnly_SetPrimerPending(true);
        Sub->TestOnly_SetScheduledCount(18);    // available = 23 − 18 = 5
        Sub->TestOnly_SetBarrageOwed(true);
        Sub->TestOnly_SetCurrentTimeOverride(1.f);

        // Precondition: confirm available == 5 before acting.
        // If kMaxConcurrentWavesStub changes (Story 005), this assertion signals
        // the test needs a new Scheduled injection rather than failing with confusing arithmetic.
        TestEqual(
            TEXT("TC7 precondition: available == 5 (18 scheduled, kMaxConcurrentWavesStub=23)"),
            Sub->TestOnly_GetAvailableSlots(),
            5);

        FDPCFrameState FrameState;
        FrameState.bIsActive          = true;
        FrameState.WaveSpawnIntervalS = 2.5f;

        // Act
        Sub->TestOnly_TriggerDPCFrameReady(FrameState);

        // Assert
        TestEqual(
            TEXT("TC7: LastAdmissionResult == Admitted — barrage reservation fulfilled (AC-WS-12c)"),
            Sub->TestOnly_GetLastAdmissionResult(),
            EAdmissionResult::Admitted);
        TestFalse(
            TEXT("TC7: bBarrageOwed == false — reservation cleared on successful barrage draw (TR-WS-018)"),
            Sub->TestOnly_GetBarrageOwed());

        return true;
    }

    // -------------------------------------------------------------------------
    // TC8 — AC-WS-12c (TR-WS-018): Reservation persists across pause-flush.
    //
    // Drives the real pause-flush lifecycle sequence via TransitionTo() (not seam bypass)
    // to exercise the actual OnLifecycleTransition() code paths that Story 007 will modify.
    // bBarrageOwed must NOT be cleared by any transition on the pause-flush path; it is
    // only cleared on successful barrage admission or Run Termination (Flushing→Cold).
    //
    // Pause-flush sequence per ADR-0011 D3:
    //   Cold→Active:        initialise OpenerPool pointer + primer (both irrelevant here)
    //   Active→Holding:     phase-boundary drain window entry (Opener→Mid pool swap)
    //   Holding→Flushing:   pause-flush path — stale scheduled_slots; Story 007 owns flush logic
    //   Flushing→Active:    pause-flush complete; admissions resume
    //
    // Given:  bBarrageOwed=true (set after Cold→Active, mid-run scenario).
    // When:   TransitionTo(Holding) → TransitionTo(Flushing) → TransitionTo(Active).
    // Then:   bBarrageOwed == true (unchanged — no OnLifecycleTransition case clears it
    //         except the Cold case, which is run termination, not pause-flush).
    //
    // This TC would fail if a future OnLifecycleTransition(Flushing) or
    // OnLifecycleTransition(Active) accidentally added bBarrageOwed = false — catching
    // the regression that TC8's prior seam-bypass form could not detect.
    // -------------------------------------------------------------------------
    if (Parameters == TEXT("Barrage.ReservationPersistsPausedFlush"))
    {
        UWaveSpawnerSubsystem* Sub =
            NewObject<UWaveSpawnerSubsystem>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC8: NewObject succeeded"), Sub))
        {
            return false;
        }

        // Arrange + Act: drive the real pause-flush lifecycle via TransitionTo().
        // Cold→Active: sets ActiveDrawPool=&OpenerPool, bPrimerPending=true (harmless here).
        Sub->TransitionTo(EWaveSpawnerLifecycleState::Active);
        // Simulate barrage reservation set during the active run (e.g. by a prior drop).
        Sub->TestOnly_SetBarrageOwed(true);
        // Active→Holding: phase-boundary drain window; advances pool Opener→Mid.
        Sub->TransitionTo(EWaveSpawnerLifecycleState::Holding);
        // Holding→Flushing: pause-flush path per ADR-0011 D3 (stale scheduled_slots rule).
        // This is the transition Story 007 will modify — the most likely site for a future
        // accidental bBarrageOwed = false if the invariant were ever broken.
        Sub->TransitionTo(EWaveSpawnerLifecycleState::Flushing);
        // Flushing→Active: pause-flush complete; admissions resume under existing pool.
        Sub->TransitionTo(EWaveSpawnerLifecycleState::Active);

        // Assert: bBarrageOwed must survive the full pause-flush sequence.
        TestTrue(
            TEXT("TC8: bBarrageOwed == true — reservation NOT cleared by Cold->Active->Holding->Flushing->Active; "
                 "only cleared on successful barrage draw or Run Termination (AC-WS-12c, TR-WS-018)"),
            Sub->TestOnly_GetBarrageOwed());

        return true;
    }

    // -------------------------------------------------------------------------
    // TC9 — AC-WS-12b (TR-WS-017): Barrage admitted at exact 3-slot boundary.
    //
    // The admission predicate is `available >= 3`, not `available > 3`. An off-by-one
    // (`> 3`) would silently admit barrages only at 4+ slots, leaving the 3-slot case
    // as a deferred drop even though concurrency permits it. This TC catches that
    // regression by driving exactly the threshold value.
    //
    // Given:  ScheduledCount=20 → available = kMaxConcurrentWavesStub(23) − 20 = 3 (exact).
    //         bBarrageOwed=true (pending reservation).
    // When:   OnDPCFrameReady() fires with bIsActive=true (primer already fulfilled).
    // Then:   LastAdmissionResult == Admitted — 3-slot window is sufficient.
    //         ScheduledCount == 23 — exactly 3 new slots pre-committed.
    //         bBarrageOwed == false — reservation fulfilled on this tick.
    // -------------------------------------------------------------------------
    if (Parameters == TEXT("Barrage.BarrageAdmittedAtExactBoundary"))
    {
        UWaveSpawnerSubsystem* Sub =
            NewObject<UWaveSpawnerSubsystem>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC9: NewObject succeeded"), Sub))
        {
            return false;
        }

        // Arrange
        Sub->TestOnly_SetLifecycleState(EWaveSpawnerLifecycleState::Active);
        Sub->TestOnly_SetPrimerPending(true);   // primer already armed so cadence gate bypassed
        Sub->TestOnly_SetScheduledCount(20);    // available = 23 − 20 = 3 (exact threshold)
        Sub->TestOnly_SetBarrageOwed(true);
        Sub->TestOnly_SetCurrentTimeOverride(1.f);

        // Precondition assertion — catch test fixture errors early.
        TestEqual(
            TEXT("TC9 precondition: available == 3 (exact barrage threshold, kMaxConcurrentWavesStub=23)"),
            Sub->TestOnly_GetAvailableSlots(), 3);

        // Act
        FDPCFrameState FrameState;
        FrameState.bIsActive          = true;
        FrameState.WaveSpawnIntervalS = 2.5f;
        Sub->TestOnly_TriggerDPCFrameReady(FrameState);

        // Assert: barrage must be admitted at exactly 3 available slots.
        TestEqual(
            TEXT("TC9: LastAdmissionResult == Admitted — threshold is >= 3, not > 3 (AC-WS-12b, TR-WS-017)"),
            Sub->TestOnly_GetLastAdmissionResult(), EAdmissionResult::Admitted);
        TestEqual(
            TEXT("TC9: ScheduledCount == 23 — 3 barrage slots pre-committed at exact boundary (20 + 3)"),
            Sub->TestOnly_GetScheduledCount(), 23);
        TestFalse(
            TEXT("TC9: bBarrageOwed == false — reservation fulfilled at 3-slot boundary (TR-WS-018)"),
            Sub->TestOnly_GetBarrageOwed());

        return true;
    }

    // Unknown parameter — fail explicitly rather than silently returning true.
    AddError(FString::Printf(
        TEXT("FWaveSpawnerBarrageAdmissionTest::RunTest — unknown Parameters value: '%s'"),
        *Parameters));
    return false;
}

#endif // WITH_DEV_AUTOMATION_TESTS

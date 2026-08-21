// Copyright (c) Game Studio. All rights reserved.
// SLIPSTORM — 60-second voxel endless runner, mobile, Unreal Engine 5.7
//
// WaveSpawnerTelemetryTest.cpp — Integration tests for Story 009 telemetry infrastructure.
//
// Story: production/epics/wave-spawner/story-009-telemetry-edge-case-defense.md
// TRs:   TR-WS-019, TR-WS-030
// ACs:   AC-WS-23, AC-WS-24, AC-WS-25, AC-WS-26, AC-WS-27a/b, AC-WS-30
// ADR:   docs/architecture/adr-0011-wave-spawner-pattern-library.md
//
// Test naming: SLIPSTORM.WaveSpawner.Telemetry.[Feature]
// Framework:   UE Automation Framework (IMPLEMENT_SIMPLE_AUTOMATION_TEST)
// Isolation:   Each test case creates a fresh UWaveSpawnerSubsystem via NewObject<>;
//              no world or actor pool needed for pure-logic paths.

#if WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "WaveSpawner/WaveSpawnerSubsystem.h"
#include "WaveSpawner/WaveSpawnerCookTimeValidator.h"
#include "DPC/DPCSubsystem.h"

// ---------------------------------------------------------------------------
// Helper: build a minimal FDPCFrameState for tests that trigger admission.
// ---------------------------------------------------------------------------
static FDPCFrameState MakeActiveFrameState(float TelegraphWindowS = 1.0f, float SpawnIntervalS = 2.5f)
{
    FDPCFrameState F;
    F.bIsActive            = true;
    F.TelegraphWindowS     = TelegraphWindowS;
    F.WaveSpawnIntervalS   = SpawnIntervalS;
    return F;
}

// ---------------------------------------------------------------------------
// Helper: build a minimal non-barrage FPatternDefinition with a unique PatternId.
// ---------------------------------------------------------------------------
static FPatternDefinition MakeNonBarragePattern(FName PatternId)
{
    FPatternDefinition P;
    P.PatternId   = PatternId;
    P.bIsBarrage  = false;
    return P;
}

// ---------------------------------------------------------------------------
// Helper: build a valid PEAK barrage FPatternDefinition (tier ≥ 2, valid triplet).
// Uses first valid triplet: {0,1,3}.
// ---------------------------------------------------------------------------
static FPatternDefinition MakeValidPeakBarragePattern(FName PatternId)
{
    FPatternDefinition P;
    P.PatternId          = PatternId;
    P.bIsBarrage         = true;
    P.LeanMagnitudeTier  = 2;
    P.SourceLanes        = { 0, 1, 3 };
    return P;
}

// ---------------------------------------------------------------------------
// Helper: build an invalid PEAK barrage FPatternDefinition (tier < 2).
// ---------------------------------------------------------------------------
static FPatternDefinition MakeInvalidPeakBarragePattern(FName PatternId)
{
    FPatternDefinition P;
    P.PatternId          = PatternId;
    P.bIsBarrage         = true;
    P.LeanMagnitudeTier  = 1;  // invalid: must be >= 2 (AC-WS-27a)
    P.SourceLanes        = { 0, 1, 3 };
    return P;
}

// ---------------------------------------------------------------------------
// Helper: drive subsystem into Active lifecycle state (skipping RSM).
// Primer pending is set true so TryAdmitPattern bypasses the cadence gate.
// ---------------------------------------------------------------------------
static void DriveToActive(UWaveSpawnerSubsystem* S, float CurrentTime = 1.0f)
{
    S->TestOnly_SetCurrentTimeOverride(CurrentTime);
    // Cold → Active: normally fired by HandleRunStateChanged(RUNNING). Use FireRunStateChanged seam.
    S->TestOnly_FireRunStateChanged(ERunState::IDLE, ERunState::RUNNING, ERunOutcome::None, 0.0);
    // Set primer so TryAdmitPattern bypasses cadence gate on the first call.
    S->TestOnly_SetPrimerPending(true);
}

// ===========================================================================
// TC1 — AC-WS-25: pattern_admitted event fires on successful non-barrage draw
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWaveSpawnerTelemetryPatternAdmittedTest,
    "SLIPSTORM.WaveSpawner.Telemetry.PatternAdmitted",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FWaveSpawnerTelemetryPatternAdmittedTest::RunTest(const FString& Parameters)
{
    UWaveSpawnerSubsystem* S = NewObject<UWaveSpawnerSubsystem>();
    S->TestOnly_ResetTelemetryCapture();

    // Inject a valid non-barrage pattern into the Opener pool.
    TArray<FPatternDefinition> Patterns;
    Patterns.Add(MakeNonBarragePattern(FName(TEXT("pat_opener_01"))));
    S->TestOnly_InjectOpenerPool(Patterns);

    DriveToActive(S, 1.0f);

    // Bypass primer: call TryAdmitPattern directly via test seam.
    const FDPCFrameState Frame = MakeActiveFrameState(/*TelegraphWindowS=*/1.5f);
    S->TestOnly_TryAdmitPattern(Frame);

    // Verify pattern_admitted fired with correct fields.
    TestTrue(TEXT("pattern_admitted emitted"),
        S->TestOnly_WasEventEmitted(FName(TEXT("pattern_admitted"))));
    TestEqual(TEXT("PhasePool == OPENER"),
        S->TestOnly_GetLastEventPayload(FName(TEXT("pattern_admitted")), FName(TEXT("PhasePool"))),
        FString(TEXT("OPENER")));
    TestEqual(TEXT("bIsBarrage == false"),
        S->TestOnly_GetLastEventPayload(FName(TEXT("pattern_admitted")), FName(TEXT("bIsBarrage"))),
        FString(TEXT("false")));
    TestEqual(TEXT("WaveId == 0"),
        S->TestOnly_GetLastEventPayload(FName(TEXT("pattern_admitted")), FName(TEXT("WaveId"))),
        FString(TEXT("0")));
    TestFalse(TEXT("TelegraphWindowS field present"),
        S->TestOnly_GetLastEventPayload(FName(TEXT("pattern_admitted")), FName(TEXT("TelegraphWindowS"))).IsEmpty());

    return true;
}

// ===========================================================================
// TC2 — AC-WS-26: pause_flush_executed fires with correct WavesFlushedCount
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWaveSpawnerTelemetryPauseFlushTest,
    "SLIPSTORM.WaveSpawner.Telemetry.PauseFlushExecuted",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FWaveSpawnerTelemetryPauseFlushTest::RunTest(const FString& Parameters)
{
    UWaveSpawnerSubsystem* S = NewObject<UWaveSpawnerSubsystem>();
    S->TestOnly_ResetTelemetryCapture();

    DriveToActive(S, 1.0f);

    // Inject 1 live slot to simulate a wave in flight.
    S->TestOnly_SetLiveCount(1);
    S->TestOnly_ResetTelemetryCapture();  // clear primer events

    // Fire pause → triggers Rule 13 pause-flush.
    S->TestOnly_FirePausedChanged(true, 1.0);

    TestTrue(TEXT("pause_flush_executed emitted"),
        S->TestOnly_WasEventEmitted(FName(TEXT("pause_flush_executed"))));
    TestEqual(TEXT("WavesFlushedCount == 1"),
        S->TestOnly_GetLastEventPayload(FName(TEXT("pause_flush_executed")), FName(TEXT("WavesFlushedCount"))),
        FString(TEXT("1")));

    return true;
}

// ===========================================================================
// TC3 — AC-WS-27: run_termination_flush_executed fires with WavesTerminatedCount
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWaveSpawnerTelemetryRunTerminationTest,
    "SLIPSTORM.WaveSpawner.Telemetry.RunTerminationFlushExecuted",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FWaveSpawnerTelemetryRunTerminationTest::RunTest(const FString& Parameters)
{
    UWaveSpawnerSubsystem* S = NewObject<UWaveSpawnerSubsystem>();
    S->TestOnly_ResetTelemetryCapture();

    DriveToActive(S, 1.0f);
    S->TestOnly_SetLiveCount(1);
    S->TestOnly_ResetTelemetryCapture();

    // Fire run-termination (RUNNING → DEAD).
    S->TestOnly_FireRunStateChanged(ERunState::RUNNING, ERunState::DEAD, ERunOutcome::None, 1.0);

    TestTrue(TEXT("run_termination_flush_executed emitted"),
        S->TestOnly_WasEventEmitted(FName(TEXT("run_termination_flush_executed"))));
    TestEqual(TEXT("WavesTerminatedCount == 1"),
        S->TestOnly_GetLastEventPayload(FName(TEXT("run_termination_flush_executed")), FName(TEXT("WavesTerminatedCount"))),
        FString(TEXT("1")));

    return true;
}

// ===========================================================================
// TC4 — AC-WS-26b: empty_pool_at_draw fires and does not crash when Opener pool is empty
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWaveSpawnerTelemetryEmptyPoolAtDrawTest,
    "SLIPSTORM.WaveSpawner.Telemetry.EmptyPoolAtDraw.NoCrash",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FWaveSpawnerTelemetryEmptyPoolAtDrawTest::RunTest(const FString& Parameters)
{
    UWaveSpawnerSubsystem* S = NewObject<UWaveSpawnerSubsystem>();
    S->TestOnly_ResetTelemetryCapture();

    // Drive to Active with an EMPTY Opener pool (no patterns injected).
    DriveToActive(S, 1.0f);

    // Attempt admission — draw will find null/empty ActiveDrawPool and return false.
    const FDPCFrameState Frame = MakeActiveFrameState();
    S->TestOnly_TryAdmitPattern(Frame);

    // Must emit empty_pool_at_draw and not crash (AC-WS-25 no-crash assertion is implicit:
    // if we reach this line the subsystem survived the empty-pool draw attempt).
    TestTrue(TEXT("empty_pool_at_draw emitted"),
        S->TestOnly_WasEventEmitted(FName(TEXT("empty_pool_at_draw"))));

    return true;
}

// ===========================================================================
// TC5 — AC-WS-27a/b: invalid barrage pruned → critical_pool_empty_post_load → lifecycle Idle
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWaveSpawnerTelemetryInvalidPatternAtLoadTest,
    "SLIPSTORM.WaveSpawner.Telemetry.InvalidPatternAtLoad.CriticalPoolEmpty",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FWaveSpawnerTelemetryInvalidPatternAtLoadTest::RunTest(const FString& Parameters)
{
    // ValidateAndPrunePoolsAtLoad() logs UE_LOG(Error) on critical-empty.
    // AddExpectedError prevents the automation framework from treating the Error log as a test failure.
    // W-3 note (Story 009 review): verify EAutomationExpectedErrorFlags enum name against
    // Engine/Source/Runtime/Core/Public/Misc/AutomationTest.h in UE 5.7 before shipping —
    // it may have been renamed alongside the AddExpectedMessage/EAutomationExpectedMessageFlags
    // family added post-5.3. Pattern "critical pool" is intentionally a substring (not "critical
    // pool(s)") to avoid treating the trailing (s) as a regex capturing group.
    AddExpectedError(TEXT("critical pool"), EAutomationExpectedErrorFlags::Contains, 1);

    UWaveSpawnerSubsystem* S = NewObject<UWaveSpawnerSubsystem>();
    S->TestOnly_ResetTelemetryCapture();

    // Inject only an invalid barrage into PeakPool (tier < 2).
    // After pruning, PeakPool.BarragePatterns will be empty → critical_pool_empty_post_load.
    TArray<FPatternDefinition> InvalidBarrages;
    InvalidBarrages.Add(MakeInvalidPeakBarragePattern(FName(TEXT("bad_barrage_01"))));
    // Use TestOnly_InjectPeakBarragePool to populate Peak barrage sub-pool directly.
    S->TestOnly_InjectPeakBarragePool(InvalidBarrages);

    // Trigger load-time validation.
    S->TestOnly_TriggerValidateAndPrunePoolsAtLoad();

    TestTrue(TEXT("pattern_asset_invalid_at_load emitted"),
        S->TestOnly_WasEventEmitted(FName(TEXT("pattern_asset_invalid_at_load"))));
    TestTrue(TEXT("critical_pool_empty_post_load emitted"),
        S->TestOnly_WasEventEmitted(FName(TEXT("critical_pool_empty_post_load"))));
    TestEqual(TEXT("lifecycle transitioned to Idle"),
        S->TestOnly_GetLifecycleState(),
        EWaveSpawnerLifecycleState::Idle);

    return true;
}

// ===========================================================================
// TC6 — AC-WS-24: barrage_dropped_due_to_concurrency rate-limited to 1 event per Hz
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWaveSpawnerTelemetryBarrageDropRateLimitTest,
    "SLIPSTORM.WaveSpawner.Telemetry.BarrageDropRateLimit",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FWaveSpawnerTelemetryBarrageDropRateLimitTest::RunTest(const FString& Parameters)
{
    UWaveSpawnerSubsystem* S = NewObject<UWaveSpawnerSubsystem>();
    S->TestOnly_ResetTelemetryCapture();

    DriveToActive(S, 1.0f);

    // Fill 21 scheduled slots so Available = 23 - 21 = 2 (< 3, barrage cannot admit).
    S->TestOnly_SetScheduledCount(21);
    // Force bBarrageOwed so TryAdmitPattern takes the barrage path immediately.
    S->TestOnly_SetBarrageOwed(true);

    // Inject a valid non-barrage pattern into Opener so the fallback draw can succeed.
    TArray<FPatternDefinition> Patterns;
    Patterns.Add(MakeNonBarragePattern(FName(TEXT("pat_opener_fallback"))));
    S->TestOnly_InjectOpenerPool(Patterns);

    S->TestOnly_ResetTelemetryCapture();

    // Call TryAdmitPattern twice at the same simulated time (T=1.0f).
    // Rate limit is 1 Hz — only the first call should emit barrage_dropped_due_to_concurrency.
    S->TestOnly_SetCurrentTimeOverride(1.0f);
    S->TestOnly_TryAdmitPattern(MakeActiveFrameState());

    // Reset scheduled/barrage state for second call (simulate next DPC frame, same time).
    S->TestOnly_SetScheduledCount(21);
    S->TestOnly_SetBarrageOwed(true);
    S->TestOnly_TryAdmitPattern(MakeActiveFrameState());

    TestEqual(TEXT("barrage_dropped_due_to_concurrency emitted exactly once (rate-limited)"),
        S->TestOnly_CountEvents(FName(TEXT("barrage_dropped_due_to_concurrency"))),
        1);

    return true;
}

// ===========================================================================
// TC7 — AC-WS-27a: valid pool with 7 triplet barrage patterns produces no error events
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FWaveSpawnerTelemetryValidPoolNoErrorsTest,
    "SLIPSTORM.WaveSpawner.Telemetry.ValidPoolNoErrors",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FWaveSpawnerTelemetryValidPoolNoErrorsTest::RunTest(const FString& Parameters)
{
    UWaveSpawnerSubsystem* S = NewObject<UWaveSpawnerSubsystem>();
    S->TestOnly_ResetTelemetryCapture();

    // Build the 7 valid PEAK barrage triplets. Each must have LeanMagnitudeTier >= 2.
    // ValidPeakTriplets: {0,1,3},{0,1,4},{0,2,3},{0,2,4},{0,3,4},{1,2,4},{1,3,4}
    TArray<FPatternDefinition> PeakBarrages;
    const TArray<TArray<int32>>& Triplets = FWaveSpawnerCookTimeValidator::ValidPeakTriplets;
    for (int32 i = 0; i < Triplets.Num(); ++i)
    {
        FPatternDefinition P;
        P.PatternId         = FName(*FString::Printf(TEXT("peak_barrage_%02d"), i));
        P.bIsBarrage        = true;
        P.LeanMagnitudeTier = 2;
        P.SourceLanes       = Triplets[i];
        PeakBarrages.Add(P);
    }
    S->TestOnly_InjectPeakBarragePool(PeakBarrages);

    // Inject a valid non-barrage pattern into each pool so critical-empty is not triggered.
    TArray<FPatternDefinition> NonBarrage;
    NonBarrage.Add(MakeNonBarragePattern(FName(TEXT("opener_nb_01"))));
    S->TestOnly_InjectOpenerPool(NonBarrage);
    S->TestOnly_InjectMidPool(NonBarrage);
    S->TestOnly_InjectPeakNonBarragePool(NonBarrage);

    S->TestOnly_TriggerValidateAndPrunePoolsAtLoad();

    TestFalse(TEXT("pattern_asset_invalid_at_load NOT emitted"),
        S->TestOnly_WasEventEmitted(FName(TEXT("pattern_asset_invalid_at_load"))));
    TestFalse(TEXT("critical_pool_empty_post_load NOT emitted"),
        S->TestOnly_WasEventEmitted(FName(TEXT("critical_pool_empty_post_load"))));
    TestEqual(TEXT("lifecycle remains Cold"),
        S->TestOnly_GetLifecycleState(),
        EWaveSpawnerLifecycleState::Cold);

    return true;
}

// ---------------------------------------------------------------------------
// AC-WS-24 coverage gap note (pool_exhaustion_detected):
//
// AC-WS-24 also specifies a pool_exhaustion_detected event emitted when
// Deferred_PoolExhaustion is returned from TryAdmitPattern. That result code
// is only reachable via AcquireFromPool() returning nullptr — a path that
// requires a live UWorld and populated actor pool (23 AWave actors from
// OnFirstWorldLoaded). This path is not exercisable with NewObject<> headless
// testing; it is covered by playtest (QA evidence in production/qa/evidence/)
// and the Rule 12 despawn pipeline integration tests.
// ---------------------------------------------------------------------------

#endif // WITH_DEV_AUTOMATION_TESTS

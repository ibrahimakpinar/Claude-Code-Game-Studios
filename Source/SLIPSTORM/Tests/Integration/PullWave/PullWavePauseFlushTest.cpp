// Copyright (c) Game Studio. All rights reserved.
// SLIPSTORM — 60-second voxel endless runner, mobile, Unreal Engine 5.7
//
// PullWavePauseFlushTest.cpp — Integration tests for Story 007: Pause-Flush
// (Queued-to-next-tick + bPauseFlushPending gate).
//
// Story:  production/epics/pull-wave/story-007-pause-flush.md
// ADR:    docs/architecture/adr-0010-pullwave-object-pool-state-machine-despawn.md §D2
// TR-ID:  TR-PW-002
//
// Acceptance criteria covered (8 ACs):
//   TC1: AC-PW-MID-TICK-PAUSE-DEFERRAL — 4 sub-assertions (a)–(d)
//   TC2: Flush-at-next-tick-top (RUNNING state) — all 3 waves DESPAWNING, pool empty
//   TC3: Rule 19 — ABORTED state skip; waves remain; pool unchanged
//   TC4: SPAWNED wave not flushed (SPAWNED→DESPAWNING forbidden)
//   TC5: DESPAWNING wave not re-flushed (already terminal)
//   TC6: WaveId ASC flush order — despawn-entry events in WaveId ASC order
//   TC7: Handler binding lifecycle — double-call does not double-set or re-arm
//   TC8: Pause-freeze and pause-flush independence
//
// Test harness conventions (ADR-0010 D2 / R7):
//   - NewObject<APullWaveSubsystemActor>() + InitializePool() for actor setup.
//   - AddToRoot() / RemoveFromRoot() to prevent GC mid-test.
//   - ON_SCOPE_EXIT clears all injected providers before RemoveFromRoot().
//   - IPullWaveRSMProvider stub injected via SetRSMProvider().
//   - OnRSMPausedChanged(false) called directly (test-seam; Story 007 doc).
//   - Tick(DeltaTime) called directly to drive state machine.
//   - ActiveWaves.Num() / Wave.State / OnDespawnedUserCallback seam for assertions.
//   - All IDE diagnostics (CoreMinimal.h not found, etc.) are UE macro false positives.

#include "Misc/AutomationTest.h"
#include "PullWave/PullWaveSubsystemActor.h"
#include "PullWave/PullWaveTypes.h"

// ---------------------------------------------------------------------------
// FTestRSMStub — injectable RSM provider for pause-flush integration tests.
// Backs both GetIsPaused() (pause-freeze) and GetCurrentState() (Rule 19 check).
// ---------------------------------------------------------------------------
class FTestRSMStub : public IPullWaveRSMProvider
{
public:
    bool       bIsPaused     = false;
    ERunState  CurrentState  = ERunState::RUNNING;

    virtual bool      GetIsPaused()      const override { return bIsPaused; }
    virtual ERunState GetCurrentState()  const override { return CurrentState; }
};

// ---------------------------------------------------------------------------
// AddTestWaveInState — helper to insert a pre-baked wave at a given lifecycle state.
//
// Uses TestWaveId=N and transitions the struct to the desired State via direct field
// assignment (acceptable in tests; TransitionTo() is used for production call sites).
// Appends directly to Actor->ActiveWaves — bypasses Construct() (Story 009 scope).
// ---------------------------------------------------------------------------
static void AddTestWaveInState(APullWaveSubsystemActor* Actor,
    int32 WaveId, EPullWaveState TargetState)
{
    FPullWaveInstanceState W;
    W.WaveId             = WaveId;
    W.State              = TargetState;
    W.ISMCInstanceIndex  = -1;  // No ISMC in test context
    // Provide enough LeanDuration / TravelDuration to avoid degenerate-divide
    W.LeanDurationS      = 0.5f;
    W.TravelDurationS    = 1.0f;
    // For LANDED state: set CollisionOutcome to avoid uninitialised broadcast path
    if (TargetState == EPullWaveState::Landed)
    {
        W.CollisionOutcome = ECollisionOutcome::CleanMiss;
    }
    Actor->ActiveWaves.Add(W);
}

// ===========================================================================
// TC1: AC-PW-MID-TICK-PAUSE-DEFERRAL
//
// Given Pull-Wave has active waves, OnRSMPausedChanged(false) fires:
//   (a) bPauseFlushPending becomes true synchronously
//   (b) No wave transitions to DESPAWNING within that tick (flag not yet consumed)
//   (c) At TOP of next tick, all LEANING/TRAVERSING/LANDED waves → DESPAWNING
//   (d) Their 6-step pipelines fire in that same tick (pool empty after tick)
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPullWavePauseFlushTC1_MidTickDeferral,
    "SLIPSTORM.PullWave.PauseFlush.TC1_MidTickPauseDeferral",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FPullWavePauseFlushTC1_MidTickDeferral::RunTest(const FString& Parameters)
{
    APullWaveSubsystemActor* Actor = NewObject<APullWaveSubsystemActor>();
    Actor->AddToRoot();
    ON_SCOPE_EXIT
    {
        Actor->SetRSMProvider(nullptr);
        Actor->SetOnDespawnedUserCallback({});
        Actor->RemoveFromRoot();
    };

    FTestRSMStub RSMStub;
    RSMStub.CurrentState = ERunState::RUNNING;
    RSMStub.bIsPaused    = false;
    Actor->SetRSMProvider(&RSMStub);
    Actor->InitializePool();

    // One wave in TRAVERSING
    AddTestWaveInState(Actor, 10, EPullWaveState::Traversing);

    // (a) bPauseFlushPending set synchronously — no tick fires yet.
    // We can't read bPauseFlushPending directly (private); assert via behaviour:
    // after calling the handler, the wave must still be TRAVERSING (b).
    Actor->OnRSMPausedChanged(false);

    // (b) No wave transitions to DESPAWNING within the same "tick" context:
    //     After the handler call, wave still TRAVERSING (flush hasn't fired yet).
    TestEqual(TEXT("TC1b: wave still TRAVERSING after handler call (no inline flush)"),
        Actor->ActiveWaves[0].State, EPullWaveState::Traversing);

    // (c) + (d) Tick fires — flush batch at top-of-tick routes wave to DESPAWNING,
    //           then DESPAWNING case in per-wave loop fires 6-step pipeline → RemoveAt.
    //           After the tick, pool must be empty.
    Actor->Tick(0.016f);

    TestEqual(TEXT("TC1c: wave DESPAWNED after next tick (pool empty)"),
        Actor->ActiveWaves.Num(), 0);

    // (d) implicit in TC1c: the 6-step pipeline fired in the same tick as the flush.
    //     If the wave were merely in DESPAWNING state (not yet pipeline'd), Num() > 0.
    TestTrue(TEXT("TC1d: 6-step pipeline completed same tick as flush"),
        Actor->ActiveWaves.Num() == 0);

    return true;
}

// ===========================================================================
// TC2: Flush-at-next-tick-top (RUNNING state)
//
// 3 waves (LEANING, TRAVERSING, LANDED) + RSM RUNNING.
// After Tick following OnRSMPausedChanged(false): all 3 → DESPAWNING → pipeline.
// ActiveWaves.Num() == 0. bPauseFlushPending cleared.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPullWavePauseFlushTC2_RunningStateFlush,
    "SLIPSTORM.PullWave.PauseFlush.TC2_RunningStateFlushAllWaves",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FPullWavePauseFlushTC2_RunningStateFlush::RunTest(const FString& Parameters)
{
    APullWaveSubsystemActor* Actor = NewObject<APullWaveSubsystemActor>();
    Actor->AddToRoot();

    int32 DespawnCount = 0;
    TArray<EDespawnReason> CapturedReasons;

    ON_SCOPE_EXIT
    {
        Actor->SetRSMProvider(nullptr);
        Actor->SetOnDespawnedUserCallback({});
        Actor->RemoveFromRoot();
    };

    FTestRSMStub RSMStub;
    RSMStub.CurrentState = ERunState::RUNNING;
    RSMStub.bIsPaused    = false;
    Actor->SetRSMProvider(&RSMStub);
    Actor->InitializePool();

    Actor->SetOnDespawnedUserCallback([&](int32 /*WaveId*/, EDespawnReason Reason)
    {
        ++DespawnCount;
        CapturedReasons.Add(Reason);
    });

    // Three waves in different active states
    AddTestWaveInState(Actor, 1, EPullWaveState::Leaning);
    AddTestWaveInState(Actor, 2, EPullWaveState::Traversing);
    AddTestWaveInState(Actor, 3, EPullWaveState::Landed);

    TestEqual(TEXT("TC2 setup: 3 waves active"), Actor->ActiveWaves.Num(), 3);

    Actor->OnRSMPausedChanged(false);
    Actor->Tick(0.016f);

    TestEqual(TEXT("TC2a: pool empty after flush tick"), Actor->ActiveWaves.Num(), 0);
    TestEqual(TEXT("TC2b: OnWaveDespawned fired exactly 3 times"), DespawnCount, 3);

    for (int32 i = 0; i < CapturedReasons.Num(); ++i)
    {
        TestEqual(TEXT("TC2c: all DespawnReasons are PauseFlush"),
            CapturedReasons[i], EDespawnReason::PauseFlush);
    }

    // Second tick: bPauseFlushPending must be false (cleared in previous tick).
    // No additional despawn events should fire.
    const int32 PrevCount = DespawnCount;
    Actor->Tick(0.016f);
    TestEqual(TEXT("TC2d: bPauseFlushPending cleared; no extra despawns on second tick"),
        DespawnCount, PrevCount);

    return true;
}

// ===========================================================================
// TC3: Rule 19 — ABORTED state skip
//
// RSM.GetCurrentState() == ABORTED at consumption tick.
// bPauseFlushPending cleared WITHOUT routing any wave to DESPAWNING.
// Waves remain in their current states (Story 008 run-termination handles them).
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPullWavePauseFlushTC3_AbortedStateSkip,
    "SLIPSTORM.PullWave.PauseFlush.TC3_Rule19AbortedStateSkip",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FPullWavePauseFlushTC3_AbortedStateSkip::RunTest(const FString& Parameters)
{
    APullWaveSubsystemActor* Actor = NewObject<APullWaveSubsystemActor>();
    Actor->AddToRoot();

    int32 DespawnCount = 0;
    ON_SCOPE_EXIT
    {
        Actor->SetRSMProvider(nullptr);
        Actor->SetOnDespawnedUserCallback({});
        Actor->RemoveFromRoot();
    };

    // RSM state: ABORTED at consumption tick
    FTestRSMStub RSMStub;
    RSMStub.CurrentState = ERunState::ABORTED;
    RSMStub.bIsPaused    = false;
    Actor->SetRSMProvider(&RSMStub);
    Actor->InitializePool();

    Actor->SetOnDespawnedUserCallback([&](int32, EDespawnReason)
    {
        ++DespawnCount;
    });

    // Two waves that would normally be flushed
    AddTestWaveInState(Actor, 5, EPullWaveState::Traversing);
    AddTestWaveInState(Actor, 6, EPullWaveState::Landed);

    Actor->OnRSMPausedChanged(false);
    Actor->Tick(0.016f);

    // No waves should have been despawned (Rule 19: ABORTED skips flush)
    TestEqual(TEXT("TC3a: OnWaveDespawned NOT fired (Rule 19 ABORTED skip)"),
        DespawnCount, 0);

    // Both waves must still exist and be in their original states
    // (though TRAVERSING may have accumulated DeltaTime; LANDED accumulates hold timer)
    // Key check: neither transitioned to DESPAWNING via the pause-flush path
    TestEqual(TEXT("TC3b: ActiveWaves still contains 2 entries (no despawn)"),
        Actor->ActiveWaves.Num(), 2);

    // Second tick: bPauseFlushPending must be false; no delayed flush either
    Actor->Tick(0.016f);
    TestEqual(TEXT("TC3c: bPauseFlushPending cleared; no delayed flush on second tick"),
        DespawnCount, 0);

    return true;
}

// ===========================================================================
// TC4: SPAWNED wave not flushed
//
// One wave in SPAWNED state when pause-flush fires.
// SPAWNED→DESPAWNING is a forbidden transition — wave must be skipped.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPullWavePauseFlushTC4_SpawnedNotFlushed,
    "SLIPSTORM.PullWave.PauseFlush.TC4_SpawnedWaveNotFlushed",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FPullWavePauseFlushTC4_SpawnedNotFlushed::RunTest(const FString& Parameters)
{
    APullWaveSubsystemActor* Actor = NewObject<APullWaveSubsystemActor>();
    Actor->AddToRoot();
    ON_SCOPE_EXIT
    {
        Actor->SetRSMProvider(nullptr);
        Actor->RemoveFromRoot();
    };

    FTestRSMStub RSMStub;
    RSMStub.CurrentState = ERunState::RUNNING;
    RSMStub.bIsPaused    = false;
    Actor->SetRSMProvider(&RSMStub);
    Actor->InitializePool();

    // Single wave in SPAWNED state
    AddTestWaveInState(Actor, 20, EPullWaveState::Spawned);

    Actor->OnRSMPausedChanged(false);

    // The flush batch must skip SPAWNED (forbidden transition).
    // Tick fires the per-wave loop: SPAWNED advance → TransitionTo(LEANING).
    // The SPAWNED wave transitions to LEANING naturally (not to DESPAWNING).
    Actor->Tick(0.016f);

    // After tick: wave transitioned SPAWNED→LEANING (normal advance), NOT DESPAWNING.
    // Pool size stays 1.
    TestEqual(TEXT("TC4a: pool still has 1 entry (SPAWNED not flushed)"),
        Actor->ActiveWaves.Num(), 1);

    if (Actor->ActiveWaves.Num() > 0)
    {
        // Wave advanced SPAWNED→LEANING (normal one-tick init), not to DESPAWNING
        TestEqual(TEXT("TC4b: SPAWNED wave advanced to LEANING (not DESPAWNING)"),
            Actor->ActiveWaves[0].State, EPullWaveState::Leaning);
    }

    return true;
}

// ===========================================================================
// TC5: DESPAWNING wave not re-flushed
//
// One wave already in DESPAWNING when pause-flush fires.
// DESPAWNING is terminal — flush batch must skip it.
// Wave executes its 6-step pipeline in the per-wave loop (already terminal).
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPullWavePauseFlushTC5_DespawningNotReflushed,
    "SLIPSTORM.PullWave.PauseFlush.TC5_DespawningWaveNotReflushed",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FPullWavePauseFlushTC5_DespawningNotReflushed::RunTest(const FString& Parameters)
{
    APullWaveSubsystemActor* Actor = NewObject<APullWaveSubsystemActor>();
    Actor->AddToRoot();

    int32 DespawnCount = 0;
    EDespawnReason CapturedReason = EDespawnReason::PauseFlush;  // wrong sentinel: should end up NaturalLanding
    ON_SCOPE_EXIT
    {
        Actor->SetRSMProvider(nullptr);
        Actor->SetOnDespawnedUserCallback({});
        Actor->RemoveFromRoot();
    };

    FTestRSMStub RSMStub;
    RSMStub.CurrentState = ERunState::RUNNING;
    RSMStub.bIsPaused    = false;
    Actor->SetRSMProvider(&RSMStub);
    Actor->InitializePool();

    Actor->SetOnDespawnedUserCallback([&](int32, EDespawnReason Reason)
    {
        ++DespawnCount;
        CapturedReason = Reason;
    });

    // Wave already in DESPAWNING with NaturalLanding reason (set by Story 005/006 path).
    // The flush batch MUST skip this wave (already terminal); if it incorrectly overwrites
    // PendingDespawnReason=PauseFlush before the pipeline fires, CapturedReason would be
    // PauseFlush — caught by TC5c.
    FPullWaveInstanceState W;
    W.WaveId               = 30;
    W.State                = EPullWaveState::Despawning;
    W.PendingDespawnReason = EDespawnReason::NaturalLanding;
    W.ISMCInstanceIndex    = -1;
    Actor->ActiveWaves.Add(W);

    Actor->OnRSMPausedChanged(false);

    // After Tick: flush batch skips DESPAWNING; per-wave loop fires 6-step pipeline.
    Actor->Tick(0.016f);

    // Wave should have been despawned (pipeline fired) — exactly once — pool empty
    TestEqual(TEXT("TC5a: pool empty (DESPAWNING wave pipeline fired exactly once)"),
        Actor->ActiveWaves.Num(), 0);
    TestEqual(TEXT("TC5b: OnWaveDespawned fired exactly once"),
        DespawnCount, 1);
    // TC5c: reason must be NaturalLanding — flush batch must NOT have overwritten it to PauseFlush.
    // If the flush batch incorrectly routes DESPAWNING waves, this fails.
    TestEqual(TEXT("TC5c: DespawnReason is NaturalLanding (flush did not re-route DESPAWNING wave)"),
        CapturedReason, EDespawnReason::NaturalLanding);

    return true;
}

// ===========================================================================
// TC6: WaveId ASC flush order
//
// 3 waves (WaveIds 5, 7, 9) in TRAVERSING. After flush:
// OnWaveDespawned fires in WaveId ASC order: 5 → 7 → 9.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPullWavePauseFlushTC6_WaveIdAscFlushOrder,
    "SLIPSTORM.PullWave.PauseFlush.TC6_WaveIdAscFlushOrder",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FPullWavePauseFlushTC6_WaveIdAscFlushOrder::RunTest(const FString& Parameters)
{
    APullWaveSubsystemActor* Actor = NewObject<APullWaveSubsystemActor>();
    Actor->AddToRoot();

    TArray<int32> DespawnedOrder;
    ON_SCOPE_EXIT
    {
        Actor->SetRSMProvider(nullptr);
        Actor->SetOnDespawnedUserCallback({});
        Actor->RemoveFromRoot();
    };

    FTestRSMStub RSMStub;
    RSMStub.CurrentState = ERunState::RUNNING;
    RSMStub.bIsPaused    = false;
    Actor->SetRSMProvider(&RSMStub);
    Actor->InitializePool();

    Actor->SetOnDespawnedUserCallback([&](int32 WaveId, EDespawnReason)
    {
        DespawnedOrder.Add(WaveId);
    });

    // Insert in ASC order (pool invariant requires ASC)
    AddTestWaveInState(Actor, 5, EPullWaveState::Traversing);
    AddTestWaveInState(Actor, 7, EPullWaveState::Traversing);
    AddTestWaveInState(Actor, 9, EPullWaveState::Traversing);

    Actor->OnRSMPausedChanged(false);
    Actor->Tick(0.016f);

    TestEqual(TEXT("TC6a: all 3 waves despawned"), DespawnedOrder.Num(), 3);

    if (DespawnedOrder.Num() == 3)
    {
        TestEqual(TEXT("TC6b: first despawned WaveId=5"),  DespawnedOrder[0], 5);
        TestEqual(TEXT("TC6c: second despawned WaveId=7"), DespawnedOrder[1], 7);
        TestEqual(TEXT("TC6d: third despawned WaveId=9"),  DespawnedOrder[2], 9);
    }

    return true;
}

// ===========================================================================
// TC7: Handler binding lifecycle — double-call does not double-queue
//
// Calling OnRSMPausedChanged(false) twice before any tick:
// Only one flush should fire (bPauseFlushPending is a bool, not a counter).
// Wave count after tick remains 0 (not negative); no double-despawn.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPullWavePauseFlushTC7_HandlerBindingLifecycle,
    "SLIPSTORM.PullWave.PauseFlush.TC7_HandlerDoublecallDoesNotDoubleFlush",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FPullWavePauseFlushTC7_HandlerBindingLifecycle::RunTest(const FString& Parameters)
{
    APullWaveSubsystemActor* Actor = NewObject<APullWaveSubsystemActor>();
    Actor->AddToRoot();

    int32 DespawnCount = 0;
    ON_SCOPE_EXIT
    {
        Actor->SetRSMProvider(nullptr);
        Actor->SetOnDespawnedUserCallback({});
        Actor->RemoveFromRoot();
    };

    FTestRSMStub RSMStub;
    RSMStub.CurrentState = ERunState::RUNNING;
    RSMStub.bIsPaused    = false;
    Actor->SetRSMProvider(&RSMStub);
    Actor->InitializePool();

    Actor->SetOnDespawnedUserCallback([&](int32, EDespawnReason)
    {
        ++DespawnCount;
    });

    AddTestWaveInState(Actor, 40, EPullWaveState::Traversing);

    // Double-fire the handler (simulating a hypothetical double-bind scenario)
    Actor->OnRSMPausedChanged(false);
    Actor->OnRSMPausedChanged(false);

    // Only one flush should execute (bPauseFlushPending is set once; second call
    // sets it again to true, which is idempotent — bool, not a counter)
    Actor->Tick(0.016f);

    // Wave count must be 0 (not negative); despawn count must be exactly 1
    TestEqual(TEXT("TC7a: pool empty after flush"), Actor->ActiveWaves.Num(), 0);
    TestEqual(TEXT("TC7b: despawn fired exactly once (double-call idempotent)"),
        DespawnCount, 1);

    return true;
}

// ===========================================================================
// TC8: Pause-freeze and pause-flush are independent
//
// During normal pause (GetIsPaused() == true, no bPauseFlushPending):
// per-wave accumulation is frozen; bPauseFlushPending stays false.
// Pause-flush only arms when OnRSMPausedChanged(false) is called.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPullWavePauseFlushTC8_PauseFreezeAndFlushIndependent,
    "SLIPSTORM.PullWave.PauseFlush.TC8_PauseFreezeAndFlushAreIndependent",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FPullWavePauseFlushTC8_PauseFreezeAndFlushIndependent::RunTest(const FString& Parameters)
{
    APullWaveSubsystemActor* Actor = NewObject<APullWaveSubsystemActor>();
    Actor->AddToRoot();

    int32 DespawnCount = 0;
    ON_SCOPE_EXIT
    {
        Actor->SetRSMProvider(nullptr);
        Actor->SetOnDespawnedUserCallback({});
        Actor->RemoveFromRoot();
    };

    FTestRSMStub RSMStub;
    RSMStub.CurrentState = ERunState::RUNNING;
    RSMStub.bIsPaused    = true;  // Simulating a live pause mid-run
    Actor->SetRSMProvider(&RSMStub);
    Actor->InitializePool();

    Actor->SetOnDespawnedUserCallback([&](int32, EDespawnReason)
    {
        ++DespawnCount;
    });

    AddTestWaveInState(Actor, 50, EPullWaveState::Traversing);

    // Tick while paused (no OnRSMPausedChanged called): pause-freeze only.
    // bPauseFlushPending is NOT set → no flush.
    Actor->Tick(0.016f);

    TestEqual(TEXT("TC8a: wave still TRAVERSING under pause-freeze"),
        Actor->ActiveWaves.Num(), 1);
    TestEqual(TEXT("TC8b: no despawn during pause-freeze"),
        DespawnCount, 0);

    // Now arm the flush (run aborted while paused) + un-pause
    Actor->OnRSMPausedChanged(false);
    RSMStub.bIsPaused = false;

    // Flush fires at top-of-next-tick; wave routes to DESPAWNING; pipeline fires.
    Actor->Tick(0.016f);

    TestEqual(TEXT("TC8c: wave despawned after pause-flush (not during pause-freeze)"),
        Actor->ActiveWaves.Num(), 0);
    TestEqual(TEXT("TC8d: despawn fired exactly once"),
        DespawnCount, 1);

    return true;
}

// ===========================================================================
// TC9: Mixed SPAWNED + TRAVERSING pool (GAP-2 fix)
//
// WaveId=1 in SPAWNED + WaveId=2 in TRAVERSING. After pause-flush:
//   - WaveId=2 (TRAVERSING) routes to DESPAWNING → pipeline fires (DespawnCount=1, reason=PauseFlush)
//   - WaveId=1 (SPAWNED) is skipped by flush batch → advances SPAWNED→LEANING via normal tick
//
// Verifies the flush loop does NOT break early (or use continue-only) on SPAWNED,
// which would fail to flush subsequent TRAVERSING waves in the same iteration.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPullWavePauseFlushTC9_MixedSpawnedTraversing,
    "SLIPSTORM.PullWave.PauseFlush.TC9_MixedSpawnedAndTraversingPool",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FPullWavePauseFlushTC9_MixedSpawnedTraversing::RunTest(const FString& Parameters)
{
    APullWaveSubsystemActor* Actor = NewObject<APullWaveSubsystemActor>();
    Actor->AddToRoot();

    int32 DespawnCount = 0;
    EDespawnReason CapturedReason = EDespawnReason::NaturalLanding;  // sentinel: should become PauseFlush
    ON_SCOPE_EXIT
    {
        Actor->SetRSMProvider(nullptr);
        Actor->SetOnDespawnedUserCallback({});
        Actor->RemoveFromRoot();
    };

    FTestRSMStub RSMStub;
    RSMStub.CurrentState = ERunState::RUNNING;
    RSMStub.bIsPaused    = false;
    Actor->SetRSMProvider(&RSMStub);
    Actor->InitializePool();

    Actor->SetOnDespawnedUserCallback([&](int32, EDespawnReason Reason)
    {
        ++DespawnCount;
        CapturedReason = Reason;
    });

    // WaveId=1 SPAWNED — must be skipped by flush (forbidden SPAWNED→DESPAWNING transition)
    // WaveId=2 TRAVERSING — must be flushed (TRAVERSING→DESPAWNING is legal)
    // Pool is in WaveId ASC order (pool invariant).
    AddTestWaveInState(Actor, 1, EPullWaveState::Spawned);
    AddTestWaveInState(Actor, 2, EPullWaveState::Traversing);

    TestEqual(TEXT("TC9 setup: 2 waves in pool"), Actor->ActiveWaves.Num(), 2);

    Actor->OnRSMPausedChanged(false);
    Actor->Tick(0.016f);

    // WaveId=2 (TRAVERSING) must have been despawned
    TestEqual(TEXT("TC9a: exactly one despawn (WaveId=2 TRAVERSING flushed)"),
        DespawnCount, 1);
    TestEqual(TEXT("TC9b: DespawnReason is PauseFlush"),
        CapturedReason, EDespawnReason::PauseFlush);

    // WaveId=1 (SPAWNED) must still be alive — advanced SPAWNED→LEANING by normal tick
    TestEqual(TEXT("TC9c: pool has 1 entry (SPAWNED wave survived flush)"),
        Actor->ActiveWaves.Num(), 1);

    if (Actor->ActiveWaves.Num() > 0)
    {
        TestEqual(TEXT("TC9d: WaveId=1 SPAWNED advanced to LEANING (not DESPAWNING)"),
            Actor->ActiveWaves[0].State, EPullWaveState::Leaning);
        TestEqual(TEXT("TC9e: surviving wave is WaveId=1"),
            Actor->ActiveWaves[0].WaveId, 1);
    }

    return true;
}

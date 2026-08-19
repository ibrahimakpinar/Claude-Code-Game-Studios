// Copyright (c) Game Studio. All rights reserved.
// SLIPSTORM — 60-second voxel endless runner, mobile, Unreal Engine 5.7
//
// PullWaveRunTerminationDrainTest.cpp — Integration tests for Story 008:
// Run-Termination Drain Semantics.
//
// Story:  production/epics/pull-wave/story-008-run-termination-drain.md
// ADR:    docs/architecture/adr-0010-pullwave-object-pool-state-machine-despawn.md §D2
// TR-ID:  TR-PW-002
//
// Acceptance criteria covered (8 ACs, 9 TCs):
//   TC1: Drain continues normally from LEANING → full lifecycle → DespawnReason=RunTermination
//   TC2: Drain continues normally from TRAVERSING → LANDED → DespawnReason=RunTermination
//   TC3: Drain continues from LANDED hold → DespawnReason=RunTermination; hits do not re-fire
//   TC4: Hit fires during drain (OnWaveHit fires at LANDED entry; DespawnReason=RunTermination)
//   TC5: Distinct from PauseFlush (ABORTED + OnPausedChanged: Rule 19 skips flush; drain proceeds)
//   TC6: DespawnReason=RunTermination for all post-termination despawns (explicit reason check)
//   TC7: Pause-freeze still applies during drain (bIsPaused=true suppresses tick bodies)
//   TC8: Pool drains to 0 monotonically (no new admissions; Num() decreases to 0)
//   TC9: Negative control — DespawnReason=NaturalLanding when bRunTerminated==false (no signal fired)
//
// Test harness conventions:
//   - NewObject<APullWaveSubsystemActor>() + InitializePool() for actor setup.
//   - AddToRoot() / RemoveFromRoot() guard all tests.
//   - ON_SCOPE_EXIT clears all injected providers before RemoveFromRoot().
//   - FTestRSMStub8 injects RSM state via SetRSMProvider().
//   - Actor->OnRSMRunStateChanged(ERunState::DEAD) called directly as test seam.
//   - Actor->OnRSMPausedChanged(false) called directly for TC5 interaction test.
//   - Multi-tick drain loops use DeltaTime=0.1f with max-iteration guard (100 ticks).
//   - Wave setup: LeanDurationS=0.1f, ForwardVelocityMs=30.0f → TravelDurationS=0.5s.
//     At DeltaTime=0.1f: LEANING ~1 tick, TRAVERSING ~5 ticks, LANDED ~2 ticks.
//   - All IDE diagnostics (CoreMinimal.h not found, etc.) are UE macro false positives.

#include "Misc/AutomationTest.h"
#include "PullWave/PullWaveSubsystemActor.h"
#include "PullWave/PullWaveTypes.h"
#include "Seam/PlayerMovementProvider.h"

// ---------------------------------------------------------------------------
// FTestRSMStub8 — RSM provider stub for run-termination drain tests.
// Backs GetIsPaused() (pause-freeze) and GetCurrentState() (Rule 19 / flush batch).
// ---------------------------------------------------------------------------
class FTestRSMStub8 : public IPullWaveRSMProvider
{
public:
    bool      bIsPaused    = false;
    ERunState CurrentState = ERunState::RUNNING;

    virtual bool      GetIsPaused()     const override { return bIsPaused; }
    virtual ERunState GetCurrentState() const override { return CurrentState; }
};

// ---------------------------------------------------------------------------
// FTestPMStub8 — minimal PM provider stub for hit-detection tests (TC4).
// ---------------------------------------------------------------------------
class FTestPMStub8 : public IPlayerMovementProvider
{
public:
    int32          CurrentLane  = 2;
    int32          TargetLane_  = 2;
    EMovementState MovState     = EMovementState::SETTLED;

    virtual int32          GetCurrentLane()    const override { return CurrentLane; }
    virtual int32          GetTargetLane()     const override { return TargetLane_; }
    virtual EMovementState GetMovementState()  const override { return MovState; }
    virtual void           TriggerNearMissBeat() override {}
};

// ---------------------------------------------------------------------------
// AddDrainWave — add a wave preset for drain tests.
// LeanDurationS=0.1f so LEANING completes in 1 tick at DeltaTime=0.1f.
// ForwardVelocityMs=30.0f → TravelDurationS=0.5s (5 ticks at 0.1f DeltaTime).
// ISMCInstanceIndex=-1 (no ISMC in test context).
// ---------------------------------------------------------------------------
static void AddDrainWave(APullWaveSubsystemActor* Actor,
    int32 WaveId, EPullWaveState StartState, int32 TargetLane = 2)
{
    FPullWaveInstanceState W;
    W.WaveId            = WaveId;
    W.State             = StartState;
    W.ISMCInstanceIndex = -1;
    W.LeanDurationS     = 0.1f;       // Short lean → 1 tick at DeltaTime=0.1f
    W.ForwardVelocityMs = 30.0f;      // TravelDurationS = 15/30 = 0.5s
    W.TravelDurationS   = SPAWN_PLANE_Z_OFFSET_M / W.ForwardVelocityMs;  // 0.5f
    W.TargetLane        = TargetLane;
    W.SourceLane        = TargetLane;  // Same lane — trajectory goes straight
    if (StartState == EPullWaveState::Landed)
    {
        W.CollisionOutcome = ECollisionOutcome::CleanMiss;
        W.TraverseElapsedS = W.TravelDurationS;  // Already at t_norm=1 (player plane)
    }
    if (StartState == EPullWaveState::Traversing)
    {
        // Start at t_norm=0.3 (30% through traversal)
        W.TraverseElapsedS = W.TravelDurationS * 0.3f;
    }
    Actor->ActiveWaves.Add(W);
}

// ---------------------------------------------------------------------------
// DrainUntilEmpty — tick Actor until pool is empty or MaxTicks reached.
// Returns true if pool drained completely within MaxTicks.
// ---------------------------------------------------------------------------
static bool DrainUntilEmpty(APullWaveSubsystemActor* Actor,
    float DeltaTime = 0.1f, int32 MaxTicks = 100)
{
    for (int32 i = 0; i < MaxTicks; ++i)
    {
        if (Actor->ActiveWaves.Num() == 0) { return true; }
        Actor->Tick(DeltaTime);
    }
    return Actor->ActiveWaves.Num() == 0;
}

// ===========================================================================
// TC1: Drain continues normally from LEANING
//
// Given wave in LEANING when OnRSMRunStateChanged(DEAD) fires:
//   - Wave does NOT immediately transition to DESPAWNING
//   - OnLeanProgress continues firing per tick
//   - Wave advances LEANING→TRAVERSING→LANDED→DESPAWNING naturally
//   - DespawnReason=RunTermination
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPullWaveRunTermDrainTC1_LeaningDrain,
    "SLIPSTORM.PullWave.RunTerminationDrain.TC1_DrainFromLeaning",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FPullWaveRunTermDrainTC1_LeaningDrain::RunTest(const FString& Parameters)
{
    APullWaveSubsystemActor* Actor = NewObject<APullWaveSubsystemActor>();
    Actor->AddToRoot();

    int32 DespawnCount = 0;
    EDespawnReason CapturedReason = EDespawnReason::NaturalLanding;  // sentinel
    int32 LeanProgressFireCount   = 0;
    ON_SCOPE_EXIT
    {
        Actor->SetRSMProvider(nullptr);
        Actor->SetOnDespawnedUserCallback({});
        Actor->OnLeanProgress.Clear();
        Actor->RemoveFromRoot();
    };

    FTestRSMStub8 RSMStub;
    RSMStub.CurrentState = ERunState::RUNNING;
    RSMStub.bIsPaused    = false;
    Actor->SetRSMProvider(&RSMStub);
    Actor->InitializePool();

    Actor->SetOnDespawnedUserCallback([&](int32, EDespawnReason Reason)
    {
        ++DespawnCount;
        CapturedReason = Reason;
    });
    Actor->OnLeanProgress.AddLambda([&](const FLeanTickData&) { ++LeanProgressFireCount; });

    // Wave starts in LEANING
    AddDrainWave(Actor, 1, EPullWaveState::Leaning);

    // Signal run termination — must NOT immediately route wave to DESPAWNING
    Actor->OnRSMRunStateChanged(ERunState::DEAD);
    RSMStub.CurrentState = ERunState::DEAD;

    // TC1a: Wave must still be in LEANING right after handler call (no inline flush)
    TestEqual(TEXT("TC1a: wave still LEANING after run-termination signal (no immediate flush)"),
        Actor->ActiveWaves[0].State, EPullWaveState::Leaning);

    // TC1b: Tick once — wave advances LEANING (not DESPAWNING)
    Actor->Tick(0.1f);
    // After 1 tick at DeltaTime=0.1f with LeanDurationS=0.1f: LeanProgress≥1 → TRAVERSING
    // OnLeanProgress fired at least once
    TestTrue(TEXT("TC1b: OnLeanProgress fired during drain tick"),
        LeanProgressFireCount > 0);

    // TC1c: Drain to empty — wave must complete full lifecycle naturally
    const bool bDrained = DrainUntilEmpty(Actor);
    TestTrue(TEXT("TC1c: wave fully drained to pool-empty (no stall)"), bDrained);
    TestEqual(TEXT("TC1d: OnWaveDespawned fired exactly once"), DespawnCount, 1);
    TestEqual(TEXT("TC1e: DespawnReason is RunTermination (not NaturalLanding)"),
        CapturedReason, EDespawnReason::RunTermination);

    return true;
}

// ===========================================================================
// TC2: Drain continues normally from TRAVERSING
//
// Given wave in TRAVERSING at t_norm≈0.3 when run-termination fires:
//   - TraverseElapsedS continues accumulating per tick
//   - Wave eventually crosses player plane → LANDED → DESPAWNING
//   - DespawnReason=RunTermination
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPullWaveRunTermDrainTC2_TraversingDrain,
    "SLIPSTORM.PullWave.RunTerminationDrain.TC2_DrainFromTraversing",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FPullWaveRunTermDrainTC2_TraversingDrain::RunTest(const FString& Parameters)
{
    APullWaveSubsystemActor* Actor = NewObject<APullWaveSubsystemActor>();
    Actor->AddToRoot();

    int32 DespawnCount = 0;
    EDespawnReason CapturedReason = EDespawnReason::NaturalLanding;
    ON_SCOPE_EXIT
    {
        Actor->SetRSMProvider(nullptr);
        Actor->SetOnDespawnedUserCallback({});
        Actor->RemoveFromRoot();
    };

    FTestRSMStub8 RSMStub;
    RSMStub.CurrentState = ERunState::RUNNING;
    RSMStub.bIsPaused    = false;
    Actor->SetRSMProvider(&RSMStub);
    Actor->InitializePool();

    Actor->SetOnDespawnedUserCallback([&](int32, EDespawnReason Reason)
    {
        ++DespawnCount;
        CapturedReason = Reason;
    });

    // Wave starts at TRAVERSING t_norm=0.3
    AddDrainWave(Actor, 2, EPullWaveState::Traversing);

    Actor->OnRSMRunStateChanged(ERunState::DEAD);
    RSMStub.CurrentState = ERunState::DEAD;

    // TC2a: Wave still TRAVERSING right after signal
    TestEqual(TEXT("TC2a: wave still TRAVERSING after run-termination (no flush)"),
        Actor->ActiveWaves[0].State, EPullWaveState::Traversing);

    // TC2b: Drain naturally — wave crosses player plane, lands, despawns
    const bool bDrained = DrainUntilEmpty(Actor);
    TestTrue(TEXT("TC2b: wave drained to pool-empty"), bDrained);
    TestEqual(TEXT("TC2c: OnWaveDespawned fired once"), DespawnCount, 1);
    TestEqual(TEXT("TC2d: DespawnReason=RunTermination"),
        CapturedReason, EDespawnReason::RunTermination);

    return true;
}

// ===========================================================================
// TC3: Drain continues from LANDED hold; hits do NOT re-fire
//
// Given wave in LANDED hold when run-termination fires:
//   - LandedHoldElapsedS continues accumulating
//   - Hold expires → DESPAWNING with DespawnReason=RunTermination
//   - OnWaveHit/OnNearMiss do NOT re-fire (already resolved at LANDED entry)
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPullWaveRunTermDrainTC3_LandedHoldDrain,
    "SLIPSTORM.PullWave.RunTerminationDrain.TC3_DrainFromLandedHold",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FPullWaveRunTermDrainTC3_LandedHoldDrain::RunTest(const FString& Parameters)
{
    APullWaveSubsystemActor* Actor = NewObject<APullWaveSubsystemActor>();
    Actor->AddToRoot();

    int32 DespawnCount = 0;
    EDespawnReason CapturedReason = EDespawnReason::NaturalLanding;
    int32 HitFireCount = 0;
    ON_SCOPE_EXIT
    {
        Actor->SetRSMProvider(nullptr);
        Actor->SetOnDespawnedUserCallback({});
        Actor->OnWaveHit.Clear();
        Actor->RemoveFromRoot();
    };

    FTestRSMStub8 RSMStub;
    RSMStub.CurrentState = ERunState::RUNNING;
    RSMStub.bIsPaused    = false;
    Actor->SetRSMProvider(&RSMStub);
    Actor->InitializePool();

    Actor->SetOnDespawnedUserCallback([&](int32, EDespawnReason Reason)
    {
        ++DespawnCount;
        CapturedReason = Reason;
    });
    Actor->OnWaveHit.AddLambda([&](int32, int32, int32, float) { ++HitFireCount; });

    // Wave already in LANDED hold (CollisionOutcome=CleanMiss pre-set)
    AddDrainWave(Actor, 3, EPullWaveState::Landed);

    Actor->OnRSMRunStateChanged(ERunState::DEAD);
    RSMStub.CurrentState = ERunState::DEAD;

    // TC3a: Wave still LANDED (hold not yet expired in this tick)
    TestEqual(TEXT("TC3a: wave still LANDED after termination signal"),
        Actor->ActiveWaves[0].State, EPullWaveState::Landed);

    // TC3b: Tick until despawn — hold expires → DESPAWNING → pipeline
    // WAVE_DESPAWN_HOLD_S=0.15f; 2 ticks at DeltaTime=0.1f covers it.
    const bool bDrained = DrainUntilEmpty(Actor);
    TestTrue(TEXT("TC3b: wave drained (LANDED hold expired → DESPAWNING)"), bDrained);
    TestEqual(TEXT("TC3c: DespawnReason=RunTermination"), CapturedReason, EDespawnReason::RunTermination);

    // TC3d: OnWaveHit must NOT have re-fired during the LANDED hold ticks
    TestEqual(TEXT("TC3d: OnWaveHit did not re-fire during hold drain (already resolved)"),
        HitFireCount, 0);

    return true;
}

// ===========================================================================
// TC4: Hit fires during drain
//
// Wave in TRAVERSING when run-termination fires. PMProvider = SETTLED + matching lane.
// OnWaveHit must fire at LANDED entry. DespawnReason=RunTermination at DESPAWNING.
// Run-termination does NOT suppress collision events.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPullWaveRunTermDrainTC4_HitFiresDuringDrain,
    "SLIPSTORM.PullWave.RunTerminationDrain.TC4_HitEventFiresDuringDrain",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FPullWaveRunTermDrainTC4_HitFiresDuringDrain::RunTest(const FString& Parameters)
{
    APullWaveSubsystemActor* Actor = NewObject<APullWaveSubsystemActor>();
    Actor->AddToRoot();

    int32 DespawnCount = 0;
    EDespawnReason CapturedReason = EDespawnReason::NaturalLanding;
    int32 HitCount = 0;
    ON_SCOPE_EXIT
    {
        Actor->SetRSMProvider(nullptr);
        Actor->SetPMProvider(nullptr);
        Actor->SetOnDespawnedUserCallback({});
        Actor->OnWaveHit.Clear();
        Actor->RemoveFromRoot();
    };

    FTestRSMStub8 RSMStub;
    RSMStub.CurrentState = ERunState::RUNNING;
    RSMStub.bIsPaused    = false;
    Actor->SetRSMProvider(&RSMStub);

    // PM: player SETTLED in lane 2 — will produce a Hit when wave lands in lane 2
    FTestPMStub8 PMStub;
    PMStub.CurrentLane = 2;
    PMStub.TargetLane_ = 2;
    PMStub.MovState    = EMovementState::SETTLED;
    Actor->SetPMProvider(&PMStub);

    Actor->InitializePool();

    Actor->SetOnDespawnedUserCallback([&](int32, EDespawnReason Reason)
    {
        ++DespawnCount;
        CapturedReason = Reason;
    });
    Actor->OnWaveHit.AddLambda([&](int32, int32, int32, float) { ++HitCount; });

    // Wave in TRAVERSING, targeting lane 2 (matches PM current lane)
    AddDrainWave(Actor, 4, EPullWaveState::Traversing, /*TargetLane=*/2);

    Actor->OnRSMRunStateChanged(ERunState::DEAD);
    RSMStub.CurrentState = ERunState::DEAD;

    // Drain until pool is empty — wave must land and fire OnWaveHit
    const bool bDrained = DrainUntilEmpty(Actor);
    TestTrue(TEXT("TC4a: wave drained"), bDrained);

    // TC4b: OnWaveHit must have fired exactly once at LANDED entry
    TestEqual(TEXT("TC4b: OnWaveHit fired once during drain"),
        HitCount, 1);

    // TC4c: DespawnReason must be RunTermination (not NaturalLanding)
    TestEqual(TEXT("TC4c: DespawnReason=RunTermination even after hit"),
        CapturedReason, EDespawnReason::RunTermination);

    return true;
}

// ===========================================================================
// TC5: Distinct from PauseFlush (ABORTED + OnPausedChanged interaction)
//
// Scenario: RSM.state=ABORTED + OnPausedChanged(false) fires.
//   - Rule 19 (Story 007) skips the flush (state is not RUNNING)
//   - OnRSMRunStateChanged(ABORTED) sets bRunTerminated=true (drain semantics)
//   - No immediate flush; wave keeps draining
//   - Wave eventually despawns with DespawnReason=RunTermination (not PauseFlush)
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPullWaveRunTermDrainTC5_DistinctFromPauseFlush,
    "SLIPSTORM.PullWave.RunTerminationDrain.TC5_DistinctFromPauseFlushAbortedBranch",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FPullWaveRunTermDrainTC5_DistinctFromPauseFlush::RunTest(const FString& Parameters)
{
    APullWaveSubsystemActor* Actor = NewObject<APullWaveSubsystemActor>();
    Actor->AddToRoot();

    int32 DespawnCount = 0;
    EDespawnReason CapturedReason = EDespawnReason::PauseFlush;  // wrong sentinel; should not be PauseFlush
    ON_SCOPE_EXIT
    {
        Actor->SetRSMProvider(nullptr);
        Actor->SetOnDespawnedUserCallback({});
        Actor->RemoveFromRoot();
    };

    FTestRSMStub8 RSMStub;
    RSMStub.CurrentState = ERunState::ABORTED;  // Already ABORTED at handler-call time
    RSMStub.bIsPaused    = false;
    Actor->SetRSMProvider(&RSMStub);
    Actor->InitializePool();

    Actor->SetOnDespawnedUserCallback([&](int32, EDespawnReason Reason)
    {
        ++DespawnCount;
        CapturedReason = Reason;
    });

    AddDrainWave(Actor, 5, EPullWaveState::Traversing);

    // Fire both: run-termination handler (sets bRunTerminated=true) and
    // pause-changed handler with ABORTED state (Rule 19 skips flush)
    Actor->OnRSMRunStateChanged(ERunState::ABORTED);
    Actor->OnRSMPausedChanged(false);  // bPauseFlushPending = true

    // Tick once: flush batch checks GetCurrentState()==RUNNING → false (ABORTED) →
    // bPauseFlushPending cleared without flushing. Wave still TRAVERSING.
    Actor->Tick(0.1f);

    // TC5a: After first tick, wave must NOT have been flushed (Rule 19 skipped)
    // DespawnCount==0 means no flush fired
    TestEqual(TEXT("TC5a: DespawnCount==0 after tick (Rule 19 skipped PauseFlush)"),
        DespawnCount, 0);

    // TC5b: Drain naturally — wave despawns via RunTermination path
    const bool bDrained = DrainUntilEmpty(Actor);
    TestTrue(TEXT("TC5b: wave drained naturally"), bDrained);
    TestEqual(TEXT("TC5c: DespawnReason=RunTermination (not PauseFlush)"),
        CapturedReason, EDespawnReason::RunTermination);

    return true;
}

// ===========================================================================
// TC6: DespawnReason=RunTermination for all post-termination despawns
//
// Three waves (LEANING, TRAVERSING, LANDED). All despawn after run-termination.
// All three must have DespawnReason=RunTermination.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPullWaveRunTermDrainTC6_AllWavesRunTerminationReason,
    "SLIPSTORM.PullWave.RunTerminationDrain.TC6_AllDespawnsHaveRunTerminationReason",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FPullWaveRunTermDrainTC6_AllWavesRunTerminationReason::RunTest(const FString& Parameters)
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

    FTestRSMStub8 RSMStub;
    RSMStub.CurrentState = ERunState::RUNNING;
    RSMStub.bIsPaused    = false;
    Actor->SetRSMProvider(&RSMStub);
    Actor->InitializePool();

    Actor->SetOnDespawnedUserCallback([&](int32, EDespawnReason Reason)
    {
        ++DespawnCount;
        CapturedReasons.Add(Reason);
    });

    AddDrainWave(Actor, 10, EPullWaveState::Leaning);
    AddDrainWave(Actor, 11, EPullWaveState::Traversing);
    AddDrainWave(Actor, 12, EPullWaveState::Landed);

    Actor->OnRSMRunStateChanged(ERunState::DEAD);
    RSMStub.CurrentState = ERunState::DEAD;

    const bool bDrained = DrainUntilEmpty(Actor);
    TestTrue(TEXT("TC6a: all waves drained"), bDrained);
    TestEqual(TEXT("TC6b: three despawns fired"), DespawnCount, 3);

    for (int32 i = 0; i < CapturedReasons.Num(); ++i)
    {
        TestEqual(TEXT("TC6c: each DespawnReason=RunTermination"),
            CapturedReasons[i], EDespawnReason::RunTermination);
    }

    return true;
}

// ===========================================================================
// TC7: Pause-freeze still applies during drain
//
// RSM: GetIsPaused()=true (paused), GetCurrentState()=DEAD (terminated).
// Per-wave tick bodies must be suppressed by the pause-freeze gate.
// Waves do NOT advance during paused drain ticks.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPullWaveRunTermDrainTC7_PauseFreezeAppliesToDrain,
    "SLIPSTORM.PullWave.RunTerminationDrain.TC7_PauseFreezeStillAppliesDuringDrain",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FPullWaveRunTermDrainTC7_PauseFreezeAppliesToDrain::RunTest(const FString& Parameters)
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

    FTestRSMStub8 RSMStub;
    RSMStub.CurrentState = ERunState::DEAD;
    RSMStub.bIsPaused    = true;  // Paused AND terminated
    Actor->SetRSMProvider(&RSMStub);
    Actor->InitializePool();

    Actor->SetOnDespawnedUserCallback([&](int32, EDespawnReason) { ++DespawnCount; });

    AddDrainWave(Actor, 20, EPullWaveState::Leaning);
    Actor->OnRSMRunStateChanged(ERunState::DEAD);

    // Tick several times while paused — wave must not advance
    for (int32 i = 0; i < 10; ++i)
    {
        Actor->Tick(0.1f);
    }

    // TC7a: Wave still in LEANING (pause-freeze suppressed drain ticks)
    TestEqual(TEXT("TC7a: wave still LEANING under pause-freeze during drain"),
        Actor->ActiveWaves.Num(), 1);
    TestEqual(TEXT("TC7b: no despawn fired during paused drain"),
        DespawnCount, 0);

    // Un-pause → drain should resume
    RSMStub.bIsPaused = false;
    const bool bDrained = DrainUntilEmpty(Actor);
    TestTrue(TEXT("TC7c: drain resumes and completes after un-pause"), bDrained);
    TestEqual(TEXT("TC7d: despawn fired once after un-pause"), DespawnCount, 1);

    return true;
}

// ===========================================================================
// TC8: Pool drains to 0 monotonically (no new admissions)
//
// Three waves in different states. Run-termination fires. Pool drains to 0.
// Num() decreases monotonically (never increases) during drain.
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPullWaveRunTermDrainTC8_PoolDrainsMonotonically,
    "SLIPSTORM.PullWave.RunTerminationDrain.TC8_PoolDrainsMonotonicallyToZero",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FPullWaveRunTermDrainTC8_PoolDrainsMonotonically::RunTest(const FString& Parameters)
{
    APullWaveSubsystemActor* Actor = NewObject<APullWaveSubsystemActor>();
    Actor->AddToRoot();
    ON_SCOPE_EXIT
    {
        Actor->SetRSMProvider(nullptr);
        Actor->RemoveFromRoot();
    };

    FTestRSMStub8 RSMStub;
    RSMStub.CurrentState = ERunState::RUNNING;
    RSMStub.bIsPaused    = false;
    Actor->SetRSMProvider(&RSMStub);
    Actor->InitializePool();

    AddDrainWave(Actor, 30, EPullWaveState::Leaning);
    AddDrainWave(Actor, 31, EPullWaveState::Traversing);
    AddDrainWave(Actor, 32, EPullWaveState::Landed);

    Actor->OnRSMRunStateChanged(ERunState::DEAD);
    RSMStub.CurrentState = ERunState::DEAD;

    int32 PrevNum = Actor->ActiveWaves.Num();
    TestEqual(TEXT("TC8 setup: 3 waves"), PrevNum, 3);

    // Tick until empty — assert monotonic decrease
    for (int32 i = 0; i < 100; ++i)
    {
        if (Actor->ActiveWaves.Num() == 0) { break; }
        Actor->Tick(0.1f);
        const int32 CurrentNum = Actor->ActiveWaves.Num();
        // Pool can decrease or stay the same within a tick, never increase
        TestTrue(TEXT("TC8a: Num() does not increase between ticks (monotonic drain)"),
            CurrentNum <= PrevNum);
        PrevNum = CurrentNum;
    }

    TestEqual(TEXT("TC8b: pool empty after drain"), Actor->ActiveWaves.Num(), 0);

    return true;
}

// ===========================================================================
// TC9: Negative control — DespawnReason=NaturalLanding without run-termination
//
// Verifies the ternary in AdvanceLanded: when bRunTerminated==false (no
// OnRSMRunStateChanged call), a wave that drains naturally must despawn with
// DespawnReason=NaturalLanding — NOT RunTermination.
//
// Without this TC a broken AdvanceLanded that always assigns RunTermination
// would pass TC1–TC8 (all of which set bRunTerminated=true via the handler).
// ===========================================================================
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPullWaveRunTermDrainTC9_NaturalLandingNegativeControl,
    "SLIPSTORM.PullWave.RunTerminationDrain.TC9_NaturalLandingWhenNoRunTermination",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FPullWaveRunTermDrainTC9_NaturalLandingNegativeControl::RunTest(const FString& Parameters)
{
    APullWaveSubsystemActor* Actor = NewObject<APullWaveSubsystemActor>();
    Actor->AddToRoot();

    int32 DespawnCount = 0;
    EDespawnReason CapturedReason = EDespawnReason::RunTermination;  // wrong sentinel
    ON_SCOPE_EXIT
    {
        Actor->SetRSMProvider(nullptr);
        Actor->SetOnDespawnedUserCallback({});
        Actor->RemoveFromRoot();
    };

    FTestRSMStub8 RSMStub;
    RSMStub.CurrentState = ERunState::RUNNING;  // stays RUNNING — no termination signal
    RSMStub.bIsPaused    = false;
    Actor->SetRSMProvider(&RSMStub);
    Actor->InitializePool();

    Actor->SetOnDespawnedUserCallback([&](int32, EDespawnReason Reason)
    {
        ++DespawnCount;
        CapturedReason = Reason;
    });

    // Wave starts in LEANING — no OnRSMRunStateChanged called at any point
    AddDrainWave(Actor, 40, EPullWaveState::Leaning);

    // TC9a: Drain to empty without signalling run-termination
    const bool bDrained = DrainUntilEmpty(Actor);
    TestTrue(TEXT("TC9a: wave drained naturally (RUNNING state throughout)"), bDrained);
    TestEqual(TEXT("TC9b: OnWaveDespawned fired once"), DespawnCount, 1);

    // TC9c: Reason must be NaturalLanding — NOT RunTermination
    // (bRunTerminated==false throughout; AdvanceLanded must take the NaturalLanding branch)
    TestEqual(TEXT("TC9c: DespawnReason=NaturalLanding (bRunTerminated was false — no termination signal)"),
        CapturedReason, EDespawnReason::NaturalLanding);

    return true;
}

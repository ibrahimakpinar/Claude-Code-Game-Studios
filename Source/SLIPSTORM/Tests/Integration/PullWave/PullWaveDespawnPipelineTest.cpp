// Copyright (c) Game Studio. All rights reserved.
// SLIPSTORM — 60-second voxel endless runner, mobile, Unreal Engine 5.7
//
// PullWaveDespawnPipelineTest.cpp — Story 006 integration tests.
// Rule 13 six-step despawn pipeline: ordering, reason propagation,
// ISMC hide, state clear, pool compaction, and AC-PW-15 contiguity.
//
// Story:  production/epics/pull-wave/story-006-despawn-pipeline.md
// ADR:    docs/architecture/adr-0010-pullwave-object-pool-state-machine-despawn.md
// TRs:    TR-PW-016 (six-step despawn pipeline), TR-PW-017 (OnWaveDespawned delegate)
//
// Test seam: NewObject<APullWaveSubsystemActor>(GetTransientPackage()) + InitializePool().
// CollisionProvider / TelegraphProvider injected via SetCollisionProvider() /
// SetTelegraphProvider() using file-local stubs.
// WaveMassISMC is null — ISMC step 4 observable via SetOnISMCHideOverride() seam.
// RSMProvider defaults to null (not paused) in all tests.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "PullWave/PullWaveSubsystemActor.h"
#include "PullWave/PullWaveTypes.h"
#include "Seam/CollisionWaveProvider.h"
#include "Seam/TelegraphWaveProvider.h"

// ---------------------------------------------------------------------------
// FTestCollisionProvider — stub for ICollisionWaveProvider (Seam, step 1).
// ---------------------------------------------------------------------------
struct FTestCollisionProvider final : public ICollisionWaveProvider
{
    TArray<int32> UnregisteredWaveIds;
    TFunction<void(int32)> OnUnregister;

    virtual void UnregisterWave(int32 WaveId) override
    {
        UnregisteredWaveIds.Add(WaveId);
        if (OnUnregister) OnUnregister(WaveId);
    }
};

// ---------------------------------------------------------------------------
// FTestTelegraphProvider — stub for ITelegraphWaveProvider (Seam, step 2).
// ---------------------------------------------------------------------------
struct FTestTelegraphProvider final : public ITelegraphWaveProvider
{
    TArray<int32> UnregisteredWaveIds;
    TFunction<void(int32)> OnUnregister;

    virtual void UnregisterWave(int32 WaveId) override
    {
        UnregisteredWaveIds.Add(WaveId);
        if (OnUnregister) OnUnregister(WaveId);
    }
};

// ---------------------------------------------------------------------------
// MakeWaveS006 — minimal wave state factory for Story 006 tests.
//
// ForwardVelocityMs=7.5  → TravelDurationS = SPAWN_PLANE_Z_OFFSET_M / 7.5 = 2.0s.
// LeanDurationS = TELEGRAPH_WINDOW_FLOOR_S.
// ISMCInstanceIndex defaults to 0 (observable via OnISMCHideOverride seam).
// PendingDespawnReason defaults to NaturalLanding; callers may override.
// ---------------------------------------------------------------------------
static FPullWaveInstanceState MakeWaveS006(
    EPullWaveState State,
    int32 WaveId    = 1,
    int32 ISMCIdx   = 0,
    EDespawnReason Reason = EDespawnReason::NaturalLanding)
{
    FPullWaveInstanceState Wave;
    Wave.WaveId               = WaveId;
    Wave.SourceLane           = 0;
    Wave.TargetLane           = 2;
    Wave.ForwardVelocityMs    = 7.5f;
    Wave.LeanDurationS        = TELEGRAPH_WINDOW_FLOOR_S;
    Wave.TravelDurationS      = SPAWN_PLANE_Z_OFFSET_M / 7.5f;
    Wave.State                = State;
    Wave.ISMCInstanceIndex    = ISMCIdx;
    Wave.PendingDespawnReason = Reason;
    return Wave;
}

// ---------------------------------------------------------------------------
// Test suite
// ---------------------------------------------------------------------------
IMPLEMENT_COMPLEX_AUTOMATION_TEST(
    FPullWaveDespawnPipelineTest,
    "SLIPSTORM.PullWave.Integration.DespawnPipeline",
    EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::EditorContext
    | EAutomationTestFlags::ClientContext)

void FPullWaveDespawnPipelineTest::GetTests(
    TArray<FString>& OutBeautifiedNames,
    TArray<FString>& OutTestCommands) const
{
    OutBeautifiedNames.Add(TEXT("six_step_natural_landing: all 6 steps fire in order for NaturalLanding reason"));
    OutTestCommands.Add(TEXT("six_step_natural_landing"));

    OutBeautifiedNames.Add(TEXT("six_step_pause_flush_reason: step 3 carries PauseFlush reason"));
    OutTestCommands.Add(TEXT("six_step_pause_flush_reason"));

    OutBeautifiedNames.Add(TEXT("six_step_run_termination_reason: step 3 carries RunTermination reason"));
    OutTestCommands.Add(TEXT("six_step_run_termination_reason"));

    OutBeautifiedNames.Add(TEXT("ismc_hide_step4: OnISMCHideOverride fires with InstanceIdx and DataIdx=2, Value=1.0"));
    OutTestCommands.Add(TEXT("ismc_hide_step4"));

    OutBeautifiedNames.Add(TEXT("state_clear_step5: wave struct is zero-initialized before RemoveAt"));
    OutTestCommands.Add(TEXT("state_clear_step5"));

    OutBeautifiedNames.Add(TEXT("remove_at_step6_num_decreases: ActiveWaves.Num() decreases by 1 after despawn"));
    OutTestCommands.Add(TEXT("remove_at_step6_num_decreases"));

    OutBeautifiedNames.Add(TEXT("waveid_asc_preserved_post_removal: remaining waves keep WaveId ASC order after RemoveAt"));
    OutTestCommands.Add(TEXT("waveid_asc_preserved_post_removal"));

    OutBeautifiedNames.Add(TEXT("ac_pw15_contiguity_three_waves: three-wave pipeline fires fully per-wave before next (AC-PW-15)"));
    OutTestCommands.Add(TEXT("ac_pw15_contiguity_three_waves"));

    OutBeautifiedNames.Add(TEXT("seam13_fires_alongside_delegate: OnDespawnedUserCallback fires same step as OnWaveDespawned"));
    OutTestCommands.Add(TEXT("seam13_fires_alongside_delegate"));

    OutBeautifiedNames.Add(TEXT("no_lean_progress_during_despawning: OnLeanProgress does NOT fire during DESPAWNING"));
    OutTestCommands.Add(TEXT("no_lean_progress_during_despawning"));

    OutBeautifiedNames.Add(TEXT("null_providers_safe: null Collision + Telegraph providers do not crash (step 1 and 2 silently skipped)"));
    OutTestCommands.Add(TEXT("null_providers_safe"));
}

bool FPullWaveDespawnPipelineTest::RunTest(const FString& Parameters)
{
    // -----------------------------------------------------------------------
    // TC1 — six_step_natural_landing
    //
    // Wave in DESPAWNING with NaturalLanding reason. One Tick() fires all 6 steps.
    // Verified: Coll-unregister(1) → Tel-unregister(1) → OnWaveDespawned(1, NaturalLanding)
    //           → ISMC-hide(ISMCIdx=0, DataIdx=2, 1.0) → state-clear → RemoveAt.
    // Post-Tick: ActiveWaves.Num() == 0.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("six_step_natural_landing"))
    {
        APullWaveSubsystemActor* Actor =
            NewObject<APullWaveSubsystemActor>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC1: actor created"), Actor)) return false;
        Actor->AddToRoot();
        ON_SCOPE_EXIT { Actor->RemoveFromRoot(); };
        Actor->InitializePool();

        FTestCollisionProvider  CollStub;
        FTestTelegraphProvider  TelStub;
        Actor->SetCollisionProvider(&CollStub);
        Actor->SetTelegraphProvider(&TelStub);
        ON_SCOPE_EXIT
        {
            Actor->SetCollisionProvider(nullptr);
            Actor->SetTelegraphProvider(nullptr);
        };

        // Track step 3 (delegate) and step 4 (ISMC seam) order relative to steps 1/2.
        TArray<FString> StepLog;
        CollStub.OnUnregister = [&](int32 Id){ StepLog.Add(FString::Printf(TEXT("Coll:%d"), Id)); };
        TelStub.OnUnregister  = [&](int32 Id){ StepLog.Add(FString::Printf(TEXT("Tel:%d"),  Id)); };

        int32 DespawnWaveId    = -1;
        int32 DespawnFireCount = 0;
        EDespawnReason DespawnReason = EDespawnReason::PauseFlush; // sentinel non-Natural
        Actor->OnWaveDespawned.AddLambda([&](int32 WId, EDespawnReason R)
        {
            ++DespawnFireCount;
            DespawnWaveId = WId;
            DespawnReason = R;
            StepLog.Add(FString::Printf(TEXT("Desp:%d"), WId));
        });

        int32 ISMCInstIdx  = -99;
        int32 ISMCDataIdx  = -99;
        float ISMCValue    = -1.0f;
        Actor->SetOnISMCHideOverride([&](int32 Inst, int32 Data, float Val)
        {
            ISMCInstIdx = Inst;
            ISMCDataIdx = Data;
            ISMCValue   = Val;
            StepLog.Add(FString::Printf(TEXT("ISMC:%d"), Inst));
        });

        FPullWaveInstanceState Wave = MakeWaveS006(
            EPullWaveState::Despawning, /*WaveId=*/1, /*ISMCIdx=*/5,
            EDespawnReason::NaturalLanding);
        Actor->ActiveWaves.Add(Wave);

        Actor->Tick(0.016f);

        // Steps 1–4 fired in order
        if (!TestEqual(TEXT("TC1a: 4 step-log entries"),     StepLog.Num(), 4)) return false;
        TestEqual(TEXT("TC1b: step 1 = Coll"), StepLog[0], FString(TEXT("Coll:1")));
        TestEqual(TEXT("TC1c: step 2 = Tel"),  StepLog[1], FString(TEXT("Tel:1")));
        TestEqual(TEXT("TC1d: step 3 = Desp"), StepLog[2], FString(TEXT("Desp:1")));
        TestEqual(TEXT("TC1e: step 4 = ISMC"), StepLog[3], FString(TEXT("ISMC:5")));

        // Step 3 payload
        TestEqual(TEXT("TC1f: OnWaveDespawned WaveId=1"),           DespawnWaveId, 1);
        TestEqual(TEXT("TC1g: OnWaveDespawned reason=NaturalLanding"),
            DespawnReason, EDespawnReason::NaturalLanding);

        // Step 4 payload
        TestEqual(TEXT("TC1h: ISMC InstanceIdx=5"),  ISMCInstIdx, 5);
        TestEqual(TEXT("TC1i: ISMC DataIdx=2"),       ISMCDataIdx, 2);
        TestTrue(TEXT("TC1j: ISMC Value=1.0"),        FMath::Abs(ISMCValue - 1.0f) < 1e-5f);

        // Step 6: pool compaction
        TestEqual(TEXT("TC1k: ActiveWaves.Num()==0 after despawn"), Actor->ActiveWaves.Num(), 0);

        // OnWaveDespawned fires exactly once
        TestEqual(TEXT("TC1n: OnWaveDespawned fires exactly once"), DespawnFireCount, 1);

        // Coll step 1 captured
        TestEqual(TEXT("TC1o: Coll UnregisteredWaveIds=[1]"),
            CollStub.UnregisteredWaveIds.Num(), 1);
        TestEqual(TEXT("TC1p: Coll unregistered WaveId=1"),
            CollStub.UnregisteredWaveIds[0], 1);
        return true;
    }

    // -----------------------------------------------------------------------
    // TC2 — six_step_pause_flush_reason
    //
    // Wave in DESPAWNING with PauseFlush reason. Verify step 3 payload carries
    // EDespawnReason::PauseFlush.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("six_step_pause_flush_reason"))
    {
        APullWaveSubsystemActor* Actor =
            NewObject<APullWaveSubsystemActor>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC2: actor created"), Actor)) return false;
        Actor->AddToRoot();
        ON_SCOPE_EXIT { Actor->RemoveFromRoot(); };
        Actor->InitializePool();

        EDespawnReason CapturedReason  = EDespawnReason::NaturalLanding; // sentinel
        int32          DespawnFireCount = 0;
        Actor->OnWaveDespawned.AddLambda([&](int32, EDespawnReason R)
            { ++DespawnFireCount; CapturedReason = R; });

        FPullWaveInstanceState Wave = MakeWaveS006(
            EPullWaveState::Despawning, 1, 0, EDespawnReason::PauseFlush);
        Actor->ActiveWaves.Add(Wave);

        Actor->Tick(0.016f);

        TestEqual(TEXT("TC2a: OnWaveDespawned reason==PauseFlush"),
            CapturedReason, EDespawnReason::PauseFlush);
        TestEqual(TEXT("TC2b: wave removed from pool"), Actor->ActiveWaves.Num(), 0);
        TestEqual(TEXT("TC2c: OnWaveDespawned fires exactly once"), DespawnFireCount, 1);
        return true;
    }

    // -----------------------------------------------------------------------
    // TC3 — six_step_run_termination_reason
    //
    // Wave in DESPAWNING with RunTermination reason. Verify step 3 payload carries
    // EDespawnReason::RunTermination.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("six_step_run_termination_reason"))
    {
        APullWaveSubsystemActor* Actor =
            NewObject<APullWaveSubsystemActor>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC3: actor created"), Actor)) return false;
        Actor->AddToRoot();
        ON_SCOPE_EXIT { Actor->RemoveFromRoot(); };
        Actor->InitializePool();

        EDespawnReason CapturedReason  = EDespawnReason::NaturalLanding; // sentinel
        int32          DespawnFireCount = 0;
        Actor->OnWaveDespawned.AddLambda([&](int32, EDespawnReason R)
            { ++DespawnFireCount; CapturedReason = R; });

        FPullWaveInstanceState Wave = MakeWaveS006(
            EPullWaveState::Despawning, 1, 0, EDespawnReason::RunTermination);
        Actor->ActiveWaves.Add(Wave);

        Actor->Tick(0.016f);

        TestEqual(TEXT("TC3a: OnWaveDespawned reason==RunTermination"),
            CapturedReason, EDespawnReason::RunTermination);
        TestEqual(TEXT("TC3b: wave removed from pool"), Actor->ActiveWaves.Num(), 0);
        TestEqual(TEXT("TC3c: OnWaveDespawned fires exactly once"), DespawnFireCount, 1);
        return true;
    }

    // -----------------------------------------------------------------------
    // TC4 — ismc_hide_step4
    //
    // Verify OnISMCHideOverride fires with correct InstanceIdx, DataIdx=2, Value=1.0.
    // Wave has ISMCInstanceIndex=7 to ensure non-trivial index captured.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("ismc_hide_step4"))
    {
        APullWaveSubsystemActor* Actor =
            NewObject<APullWaveSubsystemActor>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC4: actor created"), Actor)) return false;
        Actor->AddToRoot();
        ON_SCOPE_EXIT { Actor->RemoveFromRoot(); };
        Actor->InitializePool();

        int32 CapturedInst  = -99;
        int32 CapturedData  = -99;
        float CapturedValue = -1.0f;
        int32 CallCount     = 0;

        Actor->SetOnISMCHideOverride([&](int32 Inst, int32 Data, float Val)
        {
            ++CallCount;
            CapturedInst  = Inst;
            CapturedData  = Data;
            CapturedValue = Val;
        });

        FPullWaveInstanceState Wave = MakeWaveS006(
            EPullWaveState::Despawning, 1, /*ISMCIdx=*/7);
        Actor->ActiveWaves.Add(Wave);

        Actor->Tick(0.016f);

        TestEqual(TEXT("TC4a: OnISMCHideOverride called once"),   CallCount,     1);
        TestEqual(TEXT("TC4b: InstanceIdx=7"),                     CapturedInst,  7);
        TestEqual(TEXT("TC4c: DataIdx=2"),                         CapturedData,  2);
        TestTrue(TEXT("TC4d: Value=1.0"),                          FMath::Abs(CapturedValue - 1.0f) < 1e-5f);
        return true;
    }

    // -----------------------------------------------------------------------
    // TC5 — state_clear_step5
    //
    // Step 5 zero-initializes the wave struct. Verified via SetOnStateClearOverride seam:
    // callback fires AFTER Wave = FPullWaveInstanceState{} and BEFORE RemoveAt.
    // Two waves (WaveId 1 and 2) each receive a cleared-struct snapshot; individual
    // fields (WaveId, State, CollisionOutcome, ForwardVelocityMs, TargetLane) are asserted.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("state_clear_step5"))
    {
        APullWaveSubsystemActor* Actor =
            NewObject<APullWaveSubsystemActor>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC5: actor created"), Actor)) return false;
        Actor->AddToRoot();
        ON_SCOPE_EXIT
        {
            Actor->SetOnStateClearOverride({});
            Actor->RemoveFromRoot();
        };
        Actor->InitializePool();

        // Capture cleared struct snapshots from the step-5 seam.
        TArray<FPullWaveInstanceState> ClearedSnapshots;
        Actor->SetOnStateClearOverride([&](const FPullWaveInstanceState& Cleared)
        {
            ClearedSnapshots.Add(Cleared);
        });

        // Also capture pre-clear WaveIds from step 3 (confirms capture-before-clear works).
        TArray<int32> DespawnedIds;
        Actor->OnWaveDespawned.AddLambda([&](int32 WId, EDespawnReason)
        {
            DespawnedIds.Add(WId);
        });

        FPullWaveInstanceState Wave1 = MakeWaveS006(EPullWaveState::Despawning, 1, 0);
        FPullWaveInstanceState Wave2 = MakeWaveS006(EPullWaveState::Despawning, 2, 1);
        Actor->ActiveWaves.Add(Wave1);
        Actor->ActiveWaves.Add(Wave2);

        Actor->Tick(0.016f);

        // Both waves removed.
        TestEqual(TEXT("TC5a: ActiveWaves empty after both despawned"),
            Actor->ActiveWaves.Num(), 0);

        // step 3 captured pre-clear WaveIds (proves capture-before-clear in AdvanceDespawning).
        TestEqual(TEXT("TC5b: two OnWaveDespawned events captured"), DespawnedIds.Num(), 2);
        TestTrue(TEXT("TC5c: WaveId=1 despawned"), DespawnedIds.Contains(1));
        TestTrue(TEXT("TC5d: WaveId=2 despawned"), DespawnedIds.Contains(2));

        // Step 5 seam: two cleared snapshots, one per wave.
        if (!TestEqual(TEXT("TC5e: step-5 seam fired twice"), ClearedSnapshots.Num(), 2))
            return false;

        // All struct fields must be zero/default after Wave = FPullWaveInstanceState{}.
        for (int32 k = 0; k < ClearedSnapshots.Num(); ++k)
        {
            const FPullWaveInstanceState& S = ClearedSnapshots[k];
            TestEqual(FString::Printf(TEXT("TC5f[%d]: WaveId cleared to 0"), k),
                S.WaveId, 0);
            TestEqual(FString::Printf(TEXT("TC5g[%d]: TargetLane cleared to 0"), k),
                S.TargetLane, 0);
            TestEqual(FString::Printf(TEXT("TC5h[%d]: State cleared to Spawned"), k),
                S.State, EPullWaveState::Spawned);
            TestEqual(FString::Printf(TEXT("TC5i[%d]: CollisionOutcome cleared to Unresolved"), k),
                S.CollisionOutcome, ECollisionOutcome::Unresolved);
            TestTrue(FString::Printf(TEXT("TC5j[%d]: ForwardVelocityMs cleared to 0"), k),
                FMath::Abs(S.ForwardVelocityMs) < 1e-5f);
            TestTrue(FString::Printf(TEXT("TC5k[%d]: TraverseElapsedS cleared to 0"), k),
                FMath::Abs(S.TraverseElapsedS) < 1e-5f);
        }
        return true;
    }

    // -----------------------------------------------------------------------
    // TC6 — remove_at_step6_num_decreases
    //
    // Single wave in DESPAWNING. After Tick, ActiveWaves.Num() == 0.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("remove_at_step6_num_decreases"))
    {
        APullWaveSubsystemActor* Actor =
            NewObject<APullWaveSubsystemActor>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC6: actor created"), Actor)) return false;
        Actor->AddToRoot();
        ON_SCOPE_EXIT { Actor->RemoveFromRoot(); };
        Actor->InitializePool();

        FPullWaveInstanceState Wave = MakeWaveS006(EPullWaveState::Despawning, 1);
        Actor->ActiveWaves.Add(Wave);
        TestEqual(TEXT("TC6 pre: Num==1"), Actor->ActiveWaves.Num(), 1);

        Actor->Tick(0.016f);

        TestEqual(TEXT("TC6a: Num==0 after RemoveAt (step 6)"),
            Actor->ActiveWaves.Num(), 0);
        return true;
    }

    // -----------------------------------------------------------------------
    // TC7 — waveid_asc_preserved_post_removal
    //
    // Three waves: WaveId 1 in DESPAWNING, WaveId 2 in LEANING, WaveId 3 in LEANING.
    // After Tick: WaveId 1 removed; WaveIds [2, 3] remain in ASC order.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("waveid_asc_preserved_post_removal"))
    {
        APullWaveSubsystemActor* Actor =
            NewObject<APullWaveSubsystemActor>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC7: actor created"), Actor)) return false;
        Actor->AddToRoot();
        ON_SCOPE_EXIT { Actor->RemoveFromRoot(); };
        Actor->InitializePool();

        FPullWaveInstanceState Wave1 = MakeWaveS006(EPullWaveState::Despawning, 1);
        FPullWaveInstanceState Wave2 = MakeWaveS006(EPullWaveState::Leaning,    2);
        FPullWaveInstanceState Wave3 = MakeWaveS006(EPullWaveState::Leaning,    3);
        // Give lean-state waves valid LeanDurationS so AdvanceLeaning doesn't transition.
        Wave2.LeanProgress = 0.1f;
        Wave3.LeanProgress = 0.1f;
        Actor->ActiveWaves.Add(Wave1);
        Actor->ActiveWaves.Add(Wave2);
        Actor->ActiveWaves.Add(Wave3);

        Actor->Tick(0.001f);  // Small DeltaTime so LEANING waves don't complete lean.

        TestEqual(TEXT("TC7a: Num==2 after Wave1 removed"), Actor->ActiveWaves.Num(), 2);
        TestEqual(TEXT("TC7b: remaining[0].WaveId==2 (ASC preserved)"),
            Actor->ActiveWaves[0].WaveId, 2);
        TestEqual(TEXT("TC7c: remaining[1].WaveId==3 (ASC preserved)"),
            Actor->ActiveWaves[1].WaveId, 3);
        return true;
    }

    // -----------------------------------------------------------------------
    // TC8 — ac_pw15_contiguity_three_waves  (AC-PW-15)
    //
    // Three waves (WaveIds 1, 2, 3) all in DESPAWNING simultaneously.
    // Event log must be: Coll:1, Tel:1, Desp:1, ISMC:1,
    //                    Coll:2, Tel:2, Desp:2, ISMC:2,
    //                    Coll:3, Tel:3, Desp:3, ISMC:3.
    // No interleaving across WaveIds — each wave's 4 observable steps are contiguous.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("ac_pw15_contiguity_three_waves"))
    {
        APullWaveSubsystemActor* Actor =
            NewObject<APullWaveSubsystemActor>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC8: actor created"), Actor)) return false;
        Actor->AddToRoot();
        ON_SCOPE_EXIT
        {
            Actor->SetCollisionProvider(nullptr);
            Actor->SetTelegraphProvider(nullptr);
            Actor->RemoveFromRoot();
        };
        Actor->InitializePool();

        FTestCollisionProvider  CollStub;
        FTestTelegraphProvider  TelStub;
        Actor->SetCollisionProvider(&CollStub);
        Actor->SetTelegraphProvider(&TelStub);

        TArray<FString> EventLog;

        CollStub.OnUnregister = [&](int32 Id){ EventLog.Add(FString::Printf(TEXT("Coll:%d"), Id)); };
        TelStub.OnUnregister  = [&](int32 Id){ EventLog.Add(FString::Printf(TEXT("Tel:%d"),  Id)); };

        Actor->OnWaveDespawned.AddLambda([&](int32 WId, EDespawnReason)
        {
            EventLog.Add(FString::Printf(TEXT("Desp:%d"), WId));
        });

        Actor->SetOnISMCHideOverride([&](int32 Inst, int32, float)
        {
            EventLog.Add(FString::Printf(TEXT("ISMC:%d"), Inst));
        });

        // ISMCInstanceIndex matches WaveId so log entries are distinguishable.
        Actor->ActiveWaves.Add(MakeWaveS006(EPullWaveState::Despawning, 1, /*ISMCIdx=*/1));
        Actor->ActiveWaves.Add(MakeWaveS006(EPullWaveState::Despawning, 2, /*ISMCIdx=*/2));
        Actor->ActiveWaves.Add(MakeWaveS006(EPullWaveState::Despawning, 3, /*ISMCIdx=*/3));

        Actor->Tick(0.016f);

        // All three waves removed.
        TestEqual(TEXT("TC8a: pool empty after three despawns"),
            Actor->ActiveWaves.Num(), 0);

        // 12 events: 4 per wave × 3 waves.
        if (!TestEqual(TEXT("TC8b: 12 events total"), EventLog.Num(), 12)) return false;

        // Verify contiguous per-wave ordering (AC-PW-15).
        const TArray<FString> Expected = {
            TEXT("Coll:1"), TEXT("Tel:1"), TEXT("Desp:1"), TEXT("ISMC:1"),
            TEXT("Coll:2"), TEXT("Tel:2"), TEXT("Desp:2"), TEXT("ISMC:2"),
            TEXT("Coll:3"), TEXT("Tel:3"), TEXT("Desp:3"), TEXT("ISMC:3"),
        };
        for (int32 k = 0; k < Expected.Num(); ++k)
        {
            TestEqual(FString::Printf(TEXT("TC8c event[%d]"), k),
                EventLog[k], Expected[k]);
        }
        return true;
    }

    // -----------------------------------------------------------------------
    // TC9 — seam13_fires_alongside_delegate
    //
    // OnDespawnedUserCallback (Seam 13) fires in step 3, same tick as
    // OnWaveDespawned delegate. Both capture the same WaveId and Reason.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("seam13_fires_alongside_delegate"))
    {
        APullWaveSubsystemActor* Actor =
            NewObject<APullWaveSubsystemActor>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC9: actor created"), Actor)) return false;
        Actor->AddToRoot();
        ON_SCOPE_EXIT { Actor->RemoveFromRoot(); };
        Actor->InitializePool();

        int32 DelegateWaveId  = -1;
        int32 CallbackWaveId  = -1;
        EDespawnReason DelegateReason  = EDespawnReason::PauseFlush;    // sentinels
        EDespawnReason CallbackReason  = EDespawnReason::RunTermination;

        Actor->OnWaveDespawned.AddLambda([&](int32 WId, EDespawnReason R)
        {
            DelegateWaveId   = WId;
            DelegateReason   = R;
        });

        Actor->SetOnDespawnedUserCallback([&](int32 WId, EDespawnReason R)
        {
            CallbackWaveId  = WId;
            CallbackReason  = R;
        });

        FPullWaveInstanceState Wave = MakeWaveS006(
            EPullWaveState::Despawning, 1, 0, EDespawnReason::NaturalLanding);
        Actor->ActiveWaves.Add(Wave);

        Actor->Tick(0.016f);

        TestEqual(TEXT("TC9a: delegate WaveId=1"),    DelegateWaveId,  1);
        TestEqual(TEXT("TC9b: callback WaveId=1"),    CallbackWaveId,  1);
        TestEqual(TEXT("TC9c: delegate reason=NaturalLanding"),
            DelegateReason, EDespawnReason::NaturalLanding);
        TestEqual(TEXT("TC9d: callback reason=NaturalLanding"),
            CallbackReason, EDespawnReason::NaturalLanding);
        return true;
    }

    // -----------------------------------------------------------------------
    // TC10 — no_lean_progress_during_despawning
    //
    // Wave placed directly in DESPAWNING state. Tick dispatches AdvanceDespawning,
    // not AdvanceLeaning. OnLeanProgress must NOT fire.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("no_lean_progress_during_despawning"))
    {
        APullWaveSubsystemActor* Actor =
            NewObject<APullWaveSubsystemActor>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC10: actor created"), Actor)) return false;
        Actor->AddToRoot();
        ON_SCOPE_EXIT { Actor->RemoveFromRoot(); };
        Actor->InitializePool();

        int32 LeanProgressFires = 0;
        Actor->OnLeanProgress.AddLambda([&](const FLeanTickData&) { ++LeanProgressFires; });

        FPullWaveInstanceState Wave = MakeWaveS006(
            EPullWaveState::Despawning, 1, 0, EDespawnReason::NaturalLanding);
        Actor->ActiveWaves.Add(Wave);

        Actor->Tick(0.016f);

        TestEqual(TEXT("TC10: OnLeanProgress NOT fired during DESPAWNING"),
            LeanProgressFires, 0);
        return true;
    }

    // -----------------------------------------------------------------------
    // TC11 — null_providers_safe
    //
    // No CollisionProvider or TelegraphProvider injected. Steps 1 and 2 silently
    // skipped. Wave still despawns: step 3+ fire, Num decreases to 0.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("null_providers_safe"))
    {
        APullWaveSubsystemActor* Actor =
            NewObject<APullWaveSubsystemActor>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC11: actor created"), Actor)) return false;
        Actor->AddToRoot();
        ON_SCOPE_EXIT { Actor->RemoveFromRoot(); };
        Actor->InitializePool();

        // Do NOT inject any collision or telegraph provider — test null-guard path.
        int32 DespawnCount = 0;
        Actor->OnWaveDespawned.AddLambda([&](int32, EDespawnReason) { ++DespawnCount; });

        FPullWaveInstanceState Wave = MakeWaveS006(
            EPullWaveState::Despawning, 1, 0, EDespawnReason::NaturalLanding);
        Actor->ActiveWaves.Add(Wave);

        // Should not crash even with null providers.
        Actor->Tick(0.016f);

        TestEqual(TEXT("TC11a: OnWaveDespawned still fired despite null providers"),
            DespawnCount, 1);
        TestEqual(TEXT("TC11b: wave removed from pool"),
            Actor->ActiveWaves.Num(), 0);
        return true;
    }

    AddError(FString::Printf(TEXT("Unknown test command: %s"), *Parameters));
    return false;
}

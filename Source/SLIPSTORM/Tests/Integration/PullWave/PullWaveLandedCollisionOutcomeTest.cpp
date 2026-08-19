// Copyright (c) Game Studio. All rights reserved.
// SLIPSTORM — 60-second voxel endless runner, mobile, Unreal Engine 5.7
//
// PullWaveLandedCollisionOutcomeTest.cpp — Story 005 integration tests.
// LANDED entry body: CollisionOutcome resolution and Hit/NearMiss broadcasts.
//
// Story:  production/epics/pull-wave/story-005-landed-collision-outcome.md
// ADR:    docs/architecture/adr-0010-pullwave-object-pool-state-machine-despawn.md
// TRs:    TR-PW-008 (hit/near-miss resolution), TR-PW-009 (wave hold timer)
//
// Test seam: NewObject<APullWaveSubsystemActor>(GetTransientPackage()) + InitializePool().
// PMProvider injected via SetPMProvider() using a local FTestPMProvider stub.
// RSMProvider defaults to null (not paused) in all tests.
// WaveMassISMC is null — ISMC call sites are null-guarded by AdvanceTraversing.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "PullWave/PullWaveSubsystemActor.h"
#include "PullWave/PullWaveTypes.h"
#include "Seam/PlayerMovementProvider.h"
#include "Player/EPlayerLane.h"

// ---------------------------------------------------------------------------
// FTestPMProvider — local PM stub for collision outcome seam (Seam 12).
// ---------------------------------------------------------------------------
struct FTestPMProvider final : public IPlayerMovementProvider
{
    EPlayerLane     CurrentLane       = EPlayerLane::Center;
    EPlayerLane     PMTargetLane      = EPlayerLane::Center;
    EMovementState  MovState          = EMovementState::SETTLED;
    int32           NearMissBeatCount = 0;

    virtual EPlayerLane     GetCurrentLane()    const override { return CurrentLane;       }
    virtual EPlayerLane     GetTargetLane()     const override { return PMTargetLane;      }
    virtual EMovementState  GetMovementState()  const override { return MovState;          }
    virtual void            TriggerNearMissBeat()    override { ++NearMissBeatCount;       }
};

// ---------------------------------------------------------------------------
// MakeWaveS005 — minimal wave state for Story 005 tests.
//
// ForwardVelocityMs=7.5 → TravelDurationS=15.0/7.5=2.0s.
// ISMCInstanceIndex=-1 so AdvanceTraversing null-guards ISMC updates.
// ---------------------------------------------------------------------------
static FPullWaveInstanceState MakeWaveS005(
    EPullWaveState State,
    int32 SourceLane        = 2,
    int32 TargetLane        = 2,
    float ForwardVelocityMs = 7.5f)
{
    FPullWaveInstanceState Wave;
    Wave.WaveId             = 1;
    Wave.SourceLane         = SourceLane;
    Wave.TargetLane         = TargetLane;
    Wave.ForwardVelocityMs  = ForwardVelocityMs;
    Wave.LeanDurationS      = TELEGRAPH_WINDOW_FLOOR_S;
    Wave.TravelDurationS    =
        SPAWN_PLANE_Z_OFFSET_M / FMath::Max(ForwardVelocityMs, KINDA_SMALL_NUMBER);
    Wave.State              = State;
    Wave.ISMCInstanceIndex  = -1;
    Wave.SpawnTimeS         = 0.0f;
    return Wave;
}

// ---------------------------------------------------------------------------
// Test suite
// ---------------------------------------------------------------------------
IMPLEMENT_COMPLEX_AUTOMATION_TEST(
    FPullWaveLandedCollisionTest,
    "SLIPSTORM.PullWave.Integration.LandedCollisionOutcome",
    EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::EditorContext
    | EAutomationTestFlags::ClientContext)

void FPullWaveLandedCollisionTest::GetTests(
    TArray<FString>& OutBeautifiedNames,
    TArray<FString>& OutTestCommands) const
{
    OutBeautifiedNames.Add(TEXT("threshold_cross: CollisionOutcome resolved same tick as TRAVERSING→LANDED (AC-PW-13)"));
    OutTestCommands.Add(TEXT("threshold_cross_same_tick"));

    OutBeautifiedNames.Add(TEXT("landed_hold: expires after WAVE_DESPAWN_HOLD_S; events not re-broadcast (AC-PW-14)"));
    OutTestCommands.Add(TEXT("landed_hold_expires_no_refire"));

    OutBeautifiedNames.Add(TEXT("hit_settled: PM SETTLED in target lane → Hit + OnWaveHit broadcast (AC-PW-21a)"));
    OutTestCommands.Add(TEXT("hit_settled_target_lane"));

    OutBeautifiedNames.Add(TEXT("hit_slipping: PM SLIPPING toward target lane → Hit + OnWaveHit broadcast (AC-PW-21b)"));
    OutTestCommands.Add(TEXT("hit_slipping_toward_target"));

    OutBeautifiedNames.Add(TEXT("clean_miss: PM SETTLED in wrong lane → CleanMiss, no events (AC-PW-21c)"));
    OutTestCommands.Add(TEXT("clean_miss_settled_wrong_lane"));

    OutBeautifiedNames.Add(TEXT("near_miss: PM SLIPPING from target lane → NearMiss + TriggerNearMissBeat + OnNearMiss (AC-PW-22)"));
    OutTestCommands.Add(TEXT("near_miss_slipping_from_target"));

    OutBeautifiedNames.Add(TEXT("concurrent: two waves land same tick in same lane, each resolved independently (AC-PW-28)"));
    OutTestCommands.Add(TEXT("concurrent_landing_same_lane"));

    OutBeautifiedNames.Add(TEXT("overshoot: large DeltaTime clamps TraverseElapsedS to TravelDurationS at landing (AC-PW-31)"));
    OutTestCommands.Add(TEXT("overshoot_hitch_landing"));

    OutBeautifiedNames.Add(TEXT("no_lean_progress: OnLeanProgress does not fire during LANDED state"));
    OutTestCommands.Add(TEXT("no_lean_progress_during_landed"));
}

bool FPullWaveLandedCollisionTest::RunTest(const FString& Parameters)
{
    const float Eps = 1e-5f;

    // -----------------------------------------------------------------------
    // TC1 — AC-PW-13: CollisionOutcome resolved in the same tick as the transition.
    //
    // Wave in TRAVERSING just below threshold-cross. PM settled in target lane.
    // One Tick() fires AdvanceTraversing → threshold crossed → TransitionTo(LANDED)
    // → ResolveLandedEntry assigns CollisionOutcome and fires OnWaveHit, all same tick.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("threshold_cross_same_tick"))
    {
        APullWaveSubsystemActor* Actor =
            NewObject<APullWaveSubsystemActor>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC1: actor created"), Actor)) return false;
        Actor->AddToRoot();
        ON_SCOPE_EXIT { Actor->RemoveFromRoot(); };
        Actor->InitializePool();

        FTestPMProvider PMStub;
        PMStub.CurrentLane = EPlayerLane::Center;   // ordinal 2 = wave TargetLane
        PMStub.MovState    = EMovementState::SETTLED;
        Actor->SetPMProvider(&PMStub);

        FPullWaveInstanceState Wave = MakeWaveS005(EPullWaveState::Traversing, /*src=*/0, /*tgt=*/2);
        Wave.TraverseElapsedS = Wave.TravelDurationS - 0.001f;
        Actor->ActiveWaves.Add(Wave);

        int32 HitCount = 0;
        Actor->OnWaveHit.AddLambda([&](int32, int32, int32, float) { ++HitCount; });

        Actor->Tick(0.016f);

        TestEqual(TEXT("TC1a: State==LANDED after threshold-cross tick"),
            Actor->ActiveWaves[0].State, EPullWaveState::Landed);
        TestEqual(TEXT("TC1b: CollisionOutcome==Hit set same tick as transition (AC-PW-13)"),
            Actor->ActiveWaves[0].CollisionOutcome, ECollisionOutcome::Hit);
        TestEqual(TEXT("TC1c: OnWaveHit fired once on transition tick"), HitCount, 1);
        return true;
    }

    // -----------------------------------------------------------------------
    // TC2 — AC-PW-14: hold timer expires → DESPAWNING; events NOT re-broadcast.
    //
    // Wave placed directly in LANDED (simulates post-entry state). 10 × 0.016s
    // = 0.16s > WAVE_DESPAWN_HOLD_S=0.15s → transition to DESPAWNING.
    // OnWaveHit / OnNearMiss listeners count must remain 0 during the hold.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("landed_hold_expires_no_refire"))
    {
        APullWaveSubsystemActor* Actor =
            NewObject<APullWaveSubsystemActor>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC2: actor created"), Actor)) return false;
        Actor->AddToRoot();
        ON_SCOPE_EXIT { Actor->RemoveFromRoot(); };
        Actor->InitializePool();

        FPullWaveInstanceState Wave = MakeWaveS005(EPullWaveState::Landed, /*src=*/0, /*tgt=*/2);
        Wave.CollisionOutcome   = ECollisionOutcome::Hit;  // pre-set (ResolveLandedEntry already ran)
        Wave.LandedHoldElapsedS = 0.0f;
        Actor->ActiveWaves.Add(Wave);

        int32 HitFires      = 0;
        int32 NearMissFires = 0;
        Actor->OnWaveHit.AddLambda([&](int32, int32, int32, float) { ++HitFires;      });
        Actor->OnNearMiss.AddLambda([&](int32, int32, int32)        { ++NearMissFires; });

        for (int32 i = 0; i < 10; ++i) { Actor->Tick(0.016f); }

        TestEqual(TEXT("TC2a: State==DESPAWNING after hold expires"),
            Actor->ActiveWaves[0].State, EPullWaveState::Despawning);
        TestEqual(TEXT("TC2b: OnWaveHit NOT re-broadcast during hold (AC-PW-14)"),
            HitFires, 0);
        TestEqual(TEXT("TC2c: OnNearMiss NOT re-broadcast during hold (AC-PW-14)"),
            NearMissFires, 0);
        return true;
    }

    // -----------------------------------------------------------------------
    // TC3 — AC-PW-21a: Hit when PM SETTLED in target lane.
    //
    // Verifies delegate params: WaveId, TargetLane, SourceLane, SpawnTimeS.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("hit_settled_target_lane"))
    {
        APullWaveSubsystemActor* Actor =
            NewObject<APullWaveSubsystemActor>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC3: actor created"), Actor)) return false;
        Actor->AddToRoot();
        ON_SCOPE_EXIT { Actor->RemoveFromRoot(); };
        Actor->InitializePool();

        FTestPMProvider PMStub;
        PMStub.CurrentLane = EPlayerLane::Right;   // ordinal 3 = wave TargetLane
        PMStub.MovState    = EMovementState::SETTLED;
        Actor->SetPMProvider(&PMStub);

        FPullWaveInstanceState Wave = MakeWaveS005(EPullWaveState::Traversing, /*src=*/1, /*tgt=*/3);
        Wave.TraverseElapsedS = Wave.TravelDurationS - 0.001f;
        Actor->ActiveWaves.Add(Wave);

        int32 HitWaveId     = -1;
        int32 HitTargetLane = -1;
        int32 HitSourceLane = -1;
        float HitSpawnTimeS = -1.0f;
        Actor->OnWaveHit.AddLambda([&](int32 WId, int32 TL, int32 SL, float ST)
        {
            HitWaveId     = WId;
            HitTargetLane = TL;
            HitSourceLane = SL;
            HitSpawnTimeS = ST;
        });

        Actor->Tick(0.016f);

        TestEqual(TEXT("TC3a: CollisionOutcome==Hit (SETTLED in target lane)"),
            Actor->ActiveWaves[0].CollisionOutcome, ECollisionOutcome::Hit);
        TestEqual(TEXT("TC3b: OnWaveHit WaveId=1"),     HitWaveId,     1);
        TestEqual(TEXT("TC3c: OnWaveHit TargetLane=3"), HitTargetLane, 3);
        TestEqual(TEXT("TC3d: OnWaveHit SourceLane=1"), HitSourceLane, 1);
        TestTrue(TEXT("TC3e: OnWaveHit SpawnTimeS=0.0"), FMath::Abs(HitSpawnTimeS) < Eps);
        return true;
    }

    // -----------------------------------------------------------------------
    // TC4 — AC-PW-21b: Hit when PM SLIPPING toward target lane.
    //
    // PM current=Center(2), PMTarget=Right(3) = wave TargetLane → bHitSlipping.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("hit_slipping_toward_target"))
    {
        APullWaveSubsystemActor* Actor =
            NewObject<APullWaveSubsystemActor>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC4: actor created"), Actor)) return false;
        Actor->AddToRoot();
        ON_SCOPE_EXIT { Actor->RemoveFromRoot(); };
        Actor->InitializePool();

        FTestPMProvider PMStub;
        PMStub.CurrentLane  = EPlayerLane::Center;  // ordinal 2 (not target)
        PMStub.PMTargetLane = EPlayerLane::Right;   // ordinal 3 = wave TargetLane
        PMStub.MovState     = EMovementState::SLIPPING;
        Actor->SetPMProvider(&PMStub);

        FPullWaveInstanceState Wave = MakeWaveS005(EPullWaveState::Traversing, /*src=*/1, /*tgt=*/3);
        Wave.TraverseElapsedS = Wave.TravelDurationS - 0.001f;
        Actor->ActiveWaves.Add(Wave);

        int32 HitCount = 0;
        Actor->OnWaveHit.AddLambda([&](int32, int32, int32, float) { ++HitCount; });

        Actor->Tick(0.016f);

        TestEqual(TEXT("TC4a: CollisionOutcome==Hit (SLIPPING toward target)"),
            Actor->ActiveWaves[0].CollisionOutcome, ECollisionOutcome::Hit);
        TestEqual(TEXT("TC4b: OnWaveHit fired once"), HitCount, 1);
        return true;
    }

    // -----------------------------------------------------------------------
    // TC5 — AC-PW-21c: CleanMiss when PM SETTLED in wrong lane.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("clean_miss_settled_wrong_lane"))
    {
        APullWaveSubsystemActor* Actor =
            NewObject<APullWaveSubsystemActor>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC5: actor created"), Actor)) return false;
        Actor->AddToRoot();
        ON_SCOPE_EXIT { Actor->RemoveFromRoot(); };
        Actor->InitializePool();

        FTestPMProvider PMStub;
        PMStub.CurrentLane = EPlayerLane::FarRight;  // ordinal 4 ≠ wave TargetLane(2)
        PMStub.MovState    = EMovementState::SETTLED;
        Actor->SetPMProvider(&PMStub);

        FPullWaveInstanceState Wave = MakeWaveS005(EPullWaveState::Traversing, /*src=*/0, /*tgt=*/2);
        Wave.TraverseElapsedS = Wave.TravelDurationS - 0.001f;
        Actor->ActiveWaves.Add(Wave);

        int32 HitCount      = 0;
        int32 NearMissCount = 0;
        Actor->OnWaveHit.AddLambda([&](int32, int32, int32, float) { ++HitCount;      });
        Actor->OnNearMiss.AddLambda([&](int32, int32, int32)        { ++NearMissCount; });

        Actor->Tick(0.016f);

        TestEqual(TEXT("TC5a: CollisionOutcome==CleanMiss (SETTLED wrong lane)"),
            Actor->ActiveWaves[0].CollisionOutcome, ECollisionOutcome::CleanMiss);
        TestEqual(TEXT("TC5b: OnWaveHit NOT fired"),   HitCount,      0);
        TestEqual(TEXT("TC5c: OnNearMiss NOT fired"), NearMissCount, 0);
        return true;
    }

    // -----------------------------------------------------------------------
    // TC6 — AC-PW-22: NearMiss when PM SLIPPING from target lane.
    //
    // PM in target lane (current=Center=2) slipping AWAY to Right(3).
    // bNearMiss = SLIPPING && currentLane(2)==TargetLane(2).
    // TriggerNearMissBeat() must fire BEFORE OnNearMiss broadcast (control manifest order).
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("near_miss_slipping_from_target"))
    {
        APullWaveSubsystemActor* Actor =
            NewObject<APullWaveSubsystemActor>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC6: actor created"), Actor)) return false;
        Actor->AddToRoot();
        ON_SCOPE_EXIT { Actor->RemoveFromRoot(); };
        Actor->InitializePool();

        FTestPMProvider PMStub;
        PMStub.CurrentLane  = EPlayerLane::Center;  // ordinal 2 = wave TargetLane
        PMStub.PMTargetLane = EPlayerLane::Right;   // ordinal 3 ≠ wave TargetLane → not bHitSlipping
        PMStub.MovState     = EMovementState::SLIPPING;
        Actor->SetPMProvider(&PMStub);

        FPullWaveInstanceState Wave = MakeWaveS005(EPullWaveState::Traversing, /*src=*/0, /*tgt=*/2);
        Wave.TraverseElapsedS = Wave.TravelDurationS - 0.001f;
        Actor->ActiveWaves.Add(Wave);

        int32 NearMissWaveId  = -1;
        int32 NearMissTgtLane = -1;
        int32 SlippedFromLane = -1;
        bool  bBeatBeforeBcast = false;

        Actor->OnNearMiss.AddLambda([&](int32 WId, int32 TL, int32 FromLane)
        {
            // TriggerNearMissBeat must have already incremented when this lambda fires.
            bBeatBeforeBcast = (PMStub.NearMissBeatCount > 0);
            NearMissWaveId   = WId;
            NearMissTgtLane  = TL;
            SlippedFromLane  = FromLane;
        });

        Actor->Tick(0.016f);

        TestEqual(TEXT("TC6a: CollisionOutcome==NearMiss"),
            Actor->ActiveWaves[0].CollisionOutcome, ECollisionOutcome::NearMiss);
        TestEqual(TEXT("TC6b: TriggerNearMissBeat called once"),
            PMStub.NearMissBeatCount, 1);
        TestTrue(TEXT("TC6c: TriggerNearMissBeat called BEFORE OnNearMiss broadcast (control manifest)"),
            bBeatBeforeBcast);
        TestEqual(TEXT("TC6d: OnNearMiss WaveId=1"),            NearMissWaveId,  1);
        TestEqual(TEXT("TC6e: OnNearMiss TargetLane=2"),        NearMissTgtLane, 2);
        TestEqual(TEXT("TC6f: OnNearMiss SlippedFromLane=2 (PM.GetCurrentLane())"),
            SlippedFromLane, 2);
        return true;
    }

    // -----------------------------------------------------------------------
    // TC7 — AC-PW-28: two waves land same tick in same target lane.
    //
    // Each wave resolved independently; OnWaveHit fires once per wave.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("concurrent_landing_same_lane"))
    {
        APullWaveSubsystemActor* Actor =
            NewObject<APullWaveSubsystemActor>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC7: actor created"), Actor)) return false;
        Actor->AddToRoot();
        ON_SCOPE_EXIT { Actor->RemoveFromRoot(); };
        Actor->InitializePool();

        FTestPMProvider PMStub;
        PMStub.CurrentLane = EPlayerLane::Center;   // ordinal 2 = both waves' TargetLane
        PMStub.MovState    = EMovementState::SETTLED;
        Actor->SetPMProvider(&PMStub);

        FPullWaveInstanceState Wave1 = MakeWaveS005(EPullWaveState::Traversing, /*src=*/0, /*tgt=*/2);
        Wave1.WaveId          = 1;
        Wave1.TraverseElapsedS = Wave1.TravelDurationS - 0.001f;
        Actor->ActiveWaves.Add(Wave1);

        FPullWaveInstanceState Wave2 = MakeWaveS005(EPullWaveState::Traversing, /*src=*/4, /*tgt=*/2);
        Wave2.WaveId          = 2;
        Wave2.TraverseElapsedS = Wave2.TravelDurationS - 0.001f;
        Actor->ActiveWaves.Add(Wave2);

        TArray<int32> FiredWaveIds;
        Actor->OnWaveHit.AddLambda([&](int32 WId, int32, int32, float)
        {
            FiredWaveIds.Add(WId);
        });

        Actor->Tick(0.016f);

        TestEqual(TEXT("TC7a: Wave1 State==LANDED"),
            Actor->ActiveWaves[0].State, EPullWaveState::Landed);
        TestEqual(TEXT("TC7b: Wave2 State==LANDED"),
            Actor->ActiveWaves[1].State, EPullWaveState::Landed);
        TestEqual(TEXT("TC7c: Wave1 CollisionOutcome==Hit"),
            Actor->ActiveWaves[0].CollisionOutcome, ECollisionOutcome::Hit);
        TestEqual(TEXT("TC7d: Wave2 CollisionOutcome==Hit"),
            Actor->ActiveWaves[1].CollisionOutcome, ECollisionOutcome::Hit);
        TestEqual(TEXT("TC7e: OnWaveHit fired twice (once per wave)"),
            FiredWaveIds.Num(), 2);
        TestTrue(TEXT("TC7f: OnWaveHit contains WaveId=1"), FiredWaveIds.Contains(1));
        TestTrue(TEXT("TC7g: OnWaveHit contains WaveId=2"), FiredWaveIds.Contains(2));
        return true;
    }

    // -----------------------------------------------------------------------
    // TC8 — AC-PW-31: large DeltaTime overshoot clamped at TravelDurationS.
    //
    // ResolveLandedEntry must clamp TraverseElapsedS = Min(elapsed, TravelDurationS).
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("overshoot_hitch_landing"))
    {
        APullWaveSubsystemActor* Actor =
            NewObject<APullWaveSubsystemActor>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC8: actor created"), Actor)) return false;
        Actor->AddToRoot();
        ON_SCOPE_EXIT { Actor->RemoveFromRoot(); };
        Actor->InitializePool();

        FTestPMProvider PMStub;
        PMStub.CurrentLane = EPlayerLane::FarLeft;   // ordinal 0 ≠ 2 → CleanMiss (outcome not the focus)
        PMStub.MovState    = EMovementState::SETTLED;
        Actor->SetPMProvider(&PMStub);

        FPullWaveInstanceState Wave = MakeWaveS005(EPullWaveState::Traversing, /*src=*/0, /*tgt=*/2);
        Wave.TraverseElapsedS      = Wave.TravelDurationS - 0.001f;
        const float TravelDur      = Wave.TravelDurationS;
        Actor->ActiveWaves.Add(Wave);

        // 1.0s DeltaTime far overshoots TravelDurationS → ResolveLandedEntry must clamp.
        Actor->Tick(1.0f);

        TestEqual(TEXT("TC8a: State==LANDED after large-DeltaTime overshoot"),
            Actor->ActiveWaves[0].State, EPullWaveState::Landed);
        TestTrue(TEXT("TC8b: TraverseElapsedS clamped to TravelDurationS (AC-PW-31)"),
            FMath::Abs(Actor->ActiveWaves[0].TraverseElapsedS - TravelDur) < Eps);
        return true;
    }

    // -----------------------------------------------------------------------
    // TC9 — OnLeanProgress does NOT fire during LANDED state.
    //
    // Wave placed directly in LANDED. Tick() dispatches AdvanceLanded(), which
    // does not call AdvanceLeaning(). Confirms no cross-state event leakage.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("no_lean_progress_during_landed"))
    {
        APullWaveSubsystemActor* Actor =
            NewObject<APullWaveSubsystemActor>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC9: actor created"), Actor)) return false;
        Actor->AddToRoot();
        ON_SCOPE_EXIT { Actor->RemoveFromRoot(); };
        Actor->InitializePool();

        FPullWaveInstanceState Wave = MakeWaveS005(EPullWaveState::Landed, /*src=*/0, /*tgt=*/2);
        Wave.CollisionOutcome   = ECollisionOutcome::CleanMiss;
        Wave.LandedHoldElapsedS = 0.0f;
        Actor->ActiveWaves.Add(Wave);

        int32 LeanProgressFires = 0;
        Actor->OnLeanProgress.AddLambda([&](const FLeanTickData&) { ++LeanProgressFires; });

        for (int32 i = 0; i < 5; ++i) { Actor->Tick(0.016f); }

        TestEqual(TEXT("TC9: OnLeanProgress NOT fired during LANDED state"),
            LeanProgressFires, 0);
        return true;
    }

    AddError(FString::Printf(TEXT("Unknown test command: %s"), *Parameters));
    return false;
}

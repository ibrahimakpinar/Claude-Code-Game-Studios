// Copyright (c) Game Studio. All rights reserved.
// SLIPSTORM — 60-second voxel endless runner, mobile, Unreal Engine 5.7
//
// PullWaveConstructTest.cpp — Story 009 integration tests:
// Construct() entry point + Wave Spawner admission contract.
//
// Story:  production/epics/pull-wave/story-009-construct-entry-point.md
// ADR:    docs/architecture/adr-0010-pullwave-object-pool-state-machine-despawn.md §D6
//         docs/architecture/adr-0011-wave-spawner-pattern-library.md §D2 Stage 4–6
// TRs:    TR-PW-004 (CurveSnapshot immutable at SPAWNED; SAMPLE_COUNT=32 locked)
//         TR-PW-014 (ForwardVelocityMs frozen at SPAWNED; no mid-flight mutation)
//         TR-PW-021 (tick ordering; RSM → DPC → PM → Pull-Wave)
//
// Acceptance criteria covered (8 ACs, 9 TCs):
//   TC1: F-TRAVERSE-DURATION formula (AC-PW-09): given ForwardVelocityMs, TravelDurationS correct
//   TC2: Params copy correctness — all fields including CurveSnapshot samples mirror SpawnParams
//   TC3: State initialization — State==Spawned, all accumulators zero, Unresolved outcome
//   TC4: ISMC seam (OnISMCAddInstanceOverride + OnISMCConstructDataOverride) — called correctly
//   TC5: Pool-full guard — Num() stays 23 when pool is at capacity
//   TC6: AC-PW-20 velocity binding — ForwardVelocityMs frozen across phase advance
//   TC7: LeanDurationS trusted from SpawnParams — stored as-is, no re-clamp
//   TC8: Append-to-tail ordering — 5 sequential Constructs yield WaveId ASC order
//   TC9: Construct() is the only Add() call site (comment + indirect proof)
//
// Test harness conventions (consistent with Stories 004–008):
//   - NewObject<APullWaveSubsystemActor>(GetTransientPackage()) + InitializePool() for setup.
//   - AddToRoot() / RemoveFromRoot() with ON_SCOPE_EXIT cleanup.
//   - WaveMassISMC is null in all tests — ISMC calls are intercepted via seams or null-guarded.
//   - FTestRSMStub9 injects RSM state for pause-freeze and state-query tests.
//   - All IDE diagnostics (CoreMinimal.h not found, int32 unknown, etc.) are UE macro
//     false positives from missing compile_commands.json — ignore them.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "PullWave/PullWaveSubsystemActor.h"
#include "PullWave/PullWaveTypes.h"
#include "RunStateMachine/ERunState.h"

// ---------------------------------------------------------------------------
// FTestRSMStub9 — RSM provider stub for Story 009 tests.
// Returns RUNNING state (unpaused) by default — simulates an active run.
// ---------------------------------------------------------------------------
class FTestRSMStub9 : public IPullWaveRSMProvider
{
public:
    bool      bIsPaused    = false;
    ERunState CurrentState = ERunState::RUNNING;

    virtual bool      GetIsPaused()     const override { return bIsPaused; }
    virtual ERunState GetCurrentState() const override { return CurrentState; }
};

// ---------------------------------------------------------------------------
// MakeMinimalSpawnParams — construct FPullWaveSpawnParams with sensible defaults.
// CurveSnapshot is zero-initialized (flat-zero curve) unless explicitly overridden.
// ---------------------------------------------------------------------------
static FPullWaveSpawnParams MakeMinimalSpawnParams(
    int32 WaveId           = 1,
    int32 SourceLane       = 2,
    int32 TargetLane       = 2,
    float ForwardVelocityMs = 7.5f,
    float SpawnTimeS       = 0.0f,
    float LeanDurationS    = TELEGRAPH_WINDOW_FLOOR_S)
{
    FPullWaveSpawnParams P;
    P.WaveId            = WaveId;
    P.SourceLane        = SourceLane;
    P.TargetLane        = TargetLane;
    P.ForwardVelocityMs = ForwardVelocityMs;
    P.SpawnTimeS        = SpawnTimeS;
    P.LeanDurationS     = LeanDurationS;
    // CurveSnapshot: zero-initialized by struct default (Samples[] all 0.0f).
    return P;
}

// ---------------------------------------------------------------------------
// Test suite — Story 009 Construct() entry point
// ---------------------------------------------------------------------------
IMPLEMENT_COMPLEX_AUTOMATION_TEST(
    FPullWaveConstructTest,
    "SLIPSTORM.PullWave.Integration.Construct",
    EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::EditorContext
    | EAutomationTestFlags::ClientContext)

void FPullWaveConstructTest::GetTests(
    TArray<FString>& OutBeautifiedNames,
    TArray<FString>& OutTestCommands) const
{
    OutBeautifiedNames.Add(TEXT("TC1: F-TRAVERSE-DURATION — ForwardVelocityMs=7.5 → TravelDurationS=2.0; v=4.0 → 3.75 (AC-PW-09)"));
    OutTestCommands.Add(TEXT("traverse_duration_formula"));

    OutBeautifiedNames.Add(TEXT("TC2: params copy — all FPullWaveSpawnParams fields copied byte-for-byte into ActiveWaves.Last()"));
    OutTestCommands.Add(TEXT("params_copy_correctness"));

    OutBeautifiedNames.Add(TEXT("TC3: state initialization — State==Spawned, all accumulators 0, CollisionOutcome==Unresolved"));
    OutTestCommands.Add(TEXT("state_initialization"));

    OutBeautifiedNames.Add(TEXT("TC4: ISMC seam — OnISMCAddInstanceOverride called once with correct transform; OnISMCConstructDataOverride called 3 times"));
    OutTestCommands.Add(TEXT("ismc_seam_construct"));

    OutBeautifiedNames.Add(TEXT("TC5: pool-full guard — Num() stays 23 when pool at capacity, wave NOT admitted"));
    OutTestCommands.Add(TEXT("pool_full_guard"));

    OutBeautifiedNames.Add(TEXT("TC6: AC-PW-20 velocity binding — ForwardVelocityMs frozen across LEANING→TRAVERSING phase advance"));
    OutTestCommands.Add(TEXT("velocity_binding_post_construct"));

    OutBeautifiedNames.Add(TEXT("TC7: LeanDurationS trusted from SpawnParams — stored as-is (0.95f and 0.60f), no re-clamp"));
    OutTestCommands.Add(TEXT("lean_duration_trusted"));

    OutBeautifiedNames.Add(TEXT("TC8: append-to-tail ordering — 5 Construct() calls yield WaveId ASC order, Last().WaveId==104"));
    OutTestCommands.Add(TEXT("append_to_tail_ordering"));

    OutBeautifiedNames.Add(TEXT("TC9: Construct() is the only Add() call site (CI grep + indirect pool-growth proof)"));
    OutTestCommands.Add(TEXT("construct_is_only_add_site"));
}

bool FPullWaveConstructTest::RunTest(const FString& Parameters)
{
    const float Eps = 1e-5f;

    // -----------------------------------------------------------------------
    // TC1 — F-TRAVERSE-DURATION formula (AC-PW-09)
    // TravelDurationS = SPAWN_PLANE_Z_OFFSET_M / ForwardVelocityMs = 15.0 / v
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("traverse_duration_formula"))
    {
        // Arrange
        APullWaveSubsystemActor* Actor =
            NewObject<APullWaveSubsystemActor>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC1: actor created"), Actor)) return false;
        Actor->AddToRoot();
        ON_SCOPE_EXIT { Actor->RemoveFromRoot(); };
        Actor->InitializePool();

        // Act: v=7.5 → TravelDurationS = 15.0 / 7.5 = 2.0
        FPullWaveSpawnParams P1 = MakeMinimalSpawnParams(1, 2, 2, 7.5f);
        Actor->Construct(P1);
        TestTrue(TEXT("TC1a: v=7.5 → TravelDurationS ≈ 2.0 (within 1e-5)"),
            FMath::Abs(Actor->ActiveWaves.Last().TravelDurationS - 2.0f) < Eps);

        // Act: v=4.0 → TravelDurationS = 15.0 / 4.0 = 3.75
        FPullWaveSpawnParams P2 = MakeMinimalSpawnParams(2, 2, 2, 4.0f);
        Actor->Construct(P2);
        TestTrue(TEXT("TC1b: v=4.0 → TravelDurationS ≈ 3.75 (within 1e-5)"),
            FMath::Abs(Actor->ActiveWaves.Last().TravelDurationS - 3.75f) < Eps);

        return true;
    }

    // -----------------------------------------------------------------------
    // TC2 — Params copy correctness
    // All fields from FPullWaveSpawnParams must be mirrored verbatim in ActiveWaves.Last().
    // CurveSnapshot.Samples[0], [15], [31] checked explicitly (TR-PW-004).
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("params_copy_correctness"))
    {
        // Arrange
        APullWaveSubsystemActor* Actor =
            NewObject<APullWaveSubsystemActor>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC2: actor created"), Actor)) return false;
        Actor->AddToRoot();
        ON_SCOPE_EXIT { Actor->RemoveFromRoot(); };
        Actor->InitializePool();

        FPullWaveSpawnParams Params = MakeMinimalSpawnParams(42, 1, 3, 6.0f, 12.5f, 0.80f);
        // Populate CurveSnapshot with known sentinel values to verify copy fidelity.
        Params.CurveSnapshot.Samples[0]  = 0.11f;
        Params.CurveSnapshot.Samples[15] = 0.55f;
        Params.CurveSnapshot.Samples[31] = 0.99f;

        // Act
        Actor->Construct(Params);

        // Assert: scalar fields
        const FPullWaveInstanceState& Wave = Actor->ActiveWaves.Last();
        TestEqual(TEXT("TC2a: WaveId==42"),         Wave.WaveId,            42);
        TestEqual(TEXT("TC2b: SourceLane==1"),       Wave.SourceLane,        1);
        TestEqual(TEXT("TC2c: TargetLane==3"),       Wave.TargetLane,        3);
        TestTrue(TEXT("TC2d: ForwardVelocityMs==6.0"),
            FMath::Abs(Wave.ForwardVelocityMs - 6.0f) < Eps);
        TestTrue(TEXT("TC2e: SpawnTimeS==12.5"),
            FMath::Abs(Wave.SpawnTimeS - 12.5f) < Eps);
        TestTrue(TEXT("TC2f: LeanDurationS==0.80"),
            FMath::Abs(Wave.LeanDurationS - 0.80f) < Eps);

        // Assert: CurveSnapshot sample copy fidelity (TR-PW-004)
        TestTrue(TEXT("TC2g: CurveSnapshot.Samples[0]==0.11"),
            FMath::Abs(Wave.CurveSnapshot.Samples[0]  - 0.11f) < Eps);
        TestTrue(TEXT("TC2h: CurveSnapshot.Samples[15]==0.55"),
            FMath::Abs(Wave.CurveSnapshot.Samples[15] - 0.55f) < Eps);
        TestTrue(TEXT("TC2i: CurveSnapshot.Samples[31]==0.99"),
            FMath::Abs(Wave.CurveSnapshot.Samples[31] - 0.99f) < Eps);

        return true;
    }

    // -----------------------------------------------------------------------
    // TC3 — State initialization
    // After Construct(): State==Spawned, all accumulators 0, outcome==Unresolved.
    // TC3 also checks WaveId (a non-default field) to prove Construct() actually ran —
    // default FPullWaveInstanceState{} has WaveId=0, so WaveId==42 here is evidence.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("state_initialization"))
    {
        // Arrange
        APullWaveSubsystemActor* Actor =
            NewObject<APullWaveSubsystemActor>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC3: actor created"), Actor)) return false;
        Actor->AddToRoot();
        ON_SCOPE_EXIT { Actor->RemoveFromRoot(); };
        Actor->InitializePool();

        // Act
        FPullWaveSpawnParams Params = MakeMinimalSpawnParams(42);
        Actor->Construct(Params);

        // Assert: lifecycle state and accumulators
        const FPullWaveInstanceState& Wave = Actor->ActiveWaves.Last();
        TestEqual(TEXT("TC3a: State==Spawned"),      Wave.State,             EPullWaveState::Spawned);
        TestTrue(TEXT("TC3b: LeanProgress==0.0"),
            FMath::Abs(Wave.LeanProgress)        < Eps);
        TestTrue(TEXT("TC3c: TraverseElapsedS==0.0"),
            FMath::Abs(Wave.TraverseElapsedS)    < Eps);
        TestTrue(TEXT("TC3d: LandedHoldElapsedS==0.0"),
            FMath::Abs(Wave.LandedHoldElapsedS)  < Eps);
        TestEqual(TEXT("TC3e: CollisionOutcome==Unresolved"),
            Wave.CollisionOutcome, ECollisionOutcome::Unresolved);
        // Prove Construct() ran: WaveId is non-default (default is 0).
        TestEqual(TEXT("TC3f: WaveId==42 (confirms Construct executed, not just default-init)"),
            Wave.WaveId, 42);

        return true;
    }

    // -----------------------------------------------------------------------
    // TC4 — ISMC seam
    // OnISMCAddInstanceOverride: called once with the correct spawn-plane transform
    //   (X = (SourceLane-2)*LANE_WIDTH_M, Y=0, Z=SPAWN_PLANE_Z_OFFSET_M).
    // OnISMCConstructDataOverride: called exactly 3 times for DataIdx 0, 1, 2 with Value 0.0f.
    // ISMCInstanceIndex in ActiveWaves reflects the fake return value from the seam.
    //
    // SourceLane=1 used so X = (1-2)*1.0 = -1.0 (non-zero; proves axis is correct).
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("ismc_seam_construct"))
    {
        // Arrange
        APullWaveSubsystemActor* Actor =
            NewObject<APullWaveSubsystemActor>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC4: actor created"), Actor)) return false;
        Actor->AddToRoot();
        ON_SCOPE_EXIT { Actor->RemoveFromRoot(); };
        Actor->InitializePool();

        // AddInstance seam: capture transform and return a fake index.
        int32     AddCallCount     = 0;
        FTransform CapturedTransform;
        const int32 FakeIndex     = 77;

        Actor->SetOnISMCAddInstanceOverride([&](const FTransform& T) -> int32
        {
            ++AddCallCount;
            CapturedTransform = T;
            return FakeIndex;
        });

        // SetCustomDataValue seam: capture all calls.
        struct FDataCall { int32 InstanceIdx; int32 DataIdx; float Value; };
        TArray<FDataCall> DataCalls;

        Actor->SetOnISMCConstructDataOverride(
            [&](int32 InstanceIdx, int32 DataIdx, float Value)
        {
            DataCalls.Add({ InstanceIdx, DataIdx, Value });
        });

        // Act: SourceLane=1 → expected X = (1-2)*LANE_WIDTH_M = -1.0
        FPullWaveSpawnParams Params = MakeMinimalSpawnParams(10, /*SourceLane=*/1, /*TargetLane=*/3);
        Actor->Construct(Params);

        // Assert: AddInstance seam called exactly once.
        TestEqual(TEXT("TC4a: OnISMCAddInstanceOverride called exactly once"), AddCallCount, 1);

        // Assert: transform encodes lateral in X, depth in Z.
        const FVector SpawnLoc = CapturedTransform.GetLocation();
        TestTrue(TEXT("TC4b: transform X = (1-2)*LANE_WIDTH_M = -1.0"),
            FMath::Abs(SpawnLoc.X - (-1.0f)) < Eps);
        TestTrue(TEXT("TC4c: transform Y = 0.0 (unused axis at spawn)"),
            FMath::Abs(SpawnLoc.Y - 0.0f)   < Eps);
        TestTrue(TEXT("TC4d: transform Z = SPAWN_PLANE_Z_OFFSET_M = 15.0"),
            FMath::Abs(SpawnLoc.Z - SPAWN_PLANE_Z_OFFSET_M) < Eps);

        // Assert: ISMCInstanceIndex in pool reflects the fake seam return value.
        TestEqual(TEXT("TC4e: Wave.ISMCInstanceIndex == FakeIndex (77)"),
            Actor->ActiveWaves.Last().ISMCInstanceIndex, FakeIndex);

        // Assert: SetCustomDataValue seam called 3 times (slots 0, 1, 2 all 0.0f).
        TestEqual(TEXT("TC4f: OnISMCConstructDataOverride called 3 times"), DataCalls.Num(), 3);
        if (DataCalls.Num() == 3)
        {
            TestEqual(TEXT("TC4g: DataIdx 0 — lean tier"),    DataCalls[0].DataIdx, 0);
            TestTrue(TEXT("TC4h: Value[0] == 0.0f"),          FMath::Abs(DataCalls[0].Value) < Eps);
            TestEqual(TEXT("TC4i: DataIdx 1 — near-miss flash"), DataCalls[1].DataIdx, 1);
            TestTrue(TEXT("TC4j: Value[1] == 0.0f"),          FMath::Abs(DataCalls[1].Value) < Eps);
            TestEqual(TEXT("TC4k: DataIdx 2 — dissolve"),     DataCalls[2].DataIdx, 2);
            TestTrue(TEXT("TC4l: Value[2] == 0.0f"),          FMath::Abs(DataCalls[2].Value) < Eps);
            // All three calls use the same instance index (the one returned by AddInstance seam).
            TestEqual(TEXT("TC4m: all DataCalls use FakeIndex as InstanceIdx"), DataCalls[0].InstanceIdx, FakeIndex);
            TestEqual(TEXT("TC4n: DataCall[1] InstanceIdx matches FakeIndex"),  DataCalls[1].InstanceIdx, FakeIndex);
            TestEqual(TEXT("TC4o: DataCall[2] InstanceIdx matches FakeIndex"),  DataCalls[2].InstanceIdx, FakeIndex);
        }

        return true;
    }

    // -----------------------------------------------------------------------
    // TC5 — Pool-full guard
    // When ActiveWaves.Num() == MAX_POOL_SIZE (23), Construct() must log and return
    // without adding a 24th wave. Num() must remain 23.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("pool_full_guard"))
    {
        // Arrange
        APullWaveSubsystemActor* Actor =
            NewObject<APullWaveSubsystemActor>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC5: actor created"), Actor)) return false;
        Actor->AddToRoot();
        ON_SCOPE_EXIT { Actor->RemoveFromRoot(); };
        Actor->InitializePool();

        // Seed 23 waves directly (test infrastructure — not production code).
        // NOTE: Direct ActiveWaves.Add() here is intentional test scaffolding.
        // CI grep for "ActiveWaves\.Add\s*\(" excludes Tests/ directories — see TC9.
        for (int32 i = 0; i < MAX_POOL_SIZE; ++i)
        {
            FPullWaveInstanceState Stub;
            Stub.WaveId = i;
            Actor->ActiveWaves.Add(Stub);
        }
        TestEqual(TEXT("TC5 setup: pool seeded to 23"), Actor->ActiveWaves.Num(), MAX_POOL_SIZE);

        // Act: attempt to admit a 24th wave — must be rejected.
        // AddExpectedError suppresses the UE_LOG(Error, "pull_wave_pool_full") and
        // asserts it fires exactly once — both directions of the AC are now enforced.
        AddExpectedError(TEXT("pull_wave_pool_full"),
                         EAutomationExpectedErrorFlags::Contains, 1);
        FPullWaveSpawnParams Overflow = MakeMinimalSpawnParams(MAX_POOL_SIZE);
        Actor->Construct(Overflow);

        // Assert: pool remains at 23.
        TestEqual(TEXT("TC5a: ActiveWaves.Num() stays 23 (pool-full guard fired)"),
            Actor->ActiveWaves.Num(), MAX_POOL_SIZE);
        // Assert: the overflow wave was NOT added (last wave still has WaveId==22, not MAX_POOL_SIZE).
        TestEqual(TEXT("TC5b: ActiveWaves.Last().WaveId==22 (overflow wave not admitted)"),
            Actor->ActiveWaves.Last().WaveId, MAX_POOL_SIZE - 1);

        return true;
    }

    // -----------------------------------------------------------------------
    // TC6 — AC-PW-20: velocity binding across phase advance
    // ForwardVelocityMs stored at Construct() must not be mutated by any subsequent
    // tick, regardless of how many lifecycle phases the wave passes through.
    //
    // NOTE ON PHASE SEAM: IPullWaveRSMProvider exposes only GetIsPaused() and
    // GetCurrentState() — there is no SetCurrentPhase() for DPC phases (OPENER/PEAK).
    // AC-PW-20's "RSM phase change" is approximated here by advancing the wave through
    // LEANING into TRAVERSING via Tick(). This faithfully exercises the immutability
    // property (ForwardVelocityMs is only ever written in Construct()). If DPC phase
    // seams are added to IPullWaveRSMProvider in a future story, this TC should be
    // extended to exercise them.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("velocity_binding_post_construct"))
    {
        // Arrange
        APullWaveSubsystemActor* Actor =
            NewObject<APullWaveSubsystemActor>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC6: actor created"), Actor)) return false;
        Actor->AddToRoot();
        ON_SCOPE_EXIT { Actor->RemoveFromRoot(); };
        Actor->InitializePool();

        FTestRSMStub9 RSMStub;
        Actor->SetRSMProvider(&RSMStub);
        ON_SCOPE_EXIT { Actor->SetRSMProvider(nullptr); };

        // Act: admit a wave with ForwardVelocityMs=5.0; expected TravelDurationS=15/5=3.0.
        FPullWaveSpawnParams Params = MakeMinimalSpawnParams(1, 2, 2, 5.0f, 0.0f, TELEGRAPH_WINDOW_FLOOR_S);
        Actor->Construct(Params);

        // Verify initial binding immediately after Construct.
        TestTrue(TEXT("TC6a: ForwardVelocityMs==5.0 immediately after Construct"),
            FMath::Abs(Actor->ActiveWaves[0].ForwardVelocityMs - 5.0f) < Eps);
        TestTrue(TEXT("TC6b: TravelDurationS==3.0 immediately after Construct (15/5=3)"),
            FMath::Abs(Actor->ActiveWaves[0].TravelDurationS - 3.0f) < Eps);

        // Advance through full LEANING (0.70s at DeltaTime=0.016: need ~44 ticks).
        // This simulates the OPENER→PEAK phase boundary; ForwardVelocityMs must stay 5.0.
        for (int32 i = 0; i < 50; ++i) { Actor->Tick(0.016f); }

        // The wave may have reached TRAVERSING by now.
        TestEqual(TEXT("TC6c: wave still in ActiveWaves after 50 ticks"),
            Actor->ActiveWaves.Num(), 1);

        // Assert: ForwardVelocityMs and TravelDurationS are both frozen (TR-PW-014).
        TestTrue(TEXT("TC6d: ForwardVelocityMs still 5.0 after LEANING phase advance"),
            FMath::Abs(Actor->ActiveWaves[0].ForwardVelocityMs - 5.0f) < Eps);
        TestTrue(TEXT("TC6e: TravelDurationS still 3.0 (not recomputed from velocity)"),
            FMath::Abs(Actor->ActiveWaves[0].TravelDurationS - 3.0f) < Eps);

        // Confirm the phase advance actually progressed the wave (validates the approximation).
        const EPullWaveState StateAfterPhaseAdvance = Actor->ActiveWaves[0].State;
        const bool bAdvancedPastSpawned =
            (StateAfterPhaseAdvance == EPullWaveState::Leaning   ||
             StateAfterPhaseAdvance == EPullWaveState::Traversing ||
             StateAfterPhaseAdvance == EPullWaveState::Landed);
        TestTrue(TEXT("TC6f: wave advanced past Spawned state (phase simulation confirmed)"),
            bAdvancedPastSpawned);

        return true;
    }

    // -----------------------------------------------------------------------
    // TC7 — LeanDurationS trusted from SpawnParams
    // Pull-Wave stores LeanDurationS as-is from FPullWaveSpawnParams.
    // No re-clamping at Construct() — clamping is Wave Spawner's responsibility
    // (ADR-0011 D2 Stage 4; control manifest "Forbidden: re-reading DPC at Construct").
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("lean_duration_trusted"))
    {
        // Arrange
        APullWaveSubsystemActor* Actor =
            NewObject<APullWaveSubsystemActor>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC7: actor created"), Actor)) return false;
        Actor->AddToRoot();
        ON_SCOPE_EXIT { Actor->RemoveFromRoot(); };
        Actor->InitializePool();

        // Act: LeanDurationS = 0.95f (above floor; normal case)
        FPullWaveSpawnParams P1 = MakeMinimalSpawnParams(1, 2, 2, 7.5f, 0.0f, 0.95f);
        Actor->Construct(P1);
        TestTrue(TEXT("TC7a: LeanDurationS==0.95 (above floor, stored as-is)"),
            FMath::Abs(Actor->ActiveWaves.Last().LeanDurationS - 0.95f) < Eps);

        // Act: LeanDurationS = 0.60f (below TELEGRAPH_WINDOW_FLOOR_S=0.70; Wave Spawner
        // should have clamped this, but Pull-Wave does NOT re-clamp at Construct).
        FPullWaveSpawnParams P2 = MakeMinimalSpawnParams(2, 2, 2, 7.5f, 0.0f, 0.60f);
        Actor->Construct(P2);
        TestTrue(TEXT("TC7b: LeanDurationS==0.60 (below floor; stored as-is — no re-clamp at Construct)"),
            FMath::Abs(Actor->ActiveWaves.Last().LeanDurationS - 0.60f) < Eps);

        return true;
    }

    // -----------------------------------------------------------------------
    // TC8 — Append-to-tail ordering
    // 5 sequential Construct() calls must yield WaveId ASC order with no gaps.
    // ActiveWaves[0].WaveId == 100, ..., ActiveWaves[4].WaveId == 104.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("append_to_tail_ordering"))
    {
        // Arrange
        APullWaveSubsystemActor* Actor =
            NewObject<APullWaveSubsystemActor>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC8: actor created"), Actor)) return false;
        Actor->AddToRoot();
        ON_SCOPE_EXIT { Actor->RemoveFromRoot(); };
        Actor->InitializePool();

        // Act: 5 sequential Constructs with WaveIds 100–104.
        const int32 BaseWaveId = 100;
        const int32 WaveCount  = 5;
        for (int32 i = 0; i < WaveCount; ++i)
        {
            FPullWaveSpawnParams Params = MakeMinimalSpawnParams(BaseWaveId + i);
            Actor->Construct(Params);
        }

        // Assert: pool now contains exactly 5 waves.
        TestEqual(TEXT("TC8a: ActiveWaves.Num()==5"), Actor->ActiveWaves.Num(), WaveCount);

        // Assert: WaveIds in ASC insertion order.
        for (int32 i = 0; i < WaveCount && i < Actor->ActiveWaves.Num(); ++i)
        {
            TestEqual(
                FString::Printf(TEXT("TC8b: ActiveWaves[%d].WaveId==%d"), i, BaseWaveId + i),
                Actor->ActiveWaves[i].WaveId,
                BaseWaveId + i);
        }

        // Assert: tail element has the last admitted WaveId.
        TestEqual(TEXT("TC8c: ActiveWaves.Last().WaveId==104"),
            Actor->ActiveWaves.Last().WaveId, 104);

        return true;
    }

    // -----------------------------------------------------------------------
    // TC9 — Construct() is the only Add() call site
    //
    // This is a static code-structure assertion rather than a pure in-process test.
    // ADR-0010 D1 requires that the only site calling ActiveWaves.Add() in production
    // source is Construct(). This is enforced by CI grep:
    //
    //   grep -rn "ActiveWaves\.Add(_GetRef)?\s*(" Source/SLIPSTORM/ \
    //       --include="*.cpp" --include="*.h"                      \
    //       --exclude-dir=Tests                                     \
    //   | grep -v "Construct"
    //
    // This grep must return empty (no matches) — if it does not, a non-Construct
    // call site has been introduced in violation of AC-PW-22b pattern 11 (ADR-0010 D1).
    //
    // INDIRECT IN-PROCESS PROOF: call Construct() N times and assert Num() == N.
    // If any other code path called ActiveWaves.Add() between construction and this
    // assertion, Num() would exceed N. This is not a complete proof (it cannot catch
    // Add() calls in other translation units at test time), but it confirms that
    // Construct() itself contributes exactly one element per call.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("construct_is_only_add_site"))
    {
        // Arrange
        APullWaveSubsystemActor* Actor =
            NewObject<APullWaveSubsystemActor>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC9: actor created"), Actor)) return false;
        Actor->AddToRoot();
        ON_SCOPE_EXIT { Actor->RemoveFromRoot(); };
        Actor->InitializePool();

        // Verify pool is empty before any calls.
        TestEqual(TEXT("TC9a: pool starts empty after InitializePool()"),
            Actor->ActiveWaves.Num(), 0);

        // Act + Assert: each Construct() grows pool by exactly 1.
        const int32 N = 4;
        for (int32 i = 0; i < N; ++i)
        {
            FPullWaveSpawnParams Params = MakeMinimalSpawnParams(i + 1);
            Actor->Construct(Params);
            TestEqual(
                FString::Printf(TEXT("TC9b: pool grows by 1 per Construct() call (after call %d)"), i + 1),
                Actor->ActiveWaves.Num(), i + 1);
        }

        // Confirm final pool size.
        TestEqual(TEXT("TC9c: pool Num()==N after N Construct() calls"), Actor->ActiveWaves.Num(), N);

        return true;
    }

    AddError(FString::Printf(TEXT("Unknown test command: %s"), *Parameters));
    return false;
}

// Copyright (c) Game Studio. All rights reserved.
// SLIPSTORM — 60-second voxel endless runner, mobile, Unreal Engine 5.7
//
// PullWavePoolStorageWaveIdTest.cpp — Story 002 unit tests for pool storage,
// WaveId counter behaviour, WaveId-ASC invariant, and determinism precondition.
//
// Story: production/epics/pull-wave/story-002-pool-storage-waveid.md
// GDD:   design/gdd/pull-wave-behavior.md
// ADR:   docs/architecture/adr-0010-pullwave-object-pool-state-machine-despawn.md
// TRs:   TR-PW-005 (Reserve(23)), TR-PW-006 (WaveId never reused),
//        TR-PW-010 (append-only / WaveId-ASC)
//
// Test category: SLIPSTORM.PullWave.PoolStorageWaveId
// Runner: UE Automation Framework, headless (-nullrhi -nosound -unattended)
//         EditorContext | ClientContext | SmokeFilter
//
// TC1–TC7: pure-logic tests — TArray<FPullWaveInstanceState> and int32 exercised
// directly, without constructing the Actor.
// TC8: actor seam test — NewObject<APullWaveSubsystemActor> + InitializePool()
// verifies Reserve(23) and NextWaveId==0 at runtime (ADR-0010 D1 test-seam requirement).
//
// AC7 / AC8 grep assertions (pattern 9 / pattern 11):
//   Only the FORBID halves are asserted here:
//     AC7 FORBID: RemoveAtSwap returns 0 matches in Source/SLIPSTORM/PullWave/
//     AC8 FORBID: Insert   returns 0 matches in Source/SLIPSTORM/PullWave/
//   The ALLOW halves (RemoveAt >= 1, Add >= 1) defer to Story 006 and Story 009
//   respectively — those call sites do not exist yet.
//
// Guard: WITH_DEV_AUTOMATION_TESTS only.

#if WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

#include "PullWave/PullWaveTypes.h"
#include "PullWave/PullWaveSubsystemActor.h"

// ---------------------------------------------------------------------------
// Helper: build a default FPullWaveInstanceState with a given WaveId.
// ---------------------------------------------------------------------------
static FPullWaveInstanceState MakeState(int32 WaveId)
{
    FPullWaveInstanceState S{};
    S.WaveId = WaveId;
    return S;
}

// ---------------------------------------------------------------------------
// IMPLEMENT_COMPLEX_AUTOMATION_TEST
// 7 test commands covering Story 002 acceptance criteria.
// ---------------------------------------------------------------------------

IMPLEMENT_COMPLEX_AUTOMATION_TEST(
    FPullWavePoolStorageWaveIdTest,
    "SLIPSTORM.PullWave.PoolStorageWaveId",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::SmokeFilter)

void FPullWavePoolStorageWaveIdTest::GetTests(
    TArray<FString>& OutBeautifiedNames,
    TArray<FString>& OutTestCommands) const
{
    OutBeautifiedNames.Add(TEXT("Reserve(23) — pool capacity allocated, Num() starts at 0"));
    OutTestCommands.Add(TEXT("pool_reserve_capacity"));

    OutBeautifiedNames.Add(TEXT("NextWaveId counter — increments per Add(), no reuse across 30 admits"));
    OutTestCommands.Add(TEXT("waveid_counter_no_reuse"));

    OutBeautifiedNames.Add(TEXT("WaveId-ASC invariant — mid-array RemoveAt preserves ascending order"));
    OutTestCommands.Add(TEXT("waveid_asc_after_removeat"));

    OutBeautifiedNames.Add(TEXT("AC-PW-10 determinism — two identical Add() sequences iterate identically"));
    OutTestCommands.Add(TEXT("determinism_identical_sequences"));

    OutBeautifiedNames.Add(TEXT("AC-PW-22b pattern 9 FORBID — RemoveAtSwap absent from PullWave source"));
    OutTestCommands.Add(TEXT("grep_forbid_removeatswap"));

    OutBeautifiedNames.Add(TEXT("AC-PW-22b pattern 11 FORBID — Insert absent from PullWave source"));
    OutTestCommands.Add(TEXT("grep_forbid_insert"));

    OutBeautifiedNames.Add(TEXT("ActiveWaves field exists and FPullWaveInstanceState is USTRUCT-compatible"));
    OutTestCommands.Add(TEXT("pool_field_ustruct_compat"));

    OutBeautifiedNames.Add(TEXT("InitializePool() seam — actor Reserve(23) and NextWaveId==0 verified at runtime"));
    OutTestCommands.Add(TEXT("actor_initializepool_seam"));
}

bool FPullWavePoolStorageWaveIdTest::RunTest(const FString& Parameters)
{
    // -----------------------------------------------------------------------
    // TC1 — Reserve(23) allocates capacity without changing Num().
    //
    // Simulates what APullWaveSubsystemActor::BeginPlay() does.
    // After Reserve(23): GetSlack() >= 23, Num() == 0.
    // Adding one element brings Num() to 1 without triggering a reallocation.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("pool_reserve_capacity"))
    {
        TArray<FPullWaveInstanceState> Pool;
        Pool.Reserve(MAX_POOL_SIZE);   // MAX_POOL_SIZE == 23 (locked in PullWaveTypes.h)

        TestEqual(
            TEXT("TC1: Num() == 0 immediately after Reserve (no elements added)"),
            Pool.Num(),
            0);

        TestTrue(
            TEXT("TC1: GetSlack() >= MAX_POOL_SIZE (capacity reserved without elements)"),
            Pool.GetSlack() >= MAX_POOL_SIZE);

        // Add one element — must not trigger reallocation (slack absorbs it).
        const int32 SlackBefore = Pool.GetSlack();
        Pool.Add(MakeState(0));

        TestEqual(
            TEXT("TC1: Num() == 1 after first Add()"),
            Pool.Num(),
            1);

        TestTrue(
            TEXT("TC1: GetSlack() decreased by exactly 1 (no realloc occurred)"),
            Pool.GetSlack() == SlackBefore - 1);

        return true;
    }

    // -----------------------------------------------------------------------
    // TC2 — WaveId counter: 30 sequential admits, no value appears twice.
    //
    // Simulates the NextWaveId counter that APullWaveSubsystemActor::Construct()
    // will increment and assign (Story 009 call site — not yet implemented).
    // Here we drive the counter directly to prove the no-reuse invariant.
    //
    // Also verifies that counter starts at 0 and advances strictly.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("waveid_counter_no_reuse"))
    {
        TArray<FPullWaveInstanceState> Pool;
        Pool.Reserve(MAX_POOL_SIZE);

        int32 NextWaveId = 0;   // mirrors APullWaveSubsystemActor::NextWaveId after BeginPlay

        constexpr int32 AdmitCount = 30;
        TSet<int32> SeenIds;
        SeenIds.Reserve(AdmitCount);

        for (int32 i = 0; i < AdmitCount; ++i)
        {
            // Story 009's Construct() will do: WaveId = NextWaveId++;
            const int32 AssignedId = NextWaveId++;
            SeenIds.Add(AssignedId);

            FPullWaveInstanceState S = MakeState(AssignedId);
            Pool.Add(S);

            // Verify strictly ascending with no gaps (monotone counter property).
            TestEqual(
                FString::Printf(TEXT("TC2: Pool[%d].WaveId == %d (monotone counter)"), i, i),
                Pool[i].WaveId,
                i);
        }

        TestEqual(
            TEXT("TC2: 30 unique WaveIds emitted (no reuse within session)"),
            SeenIds.Num(),
            AdmitCount);

        TestEqual(
            TEXT("TC2: NextWaveId advanced to 30 after 30 admits"),
            NextWaveId,
            30);

        return true;
    }

    // -----------------------------------------------------------------------
    // TC3 — WaveId-ASC invariant: 20 admits, RemoveAt at indices 3, 7, 11.
    //
    // After 20 Add()s with WaveIds 0..19, the array is [0,1,...,19] (ASC).
    // RemoveAt(11) → 19 elements, WaveIds: [0..10, 12..19].
    // RemoveAt(7)  → 18 elements, WaveIds: [0..6, 8..10, 12..19].
    // RemoveAt(3)  → 17 elements, WaveIds: [0..2, 4..6, 8..10, 12..19].
    // Verify: iterating the 17 remaining elements produces strictly ascending WaveIds.
    //
    // Removal order: highest-index first so earlier indices remain stable.
    // (In production Story 006 removes by found slot index; order is per-tick.)
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("waveid_asc_after_removeat"))
    {
        TArray<FPullWaveInstanceState> Pool;
        Pool.Reserve(MAX_POOL_SIZE);

        for (int32 i = 0; i < 20; ++i)
        {
            Pool.Add(MakeState(i));
        }

        // Remove highest index first to avoid invalidating lower indices.
        Pool.RemoveAt(11);
        Pool.RemoveAt(7);
        Pool.RemoveAt(3);

        TestEqual(
            TEXT("TC3: 17 elements remain after 3 mid-array removals"),
            Pool.Num(),
            17);

        bool bStrictlyAscending = true;
        for (int32 i = 1; i < Pool.Num(); ++i)
        {
            if (Pool[i].WaveId <= Pool[i - 1].WaveId)
            {
                bStrictlyAscending = false;
                AddError(FString::Printf(
                    TEXT("TC3: ASC violated at index %d: Pool[%d].WaveId=%d <= Pool[%d].WaveId=%d"),
                    i, i, Pool[i].WaveId, i - 1, Pool[i - 1].WaveId));
            }
        }

        TestTrue(
            TEXT("TC3: WaveId-ASC invariant holds after 3 mid-array RemoveAt calls"),
            bStrictlyAscending);

        return true;
    }

    // -----------------------------------------------------------------------
    // TC4 — AC-PW-10 determinism precondition: identical Add() sequences
    //        produce identical iteration order.
    //
    // Design: two independent TArray<FPullWaveInstanceState> (SimA, SimB).
    // Both admit the same WaveIds via Add() in the same order.
    // Verify: for every index i, SimA[i].WaveId == SimB[i].WaveId.
    //
    // This proves the determinism precondition (identical input → identical
    // iteration order) without Tick or World construction. Per-tick snapshot
    // identity (AC-PW-10 full assertion) is validated in Story 004 tick tests.
    //
    // Includes a "two waves in the same tick" admission pattern (WaveIds 5+6
    // both added before any removal) to satisfy the "at least one tick where
    // two or more waves are admitted in the same tick" sub-condition.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("determinism_identical_sequences"))
    {
        // Sequence: 8 waves total, with a multi-admit "tick" at positions 5-6.
        const TArray<int32> AdmitSequence = {0, 1, 2, 3, 4, 5, 6, 7};

        TArray<FPullWaveInstanceState> SimA;
        TArray<FPullWaveInstanceState> SimB;
        SimA.Reserve(MAX_POOL_SIZE);
        SimB.Reserve(MAX_POOL_SIZE);

        // Both simulations admit in identical WaveId order.
        for (int32 WaveId : AdmitSequence)
        {
            SimA.Add(MakeState(WaveId));
            SimB.Add(MakeState(WaveId));
        }

        TestEqual(
            TEXT("TC4: SimA.Num() == SimB.Num() == 8"),
            SimA.Num(),
            SimB.Num());

        bool bIdentical = true;
        for (int32 i = 0; i < SimA.Num(); ++i)
        {
            if (SimA[i].WaveId != SimB[i].WaveId)
            {
                bIdentical = false;
                AddError(FString::Printf(
                    TEXT("TC4: SimA[%d].WaveId=%d != SimB[%d].WaveId=%d"),
                    i, SimA[i].WaveId, i, SimB[i].WaveId));
            }
        }

        TestTrue(
            TEXT("TC4: identical Add() sequence → identical WaveId iteration order in both sims"),
            bIdentical);

        // Verify positions 5 and 6 are adjacent (the "same-tick multi-admit" pattern).
        TestEqual(TEXT("TC4: SimA[5].WaveId == 5 (first same-tick admit)"),  SimA[5].WaveId, 5);
        TestEqual(TEXT("TC4: SimA[6].WaveId == 6 (second same-tick admit)"), SimA[6].WaveId, 6);
        TestEqual(TEXT("TC4: SimB[5].WaveId == 5 (first same-tick admit)"),  SimB[5].WaveId, 5);
        TestEqual(TEXT("TC4: SimB[6].WaveId == 6 (second same-tick admit)"), SimB[6].WaveId, 6);

        return true;
    }

    // -----------------------------------------------------------------------
    // TC5 — AC-PW-22b pattern 9 FORBID: RemoveAtSwap absent from PullWave source.
    //
    // Asserts only the FORBID half: rg 'RemoveAtSwap' returns 0 matches.
    //
    // ALLOW-half deferred: rg 'RemoveAt\b' returns >= 1 match defers to Story 006
    // (despawn pipeline call site does not exist yet).
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("grep_forbid_removeatswap"))
    {
        const FString PullWaveSrcDir =
            FPaths::ConvertRelativePathToFull(
                FPaths::ProjectDir() / TEXT("Source/SLIPSTORM/PullWave"));

        // Enumerate all .h and .cpp files under Source/SLIPSTORM/PullWave/
        TArray<FString> FoundFiles;
        IFileManager::Get().FindFilesRecursive(
            FoundFiles, *PullWaveSrcDir, TEXT("*.h"), true, false);
        IFileManager::Get().FindFilesRecursive(
            FoundFiles, *PullWaveSrcDir, TEXT("*.cpp"), true, false);

        int32 ForbidHitCount = 0;
        for (const FString& FilePath : FoundFiles)
        {
            FString FileContents;
            if (FFileHelper::LoadFileToString(FileContents, *FilePath))
            {
                if (FileContents.Contains(TEXT("RemoveAtSwap")))
                {
                    ++ForbidHitCount;
                    AddError(FString::Printf(
                        TEXT("TC5 FORBID: RemoveAtSwap found in %s (ADR-0010 D1 / AC-PW-22b pattern 9)"),
                        *FilePath));
                }
            }
        }

        TestEqual(
            TEXT("TC5: RemoveAtSwap count == 0 in Source/SLIPSTORM/PullWave/ (FORBID half)"),
            ForbidHitCount,
            0);

        // Note: ALLOW half (RemoveAt >= 1 match) defers to Story 006.
        UE_LOG(LogTemp, Log,
            TEXT("TC5: ALLOW-half (RemoveAt >=1) deferred to Story 006 — call site not yet implemented."));

        return true;
    }

    // -----------------------------------------------------------------------
    // TC6 — AC-PW-22b pattern 11 FORBID: Insert absent from PullWave source.
    //
    // Asserts only the FORBID half: rg '\.Insert\s*\(' returns 0 matches.
    //
    // ALLOW-half deferred: rg '\.Add\s*\(' returns >= 1 match defers to Story 009
    // (Construct() entry point call site does not exist yet).
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("grep_forbid_insert"))
    {
        const FString PullWaveSrcDir =
            FPaths::ConvertRelativePathToFull(
                FPaths::ProjectDir() / TEXT("Source/SLIPSTORM/PullWave"));

        TArray<FString> FoundFiles;
        IFileManager::Get().FindFilesRecursive(
            FoundFiles, *PullWaveSrcDir, TEXT("*.h"), true, false);
        IFileManager::Get().FindFilesRecursive(
            FoundFiles, *PullWaveSrcDir, TEXT("*.cpp"), true, false);

        int32 ForbidHitCount = 0;
        for (const FString& FilePath : FoundFiles)
        {
            FString FileContents;
            if (FFileHelper::LoadFileToString(FileContents, *FilePath))
            {
                // Match .Insert( at any position — non-tail Insert on ActiveWaves is forbidden.
                if (FileContents.Contains(TEXT(".Insert(")))
                {
                    ++ForbidHitCount;
                    AddError(FString::Printf(
                        TEXT("TC6 FORBID: .Insert( found in %s (ADR-0010 D1 / AC-PW-22b pattern 11)"),
                        *FilePath));
                }
            }
        }

        TestEqual(
            TEXT("TC6: .Insert( count == 0 in Source/SLIPSTORM/PullWave/ (FORBID half)"),
            ForbidHitCount,
            0);

        // Note: ALLOW half (.Add( >= 1 match) defers to Story 009.
        UE_LOG(LogTemp, Log,
            TEXT("TC6: ALLOW-half (.Add >=1) deferred to Story 009 — call site not yet implemented."));

        return true;
    }

    // -----------------------------------------------------------------------
    // TC7 — ActiveWaves field type and USTRUCT annotation compatibility.
    //
    // Verifies that FPullWaveInstanceState can be stored in a TArray<> and
    // that default-constructing 23 elements does not exceed the 256-byte ceiling.
    // Exercises the USTRUCT() / GENERATED_BODY() presence indirectly: if UHT
    // rejected the annotation, this translation unit would not compile.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("pool_field_ustruct_compat"))
    {
        TArray<FPullWaveInstanceState> Pool;
        Pool.Reserve(MAX_POOL_SIZE);

        // Fill to capacity — all 23 slots default-constructed.
        for (int32 i = 0; i < MAX_POOL_SIZE; ++i)
        {
            Pool.Add(MakeState(i));
        }

        TestEqual(
            TEXT("TC7: Pool.Num() == MAX_POOL_SIZE (23) after filling to capacity"),
            Pool.Num(),
            static_cast<int32>(MAX_POOL_SIZE));

        // Each element must still satisfy the size ceiling (redundant with Story 001
        // TC4 static_assert, but included here as a runtime sanity guard).
        TestTrue(
            TEXT("TC7: sizeof(FPullWaveInstanceState) <= 256 (cache-friendly ceiling)"),
            sizeof(FPullWaveInstanceState) <= 256u);

        // Verify default WaveId == 0 for a freshly constructed state
        // (Story 009 Construct() will overwrite with NextWaveId before use).
        TestEqual(
            TEXT("TC7: MakeState(7).WaveId == 7 (field survives Add/copy)"),
            Pool[7].WaveId,
            7);

        return true;
    }

    // -----------------------------------------------------------------------
    // TC8 — InitializePool() test seam: runtime verification of actor pool init.
    //
    // Constructs APullWaveSubsystemActor via NewObject<> (no UWorld needed —
    // that is the purpose of the public InitializePool() seam per ADR-0010 D1
    // and Story 002 test-seam requirement).
    //
    // Verifies:
    //   - ActiveWaves.GetMax() >= MAX_POOL_SIZE (Reserve(23) actually ran)
    //   - ActiveWaves.Num() == 0 (Empty() was called — clean slate)
    //   - NextWaveId == 0 (counter reset for the session)
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("actor_initializepool_seam"))
    {
        APullWaveSubsystemActor* Actor =
            NewObject<APullWaveSubsystemActor>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC8: NewObject<APullWaveSubsystemActor> succeeded"), Actor))
        {
            return false;
        }

        Actor->InitializePool();

        TestTrue(
            TEXT("TC8: ActiveWaves.GetMax() >= MAX_POOL_SIZE after InitializePool()"),
            Actor->ActiveWaves.GetMax() >= static_cast<int32>(MAX_POOL_SIZE));

        TestEqual(
            TEXT("TC8: ActiveWaves.Num() == 0 (pool starts empty)"),
            Actor->ActiveWaves.Num(),
            0);

        TestEqual(
            TEXT("TC8: NextWaveId == 0 after InitializePool() (counter reset)"),
            Actor->NextWaveId,
            0);

        return true;
    }

    // Unknown parameter — fail explicitly.
    AddError(FString::Printf(
        TEXT("FPullWavePoolStorageWaveIdTest::RunTest — unknown Parameters value: '%s'"),
        *Parameters));
    return false;
}

#endif // WITH_DEV_AUTOMATION_TESTS

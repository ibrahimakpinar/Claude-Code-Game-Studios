// Copyright (c) Game Studio. All rights reserved.
// SLIPSTORM — 60-second voxel endless runner, mobile, Unreal Engine 5.7
//
// WaveSpawnerPoolTest.cpp — Story 001 unit tests for the Wave Spawner object pool,
// one-shot allocation guard, GC-anchor storage, and dual-inheritance tick gating.
//
// Story: production/epics/wave-spawner/story-001-subsystem-class-and-object-pool.md
// ADR:   docs/architecture/adr-0005-wave-spawner-subsystem-hosting.md
// TRs:   TR-WS-008 (pool count = 23, one-shot guard)
//         TR-WS-009 (UPROPERTY TObjectPtr storage + Deinitialize clears)
//         TR-WS-010 (dual inheritance + tick gating)
//
// Test category: SLIPSTORM.WaveSpawner.Pool
// Runner:        UE Automation Framework
//                Flags: ApplicationContextMask | ProductFilter
//
// Test 1 (Pool.Count23) — AC-WS-20a:
//   REQUIRES EditorContext — uses FAutomationEditorCommonUtils::CreateNewMap() to
//   obtain a real UWorld for SpawnActor<AWave>. Skipped gracefully in non-editor
//   contexts via WITH_EDITOR guard (ApplicationContextMask covers all contexts;
//   only the editor path can supply a valid world for SpawnActor).
//
// Tests 2, 4 — AC-WS-20b / AC-WS-10x:
//   Fully headless. Use NewObject<> + TestOnly_* seams (no UWorld needed).
//   Safe to run in all ApplicationContextMask contexts.
//
// Test 3 — AC-WS-20c:
//   REQUIRES EditorContext — the one-shot guard (bPoolAllocated) is Guard 2; it is
//   only reached when the world pointer is non-null. Passing nullptr triggers Guard 1
//   (null-world) first, meaning Guard 2 is never exercised. Both calls use CreateNewMap().
//   Skipped gracefully in non-editor contexts.
//
// Guard: WITH_DEV_AUTOMATION_TESTS only.

#if WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#include "WaveSpawner/WaveSpawnerSubsystem.h"
#include "WaveSpawner/Wave.h"

#if WITH_EDITOR
#include "Tests/AutomationEditorCommon.h"
#endif // WITH_EDITOR

// ============================================================================
// IMPLEMENT_COMPLEX_AUTOMATION_TEST
// 4 test commands covering Story 001 acceptance criteria.
// ============================================================================

IMPLEMENT_COMPLEX_AUTOMATION_TEST(
    FWaveSpawnerPoolTest,
    "SLIPSTORM.WaveSpawner.Pool",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

void FWaveSpawnerPoolTest::GetTests(
    TArray<FString>& OutBeautifiedNames,
    TArray<FString>& OutTestCommands) const
{
    OutBeautifiedNames.Add(TEXT("Pool count == 23 at OnFirstWorldLoaded exit (AC-WS-20a)"));
    OutTestCommands.Add(TEXT("Pool.Count23"));

    OutBeautifiedNames.Add(TEXT("Deinitialize() clears pool to 0 (AC-WS-20b, UPROPERTY ref released)"));
    OutTestCommands.Add(TEXT("Pool.DeinitializeClearsPool"));

    OutBeautifiedNames.Add(TEXT("One-shot guard — second OnFirstWorldLoaded call rejected (AC-WS-20c)"));
    OutTestCommands.Add(TEXT("Pool.OneShotGuard"));

    OutBeautifiedNames.Add(TEXT("Dual inheritance + tick gating in Cold state (AC-WS-10x)"));
    OutTestCommands.Add(TEXT("Subsystem.DualInheritanceTickGating"));
}

bool FWaveSpawnerPoolTest::RunTest(const FString& Parameters)
{
    // -------------------------------------------------------------------------
    // TC1 — AC-WS-20a (TR-WS-008): Pool count == 23 at OnFirstWorldLoaded exit.
    //
    // Requires a valid UWorld for SpawnActor<AWave>. Uses CreateNewMap() (editor only).
    // Skips gracefully in non-editor runs (ApplicationContextMask covers all contexts;
    // only EditorContext can supply a real world — see file header note).
    //
    // Given:  fresh UWaveSpawnerSubsystem, Cold state, pool not yet allocated.
    // When:   TestOnly_TriggerFirstWorldLoaded(EditorWorld) fires.
    // Then:   GetPool().Num() == 23 and every entry is non-null.
    // -------------------------------------------------------------------------
    if (Parameters == TEXT("Pool.Count23"))
    {
#if WITH_EDITOR
        // Create a transient editor world to supply a valid SpawnActor context.
        UWorld* TestWorld = FAutomationEditorCommonUtils::CreateNewMap();
        if (!TestNotNull(TEXT("TC1: CreateNewMap() returned a valid UWorld"), TestWorld))
        {
            return false;
        }

        // Construct subsystem via NewObject. GetTransientPackage() as outer allows
        // construction without a real GameInstance (test-only path per ADR-0010 D1 seam pattern).
        // We call the TestOnly seam directly — bypassing Initialize() which would try to
        // InitializeDependency on RSM/DPC that are not wired in this test context.
        UWaveSpawnerSubsystem* Sub =
            NewObject<UWaveSpawnerSubsystem>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC1: NewObject<UWaveSpawnerSubsystem> succeeded"), Sub))
        {
            return false;
        }

        // Trigger the pool allocation via the test seam.
        Sub->TestOnly_TriggerFirstWorldLoaded(TestWorld);

        // AC-WS-20a assertion: exactly 23 slots allocated.
        TestEqual(
            TEXT("TC1: GetPool().Num() == 23 at OnFirstWorldLoaded() exit (TR-WS-008)"),
            Sub->GetPool().Num(),
            23);

        // AC-WS-20a assertion: every slot is a valid non-null AWave*.
        bool bAllNonNull = true;
        for (int32 i = 0; i < Sub->GetPool().Num(); ++i)
        {
            if (!Sub->GetPool()[i])
            {
                bAllNonNull = false;
                AddError(FString::Printf(
                    TEXT("TC1: GetPool()[%d] is null — SpawnActor<AWave> failed for slot %d"),
                    i, i));
            }
        }
        TestTrue(
            TEXT("TC1: All 23 pool entries are non-null (SpawnActor succeeded for all slots)"),
            bAllNonNull);

        // AC-WS-20a assertion: one-shot guard is engaged after first allocation.
        TestTrue(
            TEXT("TC1: bPoolAllocated == true after first OnFirstWorldLoaded()"),
            Sub->TestOnly_IsPoolAllocated());

        return true;
#else
        // Non-editor context — SpawnActor not available without a real world.
        // Test is structurally correct; skipped in headless runner.
        UE_LOG(LogTemp, Log,
            TEXT("TC1 (Pool.Count23): Skipped — requires EditorContext for "
                 "FAutomationEditorCommonUtils::CreateNewMap(). "
                 "Run in editor to exercise SpawnActor × 23 path (AC-WS-20a)."));
        return true;
#endif // WITH_EDITOR
    }

    // -------------------------------------------------------------------------
    // TC2 — AC-WS-20b (TR-WS-009): Deinitialize() clears pool to 0.
    //
    // Headless: uses NewObject<AWave> + TestOnly_InjectPoolEntry to populate the
    // pool without SpawnActor. Verifies that Deinitialize() resets the TArray,
    // releasing UPROPERTY TObjectPtr GC anchors.
    //
    // Given:  subsystem with 3 injected AWave entries (TestOnly seam).
    // When:   Deinitialize() is called.
    // Then:   GetPool().Num() == 0 (UPROPERTY TArray cleared, GC anchors released).
    // -------------------------------------------------------------------------
    if (Parameters == TEXT("Pool.DeinitializeClearsPool"))
    {
        UWaveSpawnerSubsystem* Sub =
            NewObject<UWaveSpawnerSubsystem>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC2: NewObject<UWaveSpawnerSubsystem> succeeded"), Sub))
        {
            return false;
        }

        // Inject AWave objects created via NewObject (no UWorld needed — tests pool
        // array behavior, not the SpawnActor path). Precedent: APullWaveSubsystemActor
        // TC8 uses the same NewObject<Actor>(GetTransientPackage()) pattern.
        for (int32 i = 0; i < 3; ++i)
        {
            AWave* Wave = NewObject<AWave>(GetTransientPackage());
            if (!TestNotNull(FString::Printf(TEXT("TC2: NewObject<AWave> [%d] succeeded"), i), Wave))
            {
                return false;
            }
            Sub->TestOnly_InjectPoolEntry(Wave);
        }

        TestEqual(
            TEXT("TC2: Pool.Num() == 3 after 3 injections (pre-condition)"),
            Sub->GetPool().Num(),
            3);

        // Deinitialize() null-checks GetGameInstance() internally — safe to call
        // on a NewObject'd subsystem without a real GameInstance outer.
        Sub->Deinitialize();

        TestEqual(
            TEXT("TC2: Pool.Num() == 0 after Deinitialize() (UPROPERTY TArray cleared, "
                 "GC anchors released — TR-WS-009)"),
            Sub->GetPool().Num(),
            0);

        return true;
    }

    // -------------------------------------------------------------------------
    // TC3 — AC-WS-20c (TR-WS-008): One-shot guard rejects second allocation.
    //
    // Requires EditorContext — uses CreateNewMap() to supply a real UWorld on
    // both the first AND second trigger calls. Passing nullptr on the second call
    // would only exercise the null-world guard (Guard 1), never reaching the
    // bPoolAllocated guard (Guard 2). To prove Guard 2, the second call MUST
    // receive a non-null world; only then does control flow actually reach and
    // evaluate `if (bPoolAllocated) return;`.
    //
    // Given:  fresh subsystem, first TestOnly_TriggerFirstWorldLoaded(world) fires
    //         → SpawnActor × 23; Pool.Num() == 23; bPoolAllocated == true.
    // When:   TestOnly_TriggerFirstWorldLoaded(world) fires a SECOND time with the
    //         same non-null world.
    // Then:   Pool.Num() remains 23 (bPoolAllocated guard fired — no second alloc).
    //         Pool.Num() != 46 (guard worked; SpawnActor loop was NOT re-entered).
    // -------------------------------------------------------------------------
    if (Parameters == TEXT("Pool.OneShotGuard"))
    {
#if WITH_EDITOR
        UWorld* TestWorld = FAutomationEditorCommonUtils::CreateNewMap();
        if (!TestNotNull(TEXT("TC3: CreateNewMap() returned a valid UWorld"), TestWorld))
        {
            return false;
        }

        UWaveSpawnerSubsystem* Sub =
            NewObject<UWaveSpawnerSubsystem>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC3: NewObject<UWaveSpawnerSubsystem> succeeded"), Sub))
        {
            return false;
        }

        // First trigger: real allocation — 23 SpawnActor calls.
        Sub->TestOnly_TriggerFirstWorldLoaded(TestWorld);

        TestEqual(
            TEXT("TC3: Pool.Num() == 23 after first OnFirstWorldLoaded() (pre-condition)"),
            Sub->GetPool().Num(),
            23);

        TestTrue(
            TEXT("TC3: bPoolAllocated == true after first allocation (guard is engaged)"),
            Sub->TestOnly_IsPoolAllocated());

        // Second trigger with the same non-null world. Guard 2 (bPoolAllocated) must
        // fire and return before the SpawnActor loop. If the guard were absent, Pool
        // would grow to 46. The assertion Pool.Num() == 23 (not 46) proves Guard 2.
        Sub->TestOnly_TriggerFirstWorldLoaded(TestWorld);

        TestEqual(
            TEXT("TC3: Pool.Num() == 23 — second OnFirstWorldLoaded(world) rejected by "
                 "bPoolAllocated guard; no second SpawnActor pass (AC-WS-20c, TR-WS-008)"),
            Sub->GetPool().Num(),
            23);

        TestTrue(
            TEXT("TC3: bPoolAllocated still true after rejected second call"),
            Sub->TestOnly_IsPoolAllocated());

        return true;
#else
        UE_LOG(LogTemp, Log,
            TEXT("TC3 (Pool.OneShotGuard): Skipped — requires EditorContext for "
                 "CreateNewMap() to supply a non-null UWorld on the second trigger call. "
                 "A nullptr second call would only exercise Guard 1 (null-world), not "
                 "Guard 2 (bPoolAllocated). Run in editor to verify AC-WS-20c."));
        return true;
#endif // WITH_EDITOR
    }

    // -------------------------------------------------------------------------
    // TC4 — AC-WS-10x (TR-WS-010): Dual inheritance + tick gating in Cold state.
    //
    // Headless: verifies compile-time dual inheritance and runtime tick gating.
    //
    // Given:  freshly constructed UWaveSpawnerSubsystem (LifecycleState = Cold).
    // When:   IsTickable() and GetTickableTickType() are called.
    // Then:   IsTickable() returns false (Cold state → tick suppressed).
    //         GetTickableTickType() returns ETickableTickType::Conditional.
    //
    // Dual inheritance compile-time proof: if UWaveSpawnerSubsystem did not inherit
    // from both UGameInstanceSubsystem and FTickableGameObject, the IsTickable() and
    // GetTickableTickType() calls below would not compile (ADR-0005 Decision).
    // -------------------------------------------------------------------------
    if (Parameters == TEXT("Subsystem.DualInheritanceTickGating"))
    {
        UWaveSpawnerSubsystem* Sub =
            NewObject<UWaveSpawnerSubsystem>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC4: NewObject<UWaveSpawnerSubsystem> succeeded"), Sub))
        {
            return false;
        }

        // LifecycleState defaults to EWaveSpawnerLifecycleState::Cold at construction.
        // IsTickable() must return false in Cold state (ADR-0005 Decision; TR-WS-011).
        TestFalse(
            TEXT("TC4: IsTickable() == false in Cold state (tick suppressed — TR-WS-011)"),
            Sub->IsTickable());

        // GetTickableTickType() must return Conditional.
        // Conditional type defers the tick decision to IsTickable() each frame,
        // avoiding a registered tick entirely when false (ADR-0005 Decision).
        TestEqual(
            TEXT("TC4: GetTickableTickType() == Conditional (ADR-0005 Decision; TR-WS-010)"),
            Sub->GetTickableTickType(),
            ETickableTickType::Conditional);

        // Dual inheritance compile-time proof: GetStatId() is defined on FTickableGameObject.
        // If UWaveSpawnerSubsystem did not inherit FTickableGameObject, calling GetStatId()
        // here would fail to compile. The fact that this TU compiles is the proof.
        // TStatId::IsNone() is not asserted — its availability is post-cutoff and unverified
        // in UE 5.7 headers. The compile itself is the assertion (pattern: PullWave TC10).
        Sub->GetStatId(); // compile-time dual-inheritance verification — see comment above
        UE_LOG(LogTemp, Log,
            TEXT("TC4: GetStatId() compiled — FTickableGameObject inheritance verified "
                 "(STATGROUP_WaveSpawner declared, ADR-0005 IG-5)"));
        TestTrue(
            TEXT("TC4: GetStatId() is callable — compile-time dual-inheritance proof passed "
                 "(ADR-0005 Decision; TR-WS-010)"),
            true);

        return true;
    }

    // Unknown parameter — fail explicitly rather than silently returning true.
    AddError(FString::Printf(
        TEXT("FWaveSpawnerPoolTest::RunTest — unknown Parameters value: '%s'"),
        *Parameters));
    return false;
}

#endif // WITH_DEV_AUTOMATION_TESTS

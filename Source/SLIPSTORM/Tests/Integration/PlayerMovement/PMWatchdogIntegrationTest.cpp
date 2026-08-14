// Copyright (c) Game Studio. All rights reserved.
// SLIPSTORM — 60-second voxel endless runner, mobile, Unreal Engine 5.7
//
// PMWatchdogIntegrationTest.cpp — S1-09: integration test for the sequential
// Setup B → Setup D DT watchdog release-latency chain.
//
// Story: S1-09 (production/sprints/sprint-1.md line 36)
// GDD:   design/gdd/player-movement-platform.md §3 Hardware Contract + §8 AC-HW-A Setup B/D
// ADR:   docs/architecture/adr-0009-player-movement-hosting.md
// Spec:  tests/integration/player-movement/pm-watchdog-integration-spec.md
//
// Test category: SLIPSTORM.PlayerMovement.WatchdogIntegration
// Runner: UE Automation Framework, headless per docs/tests-headless.md.
//
// Companion: PMWatchdogTest.cpp (unit) validates Setup B and Setup D in
// isolation via SEED_* macros; this integration test validates the *sequential*
// path where Setup D immediately follows Setup B on the same live PM component
// (buffer must flush breach samples before hysteresis accumulator can start —
// the unit test comment at PMWatchdogTest.cpp:335-338 explicitly flagged this
// scenario as belonging in integration).
//
// Assertions verify design invariants (broadcast counts, event ordering,
// bounded latency), not exact tick counts. Code-derived expected latency is
// 60 buffer-flush + 180 hysteresis-accumulate = 240 ticks; the test accepts
// the plausible range [180, 260] to remain robust to buffer semantics.
//
// Build guard: WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS per file-family pattern.

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"      // FTestWorldWrapper (S1-04 harness fix)
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

#include "Player/PlayerLaneMovementComponent.h"
#include "Player/SlipstormPlayerPawn.h"

// ---------------------------------------------------------------------------
// Helper: file-local mirror of the pattern used across
// Source/SLIPSTORM/Tests/Integration/PlayerMovement/. See
// PMLifecycleAndSeamTest.cpp:82-109 for the full rationale + engine citation.
// ---------------------------------------------------------------------------

static UWorld* CreateTestPlayWorld(FAutomationTestBase* T, FTestWorldWrapper& WorldWrapper, const TCHAR* Label)
{
    if (!WorldWrapper.CreateTestWorld(EWorldType::Game))
    {
        T->AddError(FString::Printf(TEXT("%s: FTestWorldWrapper::CreateTestWorld failed"), Label));
        return nullptr;
    }
    if (!WorldWrapper.BeginPlayInTestWorld())
    {
        T->AddError(FString::Printf(TEXT("%s: FTestWorldWrapper::BeginPlayInTestWorld failed"), Label));
        return nullptr;
    }
    return WorldWrapper.GetTestWorld();
}

// ---------------------------------------------------------------------------
// IMPLEMENT_COMPLEX_AUTOMATION_TEST
// EditorContext + ClientContext + ProductFilter per docs/tests-headless.md §2
// flag-registration rule (S1-04 root cause).
// ---------------------------------------------------------------------------

IMPLEMENT_COMPLEX_AUTOMATION_TEST(
    FPMWatchdogIntegrationTest,
    "SLIPSTORM.PlayerMovement.WatchdogIntegration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

void FPMWatchdogIntegrationTest::GetTests(
    TArray<FString>& OutBeautifiedNames,
    TArray<FString>& OutTestCommands) const
{
    OutBeautifiedNames.Add(TEXT("Sequential Setup B -> Setup D release-latency chain"));
    OutTestCommands.Add(TEXT("sequential_b_to_d_release_latency"));
}

bool FPMWatchdogIntegrationTest::RunTest(const FString& Parameters)
{
    // -----------------------------------------------------------------------
    // TC1 — Sequential Setup B → Setup D release-latency chain
    //
    // Given: fresh PM on a spawned ASlipstormPlayerPawn in a real
    //        EWorldType::Game world (via FTestWorldWrapper) with sentinel-
    //        filled DT rolling buffer.
    // When:  30 x WatchdogTick(0.020f) drives Setup B (breach entry) then
    //        WatchdogTick(0.01667f) is called repeatedly until release fires.
    // Then:  (a) breach broadcast fires exactly once during entry phase.
    //        (b) release broadcast fires exactly once during release phase.
    //        (c) subscriber sees exactly [true, false] in order.
    //        (d) total release-phase tick count is bounded in [180, 260] —
    //            60 buffer-flush ticks + 180 hysteresis-accumulate ticks =
    //            240 code-derived expected; range accommodates buffer-
    //            semantics variance.
    //        (e) is_hw_performance_degraded transitions true→false correctly.
    //
    // ADR-0009 IG-3 scope carve-out per PlayerLaneMovementComponent.h:122 —
    // test-scope lambda subscriber is exempt (subscriber lifetime bounded by
    // RunTest with explicit Remove(Handle) at cleanup).
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("sequential_b_to_d_release_latency"))
    {
        FTestWorldWrapper WorldWrapper;
        UWorld* TestWorld = CreateTestPlayWorld(this, WorldWrapper, TEXT("TC1"));
        if (!TestWorld)
        {
            return false;
        }

        // Spawn expects null curves — three null-curve LogErrors are not the
        // concern of this test.
        AddExpectedError(TEXT("SlipCurve is null"),       EAutomationExpectedErrorFlags::Contains, 1);
        AddExpectedError(TEXT("LeanCurve is null"),       EAutomationExpectedErrorFlags::Contains, 1);
        AddExpectedError(TEXT("EdgeAbsorbCurve is null"), EAutomationExpectedErrorFlags::Contains, 1);
        // Setup B injection produces exactly one "Watchdog entered breach" LogError.
        AddExpectedError(TEXT("Watchdog entered breach"), EAutomationExpectedErrorFlags::Contains, 1);

        ASlipstormPlayerPawn* Pawn = TestWorld->SpawnActorDeferred<ASlipstormPlayerPawn>(
            ASlipstormPlayerPawn::StaticClass(), FTransform::Identity);
        if (!TestNotNull(TEXT("TC1: SpawnActorDeferred returned null"), Pawn))
        {
            return false;
        }
        UGameplayStatics::FinishSpawningActor(Pawn, FTransform::Identity);

        UPlayerLaneMovementComponent* PM = Pawn->MovementComponent.Get();
        if (!TestNotNull(TEXT("TC1: MovementComponent is null after spawn"), PM))
        {
            return false;
        }

        // Precondition: sentinel-filled buffer + clean state.
        TestFalse(TEXT("TC1: precondition — is_hw_performance_degraded == false"),
                  PM->is_hw_performance_degraded);
        TestFalse(TEXT("TC1: precondition — bHardwarePerformanceBreachActive == false"),
                  PM->bHardwarePerformanceBreachActive);

        // Bind test-scope subscriber recording every bEntering value.
        TArray<bool> Received;
        FDelegateHandle Handle = PM->OnHardwarePerformanceBreach.AddLambda(
            [&Received](bool bEntering)
            {
                Received.Add(bEntering);
            });

        // ------------- Phase 1: Setup B entry (30 x 0.020s → breach) -------
        for (int32 i = 0; i < 30; ++i)
        {
            PM->WatchdogTick(0.020f);
        }

        TestTrue(TEXT("TC1 Phase 1: is_hw_performance_degraded == true after 30 x 0.020s"),
                 PM->is_hw_performance_degraded);
        TestTrue(TEXT("TC1 Phase 1: bHardwarePerformanceBreachActive == true"),
                 PM->bHardwarePerformanceBreachActive);
        TestEqual(TEXT("TC1 Phase 1: subscriber received exactly 1 event"),
                  Received.Num(), 1);
        if (Received.Num() >= 1)
        {
            TestTrue(TEXT("TC1 Phase 1: first event bEntering == true"),
                     Received[0] == true);
        }

        // ------------- Phase 2: sequential Setup D release -----------------
        // Loop clean ticks until release fires. Cap at 300 to bound the test.
        int32 ReleaseTickCount = 0;
        const int32 MaxReleasePhaseTicks = 300;
        for (int32 i = 1; i <= MaxReleasePhaseTicks; ++i)
        {
            PM->WatchdogTick(0.01667f);
            if (Received.Num() >= 2)
            {
                ReleaseTickCount = i;
                break;
            }
        }

        TestTrue(TEXT("TC1 Phase 2: release fired within 300 ticks"),
                 ReleaseTickCount > 0);
        TestTrue(TEXT("TC1 Phase 2: release tick count in plausible range [180, 260]"),
                 ReleaseTickCount >= 180 && ReleaseTickCount <= 260);
        TestEqual(TEXT("TC1 Phase 2: subscriber received exactly 2 events total"),
                  Received.Num(), 2);
        if (Received.Num() >= 2)
        {
            TestTrue(TEXT("TC1 Phase 2: second event bEntering == false"),
                     Received[1] == false);
        }
        TestFalse(TEXT("TC1 Phase 2: is_hw_performance_degraded == false post-release"),
                  PM->is_hw_performance_degraded);
        TestFalse(TEXT("TC1 Phase 2: bHardwarePerformanceBreachActive == false post-release"),
                  PM->bHardwarePerformanceBreachActive);

        // Cleanup — unbind lambda + destroy pawn.
        PM->OnHardwarePerformanceBreach.Remove(Handle);
        TestWorld->DestroyActor(Pawn);
        return true;
    }
    return false;
}

#endif // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

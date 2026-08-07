// Copyright (c) Game Studio. All rights reserved.
// SLIPSTORM — 60-second voxel endless runner, mobile, Unreal Engine 5.7
//
// PMLifecycleAndSeamTest.cpp — Story 001a: integration test harness for
// Story 001 lifecycle + Seam 12 tests.
//
// Spec: tests/integration/player-movement/pm-lifecycle-and-seam-spec.md
// GDD: design/gdd/player-movement-mechanics.md + player-movement-platform.md
// ADR: docs/architecture/adr-0009-player-movement-hosting.md (SD1/2/6)
//      docs/architecture/adr-0007-run-state-machine-hosting.md
// Story: production/epics/player-movement/story-001a-test-harness.md
//
// Test category: SLIPSTORM.PlayerMovement.LifecycleAndSeam
// Runner: UE Automation Framework, headless (-nullrhi -nosound -unattended)
//         ClientContext | ProductFilter — compatible with the CI headless runner.
//
// Note: AC-SS-B (ordinal lockstep static_assert) and AC-SS-E (MIN_ESCAPE_SLIPS
// static_assert) are compile-time gates — a successful build IS the test.
// No runtime cases are added for compile-time ACs.
//
// Build guard: entire file is gated behind WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS.
// FAutomationEditorCommonUtils::CreateNewMap() is an editor-only API; it must
// never link into Shipping or Game builds.  See SLIPSTORM.Build.cs bBuildEditor
// block for the UnrealEd module dependency that satisfies the linker.

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Curves/CurveFloat.h"

#include "Player/PlayerLaneMovementComponent.h"
#include "Player/SlipstormPlayerPawn.h"
#include "Player/EPlayerLane.h"
#include "Player/ERunSlipState.h"
#include "RunStateMachine/RunStateMachineSubsystem.h"
#include "Seam/PlayerMovementProvider.h"

// ---------------------------------------------------------------------------
// Helper: construct a valid UCurveFloat with 2 keys spanning [0, 1].
// Anchored to InOuter to survive GC within the test scope.
// ---------------------------------------------------------------------------

static UCurveFloat* MakeValidCurve(UObject* InOuter)
{
    UCurveFloat* C = NewObject<UCurveFloat>(InOuter);
    C->FloatCurve.AddKey(0.0f, 0.0f);
    C->FloatCurve.AddKey(1.0f, 1.0f);
    return C;
}

// ---------------------------------------------------------------------------
// Helper: construct a UCurveFloat with exactly 1 key (key-count validation fail).
// ---------------------------------------------------------------------------

static UCurveFloat* MakeSingleKeyCurve(UObject* InOuter)
{
    UCurveFloat* C = NewObject<UCurveFloat>(InOuter);
    C->FloatCurve.AddKey(0.0f, 0.5f);
    return C;
}

// ---------------------------------------------------------------------------
// Helper: construct a UCurveFloat with 2 keys but range NOT spanning [0, 1].
// First key > 0 and last key < 1 both fail the ValidateCurveAsset range check.
// ---------------------------------------------------------------------------

static UCurveFloat* MakeRangeFailCurve(UObject* InOuter)
{
    UCurveFloat* C = NewObject<UCurveFloat>(InOuter);
    C->FloatCurve.AddKey(0.1f, 0.1f);
    C->FloatCurve.AddKey(0.9f, 0.9f);
    return C;
}

// ---------------------------------------------------------------------------
// Helper: create a new map AND start play mode so BeginPlay auto-dispatches
// on SpawnActor / FinishSpawningActor calls.
//
// Fix 2 (code-review pass 2026-07-11): FAutomationEditorCommonUtils::CreateNewMap()
// returns an EWorldType::Editor world where HasBegunPlay() == false, causing
// PostActorConstruction to skip auto-dispatch of BeginPlay.
// Calling InitializeActorsForPlay + BeginPlay transitions the world to play mode
// before any spawns occur, so the standard deferred-spawn + FinishSpawningActor
// path correctly triggers BeginPlay on each pawn.  Apply CONSISTENTLY at all
// CreateNewMap sites — never mix with per-pawn DispatchBeginPlay().
// ---------------------------------------------------------------------------

static UWorld* CreateTestPlayWorld(FAutomationTestBase* T, const TCHAR* Label)
{
    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    if (!World)
    {
        T->AddError(FString::Printf(TEXT("%s: CreateNewMap returned null"), Label));
        return nullptr;
    }
    World->InitializeActorsForPlay(FURL(nullptr));
    World->BeginPlay();
    return World;
}

// ---------------------------------------------------------------------------
// IMPLEMENT_COMPLEX_AUTOMATION_TEST
// 6 GetTests() entries per Story 001a Implementation Notes skeleton verbatim.
// Each RunTest dispatch owns its own world setup + teardown.
// ---------------------------------------------------------------------------

IMPLEMENT_COMPLEX_AUTOMATION_TEST(
    FPMLifecycleAndSeamTest,
    "SLIPSTORM.PlayerMovement.LifecycleAndSeam",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

void FPMLifecycleAndSeamTest::GetTests(
    TArray<FString>& OutBeautifiedNames,
    TArray<FString>& OutTestCommands) const
{
    OutBeautifiedNames.Add(TEXT("Curve fallback — null SlipCurve"));       OutTestCommands.Add(TEXT("curve_fallback_null_slip"));
    OutBeautifiedNames.Add(TEXT("Curve fallback — all curves valid"));     OutTestCommands.Add(TEXT("curve_fallback_valid_all"));
    OutBeautifiedNames.Add(TEXT("Watchdog sentinel init"));                OutTestCommands.Add(TEXT("watchdog_sentinel_init"));
    OutBeautifiedNames.Add(TEXT("Delegate lifecycle bind + unbind"));      OutTestCommands.Add(TEXT("delegate_lifecycle"));
    OutBeautifiedNames.Add(TEXT("Seam 12 production provider proxy"));     OutTestCommands.Add(TEXT("seam12_provider_proxy"));
    OutBeautifiedNames.Add(TEXT("Pawn subobject wiring + attachment"));    OutTestCommands.Add(TEXT("pawn_subobject_wiring"));
}

bool FPMLifecycleAndSeamTest::RunTest(const FString& Parameters)
{
    // -----------------------------------------------------------------------
    // TC1 — AC-SS-D: Curve fallback flag set when curves are null / invalid.
    //
    // Primary case: all three curves null → bCurveFallbackActive = true.
    // Three sub-case variants each in their own deferred-spawn + teardown scope:
    //   (a) LeanCurve null only: only LeanCurve absent; others valid.
    //   (b) EdgeAbsorbCurve with 1 key (key-count fail).
    //   (c) Range fail: first-key > 0 AND last-key < 1 on SlipCurve.
    //
    // Uses SpawnActorDeferred so curves can be injected before BeginPlay fires.
    // AddExpectedError() silences the Error log entries that ValidateCurveAsset
    // emits — without these, the UE Automation runner treats Error logs as test
    // failures regardless of assertion outcomes.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("curve_fallback_null_slip"))
    {
        // --- Primary: all three curves null ---
        {
            UWorld* TestWorld = CreateTestPlayWorld(this, TEXT("TC1-primary"));
            if (!TestWorld)
            {
                return false;
            }

            // Expect 3 Error log entries — one per null curve.
            AddExpectedError(TEXT("SlipCurve is null"),       EAutomationExpectedErrorFlags::Contains, 1);
            AddExpectedError(TEXT("LeanCurve is null"),       EAutomationExpectedErrorFlags::Contains, 1);
            AddExpectedError(TEXT("EdgeAbsorbCurve is null"), EAutomationExpectedErrorFlags::Contains, 1);

            ASlipstormPlayerPawn* Pawn = TestWorld->SpawnActorDeferred<ASlipstormPlayerPawn>(
                ASlipstormPlayerPawn::StaticClass(), FTransform::Identity);
            if (!TestNotNull(TEXT("TC1-primary: SpawnActorDeferred returned null"), Pawn))
            {
                return false;
            }
            // All three curves default to nullptr — no assignment needed.
            // But explicit for documentation of intent:
            Pawn->MovementComponent->SlipCurve       = nullptr;
            Pawn->MovementComponent->LeanCurve       = nullptr;
            Pawn->MovementComponent->EdgeAbsorbCurve = nullptr;

            UGameplayStatics::FinishSpawningActor(Pawn, FTransform::Identity);
            // BeginPlay has now run.

            TestTrue(
                TEXT("TC1-primary: bCurveFallbackActive == true after all-null BeginPlay"),
                Pawn->MovementComponent->bCurveFallbackActive);

            TestWorld->DestroyActor(Pawn);
        }

        // --- Sub-case (a): LeanCurve null only ---
        {
            UWorld* TestWorld = CreateTestPlayWorld(this, TEXT("TC1-a"));
            if (!TestWorld)
            {
                return false;
            }

            AddExpectedError(TEXT("LeanCurve is null"), EAutomationExpectedErrorFlags::Contains, 1);

            ASlipstormPlayerPawn* Pawn = TestWorld->SpawnActorDeferred<ASlipstormPlayerPawn>(
                ASlipstormPlayerPawn::StaticClass(), FTransform::Identity);
            if (!TestNotNull(TEXT("TC1-a: SpawnActorDeferred returned null"), Pawn))
            {
                return false;
            }
            Pawn->MovementComponent->SlipCurve       = MakeValidCurve(TestWorld);
            Pawn->MovementComponent->LeanCurve       = nullptr; // single null
            Pawn->MovementComponent->EdgeAbsorbCurve = MakeValidCurve(TestWorld);

            UGameplayStatics::FinishSpawningActor(Pawn, FTransform::Identity);

            TestTrue(
                TEXT("TC1-a: bCurveFallbackActive == true with LeanCurve null only"),
                Pawn->MovementComponent->bCurveFallbackActive);

            TestWorld->DestroyActor(Pawn);
        }

        // --- Sub-case (b): EdgeAbsorbCurve with exactly 1 key (key-count fail) ---
        {
            UWorld* TestWorld = CreateTestPlayWorld(this, TEXT("TC1-b"));
            if (!TestWorld)
            {
                return false;
            }

            // ValidateCurveAsset logs the name + "has N key(s); requires >= 2"
            AddExpectedError(TEXT("EdgeAbsorbCurve has 1 key"), EAutomationExpectedErrorFlags::Contains, 1);

            ASlipstormPlayerPawn* Pawn = TestWorld->SpawnActorDeferred<ASlipstormPlayerPawn>(
                ASlipstormPlayerPawn::StaticClass(), FTransform::Identity);
            if (!TestNotNull(TEXT("TC1-b: SpawnActorDeferred returned null"), Pawn))
            {
                return false;
            }
            Pawn->MovementComponent->SlipCurve       = MakeValidCurve(TestWorld);
            Pawn->MovementComponent->LeanCurve       = MakeValidCurve(TestWorld);
            Pawn->MovementComponent->EdgeAbsorbCurve = MakeSingleKeyCurve(TestWorld); // 1 key

            UGameplayStatics::FinishSpawningActor(Pawn, FTransform::Identity);

            TestTrue(
                TEXT("TC1-b: bCurveFallbackActive == true with EdgeAbsorbCurve having 1 key"),
                Pawn->MovementComponent->bCurveFallbackActive);

            TestWorld->DestroyActor(Pawn);
        }

        // --- Sub-case (c): Range fail on SlipCurve (first-key > 0, last-key < 1) ---
        {
            UWorld* TestWorld = CreateTestPlayWorld(this, TEXT("TC1-c"));
            if (!TestWorld)
            {
                return false;
            }

            // ValidateCurveAsset logs name + "key range [0.1, 0.9] does not span [0, 1]"
            AddExpectedError(TEXT("SlipCurve key range"), EAutomationExpectedErrorFlags::Contains, 1);

            ASlipstormPlayerPawn* Pawn = TestWorld->SpawnActorDeferred<ASlipstormPlayerPawn>(
                ASlipstormPlayerPawn::StaticClass(), FTransform::Identity);
            if (!TestNotNull(TEXT("TC1-c: SpawnActorDeferred returned null"), Pawn))
            {
                return false;
            }
            Pawn->MovementComponent->SlipCurve       = MakeRangeFailCurve(TestWorld); // [0.1, 0.9]
            Pawn->MovementComponent->LeanCurve       = MakeValidCurve(TestWorld);
            Pawn->MovementComponent->EdgeAbsorbCurve = MakeValidCurve(TestWorld);

            UGameplayStatics::FinishSpawningActor(Pawn, FTransform::Identity);

            TestTrue(
                TEXT("TC1-c: bCurveFallbackActive == true with SlipCurve range [0.1, 0.9]"),
                Pawn->MovementComponent->bCurveFallbackActive);

            TestWorld->DestroyActor(Pawn);
        }

        return true;
    }

    // -----------------------------------------------------------------------
    // TC2 — AC-SS-D: Curve fallback flag NOT set when all curves are valid.
    //
    // Given: all three curves have >= 2 keys with range spanning [0, 1].
    // When: BeginPlay runs.
    // Then: bCurveFallbackActive == false; no Error logs from LogPlayerMovement.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("curve_fallback_valid_all"))
    {
        UWorld* TestWorld = CreateTestPlayWorld(this, TEXT("TC2"));
        if (!TestWorld)
        {
            return false;
        }

        ASlipstormPlayerPawn* Pawn = TestWorld->SpawnActorDeferred<ASlipstormPlayerPawn>(
            ASlipstormPlayerPawn::StaticClass(), FTransform::Identity);
        if (!TestNotNull(TEXT("TC2: SpawnActorDeferred returned null"), Pawn))
        {
            return false;
        }
        Pawn->MovementComponent->SlipCurve       = MakeValidCurve(TestWorld);
        Pawn->MovementComponent->LeanCurve       = MakeValidCurve(TestWorld);
        Pawn->MovementComponent->EdgeAbsorbCurve = MakeValidCurve(TestWorld);

        UGameplayStatics::FinishSpawningActor(Pawn, FTransform::Identity);

        TestFalse(
            TEXT("TC2: bCurveFallbackActive == false with all valid curves"),
            Pawn->MovementComponent->bCurveFallbackActive);

        TestWorld->DestroyActor(Pawn);
        return true;
    }

    // -----------------------------------------------------------------------
    // TC3 — Watchdog sentinel init: buffer pre-fill.
    //
    // Given: fresh PM after BeginPlay in a valid test world.
    // When: BeginPlay completes.
    // Then:
    //   - TickDTRollingBuffer[i] == 0.01667f for all i in [0, 60).
    //   - TickDTRingIndex == 0.
    //   - ContinuousCleanWindowTime == 0.0f.
    //   - bHardwarePerformanceBreachActive == false.
    //
    // Private fields accessed via friend class FPMLifecycleAndSeamTest
    // (declared at PlayerLaneMovementComponent.h under WITH_DEV_AUTOMATION_TESTS).
    //
    // This is the AC-HW-A Setup G Part 1 regression catch — future refactors that
    // remove the sentinel init MUST fail this test.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("watchdog_sentinel_init"))
    {
        UWorld* TestWorld = CreateTestPlayWorld(this, TEXT("TC3"));
        if (!TestWorld)
        {
            return false;
        }

        // All curves null to keep setup minimal (fallback flag irrelevant here).
        AddExpectedError(TEXT("SlipCurve is null"),       EAutomationExpectedErrorFlags::Contains, 1);
        AddExpectedError(TEXT("LeanCurve is null"),       EAutomationExpectedErrorFlags::Contains, 1);
        AddExpectedError(TEXT("EdgeAbsorbCurve is null"), EAutomationExpectedErrorFlags::Contains, 1);

        ASlipstormPlayerPawn* Pawn = TestWorld->SpawnActorDeferred<ASlipstormPlayerPawn>(
            ASlipstormPlayerPawn::StaticClass(), FTransform::Identity);
        if (!TestNotNull(TEXT("TC3: SpawnActorDeferred returned null"), Pawn))
        {
            return false;
        }
        UGameplayStatics::FinishSpawningActor(Pawn, FTransform::Identity);

        UPlayerLaneMovementComponent* PM = Pawn->MovementComponent.Get();
        if (!TestNotNull(TEXT("TC3: MovementComponent is null after spawn"), PM))
        {
            return false;
        }

        // Verify all 60 sentinel slots (friend access to private TickDTRollingBuffer).
        bool bAllSlotsCorrect = true;
        for (int32 i = 0; i < 60; ++i)
        {
            if (!FMath::IsNearlyEqual(PM->TickDTRollingBuffer[i], 0.01667f, KINDA_SMALL_NUMBER))
            {
                AddError(FString::Printf(
                    TEXT("TC3: TickDTRollingBuffer[%d] == %f, expected 0.01667f"),
                    i, PM->TickDTRollingBuffer[i]));
                bAllSlotsCorrect = false;
            }
        }
        TestTrue(TEXT("TC3: All 60 TickDTRollingBuffer slots == 0.01667f"), bAllSlotsCorrect);

        TestEqual(TEXT("TC3: TickDTRingIndex == 0"),          PM->TickDTRingIndex,              0);
        TestEqual(TEXT("TC3: ContinuousCleanWindowTime == 0.0f"), PM->ContinuousCleanWindowTime, 0.0f);
        TestFalse(TEXT("TC3: bHardwarePerformanceBreachActive == false"), PM->bHardwarePerformanceBreachActive);

        // Public field corroboration.
        TestFalse(TEXT("TC3: is_hw_performance_degraded == false"), PM->is_hw_performance_degraded);

        // Tick must be enabled post-BeginPlay.
        TestTrue(TEXT("TC3: PrimaryComponentTick.bCanEverTick == true after BeginPlay"),
                 PM->PrimaryComponentTick.bCanEverTick);

        TestWorld->DestroyActor(Pawn);
        return true;
    }

    // -----------------------------------------------------------------------
    // TC4 — Delegate lifecycle: BeginPlay bind + EndPlay unbind + IsValid guard.
    //
    // Given: ASlipstormPlayerPawn spawned into a test world with a valid
    //        URunStateMachineSubsystem available via GameInstance.
    // When:  BeginPlay runs, then EndPlay(Destroyed) runs.
    // Then:
    //   (a) Post-BeginPlay: StateChangedHandle.IsValid() == true.
    //   (b) Post-BeginPlay: PausedChangedHandle.IsValid() == true.
    //   (c) Post-BeginPlay: PrimaryComponentTick.bCanEverTick == true.
    //   (d) Post-BeginPlay: RSMSubsystem->OnStateChanged.IsBound() == true
    //       (PM is the sole subscriber in the stub world).
    //   (e) EndPlay call returns without crash.
    //   (f) Post-EndPlay: PrimaryComponentTick.bCanEverTick == false.
    //   (g) Edge case: second EndPlay call (direct component call) must not crash.
    //   (h) Edge case: EndPlay when RSMSubsystem GC'd first — must not crash.
    //
    // NOTE on (d): relies on PM being the sole subscriber to OnStateChanged
    // in this test world.  If the RSM stub gains additional internal bindings
    // in a future story, this assertion must be updated.
    //
    // Friend access used for StateChangedHandle, PausedChangedHandle, RSMSubsystem.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("delegate_lifecycle"))
    {
        UWorld* TestWorld = CreateTestPlayWorld(this, TEXT("TC4"));
        if (!TestWorld)
        {
            return false;
        }

        // Suppress curve null errors — curves not relevant to this case.
        AddExpectedError(TEXT("SlipCurve is null"),       EAutomationExpectedErrorFlags::Contains, 1);
        AddExpectedError(TEXT("LeanCurve is null"),       EAutomationExpectedErrorFlags::Contains, 1);
        AddExpectedError(TEXT("EdgeAbsorbCurve is null"), EAutomationExpectedErrorFlags::Contains, 1);

        ASlipstormPlayerPawn* Pawn = TestWorld->SpawnActorDeferred<ASlipstormPlayerPawn>(
            ASlipstormPlayerPawn::StaticClass(), FTransform::Identity);
        if (!TestNotNull(TEXT("TC4: SpawnActorDeferred returned null"), Pawn))
        {
            return false;
        }
        UGameplayStatics::FinishSpawningActor(Pawn, FTransform::Identity);

        UPlayerLaneMovementComponent* PM = Pawn->MovementComponent.Get();
        if (!TestNotNull(TEXT("TC4: MovementComponent is null after spawn"), PM))
        {
            return false;
        }

        // (a) StateChangedHandle valid post-BeginPlay.
        TestTrue(TEXT("TC4-a: StateChangedHandle.IsValid() post-BeginPlay"),
                 PM->StateChangedHandle.IsValid());

        // (b) PausedChangedHandle valid post-BeginPlay.
        TestTrue(TEXT("TC4-b: PausedChangedHandle.IsValid() post-BeginPlay"),
                 PM->PausedChangedHandle.IsValid());

        // (c) Ticking enabled.
        TestTrue(TEXT("TC4-c: PrimaryComponentTick.bCanEverTick == true post-BeginPlay"),
                 PM->PrimaryComponentTick.bCanEverTick);

        // (d) TC4-precondition: RSMSubsystem must be registered in the test world's
        // GameInstance. If this fails, all TC4 assertions below are meaningless — abort
        // explicitly rather than silently skipping (former code review found silent
        // skip pattern was a false-confidence hazard).
        if (!TestTrue(TEXT("TC4-precondition: RSMSubsystem registered in test world"),
                      IsValid(PM->RSMSubsystem)))
        {
            TestWorld->DestroyActor(Pawn);
            return false;
        }
        // PM is the sole subscriber in this stub world; IsBound() is sufficient.
        TestTrue(TEXT("TC4-d: RSMSubsystem->OnStateChanged.IsBound() post-BeginPlay"),
                 PM->RSMSubsystem->OnStateChanged.IsBound());

        // (e + f) EndPlay via DestroyActor — no crash expected.
        // Direct EndPlay call first (double-EndPlay test per story AC).
        PM->EndPlay(EEndPlayReason::Destroyed);
        TestFalse(TEXT("TC4-f: PrimaryComponentTick.bCanEverTick == false after direct EndPlay"),
                  PM->PrimaryComponentTick.bCanEverTick);

        // (g) Second EndPlay must not crash (double-Remove on FDelegateHandle is safe
        // in UE multicast — Remove on an invalid handle is a no-op).
        PM->EndPlay(EEndPlayReason::Destroyed);

        // Post-EndPlay broadcast verification (Story 001 QA — Delegate lifecycle Then (b)):
        // Prove that EndPlay actually detached the handler — broadcasting OnStateChanged
        // must NOT invoke HandleStateChanged, so the test-only call counter stays put.
        // RSMSubsystem is still valid here (Remove doesn't destroy the subsystem itself;
        // it only removes the callback). Fix 5c per code-review 2026-07-11.
        const int32 CallsBeforeBroadcast = PM->HandleStateChanged_TestOnlyCallCount;
        if (IsValid(PM->RSMSubsystem))
        {
            PM->RSMSubsystem->OnStateChanged.Broadcast(
                ERunState::IDLE, ERunState::RUNNING, ERunOutcome::NONE, 0.0);
        }
        TestEqual(
            TEXT("TC4-broadcast: HandleStateChanged NOT invoked post-EndPlay"),
            PM->HandleStateChanged_TestOnlyCallCount,
            CallsBeforeBroadcast);

        // Now destroy the actor (triggers its own EndPlay; third removal, must be safe).
        TestWorld->DestroyActor(Pawn);

        // (h) Edge case: destroy a second pawn after nulling RSMSubsystem manually
        // to simulate the "RSM GC'd before EndPlay" scenario.
        // RSMSubsystem pointer is set to null via friend access before EndPlay.
        {
            AddExpectedError(TEXT("SlipCurve is null"),       EAutomationExpectedErrorFlags::Contains, 1);
            AddExpectedError(TEXT("LeanCurve is null"),       EAutomationExpectedErrorFlags::Contains, 1);
            AddExpectedError(TEXT("EdgeAbsorbCurve is null"), EAutomationExpectedErrorFlags::Contains, 1);

            ASlipstormPlayerPawn* Pawn2 = TestWorld->SpawnActorDeferred<ASlipstormPlayerPawn>(
                ASlipstormPlayerPawn::StaticClass(), FTransform::Identity);
            if (TestNotNull(TEXT("TC4-h: Pawn2 SpawnActorDeferred returned null"), Pawn2))
            {
                UGameplayStatics::FinishSpawningActor(Pawn2, FTransform::Identity);
                UPlayerLaneMovementComponent* PM2 = Pawn2->MovementComponent.Get();
                if (PM2)
                {
                    // Grab a reference to the world's actual RSMSubsystem before nulling
                    // PM2's cached pointer — we need it for the broadcast verification
                    // below (fix 5c mirror per code-review 2026-07-11).
                    URunStateMachineSubsystem* WorldRSM = TestWorld->GetGameInstance()
                        ? TestWorld->GetGameInstance()->GetSubsystem<URunStateMachineSubsystem>()
                        : nullptr;
                    const int32 PM2CallsBeforeBroadcast = PM2->HandleStateChanged_TestOnlyCallCount;

                    // Simulate RSM being GC'd: null the cached pointer directly
                    // (friend access to private RSMSubsystem).
                    PM2->RSMSubsystem = nullptr;
                    // EndPlay with nulled RSMSubsystem must not crash — the
                    // IsValid(RSMSubsystem) guard in EndPlay protects the Remove calls.
                    PM2->EndPlay(EEndPlayReason::Destroyed);

                    // Broadcast on the world's RSM — PM2's handler was Removed at BeginPlay
                    // subscribe time (since it bound before we nulled the cached ptr),
                    // so no invocation is expected. Verifies unbind path even when the
                    // cached pointer was subsequently lost.
                    if (WorldRSM)
                    {
                        WorldRSM->OnStateChanged.Broadcast(
                            ERunState::IDLE, ERunState::RUNNING, ERunOutcome::NONE, 0.0);
                    }
                    TestEqual(
                        TEXT("TC4-h-broadcast: HandleStateChanged NOT invoked post-EndPlay (nulled-cache path)"),
                        PM2->HandleStateChanged_TestOnlyCallCount,
                        PM2CallsBeforeBroadcast);
                }
                TestWorld->DestroyActor(Pawn2);
            }
        }

        return true;
    }

    // -----------------------------------------------------------------------
    // TC5 — Seam 12 production provider: constructor and proxy semantics.
    //
    // Given: valid UPlayerLaneMovementComponent* with default-initialized fields.
    // When: FPlayerMovementProvider_Production Provider(PM) constructed.
    // Then:
    //   - Provider.GetMovementState() == EMovementState::SETTLED (default).
    //   - Provider.GetCurrentLane()   == EPlayerLane::Center (default).
    //   - No compile errors on ordinal cast (AC-SS-B static_assert guards drift).
    // Proxy mutation: mutate PM fields, Provider reflects them.
    //
    // No world spawn required — FPlayerMovementProvider_Production holds a
    // TWeakObjectPtr and safe-defaults on null; NewObject is sufficient here.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("seam12_provider_proxy"))
    {
        UPlayerLaneMovementComponent* PM =
            NewObject<UPlayerLaneMovementComponent>();

        FPlayerMovementProvider_Production Provider(PM);

        TestEqual(
            TEXT("TC5: Provider.GetMovementState() == SETTLED by default"),
            Provider.GetMovementState(),
            EMovementState::SETTLED);

        TestEqual(
            TEXT("TC5: Provider.GetCurrentLane() == Center by default"),
            Provider.GetCurrentLane(),
            EPlayerLane::Center);

        // Proxy mutation check.
        PM->current_lane   = EPlayerLane::Right;
        PM->movement_state = ERunSlipState::SLIPPING;

        TestEqual(
            TEXT("TC5: Provider.GetCurrentLane() proxies PM->current_lane after mutation"),
            Provider.GetCurrentLane(),
            EPlayerLane::Right);

        TestEqual(
            TEXT("TC5: Provider.GetMovementState() proxies PM->movement_state after mutation"),
            Provider.GetMovementState(),
            EMovementState::SLIPPING);

        return true;
    }

    // -----------------------------------------------------------------------
    // TC6 — Pawn subobject wiring + attachment.
    //
    // Given: ASlipstormPlayerPawn spawned via SpawnActor into a test world.
    // When: SpawnActor completes (BeginPlay runs).
    // Then:
    //   - pawn->MovementComponent != nullptr.
    //   - pawn->RootSceneComponent != nullptr.
    //   - pawn->GetRootComponent() == pawn->RootSceneComponent (story-001a addition).
    //   - pawn->MeshComponent != nullptr.
    //   - pawn->MeshComponent->GetAttachParent() == pawn->RootSceneComponent.
    //   - pawn->MovementComponent->GetOwner() == pawn (real spawn context — verified
    //     at BeginPlay, not construction).
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("pawn_subobject_wiring"))
    {
        UWorld* TestWorld = CreateTestPlayWorld(this, TEXT("TC6"));
        if (!TestWorld)
        {
            return false;
        }

        // Suppress expected BeginPlay errors — wiring test, not curve test.
        AddExpectedError(TEXT("SlipCurve is null"),       EAutomationExpectedErrorFlags::Contains, 1);
        AddExpectedError(TEXT("LeanCurve is null"),       EAutomationExpectedErrorFlags::Contains, 1);
        AddExpectedError(TEXT("EdgeAbsorbCurve is null"), EAutomationExpectedErrorFlags::Contains, 1);

        ASlipstormPlayerPawn* Pawn = TestWorld->SpawnActor<ASlipstormPlayerPawn>(
            ASlipstormPlayerPawn::StaticClass(), FTransform::Identity);
        if (!TestNotNull(TEXT("TC6: SpawnActor returned null"), Pawn))
        {
            return false;
        }

        TestNotNull(TEXT("TC6: MovementComponent non-null"),
                    Pawn->MovementComponent.Get());

        TestNotNull(TEXT("TC6: RootSceneComponent non-null"),
                    Pawn->RootSceneComponent.Get());

        // Story 001a addition: GetRootComponent() must equal RootSceneComponent.
        TestEqual(TEXT("TC6: GetRootComponent() == RootSceneComponent"),
                  Pawn->GetRootComponent(),
                  static_cast<USceneComponent*>(Pawn->RootSceneComponent.Get()));

        TestNotNull(TEXT("TC6: MeshComponent non-null"),
                    Pawn->MeshComponent.Get());

        if (Pawn->MeshComponent && Pawn->RootSceneComponent)
        {
            TestEqual(TEXT("TC6: MeshComponent attached to RootSceneComponent"),
                      Pawn->MeshComponent->GetAttachParent(),
                      static_cast<USceneComponent*>(Pawn->RootSceneComponent.Get()));
        }

        // Story 001a addition: GetOwner() == pawn at BeginPlay (real spawn context).
        if (Pawn->MovementComponent)
        {
            TestEqual(TEXT("TC6: MovementComponent->GetOwner() == Pawn post-SpawnActor"),
                      Pawn->MovementComponent->GetOwner(),
                      static_cast<AActor*>(Pawn));
        }

        TestWorld->DestroyActor(Pawn);
        return true;
    }

    // Unknown parameter — fail explicitly rather than silently returning true.
    AddError(FString::Printf(TEXT("FPMLifecycleAndSeamTest::RunTest — unknown Parameters value: '%s'"), *Parameters));
    return false;
}

#endif // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

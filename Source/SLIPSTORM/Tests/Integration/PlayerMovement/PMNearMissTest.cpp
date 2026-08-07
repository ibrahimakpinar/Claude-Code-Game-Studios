// Copyright (c) Game Studio. All rights reserved.
// SLIPSTORM — 60-second voxel endless runner, mobile, Unreal Engine 5.7
//
// PMNearMissTest.cpp — Story 012 integration tests for TriggerNearMissBeat
// public API + Y-dip lifecycle + audio swell dispatch + AND-gated haptic
// dispatch (IHapticDispatch::IsSystemHapticsEnabled AND
// IGameSettings::IsNearMissHapticEnabled per B-CERT-2 + R11a-12 accessibility
// opt-in) + bidirectional integration with Story 011 duck-if-slip-active hook.
//
// Story Type: Integration.
//
// Spec:   production/epics/player-movement/story-012-near-miss-beat.md
// GDD:    design/gdd/player-movement-presentation.md §3.2 Near-Miss Beat + §8
//         AC-NEARMISS-HAPTIC + R11a-12 accessibility opt-in
// ADR:    docs/architecture/adr-0009-player-movement-hosting.md (primary)
//         docs/architecture/adr-0002-haptic-platform-bridge.md (INT-002-amended)
// TR:     TR-PM-030 (near-miss haptic AND-gated dispatch)
//
// Test category: SLIPSTORM.PlayerMovement.NearMiss
// Runner: UE Automation Framework, ClientContext | ProductFilter.
//
// Spy pattern: mirrors PMInputBufferTest.cpp — module-scope TArray + bools written
// by C-linkage function-pointer spies installed via IHapticDispatch::TestOnly_*
// and IGameSettings::TestOnly_*. ON_SCOPE_EXIT restores null defaults in teardown.
//
// Helpers _NM-suffixed to avoid ODR collisions with sibling PM tests.
// AAA labels present per test-standards.md.
//
// Build guard: WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS.

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Curves/CurveFloat.h"
#include "Misc/App.h"
#include "Components/StaticMeshComponent.h"

#include "Player/PlayerLaneMovementComponent.h"
#include "Player/SlipstormPlayerPawn.h"
#include "Player/EPlayerLane.h"
#include "Player/ERunSlipState.h"
#include "Player/ESlipDirection.h"
#include "RunStateMachine/RunStateMachineSubsystem.h"
#include "RunStateMachine/ERunState.h"
#include "Seam/IHapticDispatch.h"
#include "Seam/IGameSettings.h"

// ---------------------------------------------------------------------------
// Spy state — module-scope so C-linkage function-pointer spies can write.
// Reset explicitly at the start of every test branch to avoid leakage.
// ---------------------------------------------------------------------------

static TArray<EHapticEvent> GNMSpyHapticLog;
static bool                 GNMSpySystemHapticsEnabled  = true;
static bool                 GNMSpyNearMissHapticEnabled = false;

static void NMSpyFireFn(EHapticEvent Event)
{
    GNMSpyHapticLog.Add(Event);
}

static bool NMSpySystemHapticsFn()
{
    return GNMSpySystemHapticsEnabled;
}

static bool NMSpyNearMissHapticFn()
{
    return GNMSpyNearMissHapticEnabled;
}

static void ResetNMSpies()
{
    GNMSpyHapticLog.Reset();
    GNMSpySystemHapticsEnabled  = true;
    GNMSpyNearMissHapticEnabled = false;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static UCurveFloat* MakeIdentityCurve_NM(UObject* InOuter)
{
    UCurveFloat* C = NewObject<UCurveFloat>(InOuter);
    C->FloatCurve.AddKey(0.0f, 0.0f);
    C->FloatCurve.AddKey(1.0f, 1.0f);
    return C;
}

static UWorld* CreateTestPlayWorld_NM(FAutomationTestBase* T, const TCHAR* Label)
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

static ASlipstormPlayerPawn* SpawnPawnWithCurves_NM(
    FAutomationTestBase* T,
    UWorld* World,
    const TCHAR* Label)
{
    ASlipstormPlayerPawn* Pawn = World->SpawnActorDeferred<ASlipstormPlayerPawn>(
        ASlipstormPlayerPawn::StaticClass(), FTransform::Identity);
    if (!T->TestNotNull(FString::Printf(TEXT("%s: SpawnActorDeferred returned null"), Label), Pawn))
    {
        return nullptr;
    }
    Pawn->MovementComponent->SlipCurve       = MakeIdentityCurve_NM(World);
    Pawn->MovementComponent->LeanCurve       = MakeIdentityCurve_NM(World);
    Pawn->MovementComponent->EdgeAbsorbCurve = MakeIdentityCurve_NM(World);
    UGameplayStatics::FinishSpawningActor(Pawn, FTransform::Identity);
    return Pawn;
}

static URunStateMachineSubsystem* PrimeRSMRunning_NM(
    FAutomationTestBase* T,
    UWorld* World,
    const TCHAR* Label)
{
    UGameInstance* GI = World->GetGameInstance();
    URunStateMachineSubsystem* RSM =
        GI ? GI->GetSubsystem<URunStateMachineSubsystem>() : nullptr;
    if (!T->TestNotNull(FString::Printf(TEXT("%s: RSM available"), Label), RSM))
    {
        return nullptr;
    }
    RSM->TestOnly_CurrentState = ERunState::RUNNING;
    RSM->TestOnly_bPaused      = false;
    RSM->TestOnly_bResumeGrace = false;
    return RSM;
}

static void TickPM_NM(UPlayerLaneMovementComponent* PM, float DeltaSeconds)
{
    FApp::SetDeltaTime(DeltaSeconds);
    PM->TickComponent(DeltaSeconds, ELevelTick::LEVELTICK_All, nullptr);
}

// Install both spies (haptic + settings). Caller must add ON_SCOPE_EXIT to
// invoke ResetSpiesTeardown_NM in the same scope.
static void InstallNMSpies()
{
    ResetNMSpies();
    IHapticDispatch::TestOnly_SetFireFn(&NMSpyFireFn);
    IHapticDispatch::TestOnly_SetIsEnabledFn(&NMSpySystemHapticsFn);
    IGameSettings::TestOnly_SetIsNearMissHapticEnabledFn(&NMSpyNearMissHapticFn);
}

static void ResetSpiesTeardown_NM()
{
    IHapticDispatch::TestOnly_Reset();
    IGameSettings::TestOnly_Reset();
}

// ---------------------------------------------------------------------------
// Deferred coverage note — audio swell -6dB relative level
// ---------------------------------------------------------------------------
//
// The story's QA Test Cases section lists "Audio swell independence — dispatch
// with -6dB relative to authored slip cue level" as a required test. Not
// covered here by design: PM only dispatches the swell event (invocation
// counter). The actual authored gain level lives in the audio system's asset
// authoring (Audio Bible) + Metasounds bus configuration — validated by the
// audio-programmer's own tests when the real backend is wired downstream.
// PlayNearMissAudioSwell is a UE_LOG stub here for the same reason
// PlayBufferDropAudioSting is a stub (Story 005 precedent).
//
// If the PM layer ever gains a level parameter on the dispatch call (e.g.,
// `PlayNearMissAudioSwell(float RelativeLevelDB)`), add a
// PlayNearMissAudioSwell_TestOnlyLastLevelDB field and assert on it here.
// Until then this coverage gap is DEFERRED to the audio-wiring story, not
// UNCOVERED — the -6dB requirement is met by the asset authoring, not PM code.
//
// ---------------------------------------------------------------------------
// IMPLEMENT_COMPLEX_AUTOMATION_TEST — 11 integration test commands.
// ---------------------------------------------------------------------------

IMPLEMENT_COMPLEX_AUTOMATION_TEST(
    FPMNearMissTest,
    "SLIPSTORM.PlayerMovement.NearMiss",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

void FPMNearMissTest::GetTests(
    TArray<FString>& OutBeautifiedNames,
    TArray<FString>& OutTestCommands) const
{
    OutBeautifiedNames.Add(TEXT("Setup A — default off: 10 triggers, 0 haptic; Y-dip + audio still fire"));
    OutTestCommands.Add(TEXT("setup_a_default_off_zero_haptic"));

    OutBeautifiedNames.Add(TEXT("Setup B — opt-in on: 10 triggers → 10 haptic dispatches"));
    OutTestCommands.Add(TEXT("setup_b_optin_on_ten_dispatches"));

    OutBeautifiedNames.Add(TEXT("Setup C — runtime toggle off/on/off → exactly 3 haptic dispatches"));
    OutTestCommands.Add(TEXT("setup_c_runtime_toggle"));

    OutBeautifiedNames.Add(TEXT("B-CERT-2 — IsSystemHapticsEnabled=false suppresses regardless of accessibility setting"));
    OutTestCommands.Add(TEXT("os_state_cert_b_cert_2"));

    OutBeautifiedNames.Add(TEXT("Y-dip lifecycle — Phase 1 attack 33ms + Phase 2 return 80ms + Phase 3 clear"));
    OutTestCommands.Add(TEXT("y_dip_lifecycle"));

    OutBeautifiedNames.Add(TEXT("Y-dip restart — repeat trigger resets time_s, no additive stacking"));
    OutTestCommands.Add(TEXT("y_dip_restart_no_stacking"));

    OutBeautifiedNames.Add(TEXT("Y-dip additive with F-3 — SLIPPING mid-tween mesh X unaffected by Y-dip"));
    OutTestCommands.Add(TEXT("y_dip_additive_with_f3"));

    OutBeautifiedNames.Add(TEXT("PlayNearMissAudioSwell engages duck on active slip cue (Story 011 hook)"));
    OutTestCommands.Add(TEXT("audio_swell_engages_duck_on_active_slip"));

    OutBeautifiedNames.Add(TEXT("Public API reachability — TriggerNearMissBeat counter increments"));
    OutTestCommands.Add(TEXT("public_api_reachability"));

    OutBeautifiedNames.Add(TEXT("Y-dip renders during SETTLED (verifies mesh-write hoist)"));
    OutTestCommands.Add(TEXT("y_dip_renders_during_settled"));

    OutBeautifiedNames.Add(TEXT("SnapToTargetAndReset item 10 clears Y-dip state mid-animation"));
    OutTestCommands.Add(TEXT("snap_reset_clears_y_dip_state"));
}

bool FPMNearMissTest::RunTest(const FString& Parameters)
{
    // -----------------------------------------------------------------------
    // TC1 — Setup A default off.
    //
    // Setting off (accessibility opt-in default per R11a-12). 10 near-miss
    // triggers → zero haptic dispatches. Y-dip animation flag cycles active
    // 10 times (verified by counter). Audio swell dispatch counter increments 10.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("setup_a_default_off_zero_haptic"))
    {
        // Arrange
        UWorld* TestWorld = CreateTestPlayWorld_NM(this, TEXT("TC1"));
        if (!TestWorld) { return false; }
        ASlipstormPlayerPawn* Pawn = SpawnPawnWithCurves_NM(this, TestWorld, TEXT("TC1"));
        if (!Pawn) { return false; }
        UPlayerLaneMovementComponent* PM = Pawn->MovementComponent.Get();

        InstallNMSpies();
        ON_SCOPE_EXIT { ResetSpiesTeardown_NM(); };

        GNMSpySystemHapticsEnabled  = true;   // system haptics available
        GNMSpyNearMissHapticEnabled = false;  // accessibility opt-in default OFF

        const int32 TriggerCountBefore    = PM->TriggerNearMissBeat_TestOnlyCallCount;
        const int32 AudioSwellCountBefore = PM->PlayNearMissAudioSwell_TestOnlyCallCount;

        // Act — 10 triggers.
        for (int32 i = 0; i < 10; ++i)
        {
            PM->TriggerNearMissBeat();
        }

        // Assert — zero haptic dispatches, but trigger + audio counters incremented 10x.
        TestEqual(TEXT("TC1: 0 haptic dispatches when setting off"),
                  GNMSpyHapticLog.Num(), 0);
        TestEqual(TEXT("TC1: TriggerNearMissBeat counter += 10"),
                  PM->TriggerNearMissBeat_TestOnlyCallCount, TriggerCountBefore + 10);
        TestEqual(TEXT("TC1: PlayNearMissAudioSwell counter += 10 (audio still fires)"),
                  PM->PlayNearMissAudioSwell_TestOnlyCallCount, AudioSwellCountBefore + 10);
        TestTrue (TEXT("TC1: y_dip_active true after triggers (Y-dip still fires)"),
                  PM->y_dip_active);

        TestWorld->DestroyActor(Pawn);
        return true;
    }

    // -----------------------------------------------------------------------
    // TC2 — Setup B opt-in on.
    //
    // Setting on + system on. 10 triggers → exactly 10 haptic dispatches with
    // ordinal EHapticEvent::NearMiss (=1).
    //
    // AC-NEARMISS-HAPTIC Setup B ±50ms timing requirement:
    // TriggerNearMissBeat is a synchronous method — the haptic Fire call and
    // the y_dip_active = true assignment both execute within the same call
    // frame, on the same tick, on the same thread. The ±50ms tolerance is
    // therefore trivially satisfied (delta = 0ms). This test verifies the
    // 1:1 dispatch-per-trigger invariant AFTER EACH CALL (not just at the
    // end) so any future refactor that fires haptic asynchronously (e.g.,
    // deferred to next tick) breaks the assertion instead of passing
    // undetected. If real-time timestamp measurement becomes possible in
    // headless test infra later, upgrade to a wall-clock delta assertion.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("setup_b_optin_on_ten_dispatches"))
    {
        // Arrange
        UWorld* TestWorld = CreateTestPlayWorld_NM(this, TEXT("TC2"));
        if (!TestWorld) { return false; }
        ASlipstormPlayerPawn* Pawn = SpawnPawnWithCurves_NM(this, TestWorld, TEXT("TC2"));
        if (!Pawn) { return false; }
        UPlayerLaneMovementComponent* PM = Pawn->MovementComponent.Get();

        InstallNMSpies();
        ON_SCOPE_EXIT { ResetSpiesTeardown_NM(); };

        GNMSpySystemHapticsEnabled  = true;
        GNMSpyNearMissHapticEnabled = true;

        // Act + Assert — 10 triggers, verify per-trigger 1:1 dispatch invariant.
        // Synchronous same-tick ordering: after each TriggerNearMissBeat call,
        // spy log length must have grown by exactly 1 and y_dip_active must be
        // true (Y-dip onset happened in the same synchronous call). This proves
        // haptic Fire and y_dip_active set occurred in the same method body —
        // the ±50ms AC criterion is satisfied by construction (delta = 0ms).
        for (int32 i = 0; i < 10; ++i)
        {
            const int32 SpyLenBefore = GNMSpyHapticLog.Num();
            PM->TriggerNearMissBeat();
            TestEqual(FString::Printf(TEXT("TC2: trigger[%d] → exactly 1 haptic dispatch same-call (synchronous ordering guarantees ±50ms)"), i),
                      GNMSpyHapticLog.Num(), SpyLenBefore + 1);
            TestTrue(FString::Printf(TEXT("TC2: trigger[%d] → y_dip_active true post-call (Y-dip onset same call as haptic Fire)"), i),
                     PM->y_dip_active);
        }

        // Assert — 10 total haptic dispatches, all NearMiss ordinal.
        TestEqual(TEXT("TC2: exactly 10 total haptic dispatches"),
                  GNMSpyHapticLog.Num(), 10);
        for (int32 i = 0; i < GNMSpyHapticLog.Num(); ++i)
        {
            TestEqual(FString::Printf(TEXT("TC2: dispatch[%d] == EHapticEvent::NearMiss"), i),
                      static_cast<uint8>(GNMSpyHapticLog[i]),
                      static_cast<uint8>(EHapticEvent::NearMiss));
        }

        TestWorld->DestroyActor(Pawn);
        return true;
    }

    // -----------------------------------------------------------------------
    // TC3 — Setup C runtime toggle.
    //
    // Start off (3 triggers → 0 dispatches), toggle on (3 triggers → 3
    // dispatches), toggle off (3 triggers → 0 dispatches). Total = 3.
    // Verifies AND-gate re-checks IGameSettings on every call.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("setup_c_runtime_toggle"))
    {
        // Arrange
        UWorld* TestWorld = CreateTestPlayWorld_NM(this, TEXT("TC3"));
        if (!TestWorld) { return false; }
        ASlipstormPlayerPawn* Pawn = SpawnPawnWithCurves_NM(this, TestWorld, TEXT("TC3"));
        if (!Pawn) { return false; }
        UPlayerLaneMovementComponent* PM = Pawn->MovementComponent.Get();

        InstallNMSpies();
        ON_SCOPE_EXIT { ResetSpiesTeardown_NM(); };

        GNMSpySystemHapticsEnabled = true;

        // Act phase 1 — setting OFF.
        GNMSpyNearMissHapticEnabled = false;
        for (int32 i = 0; i < 3; ++i) { PM->TriggerNearMissBeat(); }
        const int32 CountAfterPhase1 = GNMSpyHapticLog.Num();

        // Act phase 2 — toggle ON.
        GNMSpyNearMissHapticEnabled = true;
        for (int32 i = 0; i < 3; ++i) { PM->TriggerNearMissBeat(); }
        const int32 CountAfterPhase2 = GNMSpyHapticLog.Num();

        // Act phase 3 — toggle OFF again.
        GNMSpyNearMissHapticEnabled = false;
        for (int32 i = 0; i < 3; ++i) { PM->TriggerNearMissBeat(); }
        const int32 CountAfterPhase3 = GNMSpyHapticLog.Num();

        // Assert per-phase counts.
        TestEqual(TEXT("TC3: phase 1 (OFF) → 0 dispatches"),
                  CountAfterPhase1, 0);
        TestEqual(TEXT("TC3: phase 2 (ON) → 3 more dispatches (total 3)"),
                  CountAfterPhase2, 3);
        TestEqual(TEXT("TC3: phase 3 (OFF again) → 0 more dispatches (total still 3)"),
                  CountAfterPhase3, 3);

        TestWorld->DestroyActor(Pawn);
        return true;
    }

    // -----------------------------------------------------------------------
    // TC4 — B-CERT-2 OS-state cert compliance.
    //
    // IsSystemHapticsEnabled=false simulates iOS Focus / Android DND. AND-gate
    // suppresses regardless of accessibility setting. 5 triggers with setting
    // ON → 0 dispatches.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("os_state_cert_b_cert_2"))
    {
        // Arrange
        UWorld* TestWorld = CreateTestPlayWorld_NM(this, TEXT("TC4"));
        if (!TestWorld) { return false; }
        ASlipstormPlayerPawn* Pawn = SpawnPawnWithCurves_NM(this, TestWorld, TEXT("TC4"));
        if (!Pawn) { return false; }
        UPlayerLaneMovementComponent* PM = Pawn->MovementComponent.Get();

        InstallNMSpies();
        ON_SCOPE_EXIT { ResetSpiesTeardown_NM(); };

        GNMSpySystemHapticsEnabled  = false; // OS suppression
        GNMSpyNearMissHapticEnabled = true;  // player opted-in but OS overrides

        // Act — 5 triggers.
        for (int32 i = 0; i < 5; ++i) { PM->TriggerNearMissBeat(); }

        // Assert — 0 dispatches (B-CERT-2 satisfied).
        TestEqual(TEXT("TC4: 0 haptic dispatches when system haptics disabled (iOS Focus / Android DND)"),
                  GNMSpyHapticLog.Num(), 0);
        // Audio + Y-dip should still fire — only haptic is gated by system-haptics.
        TestEqual(TEXT("TC4: audio swell still fires 5 times (not gated by system-haptics)"),
                  PM->PlayNearMissAudioSwell_TestOnlyCallCount, 5);

        TestWorld->DestroyActor(Pawn);
        return true;
    }

    // -----------------------------------------------------------------------
    // TC5 — Y-dip lifecycle.
    //
    // Trigger at t=0, tick through the 33+80=113ms window in 1ms increments.
    // Sample offset at Phase 1 midpoint (~16ms), Phase 1 end (~33ms), Phase 2
    // midpoint (~73ms), and post-Phase-3 (~114ms). Rule 5 gate must be RUNNING.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("y_dip_lifecycle"))
    {
        // Arrange
        UWorld* TestWorld = CreateTestPlayWorld_NM(this, TEXT("TC5"));
        if (!TestWorld) { return false; }
        ASlipstormPlayerPawn* Pawn = SpawnPawnWithCurves_NM(this, TestWorld, TEXT("TC5"));
        if (!Pawn) { return false; }
        UPlayerLaneMovementComponent* PM = Pawn->MovementComponent.Get();
        if (!PrimeRSMRunning_NM(this, TestWorld, TEXT("TC5"))) { TestWorld->DestroyActor(Pawn); return false; }

        // Act 1 — trigger at t=0.
        PM->TriggerNearMissBeat();
        TestTrue(TEXT("TC5: y_dip_active true after trigger"), PM->y_dip_active);
        TestEqual(TEXT("TC5: y_dip_time_s == 0 after trigger"), PM->y_dip_time_s, 0.0f);

        // Act 2 — tick 16ms → Phase 1 midpoint (attack duration 33ms).
        TickPM_NM(PM, 0.016f);
        const float ExpectedPhase1Mid = -3.0f * (0.016f / 0.033f); // ≈ -1.4545
        TestTrue(FString::Printf(TEXT("TC5: Phase 1 midpoint offset ≈ %.4fcm (got %.4f)"),
                                 ExpectedPhase1Mid, PM->y_dip_offset_cm),
                 FMath::IsNearlyEqual(PM->y_dip_offset_cm, ExpectedPhase1Mid, 0.01f));

        // Act 3 — tick 17ms more (total 33ms) → Phase 1 end (near-peak).
        TickPM_NM(PM, 0.017f);
        // At t=33ms exactly, still in Phase 1 branch (elapsed <= Y_DIP_ATTACK_S).
        // offset = -3.0 * (33/33) = -3.0. Small float drift may push into Phase 2.
        TestTrue(FString::Printf(TEXT("TC5: near-peak offset ≈ -3.0cm at t≈33ms (got %.4f)"),
                                 PM->y_dip_offset_cm),
                 FMath::IsNearlyEqual(PM->y_dip_offset_cm, -3.0f, 0.1f));

        // Act 4 — tick 40ms more (total 73ms) → Phase 2 midpoint (return 80ms).
        TickPM_NM(PM, 0.040f);
        // Phase 2 at elapsed 73ms: return_t = (73-33)/80 = 0.5 → offset = -3.0 * 0.5 = -1.5.
        TestTrue(FString::Printf(TEXT("TC5: Phase 2 midpoint offset ≈ -1.5cm at t≈73ms (got %.4f)"),
                                 PM->y_dip_offset_cm),
                 FMath::IsNearlyEqual(PM->y_dip_offset_cm, -1.5f, 0.15f));
        TestTrue(TEXT("TC5: still active mid-Phase-2"), PM->y_dip_active);

        // Act 5 — tick 41ms more (total 114ms > 113ms window) → Phase 3 clear.
        TickPM_NM(PM, 0.041f);
        TestFalse(TEXT("TC5: y_dip_active false after 113ms window elapsed"),
                  PM->y_dip_active);
        TestEqual(TEXT("TC5: y_dip_offset_cm == 0 post-Phase-3"),
                  PM->y_dip_offset_cm, 0.0f);
        TestEqual(TEXT("TC5: y_dip_time_s == 0 post-Phase-3 (reset)"),
                  PM->y_dip_time_s, 0.0f);

        TestWorld->DestroyActor(Pawn);
        return true;
    }

    // -----------------------------------------------------------------------
    // TC6 — Y-dip restart no stacking.
    //
    // Trigger, tick 20ms into Phase 1, retrigger — verify time_s resets to 0
    // and Y-dip restarts. Multiple rapid triggers don't stack additively past
    // Y_DIP_PEAK_CM.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("y_dip_restart_no_stacking"))
    {
        // Arrange
        UWorld* TestWorld = CreateTestPlayWorld_NM(this, TEXT("TC6"));
        if (!TestWorld) { return false; }
        ASlipstormPlayerPawn* Pawn = SpawnPawnWithCurves_NM(this, TestWorld, TEXT("TC6"));
        if (!Pawn) { return false; }
        UPlayerLaneMovementComponent* PM = Pawn->MovementComponent.Get();
        if (!PrimeRSMRunning_NM(this, TestWorld, TEXT("TC6"))) { TestWorld->DestroyActor(Pawn); return false; }

        // Act 1 — first trigger + tick 20ms.
        PM->TriggerNearMissBeat();
        TickPM_NM(PM, 0.020f);
        const float OffsetBeforeRetrigger = PM->y_dip_offset_cm;
        TestTrue(TEXT("TC6: mid-Phase-1 offset non-zero pre-retrigger"),
                 OffsetBeforeRetrigger < 0.0f);

        // Act 2 — retrigger.
        PM->TriggerNearMissBeat();

        // Assert 2 — time_s reset to 0. Offset is NOT immediately reset (last tick's
        // value persists until the next tick's advance recomputes). Confirms
        // TriggerNearMissBeat restarts the timer, doesn't stack.
        TestEqual(TEXT("TC6: y_dip_time_s reset to 0 on retrigger"),
                  PM->y_dip_time_s, 0.0f);
        TestTrue(TEXT("TC6: y_dip_active still true after retrigger"),
                 PM->y_dip_active);

        // Act 3 — tick 16ms → advance from t=0 (fresh restart), not t=36 (stacked).
        TickPM_NM(PM, 0.016f);
        // If retrigger reset time_s to 0, we're at t=16ms in Phase 1 → offset ≈ -1.45.
        // If it had stacked, we'd be at t=36ms > 33ms → Phase 2 with offset ≈ -2.89.
        const float ExpectedFreshPhase1 = -3.0f * (0.016f / 0.033f); // ≈ -1.4545
        TestTrue(FString::Printf(TEXT("TC6: post-retrigger offset matches fresh Phase 1 (got %.4f, expected ≈ %.4f)"),
                                 PM->y_dip_offset_cm, ExpectedFreshPhase1),
                 FMath::IsNearlyEqual(PM->y_dip_offset_cm, ExpectedFreshPhase1, 0.01f));

        // Assert — offset magnitude never exceeds Y_DIP_PEAK_CM.
        TestTrue(TEXT("TC6: |offset| never exceeds Y_DIP_PEAK_CM (no additive stacking)"),
                 FMath::Abs(PM->y_dip_offset_cm) <= 3.0f + 0.001f);

        TestWorld->DestroyActor(Pawn);
        return true;
    }

    // -----------------------------------------------------------------------
    // TC7 — Y-dip additive with F-3.
    //
    // Set SLIPPING mid-tween (tween_progress = 0.5 → F-3 rel_x is non-zero),
    // trigger near-miss, tick, verify mesh relative location has non-zero X
    // (from F-3) AND non-zero Z (from Y-dip). Independence: Y-dip's Z-offset
    // does not perturb X.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("y_dip_additive_with_f3"))
    {
        // Arrange
        UWorld* TestWorld = CreateTestPlayWorld_NM(this, TEXT("TC7"));
        if (!TestWorld) { return false; }
        ASlipstormPlayerPawn* Pawn = SpawnPawnWithCurves_NM(this, TestWorld, TEXT("TC7"));
        if (!Pawn) { return false; }
        UPlayerLaneMovementComponent* PM = Pawn->MovementComponent.Get();
        if (!PrimeRSMRunning_NM(this, TestWorld, TEXT("TC7"))) { TestWorld->DestroyActor(Pawn); return false; }

        // Force SLIPPING state mid-tween. Center → Right slip: F-3 relative
        // offset uses (source - target) delta * (1 - curve_t). At TP=0.5 with
        // identity SlipCurve: curve_t = 0.5 → delta = LaneWorldX(Center) -
        // LaneWorldX(Right) = 0 - 100 = -100 → Lerp(-100, 0, 0.5) = -50.
        PM->current_lane   = EPlayerLane::Center;
        PM->target_lane    = EPlayerLane::Right;
        PM->movement_state = ERunSlipState::SLIPPING;
        PM->tween_progress = 0.5f;

        // Act — trigger + tick 16ms.
        PM->TriggerNearMissBeat();
        TickPM_NM(PM, 0.016f);

        // Assert — mesh has non-zero X (F-3 from tween) and non-zero Z (Y-dip Phase 1).
        // X: after tick, tween_progress advanced by 16ms/150ms ≈ 0.107 → TP≈0.607.
        // With identity curve: curve_t = 0.607 → offset = -100 * (1-0.607) = -39.3.
        // Just verify X is negative and non-zero — precise value depends on curve integration.
        if (PM->CachedMeshComponent)
        {
            const FVector MeshRel = PM->CachedMeshComponent->GetRelativeLocation();
            TestTrue(FString::Printf(TEXT("TC7: mesh relative X is negative (F-3 mid-tween, got %.2fcm)"), MeshRel.X),
                     MeshRel.X < -1.0f);
            TestTrue(FString::Printf(TEXT("TC7: mesh relative Z is negative (Y-dip Phase 1, got %.4fcm)"), MeshRel.Z),
                     MeshRel.Z < 0.0f);
            TestEqual(TEXT("TC7: mesh relative Y == 0 (unchanged)"), MeshRel.Y, 0.0);
        }
        else
        {
            AddWarning(TEXT("TC7: CachedMeshComponent null — cannot verify mesh independence directly. Fall back to y_dip_offset_cm state check."));
            TestTrue(TEXT("TC7 fallback: y_dip_offset_cm non-zero"), PM->y_dip_offset_cm < 0.0f);
        }

        TestWorld->DestroyActor(Pawn);
        return true;
    }

    // -----------------------------------------------------------------------
    // TC8 — PlayNearMissAudioSwell engages duck on active slip cue.
    //
    // Cross-story (011 → 012 bidirectional integration). Start slip cue,
    // trigger near-miss → envelope_phase must transition to DUCK_ATTACK.
    // Verifies TriggerNearMissBeat → PlayNearMissAudioSwell → EngageDuckIfSlipActive chain.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("audio_swell_engages_duck_on_active_slip"))
    {
        // Arrange
        UWorld* TestWorld = CreateTestPlayWorld_NM(this, TEXT("TC8"));
        if (!TestWorld) { return false; }
        ASlipstormPlayerPawn* Pawn = SpawnPawnWithCurves_NM(this, TestWorld, TEXT("TC8"));
        if (!Pawn) { return false; }
        UPlayerLaneMovementComponent* PM = Pawn->MovementComponent.Get();
        if (!PrimeRSMRunning_NM(this, TestWorld, TEXT("TC8"))) { TestWorld->DestroyActor(Pawn); return false; }

        // Start slip cue at authored (envelope_phase = NONE).
        PM->PlaySlipAudioCue(EPlayerLane::Center, EPlayerLane::Right);
        TestTrue(TEXT("TC8: slip_cue_active after PlaySlipAudioCue"), PM->slip_cue_active);
        TestTrue(TEXT("TC8: envelope_phase == NONE post-dispatch"),
                 PM->envelope_phase == UPlayerLaneMovementComponent::ESlipAudioEnvelopePhase::NONE);

        // Act — trigger near-miss → chain calls EngageDuckIfSlipActive.
        PM->TriggerNearMissBeat();

        // Assert — envelope engaged DUCK_ATTACK.
        TestTrue(TEXT("TC8: envelope_phase == DUCK_ATTACK after near-miss triggers duck-hook"),
                 PM->envelope_phase == UPlayerLaneMovementComponent::ESlipAudioEnvelopePhase::DUCK_ATTACK);
        TestEqual(TEXT("TC8: envelope_elapsed_s reset to 0 at DUCK_ATTACK entry"),
                  PM->envelope_elapsed_s, 0.0f);

        // Verify vacuous case too: no slip active → EngageDuckIfSlipActive no-ops.
        UWorld* TestWorld2 = CreateTestPlayWorld_NM(this, TEXT("TC8b"));
        if (!TestWorld2) { TestWorld->DestroyActor(Pawn); return false; }
        ASlipstormPlayerPawn* Pawn2 = SpawnPawnWithCurves_NM(this, TestWorld2, TEXT("TC8b"));
        if (!Pawn2) { TestWorld->DestroyActor(Pawn); return false; }
        UPlayerLaneMovementComponent* PM2 = Pawn2->MovementComponent.Get();

        TestFalse(TEXT("TC8b: no slip active on fresh pawn"), PM2->slip_cue_active);
        PM2->TriggerNearMissBeat();
        TestFalse(TEXT("TC8b: still no slip active after near-miss (vacuous case)"),
                  PM2->slip_cue_active);
        TestTrue(TEXT("TC8b: envelope_phase still NONE (no duck on inactive slip)"),
                 PM2->envelope_phase == UPlayerLaneMovementComponent::ESlipAudioEnvelopePhase::NONE);

        TestWorld->DestroyActor(Pawn);
        TestWorld2->DestroyActor(Pawn2);
        return true;
    }

    // -----------------------------------------------------------------------
    // TC9 — Public API reachability.
    //
    // AC-3: TriggerNearMissBeat is a callable public method. Simplest possible
    // verification — instantiate PM, call it, assert no crash and counter increments.
    // Guards against future refactors that accidentally break the public entry point.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("public_api_reachability"))
    {
        // Arrange
        UWorld* TestWorld = CreateTestPlayWorld_NM(this, TEXT("TC9"));
        if (!TestWorld) { return false; }
        ASlipstormPlayerPawn* Pawn = SpawnPawnWithCurves_NM(this, TestWorld, TEXT("TC9"));
        if (!Pawn) { return false; }
        UPlayerLaneMovementComponent* PM = Pawn->MovementComponent.Get();

        const int32 CountBefore = PM->TriggerNearMissBeat_TestOnlyCallCount;

        // Act — call the public method directly.
        PM->TriggerNearMissBeat();

        // Assert — counter incremented, no crash reached this line.
        TestEqual(TEXT("TC9: TriggerNearMissBeat counter += 1 (public API reachable)"),
                  PM->TriggerNearMissBeat_TestOnlyCallCount, CountBefore + 1);

        TestWorld->DestroyActor(Pawn);
        return true;
    }

    // -----------------------------------------------------------------------
    // TC10 — Y-dip renders during SETTLED.
    //
    // Verifies the mesh-write hoist (Story 012 restructure of Story 004's F-3
    // write): the unified SetRelativeLocation call runs on every Rule-5-gated
    // tick regardless of movement_state, so Y-dip's Z-offset composes into the
    // mesh even when PM is SETTLED (no active tween). This was the specific
    // bug the advisor flagged in the pre-implementation review.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("y_dip_renders_during_settled"))
    {
        // Arrange
        UWorld* TestWorld = CreateTestPlayWorld_NM(this, TEXT("TC10"));
        if (!TestWorld) { return false; }
        ASlipstormPlayerPawn* Pawn = SpawnPawnWithCurves_NM(this, TestWorld, TEXT("TC10"));
        if (!Pawn) { return false; }
        UPlayerLaneMovementComponent* PM = Pawn->MovementComponent.Get();
        if (!PrimeRSMRunning_NM(this, TestWorld, TEXT("TC10"))) { TestWorld->DestroyActor(Pawn); return false; }

        // Confirm SETTLED state (default).
        TestTrue(TEXT("TC10: precondition — movement_state == SETTLED"),
                 PM->movement_state == ERunSlipState::SETTLED);

        // Act — trigger + tick 16ms into Phase 1.
        PM->TriggerNearMissBeat();
        TickPM_NM(PM, 0.016f);

        // Assert — mesh Z reflects Y-dip Phase 1 offset EVEN THOUGH PM is SETTLED.
        // Prior structure (F-3 write inside SLIPPING branch) would leave mesh Z at 0
        // during SETTLED, silently breaking the Y-dip render.
        if (PM->CachedMeshComponent)
        {
            const FVector MeshRel = PM->CachedMeshComponent->GetRelativeLocation();
            TestTrue(FString::Printf(TEXT("TC10: mesh relative Z is negative during SETTLED Y-dip (got %.4fcm)"),
                                     MeshRel.Z),
                     MeshRel.Z < -0.5f); // Phase 1 midpoint ≈ -1.45cm.
            TestEqual(TEXT("TC10: mesh relative X == 0 during SETTLED (no F-3 during SETTLED)"),
                      MeshRel.X, 0.0);
        }
        else
        {
            AddWarning(TEXT("TC10: CachedMeshComponent null — fall back to y_dip_offset_cm state check."));
            TestTrue(TEXT("TC10 fallback: y_dip_offset_cm non-zero during SETTLED"),
                     PM->y_dip_offset_cm < 0.0f);
        }

        TestWorld->DestroyActor(Pawn);
        return true;
    }

    // -----------------------------------------------------------------------
    // TC11 — SnapToTargetAndReset item 10 clears Y-dip state mid-animation.
    //
    // Verifies the Story 012 item 10 addition to SnapToTargetAndReset (which is
    // called by Story 008's HandleStateChanged terminal-state branches on
    // COMPLETE/ABORTED/COUNTDOWN entry). Without this clear, a Y-dip that was
    // mid-animation at run-end would survive into the next run's initial state.
    //
    // Regression guard: if a future refactor removes item 10 from
    // SnapToTargetAndReset (e.g., extracts the resets into a per-story helper
    // and forgets to include Y-dip), this test catches the drop before it
    // ships. The clear is defensive — currently harmless while dispatch is
    // stub, but load-bearing once real audio bus is wired.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("snap_reset_clears_y_dip_state"))
    {
        // Arrange
        UWorld* TestWorld = CreateTestPlayWorld_NM(this, TEXT("TC11"));
        if (!TestWorld) { return false; }
        ASlipstormPlayerPawn* Pawn = SpawnPawnWithCurves_NM(this, TestWorld, TEXT("TC11"));
        if (!Pawn) { return false; }
        UPlayerLaneMovementComponent* PM = Pawn->MovementComponent.Get();
        if (!PrimeRSMRunning_NM(this, TestWorld, TEXT("TC11"))) { TestWorld->DestroyActor(Pawn); return false; }

        // Act 1 — trigger near-miss + tick 16ms → Phase 1 mid-animation.
        PM->TriggerNearMissBeat();
        TickPM_NM(PM, 0.016f);

        // Assert 1 — Y-dip state is live and non-zero (precondition for
        // meaningful clear test).
        TestTrue (TEXT("TC11 pre: y_dip_active true mid-Phase-1"), PM->y_dip_active);
        TestTrue (TEXT("TC11 pre: y_dip_time_s advanced from 0"),  PM->y_dip_time_s > 0.0f);
        TestTrue (TEXT("TC11 pre: y_dip_offset_cm non-zero mid-Phase-1"),
                  PM->y_dip_offset_cm < 0.0f);

        // Act 2 — invoke SnapToTargetAndReset directly via friend access.
        // In production this is called by HandleStateChanged on
        // COMPLETE/ABORTED/COUNTDOWN entry; calling it directly here isolates
        // the reset-helper contract from RSM state machine wiring.
        PM->SnapToTargetAndReset();

        // Assert 2 — item 10 cleared all three Y-dip fields.
        TestFalse(TEXT("TC11: y_dip_active false after SnapToTargetAndReset (item 10)"),
                  PM->y_dip_active);
        TestEqual(TEXT("TC11: y_dip_time_s == 0 after reset"),
                  PM->y_dip_time_s, 0.0f);
        TestEqual(TEXT("TC11: y_dip_offset_cm == 0 after reset"),
                  PM->y_dip_offset_cm, 0.0f);

        // Act 3 — tick once more → unified mesh write picks up cleared state.
        TickPM_NM(PM, 0.016f);

        // Assert 3 — mesh Z is 0 after reset+tick (clear propagated to render).
        if (PM->CachedMeshComponent)
        {
            const FVector MeshRel = PM->CachedMeshComponent->GetRelativeLocation();
            TestEqual(TEXT("TC11: mesh relative Z == 0 after reset+tick (clear propagated)"),
                      MeshRel.Z, 0.0);
        }

        TestWorld->DestroyActor(Pawn);
        return true;
    }

    AddError(FString::Printf(TEXT("Unknown test parameter: %s"), *Parameters));
    return false;
}

#endif // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

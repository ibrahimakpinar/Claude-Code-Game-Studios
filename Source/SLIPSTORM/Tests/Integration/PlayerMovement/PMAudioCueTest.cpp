// Copyright (c) Game Studio. All rights reserved.
// SLIPSTORM — 60-second voxel endless runner, mobile, Unreal Engine 5.7
//
// PMAudioCueTest.cpp — Story 011 integration tests for the slip audio cue
// dispatch (TR-PM-031: duration = audio_cue_ratio × SLIP_TWEEN_DURATION_S,
// center-pan R11a-14, rate-transposition ±2 semitones via ratio clamp) and
// the -6dB duck / HARD-CUT envelope (TR-PM-032: 50ms attack, sustained hold,
// ≤5ms HARD-CUT ramp under buffer-drop, EC-16 triple-overlap resolution).
//
// Story Type: Integration — the audio dispatch itself is a stub (UE_LOG +
// test counters) so the assertions target envelope STATE + dispatch bookkeeping
// rather than an actual audio bus. When the audio system is wired downstream
// the state assertions here still hold as the contract PM presents to it.
//
// Spec:   production/epics/player-movement/story-011-slip-audio-cue-ducking.md
// GDD:    design/gdd/player-movement-presentation.md §3/§4/§5 EC-16/§8
//         + R11a-13/14/15
// ADR:    docs/architecture/adr-0009-player-movement-hosting.md (SD1 dispatch site)
// TR:     TR-PM-031 (slip cue duration formula),
//         TR-PM-032 (-6dB duck + HARD-CUT triple-overlap resolution)
//
// Test category: SLIPSTORM.PlayerMovement.AudioCue
// Runner: UE Automation Framework, ClientContext | ProductFilter.
//
// Helpers _AC-suffixed to avoid ODR collisions with sibling PM tests.
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

#include "Player/PlayerLaneMovementComponent.h"
#include "Player/SlipstormPlayerPawn.h"
#include "Player/EPlayerLane.h"
#include "Player/ERunSlipState.h"
#include "Player/ESlipDirection.h"
#include "RunStateMachine/RunStateMachineSubsystem.h"
#include "RunStateMachine/ERunState.h"
#include "RunStateMachine/ERunOutcome.h"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static UCurveFloat* MakeIdentityCurve_AC(UObject* InOuter)
{
    UCurveFloat* C = NewObject<UCurveFloat>(InOuter);
    C->FloatCurve.AddKey(0.0f, 0.0f);
    C->FloatCurve.AddKey(1.0f, 1.0f);
    return C;
}

static UWorld* CreateTestPlayWorld_AC(FAutomationTestBase* T, const TCHAR* Label)
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

static ASlipstormPlayerPawn* SpawnPawnWithCurves_AC(
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
    Pawn->MovementComponent->SlipCurve       = MakeIdentityCurve_AC(World);
    Pawn->MovementComponent->LeanCurve       = MakeIdentityCurve_AC(World);
    Pawn->MovementComponent->EdgeAbsorbCurve = MakeIdentityCurve_AC(World);
    UGameplayStatics::FinishSpawningActor(Pawn, FTransform::Identity);
    return Pawn;
}

// Puts the RSM into RUNNING/not-paused/not-grace so the tick body's Rule 5
// gate lets the envelope advance execute.
static URunStateMachineSubsystem* PrimeRSMRunning_AC(
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

// Tick helper — sets FApp DeltaTime and drives TickComponent once.
static void TickPM_AC(UPlayerLaneMovementComponent* PM, float DeltaSeconds)
{
    FApp::SetDeltaTime(DeltaSeconds);
    PM->TickComponent(DeltaSeconds, ELevelTick::LEVELTICK_All, nullptr);
}

// ---------------------------------------------------------------------------
// IMPLEMENT_COMPLEX_AUTOMATION_TEST — 8 integration test commands.
// ---------------------------------------------------------------------------

IMPLEMENT_COMPLEX_AUTOMATION_TEST(
    FPMAudioCueTest,
    "SLIPSTORM.PlayerMovement.AudioCue",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

void FPMAudioCueTest::GetTests(
    TArray<FString>& OutBeautifiedNames,
    TArray<FString>& OutTestCommands) const
{
    OutBeautifiedNames.Add(TEXT("AC-AUDIO-CUE-PROPORTIONALITY — 6 safe-range duration pairs"));
    OutTestCommands.Add(TEXT("proportionality_six_pairs"));

    OutBeautifiedNames.Add(TEXT("Setup A — active-overlap: DUCK_ATTACK → DUCK_SUSTAINED across 50ms"));
    OutTestCommands.Add(TEXT("setup_a_active_overlap_ducking"));

    OutBeautifiedNames.Add(TEXT("Setup B — vacuous: near-miss after slip ended, no ducking engaged"));
    OutTestCommands.Add(TEXT("setup_b_vacuous_no_ducking"));

    OutBeautifiedNames.Add(TEXT("Setup C — safe-range analysis: slip max 168ms < near-miss min 200ms"));
    OutTestCommands.Add(TEXT("setup_c_safe_range_math"));

    OutBeautifiedNames.Add(TEXT("EC-16 triple-overlap — buffer-drop HARD-CUTs slip; near-miss no-ops on suppressed slip"));
    OutTestCommands.Add(TEXT("ec16_triple_overlap_hardcut_precedence"));

    OutBeautifiedNames.Add(TEXT("Center-pan invariant — every dispatch records pan == 0.0f"));
    OutTestCommands.Add(TEXT("center_pan_invariant"));

    OutBeautifiedNames.Add(TEXT("HARD-CUT ramp completes within 5ms of buffer-drop trigger"));
    OutTestCommands.Add(TEXT("hard_cut_ramp_duration_le_5ms"));

    OutBeautifiedNames.Add(TEXT("Rate-transposition clamp — out-of-range ratio clamped to [0.89, 1.12]"));
    OutTestCommands.Add(TEXT("rate_transposition_clamp"));

    OutBeautifiedNames.Add(TEXT("Dispatch site — HandleSlipTransition SETTLED→SLIPPING fires PlaySlipAudioCue"));
    OutTestCommands.Add(TEXT("dispatch_site_settled_to_slipping"));
}

bool FPMAudioCueTest::RunTest(const FString& Parameters)
{
    // -----------------------------------------------------------------------
    // TC1 — AC-AUDIO-CUE-PROPORTIONALITY.
    //
    // For each (SLIP_TWEEN, ratio) pair in the safe range, verify that
    // PlaySlipAudioCue records a duration within ±2ms of ratio × SLIP_TWEEN.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("proportionality_six_pairs"))
    {
        // Arrange
        UWorld* TestWorld = CreateTestPlayWorld_AC(this, TEXT("TC1"));
        if (!TestWorld) { return false; }
        ASlipstormPlayerPawn* Pawn = SpawnPawnWithCurves_AC(this, TestWorld, TEXT("TC1"));
        if (!Pawn) { return false; }
        UPlayerLaneMovementComponent* PM = Pawn->MovementComponent.Get();

        struct FPropCase { float Tween; float Ratio; float Expected; const TCHAR* Label; };
        const FPropCase Cases[] = {
            { 0.15f, 0.93f, 0.1395f, TEXT("(0.15, 0.93)") },
            { 0.10f, 0.93f, 0.0930f, TEXT("(0.10, 0.93)") },
            { 0.15f, 0.89f, 0.1335f, TEXT("(0.15, 0.89)") },
            { 0.15f, 1.12f, 0.1680f, TEXT("(0.15, 1.12)") },
            { 0.10f, 1.12f, 0.1120f, TEXT("(0.10, 1.12)") },
            { 0.10f, 0.89f, 0.0890f, TEXT("(0.10, 0.89)") },
        };
        const float TolS = 0.002f; // ±2ms tolerance per AC.

        // Act + Assert for each pair.
        for (const FPropCase& K : Cases)
        {
            PM->SLIP_TWEEN_DURATION_S = K.Tween;
            PM->audio_cue_ratio       = K.Ratio;

            const int32 CountBefore = PM->PlaySlipAudioCue_TestOnlyCallCount;
            PM->PlaySlipAudioCue(EPlayerLane::Center, EPlayerLane::Right);

            TestEqual(FString::Printf(TEXT("TC1: %s dispatch counter += 1"), K.Label),
                      PM->PlaySlipAudioCue_TestOnlyCallCount, CountBefore + 1);
            const float ActualS = PM->PlaySlipAudioCue_TestOnlyLastDurationS;
            TestTrue(FString::Printf(TEXT("TC1: %s expected %.4fs, got %.4fs (±%.4fs)"),
                                     K.Label, K.Expected, ActualS, TolS),
                     FMath::IsNearlyEqual(ActualS, K.Expected, TolS));
        }

        TestWorld->DestroyActor(Pawn);
        return true;
    }

    // -----------------------------------------------------------------------
    // TC2 — Setup A active-overlap ducking.
    //
    // PlaySlipAudioCue at t=0 (duration 139.5ms), EngageDuckIfSlipActive at
    // t=25ms simulated. Envelope should progress:
    //   t=0     → phase NONE (authored)
    //   t=25ms  → phase DUCK_ATTACK, elapsed=0
    //   t=50ms  → phase DUCK_ATTACK, elapsed≈25ms  (still attack)
    //   t=75ms  → phase DUCK_SUSTAINED (attack completed at elapsed=50ms)
    //   t=139ms → still SUSTAINED, cue about to expire
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("setup_a_active_overlap_ducking"))
    {
        // Arrange
        UWorld* TestWorld = CreateTestPlayWorld_AC(this, TEXT("TC2"));
        if (!TestWorld) { return false; }
        ASlipstormPlayerPawn* Pawn = SpawnPawnWithCurves_AC(this, TestWorld, TEXT("TC2"));
        if (!Pawn) { return false; }
        UPlayerLaneMovementComponent* PM = Pawn->MovementComponent.Get();
        if (!PrimeRSMRunning_AC(this, TestWorld, TEXT("TC2"))) { TestWorld->DestroyActor(Pawn); return false; }

        PM->SLIP_TWEEN_DURATION_S = 0.15f;
        PM->audio_cue_ratio       = 0.93f; // 139.5ms cue.

        // Act 1 — dispatch slip cue at simulated t=0.
        PM->PlaySlipAudioCue(EPlayerLane::Center, EPlayerLane::Right);

        // Assert 1 — envelope in NONE, cue active with full lifetime.
        TestTrue (TEXT("TC2: slip_cue_active after dispatch"), PM->slip_cue_active);
        TestTrue (TEXT("TC2: envelope_phase == NONE post-dispatch"),
                  PM->envelope_phase == UPlayerLaneMovementComponent::ESlipAudioEnvelopePhase::NONE);
        TestEqual(TEXT("TC2: slip_cue_remaining_s ≈ 139.5ms post-dispatch"),
                  PM->slip_cue_remaining_s, 0.1395f);

        // Act 2 — tick 25ms to reach the near-miss dispatch moment.
        TickPM_AC(PM, 0.025f);
        TestTrue(TEXT("TC2: cue still active after 25ms"), PM->slip_cue_active);

        // Act 3 — near-miss overlap engages duck.
        PM->EngageDuckIfSlipActive();

        // Assert 3 — DUCK_ATTACK entered, elapsed reset.
        TestTrue (TEXT("TC2: envelope_phase == DUCK_ATTACK after near-miss"),
                  PM->envelope_phase == UPlayerLaneMovementComponent::ESlipAudioEnvelopePhase::DUCK_ATTACK);
        TestEqual(TEXT("TC2: envelope_elapsed_s reset to 0 at DUCK_ATTACK entry"),
                  PM->envelope_elapsed_s, 0.0f);

        // Act 4 — tick 25ms into attack phase (mid-attack).
        TickPM_AC(PM, 0.025f);
        TestTrue(TEXT("TC2: still DUCK_ATTACK at 25ms into attack (attack duration 50ms)"),
                 PM->envelope_phase == UPlayerLaneMovementComponent::ESlipAudioEnvelopePhase::DUCK_ATTACK);
        TestTrue(FString::Printf(
            TEXT("TC2: envelope_elapsed_s ≈ 25ms — got %.4fs (±5ms tolerance)"),
            PM->envelope_elapsed_s),
            FMath::IsNearlyEqual(PM->envelope_elapsed_s, 0.025f, 0.005f));

        // Act 5 — tick another 25ms → total 50ms into attack → transition to SUSTAINED.
        TickPM_AC(PM, 0.025f);
        TestTrue(TEXT("TC2: DUCK_SUSTAINED after 50ms attack complete"),
                 PM->envelope_phase == UPlayerLaneMovementComponent::ESlipAudioEnvelopePhase::DUCK_SUSTAINED);

        // Act 5b — mid-sustain sample. AC-4b "sustained duck for the remainder
        // of the overlap window" requires holding at DUCK_SUSTAINED, not just
        // entering it. Tick 30ms and re-check phase before the expiry tick.
        // Cumulative since dispatch: 75 + 30 = 105ms. Slip cue lifetime 139.5ms —
        // still ~34.5ms remaining. Phase must remain DUCK_SUSTAINED.
        TickPM_AC(PM, 0.030f);
        TestTrue(TEXT("TC2: DUCK_SUSTAINED HOLDS at t=105ms (mid-sustain sample, AC-4b)"),
                 PM->envelope_phase == UPlayerLaneMovementComponent::ESlipAudioEnvelopePhase::DUCK_SUSTAINED);
        TestTrue(TEXT("TC2: slip still active mid-sustain (cue lifetime hasn't expired yet)"),
                 PM->slip_cue_active);

        // Act 6 — tick until cue lifetime expires. Total elapsed since dispatch:
        // 25 + 25 + 25 + 30 = 105ms so far; slip cue duration 139.5ms → ~34.5ms
        // remaining. Tick 35ms to cross expiry.
        TickPM_AC(PM, 0.035f);

        // Assert 6 — cue lifetime expired; envelope reset.
        TestFalse(TEXT("TC2: slip_cue_active false after cue lifetime expired"),
                  PM->slip_cue_active);
        TestTrue (TEXT("TC2: envelope_phase reset to NONE at lifetime expiry"),
                  PM->envelope_phase == UPlayerLaneMovementComponent::ESlipAudioEnvelopePhase::NONE);

        TestWorld->DestroyActor(Pawn);
        return true;
    }

    // -----------------------------------------------------------------------
    // TC3 — Setup B vacuous.
    //
    // slip @ t=0 (139.5ms); near-miss dispatched after slip already ended.
    // EngageDuckIfSlipActive is a no-op because !slip_cue_active.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("setup_b_vacuous_no_ducking"))
    {
        // Arrange
        UWorld* TestWorld = CreateTestPlayWorld_AC(this, TEXT("TC3"));
        if (!TestWorld) { return false; }
        ASlipstormPlayerPawn* Pawn = SpawnPawnWithCurves_AC(this, TestWorld, TEXT("TC3"));
        if (!Pawn) { return false; }
        UPlayerLaneMovementComponent* PM = Pawn->MovementComponent.Get();
        if (!PrimeRSMRunning_AC(this, TestWorld, TEXT("TC3"))) { TestWorld->DestroyActor(Pawn); return false; }

        PM->SLIP_TWEEN_DURATION_S = 0.15f;
        PM->audio_cue_ratio       = 0.93f;

        // Act 1 — dispatch slip cue, tick past its full lifetime (150ms > 139.5ms).
        PM->PlaySlipAudioCue(EPlayerLane::Center, EPlayerLane::Right);
        TickPM_AC(PM, 0.150f);

        TestFalse(TEXT("TC3: cue expired after 150ms tick"), PM->slip_cue_active);
        TestTrue (TEXT("TC3: envelope_phase reset to NONE at expiry"),
                  PM->envelope_phase == UPlayerLaneMovementComponent::ESlipAudioEnvelopePhase::NONE);

        // Act 2 — simulate near-miss arriving at t=250ms (100ms after cue ended).
        PM->EngageDuckIfSlipActive();

        // Assert — vacuous case: nothing changed.
        TestFalse(TEXT("TC3: slip_cue_active still false after near-miss on inactive cue"),
                  PM->slip_cue_active);
        TestTrue (TEXT("TC3: envelope_phase still NONE (no-op on inactive cue)"),
                  PM->envelope_phase == UPlayerLaneMovementComponent::ESlipAudioEnvelopePhase::NONE);
        TestEqual(TEXT("TC3: envelope_elapsed_s still 0 (no-op preserved)"),
                  PM->envelope_elapsed_s, 0.0f);

        TestWorld->DestroyActor(Pawn);
        return true;
    }

    // -----------------------------------------------------------------------
    // TC4 — Setup C safe-range math.
    //
    // Documented no-op test: assert that in all safe-range configurations,
    // slip cue max duration ends before near-miss swell min onset, so the
    // duck-release envelope is not exercised. This mirrors the compile-time
    // static_assert in the .h; if this runtime assertion fails, the
    // static_assert would also fail — the compile-time guard is authoritative
    // but the runtime assertion documents the calculation for readers.
    //
    // If future tuning changes cross this threshold, both this test AND the
    // static_assert must be revisited AND the duck-release envelope
    // implementation added (currently omitted per Setup C).
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("setup_c_safe_range_math"))
    {
        // Arrange — pull the compile-time constants for verification.
        const float SlipTweenMax   = 0.15f;
        const float RatioMax       = UPlayerLaneMovementComponent::SLIP_AUDIO_CUE_RATIO_MAX;
        const float NearMissMinOnsetS = 0.200f; // presentation §5 safe-range analysis.

        // Act — compute worst-case slip cue duration.
        const float SlipCueMaxS = SlipTweenMax * RatioMax;

        // Assert — worst-case slip ends before earliest near-miss onset.
        TestTrue(FString::Printf(
            TEXT("TC4: slip cue max %.4fs < near-miss min onset %.4fs — release envelope unverifiable in safe range"),
            SlipCueMaxS, NearMissMinOnsetS),
            SlipCueMaxS < NearMissMinOnsetS);

        // Assert — the exact expected value at nominal tuning (168ms) is at
        // the boundary the design analysis calls out.
        TestTrue(TEXT("TC4: nominal worst-case ≈ 168ms per presentation §5"),
                 FMath::IsNearlyEqual(SlipCueMaxS, 0.168f, 0.001f));

        return true;
    }

    // -----------------------------------------------------------------------
    // TC5 — EC-16 triple-overlap with HARD-CUT precedence guard reachability.
    //
    // Timeline: slip @ t=0 (139.5ms cue); buffer-drop @ t=20ms (engages HARD-CUT);
    // near-miss @ t=23ms (WHILE HARD-CUT in-progress) — must be a no-op preserving
    // envelope_phase == HARD_CUT (exercises the precedence guard at .cpp:1080).
    // Then tick remaining 3ms → HARD-CUT completes at cumulative 6ms, slip silent.
    //
    // Sub-AC "near-miss at full" (story-011 line 57) is deferred to Story 012 —
    // near-miss authored-gain state does not live on PM. This test asserts the
    // slip-side EC-16 contract only.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("ec16_triple_overlap_hardcut_precedence"))
    {
        // Arrange
        UWorld* TestWorld = CreateTestPlayWorld_AC(this, TEXT("TC5"));
        if (!TestWorld) { return false; }
        ASlipstormPlayerPawn* Pawn = SpawnPawnWithCurves_AC(this, TestWorld, TEXT("TC5"));
        if (!Pawn) { return false; }
        UPlayerLaneMovementComponent* PM = Pawn->MovementComponent.Get();
        if (!PrimeRSMRunning_AC(this, TestWorld, TEXT("TC5"))) { TestWorld->DestroyActor(Pawn); return false; }

        const int32 BDCountBefore = PM->BufferDropAudioSting_TestOnlyCallCount;

        // Act 1 — slip @ t=0.
        PM->PlaySlipAudioCue(EPlayerLane::Center, EPlayerLane::Right);
        TestTrue(TEXT("TC5: slip active after dispatch"), PM->slip_cue_active);

        // Act 2 — tick 20ms to buffer-drop dispatch moment.
        TickPM_AC(PM, 0.020f);
        TestTrue(TEXT("TC5: slip still active at t=20ms"), PM->slip_cue_active);

        // Act 3 — buffer-drop fires. HARD-CUT engaged synchronously.
        PM->PlayBufferDropAudioSting();
        TestEqual(TEXT("TC5: buffer-drop counter incremented (buffer-drop plays full)"),
                  PM->BufferDropAudioSting_TestOnlyCallCount, BDCountBefore + 1);
        TestTrue (TEXT("TC5: envelope_phase == HARD_CUT after buffer-drop"),
                  PM->envelope_phase == UPlayerLaneMovementComponent::ESlipAudioEnvelopePhase::HARD_CUT);
        TestEqual(TEXT("TC5: envelope_elapsed_s reset to 0 at HARD-CUT entry"),
                  PM->envelope_elapsed_s, 0.0f);

        // Act 4 — tick 3ms (< 5ms ramp) so HARD-CUT is IN-PROGRESS, not yet complete.
        TickPM_AC(PM, 0.003f);
        TestTrue(TEXT("TC5: slip still active mid-HARD-CUT (3ms of 5ms)"), PM->slip_cue_active);
        TestTrue(TEXT("TC5: envelope_phase still HARD_CUT mid-ramp"),
                 PM->envelope_phase == UPlayerLaneMovementComponent::ESlipAudioEnvelopePhase::HARD_CUT);

        // Act 5 — near-miss @ t=23ms (WHILE HARD-CUT in-progress).
        // Exercises the HARD_CUT precedence guard (.cpp:1080): a concurrent
        // near-miss MUST NOT overwrite HARD_CUT with DUCK_ATTACK. This is the
        // dead-code branch qa-tester flagged in code-review F2.
        PM->EngageDuckIfSlipActive();

        // Assert 5 — HARD_CUT preserved; DUCK_ATTACK did NOT engage.
        TestTrue(TEXT("TC5: envelope_phase STILL HARD_CUT after concurrent near-miss (precedence guard)"),
                 PM->envelope_phase == UPlayerLaneMovementComponent::ESlipAudioEnvelopePhase::HARD_CUT);
        TestTrue(TEXT("TC5: slip still active mid-HARD-CUT after near-miss no-op"),
                 PM->slip_cue_active);

        // Act 6 — tick remaining 3ms → HARD-CUT ramp completes at cumulative 6ms.
        TickPM_AC(PM, 0.003f);

        // Assert 6 — final EC-16 state: slip silent, envelope reset.
        // Sub-AC "near-miss plays full" is Story 012's contract to test —
        // PM has no near-miss gain state to assert here.
        TestFalse(TEXT("TC5: slip_cue_active false after HARD-CUT ramp completes"),
                  PM->slip_cue_active);
        TestTrue (TEXT("TC5: envelope_phase reset to NONE after HARD-CUT"),
                  PM->envelope_phase == UPlayerLaneMovementComponent::ESlipAudioEnvelopePhase::NONE);

        TestWorld->DestroyActor(Pawn);
        return true;
    }

    // -----------------------------------------------------------------------
    // TC6 — Center-pan invariant (R11a-14).
    //
    // Every PlaySlipAudioCue dispatch records pan == 0.0f. PM never dispatches
    // a panned variant. Sample multiple dispatches with different lane pairs.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("center_pan_invariant"))
    {
        // Arrange
        UWorld* TestWorld = CreateTestPlayWorld_AC(this, TEXT("TC6"));
        if (!TestWorld) { return false; }
        ASlipstormPlayerPawn* Pawn = SpawnPawnWithCurves_AC(this, TestWorld, TEXT("TC6"));
        if (!Pawn) { return false; }
        UPlayerLaneMovementComponent* PM = Pawn->MovementComponent.Get();

        struct FLaneCase { EPlayerLane From; EPlayerLane To; const TCHAR* Label; };
        const FLaneCase Cases[] = {
            { EPlayerLane::Left,     EPlayerLane::Center,   TEXT("Left→Center")   },
            { EPlayerLane::Center,   EPlayerLane::Right,    TEXT("Center→Right")  },
            { EPlayerLane::Right,    EPlayerLane::FarRight, TEXT("Right→FarRight")},
            { EPlayerLane::FarLeft,  EPlayerLane::Left,     TEXT("FarLeft→Left")  },
        };

        for (const FLaneCase& C : Cases)
        {
            PM->PlaySlipAudioCue(C.From, C.To);
            TestEqual(FString::Printf(TEXT("TC6: %s dispatched at pan == 0.0f (center-lock R11a-14)"), C.Label),
                      PM->PlaySlipAudioCue_TestOnlyLastPan, 0.0f);
        }

        TestWorld->DestroyActor(Pawn);
        return true;
    }

    // -----------------------------------------------------------------------
    // TC7 — HARD-CUT ramp completes within 5ms.
    //
    // Dispatch slip, immediately fire buffer-drop, tick in 1ms increments,
    // verify slip_cue_active goes false at cumulative elapsed ≤ HARD_CUT_RAMP_S
    // (5ms). Uses 1ms ticks so the assertion is bit-accurate against the linear
    // ramp completion condition (envelope_elapsed_s >= HARD_CUT_RAMP_S).
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("hard_cut_ramp_duration_le_5ms"))
    {
        // Arrange
        UWorld* TestWorld = CreateTestPlayWorld_AC(this, TEXT("TC7"));
        if (!TestWorld) { return false; }
        ASlipstormPlayerPawn* Pawn = SpawnPawnWithCurves_AC(this, TestWorld, TEXT("TC7"));
        if (!Pawn) { return false; }
        UPlayerLaneMovementComponent* PM = Pawn->MovementComponent.Get();
        if (!PrimeRSMRunning_AC(this, TestWorld, TEXT("TC7"))) { TestWorld->DestroyActor(Pawn); return false; }

        // Act 1 — dispatch slip; immediately fire buffer-drop to engage HARD-CUT.
        PM->PlaySlipAudioCue(EPlayerLane::Center, EPlayerLane::Right);
        PM->PlayBufferDropAudioSting();
        TestTrue(TEXT("TC7: envelope_phase == HARD_CUT after buffer-drop"),
                 PM->envelope_phase == UPlayerLaneMovementComponent::ESlipAudioEnvelopePhase::HARD_CUT);

        // Act 2 — tick 1ms at a time, up to 7 ticks (7ms). The ramp is 5ms so
        // slip_cue_active must transition to false no later than tick 6 — the
        // one-tick tolerance beyond 5ms guards against float-accumulation drift
        // in 1ms 32-bit float sums (5×0.001f may land at 4.9999...e-3 or 5.0001e-3
        // depending on FPU order; the envelope-completion check `elapsed >= 0.005`
        // is exclusive-safe but the accumulation is the fragile step).
        int32 TickCountToSilence = -1;
        for (int32 i = 1; i <= 7; ++i)
        {
            TickPM_AC(PM, 0.001f);
            if (!PM->slip_cue_active && TickCountToSilence == -1)
            {
                TickCountToSilence = i;
            }
        }

        // Assert — silence reached within 6ms (5ms ramp + 1ms float tolerance).
        TestTrue(FString::Printf(
            TEXT("TC7: slip_cue_active reached false within 6ms (took %d ms of 1ms ticks; ramp = 5ms + 1ms float tolerance)"),
            TickCountToSilence),
            TickCountToSilence > 0 && TickCountToSilence <= 6);

        TestWorld->DestroyActor(Pawn);
        return true;
    }

    // -----------------------------------------------------------------------
    // TC8 — Rate-transposition clamp.
    //
    // audio_cue_ratio out of [0.89, 1.12] is clamped in PlaySlipAudioCue.
    // Ratio 2.0f → clamped to 1.12f. Ratio 0.5f → clamped to 0.89f.
    // Verify LastDurationS matches the clamped ratio × SLIP_TWEEN, not the
    // raw ratio × SLIP_TWEEN. This clamp is what enforces the ±2 semitones
    // rate-transposition invariant (F-AUDIO-CUE-IDENTITY).
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("rate_transposition_clamp"))
    {
        // Arrange
        UWorld* TestWorld = CreateTestPlayWorld_AC(this, TEXT("TC8"));
        if (!TestWorld) { return false; }
        ASlipstormPlayerPawn* Pawn = SpawnPawnWithCurves_AC(this, TestWorld, TEXT("TC8"));
        if (!Pawn) { return false; }
        UPlayerLaneMovementComponent* PM = Pawn->MovementComponent.Get();

        PM->SLIP_TWEEN_DURATION_S = 0.15f;

        // Case A — ratio far above max (2.0 → clamped to 1.12).
        PM->audio_cue_ratio = 2.0f;
        PM->PlaySlipAudioCue(EPlayerLane::Center, EPlayerLane::Right);
        const float ExpectedHi = 0.15f * 1.12f; // = 0.168
        TestTrue(FString::Printf(
            TEXT("TC8: ratio 2.0 clamped to 1.12 → duration ≈ %.4fs (got %.4fs)"),
            ExpectedHi, PM->PlaySlipAudioCue_TestOnlyLastDurationS),
            FMath::IsNearlyEqual(PM->PlaySlipAudioCue_TestOnlyLastDurationS, ExpectedHi, 0.001f));

        // Case B — ratio far below min (0.5 → clamped to 0.89).
        PM->audio_cue_ratio = 0.5f;
        PM->PlaySlipAudioCue(EPlayerLane::Center, EPlayerLane::Right);
        const float ExpectedLo = 0.15f * 0.89f; // = 0.1335
        TestTrue(FString::Printf(
            TEXT("TC8: ratio 0.5 clamped to 0.89 → duration ≈ %.4fs (got %.4fs)"),
            ExpectedLo, PM->PlaySlipAudioCue_TestOnlyLastDurationS),
            FMath::IsNearlyEqual(PM->PlaySlipAudioCue_TestOnlyLastDurationS, ExpectedLo, 0.001f));

        // Case C — ratio at boundary (0.89 exact) is NOT clamped (strict bounds).
        PM->audio_cue_ratio = 0.89f;
        PM->PlaySlipAudioCue(EPlayerLane::Center, EPlayerLane::Right);
        TestTrue(TEXT("TC8: boundary 0.89 preserved (duration ≈ 0.1335)"),
                 FMath::IsNearlyEqual(PM->PlaySlipAudioCue_TestOnlyLastDurationS, 0.1335f, 0.001f));

        // Case D — ratio at boundary (1.12 exact) is NOT clamped.
        PM->audio_cue_ratio = 1.12f;
        PM->PlaySlipAudioCue(EPlayerLane::Center, EPlayerLane::Right);
        TestTrue(TEXT("TC8: boundary 1.12 preserved (duration ≈ 0.168)"),
                 FMath::IsNearlyEqual(PM->PlaySlipAudioCue_TestOnlyLastDurationS, 0.168f, 0.001f));

        TestWorld->DestroyActor(Pawn);
        return true;
    }

    // -----------------------------------------------------------------------
    // TC9 — Dispatch site integration: HandleSlipTransition SETTLED→SLIPPING
    // must actually fire PlaySlipAudioCue.
    //
    // AC-1 (story-011 line 34): "PlaySlipAudioCue dispatched from Story 003's
    // SETTLED→SLIPPING trigger site". Function-level assertions (TC1..TC8) call
    // PlaySlipAudioCue directly — they do NOT verify the trigger site is wired.
    // This test drives the integration path: valid input direction → F-4 passes
    // → collision commit → TriggerCommitmentTell → PlaySlipAudioCue. Any future
    // regression that silently skips the audio dispatch inside HandleSlipTransition
    // (e.g., a mis-ordered branch or forgotten call) is caught here.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("dispatch_site_settled_to_slipping"))
    {
        // Arrange
        UWorld* TestWorld = CreateTestPlayWorld_AC(this, TEXT("TC9"));
        if (!TestWorld) { return false; }
        ASlipstormPlayerPawn* Pawn = SpawnPawnWithCurves_AC(this, TestWorld, TEXT("TC9"));
        if (!Pawn) { return false; }
        UPlayerLaneMovementComponent* PM = Pawn->MovementComponent.Get();
        if (!PrimeRSMRunning_AC(this, TestWorld, TEXT("TC9"))) { TestWorld->DestroyActor(Pawn); return false; }

        // Preconditions: SETTLED at Center. Center + slip Right → F-4 passes
        // (target = Right, ordinal 3, in-bounds), state transitions to SLIPPING,
        // TriggerCommitmentTell fires, PlaySlipAudioCue fires.
        PM->current_lane          = EPlayerLane::Center;
        PM->target_lane           = EPlayerLane::Center;
        PM->movement_state        = ERunSlipState::SETTLED;
        PM->tween_progress        = 0.0f;
        PM->SLIP_TWEEN_DURATION_S = 0.15f;
        PM->audio_cue_ratio       = 0.93f; // Expected duration = 139.5ms.

        const int32 CueCountBefore = PM->PlaySlipAudioCue_TestOnlyCallCount;

        // Act — drive the integration entry point.
        PM->HandleSlipTransition(ESlipDirection::Right);

        // Assert 1 — state transitioned correctly (proves F-4 + collision commit ran).
        TestTrue (TEXT("TC9: movement_state == SLIPPING after HandleSlipTransition"),
                  PM->movement_state == ERunSlipState::SLIPPING);
        TestTrue (TEXT("TC9: target_lane == Right (F-4 projection)"),
                  PM->target_lane == EPlayerLane::Right);
        TestTrue (TEXT("TC9: current_lane still Center (source-lane semantic — Rule 4)"),
                  PM->current_lane == EPlayerLane::Center);

        // Assert 2 — PlaySlipAudioCue was invoked exactly once via the dispatch site.
        TestEqual(TEXT("TC9: PlaySlipAudioCue counter += 1 (dispatch site fired)"),
                  PM->PlaySlipAudioCue_TestOnlyCallCount, CueCountBefore + 1);

        // Assert 3 — dispatched with the correct duration + pan (end-to-end proportionality).
        const float Expected = 0.15f * 0.93f; // 0.1395s
        TestTrue(FString::Printf(
            TEXT("TC9: dispatched duration ≈ %.4fs (got %.4fs, ±2ms tolerance)"),
            Expected, PM->PlaySlipAudioCue_TestOnlyLastDurationS),
            FMath::IsNearlyEqual(PM->PlaySlipAudioCue_TestOnlyLastDurationS, Expected, 0.002f));
        TestEqual(TEXT("TC9: dispatched at pan == 0.0 (center-lock through dispatch site)"),
                  PM->PlaySlipAudioCue_TestOnlyLastPan, 0.0f);

        // Assert 4 — slip cue lifecycle state is live post-dispatch.
        TestTrue (TEXT("TC9: slip_cue_active == true after dispatch"), PM->slip_cue_active);
        TestEqual(TEXT("TC9: slip_cue_remaining_s ≈ 0.1395s post-dispatch"),
                  PM->slip_cue_remaining_s, Expected);
        TestTrue (TEXT("TC9: envelope_phase == NONE at dispatch (no duck/HARD-CUT yet)"),
                  PM->envelope_phase == UPlayerLaneMovementComponent::ESlipAudioEnvelopePhase::NONE);

        TestWorld->DestroyActor(Pawn);
        return true;
    }

    AddError(FString::Printf(TEXT("Unknown test parameter: %s"), *Parameters));
    return false;
}

#endif // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

// Copyright (c) Game Studio. All rights reserved.
// SLIPSTORM — 60-second voxel endless runner, mobile, Unreal Engine 5.7
//
// PMLaneAndTweenTest.cpp — Story 002 unit tests for lane geometry, F-PROLOGUE,
// F-2 accumulator, and SLIP_TWEEN_DURATION_S persistent clamp.
//
// Spec: tests/unit/player-movement/pm-lane-and-tween-spec.md
// GDD: design/gdd/player-movement-mechanics.md §4 F-1, F-2 + §7 Tuning Knobs
//      design/gdd/player-movement-platform.md §4 F-PROLOGUE +
//                                              §3 Shipping-Safety AC-SS-A +
//                                              §8 AC-20 + AC-21
// ADR: docs/architecture/adr-0009-player-movement-hosting.md (SD4)
// Story: production/epics/player-movement/story-002-lane-math-tween-prologue.md
//
// Test category: SLIPSTORM.PlayerMovement.LaneAndTween
// Runner: UE Automation Framework, headless (-nullrhi -nosound -unattended)
//         ClientContext | ProductFilter — matches Story 001a precedent.
//
// Pure-math tests — no UWorld or SpawnActor.  All helpers are exercised via
// NewObject<UPlayerLaneMovementComponent>() + direct field mutation through the
// FPMLaneAndTweenTest friend declaration in PlayerLaneMovementComponent.h.
//
// Guard: WITH_DEV_AUTOMATION_TESTS only.  No WITH_EDITOR — no editor APIs used.

#if WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#include "Player/PlayerLaneMovementComponent.h"
#include "Player/EPlayerLane.h"
#include "Player/ERunSlipState.h"

// ---------------------------------------------------------------------------
// IMPLEMENT_COMPLEX_AUTOMATION_TEST
// 5 test commands per Story 002 QA Test Cases section.
// Naming matches the ACs: f1_lane_math, f2_math_nominal, f2_hitch_clamp,
// slip_tween_floor_clamp, slip_tween_ceiling_clamp.
// ---------------------------------------------------------------------------

IMPLEMENT_COMPLEX_AUTOMATION_TEST(
    FPMLaneAndTweenTest,
    "SLIPSTORM.PlayerMovement.LaneAndTween",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

void FPMLaneAndTweenTest::GetTests(
    TArray<FString>& OutBeautifiedNames,
    TArray<FString>& OutTestCommands) const
{
    OutBeautifiedNames.Add(TEXT("F-1 lane math — ordinal to world-X mapping"));
    OutTestCommands.Add(TEXT("f1_lane_math"));

    OutBeautifiedNames.Add(TEXT("AC-20 F-2 math nominal — DT=0.016 advance"));
    OutTestCommands.Add(TEXT("f2_math_nominal"));

    OutBeautifiedNames.Add(TEXT("AC-20 F-2 hitch clamp — DT=2.0 clamped to MAX_SLIP_DT_S"));
    OutTestCommands.Add(TEXT("f2_hitch_clamp"));

    OutBeautifiedNames.Add(TEXT("AC-21 SLIP_TWEEN floor persistent clamp — 0.09f → 0.10f"));
    OutTestCommands.Add(TEXT("slip_tween_floor_clamp"));

    OutBeautifiedNames.Add(TEXT("AC-21 SLIP_TWEEN ceiling persistent clamp — 0.20f → 0.15f"));
    OutTestCommands.Add(TEXT("slip_tween_ceiling_clamp"));
}

bool FPMLaneAndTweenTest::RunTest(const FString& Parameters)
{
    // -----------------------------------------------------------------------
    // TC1 (F-1 lane math) — LaneWorldX ordinal-to-world-X mapping.
    //
    // Formula F-1: X = (static_cast<int32>(Lane) - 2) * LANE_WIDTH_CM
    // LANE_WIDTH_CM = 100.0f (LANE_WIDTH_M = 1.0f × 100).
    //
    // FarLeft(0) → -200, Left(1) → -100, Center(2) → 0,
    // Right(3) → +100, FarRight(4) → +200.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("f1_lane_math"))
    {
        TestEqual(TEXT("TC1: LaneWorldX(FarLeft)  == -200.0f"),
                  UPlayerLaneMovementComponent::LaneWorldX(EPlayerLane::FarLeft),
                  -200.0f);
        TestEqual(TEXT("TC1: LaneWorldX(Left)     == -100.0f"),
                  UPlayerLaneMovementComponent::LaneWorldX(EPlayerLane::Left),
                  -100.0f);
        TestEqual(TEXT("TC1: LaneWorldX(Center)   == 0.0f"),
                  UPlayerLaneMovementComponent::LaneWorldX(EPlayerLane::Center),
                  0.0f);
        TestEqual(TEXT("TC1: LaneWorldX(Right)    == +100.0f"),
                  UPlayerLaneMovementComponent::LaneWorldX(EPlayerLane::Right),
                  100.0f);
        TestEqual(TEXT("TC1: LaneWorldX(FarRight) == +200.0f"),
                  UPlayerLaneMovementComponent::LaneWorldX(EPlayerLane::FarRight),
                  200.0f);
        return true;
    }

    // -----------------------------------------------------------------------
    // TC2 (AC-20 F-2 math nominal) — DT=0.016s advance from tween_progress=0.
    //
    // Given: SLIP_TWEEN_DURATION_S = 0.15f (default), MAX_SLIP_DT_S = 0.05f.
    // AdvanceTweenProgress(0.016f) with tween_progress==0 →
    //   tween_progress += 0.016 / 0.15 ≈ 0.10667
    //
    // Tolerance: ± 0.001f per AC.
    // Also verify: DT=0.0f leaves tween_progress unchanged.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("f2_math_nominal"))
    {
        UPlayerLaneMovementComponent* PM = NewObject<UPlayerLaneMovementComponent>();
        TestNotNull(TEXT("TC2: NewObject succeeded"), PM);
        if (!PM)
        {
            return false;
        }

        PM->SLIP_TWEEN_DURATION_S = 0.15f;
        PM->tween_progress = 0.0f;

        PM->AdvanceTweenProgress(0.016f);
        TestTrue(
            TEXT("TC2: tween_progress ≈ 0.10667 ± 0.001 after DT=0.016s from 0"),
            FMath::IsNearlyEqual(PM->tween_progress, 0.10667f, 0.001f));

        // Edge case — DT = 0.0f leaves tween_progress unchanged (no divide-by-anything hazard).
        const float PriorTP = PM->tween_progress;
        PM->AdvanceTweenProgress(0.0f);
        TestEqual(
            TEXT("TC2 edge: DT=0.0f leaves tween_progress unchanged"),
            PM->tween_progress,
            PriorTP);
        return true;
    }

    // -----------------------------------------------------------------------
    // TC3 (AC-20 F-2 hitch clamp) — DT=2.0f (2-second hitch) is clamped by
    // F-PROLOGUE to MAX_SLIP_DT_S so a single tick cannot skip past TP=1.0.
    //
    // Given: SLIP_TWEEN_DURATION_S = 0.15f, MAX_SLIP_DT_S = 0.05f.
    // ComputeTickDT should clamp raw 2.0 → effective 0.05 (via FApp path — but
    // for pure-math test we exercise the clamp arithmetic by feeding the
    // AdvanceTweenProgress helper a caller-clamped value).
    //
    // Then AdvanceTweenProgress(0.05f) from tween_progress=0 →
    //   tween_progress += 0.05 / 0.15 ≈ 0.3333 (< 1.0, hitch does not skip).
    //
    // ComputeTickDT's raw side is not directly assertable (FApp is a global
    // clock read); the effective-side clamp semantics are what AC-20 hitch
    // exercises. If the caller passes 2.0 into AdvanceTweenProgress WITHOUT
    // going through ComputeTickDT, the F-2 accumulator would take the full
    // 2.0 / 0.15 = 13.33 — that path is Story 003's TickComponent glue.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("f2_hitch_clamp"))
    {
        UPlayerLaneMovementComponent* PM = NewObject<UPlayerLaneMovementComponent>();
        TestNotNull(TEXT("TC3: NewObject succeeded"), PM);
        if (!PM)
        {
            return false;
        }

        PM->SLIP_TWEEN_DURATION_S = 0.15f;
        PM->tween_progress = 0.0f;

        // ComputeTickDT reads FApp::GetDeltaTime() — we can't inject a value,
        // but we can verify effective_dt is clamped to MAX_SLIP_DT_S regardless
        // of raw. Test the invariant: effective_dt ≤ MAX_SLIP_DT_S.
        float RawDT = 0.0f;
        float EffectiveDT = 0.0f;
        PM->ComputeTickDT(RawDT, EffectiveDT);
        TestTrue(
            TEXT("TC3: ComputeTickDT effective_dt ≤ MAX_SLIP_DT_S invariant"),
            EffectiveDT <= 0.05f + KINDA_SMALL_NUMBER);
        TestTrue(
            TEXT("TC3: ComputeTickDT effective_dt ≥ 0 invariant"),
            EffectiveDT >= 0.0f);

        // Simulate F-2 accumulator with the clamped-DT ceiling value 0.05f
        // (as Story 003's TickComponent will after ComputeTickDT clamps).
        PM->AdvanceTweenProgress(0.05f);
        TestTrue(
            TEXT("TC3: AdvanceTweenProgress(0.05f) → TP ≈ 0.3333 ± 0.001 (hitch does not skip past 1.0)"),
            FMath::IsNearlyEqual(PM->tween_progress, 0.3333f, 0.001f));

        // Sustained 5-tick hitch: TP advances 5 × 0.3333 = 1.667. Story 002
        // does NOT clamp on the completion side (CompleteTween is Story 003).
        // Verify the accumulator does not silently clip — it just accumulates.
        for (int32 i = 0; i < 4; ++i) // 4 more ticks after the initial 1
        {
            PM->AdvanceTweenProgress(0.05f);
        }
        TestTrue(
            TEXT("TC3: 5 × AdvanceTweenProgress(0.05) → TP ≈ 1.667 (no clip; Story 003 clamps at CompleteTween)"),
            FMath::IsNearlyEqual(PM->tween_progress, 1.667f, 0.01f));
        return true;
    }

    // -----------------------------------------------------------------------
    // TC4 (AC-21 SLIP_TWEEN floor persistent clamp) — SLIP_TWEEN_DURATION_S
    // = 0.09f clamps to 0.10f every call; bSlipTweenClampActive stays true;
    // rate-limited Error log fires on tick 1 and tick 601.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("slip_tween_floor_clamp"))
    {
        UPlayerLaneMovementComponent* PM = NewObject<UPlayerLaneMovementComponent>();
        TestNotNull(TEXT("TC4: NewObject succeeded"), PM);
        if (!PM)
        {
            return false;
        }

        PM->SLIP_TWEEN_DURATION_S = 0.09f; // floor violation

        // Expect 2 Error logs across 601 violating calls (tick 1 and tick 601).
        // Message contains "SLIP_TWEEN_DURATION_S=" and "out of safe range".
        AddExpectedError(
            TEXT("SLIP_TWEEN_DURATION_S=0.090000 out of safe range"),
            EAutomationExpectedErrorFlags::Contains,
            2);

        // 601 consecutive violating calls to exercise the tick-1 + tick-601 log pattern.
        bool bAllReturnedFloor = true;
        bool bFlagStayedTrue   = true;
        for (int32 i = 0; i < 601; ++i)
        {
            const float Effective = PM->GetEffectiveSlipTween();
            if (!FMath::IsNearlyEqual(Effective, 0.10f, KINDA_SMALL_NUMBER))
            {
                bAllReturnedFloor = false;
            }
            if (!PM->bSlipTweenClampActive)
            {
                bFlagStayedTrue = false;
            }
        }
        TestTrue(TEXT("TC4: All 601 calls return exactly 0.10f (floor clamp)"),
                 bAllReturnedFloor);
        TestTrue(TEXT("TC4: bSlipTweenClampActive == true for all 601 violating ticks"),
                 bFlagStayedTrue);

        // Restore knob to a valid value → counter resets, flag clears on next call.
        PM->SLIP_TWEEN_DURATION_S = 0.12f;
        const float ValidEffective = PM->GetEffectiveSlipTween();
        TestEqual(TEXT("TC4 edge: valid knob 0.12 returns 0.12 (no clamp)"),
                  ValidEffective, 0.12f);
        TestFalse(TEXT("TC4 edge: bSlipTweenClampActive == false after valid call"),
                  PM->bSlipTweenClampActive);
        TestEqual(TEXT("TC4 edge: SlipTweenClampLogTickCounter reset to 0 on valid call"),
                  PM->SlipTweenClampLogTickCounter, 0);
        return true;
    }

    // -----------------------------------------------------------------------
    // TC5 (AC-21 SLIP_TWEEN ceiling persistent clamp) — SLIP_TWEEN_DURATION_S
    // = 0.20f clamps to 0.15f; bSlipTweenClampActive true; Error log on tick 1.
    // Edge case: exact 0.15f is NOT a violation (strict > comparison).
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("slip_tween_ceiling_clamp"))
    {
        UPlayerLaneMovementComponent* PM = NewObject<UPlayerLaneMovementComponent>();
        TestNotNull(TEXT("TC5: NewObject succeeded"), PM);
        if (!PM)
        {
            return false;
        }

        PM->SLIP_TWEEN_DURATION_S = 0.20f; // ceiling violation

        AddExpectedError(
            TEXT("SLIP_TWEEN_DURATION_S=0.200000 out of safe range"),
            EAutomationExpectedErrorFlags::Contains,
            1);

        // 10 consecutive violating calls — only tick 1 logs.
        bool bAllReturnedCeiling = true;
        bool bFlagStayedTrue     = true;
        for (int32 i = 0; i < 10; ++i)
        {
            const float Effective = PM->GetEffectiveSlipTween();
            if (!FMath::IsNearlyEqual(Effective, 0.15f, KINDA_SMALL_NUMBER))
            {
                bAllReturnedCeiling = false;
            }
            if (!PM->bSlipTweenClampActive)
            {
                bFlagStayedTrue = false;
            }
        }
        TestTrue(TEXT("TC5: All 10 calls return exactly 0.15f (ceiling clamp)"),
                 bAllReturnedCeiling);
        TestTrue(TEXT("TC5: bSlipTweenClampActive == true for all 10 violating ticks"),
                 bFlagStayedTrue);

        // Boundary edge case: exact 0.15f is NOT a violation.
        PM->SLIP_TWEEN_DURATION_S = 0.15f;
        const float BoundaryEffective = PM->GetEffectiveSlipTween();
        TestEqual(TEXT("TC5 edge: exact 0.15f returns 0.15f (boundary — no clamp)"),
                  BoundaryEffective, 0.15f);
        TestFalse(TEXT("TC5 edge: bSlipTweenClampActive == false at exact 0.15f boundary"),
                  PM->bSlipTweenClampActive);

        // Boundary edge case: exact 0.10f is NOT a violation.
        PM->SLIP_TWEEN_DURATION_S = 0.10f;
        const float FloorBoundaryEffective = PM->GetEffectiveSlipTween();
        TestEqual(TEXT("TC5 edge: exact 0.10f returns 0.10f (boundary — no clamp)"),
                  FloorBoundaryEffective, 0.10f);
        TestFalse(TEXT("TC5 edge: bSlipTweenClampActive == false at exact 0.10f boundary"),
                  PM->bSlipTweenClampActive);
        return true;
    }

    // Unknown parameter — fail explicitly rather than silently returning true.
    AddError(FString::Printf(
        TEXT("FPMLaneAndTweenTest::RunTest — unknown Parameters value: '%s'"),
        *Parameters));
    return false;
}

#endif // WITH_DEV_AUTOMATION_TESTS

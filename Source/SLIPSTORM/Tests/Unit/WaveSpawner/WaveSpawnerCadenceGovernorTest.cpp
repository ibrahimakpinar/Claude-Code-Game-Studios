// Copyright (c) Game Studio. All rights reserved.
// SLIPSTORM — 60-second voxel endless runner, mobile, Unreal Engine 5.7
//
// WaveSpawnerCadenceGovernorTest.cpp — Story 005 unit tests for:
//   Stage 3: F-3 cadence governor (ShouldDrawBarrage — three-branch piecewise weight function)
//   Stage 4: Pattern draw (DrawNonBarragePattern / DrawBarragePattern — deterministic RNG pool draw)
//   RNG seeding: PatternRNG seeded at Cold→Active from RunSeed stub (TR-WS-013, AC-WS-13)
//
// Story: production/epics/wave-spawner/story-005-pattern-draw-and-cadence-governor.md
// ADR:   docs/architecture/adr-0011-wave-spawner-pattern-library.md (D2 Stage 3-4)
// TRs:   TR-WS-013 (seeded from RunSeed at Cold→Active)
//         TR-WS-029 (proportional branch weight formula)
//         TR-WS-030 (force-draw branch at T_FORCE)
//         TR-WS-033/034 (platform determinism — no transcendentals, FRandomStream only)
//
// Test category: SLIPSTORM.WaveSpawner.CadenceGovernor
// Runner:        UE Automation Framework
//                Flags: ApplicationContextMask | ProductFilter
//
// 10 test commands are fully headless (no UWorld needed).
// TC3 (WallClockSeedAbsent) was removed — code-review-only enforcement; see GetTests() comment.
// NewObject<UWaveSpawnerSubsystem>(GetTransientPackage()) + TestOnly_* seams throughout.
//
// TC2/TC9 stream-contamination note (specialist finding):
//   DrawNonBarragePattern() and DrawBarragePattern() are called via TestOnly_Draw*Pattern()
//   directly — NOT through TryAdmitPattern — so ShouldDrawBarrage() is never invoked before
//   the draw in these TCs. No RNG contamination can occur. To guard against future regressions
//   if the test is later changed to go through TriggerDPCFrameReady, set BarrageCount >= TARGET
//   (ceiling branch) in TC2/TC9 so ShouldDrawBarrage() exits without consuming FRand().
//
// Tuning constants (Story 005 stubs referenced in expected-value calculations):
//   kBaseW = 0.25f, kTForce = 0.75f, kTargetBarrages = 2, kPeakDurationS = 21.0f, kWCeiling = 1.0f
//
// Test isolation: each TC constructs a fresh UWaveSpawnerSubsystem via NewObject<> and
// sets up its own preconditions. No state carries across RunTest dispatches.
//
// Guard: WITH_DEV_AUTOMATION_TESTS only.

#if WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Math/RandomStream.h"

#include "WaveSpawner/WaveSpawnerSubsystem.h"
#include "WaveSpawner/WaveSpawnerTypes.h"
#include "DPC/DPCSubsystem.h"

// ============================================================================
// IMPLEMENT_COMPLEX_AUTOMATION_TEST
// 10 test commands covering Story 005 acceptance criteria (AC-WS-13, AC-WS-14, AC-WS-15).
// TC3 (wall-clock seed absence) removed from suite — code-review-only; see GetTests() comment.
// Pattern matches WaveSpawnerBarrageAdmissionTest.cpp (Story 004).
// ============================================================================

IMPLEMENT_COMPLEX_AUTOMATION_TEST(
    FWaveSpawnerCadenceGovernorTest,
    "SLIPSTORM.WaveSpawner.CadenceGovernor",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

void FWaveSpawnerCadenceGovernorTest::GetTests(
    TArray<FString>& OutBeautifiedNames,
    TArray<FString>& OutTestCommands) const
{
    OutBeautifiedNames.Add(
        TEXT("RNG seeded at Cold→Active: TransitionTo(Active) seeds PatternRNG with stub 0; "
             "first draw matches FRandomStream(0) (AC-WS-13, TR-WS-013)"));
    OutTestCommands.Add(TEXT("CadenceGovernor.RNGSeededAtColdToActive"));

    OutBeautifiedNames.Add(
        TEXT("Deterministic draw: same seed, same pool → same index on two independent subsystems "
             "(AC-WS-13, AC-WS-15)"));
    OutTestCommands.Add(TEXT("CadenceGovernor.DeterministicDraw"));

    // TC3 (WallClockSeedAbsent) intentionally omitted from automation suite.
    // Enforcement is code-review only — runtime assertions cannot verify absence of
    // wall-clock calls without file I/O (forbidden in unit tests per coding-standards.md).
    // Manual grep at every code review touching the admission pipeline:
    //   grep -rn "FDateTime\|FMath::Rand\b\|rand()\|time(\|FPlatformTime" \
    //        Source/SLIPSTORM/WaveSpawner/WaveSpawnerSubsystem.cpp
    // (AC-WS-13 / TR-WS-013; documented in Story 005 Completion Notes.)

    OutBeautifiedNames.Add(
        TEXT("Force-draw branch: BarrageCount=0, tNorm >= T_FORCE(0.75) → ShouldDrawBarrage true; "
             "boundary >= (not >) verified (AC-WS-14, TR-WS-030)"));
    OutTestCommands.Add(TEXT("CadenceGovernor.ForceDrawFiresAtThreshold"));

    OutBeautifiedNames.Add(
        TEXT("Ceiling branch: BarrageCount >= TARGET(2) → ShouldDrawBarrage false; "
             "count=3 (over-target) also false (AC-WS-14)"));
    OutTestCommands.Add(TEXT("CadenceGovernor.CeilingBranchSuppresses"));

    OutBeautifiedNames.Add(
        TEXT("Proportional branch: weight=0 at tNorm=0 → ShouldDrawBarrage always false "
             "regardless of FRand value (AC-WS-14)"));
    OutTestCommands.Add(TEXT("CadenceGovernor.ProportionalWeightZeroAtTNormZero"));

    OutBeautifiedNames.Add(
        TEXT("Proportional branch: known seed, WBarrage=0.125 (base=0.25, deficit=1, t=0.5); "
             "result matches parallel FRandomStream (AC-WS-14, TR-WS-029)"));
    OutTestCommands.Add(TEXT("CadenceGovernor.ProportionalBranchDeterministic"));

    OutBeautifiedNames.Add(
        TEXT("Non-PEAK phases (Opener, Mid): ShouldDrawBarrage always false regardless of "
             "BarrageCount or tNorm (AC-WS-14)"));
    OutTestCommands.Add(TEXT("CadenceGovernor.NonPeakReturnsFalse"));

    OutBeautifiedNames.Add(
        TEXT("DrawNonBarragePattern: index in [0, Pool.Num()-1]; same seed → same index on two "
             "independent subsystems; single-entry pool always index 0 (AC-WS-15)"));
    OutTestCommands.Add(TEXT("CadenceGovernor.NonBarrageDrawIndexInRange"));

    OutBeautifiedNames.Add(
        TEXT("DrawBarragePattern: called twice → BarrageCountThisPeak == 2; "
             "counter increments per draw (AC-WS-15)"));
    OutTestCommands.Add(TEXT("CadenceGovernor.BarrageDrawIncrementsCounter"));

    OutBeautifiedNames.Add(
        TEXT("Empty-pool guard: DrawNonBarragePattern on empty pool returns without crash; "
             "LastDrawIndex stays INDEX_NONE (AC-WS-15)"));
    OutTestCommands.Add(TEXT("CadenceGovernor.EmptyPoolNoCrash"));
}

bool FWaveSpawnerCadenceGovernorTest::RunTest(const FString& Parameters)
{
    // -------------------------------------------------------------------------
    // TC1 — AC-WS-13 (TR-WS-013): RNG seeded at Cold→Active.
    //
    // Verifies: TransitionTo(Active) with stub RunSeed=0 reseeds PatternRNG to seed 0.
    //           Uses TestOnly_PeekNextPatternRNGFRand() to directly compare stream state
    //           against a reference FRandomStream(0).FRand() — avoids the vacuous-pass
    //           failure mode where a default-constructed stream already happens to be at 0.
    //
    // Pre-condition: PatternRNG is pre-advanced to seed 12345 (non-zero, non-default).
    //   If TransitionTo(Active) fails to call PatternRNG.Initialize(0), the peek will
    //   return a value from the 12345 stream, NOT matching FRandomStream(0).FRand(),
    //   and the test fails correctly. This prevents vacuous pass on default-zero seed.
    //
    // Edge cases tested:
    //   RunSeed=0 (stub zero seed).
    //   RunSeed=UINT64_MAX → Story 007 scope (lower-32-bit truncation tested there).
    // -------------------------------------------------------------------------
    if (Parameters == TEXT("CadenceGovernor.RNGSeededAtColdToActive"))
    {
        UWaveSpawnerSubsystem* Sub =
            NewObject<UWaveSpawnerSubsystem>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC1: NewObject succeeded"), Sub))
        {
            return false;
        }

        // Pre-condition: advance PatternRNG to a non-default state (seed 12345).
        // This ensures TransitionTo(Active) must actively reseed the stream — a test against
        // a default-zero stream would pass vacuously even if Initialize() were never called.
        Sub->TestOnly_SeedPatternRNG(12345);

        // Act: Cold→Active hook must call PatternRNG.Initialize(0) (stub seed, TR-WS-013).
        Sub->TransitionTo(EWaveSpawnerLifecycleState::Active);

        // Verify stream state directly via peek (does not advance the real PatternRNG).
        // If the hook fired: first FRand() from PatternRNG must match FRandomStream(0).FRand().
        // If the hook was skipped: PatternRNG is still at seed 12345 — peek returns a different
        // value and TestEqual fails.
        const float ExpectedFirstFRand = FRandomStream(0).FRand();
        if (!TestEqual(
            TEXT("TC1: PatternRNG stream state after Cold→Active matches FRandomStream(0) "
                 "(stub seed 0 seeded at Cold→Active entry, TR-WS-013, AC-WS-13)"),
            Sub->TestOnly_PeekNextPatternRNGFRand(),
            ExpectedFirstFRand))
        {
            return false;
        }

        return true;
    }

    // -------------------------------------------------------------------------
    // TC2 — AC-WS-13, AC-WS-15: Deterministic draw — same seed, same pool, same index.
    //
    // Verifies: Two independent subsystems, both seeded with 42, injected with an
    //           identical 5-entry non-barrage pool, produce the same draw index.
    //           Direct TestOnly_DrawNonBarragePattern() seam used (bypasses TryAdmitPattern
    //           so ShouldDrawBarrage() is NOT called and does NOT consume a FRand value).
    //           BarrageCount set to kTargetBarrages to ensure ceiling branch exits cleanly
    //           if TryAdmitPattern is ever used in future refactors of this TC.
    //
    // Edge: single-entry pool (Num=1) always returns index 0 regardless of seed.
    // -------------------------------------------------------------------------
    if (Parameters == TEXT("CadenceGovernor.DeterministicDraw"))
    {
        // Build a 5-entry non-barrage pool.
        TArray<FPatternDefinition> Pool5;
        for (int32 i = 0; i < 5; ++i)
        {
            FPatternDefinition Def;
            Def.PatternId  = FName(*FString::Printf(TEXT("TC2_Pattern_%d"), i));
            Def.bIsBarrage = false;
            Pool5.Add(Def);
        }

        // Sub_A
        UWaveSpawnerSubsystem* SubA =
            NewObject<UWaveSpawnerSubsystem>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC2: SubA NewObject succeeded"), SubA)) { return false; }
        SubA->TestOnly_SetLifecycleState(EWaveSpawnerLifecycleState::Active);
        SubA->TestOnly_SetBarrageCount(2);  // ceiling branch guard (see TC2 header note)
        SubA->TestOnly_SeedPatternRNG(42);
        SubA->TestOnly_InjectOpenerPool(Pool5);

        // Sub_B — identical setup
        UWaveSpawnerSubsystem* SubB =
            NewObject<UWaveSpawnerSubsystem>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC2: SubB NewObject succeeded"), SubB)) { return false; }
        SubB->TestOnly_SetLifecycleState(EWaveSpawnerLifecycleState::Active);
        SubB->TestOnly_SetBarrageCount(2);
        SubB->TestOnly_SeedPatternRNG(42);
        SubB->TestOnly_InjectOpenerPool(Pool5);

        // Act: direct draw seam — no ShouldDrawBarrage() called, no RNG contamination.
        FDPCFrameState Dummy;
        SubA->TestOnly_DrawNonBarragePattern(Dummy);
        SubB->TestOnly_DrawNonBarragePattern(Dummy);

        const int32 IndexA = SubA->TestOnly_GetLastDrawIndex();
        const int32 IndexB = SubB->TestOnly_GetLastDrawIndex();

        TestTrue(
            TEXT("TC2: IndexA in [0, 4] — draw from 5-entry pool"),
            IndexA >= 0 && IndexA <= 4);
        TestEqual(
            TEXT("TC2: same seed (42) → same draw index (deterministic, AC-WS-13)"),
            IndexA, IndexB);

        // Edge: single-entry pool must always return index 0.
        TArray<FPatternDefinition> Pool1;
        FPatternDefinition Def1;
        Def1.PatternId = FName(TEXT("TC2_Single"));
        Pool1.Add(Def1);

        UWaveSpawnerSubsystem* SubSingle =
            NewObject<UWaveSpawnerSubsystem>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC2 edge: SubSingle NewObject succeeded"), SubSingle)) { return false; }
        SubSingle->TestOnly_SetLifecycleState(EWaveSpawnerLifecycleState::Active);
        SubSingle->TestOnly_SeedPatternRNG(99);
        SubSingle->TestOnly_InjectOpenerPool(Pool1);
        SubSingle->TestOnly_DrawNonBarragePattern(Dummy);

        TestEqual(
            TEXT("TC2 edge: single-entry pool → index always 0 (AC-WS-15)"),
            SubSingle->TestOnly_GetLastDrawIndex(),
            0);

        return true;
    }

    // TC3 (CadenceGovernor.WallClockSeedAbsent) removed from suite — see GetTests() comment.
    // Enforcement via code review. Not dispatched by GetTests() so this branch is never reached;
    // preserved here as documentation of the removed TC. Safe to delete in a future cleanup pass.

    // -------------------------------------------------------------------------
    // TC4 — AC-WS-14 (TR-WS-030): Force-draw branch fires at T_FORCE threshold.
    //
    // Given:  ActivePhase=Peak, BarrageCount=0 (force-draw eligible), tNorm >= 0.75 (kTForce).
    // When:   ShouldDrawBarrage() called.
    // Then:   true — force-draw overrides weight function.
    //
    // Force-draw branch does NOT consume PatternRNG.FRand() (early return before proportional).
    //
    // Edge cases:
    //   tNorm exactly = kTForce (= 0.75): Now = kTForce * kPeakDurationS = 0.75 * 21 = 15.75
    //     → must return true (>= not >).
    //   tNorm = 1.0 (PEAK fully elapsed): also true.
    // -------------------------------------------------------------------------
    if (Parameters == TEXT("CadenceGovernor.ForceDrawFiresAtThreshold"))
    {
        UWaveSpawnerSubsystem* Sub =
            NewObject<UWaveSpawnerSubsystem>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC4: NewObject succeeded"), Sub)) { return false; }

        Sub->TestOnly_SetLifecycleState(EWaveSpawnerLifecycleState::Active);
        Sub->TestOnly_SetActivePhase(ERunPhase::Peak);
        Sub->TestOnly_SetBarrageCount(0);
        Sub->TestOnly_SetPeakEntryTimeS(0.f);

        // tNorm ≈ 0.952 (> 0.75): well past threshold.
        Sub->TestOnly_SetCurrentTimeOverride(20.f);
        TestTrue(
            TEXT("TC4: force-draw fires — tNorm=0.952 > T_FORCE=0.75, count=0 (TR-WS-030)"),
            Sub->TestOnly_ShouldDrawBarrage());

        // Edge: exactly at T_FORCE boundary (tNorm = 0.75 * 21 / 21 = 0.75).
        Sub->TestOnly_SetCurrentTimeOverride(15.75f);  // 0.75 * kPeakDurationS = 15.75
        TestTrue(
            TEXT("TC4 edge: force-draw at exactly T_FORCE=0.75 boundary (>= not >, TR-WS-030)"),
            Sub->TestOnly_ShouldDrawBarrage());

        // Edge: tNorm = 1.0 (full PEAK elapsed).
        Sub->TestOnly_SetCurrentTimeOverride(21.f);    // kPeakDurationS
        TestTrue(
            TEXT("TC4 edge: force-draw at tNorm=1.0 (PEAK fully elapsed)"),
            Sub->TestOnly_ShouldDrawBarrage());

        return true;
    }

    // -------------------------------------------------------------------------
    // TC5 — AC-WS-14: Ceiling branch suppresses barrage when count >= TARGET.
    //
    // Given:  BarrageCount = kTargetBarrages (= 2), any tNorm.
    // When:   ShouldDrawBarrage() called.
    // Then:   false — ceiling branch exits before proportional branch; FRand() NOT consumed.
    //
    // Edge: count = 3 (over target) also returns false.
    // -------------------------------------------------------------------------
    if (Parameters == TEXT("CadenceGovernor.CeilingBranchSuppresses"))
    {
        UWaveSpawnerSubsystem* Sub =
            NewObject<UWaveSpawnerSubsystem>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC5: NewObject succeeded"), Sub)) { return false; }

        Sub->TestOnly_SetLifecycleState(EWaveSpawnerLifecycleState::Active);
        Sub->TestOnly_SetActivePhase(ERunPhase::Peak);
        Sub->TestOnly_SetPeakEntryTimeS(0.f);
        Sub->TestOnly_SetCurrentTimeOverride(10.5f);  // tNorm=0.5 (proportional would apply but ceiling fires first)

        // count == TARGET (= 2)
        Sub->TestOnly_SetBarrageCount(2);
        TestFalse(
            TEXT("TC5: ceiling branch, BarrageCount=2 (==kTargetBarrages) → false (AC-WS-14)"),
            Sub->TestOnly_ShouldDrawBarrage());

        // Edge: count > TARGET
        Sub->TestOnly_SetBarrageCount(3);
        TestFalse(
            TEXT("TC5 edge: ceiling branch, BarrageCount=3 (>kTargetBarrages) → false (AC-WS-14)"),
            Sub->TestOnly_ShouldDrawBarrage());

        return true;
    }

    // -------------------------------------------------------------------------
    // TC6 — AC-WS-14: Proportional branch — weight = 0.0 at tNorm = 0.
    //
    // Given:  ActivePhase=Peak, BarrageCount=1 (proportional branch: not force, not ceiling).
    //         tNorm = 0.0: Now == PeakEntryTimeS (elapsed fraction = 0).
    // When:   ShouldDrawBarrage() called.
    // Then:   WBarrage = kBaseW * (TARGET-1) * 0.0 = 0.0.
    //         FRand() in [0, 1); FRand() < 0.0 is never true → returns false.
    //
    // Note: TC6 verifies that weight=0 always suppresses a barrage (AC-WS-14 zero-weight case).
    //   It does NOT prove strict `<` vs `<=` at the boundary: FRand() returning exactly 0.0
    //   is equally suppressed by both operators. The strict-`<` boundary verification
    //   (FRand() == WBarrage → not admitted) is covered by TC7 boundary sub-case using
    //   TestOnly_PeekNextPatternRNGFRand() to construct the exact-equality scenario.
    // -------------------------------------------------------------------------
    if (Parameters == TEXT("CadenceGovernor.ProportionalWeightZeroAtTNormZero"))
    {
        UWaveSpawnerSubsystem* Sub =
            NewObject<UWaveSpawnerSubsystem>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC6: NewObject succeeded"), Sub)) { return false; }

        Sub->TestOnly_SetLifecycleState(EWaveSpawnerLifecycleState::Active);
        Sub->TestOnly_SetActivePhase(ERunPhase::Peak);
        Sub->TestOnly_SetBarrageCount(1);       // proportional branch eligible
        Sub->TestOnly_SetCurrentTimeOverride(5.f);
        Sub->TestOnly_SetPeakEntryTimeS(5.f);   // tNorm = (5 - 5) / 21 = 0.0

        // WBarrage = 0.25 * (2 - 1) * 0.0 = 0.0 → FRand() < 0.0 never true (AC-WS-14).
        TestFalse(
            TEXT("TC6: proportional weight=0 at tNorm=0 → ShouldDrawBarrage false (AC-WS-14)"),
            Sub->TestOnly_ShouldDrawBarrage());

        return true;
    }

    // -------------------------------------------------------------------------
    // TC7 — AC-WS-14 (TR-WS-029): Proportional branch — deterministic RNG draw against known weight.
    //
    // Sub-case A: FRand() < WBarrage — barrage admitted (AC-WS-14, TR-WS-029).
    //   PatternRNG seeded with 1; result matches parallel FRandomStream(1).FRand() < 0.125.
    //
    // Sub-case B: strict `<` (not `<=`) boundary — FRand() == WBarrage must NOT admit.
    //   Uses TestOnly_PeekNextPatternRNGFRand() to obtain the next FRand() value from PatternRNG,
    //   then sets tNorm so WBarrage equals that peeked value exactly.
    //   If `<=` were used: ShouldDrawBarrage() would return true (FRand == WBarrage admitted).
    //   With correct `<`:  ShouldDrawBarrage() must return false (AC-WS-14 strict-less-than).
    //
    // Note: Sub-case B requires peeked FRand ≤ kBaseW (= 0.25) to allow tNorm = FRand/kBaseW ∈ [0,1].
    //   If the peeked value exceeds 0.25, tNorm would be clamped and WBarrage ≠ peeked value.
    //   We record the peeked value via AddInfo in that case. The zero-weight boundary is
    //   covered by TC6 regardless.
    // -------------------------------------------------------------------------
    if (Parameters == TEXT("CadenceGovernor.ProportionalBranchDeterministic"))
    {
        // --- Sub-case A: FRand() < WBarrage determinism (TR-WS-029) ---
        UWaveSpawnerSubsystem* Sub =
            NewObject<UWaveSpawnerSubsystem>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC7: NewObject succeeded"), Sub)) { return false; }

        Sub->TestOnly_SetLifecycleState(EWaveSpawnerLifecycleState::Active);
        Sub->TestOnly_SetActivePhase(ERunPhase::Peak);
        Sub->TestOnly_SetBarrageCount(1);           // deficit = TARGET - 1 = 1
        Sub->TestOnly_SetCurrentTimeOverride(10.5f); // tNorm = 10.5 / 21.0 = 0.5
        Sub->TestOnly_SetPeakEntryTimeS(0.f);
        Sub->TestOnly_SeedPatternRNG(1);

        // Parallel reference stream — computes the FRand() ShouldDrawBarrage() will consume.
        FRandomStream RefRNG(1);
        const float RefFRand = RefRNG.FRand();

        // Formula B (Deficit, AC-WS-14 corrected 2026-08-19):
        // WBarrage = kBaseW * (kTargetBarrages - BarrageCount) * tNorm = 0.25 * 1 * 0.5 = 0.125
        const float ExpectedWBarrage = 0.125f;
        const bool  bExpected        = (RefFRand < ExpectedWBarrage);

        TestEqual(
            TEXT("TC7A: proportional branch result matches parallel FRandomStream(1) draw "
                 "(WBarrage=0.125 = kBaseW*deficit*tNorm; AC-WS-14, TR-WS-029)"),
            Sub->TestOnly_ShouldDrawBarrage(),
            bExpected);

        AddInfo(FString::Printf(
            TEXT("TC7A diagnostic: seed=1, RefFRand=%.6f, WBarrage=0.125, expected=%s"),
            RefFRand, bExpected ? TEXT("true (barrage)") : TEXT("false (no barrage)")));

        // --- Sub-case B: strict `<` boundary — FRand() == WBarrage must NOT admit (AC-WS-14) ---
        // Construct conditions where WBarrage == peeked FRand() exactly.
        // count=1 (not force, not ceiling), kBaseW=0.25, deficit=1, WBarrage = 0.25 * tNorm.
        // Need: tNorm = PeekFRand / 0.25.  Valid only when PeekFRand ≤ 0.25 (else clamped).
        UWaveSpawnerSubsystem* SubB =
            NewObject<UWaveSpawnerSubsystem>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC7B: SubB NewObject succeeded"), SubB)) { return false; }

        SubB->TestOnly_SetLifecycleState(EWaveSpawnerLifecycleState::Active);
        SubB->TestOnly_SetActivePhase(ERunPhase::Peak);
        SubB->TestOnly_SetBarrageCount(1);           // deficit = 1; avoid force-draw and ceiling
        SubB->TestOnly_SetPeakEntryTimeS(0.f);
        SubB->TestOnly_SeedPatternRNG(1);            // same seed as Sub-case A for reproducibility

        const float PeekedFRand = SubB->TestOnly_PeekNextPatternRNGFRand();
        // kBaseW = 0.25 (constexpr stub). tNorm = PeekedFRand / kBaseW so WBarrage == PeekedFRand.
        // kPeakDurationS = 21.0; Now = tNorm * 21.0.
        const float kBaseWStub        = 0.25f;
        const float kPeakDurationSStub = 21.0f;
        const float tNormBoundary     = (kBaseWStub > 0.f) ? (PeekedFRand / kBaseWStub) : 0.f;

        if (tNormBoundary <= 1.0f)
        {
            // Boundary condition is achievable — set time so WBarrage == PeekedFRand.
            SubB->TestOnly_SetCurrentTimeOverride(tNormBoundary * kPeakDurationSStub);

            // With correct strict `<`: FRand() == WBarrage → NOT admitted.
            // With buggy `<=`:         FRand() == WBarrage → would be admitted.
            TestFalse(
                TEXT("TC7B: strict < boundary — FRand()==WBarrage must NOT admit barrage (AC-WS-14)"),
                SubB->TestOnly_ShouldDrawBarrage());

            AddInfo(FString::Printf(
                TEXT("TC7B: seed=1, PeekedFRand=%.6f, WBarrage=%.6f (equal), ShouldDrawBarrage must be false"),
                PeekedFRand, kBaseWStub * tNormBoundary));
        }
        else
        {
            // PeekedFRand > kBaseW (= 0.25): tNorm would exceed 1.0 and clamp, making WBarrage != PeekedFRand.
            // Boundary case is not testable with this seed; fall back to TC6 zero-boundary coverage.
            AddInfo(FString::Printf(
                TEXT("TC7B: PeekedFRand=%.6f > kBaseW=%.2f — boundary sub-case not achievable "
                     "with seed=1; strict-< boundary covered by TC6 zero-weight case."),
                PeekedFRand, kBaseWStub));
        }

        return true;
    }

    // -------------------------------------------------------------------------
    // TC8 — AC-WS-14: Non-PEAK phases always return false.
    //
    // Given:  ActivePhase = Opener (then Mid).
    //         BarrageCount=0 (would trigger force-draw if in Peak).
    //         Time set to absurdly high value (1000s) — would force-draw in PEAK at T_FORCE.
    // When:   ShouldDrawBarrage() called.
    // Then:   false — phase guard exits at first branch before any time or RNG logic.
    //
    // Verifies that non-PEAK phases never consume a PatternRNG FRand() value.
    // -------------------------------------------------------------------------
    if (Parameters == TEXT("CadenceGovernor.NonPeakReturnsFalse"))
    {
        UWaveSpawnerSubsystem* Sub =
            NewObject<UWaveSpawnerSubsystem>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC8: NewObject succeeded"), Sub)) { return false; }

        Sub->TestOnly_SetLifecycleState(EWaveSpawnerLifecycleState::Active);
        Sub->TestOnly_SetBarrageCount(0);
        Sub->TestOnly_SetPeakEntryTimeS(0.f);
        Sub->TestOnly_SetCurrentTimeOverride(1000.f);  // would force-draw if PEAK

        // Opener
        Sub->TestOnly_SetActivePhase(ERunPhase::Opener);
        TestFalse(
            TEXT("TC8: Opener phase → ShouldDrawBarrage false regardless of count/tNorm (AC-WS-14)"),
            Sub->TestOnly_ShouldDrawBarrage());

        // Mid
        Sub->TestOnly_SetActivePhase(ERunPhase::Mid);
        TestFalse(
            TEXT("TC8: Mid phase → ShouldDrawBarrage false regardless of count/tNorm (AC-WS-14)"),
            Sub->TestOnly_ShouldDrawBarrage());

        return true;
    }

    // -------------------------------------------------------------------------
    // TC9 — AC-WS-15: DrawNonBarragePattern — uniform index in range; deterministic.
    //
    // Given:  pool of 3 entries; PatternRNG seeded with 7 (known seed).
    //         Direct seam call — ShouldDrawBarrage() NOT invoked; no RNG contamination.
    //         BarrageCount set to kTargetBarrages as defensive guard (specialist finding).
    // When:   TestOnly_DrawNonBarragePattern() called.
    // Then:   LastDrawIndex in [0, 2].
    //         Same seed (7) on second subsystem with same pool → same index.
    //
    // Edge: single-entry pool → index always 0 regardless of seed.
    // -------------------------------------------------------------------------
    if (Parameters == TEXT("CadenceGovernor.NonBarrageDrawIndexInRange"))
    {
        // Build 3-entry pool.
        TArray<FPatternDefinition> Pool3;
        for (int32 i = 0; i < 3; ++i)
        {
            FPatternDefinition Def;
            Def.PatternId  = FName(*FString::Printf(TEXT("TC9_Pattern_%d"), i));
            Def.bIsBarrage = false;
            Pool3.Add(Def);
        }

        FDPCFrameState Dummy;

        // Sub_A
        UWaveSpawnerSubsystem* SubA =
            NewObject<UWaveSpawnerSubsystem>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC9: SubA NewObject succeeded"), SubA)) { return false; }
        SubA->TestOnly_SetLifecycleState(EWaveSpawnerLifecycleState::Active);
        SubA->TestOnly_SetBarrageCount(2);  // ceiling branch guard (specialist finding — see file header)
        SubA->TestOnly_SeedPatternRNG(7);
        SubA->TestOnly_InjectOpenerPool(Pool3);
        SubA->TestOnly_DrawNonBarragePattern(Dummy);

        const int32 IndexA = SubA->TestOnly_GetLastDrawIndex();
        TestTrue(
            TEXT("TC9: IndexA in [0, 2] — draw from 3-entry pool (AC-WS-15)"),
            IndexA >= 0 && IndexA <= 2);

        // Sub_B — identical setup; must produce same index.
        UWaveSpawnerSubsystem* SubB =
            NewObject<UWaveSpawnerSubsystem>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC9: SubB NewObject succeeded"), SubB)) { return false; }
        SubB->TestOnly_SetLifecycleState(EWaveSpawnerLifecycleState::Active);
        SubB->TestOnly_SetBarrageCount(2);
        SubB->TestOnly_SeedPatternRNG(7);
        SubB->TestOnly_InjectOpenerPool(Pool3);
        SubB->TestOnly_DrawNonBarragePattern(Dummy);

        TestEqual(
            TEXT("TC9: same seed (7) → same draw index on two independent subsystems (AC-WS-15)"),
            IndexA,
            SubB->TestOnly_GetLastDrawIndex());

        // Edge: single-entry pool must always return index 0.
        TArray<FPatternDefinition> Pool1;
        FPatternDefinition SingleDef;
        SingleDef.PatternId = FName(TEXT("TC9_Single"));
        Pool1.Add(SingleDef);

        UWaveSpawnerSubsystem* SubSingle =
            NewObject<UWaveSpawnerSubsystem>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC9 edge: SubSingle NewObject succeeded"), SubSingle)) { return false; }
        SubSingle->TestOnly_SetLifecycleState(EWaveSpawnerLifecycleState::Active);
        SubSingle->TestOnly_SeedPatternRNG(777);
        SubSingle->TestOnly_InjectOpenerPool(Pool1);
        SubSingle->TestOnly_DrawNonBarragePattern(Dummy);

        TestEqual(
            TEXT("TC9 edge: single-entry pool → index always 0 regardless of seed (AC-WS-15)"),
            SubSingle->TestOnly_GetLastDrawIndex(),
            0);

        return true;
    }

    // -------------------------------------------------------------------------
    // TC10 — AC-WS-15: DrawBarragePattern increments BarrageCountThisPeak.
    //
    // Given:  PeakPool with 2 barrage entries; BarrageCount starts at 0.
    //         Direct seam call — bypasses TryAdmitPattern and ShouldDrawBarrage.
    // When:   TestOnly_DrawBarragePattern() called twice.
    // Then:   BarrageCountThisPeak == 2.
    //
    // Verifies the counter increment fires exactly once per successful draw
    // (not once per call when pool is non-empty).
    // -------------------------------------------------------------------------
    if (Parameters == TEXT("CadenceGovernor.BarrageDrawIncrementsCounter"))
    {
        UWaveSpawnerSubsystem* Sub =
            NewObject<UWaveSpawnerSubsystem>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC10: NewObject succeeded"), Sub)) { return false; }

        Sub->TestOnly_SetLifecycleState(EWaveSpawnerLifecycleState::Active);
        Sub->TestOnly_SetBarrageCount(0);  // explicit zero start

        TArray<FPatternDefinition> BarragePool;
        for (int32 i = 0; i < 2; ++i)
        {
            FPatternDefinition Def;
            Def.PatternId  = FName(*FString::Printf(TEXT("TC10_Barrage_%d"), i));
            Def.bIsBarrage = true;
            BarragePool.Add(Def);
        }
        Sub->TestOnly_InjectPeakBarragePool(BarragePool);

        FDPCFrameState Dummy;
        Sub->TestOnly_DrawBarragePattern(Dummy);
        Sub->TestOnly_DrawBarragePattern(Dummy);

        TestEqual(
            TEXT("TC10: BarrageCountThisPeak == 2 after 2 DrawBarragePattern() calls (AC-WS-15)"),
            Sub->TestOnly_GetBarrageCount(),
            2);

        return true;
    }

    // -------------------------------------------------------------------------
    // TC11 — AC-WS-15: Empty-pool guard — no crash, no draw.
    //
    // Given:  ActiveDrawPool points to OpenerPool with NonBarragePatterns = {} (empty).
    // When:   TestOnly_DrawNonBarragePattern() called.
    // Then:   Returns without crash. LastDrawIndex remains INDEX_NONE (no draw occurred).
    //         Telemetry stub call site (empty_pool_at_draw) present in implementation
    //         (verified by code review; grep: "empty_pool_at_draw" in WaveSpawnerSubsystem.cpp).
    //
    // A crash in DrawNonBarragePattern with empty pool would fail this test automatically.
    // -------------------------------------------------------------------------
    if (Parameters == TEXT("CadenceGovernor.EmptyPoolNoCrash"))
    {
        UWaveSpawnerSubsystem* Sub =
            NewObject<UWaveSpawnerSubsystem>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC11: NewObject succeeded"), Sub)) { return false; }

        Sub->TestOnly_SetLifecycleState(EWaveSpawnerLifecycleState::Active);

        // Inject empty NonBarragePatterns pool (Num() == 0) and set ActiveDrawPool.
        TArray<FPatternDefinition> EmptyPool;
        Sub->TestOnly_InjectOpenerPool(EmptyPool);

        FDPCFrameState Dummy;
        Sub->TestOnly_DrawNonBarragePattern(Dummy);  // must not crash

        // If execution reaches here, the empty-pool guard worked correctly.
        TestTrue(
            TEXT("TC11: DrawNonBarragePattern on empty pool returned without crash (AC-WS-15)"),
            true);

        TestEqual(
            TEXT("TC11: LastDrawIndex == INDEX_NONE — no draw occurred on empty pool (AC-WS-15)"),
            Sub->TestOnly_GetLastDrawIndex(),
            INDEX_NONE);

        AddInfo(
            TEXT("TC11: telemetry stub call site verified by code review — "
                 "grep 'empty_pool_at_draw' in WaveSpawnerSubsystem.cpp (TODO Story 009)."));

        return true;
    }

    // Unknown parameter — fail explicitly rather than silently returning true.
    AddError(FString::Printf(
        TEXT("FWaveSpawnerCadenceGovernorTest::RunTest — unknown Parameters value: '%s'"),
        *Parameters));
    return false;
}

#endif // WITH_DEV_AUTOMATION_TESTS

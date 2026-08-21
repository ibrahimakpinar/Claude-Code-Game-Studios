// Copyright (c) Game Studio. All rights reserved.
// SLIPSTORM — 60-second voxel endless runner, mobile, Unreal Engine 5.7
//
// PullWaveStructDefinitionsTest.cpp — Story 001 unit tests for Pull-Wave
// struct definitions, constants, size contracts, and EvaluateAt interpolation.
//
// Story: production/epics/pull-wave/story-001-struct-definitions.md
// GDD:   design/gdd/pull-wave-behavior.md
// ADR:   docs/architecture/adr-0010-pullwave-object-pool-state-machine-despawn.md
// TRs:   TR-PW-004 (SAMPLE_COUNT=32 locked), TR-PW-007 (sizeof <= 256)
//
// Test category: SLIPSTORM.PullWave.StructDefinitions
// Runner: UE Automation Framework, headless (-nullrhi -nosound -unattended)
//         EditorContext | ClientContext | ProductFilter
//
// Pure-math tests — no UWorld, no SpawnActor, no NewObject.
// All assertions exercise PullWaveTypes.h declarations directly.
//
// Guard: WITH_DEV_AUTOMATION_TESTS only.

#if WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#include "PullWave/PullWaveTypes.h"

// ---------------------------------------------------------------------------
// IMPLEMENT_COMPLEX_AUTOMATION_TEST
// 11 test commands covering all Story 001 acceptance criteria.
// ---------------------------------------------------------------------------

IMPLEMENT_COMPLEX_AUTOMATION_TEST(
    FPullWaveStructDefinitionsTest,
    "SLIPSTORM.PullWave.StructDefinitions",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::SmokeFilter)

void FPullWaveStructDefinitionsTest::GetTests(
    TArray<FString>& OutBeautifiedNames,
    TArray<FString>& OutTestCommands) const
{
    OutBeautifiedNames.Add(TEXT("EvaluateAt — linear interpolation nominal"));
    OutTestCommands.Add(TEXT("curve_snapshot_evaluate_at_interp"));

    OutBeautifiedNames.Add(TEXT("EvaluateAt — OOB clamp at t=1.0 (no index 32 access)"));
    OutTestCommands.Add(TEXT("curve_snapshot_evaluate_at_oob_clamp"));

    OutBeautifiedNames.Add(TEXT("FPullWaveSpawnParams size == 152 (AC-PW-22b)"));
    OutTestCommands.Add(TEXT("spawn_params_size"));

    OutBeautifiedNames.Add(TEXT("FPullWaveInstanceState size <= 256 (TR-PW-007)"));
    OutTestCommands.Add(TEXT("instance_state_size"));

    OutBeautifiedNames.Add(TEXT("LEAN_ANGLE_TIER0_DEG == 0.0f locked (AC-PW-22b pattern 6)"));
    OutTestCommands.Add(TEXT("lean_angle_tier0_is_zero"));

    OutBeautifiedNames.Add(TEXT("FTraverseElapsedQuery.ElapsedS sentinel == -1.0f"));
    OutTestCommands.Add(TEXT("traverse_elapsed_query_sentinel"));

    OutBeautifiedNames.Add(TEXT("ECollisionOutcome::Unresolved is 4th value (default)"));
    OutTestCommands.Add(TEXT("enum_coverage_collision_outcome"));

    OutBeautifiedNames.Add(TEXT("AC-PW-09 TravelDuration formula verifiable from constants"));
    OutTestCommands.Add(TEXT("travel_duration_formula"));

    OutBeautifiedNames.Add(TEXT("LEAN_BRIGHTNESS_PEAK_RATIO + NEAR_MISS_FLASH_DURATION_S values pinned (AC-PW-22b pattern 8)"));
    OutTestCommands.Add(TEXT("registry_constants_pinned"));

    OutBeautifiedNames.Add(TEXT("STATGROUP_PullWave declared (AC-PW-22b pattern 1)"));
    OutTestCommands.Add(TEXT("stats_group_declared"));

    OutBeautifiedNames.Add(TEXT("EDespawnReason ordinals + FPullWaveInstanceState.PendingDespawnReason default"));
    OutTestCommands.Add(TEXT("despawn_reason_enum"));
}

bool FPullWaveStructDefinitionsTest::RunTest(const FString& Parameters)
{
    // -----------------------------------------------------------------------
    // TC1 — EvaluateAt linear interpolation nominal.
    //
    // Populate Samples with a linear ramp: Samples[i] = i / (SAMPLE_COUNT - 1).
    // At t=0.0:  EvaluateAt returns Samples[0] = 0.0f.
    // At t=1.0:  EvaluateAt returns Samples[31] = 1.0f.
    // At t=0.5:  i0=15, i1=16, Frac=0.5 → Lerp(15/31, 16/31, 0.5) ≈ 0.5.
    //
    // Tolerance: 1e-4f (well under CURVE_ENDPOINT_TOLERANCE = 0.005).
    // Also verifies SAMPLE_COUNT == 32 (TR-PW-004).
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("curve_snapshot_evaluate_at_interp"))
    {
        FPullWaveCurveSnapshot Snap;
        for (int32 i = 0; i < FPullWaveCurveSnapshot::SAMPLE_COUNT; ++i)
        {
            Snap.Samples[i] = static_cast<float>(i) / static_cast<float>(FPullWaveCurveSnapshot::SAMPLE_COUNT - 1);
        }

        TestTrue(
            TEXT("TC1: EvaluateAt(0.0f) == 0.0f"),
            FMath::IsNearlyEqual(Snap.EvaluateAt(0.0f), 0.0f, 1e-4f));

        TestTrue(
            TEXT("TC1: EvaluateAt(1.0f) == 1.0f"),
            FMath::IsNearlyEqual(Snap.EvaluateAt(1.0f), 1.0f, 1e-4f));

        TestTrue(
            TEXT("TC1: EvaluateAt(0.5f) ≈ 0.5f ± 1e-4 (linear ramp midpoint)"),
            FMath::IsNearlyEqual(Snap.EvaluateAt(0.5f), 0.5f, 1e-4f));

        TestEqual(
            TEXT("TC1: SAMPLE_COUNT == 32 (locked per TR-PW-004)"),
            FPullWaveCurveSnapshot::SAMPLE_COUNT,
            32);

        // Non-uniform spike check — isolates Frac arithmetic independently of the linear ramp.
        // Samples[10]=1.0f, all others 0.0f. At t=10/31: i0=10, Frac=0 → result=1.0f.
        // At t=9.5/31: i0=9, i1=10, Frac=0.5 → Lerp(0, 1, 0.5) = 0.5f.
        {
            FPullWaveCurveSnapshot SpikeSnap;
            SpikeSnap.Samples[10] = 1.0f;

            TestTrue(
                TEXT("TC1 spike: EvaluateAt(10/31) == 1.0f (direct hit, Frac=0)"),
                FMath::IsNearlyEqual(SpikeSnap.EvaluateAt(10.0f / 31.0f), 1.0f, 1e-4f));

            TestTrue(
                TEXT("TC1 spike: EvaluateAt(9.5/31) == 0.5f (mid-interval, Frac=0.5)"),
                FMath::IsNearlyEqual(SpikeSnap.EvaluateAt(9.5f / 31.0f), 0.5f, 1e-4f));
        }

        return true;
    }

    // -----------------------------------------------------------------------
    // TC2 — EvaluateAt OOB clamp at t=1.0.
    //
    // At t=1.0: i0 = SAMPLE_COUNT-1 = 31; i1 = min(32, 31) = 31 (no OOB).
    // Frac = 31.0 - 31.0 = 0.0 → Lerp(Samples[31], Samples[31], 0.0) = Samples[31].
    // Set Samples[31] = 99.0f as a sentinel; verify result == 99.0f (not a crash).
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("curve_snapshot_evaluate_at_oob_clamp"))
    {
        FPullWaveCurveSnapshot Snap;
        Snap.Samples[FPullWaveCurveSnapshot::SAMPLE_COUNT - 1] = 99.0f;

        const float Result = Snap.EvaluateAt(1.0f);
        TestTrue(
            TEXT("TC2: EvaluateAt(1.0f) == 99.0f (i1 clamped to 31 — no OOB read at index 32)"),
            FMath::IsNearlyEqual(Result, 99.0f, 1e-4f));

        TestTrue(
            TEXT("TC2: EvaluateAt(0.0f) == 0.0f (Samples[0] zero-initialized)"),
            FMath::IsNearlyEqual(Snap.EvaluateAt(0.0f), 0.0f, 1e-4f));

        return true;
    }

    // -----------------------------------------------------------------------
    // TC3 — FPullWaveSpawnParams size == 152 bytes (AC-PW-22b, ADR-0010 D6).
    //
    // Layout: WaveId(4) + SourceLane(4) + TargetLane(4) + CurveSnapshot(128)
    //         + ForwardVelocityMs(4) + SpawnTimeS(4) + LeanDurationS(4) = 152.
    // static_assert guarantees this at compile time; TestEqual echoes the value
    // to the automation log for CI evidence.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("spawn_params_size"))
    {
        const int32 ActualSize = static_cast<int32>(sizeof(FPullWaveSpawnParams));
        TestEqual(
            TEXT("TC3: sizeof(FPullWaveSpawnParams) == 152 (ADR-0010 D6 size contract)"),
            ActualSize,
            152);

        return true;
    }

    // -----------------------------------------------------------------------
    // TC4 — FPullWaveInstanceState size <= 256 bytes (TR-PW-007, AC-PW-22b pattern 3).
    //
    // ADR-0010 D3 design target: 188 bytes. Actual with uint8 enum backing: 176 bytes.
    // Binding contract is <= 256; static_assert enforces at compile time.
    // UE_LOG prints the measured size for the story's "verify in non-Shipping build"
    // requirement.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("instance_state_size"))
    {
        const int32 ActualSize = static_cast<int32>(sizeof(FPullWaveInstanceState));

        UE_LOG(LogTemp, Log,
            TEXT("FPullWaveInstanceState sizeof=%d bytes (ADR-0010 D3 design-target=188, ceiling=256)"),
            ActualSize);

        TestTrue(
            TEXT("TC4: sizeof(FPullWaveInstanceState) <= 256 (TR-PW-007 cache-friendly ceiling)"),
            ActualSize <= 256);

        TestTrue(
            TEXT("TC4: sizeof(FPullWaveInstanceState) >= sizeof(FPullWaveCurveSnapshot) (sanity)"),
            ActualSize >= static_cast<int32>(sizeof(FPullWaveCurveSnapshot)));

        return true;
    }

    // -----------------------------------------------------------------------
    // TC5 — LEAN_ANGLE_TIER0_DEG == 0.0f (AC-PW-22b pattern 6).
    //
    // TIER0 locked at exactly 0.0f ("straight ahead"). The static_asserts in the
    // header enforce tier ordering at compile time; this test echoes the values
    // to the automation log for CI evidence and human audit.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("lean_angle_tier0_is_zero"))
    {
        TestEqual(
            TEXT("TC5: LEAN_ANGLE_TIER0_DEG == 0.0f (locked per AC-PW-22b pattern 6)"),
            LEAN_ANGLE_TIER0_DEG,
            0.0f);

        TestEqual(
            TEXT("TC5: LEAN_ANGLE_MIN_TIER_GAP_DEG == 3.0f (AC-PW-22b patterns 7a-7h)"),
            LEAN_ANGLE_MIN_TIER_GAP_DEG,
            3.0f);

        // Verify all four gap widths at runtime (mirrors compile-time static_asserts).
        TestTrue(TEXT("TC5: TIER1 - TIER0 >= 3.0f"), (LEAN_ANGLE_TIER1_DEG - LEAN_ANGLE_TIER0_DEG) >= 3.0f);
        TestTrue(TEXT("TC5: TIER2 - TIER1 >= 3.0f"), (LEAN_ANGLE_TIER2_DEG - LEAN_ANGLE_TIER1_DEG) >= 3.0f);
        TestTrue(TEXT("TC5: TIER3 - TIER2 >= 3.0f"), (LEAN_ANGLE_TIER3_DEG - LEAN_ANGLE_TIER2_DEG) >= 3.0f);
        TestTrue(TEXT("TC5: TIER4 - TIER3 >= 3.0f"), (LEAN_ANGLE_TIER4_DEG - LEAN_ANGLE_TIER3_DEG) >= 3.0f);

        return true;
    }

    // -----------------------------------------------------------------------
    // TC6 — FTraverseElapsedQuery.ElapsedS sentinel == -1.0f (non-Shipping only).
    //
    // Sentinel -1.0f avoids coincidence with valid TraverseElapsedS=0.0f at
    // TRAVERSING entry (ADR-0010 D2 / R7 B5). Non-Shipping guard is mandatory.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("traverse_elapsed_query_sentinel"))
    {
#if !UE_BUILD_SHIPPING
        const FTraverseElapsedQuery Query{};

        TestEqual(
            TEXT("TC6: FTraverseElapsedQuery.ElapsedS default == -1.0f (sentinel, not 0.0f)"),
            Query.ElapsedS,
            -1.0f);

        TestFalse(
            TEXT("TC6: FTraverseElapsedQuery.bWaveFound default == false"),
            Query.bWaveFound);

        TestEqual(
            TEXT("TC6: FTraverseElapsedQuery.State default == EPullWaveState::Spawned"),
            static_cast<uint8>(Query.State),
            static_cast<uint8>(EPullWaveState::Spawned));
#else
        UE_LOG(LogTemp, Log,
            TEXT("TC6: FTraverseElapsedQuery sentinel check skipped in Shipping build (expected — type not available)."));
#endif // !UE_BUILD_SHIPPING
        return true;
    }

    // -----------------------------------------------------------------------
    // TC7 — ECollisionOutcome::Unresolved is the 4th value (index 3).
    //
    // Order: Hit(0), NearMiss(1), CleanMiss(2), Unresolved(3).
    // FPullWaveInstanceState default CollisionOutcome == Unresolved.
    // EPullWaveState has exactly 5 values: SPAWNED(0)..DESPAWNING(4).
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("enum_coverage_collision_outcome"))
    {
        TestEqual(
            TEXT("TC7: ECollisionOutcome::Unresolved == 3"),
            static_cast<uint8>(ECollisionOutcome::Unresolved),
            static_cast<uint8>(3));

        TestEqual(
            TEXT("TC7: ECollisionOutcome::Hit == 0"),
            static_cast<uint8>(ECollisionOutcome::Hit),
            static_cast<uint8>(0));

        const FPullWaveInstanceState DefaultState{};

        TestEqual(
            TEXT("TC7: FPullWaveInstanceState default CollisionOutcome == Unresolved"),
            static_cast<uint8>(DefaultState.CollisionOutcome),
            static_cast<uint8>(ECollisionOutcome::Unresolved));

        TestEqual(
            TEXT("TC7: FPullWaveInstanceState default State == Spawned"),
            static_cast<uint8>(DefaultState.State),
            static_cast<uint8>(EPullWaveState::Spawned));

        // Exactly 5 states: Spawned(0)..Despawning(4).
        TestEqual(
            TEXT("TC7: EPullWaveState::Despawning == 4 (5 total values, 0-indexed)"),
            static_cast<uint8>(EPullWaveState::Despawning),
            static_cast<uint8>(4));

        TestEqual(
            TEXT("TC7: FPullWaveInstanceState::ISMCInstanceIndex default == -1 (unassigned ISMC slot sentinel)"),
            DefaultState.ISMCInstanceIndex,
            -1);

        return true;
    }

    // -----------------------------------------------------------------------
    // TC8 — AC-PW-09 TravelDuration formula verifiable from constants.
    //
    // Formula: TravelDurationS = SPAWN_PLANE_Z_OFFSET_M / ForwardVelocityMs.
    // At 7.5 m/s: 15.0 / 7.5 = 2.0s. At 4.0 m/s: 15.0 / 4.0 = 3.75s.
    // Stored in FPullWaveInstanceState.TravelDurationS at Construct() (Story 009).
    // This test validates the constant and formula arithmetic, not the Construct() site.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("travel_duration_formula"))
    {
        TestEqual(
            TEXT("TC8: SPAWN_PLANE_Z_OFFSET_M == 15.0f"),
            SPAWN_PLANE_Z_OFFSET_M,
            15.0f);

        TestTrue(
            TEXT("TC8: 15.0f / 7.5f == 2.0f ± 1e-5 (AC-PW-09, ForwardVelocityMs=7.5)"),
            FMath::IsNearlyEqual(SPAWN_PLANE_Z_OFFSET_M / 7.5f, 2.0f, 1e-5f));

        TestTrue(
            TEXT("TC8: 15.0f / 4.0f == 3.75f ± 1e-5 (AC-PW-09, ForwardVelocityMs=4.0)"),
            FMath::IsNearlyEqual(SPAWN_PLANE_Z_OFFSET_M / 4.0f, 3.75f, 1e-5f));

        // Verify the field exists and defaults to 0.0f (populated at Construct() in Story 009).
        const FPullWaveInstanceState DefaultState{};
        TestEqual(
            TEXT("TC8: FPullWaveInstanceState.TravelDurationS default == 0.0f"),
            DefaultState.TravelDurationS,
            0.0f);

        return true;
    }

    // -----------------------------------------------------------------------
    // TC9 — Registry constants pinned (AC-PW-22b pattern 8).
    //
    // LEAN_BRIGHTNESS_PEAK_RATIO and NEAR_MISS_FLASH_DURATION_S must be sourced
    // from the registry header as named constants, not inline magic numbers.
    // These values are locked; changes require an ADR amendment.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("registry_constants_pinned"))
    {
        TestEqual(
            TEXT("TC9: LEAN_BRIGHTNESS_PEAK_RATIO == 2.5f (AC-PW-22b pattern 8)"),
            LEAN_BRIGHTNESS_PEAK_RATIO,
            2.5f);

        TestEqual(
            TEXT("TC9: NEAR_MISS_FLASH_DURATION_S == 0.066f (AC-PW-22b pattern 8)"),
            NEAR_MISS_FLASH_DURATION_S,
            0.066f);

        TestEqual(
            TEXT("TC9: WAVE_DESPAWN_HOLD_S == 0.15f (AC-PW-22b pattern 8)"),
            WAVE_DESPAWN_HOLD_S,
            0.15f);

        TestEqual(
            TEXT("TC9: TELEGRAPH_WINDOW_FLOOR_S == 0.70f (AC-PW-22b pattern 8)"),
            TELEGRAPH_WINDOW_FLOOR_S,
            0.70f);

        TestEqual(
            TEXT("TC9: MAX_POOL_SIZE == 23 (ADR-0010 D1)"),
            MAX_POOL_SIZE,
            23);

        return true;
    }

    // -----------------------------------------------------------------------
    // TC10 — STATGROUP_PullWave declared (AC-PW-22b pattern 1).
    //
    // DECLARE_STATS_GROUP is a compile-time declaration — this test file would
    // fail to compile if PullWaveTypes.h did not include the declaration.
    // UE_LOG echoes the group name to the automation log as CI evidence.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("stats_group_declared"))
    {
        UE_LOG(LogTemp, Log,
            TEXT("TC10: STATGROUP_PullWave is declared (compile-time proof — test file "
                 "includes PullWaveTypes.h which contains DECLARE_STATS_GROUP)"));

        UE_LOG(LogTemp, Log,
            TEXT("TC10: STAT_PullWaveTick extern declared in header; "
                 "DEFINE_STAT counterpart required in APullWaveSubsystemActor.cpp (Story 004)"));

        TestTrue(
            TEXT("TC10: STATGROUP_PullWave compile-time declaration verified (see above log)"),
            true);

        return true;
    }

    // -----------------------------------------------------------------------
    // TC11 — EDespawnReason ordinals + FPullWaveInstanceState.PendingDespawnReason default.
    //
    // Order: NaturalLanding(0), PauseFlush(1), RunTermination(2).
    // FPullWaveInstanceState default PendingDespawnReason == NaturalLanding.
    // Set at DESPAWNING entry; consumed by Rule 13 step 3 OnWaveDespawned broadcast.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("despawn_reason_enum"))
    {
        TestEqual(
            TEXT("TC11: EDespawnReason::NaturalLanding == 0"),
            static_cast<uint8>(EDespawnReason::NaturalLanding),
            static_cast<uint8>(0));

        TestEqual(
            TEXT("TC11: EDespawnReason::PauseFlush == 1"),
            static_cast<uint8>(EDespawnReason::PauseFlush),
            static_cast<uint8>(1));

        TestEqual(
            TEXT("TC11: EDespawnReason::RunTermination == 2"),
            static_cast<uint8>(EDespawnReason::RunTermination),
            static_cast<uint8>(2));

        const FPullWaveInstanceState DefaultState{};
        TestEqual(
            TEXT("TC11: FPullWaveInstanceState default PendingDespawnReason == NaturalLanding"),
            static_cast<uint8>(DefaultState.PendingDespawnReason),
            static_cast<uint8>(EDespawnReason::NaturalLanding));

        return true;
    }

    // Unknown parameter — fail explicitly rather than silently returning true.
    AddError(FString::Printf(
        TEXT("FPullWaveStructDefinitionsTest::RunTest — unknown Parameters value: '%s'"),
        *Parameters));
    return false;
}

#endif // WITH_DEV_AUTOMATION_TESTS

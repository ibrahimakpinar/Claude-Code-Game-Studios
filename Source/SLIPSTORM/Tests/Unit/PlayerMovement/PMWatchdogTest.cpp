// Copyright (c) Game Studio. All rights reserved.
// SLIPSTORM — 60-second voxel endless runner, mobile, Unreal Engine 5.7
//
// PMWatchdogTest.cpp — Story 013 unit tests for the DT watchdog:
// 60-slot rolling ring buffer advance, OR-composed breach detection
// (sustained sub-55 fps + hitch cluster), 3.0s continuous-clean
// hysteresis-release, OnHardwarePerformanceBreach delegate broadcast
// on state transition (idempotent within state), and raw_dt-not-effective_dt
// single-source-DT invariant.
//
// Spec: production/epics/player-movement/story-013-dt-watchdog.md
// GDD:  design/gdd/player-movement-platform.md §3 Runtime DT watchdog
//                                              §4 F-WATCHDOG-ROLLING-BUFFER
//                                              §8 AC-HW-A Setups A-G
//       R11a-6 (sentinel), R11a-7 (orthogonal criteria), R11a-8 (grace)
// ADR:  docs/architecture/adr-0009-player-movement-hosting.md (SD1, SD4)
// TR:   TR-PM-020 (60-sample rolling), TR-PM-021 (3.0s hysteresis-release),
//       TR-PM-022 (broadcast), TR-PM-024 (public flag), TR-PM-026 (Shipping)
//
// Test category: SLIPSTORM.PlayerMovement.Watchdog
// Runner: UE Automation Framework, headless (-nullrhi -nosound -unattended)
//         ClientContext | ProductFilter — matches Story 002/007/009 precedent.
//
// Pure-math tests — no UWorld or SpawnActor. All state is manipulated via
// NewObject<UPlayerLaneMovementComponent>() + direct field access through
// the FPMWatchdogTest friend declaration in PlayerLaneMovementComponent.h.
// BeginPlay is NOT called (would touch owner-pawn / RSM / curves which are
// out-of-scope for this Logic-type story); the watchdog fields are seeded
// directly via SEED_CLEAN_STATE / SEED_BREACH_WITH_CLEAN_BUFFER macros
// below (macros — NOT helpers — because free-function helpers cannot access
// private members even with a class-level friend declaration).
//
// Guard: WITH_DEV_AUTOMATION_TESTS only. No WITH_EDITOR — no editor APIs used.
//
// raw_dt invariant: the TickComponent call-site at PLMC.cpp passes raw_dt
// (from ComputeTickDT's F-PROLOGUE) — NOT effective_dt (which is clamped
// to MAX_SLIP_DT_S = 0.05f and would silently hide 2.0s stalls). TC10
// verifies the API contract by direct injection; the call-site plumbing
// is a code-review invariant (grep gate on WatchdogTick(raw_dt) callers).

#if WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#include "Player/PlayerLaneMovementComponent.h"

// ---------------------------------------------------------------------------
// Test-scope seeding macros — expand inline inside RunTest, which is a member
// of FPMWatchdogTest and therefore friends with UPlayerLaneMovementComponent.
// Free / anonymous-namespace helpers CANNOT access private fields (the friend
// declaration names the class, not arbitrary helpers — same pattern as
// PMCommitmentTellTest per line 91-95 of that file).
//
// SEED_CLEAN_STATE(pm)              — sentinel-fill buffer, reset all watchdog
//                                     state + test counters. Mirrors BeginPlay
//                                     R11a-6 init exactly. Post: count_gt_1818
//                                     == 0, count_gt_1667 == 0 on the whole
//                                     window (strict '>' comparison, boundary
//                                     value 0.01667f is clean).
// SEED_BREACH_WITH_CLEAN_BUFFER(pm) — same as above, then flip breach flags
//                                     ON to simulate "just-entered-breach with
//                                     a hypothetically-flushed buffer" — the
//                                     minimal precondition Setup D asserts on
//                                     (releases exactly when accumulator hits
//                                     3.0f, no buffer-flush latency).
// ---------------------------------------------------------------------------

#define SEED_CLEAN_STATE(pm)                                                                \
    do                                                                                       \
    {                                                                                        \
        for (int32 _seed_i = 0; _seed_i < 60; ++_seed_i)                                     \
        {                                                                                    \
            (pm)->TickDTRollingBuffer[_seed_i] = 0.01667f;                                   \
        }                                                                                    \
        (pm)->TickDTRingIndex                              = 0;                              \
        (pm)->ContinuousCleanWindowTime                    = 0.0f;                           \
        (pm)->bHardwarePerformanceBreachActive             = false;                          \
        (pm)->is_hw_performance_degraded                   = false;                          \
        (pm)->WatchdogBroadcastEnter_TestOnlyCallCount     = 0;                              \
        (pm)->WatchdogBroadcastRelease_TestOnlyCallCount   = 0;                              \
    } while (0)

#define SEED_BREACH_WITH_CLEAN_BUFFER(pm)                                                   \
    do                                                                                       \
    {                                                                                        \
        SEED_CLEAN_STATE(pm);                                                                \
        (pm)->bHardwarePerformanceBreachActive = true;                                       \
        (pm)->is_hw_performance_degraded       = true;                                       \
    } while (0)

// ---------------------------------------------------------------------------
// IMPLEMENT_COMPLEX_AUTOMATION_TEST
// 12 test commands covering AC-HW-A Setups A/B/C/D/E/F/G Part 2 +
// same-state idempotency + raw_dt-not-effective_dt invariant + Setup C
// max-below-33ms edge case + two exact-boundary comparator tests
// (0.01818f AT sustained threshold, 0.033f AT hitch max threshold —
// strict '>' means boundary values are NOT breaches).
// ---------------------------------------------------------------------------

// Context flags: EditorContext | ClientContext. The Editor's default headless
// discovery pass runs in EditorContext only; ClientContext-only tests are filtered
// out of the manifest and become undiscoverable via `Automation List` / `RunTests`
// without an active PIE session. Both flags satisfy static_asserts inside
// EAutomationTestFlags_ApplicationContextMask (UE 5.7 AutomationTest.h line 4110).
// See production/session-state/active.md epic-wide runtime-verification research.
IMPLEMENT_COMPLEX_AUTOMATION_TEST(
    FPMWatchdogTest,
    "SLIPSTORM.PlayerMovement.Watchdog",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

void FPMWatchdogTest::GetTests(
    TArray<FString>& OutBeautifiedNames,
    TArray<FString>& OutTestCommands) const
{
    OutBeautifiedNames.Add(TEXT("AC-HW-A Setup A clean baseline — 60x 0.01667s no breach"));
    OutTestCommands.Add(TEXT("setup_a_clean_baseline_no_breach"));

    OutBeautifiedNames.Add(TEXT("AC-HW-A Setup B sustained sub-55 entry — 30x 0.020s breaches on 30th"));
    OutTestCommands.Add(TEXT("setup_b_sustained_sub55_entry_at_30th"));

    OutBeautifiedNames.Add(TEXT("AC-HW-A Setup C hitch cluster entry — 17x 0.018 + 1x 0.040 breaches on 18th"));
    OutTestCommands.Add(TEXT("setup_c_hitch_cluster_entry_at_18th"));

    OutBeautifiedNames.Add(TEXT("AC-HW-A Setup C edge — 17x 0.018 + 1x 0.030 (max < 33ms) no breach"));
    OutTestCommands.Add(TEXT("setup_c_hitch_cluster_max_below_33ms_no_breach"));

    OutBeautifiedNames.Add(TEXT("AC-HW-A Setup D hysteresis-release — 180 clean samples fires release"));
    OutTestCommands.Add(TEXT("setup_d_hysteresis_release_at_180_samples"));

    OutBeautifiedNames.Add(TEXT("AC-HW-A Setup E flap prevention — single hitch never breaches, accumulator resets"));
    OutTestCommands.Add(TEXT("setup_e_flap_prevention_single_hitch"));

    OutBeautifiedNames.Add(TEXT("AC-HW-A Setup F subscriber correctness — bound lambda receives true/false exactly once each"));
    OutTestCommands.Add(TEXT("setup_f_subscriber_receives_transitions_no_duplicates"));

    OutBeautifiedNames.Add(TEXT("AC-HW-A Setup G Part 2 breach latency — 30 samples from sentinel-filled start"));
    OutTestCommands.Add(TEXT("setup_g_part2_breach_latency_within_30_samples"));

    OutBeautifiedNames.Add(TEXT("Idempotent same-state — additional degraded ticks fire no extra Broadcast(true)"));
    OutTestCommands.Add(TEXT("idempotent_same_state_no_extra_broadcast"));

    OutBeautifiedNames.Add(TEXT("raw_dt invariant — WatchdogTick stores the unclamped value (2.0s hitch)"));
    OutTestCommands.Add(TEXT("raw_dt_invariant_watchdog_sees_unclamped_hitch"));

    OutBeautifiedNames.Add(TEXT("Boundary — 30x exactly 0.01818f is NOT breach (strict > sustained threshold)"));
    OutTestCommands.Add(TEXT("boundary_sustained_exact_01818f_no_breach"));

    OutBeautifiedNames.Add(TEXT("Boundary — 17x 0.018 + 1x exactly 0.033f is NOT breach (strict > hitch threshold)"));
    OutTestCommands.Add(TEXT("boundary_hitch_exact_033f_no_breach"));
}

bool FPMWatchdogTest::RunTest(const FString& Parameters)
{
    // -----------------------------------------------------------------------
    // TC1 (AC-HW-A Setup A — clean baseline)
    //
    // Arrange: fresh PM with sentinel buffer (0.01667f x 60).
    // Act:     inject 60 samples of 0.01667f (nominal 60 fps).
    // Assert:  is_hw_performance_degraded == false throughout,
    //          bHardwarePerformanceBreachActive == false,
    //          WatchdogBroadcastEnter_TestOnlyCallCount == 0,
    //          ContinuousCleanWindowTime > 0 (accumulator advances since
    //          every sample is EXACTLY 0.01667f and count_gt_1667 stays 0
    //          — strict '>' comparison, boundary value is clean).
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("setup_a_clean_baseline_no_breach"))
    {
        UPlayerLaneMovementComponent* PM = NewObject<UPlayerLaneMovementComponent>();
        TestNotNull(TEXT("TC1: NewObject succeeded"), PM);
        if (!PM) { return false; }

        SEED_CLEAN_STATE(PM);

        for (int32 i = 0; i < 60; ++i)
        {
            PM->WatchdogTick(0.01667f);
        }

        TestFalse(TEXT("TC1: is_hw_performance_degraded stays false through 60 clean ticks"),
                  PM->is_hw_performance_degraded);
        TestFalse(TEXT("TC1: bHardwarePerformanceBreachActive stays false"),
                  PM->bHardwarePerformanceBreachActive);
        TestEqual(TEXT("TC1: no enter-broadcast fired"),
                  PM->WatchdogBroadcastEnter_TestOnlyCallCount, 0);
        TestEqual(TEXT("TC1: no release-broadcast fired"),
                  PM->WatchdogBroadcastRelease_TestOnlyCallCount, 0);
        TestTrue(TEXT("TC1: ContinuousCleanWindowTime advanced (>= 60 x 0.01667 = 1.0002)"),
                 PM->ContinuousCleanWindowTime >= 1.0f);
        return true;
    }

    // -----------------------------------------------------------------------
    // TC2 (AC-HW-A Setup B — sustained sub-55 entry)
    //
    // Arrange: fresh sentinel-filled PM.
    // Act:     tick 29 samples of 0.020f (below the >= 30 threshold), then
    //          tick the 30th sample.
    // Assert:  after 29 ticks no breach; after tick 30 breach fires exactly
    //          once with is_hw_performance_degraded == true.
    // Also verify: ticking a 31st degraded sample does NOT double-broadcast
    //              (idempotent within-state — the state-machine same-state
    //              branch owns this).
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("setup_b_sustained_sub55_entry_at_30th"))
    {
        UPlayerLaneMovementComponent* PM = NewObject<UPlayerLaneMovementComponent>();
        TestNotNull(TEXT("TC2: NewObject succeeded"), PM);
        if (!PM) { return false; }

        SEED_CLEAN_STATE(PM);

        // Expect the one Error log at breach entry (subst-match).
        AddExpectedError(TEXT("Watchdog entered breach"),
                         EAutomationExpectedErrorFlags::Contains, 1);

        // 29 degraded ticks — below the threshold.
        for (int32 i = 0; i < 29; ++i)
        {
            PM->WatchdogTick(0.020f);
        }
        TestFalse(TEXT("TC2: after 29 x 0.020s ticks, still not in breach"),
                  PM->is_hw_performance_degraded);
        TestEqual(TEXT("TC2: no enter-broadcast after 29 ticks"),
                  PM->WatchdogBroadcastEnter_TestOnlyCallCount, 0);

        // 30th degraded tick — breach fires.
        PM->WatchdogTick(0.020f);
        TestTrue(TEXT("TC2: on 30th 0.020s tick, is_hw_performance_degraded == true"),
                 PM->is_hw_performance_degraded);
        TestTrue(TEXT("TC2: on 30th tick, bHardwarePerformanceBreachActive == true"),
                 PM->bHardwarePerformanceBreachActive);
        TestEqual(TEXT("TC2: enter-broadcast fired exactly once on 30th"),
                  PM->WatchdogBroadcastEnter_TestOnlyCallCount, 1);

        // 31st degraded tick — no double-broadcast.
        PM->WatchdogTick(0.020f);
        TestEqual(TEXT("TC2: enter-broadcast stays at 1 after 31st tick (idempotent same-state)"),
                  PM->WatchdogBroadcastEnter_TestOnlyCallCount, 1);
        return true;
    }

    // -----------------------------------------------------------------------
    // TC3 (AC-HW-A Setup C — hitch cluster entry)
    //
    // Arrange: fresh sentinel-filled PM.
    // Act:     inject 17 samples of 0.018f (crosses > 16.67ms but NOT
    //          > 18.18ms — sentinels above 60 fps but below 55 fps
    //          threshold), then 1 sample of 0.040f (a visible hitch >
    //          33ms; also crosses both counters + max).
    // Assert:  after 17 samples no breach; after the 18th (0.040f) breach
    //          fires. Verifies hitch-cluster criterion:
    //          (count_gt_1667 >= 18) AND (max_sample > 0.033f).
    //
    // Note: 0.018f > 0.01667f (true) and 0.018f > 0.01818f (false — 0.018 in
    // float rounds slightly below 0.01818f), so 0.018 samples only increment
    // count_gt_1667. On the 18th tick:
    //   count_gt_1667 = 18 (17 x 0.018 + 1 x 0.040)
    //   count_gt_1818 = 1  (only the 0.040 sample)
    //   max_sample    = 0.040
    // Breach condition second clause fires: (18 >= 18) && (0.040 > 0.033).
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("setup_c_hitch_cluster_entry_at_18th"))
    {
        UPlayerLaneMovementComponent* PM = NewObject<UPlayerLaneMovementComponent>();
        TestNotNull(TEXT("TC3: NewObject succeeded"), PM);
        if (!PM) { return false; }

        SEED_CLEAN_STATE(PM);

        AddExpectedError(TEXT("Watchdog entered breach"),
                         EAutomationExpectedErrorFlags::Contains, 1);

        for (int32 i = 0; i < 17; ++i)
        {
            PM->WatchdogTick(0.018f);
        }
        TestFalse(TEXT("TC3: after 17 x 0.018s (count_gt_1667 = 17 < 18), no breach"),
                  PM->is_hw_performance_degraded);
        TestEqual(TEXT("TC3: no enter-broadcast after 17 sub-60 samples"),
                  PM->WatchdogBroadcastEnter_TestOnlyCallCount, 0);

        // 18th sample — the 0.040f hitch. Now count_gt_1667 == 18, max == 0.040 > 0.033.
        PM->WatchdogTick(0.040f);
        TestTrue(TEXT("TC3: on 18th sample (0.040 hitch), breach fires"),
                 PM->is_hw_performance_degraded);
        TestEqual(TEXT("TC3: enter-broadcast fired exactly once"),
                  PM->WatchdogBroadcastEnter_TestOnlyCallCount, 1);
        return true;
    }

    // -----------------------------------------------------------------------
    // TC4 (AC-HW-A Setup C edge — max sample below 33ms threshold)
    //
    // Arrange: fresh sentinel-filled PM.
    // Act:     17 x 0.018f + 1 x 0.030f. The max sample (0.030) is BELOW
    //          the 33ms threshold, so the hitch-cluster criterion FAILS
    //          even though count_gt_1667 == 18. Sustained criterion also
    //          fails (count_gt_1818 = 0 — 0.018 and 0.030 both < 0.01818f
    //          ...wait, 0.030 > 0.01818f is TRUE. Re-derive: count_gt_1818
    //          = 1 after the 0.030 tick. Still < 30 threshold, so primary
    //          criterion fails).
    // Assert:  no breach fires — this is the "hitch cluster without a
    //          visible-hitch peak" negative case.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("setup_c_hitch_cluster_max_below_33ms_no_breach"))
    {
        UPlayerLaneMovementComponent* PM = NewObject<UPlayerLaneMovementComponent>();
        TestNotNull(TEXT("TC4: NewObject succeeded"), PM);
        if (!PM) { return false; }

        SEED_CLEAN_STATE(PM);

        for (int32 i = 0; i < 17; ++i)
        {
            PM->WatchdogTick(0.018f);
        }
        // 18th sample only 0.030 — below the > 33ms visible-hitch guard.
        PM->WatchdogTick(0.030f);

        TestFalse(TEXT("TC4: max_sample 0.030 (< 0.033 threshold) means no hitch-cluster breach"),
                  PM->is_hw_performance_degraded);
        TestFalse(TEXT("TC4: bHardwarePerformanceBreachActive stays false"),
                  PM->bHardwarePerformanceBreachActive);
        TestEqual(TEXT("TC4: no enter-broadcast on max-below-threshold case"),
                  PM->WatchdogBroadcastEnter_TestOnlyCallCount, 0);
        return true;
    }

    // -----------------------------------------------------------------------
    // TC5 (AC-HW-A Setup D — hysteresis-release)
    //
    // Arrange: PM already-in-breach with a fully-clean buffer (seeded via
    //          SEED_BREACH_WITH_CLEAN_BUFFER). This decouples the release test
    //          from the entry sequence — the sequential Setup-B → Setup-D
    //          path would need buffer flush ticks first (~30 flush + 180
    //          accumulate = 210 ticks total), which is a real behavior but
    //          not what the AC-D wording measures.
    // Act:     inject 179 samples of 0.01667f (below the 3.0s threshold),
    //          then the 180th.
    // Assert:  after 179 clean ticks still in breach (accumulator ==
    //          179 * 0.01667 = 2.9829 < 3.0); on the 180th, release fires
    //          exactly once with Broadcast(false).
    //
    // Note on Setup D wording: "PM in breach, inject 180 samples → release"
    // literally holds only when the buffer is already clean at the release
    // phase start. The GDD line 168-170 pseudo-code makes ContinuousClean-
    // WindowTime advance conditional on ALL 60 slots being clean (matches
    // spec's sample code `all_clean = (count_gt_1667 == 0)`), which is
    // correct defensive design (release requires sustained buffer-wide
    // recovery). The sequential path is intrinsic to the design and would
    // be exercised in an integration test with real gameplay; this Logic-
    // type story validates the release condition in isolation.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("setup_d_hysteresis_release_at_180_samples"))
    {
        UPlayerLaneMovementComponent* PM = NewObject<UPlayerLaneMovementComponent>();
        TestNotNull(TEXT("TC5: NewObject succeeded"), PM);
        if (!PM) { return false; }

        SEED_BREACH_WITH_CLEAN_BUFFER(PM);
        TestTrue(TEXT("TC5: precondition — in breach (internal flag)"),
                 PM->bHardwarePerformanceBreachActive);
        TestTrue(TEXT("TC5: precondition — is_hw_performance_degraded (public flag) — symmetric with release assertion"),
                 PM->is_hw_performance_degraded);

        for (int32 i = 0; i < 179; ++i)
        {
            PM->WatchdogTick(0.01667f);
        }
        TestTrue(TEXT("TC5: after 179 clean ticks (2.9829s < 3.0s), still in breach"),
                 PM->bHardwarePerformanceBreachActive);
        TestTrue(TEXT("TC5: is_hw_performance_degraded still true after 179 clean"),
                 PM->is_hw_performance_degraded);
        TestEqual(TEXT("TC5: no release-broadcast yet"),
                  PM->WatchdogBroadcastRelease_TestOnlyCallCount, 0);

        // 180th sample — accumulator crosses 3.0f exactly (180 * 0.01667 = 3.0006).
        PM->WatchdogTick(0.01667f);
        TestFalse(TEXT("TC5: on 180th clean tick, bHardwarePerformanceBreachActive == false"),
                  PM->bHardwarePerformanceBreachActive);
        TestFalse(TEXT("TC5: is_hw_performance_degraded == false"),
                  PM->is_hw_performance_degraded);
        TestEqual(TEXT("TC5: release-broadcast fired exactly once"),
                  PM->WatchdogBroadcastRelease_TestOnlyCallCount, 1);
        return true;
    }

    // -----------------------------------------------------------------------
    // TC6 (AC-HW-A Setup E — flap prevention)
    //
    // Arrange: fresh sentinel-filled PM.
    // Act:     100 clean ticks, then 1 hitch of 0.020f, then 100 more clean
    //          ticks.
    // Assert:  is_hw_performance_degraded never becomes true (single hitch
    //          nowhere near the 30-sample sustained threshold); accumulator
    //          resets to 0 on the hitch tick, then advances again after.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("setup_e_flap_prevention_single_hitch"))
    {
        UPlayerLaneMovementComponent* PM = NewObject<UPlayerLaneMovementComponent>();
        TestNotNull(TEXT("TC6: NewObject succeeded"), PM);
        if (!PM) { return false; }

        SEED_CLEAN_STATE(PM);

        // 100 clean ticks — accumulator crosses 1.6s (>> anything sensitive).
        for (int32 i = 0; i < 100; ++i)
        {
            PM->WatchdogTick(0.01667f);
        }
        TestTrue(TEXT("TC6: after 100 clean ticks, accumulator > 1.5s"),
                 PM->ContinuousCleanWindowTime > 1.5f);
        TestFalse(TEXT("TC6: after 100 clean ticks, no breach"),
                  PM->is_hw_performance_degraded);

        // 1 hitch tick — resets accumulator; but 1 hitch alone can't breach.
        PM->WatchdogTick(0.020f);
        TestFalse(TEXT("TC6: 1 hitch in a clean window: still no breach (1 < 30)"),
                  PM->is_hw_performance_degraded);
        TestEqual(TEXT("TC6: accumulator RESET to 0 on the hitch tick"),
                  PM->ContinuousCleanWindowTime, 0.0f);
        TestEqual(TEXT("TC6: still no enter-broadcast"),
                  PM->WatchdogBroadcastEnter_TestOnlyCallCount, 0);

        // 100 clean ticks after — accumulator advances again, but only after
        // the buffer flushes the single hitch. We just verify it does advance
        // beyond zero at some point and that we still never breach.
        for (int32 i = 0; i < 100; ++i)
        {
            PM->WatchdogTick(0.01667f);
        }
        TestTrue(TEXT("TC6: accumulator advances again after hitch flushes from buffer"),
                 PM->ContinuousCleanWindowTime > 0.5f);
        TestFalse(TEXT("TC6: still no breach after post-hitch clean stretch"),
                  PM->is_hw_performance_degraded);
        TestEqual(TEXT("TC6: enter-broadcast count remains 0 through entire flap sequence"),
                  PM->WatchdogBroadcastEnter_TestOnlyCallCount, 0);
        return true;
    }

    // -----------------------------------------------------------------------
    // TC7 (AC-HW-A Setup F — broadcast subscriber correctness)
    //
    // Arrange: fresh sentinel-filled PM, bind a lambda subscriber that
    //          records every bEntering value into a local array.
    // Act:     drive full entry (30 x 0.020) then full release (180 x
    //          0.01667 from clean-buffer breach state — re-seed between
    //          phases so release phase runs cleanly).
    // Assert:  subscriber received exactly [true, false] in order,
    //          no duplicates.
    //
    // This test validates the delegate wiring end-to-end — orthogonal to
    // TC2/TC5 which just count the internal test counter.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("setup_f_subscriber_receives_transitions_no_duplicates"))
    {
        UPlayerLaneMovementComponent* PM = NewObject<UPlayerLaneMovementComponent>();
        TestNotNull(TEXT("TC7: NewObject succeeded"), PM);
        if (!PM) { return false; }

        TArray<bool> Received;
        // ADR-0009 IG-3 AddUObject-only rule governs PM's OUTBOUND bindings to RSM
        // (GC safety on subscriber lifetime). Test-scope lambda with explicit
        // Remove(Handle) below is exempt — subscriber lifetime is bounded by RunTest.
        FDelegateHandle Handle = PM->OnHardwarePerformanceBreach.AddLambda(
            [&Received](bool bEntering)
            {
                Received.Add(bEntering);
            });

        // Phase 1: entry via 30 x 0.020 from clean.
        SEED_CLEAN_STATE(PM);
        AddExpectedError(TEXT("Watchdog entered breach"),
                         EAutomationExpectedErrorFlags::Contains, 1);
        for (int32 i = 0; i < 30; ++i)
        {
            PM->WatchdogTick(0.020f);
        }
        TestEqual(TEXT("TC7: after entry phase, subscriber has exactly 1 event"),
                  Received.Num(), 1);
        TestTrue(TEXT("TC7: first event bEntering == true"),
                 Received.Num() >= 1 && Received[0] == true);

        // Phase 2: release via 180 clean from breach-with-clean-buffer state.
        // Re-seed to avoid re-entering breach through the sequential path.
        SEED_BREACH_WITH_CLEAN_BUFFER(PM);
        for (int32 i = 0; i < 180; ++i)
        {
            PM->WatchdogTick(0.01667f);
        }
        TestEqual(TEXT("TC7: after release phase, subscriber has exactly 2 events"),
                  Received.Num(), 2);
        TestTrue(TEXT("TC7: second event bEntering == false"),
                 Received.Num() >= 2 && Received[1] == false);

        // Cleanup — unbind to prevent stale lambda if PM survives past test scope.
        PM->OnHardwarePerformanceBreach.Remove(Handle);
        return true;
    }

    // -----------------------------------------------------------------------
    // TC8 (AC-HW-A Setup G Part 2 — breach latency from sentinel-filled start)
    //
    // Arrange: fresh PM with sentinel pre-fill (0.01667f x 60). This mirrors
    //          PIE-start conditions per R11a-6 — buffer is populated at
    //          BeginPlay so the 1.0s startup window is not blind.
    // Act:     start injecting 0.020f samples from tick 1.
    // Assert:  breach entry occurs at sample 30 exactly (± 0 in this pure
    //          math case). Latency <= 60 samples from degradation onset.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("setup_g_part2_breach_latency_within_30_samples"))
    {
        UPlayerLaneMovementComponent* PM = NewObject<UPlayerLaneMovementComponent>();
        TestNotNull(TEXT("TC8: NewObject succeeded"), PM);
        if (!PM) { return false; }

        SEED_CLEAN_STATE(PM); // sentinel-filled — mimics BeginPlay per R11a-6

        AddExpectedError(TEXT("Watchdog entered breach"),
                         EAutomationExpectedErrorFlags::Contains, 1);

        int32 BreachOnTick = -1;
        for (int32 i = 1; i <= 60; ++i)
        {
            PM->WatchdogTick(0.020f);
            if (PM->WatchdogBroadcastEnter_TestOnlyCallCount == 1 && BreachOnTick == -1)
            {
                BreachOnTick = i;
                break;
            }
        }
        TestTrue(TEXT("TC8: breach entry observed within first 60 samples"),
                 BreachOnTick > 0);
        // Story AC line 56 wording: "latency = 30 samples ± 1". Pure-math
        // injection is deterministic-30, but the AC's ± 1 tolerance is the
        // spec — asserting exactly 30 would fail behavior that is within
        // spec if any future warmup gate or index-advance ordering shifts.
        TestTrue(TEXT("TC8: breach entry latency == 30 samples ± 1 per AC-HW-A Setup G Part 2"),
                 BreachOnTick >= 29 && BreachOnTick <= 31);
        return true;
    }

    // -----------------------------------------------------------------------
    // TC9 (Idempotent same-state — no re-broadcast during sustained breach)
    //
    // Arrange: PM in breach (via sequential entry).
    // Act:     tick many more degraded samples.
    // Assert:  WatchdogBroadcastEnter_TestOnlyCallCount stays at 1.
    //
    // Explicit orthogonal test — TC2 covers 1 extra tick; this covers a
    // sustained-degradation stress path so any accidental broadcast-on-tick
    // regression would show up.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("idempotent_same_state_no_extra_broadcast"))
    {
        UPlayerLaneMovementComponent* PM = NewObject<UPlayerLaneMovementComponent>();
        TestNotNull(TEXT("TC9: NewObject succeeded"), PM);
        if (!PM) { return false; }

        SEED_CLEAN_STATE(PM);

        AddExpectedError(TEXT("Watchdog entered breach"),
                         EAutomationExpectedErrorFlags::Contains, 1);

        for (int32 i = 0; i < 30; ++i)
        {
            PM->WatchdogTick(0.020f);
        }
        TestEqual(TEXT("TC9: enter-broadcast fires exactly once on breach entry"),
                  PM->WatchdogBroadcastEnter_TestOnlyCallCount, 1);

        // 100 more degraded ticks — sustained breach.
        for (int32 i = 0; i < 100; ++i)
        {
            PM->WatchdogTick(0.020f);
        }
        TestEqual(TEXT("TC9: after 100 additional degraded ticks, enter-count still 1"),
                  PM->WatchdogBroadcastEnter_TestOnlyCallCount, 1);
        TestEqual(TEXT("TC9: no release fires while breach is sustained"),
                  PM->WatchdogBroadcastRelease_TestOnlyCallCount, 0);
        TestTrue(TEXT("TC9: still in breach"),
                 PM->bHardwarePerformanceBreachActive);
        return true;
    }

    // -----------------------------------------------------------------------
    // TC10 (raw_dt invariant — WatchdogTick honors the unclamped value)
    //
    // Arrange: fresh sentinel-filled PM.
    // Act:     inject a 2.0f (2-second stall) sample. The mechanics-side
    //          F-PROLOGUE would clamp this to effective_dt = 0.05f; the
    //          watchdog MUST see the raw 2.0f value in its buffer,
    //          otherwise sub-30-fps stalls would be hidden.
    // Assert:  the newest buffer slot contains 2.0f. This verifies the
    //          WatchdogTick API contract accepts and stores unclamped input;
    //          the TickComponent call-site passes raw_dt (from ComputeTickDT)
    //          per source line convention — code-review invariant.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("raw_dt_invariant_watchdog_sees_unclamped_hitch"))
    {
        UPlayerLaneMovementComponent* PM = NewObject<UPlayerLaneMovementComponent>();
        TestNotNull(TEXT("TC10: NewObject succeeded"), PM);
        if (!PM) { return false; }

        SEED_CLEAN_STATE(PM);

        // A single 2.0f tick will NOT trigger breach — both criteria require
        // multiple samples across the 60-slot window: sustained-sub-55 needs
        // count_gt_1818 >= 30 and hitch-cluster needs count_gt_1667 >= 18
        // AND max > 0.033. From a sentinel-filled buffer, one sample crosses
        // only 1 of each counter, so neither branch fires. Perfect: no breach
        // expected, we're only checking storage.
        PM->WatchdogTick(2.0f);

        // Newest sample is at TickDTRingIndex - 1 (advanced after write).
        // From index 0 after SEED_CLEAN_STATE → wrote to slot 0 → index now 1.
        // Newest sample is at slot (index - 1 + 60) % 60 = slot 0.
        const int32 NewestSlot = (PM->TickDTRingIndex + 60 - 1) % 60;
        TestEqual(TEXT("TC10: newest slot contains the raw 2.0f (unclamped, not effective_dt=0.05f)"),
                  PM->TickDTRollingBuffer[NewestSlot], 2.0f);

        // No breach — single hitch fails both criteria.
        TestFalse(TEXT("TC10: single 2.0f hitch alone does not breach (below thresholds)"),
                  PM->is_hw_performance_degraded);
        return true;
    }

    // -----------------------------------------------------------------------
    // TC11 (Boundary — exact 0.01818f sustained-threshold test)
    //
    // Arrange: fresh sentinel-filled PM.
    // Act:     inject 30 samples of EXACTLY 0.01818f (the sustained
    //          count_gt_1818 comparator boundary).
    // Assert:  no breach fires. Strict '>' means 0.01818f is NOT counted
    //          as > 0.01818f — count_gt_1818 stays 0 across the window,
    //          so the primary criterion (>= 30) cannot fire. The secondary
    //          hitch-cluster criterion also fails because max_sample =
    //          0.01818f fails the > 0.033f guard.
    //
    // Guards the primary comparator against a `>=` regression that would
    // silently trip breach at nominal 55 fps.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("boundary_sustained_exact_01818f_no_breach"))
    {
        UPlayerLaneMovementComponent* PM = NewObject<UPlayerLaneMovementComponent>();
        TestNotNull(TEXT("TC11: NewObject succeeded"), PM);
        if (!PM) { return false; }

        SEED_CLEAN_STATE(PM);

        for (int32 i = 0; i < 30; ++i)
        {
            PM->WatchdogTick(0.01818f);
        }
        TestFalse(TEXT("TC11: 30x exact 0.01818f does NOT breach (strict > boundary)"),
                  PM->is_hw_performance_degraded);
        TestFalse(TEXT("TC11: bHardwarePerformanceBreachActive stays false"),
                  PM->bHardwarePerformanceBreachActive);
        TestEqual(TEXT("TC11: no enter-broadcast on exact-boundary sustained samples"),
                  PM->WatchdogBroadcastEnter_TestOnlyCallCount, 0);
        return true;
    }

    // -----------------------------------------------------------------------
    // TC12 (Boundary — exact 0.033f hitch-max-threshold test)
    //
    // Arrange: fresh sentinel-filled PM.
    // Act:     17 x 0.018f + 1 x EXACTLY 0.033f. count_gt_1667 = 18 after
    //          the last sample (all 18 samples cross 0.01667f); max_sample
    //          = 0.033f.
    // Assert:  no breach fires. The secondary hitch-cluster criterion
    //          requires (count_gt_1667 >= 18) AND (max_sample > 0.033f).
    //          Strict '>' means 0.033f is NOT counted as > 0.033f, so the
    //          AND clause fails. Primary sustained criterion also fails
    //          (count_gt_1818 = 1 from the 0.033f sample, well below 30).
    //
    // Guards the visible-hitch max threshold against a `>=` regression
    // that would trip breach on merely-borderline 33 ms samples.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("boundary_hitch_exact_033f_no_breach"))
    {
        UPlayerLaneMovementComponent* PM = NewObject<UPlayerLaneMovementComponent>();
        TestNotNull(TEXT("TC12: NewObject succeeded"), PM);
        if (!PM) { return false; }

        SEED_CLEAN_STATE(PM);

        for (int32 i = 0; i < 17; ++i)
        {
            PM->WatchdogTick(0.018f);
        }
        // 18th sample AT the visible-hitch boundary — should NOT breach.
        PM->WatchdogTick(0.033f);

        TestFalse(TEXT("TC12: max_sample exactly 0.033f (not > 0.033f) does NOT trip hitch-cluster"),
                  PM->is_hw_performance_degraded);
        TestFalse(TEXT("TC12: bHardwarePerformanceBreachActive stays false"),
                  PM->bHardwarePerformanceBreachActive);
        TestEqual(TEXT("TC12: no enter-broadcast on exact-boundary hitch max"),
                  PM->WatchdogBroadcastEnter_TestOnlyCallCount, 0);
        return true;
    }

    return false;
}

#endif // WITH_DEV_AUTOMATION_TESTS

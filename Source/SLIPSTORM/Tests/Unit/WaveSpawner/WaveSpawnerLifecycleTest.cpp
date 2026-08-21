// Copyright (c) Game Studio. All rights reserved.
// SLIPSTORM — 60-second voxel endless runner, mobile, Unreal Engine 5.7
//
// WaveSpawnerLifecycleTest.cpp — Story 002 unit tests for the six-state lifecycle,
// atomic pool-pointer swap, Rule 9 drain window, and forbidden transition enforcement.
//
// Story: production/epics/wave-spawner/story-002-six-state-lifecycle.md
// ADR:   docs/architecture/adr-0011-wave-spawner-pattern-library.md (D3)
// TRs:   TR-WS-011 (lifecycle state gating / IsTickable())
//         TR-WS-020 (pool phase initialization at Cold→Active)
//         TR-WS-021 (atomic pool-pointer swap; pattern data never copied)
//         TR-WS-022 (in-flight wave snapshot immutability after pool swap)
//         TR-WS-023 (drain window clearance → Holding→Active)
//
// Test category: SLIPSTORM.WaveSpawner.Lifecycle
// Runner:        UE Automation Framework
//                Flags: ApplicationContextMask | ProductFilter
//
// All 7 test commands are fully headless (no UWorld needed).
// NewObject<UWaveSpawnerSubsystem>(GetTransientPackage()) + TestOnly_* seams used throughout.
//
// ADR deviation documented here (also in WaveSpawnerSubsystem.cpp):
//   Story AC-WS-15a lists 8 transitions including "Flushing→Idle" and "Idle→Cold".
//   ADR-0011 D3 is authoritative and defines 9 valid transitions, superseding those ACs:
//     - Flushing→Cold  (run-termination path; not via Idle)
//     - Holding→Idle   (long-pause path; waves despawn naturally during Holding)
//     - Idle→Active    (resume from long pause; no Cold reset needed)
//   Tests enforce the ADR D3 table. Story file ACs will be reconciled by the story owner.
//
// Forbidden-transition test design (Option A — no ADR deviation):
//   TransitionTo() calls check(false) in non-Shipping builds, which aborts the process.
//   AddExpectedError() cannot catch check() — it only filters UE_LOG entries.
//   Tests therefore call TestOnly_IsValidTransition() (predicate seam) directly instead of
//   TransitionTo(), asserting the predicate returns false for forbidden pairs.
//   This gives equivalent AC coverage: IsValidTransition() IS the gate that TransitionTo()
//   enforces. See TestOnly_IsValidTransition() doc comment in WaveSpawnerSubsystem.h.
//
// Guard: WITH_DEV_AUTOMATION_TESTS only.

#if WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#include "WaveSpawner/WaveSpawnerSubsystem.h"
#include "WaveSpawner/WaveSpawnerTypes.h"
#include "WaveSpawner/Wave.h"

// ============================================================================
// IMPLEMENT_COMPLEX_AUTOMATION_TEST
// 7 test commands covering Story 002 acceptance criteria.
// Pattern matches WaveSpawnerPoolTest.cpp (Story 001 precedent).
// ============================================================================

IMPLEMENT_COMPLEX_AUTOMATION_TEST(
    FWaveSpawnerLifecycleTest,
    "SLIPSTORM.WaveSpawner.Lifecycle",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

void FWaveSpawnerLifecycleTest::GetTests(
    TArray<FString>& OutBeautifiedNames,
    TArray<FString>& OutTestCommands) const
{
    OutBeautifiedNames.Add(
        TEXT("All 9 valid ADR-D3 transitions accepted; IsTickable() gating correct (AC-WS-15a)"));
    OutTestCommands.Add(TEXT("Lifecycle.ValidTransitions"));

    OutBeautifiedNames.Add(
        TEXT("Forbidden transitions rejected by predicate; 4 forbidden pairs tested (AC-WS-15a)"));
    OutTestCommands.Add(TEXT("Lifecycle.ForbiddenTransitions"));

    OutBeautifiedNames.Add(
        TEXT("Pool swap: Active→Holding advances Opener→Mid atomically (AC-WS-15b, TR-WS-021)"));
    OutTestCommands.Add(TEXT("Lifecycle.PoolSwapOpenerToMid"));

    OutBeautifiedNames.Add(
        TEXT("Pool swap: second Active→Holding advances Mid→Peak atomically (AC-WS-15b, TR-WS-021)"));
    OutTestCommands.Add(TEXT("Lifecycle.PoolSwapMidToPeak"));

    OutBeautifiedNames.Add(
        TEXT("Pool swap does NOT mutate in-flight AWave fields (AC-WS-15b, TR-WS-022)"));
    OutTestCommands.Add(TEXT("Lifecycle.InFlightImmutability"));

    OutBeautifiedNames.Add(
        TEXT("Drain window: true on Holding entry, false after Holding→Active (AC-WS-15c, TR-WS-023)"));
    OutTestCommands.Add(TEXT("Lifecycle.DrainWindowClearance"));

    OutBeautifiedNames.Add(
        TEXT("Flushing→Cold resets ActiveDrawPool, ActivePhase, bDrainWindowActive (AC-WS-15c)"));
    OutTestCommands.Add(TEXT("Lifecycle.ColdResetOnTermination"));
}

bool FWaveSpawnerLifecycleTest::RunTest(const FString& Parameters)
{
    // -------------------------------------------------------------------------
    // TC1 — AC-WS-15a (TR-WS-011, TR-WS-020):
    // All 9 valid ADR-0011 D3 transitions are accepted by TransitionTo().
    // IsTickable() returns false in Cold and Idle; true in Active, Holding, Flushing.
    //
    // Transition sequence (all 9 unique ADR D3 valid pairs exercised):
    //   (1) Cold→Active         (2) Active→Holding    (3) Holding→Active
    //   (4) Active→Holding      (5) Holding→Idle      (6) Idle→Active
    //   (7) Active→Flushing     (8) Flushing→Cold     (9a) Cold→Active
    //   (9b) Active→Holding     (9c) Holding→Flushing (9d) Flushing→Active
    //   Covers all 9: pairs (1)(2)(5)(6)(7)(8) explicit; (3)(4)(9c)(9d) via loops.
    //
    // Given:  fresh UWaveSpawnerSubsystem (LifecycleState == Cold).
    // When:   TransitionTo() is called through each valid pair.
    // Then:   State changes correctly; IsTickable() contracts held at each state.
    // -------------------------------------------------------------------------
    if (Parameters == TEXT("Lifecycle.ValidTransitions"))
    {
        UWaveSpawnerSubsystem* Sub =
            NewObject<UWaveSpawnerSubsystem>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC1: NewObject<UWaveSpawnerSubsystem> succeeded"), Sub))
        {
            return false;
        }

        // Initial state: Cold — tick suppressed
        TestEqual(TEXT("TC1: Initial state is Cold"),
            Sub->TestOnly_GetLifecycleState(),
            EWaveSpawnerLifecycleState::Cold);
        TestFalse(TEXT("TC1: IsTickable() == false in Cold (TR-WS-011)"),
            Sub->IsTickable());

        // (1) Cold → Active
        Sub->TransitionTo(EWaveSpawnerLifecycleState::Active);
        TestEqual(TEXT("TC1 (1/9): Cold→Active"),
            Sub->TestOnly_GetLifecycleState(),
            EWaveSpawnerLifecycleState::Active);
        TestTrue(TEXT("TC1: IsTickable() == true in Active"), Sub->IsTickable());

        // (2) Active → Holding  (Opener→Mid pool swap fires)
        Sub->TransitionTo(EWaveSpawnerLifecycleState::Holding);
        TestEqual(TEXT("TC1 (2/9): Active→Holding"),
            Sub->TestOnly_GetLifecycleState(),
            EWaveSpawnerLifecycleState::Holding);
        TestTrue(TEXT("TC1: IsTickable() == true in Holding"), Sub->IsTickable());

        // (3) Holding → Active  (drain-window cleared; pool stays at Mid)
        Sub->TransitionTo(EWaveSpawnerLifecycleState::Active);
        TestEqual(TEXT("TC1 (3/9): Holding→Active"),
            Sub->TestOnly_GetLifecycleState(),
            EWaveSpawnerLifecycleState::Active);
        TestTrue(TEXT("TC1: IsTickable() == true in Active"), Sub->IsTickable());

        // (4) Active → Holding  (second time — Mid→Peak pool swap fires)
        Sub->TransitionTo(EWaveSpawnerLifecycleState::Holding);
        TestEqual(TEXT("TC1 (4/9): Active→Holding (2nd — Mid→Peak swap)"),
            Sub->TestOnly_GetLifecycleState(),
            EWaveSpawnerLifecycleState::Holding);
        TestTrue(TEXT("TC1: IsTickable() == true in Holding"), Sub->IsTickable());

        // (5) Holding → Idle  (long-pause path: drain window clears naturally)
        Sub->TransitionTo(EWaveSpawnerLifecycleState::Idle);
        TestEqual(TEXT("TC1 (5/9): Holding→Idle"),
            Sub->TestOnly_GetLifecycleState(),
            EWaveSpawnerLifecycleState::Idle);
        TestFalse(TEXT("TC1: IsTickable() == false in Idle (TR-WS-011)"),
            Sub->IsTickable());

        // (6) Idle → Active  (resume from long pause — no flush needed)
        Sub->TransitionTo(EWaveSpawnerLifecycleState::Active);
        TestEqual(TEXT("TC1 (6/9): Idle→Active"),
            Sub->TestOnly_GetLifecycleState(),
            EWaveSpawnerLifecycleState::Active);
        TestTrue(TEXT("TC1: IsTickable() == true in Active"), Sub->IsTickable());

        // (7) Active → Flushing  (RSM terminal state: DEAD|COMPLETE|ABORTED)
        Sub->TransitionTo(EWaveSpawnerLifecycleState::Flushing);
        TestEqual(TEXT("TC1 (7/9): Active→Flushing"),
            Sub->TestOnly_GetLifecycleState(),
            EWaveSpawnerLifecycleState::Flushing);
        TestTrue(TEXT("TC1: IsTickable() == true in Flushing"), Sub->IsTickable());

        // (8) Flushing → Cold  (run-termination: all live drained → reset)
        Sub->TransitionTo(EWaveSpawnerLifecycleState::Cold);
        TestEqual(TEXT("TC1 (8/9): Flushing→Cold"),
            Sub->TestOnly_GetLifecycleState(),
            EWaveSpawnerLifecycleState::Cold);
        TestFalse(TEXT("TC1: IsTickable() == false in Cold after reset"),
            Sub->IsTickable());

        // Cover remaining two pairs: Holding→Flushing and Flushing→Active.
        // Cold→Active→Holding→Flushing→Active sequence:
        Sub->TransitionTo(EWaveSpawnerLifecycleState::Active);   // Cold→Active (phase reset)
        Sub->TransitionTo(EWaveSpawnerLifecycleState::Holding);  // Active→Holding (Opener→Mid)

        // (9c) Holding → Flushing  (pause with stale scheduled_slots — Rule 13)
        Sub->TransitionTo(EWaveSpawnerLifecycleState::Flushing);
        TestEqual(TEXT("TC1 (9c/9): Holding→Flushing"),
            Sub->TestOnly_GetLifecycleState(),
            EWaveSpawnerLifecycleState::Flushing);
        TestTrue(TEXT("TC1: IsTickable() == true in Flushing"), Sub->IsTickable());

        // (9d) Flushing → Active  (pause-flush complete: stale slots drained)
        Sub->TransitionTo(EWaveSpawnerLifecycleState::Active);
        TestEqual(TEXT("TC1 (9d/9): Flushing→Active"),
            Sub->TestOnly_GetLifecycleState(),
            EWaveSpawnerLifecycleState::Active);
        TestTrue(TEXT("TC1: IsTickable() == true in Active"), Sub->IsTickable());

        return true;
    }

    // -------------------------------------------------------------------------
    // TC2 — AC-WS-15a (TR-WS-011):
    // Forbidden transitions are rejected: IsValidTransition() returns false.
    //
    // Tests 4 forbidden pairs (ADR-0011 D3):
    //   Cold→Holding   must go Cold→Active first
    //   Cold→Flushing  nothing to flush in Cold
    //   Active→Cold    must transition through Flushing (Rule 14)
    //   Flushing→Idle  NOT valid per ADR-0011 D3 — run-termination is Flushing→Cold
    //                  (Story AC-WS-15a listed this as valid; ADR supersedes)
    //
    // DESIGN: Uses TestOnly_IsValidTransition() (predicate seam) rather than
    // TransitionTo() directly. Rationale: TransitionTo() calls check(false) in
    // non-Shipping builds — check() aborts the process and is not catchable via
    // AddExpectedError(). Testing the predicate directly gives equivalent coverage:
    // IsValidTransition() IS the gate that TransitionTo() enforces. See file header.
    //
    // Given:  each forbidden (From, To) pair.
    // When:   TestOnly_IsValidTransition(From, To) is called.
    // Then:   returns false (transition would be rejected by TransitionTo()).
    // -------------------------------------------------------------------------
    if (Parameters == TEXT("Lifecycle.ForbiddenTransitions"))
    {
        UWaveSpawnerSubsystem* Sub =
            NewObject<UWaveSpawnerSubsystem>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC2: NewObject<UWaveSpawnerSubsystem> succeeded"), Sub))
        {
            return false;
        }

        // Forbidden pair 1: Cold → Holding
        TestFalse(
            TEXT("TC2a: Cold→Holding is forbidden (must go Cold→Active first — ADR-0011 D3)"),
            Sub->TestOnly_IsValidTransition(
                EWaveSpawnerLifecycleState::Cold,
                EWaveSpawnerLifecycleState::Holding));

        // Forbidden pair 2: Cold → Flushing
        TestFalse(
            TEXT("TC2b: Cold→Flushing is forbidden (nothing to flush in Cold — ADR-0011 D3)"),
            Sub->TestOnly_IsValidTransition(
                EWaveSpawnerLifecycleState::Cold,
                EWaveSpawnerLifecycleState::Flushing));

        // Forbidden pair 3: Active → Cold
        TestFalse(
            TEXT("TC2c: Active→Cold is forbidden (must transition through Flushing — ADR-0011 D3)"),
            Sub->TestOnly_IsValidTransition(
                EWaveSpawnerLifecycleState::Active,
                EWaveSpawnerLifecycleState::Cold));

        // Forbidden pair 4: Flushing → Idle
        // ADR-0011 D3 supersedes Story AC-WS-15a: run-termination is Flushing→Cold,
        // not Flushing→Idle. Holding→Idle is the long-pause path. Flushing→Idle is forbidden.
        TestFalse(
            TEXT("TC2d: Flushing→Idle is forbidden (run-termination is Flushing→Cold — ADR-0011 D3)"),
            Sub->TestOnly_IsValidTransition(
                EWaveSpawnerLifecycleState::Flushing,
                EWaveSpawnerLifecycleState::Idle));

        return true;
    }

    // -------------------------------------------------------------------------
    // TC3 — AC-WS-15b (TR-WS-021):
    // Pool swap: Active→Holding advances ActiveDrawPool from &OpenerPool to &MidPool.
    // GetActivePhase() transitions from Opener to Mid.
    // Pattern data is NEVER copied — only the raw pointer changes.
    //
    // Given:  subsystem in Cold state.
    // When:   TransitionTo(Active) then TransitionTo(Holding).
    // Then:   GetActivePhase() == Mid;
    //         GetActiveDrawPool() is non-null and differs from the pre-swap pointer.
    // -------------------------------------------------------------------------
    if (Parameters == TEXT("Lifecycle.PoolSwapOpenerToMid"))
    {
        UWaveSpawnerSubsystem* Sub =
            NewObject<UWaveSpawnerSubsystem>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC3: NewObject succeeded"), Sub)) { return false; }

        // Cold → Active: pool initialised to OpenerPool
        Sub->TransitionTo(EWaveSpawnerLifecycleState::Active);
        TestEqual(TEXT("TC3 (pre-cond): Phase == Opener after Cold→Active (TR-WS-020)"),
            Sub->GetActivePhase(), ERunPhase::Opener);
        const FPatternPool* const OpenerPoolPtr = Sub->GetActiveDrawPool();
        TestTrue(TEXT("TC3 (pre-cond): GetActiveDrawPool() != nullptr after Cold→Active"),
            OpenerPoolPtr != nullptr);

        // Active → Holding: atomic swap → MidPool
        Sub->TransitionTo(EWaveSpawnerLifecycleState::Holding);

        TestEqual(TEXT("TC3: Phase == Mid after first Holding entry (AC-WS-15b, TR-WS-021)"),
            Sub->GetActivePhase(), ERunPhase::Mid);

        const FPatternPool* const AfterSwapPtr = Sub->GetActiveDrawPool();
        TestTrue(TEXT("TC3: GetActiveDrawPool() != nullptr after pool swap"),
            AfterSwapPtr != nullptr);
        // Pointer must have changed — OpenerPool→MidPool reassignment (pointer only, no copy).
        TestTrue(
            TEXT("TC3: Pool pointer changed Opener→Mid (pointer-only atomic swap — TR-WS-021)"),
            AfterSwapPtr != OpenerPoolPtr);

        return true;
    }

    // -------------------------------------------------------------------------
    // TC4 — AC-WS-15b (TR-WS-021):
    // Pool swap: second Active→Holding advances ActiveDrawPool from &MidPool to &PeakPool.
    // GetActivePhase() transitions from Mid to Peak.
    //
    // Given:  subsystem driven Cold→Active→Holding→Active (drain cleared) → Holding.
    // When:   Second TransitionTo(Holding) fires.
    // Then:   GetActivePhase() == Peak;
    //         GetActiveDrawPool() is a third distinct pointer (not MidPool's address).
    //         Pointer on Holding→Active (drain cleared) is UNCHANGED (no swap on Active entry).
    // -------------------------------------------------------------------------
    if (Parameters == TEXT("Lifecycle.PoolSwapMidToPeak"))
    {
        UWaveSpawnerSubsystem* Sub =
            NewObject<UWaveSpawnerSubsystem>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC4: NewObject succeeded"), Sub)) { return false; }

        // First swap: Opener → Mid
        Sub->TransitionTo(EWaveSpawnerLifecycleState::Active);   // Cold→Active (Opener)
        Sub->TransitionTo(EWaveSpawnerLifecycleState::Holding);  // Opener→Mid swap
        const FPatternPool* const MidPoolPtr = Sub->GetActiveDrawPool();
        TestEqual(TEXT("TC4 (pre-cond): Phase == Mid after first Holding entry"),
            Sub->GetActivePhase(), ERunPhase::Mid);

        // Drain-window cleared: Holding→Active (NO pool swap on Active entry)
        Sub->TransitionTo(EWaveSpawnerLifecycleState::Active);
        TestEqual(TEXT("TC4 (pre-cond): Phase still Mid after Holding→Active (no swap on Active)"),
            Sub->GetActivePhase(), ERunPhase::Mid);
        TestTrue(TEXT("TC4 (pre-cond): Pool pointer unchanged on Holding→Active"),
            Sub->GetActiveDrawPool() == MidPoolPtr);

        // Second swap: Mid → Peak
        Sub->TransitionTo(EWaveSpawnerLifecycleState::Holding);

        TestEqual(TEXT("TC4: Phase == Peak after second Holding entry (AC-WS-15b, TR-WS-021)"),
            Sub->GetActivePhase(), ERunPhase::Peak);

        const FPatternPool* const PeakPoolPtr = Sub->GetActiveDrawPool();
        TestTrue(TEXT("TC4: GetActiveDrawPool() != nullptr after Mid→Peak swap"),
            PeakPoolPtr != nullptr);
        TestTrue(
            TEXT("TC4: Pool pointer changed Mid→Peak (pointer-only atomic swap — TR-WS-021)"),
            PeakPoolPtr != MidPoolPtr);

        return true;
    }

    // -------------------------------------------------------------------------
    // TC5 — AC-WS-15b (TR-WS-022):
    // Pool swap does NOT mutate in-flight AWave pool entry fields.
    //
    // An injected AWave with WaveId=42 and bInUse=true must have both fields
    // unchanged after Active→Holding fires the atomic pool-pointer swap.
    // Verifies that OnLifecycleTransition(Holding) only reassigns ActiveDrawPool —
    // it NEVER iterates or modifies pool slot contents.
    //
    // Given:  subsystem in Active state with an injected in-flight AWave.
    // When:   TransitionTo(Holding) fires the pool-pointer swap.
    // Then:   Wave->WaveId == 42; Wave->bInUse == true (no FWaveInFlightState mutation).
    // -------------------------------------------------------------------------
    if (Parameters == TEXT("Lifecycle.InFlightImmutability"))
    {
        UWaveSpawnerSubsystem* Sub =
            NewObject<UWaveSpawnerSubsystem>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC5: NewObject<UWaveSpawnerSubsystem> succeeded"), Sub))
        {
            return false;
        }

        // Inject an in-flight wave via test seam — headless, no SpawnActor needed.
        // Precedent: WaveSpawnerPoolTest TC2 uses the same NewObject<AWave> + inject pattern.
        AWave* Wave = NewObject<AWave>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC5: NewObject<AWave> succeeded"), Wave))
        {
            return false;
        }
        Wave->WaveId = 42;
        Wave->bInUse = true;
        Sub->TestOnly_InjectPoolEntry(Wave);

        // Cold→Active: initialise draw pool (required so Active→Holding is a valid transition)
        Sub->TransitionTo(EWaveSpawnerLifecycleState::Active);
        TestEqual(TEXT("TC5 (pre-cond): Phase == Opener after Cold→Active"),
            Sub->GetActivePhase(), ERunPhase::Opener);

        // Active→Holding: fire the atomic pool-pointer swap
        Sub->TransitionTo(EWaveSpawnerLifecycleState::Holding);

        // Assert pool-entry fields are unchanged.
        // The pool-pointer swap must NOT iterate or mutate any pool slot (TR-WS-022).
        TestEqual(
            TEXT("TC5: Wave->WaveId == 42 after pool swap — FWaveInFlightState immutable (TR-WS-022)"),
            Wave->WaveId, 42);
        TestTrue(
            TEXT("TC5: Wave->bInUse == true after pool swap — slot state not affected (TR-WS-022)"),
            Wave->bInUse);

        return true;
    }

    // -------------------------------------------------------------------------
    // TC6 — AC-WS-15c (TR-WS-023):
    // Rule 9 drain window: bDrainWindowActive set true on Active→Holding;
    // cleared to false on Holding→Active (drain window clears, admissions resume).
    //
    // Given:  subsystem driven Cold→Active.
    // When:   TransitionTo(Holding); then TransitionTo(Active) (simulating drain cleared).
    // Then:   IsDrainWindowActive() == true after Holding entry;
    //         IsDrainWindowActive() == false after Holding→Active.
    // -------------------------------------------------------------------------
    if (Parameters == TEXT("Lifecycle.DrainWindowClearance"))
    {
        UWaveSpawnerSubsystem* Sub =
            NewObject<UWaveSpawnerSubsystem>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC6: NewObject succeeded"), Sub)) { return false; }

        // Cold → Active: drain window must start false
        Sub->TransitionTo(EWaveSpawnerLifecycleState::Active);
        TestFalse(
            TEXT("TC6 (pre-cond): IsDrainWindowActive() == false after Cold→Active"),
            Sub->IsDrainWindowActive());

        // Active → Holding: Rule 9 drain window opens
        Sub->TransitionTo(EWaveSpawnerLifecycleState::Holding);
        TestTrue(
            TEXT("TC6: IsDrainWindowActive() == true after Active→Holding (TR-WS-023)"),
            Sub->IsDrainWindowActive());

        // Holding → Active: drain window clears (CountInFlightWaves()==0 path, Story 007)
        // Here we call directly to verify the flag is cleared on Active entry.
        Sub->TransitionTo(EWaveSpawnerLifecycleState::Active);
        TestFalse(
            TEXT("TC6: IsDrainWindowActive() == false after Holding→Active (drain cleared — TR-WS-023)"),
            Sub->IsDrainWindowActive());

        return true;
    }

    // -------------------------------------------------------------------------
    // TC7 — AC-WS-15c (TR-WS-022/TR-WS-023):
    // Flushing→Cold resets all per-run phase state for the next replay.
    //
    // After Flushing→Cold:
    //   GetActiveDrawPool() == nullptr   (cleared; next Cold→Active re-initialises)
    //   GetActivePhase()    == Opener    (reset for next run start)
    //   IsDrainWindowActive() == false
    //
    // DEVIATION NOTE: Story AC-WS-15c described "Idle→Cold resets bDrainWindowActive
    // and ActivePhase". ADR-0011 D3 supersedes: run-termination is Flushing→Cold
    // (no intermediate Idle step). The reset fires at Flushing→Cold, not at a
    // hypothetical "Idle→Cold" transition. Story file will be reconciled by the story owner.
    //
    // Given:  subsystem driven Cold→Active→Flushing (run-termination path).
    // When:   TransitionTo(Cold) fires (all live waves drained).
    // Then:   GetActiveDrawPool() == nullptr; GetActivePhase() == Opener;
    //         IsDrainWindowActive() == false.
    // -------------------------------------------------------------------------
    if (Parameters == TEXT("Lifecycle.ColdResetOnTermination"))
    {
        UWaveSpawnerSubsystem* Sub =
            NewObject<UWaveSpawnerSubsystem>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC7: NewObject succeeded"), Sub)) { return false; }

        // Drive to Active (sets up draw pool) then to Flushing (run-termination entry)
        Sub->TransitionTo(EWaveSpawnerLifecycleState::Active);
        TestTrue(TEXT("TC7 (pre-cond): ActiveDrawPool != nullptr after Cold→Active"),
            Sub->GetActiveDrawPool() != nullptr);
        TestEqual(TEXT("TC7 (pre-cond): Phase == Opener after Cold→Active"),
            Sub->GetActivePhase(), ERunPhase::Opener);

        Sub->TransitionTo(EWaveSpawnerLifecycleState::Flushing);

        // Flushing → Cold: run-termination complete — all per-run state must reset
        Sub->TransitionTo(EWaveSpawnerLifecycleState::Cold);

        TestTrue(
            TEXT("TC7: ActiveDrawPool == nullptr after Flushing→Cold (reset for next run — AC-WS-15c)"),
            Sub->GetActiveDrawPool() == nullptr);
        TestEqual(
            TEXT("TC7: ActivePhase == Opener after Flushing→Cold (reset for next run — AC-WS-15c)"),
            Sub->GetActivePhase(), ERunPhase::Opener);
        TestFalse(
            TEXT("TC7: IsDrainWindowActive() == false after Flushing→Cold (AC-WS-15c)"),
            Sub->IsDrainWindowActive());

        return true;
    }

    // Unknown parameter — fail explicitly rather than silently returning true.
    // Matches error pattern from WaveSpawnerPoolTest.cpp.
    AddError(FString::Printf(
        TEXT("FWaveSpawnerLifecycleTest::RunTest — unknown Parameters value: '%s'"),
        *Parameters));
    return false;
}

#endif // WITH_DEV_AUTOMATION_TESTS

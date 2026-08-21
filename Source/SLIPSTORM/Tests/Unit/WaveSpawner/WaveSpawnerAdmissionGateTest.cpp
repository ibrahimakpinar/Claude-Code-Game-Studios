// Copyright (c) Game Studio. All rights reserved.
// SLIPSTORM — 60-second voxel endless runner, mobile, Unreal Engine 5.7
//
// WaveSpawnerAdmissionGateTest.cpp — Story 003 unit tests for the Rule 1 admission
// gate, F-3b cadence gate, and Primer Bypass in UWaveSpawnerSubsystem::OnDPCFrameReady.
//
// Story: production/epics/wave-spawner/story-003-admission-gate-and-primer-bypass.md
// TRs:   TR-WS-012 (Rule 1 gate — DPC is_active, lifecycle, resume grace)
//         TR-WS-015 (F-3b cadence gate: (now − last_spawn_time) >= wave_spawn_interval_s)
//         TR-WS-016 (Primer bypass: first draw after Cold→Active skips cadence gate)
//
// Test category: SLIPSTORM.WaveSpawner.AdmissionGate
// Runner:        UE Automation Framework
//                Flags: ApplicationContextMask | ProductFilter
//
// All 8 test commands are fully headless (no UWorld needed).
// NewObject<UWaveSpawnerSubsystem>(GetTransientPackage()) + TestOnly_* seams throughout.
//
// AC DEVIATION NOTE (AC-WS-10):
//   AC-WS-10 names FDPCTestStub / FRSMTestStub injected at Initialize() as the
//   verification vehicle. FRSMTestStub injection is Story 007 scope (out of scope for
//   Story 003). These tests use TestOnly_* seams instead, following the same rationale
//   documented for TestOnly_IsValidTransition in WaveSpawnerSubsystem.h: the seams
//   exercise the exact gate predicate that production code enforces, giving equivalent
//   AC coverage without requiring the Story 007 RSM wiring.
//
// AC DEVIATION NOTE (AC-WS-11b):
//   AC-WS-11b specifies WaveSpawnIntervalS "loaded from project config at Initialize()".
//   Production reads FrameState.WaveSpawnIntervalS from the DPC snapshot each frame.
//   TC8 injects a non-default FrameState.WaveSpawnIntervalS (12.f) and asserts the gate
//   uses that injected value — proving no hardcoded 2.5f literal is consulted.
//   Story owner to reconcile AC-WS-11b text; grep criterion ("WaveSpawnIntervalS" at
//   the comparison point, no bare 2.5f literal) is met by the production implementation.
//
// Test isolation: each TC constructs a fresh UWaveSpawnerSubsystem via NewObject<> and
// sets up its own preconditions. No state carries across RunTest dispatches
// (IMPLEMENT_COMPLEX_AUTOMATION_TEST issues a separate RunTest call per command).
//
// Guard: WITH_DEV_AUTOMATION_TESTS only.

#if WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#include "WaveSpawner/WaveSpawnerSubsystem.h"
#include "WaveSpawner/WaveSpawnerTypes.h"
#include "DPC/DPCSubsystem.h"

// ============================================================================
// IMPLEMENT_COMPLEX_AUTOMATION_TEST
// 8 test commands covering Story 003 acceptance criteria.
// Pattern matches WaveSpawnerPoolTest.cpp (Story 001) and
// WaveSpawnerLifecycleTest.cpp (Story 002).
// ============================================================================

IMPLEMENT_COMPLEX_AUTOMATION_TEST(
    FWaveSpawnerAdmissionGateTest,
    "SLIPSTORM.WaveSpawner.AdmissionGate",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

void FWaveSpawnerAdmissionGateTest::GetTests(
    TArray<FString>& OutBeautifiedNames,
    TArray<FString>& OutTestCommands) const
{
    OutBeautifiedNames.Add(
        TEXT("Rule 1 gate: DPC is_active=false vetoes admission (AC-WS-10, TR-WS-012)"));
    OutTestCommands.Add(TEXT("Gate.DPCInactiveVeto"));

    OutBeautifiedNames.Add(
        TEXT("Rule 1 gate: Holding lifecycle state vetoes admission (AC-WS-10, TR-WS-012)"));
    OutTestCommands.Add(TEXT("Gate.LifecycleNotActiveVeto"));

    OutBeautifiedNames.Add(
        TEXT("Rule 1 gate: resume grace window vetoes and then clears lazily (AC-WS-10, TR-WS-012)"));
    OutTestCommands.Add(TEXT("Gate.ResumeGraceVeto"));

    OutBeautifiedNames.Add(
        TEXT("Cadence gate: ε before interval — admission deferred (AC-WS-11, TR-WS-015)"));
    OutTestCommands.Add(TEXT("Gate.CadenceNotElapsed"));

    OutBeautifiedNames.Add(
        TEXT("Cadence gate: exactly at interval — admission passes (AC-WS-11, TR-WS-015)"));
    OutTestCommands.Add(TEXT("Gate.CadenceElapsed"));

    OutBeautifiedNames.Add(
        TEXT("Primer bypass: first draw after Cold→Active skips cadence gate (AC-WS-11, TR-WS-016)"));
    OutTestCommands.Add(TEXT("Gate.PrimerBypass"));

    OutBeautifiedNames.Add(
        TEXT("After primer: second tick applies cadence gate normally (AC-WS-11, TR-WS-016)"));
    OutTestCommands.Add(TEXT("Gate.PostPrimerCadenceNormal"));

    OutBeautifiedNames.Add(
        TEXT("Cadence gate uses FrameState.WaveSpawnIntervalS, not a hardcoded literal (AC-WS-11b)"));
    OutTestCommands.Add(TEXT("Gate.IntervalFromDPCSnapshot"));
}

bool FWaveSpawnerAdmissionGateTest::RunTest(const FString& Parameters)
{
    // -------------------------------------------------------------------------
    // TC1 — AC-WS-10 (TR-WS-012): DPC is_active=false vetoes admission.
    //
    // Given:  subsystem in Active state (set via seam); bPrimerPending=true (so
    //         any post-veto assertion can observe it is unchanged); time=10.f.
    //         FrameState.bIsActive = false.
    // When:   TestOnly_TriggerDPCFrameReady(FrameState).
    // Then:   LastSpawnTimeS == 0.f (unchanged — veto fired at gate 1a).
    //         bPrimerPending == true (unchanged — execution never reached primer block).
    // -------------------------------------------------------------------------
    if (Parameters == TEXT("Gate.DPCInactiveVeto"))
    {
        UWaveSpawnerSubsystem* Sub =
            NewObject<UWaveSpawnerSubsystem>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC1: NewObject succeeded"), Sub))
        {
            return false;
        }

        Sub->TestOnly_SetLifecycleState(EWaveSpawnerLifecycleState::Active);
        Sub->TestOnly_SetPrimerPending(true);   // verify unchanged after veto
        Sub->TestOnly_SetCurrentTimeOverride(10.f);

        FDPCFrameState FrameState;
        FrameState.bIsActive          = false;
        FrameState.WaveSpawnIntervalS = 2.5f;
        Sub->TestOnly_TriggerDPCFrameReady(FrameState);

        TestEqual(
            TEXT("TC1: LastSpawnTimeS == 0.f — DPC inactive veto fired (TR-WS-012)"),
            Sub->TestOnly_GetLastSpawnTimeS(),
            0.f);
        TestTrue(
            TEXT("TC1: bPrimerPending unchanged — execution did not reach primer block"),
            Sub->TestOnly_IsPrimerPending());

        return true;
    }

    // -------------------------------------------------------------------------
    // TC2 — AC-WS-10 (TR-WS-012): Holding lifecycle state vetoes admission.
    //
    // Given:  subsystem with LifecycleState = Holding (set via seam); time=10.f.
    //         FrameState.bIsActive = true (passes gate 1a, blocked by gate 1b).
    // When:   TestOnly_TriggerDPCFrameReady(FrameState).
    // Then:   LastSpawnTimeS == 0.f (unchanged — lifecycle guard fired at gate 1b).
    // -------------------------------------------------------------------------
    if (Parameters == TEXT("Gate.LifecycleNotActiveVeto"))
    {
        UWaveSpawnerSubsystem* Sub =
            NewObject<UWaveSpawnerSubsystem>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC2: NewObject succeeded"), Sub))
        {
            return false;
        }

        Sub->TestOnly_SetLifecycleState(EWaveSpawnerLifecycleState::Holding);
        Sub->TestOnly_SetCurrentTimeOverride(10.f);

        FDPCFrameState FrameState;
        FrameState.bIsActive          = true;
        FrameState.WaveSpawnIntervalS = 2.5f;
        Sub->TestOnly_TriggerDPCFrameReady(FrameState);

        TestEqual(
            TEXT("TC2: LastSpawnTimeS == 0.f — Holding lifecycle veto fired (TR-WS-012)"),
            Sub->TestOnly_GetLastSpawnTimeS(),
            0.f);

        return true;
    }

    // -------------------------------------------------------------------------
    // TC3 — AC-WS-10 (TR-WS-012): Resume grace window vetoes; clears lazily.
    //
    // Part A — within grace window:
    //   Given:  Active state; bInResumeGrace=true, ResumeGraceEndTimeS=100.f;
    //           time=50.f (within window); FrameState.bIsActive=true.
    //   When:   TestOnly_TriggerDPCFrameReady.
    //   Then:   LastSpawnTimeS == 0.f (veto fired at gate 1c — 50.f < 100.f).
    //
    // Part B — grace window just expired (lazy clearance):
    //   Given:  same subsystem state; bPrimerPending=true (deterministic path);
    //           time=101.f (> ResumeGraceEndTimeS=100.f).
    //   When:   TestOnly_TriggerDPCFrameReady.
    //   Then:   LastSpawnTimeS == 101.f (grace cleared lazily; primer admitted at T=101).
    // -------------------------------------------------------------------------
    if (Parameters == TEXT("Gate.ResumeGraceVeto"))
    {
        UWaveSpawnerSubsystem* Sub =
            NewObject<UWaveSpawnerSubsystem>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC3: NewObject succeeded"), Sub))
        {
            return false;
        }

        Sub->TestOnly_SetLifecycleState(EWaveSpawnerLifecycleState::Active);
        Sub->TestOnly_SetResumeGrace(true, 100.f);
        Sub->TestOnly_SetCurrentTimeOverride(50.f);

        FDPCFrameState FrameState;
        FrameState.bIsActive          = true;
        FrameState.WaveSpawnIntervalS = 2.5f;

        // Part A: within grace window — veto
        Sub->TestOnly_TriggerDPCFrameReady(FrameState);
        TestEqual(
            TEXT("TC3 Part A: LastSpawnTimeS == 0.f — grace window veto fired (T=50 < EndT=100)"),
            Sub->TestOnly_GetLastSpawnTimeS(),
            0.f);

        // Part B: time past grace end — grace clears lazily; primer fires
        Sub->TestOnly_SetCurrentTimeOverride(101.f);
        Sub->TestOnly_SetPrimerPending(true);  // force primer path for exact assertion
        Sub->TestOnly_TriggerDPCFrameReady(FrameState);
        TestEqual(
            TEXT("TC3 Part B: LastSpawnTimeS == 101.f — grace cleared lazily; primer admitted at T=101"),
            Sub->TestOnly_GetLastSpawnTimeS(),
            101.f);

        return true;
    }

    // -------------------------------------------------------------------------
    // TC4 — AC-WS-11 (TR-WS-015): Cadence gate — ε before interval, admission deferred.
    //
    // Given:  Active state; primer cleared; LastSpawnTimeS=0.f (default);
    //         time=2.499f; FrameState.WaveSpawnIntervalS=2.5f.
    //         Elapsed = 2.499f < 2.5f → gate must veto.
    // When:   TestOnly_TriggerDPCFrameReady.
    // Then:   LastSpawnTimeS == 0.f (unchanged — cadence gate blocked admission).
    // -------------------------------------------------------------------------
    if (Parameters == TEXT("Gate.CadenceNotElapsed"))
    {
        UWaveSpawnerSubsystem* Sub =
            NewObject<UWaveSpawnerSubsystem>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC4: NewObject succeeded"), Sub))
        {
            return false;
        }

        Sub->TestOnly_SetLifecycleState(EWaveSpawnerLifecycleState::Active);
        Sub->TestOnly_SetPrimerPending(false);
        Sub->TestOnly_SetCurrentTimeOverride(2.499f);

        FDPCFrameState FrameState;
        FrameState.bIsActive          = true;
        FrameState.WaveSpawnIntervalS = 2.5f;
        Sub->TestOnly_TriggerDPCFrameReady(FrameState);

        TestEqual(
            TEXT("TC4: LastSpawnTimeS == 0.f — cadence gate blocked (elapsed 2.499f < interval 2.5f, TR-WS-015)"),
            Sub->TestOnly_GetLastSpawnTimeS(),
            0.f);

        return true;
    }

    // -------------------------------------------------------------------------
    // TC5 — AC-WS-11 (TR-WS-015): Cadence gate — exactly at interval, admission passes.
    //
    // Given:  Active state; primer cleared; LastSpawnTimeS=0.f (default);
    //         time=2.5f; FrameState.WaveSpawnIntervalS=2.5f.
    //         Elapsed = 2.5f >= 2.5f → gate must pass.
    // When:   TestOnly_TriggerDPCFrameReady.
    // Then:   LastSpawnTimeS == 2.5f (updated — cadence gate passed).
    // -------------------------------------------------------------------------
    if (Parameters == TEXT("Gate.CadenceElapsed"))
    {
        UWaveSpawnerSubsystem* Sub =
            NewObject<UWaveSpawnerSubsystem>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC5: NewObject succeeded"), Sub))
        {
            return false;
        }

        Sub->TestOnly_SetLifecycleState(EWaveSpawnerLifecycleState::Active);
        Sub->TestOnly_SetPrimerPending(false);
        Sub->TestOnly_SetCurrentTimeOverride(2.5f);

        FDPCFrameState FrameState;
        FrameState.bIsActive          = true;
        FrameState.WaveSpawnIntervalS = 2.5f;
        Sub->TestOnly_TriggerDPCFrameReady(FrameState);

        TestEqual(
            TEXT("TC5: LastSpawnTimeS == 2.5f — cadence gate passed (elapsed 2.5f >= interval 2.5f, TR-WS-015)"),
            Sub->TestOnly_GetLastSpawnTimeS(),
            2.5f);

        return true;
    }

    // -------------------------------------------------------------------------
    // TC6 — AC-WS-11 (TR-WS-016): Primer bypass — first draw after Cold→Active
    //                              skips cadence gate.
    //
    // Given:  fresh subsystem; TransitionTo(Active) from Cold fires OnLifecycleTransition
    //         → bPrimerPending=true; time=0.1f (elapsed=0.1f << interval=2.5f — would
    //         fail cadence if the gate ran).
    //         FrameState.bIsActive=true, WaveSpawnIntervalS=2.5f.
    // When:   TestOnly_TriggerDPCFrameReady.
    // Then:   bPrimerPending == false (primer consumed).
    //         LastSpawnTimeS == 0.1f (set to Now post-primer; NOT 0.f which would
    //         mean the gate ran and blocked, nor some other value).
    // -------------------------------------------------------------------------
    if (Parameters == TEXT("Gate.PrimerBypass"))
    {
        UWaveSpawnerSubsystem* Sub =
            NewObject<UWaveSpawnerSubsystem>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC6: NewObject succeeded"), Sub))
        {
            return false;
        }

        // Drive Cold→Active via TransitionTo to exercise the real OnLifecycleTransition
        // hook that sets bPrimerPending = true (TR-WS-016).
        Sub->TransitionTo(EWaveSpawnerLifecycleState::Active);
        if (!TestTrue(TEXT("TC6: bPrimerPending == true after Cold→Active (TR-WS-016)"),
            Sub->TestOnly_IsPrimerPending()))
        {
            return false;
        }

        Sub->TestOnly_SetCurrentTimeOverride(0.1f);

        FDPCFrameState FrameState;
        FrameState.bIsActive          = true;
        FrameState.WaveSpawnIntervalS = 2.5f;
        Sub->TestOnly_TriggerDPCFrameReady(FrameState);

        TestFalse(
            TEXT("TC6: bPrimerPending == false — primer was consumed (TR-WS-016)"),
            Sub->TestOnly_IsPrimerPending());
        TestEqual(
            TEXT("TC6: LastSpawnTimeS == 0.1f — set to Now post-primer; cadence gate was skipped "
                 "(LastSpawnTimeS == 0.f would mean gate ran and blocked — TR-WS-016)"),
            Sub->TestOnly_GetLastSpawnTimeS(),
            0.1f);

        return true;
    }

    // -------------------------------------------------------------------------
    // TC7 — AC-WS-11 (TR-WS-016): After primer, second tick applies cadence gate normally.
    //
    // State rebuilt from scratch — no cross-TC dependence (each RunTest is independent).
    //
    // Given:  fresh subsystem → Cold→Active (primer fires on first trigger at T=0.1f).
    //         After primer: bPrimerPending=false, LastSpawnTimeS=0.1f.
    //         Second trigger at T=0.6f: elapsed = 0.6f − 0.1f = 0.5f < interval 2.5f.
    // When:   TestOnly_TriggerDPCFrameReady (second call).
    // Then:   LastSpawnTimeS == 0.1f (unchanged — cadence gate blocked second admission).
    // -------------------------------------------------------------------------
    if (Parameters == TEXT("Gate.PostPrimerCadenceNormal"))
    {
        UWaveSpawnerSubsystem* Sub =
            NewObject<UWaveSpawnerSubsystem>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC7: NewObject succeeded"), Sub))
        {
            return false;
        }

        // Drive Cold→Active to arm the primer.
        Sub->TransitionTo(EWaveSpawnerLifecycleState::Active);

        FDPCFrameState FrameState;
        FrameState.bIsActive          = true;
        FrameState.WaveSpawnIntervalS = 2.5f;

        // Trigger #1 at T=0.1f — primer fires; LastSpawnTimeS → 0.1f.
        Sub->TestOnly_SetCurrentTimeOverride(0.1f);
        Sub->TestOnly_TriggerDPCFrameReady(FrameState);
        if (!TestEqual(
            TEXT("TC7 precondition: LastSpawnTimeS == 0.1f after primer fire"),
            Sub->TestOnly_GetLastSpawnTimeS(),
            0.1f))
        {
            return false;
        }
        if (!TestFalse(
            TEXT("TC7 precondition: bPrimerPending == false after primer consumed"),
            Sub->TestOnly_IsPrimerPending()))
        {
            return false;
        }

        // Trigger #2 at T=0.6f — elapsed = 0.5f < 2.5f → cadence gate blocks.
        Sub->TestOnly_SetCurrentTimeOverride(0.6f);
        Sub->TestOnly_TriggerDPCFrameReady(FrameState);

        TestEqual(
            TEXT("TC7: LastSpawnTimeS == 0.1f — cadence gate blocked second admission "
                 "(elapsed 0.5f < interval 2.5f; TR-WS-015 applies normally post-primer)"),
            Sub->TestOnly_GetLastSpawnTimeS(),
            0.1f);

        return true;
    }

    // -------------------------------------------------------------------------
    // TC8 — AC-WS-11b: Cadence gate uses FrameState.WaveSpawnIntervalS (not hardcoded).
    //
    // Injects a non-default interval (12.f) and asserts the gate uses it, proving
    // no hardcoded 2.5f literal is consulted at the comparison site.
    //
    // Part A — interval=12.f, time=10.f: elapsed=10.f < 12.f → veto.
    // Part B — interval=12.f, time=12.f: elapsed=12.f >= 12.f → pass.
    // -------------------------------------------------------------------------
    if (Parameters == TEXT("Gate.IntervalFromDPCSnapshot"))
    {
        UWaveSpawnerSubsystem* Sub =
            NewObject<UWaveSpawnerSubsystem>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC8: NewObject succeeded"), Sub))
        {
            return false;
        }

        Sub->TestOnly_SetLifecycleState(EWaveSpawnerLifecycleState::Active);
        Sub->TestOnly_SetPrimerPending(false);

        // Part A: non-default interval 12.f; T=10.f — not elapsed.
        FDPCFrameState FrameState;
        FrameState.bIsActive          = true;
        FrameState.WaveSpawnIntervalS = 12.f;  // non-default value injected via DPC snapshot

        Sub->TestOnly_SetCurrentTimeOverride(10.f);
        Sub->TestOnly_TriggerDPCFrameReady(FrameState);
        TestEqual(
            TEXT("TC8 Part A: LastSpawnTimeS == 0.f — cadence gate used injected interval 12.f "
                 "(elapsed 10.f < 12.f; no hardcoded 2.5f literal — AC-WS-11b)"),
            Sub->TestOnly_GetLastSpawnTimeS(),
            0.f);

        // Part B: same interval 12.f; T=12.f — exactly elapsed → admission.
        Sub->TestOnly_SetCurrentTimeOverride(12.f);
        Sub->TestOnly_TriggerDPCFrameReady(FrameState);
        TestEqual(
            TEXT("TC8 Part B: LastSpawnTimeS == 12.f — cadence gate passed at T=12.f "
                 "using injected interval 12.f (elapsed 12.f >= 12.f — AC-WS-11b)"),
            Sub->TestOnly_GetLastSpawnTimeS(),
            12.f);

        return true;
    }

    // Unknown parameter — fail explicitly rather than silently returning true.
    AddError(FString::Printf(
        TEXT("FWaveSpawnerAdmissionGateTest::RunTest — unknown Parameters value: '%s'"),
        *Parameters));
    return false;
}

#endif // WITH_DEV_AUTOMATION_TESTS

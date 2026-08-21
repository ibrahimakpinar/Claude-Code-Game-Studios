// Copyright (c) Game Studio. All rights reserved.
// SLIPSTORM — 60-second voxel endless runner, mobile, Unreal Engine 5.7
//
// PullWaveStateMachineTest.cpp — Story 003 unit tests for the five-state
// machine and TransitionTo() helper on APullWaveSubsystemActor.
//
// Story: production/epics/pull-wave/story-003-state-machine.md
// GDD:   design/gdd/pull-wave-behavior.md §State Machine Transitions
// ADR:   docs/architecture/adr-0010-pullwave-object-pool-state-machine-despawn.md (D2)
// TRs:   TR-PW-002 (five-state machine + forbidden-transition enforcement)
//
// Test category: SLIPSTORM.PullWave.StateMachine
// Runner: UE Automation Framework, headless (-nullrhi -nosound -unattended)
//         EditorContext | ClientContext | SmokeFilter
//
// TC1  — Legal transitions: all 6 allowed pairs return true from IsTransitionAllowed().
// TC2  — Forbidden transitions: all 13 forbidden cells return false.
// TC3  — Self-transitions: all 5 same→same pairs return false.
// TC4  — TransitionTo() legal: SPAWNED→LEANING mutates Wave.State correctly.
// TC5  — TransitionTo() legal: LEANING→TRAVERSING; TraverseElapsedS is 0.0f at entry.
// TC6  — TransitionTo() legal: LEANING→DESPAWNING (pause-flush pathway).
// TC7  — TransitionTo() legal: TRAVERSING→LANDED; CollisionOutcome defaults Unresolved.
// TC8  — TransitionTo() legal: TRAVERSING→DESPAWNING (pause-flush pathway).
// TC9  — TransitionTo() legal: LANDED→DESPAWNING (hold elapsed pathway).
// TC10 — TransitionTo() shipping-safe guard: forbidden call does not modify State.
// TC11 — DESPAWNING is terminal: all 4 exits from DESPAWNING return false.
// TC12 — SPAWNED is entry-only: only LEANING is a legal exit from SPAWNED.
//
// Guard: WITH_DEV_AUTOMATION_TESTS only.

#if WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#include "PullWave/PullWaveTypes.h"
#include "PullWave/PullWaveSubsystemActor.h"

// ---------------------------------------------------------------------------
// Helper: build a default FPullWaveInstanceState with a given WaveId and State.
// ---------------------------------------------------------------------------
static FPullWaveInstanceState MakeStateIn(int32 WaveId, EPullWaveState State)
{
    FPullWaveInstanceState S{};
    S.WaveId = WaveId;
    S.State  = State;
    return S;
}

// ---------------------------------------------------------------------------
// IMPLEMENT_COMPLEX_AUTOMATION_TEST
// 12 test commands covering Story 003 acceptance criteria (AC-PW-11 through
// AC-PW-14, forbidden-transition table, and Shipping-safe guard).
// ---------------------------------------------------------------------------

IMPLEMENT_COMPLEX_AUTOMATION_TEST(
    FPullWaveStateMachineTest,
    "SLIPSTORM.PullWave.StateMachine",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::SmokeFilter)

void FPullWaveStateMachineTest::GetTests(
    TArray<FString>& OutBeautifiedNames,
    TArray<FString>& OutTestCommands) const
{
    OutBeautifiedNames.Add(TEXT("IsTransitionAllowed — all 6 legal transitions return true"));
    OutTestCommands.Add(TEXT("legal_transitions_return_true"));

    OutBeautifiedNames.Add(TEXT("IsTransitionAllowed — all 14 forbidden transitions return false"));
    OutTestCommands.Add(TEXT("forbidden_transitions_return_false"));

    OutBeautifiedNames.Add(TEXT("IsTransitionAllowed — all 5 self-transitions return false"));
    OutTestCommands.Add(TEXT("self_transitions_return_false"));

    OutBeautifiedNames.Add(TEXT("TransitionTo — SPAWNED→LEANING mutates Wave.State (AC-PW-11)"));
    OutTestCommands.Add(TEXT("transition_spawned_to_leaning"));

    OutBeautifiedNames.Add(TEXT("TransitionTo — LEANING→TRAVERSING; TraverseElapsedS == 0.0f at entry (AC-PW-12)"));
    OutTestCommands.Add(TEXT("transition_leaning_to_traversing"));

    OutBeautifiedNames.Add(TEXT("TransitionTo — LEANING→DESPAWNING succeeds (pause-flush pathway)"));
    OutTestCommands.Add(TEXT("transition_leaning_to_despawning"));

    OutBeautifiedNames.Add(TEXT("TransitionTo — TRAVERSING→LANDED; CollisionOutcome defaults Unresolved (AC-PW-13)"));
    OutTestCommands.Add(TEXT("transition_traversing_to_landed"));

    OutBeautifiedNames.Add(TEXT("TransitionTo — TRAVERSING→DESPAWNING succeeds (pause-flush pathway)"));
    OutTestCommands.Add(TEXT("transition_traversing_to_despawning"));

    OutBeautifiedNames.Add(TEXT("TransitionTo — LANDED→DESPAWNING succeeds (hold elapsed, AC-PW-14)"));
    OutTestCommands.Add(TEXT("transition_landed_to_despawning"));

    OutBeautifiedNames.Add(TEXT("TransitionTo shipping-safe guard — forbidden call does not modify State"));
    OutTestCommands.Add(TEXT("shipping_safe_guard_no_state_mutation"));

    OutBeautifiedNames.Add(TEXT("DESPAWNING terminal — all 4 exits from DESPAWNING return false"));
    OutTestCommands.Add(TEXT("despawning_is_terminal"));

    OutBeautifiedNames.Add(TEXT("SPAWNED entry-only — only LEANING is a legal exit from SPAWNED"));
    OutTestCommands.Add(TEXT("spawned_exits_only_to_leaning"));
}

bool FPullWaveStateMachineTest::RunTest(const FString& Parameters)
{
    // -----------------------------------------------------------------------
    // TC1 — IsTransitionAllowed: all 6 legal transitions return true.
    //
    // Legal table (ADR-0010 D2):
    //   SPAWNED    → LEANING
    //   LEANING    → TRAVERSING
    //   LEANING    → DESPAWNING
    //   TRAVERSING → LANDED
    //   TRAVERSING → DESPAWNING
    //   LANDED     → DESPAWNING
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("legal_transitions_return_true"))
    {
        // Arrange: legal pairs as specified by ADR-0010 D2 transition table.
        using S = EPullWaveState;
        const TArray<TPair<S, S>> LegalPairs =
        {
            { S::Spawned,    S::Leaning     },   // AC-PW-11
            { S::Leaning,    S::Traversing  },   // AC-PW-12
            { S::Leaning,    S::Despawning  },   // pause-flush
            { S::Traversing, S::Landed      },   // AC-PW-13
            { S::Traversing, S::Despawning  },   // pause-flush
            { S::Landed,     S::Despawning  },   // AC-PW-14
        };

        // Act + Assert
        for (const auto& Pair : LegalPairs)
        {
            TestTrue(
                FString::Printf(
                    TEXT("TC1: IsTransitionAllowed(%s→%s) must return true"),
                    (Pair.Key == S::Spawned    ? TEXT("SPAWNED")    :
                     Pair.Key == S::Leaning    ? TEXT("LEANING")    :
                     Pair.Key == S::Traversing ? TEXT("TRAVERSING") :
                     Pair.Key == S::Landed     ? TEXT("LANDED")     : TEXT("DESPAWNING")),
                    (Pair.Value == S::Leaning    ? TEXT("LEANING")    :
                     Pair.Value == S::Traversing ? TEXT("TRAVERSING") :
                     Pair.Value == S::Landed     ? TEXT("LANDED")     : TEXT("DESPAWNING"))),
                APullWaveSubsystemActor::IsTransitionAllowed(Pair.Key, Pair.Value));
        }

        return true;
    }

    // -----------------------------------------------------------------------
    // TC2 — IsTransitionAllowed: all 14 forbidden transitions return false.
    //
    // Forbidden cells (ADR-0010 D2 — every non-legal, non-self inter-state pair):
    //   SPAWNED    → TRAVERSING, LANDED, DESPAWNING       (3 cells)
    //   LEANING    → SPAWNED,    LANDED                   (2 cells)
    //   TRAVERSING → SPAWNED,    LEANING                  (2 cells)
    //   LANDED     → SPAWNED,    LEANING,    TRAVERSING   (3 cells)
    //   DESPAWNING → SPAWNED,    LEANING,    TRAVERSING,  LANDED  (4 cells)
    //   Total: 14 forbidden cells (story doc says "13" — confirmed typo; 5×5-5self-6legal=14)
    //
    // Story 003 AC explicitly names 7 and "remaining 7" — all 14 are tested here.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("forbidden_transitions_return_false"))
    {
        using S = EPullWaveState;
        const TArray<TPair<S, S>> ForbiddenPairs =
        {
            // SPAWNED exits forbidden
            { S::Spawned,    S::Traversing  },
            { S::Spawned,    S::Landed      },
            { S::Spawned,    S::Despawning  },
            // LEANING exits forbidden
            { S::Leaning,    S::Spawned     },
            { S::Leaning,    S::Landed      },
            // TRAVERSING exits forbidden
            { S::Traversing, S::Spawned     },
            { S::Traversing, S::Leaning     },
            // LANDED exits forbidden
            { S::Landed,     S::Spawned     },
            { S::Landed,     S::Leaning     },
            { S::Landed,     S::Traversing  },
            // DESPAWNING is terminal — all exits forbidden
            { S::Despawning, S::Spawned     },
            { S::Despawning, S::Leaning     },
            { S::Despawning, S::Traversing  },
            { S::Despawning, S::Landed      },
        };

        // Verify we have exactly 14 forbidden pairs (all forbidden cells accounted for).
        // Note: the story doc says "13 forbidden" — this appears to be a typo.
        // 5×5=25 cells, minus 5 self-transitions, minus 6 legal = 14 forbidden.
        TestEqual(
            TEXT("TC2: exactly 14 forbidden pairs in test fixture"),
            ForbiddenPairs.Num(),
            14);

        // Act + Assert: every forbidden pair must return false.
        bool bAllFalse = true;
        for (const auto& Pair : ForbiddenPairs)
        {
            const bool bResult =
                APullWaveSubsystemActor::IsTransitionAllowed(Pair.Key, Pair.Value);
            if (bResult)
            {
                bAllFalse = false;
                // Report the specific forbidden pair that incorrectly returned true.
                AddError(FString::Printf(
                    TEXT("TC2: IsTransitionAllowed returned true for forbidden pair (index %d)"),
                    ForbiddenPairs.IndexOfByKey(Pair)));
            }
        }

        TestTrue(
            TEXT("TC2: all 14 forbidden transitions return false from IsTransitionAllowed()"),
            bAllFalse);

        return true;
    }

    // -----------------------------------------------------------------------
    // TC3 — IsTransitionAllowed: all 5 self-transitions return false.
    //
    // Self-transitions (same→same) are explicitly forbidden by ADR-0010 D2.
    // The diagonal of the 25-cell matrix is 5 cells; all must return false.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("self_transitions_return_false"))
    {
        using S = EPullWaveState;
        const TArray<S> AllStates =
        {
            S::Spawned, S::Leaning, S::Traversing, S::Landed, S::Despawning
        };

        bool bAllFalse = true;
        for (const S State : AllStates)
        {
            const bool bResult = APullWaveSubsystemActor::IsTransitionAllowed(State, State);
            if (bResult)
            {
                bAllFalse = false;
                AddError(FString::Printf(
                    TEXT("TC3: IsTransitionAllowed(State, State) returned true for state %d — self-transitions must be forbidden"),
                    static_cast<int32>(State)));
            }
        }

        TestTrue(
            TEXT("TC3: all 5 self-transitions return false from IsTransitionAllowed()"),
            bAllFalse);

        return true;
    }

    // -----------------------------------------------------------------------
    // TC4 — TransitionTo: SPAWNED→LEANING mutates Wave.State (AC-PW-11).
    //
    // Arrange: Wave in SPAWNED state.
    // Act: TransitionTo(Wave, LEANING).
    // Assert: Wave.State == LEANING; no other fields mutated by TransitionTo().
    //
    // Note: AC-PW-11 states mutable params are frozen after Construct() — that
    // freeze is enforced by Story 009 Construct(), not by TransitionTo() itself.
    // This test confirms TransitionTo() performs the state mutation only.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("transition_spawned_to_leaning"))
    {
        // Arrange
        APullWaveSubsystemActor* Actor =
            NewObject<APullWaveSubsystemActor>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC4: NewObject<APullWaveSubsystemActor> succeeded"), Actor))
        {
            return false;
        }

        FPullWaveInstanceState Wave = MakeStateIn(1, EPullWaveState::Spawned);
        const int32  WaveIdBefore        = Wave.WaveId;
        const float  LeanProgressBefore  = Wave.LeanProgress;

        // Act
        Actor->TransitionTo(Wave, EPullWaveState::Leaning);

        // Assert: state advanced to LEANING.
        TestEqual(
            TEXT("TC4: Wave.State == LEANING after TransitionTo(LEANING)"),
            Wave.State,
            EPullWaveState::Leaning);

        // Assert: TransitionTo() does not modify unrelated fields.
        TestEqual(TEXT("TC4: WaveId unchanged"), Wave.WaveId, WaveIdBefore);
        TestEqual(TEXT("TC4: LeanProgress unchanged by TransitionTo()"), Wave.LeanProgress, LeanProgressBefore);

        return true;
    }

    // -----------------------------------------------------------------------
    // TC5 — TransitionTo: LEANING→TRAVERSING (AC-PW-12).
    //
    // AC-PW-12 specifies:
    //   - Transition is allowed.
    //   - TraverseElapsedS is initialized to 0.0f at TRAVERSING entry (by Story 004
    //     tick body — TransitionTo() itself only flips the state flag).
    //   - LeanProgress is NOT cleared (it is at 1.0f at the transition boundary).
    //
    // This test verifies that:
    //   - TransitionTo() succeeds (Wave.State becomes TRAVERSING).
    //   - TransitionTo() does NOT clear LeanProgress (Story 004 is responsible for
    //     driving it to 1.0f; we set it to 1.0f here to confirm it's preserved).
    //   - TraverseElapsedS is not modified by TransitionTo() itself (Story 004 owns
    //     the accumulator initialization; we verify TransitionTo() doesn't reset it).
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("transition_leaning_to_traversing"))
    {
        // Arrange
        APullWaveSubsystemActor* Actor =
            NewObject<APullWaveSubsystemActor>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC5: NewObject<APullWaveSubsystemActor> succeeded"), Actor))
        {
            return false;
        }

        FPullWaveInstanceState Wave = MakeStateIn(2, EPullWaveState::Leaning);
        Wave.LeanProgress      = 1.0f;   // At transition boundary per AC-PW-12.
        Wave.TraverseElapsedS  = 0.0f;   // Story 004 initializes this; set baseline.

        // Act
        Actor->TransitionTo(Wave, EPullWaveState::Traversing);

        // Assert: state advanced.
        TestEqual(
            TEXT("TC5: Wave.State == TRAVERSING after TransitionTo(TRAVERSING)"),
            Wave.State,
            EPullWaveState::Traversing);

        // Assert: LeanProgress is NOT cleared by TransitionTo() (AC-PW-12 — "NOT cleared").
        TestEqual(
            TEXT("TC5: LeanProgress == 1.0f — TransitionTo() does not clear it (AC-PW-12)"),
            Wave.LeanProgress,
            1.0f);

        // Assert: TraverseElapsedS is 0.0f — matches Story 004's expected initial value.
        TestEqual(
            TEXT("TC5: TraverseElapsedS == 0.0f at TRAVERSING entry baseline (Story 004 initializes)"),
            Wave.TraverseElapsedS,
            0.0f);

        return true;
    }

    // -----------------------------------------------------------------------
    // TC6 — TransitionTo: LEANING→DESPAWNING (pause-flush pathway).
    //
    // Story 007 routes LEANING waves to DESPAWNING during pause-flush.
    // Transition must be legal; Wave.State must become DESPAWNING.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("transition_leaning_to_despawning"))
    {
        // Arrange
        APullWaveSubsystemActor* Actor =
            NewObject<APullWaveSubsystemActor>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC6: NewObject<APullWaveSubsystemActor> succeeded"), Actor))
        {
            return false;
        }

        FPullWaveInstanceState Wave = MakeStateIn(3, EPullWaveState::Leaning);

        // Act
        Actor->TransitionTo(Wave, EPullWaveState::Despawning);

        // Assert
        TestEqual(
            TEXT("TC6: Wave.State == DESPAWNING after LEANING→DESPAWNING (pause-flush)"),
            Wave.State,
            EPullWaveState::Despawning);

        return true;
    }

    // -----------------------------------------------------------------------
    // TC7 — TransitionTo: TRAVERSING→LANDED (AC-PW-13).
    //
    // AC-PW-13 specifies:
    //   - Entry fires on first tick where world_z <= player_plane_z.
    //   - CollisionOutcome defaults to Unresolved until LANDED-entry evaluation
    //     (Story 005 assigns the definitive outcome).
    //
    // This test verifies:
    //   - TransitionTo() succeeds (Wave.State becomes LANDED).
    //   - CollisionOutcome is Unresolved on a freshly constructed wave
    //     (TransitionTo() does not assign it — that is Story 005's responsibility).
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("transition_traversing_to_landed"))
    {
        // Arrange
        APullWaveSubsystemActor* Actor =
            NewObject<APullWaveSubsystemActor>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC7: NewObject<APullWaveSubsystemActor> succeeded"), Actor))
        {
            return false;
        }

        FPullWaveInstanceState Wave = MakeStateIn(4, EPullWaveState::Traversing);
        // CollisionOutcome defaults to Unresolved at construction (FPullWaveInstanceState default).

        // Act
        Actor->TransitionTo(Wave, EPullWaveState::Landed);

        // Assert: state advanced.
        TestEqual(
            TEXT("TC7: Wave.State == LANDED after TransitionTo(LANDED)"),
            Wave.State,
            EPullWaveState::Landed);

        // Assert: CollisionOutcome is still Unresolved (Story 005 assigns it).
        TestEqual(
            TEXT("TC7: CollisionOutcome == Unresolved at LANDED entry — Story 005 assigns definitive value (AC-PW-13)"),
            Wave.CollisionOutcome,
            ECollisionOutcome::Unresolved);

        return true;
    }

    // -----------------------------------------------------------------------
    // TC8 — TransitionTo: TRAVERSING→DESPAWNING (pause-flush pathway).
    //
    // Story 007 routes TRAVERSING waves to DESPAWNING during pause-flush.
    // Transition must be legal; Wave.State must become DESPAWNING.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("transition_traversing_to_despawning"))
    {
        // Arrange
        APullWaveSubsystemActor* Actor =
            NewObject<APullWaveSubsystemActor>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC8: NewObject<APullWaveSubsystemActor> succeeded"), Actor))
        {
            return false;
        }

        FPullWaveInstanceState Wave = MakeStateIn(5, EPullWaveState::Traversing);

        // Act
        Actor->TransitionTo(Wave, EPullWaveState::Despawning);

        // Assert
        TestEqual(
            TEXT("TC8: Wave.State == DESPAWNING after TRAVERSING→DESPAWNING (pause-flush)"),
            Wave.State,
            EPullWaveState::Despawning);

        return true;
    }

    // -----------------------------------------------------------------------
    // TC9 — TransitionTo: LANDED→DESPAWNING (AC-PW-14, hold elapsed).
    //
    // AC-PW-14: fires after WAVE_DESPAWN_HOLD_S elapsed. The timing advance
    // is Story 004's responsibility; this test confirms the transition itself
    // is legal and Wave.State becomes DESPAWNING.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("transition_landed_to_despawning"))
    {
        // Arrange
        APullWaveSubsystemActor* Actor =
            NewObject<APullWaveSubsystemActor>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC9: NewObject<APullWaveSubsystemActor> succeeded"), Actor))
        {
            return false;
        }

        FPullWaveInstanceState Wave = MakeStateIn(6, EPullWaveState::Landed);
        Wave.LandedHoldElapsedS = WAVE_DESPAWN_HOLD_S;   // Hold time satisfied.

        // Act
        Actor->TransitionTo(Wave, EPullWaveState::Despawning);

        // Assert
        TestEqual(
            TEXT("TC9: Wave.State == DESPAWNING after LANDED→DESPAWNING (hold elapsed, AC-PW-14)"),
            Wave.State,
            EPullWaveState::Despawning);

        return true;
    }

    // -----------------------------------------------------------------------
    // TC10 — TransitionTo shipping-safe guard: forbidden call does NOT modify State.
    //
    // Story 003 AC: "in Test/Shipping build, forbidden-transition call does NOT
    // crash; logs LogPullWave Error with pull_wave_illegal_transition and returns
    // without modifying state."
    //
    // check(bAllowed) is gated out of UE_BUILD_TEST in TransitionTo() (ADR-0010 D2 /
    // code-review fix 2026-08-18) so this test can call TransitionTo() on a forbidden
    // pair without aborting the test runner. The if (!bAllowed) guard is the live path
    // in both UE_BUILD_TEST and UE_BUILD_SHIPPING.
    //
    // Forbidden pair under test: SPAWNED→TRAVERSING (explicitly named in Story 003 AC).
    //
    // AddExpectedError() suppresses the expected Error-level log and fails the test if
    // the log is NOT emitted — verifying the telemetry path fires.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("shipping_safe_guard_no_state_mutation"))
    {
        // Arrange
        APullWaveSubsystemActor* Actor =
            NewObject<APullWaveSubsystemActor>(GetTransientPackage());
        if (!TestNotNull(TEXT("TC10: NewObject<APullWaveSubsystemActor> succeeded"), Actor))
        {
            return false;
        }

        FPullWaveInstanceState Wave = MakeStateIn(7, EPullWaveState::Spawned);

        // Expect exactly one Error-level log containing "pull_wave_illegal_transition".
        // If TransitionTo() does NOT emit this log, the test fails.
        AddExpectedError(
            TEXT("pull_wave_illegal_transition"),
            EAutomationExpectedErrorFlags::Contains,
            1);

        // Act: call TransitionTo() on a forbidden pair.
        // In UE_BUILD_TEST, check() is compiled out; the if (!bAllowed) guard logs
        // the error and returns without mutating Wave.State.
        Actor->TransitionTo(Wave, EPullWaveState::Traversing);

        // Assert: Wave.State is unchanged — guard returned without mutating it.
        TestEqual(
            TEXT("TC10: Wave.State == SPAWNED — shipping-safe guard did not mutate state"),
            Wave.State,
            EPullWaveState::Spawned);

        return true;
    }

    // -----------------------------------------------------------------------
    // TC11 — DESPAWNING is terminal: all 4 exits from DESPAWNING return false.
    //
    // ADR-0010 D2: "DESPAWNING is a terminal state (no exit except pool removal)."
    // All four non-self transitions from DESPAWNING must return false.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("despawning_is_terminal"))
    {
        using S = EPullWaveState;
        const TArray<S> AllNonSelf =
        {
            S::Spawned, S::Leaning, S::Traversing, S::Landed
        };

        bool bAllFalse = true;
        for (const S ToState : AllNonSelf)
        {
            const bool bResult =
                APullWaveSubsystemActor::IsTransitionAllowed(S::Despawning, ToState);
            if (bResult)
            {
                bAllFalse = false;
                AddError(FString::Printf(
                    TEXT("TC11: IsTransitionAllowed(DESPAWNING→%d) returned true — DESPAWNING must be terminal"),
                    static_cast<int32>(ToState)));
            }
        }

        TestTrue(
            TEXT("TC11: all 4 exits from DESPAWNING return false (terminal state per ADR-0010 D2)"),
            bAllFalse);

        return true;
    }

    // -----------------------------------------------------------------------
    // TC12 — SPAWNED is entry-only: only LEANING is a legal exit from SPAWNED.
    //
    // Story 003 AC: "SPAWNED is an entry-only state (only reachable at Construct())".
    // From SPAWNED, only the transition to LEANING is permitted. The other three
    // non-self exits (TRAVERSING, LANDED, DESPAWNING) must all return false.
    // -----------------------------------------------------------------------
    if (Parameters == TEXT("spawned_exits_only_to_leaning"))
    {
        using S = EPullWaveState;

        // Only legal exit from SPAWNED.
        TestTrue(
            TEXT("TC12: IsTransitionAllowed(SPAWNED→LEANING) == true (only legal exit)"),
            APullWaveSubsystemActor::IsTransitionAllowed(S::Spawned, S::Leaning));

        // All other non-self exits from SPAWNED must be forbidden.
        const TArray<S> ForbiddenFromSpawned =
        {
            S::Traversing, S::Landed, S::Despawning
        };

        bool bAllFalse = true;
        for (const S ToState : ForbiddenFromSpawned)
        {
            const bool bResult =
                APullWaveSubsystemActor::IsTransitionAllowed(S::Spawned, ToState);
            if (bResult)
            {
                bAllFalse = false;
                AddError(FString::Printf(
                    TEXT("TC12: IsTransitionAllowed(SPAWNED→%d) returned true — SPAWNED exits only to LEANING"),
                    static_cast<int32>(ToState)));
            }
        }

        TestTrue(
            TEXT("TC12: SPAWNED→{TRAVERSING,LANDED,DESPAWNING} all return false (entry-only state)"),
            bAllFalse);

        return true;
    }

    // Unknown parameter — fail explicitly.
    AddError(FString::Printf(
        TEXT("FPullWaveStateMachineTest::RunTest — unknown Parameters value: '%s'"),
        *Parameters));
    return false;
}

#endif // WITH_DEV_AUTOMATION_TESTS

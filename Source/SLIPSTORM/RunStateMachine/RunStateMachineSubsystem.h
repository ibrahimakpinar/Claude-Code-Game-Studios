// Copyright (c) Game Studio. All rights reserved.
// SLIPSTORM — 60-second voxel endless runner, mobile, Unreal Engine 5.7
//
// RunStateMachineSubsystem — stub declaration for the RSM subsystem.
// Full implementation deferred to RSM epic (ADR-0007).
// TODO(RSM epic): stub — replace with ADR-0007 implementation.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "RunStateMachine/ERunState.h"
#include "RunStateMachine/ERunOutcome.h"
#include "RunStateMachineSubsystem.generated.h"

// --- Non-dynamic multicast delegates (ADR-0007 lines 199-210) ---
// Per GDD Rule 15: non-dynamic multicast.
// TODO(RSM epic): stub — replace with ADR-0007 implementation.
DECLARE_MULTICAST_DELEGATE_FourParams(
    FOnStateChanged,
    ERunState   /* PreviousState */,
    ERunState   /* NewState */,
    ERunOutcome /* Outcome */,
    double      /* Timestamp */);

// Per GDD Rule 18 (Wave Spawner R3a FC-2): non-dynamic multicast.
// TODO(RSM epic): stub — replace with ADR-0007 implementation.
DECLARE_MULTICAST_DELEGATE_TwoParams(
    FOnPausedChanged,
    bool   /* bIsPaused */,
    double /* Timestamp */);

/**
 * URunStateMachineSubsystem
 *
 * Stub. Full ADR-0007 implementation (UGameInstanceSubsystem + FTickableGameObject,
 * state machine, sleep-aware time source) deferred to RSM epic.
 * All public methods return safe defaults (IDLE / NONE / false).
 *
 * TODO(RSM epic): stub — replace with ADR-0007 implementation.
 */
UCLASS()
class SLIPSTORM_API URunStateMachineSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    // --- Public delegates (ADR-0007 Key Interfaces) ---
    // TODO(RSM epic): stub — replace with ADR-0007 implementation.
    FOnStateChanged   OnStateChanged;
    FOnPausedChanged  OnPausedChanged;

    // --- Public API (ADR-0007 Key Interfaces) ---

    /** Force RSM to tick synchronously. Idempotent within-frame via bHasTickedThisFrame guard.
     *  PM calls this as first statement in TickComponent (ADR-0009 SD4, ADR-0007 SD2).
     *  TODO(RSM epic): stub — replace with ADR-0007 implementation. */
    void ForceTickNow();

    /** Returns the current RSM state. Stub returns IDLE.
     *  TODO(RSM epic): stub — replace with ADR-0007 implementation. */
    ERunState GetCurrentState() const;

    /** Returns the outcome of the concluded run. Stub returns NONE.
     *  TODO(RSM epic): stub — replace with ADR-0007 implementation. */
    ERunOutcome GetRunOutcome() const;

    /** Returns true when the run is paused. Stub returns false.
     *  TODO(RSM epic): stub — replace with ADR-0007 implementation. */
    bool IsPaused() const;

    /** Returns true during the resume-grace window. Stub returns false.
     *  TODO(RSM epic): stub — replace with ADR-0007 implementation. */
    bool IsResumeGrace() const;

    /**
     * Returns the 64-bit deterministic seed generated at COUNTDOWN→RUNNING.
     * WaveSpawnerSubsystem captures this at Cold→Active to seed PatternRNG (TR-WS-013).
     * Stub returns 0 until RSM epic delivers the real seed from the ADR-0007 run sequence.
     * TODO(RSM epic): stub — replace with ADR-0007 implementation.
     */
    uint64 GetRunSeed() const;

#if WITH_DEV_AUTOMATION_TESTS
    /** Test-only: increments once per ForceTickNow invocation. Story 003 JC-1. */
    mutable int32 TestOnly_ForceTickNowCallCount = 0;

    /** Test-only: increments once per GetCurrentState invocation. */
    mutable int32 TestOnly_GetCurrentStateCallCount = 0;

    /** Test-only: snapshot of TestOnly_GetCurrentStateCallCount captured inside
     *  ForceTickNow's body BEFORE the ForceTickNow counter increments. Sentinel
     *  -1 means never snapshotted. After a TickComponent call, this MUST be 0
     *  to prove ForceTickNow ran BEFORE any GetCurrentState property read
     *  (ADR-0009 IG-1 ordering invariant). */
    mutable int32 TestOnly_GetCurrentStateCountAtForceTickNow = -1;

    /** Test-only override for GetCurrentState. Default IDLE preserves the
     *  pre-existing stub behavior for tests that don't opt in. Story 003 JC-1. */
    ERunState TestOnly_CurrentState = ERunState::IDLE;

    /** Test-only override for IsPaused. Default false. Story 003 JC-1. */
    bool TestOnly_bPaused = false;

    /** Test-only override for IsResumeGrace. Default false. Story 003 JC-1. */
    bool TestOnly_bResumeGrace = false;

    /** Grants FPMStateMachineTest + FPMLifecycleAndSeamTest direct read/write
     *  access to TestOnly_* fields. Story 003 JC-1 + Story 001a friend. */
    friend class FPMStateMachineTest;
#endif // WITH_DEV_AUTOMATION_TESTS
};

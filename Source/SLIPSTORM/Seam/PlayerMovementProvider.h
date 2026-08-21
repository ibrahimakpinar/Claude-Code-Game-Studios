// Copyright (c) Game Studio. All rights reserved.
// SLIPSTORM — 60-second voxel endless runner, mobile, Unreal Engine 5.7
//
// PlayerMovementProvider.h — Seam 12: IPlayerMovementProvider interface
// and production implementation.
//
// This seam decouples Pull-Wave (consumer) from the concrete
// UPlayerLaneMovementComponent (provider).  It must never introduce a
// hard dependency in the direction consumer → implementation.
//
// Key invariant (AC-SS-B): EMovementState ordinals MUST stay locked to
// ERunSlipState ordinals at all times.  The static_assert in
// PlayerMovementProvider.cpp enforces this at compile time.
//
// See docs/architecture/platform-seam-interfaces.md §Seam 12,
//     design/gdd/player-movement-mechanics.md §Movement State Enum,
//     ADR-0009-player-movement-hosting.md.

#pragma once

#include "CoreMinimal.h"
#include "Player/EPlayerLane.h"

// Forward declaration — Seam consumers see only the interface, not the impl.
class UPlayerLaneMovementComponent;

// ---------------------------------------------------------------------------
// EMovementState — Seam 12 mirror of ERunSlipState.
// Ordinals are PINNED and MUST stay ordinal-locked with ERunSlipState.
// AC-SS-B compile-time gate in PlayerMovementProvider.cpp enforces this.
// DO NOT reorder without simultaneously updating ERunSlipState.
// ---------------------------------------------------------------------------

/** Seam 12 movement state — mirrors ERunSlipState ordinals exactly.
 *  SETTLED=0, SLIPPING=1 — pinned; do not reorder.
 *  AC-SS-B static_assert in PlayerMovementProvider.cpp enforces drift = 0. */
enum class EMovementState : uint8
{
    SETTLED  = 0,   // At rest in current lane (mirrors ERunSlipState::SETTLED)
    SLIPPING = 1,   // Mid-tween to target lane (mirrors ERunSlipState::SLIPPING)
};

// ---------------------------------------------------------------------------
// IPlayerMovementProvider — pure-virtual interface for Seam 12.
// ---------------------------------------------------------------------------

/**
 * IPlayerMovementProvider
 *
 * Pure-virtual seam interface between Pull-Wave (consumer) and
 * UPlayerLaneMovementComponent (provider).  All getters are const; the
 * near-miss beat trigger is the only mutating call.
 *
 * Test stub: FPlayerMovementProvider_TestStub (lives in Tests/).
 * Production impl: FPlayerMovementProvider_Production (below).
 */
class IPlayerMovementProvider
{
public:
    virtual ~IPlayerMovementProvider() = default;

    /** Returns the lane the player currently occupies (committed root position). */
    virtual EPlayerLane GetCurrentLane() const = 0;

    /** Returns the lane the player is tweening toward (or same as current if SETTLED). */
    virtual EPlayerLane GetTargetLane() const = 0;

    /** Returns the current movement state (SETTLED / SLIPPING). */
    virtual EMovementState GetMovementState() const = 0;

    /** Triggers a near-miss beat on the PM component (Story 012 / ADR-0002 bridge).
     *  Pull-Wave calls this when a wave passes the player within the near-miss
     *  threshold.  The PM component owns haptic + audio dispatch. */
    virtual void TriggerNearMissBeat() = 0;
};

// ---------------------------------------------------------------------------
// FPlayerMovementProvider_Production — concrete production implementation.
// ---------------------------------------------------------------------------

/**
 * FPlayerMovementProvider_Production
 *
 * Production Seam 12 wrapper around UPlayerLaneMovementComponent.
 * Holds a TWeakObjectPtr so it does not pin the component in GC.
 *
 * Construct once per game session after the pawn is spawned:
 *   FPlayerMovementProvider_Production Provider(MyPM);
 *
 * All getters return safe defaults (Center / SETTLED) if the weak pointer
 * is no longer valid (e.g. pawn destroyed mid-frame).
 */
class FPlayerMovementProvider_Production final : public IPlayerMovementProvider
{
public:
    explicit FPlayerMovementProvider_Production(UPlayerLaneMovementComponent* InPM);

    // IPlayerMovementProvider
    virtual EPlayerLane    GetCurrentLane()    const override;
    virtual EPlayerLane    GetTargetLane()     const override;
    virtual EMovementState GetMovementState()  const override;
    virtual void           TriggerNearMissBeat()    override;

private:
    TWeakObjectPtr<UPlayerLaneMovementComponent> WeakPM;
};

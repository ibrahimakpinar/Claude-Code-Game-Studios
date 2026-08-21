// Copyright (c) Game Studio. All rights reserved.
// SLIPSTORM — 60-second voxel endless runner, mobile, Unreal Engine 5.7
//
// PlayerMovementProvider.cpp — Seam 12 production implementation.
//
// AC-SS-B compile-time gate: the static_assert below fires at compile time
// if ERunSlipState and EMovementState ordinals drift.
// See design/gdd/player-movement-mechanics.md §Movement State Enum,
//     ADR-0009, R7-PM-PROPAGATION-REVIEW.

#include "Seam/PlayerMovementProvider.h"
#include "Player/PlayerLaneMovementComponent.h"
#include "Player/ERunSlipState.h"

// ---------------------------------------------------------------------------
// AC-SS-B — compile-time ordinal lockstep gate.
// This fires immediately if either enum is reordered without updating the
// other.  Do not move, comment out, or wrap in #if.
// Reference: ADR-0009 Validation Criteria 5 + R7-PM-PROPAGATION-REVIEW.
// ---------------------------------------------------------------------------
static_assert(
    static_cast<uint8>(ERunSlipState::SETTLED)  == static_cast<uint8>(EMovementState::SETTLED)  &&
    static_cast<uint8>(ERunSlipState::SLIPPING) == static_cast<uint8>(EMovementState::SLIPPING),
    "ordinal drift PM ↔ Seam 12 ↔ R7-PM-PROPAGATION-REVIEW");

// ---------------------------------------------------------------------------
// FPlayerMovementProvider_Production
// ---------------------------------------------------------------------------

FPlayerMovementProvider_Production::FPlayerMovementProvider_Production(
    UPlayerLaneMovementComponent* InPM)
    : WeakPM(InPM)
{
}

EPlayerLane FPlayerMovementProvider_Production::GetCurrentLane() const
{
    if (UPlayerLaneMovementComponent* PM = WeakPM.Get())
    {
        return PM->current_lane;
    }
    return EPlayerLane::Center;
}

EPlayerLane FPlayerMovementProvider_Production::GetTargetLane() const
{
    if (UPlayerLaneMovementComponent* PM = WeakPM.Get())
    {
        return PM->target_lane;
    }
    return EPlayerLane::Center;
}

EMovementState FPlayerMovementProvider_Production::GetMovementState() const
{
    if (UPlayerLaneMovementComponent* PM = WeakPM.Get())
    {
        // Ordinal-safe cast — AC-SS-B static_assert above guarantees no drift.
        return static_cast<EMovementState>(static_cast<uint8>(PM->movement_state));
    }
    return EMovementState::SETTLED;
}

void FPlayerMovementProvider_Production::TriggerNearMissBeat()
{
    // TODO(Story 012): extend IHapticDispatch with the NearMiss enum value + wire
    // the near-miss dispatch site here. Interface + BufferDrop enum landed in
    // Story 005 (Source/SLIPSTORM/Seam/IHapticDispatch.h).
    // WeakPM check required — pawn may have been destroyed.
    if (UPlayerLaneMovementComponent* PM = WeakPM.Get())
    {
        (void)PM; // placeholder until Story 012 fills the body
    }
}

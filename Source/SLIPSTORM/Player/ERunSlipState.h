// Copyright (c) Game Studio. All rights reserved.
// SLIPSTORM — 60-second voxel endless runner, mobile, Unreal Engine 5.7
//
// ERunSlipState — player movement state enum.
// Ordinals are pinned and MUST match EMovementState in Seam/PlayerMovementProvider.h.
// See design/gdd/player-movement-mechanics.md §Movement State Enum + R7-PM-PROPAGATION-REVIEW.

#pragma once

#include "CoreMinimal.h"
#include "ERunSlipState.generated.h"

/** Player movement state — pinned ordinals.
 *  MUST stay ordinal-locked with EMovementState (Seam 12) at all times.
 *  AC-SS-B static_assert in Seam/PlayerMovementProvider.cpp enforces this at compile time.
 *
 *  R7-PM-PROPAGATION-REVIEW (2026-06-11): pruned from 4 to 2 members.
 *  BUFFERED removed (orthogonal flag, not a state); DEAD removed (DEAD freeze sets SETTLED).
 *  Adding members without updating EMovementState is forbidden. */
UENUM(BlueprintType)
enum class ERunSlipState : uint8
{
    SETTLED  = 0  UMETA(DisplayName = "Settled"),  // At rest in current_lane (also DEAD freeze state)
    SLIPPING = 1  UMETA(DisplayName = "Slipping"), // Mid-tween between current_lane and target_lane
};

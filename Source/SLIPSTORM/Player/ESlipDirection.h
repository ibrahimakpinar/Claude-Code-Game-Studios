// Copyright (c) Game Studio. All rights reserved.
// SLIPSTORM — 60-second voxel endless runner, mobile, Unreal Engine 5.7
//
// ESlipDirection — two-value enum for the queued input direction in the
// single-slot input buffer.  See design/gdd/player-movement-mechanics.md §5
// (Rule 3: single-slot buffer) and Story 005 (input buffer implementation).

#pragma once

#include "CoreMinimal.h"
#include "ESlipDirection.generated.h"

/** Direction of a queued lane-slip input.
 *  Used by UPlayerLaneMovementComponent::queued_input_direction.
 *  Buffer semantics (accept/discard/replace) implemented in Story 005. */
UENUM(BlueprintType)
enum class ESlipDirection : uint8
{
    Left  = 0  UMETA(DisplayName = "Left"),
    Right = 1  UMETA(DisplayName = "Right"),
};

// Copyright (c) Game Studio. All rights reserved.
// SLIPSTORM — 60-second voxel endless runner, mobile, Unreal Engine 5.7
//
// EPlayerLane — 5-lane enum for player lane positions.
// Ordinals are pinned. See design/gdd/player-movement-mechanics.md.

#pragma once

#include "CoreMinimal.h"
#include "EPlayerLane.generated.h"

/** Player lane index — pinned ordinals, do not reorder.
 *  5-lane layout: FarLeft (0) ... FarRight (4).
 *  See design/gdd/player-movement-mechanics.md §Movement State Enum. */
UENUM(BlueprintType)
enum class EPlayerLane : uint8
{
    FarLeft  = 0  UMETA(DisplayName = "Far Left"),
    Left     = 1  UMETA(DisplayName = "Left"),
    Center   = 2  UMETA(DisplayName = "Center"),
    Right    = 3  UMETA(DisplayName = "Right"),
    FarRight = 4  UMETA(DisplayName = "Far Right"),
};

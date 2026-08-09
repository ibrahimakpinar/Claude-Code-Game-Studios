// Copyright (c) Game Studio. All rights reserved.
// SLIPSTORM — 60-second voxel endless runner, mobile, Unreal Engine 5.7
//
// ERunOutcome — Run outcome enum used as 3rd param of FOnStateChanged.
// Full implementation deferred to RSM epic.
// TODO(RSM epic): implement full RSM lifecycle in RunStateMachineSubsystem.

#pragma once

#include "CoreMinimal.h"
#include "ERunOutcome.generated.h"

/** Run outcome — pinned ordinals. See ADR-0007 line 196.
 *  NONE is the neutral/default; set to the terminal outcome on DEAD/COMPLETE/ABORTED transitions. */
UENUM(BlueprintType)
enum class ERunOutcome : uint8
{
    NONE     = 0  UMETA(DisplayName = "None"),     // TODO(RSM epic): no terminal outcome yet
    DEAD     = 1  UMETA(DisplayName = "Dead"),     // TODO(RSM epic): fatal collision
    COMPLETE = 2  UMETA(DisplayName = "Complete"), // TODO(RSM epic): 60s elapsed
    ABORTED  = 3  UMETA(DisplayName = "Aborted"),  // TODO(RSM epic): user quit
};

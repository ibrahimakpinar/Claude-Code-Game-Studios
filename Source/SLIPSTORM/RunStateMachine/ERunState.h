// Copyright (c) Game Studio. All rights reserved.
// SLIPSTORM — 60-second voxel endless runner, mobile, Unreal Engine 5.7
//
// ERunState — Run State Machine state enum.
// Full implementation deferred to RSM epic. Ordinals are pinned (ABI safety).
// TODO(RSM epic): implement full RSM lifecycle in RunStateMachineSubsystem.

#pragma once

#include "CoreMinimal.h"
#include "ERunState.generated.h"

/** Pinned ordinals — do not reorder. See design/gdd/run-state-machine.md.
 *  ADR-0007 line 193: enum class ERunState : uint8 { IDLE, COUNTDOWN, RUNNING, DEAD, COMPLETE, RESOLVING, ABORTED }. */
UENUM(BlueprintType)
enum class ERunState : uint8
{
    IDLE       = 0  UMETA(DisplayName = "Idle"),       // TODO(RSM epic): pre-session idle
    COUNTDOWN  = 1  UMETA(DisplayName = "Countdown"),  // TODO(RSM epic): 3-2-1 countdown
    RUNNING    = 2  UMETA(DisplayName = "Running"),    // TODO(RSM epic): active 60-second run
    DEAD       = 3  UMETA(DisplayName = "Dead"),       // TODO(RSM epic): fatal collision resolving
    COMPLETE   = 4  UMETA(DisplayName = "Complete"),   // TODO(RSM epic): 60s elapsed — run won
    RESOLVING  = 5  UMETA(DisplayName = "Resolving"),  // TODO(RSM epic): score/replay commit
    ABORTED    = 6  UMETA(DisplayName = "Aborted"),    // TODO(RSM epic): user quit mid-run
};

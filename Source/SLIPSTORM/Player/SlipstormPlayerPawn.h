// Copyright (c) Game Studio. All rights reserved.
// SLIPSTORM — 60-second voxel endless runner, mobile, Unreal Engine 5.7
//
// SlipstormPlayerPawn.h
//
// ASlipstormPlayerPawn — the player pawn for SLIPSTORM.
// Stationary in world space (Option (c) moving-world forward-motion model,
// ADR-0009 SD2 + mechanics OQ-2).  Owns UPlayerLaneMovementComponent.
//
// Blueprint subclass: Content/Player/BP_SlipstormPlayerPawn
// GameMode DefaultPawnClass: BP_SlipstormPlayerPawn (wired in GameMode story)
//
// ADR: docs/architecture/adr-0009-player-movement-hosting.md (SD1, SD2)
// Story: production/epics/player-movement/story-001-pawn-component-skeleton.md

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Player/PlayerLaneMovementComponent.h"
#include "SlipstormPlayerPawn.generated.h"

/**
 * ASlipstormPlayerPawn
 *
 * The player pawn.  Stationary in world space — forward motion is achieved by
 * moving the world (waves, obstacles) past the player (ADR-0009 SD2, Option (c)).
 *
 * Subobjects created in the constructor:
 *   - RootSceneComponent (USceneComponent) — stable world-space anchor
 *   - MeshComponent (UStaticMeshComponent) — voxel player mesh, child of Root
 *   - MovementComponent (UPlayerLaneMovementComponent) — lane-slip logic
 *
 * Lane position is committed to the pawn root via SetActorLocation (Rule 2
 * collision commit); mesh visual position is interpolated via
 * MeshComponent->SetRelativeLocation (F-3 tween) by UPlayerLaneMovementComponent.
 */
UCLASS()
class SLIPSTORM_API ASlipstormPlayerPawn : public APawn
{
    GENERATED_BODY()

public:
    ASlipstormPlayerPawn();

    /** Stable world-space root — pawn root stays at identity Y/Z during play. */
    UPROPERTY(VisibleAnywhere, Category="SLIPSTORM|Movement")
    TObjectPtr<USceneComponent> RootSceneComponent;

    /** Voxel player mesh — child of Root; interpolated by PM during lane slip. */
    UPROPERTY(VisibleAnywhere, Category="SLIPSTORM|Movement")
    TObjectPtr<UStaticMeshComponent> MeshComponent;

    /** Lane-slip movement component — owns all slip logic and delegates. */
    UPROPERTY(VisibleAnywhere, Category="SLIPSTORM|Movement")
    TObjectPtr<UPlayerLaneMovementComponent> MovementComponent;
};

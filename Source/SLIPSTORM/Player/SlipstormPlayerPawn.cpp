// Copyright (c) Game Studio. All rights reserved.
// SLIPSTORM — 60-second voxel endless runner, mobile, Unreal Engine 5.7
//
// SlipstormPlayerPawn.cpp
//
// ADR-0009 Migration Plan Step 2: create ASlipstormPlayerPawn with subobjects.
// Subobject creation order follows Step 3 of the migration plan:
//   Root → Mesh (attached to Root) → MovementComponent
//
// The pawn root's world position is the committed collision X for the current
// target lane (Rule 2, ADR-0009 SD2).  The mesh's relative X is the fractional
// between-lane visual position (F-3, implemented in Story 004).

#include "Player/SlipstormPlayerPawn.h"
#include "Components/StaticMeshComponent.h"

ASlipstormPlayerPawn::ASlipstormPlayerPawn()
{
    // Stable world-space root.  All lane-commit SetActorLocation calls land here.
    RootSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent = RootSceneComponent;

    // Voxel player mesh — attached to root so it moves with pawn world position.
    // UPlayerLaneMovementComponent writes SetRelativeLocation on this component
    // to produce the fractional between-lane visual offset (F-3, Story 004).
    MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    MeshComponent->SetupAttachment(RootComponent);

    // Lane-slip movement component.  No init work at construction — all lifecycle
    // init happens in UPlayerLaneMovementComponent::BeginPlay (ADR-0009 IG-8).
    MovementComponent = CreateDefaultSubobject<UPlayerLaneMovementComponent>(
        TEXT("MovementComponent"));
}

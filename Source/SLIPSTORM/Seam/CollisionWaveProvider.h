// Copyright (c) Game Studio. All rights reserved.
// SLIPSTORM — 60-second voxel endless runner, mobile, Unreal Engine 5.7
//
// CollisionWaveProvider.h — Seam: ICollisionWaveProvider interface.
// Decouples Pull-Wave despawn pipeline (step 1) from the Collision subsystem.
// Injected via APullWaveSubsystemActor::SetCollisionProvider().
// Null → step 1 silently skipped (Collision epic not yet wired).
//
// ADR:   docs/architecture/adr-0010-pullwave-object-pool-state-machine-despawn.md §D4
// Story: production/epics/pull-wave/story-006-despawn-pipeline.md

#pragma once
#include "CoreMinimal.h"

class ICollisionWaveProvider
{
public:
    virtual ~ICollisionWaveProvider() = default;
    /** Unregister WaveId from the collision subsystem at DESPAWNING entry (Rule 13 step 1). */
    virtual void UnregisterWave(int32 WaveId) = 0;
};

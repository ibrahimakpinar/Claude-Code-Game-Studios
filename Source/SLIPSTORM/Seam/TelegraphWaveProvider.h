// Copyright (c) Game Studio. All rights reserved.
// SLIPSTORM — 60-second voxel endless runner, mobile, Unreal Engine 5.7
//
// TelegraphWaveProvider.h — Seam: ITelegraphWaveProvider interface.
// Decouples Pull-Wave despawn pipeline (step 2) from the Telegraph subsystem.
// Injected via APullWaveSubsystemActor::SetTelegraphProvider().
// Null → step 2 silently skipped (Telegraph epic not yet wired).
//
// ADR:   docs/architecture/adr-0010-pullwave-object-pool-state-machine-despawn.md §D4
// Story: production/epics/pull-wave/story-006-despawn-pipeline.md

#pragma once
#include "CoreMinimal.h"

class ITelegraphWaveProvider
{
public:
    virtual ~ITelegraphWaveProvider() = default;
    /** Unregister WaveId from the telegraph subsystem at DESPAWNING entry (Rule 13 step 2). */
    virtual void UnregisterWave(int32 WaveId) = 0;
};

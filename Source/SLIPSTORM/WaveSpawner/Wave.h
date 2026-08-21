// Copyright (c) Game Studio. All rights reserved.
// SLIPSTORM — 60-second voxel endless runner, mobile, Unreal Engine 5.7
//
// Wave.h — AWave: lightweight state token for the Wave Spawner object pool.
//
// AWave is NOT a renderer-owning actor. Per ADR-0005 IG-7 and the INT-001
// component-ownership topology, renderer components (WaveMassISMC, TrailCubeISMC)
// are owned by APullWaveSubsystemActor — NOT by individual pooled AWave actors.
// The forbidden pattern "per_AWave_actor_render_component_ownership" is registered
// in ADR-0005; do not add ISMC members here.
//
// AWave carries per-wave pool-slot tracking fields (WaveId, bInUse) consulted by
// code holding an AWave*. Full per-wave gameplay fields (lane, phase, timestamps)
// are added in Story 002+ when the lifecycle state machine is implemented.
//
// Pool management:
//   Pre-allocated: 23 instances in UWaveSpawnerSubsystem::OnFirstWorldLoaded()
//   Acquire:       UWaveSpawnerSubsystem::AcquireFromPool() → AWave*
//   Release:       UWaveSpawnerSubsystem::ReleaseToPool(WaveId)
//
// Story: production/epics/wave-spawner/story-001-subsystem-class-and-object-pool.md
// ADR:   docs/architecture/adr-0005-wave-spawner-subsystem-hosting.md (IG-7)
// TRs:   TR-WS-008, TR-WS-009

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Wave.generated.h"

/**
 * AWave — pooled state token for the Wave Spawner.
 *
 * Lightweight AActor stub. Contains only pool-slot tracking fields for Story 001.
 * Full per-wave gameplay state (lane, phase, timing) added in Story 002+.
 *
 * MUST NOT contain UInstancedStaticMeshComponent members named WaveMassISMC or
 * TrailCubeISMC. Per ADR-0005 IG-7, those live on APullWaveSubsystemActor.
 */
UCLASS()
class SLIPSTORM_API AWave : public AActor
{
    GENERATED_BODY()

public:
    AWave();

    /**
     * Unique identifier for this pool slot while acquired.
     * INDEX_NONE (-1) when the slot is available in the pool (unacquired).
     * Assigned by the admission pipeline at AcquireFromPool(); reset to INDEX_NONE
     * by ReleaseToPool(). Monotonically increasing per ADR-0005 pool-acquire contract.
     */
    UPROPERTY()
    int32 WaveId = INDEX_NONE;

    /**
     * True while this slot is leased to an active wave. False while available in pool.
     * Set to true by UWaveSpawnerSubsystem::AcquireFromPool().
     * Cleared to false by UWaveSpawnerSubsystem::ReleaseToPool(WaveId).
     * Not UPROPERTY — pool-slot tracking only; no GC or serialization needed.
     */
    bool bInUse = false;

protected:
    /** Story 001 stub — gameplay BeginPlay logic added in Story 002+. */
    virtual void BeginPlay() override;
};

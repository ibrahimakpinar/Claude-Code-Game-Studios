// Copyright (c) Game Studio. All rights reserved.
// SLIPSTORM — 60-second voxel endless runner, mobile, Unreal Engine 5.7
//
// DPCSubsystem.h — stub declaration for the DPC (Difficulty Phase Controller) subsystem.
// Full implementation deferred to DPC epic (ADR-0008).
// TODO(DPC epic): stub — replace with ADR-0008 implementation.
//
// Minimal stub required by WaveSpawnerSubsystem::Initialize() to satisfy the
// ADR-0005 INT-004 mandatory ordering pins:
//   - Collection.InitializeDependency(UDPCSubsystem::StaticClass())  (ADR-0005 IG-8)
//   - GetSubsystem<UDPCSubsystem>()  (ADR-0005 Initialize skeleton)
//   - DPC->OnPostTickFrameStatePublished.AddUObject(...)  (ADR-0005 R2a-2)
//
// ADR: docs/architecture/adr-0008-dpc-subsystem-hosting.md
// Story: production/epics/wave-spawner/story-001-subsystem-class-and-object-pool.md

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "DPCSubsystem.generated.h"

// ---------------------------------------------------------------------------
// FDPCFrameState — published each tick by DPC (ADR-0008 Key Interfaces).
//
// Stub: minimal fields consumed by WaveSpawnerSubsystem's admission pipeline.
// WaveSpawner reads this struct in OnDPCFrameReady() to evaluate Rule 1 gate
// and Rule 7 admission (Stories 003–007).
//
// TODO(DPC epic): stub — replace with ADR-0008 full FDPCFrameState declaration.
// ---------------------------------------------------------------------------
USTRUCT()
struct SLIPSTORM_API FDPCFrameState
{
    GENERATED_BODY()

    /** True when DPC is actively computing difficulty for a RUNNING, non-paused state.
     *  WaveSpawner gates admission on bIsActive (TR-DPC-021 / Rule 1, Story 003). */
    UPROPERTY()
    bool bIsActive = false;

    /** Current telegraph window in seconds. Clamped >= TELEGRAPH_WINDOW_FLOOR_S (0.70s).
     *  Snapshot immutable at Wave SPAWNED entry (TR-PW-004, ADR-0008). */
    UPROPERTY()
    float TelegraphWindowS = 0.94f;

    /** Current wave spawn interval in seconds. Clamped >= WAVE_SPAWN_INTERVAL_FLOOR_S (0.25s).
     *  Cadence gate uses this field (TR-WS-015, Story 003). */
    UPROPERTY()
    float WaveSpawnIntervalS = 1.5f;
};

// ---------------------------------------------------------------------------
// FOnPostTickFrameStatePublished — fired by DPC after each tick snapshot write.
//
// WaveSpawnerSubsystem binds OnDPCFrameReady to this delegate in Initialize()
// to achieve structural tick ordering: DPC publishes → WaveSpawner reads.
// (ADR-0005 R2a-2; TR-WS-024; TR-DPC-013)
//
// TODO(DPC epic): stub — replace with ADR-0008 full delegate declaration.
// ---------------------------------------------------------------------------
DECLARE_MULTICAST_DELEGATE_OneParam(FOnPostTickFrameStatePublished, const FDPCFrameState&);

/**
 * UDPCSubsystem — Difficulty Phase Controller.
 *
 * Stub. Full ADR-0008 implementation (UGameInstanceSubsystem + FTickableGameObject,
 * RSM-gated tick ordering, phase computation, FDPCFrameState publishing,
 * OnPostTickFrameStatePublished broadcast) deferred to DPC epic.
 *
 * Exposed here so UWaveSpawnerSubsystem::Initialize() can call:
 *   Collection.InitializeDependency(UDPCSubsystem::StaticClass())  (ADR-0005 IG-8)
 *   GetSubsystem<UDPCSubsystem>() → bind OnPostTickFrameStatePublished
 *
 * TODO(DPC epic): stub — replace with ADR-0008 implementation.
 */
UCLASS()
class SLIPSTORM_API UDPCSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    /**
     * WaveSpawnerSubsystem binds OnDPCFrameReady to this delegate in Initialize().
     * DPC broadcasts this after computing and writing its FDPCFrameState snapshot
     * each tick — establishing the structural tick-ordering guarantee (ADR-0005 R2a-2).
     * TODO(DPC epic): stub — replace with ADR-0008 implementation.
     */
    FOnPostTickFrameStatePublished OnPostTickFrameStatePublished;
};

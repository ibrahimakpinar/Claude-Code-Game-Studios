// Copyright (c) Game Studio. All rights reserved.
// SLIPSTORM — 60-second voxel endless runner, mobile, Unreal Engine 5.7
//
// WaveSpawnerCallback.h — Seam: IWaveSpawnerCallback production interface.
//
// Defines the pure C++ observer interface for the WaveSpawner Rule 12 despawn
// pipeline (ADR-0011 D3, Rule 12). This is distinct from the Pull-Wave despawn
// pipeline (ADR-0010 Rule 13 — see CollisionWaveProvider.h / TelegraphWaveProvider.h).
//
// Do NOT confuse these two despawn pipelines:
//   ADR-0010 (Pull-Wave):    CollisionWaveProvider + TelegraphWaveProvider
//   ADR-0011 (WaveSpawner):  IWaveSpawnerCallback  <- THIS FILE
//
// Implemented by:
//   - FWaveSpawnerCallbackTestStub (Seam 13) — integration tests, non-Shipping only
//   - Production observers wired by Story 007 (collision, telegraph, telemetry)
//
// Seam 13 contract: SetOnDespawnedUserCallback is intentionally NOT on this
// interface. It belongs only on FWaveSpawnerCallbackTestStub (non-Shipping).
// A C++20 requires-expression static_assert in WaveSpawnerCallbackTestStub.h
// enforces this at compile time.
//
// Plain C++ only — no UINTERFACE macro, no UObject inheritance, no UHT overhead.
//
// ADR: docs/architecture/adr-0011-wave-spawner-pattern-library.md (D3 Rule 12, Seam 13)
// Story: production/epics/wave-spawner/story-006-despawn-pipeline-and-seam-13.md
// TR:    TR-WS-026

#pragma once

#include "CoreMinimal.h"
#include "WaveSpawner/WaveSpawnerTypes.h"

/**
 * IWaveSpawnerCallback
 *
 * Production observer interface for the WaveSpawner Rule 12 despawn pipeline.
 * Implemented by any system that receives ordered notification when a wave despawns.
 *
 * Mandatory pipeline order (enforced by UWaveSpawnerSubsystem::DespawnWave,
 * TR-WS-026). This sequence fires regardless of EWaveDespawnReason:
 *   1. OnCollisionUnregistered(WaveId)  — collision subsystem cleanup
 *   2. OnTelegraphUnregistered(WaveId)  — telegraph subsystem cleanup
 *      [ Live slot released from LiveSlots here ]
 *   3. OnWaveDespawned(WaveId, Reason)  — final notification; slot already released
 *
 * Plain C++ — no UObject overhead, not a UInterface.
 */
class IWaveSpawnerCallback
{
public:
    virtual ~IWaveSpawnerCallback() = default;

    /**
     * Step 1 of the Rule 12 ordered despawn pipeline (TR-WS-026).
     * Called first, before OnTelegraphUnregistered and before the Live slot is released.
     * Implementors should unregister WaveId from the collision subsystem here.
     *
     * @param WaveId  Identifier of the despawning AWave actor.
     */
    virtual void OnCollisionUnregistered(int32 WaveId) = 0;

    /**
     * Step 2 of the Rule 12 ordered despawn pipeline (TR-WS-026).
     * Called after OnCollisionUnregistered, before the Live slot is released.
     * Implementors should unregister WaveId from the telegraph subsystem here.
     *
     * @param WaveId  Identifier of the despawning AWave actor.
     */
    virtual void OnTelegraphUnregistered(int32 WaveId) = 0;

    /**
     * Step 3 (final) of the Rule 12 ordered despawn pipeline (TR-WS-026).
     * Called after the Live slot has already been released from LiveSlots.
     * Implementors receive the despawn reason for routing, telemetry, or assertions.
     *
     * @param WaveId   Identifier of the despawned AWave actor.
     * @param Reason   Why the wave was despawned (see EWaveDespawnReason).
     *
     * NOTE: SetOnDespawnedUserCallback is intentionally NOT on this interface.
     * It is Seam 13 test-infrastructure only — see FWaveSpawnerCallbackTestStub.
     */
    virtual void OnWaveDespawned(int32 WaveId, EWaveDespawnReason Reason) = 0;
};

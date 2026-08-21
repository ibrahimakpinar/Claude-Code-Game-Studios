// Copyright (c) Game Studio. All rights reserved.
// SLIPSTORM — 60-second voxel endless runner, mobile, Unreal Engine 5.7
//
// Wave.cpp — AWave implementation.
//
// Lightweight state token only. Tick disabled at construction (no per-tick cost).
// Full gameplay logic deferred to Story 002+.
//
// Story: production/epics/wave-spawner/story-001-subsystem-class-and-object-pool.md
// ADR:   docs/architecture/adr-0005-wave-spawner-subsystem-hosting.md (IG-7)

#include "WaveSpawner/Wave.h"

AWave::AWave()
{
    // Lightweight state token — no tick needed.
    // Visual representation handled by APullWaveSubsystemActor's ISMC (ADR-0005 IG-7).
    PrimaryActorTick.bCanEverTick = false;
}

void AWave::BeginPlay()
{
    Super::BeginPlay();
    // Story 001 stub — gameplay logic added in Story 002+.
}

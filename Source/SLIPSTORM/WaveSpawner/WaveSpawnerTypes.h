// Copyright (c) Game Studio. All rights reserved.
// SLIPSTORM — 60-second voxel endless runner, mobile, Unreal Engine 5.7
//
// WaveSpawnerTypes.h — shared types for the Wave Spawner subsystem.
//
// Contents:
//   EWaveSpawnerLifecycleState — lifecycle state enum; controls IsTickable() gating
//                                (Story 001); full transitions in Story 002.
//   FPatternDefinition          — minimal stub; full fields in Story 003+.
//   FPatternPool                — three-pool data structure per ADR-0011 D1.
//   EAdmissionResult            — admission gate result codes; full use in Story 003+.
//
// Story: production/epics/wave-spawner/story-001-subsystem-class-and-object-pool.md
// ADR:   docs/architecture/adr-0005-wave-spawner-subsystem-hosting.md (IG-5)
// ADR:   docs/architecture/adr-0011-wave-spawner-pattern-library-d1-three-pool.md
// TRs:   TR-WS-010, TR-WS-011

#pragma once

#include "CoreMinimal.h"
#include "WaveSpawnerTypes.generated.h"

// ---------------------------------------------------------------------------
// Stat group for Unreal Insights and stat unit profiling (ADR-0005 IG-5).
// Declared here (in the types header) so all translation units that include
// WaveSpawnerSubsystem.h (via this header) see the group before the inline
// GetStatId() body references STATGROUP_WaveSpawner. Matches the existing
// STATGROUP_PullWave declaration pattern in PullWaveTypes.h.
// AC-WS-30: admission-tick CPU <= 0.30ms p99 profiled under this stat scope.
// ---------------------------------------------------------------------------
DECLARE_STATS_GROUP(TEXT("WaveSpawner"), STATGROUP_WaveSpawner, STATCAT_Advanced);

// ---------------------------------------------------------------------------
// EWaveSpawnerLifecycleState
//
// Internal lifecycle state machine. Controls IsTickable() gating (Story 001).
// Full Cold → Active → Holding → Flushing → Idle transition logic is Story 002.
//
// IsTickable() contract (ADR-0005 Decision; TR-WS-011):
//   Cold  → false  (pool may be allocated; run not yet started)
//   Idle  → false  (run ended; no waves in flight)
//   All other states → true
// ---------------------------------------------------------------------------
UENUM(BlueprintType)
enum class EWaveSpawnerLifecycleState : uint8
{
    /**
     * Initial state on subsystem creation.
     * Pool allocated at PostLoadMapWithWorld while in this state; subsystem
     * awaits Cold → Active trigger (RSM RUNNING transition, Story 002).
     */
    Cold,

    /**
     * Actively admitting patterns and spawning waves.
     * Admission pipeline fires on each OnDPCFrameReady() callback.
     */
    Active,

    /**
     * Pattern admission paused (near phase-boundary drain window per Rule 9).
     * In-flight waves continue; no new patterns admitted.
     */
    Holding,

    /**
     * All in-flight waves being returned to pool.
     * Entered on pause (Rule 13) or run termination.
     */
    Flushing,

    /**
     * All waves despawned; run ended; pool fully returned.
     * Awaiting next run trigger (RSM COUNTDOWN → RUNNING, Story 002).
     */
    Idle,
};

// ---------------------------------------------------------------------------
// FPatternDefinition — stub for Story 001.
//
// Full fields (lane assignments, tier, onset offsets, pattern variant) added in
// Story 003+ when the admission pipeline and pattern library are implemented.
// Cook-time validator populates these at editor time (Story 008).
// ---------------------------------------------------------------------------
USTRUCT(BlueprintType)
struct SLIPSTORM_API FPatternDefinition
{
    GENERATED_BODY()

    /**
     * Unique identifier for this pattern. Matches the cook-time asset registry
     * entry used by the validator (Story 008) and the Death Replay system (TR-WS-014).
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName PatternId = NAME_None;

    /**
     * True when this pattern is a barrage (3-lane simultaneous onset cluster).
     * False for non-barrage (single-wave staggered).
     * Determines which FPatternPool sub-array this entry populates:
     *   true  → BarragePatterns   (PEAK phase only; TR-WS-004/005)
     *   false → NonBarragePatterns (all phases)
     * Used by admission gate Rule 7 (Story 003).
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bIsBarrage = false;
};

// ---------------------------------------------------------------------------
// FPatternPool — per-phase pattern pool (OPENER / MID / PEAK).
//
// ADR-0011 D1 three-pool architecture. Each phase gets its own FPatternPool.
// Barrage and non-barrage patterns are separated into sub-arrays for O(1)
// selection during the admission pipeline (Story 003+).
//
// Invariants (enforced by cook-time validator, Story 008):
//   OPENER: BarragePatterns.Num() == 0  (TR-WS-002)
//   MID:    BarragePatterns.Num() == 0  (TR-WS-003)
//   PEAK:   BarragePatterns contains exactly 7 valid triplet patterns (TR-WS-004)
//
// Runtime: pools populated at cook time; never mutated during gameplay.
// ---------------------------------------------------------------------------
USTRUCT(BlueprintType)
struct SLIPSTORM_API FPatternPool
{
    GENERATED_BODY()

    /**
     * Barrage patterns (3-lane simultaneous). PEAK phase only.
     * OPENER and MID pools must leave this empty (TR-WS-002/003).
     * Cook-time validator enforces exactly 7 valid triplet entries for PEAK (TR-WS-004).
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<FPatternDefinition> BarragePatterns;

    /**
     * Non-barrage (single-wave) patterns. All three phases use this sub-array.
     * Cook-time validator enforces minimum pool depth per phase (Story 008).
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<FPatternDefinition> NonBarragePatterns;
};

// ---------------------------------------------------------------------------
// EAdmissionResult — admission gate outcome codes (Story 003+).
//
// Stub for Story 001. Full admission gate (Rule 1 + Rule 7) implemented in Story 003.
// Referenced by AcquireFromPool() in documentation and by telemetry events (TR-WS-019).
// ---------------------------------------------------------------------------
UENUM(BlueprintType)
enum class EAdmissionResult : uint8
{
    /** Pattern admitted and AWave slot leased from pool. Spawn proceeds. */
    Admitted,

    /** Deferred: active wave count >= max_concurrent_waves (DPC concurrency cap, TR-WS-017). */
    Deferred_ConcurrencyCap,

    /** Deferred: barrage requires 3 free slots atomically; fewer available (TR-WS-017). */
    Deferred_SlotAtomic,

    /** Deferred: barrage_owed flag set; next 3-slot window reserved for barrage draw (TR-WS-018). */
    Deferred_BarrageOwed,

    /** All 23 pool slots in use. Should be prevented by concurrency cap (TR-WS-008). */
    PoolExhausted,
};

// ---------------------------------------------------------------------------
// ERunPhase — current pattern pool phase within a run.
//
// Initialized to Opener at Cold→Active entry (first RunSeed capture).
// Advanced by atomic pool-pointer swap on each Active→Holding entry per
// ADR-0011 D3 Rule 10: Opener→Mid on first boundary, Mid→Peak on second.
// Reset to Opener on Flushing→Cold (run-termination path).
//
// Phase never regresses. Peak→Holding stays at Peak (barrage_owed logic
// continues; no further phase to advance to).
//
// Story: production/epics/wave-spawner/story-002-six-state-lifecycle.md
// ADR:   docs/architecture/adr-0011-wave-spawner-pattern-library.md (D3)
// TRs:   TR-WS-020 (phase initialization at run start)
//         TR-WS-021 (atomic pointer swap; pattern data never copied)
// ---------------------------------------------------------------------------
UENUM(BlueprintType)
enum class ERunPhase : uint8
{
    /**
     * Initial phase: t_norm 0.0 → OPENER_END.
     * Non-barrage patterns only (TR-WS-002).
     * Active draw pool: OpenerPool.
     */
    Opener  UMETA(DisplayName = "Opener"),

    /**
     * Mid-run phase: OPENER_END → PEAK_START.
     * Non-barrage patterns only (TR-WS-003).
     * Active draw pool: MidPool.
     */
    Mid     UMETA(DisplayName = "Mid"),

    /**
     * Peak phase: PEAK_START → 1.0.
     * Both barrage and non-barrage patterns admitted (TR-WS-004).
     * Active draw pool: PeakPool.
     */
    Peak    UMETA(DisplayName = "Peak"),
};

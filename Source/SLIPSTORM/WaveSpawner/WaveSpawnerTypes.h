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
// AC-WS-30: cycle stats for admission-tick and pool-alloc profiling (Story 009 / TR-WS-030).
// DEFINE_STAT counterparts live in WaveSpawnerSubsystem.cpp.
// Empty 4th arg matches the STATGROUP_PullWave pattern in PullWaveTypes.h (no export needed here;
// the group is defined in the same TU that includes this header via the subsystem).
DECLARE_CYCLE_STAT_EXTERN(TEXT("WaveSpawner Admission Tick"), STAT_WaveSpawnerAdmissionTick, STATGROUP_WaveSpawner, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("WaveSpawner Pool Alloc"),     STAT_WaveSpawnerPoolAlloc,     STATGROUP_WaveSpawner, );

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

    /**
     * Source lane indices. Barrage patterns use exactly 3 distinct lane indices (the
     * "triplet"). Non-barrage patterns use 1 or more lanes (typically 1). Lanes are
     * 0-indexed; valid range [0, 4] for a 5-lane level.
     *
     * Used by the cook-time validator (Story 008):
     *   PEAK_SURVIVING_TRIPLETS — barrage sub-pool covers exactly the 7 valid triplets.
     *   MIN_BARRAGE_PATTERN_COUNT_PER_TRIPLET — each of 7 triplets has >= 1 pattern.
     *   BARRAGE_DISTINCT_SOURCE_LANES — no two PEAK barrage patterns share the same lane set.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<int32> SourceLanes;

    /**
     * Telegraph onset times in seconds, relative to the spawn trigger (first onset = 0.0f).
     * Entries must be in ascending order.
     *
     * Cook-time constraints (Rule 15, AC-WS-05/06):
     *   Barrage patterns     — all onsets must cluster within TELEGRAPH_WINDOW_FLOOR_S / 2 = 0.35s
     *                          (BARRAGE_W_SPAN check).
     *   Non-barrage patterns — consecutive onset gaps must each be >= TELEGRAPH_WINDOW_FLOOR_S = 0.70s
     *                          (NON_BARRAGE_STAGGER check).
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<float> OnsetTimes;

    /**
     * Lean magnitude tier (1, 2, or 3) controlling the Pull-Wave's approach lean angle.
     *
     * Cook-time constraints (Rule 15, AC-WS-04/35):
     *   PEAK_BARRAGE_MIN_TIER   — PEAK barrage patterns require tier >= 2 (tier-1 banned).
     *   BARRAGE_UNIFORM_TIER    — all PEAK barrage patterns must share the same tier value.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 LeanMagnitudeTier = 1;

    /**
     * True if this pattern is eligible to serve as the primer (first wave spawned during
     * the OPENER phase per Rule 9b). At least one primer-eligible non-barrage pattern is
     * required in the OPENER pool.
     *
     * Cook-time constraint: PRIMER_PATTERN check (AC-WS-07, Rule 15).
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bIsPrimerEligible = false;

    /**
     * True if this pattern allows the player to make a SLIP (lane-transition) verb move
     * while the wave is approaching. At least one pattern in each non-empty pool must
     * have this set to true.
     *
     * Cook-time constraint: PILLAR_1_VERB_SLIP check (AC-WS-08, Rule 15).
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bAllowsSlip = false;
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

// ---------------------------------------------------------------------------
// EWaveDespawnReason — reason code for the Rule 12 DespawnWave() pipeline.
//
// Passed as the final argument to IWaveSpawnerCallback::OnWaveDespawned().
// Fires for all three values; the pipeline order (CollisionUnregistered →
// TelegraphUnregistered → OnWaveDespawned) is mandatory regardless of reason.
//
// None=0 sentinel: prevents {} initializer from aliasing NaturalLanding when
// FDespawnEvent entries are constructed for CollisionUnregistered and
// TelegraphUnregistered events (which carry no meaningful Reason value).
//
// Story: production/epics/wave-spawner/story-006-despawn-pipeline-and-seam-13.md
// ADR:   docs/architecture/adr-0011-wave-spawner-pattern-library.md (D3, Rule 12)
// TR:    TR-WS-026
// ---------------------------------------------------------------------------
UENUM(BlueprintType)
enum class EWaveDespawnReason : uint8
{
    /** Sentinel — Reason field not applicable (CollisionUnregistered, TelegraphUnregistered events). */
    None           = 0,

    /** Wave completed its normal lifecycle (reached the landing zone). */
    NaturalLanding = 1,

    /** RSM moved to DEAD/COMPLETE/ABORTED (Story 007 run-termination path — Rule 14). */
    RunTermination = 2,

    /** RSM pause event flushed in-flight waves (Story 007 pause-flush path — Rule 13). */
    PauseFlush     = 3,
};

// ---------------------------------------------------------------------------
// FWaveInFlightState — per-wave admission snapshot (Story 007).
//
// Captured once in TryAdmitPattern() at the moment of slot pre-commitment.
// NEVER mutated after capture — DPC publishing a new frame, or a pool-pointer
// swap at a phase boundary, does NOT re-parametrize admitted waves (TR-WS-022,
// AC-WS-21, AC-WS-29).
//
// Storage: UWaveSpawnerSubsystem::InFlightWaves (TMap<int32, FWaveInFlightState>).
// Removed in DespawnWave() alongside LiveSlots.Remove(). Reset on Flushing→Cold.
//
// Plain C++ struct — not a USTRUCT. Value type used as TMap<int32, ...> mapped value;
// no UObject lifecycle involvement. GC-safe: contains only int32 and float.
//
// ADR:   docs/architecture/adr-0011-wave-spawner-pattern-library.md (D3 TR-WS-022)
// Story: production/epics/wave-spawner/story-007-rsm-dpc-integration.md
// TRs:   TR-WS-022 (snapshot immutability post-admission)
// ---------------------------------------------------------------------------
struct FWaveInFlightState
{
    /** Wave slot identifier. Matches the key in the InFlightWaves TMap. */
    int32 WaveId = INDEX_NONE;

    /**
     * Telegraph window in seconds captured from FDPCFrameState.TelegraphWindowS at
     * TryAdmitPattern() time. NEVER mutated after this point (TR-WS-022).
     * A subsequent DPC frame publishing a different TelegraphWindowS value does NOT
     * overwrite this field (AC-WS-21). A mid-phase pool-pointer swap (Active→Holding)
     * does NOT overwrite this field (AC-WS-29).
     */
    float TelegraphWindowS = 0.f;
};

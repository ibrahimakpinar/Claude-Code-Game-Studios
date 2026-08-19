// Copyright (c) Game Studio. All rights reserved.
// SLIPSTORM — 60-second voxel endless runner, mobile, Unreal Engine 5.7
//
// PullWaveTypes.h — Struct and enum definitions for the Pull-Wave subsystem.
//
// FPullWaveInstanceState is a USTRUCT() so APullWaveSubsystemActor can declare
// UPROPERTY() TArray<FPullWaveInstanceState> ActiveWaves (Story 002 / ADR-0010 D1).
// FPullWaveCurveSnapshot and FPullWaveSpawnParams remain plain C++ structs —
// they are never stored in a UPROPERTY() container directly.
//
// See ADR-0010 §D3 + §D6 for layout rationale and size budget.
//
// GDD:   design/gdd/pull-wave-behavior.md
// ADR:   docs/architecture/adr-0010-pullwave-object-pool-state-machine-despawn.md
// Story: production/epics/pull-wave/story-001-struct-definitions.md (struct defs)
//        production/epics/pull-wave/story-002-pool-storage-waveid.md (USTRUCT scope)
// TRs:   TR-PW-004 (SAMPLE_COUNT=32 locked), TR-PW-007 (sizeof <= 256)
//
// STORY 004 NOTE: DECLARE_CYCLE_STAT_EXTERN(STAT_PullWaveTick) requires a matching
// DEFINE_STAT(STAT_PullWaveTick) in the tick body .cpp. Add it in Story 004.

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "PullWaveTypes.generated.h"

// ---------------------------------------------------------------------------
// Pull-Wave stats group (AC-PW-22b pattern 1)
// DEFINE_STAT counterpart must live in APullWaveSubsystemActor.cpp (Story 004).
// ---------------------------------------------------------------------------
DECLARE_STATS_GROUP(TEXT("PullWave"), STATGROUP_PullWave, STATCAT_Advanced);
DECLARE_CYCLE_STAT_EXTERN(TEXT("PullWave Tick"), STAT_PullWaveTick, STATGROUP_PullWave, );

// ---------------------------------------------------------------------------
// Pull-Wave registry constants (AC-PW-22b pattern 8)
// ---------------------------------------------------------------------------
static constexpr float LEAN_BRIGHTNESS_PEAK_RATIO    = 2.5f;
static constexpr float NEAR_MISS_FLASH_DURATION_S    = 0.066f;
static constexpr float WAVE_DESPAWN_HOLD_S           = 0.15f;
static constexpr float SPAWN_PLANE_Z_OFFSET_M        = 15.0f;
static constexpr float LANE_WIDTH_M                  = 1.0f;
static constexpr float TELEGRAPH_WINDOW_FLOOR_S      = 0.70f;
static constexpr float LEAN_ANGLE_MIN_TIER_GAP_DEG   = 3.0f;
static constexpr int32 MAX_POOL_SIZE                 = 23;
static constexpr int32 MAX_CONCURRENT_WAVES_CAP      = 16;
static constexpr float PLAYER_PLANE_Z                = 0.0f;

// Lean angle tier constants (AC-PW-22b pattern 6 — TIER0 locked at 0.0f)
static constexpr float LEAN_ANGLE_TIER0_DEG = 0.0f;
static constexpr float LEAN_ANGLE_TIER1_DEG = 12.0f;
static constexpr float LEAN_ANGLE_TIER2_DEG = 17.0f;
static constexpr float LEAN_ANGLE_TIER3_DEG = 22.0f;
static constexpr float LEAN_ANGLE_TIER4_DEG = 26.0f;

// Tier ordering static_asserts (AC-PW-22b patterns 7a–7h)
// 7a–7d: strict ordering
static_assert(LEAN_ANGLE_TIER0_DEG < LEAN_ANGLE_TIER1_DEG, "AC-PW-22b-7a: TIER0 < TIER1");
static_assert(LEAN_ANGLE_TIER1_DEG < LEAN_ANGLE_TIER2_DEG, "AC-PW-22b-7b: TIER1 < TIER2");
static_assert(LEAN_ANGLE_TIER2_DEG < LEAN_ANGLE_TIER3_DEG, "AC-PW-22b-7c: TIER2 < TIER3");
static_assert(LEAN_ANGLE_TIER3_DEG < LEAN_ANGLE_TIER4_DEG, "AC-PW-22b-7d: TIER3 < TIER4");
// 7e–7h: minimum gap checks (each adjacent pair >= LEAN_ANGLE_MIN_TIER_GAP_DEG)
static_assert((LEAN_ANGLE_TIER1_DEG - LEAN_ANGLE_TIER0_DEG) >= LEAN_ANGLE_MIN_TIER_GAP_DEG, "AC-PW-22b-7e: TIER1-TIER0 >= MIN_GAP");
static_assert((LEAN_ANGLE_TIER2_DEG - LEAN_ANGLE_TIER1_DEG) >= LEAN_ANGLE_MIN_TIER_GAP_DEG, "AC-PW-22b-7f: TIER2-TIER1 >= MIN_GAP");
static_assert((LEAN_ANGLE_TIER3_DEG - LEAN_ANGLE_TIER2_DEG) >= LEAN_ANGLE_MIN_TIER_GAP_DEG, "AC-PW-22b-7g: TIER3-TIER2 >= MIN_GAP");
static_assert((LEAN_ANGLE_TIER4_DEG - LEAN_ANGLE_TIER3_DEG) >= LEAN_ANGLE_MIN_TIER_GAP_DEG, "AC-PW-22b-7h: TIER4-TIER3 >= MIN_GAP");

// ---------------------------------------------------------------------------
// EPullWaveState — five lifecycle states (ADR-0010 D2).
// Exactly five values; no extension without ADR amendment.
// ---------------------------------------------------------------------------
enum class EPullWaveState : uint8
{
    Spawned,
    Leaning,
    Traversing,
    Landed,
    Despawning,
};

// ---------------------------------------------------------------------------
// ECollisionOutcome — wave/player collision result (ADR-0010 D2).
// Unresolved is the default until LANDED entry assigns a definitive outcome.
// ---------------------------------------------------------------------------
enum class ECollisionOutcome : uint8
{
    Hit,
    NearMiss,
    CleanMiss,
    Unresolved,
};

// ---------------------------------------------------------------------------
// EDespawnReason — reason the despawn pipeline was triggered (ADR-0010 D4).
// ---------------------------------------------------------------------------
enum class EDespawnReason : uint8
{
    NaturalLanding,
    PauseFlush,
    RunTermination,
};

// ---------------------------------------------------------------------------
// FPullWaveCurveSnapshot — immutable per-wave lateral trajectory curve.
// SAMPLE_COUNT = 32 is locked (ADR-0010 D3; TR-PW-004).
// Copied by value into FPullWaveInstanceState at Construct(); never mutated.
// No UCurveFloat* reference held at runtime (D3 Alternative 4 rejection).
// ---------------------------------------------------------------------------
struct FPullWaveCurveSnapshot
{
    /** Number of curve samples. LOCKED — do not change without ADR amendment. */
    static constexpr int32 SAMPLE_COUNT = 32;

    /** Sampled at t_norm = i / (SAMPLE_COUNT - 1). Zero-initialized by default. */
    float Samples[SAMPLE_COUNT] = {};

    /**
     * Evaluate the curve at a normalized time.
     * TNormClamped is internally clamped to [0, 1] — safe against DeltaTime
     * accumulation overshoot. i1 = min(i0+1, SAMPLE_COUNT-1) prevents i1 OOB.
     * Pure function; O(1); no allocations.
     */
    FORCEINLINE float EvaluateAt(float TNormClamped) const
    {
        TNormClamped     = FMath::Clamp(TNormClamped, 0.0f, 1.0f);
        const int32 i0   = static_cast<int32>(TNormClamped * static_cast<float>(SAMPLE_COUNT - 1));
        const int32 i1   = FMath::Min(i0 + 1, SAMPLE_COUNT - 1);
        const float Frac = TNormClamped * static_cast<float>(SAMPLE_COUNT - 1) - static_cast<float>(i0);
        return FMath::Lerp(Samples[i0], Samples[i1], Frac);
    }
};

// ---------------------------------------------------------------------------
// FPullWaveSpawnParams — Wave Spawner → Pull-Wave handoff (ADR-0010 D6).
// Passed by value at Construct(); immutable after Construct() returns.
// Field order is fixed — do not reorder (size contract enforced below).
//
// Layout: 4 + 4 + 4 + 128 + 4 + 4 + 4 = 152 bytes.
// ---------------------------------------------------------------------------
struct FPullWaveSpawnParams
{
    /** Monotonic wave ID; assigned by Wave Spawner from subsystem counter. Never reused. */
    int32 WaveId = 0;

    /** Source lane index [0, 4]. */
    int32 SourceLane = 0;

    /** Target lane index [0, 4]. */
    int32 TargetLane = 0;

    /**
     * Immutable curve snapshot. Sampled by Wave Spawner from a validated UCurveFloat
     * at admission (ADR-0011 D2 Stage 4). Pull-Wave never touches the source asset.
     */
    FPullWaveCurveSnapshot CurveSnapshot;

    /** Forward velocity in m/s. Const for the wave's lifetime (TR-PW-014). */
    float ForwardVelocityMs = 0.0f;

    /** Absolute game-time at Wave Spawner admission. */
    float SpawnTimeS = 0.0f;

    /**
     * = clamp(DPC.telegraph_window_s, TELEGRAPH_WINDOW_FLOOR_S, inf).
     * Captured by Wave Spawner from DPC snapshot at admission; NOT re-read by Pull-Wave.
     */
    float LeanDurationS = 0.0f;
};

static_assert(sizeof(FPullWaveSpawnParams) == 152,
    "AC-PW-22b: FPullWaveSpawnParams size contract (ADR-0010 D6). "
    "Field order or alignment changed — do not reorder fields.");

// ---------------------------------------------------------------------------
// FPullWaveInstanceState — per-wave runtime state (ADR-0010 D3; TR-PW-007).
// USTRUCT() added in Story 002 so APullWaveSubsystemActor can declare
// UPROPERTY() TArray<FPullWaveInstanceState> ActiveWaves.
// Fields are NOT marked UPROPERTY — no GC visibility needed on struct internals
// (ADR-0010 Alternative 2 rejection: no GC overhead at per-wave level).
//
// ADR-0010 D3 design target: 60 scalar bytes + 128 CurveSnapshot = 188 bytes.
// Actual layout with uint8 enum backing: 176 bytes (within 256-byte ceiling).
// Binding contract: sizeof <= 256. Measured size logged in non-Shipping test.
// ---------------------------------------------------------------------------
USTRUCT()
struct FPullWaveInstanceState
{
    GENERATED_BODY()
    /** Monotonic wave ID. Never reused within a session (TR-PW-006). */
    int32 WaveId = 0;

    /** Source lane index [0, 4]. */
    int32 SourceLane = 0;

    /** Target lane index [0, 4]. */
    int32 TargetLane = 0;

    /** ISMC instance index on APullWaveSubsystemActor::WaveMassISMC (ADR-0006). */
    int32 ISMCInstanceIndex = -1;

    /**
     * Immutable curve snapshot copied from FPullWaveSpawnParams at Construct().
     * Never mutated after Construct() returns (Rule 14a; TR-PW-004).
     */
    FPullWaveCurveSnapshot CurveSnapshot;

    /** Forward velocity in m/s. Const for the wave's lifetime (TR-PW-014). */
    float ForwardVelocityMs = 0.0f;

    /** Absolute game-time at admission. */
    float SpawnTimeS = 0.0f;

    /**
     * Duration of the LEANING state in seconds.
     * = clamp(DPC.telegraph_window_s, TELEGRAPH_WINDOW_FLOOR_S, inf).
     */
    float LeanDurationS = 0.0f;

    /**
     * Duration of the TRAVERSING state in seconds (F-TRAVERSE-DURATION formula).
     * TravelDurationS = SPAWN_PLANE_Z_OFFSET_M / ForwardVelocityMs.
     * Computed and stored at Construct() (Story 009).
     */
    float TravelDurationS = 0.0f;

    /** Normalized lean progress in [0, 1]. Advances in LEANING; frozen during pause. */
    float LeanProgress = 0.0f;

    /**
     * Elapsed traversal time in seconds (canonical time source — TR-PW-015).
     * Advances in TRAVERSING; frozen during pause. Single-precision sufficient:
     * range 0–6.25s well within float precision (AC-PW-17b N_MAX=391 analysis).
     */
    float TraverseElapsedS = 0.0f;

    /** Elapsed hold time in LANDED state. Advances toward WAVE_DESPAWN_HOLD_S. */
    float LandedHoldElapsedS = 0.0f;

    /** Current lifecycle state. */
    EPullWaveState State = EPullWaveState::Spawned;

    /** Collision outcome; Unresolved until LANDED entry assigns a definitive value. */
    ECollisionOutcome CollisionOutcome = ECollisionOutcome::Unresolved;

    /**
     * Reason the despawn pipeline was triggered.
     * Set at DESPAWNING entry; consumed by Rule 13 step 3 OnWaveDespawned broadcast.
     * Assigned in Story 006 / Story 007 / Story 008.
     */
    EDespawnReason PendingDespawnReason = EDespawnReason::NaturalLanding;
};

// AC-PW-22b pattern 3: size ceiling check (TR-PW-007).
// Binding contract is <= 256. ADR-0010 D3 design target: 188 bytes.
// Actual compiled size with uint8 enum backing: 176 bytes.
// The measured value is logged at runtime in PullWaveStructDefinitionsTest (TC4).
static_assert(sizeof(FPullWaveInstanceState) <= 256,
    "AC-PW-22b pattern 3: FPullWaveInstanceState exceeds 256-byte cache-friendly ceiling (TR-PW-007).");

// ---------------------------------------------------------------------------
// FLeanTickData — per-tick lean payload broadcast by OnLeanProgress (Story 004).
// Consumed by the Telegraph system to drive visual feedback during LEANING.
// ---------------------------------------------------------------------------
struct FLeanTickData
{
    /** WaveId of the leaning wave. */
    int32 WaveId = 0;

    /** Normalized lean progress in [0, 1]. */
    float LeanProgress = 0.0f;

    /** Lean magnitude tier [0–4]. 0 = same lane (straight forward / tier-0). */
    int32 LeanMagnitudeTier = 0;

    /** Lean angle in degrees. LEAN_ANGLE_TIER0_DEG (0.0°) for tier-0 waves. */
    float LeanAngleDeg = 0.0f;

    /** Charge intensity scalar. Lerps 1.0 → LEAN_BRIGHTNESS_PEAK_RATIO over lean. */
    float ChargeIntensity = 1.0f;
};

// Multicast delegate fired each LEANING tick (ADR-0010 D2 / Rule 12).
// Blueprint-unexposed: internal subsystem signal consumed by Telegraph.
DECLARE_MULTICAST_DELEGATE_OneParam(FOnLeanProgressDelegate, const FLeanTickData&);

// ---------------------------------------------------------------------------
// FTraverseElapsedQuery — non-Shipping test accessor shape (ADR-0010 D2 / R7 B5).
// NOT stored in FPullWaveInstanceState — read-path accessor only.
// ElapsedS sentinel = -1.0f when wave found but State in {SPAWNED, LEANING, DESPAWNING}
// (avoids coincidence with valid TraverseElapsedS=0.0f at TRAVERSING entry).
// ---------------------------------------------------------------------------
#if !UE_BUILD_SHIPPING
struct FTraverseElapsedQuery
{
    /** True if a wave with the queried WaveId was found in the pool. */
    bool bWaveFound = false;

    /** The wave's current state (valid when bWaveFound == true). */
    EPullWaveState State = EPullWaveState::Spawned;

    /**
     * Elapsed traversal seconds when State == TRAVERSING or LANDED.
     * Sentinel -1.0f when State is in {SPAWNED, LEANING, DESPAWNING} (R7 B5).
     */
    float ElapsedS = -1.0f;
};
#endif // !UE_BUILD_SHIPPING

// ---------------------------------------------------------------------------
// OnWaveHit / OnNearMiss multicast delegates (Story 005 / ADR-0010 D2 Rule 12).
// Non-dynamic: internal subsystem signals consumed by the Collision GDD.
// ---------------------------------------------------------------------------

/**
 * Broadcast once at LANDED entry when a wave scores a hit.
 * Params: WaveId, TargetLane (int32), SourceLane (int32), SpawnTimeS (float).
 * Collision GDD subscribes to trigger RSM→DEAD; Pull-Wave does NOT call RSM directly.
 */
DECLARE_MULTICAST_DELEGATE_FourParams(FOnWaveHit,
    int32 /*WaveId*/, int32 /*TargetLane*/, int32 /*SourceLane*/, float /*SpawnTimeS*/);

/**
 * Broadcast once at LANDED entry when a wave is a near-miss.
 * Params: WaveId, TargetLane (int32), SlippedFromLane (int32 — PM.GetCurrentLane() at entry).
 * TriggerNearMissBeat() is called on PM BEFORE this delegate fires (control manifest order).
 */
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnNearMiss,
    int32 /*WaveId*/, int32 /*TargetLane*/, int32 /*SlippedFromLane*/);

/**
 * Broadcast once at DESPAWNING entry per wave (ADR-0010 D4 / Rule 13 step 3).
 * Params: WaveId (int32), DespawnReason (EDespawnReason).
 * Subscriber constraint: same as OnWaveHit — no synchronous ActiveWaves mutation.
 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnWaveDespawned, int32 /*WaveId*/, EDespawnReason /*Reason*/);

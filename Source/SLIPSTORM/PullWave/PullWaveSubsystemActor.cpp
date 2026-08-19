// Copyright (c) Game Studio. All rights reserved.
// SLIPSTORM — 60-second voxel endless runner, mobile, Unreal Engine 5.7
//
// PullWaveSubsystemActor.cpp — Pool initialization, WaveId counter reset,
// five-state machine enforcement, and per-tick wave advance (Stories 002–004).
//
// Story: production/epics/pull-wave/story-002-pool-storage-waveid.md (pool/WaveId)
//        production/epics/pull-wave/story-003-state-machine.md (TransitionTo)
//        production/epics/pull-wave/story-004-tick-advance-traversing.md (tick body)
//        production/epics/pull-wave/story-006-despawn-pipeline.md (despawn pipeline)
//        production/epics/pull-wave/story-007-pause-flush.md (pause-flush batch)
//        production/epics/pull-wave/story-008-run-termination-drain.md (bRunTerminated + drain reason)
//        production/epics/pull-wave/story-009-construct-entry-point.md (Construct() + ComputeSpawnTransform)
// ADR:   docs/architecture/adr-0010-pullwave-object-pool-state-machine-despawn.md
//        docs/architecture/adr-0011-wave-spawner-pattern-library.md
// TRs:   TR-PW-002 (five-state machine + forbidden-transition enforcement)
//        TR-PW-003 (per-tick state advance), TR-PW-015 (TraverseElapsedS accumulator),
//        TR-PW-021 (pause-freeze gate)
//        TR-PW-004 (CurveSnapshot immutable at SPAWNED; SAMPLE_COUNT=32 locked)
//        TR-PW-005 (pool capacity), TR-PW-006 (WaveId uniqueness per session),
//        TR-PW-014 (ForwardVelocityMs frozen at SPAWNED; no mid-flight mutation)
//        TR-PW-016 (six-step despawn pipeline), TR-PW-017 (OnWaveDespawned delegate)

#include "PullWave/PullWaveSubsystemActor.h"
#include "PullWave/PullWaveTypes.h"

// ---------------------------------------------------------------------------
// DEFINE_STAT counterpart for DECLARE_CYCLE_STAT_EXTERN in PullWaveTypes.h.
// (PullWaveStats.cpp stub deleted when this Story 004 implementation was added.)
// ---------------------------------------------------------------------------
DEFINE_STAT(STAT_PullWaveTick);

// ---------------------------------------------------------------------------
// LogPullWave category definition (declaration in PullWaveSubsystemActor.h).
// Story 003 / ADR-0010 D2 — used by TransitionTo() for Shipping-safe telemetry.
// ---------------------------------------------------------------------------
DEFINE_LOG_CATEGORY(LogPullWave);

// ---------------------------------------------------------------------------
// PullWaveStateToString — local string helper for TransitionTo() log messages.
//
// EPullWaveState is a plain enum class : uint8, NOT a UENUM(), so LexToString()
// is not available (Story 003 Implementation Notes). This static helper is used
// instead of LexToString() in UE_LOG calls.
// ---------------------------------------------------------------------------
static const TCHAR* PullWaveStateToString(EPullWaveState S)
{
    switch (S)
    {
        case EPullWaveState::Spawned:     return TEXT("SPAWNED");
        case EPullWaveState::Leaning:     return TEXT("LEANING");
        case EPullWaveState::Traversing:  return TEXT("TRAVERSING");
        case EPullWaveState::Landed:      return TEXT("LANDED");
        case EPullWaveState::Despawning:  return TEXT("DESPAWNING");
        default:                          return TEXT("UNKNOWN");
    }
}

// ---------------------------------------------------------------------------

APullWaveSubsystemActor::APullWaveSubsystemActor()
{
    // Tick enabled: Story 004 adds the per-wave advance body.
    // ADR-0007/0008 prerequisite mechanism ensures RSM→DPC→PM tick before Pull-Wave.
    PrimaryActorTick.bCanEverTick = true;
}

void APullWaveSubsystemActor::InitializePool()
{
    // Lock pool capacity at 23 slots (ADR-0010 D1).
    // Pool sizing: 16 (MAX_CONCURRENT_WAVES_CAP)
    //            +  2 (DESPAWNING_RETURN_LATENCY_SLOTS)
    //            +  5 (MAX_CONCURRENT_WAVES_CAP_CEIL_MARGIN)
    //            = 23 (MAX_POOL_SIZE, locked in PullWaveTypes.h)
    //
    // Reserve allocates the internal buffer once; no heap reallocation can occur
    // during a run as long as ActiveWaves.Num() never exceeds MAX_POOL_SIZE.
    ActiveWaves.Empty();
    ActiveWaves.Reserve(MAX_POOL_SIZE);

    // Reset WaveId counter for this session (TR-PW-006: never reused within session).
    // This is the ONLY site that writes NextWaveId to 0.
    // NOTE: NextWaveId is reserved for future Wave Spawner integration. Construct() (Story 009)
    // does NOT increment it — WaveId is assigned by the Wave Spawner and passed in via
    // FPullWaveSpawnParams (ADR-0011 D2 Stage 6; Story 009 Out of Scope).
    NextWaveId = 0;

    // Reset run-termination flag for the new session (Story 008).
    // bRunTerminated is set by OnRSMRunStateChanged() when RSM leaves RUNNING state.
    bRunTerminated = false;
}

void APullWaveSubsystemActor::BeginPlay()
{
    Super::BeginPlay();
    InitializePool();
}

// ---------------------------------------------------------------------------
// Construct — sole admission gateway from Wave Spawner into the pool
// (ADR-0010 D6; ADR-0011 D2 Stage 6; Story 009 / TR-PW-004, TR-PW-014).
//
// This is the ONLY call site that legally appends to ActiveWaves (TArray::Add).
// Any other direct ActiveWaves.Add() is forbidden (ADR-0010 D1 / AC-PW-22b
// pattern 11; enforced by CI grep on "ActiveWaves\.Add\s*\(" excluding Tests/).
// ---------------------------------------------------------------------------
void APullWaveSubsystemActor::Construct(FPullWaveSpawnParams SpawnParams)
{
    // Step 1: Pool-full guard (Shipping-safe no-op). MAX_POOL_SIZE = 23.
    // Prevents a 24th wave from exceeding the pre-reserved buffer (ADR-0010 D1).
    if (ActiveWaves.Num() >= MAX_POOL_SIZE)
    {
        UE_LOG(LogPullWave, Error,
               TEXT("pull_wave_pool_full WaveId=%d — admission rejected"),
               SpawnParams.WaveId);
        return;
    }

    // Step 2: Acquire pool slot (append-to-tail; O(1) amortized — buffer pre-reserved).
    // WaveId ASC ordering is maintained because Wave Spawner assigns WaveIds
    // monotonically (ADR-0011 D2 Stage 6) — append == ASC append.
    // Add_GetRef returns a reference into the newly appended element without an
    // extra array lookup (ADR-0010 D6 implementation note).
    FPullWaveInstanceState& Wave = ActiveWaves.Add_GetRef(FPullWaveInstanceState{});

    // Step 3: Copy spawn params field-by-field (ADR-0010 D6).
    // WaveId is assigned by Wave Spawner from its own counter (ADR-0011 D2 Stage 6).
    // Construct() receives it; it does NOT read or increment NextWaveId here.
    // (NextWaveId header comment is stale — see Story 009 Out of Scope / ADR-0011.)
    Wave.WaveId            = SpawnParams.WaveId;
    Wave.SourceLane        = SpawnParams.SourceLane;
    Wave.TargetLane        = SpawnParams.TargetLane;
    Wave.CurveSnapshot     = SpawnParams.CurveSnapshot;   // 128-byte value copy (TR-PW-004)
    Wave.ForwardVelocityMs = SpawnParams.ForwardVelocityMs;  // immutable post-Construct (TR-PW-014)
    Wave.SpawnTimeS        = SpawnParams.SpawnTimeS;
    Wave.LeanDurationS     = SpawnParams.LeanDurationS;   // Wave Spawner owns floor-clamp (ADR-0011 D2 Stage 4)

    // Step 6 (out of order for readability — derived value from immutable spawn fields):
    // F-TRAVERSE-DURATION: TravelDurationS = SPAWN_PLANE_Z_OFFSET_M / ForwardVelocityMs.
    // Computed once at Construct() and never re-computed mid-flight (TR-PW-014).
    // NOTE: unguarded divide — ForwardVelocityMs == 0 yields +inf (TravelDurationS).
    // Wave Spawner must ensure ForwardVelocityMs > 0 (ADR-0010 D6 trust contract;
    // not validated here). See KINDA_SMALL_NUMBER guard in ComputeTNorm for comparison.
    Wave.TravelDurationS   = SPAWN_PLANE_Z_OFFSET_M / SpawnParams.ForwardVelocityMs;

    // Step 4: Set initial state — DIRECT ASSIGNMENT, not TransitionTo().
    // Rationale: this is pool-entry construction, not a lifecycle transition.
    // No "from" state exists; the 25-cell table does not cover construction.
    // TransitionTo() is the write site for all subsequent state changes.
    Wave.State             = EPullWaveState::Spawned;

    // Step 5: Initialize accumulators to zero (clean slate for the new lifecycle).
    Wave.LeanProgress        = 0.0f;
    Wave.TraverseElapsedS    = 0.0f;
    Wave.LandedHoldElapsedS  = 0.0f;
    Wave.CollisionOutcome    = ECollisionOutcome::Unresolved;

    // Step 7: Register ISMC instance at the spawn-plane world transform.
    // ComputeSpawnTransform encodes lateral in X (per ADR-0010 D6 spec).
    // NOTE: AdvanceTraversing (Story 004) uses Y for lateral via FVector(0, WorldX, WorldZ).
    // The two conventions agree only at SourceLane==2 (lateral==0). This axis mismatch
    // is intentionally left unresolved here — Story 009 implements to spec; the
    // discrepancy is flagged for architecture review (see ComputeSpawnTransform doc).
    const FTransform InitialTransform = ComputeSpawnTransform(SpawnParams.SourceLane);

#if !UE_BUILD_SHIPPING
    if (OnISMCAddInstanceOverride)
    {
        Wave.ISMCInstanceIndex = OnISMCAddInstanceOverride(InitialTransform);
    }
    else
#endif
    if (WaveMassISMC)
    {
        Wave.ISMCInstanceIndex = WaveMassISMC->AddInstance(InitialTransform, /*bWorldSpace=*/true);
    }
    // else: null ISMC in test context without seam → ISMCInstanceIndex stays -1
    // (default-initialized from FPullWaveInstanceState{} in step 2 above).

#if !UE_BUILD_SHIPPING
    if (OnISMCConstructDataOverride)
    {
        OnISMCConstructDataOverride(Wave.ISMCInstanceIndex, 0, 0.0f);
        OnISMCConstructDataOverride(Wave.ISMCInstanceIndex, 1, 0.0f);
        OnISMCConstructDataOverride(Wave.ISMCInstanceIndex, 2, 0.0f);
    }
    else
#endif
    if (WaveMassISMC && Wave.ISMCInstanceIndex >= 0)
    {
        // PerInstanceCustomData[0]=0 (lean tier), [1]=0 (near-miss flash), [2]=0 (dissolve).
        // bMarkDirty=false: render state batched once per frame after the ActiveWaves loop
        // (ADR-0010 D2; MarkRenderStateDirty called at the end of Tick, not here).
        WaveMassISMC->SetCustomDataValue(Wave.ISMCInstanceIndex, 0, 0.0f, /*bMarkDirty=*/false);
        WaveMassISMC->SetCustomDataValue(Wave.ISMCInstanceIndex, 1, 0.0f, /*bMarkDirty=*/false);
        WaveMassISMC->SetCustomDataValue(Wave.ISMCInstanceIndex, 2, 0.0f, /*bMarkDirty=*/false);
    }
}

// ---------------------------------------------------------------------------
// ComputeSpawnTransform — world-space transform at the spawn plane (Story 009).
//
// Lateral axis: X = (SourceLane - 2) × LANE_WIDTH_M  (lane 2 = X=0 centre).
// Forward axis: Z = SPAWN_PLANE_Z_OFFSET_M (wave enters from above the player plane).
// Y axis: 0 (depth into/out of screen — unused at spawn time).
//
// AXIS NOTE: AdvanceTraversing (Story 004) uses Y for lateral once a wave is
// TRAVERSING. This initial spawn transform and the TRAVERSING update diverge for
// any SourceLane != 2. The discrepancy is a pre-existing spec gap; do not silently
// reconcile here (flagged for architecture review, out of scope for Story 009).
// ---------------------------------------------------------------------------
FTransform APullWaveSubsystemActor::ComputeSpawnTransform(int32 SourceLane) const
{
    const float LaneX = static_cast<float>(SourceLane - 2) * LANE_WIDTH_M;
    return FTransform(
        FRotator::ZeroRotator,
        FVector(LaneX, 0.0f, SPAWN_PLANE_Z_OFFSET_M),
        FVector::OneVector);
}

void APullWaveSubsystemActor::SetRSMProvider(IPullWaveRSMProvider* Provider)
{
    RSMProvider = Provider;
}

void APullWaveSubsystemActor::SetPMProvider(IPlayerMovementProvider* Provider)
{
    PMProvider = Provider;
}

void APullWaveSubsystemActor::SetCollisionProvider(ICollisionWaveProvider* Provider)
{
    CollisionProvider = Provider;
}

void APullWaveSubsystemActor::SetTelegraphProvider(ITelegraphWaveProvider* Provider)
{
    TelegraphProvider = Provider;
}

// ---------------------------------------------------------------------------
// Tick — per-frame wave advance (Story 004 / ADR-0010 D2).
//
// Tick order: RSM → DPC → PM → Pull-Wave → Telegraph → Collision.
// SCOPE_CYCLE_COUNTER is FIRST statement after Super::Tick() (AC-PW-22b pattern 2).
// Pause-freeze: if RSM is paused, skip advance for all waves (continue).
// ISMC dirty: MarkRenderStateDirty() called ONCE after the loop, not per wave.
// ---------------------------------------------------------------------------
void APullWaveSubsystemActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    SCOPE_CYCLE_COUNTER(STAT_PullWaveTick);  // AC-PW-22b pattern 2: FIRST after Super

    // ---------------------------------------------------------------------------
    // Pause-flush top-of-tick batch (Story 007 / ADR-0010 D2 R7 B13).
    //
    // bPauseFlushPending is set synchronously in OnRSMPausedChanged(false).
    // Consumed HERE — before the per-wave iteration loop — so the flush is always
    // batch-atomic (AC-PW-MID-TICK-PAUSE-DEFERRAL guarantee). Inline mid-tick flush
    // is explicitly rejected (ADR-0010 Alternative 3).
    //
    // Rule 19: only flush when RSM is in RUNNING state at consumption time.
    // If ABORTED/DEAD/etc. at the next tick, skip the flush and clear the flag —
    // Story 008 run-termination drain handles those waves instead.
    //
    // Flush scope: LEANING/TRAVERSING/LANDED waves only.
    //   SPAWNED → DESPAWNING is a forbidden transition (transition table) — skipped.
    //   DESPAWNING is already terminal — skipped.
    // TransitionTo() only mutates Wave.State here; no RemoveAt fires in this loop.
    // The DESPAWNING pipeline (Rule 13, six steps, Story 006) fires for flushed waves
    // in the per-wave iteration loop that follows.
    // ---------------------------------------------------------------------------
    if (bPauseFlushPending)
    {
        bPauseFlushPending = false;  // Consume flag immediately — cleared regardless of RUNNING check.

        if (RSMProvider && RSMProvider->GetCurrentState() == ERunState::RUNNING)
        {
            // Walk in WaveId ASC order (ActiveWaves is always ASC — pool invariant).
            // Range-for is safe: TransitionTo() does NOT modify array structure here
            // (no RemoveAt until per-wave loop below). Only Wave.State changes.
            for (FPullWaveInstanceState& Wave : ActiveWaves)
            {
                if (Wave.State == EPullWaveState::Leaning   ||
                    Wave.State == EPullWaveState::Traversing ||
                    Wave.State == EPullWaveState::Landed)
                {
                    Wave.PendingDespawnReason = EDespawnReason::PauseFlush;
                    TransitionTo(Wave, EPullWaveState::Despawning);
                }
                // SPAWNED: forbidden SPAWNED→DESPAWNING transition — skipped.
                // DESPAWNING: already terminal — skipped.
            }
        }
        // RSMProvider is null, or state is not RUNNING: clear flag without flushing.
        // Story 008 handles ABORTED/DEAD run-termination drain.
    }

    const bool bIsPaused = RSMProvider ? RSMProvider->GetIsPaused() : false;

    for (int32 i = 0; i < ActiveWaves.Num(); ++i)
    {
        FPullWaveInstanceState& Wave = ActiveWaves[i];

        if (bIsPaused)
        {
            continue;  // D2 pause-freeze: skip advance; accumulator does not change
        }

        switch (Wave.State)
        {
            case EPullWaveState::Spawned:
                AdvanceSpawned(Wave);
                break;
            case EPullWaveState::Leaning:
                AdvanceLeaning(Wave, DeltaTime);
                break;
            case EPullWaveState::Traversing:
                AdvanceTraversing(Wave, DeltaTime);
                break;
            case EPullWaveState::Landed:
                AdvanceLanded(Wave, DeltaTime);
                break;
            case EPullWaveState::Despawning:
                // Story 006 — six-step despawn pipeline.
                // After AdvanceDespawning returns, Wave& is dangling (RemoveAt step 6 fired).
                // --i compensates so the next ++i in the for-header revisits the same index,
                // which now holds the element that was at [i+1] before RemoveAt.
                AdvanceDespawning(Wave, Wave.PendingDespawnReason, i);
                --i;
                break;
            default:
                break;
        }
    }

    // Batch ISMC dirty after all wave transforms for this frame are updated.
    // One call per frame (not per wave) per ADR-0010 D2 render-state batching rule.
    if (WaveMassISMC)
    {
        WaveMassISMC->MarkRenderStateDirty();
    }
}

// ---------------------------------------------------------------------------
// AdvanceSpawned — SPAWNED is a one-tick init state (ADR-0010 D2 / D6).
//
// D6 clarification: LeanDurationS is captured by the Wave Spawner at admission
// (Construct() / Story 009) — it is NOT re-read from DPC here. The SPAWNED tick
// body's sole responsibility is transitioning to LEANING.
// ---------------------------------------------------------------------------
void APullWaveSubsystemActor::AdvanceSpawned(FPullWaveInstanceState& Wave)
{
    TransitionTo(Wave, EPullWaveState::Leaning);
}

// ---------------------------------------------------------------------------
// AdvanceLeaning — accumulate LeanProgress, broadcast lean data, check transition.
// ---------------------------------------------------------------------------
void APullWaveSubsystemActor::AdvanceLeaning(FPullWaveInstanceState& Wave, float DeltaTime)
{
    Wave.LeanProgress = FMath::Clamp(
        Wave.LeanProgress + DeltaTime / FMath::Max(Wave.LeanDurationS, KINDA_SMALL_NUMBER),
        0.0f, 1.0f);

    // Compute lean animation payload for this tick and broadcast to Telegraph.
    FLeanTickData Data;
    Data.WaveId = Wave.WaveId;
    Data.LeanProgress = Wave.LeanProgress;
    Data.LeanMagnitudeTier = FMath::Clamp(FMath::Abs(Wave.TargetLane - Wave.SourceLane), 0, 4);
    switch (Data.LeanMagnitudeTier)
    {
        case 0: Data.LeanAngleDeg = LEAN_ANGLE_TIER0_DEG; break;
        case 1: Data.LeanAngleDeg = LEAN_ANGLE_TIER1_DEG; break;
        case 2: Data.LeanAngleDeg = LEAN_ANGLE_TIER2_DEG; break;
        case 3: Data.LeanAngleDeg = LEAN_ANGLE_TIER3_DEG; break;
        default: Data.LeanAngleDeg = LEAN_ANGLE_TIER4_DEG; break;
    }
    Data.ChargeIntensity = FMath::Lerp(1.0f, LEAN_BRIGHTNESS_PEAK_RATIO, Wave.LeanProgress);
    OnLeanProgress.Broadcast(Data);

    if (Wave.LeanProgress >= 1.0f)
    {
        TransitionTo(Wave, EPullWaveState::Traversing);
        Wave.TraverseElapsedS = 0.0f;  // AC-PW-12: initialized at TRAVERSING entry
    }
}

// ---------------------------------------------------------------------------
// AdvanceTraversing — accumulate elapsed, evaluate F-TRAJ-*, update ISMC.
//
// TraverseElapsedS is the canonical time source (AC-PW-22b pattern 5a):
//   TraverseElapsedS += DeltaTime   ← accumulator (CORRECT)
//   NEVER: world_z from GameTime − SpawnTimeS  ← FORBIDDEN
// ---------------------------------------------------------------------------
void APullWaveSubsystemActor::AdvanceTraversing(FPullWaveInstanceState& Wave, float DeltaTime)
{
    Wave.TraverseElapsedS += DeltaTime;  // accumulator — NOT GameTime - SpawnTimeS (FORBID)

    const float TNorm  = ComputeTNorm(Wave.TraverseElapsedS, Wave.TravelDurationS);
    const float WorldX = ComputeWorldX(Wave.SourceLane, Wave.TargetLane, TNorm, Wave.CurveSnapshot);
    const float WorldZ = ComputeWorldZ(TNorm);

    // Update ISMC instance transform (bWorldSpace=true; defer dirty — batched after loop).
    if (WaveMassISMC && Wave.ISMCInstanceIndex >= 0)
    {
        const FTransform NewTransform(
            FRotator::ZeroRotator,
            FVector(0.0f, WorldX, WorldZ),
            FVector::OneVector);
        WaveMassISMC->UpdateInstanceTransform(Wave.ISMCInstanceIndex, NewTransform,
            /*bWorldSpace=*/true, /*bMarkRenderStateDirty=*/false, /*bTeleport=*/false);
    }

    // Threshold-cross: transition to LANDED when wave reaches the player plane.
    // ResolveLandedEntry must fire same tick as the transition (AC-PW-13).
    if (WorldZ <= PLAYER_PLANE_Z)
    {
        TransitionTo(Wave, EPullWaveState::Landed);
        ResolveLandedEntry(Wave);
    }
}

// ---------------------------------------------------------------------------
// F-TRAJ-* formula helpers (ADR-0010 D2 / Story 004)
// Static: callable from integration tests without an actor instance.
// ---------------------------------------------------------------------------

/* static */ float APullWaveSubsystemActor::ComputeTNorm(
    float TraverseElapsedS, float TravelDurationS)
{
    return FMath::Clamp(
        TraverseElapsedS / FMath::Max(TravelDurationS, KINDA_SMALL_NUMBER),
        0.0f, 1.0f);
}

/* static */ float APullWaveSubsystemActor::ComputeWorldX(
    int32 SourceLane, int32 TargetLane, float TNorm,
    const FPullWaveCurveSnapshot& Snapshot)
{
    const float SourceLaneX   = static_cast<float>(SourceLane - 2) * LANE_WIDTH_M;
    const float LateralOffset = Snapshot.EvaluateAt(TNorm)
                                * static_cast<float>(TargetLane - SourceLane)
                                * LANE_WIDTH_M;
    return SourceLaneX + LateralOffset;
}

/* static */ float APullWaveSubsystemActor::ComputeWorldZ(float TNorm)
{
    return SPAWN_PLANE_Z_OFFSET_M * (1.0f - TNorm);  // F-TRAJ-FORWARD
}

// ---------------------------------------------------------------------------
// IsTransitionAllowed — 25-cell transition table (ADR-0010 D2).
//
// 6 legal transitions:
//   SPAWNED     → LEANING      (normal lifecycle entry)
//   LEANING     → TRAVERSING   (normal lifecycle)
//   LEANING     → DESPAWNING   (pause-flush / run-termination)
//   TRAVERSING  → LANDED       (threshold-cross at player plane)
//   TRAVERSING  → DESPAWNING   (pause-flush / run-termination)
//   LANDED      → DESPAWNING   (WAVE_DESPAWN_HOLD_S elapsed / natural landing)
//
// 14 forbidden transitions (all others, including all self-transitions):
//   SPAWNED     → TRAVERSING, LANDED, DESPAWNING
//   LEANING     → SPAWNED, LANDED
//   TRAVERSING  → SPAWNED, LEANING
//   LANDED      → SPAWNED, LEANING, TRAVERSING
//   DESPAWNING  → SPAWNED, LEANING, TRAVERSING, LANDED (terminal state)
//   All self-transitions (same→same)
//
// Static: callable from unit tests without an actor instance (Story 003
// Implementation Notes — "must be a constexpr or static function").
// ---------------------------------------------------------------------------
/* static */ bool APullWaveSubsystemActor::IsTransitionAllowed(
    EPullWaveState From, EPullWaveState To)
{
    // Self-transitions are always forbidden.
    if (From == To)
    {
        return false;
    }

    switch (From)
    {
        case EPullWaveState::Spawned:
            // SPAWNED exits only to LEANING (AC-PW-11).
            return To == EPullWaveState::Leaning;

        case EPullWaveState::Leaning:
            // LEANING → TRAVERSING (AC-PW-12) or DESPAWNING (pause-flush/run-term).
            return (To == EPullWaveState::Traversing || To == EPullWaveState::Despawning);

        case EPullWaveState::Traversing:
            // TRAVERSING → LANDED (AC-PW-13) or DESPAWNING (pause-flush/run-term).
            return (To == EPullWaveState::Landed || To == EPullWaveState::Despawning);

        case EPullWaveState::Landed:
            // LANDED → DESPAWNING only (AC-PW-14, after WAVE_DESPAWN_HOLD_S).
            return To == EPullWaveState::Despawning;

        case EPullWaveState::Despawning:
            // DESPAWNING is terminal — no exit (ADR-0010 D2).
            return false;

        default:
            return false;
    }
}

// ---------------------------------------------------------------------------
// TransitionTo — single write site for Wave.State (ADR-0010 D2).
//
// Non-Shipping: check(bAllowed) aborts the process on any forbidden transition,
//   catching misuse during development.
// Shipping-safe: if (!bAllowed) logs pull_wave_illegal_transition Error-level
//   telemetry and returns without modifying state — no crash in production
//   (ADR-0010 Risks table — Shipping-Safety Enforcement Policy).
//
// Direct assignment (Wave.State = X) outside this function is forbidden.
// ---------------------------------------------------------------------------
void APullWaveSubsystemActor::TransitionTo(
    FPullWaveInstanceState& Wave, EPullWaveState NewState)
{
    const bool bAllowed = IsTransitionAllowed(Wave.State, NewState);

    // Fires in Debug/Development only; gated out of UE_BUILD_TEST so automation
    // tests can exercise the Shipping-safe guard path (TC10, PullWaveStateMachineTest).
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    check(bAllowed);
#endif

    // Shipping-safe guard — runs in all build configurations.
    if (!bAllowed)
    {
        UE_LOG(LogPullWave, Error,
            TEXT("pull_wave_illegal_transition: WaveId=%d %s→%s"),
            Wave.WaveId,
            PullWaveStateToString(Wave.State),
            PullWaveStateToString(NewState));
        return;  // No-op: Wave.State is NOT modified.
    }

    Wave.State = NewState;
}

// ---------------------------------------------------------------------------
// ResolveLandedEntry — collision outcome determination at TRAVERSING→LANDED tick.
//
// Called inline from AdvanceTraversing immediately after TransitionTo(LANDED) so
// CollisionOutcome is set in the same tick as the state transition (AC-PW-13).
//
// Collision logic (ADR-0010 D2 / Story 005):
//   Hit-SETTLED:  PM in SETTLED + currentLane == TargetLane
//   Hit-SLIPPING: PM in SLIPPING + targetLane == TargetLane (heading into the wave)
//   NearMiss:     PM in SLIPPING + currentLane == TargetLane (slipping away from wave)
//   CleanMiss:    all other cases (or no PMProvider)
//
// TriggerNearMissBeat() fires BEFORE OnNearMiss broadcast (control manifest order).
// ---------------------------------------------------------------------------
void APullWaveSubsystemActor::ResolveLandedEntry(FPullWaveInstanceState& Wave)
{
    // Clamp elapsed to travel duration so ISMC transform is stable at threshold.
    Wave.TraverseElapsedS = FMath::Min(Wave.TraverseElapsedS, Wave.TravelDurationS);

    if (!PMProvider)
    {
        Wave.CollisionOutcome = ECollisionOutcome::CleanMiss;
        return;
    }

    const int32          CurrentLane  = static_cast<int32>(PMProvider->GetCurrentLane());
    const int32          PMTargetLane = static_cast<int32>(PMProvider->GetTargetLane());
    const EMovementState MovState     = PMProvider->GetMovementState();

    const bool bHitSettled  = (MovState == EMovementState::SETTLED  && CurrentLane  == Wave.TargetLane);
    const bool bHitSlipping = (MovState == EMovementState::SLIPPING && PMTargetLane == Wave.TargetLane);
    const bool bNearMiss    = (MovState == EMovementState::SLIPPING && CurrentLane  == Wave.TargetLane);

    if (bHitSettled || bHitSlipping)
    {
        Wave.CollisionOutcome = ECollisionOutcome::Hit;
        // Subscribers must not Add() to ActiveWaves during broadcast (firing inside
        // indexed Tick loop — Add past reserve would reallocate and invalidate Wave&).
        ensure(ActiveWaves.GetSlack() > 0);
        OnWaveHit.Broadcast(Wave.WaveId, Wave.TargetLane, Wave.SourceLane, Wave.SpawnTimeS);
    }
    else if (bNearMiss)
    {
        Wave.CollisionOutcome = ECollisionOutcome::NearMiss;
        PMProvider->TriggerNearMissBeat();  // BEFORE broadcast (control manifest order)
        ensure(ActiveWaves.GetSlack() > 0);
        OnNearMiss.Broadcast(Wave.WaveId, Wave.TargetLane, CurrentLane);
    }
    else
    {
        Wave.CollisionOutcome = ECollisionOutcome::CleanMiss;
    }
}

// ---------------------------------------------------------------------------
// AdvanceLanded — hold timer accumulation; LANDED → DESPAWNING after hold expires.
//
// CollisionOutcome and delegates are resolved by ResolveLandedEntry (same tick as
// the TRAVERSING→LANDED transition). This function only advances the hold timer.
// The outer pause-freeze gate in Tick() already handles LANDED — no second check here.
// ---------------------------------------------------------------------------
void APullWaveSubsystemActor::AdvanceLanded(FPullWaveInstanceState& Wave, float DeltaTime)
{
    Wave.LandedHoldElapsedS += DeltaTime;
    if (Wave.LandedHoldElapsedS >= WAVE_DESPAWN_HOLD_S)
    {
        // Story 008: if RSM has left RUNNING state, tag as RunTermination rather than NaturalLanding.
        // PauseFlush is mutually exclusive (Rule 19: flush fires only when RSM==RUNNING; once DEAD/
        // COMPLETE/ABORTED, bRunTerminated=true and the flush batch is always skipped).
        Wave.PendingDespawnReason = bRunTerminated
            ? EDespawnReason::RunTermination
            : EDespawnReason::NaturalLanding;
        TransitionTo(Wave, EPullWaveState::Despawning);
    }
}

// ---------------------------------------------------------------------------
// AdvanceDespawning — Rule 13 six-step despawn pipeline (Story 006 / ADR-0010 D4).
//
// Executes exactly once per wave, on the tick where State == DESPAWNING.
// Step ordering is fixed by Rule 13 — no reordering permitted.
//
// Step 1: Collision unregister (null-guarded; Collision epic not yet wired).
// Step 2: Telegraph unregister (null-guarded; Telegraph epic not yet wired).
// Step 3: OnWaveDespawned broadcast + Seam 13 user callback (non-Shipping only).
// Step 4: ISMC hide via PerInstanceCustomData[2]=1.0 + OnISMCHideOverride seam.
// Step 5: State clear (zero-initialize Wave; fields invalidated after this point).
// Step 6: ActiveWaves.RemoveAt(SlotIndex) — Wave& is DANGLING after this line.
//
// After this function returns, SlotIndex and Wave& must NOT be accessed by caller.
// Caller (Tick) decrements i by 1 immediately after this call (--i; break;).
// ---------------------------------------------------------------------------
void APullWaveSubsystemActor::AdvanceDespawning(
    FPullWaveInstanceState& Wave, EDespawnReason Reason, int32 SlotIndex)
{
    // Capture WaveId and ISMCInstanceIndex BEFORE step 5 clears the struct.
    const int32 WaveId         = Wave.WaveId;
    const int32 ISMCInstanceIdx = Wave.ISMCInstanceIndex;

    // Step 1: Collision subsystem unregister.
    if (CollisionProvider)
    {
        CollisionProvider->UnregisterWave(WaveId);
    }

    // Step 2: Telegraph subsystem unregister.
    if (TelegraphProvider)
    {
        TelegraphProvider->UnregisterWave(WaveId);
    }

    // Step 3: OnWaveDespawned broadcast (delegate) + Seam 13 user callback.
    // Subscribers must not Add() to ActiveWaves during broadcast — same constraint as
    // OnWaveHit/OnNearMiss. Add past reserve would reallocate, invalidating Wave& before
    // steps 5–6 fire (ADR-0010 D4; subscriber constraint documented on OnWaveDespawned).
    ensure(ActiveWaves.GetSlack() > 0);
    OnWaveDespawned.Broadcast(WaveId, Reason);
#if !UE_BUILD_SHIPPING
    if (OnDespawnedUserCallback)
    {
        OnDespawnedUserCallback(WaveId, Reason);
    }
#endif

    // Step 4: ISMC hide — set PerInstanceCustomData[2]=1.0 (dissolve signal).
    // bMarkDirty=false: render state batched once per frame after the loop (ADR-0010 D2).
    if (WaveMassISMC && ISMCInstanceIdx >= 0)
    {
        WaveMassISMC->SetCustomDataValue(ISMCInstanceIdx, 2, 1.0f, /*bMarkDirty=*/false);
    }
#if !UE_BUILD_SHIPPING
    if (OnISMCHideOverride)
    {
        OnISMCHideOverride(ISMCInstanceIdx, 2, 1.0f);
    }
#endif

    // Step 5: State clear — zero-initialize entire struct.
    // Wave& references an element of ActiveWaves that is about to be removed.
    // After this line, all fields of Wave are default-initialized; the struct is inert.
    Wave = FPullWaveInstanceState{};

#if !UE_BUILD_SHIPPING
    // Step 5 seam: fire AFTER clear, BEFORE RemoveAt — gives tests a view of the
    // zero-initialized struct to assert field-level state clear (AC story-006 step 5).
    if (OnStateClearOverride)
    {
        OnStateClearOverride(Wave);
    }
#endif

    // Step 6: Pool slot return — O(N) shift; preserves WaveId ASC ordering.
    // After RemoveAt, Wave& is DANGLING. Do not access Wave after this point.
    // RemoveAtSwap is FORBIDDEN (AC-PW-22b pattern 9; breaks WaveId ASC invariant).
    ActiveWaves.RemoveAt(SlotIndex);
}

// ---------------------------------------------------------------------------
// OnRSMRunStateChanged — run-termination handler (Story 008 / ADR-0010 D2).
//
// Bound at BeginPlay to RSM.OnRunStateChanged (production).
// Called directly by integration tests as a test seam (no real RSM delegate).
//
// Semantics: run-termination uses DRAIN, not FLUSH.
//   - bRunTerminated=true means subsequent AdvanceLanded() calls assign RunTermination reason.
//   - Waves already in LEANING/TRAVERSING/LANDED continue advancing normally every tick.
//   - The 6-step despawn pipeline (Story 006) runs at DESPAWNING entry with the correct reason.
//
// This is the explicit OPPOSITE of PauseFlush: PauseFlush immediately routes waves to
// DESPAWNING; run-termination lets them drain at their own cadence (ADR-0010 D2 distinction).
// ---------------------------------------------------------------------------
void APullWaveSubsystemActor::OnRSMRunStateChanged(ERunState NewState)
{
    if (NewState == ERunState::DEAD     ||
        NewState == ERunState::COMPLETE ||
        NewState == ERunState::ABORTED)
    {
        // Flag all future DESPAWNING entries as RunTermination.
        // Do NOT walk or mutate ActiveWaves here — drain semantics require no immediate flush.
        bRunTerminated = true;
    }
    // IDLE, COUNTDOWN, RUNNING, RESOLVING: no action.
}

// ---------------------------------------------------------------------------
// OnRSMPausedChanged — pause-boundary event handler (Story 007 / ADR-0010 D2 R7 B13).
//
// Bound at BeginPlay to RSM.OnPausedChanged via AddUObject (production).
// Also called directly by integration tests as a test seam (no real RSM delegate).
//
// IMPORTANT: Must NOT walk ActiveWaves inline — doing so mid-tick would interleave
// this tick's per-wave events with DESPAWNING-entry events for the flushed waves,
// violating AC-PW-15 contiguity (ADR-0010 Alternative 3 rejection).
// Instead, set the flag; flush executes at the TOP of the NEXT tick.
// ---------------------------------------------------------------------------
void APullWaveSubsystemActor::OnRSMPausedChanged(bool bNewIsPaused)
{
    if (!bNewIsPaused)
    {
        // Run ended while paused — queue flush for next tick's top-of-tick check.
        // bPauseFlushPending is consumed and cleared in Tick() before the per-wave loop.
        bPauseFlushPending = true;
    }
    // bNewIsPaused == true (normal pause during live run): no action.
    // Pause-freeze (Story 004, bIsPaused gate in Tick) handles mid-run paused state.
}

// ---------------------------------------------------------------------------
// EndPlay — delegate cleanup (Story 007 / ADR-0010 D2).
//
// Resets the FDelegateHandle that was stored when binding to RSM.OnPausedChanged.
// In test contexts PausedChangedHandle.IsValid() == false (handler called directly);
// Reset() is a no-op in that case — safe to call unconditionally.
//
// In production, the real Remove() call would be:
//   if (RSMSubsystem && PausedChangedHandle.IsValid())
//       RSMSubsystem->OnPausedChanged.Remove(PausedChangedHandle);
// (Deferred to RSM integration story when RSMSubsystem UObject is wired.)
// ---------------------------------------------------------------------------
void APullWaveSubsystemActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // TODO(story-RSM-integration): Add Remove() calls here BEFORE Reset() when AddUObject bindings
    // are wired at BeginPlay. Pattern for each handle:
    //   if (RSMSubsystem && RunStateChangedHandle.IsValid())
    //       RSMSubsystem->OnRunStateChanged.Remove(RunStateChangedHandle);
    //   if (RSMSubsystem && PausedChangedHandle.IsValid())
    //       RSMSubsystem->OnPausedChanged.Remove(PausedChangedHandle);
    // Without Remove(), Reset() only invalidates the local handle ID — the callback remains
    // registered on the RSM delegate and fires as a dangling pointer after this actor is destroyed.
    RunStateChangedHandle.Reset();
    PausedChangedHandle.Reset();

    Super::EndPlay(EndPlayReason);
}

// ---------------------------------------------------------------------------

#if !UE_BUILD_SHIPPING
FTraverseElapsedQuery APullWaveSubsystemActor::GetTraverseElapsedForWave(int32 WaveId) const
{
    for (const FPullWaveInstanceState& Wave : ActiveWaves)
    {
        if (Wave.WaveId == WaveId)
        {
            FTraverseElapsedQuery Result;
            Result.bWaveFound = true;
            Result.State = Wave.State;
            // Return ElapsedS only for states where TraverseElapsedS is meaningful.
            if (Wave.State == EPullWaveState::Traversing
                || Wave.State == EPullWaveState::Landed)
            {
                Result.ElapsedS = Wave.TraverseElapsedS;
            }
            // Otherwise ElapsedS stays at sentinel -1.0f (R7 B5).
            return Result;
        }
    }
    return FTraverseElapsedQuery{};  // bWaveFound = false
}
#endif // !UE_BUILD_SHIPPING

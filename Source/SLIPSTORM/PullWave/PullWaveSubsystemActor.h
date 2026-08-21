// Copyright (c) Game Studio. All rights reserved.
// SLIPSTORM — 60-second voxel endless runner, mobile, Unreal Engine 5.7
//
// PullWaveSubsystemActor.h — APullWaveSubsystemActor: renderer-host singleton
// declared by ADR-0006 as the owner of WaveMassISMC and ActiveWaves pool.
//
// Story: production/epics/pull-wave/story-002-pool-storage-waveid.md (pool/WaveId)
//        production/epics/pull-wave/story-003-state-machine.md (TransitionTo + log cat)
//        production/epics/pull-wave/story-004-tick-advance-traversing.md (tick body)
//        production/epics/pull-wave/story-006-despawn-pipeline.md (despawn pipeline)
//        production/epics/pull-wave/story-007-pause-flush.md (pause-flush batch + bPauseFlushPending)
//        production/epics/pull-wave/story-008-run-termination-drain.md (bRunTerminated flag + drain reason)
//        production/epics/pull-wave/story-009-construct-entry-point.md (Construct() + ComputeSpawnTransform)
// ADR:   docs/architecture/adr-0010-pullwave-object-pool-state-machine-despawn.md
//        docs/architecture/adr-0006-pullwave-instanced-renderer.md
//        docs/architecture/adr-0011-wave-spawner-pattern-library.md
// TRs:   TR-PW-002 (five-state machine + forbidden-transition enforcement)
//        TR-PW-003 (per-tick state advance), TR-PW-015 (TraverseElapsedS accumulator),
//        TR-PW-021 (pause-freeze gate)
//        TR-PW-004 (CurveSnapshot immutable at SPAWNED; SAMPLE_COUNT=32 locked)
//        TR-PW-005 (pool capacity locked at 23), TR-PW-006 (WaveId never reused),
//        TR-PW-010 (append-only; no mid-array Insert),
//        TR-PW-014 (ForwardVelocityMs frozen at SPAWNED; no mid-flight mutation)
//        TR-PW-016 (six-step despawn pipeline), TR-PW-017 (OnWaveDespawned delegate)
//
// Pool invariant: ActiveWaves is ordered WaveId ASC at all times.
//   - Waves appended via Add() (monotone WaveId → natural append == ASC order).
//   - Removed via RemoveAt(Index) — preserves ASC ordering via O(N) element shift.
//   - RemoveAtSwap is FORBIDDEN: breaks ASC order (ADR-0010 D1 Alternative 1 rejection).
//   - Insert at non-tail is FORBIDDEN: breaks ASC order (AC-PW-22b pattern 11).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "PullWave/PullWaveTypes.h"
#include "RunStateMachine/ERunState.h"
#include "Seam/PlayerMovementProvider.h"
#include "Seam/CollisionWaveProvider.h"
#include "Seam/TelegraphWaveProvider.h"
#include "PullWaveSubsystemActor.generated.h"

// ---------------------------------------------------------------------------
// LogPullWave — pull-wave subsystem log category (Story 003 / ADR-0010 D2).
// Declared here so TransitionTo() and tick body can both use it.
// DEFINE_LOG_CATEGORY counterpart is in PullWaveSubsystemActor.cpp.
// ---------------------------------------------------------------------------
DECLARE_LOG_CATEGORY_EXTERN(LogPullWave, Log, All);

// ---------------------------------------------------------------------------
// IPullWaveRSMProvider — RSM query interface for Pull-Wave (Stories 004 + 007).
//
// Story 004: GetIsPaused() — pause-freeze gate; skips per-wave advance when paused.
// Story 007: GetCurrentState() — consumed at pause-flush top-of-tick to distinguish
//   RUNNING (flush all waves) from ABORTED/DEAD/etc. (skip flush per Rule 19).
//
// Tests inject stubs via SetRSMProvider(). Production adapter wired by RSM epic.
// ---------------------------------------------------------------------------
class IPullWaveRSMProvider
{
public:
    virtual ~IPullWaveRSMProvider() = default;

    /** Returns true when the run is in a paused state (pause-freeze gate / Story 004). */
    virtual bool GetIsPaused() const = 0;

    /**
     * Returns the current RSM run-state enum value.
     * Consumed at bPauseFlushPending consumption tick to check for ERunState::RUNNING
     * before flushing (Rule 19 — only RUNNING triggers the flush; Story 007 / ADR-0010 D2).
     */
    virtual ERunState GetCurrentState() const = 0;
};

/**
 * APullWaveSubsystemActor
 *
 * Singleton actor (placed in level, declared by ADR-0006) that owns:
 *   - ActiveWaves: the 23-slot object pool of per-wave runtime state.
 *   - NextWaveId: monotonically increasing counter; never reused within a session.
 *   - WaveMassISMC: ISMC rendering the wave geometry (ADR-0006).
 *
 * Capacity policy (ADR-0010 D1):
 *   16 (MAX_CONCURRENT_WAVES_CAP) + 2 (DESPAWNING_RETURN_LATENCY_SLOTS)
 *   + 5 (MAX_CONCURRENT_WAVES_CAP_CEIL_MARGIN) = 23.
 *   Reserve(23) called at BeginPlay; no heap reallocation permitted during a run.
 *
 * State machine (ADR-0010 D2 / Story 003):
 *   Transitions enforced via TransitionTo(). Direct State assignment outside
 *   TransitionTo() is forbidden. IsTransitionAllowed() encodes the 25-cell
 *   transition table as a static lookup; callable in test contexts without an actor.
 *
 * Tick body (ADR-0010 D2 / Story 004):
 *   Iterates ActiveWaves WaveId ASC. Pause-freeze gate fires before state dispatch.
 *   SPAWNED→LEANING (one-tick init), LEANING accumulates LeanProgress,
 *   TRAVERSING evaluates F-TRAJ-* formulas via static helpers and updates ISMC.
 */
UCLASS()
class SLIPSTORM_API APullWaveSubsystemActor : public AActor
{
    GENERATED_BODY()

public:
    APullWaveSubsystemActor();

    virtual void Tick(float DeltaTime) override;

    /**
     * Run-state-changed handler — bound to RSM.OnRunStateChanged delegate at BeginPlay.
     * When NewState ∈ {DEAD, COMPLETE, ABORTED}, sets bRunTerminated=true so that all
     * subsequent DESPAWNING entries assign DespawnReason=RunTermination instead of NaturalLanding.
     *
     * Does NOT immediately flush waves — run-termination uses drain semantics (waves continue
     * their normal lifecycle until DESPAWNING). Immediate flush is PauseFlush semantics (Story 007).
     *
     * Also exposed as a direct-call test seam — integration tests call this method
     * directly instead of binding via a real RSM delegate (Story 008 / ADR-0010 D2).
     */
    void OnRSMRunStateChanged(ERunState NewState);

    /**
     * Pause-changed handler — bound to RSM.OnPausedChanged delegate at BeginPlay
     * (AddUObject per ADR-0009 IG-3 rule; FDelegateHandle stored for EndPlay cleanup).
     *
     * When bNewIsPaused is false (run was aborted while paused), sets bPauseFlushPending=true
     * synchronously WITHOUT walking ActiveWaves — inline-flush is explicitly rejected
     * (ADR-0010 Alternative 3; AC-PW-MID-TICK-PAUSE-DEFERRAL). The actual flush executes
     * at the TOP of the next Pull-Wave tick, before the per-wave iteration loop.
     *
     * Also exposed as a direct-call test seam — integration tests call this method
     * directly instead of using a real RSM delegate (Story 007 / ADR-0010 D2 R7 B13).
     */
    void OnRSMPausedChanged(bool bNewIsPaused);

    /**
     * Allocates pool capacity and resets the WaveId counter for the session.
     * Called internally by BeginPlay(). Also exposed as a public seam for unit
     * tests that call NewObject<APullWaveSubsystemActor>() without a UWorld
     * (ADR-0010 D1; Story 002 test-seam requirement).
     */
    void InitializePool();

    /**
     * Sole admission gateway from Wave Spawner into the Pull-Wave pool (ADR-0010 D6;
     * ADR-0011 D2 Stage 6; Story 009 / TR-PW-004, TR-PW-014).
     *
     * Steps (ADR-0010 D6 Implementation Guidelines):
     *   1. Pool-full guard: if Num() >= MAX_POOL_SIZE, log pull_wave_pool_full + return.
     *   2. Acquire pool slot via Add_GetRef (append-to-tail; WaveId ASC ordering maintained
     *      because Wave Spawner assigns WaveIds monotonically — ADR-0011 D2 Stage 6).
     *   3. Copy FPullWaveSpawnParams fields field-by-field into FPullWaveInstanceState.
     *   4. Set Wave.State = EPullWaveState::Spawned (direct assignment — this is initial
     *      construction, not a lifecycle transition; TransitionTo() guards subsequent changes).
     *   5. Initialize accumulators: LeanProgress=0, TraverseElapsedS=0,
     *      LandedHoldElapsedS=0, CollisionOutcome=Unresolved.
     *   6. Compute TravelDurationS = SPAWN_PLANE_Z_OFFSET_M / ForwardVelocityMs (F-TRAVERSE-DURATION).
     *   7. Register ISMC instance at ComputeSpawnTransform(SourceLane); init PerInstanceCustomData[0..2]=0.
     *
     * After Construct() returns, SpawnParams is no longer referenced — immutability contract closed
     * (TR-PW-004; CurveSnapshot is a 128-byte value copy, not a pointer into the source asset).
     *
     * Note: Wave Spawner assigns WaveId from its own monotonic counter (ADR-0011 D2 Stage 6).
     * Construct() does NOT read or increment NextWaveId — that field is reserved for future
     * Wave Spawner integration; the comment on it is currently stale (Story 009 / ADR-0011).
     *
     * Seams (non-Shipping only): OnISMCAddInstanceOverride / OnISMCConstructDataOverride
     * intercept the ISMC calls so integration tests run without a real WaveMassISMC.
     */
    void Construct(FPullWaveSpawnParams SpawnParams);

    /**
     * Returns true if the transition From→To is permitted by the 25-cell table
     * (ADR-0010 D2). Static so test code can call it without an actor instance.
     *
     * Transition table — 6 legal (✅), 14 forbidden (❌):
     *   SPAWNED     → LEANING      ✅
     *   LEANING     → TRAVERSING   ✅
     *   LEANING     → DESPAWNING   ✅ (pause-flush / run-termination)
     *   TRAVERSING  → LANDED       ✅
     *   TRAVERSING  → DESPAWNING   ✅ (pause-flush / run-termination)
     *   LANDED      → DESPAWNING   ✅
     *   All other inter-state pairs ❌
     *   Self-transitions (same→same) ❌
     */
    static bool IsTransitionAllowed(EPullWaveState From, EPullWaveState To);

    /**
     * Enforces the state transition From Wave.State → NewState.
     *
     * Non-Shipping: check(bAllowed) aborts on forbidden transition.
     * Shipping-safe: if (!bAllowed) logs pull_wave_illegal_transition telemetry
     *   and returns without modifying Wave.State (ADR-0010 Risks table —
     *   Shipping-Safety Enforcement Policy).
     *
     * This is the ONLY permitted write site for Wave.State, with ONE exception:
     * Construct() assigns Wave.State = EPullWaveState::Spawned directly at initial
     * construction because no "from" state exists at pool-entry time (ADR-0010 D6;
     * Story 009). All subsequent state changes MUST route through TransitionTo().
     * Direct assignment (Wave.State = X) outside Construct() and this function is
     * forbidden (Story 003 / ADR-0010 D2 Implementation Guidelines).
     */
    void TransitionTo(FPullWaveInstanceState& Wave, EPullWaveState NewState);

    /**
     * Inject an RSM provider for testing pause-freeze (AC-PW-16).
     * Story 007 wires the real RSM adapter in BeginPlay.
     *
     * Teardown contract: the caller MUST call SetRSMProvider(nullptr) in EndPlay
     * before releasing the provider. This actor holds a raw pointer with no
     * ownership semantics — non-UObject, not GC-tracked.
     */
    void SetRSMProvider(IPullWaveRSMProvider* Provider);

    /**
     * Inject a PM provider for collision outcome resolution (Story 005 / AC-PW-21/22).
     * Seam 12 — null by default; CleanMiss assigned when no provider is wired.
     *
     * Teardown contract: the caller MUST call SetPMProvider(nullptr) in EndPlay
     * before releasing the provider. This actor holds a raw pointer with no
     * ownership semantics — non-UObject, not GC-tracked.
     */
    void SetPMProvider(IPlayerMovementProvider* Provider);

    /**
     * Inject a Collision provider for DESPAWNING entry step 1 (Story 006 / Rule 13).
     * Null by default → step 1 silently skipped (Collision epic not yet wired).
     *
     * Teardown contract: caller MUST call SetCollisionProvider(nullptr) in EndPlay.
     * Raw pointer; no ownership semantics — non-UObject, not GC-tracked.
     */
    void SetCollisionProvider(ICollisionWaveProvider* Provider);

    /**
     * Inject a Telegraph provider for DESPAWNING entry step 2 (Story 006 / Rule 13).
     * Null by default → step 2 silently skipped (Telegraph epic not yet wired).
     *
     * Teardown contract: caller MUST call SetTelegraphProvider(nullptr) in EndPlay.
     * Raw pointer; no ownership semantics — non-UObject, not GC-tracked.
     */
    void SetTelegraphProvider(ITelegraphWaveProvider* Provider);

    // -----------------------------------------------------------------------
    // F-TRAJ-* formula helpers (Story 004 / ADR-0010 D2)
    //
    // Static: callable by tests without an actor instance.
    // Used by AdvanceTraversing() and verified directly in integration tests.
    // -----------------------------------------------------------------------

    /**
     * F-TRAJ-TNORM: normalized traversal time in [0, 1].
     * t_norm = clamp(TraverseElapsedS / TravelDurationS, 0, 1).
     * Accumulator-based (AC-PW-22b pattern 5a) — never GameTime − SpawnTimeS.
     */
    static float ComputeTNorm(float TraverseElapsedS, float TravelDurationS);

    /**
     * F-TRAJ-LATERAL: lateral world-space X position.
     * world_x = (SourceLane−2)×LANE_WIDTH_M + CurveSnapshot.EvaluateAt(TNorm)×(TargetLane−SourceLane)×LANE_WIDTH_M.
     */
    static float ComputeWorldX(int32 SourceLane, int32 TargetLane, float TNorm,
                                const FPullWaveCurveSnapshot& Snapshot);

    /**
     * F-TRAJ-FORWARD: forward-axis world-space Z position.
     * world_z = SPAWN_PLANE_Z_OFFSET_M × (1 − TNorm).
     * At TNorm=0: world_z=SPAWN_PLANE_Z_OFFSET_M (spawn plane).
     * At TNorm=1: world_z=0 (player plane / PLAYER_PLANE_Z).
     */
    static float ComputeWorldZ(float TNorm);

#if !UE_BUILD_SHIPPING
    /**
     * Test seam: query traverse elapsed state for a wave by WaveId.
     * Returns bWaveFound=false if no matching wave exists.
     * ElapsedS sentinel is -1.0f when State is not TRAVERSING or LANDED (R7 B5).
     */
    FTraverseElapsedQuery GetTraverseElapsedForWave(int32 WaveId) const;
#endif // !UE_BUILD_SHIPPING

    // -----------------------------------------------------------------------
    // Multicast delegates (ADR-0010 D2 / Rule 12).
    // -----------------------------------------------------------------------

    /** Fired each LEANING tick — Telegraph binds here to drive visual lean feedback. */
    FOnLeanProgressDelegate OnLeanProgress;

    /**
     * Fired once at LANDED entry when CollisionOutcome == Hit (Story 005 / AC-PW-21).
     *
     * Subscriber constraint: listeners MUST NOT mutate ActiveWaves synchronously
     * (no Add / RemoveAt / RemoveAtSwap). This broadcast fires inside the ActiveWaves
     * indexed Tick loop — synchronous Add past reserve would reallocate and invalidate
     * the current Wave& reference; RemoveAt shifts elements and corrupts iteration.
     * Defer all pool mutations to the next tick.
     */
    FOnWaveHit OnWaveHit;

    /**
     * Fired once at LANDED entry when CollisionOutcome == NearMiss (Story 005 / AC-PW-22).
     *
     * Subscriber constraint: same as OnWaveHit — no synchronous ActiveWaves mutation.
     * See OnWaveHit doc for full constraint.
     */
    FOnNearMiss OnNearMiss;

    /**
     * Fired once at DESPAWNING entry per wave (Story 006 / ADR-0010 D4 / Rule 13 step 3).
     * Carries WaveId and EDespawnReason so subscribers can distinguish lifecycle paths.
     *
     * Subscriber constraint: same as OnWaveHit — no synchronous ActiveWaves mutation.
     */
    FOnWaveDespawned OnWaveDespawned;

#if !UE_BUILD_SHIPPING
    /**
     * Seam 13 test callback slot (R10c / Story 006 / AC-PW-S006).
     * Routes step 3's broadcast to a test stub capture slot.
     * Set via SetOnDespawnedUserCallback(); cleared by passing nullptr-equivalent lambda.
     */
    TFunction<void(int32 /*WaveId*/, EDespawnReason)> OnDespawnedUserCallback;
    void SetOnDespawnedUserCallback(TFunction<void(int32, EDespawnReason)> Callback)
        { OnDespawnedUserCallback = MoveTemp(Callback); }

    /**
     * Seam 13 ISMC-hide override (step 4 test seam).
     * When set, called instead of (in addition to) the null-guarded WaveMassISMC call.
     * Allows tests to observe the PerInstanceCustomData[2]=1.0 signal without a real ISMC.
     */
    TFunction<void(int32 /*InstanceIdx*/, int32 /*DataIdx*/, float /*Value*/)> OnISMCHideOverride;
    void SetOnISMCHideOverride(TFunction<void(int32, int32, float)> Callback)
        { OnISMCHideOverride = MoveTemp(Callback); }

    /**
     * Step 5 test seam — fires after Wave = FPullWaveInstanceState{} (zero-init) but BEFORE RemoveAt.
     * Receives the cleared struct by const-ref so tests can assert individual field values.
     * Enables TC5 to directly verify the state-clear AC without requiring a multi-tick harness.
     */
    TFunction<void(const FPullWaveInstanceState& /*ClearedWave*/)> OnStateClearOverride;
    void SetOnStateClearOverride(TFunction<void(const FPullWaveInstanceState&)> Callback)
        { OnStateClearOverride = MoveTemp(Callback); }

    /**
     * Seam for Construct()'s ISMC AddInstance call in test context (Story 009).
     * When set, called INSTEAD of WaveMassISMC->AddInstance(); returns the fake ISMCInstanceIndex.
     * If not set and WaveMassISMC is null, ISMCInstanceIndex = -1 (no-ISMC test path).
     * If not set and WaveMassISMC is non-null, the real AddInstance call fires (production path).
     */
    TFunction<int32(const FTransform& /*InitialTransform*/)> OnISMCAddInstanceOverride;
    void SetOnISMCAddInstanceOverride(TFunction<int32(const FTransform&)> Callback)
        { OnISMCAddInstanceOverride = MoveTemp(Callback); }

    /**
     * Seam for Construct()'s SetCustomDataValue calls for slots 0, 1, 2 (Story 009).
     * Fires INSTEAD of the real ISMC SetCustomDataValue calls when WaveMassISMC is null.
     * Parameters: InstanceIdx (ISMCInstanceIndex), DataIdx (0/1/2), Value (always 0.0f at construction).
     * Called three times in sequence: DataIdx=0 (lean tier), 1 (near-miss flash), 2 (dissolve).
     */
    TFunction<void(int32 /*InstanceIdx*/, int32 /*DataIdx*/, float /*Value*/)> OnISMCConstructDataOverride;
    void SetOnISMCConstructDataOverride(TFunction<void(int32, int32, float)> Callback)
        { OnISMCConstructDataOverride = MoveTemp(Callback); }
#endif // !UE_BUILD_SHIPPING

protected:
    virtual void BeginPlay() override;

    /**
     * Cleans up the RSM.OnPausedChanged delegate binding (if any) before destruction.
     * In production: PausedChangedHandle is removed from RSM's OnPausedChanged delegate.
     * In test contexts: PausedChangedHandle.IsValid() == false (tests call handler directly).
     */
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
    /**
     * Per-wave runtime state pool.
     *
     * Invariant: entries ordered WaveId ASC at all times.
     * Capacity: Reserve(23) called at BeginPlay; no dynamic resize during a run.
     * Write rules:
     *   - Append via Add() only (Story 009 Construct entry point).
     *   - Remove via RemoveAt(Index) only (Story 006 despawn pipeline).
     *   - RemoveAtSwap is FORBIDDEN (AC-PW-22b pattern 9 FORBID).
     *   - Insert at non-tail is FORBIDDEN (AC-PW-22b pattern 11 FORBID).
     */
    UPROPERTY()
    TArray<FPullWaveInstanceState> ActiveWaves;

    /**
     * Monotonically increasing WaveId counter (reserved for Wave Spawner integration).
     *
     * Initialized to 0 at BeginPlay (resets each session; never reused within session
     * per Rule 14 / TR-PW-006). NOT incremented by Construct() — WaveId is assigned by
     * the Wave Spawner from its own counter and transmitted via FPullWaveSpawnParams
     * (ADR-0011 D2 Stage 6; Story 009). Read-only after InitializePool(); write site
     * will be the Wave Spawner integration epic.
     */
    int32 NextWaveId = 0;

    /**
     * ISMC that renders all active wave geometry (ADR-0006).
     * Null in unit-test contexts (NewObject without UWorld). All ISMC call sites
     * are null-guarded. Dirty batched: MarkRenderStateDirty() called once per
     * frame after the ActiveWaves loop, not per wave (Story 004 / ADR-0010 D2).
     */
    UPROPERTY()
    TObjectPtr<UInstancedStaticMeshComponent> WaveMassISMC = nullptr;

private:
    /**
     * RSM query provider — pause state + run state (Stories 004 + 007).
     * Null → GetIsPaused() returns false, GetCurrentState() not reached (bPauseFlushPending guard).
     * Set via SetRSMProvider(); wired by production RSM epic adapter at BeginPlay.
     * Teardown contract: caller MUST call SetRSMProvider(nullptr) before releasing.
     */
    IPullWaveRSMProvider* RSMProvider = nullptr;

    /**
     * True after RSM has left the RUNNING state (DEAD / COMPLETE / ABORTED).
     * Set by OnRSMRunStateChanged(); reset at InitializePool() / BeginPlay for each session.
     * When true, AdvanceLanded() assigns DespawnReason=RunTermination instead of NaturalLanding.
     * Not UPROPERTY: transient runtime flag; not inspector-visible.
     *
     * Drain semantics: bRunTerminated does NOT trigger an immediate flush — waves continue
     * their normal lifecycle and despawn naturally at their own cadence.
     */
    bool bRunTerminated = false;

    /**
     * Stored delegate handle for RSM.OnRunStateChanged subscription (production).
     * Reset() called at EndPlay to prevent dangling callbacks.
     * Invalid in test contexts — tests call OnRSMRunStateChanged() directly.
     * TODO(story-RSM-integration): Add Remove() before Reset() when AddUObject is wired.
     */
    FDelegateHandle RunStateChangedHandle;

    /**
     * Stored delegate handle for RSM.OnPausedChanged subscription (production).
     * Reset() called at EndPlay to prevent dangling callbacks after the actor is destroyed.
     * Remains invalid (.IsValid() == false) in test contexts — tests call OnRSMPausedChanged
     * directly as a seam (no real delegate registered; no Remove() call needed at teardown).
     */
    FDelegateHandle PausedChangedHandle;

    /**
     * True when a pause-flush batch is queued for the next tick's top-of-tick check.
     *
     * Set synchronously in OnRSMPausedChanged(false) — NEVER in the per-wave loop body.
     * Consumed at the very top of Tick (before the per-wave iteration loop) and cleared
     * to false immediately on consumption (AC-PW-MID-TICK-PAUSE-DEFERRAL guarantee).
     *
     * Forbidden: reading bPauseFlushPending inside the per-wave iteration loop — it is
     * consumed once and only once per tick at top-of-tick (control manifest rule).
     * Not UPROPERTY: programmer-only runtime state; not inspector-visible.
     */
    bool bPauseFlushPending = false;

    /** PM provider for lane/movement queries at LANDED entry. Null → CleanMiss default. */
    IPlayerMovementProvider* PMProvider = nullptr;

    /** Collision provider for DESPAWNING step 1. Null → step 1 silently skipped. */
    ICollisionWaveProvider* CollisionProvider = nullptr;

    /** Telegraph provider for DESPAWNING step 2. Null → step 2 silently skipped. */
    ITelegraphWaveProvider* TelegraphProvider = nullptr;

    /** SPAWNED advance: one-tick init; transitions to LEANING (ADR-0010 D2 / D6). */
    void AdvanceSpawned(FPullWaveInstanceState& Wave);

    /** LEANING advance: accumulate LeanProgress, broadcast OnLeanProgress, check transition. */
    void AdvanceLeaning(FPullWaveInstanceState& Wave, float DeltaTime);

    /** TRAVERSING advance: accumulate TraverseElapsedS, evaluate F-TRAJ-*, update ISMC. */
    void AdvanceTraversing(FPullWaveInstanceState& Wave, float DeltaTime);

    /**
     * Called inline from AdvanceTraversing at the threshold-cross tick, immediately
     * after TransitionTo(LANDED). Sets CollisionOutcome and fires OnWaveHit / OnNearMiss.
     * Must run same tick as the TRAVERSING→LANDED transition (AC-PW-13).
     */
    void ResolveLandedEntry(FPullWaveInstanceState& Wave);

    /**
     * LANDED advance: accumulate hold timer; transition to DESPAWNING when
     * LandedHoldElapsedS >= WAVE_DESPAWN_HOLD_S (AC-PW-14).
     * Called from Tick starting the tick AFTER ResolveLandedEntry fires.
     */
    void AdvanceLanded(FPullWaveInstanceState& Wave, float DeltaTime);

    /**
     * DESPAWNING entry body — executes Rule 13 six-step pipeline exactly once per wave
     * (Story 006 / ADR-0010 D4). Called from Tick when Wave.State == DESPAWNING.
     *
     * After step 6 (RemoveAt), SlotIndex is invalid and Wave& is dangling.
     * The caller (Tick) must --i immediately after this call and break.
     *
     * @param Wave       Reference to the wave in DESPAWNING state.
     * @param Reason     Reason the pipeline was triggered (NaturalLanding / PauseFlush / RunTermination).
     * @param SlotIndex  Index of Wave in ActiveWaves (for RemoveAt step 6).
     */
    void AdvanceDespawning(FPullWaveInstanceState& Wave, EDespawnReason Reason, int32 SlotIndex);

    /**
     * Computes the world-space spawn transform for a wave entering the pool (Story 009 / ADR-0010 D6).
     *
     * Places the wave at the spawn plane (Z = SPAWN_PLANE_Z_OFFSET_M) with lateral offset
     * on the X axis: X = (SourceLane - 2) × LANE_WIDTH_M  (lane 2 = X=0, lane 0 = X=-2, lane 4 = X=2).
     *
     * AXIS NOTE: This helper uses X for the lateral axis (as specified in ADR-0010 D6 / Story 009).
     * AdvanceTraversing (Story 004) currently encodes lateral in Y via FVector(0, WorldX, WorldZ).
     * The two representations agree only for SourceLane==2 (where both yield lateral=0).
     * This discrepancy is flagged for architecture review and must not be silently resolved here.
     *
     * Identity rotation and unit scale; bWorldSpace=true at AddInstance call site.
     */
    FTransform ComputeSpawnTransform(int32 SourceLane) const;
};

// Copyright (c) Game Studio. All rights reserved.
// SLIPSTORM — 60-second voxel endless runner, mobile, Unreal Engine 5.7
//
// WaveSpawnerSubsystem.h — UWaveSpawnerSubsystem class declaration.
//
// Hosted as UGameInstanceSubsystem + FTickableGameObject (ADR-0005 Decision).
// The UGameInstanceSubsystem lifecycle persists across UWorld transitions so the
// 23-actor AWave pool survives all death-replay cycles without re-allocation.
// (Per-replay pool-alloc under UWorldSubsystem would impose a 16.6ms frame hitch
// on every retry — the decisive rejection criterion for that alternative.)
//
// Key lifecycle split (ADR-0005 IG-1, R2a-4):
//   Initialize()         — subscribe delegates only; UWorld is null here; NO SpawnActor.
//   OnFirstWorldLoaded() — pre-allocate 23 AWave actors; one-shot per session.
//   OnDPCFrameReady()    — admission pipeline entry point (Stories 003–007).
//   Tick(DeltaTime)      — rate-limit timer bookkeeping only (Story 009).
//   Deinitialize()       — unsubscribe delegates; release pool GC anchors.
//
// Three-pool architecture (ADR-0011 D1):
//   FPatternPool OpenerPool / MidPool / PeakPool — populated at cook time (Story 008).
//
// ADR-0005 forbidden patterns — never add:
//   SpawnActor in Initialize()       (UWorld null; asserts in UE 5.7 — IG-1)
//   SpawnActor/Destroy at runtime    (pool is static post-first-load — IG-2)
//   ISMC members on AWave            (renderer lives on APullWaveSubsystemActor — IG-7)
//   Admission logic in Tick()        (breaks DPC structural ordering — IG-4)
//   WaveSpawner_as_UWorldSubsystem   (re-fires 16.6ms alloc hitch every retry)
//
// Story: production/epics/wave-spawner/story-001-subsystem-class-and-object-pool.md
// ADR:   docs/architecture/adr-0005-wave-spawner-subsystem-hosting.md
// TRs:   TR-WS-008, TR-WS-009, TR-WS-010, TR-WS-011

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "WaveSpawner/Wave.h"
#include "WaveSpawner/WaveSpawnerTypes.h"
#include "Seam/WaveSpawnerCallback.h"
// Story 007: RSM enum types needed for HandleRunStateChanged / HandlePausedChanged signatures
// and TestOnly_FireRunStateChanged seam. Lightweight headers (UENUM only; no UObject deps).
#include "RunStateMachine/ERunState.h"
#include "RunStateMachine/ERunOutcome.h"
#include "WaveSpawnerSubsystem.generated.h"

// Forward declarations. Full type definitions included in the .cpp.
// DPC and RSM headers required there for InitializeDependency() + delegate binding.
class URunStateMachineSubsystem;
class UDPCSubsystem;
struct FDPCFrameState;

/**
 * UWaveSpawnerSubsystem
 *
 * Wave Spawner pattern-library subsystem. Persists across world transitions
 * (UGameInstanceSubsystem lifetime). Hosts the 23-actor AWave object pool,
 * three-phase FPatternPool structures, and the admission pipeline callback.
 *
 * Tick gating via FTickableGameObject:
 *   IsTickable()         → false in Cold and Idle states (zero idle overhead)
 *   GetTickableTickType() → ETickableTickType::Conditional
 *   GetStatId()          → STATGROUP_WaveSpawner (ADR-0005 IG-5)
 *
 * Story-scoped: this file implements Story 001 (subsystem skeleton + pool).
 * Subsequent stories add lifecycle transitions (002), admission (003–005),
 * IWaveSpawnerCallback seam (006), RSM/DPC delegate wiring (007),
 * cook-time validator (008), and telemetry (009).
 */
UCLASS()
class SLIPSTORM_API UWaveSpawnerSubsystem
    : public UGameInstanceSubsystem
    , public FTickableGameObject
{
    GENERATED_BODY()

public:
    // =========================================================================
    // UGameInstanceSubsystem interface
    // =========================================================================

    /**
     * Subscribes delegate handles only. MUST NOT call SpawnActor.
     * UWorld does not exist at this lifecycle point in UE 5.7 (R2a-4 correction).
     * SpawnActor is deferred to OnFirstWorldLoaded() via PostLoadMapWithWorld.
     *
     * Ordering pins (ADR-0005 IG-8, INT-004 2026-06-26):
     *   Collection.InitializeDependency(URunStateMachineSubsystem)
     *   Collection.InitializeDependency(UDPCSubsystem)
     * Both must return before this body proceeds. Without these pins,
     * GetSubsystem<UDPCSubsystem>() may return a partially-initialized instance,
     * silently dropping the admission delegate bind.
     * Forbidden Pattern: WaveSpawner_Initialize_without_InitializeDependency_on_RSM_and_DPC
     */
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    /**
     * Unsubscribes delegate handles; releases pool GC anchors.
     * AWave actors are destroyed by UE garbage collector once TObjectPtr refs drop
     * (ADR-0005 IG-6). No explicit Destroy() calls — pool is static (IG-2).
     */
    virtual void Deinitialize() override;

    // =========================================================================
    // FTickableGameObject interface
    // =========================================================================

    /**
     * Per-frame bookkeeping only (rate-limit timer state — Story 009).
     * Admission decision fires in OnDPCFrameReady(), NOT here (ADR-0005 IG-4).
     * Placing admission logic here would break structural DPC tick ordering.
     * Asserts game thread in non-Shipping builds (ADR-0005 Risk mitigation).
     */
    virtual void Tick(float DeltaTime) override;

    /**
     * Conditional tick type — paired with IsTickable() to suppress tick in
     * Cold and Idle states. FTickableGameObject does not support
     * AddTickPrerequisiteActor; ordering is enforced structurally via the
     * DPC delegate subscription (ADR-0005 R2a-2).
     */
    virtual ETickableTickType GetTickableTickType() const override
    {
        return ETickableTickType::Conditional;
    }

    /**
     * Returns false in Cold (pre-run) and Idle (post-run) states to avoid
     * idle CPU overhead. ETickableTickType::Conditional defers to this check.
     * Full lifecycle transition logic (Cold → Active etc.) is Story 002.
     */
    virtual bool IsTickable() const override;

    /**
     * Stat ID for Unreal Insights and stat unit profiling scope.
     * STATGROUP_WaveSpawner declared in WaveSpawnerTypes.h (included transitively
     * so all TUs that include this header see the group before this inline body).
     * AC-WS-30: admission-tick CPU <= 0.30ms p99 profiled under this stat scope.
     */
    virtual TStatId GetStatId() const override
    {
        RETURN_QUICK_DECLARE_CYCLE_STAT(UWaveSpawnerSubsystem, STATGROUP_WaveSpawner);
    }

    /**
     * Returns the UWorld this tickable object is associated with.
     * Required override (VR-7): FTickableGameObject::GetTickableGameObjectWorld()
     * defaults to nullptr, which detaches the tick from any specific game world.
     * Providing the GameInstance's world ensures the UE 5.7 tick-world association
     * is correct for mobile game-thread tick ordering (ADR-0005 IG-4 context).
     * IsAllowedToTick() is UE_DEPRECATED(5.5) — NOT overridden here.
     */
    virtual UWorld* GetTickableGameObjectWorld() const override
    {
        return GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
    }

    // =========================================================================
    // Pool access
    // =========================================================================

    /**
     * Acquires the first available AWave slot from the pool.
     * Sets the slot's bInUse = true and returns it.
     * Returns nullptr if all 23 slots are in use; logs a warning in that case.
     * NO SpawnActor calls occur here — pool is static after first world load (IG-2).
     */
    AWave* AcquireFromPool();

    /**
     * Returns a pool slot to the available state.
     * Finds the pool entry with matching WaveId, sets bInUse = false and
     * WaveId = INDEX_NONE. No-op (+ warning log) if WaveId is not found.
     * Called by the despawn pipeline at OnWaveDespawned (Story 006 Step 3).
     */
    void ReleaseToPool(int32 WaveId);

    /**
     * Read-only pool accessor for post-hoc verification in tests and debugging.
     * Returns all 23 slots; use bInUse to distinguish available vs leased entries.
     * AC-WS-20: GetPool().Num() must equal 23 at OnFirstWorldLoaded() exit.
     * AC-WS-15: UGameplayStatics census of AWave actors must match GetPool().Num().
     */
    const TArray<TObjectPtr<AWave>>& GetPool() const { return Pool; }

    // =========================================================================
    // Lifecycle transitions (Story 002 — ADR-0011 D3)
    // =========================================================================

    /**
     * Transitions to NewState, enforcing the 9 valid ADR-0011 D3 transitions.
     *
     * Valid transitions (authoritative ADR table — overrides Story AC-WS-15a where they differ;
     * see in-body comment and implementation comment in WaveSpawnerSubsystem.cpp for deviation notes):
     *   Cold    → Active    (RSM COUNTDOWN→RUNNING; captures RunSeed; sets OPENER pool)
     *   Active  → Holding   (phase-boundary drain-window entry)
     *   Active  → Flushing  (RSM terminal: DEAD|COMPLETE|ABORTED; OR Rule 13 pause-flush — see DEVIATION NOTE in HandlePausedChanged)
     *   Holding → Flushing  (OnPausedChanged(true) — pause fires during Holding; see DEVIATION NOTE in HandlePausedChanged)
     *   Holding → Active    (drain window clears: CountInFlightWaves()==0 + no stale slots)
     *   Holding → Idle      (long-pause path: all waves naturally despawn during Holding)
     *   Flushing → Cold     (run-termination: all live drained; NOT via Idle)
     *   Flushing → Active   (pause-flush complete: all stale scheduled drained)
     *   Idle    → Active    (RSM OnPausedChanged(false) resume; no flush needed)
     *
     * Forbidden transitions (all others): UE_LOG(Error) + check(false) in non-Shipping
     * (aborts in Development); UE_LOG(Error) + no-op return in Shipping.
     *
     * RSM/DPC delegate subscriptions that CALL TransitionTo() are Story 007 (out of scope here).
     *
     * Story: production/epics/wave-spawner/story-002-six-state-lifecycle.md
     * ADR:   docs/architecture/adr-0011-wave-spawner-pattern-library.md (D3)
     * TRs:   TR-WS-011, TR-WS-020
     */
    void TransitionTo(EWaveSpawnerLifecycleState NewState);

    /**
     * Returns the active draw pool pointer for the current run phase.
     * Null in Cold state (pre-run) and after Flushing→Cold reset.
     * Set to &OpenerPool at Cold→Active; reassigned atomically on Holding entry
     * (→&MidPool on first, →&PeakPool on second — TR-WS-021).
     * Non-owning raw pointer into the UPROPERTY struct members of this UObject.
     * GC-safe: struct fields are not heap-allocated UObjects; they live inside `this`
     * which is UPROPERTY-anchored. UE GC is non-compacting (VR-8 confirmed).
     * Used by Stories 003–005 (admission pipeline) and lifecycle tests.
     */
    const FPatternPool* GetActiveDrawPool() const { return ActiveDrawPool; }

    /**
     * Returns the current pattern pool phase (Opener / Mid / Peak).
     * Set to Opener at Cold→Active; advanced on each Holding entry (pool swap).
     * Reset to Opener at Flushing→Cold. Phase never regresses.
     * TR-WS-021.
     */
    ERunPhase GetActivePhase() const { return ActivePhase; }

    /**
     * Returns true during the Rule 9 phase-boundary drain window.
     * Set true on every Active→Holding entry; cleared on Holding→Active,
     * Holding→Idle, or Flushing→Cold reset.
     * Story 007 polls this (+ CountInFlightWaves()==0) to auto-fire Holding→Active.
     * TR-WS-023.
     */
    bool IsDrainWindowActive() const { return bDrainWindowActive; }

    // =========================================================================
    // Rule 12 despawn pipeline (Story 006 — ADR-0011 D3, TR-WS-026)
    // =========================================================================

    /**
     * Installs the despawn callback observer. Pass nullptr to clear.
     * Called by integration tests (SetDespawnCallback(&Stub)) and by production
     * wiring in Story 007.
     *
     * @param InCallback  Observer implementing IWaveSpawnerCallback, or nullptr.
     */
    void SetDespawnCallback(IWaveSpawnerCallback* InCallback) { Callback = InCallback; }

    /**
     * Executes the Rule 12 ordered despawn pipeline for a single AWave actor.
     *
     * Pipeline sequence (mandatory regardless of Reason — TR-WS-026):
     *   1. Callback->OnCollisionUnregistered(WaveId)
     *   2. Callback->OnTelegraphUnregistered(WaveId)
     *   3. LiveSlots.Remove(WaveId)       ← slot released HERE
     *   4. Callback->OnWaveDespawned(WaveId, Reason)
     *   5. ReleaseToPool(WaveId)          ← pool slot returned
     *
     * Each call releases exactly 1 LiveSlot regardless of barrage/non-barrage
     * (see DEVIATION NOTE in WaveSpawnerSubsystem.cpp — TSet model).
     * ScheduledSlots cleanup during PauseFlush is Story 007's responsibility (Rule 13).
     *
     * @param WaveId  Identifier of the AWave actor being despawned.
     * @param Reason  Why the wave is being despawned (NaturalLanding / RunTermination / PauseFlush).
     *
     * Story: production/epics/wave-spawner/story-006-despawn-pipeline-and-seam-13.md
     * ADR:   docs/architecture/adr-0011-wave-spawner-pattern-library.md (D3, Rule 12)
     * TR:    TR-WS-026
     */
    void DespawnWave(int32 WaveId, EWaveDespawnReason Reason);

#if WITH_DEV_AUTOMATION_TESTS
public:
    // =========================================================================
    // Test-only seams (not available in Shipping builds)
    // =========================================================================

    /**
     * Test-only: directly invokes OnFirstWorldLoaded(World).
     *
     * AC-WS-20a usage: pass a real UWorld (via FAutomationEditorCommonUtils::CreateNewMap)
     *   to exercise the full SpawnActor × 23 path and verify GetPool().Num() == 23.
     * AC-WS-20c usage: call twice with the SAME non-null UWorld. The first call allocates
     *   (Pool grows to 23, bPoolAllocated = true). The second call hits Guard 2 (bPoolAllocated)
     *   and returns before the SpawnActor loop — Pool stays at 23, not 46.
     *   Passing nullptr on the second call would only test Guard 1 (null-world), not Guard 2.
     */
    void TestOnly_TriggerFirstWorldLoaded(UWorld* World)
    {
        OnFirstWorldLoaded(World);
    }

    /**
     * Test-only: injects a pre-created AWave instance directly into the pool
     * without calling SpawnActor. Sets bPoolAllocated = true once at least one
     * entry exists. Enables headless AC-WS-20b (Deinitialize clears) and
     * AC-WS-20c (guard flag set) testing without requiring a UWorld.
     */
    void TestOnly_InjectPoolEntry(AWave* Wave)
    {
        Pool.Add(Wave);
        bPoolAllocated = (Pool.Num() > 0);
    }

    /**
     * Test-only: returns the current bPoolAllocated flag value.
     * AC-WS-20c: verifies the one-shot guard is engaged after first allocation.
     */
    bool TestOnly_IsPoolAllocated() const { return bPoolAllocated; }

    /**
     * Test-only: directly sets the lifecycle state without firing transition guards
     * or OnLifecycleTransition() hooks. Use ONLY to set up preconditions for other
     * seams. Does NOT update ActiveDrawPool, ActivePhase, or bDrainWindowActive.
     * For testing TransitionTo() itself, start from the default Cold state (construct
     * a fresh subsystem) to avoid state side-effects across test cases.
     * Story 002.
     */
    void TestOnly_SetLifecycleState(EWaveSpawnerLifecycleState State)
    {
        LifecycleState = State;
    }

    /**
     * Test-only: returns the current lifecycle state for white-box assertion.
     * Paired with TestOnly_SetLifecycleState for state precondition setup.
     * Story 002.
     */
    EWaveSpawnerLifecycleState TestOnly_GetLifecycleState() const
    {
        return LifecycleState;
    }

    /**
     * Test-only: exposes the private IsValidTransition() predicate for
     * Lifecycle.ForbiddenTransitions tests (Option A — no ADR deviation).
     *
     * Background: TransitionTo() contains check(false) on forbidden transitions,
     * which aborts the process in non-Shipping builds (check() is not catchable
     * via AddExpectedError()). The forbidden-transition test therefore calls this
     * predicate directly to assert IsValidTransition(From, To)==false for each
     * forbidden pair, rather than calling TransitionTo() and risking a crash.
     * This gives equivalent AC coverage: the predicate IS the gate that TransitionTo()
     * enforces. Story 002 / ADR-0011 D3.
     */
    bool TestOnly_IsValidTransition(
        EWaveSpawnerLifecycleState From,
        EWaveSpawnerLifecycleState To) const
    {
        return IsValidTransition(From, To);
    }

    /**
     * Test-only: fixes GetCurrentTimeS() return value for headless time-gate tests.
     * Set before calling TestOnly_TriggerDPCFrameReady to simulate elapsed time.
     * Cleared by TestOnly_ClearCurrentTimeOverride (restores the production UWorld path).
     * Story 003.
     */
    void TestOnly_SetCurrentTimeOverride(float TimeS)
    {
        TestCurrentTimeOverrideS = TimeS;
        bTestTimeOverrideActive  = true;
    }

    /** Test-only: restores GetCurrentTimeS() to the real UWorld path. Story 003. */
    void TestOnly_ClearCurrentTimeOverride() { bTestTimeOverrideActive = false; }

    /**
     * Test-only: directly invokes OnDPCFrameReady(FrameState) for headless unit testing.
     * Allows injecting a crafted FDPCFrameState without a live DPC subsystem.
     * Story 003.
     */
    void TestOnly_TriggerDPCFrameReady(const FDPCFrameState& FrameState)
    {
        OnDPCFrameReady(FrameState);
    }

    /** Test-only: returns current bPrimerPending state for assertion. Story 003. */
    bool TestOnly_IsPrimerPending() const { return bPrimerPending; }

    /** Test-only: returns current LastSpawnTimeS for assertion. Story 003. */
    float TestOnly_GetLastSpawnTimeS() const { return LastSpawnTimeS; }

    /** Test-only: sets bPrimerPending directly (bypasses lifecycle transition). Story 003. */
    void TestOnly_SetPrimerPending(bool bPending) { bPrimerPending = bPending; }

    /**
     * Test-only: sets resume grace state directly.
     *
     * Story 007 wires the production path (UWaveSpawnerSubsystem::OnResumeFromPause,
     * called from RSM OnPausedChanged(false)). That delegate subscription is out of
     * scope for Story 003. This seam allows TC3 to exercise the grace-window veto
     * (TR-WS-012) headlessly without the RSM stub that Story 007 will introduce.
     *
     * Story 003.
     */
    void TestOnly_SetResumeGrace(bool bGrace, float EndTimeS)
    {
        bInResumeGrace      = bGrace;
        ResumeGraceEndTimeS = EndTimeS;
    }

    // -------------------------------------------------------------------------
    // Story 004 test seams — Stage 2 slot accounting
    // -------------------------------------------------------------------------

    /** Test-only: injects N dummy WaveIds into ScheduledSlots (range -1 to -N).
     *  Negative IDs cannot collide with production NextWaveIdCounter output (starts at 0).
     *  Story 004 / AC-WS-12a,b,c. */
    void TestOnly_SetScheduledCount(int32 N)
    {
        ScheduledSlots.Reset();
        for (int32 i = 0; i < N; ++i) { ScheduledSlots.Add(-1 - i); }
    }

    /** Test-only: injects N dummy WaveIds into LiveSlots (range -1001 to -(1000+N)).
     *  Distinct range from ScheduledSlots injections; no overlap for N up to 23. Story 004. */
    void TestOnly_SetLiveCount(int32 N)
    {
        LiveSlots.Reset();
        for (int32 i = 0; i < N; ++i) { LiveSlots.Add(-1001 - i); }
    }

    /** Test-only: directly sets bBarrageOwed for reservation-path tests. Story 004 / AC-WS-12b,c. */
    void TestOnly_SetBarrageOwed(bool bOwed) { bBarrageOwed = bOwed; }

    /** Test-only: reads bBarrageOwed after TryAdmitPattern. Story 004 / AC-WS-12b,c. */
    bool TestOnly_GetBarrageOwed() const { return bBarrageOwed; }

    /** Test-only: reads ScheduledSlots.Num() for slot-count assertions. Story 004 / AC-WS-12a,b. */
    int32 TestOnly_GetScheduledCount() const { return ScheduledSlots.Num(); }

    /** Test-only: reads LiveSlots.Num() for slot-count assertions. Story 004. */
    int32 TestOnly_GetLiveCount() const { return LiveSlots.Num(); }

    /** Test-only: reads available slot count via GetAvailableSlots(). Story 004 / AC-WS-12a. */
    int32 TestOnly_GetAvailableSlots() const { return GetAvailableSlots(); }

    /** Test-only: reads LastAdmissionResult stored at the most recent OnDPCFrameReady callsite.
     *  Initialized to PoolExhausted so any Admitted/Deferred assertion proves TryAdmitPattern ran.
     *  Story 004 / AC-WS-12a,b,c. */
    EAdmissionResult TestOnly_GetLastAdmissionResult() const { return LastAdmissionResult; }

    // -------------------------------------------------------------------------
    // Story 005 test seams — F-3 cadence governor + RNG seeding
    // TRs: TR-WS-013, TR-WS-029, TR-WS-030
    // -------------------------------------------------------------------------

    /** Test-only: set BarrageCountThisPeak directly. Story 005 / AC-WS-14. */
    void TestOnly_SetBarrageCount(int32 N) { BarrageCountThisPeak = N; }
    /** Test-only: read BarrageCountThisPeak. Story 005 / AC-WS-15. */
    int32 TestOnly_GetBarrageCount() const { return BarrageCountThisPeak; }
    /** Test-only: set PeakEntryTimeS directly. Story 005 / AC-WS-14. */
    void TestOnly_SetPeakEntryTimeS(float T) { PeakEntryTimeS = T; }
    /** Test-only: set ActivePhase directly for F-3 branch tests without lifecycle churn. Story 005. */
    void TestOnly_SetActivePhase(ERunPhase Phase) { ActivePhase = Phase; }
    /** Test-only: seed PatternRNG with a known value for deterministic draw tests. Story 005 / AC-WS-13. */
    void TestOnly_SeedPatternRNG(int32 Seed) { PatternRNG.Initialize(Seed); }

    /**
     * Test-only: inject NonBarragePatterns into OpenerPool and point ActiveDrawPool at it.
     * Setting ActiveDrawPool is required for DrawNonBarragePattern to select from the injected pool.
     * Story 005 / AC-WS-15.
     */
    void TestOnly_InjectOpenerPool(const TArray<FPatternDefinition>& Patterns)
    {
        OpenerPool.NonBarragePatterns = Patterns;
        ActiveDrawPool = &OpenerPool;
    }

    /** Test-only: inject BarragePatterns into PeakPool for DrawBarragePattern tests. Story 005 / AC-WS-15. */
    void TestOnly_InjectPeakBarragePool(const TArray<FPatternDefinition>& Patterns)
    {
        PeakPool.BarragePatterns = Patterns;
    }

    /**
     * Test-only: exposes the private ShouldDrawBarrage() predicate directly.
     * Allows TC4–TC8 to verify F-3 three-branch behavior without routing through the
     * full DPC admission pipeline. Direct access prevents ShouldDrawBarrage from consuming
     * a PatternRNG FRand() value unintentionally in TC2/TC9 draw tests. Story 005 / AC-WS-14.
     */
    bool TestOnly_ShouldDrawBarrage() const { return ShouldDrawBarrage(); }

    /**
     * Test-only: directly calls DrawNonBarragePattern() for pool selection tests.
     * Bypasses TryAdmitPattern — no ShouldDrawBarrage call, no slot accounting.
     * Story 005 / AC-WS-15.
     */
    void TestOnly_DrawNonBarragePattern(const FDPCFrameState& F) { DrawNonBarragePattern(F); }

    /**
     * Test-only: directly calls DrawBarragePattern() for counter and pool tests.
     * Bypasses TryAdmitPattern — no ShouldDrawBarrage call, no slot accounting.
     * Story 005 / AC-WS-15.
     */
    void TestOnly_DrawBarragePattern(const FDPCFrameState& F) { DrawBarragePattern(F); }

    /**
     * Test-only: returns the pool index selected by the most recent Draw*Pattern() call.
     * Returns INDEX_NONE until a draw fires or after Cold reset (LastDrawIndexForTest = INDEX_NONE).
     * Used for determinism verification in TC2 and TC9 (AC-WS-15).
     */
    int32 TestOnly_GetLastDrawIndex() const { return LastDrawIndexForTest; }

    /**
     * Test-only: returns the next FRand() value from a COPY of PatternRNG without advancing
     * the real stream. Allows TC1 to verify PatternRNG stream state directly after
     * Cold→Active seeding (AC-WS-13), and TC7-boundary to verify strict `<` semantics
     * by setting WBarrage to the peeked value (AC-WS-14).
     * Story 005 / AC-WS-13, AC-WS-14.
     */
    float TestOnly_PeekNextPatternRNGFRand() const
    {
        FRandomStream Peek = PatternRNG;  // copy — does not advance the real stream
        return Peek.FRand();
    }

    // -------------------------------------------------------------------------
    // Story 007 test seams — RSM/DPC integration
    // TRs: TR-WS-022 (snapshot immutability), TR-WS-027 (LastSpawnTime preserve)
    // -------------------------------------------------------------------------

    /**
     * Test-only: returns the TelegraphWindowS captured in InFlightWaves for the given WaveId.
     * Returns -1.f if WaveId is not present (not in-flight or already despawned).
     * Used by AC-WS-21 and AC-WS-29 to verify snapshot immutability after a DPC frame
     * update or phase-boundary pool swap.
     * Story 007.
     */
    float TestOnly_GetInFlightTelegraphWindow(int32 WaveId) const
    {
        const FWaveInFlightState* State = InFlightWaves.Find(WaveId);
        return State ? State->TelegraphWindowS : -1.f;
    }

    /**
     * Test-only: directly sets LastSpawnTimeS.
     * Enables AC-WS-17 TR-WS-027 assertion: set a known value before pause-flush,
     * then verify the same value is present after HandlePausedChanged(true) completes.
     * Story 007 / AC-WS-17, TR-WS-027.
     */
    void TestOnly_SetLastSpawnTimeS(float T) { LastSpawnTimeS = T; }

    /**
     * Test-only: directly invokes HandlePausedChanged(bIsPaused, Timestamp).
     * Bypasses the RSM delegate — fires pause/resume events headlessly without a live RSM.
     * Caller must TestOnly_SetLifecycleState(Active) first; handlers guard on lifecycle state.
     * Story 007 / AC-WS-17, AC-WS-28.
     */
    void TestOnly_FirePausedChanged(bool bIsPaused, double Timestamp = 0.0)
    {
        HandlePausedChanged(bIsPaused, Timestamp);
    }

    /**
     * Test-only: directly invokes HandleRunStateChanged with the given state transition.
     * Bypasses the RSM delegate — fires run state transitions headlessly without a live RSM.
     * Caller must TestOnly_SetLifecycleState(Active|Cold) first; handlers guard on lifecycle.
     * Story 007 / AC-WS-18.
     */
    void TestOnly_FireRunStateChanged(
        ERunState PreviousState, ERunState NewState,
        ERunOutcome Outcome   = ERunOutcome::NONE,
        double      Timestamp = 0.0)
    {
        HandleRunStateChanged(PreviousState, NewState, Outcome, Timestamp);
    }

#endif // WITH_DEV_AUTOMATION_TESTS

private:
    // =========================================================================
    // Delegate callbacks
    // =========================================================================

    /**
     * Pool pre-allocation — fires once per session at PostLoadMapWithWorld.
     * Bound to FCoreUObjectDelegates::PostLoadMapWithWorld in Initialize().
     *
     * Guard order (VR-2 — engine-verified null-world broadcast path):
     *   1. if (!LoadedWorld) return;    — FPostLoadMapCaller broadcasts nullptr on error
     *   2. if (bPoolAllocated) return;  — one-shot guard (edge-case world loads, EC-WS-8)
     *   3. bPoolAllocated = true;       — set BEFORE loop to prevent re-entry race
     *   4. SpawnActor × 23             — pool allocation (only legal path in UE 5.7)
     *
     * Replay re-entry (Flushing → Cold → Active per EC-WS-8) does NOT re-fire this
     * path — PostLoadMapWithWorld is not broadcast on world re-use.
     */
    void OnFirstWorldLoaded(UWorld* LoadedWorld);

    /**
     * Admission pipeline entry point — fires after DPC publishes its frame state.
     * Bound to UDPCSubsystem::OnPostTickFrameStatePublished in Initialize().
     * Structural tick-ordering guarantee: DPC state is fully computed before this fires.
     * ADR-0005 R2a-2: Rule 1 gate + Rule 7 admission fires here, NOT in Tick().
     * Full admission logic implemented in Stories 003–007.
     * Asserts game thread in non-Shipping builds (ADR-0005 Risk mitigation).
     */
    void OnDPCFrameReady(const FDPCFrameState& FrameState);

    // =========================================================================
    // Pool storage — GC-anchored (ADR-0005 IG-6)
    // =========================================================================

    /**
     * Object pool: 23 pre-allocated AWave actor instances.
     * UPROPERTY() prevents UE's garbage collector from collecting pooled actors
     * that are not part of the active world's actor graph (ADR-0005 IG-6).
     * TObjectPtr<T> is the UE 5.0+ GC-safe replacement for raw UObject pointers.
     * Allocated once in OnFirstWorldLoaded(); reused across all death-replay cycles.
     * Pool sizing: 16 PEAK concurrency cap + 2 DESPAWNING latency slots + 5 margin = 23.
     */
    UPROPERTY()
    TArray<TObjectPtr<AWave>> Pool;

    // =========================================================================
    // Three-pool pattern library (ADR-0011 D1)
    // =========================================================================

    /** OPENER phase patterns (t_norm 0.0 → OPENER_END). Non-barrage only (TR-WS-002). */
    UPROPERTY()
    FPatternPool OpenerPool;

    /** MID phase patterns (OPENER_END → PEAK_START). Non-barrage only (TR-WS-003). */
    UPROPERTY()
    FPatternPool MidPool;

    /**
     * PEAK phase patterns (PEAK_START → 1.0). Contains both BarragePatterns (7 triplets
     * per TR-WS-004) and NonBarragePatterns sub-arrays.
     */
    UPROPERTY()
    FPatternPool PeakPool;

    // =========================================================================
    // Lifecycle state
    // =========================================================================

    /**
     * Internal lifecycle state. Controls IsTickable() gating.
     * Defaults to Cold on subsystem creation; transitions to Active when the run
     * begins (RSM RUNNING state — Story 002).
     * Full six-state lifecycle machine (Cold/Active/Holding/Flushing/Idle) is Story 002.
     */
    EWaveSpawnerLifecycleState LifecycleState = EWaveSpawnerLifecycleState::Cold;

    /**
     * One-shot guard: true after OnFirstWorldLoaded() completes the SpawnActor pass.
     * Prevents pool re-allocation on any subsequent PostLoadMapWithWorld broadcasts
     * (edge-case seamless travel, DLC loads, or engine error paths — ADR-0005 Risk 1).
     */
    bool bPoolAllocated = false;

    // =========================================================================
    // Delegate handles (for clean unsubscription in Deinitialize)
    // =========================================================================

    /** Handle for FCoreUObjectDelegates::PostLoadMapWithWorld subscription. */
    FDelegateHandle PostLoadMapHandle;

    /** Handle for UDPCSubsystem::OnPostTickFrameStatePublished subscription. */
    FDelegateHandle DPCFrameReadyHandle;

    // =========================================================================
    // Story 007: RSM delegate subscriptions + RunSeed capture
    // TRs: TR-WS-013 (seed), TR-WS-025 (run termination), TR-WS-027 (pause)
    // =========================================================================

    /**
     * Cached RSM pointer. Populated in Initialize() after InitializeDependency(RSM).
     * Used in HandlePausedChanged and HandleRunStateChanged.
     * Null-checked in Deinitialize() and in test paths where Initialize() is not called.
     * Not UPROPERTY — raw C++ pointer; RSM is a UGameInstanceSubsystem that outlives this.
     */
    URunStateMachineSubsystem* RSMSubsystem = nullptr;

    /** Handle for URunStateMachineSubsystem::OnPausedChanged subscription (Story 007). */
    FDelegateHandle RSMPausedHandle;

    /** Handle for URunStateMachineSubsystem::OnStateChanged subscription (Story 007). */
    FDelegateHandle RSMStateHandle;

    /**
     * RunSeed captured from RSM at Cold→Active (COUNTDOWN→RUNNING RSM transition).
     * Seeds PatternRNG lower 32 bits in OnLifecycleTransition(Active) (TR-WS-013).
     * MUST be set by HandleRunStateChanged BEFORE calling TransitionTo(Active) to avoid
     * double-initialization: OnLifecycleTransition reads this field when seeding PatternRNG.
     * Stub 0 until RSM epic delivers the real seed via GetRunSeed() (TR-WS-013).
     * Story 007 / AC-WS-13.
     */
    uint64 RunSeed = 0;

    // =========================================================================
    // Despawn pipeline callback (Story 006 — Seam 13)
    // =========================================================================

    /**
     * Active despawn callback observer. Nullptr until wired by tests or production (Story 007).
     * Not UPROPERTY — plain C++ pointer; IWaveSpawnerCallback is not a UObject.
     * Lifetime: caller is responsible for ensuring the callback outlives this subsystem.
     */
    IWaveSpawnerCallback* Callback = nullptr;

    // =========================================================================
    // Lifecycle helpers (Story 002 — ADR-0011 D3)
    // =========================================================================

    /**
     * Returns true if From→To is one of the 9 valid ADR-0011 D3 transitions.
     * Returns false for all other (From, To) pairs (forbidden).
     * Called by TransitionTo() before every state change.
     * Exposed for testing via TestOnly_IsValidTransition().
     * Implementation: switch on From with nested To checks — O(1), no allocation.
     */
    bool IsValidTransition(
        EWaveSpawnerLifecycleState From,
        EWaveSpawnerLifecycleState To) const;

    /**
     * Fires after LifecycleState is updated in TransitionTo().
     * Responsibilities:
     *   — Atomic pool-pointer swap on Holding entry (ADR-0011 D3 Rule 10 / TR-WS-021):
     *     Opener→Mid on first Holding, Mid→Peak on second. Pointer only; no data copy.
     *   — Cold→Active: initialises ActiveDrawPool to &OpenerPool, ActivePhase to Opener.
     *   — Drain-window flag: set true on Holding entry; cleared on Active / Idle / Cold entry.
     *   — Flushing→Cold: full per-run reset (ActiveDrawPool=nullptr, Phase=Opener, drain=false).
     * In-flight FWaveInFlightState fields are NOT mutated at any point (TR-WS-022).
     */
    void OnLifecycleTransition(EWaveSpawnerLifecycleState NewState);

    /**
     * Returns the count of pool slots with bInUse == true.
     * Used by Story 007 drain-window clearance: when == 0 and no stale
     * scheduled_slots exist, fires Holding→Active to resume admissions.
     * TR-WS-023.
     */
    int32 CountInFlightWaves() const;

    /**
     * Returns the current game world time in seconds.
     * Extracted for testing: headless unit tests inject a fixed value via
     * TestOnly_SetCurrentTimeOverride() without requiring a live UWorld.
     * Production: GetGameInstance()->GetWorld()->GetTimeSeconds().
     * Story 003.
     */
    float GetCurrentTimeS() const;

    // =========================================================================
    // Story 007: RSM event handlers, resume grace, in-flight ordering
    // ADR:   docs/architecture/adr-0011-wave-spawner-pattern-library.md (D3 Rules 13, 14)
    // ADR:   docs/architecture/adr-0007-run-state-machine-hosting.md (OnPausedChanged, OnStateChanged)
    // TRs:   TR-WS-025, TR-WS-027, TR-WS-028
    // =========================================================================

    /**
     * Bound to URunStateMachineSubsystem::OnPausedChanged (two-param delegate per ADR-0007).
     *
     * bIsPaused == true  (Rule 13 pause-flush):
     *   Drains all LiveSlots in WaveId ASC order with EWaveDespawnReason::PauseFlush.
     *   LastSpawnTimeS is NOT reset (TR-WS-027 / AC-WS-17).
     *   Transitions lifecycle Active|Holding → Flushing.
     *
     * bIsPaused == false (resume):
     *   Sets resume grace window (kResumeGraceS / TR-WS-012 / AC-WS-28).
     *   Transitions lifecycle Flushing|Holding|Idle → Active.
     *
     * Guarded: spurious broadcasts in Cold/Idle/Flushing states are logged + ignored.
     * Story 007 / AC-WS-17, AC-WS-28.
     */
    void HandlePausedChanged(bool bIsPaused, double Timestamp);

    /**
     * Bound to URunStateMachineSubsystem::OnStateChanged (four-param delegate per ADR-0007).
     *
     * NewState == RUNNING (Cold→Active):
     *   Captures RunSeed from RSM (BEFORE TransitionTo to avoid RNG double-init, Story 007).
     *   Transitions lifecycle Cold → Active, which seeds PatternRNG (TR-WS-013).
     *
     * NewState ∈ {DEAD, COMPLETE, ABORTED} (Rule 14 run-termination):
     *   Drains all LiveSlots in WaveId ASC order with EWaveDespawnReason::RunTermination.
     *   Clears bBarrageOwed. Transitions Active|Holding → Flushing → Cold.
     *
     * DEVIATION NOTE (AC-WS-18): Story AC-WS-18 says lifecycle → Idle. ADR-0011 D3
     * is authoritative (coordinator-approved, same precedent as IsValidTransition DEVIATION NOTE):
     * Flushing→Idle is FORBIDDEN; run-termination resets to Cold. OnLifecycleTransition(Cold)
     * satisfies AC-WS-18's substantive checks (bBarrageOwed=false, Live=0, Scheduled=0).
     * AC-WS-18 text should be reconciled to read "lifecycle → Cold".
     *
     * Story 007 / AC-WS-18.
     */
    void HandleRunStateChanged(
        ERunState PreviousState, ERunState NewState,
        ERunOutcome Outcome, double Timestamp);

    /**
     * Sets bInResumeGrace = true and ResumeGraceEndTimeS = now + kResumeGraceS.
     * Rule 1 gate in OnDPCFrameReady vetoes admission until the window expires lazily.
     * Called by HandlePausedChanged when OnPausedChanged(false) fires (AC-WS-28).
     * TR-WS-012.
     */
    void OnResumeFromPause();

    /**
     * Returns WaveIds currently in LiveSlots, sorted in ascending numeric order.
     * Used by HandlePausedChanged and HandleRunStateChanged for deterministic flush order.
     * Rule 13/14: despawn order is WaveId ASC (AC-WS-17, AC-WS-18).
     * TSet::Array() + Sort() — O(N log N), N ≤ 16, negligible cost (ADR-0011 D2 cap).
     */
    TArray<int32> GetInFlightWaveIdsSorted() const;

    /**
     * Removes InFlightWaves entries for all WaveIds currently in ScheduledSlots.
     * Called before ScheduledSlots.Reset() on both the pause-flush (Rule 13) and
     * run-termination (Rule 14) paths.
     *
     * Background: TryAdmitPattern() adds an FWaveInFlightState entry to InFlightWaves for
     * every wave at slot pre-commitment time — before the wave is live. ScheduledSlots holds
     * those pre-committed IDs. If ScheduledSlots.Reset() fires without first removing the
     * corresponding InFlightWaves entries, those entries become orphans: InFlightWaves.Num()
     * inflates by up to 3 per barrage pause event and only clears at Flushing→Cold (run end).
     *
     * DespawnWave() is NOT called here — no pool actor is associated with scheduled waves.
     * N ≤ 16 per ADR-0011 D2 concurrency cap — negligible cost on infrequent event path.
     * B-1 fix (code-review 2026-08-21).
     * Story 007 / TR-WS-025.
     */
    void PurgeScheduledInFlightEntries();

    // =========================================================================
    // Phase / draw-pool state (Story 002 — ADR-0011 D3)
    // =========================================================================

    /**
     * Current pattern pool phase. Set to Opener at Cold→Active.
     * Advanced by OnLifecycleTransition(Holding): Opener→Mid, Mid→Peak.
     * Reset to Opener at Flushing→Cold. Never regresses.
     * TR-WS-020, TR-WS-021.
     */
    ERunPhase ActivePhase = ERunPhase::Opener;

    /**
     * Active draw pool pointer. Non-owning raw ptr into the OpenerPool / MidPool /
     * PeakPool UPROPERTY struct members of this subsystem.
     * GC-safe: struct fields live inside `this` (a UPROPERTY-anchored UObject);
     * UE GC is non-compacting so addresses do not change after allocation (VR-8).
     * Null in Cold state and after Flushing→Cold reset.
     * Reassigned atomically on Holding entry — no pattern data copied (TR-WS-021).
     * Game-thread-only (ADR-0005 threading model; no cross-thread access).
     */
    const FPatternPool* ActiveDrawPool = nullptr;

    /**
     * True during the Rule 9 phase-boundary drain window.
     * Set true on every Active→Holding entry (both phase-boundary and pause paths —
     * TODO Story 007: a cause parameter may be needed to distinguish them if the
     * pause-flush path requires different drain-complete semantics).
     * Set false on Holding→Active (drain complete), Holding→Idle, or Flushing→Cold.
     * Story 007 monitors this flag + CountInFlightWaves() to auto-fire Holding→Active.
     * TR-WS-023.
     */
    bool bDrainWindowActive = false;

    // =========================================================================
    // Admission gate state (Story 003 — TR-WS-012, TR-WS-015, TR-WS-016)
    // =========================================================================

    /**
     * Wall-clock time of the most recent successful pattern admission.
     * Updated after primer draw (TR-WS-016) and after each cadence-gate-passing
     * admission (TR-WS-015). Initialized to 0.f; the primer bypass skips cadence
     * on the first DPC frame after Cold→Active.
     */
    float LastSpawnTimeS = 0.f;

    /**
     * True after Cold→Active entry until the first pattern is admitted (primer draw).
     * Bypasses the cadence gate for the first admission per run (Rule 2a / TR-WS-016).
     * Set true in OnLifecycleTransition when ActiveDrawPool == nullptr on Active entry.
     * Cleared after primer draw fires.
     */
    bool bPrimerPending = false;

    /**
     * True during the post-resume grace window.
     * Set by Story 007 OnResumeFromPause(); cleared lazily in OnDPCFrameReady when
     * GetCurrentTimeS() >= ResumeGraceEndTimeS.
     * TR-WS-012 (Rule 1 gate).
     */
    bool bInResumeGrace = false;

    /** Wall-clock time at which the resume grace window expires. TR-WS-012. */
    float ResumeGraceEndTimeS = 0.f;

    // =========================================================================
    // Stage 2 slot accounting (Story 004 — ADR-0011 D2 two-layer model)
    // TRs: TR-WS-017, TR-WS-018
    // =========================================================================

    /**
     * WaveIds of patterns whose telegraph is scheduled but whose AWave actor
     * has not yet been leased from the pool. Pre-committed at TryAdmitPattern();
     * removed when the AWave transitions to Live (Story 006).
     * ADR-0011 D2 Stage 2.
     */
    UPROPERTY(Transient) TSet<int32> ScheduledSlots;

    /**
     * WaveIds of actively rendering AWave actors (leased from pool and activated).
     * Populated when AWave.Activate() fires (Story 006 seam).
     * Removed in OnWaveDespawned (Story 006).
     * ADR-0011 D2 Stage 2.
     */
    UPROPERTY(Transient) TSet<int32> LiveSlots;

    /**
     * Per-wave admission snapshots. Keyed by WaveId; value captures the DPC frame
     * parameters that were current when TryAdmitPattern() pre-committed the slot.
     *
     * Lifecycle:
     *   Add: TryAdmitPattern() at each ScheduledSlots.Add() site (3 sites, Story 007).
     *   Remove: DespawnWave() alongside LiveSlots.Remove() (Rule 12 pipeline, Story 007).
     *   Reset: OnLifecycleTransition(Cold) resets for the next run.
     *
     * Immutability contract (TR-WS-022, AC-WS-21, AC-WS-29):
     *   Once added, FWaveInFlightState fields are NEVER mutated. A subsequent DPC frame
     *   publishing a new TelegraphWindowS does not overwrite the stored value. A pool-pointer
     *   swap at a phase boundary (Active→Holding) does not touch this map.
     *
     * Not UPROPERTY — plain TMap<int32, struct>. GC-safe: key/value contain no UObject refs.
     * Game-thread-only (ADR-0005 threading model).
     * Story 007 / TR-WS-022.
     */
    TMap<int32, FWaveInFlightState> InFlightWaves;

    /**
     * True when a barrage draw was dropped due to insufficient slots.
     * The next admission window with available >= 3 is reserved for a fresh barrage draw.
     * Set in TryAdmitPattern() when barrage cannot be fulfilled atomically.
     * Cleared on successful barrage admission.
     * NOT cleared by pause flush (Holding transition). Cleared on Run Termination (Story 007).
     * TR-WS-018, AC-WS-12b, AC-WS-12c.
     */
    bool bBarrageOwed = false;

    /**
     * Stub WaveId counter for Stage 2 slot pre-commitment.
     * Incremented per ScheduledSlots.Add() call in TryAdmitPattern(); reset in Cold case.
     * Story 005 (Stage 4) replaces stub IDs with real pool-acquired WaveIds from AcquireFromPool().
     */
    int32 NextWaveIdCounter = 0;

    /**
     * Result of the most recent TryAdmitPattern() call, stored at the OnDPCFrameReady callsite.
     * Initialized to PoolExhausted — a value TryAdmitPattern() never returns in this story,
     * so any test assertion on Admitted or Deferred_* conclusively proves TryAdmitPattern ran.
     * Story 004 test seam. AC-WS-12a, AC-WS-12b, AC-WS-12c.
     */
    EAdmissionResult LastAdmissionResult = EAdmissionResult::PoolExhausted;

    // =========================================================================
    // Stage 2 admission helpers (Story 004 — ADR-0011 D2)
    // =========================================================================

    /**
     * Computes slots available for new admissions:
     *   available = KMaxConcurrentWavesStub − (ScheduledSlots.Num() + LiveSlots.Num())
     *
     * DEVIATION NOTE: ADR-0011 D2 Stage 1 reads max_concurrent_waves from FDPCFrameState.
     * That field does not exist in the DPC stub (DPCSubsystem.h — DPC epic, TODO).
     * kMaxConcurrentWavesStub (= 23, defined in WaveSpawnerSubsystem.cpp) is the structural
     * pool-size ceiling and a conservative placeholder. The real DPC-published concurrency cap
     * (MAX_CONCURRENT_WAVES_CAP = 16, TR-DPC-011) will replace it when FDPCFrameState
     * exposes max_concurrent_waves. TODO: replace with FDPCFrameState.max_concurrent_waves
     * when the DPC epic delivers that field (TR-DPC-011).
     *
     * Story 004 / ADR-0011 D2 Stage 2. TRs: TR-WS-017, TR-WS-018.
     */
    int32 GetAvailableSlots() const;

    /**
     * Implements ADR-0011 D2 Stage 2 — Rule 7 slot pre-commitment (TR-WS-017, TR-WS-018).
     * Called from OnDPCFrameReady() after all Rule 1 gates and cadence gate pass.
     *
     * Stage 3 (ShouldDrawBarrage): F-3 cadence governor — implemented Story 005 / AC-WS-14.
     * Stage 4 (pattern draw): DrawBarragePattern / DrawNonBarragePattern — implemented Story 005 / AC-WS-15.
     *   AcquireFromPool integration and real WaveIds are deferred to Story 006 (NextWaveIdCounter stub used).
     *
     * Returns EAdmissionResult:
     *   Admitted              — slot(s) pre-committed; ScheduledSlots updated.
     *   Deferred_ConcurrencyCap — non-barrage path: available == 0.
     *   Deferred_SlotAtomic   — barrage path: available < 3 AND fallback also fails (== 0).
     *   PoolExhausted         — structurally impossible in this story; never returned here.
     *   Deferred_BarrageOwed  — unreachable in Story 004; reservation is fulfilled rather than
     *                            used to veto non-barrage draws. Story 005 scope.
     *
     * Story 004 / ADR-0011 D2 Stage 2. TRs: TR-WS-017, TR-WS-018.
     */
    EAdmissionResult TryAdmitPattern(const FDPCFrameState& FrameState);

    // =========================================================================
    // Stage 3–4: F-3 cadence governor + pattern draw (Story 005)
    // TRs: TR-WS-013, TR-WS-029, TR-WS-030, TR-WS-033, TR-WS-034
    // GDD: design/gdd/wave-spawner-pattern-library.md Rules 8, F-3
    // =========================================================================

    /**
     * Stage 3: F-3 cadence governor. Returns true if the next draw should be a barrage.
     * Only returns true during PEAK phase. Three-branch piecewise weight function:
     *   1. Force-draw  (TR-WS-030): BarrageCountThisPeak == 0 AND t_norm_PEAK >= kTForce → true
     *   2. Ceiling     (implicit):  BarrageCountThisPeak >= kTargetBarrages → false
     *   3. Proportional (TR-WS-029): w = kBaseW * (kTargetBarrages - count) * t_norm_PEAK
     *      (clamped [0, kWCeiling]); admits if PatternRNG.FRand() < w.
     *
     * Uses only FMath::Clamp + linear multiplication — no pow/exp/log/sqrt (TR-WS-034, AC-WS-14).
     * Declared const; PatternRNG is mutable to allow FRand() from a const method.
     * Uses GetCurrentTimeS() (test-injectable seam) — NOT GetWorld() directly (null-safe in tests).
     *
     * Story 005 / AC-WS-14.
     */
    bool ShouldDrawBarrage() const;

    /**
     * Stage 4: Draw one pattern from the non-barrage sub-pool of the active phase pool.
     * Uniform random selection via PatternRNG.RandRange(0, Pool.Num()-1).
     * Empty-pool guard: logs warning and returns without crashing (AC-WS-15).
     * FrameState held for Story 006 AdmitPattern() integration (stub uses TODO comment).
     * Story 005 / AC-WS-15.
     */
    void DrawNonBarragePattern(const FDPCFrameState& FrameState);

    /**
     * Stage 4: Draw one pattern from PeakPool.BarragePatterns.
     * Uniform random selection via PatternRNG.RandRange(0, Pool.Num()-1).
     * Increments BarrageCountThisPeak by 1 on each successful draw.
     * Empty-pool guard: logs warning and returns without crashing (AC-WS-15).
     * FrameState held for Story 006 AdmitPattern() integration (stub uses TODO comment).
     * Story 005 / AC-WS-15.
     */
    void DrawBarragePattern(const FDPCFrameState& FrameState);

    // =========================================================================
    // Story 005: F-3 governor fields + RNG
    // =========================================================================

    /**
     * Deterministic RNG for pattern draw selection (Stage 4).
     * Seeded once at Cold→Active from RunSeed lower 32 bits (Story 007 supplies real value;
     * stub seeds with 0 until RSM integration — TR-WS-013, AC-WS-13).
     * Wall-clock-derived seeds (FDateTime::Now(), FMath::Rand()) are FORBIDDEN (AC-WS-13).
     * Declared mutable: FRandomStream::FRand() advances internal Seed, which is mutable in
     * UE 5.x. mutable allows calling FRand() from const ShouldDrawBarrage().
     * Verify FRandomStream::Seed mutable in UE 5.7 docs if compile errors arise here.
     */
    mutable FRandomStream PatternRNG;

    /**
     * Count of barrage patterns drawn in the current PEAK phase.
     * Incremented by DrawBarragePattern(); reset to 0 at Cold→Active entry (Flushing→Cold resets too).
     * Drives F-3 cadence governor three-branch logic in ShouldDrawBarrage().
     * Story 005 / AC-WS-14, AC-WS-15.
     */
    int32 BarrageCountThisPeak = 0;

    /**
     * Game time (seconds) when the current PEAK phase began.
     * Used by ShouldDrawBarrage() to compute t_norm_PEAK = (Now - PeakEntryTimeS) / kPeakDurationS.
     * Set by Story 007 RSM integration when PEAK phase starts.
     * DEVIATION NOTE: stub = 0.f until Story 007 RSM integration; forces t_norm to measure
     * from run start rather than PEAK entry, underestimating the PEAK fraction.
     * Story 005 / AC-WS-14.
     */
    float PeakEntryTimeS = 0.f;

    // =========================================================================
    // Story 005: F-3 tuning constants — stub values, will be config-loaded in Story 007.
    //
    // DEVIATION NOTE (each constant): stub value — Story 007 replaces with config-loaded value
    // from UWaveSpawnerConfig data asset. Do NOT hardcode in gameplay logic; always reference
    // through these named constants so Story 007 can replace them with a single-line config read.
    // =========================================================================

    /** Base barrage weight fraction. Must satisfy kBaseW < kWCeiling (ADR-0011 D4).
     *  DEVIATION NOTE: stub — Story 007 loads from UWaveSpawnerConfig. */
    static constexpr float kBaseW          = 0.25f;

    /** Max weight cap for proportional branch clamping.
     *  DEVIATION NOTE: stub — Story 007 loads from UWaveSpawnerConfig. */
    static constexpr float kWCeiling       = 1.0f;

    /** t_norm_PEAK threshold at which force-draw activates (0 barrages drawn and elapsed fraction).
     *  DEVIATION NOTE: stub — Story 007 loads from UWaveSpawnerConfig. */
    static constexpr float kTForce         = 0.75f;

    /** Target average barrage events per PEAK phase (BARRAGE_EVENTS_PER_PEAK_TARGET_AVG).
     *  DEVIATION NOTE: stub — Story 007 loads from UWaveSpawnerConfig. */
    static constexpr int32 kTargetBarrages = 2;

    /** Approximate PEAK phase duration in seconds (60s run × ~35%).
     *  DEVIATION NOTE: stub — Story 007 loads from UWaveSpawnerConfig (or derives from RSM). */
    static constexpr float kPeakDurationS  = 21.0f;

    /**
     * Post-resume grace window duration in seconds (Rule 1 gate / TR-WS-012).
     * No admissions fire within this window after OnPausedChanged(false) fires.
     * Named constant here so config integration replaces a single declaration line.
     * DEVIATION NOTE: stub — Story 007 loads from UWaveSpawnerConfig. AC-WS-28.
     * TODO(RSM epic): Replace bInResumeGrace + kResumeGraceS with RSMSubsystem->IsResumeGrace()
     * once the RSM epic implements the shared grace window (ADR-0007). IsResumeGrace() is the
     * authoritative signal shared by Wave Spawner, Collision, and Player Movement (TR-RSM-008/030/031).
     * If kResumeGraceS diverges from RSM's actual duration before that refactor, veto windows desync.
     */
    static constexpr float kResumeGraceS   = 1.5f;

#if WITH_DEV_AUTOMATION_TESTS
    // Backing fields for the GetCurrentTimeS() test seam.
    // TestOnly_SetCurrentTimeOverride() activates the override; production builds
    // omit these fields entirely to keep the struct lean.
    float TestCurrentTimeOverrideS = 0.f;
    bool  bTestTimeOverrideActive  = false;

    // Backing field for TestOnly_GetLastDrawIndex().
    // Set by DrawNonBarragePattern() and DrawBarragePattern() after a successful RandRange call.
    // INDEX_NONE until first draw fires in this session; reset to INDEX_NONE in Cold case.
    // Story 005 / AC-WS-15.
    int32 LastDrawIndexForTest = INDEX_NONE;
#endif
};

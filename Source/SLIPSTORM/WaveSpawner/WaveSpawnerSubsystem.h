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
     *   Active  → Flushing  (RSM terminal: DEAD|COMPLETE|ABORTED)
     *   Holding → Flushing  (OnPausedChanged(false) with stale scheduled_slots)
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
     * exposes max_concurrent_waves. Story 005 removes this constant and reads from FrameState.
     *
     * Story 004 / ADR-0011 D2 Stage 2. TRs: TR-WS-017, TR-WS-018.
     */
    int32 GetAvailableSlots() const;

    /**
     * Implements ADR-0011 D2 Stage 2 — Rule 7 slot pre-commitment (TR-WS-017, TR-WS-018).
     * Called from OnDPCFrameReady() after all Rule 1 gates and cadence gate pass.
     *
     * Stage 3 (ShouldDrawBarrage) is a Story 005 stub (always false here).
     * Stage 4 (pattern draw + AcquireFromPool) is a Story 005 stub (NextWaveIdCounter used).
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

#if WITH_DEV_AUTOMATION_TESTS
    // Backing fields for the GetCurrentTimeS() test seam.
    // TestOnly_SetCurrentTimeOverride() activates the override; production builds
    // omit these fields entirely to keep the struct lean.
    float TestCurrentTimeOverrideS = 0.f;
    bool  bTestTimeOverrideActive  = false;
#endif
};

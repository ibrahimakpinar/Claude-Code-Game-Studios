// Copyright (c) Game Studio. All rights reserved.
// SLIPSTORM — 60-second voxel endless runner, mobile, Unreal Engine 5.7
//
// WaveSpawnerSubsystem.cpp — UWaveSpawnerSubsystem implementation.
//
// Story: production/epics/wave-spawner/story-001-subsystem-class-and-object-pool.md
// ADR:   docs/architecture/adr-0005-wave-spawner-subsystem-hosting.md
// TRs:   TR-WS-008, TR-WS-009, TR-WS-010, TR-WS-011

#include "WaveSpawner/WaveSpawnerSubsystem.h"
#include "WaveSpawner/Wave.h"
#include "RunStateMachine/RunStateMachineSubsystem.h"
#include "DPC/DPCSubsystem.h"
#include "UObject/CoreDelegates.h"
#include "Engine/World.h"

// ---------------------------------------------------------------------------
// Log category for WaveSpawner pool diagnostics.
// Note: DECLARE_STATS_GROUP(STATGROUP_WaveSpawner) lives in WaveSpawnerTypes.h
// (included transitively via WaveSpawnerSubsystem.h) so all TUs that include
// the subsystem header see the group declaration before the inline GetStatId().
// ---------------------------------------------------------------------------
DEFINE_LOG_CATEGORY_STATIC(LogWaveSpawner, Log, All);

// ---------------------------------------------------------------------------
// Stage 2 concurrency cap stub (Story 004 / ADR-0011 D2).
//
// DEVIATION NOTE: ADR-0011 D2 Stage 1 reads max_concurrent_waves from FDPCFrameState.
// That field does not yet exist in the DPC stub (DPCSubsystem.h — DPC epic, TODO).
// 23 is the pool size (the structural hard ceiling, per TR-WS-008). The real DPC-
// published concurrency cap (MAX_CONCURRENT_WAVES_CAP = 16 in PullWaveTypes.h,
// TR-DPC-011) will replace this stub when FDPCFrameState.max_concurrent_waves is
// implemented. Story 005 removes this constant and reads from FrameState directly.
// ---------------------------------------------------------------------------
static constexpr int32 kMaxConcurrentWavesStub = 23;

// ============================================================================
// Initialize / Deinitialize
// ============================================================================

void UWaveSpawnerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // --- Ordering pins (ADR-0005 IG-8, INT-004 amendment 2026-06-26) ---
    // RSM and DPC must complete their Initialize() before this body proceeds.
    // Called as statements (return type may be void in UE 5.7 — use GetSubsystem
    // below to retrieve the fully-initialized pointer). Without these pins,
    // GetSubsystem<UDPCSubsystem>() may return a partially-initialized instance,
    // silently dropping the admission delegate bind.
    // Forbidden Pattern: WaveSpawner_Initialize_without_InitializeDependency_on_RSM_and_DPC
    Collection.InitializeDependency(URunStateMachineSubsystem::StaticClass());
    Collection.InitializeDependency(UDPCSubsystem::StaticClass());
    UDPCSubsystem* DPC = GetGameInstance()->GetSubsystem<UDPCSubsystem>();
    // DPC must be non-null: InitializeDependency() above guarantees it is initialized
    // before this line executes (ADR-0005 IG-8). A null result indicates a subsystem
    // registration error in the project settings, not a runtime condition.
    check(DPC);

    // --- Pool pre-allocation deferred to first world load (ADR-0005 IG-1) ---
    // UWorld does not exist at UGameInstanceSubsystem::Initialize() in UE 5.7.
    // SpawnActor here would assert. Pool allocation fires at PostLoadMapWithWorld
    // (R2a-4 lifecycle correction). PostLoadMapWithWorld fires after UWorld is fully
    // initialized and SpawnActor is legal.
    PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(
        this, &UWaveSpawnerSubsystem::OnFirstWorldLoaded);

    // --- DPC admission delegate bind (ADR-0005 R2a-2) ---
    // DPC is fully initialized by InitializeDependency above (VR-5 closed).
    // CastChecked guarantees non-null; this line is the structural tick-ordering pin:
    // DPC publishes FDPCFrameState → OnDPCFrameReady fires → admission evaluates.
    DPCFrameReadyHandle = DPC->OnPostTickFrameStatePublished.AddUObject(
        this, &UWaveSpawnerSubsystem::OnDPCFrameReady);
}

void UWaveSpawnerSubsystem::Deinitialize()
{
    // Remove PostLoadMapWithWorld subscription. Remove() on an invalid handle is a no-op.
    FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);

    // Remove DPC admission delegate subscription.
    // Null-check GetGameInstance() to allow safe Deinitialize() on subsystem objects
    // constructed via NewObject<>(GetTransientPackage()) in unit tests (no GI outer).
    // In production, GetGameInstance() is always valid at Deinitialize() (VR-4 verified).
    if (UGameInstance* GI = GetGameInstance())
    {
        if (UDPCSubsystem* DPC = GI->GetSubsystem<UDPCSubsystem>())
        {
            DPC->OnPostTickFrameStatePublished.Remove(DPCFrameReadyHandle);
        }
    }

    // Release pool GC anchors. AWave actors are collected by UE GC once
    // UPROPERTY TObjectPtr refs are cleared (ADR-0005 IG-6). No Destroy() calls.
    Pool.Reset();

    Super::Deinitialize();
}

// ============================================================================
// Pool pre-allocation (fires at PostLoadMapWithWorld — once per session)
// ============================================================================

void UWaveSpawnerSubsystem::OnFirstWorldLoaded(UWorld* LoadedWorld)
{
    // Guard 1 — null-world broadcast (VR-2 engine-verified):
    // FPostLoadMapCaller's destructor in UnrealEngine.cpp broadcasts
    // PostLoadMapWithWorld with nullptr on load abort/error. Ignore these.
    if (!LoadedWorld)
    {
        return;
    }

    // Guard 2 — one-shot guard (ADR-0005 IG-3, Risk 1):
    // Prevents re-allocation on edge-case world loads (seamless travel, DLC).
    // Replay re-entry (Flushing → Cold → Active, EC-WS-8) does NOT re-fire
    // PostLoadMapWithWorld on world re-use — this guard is belt-and-suspenders.
    if (bPoolAllocated)
    {
        return;
    }

    // Set flag BEFORE the SpawnActor loop to prevent re-entry if the loop
    // somehow triggers additional delegate broadcasts (defensive, per VR-2 analysis).
    bPoolAllocated = true;

    // Pre-allocate 23 AWave actor instances (TR-WS-008, TR-PW-010).
    // Pool sizing rationale: 16 PEAK concurrency cap + 2 DESPAWNING latency slots
    // + 5 margin = 23. SpawnActor is legal here — LoadedWorld is valid and fully init'd.
    // After this point, no SpawnActor or Destroy calls occur during gameplay (Rule 11).
    Pool.Reserve(23);
    for (int32 i = 0; i < 23; ++i)
    {
        AWave* Wave = LoadedWorld->SpawnActor<AWave>();
        // All 23 spawns must succeed. A failure here indicates a world or asset
        // configuration error, not a runtime pool-exhaustion condition.
        check(Wave);
        Wave->WaveId = INDEX_NONE; // Unacquired; WaveId assigned at AcquireFromPool().
        Wave->bInUse = false;
        Pool.Add(Wave);
    }

    UE_LOG(LogWaveSpawner, Log,
        TEXT("UWaveSpawnerSubsystem: Pool pre-allocated — 23 AWave actors (TR-WS-008). "
             "Pool persists across all death-replay cycles (EC-WS-8, ADR-0005)."));
}

// ============================================================================
// FTickableGameObject interface
// ============================================================================

void UWaveSpawnerSubsystem::Tick(float DeltaTime)
{
    // Thread safety assertion (ADR-0005 Risk mitigation; VR-3 engine-verified).
    // FTickableGameObject::TickObjects() has a hard check(IsInGameThread()) engine-side;
    // this assertion adds an explicit guard at the entry point for diagnostic clarity.
#if !UE_BUILD_SHIPPING
    check(IsInGameThread());
#endif

    // Rate-limit timer state bookkeeping — see Story 009.
    // ADMISSION LOGIC MUST NOT BE PLACED HERE (ADR-0005 IG-4).
    // Admission fires in OnDPCFrameReady() — placing it here would break the
    // structural DPC tick-ordering guarantee (RSM → DPC → [delegate fires] → admission).
    (void)DeltaTime;
}

bool UWaveSpawnerSubsystem::IsTickable() const
{
    // Suppress tick in Cold and Idle states to avoid idle CPU overhead.
    // ETickableTickType::Conditional defers to this guard each frame.
    // Full lifecycle state machine (Cold → Active transitions) is Story 002.
    return (LifecycleState != EWaveSpawnerLifecycleState::Cold)
        && (LifecycleState != EWaveSpawnerLifecycleState::Idle);
}

// ============================================================================
// DPC admission pipeline callback (stub — full implementation in Stories 003–007)
// ============================================================================

void UWaveSpawnerSubsystem::OnDPCFrameReady(const FDPCFrameState& FrameState)
{
    // Thread safety assertion (ADR-0005 Risk mitigation; VR-3 engine-verified).
#if !UE_BUILD_SHIPPING
    check(IsInGameThread());
#endif

    // -------------------------------------------------------------------------
    // Rule 1 gate — per-frame prerequisite checks (TR-WS-012).
    //
    // FrameState is the DPC snapshot passed as a function parameter.
    // Treated as immutable for this callback: no DPC state is re-read after
    // this point (ADR-0005 R2a-2 snapshot immutability contract).
    // -------------------------------------------------------------------------

    // (1a) DPC is_active: veto if DPC is not computing difficulty this frame.
    if (!FrameState.bIsActive)
    {
        return;
    }

    // (1b) Lifecycle guard: only Active state admits patterns.
    //      Holding and Flushing reach this callback because IsTickable() returns
    //      true for them, but they must not admit — veto here.
    //      (Cold/Idle are already suppressed by IsTickable() returning false.)
    if (LifecycleState != EWaveSpawnerLifecycleState::Active)
    {
        return;
    }

    // Hoist wall-clock read once — used for grace check, primer, and cadence gate.
    // Single call satisfies snapshot semantics within this callback invocation.
    const float Now = GetCurrentTimeS();

    // (1c) Resume grace window veto (TR-WS-012).
    //      bInResumeGrace is set by Story 007 OnResumeFromPause().
    //      Cleared lazily here when the window has expired.
    if (bInResumeGrace)
    {
        if (Now < ResumeGraceEndTimeS)
        {
            return;
        }
        bInResumeGrace = false;  // grace expired — clear lazily
    }

    // -------------------------------------------------------------------------
    // Primer bypass (Rule 2a / TR-WS-016):
    // On Cold→Active, the first draw admits immediately — cadence gate is skipped.
    // -------------------------------------------------------------------------
    if (bPrimerPending)
    {
        // Stage 2 (Story 004): slot pre-commitment still applies to the primer.
        // The primer bypasses the cadence gate (TR-WS-016) but not Rule 7 (TR-WS-017).
        LastAdmissionResult = TryAdmitPattern(FrameState);
        if (LastAdmissionResult == EAdmissionResult::Admitted)
        {
            LastSpawnTimeS = Now;
            bPrimerPending = false;
            UE_LOG(LogWaveSpawner, Log,
                TEXT("WaveSpawner: primer draw admitted at T=%.3f "
                     "(TR-WS-016; cadence gate bypassed)."),
                Now);
        }
        return;  // one admission attempt per DPC frame; exit after primer path
    }

    // -------------------------------------------------------------------------
    // F-3b cadence gate (TR-WS-015):
    // Pattern admission requires (Now − LastSpawnTimeS) >= WaveSpawnIntervalS.
    //
    // WaveSpawnIntervalS is sourced from FrameState (the DPC snapshot), NOT
    // hardcoded — satisfies the G.1 tuning knob requirement (ADR-0011).
    //
    // DEVIATION NOTE (AC-WS-11b): AC-WS-11b specifies that WaveSpawnIntervalS is
    // "loaded from a project config or data asset at Initialize()". The implementation
    // instead reads FrameState.WaveSpawnIntervalS from the DPC-published snapshot each
    // frame, consistent with DPCSubsystem.h (TR-WS-015 comment) and ADR-0011 which
    // list wave_spawn_interval_s among values sourced from FDPCFrameState.
    // The AC's grep criterion holds: WaveSpawnIntervalS appears at the comparison;
    // no hardcoded interval literal appears at the gate comparison site.
    // Story owner should reconcile AC-WS-11b text to reflect the DPC-snapshot
    // source rather than an Initialize()-time load.
    // -------------------------------------------------------------------------
    if ((Now - LastSpawnTimeS) < FrameState.WaveSpawnIntervalS)
    {
        return;  // cadence gate not elapsed; defer admission
    }

    // Cadence gate passed. Stage 2 (Story 004): slot pre-commitment (Rule 7 / TR-WS-017).
    // TODO Story 005 (Stage 4): DrawBarragePattern / DrawNonBarragePattern replace
    //                           stub WaveIds inside TryAdmitPattern.
    LastAdmissionResult = TryAdmitPattern(FrameState);
    if (LastAdmissionResult == EAdmissionResult::Admitted)
    {
        LastSpawnTimeS = Now;
    }
}

// ============================================================================
// Time accessor (Story 003 — test-injectable wall-clock seam)
// ============================================================================

float UWaveSpawnerSubsystem::GetCurrentTimeS() const
{
#if WITH_DEV_AUTOMATION_TESTS
    if (bTestTimeOverrideActive)
    {
        return TestCurrentTimeOverrideS;
    }
#endif
    UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
    return World ? World->GetTimeSeconds() : 0.f;
}

// ============================================================================
// Stage 2 slot accounting (Story 004 — ADR-0011 D2)
// TRs: TR-WS-017, TR-WS-018
// GDD: design/gdd/wave-spawner-pattern-library.md Rule 7
// ============================================================================

int32 UWaveSpawnerSubsystem::GetAvailableSlots() const
{
    // See DEVIATION NOTE in header: kMaxConcurrentWavesStub (23) is a conservative
    // placeholder for FDPCFrameState.max_concurrent_waves (not yet in the DPC stub).
    return kMaxConcurrentWavesStub - (ScheduledSlots.Num() + LiveSlots.Num());
}

EAdmissionResult UWaveSpawnerSubsystem::TryAdmitPattern(const FDPCFrameState& FrameState)
{
    // FrameState is unused by the stub; Story 005 ShouldDrawBarrage(FrameState) consumes it.
    // Matches (void)DeltaTime precedent in Tick(). Suppresses UBT unused-parameter warning.
    (void)FrameState;

    // Stage 3 / Story 005 stub: ShouldDrawBarrage() governs whether a barrage is
    // the intended draw. Until Story 005 implements the cadence governor F-3, this
    // is always false. The barrage path is therefore entered only when bBarrageOwed=true
    // (a prior drop set the reservation flag per TR-WS-018).
    const bool bBarrageIntended = false; // TODO Story 005: ShouldDrawBarrage(FrameState)

    // Cache available slot count once. ScheduledSlots/LiveSlots are not mutated until
    // a successful admission, so this value is stable for the duration of this call.
    // Avoids calling GetAvailableSlots() (two TSet::Num() calls) multiple times.
    const int32 Available = GetAvailableSlots();

    if (bBarrageOwed || bBarrageIntended)
    {
        // -----------------------------------------------------------------
        // Barrage path: requires 3 slots atomically (Rule 7 / TR-WS-017).
        // -----------------------------------------------------------------
        if (Available >= 3)
        {
            // Admit barrage: pre-commit 3 slots.
            // TODO Story 005 (Stage 4): DrawBarragePattern — pull from PeakPool.BarragePatterns
            //   and call Wave->InitializeFromSnapshot(). Real WaveIds from AcquireFromPool()
            //   replace these stub counter values at that point.
            ScheduledSlots.Add(NextWaveIdCounter++);
            ScheduledSlots.Add(NextWaveIdCounter++);
            ScheduledSlots.Add(NextWaveIdCounter++);
            bBarrageOwed = false;  // reservation fulfilled (AC-WS-12c / TR-WS-018)
            return EAdmissionResult::Admitted;
        }

        // Barrage dropped: insufficient slots for the required atomic 3-slot pre-commitment.
        bBarrageOwed = true;  // reserve next available >=3 window (TR-WS-018)

        // Telemetry call site (AC-WS-12b / TR-WS-019):
        // barrage_dropped_due_to_concurrency event — infrastructure deferred to Story 009.
        /* TODO Story 009: EmitTelemetry(TEXT("barrage_dropped_due_to_concurrency"), ...); */
        UE_LOG(LogWaveSpawner, Verbose,
            TEXT("WaveSpawner: barrage dropped — available=%d < 3; bBarrageOwed=true (TR-WS-018). "
                 "/* TODO Story 009: emit barrage_dropped_due_to_concurrency */"),
            Available);

        // Non-barrage fallback: a single-slot admission is better than nothing (AC-WS-12b).
        // TODO Story 005 (Stage 4): DrawNonBarragePattern — pull from active pool.
        if (Available >= 1)
        {
            ScheduledSlots.Add(NextWaveIdCounter++);
            return EAdmissionResult::Admitted;
        }

        // No slots available for even the non-barrage fallback.
        return EAdmissionResult::Deferred_SlotAtomic;
    }

    // -----------------------------------------------------------------
    // Non-barrage path: requires 1 slot (Rule 7 / TR-WS-017).
    // -----------------------------------------------------------------
    // TODO Story 005 (Stage 4): DrawNonBarragePattern — pull from active pool.
    if (Available >= 1)
    {
        ScheduledSlots.Add(NextWaveIdCounter++);
        return EAdmissionResult::Admitted;
    }

    // No slots available: concurrency cap hit (structural limit kMaxConcurrentWavesStub).
    return EAdmissionResult::Deferred_ConcurrencyCap;
}

// ============================================================================
// Pool acquire / release
// ============================================================================

AWave* UWaveSpawnerSubsystem::AcquireFromPool()
{
    for (TObjectPtr<AWave>& Slot : Pool)
    {
        if (Slot && !Slot->bInUse)
        {
            Slot->bInUse = true;
            return Slot.Get();
        }
    }

    // All 23 slots in use. The admission gate's concurrency cap (max_concurrent_waves
    // from DPC, TR-WS-017) should prevent this in normal operation.
    UE_LOG(LogWaveSpawner, Warning,
        TEXT("UWaveSpawnerSubsystem::AcquireFromPool — pool exhausted (all 23 slots in use). "
             "Returning nullptr. Verify admission gate concurrency logic (TR-WS-017)."));
    return nullptr;
}

void UWaveSpawnerSubsystem::ReleaseToPool(int32 WaveId)
{
    for (TObjectPtr<AWave>& Slot : Pool)
    {
        if (Slot && Slot->WaveId == WaveId)
        {
            Slot->bInUse = false;
            Slot->WaveId = INDEX_NONE;
            return;
        }
    }

    // WaveId not found — double-release or ID mismatch. Despawn pipeline should
    // prevent this via the ordered Rule 13 sequence (TR-PW-016, TR-WS-026).
    UE_LOG(LogWaveSpawner, Warning,
        TEXT("UWaveSpawnerSubsystem::ReleaseToPool — WaveId %d not found in pool. "
             "Double-release or mismatched WaveId (TR-WS-026)."),
        WaveId);
}

// ============================================================================
// Lifecycle state machine (Story 002 — ADR-0011 D3)
// TRs: TR-WS-011, TR-WS-020, TR-WS-021, TR-WS-022, TR-WS-023
// ============================================================================

bool UWaveSpawnerSubsystem::IsValidTransition(
    EWaveSpawnerLifecycleState From,
    EWaveSpawnerLifecycleState To) const
{
    // Encodes exactly the 9 valid ADR-0011 D3 transitions.
    // All other (From, To) pairs are forbidden and return false.
    //
    // DEVIATION NOTE — Story AC-WS-15a lists 8 transitions including "Flushing→Idle"
    // and "Idle→Cold". ADR-0011 D3 is authoritative (per coordinator approval) and
    // defines 9 transitions that supersede those ACs:
    //   - Flushing→Cold   replaces "Flushing→Idle" (run-termination is a direct reset)
    //   - Holding→Idle    is the long-pause path (waves despawn naturally in Holding)
    //   - Idle→Active     replaces "Idle→Cold" (resume from long pause; Cold not needed)
    //   - "Flushing→Idle" is therefore FORBIDDEN.
    //   - "Idle→Cold"     is therefore FORBIDDEN.
    // The story file ACs will be reconciled by the story owner post-implementation.
    switch (From)
    {
    case EWaveSpawnerLifecycleState::Cold:
        // Cold → Active  (RSM COUNTDOWN→RUNNING fires Cold→Active; RunSeed captured)
        return To == EWaveSpawnerLifecycleState::Active;

    case EWaveSpawnerLifecycleState::Active:
        // Active → Holding  (phase-boundary drain-window entry)
        // Active → Flushing (RSM terminal state: DEAD|COMPLETE|ABORTED)
        return To == EWaveSpawnerLifecycleState::Holding
            || To == EWaveSpawnerLifecycleState::Flushing;

    case EWaveSpawnerLifecycleState::Holding:
        // Holding → Flushing (OnPausedChanged(false) with stale scheduled_slots — Rule 13)
        // Holding → Active   (drain window clears: CountInFlightWaves()==0 + no stale slots)
        // Holding → Idle     (long-pause path: all waves naturally despawned during Holding)
        return To == EWaveSpawnerLifecycleState::Flushing
            || To == EWaveSpawnerLifecycleState::Active
            || To == EWaveSpawnerLifecycleState::Idle;

    case EWaveSpawnerLifecycleState::Flushing:
        // Flushing → Cold   (run-termination: all live drained — NOT via Idle; Rule 14)
        // Flushing → Active (pause-flush complete: all stale scheduled drained — Rule 13)
        return To == EWaveSpawnerLifecycleState::Cold
            || To == EWaveSpawnerLifecycleState::Active;

    case EWaveSpawnerLifecycleState::Idle:
        // Idle → Active (RSM OnPausedChanged(false) resume from long pause; no flush)
        // FORBIDDEN: Idle→Cold is NOT valid per ADR-0011 D3 (story AC-WS-15a deviation).
        return To == EWaveSpawnerLifecycleState::Active;

    default:
        return false;
    }
}

void UWaveSpawnerSubsystem::TransitionTo(EWaveSpawnerLifecycleState NewState)
{
    const bool bValid = IsValidTransition(LifecycleState, NewState);

    if (!bValid)
    {
        // Log before asserting so the error is visible in crash context (VR-10 pattern).
        // UEnum::GetValueAsString() is the correct API for UENUM values in UE 5.7 (VR-9);
        // LexToString() has no generic UENUM overload in UE 5.7.
        UE_LOG(LogWaveSpawner, Error,
            TEXT("WaveSpawner: forbidden lifecycle transition %s -> %s (ignored). "
                 "See ADR-0011 D3 for the 9 valid transitions."),
            *UEnum::GetValueAsString(LifecycleState),
            *UEnum::GetValueAsString(NewState));

        // Non-Shipping: abort immediately — forbidden transitions are programmer errors.
        // Shipping: check(false) compiles to nothing; graceful return fires instead.
        check(false);
        return;
    }

    const EWaveSpawnerLifecycleState PrevState = LifecycleState;
    LifecycleState = NewState;
    OnLifecycleTransition(NewState);

    UE_LOG(LogWaveSpawner, Log,
        TEXT("WaveSpawner lifecycle: %s -> %s (Phase=%s, DrainWindow=%s)"),
        *UEnum::GetValueAsString(PrevState),
        *UEnum::GetValueAsString(NewState),
        *UEnum::GetValueAsString(ActivePhase),
        bDrainWindowActive ? TEXT("true") : TEXT("false"));
}

void UWaveSpawnerSubsystem::OnLifecycleTransition(EWaveSpawnerLifecycleState NewState)
{
    switch (NewState)
    {
    case EWaveSpawnerLifecycleState::Active:
        // Two entry paths into Active:
        //   Cold→Active:    ActiveDrawPool is null → initialize to OPENER pool (first run or
        //                   replay after Flushing→Cold reset). RunSeed capture (TR-WS-020)
        //                   is a Story 007 concern — integration seam reserved here.
        //   Holding→Active: pool pointer already advanced on Holding entry (see Holding case
        //                   below); no second swap. Admissions resume under the new pool.
        //   Flushing→Active: same as Holding→Active — pool pointer was set on prior Holding
        //                   entry; drain-window flag clears to re-enable admissions.
        if (ActiveDrawPool == nullptr)
        {
            ActiveDrawPool = &OpenerPool;
            ActivePhase    = ERunPhase::Opener;
            bPrimerPending = true;  // primer fires on first Cold→Active (TR-WS-016)
        }
        // Clear drain window on any Active entry — Rule 9 window has closed.
        bDrainWindowActive = false;
        break;

    case EWaveSpawnerLifecycleState::Holding:
        // Active→Holding: atomic pool-pointer swap (ADR-0011 D3 Rule 10 / TR-WS-021).
        // ONLY the raw pointer is reassigned — pattern data is NEVER copied (TR-WS-021).
        // In-flight waves continue under their admission-time snapshot parameters; NO
        // FWaveInFlightState fields are mutated during this swap (TR-WS-022).
        //
        // Phase advancement:
        //   Opener → Mid  (first phase boundary)
        //   Mid    → Peak (second phase boundary)
        //   Peak   stays  (no further phases; barrage_owed continues in PeakPool)
        //
        // TODO Story 007: bDrainWindowActive cannot currently distinguish a phase-boundary
        // drain from a pause-flush drain. If the pause-flush path needs different drain-complete
        // semantics, a cause parameter should be added to TransitionTo() or Holding entry.
        bDrainWindowActive = true;
        if (ActivePhase == ERunPhase::Opener)
        {
            ActiveDrawPool = &MidPool;
            ActivePhase = ERunPhase::Mid;
        }
        else if (ActivePhase == ERunPhase::Mid)
        {
            ActiveDrawPool = &PeakPool;
            ActivePhase = ERunPhase::Peak;
        }
        // Peak→Holding: no pool advancement; PeakPool remains the active draw pool.
        break;

    case EWaveSpawnerLifecycleState::Cold:
        // Flushing→Cold (run-termination path — ADR-0011 D3 Rule 14).
        // Resets all per-run phase state so the next Cold→Active starts clean.
        //
        // DEVIATION NOTE: Story AC-WS-15c described "Idle→Cold resets bDrainWindowActive
        // and ActivePhase". Per ADR-0011 D3 (authoritative), run-termination goes directly
        // Flushing→Cold — there is no intermediate Idle step on the termination path.
        // The reset fires here, not at a hypothetical "Idle→Cold" entry. Story file will
        // be reconciled by the story owner.
        ActiveDrawPool     = nullptr;
        ActivePhase        = ERunPhase::Opener;
        bDrainWindowActive = false;
        // Story 003: reset admission gate state for the next run (TR-WS-015, TR-WS-016).
        bPrimerPending      = false;
        LastSpawnTimeS      = 0.f;
        bInResumeGrace      = false;
        ResumeGraceEndTimeS = 0.f;
        // Story 004: reset Stage 2 slot accounting for the next run (TR-WS-017, TR-WS-018).
        ScheduledSlots.Reset();
        LiveSlots.Reset();
        bBarrageOwed         = false;
        NextWaveIdCounter    = 0;
        LastAdmissionResult  = EAdmissionResult::PoolExhausted;
        break;

    case EWaveSpawnerLifecycleState::Idle:
        // Holding→Idle (long-pause path — ADR-0011 D3).
        // All previously-live waves have naturally despawned during Holding
        // (no explicit flush needed). Drain window is implicitly complete.
        // On resume (RSM OnPausedChanged(false)), Idle→Active fires without flush (Story 007).
        bDrainWindowActive = false;
        break;

    case EWaveSpawnerLifecycleState::Flushing:
        // Entry from Active→Flushing or Holding→Flushing.
        // Drain-window state is preserved: Flushing→Cold will clear it on termination;
        // Flushing→Active clears it via the Active case above on pause-flush completion.
        // Full flush logic (drain live_slots via IWaveSpawnerCallback) is Story 007.
        break;

    default:
        break;
    }
}

int32 UWaveSpawnerSubsystem::CountInFlightWaves() const
{
    // Returns the count of pool slots with bInUse == true.
    // Used by Story 007 to fire Holding→Active when the drain window clears
    // (CountInFlightWaves() == 0 AND no stale scheduled_slots — TR-WS-023).
    int32 Count = 0;
    for (const TObjectPtr<AWave>& Slot : Pool)
    {
        if (Slot && Slot->bInUse)
        {
            ++Count;
        }
    }
    return Count;
}

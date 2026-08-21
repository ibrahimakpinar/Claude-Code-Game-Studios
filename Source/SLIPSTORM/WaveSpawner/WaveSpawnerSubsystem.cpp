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
// implemented. TODO: replace with FDPCFrameState.max_concurrent_waves when the DPC
// epic delivers that field (removes structural pool-size stub of 23 with design cap
// of 16 per TR-DPC-011).
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

    // --- RSM delegate subscriptions (Story 007 / AC-WS-17, AC-WS-18, AC-WS-28) ---
    // RSM is guaranteed initialized by InitializeDependency above (ADR-0005 IG-8).
    // check() is appropriate: a null RSM here indicates a subsystem registration error
    // in project settings, not a runtime condition (same reasoning as the DPC check above).
    RSMSubsystem = GetGameInstance()->GetSubsystem<URunStateMachineSubsystem>();
    check(RSMSubsystem);
    RSMPausedHandle = RSMSubsystem->OnPausedChanged.AddUObject(
        this, &UWaveSpawnerSubsystem::HandlePausedChanged);
    RSMStateHandle = RSMSubsystem->OnStateChanged.AddUObject(
        this, &UWaveSpawnerSubsystem::HandleRunStateChanged);
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

    // Remove RSM delegate subscriptions (Story 007).
    // RSMSubsystem may be null in headless tests (NewObject<> without GameInstance);
    // null-check mirrors the DPC cleanup pattern above.
    if (RSMSubsystem)
    {
        RSMSubsystem->OnPausedChanged.Remove(RSMPausedHandle);
        RSMSubsystem->OnStateChanged.Remove(RSMStateHandle);
        RSMSubsystem = nullptr;
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

    // Cadence gate passed. Stage 2–4 pipeline (Story 004–005):
    // TryAdmitPattern: slot pre-commitment (Rule 7 / TR-WS-017) + ShouldDrawBarrage (F-3 governor)
    // + Draw*Pattern (pool draw). AcquireFromPool integration deferred to Story 006.
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
    // Stage 3: F-3 cadence governor (Story 005 / AC-WS-14).
    // Returns true only in PEAK phase via three-branch piecewise weight function.
    // May consume one PatternRNG.FRand() value in the proportional branch (mutable RNG).
    // FrameState passed to Draw*Pattern() in Stage 4 below; held for Story 006 integration.
    const bool bBarrageIntended = ShouldDrawBarrage();

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
            // Admit barrage: pre-commit 3 slots (Stage 2 / TR-WS-017).
            // Capture TelegraphWindowS snapshot per-wave at admission time (TR-WS-022, Story 007 AC-WS-21/29).
            // Real WaveIds from AcquireFromPool() replace these stub counter values in Story 006.
            const int32 Id0 = NextWaveIdCounter++;
            const int32 Id1 = NextWaveIdCounter++;
            const int32 Id2 = NextWaveIdCounter++;
            ScheduledSlots.Add(Id0);
            ScheduledSlots.Add(Id1);
            ScheduledSlots.Add(Id2);
            InFlightWaves.Add(Id0, { Id0, FrameState.TelegraphWindowS });
            InFlightWaves.Add(Id1, { Id1, FrameState.TelegraphWindowS });
            InFlightWaves.Add(Id2, { Id2, FrameState.TelegraphWindowS });
            bBarrageOwed = false;  // reservation fulfilled (AC-WS-12c / TR-WS-018)
            // Stage 4: draw from PeakPool.BarragePatterns; increments BarrageCountThisPeak (Story 005 / AC-WS-15).
            DrawBarragePattern(FrameState);
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
        if (Available >= 1)
        {
            const int32 FallbackId = NextWaveIdCounter++;
            ScheduledSlots.Add(FallbackId);
            // Snapshot TelegraphWindowS for this barrage-fallback wave (TR-WS-022, Story 007).
            InFlightWaves.Add(FallbackId, { FallbackId, FrameState.TelegraphWindowS });
            // Stage 4: barrage dropped; fallback draw from ActiveDrawPool.NonBarragePatterns (Story 005 / AC-WS-15).
            DrawNonBarragePattern(FrameState);
            return EAdmissionResult::Admitted;
        }

        // No slots available for even the non-barrage fallback.
        return EAdmissionResult::Deferred_SlotAtomic;
    }

    // -----------------------------------------------------------------
    // Non-barrage path: requires 1 slot (Rule 7 / TR-WS-017).
    // -----------------------------------------------------------------
    if (Available >= 1)
    {
        const int32 NonBarrageId = NextWaveIdCounter++;
        ScheduledSlots.Add(NonBarrageId);
        // Snapshot TelegraphWindowS for this non-barrage wave (TR-WS-022, Story 007 AC-WS-21/29).
        InFlightWaves.Add(NonBarrageId, { NonBarrageId, FrameState.TelegraphWindowS });
        // Stage 4: draw from ActiveDrawPool.NonBarragePatterns (Story 005 / AC-WS-15).
        DrawNonBarragePattern(FrameState);
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
        //
        // Cold→Active DETECTION: ActiveDrawPool == nullptr is the reliable proxy because only
        //   Flushing→Cold resets it to nullptr (Cold case below). Idle→Active intentionally
        //   does NOT re-seed PatternRNG: Idle is a mid-run long-pause; resume must continue
        //   the same draw sequence to preserve Death Replay integrity (TR-WS-013). Re-seeding
        //   here would corrupt the replay draw sequence from the Idle entry point onward.
        if (ActiveDrawPool == nullptr)
        {
            ActiveDrawPool = &OpenerPool;
            ActivePhase    = ERunPhase::Opener;
            bPrimerPending = true;  // primer fires on first Cold→Active (TR-WS-016)

            // Story 007: seed PatternRNG from RunSeed captured at Cold→Active (TR-WS-013, AC-WS-13).
            // RunSeed is set by HandleRunStateChanged BEFORE TransitionTo(Active) fires, so
            // this seeding site always sees the correct value — no double-init risk (Story 007).
            // RSM epic delivers the real RunSeed via GetRunSeed(); stub returns 0 until then.
            PatternRNG.Initialize(static_cast<int32>(RunSeed & 0xFFFFFFFF));
            BarrageCountThisPeak = 0;
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
            ActivePhase    = ERunPhase::Peak;
            // Story 007: record PEAK entry time for ShouldDrawBarrage() t_norm_PEAK computation.
            // Uses GetCurrentTimeS() (test-injectable seam) — null-safe headless, consistent
            // with ShouldDrawBarrage()'s own time reads. Retires the "stub = 0.f until Story 007"
            // DEVIATION NOTE in the header (TR-WS-020 / Story 005 AC-WS-14).
            PeakEntryTimeS = GetCurrentTimeS();
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
        // Story 005: reset F-3 cadence governor state for the next run (AC-WS-14, AC-WS-15).
        BarrageCountThisPeak = 0;
        PeakEntryTimeS       = 0.f;
        PatternRNG           = FRandomStream();  // reinitialise to default; seed set at Cold→Active entry
        // Story 007: clear per-wave in-flight snapshots for the next run (TR-WS-022).
        InFlightWaves.Reset();
#if WITH_DEV_AUTOMATION_TESTS
        LastDrawIndexForTest = INDEX_NONE;
#endif
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

// ============================================================================
// Stage 3: F-3 cadence governor (Story 005 — ADR-0011 D2 Stage 3)
// TRs: TR-WS-029, TR-WS-030, TR-WS-033, TR-WS-034
// GDD: design/gdd/wave-spawner-pattern-library.md Rule F-3
// ============================================================================

bool UWaveSpawnerSubsystem::ShouldDrawBarrage() const
{
    // F-3 cadence governor: barrages only in PEAK phase (quick-exit for non-PEAK).
    if (ActivePhase != ERunPhase::Peak) return false;

    // Compute t_norm_PEAK in [0, 1]: fraction of PEAK elapsed.
    // Uses GetCurrentTimeS() (test-injectable seam — TR-WS-033 null-safety requirement).
    // PeakEntryTimeS is set by Story 007 RSM integration; stub = 0.f (TR-WS-020).
    //
    // DEVIATION NOTE (TR-WS-033 / AC-WS-13 Death Replay): tNormPeak is derived from
    // GetCurrentTimeS() - PeakEntryTimeS, which varies between a live run and its Death Replay.
    // A timing difference that shifts this call across the force-draw threshold alters
    // FRand() consumption and desyncs the pool draw sequence from that point forward.
    // Resolution requires DPC to publish a replay-stable logical time field in
    // FDPCFrameState (Story 007 + DPC epic). Tracked: ADR-0011 amendment pending.
    const float Now       = GetCurrentTimeS();
    const float tNormPeak = FMath::Clamp(
        kPeakDurationS > 0.f ? (Now - PeakEntryTimeS) / kPeakDurationS : 0.f,
        0.f, 1.f);

    // Branch 1 — Force-draw (TR-WS-030):
    // Zero barrages drawn and elapsed fraction >= T_FORCE → must draw barrage to guarantee
    // BARRAGE_EVENTS_PER_PEAK_TARGET_AVG is met. Does NOT call FRand() (no RNG consumed).
    if (BarrageCountThisPeak == 0 && tNormPeak >= kTForce)
        return true;

    // Branch 2 — Ceiling:
    // Already met or exceeded target → no more barrages this PEAK. Does NOT call FRand().
    if (BarrageCountThisPeak >= kTargetBarrages)
        return false;

    // Branch 3 — Proportional (TR-WS-029):
    // Weight scales with remaining deficit and PEAK progress.
    // Formula (AC-WS-14, Formula B / Deficit variant):
    //   w = kBaseW * (TARGET - count) * t_norm_PEAK  (clamped to [0, kWCeiling])
    //
    // DEVIATION NOTE: AC-WS-14 text originally read `(TARGET / max(count, 1)) * t_norm`;
    //   corrected to `(TARGET - count) * t_norm` per Implementation Notes and TC7 (2026-08-19).
    //   The Deficit variant is self-consistent with the ceiling branch: w → 0 as count → TARGET.
    //
    // Arithmetic uses only FMath::Clamp + linear multiplication.
    // No pow(), exp(), log(), or sqrt() — required for ARM/x86 determinism (TR-WS-034, AC-WS-14).
    const float Deficit  = static_cast<float>(kTargetBarrages - BarrageCountThisPeak);
    const float WBarrage = FMath::Clamp(kBaseW * Deficit * tNormPeak, 0.f, kWCeiling);

    // PatternRNG.FRand() is const-callable: FRandomStream::Seed is mutable in UE 5.x.
    // mutable PatternRNG in the header declaration allows this in a const method.
    // Strict less-than: FRand() returning exactly WBarrage does NOT admit a barrage (AC-WS-14 edge).
    return PatternRNG.FRand() < WBarrage;
}

// ============================================================================
// Stage 4: Pattern draw (Story 005 — ADR-0011 D2 Stage 4)
// TRs: TR-WS-013, TR-WS-029
// GDD: design/gdd/wave-spawner-pattern-library.md Rule 8 (uniform random draw)
// ============================================================================

void UWaveSpawnerSubsystem::DrawNonBarragePattern(const FDPCFrameState& FrameState)
{
    // Null-pool guard: ActiveDrawPool may be null in Cold state or before first Cold→Active.
    if (!ActiveDrawPool)
    {
        /* TODO Story 009: EmitTelemetry(TEXT("empty_pool_at_draw"), TEXT("null_active_draw_pool")); */
        UE_LOG(LogWaveSpawner, Warning,
            TEXT("WaveSpawner: DrawNonBarragePattern — ActiveDrawPool is null (AC-WS-15). "
                 "/* TODO Story 009: emit empty_pool_at_draw */"));
        return;
    }

    const TArray<FPatternDefinition>& Pool = ActiveDrawPool->NonBarragePatterns;
    if (Pool.Num() == 0)
    {
        /* TODO Story 009: EmitTelemetry(TEXT("empty_pool_at_draw"), TEXT("non_barrage_pool_empty")); */
        UE_LOG(LogWaveSpawner, Warning,
            TEXT("WaveSpawner: DrawNonBarragePattern — NonBarragePatterns pool is empty (AC-WS-15). "
                 "/* TODO Story 009: emit empty_pool_at_draw */"));
        return;
    }

    // Uniform random selection — deterministic from PatternRNG seed (TR-WS-013, AC-WS-15).
    const int32 Index = PatternRNG.RandRange(0, Pool.Num() - 1);

#if WITH_DEV_AUTOMATION_TESTS
    LastDrawIndexForTest = Index;
#endif

    // TODO Story 006: AdmitPattern(Pool[Index], FrameState, GetCurrentTimeS(), false /*bBarrage*/);
    UE_LOG(LogWaveSpawner, Verbose,
        TEXT("WaveSpawner: DrawNonBarragePattern — drew index %d / %d (AC-WS-15). "
             "/* TODO Story 006: AdmitPattern */"),
        Index, Pool.Num() - 1);
    (void)FrameState;  // consumed by Story 006 AdmitPattern(); suppress unused-param until then
}

void UWaveSpawnerSubsystem::DrawBarragePattern(const FDPCFrameState& FrameState)
{
    const TArray<FPatternDefinition>& Pool = PeakPool.BarragePatterns;
    if (Pool.Num() == 0)
    {
        /* TODO Story 009: EmitTelemetry(TEXT("empty_pool_at_draw"), TEXT("barrage_pool_empty")); */
        UE_LOG(LogWaveSpawner, Warning,
            TEXT("WaveSpawner: DrawBarragePattern — BarragePatterns pool is empty (AC-WS-15). "
                 "/* TODO Story 009: emit empty_pool_at_draw */"));
        return;
    }

    // Uniform random selection — deterministic from PatternRNG seed (TR-WS-013, AC-WS-15).
    const int32 Index = PatternRNG.RandRange(0, Pool.Num() - 1);

#if WITH_DEV_AUTOMATION_TESTS
    LastDrawIndexForTest = Index;
#endif

    // TODO Story 006: AdmitPattern(Pool[Index], FrameState, GetCurrentTimeS(), true /*bBarrage*/);
    // Increment AFTER the draw so a draw failure (empty pool guard above) does not inflate count.
    BarrageCountThisPeak++;
    UE_LOG(LogWaveSpawner, Verbose,
        TEXT("WaveSpawner: DrawBarragePattern — drew index %d / %d; BarrageCountThisPeak=%d (AC-WS-15). "
             "/* TODO Story 006: AdmitPattern */"),
        Index, Pool.Num() - 1, BarrageCountThisPeak);
    (void)FrameState;  // consumed by Story 006 AdmitPattern(); suppress unused-param until then
}

// ============================================================================
// Story 007: RSM event handlers — Rule 13 pause-flush, Rule 14 run-termination
// ADR:   docs/architecture/adr-0011-wave-spawner-pattern-library.md (D3 Rules 13, 14)
// ADR:   docs/architecture/adr-0007-run-state-machine-hosting.md (OnPausedChanged, OnStateChanged)
// TRs:   TR-WS-013, TR-WS-025, TR-WS-027, TR-WS-028
// Story: production/epics/wave-spawner/story-007-rsm-dpc-integration.md
// ============================================================================

void UWaveSpawnerSubsystem::HandlePausedChanged(bool bIsPaused, double /*Timestamp*/)
{
    if (bIsPaused)
    {
        // Rule 13 pause-flush guard: only valid from Active or Holding lifecycle states.
        // Broadcast received in Cold/Idle/Flushing is a no-op (race or spurious delivery).
        if (LifecycleState != EWaveSpawnerLifecycleState::Active
         && LifecycleState != EWaveSpawnerLifecycleState::Holding)
        {
            UE_LOG(LogWaveSpawner, Warning,
                TEXT("HandlePausedChanged(true): received in lifecycle %s — flush skipped "
                     "(expected Active|Holding). AC-WS-17."),
                *UEnum::GetValueAsString(LifecycleState));
            return;
        }

        // Rule 13 pause-flush: drain all in-flight LiveSlots in WaveId ASC order (AC-WS-17).
        // DO NOT reset LastSpawnTimeS — TR-WS-027 mandates preservation across pause/resume.
        for (int32 WaveId : GetInFlightWaveIdsSorted())
        {
            DespawnWave(WaveId, EWaveDespawnReason::PauseFlush);
        }
        // Remove InFlightWaves entries for pre-committed scheduled waves (B-1 fix).
        // ScheduledSlots holds wave IDs admitted by TryAdmitPattern() before going live.
        // DespawnWave() is NOT called (no pool actor associated), but InFlightWaves entries
        // must be removed to prevent orphan accumulation across pause/resume cycles.
        // Without this, InFlightWaves.Num() inflates by up to 3 entries per barrage pause event
        // and only clears at run termination (OnLifecycleTransition(Cold) InFlightWaves.Reset()).
        PurgeScheduledInFlightEntries();
        // Clear ScheduledSlots — pre-committed but not yet live; no DespawnWave needed
        // (no pool actor is associated yet). Rule 13 scheduling cleanup (Story 007 / TR-WS-025).
        ScheduledSlots.Reset();

        // DEVIATION NOTE (Rule 13 / ADR-0011 D3): The TransitionTo() doc comment attributes
        // Active→Flushing to RSM terminal states only and describes the pause path as routing
        // through Holding first (Holding→Flushing per Rule 13). The implementation goes
        // Active|Holding → Flushing directly, skipping Holding, because live waves must not
        // persist on screen during a pause overlay (UX correctness). This deviation was
        // coordinator-approved alongside AC-WS-18 (ADR-0011 D3, code-review 2026-08-21).
        // The IsValidTransition() Active→Flushing comment has been reconciled to include the
        // pause-flush trigger.
        // Transition: Active|Holding → Flushing (pause drain complete).
        // Resume fires Flushing → Active via HandlePausedChanged(false) below.
        TransitionTo(EWaveSpawnerLifecycleState::Flushing);
    }
    else
    {
        // Resume guard: only valid from Flushing, Holding, or Idle states.
        // Spurious OnPausedChanged(false) while already Active is a no-op (guards against
        // spurious delegate delivery without check(false) on an externally-broadcast path).
        if (LifecycleState != EWaveSpawnerLifecycleState::Flushing
         && LifecycleState != EWaveSpawnerLifecycleState::Holding
         && LifecycleState != EWaveSpawnerLifecycleState::Idle)
        {
            UE_LOG(LogWaveSpawner, Warning,
                TEXT("HandlePausedChanged(false): received in lifecycle %s — resume skipped "
                     "(expected Flushing|Holding|Idle). AC-WS-28."),
                *UEnum::GetValueAsString(LifecycleState));
            return;
        }

        // Set resume grace window: Rule 1 gate in OnDPCFrameReady vetoes admissions until
        // GetCurrentTimeS() >= ResumeGraceEndTimeS (cleared lazily on first post-grace tick).
        OnResumeFromPause();
        // Transition: Flushing|Holding|Idle → Active (admissions resume after grace expires).
        TransitionTo(EWaveSpawnerLifecycleState::Active);
    }
}

void UWaveSpawnerSubsystem::HandleRunStateChanged(
    ERunState /*PreviousState*/, ERunState NewState,
    ERunOutcome /*Outcome*/, double /*Timestamp*/)
{
    if (NewState == ERunState::RUNNING)
    {
        // Cold→Active: only valid from Cold lifecycle state.
        // Guard prevents RNG re-seeding on duplicate RUNNING broadcasts (shouldn't occur
        // per ADR-0007, but defensive guard avoids TR-WS-013 desync on spurious delivery).
        if (LifecycleState != EWaveSpawnerLifecycleState::Cold)
        {
            UE_LOG(LogWaveSpawner, Warning,
                TEXT("HandleRunStateChanged: RUNNING received in lifecycle %s — Cold→Active skipped "
                     "(expected Cold)."),
                *UEnum::GetValueAsString(LifecycleState));
            return;
        }

        // Capture RunSeed BEFORE TransitionTo(Active).
        // OnLifecycleTransition(Active) calls PatternRNG.Initialize(RunSeed & 0xFFFFFFFF).
        // Setting RunSeed after TransitionTo would seed with stale 0 (double-init risk, Story 007).
        RunSeed = RSMSubsystem ? RSMSubsystem->GetRunSeed() : 0;
        TransitionTo(EWaveSpawnerLifecycleState::Active);  // Cold → Active
        return;
    }

    const bool bTerminating = (NewState == ERunState::DEAD
                             || NewState == ERunState::COMPLETE
                             || NewState == ERunState::ABORTED);
    if (!bTerminating)
    {
        // RESOLVING and other intermediate RSM states: no wave-spawner action (ADR-0007 R3a).
        return;
    }

    // Rule 14 run-termination guard: only flush from Active or Holding states.
    if (LifecycleState != EWaveSpawnerLifecycleState::Active
     && LifecycleState != EWaveSpawnerLifecycleState::Holding)
    {
        UE_LOG(LogWaveSpawner, Warning,
            TEXT("HandleRunStateChanged: terminal state %s received in lifecycle %s — flush skipped "
                 "(expected Active|Holding)."),
            *UEnum::GetValueAsString(NewState),
            *UEnum::GetValueAsString(LifecycleState));
        return;
    }

    // Rule 14 run-termination: despawn all in-flight waves in WaveId ASC order (AC-WS-18).
    for (int32 WaveId : GetInFlightWaveIdsSorted())
    {
        DespawnWave(WaveId, EWaveDespawnReason::RunTermination);
    }
    // Remove InFlightWaves entries for scheduled waves (defense-in-depth; symmetric with pause path).
    // OnLifecycleTransition(Cold) would also clear InFlightWaves via Reset(), but calling
    // PurgeScheduledInFlightEntries() here makes the intent explicit at both flush call sites.
    PurgeScheduledInFlightEntries();
    // Clear pre-committed scheduled slots — no pool actor associated, no DespawnWave needed.
    ScheduledSlots.Reset();
    // Clear barrage reservation — run is over; no next window to reserve (AC-WS-18, TR-WS-025).
    bBarrageOwed = false;

    // DEVIATION NOTE (AC-WS-18): Story AC-WS-18 specifies lifecycle → Idle after run termination.
    // ADR-0011 D3 is authoritative (same coordinator-approved precedent as the IsValidTransition
    // DEVIATION NOTE): Flushing→Idle is FORBIDDEN; run-termination resets to Cold.
    // OnLifecycleTransition(Cold) also resets bBarrageOwed, ScheduledSlots, and LiveSlots,
    // satisfying AC-WS-18's substantive checks (bBarrageOwed=false, Scheduled=0, Live=0).
    // AC-WS-18 text should be reconciled by the story owner to read "lifecycle → Cold".
    TransitionTo(EWaveSpawnerLifecycleState::Flushing);  // Active|Holding → Flushing
    TransitionTo(EWaveSpawnerLifecycleState::Cold);       // Flushing → Cold (Rule 14 full reset)
}

void UWaveSpawnerSubsystem::OnResumeFromPause()
{
    // Set resume grace window. Rule 1 gate (bInResumeGrace check in OnDPCFrameReady)
    // vetoes admissions until GetCurrentTimeS() >= ResumeGraceEndTimeS, cleared lazily
    // on the first post-grace DPC frame. kResumeGraceS is a named constexpr (DEVIATION NOTE:
    // stub value; config integration replaces with UWaveSpawnerConfig data asset load).
    // TODO(RSM epic): Replace bInResumeGrace + kResumeGraceS with RSMSubsystem->IsResumeGrace()
    // once the RSM epic implements the shared grace window (ADR-0007). IsResumeGrace() is the
    // authoritative signal for Wave Spawner, Collision, and Player Movement (TR-RSM-008/030/031).
    // If kResumeGraceS diverges from RSM's actual duration before that refactor, veto windows desync.
    // AC-WS-28, TR-WS-012.
    bInResumeGrace      = true;
    ResumeGraceEndTimeS = GetCurrentTimeS() + kResumeGraceS;
}

TArray<int32> UWaveSpawnerSubsystem::GetInFlightWaveIdsSorted() const
{
    // Returns LiveSlots as a sorted TArray for deterministic Rule 13/14 flush order.
    // TSet::Array() copies to a TArray; Sort() is in-place ascending (smallest int32 first).
    // N <= 16 per ADR-0011 D2 concurrency cap — O(N log N) with negligible cost (Story 007 perf note).
    TArray<int32> Ids = LiveSlots.Array();
    Ids.Sort();
    return Ids;
}

void UWaveSpawnerSubsystem::PurgeScheduledInFlightEntries()
{
    // Removes InFlightWaves entries for all WaveIds currently in ScheduledSlots.
    // Called before ScheduledSlots.Reset() on both the pause-flush (Rule 13) and
    // run-termination (Rule 14) paths to prevent orphan InFlightWaves accumulation (B-1).
    //
    // Background: TryAdmitPattern() adds FWaveInFlightState to InFlightWaves at slot
    // pre-commitment time. ScheduledSlots holds those pre-committed IDs (not yet live).
    // If ScheduledSlots.Reset() fires without removing InFlightWaves entries, those entries
    // become orphans: InFlightWaves.Num() inflates by up to 3 per barrage pause event.
    //
    // DespawnWave() is NOT called — no pool actor is associated with scheduled waves.
    // N <= 16 per ADR-0011 D2 concurrency cap — negligible cost on infrequent event path.
    // Story 007 / TR-WS-025.
    for (int32 WaveId : ScheduledSlots)
    {
        InFlightWaves.Remove(WaveId);
    }
}

// ============================================================================
// Rule 12 despawn pipeline (Story 006 — ADR-0011 D3, TR-WS-026)
// GDD:   design/gdd/wave-spawner-pattern-library.md Rule 12
// Story: production/epics/wave-spawner/story-006-despawn-pipeline-and-seam-13.md
// ============================================================================

void UWaveSpawnerSubsystem::DespawnWave(int32 WaveId, EWaveDespawnReason Reason)
{
    // Rule 12 ordered pipeline — mandatory sequence regardless of Reason (TR-WS-026).
    if (Callback) { Callback->OnCollisionUnregistered(WaveId); }
    if (Callback) { Callback->OnTelegraphUnregistered(WaveId); }

    // Release slot: remove WaveId from LiveSlots.
    //
    // DEVIATION NOTE: the story spec references GetSlotsForWave() returning 3 for barrage
    // and 1 for non-barrage, modeled against a scalar Live counter. The actual
    // implementation (Story 004) tracks live waves in LiveSlots (TSet<int32>) where
    // each AWave actor has its own unique WaveId. A barrage pattern's 3 AWave actors
    // each hold distinct WaveIds; 3 separate DespawnWave() calls each remove 1 slot,
    // totaling 3 released slots for the full barrage group. GetSlotsForWave() is
    // therefore N/A in the TSet model — per-call release is always exactly 1.
    //
    // Note: ScheduledSlots cleanup during PauseFlush is Story 007's responsibility
    // (Rule 13). DespawnWave() handles live wave despawn only.
    LiveSlots.Remove(WaveId);
    // Remove in-flight snapshot alongside live-slot release (Story 007 / TR-WS-022).
    // Follows LiveSlots.Remove so slot accounting (GetAvailableSlots) stays consistent
    // during the brief interval between the Remove calls and the callback dispatch below.
    InFlightWaves.Remove(WaveId);

    if (Callback) { Callback->OnWaveDespawned(WaveId, Reason); }

    // Return AWave actor to the object pool (Story 001 pool release, TR-WS-026).
    // ReleaseToPool() logs a warning if WaveId is not found. In headless integration
    // tests that inject LiveSlots directly (TestOnly_SetLiveCount), the AWave pool is
    // not populated — the warning is expected and does not indicate a production defect.
    ReleaseToPool(WaveId);
}

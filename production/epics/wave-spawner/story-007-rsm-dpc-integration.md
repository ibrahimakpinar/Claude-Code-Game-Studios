# Story 007: RSM/DPC Integration — Pause Flush, Run Termination, Snapshot Immutability

> **Epic**: Wave Spawner Pattern Library
> **Status**: Ready
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: (fill before sprint planning)
> **Manifest Version**: (none — docs/architecture/control-manifest.md not found; run /create-control-manifest)
> **Last Updated**: 2026-08-19

## Context

**GDD**: `design/gdd/wave-spawner-pattern-library.md`
**Requirement**: `TR-WS-014`, `TR-WS-024`, `TR-WS-025`, `TR-WS-027`, `TR-WS-028`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0011: Wave Spawner Pattern Library — D3 Six-State Lifecycle (primary); ADR-0007: Run State Machine (secondary — RSM delegate surface); ADR-0008: DPC Frame State (secondary — OnPostTickFrameStatePublished tick pin)
**ADR Decision Summary**: The spawner subscribes to RSM `OnPausedChanged` and DPC `OnPostTickFrameStatePublished` at `Initialize()`. Rule 13 (Pause Flush): `OnPausedChanged(true)` sets `bPauseFlushPending`; the next tick drains all in-flight waves in `wave_id` ASC order with `Reason = PauseFlush`; `last_spawn_time` is NOT reset. Rule 14 (Run Termination): RSM `RUNNING → {DEAD, COMPLETE, ABORTED}` triggers an immediate flush of all in-flight waves with `Reason = RunTermination`, clears `bBarrageOwed`, and transitions lifecycle to `Idle`. DPC tick pin: spawner tick fires from `OnPostTickFrameStatePublished` (not UE Actor tick), guaranteeing DPC frame state is populated before the spawner reads it. In-flight wave snapshots (`telegraph_window_s`) are immutable post-admission — DPC state mutation mid-flight does NOT re-parametrize admitted waves.

**Engine**: Unreal Engine 5.7 | **Risk**: HIGH
**Engine Notes**: RSM `OnPausedChanged` delegate — ADR-0007 specifies this as a `FSimpleMulticastDelegate` or `TMulticastDelegate`. Verify delegate type and binding pattern in UE 5.7 (post-cutoff, not in training data). DPC `OnPostTickFrameStatePublished` — ADR-0008 specifies this as a `FOnPostTickFrameStatePublished` multicast. Delegate binding via `AddUObject` (UObject-safe) — must unbind in `Deinitialize()`. RSM `RunSeed:uint64` accessed at `Cold→Active`; verify RSM exposes it as a public getter. `ForceTickNow()` on RSM — verify signature and usage pattern (ADR-0007).

**Control Manifest Rules (Feature layer)**:
- No control manifest found at `docs/architecture/control-manifest.md` — run `/create-control-manifest`.

---

## Acceptance Criteria

*From GDD `design/gdd/wave-spawner-pattern-library.md`, scoped to this story:*

- [ ] **AC-WS-17 (BLOCKING)**: Rule 13 Pause Flush. Given `OnPausedChanged(true)` fires with 3 in-flight waves (wave_id 1, 3, 7): on the next tick, `DespawnWave()` is called for wave_id 1, then 3, then 7 (ASC order). `EWaveDespawnReason = PauseFlush` for all three. `last_spawn_time` is NOT reset (its value before pause is the same value after flush). `Live` counter returns to 0 after flush.

- [ ] **AC-WS-18 (BLOCKING)**: Rule 14 Run Termination. Given RSM transitions to `DEAD` with 5 in-flight waves: all 5 are despawned with `Reason = RunTermination` immediately (same tick as RSM transition notification). `bBarrageOwed` is cleared to `false`. Lifecycle transitions to `Idle`. `Scheduled = 0`, `Live = 0` after termination flush.

- [ ] **AC-WS-21 (BLOCKING)**: Snapshot immutability. Given a wave admitted at DPC snapshot `telegraph_window_s = 0.80s`; DPC then publishes a new frame with `telegraph_window_s = 0.65s` mid-run. The in-flight wave's stored `TelegraphWindowS = 0.80f` is unchanged. Verified via `FDPCTestStub.PublishNewFrame(0.65f)` followed by reading `InFlightWaves[0].TelegraphWindowS`.

- [ ] **AC-WS-28 (BLOCKING)**: RSM resume grace honored. Given `OnPausedChanged(false)` fires: spawner sets `ResumeGraceEndTimeS = now + RESUME_GRACE_S`, sets `bInResumeGrace = true`. On ticks within the grace window: Rule 1 gate returns early (no admissions). On first tick after grace window expires: admissions resume normally.

- [ ] **AC-WS-29 (BLOCKING — fixture reframe R2a-1)**: Snapshot immutability under phase-boundary condition. Given a MID-phase pattern with 2 onsets admitted at `phase_remaining ≈ 3.05s`, onset-2 at `+2.90s` from admission (fires 150ms BEFORE the MID→PEAK pool swap). DPC's `telegraph_window_s` value at admission time (MID phase) = `0.82s`. After the MID→PEAK pool swap fires, the in-flight wave's `TelegraphWindowS` remains `0.82s` — unchanged. `FWaveSpawnerCallbackTestStub.GetEventLog()` shows no re-parametrization events.

---

## Implementation Notes

*Derived from ADR-0011 D3 + ADR-0007 + ADR-0008 Implementation Guidelines:*

**Delegate subscriptions in Initialize():**
```cpp
void UWaveSpawnerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Collection.InitializeDependency(URunStateMachineSubsystem::StaticClass());
    Collection.InitializeDependency(UDPCSubsystem::StaticClass());

    RSMSubsystem = Collection.GetSubsystem<URunStateMachineSubsystem>();
    DPCSubsystem = Collection.GetSubsystem<UDPCSubsystem>();

    // RSM delegates
    RSMSubsystem->OnPausedChanged.AddUObject(this, &UWaveSpawnerSubsystem::HandlePausedChanged);
    RSMSubsystem->OnRunStateChanged.AddUObject(this, &UWaveSpawnerSubsystem::HandleRunStateChanged);

    // DPC tick-ordering pin: spawner tick fires from DPC's post-tick publish
    DPCSubsystem->OnPostTickFrameStatePublished.AddUObject(
        this, &UWaveSpawnerSubsystem::OnDPCFrameReady);
}

void UWaveSpawnerSubsystem::Deinitialize()
{
    if (RSMSubsystem)
    {
        RSMSubsystem->OnPausedChanged.RemoveAll(this);
        RSMSubsystem->OnRunStateChanged.RemoveAll(this);
    }
    if (DPCSubsystem)
    {
        DPCSubsystem->OnPostTickFrameStatePublished.RemoveAll(this);
    }
    Super::Deinitialize();
}
```

**OnDPCFrameReady — spawner tick entry point (replaces FTickableGameObject::Tick for production):**
```cpp
void UWaveSpawnerSubsystem::OnDPCFrameReady(const FDPCFrameState& FrameState)
{
    // This is the spawner's tick in production — DPC guarantees the frame is ready.
    // In tests, FTickableGameObject::Tick() is also valid (DPC stub fires synchronously).
    TickAdmissionPipeline(FrameState);
}
```

**Rule 13 Pause Flush:**
```cpp
void UWaveSpawnerSubsystem::HandlePausedChanged(bool bIsPaused)
{
    if (bIsPaused)
    {
        // Pause: flush all in-flight waves ASC by wave_id
        TArray<int32> InFlightIds = GetInFlightWaveIdsSorted();  // ASC
        for (int32 WaveId : InFlightIds)
            DespawnWave(WaveId, EWaveDespawnReason::PauseFlush);
        // DO NOT reset last_spawn_time (TR-WS-027)
        TransitionTo(EWaveSpawnerLifecycleState::Flushing);
    }
    else
    {
        // Resume: set grace window
        OnResumeFromPause();
        TransitionTo(EWaveSpawnerLifecycleState::Holding);  // or Active depending on phase
    }
}
```

**Rule 14 Run Termination:**
```cpp
void UWaveSpawnerSubsystem::HandleRunStateChanged(ERunState NewState)
{
    if (NewState == ERunState::Running)
    {
        // Cold→Active: capture RunSeed
        RunSeed = RSMSubsystem->GetRunSeed();
        TransitionTo(EWaveSpawnerLifecycleState::Active);
        return;
    }

    const bool bTerminating = NewState == ERunState::Dead
                           || NewState == ERunState::Complete
                           || NewState == ERunState::Aborted;
    if (bTerminating)
    {
        TArray<int32> InFlightIds = GetInFlightWaveIdsSorted();
        for (int32 WaveId : InFlightIds)
            DespawnWave(WaveId, EWaveDespawnReason::RunTermination);
        bBarrageOwed = false;
        Scheduled = 0;
        Live = 0;
        TransitionTo(EWaveSpawnerLifecycleState::Idle);
    }
}
```

**Snapshot immutability contract (in-flight state struct):**
```cpp
struct FWaveInFlightState
{
    int32 WaveId;
    float TelegraphWindowS;   // captured at admission from DPC FrameState; NEVER mutated
    // ... other per-wave fields
};
```

**PEAK phase entry time capture (for F-3 in Story 005):**
- On RSM phase transition to PEAK (ADR-0007 delegate), set `PeakEntryTimeS = GetWorld()->GetTimeSeconds()`.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 003: `IsInResumeGrace()` implementation (referenced above as `OnResumeFromPause()`).
- Story 005: `ShouldDrawBarrage()` and `BarrageCountThisPeak` (F-3 governor uses `PeakEntryTimeS` set here).
- Story 006: `DespawnWave()` implementation — called here but defined in Story 006.

---

## QA Test Cases

*Test cases not yet defined — run /qa-plan to generate them.*

---

## Test Evidence

**Story Type**: Integration
**Required evidence**: `Source/SLIPSTORM/Tests/Integration/WaveSpawner/WaveSpawnerRSMDPCIntegrationTest.cpp` — must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: Story 002 (lifecycle `TransitionTo` calls), Story 003 (resume grace), Story 004 (`bBarrageOwed` clearance on termination), Story 005 (`PeakEntryTimeS` consumed by F-3), Story 006 (`DespawnWave()` called in flush paths)
- Unlocks: Full end-to-end integration — all prior stories wired to RSM/DPC events

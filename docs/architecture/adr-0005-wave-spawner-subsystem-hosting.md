# ADR-0005: Wave Spawner Subsystem Hosting — UGameInstanceSubsystem

## Status

Accepted

## Date

2026-06-24

## Last Verified

2026-06-26 (amendment INT-001 — cross-reference to ADR-0006 sub-question (f) shared-host topology inscribed)
2026-06-26 (amendment INT-004 — `Collection.InitializeDependency(URunStateMachineSubsystem)` + `Collection.InitializeDependency(UDPCSubsystem)` calls inscribed in `Initialize()` skeleton per ADR-0007 IG-3 + ADR-0008 IG-3; closes architecture-review-2026-06-26 BLOCKING Wave Spawner enter-implementation gate)

## Decision Makers

- **perf-analyst** — R7 measurement establishing the 16.6 ms per-replay pool-allocation hitch as material degradation
- **creative-director** — Q2-locked user decision at CD R1 synthesis (OQ-WS-3 closure)
- **unreal-specialist** — R1a authorship, R2a tick-host inscription, R2a-4 lifecycle correction
- **qa-lead** — R3 oracle-site sweep and lifecycle inscription verification

## Summary

SLIPSTORM's Wave Spawner must survive Unreal Engine world reloads to avoid a
16.6 ms pool-allocation hitch on every death-retry in the core replay loop. The
system is hosted as `UWaveSpawnerSubsystem : public UGameInstanceSubsystem, public
FTickableGameObject`, which persists across world transitions and allocates its
23-actor `AWave` pool exactly once per game session at the first-world-load hook.

## Engine Compatibility

| Field | Value |
|-------|-------|
| **Engine** | Unreal Engine 5.7 |
| **Domain** | Core |
| **Knowledge Risk** | HIGH — UE 5.4–5.7 are post-LLM-cutoff (May 2025); subsystem lifecycle semantics must be verified against UE 5.7 source |
| **References Consulted** | `docs/engine-reference/unreal/VERSION.md`, `docs/engine-reference/unreal/breaking-changes.md`, `docs/engine-reference/unreal/deprecated-apis.md`, `docs/engine-reference/unreal/current-best-practices.md` |
| **Post-Cutoff APIs Used** | `FCoreUObjectDelegates::PostLoadMapWithWorld` (post-load hook for pool pre-allocation; fire-once-per-load semantics require source verification in UE 5.7), `UGameInstance::OnWorldChanged` (equivalent first-fire alternative), `UGameInstanceSubsystem::Initialize()` / `Deinitialize()` (lifecycle ordering vs UWorld construction requires source verification), `FSubsystemCollectionBase::InitializeDependency()` (cross-subsystem ordering pin used to ensure RSM + DPC `Initialize()` complete before this subsystem's body runs — added by INT-004 amendment 2026-06-26; same semantic gate as ADR-0007 Risk 1 + ADR-0008 Verification 3), `FTickableGameObject::GetTickableTickType()` with `ETickableTickType::Conditional` (tick-gating pattern; verify pattern unchanged), `TObjectPtr<T>` (GC-safe pointer modernization, UE 5.0+) |
| **Verification Required** | (1) Confirm `UGameInstanceSubsystem::Initialize()` runs before any `UWorld` is constructed in UE 5.7 — `GetWorld()` MUST return null at that point, confirming `SpawnActor` cannot be called there. (2) Confirm `FCoreUObjectDelegates::PostLoadMapWithWorld` fires exactly once per first level load with a valid `UWorld`, and does NOT re-fire on replay world re-use (EC-WS-8 semantics). (3) Confirm `FTickableGameObject` callbacks run on the game thread in UE 5.7 mobile builds. (4) Confirm `UGameInstanceSubsystem::Deinitialize()` always fires on game-instance teardown — no silent leak path exists. (5) Confirm `FSubsystemCollectionBase::InitializeDependency(URunStateMachineSubsystem::StaticClass())` and `FSubsystemCollectionBase::InitializeDependency(UDPCSubsystem::StaticClass())` invoked at the top of `UWaveSpawnerSubsystem::Initialize()` guarantee that RSM and DPC `Initialize()` RETURN before control returns to this call site (i.e. Initialize-completion order, not merely construction order). Same semantic gate as ADR-0007 Risk 1 + ADR-0008 Verification 3. If verification fails (construction-only), fallback path is RSM/DPC emitting a one-shot `OnInitialized` delegate and Wave Spawner's `Initialize()` deferring the DPC delegate bind to that handler — see Risks below. Added by INT-004 amendment 2026-06-26. |

> **Note**: Knowledge Risk is HIGH. This ADR must be re-validated if the project
> upgrades engine versions. Flag as "Superseded" and author a new ADR on upgrade.

## ADR Dependencies

| Field | Value |
|-------|-------|
| **Depends On** | ADR-0007 (RSM hosting — `URunStateMachineSubsystem` is an `InitializeDependency()` target from this subsystem's `Initialize()` per Implementation Guideline 8) — added by INT-004 amendment 2026-06-26; ADR-0008 (DPC hosting — `UDPCSubsystem` is an `InitializeDependency()` target from this subsystem's `Initialize()` per Implementation Guideline 8 — and supplies the `OnPostTickFrameStatePublished` delegate Wave Spawner binds at `Initialize()` per R2a-2) — added by INT-004 amendment 2026-06-26. (Originally "None — foundational"; INT-004 amendment introduces runtime ordering dependency that did not exist in the foundational 2026-06-24 Accepted state.) |
| **Enables** | AC-WS-13, AC-WS-15, AC-WS-20, AC-WS-30, AC-WS-31 — and Wave Spawner Epic story-Done overall |
| **Blocks** | Wave Spawner Epic implementation — no Wave Spawner story may enter implementation until this ADR is Accepted |
| **Ordering Note** | This ADR must be Accepted before any Wave Spawner story enters implementation. Sibling: OQ-PW-3 (HISM vs ISMC) ADR for the `AWave` actor's rendering implementation — an independent decision, not gated by this ADR. |

## Context

### Problem Statement

The Wave Spawner requires an object pool of 23 `AWave` actor instances to service
the full 60-second run at SLIPSTORM's peak concurrency without incurring `SpawnActor`
/ `Destroy` overhead during active gameplay (Rule 11). Allocating this pool costs a
single frame of stall. The question is: which UE subsystem class hosts the spawner,
and therefore controls when and how often that stall occurs?

SLIPSTORM's core loop is death → replay → death → replay. Each replay is a re-entry
into the Wave Spawner's `Flushing → Cold → Active` path (EC-WS-8). Any hosting
choice that re-fires pool allocation on re-entry imposes that stall on the first
frame of every retry, directly degrading the retry-loop feel that is central to the
game's appeal.

The hosting decision must be made before Wave Spawner implementation begins. An
incorrect hosting choice cannot be corrected without pool allocation refactoring, which
affects the spawner's Initialize/Deinitialize lifecycle, its delegate subscriptions,
and every acceptance criterion tied to pool sizing.

### Current State

No Wave Spawner implementation exists at this decision point. The Wave Spawner GDD
(R1a closed 2026-06-21; R3a applied 2026-06-22) is the authoritative design document.
OQ-WS-3 was the open question tracking this hosting decision; it was locked at R1a.

### Constraints

- **Mobile platform, 60 fps, 16.6 ms frame budget** — per `.claude/docs/technical-preferences.md`: target 60 fps sustained on mid-tier mobile, ~100 draw calls, 1.5 GB memory ceiling.
- **Pool allocated once per session** — pool pre-allocation must survive world reloads across all replays in a session (EC-WS-8).
- **SpawnActor forbidden during gameplay** — once the pool is pre-allocated, no `UWorld::SpawnActor` or `Destroy` calls occur during active gameplay (Rule 11, GDD line 160).
- **UWorld unavailable at Initialize()** — `UGameInstanceSubsystem::Initialize()` runs before `UGameInstance::LoadComplete()` / first world load in UE 5.7; `GetWorld()` returns null; `SpawnActor` would assert (R2a-4 lifecycle correction).
- **Tick ordering must be structural** — `FTickableGameObject` lacks `AddTickPrerequisiteActor` / `AddTickPrerequisiteComponent`; ordering relative to DPC and RSM cannot be expressed via tick groups and must be enforced by delegate subscription.
- **Pool storage must prevent garbage collection** — pooled `AWave` instances are `AActor`-derived and subject to UE's garbage collector; they must be anchored via `UPROPERTY()`.

### Requirements

- Subsystem must survive world reloads (death-replay cycles) without re-firing pool allocation.
- Pool pre-allocation runs exactly once per game session; replay re-entry reuses the existing pool unchanged.
- Subsystem tick runs after DPC has published its frame state for the tick (R2a-2 structural ordering pin).
- Tick incurs zero overhead in the spawner's `Cold` and `Idle` internal states.
- Pool storage uses `UPROPERTY() TArray<TObjectPtr<AWave>>` for GC safety.
- Subsystem teardown releases the pool cleanly on game-instance teardown with no leak path.

## Decision

`UWaveSpawnerSubsystem` is hosted as a `UGameInstanceSubsystem` with `FTickableGameObject`
multiple inheritance. This is the class that was locked at GDD R1a (OQ-WS-3 closure,
2026-06-21, GDD line 34 and line 1167). All implementation details below were inscribed
in the GDD at R2a (2026-06-22) and are reproduced here for implementer reference.

The `UGameInstanceSubsystem` lifecycle spans the entire game instance, surviving all
world transitions that occur during a session. The pool is allocated once, at the first
valid world load, and reused across every replay. `UGameInstanceSubsystem::Deinitialize()`
releases the pool on game-instance teardown.

The class does NOT tick by default as a `UGameInstanceSubsystem`. Tick is provided by
additionally inheriting `FTickableGameObject`, with conditional gating to suppress idle
overhead.

The admission decision does NOT fire inside `Tick(DeltaTime)`. Instead, `Initialize()`
subscribes the spawner to DPC's `OnPostTickFrameStatePublished` delegate; the admission
pipeline executes inside that bound callback. This makes tick ordering structural: DPC
publishes its frame state at the end of its own tick, and the spawner's admission read
is guaranteed to observe DPC's fully-computed state for that frame. `Tick(DeltaTime)` is
reserved for bookkeeping (rate-limit timer state only).

### Architecture

```
UGameInstance
    │
    └── UWaveSpawnerSubsystem                     [UGameInstanceSubsystem + FTickableGameObject]
            │
            ├── Initialize()
            │       ├── Subscribes: FCoreUObjectDelegates::PostLoadMapWithWorld → OnFirstWorldLoad()
            │       └── Subscribes: UDPCSubsystem::OnPostTickFrameStatePublished → OnDPCFrameReady()
            │
            ├── OnFirstWorldLoad()                 [fires once per session; UWorld now valid]
            │       └── SpawnActor × 23 → UPROPERTY() TArray<TObjectPtr<AWave>> Pool
            │
            ├── OnDPCFrameReady(const FDPCFrameState&)   [admission decision; not in Tick]
            │       └── Rule 1 gate → Rule 7 admission → Rule 8 selection
            │
            ├── Tick(float DeltaTime)              [FTickableGameObject; bookkeeping only]
            │       └── Rate-limit timer state
            │
            ├── IsTickable() → false in Cold/Idle, true otherwise
            ├── GetTickableTickType() → ETickableTickType::Conditional
            ├── GetStatId() → STATGROUP_WaveSpawner
            │
            └── Deinitialize()
                    └── Releases Pool (all AWave actors)

Replay re-entry path (EC-WS-8): Flushing → Cold → Active
    Pool is REUSED unchanged. OnFirstWorldLoad() is NEVER re-fired on Cold re-entry.

DPC tick chain (structural ordering):
    RSM → DPC → [OnPostTickFrameStatePublished fires] → WaveSpawner admission
```

### Key Interfaces

```cpp
// UWaveSpawnerSubsystem.h
// Multiple inheritance: UGameInstanceSubsystem (lifecycle) + FTickableGameObject (tick).
// UGameInstanceSubsystem does NOT tick by default in UE 5.7.
UCLASS()
class UWaveSpawnerSubsystem
    : public UGameInstanceSubsystem
    , public FTickableGameObject
{
    GENERATED_BODY()

public:
    // --- UGameInstanceSubsystem interface ---

    // Subscribes delegate handles only. NO SpawnActor calls — UWorld does not
    // exist at Initialize() in UE 5.7 (R2a-4 lifecycle correction). SpawnActor
    // deferred to OnFirstWorldLoad() via PostLoadMapWithWorld delegate.
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    // Releases pool. Unregisters delegate handles.
    virtual void Deinitialize() override;

    // --- FTickableGameObject interface ---

    // Per-frame bookkeeping only (rate-limit timer state). Admission decision
    // fires in OnDPCFrameReady(), NOT here — ordering is structural via DPC delegate.
    virtual void Tick(float DeltaTime) override;

    // Conditional tick type: paired with IsTickable() to suppress tick in Cold/Idle.
    virtual ETickableTickType GetTickableTickType() const override
    {
        return ETickableTickType::Conditional;
    }

    // Tick disabled in Cold and Idle internal states to avoid idle CPU cost.
    virtual bool IsTickable() const override
    {
        return (CurrentLifecycleState != EWaveSpawnerState::Cold)
            && (CurrentLifecycleState != EWaveSpawnerState::Idle);
    }

    // Stat group for Unreal Insights / stat unit profiling scope.
    virtual TStatId GetStatId() const override
    {
        RETURN_QUICK_DECLARE_CYCLE_STAT(UWaveSpawnerSubsystem, STATGROUP_WaveSpawner);
    }

    // Pool accessor for AC-WS-15 and AC-WS-20 post-hoc inspection in tests.
    const TArray<TObjectPtr<AWave>>& GetPool() const { return Pool; }

private:
    // Pool pre-allocation: fires once per session at first world load.
    // Bound to FCoreUObjectDelegates::PostLoadMapWithWorld in Initialize().
    // UWorld is valid here; SpawnActor is legal.
    // Replay re-entry (Flushing → Cold → Active) does NOT re-fire this path.
    void OnFirstWorldLoad(UWorld* LoadedWorld);

    // Admission decision: fires after DPC publishes its frame state each tick.
    // Bound to UDPCSubsystem::OnPostTickFrameStatePublished in Initialize().
    // Structural tick-ordering guarantee: DPC's state is fully computed before
    // this callback fires. RSM state is also finalized (RSM ticks before DPC
    // in the Pull-Wave tick chain: RSM → DPC → PM → Pull-Wave → Telegraph → Collision).
    void OnDPCFrameReady(const FDPCFrameState& FrameState);

    // Pool storage. UPROPERTY() anchors AWave instances against GC.
    // UE 5.0+ TObjectPtr<T> replaces raw pointers for UObject references.
    // Pool is allocated once at OnFirstWorldLoad(); reused across all replays.
    UPROPERTY()
    TArray<TObjectPtr<AWave>> Pool;

    // Internal lifecycle state. Controls IsTickable() gating.
    EWaveSpawnerState CurrentLifecycleState = EWaveSpawnerState::Cold;

    // Delegate handles for clean unsubscription in Deinitialize().
    FDelegateHandle PostLoadMapHandle;
    FDelegateHandle DPCFrameReadyHandle;
};
```

```cpp
// UWaveSpawnerSubsystem.cpp — Initialize / Deinitialize skeleton

void UWaveSpawnerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // Pin subsystem-collection ordering: RSM completes its Initialize() first,
    // then DPC, then this subsystem's body proceeds. Added by INT-004 amendment
    // 2026-06-26 closing architecture-review-2026-06-26 BLOCKING gap. Without
    // these pins, retrieving UDPCSubsystem below may return a partially-
    // initialized instance (or null), silently dropping the admission delegate
    // bind. Per ADR-0007 Implementation Guideline 3 + ADR-0008 Implementation
    // Guideline 3. Semantic verification gate (Initialize-completion vs
    // construction-only ordering) is Engine Compatibility >> Verification
    // Required #5; if construction-only, Risk 5 fallback applies.
    Collection.InitializeDependency(URunStateMachineSubsystem::StaticClass());
    Collection.InitializeDependency(UDPCSubsystem::StaticClass());

    // Bind pool pre-allocation to first-world-load. UWorld does NOT exist here
    // (R2a-4 lifecycle correction — Initialize() runs before LoadComplete()).
    // SpawnActor inside this callback body would assert.
    PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(
        this, &UWaveSpawnerSubsystem::OnFirstWorldLoad);

    // Bind admission decision to DPC's post-tick delegate (R2a-2 tick ordering pin).
    // DPC is guaranteed initialized by the InitializeDependency call above
    // (per Verification Required #5 — same semantic ADR-0007 Risk 1 +
    // ADR-0008 Verification 3 depend on). The null check below is defensive
    // belt-and-suspenders coverage for the Risk 5 fallback path (if UE 5.7
    // InitializeDependency proves construction-only rather than Initialize-
    // completion-ordered). Forward contract on DPC:
    // UDPCSubsystem::OnPostTickFrameStatePublished declared as
    // DECLARE_MULTICAST_DELEGATE_OneParam(FOnPostTickFrameStatePublished,
    // const FDPCFrameState&). GDD line 452; closed by ADR-0008.
    if (UDPCSubsystem* DPC = GetGameInstance()->GetSubsystem<UDPCSubsystem>())
    {
        DPCFrameReadyHandle = DPC->OnPostTickFrameStatePublished.AddUObject(
            this, &UWaveSpawnerSubsystem::OnDPCFrameReady);
    }
}

void UWaveSpawnerSubsystem::Deinitialize()
{
    FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);

    if (UDPCSubsystem* DPC = GetGameInstance()->GetSubsystem<UDPCSubsystem>())
    {
        DPC->OnPostTickFrameStatePublished.Remove(DPCFrameReadyHandle);
    }

    // Release pool. AWave actors are destroyed by GC once UPROPERTY refs drop.
    Pool.Reset();

    Super::Deinitialize();
}
```

### Implementation Guidelines

1. **Never call `SpawnActor` in `Initialize()`.** `GetWorld()` returns null at that point in the UE 5.7 lifecycle. All `SpawnActor` calls must happen inside `OnFirstWorldLoad()`, which is bound to `FCoreUObjectDelegates::PostLoadMapWithWorld` (or the `UGameInstance::OnWorldChanged` first-fire equivalent). (R2a-4 lifecycle correction; GDD lines 157–158, 457.)

2. **Never call `SpawnActor` or `Destroy` during active gameplay.** Once `OnFirstWorldLoad()` completes, the pool must remain at exactly 23 actors for the life of the game instance. Pool entries are leased via `Acquire()` and returned via `Release(WaveId)` — no dynamic allocation. (Rule 11, GDD line 160; AC-WS-15.)

3. **Replay re-entry does not re-fire pool allocation.** On `Flushing → Cold → Active` (EC-WS-8), all pooled `AWave` actors are already allocated and available. `OnFirstWorldLoad()` must guard against re-execution (e.g., a boolean `bPoolAllocated` flag or a one-shot delegate unbind after first fire). (GDD line 157; ADR rationale.)

4. **Admission logic belongs in `OnDPCFrameReady()`, not `Tick()`.** `Tick(DeltaTime)` is bookkeeping only (rate-limit timer). Placing admission in `Tick()` reintroduces tick-ordering ambiguity that the DPC delegate subscription structurally eliminates. (R2a-2; GDD lines 450–455.)

5. **`GetStatId()` must return `STATGROUP_WaveSpawner`** for Unreal Insights scope isolation and per-tick AC-WS-30 profiling (GDD line 447; AC-WS-30 at GDD line 1138).

6. **Pool storage is `UPROPERTY() TArray<TObjectPtr<AWave>>`** — the `UPROPERTY()` macro prevents UE's garbage collector from collecting pooled actors that are not currently referenced by the world's actor graph. `TObjectPtr<T>` is the UE 5.0+ GC-safe modernization of raw pointers for UObject references. (GDD line 158; Dependencies E2 at GDD line 941.)

7. **Pooled `AWave` actors do NOT own renderer components** (INT-001 cross-reference, added by amendment 2026-06-26). Per ADR-0006 Decision sub-question (f), both `WaveMassISMC` (16 instances at PEAK) and `TrailCubeISMC` (48 instances at PEAK) are owned by a separate singleton `APullWaveSubsystemActor` — NOT by individual pooled `AWave` actors. The pooled `AWave` actors from this ADR are lightweight state tokens: they carry per-wave gameplay fields (lane, phase, timestamps) consulted by code that holds an `AWave*`. Their renderer-side identity lives as `WaveMassInstanceIndex` + `TrailCubeInstanceIndices[3]` inside `FPullWaveRuntimeState` on the singleton's `TArray<FPullWaveRuntimeState> ActiveWaves`. Two consequences for implementers of this ADR: (i) `AWave`'s C++ class declaration MUST NOT contain `TObjectPtr<UInstancedStaticMeshComponent>` members named `WaveMassISMC` or `TrailCubeISMC` — that pattern is registered as `per_AWave_actor_render_component_ownership` forbidden. (ii) If the wave-renderer epic later determines that no per-actor seam requires `AWave*` (e.g., no gameplay code calls `AWave::SomeMethod()`), the pool may be eliminated and the `FPullWaveRuntimeState` array on the singleton absorbs every per-wave field — pool sizing (23) ceases to apply. As of 2026-06-26 the pool is retained; this guideline anticipates a possible later simplification without binding it.

8. **`UWaveSpawnerSubsystem::Initialize()` MUST call `Collection.InitializeDependency(URunStateMachineSubsystem::StaticClass())` AND `Collection.InitializeDependency(UDPCSubsystem::StaticClass())` BEFORE retrieving `UDPCSubsystem` to bind `OnPostTickFrameStatePublished`** (INT-004 cross-reference, added by amendment 2026-06-26). The dependency pins live at the top of the `Initialize()` body immediately after `Super::Initialize(Collection)` — see the amended Initialize() skeleton above. RSM has no Wave-Spawner-side consumption at `Initialize()` time, but is pinned to support DPC's own `InitializeDependency(URunStateMachineSubsystem)` chain (DPC's `Tick()` calls `RSM->ForceTickNow()` per ADR-0007 Structural Decision 2 + DPC GDD R6 Rule 3); pinning RSM directly from this subsystem makes the cascading dependency explicit at every entry point in the chain. Without these two `InitializeDependency` calls, `GetSubsystem<UDPCSubsystem>()` later in `Initialize()` may return a partially-initialized DPC instance or null, silently dropping the admission delegate bind — production wave-spawning would fail. Forbidden Pattern `WaveSpawner_Initialize_without_InitializeDependency_on_RSM_and_DPC` is registered below to prevent regression. Semantic-verification gate (Initialize-completion vs construction-only) is Engine Compatibility >> Verification Required #5; fallback is Risk 5 (one-shot `OnInitialized` delegate). Closes architecture-review-2026-06-26 INT-004 BLOCKING gap.

## Alternatives Considered

### Alternative 1: UWorldSubsystem

- **Description**: Host the Wave Spawner as a `UWorldSubsystem`, which initializes and deinitializes once per `UWorld`. Pool pre-allocation would run in the subsystem's `Initialize()`, called each time a new world is loaded.
- **Pros**: Simpler lifecycle alignment with world-scoped state; automatic teardown on world destruction; no need to handle the first-world-load delegate separately.
- **Cons**: SLIPSTORM's replay path (death → replay) triggers a world reload on each retry. Every reload re-initializes the `UWorldSubsystem`, which re-fires pool pre-allocation. perf-analyst R7 measured this as a 16.6 ms frame hitch — a full frame stall on the first frame of every retry. The retry loop is the core gameplay loop; degrading it on every attempt is a material quality regression.
- **Rejection Reason**: Per-replay 16.6 ms pool-allocation hitch. GDD Rule 7 atomicity contract (line 125) and OQ-WS-3 closure (line 1167) name this as the decisive rejection criterion. The `UGameInstanceSubsystem` lifecycle was chosen specifically to avoid this pattern.

### Alternative 2: AActor Singleton in PersistentLevel

- **Description**: Place a single persistent `AWaveSpawnerActor` in the `PersistentLevel`. The actor's `BeginPlay` allocates the pool and its `EndPlay` releases it.
- **Pros**: Familiar Actor lifecycle; visible in the editor world outliner; no subsystem boilerplate.
- **Cons**: Requires manual GC anchoring (the pool would need a `UPROPERTY()` owner separate from the actor, or the actor must never be destroyed). No built-in subsystem lifecycle hooks; `Initialize`/`Deinitialize` equivalents must be invented. If the `PersistentLevel` itself is reloaded (e.g., on a full restart), pool allocation re-fires — same hitch as Alternative 1. No automatic subsystem injection; consumers must locate the singleton actor manually, introducing coupling and null-safety risk.
- **Rejection Reason**: Manual GC anchoring burden, no built-in lifecycle hooks, and susceptibility to the same per-reload allocation hitch as Alternative 1 if `PersistentLevel` is ever reloaded.

### Alternative 3: UEngineSubsystem

- **Description**: Host the Wave Spawner as a `UEngineSubsystem`, which initializes once per engine lifetime and outlives any number of game instances.
- **Pros**: Maximum persistence — pool survives even game-instance teardown.
- **Cons**: The Wave Spawner holds game-mode-scoped state (current run seed, `CurrentLifecycleState`, admission counters). A `UEngineSubsystem` lives across game sessions; restarting the game within the same engine process would carry stale run state from the previous session. The `UGameInstanceSubsystem` lifecycle maps cleanly to one game session — state is naturally reset on game-instance teardown and freshly initialized on the next session.
- **Rejection Reason**: Outlives the `GameInstance` unnecessarily; game-session-scoped state leaks across sessions in ways the `UGameInstanceSubsystem` lifecycle structurally prevents.

## Consequences

### Positive

- Pool is allocated exactly once per session. Death-replay cycles impose zero additional pool-allocation cost after the first world load.
- The first-world-load frame hitch (23 × `SpawnActor`) is masked behind the loading frame that players already accept, concurrent with level streaming and HUD construction. (GDD line 457; AC-WS-31.)
- `Deinitialize()` provides a deterministic teardown point; no manual GC anchor management beyond the `UPROPERTY()` declaration.
- The DPC delegate subscription structurally enforces tick ordering without relying on UE tick groups, which are not available for `FTickableGameObject` (R2a-2).
- Conditional tick gating (`ETickableTickType::Conditional` + `IsTickable()`) eliminates idle overhead in `Cold` and `Idle` states without disabling the tick infrastructure.

### Negative

- `Initialize()` cannot perform `SpawnActor`. The two-step lifecycle (subscribe delegates in `Initialize()`, allocate pool in `OnFirstWorldLoad()`) is less obvious than a single initialization point. Developers must understand the lifecycle split or they will introduce a UE assert.
- Multiple inheritance (`UGameInstanceSubsystem + FTickableGameObject`) is unconventional. The split between `Tick()` (bookkeeping) and `OnDPCFrameReady()` (admission logic) must be kept disciplined; if future engineers add logic to `Tick()`, tick-ordering guarantees break.
- The pool pre-allocation one-shot guard (preventing `OnFirstWorldLoad()` from re-firing) requires explicit implementation (delegate unbind or boolean flag); a missing guard would silently re-allocate on edge-case world loads.

### Neutral

- The `UGameInstanceSubsystem` pattern is not Blueprint-accessible by default. Wave Spawner is a C++ system; this is consistent with the project's architecture.
- `FTickableGameObject` stat group registration (`STATGROUP_WaveSpawner`) adds a profiling surface that did not previously exist. This is beneficial for Alpha-gate profiling (AC-WS-30).

## Risks

| Risk | Probability | Impact | Mitigation |
|------|------------|--------|-----------|
| `FCoreUObjectDelegates::PostLoadMapWithWorld` fires more than once per session in an edge case (e.g., seamless travel, DLC load) | Low | High — pool re-allocated, breaking Rule 11 | One-shot guard: unbind the delegate or gate on `bPoolAllocated` flag inside `OnFirstWorldLoad()`. Verify UE 5.7 `PostLoadMapWithWorld` semantics against source before shipping. |
| `Initialize()` is called after `UWorld` exists in a future engine version | Very Low | Medium — pool allocation would succeed at wrong lifecycle point | Source-verify the ordering assertion (Verification Required above) before each engine upgrade. |
| `FTickableGameObject` fires off-game-thread in a UE 5.7 mobile configuration | Low | High — race condition on Pool and lifecycle state | Add a thread assertion (`check(IsInGameThread())`) at the top of `Tick()` and `OnDPCFrameReady()` in non-Shipping builds. Verify against UE 5.7 mobile source. |
| Admission logic migrates into `Tick()` in a future implementation PR | Medium | Medium — breaks structural DPC ordering guarantee | Code review gate: admission logic MUST NOT appear in `Tick()`. Document in Forbidden Patterns registry (§ Forbidden Patterns below). |
| `FSubsystemCollectionBase::InitializeDependency()` guarantees construction order but NOT `Initialize()` completion order in UE 5.7 (Risk 5; INT-004 amendment 2026-06-26) | Low | High — `GetSubsystem<UDPCSubsystem>()` at `Initialize()` returns a partially-initialized DPC; `OnPostTickFrameStatePublished` may not yet be assignable, dropping the admission delegate bind silently | Source-verify before shipping (mirror of ADR-0007 Risk 1 + ADR-0008 Verification 3 — same UE 5.7 semantic). If verification fails (construction-only), fallback: RSM and/or DPC emit `OnInitialized` one-shot delegates; Wave Spawner's `Initialize()` defers the `DPC->OnPostTickFrameStatePublished.AddUObject(...)` bind to that handler instead of executing it synchronously. |

## Performance Implications

| Metric | Before | Expected After | Budget |
|--------|--------|---------------|--------|
| Per-tick admission cost (gameplay) | N/A (not yet implemented) | Small fraction of 16.6 ms frame budget; exact figure deferred to Alpha-gate profiling on mid-tier mobile (per AC-WS-30 ADVISORY-at-story-Done / BLOCKING-at-Alpha gate) | ≤ 0.30 ms p99 per AC-WS-30 (GDD line 1138) |
| Pool pre-allocation (session startup) | N/A | 23 × `SpawnActor`, single frame, masked by first-world-load frame (concurrent with level streaming + HUD construction) | ≤ 16.6 ms per AC-WS-31 (GDD line 1139); ADVISORY-at-story-Done, BLOCKING-at-Alpha |
| Per-replay additional pool cost | Would be ≥ 16.6 ms under UWorldSubsystem (rejected) | 0 ms — pool reused unchanged | 0 ms; structural guarantee |
| Idle tick overhead (`Cold`/`Idle` states) | N/A | 0 — `IsTickable()` returns false | 0 ms; structural guarantee via `ETickableTickType::Conditional` |

## Migration Plan

This ADR applies to a greenfield system. No existing Wave Spawner implementation exists to migrate. The decision constrains the initial implementation directly.

1. Author `UWaveSpawnerSubsystem` with the class signature and lifecycle split described in the Decision section. Verify `Initialize()` contains no `SpawnActor` calls (static analysis or code review).
2. **`Initialize()` first invokes `Collection.InitializeDependency(URunStateMachineSubsystem::StaticClass())` then `Collection.InitializeDependency(UDPCSubsystem::StaticClass())` immediately after `Super::Initialize(Collection)`** (added by INT-004 amendment 2026-06-26). Grep gate: any `UWaveSpawnerSubsystem::Initialize()` body where `GetSubsystem<UDPCSubsystem>` precedes both `InitializeDependency` calls is a regression. Verification Required #5 establishes the Initialize-completion semantic; if construction-only, switch to Risk 5 fallback.
3. Implement `OnFirstWorldLoad()` with a one-shot guard. Run AC-WS-20 (pool count assertion at `PostLoadMapWithWorld` exit) as the acceptance gate.
4. Bind `OnDPCFrameReady()` delegate in `Initialize()` AFTER the two `InitializeDependency` calls from step 2. Confirm admission logic does not appear in `Tick()`.
5. Register `WaveSpawner_SpawnActor_at_Initialize`, `WaveSpawner_as_UWorldSubsystem`, and `WaveSpawner_Initialize_without_InitializeDependency_on_RSM_and_DPC` in the Forbidden Patterns registry.
6. Run AC-WS-15 (zero `SpawnActor`/`Destroy` during a full 60s run) as the integration gate.

**Rollback plan**: If the `UGameInstanceSubsystem` hosting proves incorrect (e.g., a UE 5.7 lifecycle constraint not anticipated by this ADR), the fallback is to migrate to an explicit `UGameInstance` subclass owning the spawner directly — equivalent lifecycle, avoids the subsystem factory, allows manual `Initialize`/`Deinitialize` control. This migration supersedes this ADR; author a new ADR at that point.

## Validation Criteria

- [ ] AC-WS-15: `WaveSpawnerSubsystem->GetPool().Num() == 23` at `OnFirstWorldLoad()` exit; world-actor census via `UGameplayStatics::GetAllActorsOfClass` asserts 23 `AWave` actors unchanged at end of any run + replay cycle. (GDD line 1095.)
- [ ] AC-WS-20: Pool count == 23 at first-world-load hook exit; `UPROPERTY() TArray<TObjectPtr<AWave>>` confirmed via reflection. (GDD line 1107.)
- [ ] AC-WS-30: Admission-tick CPU ≤ 0.30 ms p99 on iPhone XR + Pixel 5 / Galaxy A52 under Unreal Insights; stat scope marker on `UWaveSpawnerSubsystem`. ADVISORY-at-story-Done, BLOCKING-at-Alpha. (GDD line 1138.)
- [ ] AC-WS-31: Pool pre-allocation completes within one frame (≤ 16.6 ms) on mid-tier mobile at first-world-load. Replay re-entry does NOT re-fire allocation. ADVISORY-at-story-Done, BLOCKING-at-Alpha. (GDD line 1139.)
- [ ] No `SpawnActor` or `Destroy` calls observed during active gameplay (Rule 11 regression check via instrumented Shipping-equivalent build).
- [ ] `Tick()` contains no admission logic; admission fires only inside `OnDPCFrameReady()` callback (code review gate).

## Forbidden Patterns

These patterns are registered as project-level forbidden by this ADR. Add to `docs/registry/architecture.yaml` Forbidden Patterns registry on write approval.

| Pattern | Reason |
|---------|--------|
| `WaveSpawner_as_UWorldSubsystem` | Re-initializes per `UWorld`; fires 16.6 ms pool-alloc hitch on every death-retry |
| `WaveSpawner_SpawnActor_at_Initialize` | `UWorld` unavailable at `UGameInstanceSubsystem::Initialize()` in UE 5.7; `SpawnActor` asserts |
| `WaveSpawner_per_run_pool_reallocation` | Pool must be allocated once per session and reused across all replays; per-run allocation defeats the `UGameInstanceSubsystem` hosting choice |
| `WaveSpawner_Initialize_without_InitializeDependency_on_RSM_and_DPC` | INT-004 amendment 2026-06-26. `UWaveSpawnerSubsystem::Initialize()` body must contain BOTH `Collection.InitializeDependency(URunStateMachineSubsystem::StaticClass())` AND `Collection.InitializeDependency(UDPCSubsystem::StaticClass())` BEFORE retrieving `UDPCSubsystem` via `GetSubsystem<>` to bind `OnPostTickFrameStatePublished`. Missing either call risks silent admission-delegate drop in production (partially-initialized DPC at retrieval time). Grep-verifiable: any `UWaveSpawnerSubsystem::Initialize()` where `GetSubsystem<UDPCSubsystem>` line precedes both `InitializeDependency` lines is forbidden. |

## GDD Requirements Addressed

| GDD Document | System | Requirement | How This ADR Satisfies It |
|-------------|--------|-------------|--------------------------|
| `design/gdd/wave-spawner-pattern-library.md` | Wave Spawner | Rule 7 atomicity contract (line 125): "atomic" means single tick-step on the game thread; `UGameInstanceSubsystem` is game-thread-only by default | `UGameInstanceSubsystem` subsystem runs game-thread-only; no concurrent writer to `scheduled` exists; statement-level torn-write hazards are structurally precluded |
| `design/gdd/wave-spawner-pattern-library.md` | Wave Spawner | Rule 11 pool pre-allocation (line 157): pre-allocate exactly once per session at first-world-load hook; pool reused across replays | `OnFirstWorldLoad()` bound to `FCoreUObjectDelegates::PostLoadMapWithWorld`; one-shot guard prevents re-fire; replay re-entry (`Flushing → Cold → Active` per EC-WS-8) reuses pool unchanged |
| `design/gdd/wave-spawner-pattern-library.md` | Wave Spawner | Engine subsystem integration § (lines 442–457): class is `UWaveSpawnerSubsystem`, U-prefix, `FTickableGameObject` multiple inheritance, `ETickableTickType::Conditional`, `GetStatId()` returning `STATGROUP_WaveSpawner` | Decision section and Key Interfaces faithfully implement all inscribed R2a-1 + R2a-2 + R2a-4 binding decisions |
| `design/gdd/wave-spawner-pattern-library.md` | Wave Spawner | Dependencies E1 (line 940): pool pre-allocation via `FCoreUObjectDelegates::PostLoadMapWithWorld`; no `SpawnActor`/`Destroy` during gameplay | `Initialize()` registers the delegate; `OnFirstWorldLoad()` performs allocation; `Deinitialize()` releases |
| `design/gdd/wave-spawner-pattern-library.md` | Wave Spawner | Dependencies E3 (line 942): subsystem hosted as `UGameInstanceSubsystem`; pool fires at `PostLoadMapWithWorld`; replay reuses pool | Entire Decision section implements this requirement; replay re-use is the primary justification for the hosting choice |
| `design/gdd/wave-spawner-pattern-library.md` | Wave Spawner | AC-WS-20 (line 1107): pool count == 23 at first-world-load hook exit; `TObjectPtr<AWave>` GC-safe storage | Pool storage is `UPROPERTY() TArray<TObjectPtr<AWave>>`; allocated in `OnFirstWorldLoad()` |
| `design/gdd/wave-spawner-pattern-library.md` | Wave Spawner | AC-WS-30 (line 1138): admission-tick CPU ≤ 0.30 ms p99 on mid-tier mobile | `GetStatId()` stat scope + Unreal Insights gate defined; numeric target deferred to Alpha |
| `design/gdd/wave-spawner-pattern-library.md` | Wave Spawner | AC-WS-31 (line 1139): pool pre-allocation ≤ 16.6 ms on mid-tier mobile; replay does NOT re-fire | One-shot guard in `OnFirstWorldLoad()`; gate defined; numeric verification deferred to Alpha |

## Related

- **OQ-WS-3 closure record**: `design/gdd/wave-spawner-pattern-library.md` line 1167 — "CLOSED R1a 2026-06-21 — locked as `UGameInstanceSubsystem` per perf-analyst R7 + CD R1 Q2-locked user decision."
- **R1a-2 binding decision**: `design/gdd/wave-spawner-pattern-library.md` line 34 — OQ-WS-3 locked as `UGameInstanceSubsystem`; performance-driven.
- **R2a-1 tick host inscription**: `design/gdd/wave-spawner-pattern-library.md` lines 444–448 — multi-inherit, `FTickableGameObject`, `ETickableTickType::Conditional`, `STATGROUP_WaveSpawner`.
- **R2a-2 tick ordering pin**: `design/gdd/wave-spawner-pattern-library.md` lines 450–455 — DPC delegate subscription replacing tick-group dependency.
- **R2a-4 lifecycle correction**: `design/gdd/wave-spawner-pattern-library.md` line 457 — `Initialize()` does not call `SpawnActor`; deferred to `PostLoadMapWithWorld`.
- **Rule 7 atomicity contract**: `design/gdd/wave-spawner-pattern-library.md` line 125.
- **Rule 11 pool**: `design/gdd/wave-spawner-pattern-library.md` line 157.
- **Dependencies E1**: `design/gdd/wave-spawner-pattern-library.md` line 940.
- **Dependencies E3**: `design/gdd/wave-spawner-pattern-library.md` line 942.
- **AC-WS-15**: `design/gdd/wave-spawner-pattern-library.md` line 1095.
- **AC-WS-20**: `design/gdd/wave-spawner-pattern-library.md` line 1107.
- **AC-WS-30**: `design/gdd/wave-spawner-pattern-library.md` line 1138.
- **AC-WS-31**: `design/gdd/wave-spawner-pattern-library.md` line 1139.
- ADR-0001: Palm rejection and `FTouchRadiusBridgePlugin` — establishes native plugin pattern; no dependency on this ADR.
- ADR-0002: Haptic platform bridge — independent; no dependency on this ADR.
- ADR-0003: 60 Hz drain queue architecture (`FInputSystem`) — `UGameInstanceSubsystem` was explicitly rejected for `FInputSystem` there (Alternative 2 in ADR-0003) because `MakeUnique<>` is incompatible with UObject construction. That rejection is specific to the non-UObject `FInputSystem` design and does not conflict with using `UGameInstanceSubsystem` for the Wave Spawner, which IS a UObject-derived class using the standard subsystem factory.
- OQ-PW-3 (HISM vs ISMC) ADR for Pull-Wave renderer implementation — ADR-0006 (Accepted 2026-06-24, amended 2026-06-26 for INT-001 shared-host topology). Sibling decision on class choice; coupled to this ADR by INT-001 component-ownership topology — see Implementation Guideline 7. The renderer components live on a separate singleton `APullWaveSubsystemActor`, NOT on pooled `AWave` actors.
- **architecture-review 2026-06-25** (`docs/architecture/architecture-review-2026-06-25.md`) — INT-001 surfacing. The Implementation Guideline 7 cross-reference inscribed here is the partner artifact to ADR-0006 sub-question (f).
- ADR-0007 (`docs/architecture/adr-0007-run-state-machine-hosting.md`, Accepted 2026-06-26 after Proposed 2026-06-25 + amendments INT-005 + INT-006) — declares `URunStateMachineSubsystem` (the InitializeDependency target added by INT-004 Implementation Guideline 8). ADR-0007 Implementation Guideline 3 mandates this subsystem call `InitializeDependency` on RSM before retrieving RSM-side seams.
- ADR-0008 (`docs/architecture/adr-0008-dpc-subsystem-hosting.md`, Accepted 2026-06-27 after Proposed 2026-06-26) — declares `UDPCSubsystem` (the InitializeDependency target added by INT-004 Implementation Guideline 8) and supplies `OnPostTickFrameStatePublished` bound by R2a-2. ADR-0008 Implementation Guideline 3 mandates this subsystem call `InitializeDependency` on DPC before binding `OnPostTickFrameStatePublished`.
- **architecture-review 2026-06-26** (`docs/architecture/architecture-review-2026-06-26.md`) — INT-004 surfacing (BLOCKING for Wave Spawner enter-implementation). Implementation Guideline 8 + Risk 5 + Forbidden Pattern `WaveSpawner_Initialize_without_InitializeDependency_on_RSM_and_DPC` inscribed here are the closure artifacts; ADR-0007 IG-3 + ADR-0008 IG-3 upgraded SHOULD → MUST in the same pass.

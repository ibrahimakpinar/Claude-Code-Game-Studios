# Story 001: Subsystem Class, Object Pool, and Three-Pool Structure

> **Epic**: Wave Spawner Pattern Library
> **Status**: Complete
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: M (~3–4h)
> **Manifest Version**: (none — docs/architecture/control-manifest.md not found; run /create-control-manifest)
> **Last Updated**: 2026-08-19

## Context

**GDD**: `design/gdd/wave-spawner-pattern-library.md`
**Requirement**: `TR-WS-008`, `TR-WS-009`, `TR-WS-010`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0005: Wave Spawner Subsystem Hosting (primary); ADR-0011: Wave Spawner Pattern Library — D1 Three-Pool Architecture (secondary)
**ADR Decision Summary**: The spawner is hosted as `UWaveSpawnerSubsystem : public UGameInstanceSubsystem, public FTickableGameObject`. It persists across world transitions (avoiding the 16.6ms per-replay pool-alloc hitch), pre-allocates 23 `AWave` actors at `FCoreUObjectDelegates::PostLoadMapWithWorld` (not at `Initialize()` — UWorld is unavailable there), and exposes the three-pool (`FPatternPool`) data structure for OPENER/MID/PEAK. Pool storage is `UPROPERTY() TArray<TObjectPtr<AWave>>` for GC safety.

**Engine**: Unreal Engine 5.7 | **Risk**: HIGH
**Engine Notes**: `FCoreUObjectDelegates::PostLoadMapWithWorld` fire-once-per-load semantics must be verified in UE 5.7 source. `UGameInstanceSubsystem::Initialize()` runs before UWorld construction — `GetWorld()` returns null there; `SpawnActor` is forbidden at that hook. `FSubsystemCollectionBase::InitializeDependency()` pin must guarantee RSM + DPC `Initialize()` RETURN before Wave Spawner proceeds. `TObjectPtr<T>` is UE 5.0+ GC-safe pointer — stable. `ETickableTickType::Conditional` tick gating pattern — verify pattern unchanged in UE 5.7. (ADR-0005 Verification Required §1–5.)

**Control Manifest Rules (Feature layer)**:
- No control manifest found at `docs/architecture/control-manifest.md` — run `/create-control-manifest`.

---

## Acceptance Criteria

*From GDD `design/gdd/wave-spawner-pattern-library.md`, scoped to this story:*

- [x] **AC-WS-20a (BLOCKING — TR-WS-008)**: At `FCoreUObjectDelegates::PostLoadMapWithWorld` delegate exit, `WavePool.Num() == 23`. Each entry is a valid non-null `AWave*`. Pool pre-allocation fires exactly once per session; calling `OnFirstWorldLoaded` a second time (simulated via test) leaves `WavePool.Num()` unchanged at 23.

- [x] **AC-WS-20b (BLOCKING — TR-WS-009)**: Pool storage declared as `UPROPERTY() TArray<TObjectPtr<AWave>>`. The `UPROPERTY()` macro is present (GC anchor — prevents AWave actors being garbage-collected while subsystem lives). `TObjectPtr<AWave>` is used, not raw `AWave*`. `UGameInstanceSubsystem::Deinitialize()` clears the pool array, releasing GC anchors.

- [x] **AC-WS-20c (BLOCKING — TR-WS-008)**: Replay re-entry (`Flushing → Cold → Active`) reuses the existing pool — `PostLoadMapWithWorld` is NOT re-fired on replay. Given pool allocated in session, `WavePool.Num()` remains 23 after a simulated replay cycle. `bPoolAllocated` guard confirmed true post-allocation.

- [x] **AC-WS-10x (BLOCKING — TR-WS-010)**: `UWaveSpawnerSubsystem` compiles with dual base classes (`UGameInstanceSubsystem` and `FTickableGameObject`). `GetTickableTickType()` returns `ETickableTickType::Conditional`. `GetStatId()` returns a stat ID in `STATGROUP_WaveSpawner` (per ADR-0005 IG-5 — not `STATGROUP_Tickables`). `IsTickable()` returns `false` when lifecycle state is `Cold` (verified by constructing subsystem without triggering Cold→Active).

---

## Implementation Notes

*Derived from ADR-0005 Implementation Guidelines + ADR-0011 D1:*

**Class declaration (SLIPSTORM module, `Source/SLIPSTORM/WaveSpawner/`):**
```cpp
UCLASS()
class SLIPSTORM_API UWaveSpawnerSubsystem
    : public UGameInstanceSubsystem
    , public FTickableGameObject
{
    GENERATED_BODY()

public:
    // --- UGameInstanceSubsystem ---
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // --- FTickableGameObject ---
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override
        { RETURN_QUICK_DECLARE_CYCLE_STAT(UWaveSpawnerSubsystem, STATGROUP_Tickables); }
    virtual ETickableTickType GetTickableTickType() const override
        { return ETickableTickType::Conditional; }
    virtual bool IsTickable() const override;

private:
    UPROPERTY()
    TArray<TObjectPtr<AWave>> WavePool;   // GC anchor for 23 pre-allocated AWave actors

    UPROPERTY()
    FPatternPool OpenerPool;
    UPROPERTY()
    FPatternPool MidPool;
    UPROPERTY()
    FPatternPool PeakPool;
    // ...
};
```

**Initialize() — ordering-pin FIRST, then delegate subscription, NO SpawnActor:**
```cpp
void UWaveSpawnerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    // Pin: RSM + DPC must be fully initialized before this body runs
    Collection.InitializeDependency(URunStateMachineSubsystem::StaticClass());
    Collection.InitializeDependency(UDPCSubsystem::StaticClass());

    // Subscribe to first-world-load hook (pool pre-allocation fires there, NOT here).
    // PostLoadMapWithWorld is correct: fires after UWorld is fully initialised and
    // SpawnActor is safe. OnPostWorldCreation fires too early (before world init) — do NOT use it.
    FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(
        this, &UWaveSpawnerSubsystem::OnFirstWorldLoaded);
    // DPC post-tick delegate subscription wired in Story 007 (tick ordering pin).
}
```

**OnFirstWorldLoaded (pool pre-allocation — fires once, safe to SpawnActor):**
```cpp
void UWaveSpawnerSubsystem::OnFirstWorldLoaded(UWorld* World)
{
    if (bPoolAllocated) return;  // guard: fires once per session
    WavePool.Reserve(23);
    for (int32 i = 0; i < 23; ++i)
    {
        AWave* Wave = World->SpawnActor<AWave>();
        check(Wave);
        WavePool.Add(Wave);
    }
    bPoolAllocated = true;
}
```

**Three-pool structure (`FPatternPool`)** — from ADR-0011 D1:
```cpp
USTRUCT()
struct FPatternPool
{
    GENERATED_BODY()
    UPROPERTY() TArray<FPatternDefinition> BarragePatterns;
    UPROPERTY() TArray<FPatternDefinition> NonBarragePatterns;
    // Cook-time verifier populates; runtime never mutates.
};
```

**IsTickable()** — returns false in Cold and Idle states to avoid idle CPU overhead.

**Pool Acquire/Release methods:**
- `AWave* Acquire()` — returns first available pooled AWave and marks it in-use.
- `void Release(int32 WaveId)` — returns a wave to the pool; called from `OnWaveDespawned`.

**NO `UWorld::SpawnActor` or `Destroy` calls outside `OnFirstWorldLoaded`.** Forbidden once pool is allocated (Rule 11).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 002: Six-state lifecycle state machine (Cold/Active/Holding/Flushing/Idle transitions).
- Story 003: Rule 1 admission gate and cadence logic (tick body admission flow).
- Story 006: `IWaveSpawnerCallback` production interface and Seam 13 test stub.
- Story 007: RSM + DPC delegate subscriptions and pause-flush behavior.
- Story 008: Cook-time validator (Rule 15 binding checks on pool content).

---

## QA Test Cases

*Test cases not yet defined — run /qa-plan to generate them.*

---

## Test Evidence

**Story Type**: Logic
**Required evidence**: `Source/SLIPSTORM/Tests/Unit/WaveSpawner/WaveSpawnerPoolTest.cpp` — must exist and pass

**Status**: [x] Created — `Source/SLIPSTORM/Tests/Unit/WaveSpawner/WaveSpawnerPoolTest.cpp` (4 test commands; TC1 + TC3 require EditorContext, TC2 + TC4 are headless)

---

## Dependencies

- Depends on: None (first Wave Spawner story)
- Unlocks: Story 002 (lifecycle state machine requires the class skeleton), Story 008 (cook-time validator requires FPatternPool structure)

---

## Completion Notes

**Completed**: 2026-08-19
**Criteria**: 4/4 passing
**Deviations**:
- ADVISORY: Story Implementation Notes code snippet shows `STATGROUP_Tickables` in the `GetStatId()` example — stale doc; actual implementation correctly uses `STATGROUP_WaveSpawner` per ADR-0005 IG-5. No code impact.
- FIXED before review: `WaveSpawnerTypes.h` had `DECLARE_STATS_GROUP` before `#pragma once` and `#include "CoreMinimal.h"`. Reordered: `#pragma once` → includes → `DECLARE_STATS_GROUP`.
**Test Evidence**: `Source/SLIPSTORM/Tests/Unit/WaveSpawner/WaveSpawnerPoolTest.cpp` — 4 test commands (COMPLEX macro). TC1 + TC3 require EditorContext; TC2 + TC4 headless.
**Engine VRs**: All 5 source-verified against UE 5.7 engine source (`/Users/Shared/Epic Games/UE_5.7/Engine/Source/`). VR-1 through VR-5 closed.
**Code Review**: APPROVED — all ADR-0005 IG and ADR-0011 D1 constraints satisfied.

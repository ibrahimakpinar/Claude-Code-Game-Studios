# ADR-0008: DPC Hosting and FDPCFrameState Atomic Snapshot Publication

## Status

Accepted

## Date

2026-06-26 (Proposed) / 2026-06-27 (Accepted)

## Last Verified

2026-06-27 (status promotion Proposed → Accepted — sole Depends-On gate satisfied: ADR-0007 (RSM Hosting) promoted Proposed → Accepted on 2026-06-26 after closing all three architecture-review-2026-06-26 surgical fixes (INT-004 + INT-005 + INT-006). No open INT findings against ADR-0008 — INT-004 ADR-0008-side Implementation Guideline 3 SHOULD → MUST upgrade closed in INT-004 pass (2026-06-26); architecture-review-2026-06-26 surfaced zero ADR-0008-internal blockers. ADR-0005 forward contract on `OnPostTickFrameStatePublished` (ADR-0005 lines 252–258) closed by this ADR's Decision Makers row + Migration Plan. `docs/architecture/requirements-traceability.md` line 145 prior-state "ADR-0008 is now eligible for its own Proposed → Accepted flip" advisory satisfied; line rewritten in this pass to record the promotion.)

## Decision Makers

- **creative-director (user, 2026-06-26)** — sign-off on 4 design questions (production outer, curve validation timing, watchdog accumulator mechanism, enum location)
- **DPC GDD R6 author (2026-06-05)** — pre-existing GDD design locks at Rules 3, 10, 14, EC-13; explicit "OQ-7 ADR MUST specify DPC's production outer"
- **ADR-0007 (2026-06-25, amended 2026-06-26)** — codified DPC's 5 RSM forward contracts (`ForceTickNow`, `bHasTickedThisFrame`, non-callback invariant, `RequestAbort`, game-thread); declared `EDPCAbortReason` forward-decl surface
- **ADR-0005 (Accepted 2026-06-24)** — one-sided forward contract on `OnPostTickFrameStatePublished` (ADR-0005 lines 252–258); this ADR closes the DPC half
- **unreal-specialist** — engine specialist validation (Step 5.5)

## Summary

SLIPSTORM's Difficulty & Phase Controller is hosted via a thin `UDPCSubsystem : public UGameInstanceSubsystem` wrapper that owns the canonical `UDPCController : public UObject, public FTickableGameObject` (GDD Rule 3-locked type). The subsystem provides the standard `UGameInstance::GetSubsystem<UDPCSubsystem>()` discovery path used by Wave Spawner; the controller does the work. DPC publishes the post-tick `FDPCFrameState` snapshot via `OnPostTickFrameStatePublished` multicast (closes the ADR-0005 R2a-2 forward contract). DPC's `Tick()` calls `RSM->ForceTickNow()` as its first statement, validates `UCurveFloat` assets at subsystem `Initialize()`, accumulates the async-load watchdog via DeltaTime-with-clamp inside `Tick()`, and defines `EDPCAbortReason` in a shared header (`Public/DPC/DPCAbortReason.h`) that RSM forward-declares.

## Engine Compatibility

| Field | Value |
|-------|-------|
| **Engine** | Unreal Engine 5.7 |
| **Domain** | Core |
| **Knowledge Risk** | MEDIUM — UE 5.4–5.7 are post-LLM-cutoff (May 2025), but the patterns used (UGameInstanceSubsystem holding a UPROPERTY TObjectPtr to a UObject + FTickableGameObject; UCurveFloat asset references; multicast snapshot delegate) are consistent with ADR-0005's hosting pattern and require the same UE 5.7 source-verification baseline |
| **References Consulted** | `docs/engine-reference/unreal/VERSION.md`, `docs/engine-reference/unreal/breaking-changes.md`, `docs/engine-reference/unreal/deprecated-apis.md`, `docs/engine-reference/unreal/current-best-practices.md`, `docs/architecture/adr-0005-wave-spawner-subsystem-hosting.md`, `docs/architecture/adr-0007-run-state-machine-hosting.md`, `docs/architecture/platform-seam-interfaces.md` (Seams 7, 8, 9 for DPC test seams) |
| **Post-Cutoff APIs Used** | `UGameInstanceSubsystem::Initialize()` / `Deinitialize()` (lifecycle), `FTickableGameObject::GetTickableTickType()` with `ETickableTickType::Conditional` (tick gating), `FSubsystemCollectionBase::InitializeDependency()` (dependency ordering with RSM + Wave Spawner), `UCurveFloat::GetFloatValue()` (curve evaluation hot path), `UCurveFloat::FloatCurve.Keys` (asset validation via TArray inspection), `DECLARE_MULTICAST_DELEGATE_OneParam` (non-dynamic multicast for `OnPostTickFrameStatePublished`), `TObjectPtr<T>` (GC-safe pointer modernization, UE 5.0+), `SCOPE_CYCLE_COUNTER` (per-tick profiling instrumentation per GDD Rule 10) |
| **Verification Required** | (1) Confirm `UCurveFloat::FloatCurve.Keys` inspection in `Initialize()` works against an unloaded soft reference — if not, the curve asset must be hard-referenced via `UPROPERTY()` for guaranteed availability at subsystem Initialize time. (2) Confirm `FTickableGameObject` member on a `UObject` owned by a `UGameInstanceSubsystem` (DPC GDD R6 + ADR-0008 outer choice) self-registers identically to `FTickableGameObject` on the subsystem itself — verify a `UPROPERTY()`-owned `UDPCController` instance produces exactly one tick per game frame and is not double-ticked. (3) Confirm the `UDPCSubsystem` subsystem-collection order via `Collection.InitializeDependency(URunStateMachineSubsystem::StaticClass())` guarantees RSM's `Initialize()` completes BEFORE DPC subsystem's `Initialize()` runs (this is the same verification flagged by ADR-0007 Risk 1; both ADRs depend on the same UE 5.7 semantic). (4) Confirm `UCurveFloat::GetFloatValue()` is safe to call with non-finite input clamped to [0, 1] (DPC GDD Rule 14 input-side guard) under the UE 5.7 mobile curve runtime. (5) Confirm `SCOPE_CYCLE_COUNTER` is correctly scoped when used as the first statement of a `FTickableGameObject::Tick()` override in UE 5.7. |

> **Note**: Knowledge Risk is MEDIUM. This ADR must be re-validated if the project
> upgrades engine versions. Flag as "Superseded" and author a new ADR on upgrade.

## ADR Dependencies

| Field | Value |
|-------|-------|
| **Depends On** | ADR-0007 (must be Accepted before this ADR; this ADR consumes `URunStateMachineSubsystem::ForceTickNow()` + `RequestAbort(EDPCAbortReason)` + the forward declaration of `EDPCAbortReason` in RSM's public header) |
| **Enables** | ADR-0005 (closes one-sided `OnPostTickFrameStatePublished` forward contract cited at ADR-0005 lines 252–258); all DPC, Pull-Wave (consumes `FDPCFrameState.telegraph_window_s`), Wave Spawner admission stories (Wave Spawner Initialize binds to the delegate decided here) |
| **Blocks** | DPC Epic implementation; Wave Spawner story-Done (Wave Spawner Initialize() binds to `UDPCSubsystem::OnPostTickFrameStatePublished` declared here) |
| **Ordering Note** | Core layer; depends on Foundation ADR-0007. Author after ADR-0007 amendment (codifies ForceTickNow + RequestAbort). Independent of ADR-0001/0002/0003 (Input System chain) and ADR-0004 (methodology). |

## Context

### Problem Statement

The Difficulty & Phase Controller is the run-arc authority for SLIPSTORM (DPC GDD line 10). Each tick during RUNNING, it polls RSM's `remaining_time`, computes `t_norm`, evaluates 3 `UCurveFloat` scalars (wave spawn interval, telegraph window, max concurrent waves), maps `t_norm` to one of 3 discrete `ERunPhase` values, and publishes an atomic `FDPCFrameState` snapshot consumed by Wave Spawner, Pull-Wave Behavior, In-Run HUD, Audio Controller, and Telegraph System.

The architecture-review pass on 2026-06-25 surfaced 24 DPC technical requirements (TR-DPC-001 through TR-DPC-024 in `docs/architecture/tr-registry.yaml`) with zero ADR coverage. ADR-0005 lines 252–258 cite `UDPCSubsystem::OnPostTickFrameStatePublished` as a forward contract from DPC, declared as `DECLARE_MULTICAST_DELEGATE_OneParam(FOnPostTickFrameStatePublished, const FDPCFrameState&)` — but no ADR declares it. The DPC GDD R6 (Approved 2026-06-05, R-Updated mini-review NEEDS REVISION 2026-06-18, R11a-arithmetic propagation pass DONE 2026-06-18) is the authoritative design document with 1015 lines of design specification, but five architectural choices remain GDD-open and ADR-required:

1. **Production outer**: DPC GDD Rule 3 explicitly says "the OQ-7 ADR MUST specify DPC's production outer to fully resolve the lifetime contract." DPC's UE type itself is GDD-locked at `class UDPCController : public UObject, public FTickableGameObject`, but the owner is not.
2. **`OnPostTickFrameStatePublished` declaration site**: Cited by ADR-0005 + Wave Spawner R3a FC-3 as a forward contract on DPC; no ADR has declared it.
3. **`UCurveFloat` asset validation policy**: TR-DPC-016 requires validation at Initialize, but the GDD doesn't pin which subsystem/object owns the validation pass.
4. **Async-load watchdog mechanism**: TR-DPC-020 requires a 5.0s timeout with per-tick delta clamp (DPC GDD EC-13), but the specific accumulator mechanism (DeltaTime sum vs OnEndFrame counter vs RSM TimeSource pull) is not GDD-pinned.
5. **`EDPCAbortReason` enum location**: DPC GDD R6 forward contract item 4 says "owned by DPC" but doesn't specify header/module placement; ADR-0007 forward-declares it on RSM's side without binding the definition site.

### Current State

No DPC implementation exists. The DPC GDD R6 is the authoritative design document. ADR-0007 (Accepted 2026-06-26 after Proposed 2026-06-25 + amendments INT-005 Tick() guard / INT-006 FCString::Strtoui64 salt-parse switch; original Proposed amended 2026-06-26 to add ForceTickNow + RequestAbort + EDPCAbortReason forward declaration) codifies DPC's RSM-side forward contracts but does NOT specify DPC's own hosting. ADR-0005 (Accepted 2026-06-24, amended 2026-06-26 INT-004 to add InitializeDependency calls on RSM + DPC) cites DPC's `OnPostTickFrameStatePublished` as a forward contract.

### Constraints

- **Mobile platform, 60 fps, 16.6 ms frame budget** (`.claude/docs/technical-preferences.md`).
- **DPC UE type is GDD-locked** at `class UDPCController : public UObject, public FTickableGameObject` (DPC GDD Rule 3 R6 binding). This ADR does NOT re-litigate the type choice — it specifies the OUTER that owns the controller.
- **`Tick()` body order GDD-locked**: `SCOPE_CYCLE_COUNTER(STAT_DPCTick)` → `check(IsInGameThread())` → `RSM->ForceTickNow()` → (active gate per Rule 14) → curve evaluation per Rule 8 → snapshot publication per Rule 10 → `OnPostTickFrameStatePublished` broadcast (DPC GDD Rule 3 + Rule 10).
- **Tick during async-load window**: `IsTickable()` returns `true` unconditionally (DPC GDD R7 BLOCKING fix); tick body advances the watchdog accumulator + publishes the pre-init inactive snapshot when `bInitialized == false` (GDD Rule 3 + Rule 14 + EC-13).
- **`FDPCFrameState` struct is GDD-locked** as `USTRUCT(BlueprintType)` with GENERATED_BODY and 6 `UPROPERTY(BlueprintReadOnly)` fields per Rule 10. This ADR does NOT re-specify the struct.
- **`OnPostTickFrameStatePublished` is GDD-locked** at `DECLARE_MULTICAST_DELEGATE_OneParam(FOnPostTickFrameStatePublished, const FDPCFrameState&)` per Rule 10 + Wave Spawner R3a FC-3.
- **No callbacks from inside `RSM->ForceTickNow()`** (ADR-0007 non-callback invariant); DPC's post-`ForceTickNow()` reads use direct accessors (`GetCurrentState()` / `GetRemainingTime()` / `IsPaused()` / `IsResumeGrace()`).
- **`UCurveFloat::GetFloatValue()` returns NaN on a malformed curve** — DPC must clamp the `t_norm` input AND validate the curve output (GDD Rule 8 + Rule 14).
- **`EDPCAbortReason` is owned by DPC** (DPC GDD R6 forward contract item 4); RSM ADR-0007 only forward-declares the enum on its public surface.

### Requirements

- DPC outer survives world reloads (mirrors ADR-0005 / ADR-0007 session-continuity requirement); curve assets and watchdog state do not re-initialize on death-replay.
- `UCurveFloat` asset validation runs at Initialize() — fail-fast on malformed assets at game-instance startup rather than mid-gameplay.
- Async-load watchdog accumulates correctly under variable frame rate; single huge frame (debugger resume) does not skip the timeout.
- Wave Spawner can bind to `OnPostTickFrameStatePublished` in its own `Initialize()` without ordering races.
- `EDPCAbortReason` enum visible to both RSM (forward-decl only, no header dependency) and DPC (full definition).
- Per-tick DPC CPU < 0.20 ms p99 mid-tier mobile (Alpha-gate placeholder; the GDD's existing `STAT_DPCTick` profiling surface is the measurement gate per Rule 10 + AC-22b).

## Decision

DPC is hosted via a thin `UDPCSubsystem : public UGameInstanceSubsystem` wrapper that owns a `UDPCController : public UObject, public FTickableGameObject` (DPC GDD Rule 3-locked) instance via `UPROPERTY() TObjectPtr<UDPCController>`. The four design questions surfaced by `/architecture-decision` on 2026-06-26 resolved as follows.

### Structural Decision 1 — Production outer: `UDPCSubsystem` (UGameInstanceSubsystem) owns `UDPCController`

`UDPCSubsystem` is a thin wrapper. It declares no game logic of its own — `Initialize()` creates the `UDPCController` instance, validates curve assets, subscribes to RSM via `InitializeDependency`, and exposes the `OnPostTickFrameStatePublished` multicast delegate (declared ON THE SUBSYSTEM, not on the controller, so Wave Spawner's `GetSubsystem<UDPCSubsystem>()->OnPostTickFrameStatePublished.AddUObject(...)` is the canonical bind path). `Deinitialize()` releases the controller (UPROPERTY anchor drops → GC collects).

`UDPCController` is the workhorse. It hosts the `FTickableGameObject` interface (the GDD-locked tick path), the `FDPCFrameState` current+previous fields, the `UCurveFloat` references, the watchdog accumulator, and `SCOPE_CYCLE_COUNTER(STAT_DPCTick)` instrumentation. `Tick(DeltaTime)` body is exactly as GDD Rule 3 + Rule 14 prescribe. The controller does NOT own the delegate (the subsystem does) — at the end of `Tick()`, the controller calls `OuterSubsystem->BroadcastFrameStatePublished(CurrentFrameState)` (a private subsystem method) to fire the multicast.

This split lets Wave Spawner + Pull-Wave use the canonical `UGameInstance::GetSubsystem<UDPCSubsystem>()` discovery path while preserving DPC GDD Rule 3's mandate that DPC's UE type is `UObject + FTickableGameObject` (not a subsystem class itself).

### Structural Decision 2 — `UCurveFloat` asset validation at `UDPCSubsystem::Initialize()`

Curve assets are referenced as `UPROPERTY() TObjectPtr<UCurveFloat>` fields on the subsystem (configured via a `UDataAsset` or Config-driven path; specific wiring deferred to implementation). At `Initialize()`, the subsystem validates each curve via the following gate before constructing the controller:

```cpp
bool ValidateCurveAsset(const TObjectPtr<UCurveFloat>& Curve, const TCHAR* CurveName)
{
    if (!Curve)
    {
        UE_LOG(LogDPC, Error, TEXT("Curve asset '%s' is null"), CurveName);
        return false;
    }
    const FRichCurve& FloatCurve = Curve->FloatCurve;
    if (FloatCurve.Keys.Num() < 2)
    {
        UE_LOG(LogDPC, Error, TEXT("Curve '%s' has fewer than 2 keys"), CurveName);
        return false;
    }
    const float FirstKey = FloatCurve.Keys[0].Time;
    const float LastKey  = FloatCurve.Keys.Last().Time;
    if (FirstKey > 0.0f || LastKey < 1.0f)
    {
        UE_LOG(LogDPC, Error,
            TEXT("Curve '%s' keys [%f, %f] do not span [0.0, 1.0]"),
            CurveName, FirstKey, LastKey);
        return false;
    }
    return true;
}
```

If any curve fails validation, `UDPCSubsystem::Initialize()` logs the failure and does NOT construct the controller — the subsystem enters a `bInitializationFailed = true` state. Wave Spawner's bind path detects this via `OnPostTickFrameStatePublished` never firing and falls into its own no-DPC fallback (which is, per Wave Spawner GDD, never spawning waves — equivalent to perpetual inactive snapshot). A diagnostic UE_LOG is the only player-visible side effect (debug builds only; Shipping silently no-ops with the same fallback).

Validation at Initialize satisfies TR-DPC-016 fail-fast: malformed curves surface at game-instance startup, not mid-gameplay.

### Structural Decision 3 — Watchdog accumulator: DeltaTime sum with per-tick clamp

`UDPCController::Tick(float DeltaTime)` accumulates the async-load watchdog inside the tick body:

```cpp
// Per-tick clamp prevents single huge frame (debugger resume, level streaming spike)
// from skipping the timeout. MAX_PER_TICK_DELTA_S = 0.1s caps any single tick's
// contribution; 50 consecutive max-cap ticks still cross the 5.0s timeout.
constexpr float ASYNC_LOAD_TIMEOUT_S = 5.0f;
constexpr float MAX_PER_TICK_DELTA_S = 0.1f;

watchdog_elapsed_s += FMath::Min(DeltaTime, MAX_PER_TICK_DELTA_S);

if (!bInitialized && watchdog_elapsed_s >= ASYNC_LOAD_TIMEOUT_S)
{
    // Bound-and-abort per GDD EC-13
    if (URunStateMachineSubsystem* RSM = GetRSM())
    {
        RSM->RequestAbort(EDPCAbortReason::AsyncLoadTimeout);
    }
    watchdog_elapsed_s = 0.0f;   // reset; subsequent ticks fall through
}
```

The `MAX_PER_TICK_DELTA_S = 0.1s` clamp matches DPC GDD EC-13's prescription. The accumulator state lives on `UDPCController` (private member); it is NOT shared with `FCoreDelegates::OnEndFrame` (already consumed by RSM ADR-0007 for `bHasTickedThisFrame` reset). The watchdog cancels when `bInitialized = true` is set (by the controller's own load-completion signal, OQ deferred to implementation — typically tied to curve-asset readiness verification).

### Structural Decision 4 — `EDPCAbortReason` enum location: shared header in DPC module

Definition lives at `Public/DPC/DPCAbortReason.h` inside the DPC module:

```cpp
// Public/DPC/DPCAbortReason.h
#pragma once

UENUM(BlueprintType)
enum class EDPCAbortReason : uint8
{
    None             = 0,
    AsyncLoadTimeout = 1,    // DPC GDD EC-13: watchdog expiry (5.0s timeout)
    // Future reasons appended here as DPC adds them; never renumber existing values.
};
```

RSM's public header forward-declares `enum class EDPCAbortReason : uint8;` (the `: uint8` underlying-type specification is required for forward decl with C++11 strongly-typed enums). RSM's `URunStateMachineSubsystem::RequestAbort(EDPCAbortReason Reason)` signature uses the forward declaration; RSM's `.cpp` stores the value as `LastAbortReason = static_cast<uint8>(Reason)` without including the DPC header. Code paths that need to switch on specific `EDPCAbortReason` values include the DPC header directly — this is only the DPC module itself and any future telemetry / logging consumer.

The DPC module's `Build.cs` does NOT export DPC as a public dependency from RSM. The forward-decl pattern keeps RSM's include graph clean.

### Structural Decision 5 (reaffirmation, not new) — DPC GDD Rule 3 type + tick body order

This ADR REAFFIRMS DPC GDD Rule 3's locked decisions and does not modify them. `UDPCController : public UObject, public FTickableGameObject` is the type. `Tick()` body order is `SCOPE_CYCLE_COUNTER` → `check(IsInGameThread())` → `RSM->ForceTickNow()` → active-state evaluation per Rule 14 → curve evaluation per Rule 8 → snapshot write per Rule 10 → `OuterSubsystem->BroadcastFrameStatePublished(CurrentFrameState)` (this ADR Structural Decision 1 — the broadcast moves to the subsystem because the delegate lives there). `IsTickable()` returns `true` unconditionally per R7 BLOCKING fix. `IsTickableInEditor() = false`. `IsTickableWhenPaused() = false`. `GetStatId()` returns `GET_STATID(STAT_DPCTick)`.

### Architecture

```
UGameInstance
    │
    ├── URunStateMachineSubsystem                       [ADR-0007]
    │       ├── ForceTickNow() — called by UDPCController::Tick first statement
    │       ├── RequestAbort(EDPCAbortReason) — called by UDPCController watchdog
    │       └── EDPCAbortReason forward-declared in RSM public header
    │
    ├── UDPCSubsystem                                   [UGameInstanceSubsystem; this ADR §1]
    │       │
    │       ├── Initialize(FSubsystemCollectionBase&)
    │       │       ├── Collection.InitializeDependency(URunStateMachineSubsystem::StaticClass())
    │       │       ├── Validate UCurveFloat assets (this ADR §2) — sets bInitializationFailed if any fail
    │       │       └── Construct UDPCController + set its outer-back-pointer to this subsystem
    │       │
    │       ├── Deinitialize()
    │       │       └── Releases TObjectPtr<UDPCController> Controller (GC collects on next pass)
    │       │
    │       ├── OnPostTickFrameStatePublished  [FOnPostTickFrameStatePublished multicast]
    │       │       └── Wave Spawner binds in its Initialize via AddUObject (ADR-0005 R2a-2)
    │       │
    │       ├── BroadcastFrameStatePublished(const FDPCFrameState&)  [private; called by Controller]
    │       │
    │       ├── GetCurrentFrameState() / GetPreviousFrameState() — pass-through to Controller
    │       │
    │       └── UPROPERTY() TObjectPtr<UDPCController> Controller
    │
    ├── UDPCController (UObject + FTickableGameObject)  [DPC GDD R6 Rule 3-locked type; this ADR §1, §5]
    │       │
    │       ├── Tick(float DeltaTime)
    │       │       ├── SCOPE_CYCLE_COUNTER(STAT_DPCTick)        [Rule 10 + AC-22b]
    │       │       ├── check(IsInGameThread())
    │       │       ├── RSM->ForceTickNow()                       [Rule 3 + ADR-0007]
    │       │       ├── watchdog_elapsed_s += FMath::Min(DeltaTime, 0.1f)   [§3 + EC-13]
    │       │       ├── if (!bInitialized && watchdog_elapsed_s >= 5.0f) RSM->RequestAbort(AsyncLoadTimeout)
    │       │       ├── if (RSM state != RUNNING || RSM paused): PublishInactiveSnapshot
    │       │       ├── else: compute t_norm + phase + 3 curve scalars per Rule 5 + 6 + 8
    │       │       ├── Apply NaN/finite guards per Rule 8 + Rule 14
    │       │       ├── Write FDPCFrameState fields per Rule 10
    │       │       └── OuterSubsystem->BroadcastFrameStatePublished(CurrentFrameState)
    │       │
    │       ├── FTickableGameObject:
    │       │       ├── IsTickable() → true unconditionally (DPC GDD R7 BLOCKING fix)
    │       │       ├── GetTickableTickType() → ETickableTickType::Conditional
    │       │       ├── IsTickableInEditor() → false
    │       │       ├── IsTickableWhenPaused() → false
    │       │       └── GetStatId() → GET_STATID(STAT_DPCTick)
    │       │
    │       ├── UPROPERTY() TObjectPtr<UCurveFloat> WaveSpawnIntervalCurve
    │       ├── UPROPERTY() TObjectPtr<UCurveFloat> TelegraphWindowCurve
    │       ├── UPROPERTY() TObjectPtr<UCurveFloat> MaxConcurrentWavesCurve
    │       │
    │       ├── FDPCFrameState CurrentFrameState  [GDD Rule 10 USTRUCT(BlueprintType)]
    │       ├── FDPCFrameState PreviousFrameState
    │       │
    │       ├── double watchdog_elapsed_s = 0.0
    │       ├── bool   bInitialized       = false
    │       │
    │       └── TWeakObjectPtr<UDPCSubsystem> OuterSubsystem  [back-pointer for broadcast]
    │
    └── UWaveSpawnerSubsystem                           [ADR-0005]
            └── Initialize() — Collection.InitializeDependency(UDPCSubsystem::StaticClass()) +
                               GetGameInstance()->GetSubsystem<UDPCSubsystem>()
                                   ->OnPostTickFrameStatePublished.AddUObject(this, &Handle...)
```

### Key Interfaces

```cpp
// Public/DPC/DPCAbortReason.h — definition site (Structural Decision 4)
#pragma once

UENUM(BlueprintType)
enum class EDPCAbortReason : uint8
{
    None             = 0,
    AsyncLoadTimeout = 1,    // DPC GDD EC-13: watchdog expiry
};

// Public/DPC/DPCFrameState.h — GDD Rule 10 locked (reproduced for ADR self-containment)
USTRUCT(BlueprintType)
struct FDPCFrameState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "DPC")
    ERunPhase current_phase = ERunPhase::OPENER;

    UPROPERTY(BlueprintReadOnly, Category = "DPC")
    float wave_spawn_interval_s = 4.0f;

    UPROPERTY(BlueprintReadOnly, Category = "DPC")
    float telegraph_window_s = 0.94f;     // GDD R11a-arithmetic 2026-06-18 default (was 0.9f pre-R10d)

    UPROPERTY(BlueprintReadOnly, Category = "DPC")
    int32 max_concurrent_waves = 1;

    UPROPERTY(BlueprintReadOnly, Category = "DPC")
    double t_norm = 0.0;                  // diagnostic; not a decision input

    UPROPERTY(BlueprintReadOnly, Category = "DPC")
    bool is_active = false;
};

// Per GDD Rule 10 + Wave Spawner R3a FC-3: non-dynamic multicast.
DECLARE_MULTICAST_DELEGATE_OneParam(
    FOnPostTickFrameStatePublished,
    const FDPCFrameState& /* PublishedState */);

// UDPCSubsystem.h — Structural Decision 1 outer
UCLASS()
class UDPCSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // Public delegate (Wave Spawner subscribes per ADR-0005 R2a-2 + Wave Spawner R3a FC-3)
    FOnPostTickFrameStatePublished OnPostTickFrameStatePublished;

    // Public pass-through accessors — read snapshot from owned Controller
    const FDPCFrameState& GetCurrentFrameState() const;
    const FDPCFrameState& GetPreviousFrameState() const;

private:
    // Called by Controller at end of its Tick (Structural Decision 1)
    friend class UDPCController;
    void BroadcastFrameStatePublished(const FDPCFrameState& State);

    // Curve assets (Structural Decision 2 — validated at Initialize)
    UPROPERTY()
    TObjectPtr<UCurveFloat> WaveSpawnIntervalCurve;

    UPROPERTY()
    TObjectPtr<UCurveFloat> TelegraphWindowCurve;

    UPROPERTY()
    TObjectPtr<UCurveFloat> MaxConcurrentWavesCurve;

    // Controller — owns the FTickableGameObject (DPC GDD Rule 3 locked)
    UPROPERTY()
    TObjectPtr<UDPCController> Controller;

    bool bInitializationFailed = false;
};

// UDPCController.h — DPC GDD Rule 3-locked type
UCLASS()
class UDPCController
    : public UObject
    , public FTickableGameObject
{
    GENERATED_BODY()

public:
    // Called by UDPCSubsystem::Initialize() after curve validation passes.
    // OuterSub holds the broadcast-back path; curve refs are passed for direct hold.
    void InitializeFromOuter(
        UDPCSubsystem* OuterSub,
        UCurveFloat*   InWaveSpawnIntervalCurve,
        UCurveFloat*   InTelegraphWindowCurve,
        UCurveFloat*   InMaxConcurrentWavesCurve);

    // --- FTickableGameObject interface ---
    virtual void Tick(float DeltaTime) override;
    virtual ETickableTickType GetTickableTickType() const override
    {
        return ETickableTickType::Conditional;
    }
    // DPC GDD R7 BLOCKING fix: always tickable (Path A resolution; unreal-specialist B-1)
    virtual bool IsTickable() const override { return true; }
    virtual bool IsTickableInEditor() const override { return false; }
    virtual bool IsTickableWhenPaused() const override { return false; }
    virtual TStatId GetStatId() const override
    {
        RETURN_QUICK_DECLARE_CYCLE_STAT(UDPCController, STATGROUP_DPC);
    }

    // Public accessors used by Subsystem pass-through
    const FDPCFrameState& GetCurrentFrameState()  const { return CurrentFrameState; }
    const FDPCFrameState& GetPreviousFrameState() const { return PreviousFrameState; }

private:
    URunStateMachineSubsystem* GetRSM() const;   // utility — caches result; recomputed if null

    // --- Snapshot state (GDD Rule 10 + Rule 11 — previous-frame field is OVERWRITTEN with old current before current updates) ---
    FDPCFrameState CurrentFrameState;
    FDPCFrameState PreviousFrameState;

    // --- Watchdog accumulator (Structural Decision 3 — DeltaTime sum with clamp) ---
    double watchdog_elapsed_s = 0.0;
    bool   bInitialized       = false;     // gated by GDD Rule 14

    // --- Curve refs (held via UPROPERTY for GC anchor; values mirror subsystem-side) ---
    UPROPERTY()
    TObjectPtr<UCurveFloat> WaveSpawnIntervalCurve;

    UPROPERTY()
    TObjectPtr<UCurveFloat> TelegraphWindowCurve;

    UPROPERTY()
    TObjectPtr<UCurveFloat> MaxConcurrentWavesCurve;

    // --- Outer back-pointer for broadcast (TWeakObjectPtr — subsystem is canonical owner) ---
    TWeakObjectPtr<UDPCSubsystem> OuterSubsystem;
};
```

### Implementation Guidelines

1. **`UDPCSubsystem::Initialize()` runs curve validation BEFORE constructing `UDPCController`.** If any curve fails validation, set `bInitializationFailed = true`, log a `LogDPC` error per failing curve, and return WITHOUT constructing the controller. `OnPostTickFrameStatePublished` will never fire in this state; Wave Spawner's bind path detects this implicitly (no callback ever invoked) and falls into its no-DPC fallback. (Structural Decision 2.)

2. **`UDPCSubsystem::Initialize()` MUST call `Collection.InitializeDependency(URunStateMachineSubsystem::StaticClass())` BEFORE constructing the controller.** The controller's first `Tick()` will call `RSM->ForceTickNow()`; if RSM hasn't completed Initialize by then, the call fails. (Mirror of ADR-0007 Implementation Guideline 3 applied to DPC.)

3. **`UWaveSpawnerSubsystem::Initialize()` MUST call `Collection.InitializeDependency(UDPCSubsystem::StaticClass())` BEFORE binding `OnPostTickFrameStatePublished`.** Same `InitializeDependency` mechanism as Guidelines 2; cascades RSM → DPC → Wave Spawner initialization order. ADR-0005 MUST be amended to register this Implementation Guideline — **CLOSED by ADR-0005 amendment INT-004 (2026-06-26)**: ADR-0005 Implementation Guideline 8 + Initialize() skeleton now inscribe both `Collection.InitializeDependency(URunStateMachineSubsystem)` AND `Collection.InitializeDependency(UDPCSubsystem)` calls immediately after `Super::Initialize(Collection)`, sibling to the ADR-0007 RSM-dependency addendum (closed in the same amendment pass).

4. **`UDPCController::Tick()` first statement is `SCOPE_CYCLE_COUNTER(STAT_DPCTick)`** (DPC GDD Rule 10 + AC-22b grep gate). Second is `check(IsInGameThread())`. Third is `RSM->ForceTickNow()`. The watchdog accumulator advance follows; then the active-gate evaluation per Rule 14; then the curve-eval pipeline per Rule 8 + 14 input-side guards; then the snapshot write per Rule 10; finally `OuterSubsystem->BroadcastFrameStatePublished(CurrentFrameState)`.

5. **NO subscriber callbacks fire from inside `RSM->ForceTickNow()`** (ADR-0007 non-callback invariant). DPC's reads after `ForceTickNow()` returns use direct accessors. State transitions visible to DPC immediately; callback subscribers (Audio Controller, Player Movement) observe transitions on RSM's next regular tick (1-frame latency, documented in ADR-0007).

6. **Watchdog accumulator MUST use `FMath::Min(DeltaTime, MAX_PER_TICK_DELTA_S)` clamp** (Structural Decision 3). A debugger-resume single huge frame must NOT skip the 5.0s timeout.

7. **`UDPCController` does NOT own the multicast delegate.** The delegate lives on `UDPCSubsystem`. The controller calls `OuterSubsystem->BroadcastFrameStatePublished(CurrentFrameState)` at the end of `Tick()`. This places the discovery surface (`GetSubsystem<UDPCSubsystem>()->OnPostTickFrameStatePublished`) on the canonical Subsystem path that Wave Spawner expects.

8. **`EDPCAbortReason` lives in `Public/DPC/DPCAbortReason.h`** (Structural Decision 4). RSM's public header forward-declares the enum (`enum class EDPCAbortReason : uint8;`) only. The DPC module is added as a public dependency to consumers that need the full definition (Wave Spawner does NOT; only DPC + future telemetry). **RSM's `RequestAbort(EDPCAbortReason)` MUST NOT carry a `UFUNCTION()` macro** while `EDPCAbortReason` is only forward-declared in RSM's header — UHT requires the full type definition for any `UFUNCTION()`-marked signature. Keep `RequestAbort` as a plain C++ method on `URunStateMachineSubsystem`. If a future need exposes `RequestAbort` to Blueprint (designer-side debug tooling, telemetry harness), at that point add DPC as a public RSM module dependency AND mark the function `UFUNCTION(BlueprintCallable)` together — never separately.

9. **Curve assets are held BOTH on the subsystem (for Initialize-time validation lifetime) AND on the controller (for Tick-time evaluation).** Both fields are `UPROPERTY() TObjectPtr<UCurveFloat>` — UE's reference counting allows multiple anchors; the asset is only released when both UPROPERTY anchors drop. This is intentional duplication, not a leak.

10. **`bInitialized` gating happens INSIDE the controller's tick body, not via `IsTickable()`.** DPC GDD R7 BLOCKING fix mandates `IsTickable() = true` unconditionally so the engine ticks DPC during the async-load window. The active-publication gate is the `if (RSM state != RUNNING || RSM paused)` branch inside `Tick()` body — see GDD Rule 14.

## Alternatives Considered

### Alternative 1: `URunStateMachineSubsystem` directly owns DPC

- **Description**: RSM subsystem creates `UDPCController` in its Initialize() and holds via UPROPERTY. No DPC-side subsystem layer.
- **Pros**: Tight RSM-DPC coupling mirrors DPC GDD Rule 3's `ForceTickNow` contract; no new subsystem class; one fewer indirection.
- **Cons**: Violates separation of concerns — RSM is the run state authority, NOT the difficulty authority; conflating ownership couples the two systems' lifecycles in ways that complicate testing (a unit test of DPC must bring up RSM first). Wave Spawner's bind path becomes `GetGameInstance()->GetSubsystem<URunStateMachineSubsystem>()->GetDPCController()->OnPostTickFrameStatePublished.AddUObject(...)` — three indirections + an accessor crossing a domain boundary. Adding a new RSM-coupled system in future requires the same pattern, leading to a god-subsystem antipattern.
- **Estimated Effort**: ~equivalent (different placement; similar amount of code).
- **Rejection Reason**: Domain coupling between Foundation (RSM) and Core (DPC) layers without architectural justification; god-subsystem antipattern risk.

### Alternative 2: `UGameInstance` directly owns `UDPCController` (no subsystem layer)

- **Description**: Subclass `UGameInstance` (e.g., `USlipstormGameInstance`) and hold `UPROPERTY() TObjectPtr<UDPCController>` directly. DPC discoverable via `Cast<USlipstormGameInstance>(GetGameInstance())->GetDPCController()`.
- **Pros**: No subsystem layer; canonical UE "runtime context" placement.
- **Cons**: Locks the project into a custom `UGameInstance` subclass for DPC ownership (other ADRs may also want to subclass; the subclass becomes a god-object). Wave Spawner's bind path requires a `Cast<>` (runtime type check) at every access. No subsystem-collection discovery means `InitializeDependency` ordering is unavailable — Wave Spawner cannot pin DPC initialization order via the subsystem mechanism; ordering must be manually enforced.
- **Estimated Effort**: ~equivalent.
- **Rejection Reason**: Locks project into custom UGameInstance subclass; no `InitializeDependency` ordering mechanism; canonical subsystem discovery pattern (used by RSM + Wave Spawner) is the precedent.

### Alternative 3: `UDPCController` IS the subsystem (skip the wrapper)

- **Description**: Make `UDPCController : public UGameInstanceSubsystem, public FTickableGameObject` directly — no wrapper. The controller declares the delegate, owns the curves, and runs the tick body.
- **Pros**: One fewer class; mirror of ADR-0005's Wave Spawner pattern (which is `UGameInstanceSubsystem + FTickableGameObject` directly).
- **Cons**: Contradicts DPC GDD R6 Rule 3's locked type (`class UDPCController : public UObject, public FTickableGameObject`). Adopting this alternative requires a GDD revision to Rule 3. The GDD's Rule 3 type choice was made under DPC R6 (2026-06-05) with unreal-specialist B-1 input; superseding it at the ADR layer is structurally allowed but undermines the GDD-vs-ADR layering (GDD decides "what the system does at the design level"; ADR decides "how the system is hosted").
- **Estimated Effort**: Slightly lower (one fewer class).
- **Rejection Reason**: Contradicts GDD Rule 3 explicit type lock; would require an explicit GDD revision via `/propagate-design-change`. The thin-wrapper pattern preserves the GDD authority while still providing canonical subsystem discovery.

### Alternative 4: Curve validation deferred to first Tick (Structural Decision 2 alternative)

- **Description**: Skip curve validation at `Initialize()`. Validate inside the first `Tick()` after RSM enters RUNNING.
- **Pros**: Avoids loading curves at startup if game ends in main menu (negligible savings — curves are tiny).
- **Cons**: Validation failure happens MID-GAMEPLAY — a debug build halts the game at a tick that should have been the first wave of OPENER. Player experience is worse for QA. Validation-failure abort path is more intrusive (RSM->RequestAbort with a partially-initialized run).
- **Rejection Reason**: Worse failure mode for marginal startup savings.

### Alternative 5: Curve validation lazy at first `GetFloatValue()` (Structural Decision 2 alternative)

- **Description**: Validate inside the F-1 evaluation hot path on first hit per curve.
- **Pros**: Zero startup cost.
- **Cons**: Adds unnecessary branch to hot path (~3 curve evaluations × 60 fps × first-frame check = 180 branches saved by early validation); introduces validation-timing variability across runs (first-encounter latency differs from steady-state).
- **Rejection Reason**: Hot-path noise; failure mode worse than Initialize-time validation.

### Alternative 6: Watchdog via `FCoreDelegates::OnEndFrame` counter (Structural Decision 3 alternative)

- **Description**: Increment a frame counter per OnEndFrame; convert to elapsed via assumed frame rate.
- **Pros**: Independent of DeltaTime; deterministic frame count.
- **Cons**: Assumes a frame rate (problematic on Android API 26 minimum-spec where 30fps soft caps may apply); couples to OnEndFrame which RSM already uses for `bHasTickedThisFrame` reset (two consumers, harder to reason about); less aligned with EC-13's wall-clock framing.
- **Rejection Reason**: Frame-rate-dependent semantic in a wall-clock domain; second consumer on OnEndFrame.

### Alternative 7: Watchdog via `RunTimeSource->GetCurrentTime()` pull (Structural Decision 3 alternative)

- **Description**: DPC calls `RSM->GetTimeSource()->GetCurrentTime()` at tick start and computes elapsed.
- **Pros**: Sleep-aware monotonic clock (same source as RSM F-1); no FApp/PlatformTime drift.
- **Cons**: Requires RSM to expose `TimeSource` accessor (currently private per ADR-0007 Key Interfaces); architectural over-reach for a 5s watchdog where ms precision is unnecessary; deepens the RSM-DPC coupling DPC GDD Rule 3 already established via ForceTickNow.
- **Rejection Reason**: Unnecessary precision for the use case; opens a private RSM surface for marginal benefit.

## Consequences

### Positive

- DPC discoverable via canonical `GetSubsystem<UDPCSubsystem>()` path — matches RSM + Wave Spawner pattern; engine-specialist readers will recognize the idiom.
- DPC GDD Rule 3 type (`UObject + FTickableGameObject`) is preserved verbatim; no GDD revision required.
- Curve validation at Initialize time fails fast (game-instance startup), surfacing malformed assets to QA without mid-gameplay disruption.
- Watchdog accumulator clamp matches DPC GDD EC-13 exactly; single huge frame cannot skip the 5.0s timeout.
- `EDPCAbortReason` shared-header location keeps RSM include graph clean (forward decl only); supports future telemetry consumers without RSM-DPC circular dependency.
- `Collection.InitializeDependency` chain (RSM → DPC → Wave Spawner) provides structural initialization ordering without ad-hoc workarounds.
- Subsystem-on-controller broadcast back-pointer pattern (TWeakObjectPtr<UDPCSubsystem> on UDPCController) handles teardown safely: if subsystem dies first, `OuterSubsystem.Get()` returns nullptr and the broadcast no-ops.

### Negative

- Two-class structure (subsystem wrapper + controller) is slightly heavier than ADR-0005's single-class pattern. The split is justified by the GDD Rule 3 type lock but adds one indirection.
- `UDPCSubsystem::BroadcastFrameStatePublished` is a `friend` call from `UDPCController` — slight encapsulation looseness. Alternative is a public method, but `friend` more accurately documents that the call is internal-only.
- Curve UPROPERTY held in both subsystem and controller is intentional duplication; a reader might think one of them is redundant. Inline comment + this ADR's Implementation Guideline 9 document the rationale.
- The `bInitializationFailed` silent-fallback path means a malformed-curve subsystem state is hard to surface to QA at runtime (Shipping log strings are suppressed). Mitigation: per-curve UE_LOG(Error) in debug + a runtime status accessor (`bool IsHealthy() const`) on the subsystem for diagnostic UI consumption.
- `ETickableTickType::Conditional` + `IsTickable() == true` unconditionally (DPC GDD R7 BLOCKING-fix-locked) incurs a virtual call per frame that gates nothing — `ETickableTickType::Always` would be the idiomatic choice for constant-true tickability. Because the type-and-tickable pair are GDD-locked, this is not actionable at the ADR layer; flagged for a future GDD tuning pass to evaluate switching to `::Always` (negligible per-frame cost difference; primarily a code-clarity concern).

### Neutral

- `STATGROUP_DPC` adds a new Unreal Insights profiling surface (sibling to `STATGROUP_WaveSpawner` and `STATGROUP_RunStateMachine`).
- DPC's tick-time is small (placeholder < 0.20 ms p99); does not move the frame-budget needle relative to ADR-0005's Wave Spawner allocation (0.30 ms p99) or ADR-0006's Pull-Wave allocation.

## Risks

| Risk | Probability | Impact | Mitigation |
|------|------------|--------|-----------|
| `Collection.InitializeDependency(URunStateMachineSubsystem::StaticClass())` only guarantees construction order, not Initialize-completion order, in UE 5.7 | Low | High | Source-verify before shipping (same verification flagged in ADR-0007 Risk 1; both ADRs share this concern). Fallback: RSM exposes `OnInitialized` one-shot delegate; DPC subsystem defers controller construction until that fires. |
| `UCurveFloat::FloatCurve.Keys.Num() < 2` is a valid asset state during cook-time (designer authored a 1-key curve) | Low | Medium | Curve-asset cooking pipeline should reject 1-key curves at cook time (out-of-scope for this ADR). Runtime gate documented at Initialize-time validation per Structural Decision 2; `bInitializationFailed` set, DPC silently no-ops, Wave Spawner falls into its no-DPC fallback (no wave spawning). |
| `UDPCController` ticks twice per frame — once from FTickableGameObject self-registration, once from subsystem-driven Tick (the wrapper might also implement FTickableGameObject and forget to disable Tick) | Low | High | Engine Compatibility Verification 2 explicitly checks this. `UDPCSubsystem` does NOT inherit FTickableGameObject; only `UDPCController` does. Unit test asserts tick count per frame. |
| Subsystem teardown before controller's last Tick (Deinitialize releases the controller UPROPERTY mid-frame) | Very Low | Low | `TWeakObjectPtr<UDPCSubsystem> OuterSubsystem` on the controller returns nullptr after the subsystem dies; `BroadcastFrameStatePublished` no-ops in that case. Final controller Tick simply doesn't broadcast. |
| `EDPCAbortReason` forward-decl in RSM's public header breaks if RSM module's Build.cs lists DPC as a public dependency (circular include) | Low | Medium | RSM Build.cs MUST NOT list DPC. Forward decl + uint8 storage in RSM.cpp is the canonical decoupling. Code review gate. |
| `bInitializationFailed` silent-fallback is invisible to Shipping QA — no on-screen indicator | Medium | Low | Optional debug-build UE_LOG + a runtime accessor (`UDPCSubsystem::IsHealthy()`) for HUD-overlay debugging. Out of scope for this ADR's required surface; suggest including in DPC's first Beta-gate playtest checklist. |
| Wave Spawner binds to `OnPostTickFrameStatePublished` before DPC's first broadcast (subsystem InitializeDependency rule above mitigates initialization-order; this risk is about FIRST broadcast vs FIRST bind) | Very Low | Low | DPC's `Initialize()` constructs the controller AFTER InitializeDependency confirms RSM is up. Controller's first Tick fires on next game frame. Wave Spawner's Initialize binds the delegate before the controller's first Tick fires (subsystem inits run before any FTickableGameObject Tick). No regression. |

## Performance Implications

| Metric | Expected | Budget |
|--------|----------|--------|
| Per-tick DPC `Tick()` CPU (RUNNING active state) | RSM->ForceTickNow() (no-op when already ticked: ~0.001 ms) + watchdog accumulator update + 3 UCurveFloat::GetFloatValue calls + 6 finite checks + 3 clamps + 1 FDPCFrameState write + 1 multicast broadcast (~5 subscribers, FDPCFrameState ~24 bytes pass-by-const-ref) | < 0.20 ms p99 mid-tier mobile; placeholder per DPC GDD AC-22b Alpha-gate measurement |
| Per-tick DPC `Tick()` CPU (inactive state) | RSM->ForceTickNow() + watchdog accumulator update + inactive snapshot publication + multicast broadcast | < 0.05 ms p99 |
| Curve validation at Initialize | 3 curves × (null check + Keys.Num() compare + 2 float reads) | < 0.5 ms one-shot (negligible) |
| `OnPostTickFrameStatePublished` broadcast | Multicast invocation × ~5 subscribers; `FDPCFrameState` ~24 bytes pass-by-const-ref | < 0.01 ms |
| Watchdog accumulator | One FMath::Min + one += + one compare per tick | < 0.001 ms |

## Migration Plan

Greenfield system — no existing DPC implementation to migrate.

1. Author `Public/DPC/DPCAbortReason.h` with the `EDPCAbortReason` enum. Verify RSM public header's forward declaration matches the underlying-type (`: uint8`). (Structural Decision 4.)
2. Author `Public/DPC/DPCFrameState.h` with `FDPCFrameState` USTRUCT per DPC GDD Rule 10. Author `Public/DPC/DPCStats.h` with `DECLARE_STATS_GROUP(TEXT("DPC"), STATGROUP_DPC, STATCAT_Advanced)` + `DECLARE_CYCLE_STAT(TEXT("DPC Tick"), STAT_DPCTick, STATGROUP_DPC)` declarations (header) + matching `DEFINE_STAT(STAT_DPCTick)` in `DPCStats.cpp`. Author `Public/DPC/DPCLog.h` with `DECLARE_LOG_CATEGORY_EXTERN(LogDPC, Log, All)` + matching `DEFINE_LOG_CATEGORY(LogDPC)` in `DPCLog.cpp`. Note: `GetStatId()` uses `RETURN_QUICK_DECLARE_CYCLE_STAT(UDPCController, STATGROUP_DPC)` — this is a separate stat from `STAT_DPCTick`; both are valid. `SCOPE_CYCLE_COUNTER(STAT_DPCTick)` is the per-tick body scope; `GetStatId()` is the FTickableGameObject's stat-group registration.
3. Author `UDPCSubsystem` with the class signature in Key Interfaces. Implement `Initialize()` with InitializeDependency call + curve validation + controller construction. Implement `Deinitialize()` with controller release.
4. Author `UDPCController` with the class signature in Key Interfaces. Implement `Tick()` body per DPC GDD Rules 3, 5, 6, 8, 10, 14 + this ADR's Structural Decision 3 watchdog accumulator + Structural Decision 5 reaffirmation.
5. Wire `UCurveFloat` asset references — option (a) DataAsset with TObjectPtr fields, option (b) Config-driven soft-references resolved at Initialize. Deferred to implementation; either is consistent with this ADR's curve-validation requirement.
6. Register Forbidden Patterns from this ADR (see § below).
7. Amend ADR-0005 to add the Implementation Guideline addendum: `UWaveSpawnerSubsystem::Initialize()` must call `Collection.InitializeDependency(UDPCSubsystem::StaticClass())` BEFORE binding `OnPostTickFrameStatePublished`. (Sibling addendum to the ADR-0005 RSM-dependency addendum already queued by ADR-0007.)
8. Implement DPC unit tests using Seam 7 `IRSMTimeStateProvider` fake (`FRSMTestStub`) + Seam 8 `IDPCSnapshotConsumer` test stub + Seam 9 `IDPCAbortDelegate` test stub per `platform-seam-interfaces.md`.

**Rollback plan**: If the two-class split proves too heavy in practice (no obvious mitigation surface; speculative), the path forward is to revisit Alternative 3 (`UDPCController` IS the subsystem) which requires a DPC GDD Rule 3 revision via `/propagate-design-change`. The thin-wrapper choice is reversible at the ADR layer.

## Validation Criteria

- [ ] DPC GDD AC-01 through AC-29 (DPC GDD lines 773–1003) — all observable via DPC's public surface.
- [ ] AC-12 phase boundary detection: `current_phase` snaps to new value at the first tick where `t_norm` crosses the boundary.
- [ ] AC-13 inactive snapshot: non-RUNNING / paused state publishes `is_active = false` snapshot.
- [ ] AC-22b code-review gate: `SCOPE_CYCLE_COUNTER(STAT_DPCTick)` is FIRST STATEMENT of `Tick()` body (grep gate).
- [ ] AC-NAN-GUARD: malformed curve returning NaN does NOT propagate to `FDPCFrameState`; inactive snapshot is published with diagnostic log.
- [ ] Curve validation at Initialize: 3 curve assets with non-spanning keys are rejected; `bInitializationFailed = true`; no controller constructed; subsequent `OnPostTickFrameStatePublished` never fires.
- [ ] Watchdog accumulator: 5.0s of accumulated DeltaTime (with per-tick clamp at 0.1s) fires `RSM->RequestAbort(EDPCAbortReason::AsyncLoadTimeout)` exactly once.
- [ ] Watchdog clamp: single 10.0s DeltaTime (debugger resume simulation) advances watchdog by 0.1s, NOT 10.0s.
- [ ] InitializeDependency: integration test asserts RSM `Initialize()` completes before DPC `Initialize()` runs; DPC `Initialize()` completes before Wave Spawner `Initialize()` runs.
- [ ] DPC Tick fires exactly once per game frame (verification: tick counter incremented in `Tick()`; sampled at OnEndFrame; asserted to equal expected count).
- [ ] No `FApp::GetCurrentTime()`, `FPlatformTime::Seconds()`, or `FTimerManager` calls in DPC source (grep gate; mirror of ADR-0007).
- [ ] `OnPostTickFrameStatePublished` fires exactly once per `Tick()` invocation (active OR inactive snapshots both fire).
- [ ] Wave Spawner integration: `UWaveSpawnerSubsystem::Initialize()` successfully binds to `UDPCSubsystem::OnPostTickFrameStatePublished` without observable race.
- [ ] Per-tick subsystem CPU ≤ 0.20 ms p99 on iPhone XR + Pixel 5 / Galaxy A52 under Unreal Insights (Alpha gate per DPC GDD AC-22b).

## Forbidden Patterns

Register the following at `docs/registry/architecture.yaml` Forbidden Patterns registry on write approval (skill Phase 6).

| Pattern | Reason |
|---------|--------|
| `DPC_Controller_as_Subsystem_directly` | DPC GDD R6 Rule 3 locks `UDPCController : public UObject, public FTickableGameObject` (not a subsystem). Adopting subsystem-only DPC requires a GDD revision via /propagate-design-change. Alternative 3 in this ADR; rejected. |
| `DPC_Subsystem_inherits_FTickableGameObject` | The subsystem is a thin wrapper; tick ownership belongs to the controller. If the subsystem also implements `FTickableGameObject`, DPC ticks twice per frame. Engine Compatibility Verification 2 explicit check. |
| `DPC_Tick_admission_logic_outside_Tick_body` | All admission/active-publication logic lives inside `UDPCController::Tick()` body. Future engineers must not move active-state logic into Initialize-driven event handlers or asynchronous tasks. DPC GDD Rule 3 + Rule 14. |
| `DPC_FApp_GetCurrentTime_usage` | DPC's watchdog accumulator uses `Tick()` DeltaTime parameter (Structural Decision 3). `FApp::GetCurrentTime()` is frame-cached and does not advance during lifecycle callbacks — same prohibition as RSM (mirror of ADR-0007 Forbidden Pattern). |
| `DPC_FTimerManager_usage` | Same prohibition as RSM. DPC's watchdog timeout is computed inline in Tick; no FTimerManager. (Mirror of ADR-0007 Forbidden Pattern applied to DPC.) |
| `DPC_curve_evaluation_without_finite_guards` | DPC GDD Rule 8 mandates output-side finite guards on `UCurveFloat::GetFloatValue()` returns; failure to apply produces UB when `FMath::FloorToInt32(NaN)` runs (implementation-defined on ARM, commonly produces `INT_MIN`). |
| `DPC_t_norm_input_without_clamp` | DPC GDD Rule 14 mandates `t_norm` is clamped to `[0.0, 1.0]` at the input side via `FMath::Clamp`. Without this, downstream `UCurveFloat::GetFloatValue(t_norm)` is undefined for inputs outside the curve key range. |
| `DPC_BroadcastFrameStatePublished_from_outside_controller` | Only `UDPCController::Tick()` may call `OuterSubsystem->BroadcastFrameStatePublished()`. The `friend` declaration enforces this at compile time. Code review gate against external callers via reflection or workaround. |
| `EDPCAbortReason_definition_in_RSM_module` | DPC GDD R6 forward contract item 4 ("owned by DPC"). RSM forward-declares; full definition lives in DPC module. Avoids ownership inversion. |
| `DPC_curve_validation_at_first_tick_or_lazy` | Curve validation runs at `Initialize()` per Structural Decision 2. Deferral to first Tick OR lazy at first `GetFloatValue()` rejected (Alternatives 4 + 5). |
| `DPC_watchdog_accumulator_unclamped` | Watchdog DeltaTime must be clamped via `FMath::Min(DeltaTime, MAX_PER_TICK_DELTA_S=0.1f)`. Without the clamp, a debugger-resume single huge frame skips the 5.0s timeout entirely. |

## GDD Requirements Addressed

Maps to all 24 DPC technical requirements registered in `docs/architecture/tr-registry.yaml` (TR-DPC-001 through TR-DPC-024).

| GDD Document | TR-ID | Requirement | How This ADR Addresses It |
|--------------|-------|-------------|---------------------------|
| `design/gdd/difficulty-phase-controller.md` | TR-DPC-001 | DPC is sole authority for run-phase and difficulty-parameter values during RUNNING | `UDPCController` is the sole producer of `FDPCFrameState`; subsystem is a thin wrapper. No other system computes phase or difficulty scalars. |
| `design/gdd/difficulty-phase-controller.md` | TR-DPC-002 | DPC reads `remaining_time` from RSM each tick; does not subscribe to state callbacks | Implementation Guideline 4: `Tick()` body calls `RSM->GetRemainingTime()` after `RSM->ForceTickNow()`. No `OnStateChanged` / `OnPausedChanged` subscription (Registry: `rsm_state_changed` consumer list explicitly EXCLUDES difficulty-phase-controller per ADR-0007 amendment). |
| `design/gdd/difficulty-phase-controller.md` | TR-DPC-003 | DPC executes tick after RSM updates `remaining_time` via ForceTickNow semantics | Implementation Guideline 4: `Tick()` body's third statement (after SCOPE_CYCLE_COUNTER + thread check) is `RSM->ForceTickNow()`. Idempotent within frame per ADR-0007 Structural Decision 2. |
| `design/gdd/difficulty-phase-controller.md` | TR-DPC-004 | Publish inactive snapshot when RSM is not RUNNING or `is_paused == true` | `Tick()` body has explicit branch: `if (RSM state != RUNNING || RSM paused) { PublishInactiveSnapshot(); return; }`. Inactive snapshot has `is_active = false` per FDPCFrameState defaults. |
| `design/gdd/difficulty-phase-controller.md` | TR-DPC-005 | Compute normalized progress `t_norm` using `(RUN_DURATION_S - remaining_time) / RUN_DURATION_S` | F-1 implementation lives in `UDPCController::Tick()` body; reads `RSM->GetRunDurationS()` (RSM accessor existence required — DPC GDD R6 implicit) and `RSM->GetRemainingTime()`. |
| `design/gdd/difficulty-phase-controller.md` | TR-DPC-006 | Map `t_norm` to discrete `ERunPhase` enum via step function | DPC GDD Rule 6 step function implemented in `Tick()` body; `current_phase` writes to `FDPCFrameState.current_phase`. |
| `design/gdd/difficulty-phase-controller.md` | TR-DPC-007 | Phase transitions fire at first tick where `t_norm` crosses normalized boundary | Step function evaluated each tick; `CurrentFrameState.current_phase` overwrites only on boundary cross. Previous-frame snapshot (`PreviousFrameState`) allows downstream diff. |
| `design/gdd/difficulty-phase-controller.md` | TR-DPC-008 | Evaluate three `UCurveFloat` scalars at `t_norm` with output-side finite guards | DPC GDD Rule 8 code block implemented; Forbidden Pattern `DPC_curve_evaluation_without_finite_guards` is the code-review gate. |
| `design/gdd/difficulty-phase-controller.md` | TR-DPC-009 | Clamp `telegraph_window_s` to `TELEGRAPH_WINDOW_FLOOR_S = 0.70s` | `FMath::Max(TELEGRAPH_WINDOW_FLOOR_S, tw_raw)` per GDD Rule 8 code block. Value `0.70s` is the R10d-bound floor. |
| `design/gdd/difficulty-phase-controller.md` | TR-DPC-010 | Clamp `wave_spawn_interval_s` to `WAVE_SPAWN_INTERVAL_FLOOR_S = 0.25s` | `FMath::Max(WAVE_SPAWN_INTERVAL_FLOOR_S, wsi_raw)` per GDD Rule 8 code block. |
| `design/gdd/difficulty-phase-controller.md` | TR-DPC-011 | Clamp `max_concurrent_waves` to `[1, 16]` via float-then-int conversion | `FMath::Clamp(mcw_raw, MAX_CONCURRENT_WAVES_FLOOR, MAX_CONCURRENT_WAVES_CAP)` BEFORE `FMath::FloorToInt32` per GDD Rule 8 Cluster B Item 2 fix. |
| `design/gdd/difficulty-phase-controller.md` | TR-DPC-012 | Publish atomic `FDPCFrameState` struct each tick with `is_active` flag | `UDPCController::Tick()` writes `CurrentFrameState` fields after computing all values; `BroadcastFrameStatePublished` fires once per tick. |
| `design/gdd/difficulty-phase-controller.md` | TR-DPC-013 | Broadcast `OnPostTickFrameStatePublished` multicast delegate after snapshot write | Implementation Guideline 7: `OuterSubsystem->BroadcastFrameStatePublished(CurrentFrameState)` at end of `Tick()`. Subsystem's `BroadcastFrameStatePublished` fires the delegate. |
| `design/gdd/difficulty-phase-controller.md` | TR-DPC-014 | `FDPCFrameState` contains phase, three scalars, `t_norm` diagnostic, and `is_active` bool | FDPCFrameState struct in Key Interfaces reproduces GDD Rule 10 verbatim. |
| `design/gdd/difficulty-phase-controller.md` | TR-DPC-015 | Guard against NaN/infinity in `remaining_time` at tick top before F-1 evaluation | DPC GDD Rule 14 input-side guard implemented in `Tick()` body before F-1 evaluation; non-finite `remaining_time` → publish inactive snapshot. |
| `design/gdd/difficulty-phase-controller.md` | TR-DPC-016 | Validate curve assets have keys spanning `[0.0, 1.0]` at Initialize | Structural Decision 2: `UDPCSubsystem::Initialize()` runs the `ValidateCurveAsset` gate. Failure → `bInitializationFailed = true`; no controller construction. |
| `design/gdd/difficulty-phase-controller.md` | TR-DPC-017 | Assert threshold ordering: `OPENER_END > 0.0` and `PEAK_START < 1.0` | Validation runs in `UDPCSubsystem::Initialize()` alongside curve validation; same fail-fast pattern. |
| `design/gdd/difficulty-phase-controller.md` | TR-DPC-018 | Assert MID width ≥ 0.10 normalized to prevent binary phase oscillation | Validation runs in `UDPCSubsystem::Initialize()` alongside threshold-ordering check. |
| `design/gdd/difficulty-phase-controller.md` | TR-DPC-019 | Check that PEAK `max_concurrent_waves >= MAX_PULLS_PER_BARRAGE (3)` when pool has barrages | Validation runs in `UDPCSubsystem::Initialize()` against the MaxConcurrentWavesCurve evaluated at `t_norm = PEAK_START`. |
| `design/gdd/difficulty-phase-controller.md` | TR-DPC-020 | Tick-driven watchdog on async-load accumulates at 5.0s timeout; clamp per-tick delta | Structural Decision 3: `watchdog_elapsed_s += FMath::Min(DeltaTime, 0.1f)` inside `Tick()` body; timeout fires `RSM->RequestAbort(EDPCAbortReason::AsyncLoadTimeout)`. |
| `design/gdd/difficulty-phase-controller.md` | TR-DPC-021 | Consumers must gate phase-diff on `is_active` flag per Rule 16 | `FDPCFrameState.is_active` field per Key Interfaces; consumer-side gate documented in DPC GDD Rule 16 (out of this ADR's scope but tracked here for traceability). |
| `design/gdd/difficulty-phase-controller.md` | TR-DPC-022 | Consumers must handle `is_active` rising-edge separately from phase-diff per Rule 16 | Same as TR-DPC-021 — consumer-side; documented for traceability. |
| `design/gdd/difficulty-phase-controller.md` | TR-DPC-023 | TelegraphWindowCurve Path B reauthored to keys `(0.0, 0.94), (0.333, 0.82), (0.75, 0.70)` | TelegraphWindowCurve asset content is OUT OF SCOPE for this ADR (asset-content decision); ADR specifies validation policy (Structural Decision 2) that asserts the curve keys are present and span `[0.0, 1.0]`. R10d-bound keys are an asset-authoring concern enforced at content cook time. |
| `design/gdd/difficulty-phase-controller.md` | TR-DPC-024 | Support `RSM.ForceTickNow()` idempotent entry point before reading `remaining_time` | Implementation Guideline 4: `Tick()` body's third statement (after SCOPE_CYCLE_COUNTER + thread check) is `RSM->ForceTickNow()`. ADR-0007 declares the RSM-side `ForceTickNow` public method. |

## Related

- **ADR-0005**: Wave Spawner `UGameInstanceSubsystem` hosting — cites this ADR's `OnPostTickFrameStatePublished` as forward contract (ADR-0005 lines 252–258). ADR-0005 SHOULD be amended (Implementation Guideline 3 of this ADR) to add the `Collection.InitializeDependency(UDPCSubsystem::StaticClass())` requirement at the top of `UWaveSpawnerSubsystem::Initialize()`.
- **ADR-0006**: Pull-Wave instanced renderer — independent of this ADR. Pull-Wave consumes `FDPCFrameState.telegraph_window_s` for telegraph rendering but does not directly bind to this ADR's surface.
- **ADR-0007**: RSM Hosting + Sleep-Aware Time Source — direct upstream dependency. This ADR consumes `URunStateMachineSubsystem::ForceTickNow()` + `URunStateMachineSubsystem::RequestAbort(EDPCAbortReason)` + the forward declaration of `EDPCAbortReason` decided there.
- **ADR-0001, 0002, 0003, 0004**: Independent of this ADR.
- **DPC GDD R6 Rule 3 OQ-7 closure**: "the OQ-7 ADR MUST specify DPC's production outer to fully resolve the lifetime contract" — closed by Structural Decision 1.
- **DPC GDD R6 Rule 3 forward contracts 1–5**: All five RSM-side contracts (ForceTickNow, bHasTickedThisFrame, non-callback invariant, RequestAbort, game-thread) are declared in ADR-0007 (amended 2026-06-26). This ADR consumes them as upstream.
- **Wave Spawner GDD R3a FC-3**: `OnPostTickFrameStatePublished` non-dynamic multicast — declared on `UDPCSubsystem` per this ADR's Structural Decision 1.
- **Seam 7** (`platform-seam-interfaces.md` lines 595–875): `IRSMTimeStateProvider` — DPC's test seam for RSM substitution. Production binding is `URunStateMachineSubsystem` itself per ADR-0007.
- **Seam 8** (`platform-seam-interfaces.md` lines 879–1066): `IDPCSnapshotConsumer` — test stub for snapshot consumption verification.
- **Seam 9** (`platform-seam-interfaces.md` lines 1071–1183): `IDPCAbortDelegate` — DPC's abort entry abstraction for tests. Production binding is `URunStateMachineSubsystem::RequestAbort(EDPCAbortReason)` per ADR-0007.

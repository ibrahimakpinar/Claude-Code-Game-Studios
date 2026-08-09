# ADR-0007: Run State Machine Hosting and Sleep-Aware Time Source

## Status

Accepted

## Date

2026-06-25 (Proposed) / 2026-06-26 (Accepted)

## Last Verified

2026-06-25
2026-06-26 (status promotion Proposed → Accepted — all three architecture-review-2026-06-26 surgical fixes closed: INT-004 (cross-ADR; ADR-0005-side InitializeDependency calls inscribed), INT-005 (ADR-0007 Tick() body guard), INT-006 (ADR-0007 Initialize() salt-parse FCString::Strtoui64 switch after UE 5.7 primary-source verification). `docs/architecture/requirements-traceability.md` "Open amendments before ADR-0007 → Accepted" now reads ✅ NONE REMAINING. ADR-0008 (DPC Hosting) `Depends On` gate (ADR-0007 Accepted) now satisfied — ADR-0008 unblocked for its own Proposed → Accepted flip pending no further open findings.)
2026-06-26 (amendment INT-005 — `Tick()` body skeleton gains FIRST-STATEMENT `if (bHasTickedThisFrame) return;` guard mirroring `ForceTickNow()` guard at lines 431–434; closes architecture-review-2026-06-26 BLOCKING Risk 4 mitigation code/prose contradiction; Implementation Guideline 5 + Risk 4 row + Validation Criteria item + diagram + Key Interfaces flag comment rephrased "set as FIRST STATEMENT" → "guarded then set"; ADR-0007 unblocked for Proposed → Accepted gate)
2026-06-26 (amendment INT-006 — `Initialize()` salt-parse switched from `FParse::HexNumber64(*SaltHex, Salt)` bool+out-param call to `FCString::Strtoui64(*SaltHex, nullptr, 16)` value-return call after primary-source verification against `/Users/Shared/Epic Games/UE_5.7/Engine/Source/Runtime/Core/Public/Misc/Parse.h` confirmed UE 5.7 signature is `static CORE_API uint64 HexNumber64(FStringView HexString)` — incompatible with the ADR's bool+out-param usage AND incompatible with the LLM-window 5.3 signature `static uint64 HexNumber64(const TCHAR*, TCHAR**)` (signature evolved 5.3→5.7, demonstrating FParse instability across UE post-cutoff versions). `FCString::Strtoui64` chosen as the cross-version-stable fallback per architecture-review-2026-06-26 INT-006 recommendation (C-stdlib heritage; signature unchanged across UE 5.0–5.7 per source verification at `/Users/Shared/Epic Games/UE_5.7/Engine/Source/Runtime/Core/Public/Misc/CString.h:549`). Initialize() if-chain restructured from 4-condition single-branch to 2-statement load-then-zero-check pattern (assignment-in-condition avoided per UE coding style). Structural Decision 4 prose at line 112 + `docs/registry/architecture.yaml` line 480 api: string updated in parallel. Closes last open surgical fix from architecture-review-2026-06-26; ADR-0007 fully unblocked for Proposed → Accepted gate.

## Decision Makers

- **creative-director (user, 2026-06-25)** — sign-off on 4 design questions surfaced by the architecture-decision skill (tick-ordering mechanism, clock-seam reconciliation, salt storage, Android delegate thread safety)
- **ADR-0005 precedent (2026-06-24)** — established `UGameInstanceSubsystem + FTickableGameObject` as the project's hosting pattern for session-persistent subsystems
- **unreal-specialist** — engine specialist validation (Step 5.5)
- **GDD R6 author (2026-05-06)** — pre-existing GDD design locks at `design/gdd/run-state-machine.md` Rules 13, 15, 16, 18, 21; Clock Injection section; Open Questions 2, 6, 7

## Summary

SLIPSTORM's Run State Machine is hosted as `URunStateMachineSubsystem : public UGameInstanceSubsystem, public FTickableGameObject` — mirroring ADR-0005's Wave Spawner pattern. The subsystem survives world reloads, owns the canonical run timer + state enum + `RunSeed`, and exposes `ForceTickNow()` as the public pull primitive that DPC calls first in its tick body to guarantee `remaining_time` is current — uniform across UE scheduler ordering, idempotent within frame via `bHasTickedThisFrame` (DPC GDD R6 Rule 3 binding). A second public entry, `RequestAbort(EDPCAbortReason)`, lets DPC's async-load watchdog drive RSM into ABORTED. Sleep-aware wall-clock access is injected through a `RunTimeSource` interface whose production implementation (`FAppTimeSource`) delegates to the same `mach_continuous_time()` (iOS) and `clock_gettime(CLOCK_BOOTTIME, ...)` (Android) primitives that already back IS Seam 1's `IMonotonicClock`.

## Engine Compatibility

| Field | Value |
|-------|-------|
| **Engine** | Unreal Engine 5.7 |
| **Domain** | Core |
| **Knowledge Risk** | HIGH — UE 5.4–5.7 are post-LLM-cutoff (May 2025); subsystem-collection dependency ordering and lifecycle-delegate thread dispatch must be verified against UE 5.7 source |
| **References Consulted** | `docs/engine-reference/unreal/VERSION.md`, `docs/engine-reference/unreal/breaking-changes.md`, `docs/engine-reference/unreal/deprecated-apis.md`, `docs/engine-reference/unreal/current-best-practices.md`, `docs/architecture/platform-seam-interfaces.md` (Seam 1) |
| **Post-Cutoff APIs Used** | `FCoreDelegates::ApplicationWillEnterBackgroundDelegate` / `ApplicationHasEnteredForegroundDelegate` (mobile lifecycle hooks; pair specified by GDD Rule 13), `FSubsystemCollectionBase::InitializeDependency()` (cross-subsystem ordering pin used by Wave Spawner + DPC to ensure RSM `Initialize()` completes first), `UGameInstanceSubsystem::Initialize()` / `Deinitialize()` (lifecycle), `FTickableGameObject::GetTickableTickType()` with `ETickableTickType::Conditional` (idle gating), `AsyncTask(ENamedThreads::GameThread, ...)` (Android background-delegate marshalling), `FPlatformTime::Cycles64()` (RunSeed entropy source; GDD Rule 21), `GConfig` (per-installation salt persistence), `mach_continuous_time()` (iOS sleep-aware clock; GDD EC-9), `clock_gettime(CLOCK_BOOTTIME, ...)` (Android sleep-aware clock; GDD EC-9), `TObjectPtr<T>` (GC-safe pointer modernization, UE 5.0+) |
| **Verification Required** | (1) Confirm `FSubsystemCollectionBase::InitializeDependency(URunStateMachineSubsystem::StaticClass())` invoked at the top of `UWaveSpawnerSubsystem::Initialize()` and `UDPCSubsystem::Initialize()` guarantees RSM's `Initialize()` returns BEFORE control returns to the caller (not merely that RSM is constructed). If only construction-order is guaranteed, fallback path is documented in Risks. (2) Confirm `FCoreDelegates::ApplicationWillEnterBackgroundDelegate` dispatch thread on UE 5.7 Android (some configs dispatch from the Android event thread; the `AsyncTask` wrap in Implementation Guideline 8 makes the ADR Android-thread-safe regardless). (3) Confirm iOS lifecycle delegates dispatch on GameThread in UE 5.7 (`AsyncTask` wrap is a no-op on iOS if true; either way, symmetric wrap is harmless). (4) Confirm `mach_continuous_time()` available on iOS 14 (minimum supported per ADR-0002) — Apple documents availability since iOS 10. (5) Confirm `clock_gettime(CLOCK_BOOTTIME, ...)` available on Android API 26 (minimum supported per ADR-0002) — `CLOCK_BOOTTIME` documented since API 17. (6) Confirm `GConfig` writes to `GGameIni` persist across app restarts on both mobile platforms (UE-standard behavior; verification gate before shipping). |

> **Note**: Knowledge Risk is HIGH. This ADR must be re-validated if the project
> upgrades engine versions. Flag as "Superseded" and author a new ADR on upgrade.

## ADR Dependencies

| Field | Value |
|-------|-------|
| **Depends On** | None — Foundation layer; no upstream ADR |
| **Enables** | ADR-0005 (closes one-sided forward contracts cited at ADR-0005 lines 252–258 — `OnPausedChanged` subscription target + RSM availability at Wave Spawner `Initialize()`); ADR-0008 (forthcoming DPC Hosting — declares DPC's production outer + consumes `ForceTickNow` / `RequestAbort` / `EDPCAbortReason` decided here); all RSM, DPC, Wave Spawner, Player Movement, Pull-Wave, Collision, HUD, End-Run Screen, Audio Controller, Death Replay, Scoring Logic, Score Persistence stories |
| **Blocks** | RSM Epic implementation; DPC Epic implementation (DPC GDD R6 Rule 3 binds 5 forward contracts on RSM decided here); Wave Spawner Epic implementation (binds to `OnPausedChanged` in `Initialize()` per ADR-0005 Implementation Guideline addendum below) |
| **Ordering Note** | Foundation sibling of ADR-0001/0002/0003 (Input System chain) and ADR-0004 (methodology); independent of those. Must be Accepted before ADR-0008 (DPC Hosting) enters drafting since DPC's Rule 3 forward contracts on RSM are codified here. |

## Context

### Problem Statement

The Run State Machine is the lifecycle authority for a SLIPSTORM run (GDD line 10). It owns `ERunState`, `is_paused`, `resume_grace`, `remaining_time`, `run_outcome`, and `RunSeed` — surfaces read by every downstream gameplay system. ADR-0005 (Wave Spawner, Accepted 2026-06-24) declared a forward contract on RSM at two seams: `UWaveSpawnerSubsystem::Initialize()` binds to `RSM->OnPausedChanged` (ADR-0005 lines 254–258 trace through to the DPC seam; Wave Spawner R3a FC-2 binds RSM identically); and Wave Spawner consumes `RunSeed` at its `Cold → Active` transition (GDD Rule 21; Wave Spawner R3a FC-1). Both contracts presume RSM exists, is initialized, and has its delegate types declared — but no ADR records any of those choices.

The architecture-review pass on 2026-06-25 surfaced 34 RSM technical requirements (TR-RSM-001 through TR-RSM-034 in `docs/architecture/tr-registry.yaml`) with zero ADR coverage. The RSM GDD (Approved, revised ×6, 540 lines) is internally consistent but diffuses its key architectural choices across Rule 16 (object-type prerequisite), Rule 18 (`OnPausedChanged` delegate type pin from Wave Spawner R3a closure), Rule 21 (`RunSeed` UPROPERTY pin), the Clock Injection section (sleep-aware platform clocks + no `FApp::GetCurrentTime()`, no `FPlatformTime::Seconds()`, no `FTimerManager`), and three Open Questions (OQ-2: ADR for `ERunState + bool is_paused` architecture; OQ-6: platform clock source implementation prerequisite; OQ-7: RSM object type + delegate binding + Android thread safety + re-entrancy guard).

The cost of deferring this ADR is that ADR-0005's lifecycle assumptions remain GDD-level assertions rather than ADR-validated decisions; the DPC ADR (forthcoming ADR-0008) cannot be drafted without knowing the tick-ordering mechanism RSM publishes; and the first RSM, DPC, or Wave Spawner story cannot enter implementation while their referenced ADR is Proposed (per `docs/CLAUDE.md`).

### Current State

No RSM implementation exists. The RSM GDD R6 (Approved 2026-05-06) is the authoritative design document. IS Seam 1 (`platform-seam-interfaces.md` lines 31–170) provides a directly-analogous sleep-aware monotonic-clock seam (`IMonotonicClock` with `FiOSContinuousTimeClock` + `FAndroidBootTimeClock` + `FFakeMonotonicClock`) for the Input System's millisecond-domain F-3/F-4 timing — production-ready, reviewed, but unit-mismatched against RSM's seconds-domain F-1/F-2/F-3/F-4 timer formulas.

### Constraints

- **Mobile platform, 60 fps, 16.6 ms frame budget** (`.claude/docs/technical-preferences.md`; target 60 fps sustained on mid-tier mobile).
- **Session-state continuity across world reloads** — death-replay cycles (RSM RESOLVING → IDLE → COUNTDOWN → RUNNING → DEAD → RESOLVING → IDLE) must not destroy and reconstruct RSM. `RunSeed` immutability for Death Replay (GDD Rule 21) requires the RSM instance to outlive any single world.
- **Sleep-aware wall clock** — `FPlatformTime::Seconds()` pauses during device sleep on both iOS (`mach_absolute_time` backing) and Android (`CLOCK_MONOTONIC` backing). A 5-minute device sleep would accumulate as ~0s of pause-duration in F-3, silently corrupting the run timer (GDD EC-9; GDD line 64 explicit warning).
- **Frame-cache forbidden** — `FApp::GetCurrentTime()` is set once per GameThread tick and does not advance during lifecycle callbacks. Using it in F-3 would cause the same corruption as the unwrapped `FPlatformTime::Seconds()` (GDD Clock Injection section).
- **`FTimerManager` forbidden** — incompatible with `FakeTimeSource`-based unit tests; advancing the fake clock must trigger all timer-dependent transitions (GDD line 69; AC-15, AC-18, AC-23, AC-27, AC-29).
- **`ApplicationWillEnterBackgroundDelegate` pair only** — `ApplicationWillDeactivateDelegate` fires on transient interruptions (notification banners, Control Center swipes, incoming-call overlays) and must NOT be used for run-pause semantics (GDD Rule 13).
- **Re-entrancy guards must be Shipping-safe** — `check()` and `ensure()` are gated by `DO_CHECK`, which defaults to 0 in Shipping; runtime conditional + `UE_LOG` is the only Shipping-safe guard (GDD Rule 15).
- **`RunSeed` Blueprint exposure** — `UPROPERTY(BlueprintReadOnly, Category="RSM|Run")` requires UObject ancestry (GDD Rule 21).
- **No `AddTickPrerequisite*`** — `FTickableGameObject` lacks the tick-group + prerequisite APIs available to `AActor`/`UActorComponent`; ordering must be structural (GDD Rule 16; ADR-0005 R2a-2 precedent).

### Requirements

- Subsystem survives world reloads; `RunSeed` immutable for the life of one run (GDD Rule 21; AC-WS-13).
- Sleep-aware wall clock on iOS 14+ and Android API 26+; tests use `FakeTimeSource` injected at construction (GDD Clock Injection; AC-01 through AC-30 all use `FakeTimeSource`).
- Structural tick-ordering with DPC and other downstream readers — RSM's `remaining_time` and `current_state` must be current-frame when DPC reads them (DPC GDD Rule 3 + TR-DPC-024 bind ForceTickNow pull semantics on RSM; see Structural Decision 2 below).
- Multicast delegates (non-dynamic) for `OnStateChanged` (GDD Rule 15) and `OnPausedChanged` (GDD Rule 18).
- Wave Spawner can bind to RSM in its own `Initialize()` without ordering races (this ADR Implementation Guideline 3); DPC subscribes to nothing — pulls via `ForceTickNow()` (this ADR Structural Decision 2 + DPC GDD R6 Rule 3 binding).
- External abort entry point for DPC's async-load watchdog (DPC GDD R6 Rule 3 forward contract item 4 + EC-13).
- Per-tick subsystem CPU < 0.05 ms p99 mid-tier mobile (placeholder; Alpha-gate measurement).

## Decision

`URunStateMachineSubsystem` is hosted as `UGameInstanceSubsystem` with `FTickableGameObject` multiple inheritance — identical hosting pattern to ADR-0005's `UWaveSpawnerSubsystem`. The four design questions surfaced by `/architecture-decision` on 2026-06-25 resolved as follows.

### Structural Decision 1 — Class: `UGameInstanceSubsystem + FTickableGameObject`

Both UObject ancestry (required for `UPROPERTY(BlueprintReadOnly) RunSeed` per GDD Rule 21) and game-instance-scoped lifetime (required for state continuity across death-replay world reloads) are satisfied by the same pattern ADR-0005 chose for Wave Spawner. `UGameInstanceSubsystem` does NOT tick by default; tick is provided by additionally inheriting `FTickableGameObject` with `ETickableTickType::Conditional` + `IsTickable()` gating (idle-state cost suppression).

### Structural Decision 2 — Tick ordering via `ForceTickNow()` pull primitive (DPC GDD R6 Rule 3 binding)

DPC's tick body calls `URunStateMachineSubsystem::ForceTickNow()` as the first statement of its own `UDPCController::Tick()` (after `SCOPE_CYCLE_COUNTER` and `check(IsInGameThread())`). This guarantees RSM's `remaining_time` and `current_state` are current before DPC reads them, regardless of UE scheduler ordering. `ForceTickNow()` is idempotent within a single engine frame via an RSM-internal `bHasTickedThisFrame` flag — if RSM's regular `Tick()` already fired this frame, `ForceTickNow()` is a no-op; if it hasn't, `ForceTickNow()` runs RSM's tick body inline with `DeltaTime = FApp::GetDeltaTime()`. The flag resets at frame boundary via a `FCoreDelegates::OnEndFrame` subscription whose handle is stored at `Initialize()` and removed at `Deinitialize()` (lifetime safety against PIE shutdown).

The non-callback invariant is binding: `ForceTickNow()` MUST NOT invoke any subscriber broadcast (`OnStateChanged`, `OnPausedChanged`) during its execution. The primitive only updates RSM's internal state. Callback subscribers observe transitions on RSM's next regular tick — a documented 1-frame latency that callers requiring zero latency (Wave Spawner, Audio Controller) compensate for via direct accessor reads (`GetCurrentState()`, `GetRunOutcome()`) rather than relying on the callback.

This ADR's prior draft framed tick-ordering as a `OnPostTickRunStatePublished` push delegate (cascading-delegate chain RSM → DPC → Wave Spawner). That framing contradicted DPC GDD R6 Rule 3, which is the more recent + unreal-specialist-validated design authority. The push framing has been retired; `OnPostTickRunStatePublished` is NOT declared on `URunStateMachineSubsystem`. Wave Spawner's tick ordering relative to RSM is no longer the RSM's concern — DPC pulls RSM, then publishes its own `OnPostTickFrameStatePublished` for Wave Spawner per ADR-0005 R2a-2 (which is correct as written).

External entry point for DPC's async-load watchdog (DPC GDD R6 Rule 3 forward contract item 4): `URunStateMachineSubsystem::RequestAbort(EDPCAbortReason Reason)` transitions RSM directly from RUNNING (or COUNTDOWN if applicable) to ABORTED → IDLE. The existing GDD ABORTED entry path (PAUSED_TIMEOUT_S exceeded on foreground) is extended with this external-entry path. `EDPCAbortReason` enum is owned by DPC (declared in the DPC module) and forward-declared in RSM's public header.

`OnStateChanged` and `OnPausedChanged` retain their event-driven semantics for downstream consumers (Audio Controller, Player Movement, Death Replay, Wave Spawner pause-flush) that react to discrete transitions. They are independent of the `ForceTickNow` pull mechanism.

### Structural Decision 3 — Clock seam: `RunTimeSource` separate from Seam 1 `IMonotonicClock`, shared platform primitives

`RunTimeSource` (seconds-domain, RSM-owned) and `IMonotonicClock` (milliseconds-domain, IS-owned per Seam 1 lines 51–61) remain as named in their respective GDDs. Production implementations share the same `mach_continuous_time()` and `clock_gettime(CLOCK_BOOTTIME, ...)` calls but expose unit-appropriate APIs. The two interfaces are not unified at the type level (RSM GDD prescribes non-`I`-prefixed `RunTimeSource`; IS Seam 1 uses `IMonotonicClock`); the shared platform-clock primitives live in a new module-internal header `Source/SLIPSTORM/Public/Platform/SleepAwareClock.h` so neither seam duplicates the `#if PLATFORM_IOS` branching.

### Structural Decision 4 — `RunSeed` salt storage: `GConfig` string-encoded hex

Per-installation salt persists to `GGameIni`, section `[/Script/SLIPSTORM.RunStateMachine]`, key `RunSeedSalt`, stored as a 16-character uppercase hex string (e.g. `"A3F2C9D4E1B7506F"`). `FConfigCacheIni` does NOT expose `GetUInt64`/`SetUInt64` — the canonical safe surface is `GetString`/`SetString`. Read once at RSM `Initialize()`; parse via `FCString::Strtoui64(*SaltHex, nullptr, 16)` (value-return uint64; returns 0 on invalid input which the zero-salt rejection branch subsumes). INT-006 2026-06-26: primary-source verification against UE 5.7 `Misc/Parse.h` confirmed `FParse::HexNumber64` has a `FStringView`-taking value-return signature in 5.7 (evolved from `(const TCHAR*, TCHAR**)` in 5.3) — both forms are incompatible with the original bool+out-param call; `FCString::Strtoui64` chosen for cross-version stability. First-launch path: if absent or zero, generate `FGuid::NewGuid()`, XOR-fold its 128 bits into a single `uint64`, format as hex via `FString::Printf(TEXT("%016llX"), Salt)`, persist, then `GConfig->Flush(false, GGameIni)`. The XOR happens at COUNTDOWN → RUNNING (GDD Rule 21): `RunSeed = FPlatformTime::Cycles64() XOR InstallationSalt`.

### Structural Decision 5 — Android lifecycle delegate thread safety: `AsyncTask` wrap to GameThread

Callback bodies for `ApplicationWillEnterBackgroundDelegate` and `ApplicationHasEnteredForegroundDelegate` are wrapped in `AsyncTask(ENamedThreads::GameThread, [WeakThis = TWeakObjectPtr<URunStateMachineSubsystem>(this)]() { ... })` before mutating `is_paused`, `t_pause_start`, `total_paused_duration`, or any other RSM state. iOS is GameThread-dispatched on UE 5.7 (Verification Required); Android event-thread dispatch in some configurations is the structural risk. Symmetric wrap on both platforms keeps the call-site clean and defensive — the `AsyncTask` is a no-op (zero-latency forward) when invoked from GameThread.

### Architecture

```
UGameInstance
    │
    ├── URunStateMachineSubsystem                 [UGameInstanceSubsystem + FTickableGameObject]
    │       │
    │       ├── Initialize()
    │       │       ├── Reads GConfig salt; generates+persists FGuid if absent
    │       │       ├── Constructs FAppTimeSource (production) — platform-conditional impl
    │       │       │      (delegates to shared sleep-aware primitives in Platform/SleepAwareClock.h)
    │       │       ├── Subscribes: FCoreDelegates::ApplicationWillEnterBackgroundDelegate
    │       │       │      → OnApplicationWillEnterBackground (wrapped in AsyncTask(GameThread))
    │       │       └── Subscribes: FCoreDelegates::ApplicationHasEnteredForegroundDelegate
    │       │              → OnApplicationHasEnteredForeground (wrapped in AsyncTask(GameThread))
    │       │
    │       ├── Tick(float DeltaTime)              [FTickableGameObject; FIRST stmt is `if (bHasTickedThisFrame) return;` guard, SECOND stmt sets bHasTickedThisFrame=true; INT-005 2026-06-26]
    │       │       ├── Process pending events (Rule 17 priority: death_confirmed first, then timer)
    │       │       └── Evaluate state-driven transitions (snap durations, RESOLVING thresholds, grace expiry)
    │       │
    │       ├── ForceTickNow()                     [Public; idempotent within frame via bHasTickedThisFrame]
    │       │       ├── If !bHasTickedThisFrame: invoke Tick body with DeltaTime=FApp::GetDeltaTime()
    │       │       └── Non-callback invariant: MUST NOT broadcast OnStateChanged / OnPausedChanged
    │       │
    │       ├── RequestAbort(EDPCAbortReason)      [Public; external entry from DPC async-load watchdog]
    │       │       └── Transitions RUNNING/COUNTDOWN → ABORTED → IDLE (mirrors PAUSED_TIMEOUT_S path)
    │       │
    │       ├── OnEndFrame subscription            [Resets bHasTickedThisFrame=false at frame boundary]
    │       │       └── Handle stored at Initialize(); Removed at Deinitialize() (PIE-shutdown safety)
    │       │
    │       ├── IsTickable() → false in IDLE, true otherwise
    │       ├── GetTickableTickType() → ETickableTickType::Conditional
    │       ├── GetStatId() → STATGROUP_RunStateMachine
    │       │
    │       ├── Public delegates (subscribed by downstream):
    │       │       ├── OnStateChanged (FOnStateChanged: previous, new, outcome, timestamp)
    │       │       └── OnPausedChanged (FOnPausedChanged: is_paused, timestamp)
    │       │
    │       ├── Public read-only accessors (poll-path consumers; e.g., HUD per-frame display):
    │       │       ├── GetCurrentState() / GetRemainingTime() / GetRunOutcome() / IsPaused() / IsResumeGrace()
    │       │       └── GetRunSeed() (uint64 native) / GetRunSeedBP() (int64 BlueprintPure wrapper)
    │       │
    │       └── Deinitialize()
    │              ├── Unsubscribes lifecycle delegates + OnEndFrame (FDelegateHandle Remove × 3)
    │              └── Releases FAppTimeSource (TUniquePtr drop)
    │
    ├── UWaveSpawnerSubsystem                     [ADR-0005; binds RSM in its own Initialize()]
    │       └── Initialize() — calls Collection.InitializeDependency(URunStateMachineSubsystem::StaticClass())
    │                         BEFORE binding RSM->OnPausedChanged (this ADR Implementation Guideline 3)
    │
    └── UDPCController (UObject + FTickableGameObject)  [DPC GDD R6 Rule 3; ADR-0008]
            └── Tick() — calls RSM->ForceTickNow() as first statement after SCOPE_CYCLE_COUNTER + thread check
                         then reads remaining_time + current_state via direct accessors

Tick ordering (pull-based; no cascading-delegate chain):
    DPC Tick() {
        SCOPE_CYCLE_COUNTER(STAT_DPCTick);
        check(IsInGameThread());
        RSM->ForceTickNow();                 // idempotent — runs RSM tick body only if not already ticked this frame
        const double t_remaining = RSM->GetRemainingTime();
        // ... compute FDPCFrameState ...
        OnPostTickFrameStatePublished.Broadcast(CurrentState);   // ADR-0005 R2a-2 path; Wave Spawner subscribes
    }
```

### Key Interfaces

```cpp
// URunStateMachineSubsystem.h
// Multiple inheritance: UGameInstanceSubsystem (lifecycle) + FTickableGameObject (tick).
// UGameInstanceSubsystem does NOT tick by default in UE 5.7.

UENUM(BlueprintType)
enum class ERunState : uint8 { IDLE, COUNTDOWN, RUNNING, DEAD, COMPLETE, RESOLVING, ABORTED };

UENUM(BlueprintType)
enum class ERunOutcome : uint8 { NONE = 0, DEAD, COMPLETE, ABORTED };

// Per GDD Rule 15: non-dynamic multicast.
DECLARE_MULTICAST_DELEGATE_FourParams(
    FOnStateChanged,
    ERunState   /* PreviousState */,
    ERunState   /* NewState */,
    ERunOutcome /* RunOutcome */,
    double      /* Timestamp */);

// Per GDD Rule 18 (Wave Spawner R3a FC-2): non-dynamic multicast.
DECLARE_MULTICAST_DELEGATE_TwoParams(
    FOnPausedChanged,
    bool   /* IsPaused */,
    double /* Timestamp */);

// EDPCAbortReason: declared in DPC module; forward-declared here for ForceTickNow + RequestAbort surface.
// Owned by DPC per DPC GDD R6 Rule 3 forward contract item 4. Includes (at minimum):
//   AsyncLoadTimeout (DPC EC-13 watchdog expiry), + future reasons as DPC adds them.
enum class EDPCAbortReason : uint8;

// Production clock interface — non-UObject pure C++ abstract per GDD Clock Injection.
class RunTimeSource
{
public:
    // Monotonically non-decreasing wall-clock seconds. MUST continue counting during device sleep.
    virtual double GetCurrentTime() const = 0;
    virtual ~RunTimeSource() = default;
};

UCLASS()
class URunStateMachineSubsystem
    : public UGameInstanceSubsystem
    , public FTickableGameObject
{
    GENERATED_BODY()

public:
    // --- UGameInstanceSubsystem interface ---
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // --- FTickableGameObject interface ---
    virtual void Tick(float DeltaTime) override;
    virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Conditional; }
    virtual bool IsTickable() const override { return CurrentState != ERunState::IDLE; }
    virtual TStatId GetStatId() const override
    {
        RETURN_QUICK_DECLARE_CYCLE_STAT(URunStateMachineSubsystem, STATGROUP_RunStateMachine);
    }

    // --- Public delegates ---
    FOnStateChanged  OnStateChanged;
    FOnPausedChanged OnPausedChanged;

    // --- Tick-ordering pull primitive (DPC GDD R6 Rule 3 binding; this ADR Structural Decision 2) ---
    // Idempotent within one engine frame via bHasTickedThisFrame. Game-thread only.
    // Non-callback invariant (BINDING): MUST NOT broadcast OnStateChanged or OnPausedChanged
    // during execution. Callback subscribers observe state transitions on RSM's next regular
    // tick (1-frame latency). Polling consumers (DPC, Wave Spawner, Audio Controller) get
    // immediate visibility via direct accessor reads after ForceTickNow() returns.
    void ForceTickNow();

    // --- External abort entry (DPC GDD R6 Rule 3 forward contract item 4; DPC EC-13 watchdog) ---
    // Transitions RUNNING/COUNTDOWN → ABORTED → IDLE; mirrors PAUSED_TIMEOUT_S-exceeded path.
    // Game-thread only. Idempotent in terminal states (no-op in DEAD/COMPLETE/RESOLVING/IDLE/ABORTED).
    void RequestAbort(EDPCAbortReason Reason);

    // --- Public read-only state (GDD Rule 21) ---
    // C++ accessor returns native uint64 (GDD-pinned storage type — Rule 21).
    // Blueprint exposure via UFUNCTION wrapper (UPROPERTY uint64 BlueprintReadOnly is
    // UHT-rejected on UE 5.7; documented brief deviation from GDD Rule 21's literal
    // UPROPERTY form — semantic intent preserved via UFUNCTION).
    uint64 GetRunSeed() const { return RunSeed; }

    UFUNCTION(BlueprintPure, Category = "RSM|Run", meta = (DisplayName = "Get Run Seed"))
    int64 GetRunSeedBP() const { return static_cast<int64>(RunSeed); }

    // --- Read accessors (downstream pull path) ---
    ERunState   GetCurrentState()   const { return CurrentState; }
    ERunOutcome GetRunOutcome()     const { return RunOutcome; }
    bool        IsPaused()          const { return bIsPaused; }
    bool        IsResumeGrace()     const { return bResumeGrace; }
    double      GetRemainingTime()  const; // Implements F-1; returns cached value when bIsPaused

    // --- Event entry points (UI / Collision / RESOLVING tap dispatch) ---
    void RequestRunStart();                 // IDLE → COUNTDOWN gate (AC-02, AC-03)
    void ReportDeathConfirmed();            // RUNNING → DEAD gate (AC-06, AC-14, AC-20, AC-22, AC-29)
    void RequestResolvingDismiss();         // RESOLVING → IDLE tap-skip gate (AC-16, AC-17)

#if !UE_BUILD_SHIPPING
    // Test injection: must be called BEFORE Initialize() runs (test fixtures override
    // PostInitialize or instrument UGameInstance::Init). After Initialize(), the
    // production FAppTimeSource is in place and substitution is a category error.
    void SetTimeSourceForTesting(TUniquePtr<RunTimeSource> InTimeSource);
#endif

private:
    // --- Lifecycle callback handlers (wrapped in AsyncTask(GameThread) — Decision §5) ---
    void OnApplicationWillEnterBackground();
    void OnApplicationHasEnteredForeground();

    // --- Internal state ---
    ERunState   CurrentState     = ERunState::IDLE;
    ERunOutcome RunOutcome       = ERunOutcome::NONE;
    bool        bIsPaused        = false;
    bool        bResumeGrace     = false;
    uint64      RunSeed          = 0;   // captured at COUNTDOWN → RUNNING transition
    uint64      InstallationSalt = 0;   // read from GConfig at Initialize()

    // --- Re-entrancy guards (GDD Rules 15 + 18) — runtime conditional, NOT check/ensure ---
    bool bBroadcastingState  = false;
    bool bBroadcastingPaused = false;

    // --- ForceTickNow idempotency flag (this ADR Structural Decision 2; DPC GDD R6 Rule 3 fc-2) ---
    // GUARDED on (early-return if true) AS FIRST STATEMENT of both Tick() AND
    // ForceTickNow(); SET to true immediately after the guard (before any body logic)
    // in both paths. Symmetric guard-then-set pattern closes INT-005 (2026-06-26)
    // — prevents in-frame re-entry double-advance regardless of which path executes first.
    // Reset to false at frame boundary via OnEndFrame subscription handler.
    bool bHasTickedThisFrame = false;

    // --- Cached values (frozen at transition; GDD F-1 cache-on-pause/death) ---
    double CachedRemainingAtPauseEntry = 0.0;
    double CachedRemainingAtDeathEntry = 0.0;

    // --- Timer formula state (double precision throughout; GDD F-1, F-3, F-4) ---
    double tRunStart                     = 0.0;
    double tCountdownStart               = 0.0;
    double tPauseStart                   = 0.0;  // shared field; bIsPaused gates interpretation
    double TotalPausedDuration           = 0.0;
    double TotalCountdownPausedDuration  = 0.0;

    // --- Clock injection (GDD Clock Injection) ---
    TUniquePtr<RunTimeSource> TimeSource;

    // --- Delegate handles for clean unbind ---
    FDelegateHandle WillEnterBackgroundHandle;
    FDelegateHandle HasEnteredForegroundHandle;
    FDelegateHandle OnEndFrameHandle;   // FCoreDelegates::OnEndFrame; resets bHasTickedThisFrame
};
```

```cpp
// FAppTimeSource.h — production sleep-aware impl
// Delegates to Platform/SleepAwareClock.h for the shared platform primitives also used by
// IS Seam 1's FiOSContinuousTimeClock / FAndroidBootTimeClock. RSM returns seconds; IS returns ms.

#include "Platform/SleepAwareClock.h"  // GetSleepAwareSeconds() — branched on PLATFORM_IOS / PLATFORM_ANDROID

class FAppTimeSource final : public RunTimeSource
{
public:
    double GetCurrentTime() const override { return GetSleepAwareSeconds(); }
};
```

```cpp
// Initialize() skeleton — subscribes lifecycle delegates with AsyncTask wrap
void URunStateMachineSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // 1) Read or generate per-installation salt (Structural Decision 4)
    //    GConfig stores as hex string; GetUInt64/SetUInt64 are not part of FConfigCacheIni's API.
    //    INT-006 (2026-06-26): FCString::Strtoui64 — primary-source verification of UE 5.7
    //    Misc/Parse.h confirmed FParse::HexNumber64 has a value-return FStringView signature in
    //    5.7 (evolved from the 5.3 (const TCHAR*, TCHAR**) form); neither shape matches the
    //    bool+out-param call this code originally used. FCString::Strtoui64 has stable
    //    C-stdlib heritage and a single value-return form across UE 5.0–5.7; it returns 0
    //    on invalid hex input, which the `Salt == 0` check below subsumes (failure modes
    //    "key missing", "value empty", "value non-hex", "value parses to zero" all funnel
    //    through one regenerate branch — identical to the original 4-condition semantics).
    FString SaltHex;
    uint64  Salt = 0;
    const TCHAR* Section = TEXT("/Script/SLIPSTORM.RunStateMachine");
    const TCHAR* Key     = TEXT("RunSeedSalt");
    if (GConfig->GetString(Section, Key, SaltHex, GGameIni) && !SaltHex.IsEmpty())
    {
        Salt = FCString::Strtoui64(*SaltHex, nullptr, 16);
    }
    if (Salt == 0)
    {
        const FGuid NewSalt = FGuid::NewGuid();
        const uint64 Hi = (static_cast<uint64>(NewSalt.A) << 32) | static_cast<uint64>(NewSalt.B);
        const uint64 Lo = (static_cast<uint64>(NewSalt.C) << 32) | static_cast<uint64>(NewSalt.D);
        Salt = Hi ^ Lo;
        const FString NewHex = FString::Printf(TEXT("%016llX"), Salt);
        GConfig->SetString(Section, Key, *NewHex, GGameIni);
        GConfig->Flush(false, GGameIni);
    }
    InstallationSalt = Salt;

    // 2) Construct production time source if no test-injected source is present
    if (!TimeSource.IsValid())
    {
        TimeSource = MakeUnique<FAppTimeSource>();
    }

    // 3) Subscribe lifecycle delegates (AsyncTask wrap per Structural Decision 5)
    WillEnterBackgroundHandle = FCoreDelegates::ApplicationWillEnterBackgroundDelegate.AddLambda(
        [WeakThis = TWeakObjectPtr<URunStateMachineSubsystem>(this)]()
        {
            AsyncTask(ENamedThreads::GameThread, [WeakThis]()
            {
                if (URunStateMachineSubsystem* Self = WeakThis.Get())
                {
                    Self->OnApplicationWillEnterBackground();
                }
            });
        });

    HasEnteredForegroundHandle = FCoreDelegates::ApplicationHasEnteredForegroundDelegate.AddLambda(
        [WeakThis = TWeakObjectPtr<URunStateMachineSubsystem>(this)]()
        {
            AsyncTask(ENamedThreads::GameThread, [WeakThis]()
            {
                if (URunStateMachineSubsystem* Self = WeakThis.Get())
                {
                    Self->OnApplicationHasEnteredForeground();
                }
            });
        });

    // 4) Subscribe OnEndFrame to reset bHasTickedThisFrame at frame boundary
    //    (DPC GDD R6 Rule 3 forward contract item 2 + this ADR Structural Decision 2)
    OnEndFrameHandle = FCoreDelegates::OnEndFrame.AddLambda(
        [WeakThis = TWeakObjectPtr<URunStateMachineSubsystem>(this)]()
        {
            if (URunStateMachineSubsystem* Self = WeakThis.Get())
            {
                Self->bHasTickedThisFrame = false;
            }
        });
}

// Tick() body skeleton — bHasTickedThisFrame must be GUARDED then SET before any body logic
void URunStateMachineSubsystem::Tick(float DeltaTime)
{
    // FIRST STATEMENT: early-return guard. Symmetric with ForceTickNow() at lines 431–434.
    // Closes INT-005 (architecture-review-2026-06-26 BLOCKING). Without this guard,
    // ForceTickNow() running first this frame would set the flag + execute the body,
    // then the engine's later regular Tick() call would re-execute the body —
    // double-advancing remaining_time + double-firing Rule 17 priority. Risk 4
    // mitigation row below explicitly promises this short-circuit behavior.
    if (bHasTickedThisFrame)
    {
        return;
    }
    bHasTickedThisFrame = true;   // SECOND STATEMENT, before any body logic — prevents in-frame re-entry double-advance

    // ... process pending events (Rule 17 priority) ...
    // ... evaluate state-driven transitions ...
    // ... NO subscriber broadcasts during ForceTickNow path ...
}

// ForceTickNow() — idempotent within frame; non-callback invariant binding
void URunStateMachineSubsystem::ForceTickNow()
{
    check(IsInGameThread());
    if (bHasTickedThisFrame)
    {
        return;   // already ticked this frame — no-op
    }
    Tick(FApp::GetDeltaTime());   // Tick() guards on bHasTickedThisFrame as 1st stmt then sets it as 2nd stmt (INT-005 amendment)
    // Non-callback invariant: Tick() must not have fired OnStateChanged/OnPausedChanged.
    // Internal state was updated; transition observability to callback subscribers is
    // deferred to RSM's next regular Tick (1-frame latency); poll consumers see immediately.
}
```

### Implementation Guidelines

1. **Never call `SpawnActor` in `Initialize()`.** Mirror of ADR-0005 Implementation Guideline 1. `GetWorld()` returns null at `UGameInstanceSubsystem::Initialize()` in UE 5.7; any actor creation must defer to `FCoreUObjectDelegates::PostLoadMapWithWorld` (RSM does not currently need to spawn any actors — it has no entity references per GDD Rule 12 — but the prohibition is registered as forbidden pattern below to prevent regression).

2. **Read GConfig salt and construct `FAppTimeSource` BEFORE subscribing lifecycle delegates.** A delegate firing before `TimeSource` is valid would crash on `TimeSource->GetCurrentTime()` inside the F-2 stale-detection path.

3. **Wave Spawner `Initialize()` MUST call `Collection.InitializeDependency(URunStateMachineSubsystem::StaticClass())` before binding `RSM->OnPausedChanged`.** This pins the dependency order at the subsystem-collection level. ADR-0005 MUST be amended to register this Implementation Guideline — **CLOSED by ADR-0005 amendment INT-004 (2026-06-26)**: ADR-0005 Implementation Guideline 8 + Initialize() skeleton now inscribe `Collection.InitializeDependency(URunStateMachineSubsystem::StaticClass())` immediately after `Super::Initialize(Collection)`. If UE 5.7's `InitializeDependency` only guarantees construction (not Initialize-completion) ordering, fallback path is in Risks below + mirrored at ADR-0005 Risk 5.

4. **DPC's `UObject + FTickableGameObject` construction must locate `URunStateMachineSubsystem` via `UGameInstance::GetSubsystem<URunStateMachineSubsystem>()` BEFORE DPC's first `Tick()` fires.** DPC GDD R6 Rule 3 specifies DPC's production outer is resolved by the OQ-7 ADR (ADR-0008); the outer holds a `TObjectPtr<URunStateMachineSubsystem>` resolved during DPC's construction or `Initialize()`-equivalent. Subsystem dependency-ordering via `InitializeDependency` does NOT apply to DPC because DPC is NOT itself a subsystem (it is a `UObject` ticked via `FTickableGameObject`; ADR-0008 specifies the production outer).

5. **RSM `Tick()` MUST guard on `bHasTickedThisFrame == true` with early-return AS FIRST STATEMENT, then set `bHasTickedThisFrame = true` AS SECOND STATEMENT — before any body logic.** Mirror of `ForceTickNow()` guard-then-set pattern at lines 431–434 (see Tick() body skeleton above). Without the guard, `ForceTickNow()` running first this frame sets the flag + runs the body, and the engine's later regular `Tick()` call would re-run the body — double-advancing `remaining_time` + double-firing Rule 17 priority. With the guard, the engine's later `Tick()` call short-circuits (the Risk 4 mitigation promise). Symmetric reverse case: engine ticks first → flag set + body runs → DPC's later `ForceTickNow()` short-circuits via its own guard at line 431. Closes INT-005 (architecture-review-2026-06-26 BLOCKING; 2026-06-26 amendment). DPC GDD R6 Rule 3 forward contract item 2 (R6 unreal-specialist B-3 fix).

5b. **Order of operations inside `Tick()` body**: Rule 17 priority — (a) process pending `death_confirmed` events first; (b) evaluate timer expiry only if no death event was processed; (c) evaluate snap durations / RESOLVING thresholds / grace expiry. Subscribers to `OnStateChanged` and `OnPausedChanged` observe the post-transition state at broadcast time.

5c. **Non-callback invariant inside `ForceTickNow()`**: the primitive MUST NOT cause any `OnStateChanged` or `OnPausedChanged` broadcast during its execution. State transitions that fire from inside the `Tick()` body when called via `ForceTickNow()` are observable to direct-accessor consumers (DPC, Wave Spawner accessing `GetCurrentState()`) IMMEDIATELY but not to callback subscribers until RSM's next regular tick — a documented 1-frame latency. Implementation pattern: defer subscriber broadcasts to a queued-event list that the next regular `Tick()` (not the inline `ForceTickNow()` path) drains. Wave Spawner and Audio Controller may rely on polls rather than callbacks to eliminate the latency (DPC GDD R6 Rule 3 forward contract item 3).

6. **Re-entrancy guards use runtime conditional + `UE_LOG`, NOT `check()` or `ensure()`** (GDD Rule 15 + 18). `bBroadcastingState` and `bBroadcastingPaused` are independent booleans (NOT a shared single flag — Rule 18 explicit) so that an `OnPausedChanged` subscriber legitimately triggering an `OnStateChanged` broadcast is not silently suppressed.

7. **NO `FTimerManager` usage anywhere in RSM.** All timer evaluations — snap durations, RESOLVING thresholds, resume_grace expiry, COUNTDOWN elapsed — use per-tick `TimeSource->GetCurrentTime()` comparisons (GDD Clock Injection Timer Implementation Policy).

8. **Lifecycle callbacks ALWAYS wrap in `AsyncTask(ENamedThreads::GameThread, ...)`.** Symmetric on iOS and Android even if iOS is GameThread-dispatched (the wrap is a no-op forward in that case). `WeakThis` capture protects against the subsystem being torn down while a delegate is in-flight.

9. **`RunTimeSource` is set via `SetTimeSourceForTesting()` BEFORE `Initialize()` runs in test fixtures.** Production path: no setter invocation; `Initialize()` constructs `FAppTimeSource` itself. Switching the source after `Initialize()` is a category error and is `#if !UE_BUILD_SHIPPING` gated to prevent production misuse.

10. **`RunSeed` is captured at the COUNTDOWN → RUNNING transition.** Read at any earlier point returns the previous run's value (or zero on first run). Consumers (Wave Spawner, Death Replay) must observe `OnStateChanged(NewState=RUNNING)` first, then read `RunSeed`. Direct UPROPERTY access from Blueprints carries the same constraint.

## Alternatives Considered

### Alternative 1: `AActor` singleton in `PersistentLevel`

- **Description**: Place a single `ARunStateMachineActor` in the `PersistentLevel`. `BeginPlay` reads salt + constructs time source; `EndPlay` releases. `TG_PrePhysics` tick group + `AddTickPrerequisiteActor` provide tick ordering.
- **Pros**: Tick ordering uses the canonical UE prerequisite APIs (`AddTickPrerequisiteActor`/`Component`) — though DPC GDD R6 Rule 3's `ForceTickNow` pull primitive also works against Actor-hosted RSM, so this is not a unique advantage; familiar Actor lifecycle visible in editor outliner.
- **Cons**: Death-replay world reloads can destroy and reconstruct `PersistentLevel` actors depending on level streaming policy. `RunSeed` continuity across replays would require external persistence. `UGameInstance::GetSubsystem<>()` accessor pattern is replaced by `UGameplayStatics::GetActorOfClass()`, which is O(actor count) and null-prone. Wave Spawner's `Initialize()` cannot bind to an Actor that doesn't exist yet (subsystem-collection dependency injection is unavailable for actors).
- **Estimated Effort**: ~equivalent (different lifecycle wiring; similar amount of code).
- **Rejection Reason**: Session-state continuity is at structural risk; subsystem-collection dependency mechanism is unavailable; Wave Spawner Initialize-time race is reintroduced.

### Alternative 2: `UWorldSubsystem`

- **Description**: Host RSM as a `UWorldSubsystem`. `Initialize()` runs once per `UWorld` load; pool any state in `Deinitialize()`.
- **Pros**: Simpler lifecycle alignment with world-scoped state; automatic teardown on world destruction.
- **Cons**: Re-initializes on every world reload. SLIPSTORM's replay path triggers a world reload on each death; `RunSeed` would be regenerated mid-session, breaking Death Replay (Wave Spawner AC-WS-13 binding) and Pillar 5 ("Skill Is Visible — no mystery outcomes"). Mirrors the same lifecycle-mismatch ADR-0005 rejected for Wave Spawner — same root cause (death-replay = world reload, GameInstance survives, World does not).
- **Estimated Effort**: ~equivalent.
- **Rejection Reason**: Loses state continuity across the replay cycle.

### Alternative 3: `AGameModeBase` / `AGameStateBase`

- **Description**: Embed RSM state and tick in a custom `AGameModeBase` or `AGameStateBase` subclass.
- **Pros**: Standard UE pattern for "global game logic."
- **Cons**: Pinned to a single world (same continuity gap as Alternative 2). `AGameModeBase` is server-only by convention — irrelevant to SLIPSTORM solo at MVP, but a forced rework if a future leaderboard adds networking. State exposed via `UFUNCTION(BlueprintCallable)` rather than the natural `UPROPERTY(BlueprintReadOnly)` (which works inside subsystems too).
- **Estimated Effort**: ~equivalent.
- **Rejection Reason**: Wrong lifecycle scope; future-networking constraint; no advantage over `UGameInstanceSubsystem`.

### Alternative 4: Inline `FApp::GetCurrentTime()` / `FPlatformTime::Seconds()` (no clock seam)

- **Description**: Skip `RunTimeSource` injection. Read wall clock directly from UE APIs.
- **Pros**: Zero abstraction; simplest possible code path.
- **Cons**: Two structural defects: (a) untestable — `FApp` and `FPlatformTime` cannot be substituted in unit tests; AC-08, AC-09, AC-10, AC-11, AC-12, AC-15, AC-17, AC-18, AC-22, AC-23, AC-25, AC-27, AC-28, AC-29, AC-30 all require `FakeTimeSource` injection and become uncoverable; (b) lifecycle-blind — `FApp::GetCurrentTime()` is set once per GameThread tick and does not advance during `ApplicationWillEnterBackgroundDelegate` execution. A 5-minute background pause accumulates as ~0s in F-3, silently corrupting the run timer (GDD EC-9 explicit warning at line 64). `FPlatformTime::Seconds()` pauses during device sleep on both mobile platforms.
- **Estimated Effort**: Lower (no interface).
- **Rejection Reason**: Untestable + lifecycle-blind + sleep-blind. Three independent structural failures.

### Alternative 5: Unify with IS Seam 1 `IMonotonicClock` (single shared interface)

- **Description**: Replace `RunTimeSource` with `IMonotonicClock` directly; RSM stores `t_run_start` in ms and divides by 1000 at F-1 boundary.
- **Pros**: Single seam, single production impl pair, single fake. No platform-clock duplication. Simpler to source-audit.
- **Cons**: Contradicts RSM GDD Clock Injection section explicit text ("the `I` prefix is reserved for UE UInterface types; this class must not use it") — even though Seam 1's `IMonotonicClock` is empirical evidence that the GDD's `I`-prefix rule is overstated, the GDD is the design authority and a unification here would require an explicit GDD revision. Unit conversion at the boundary (ms → s) is a small but recurring code-review trap.
- **Estimated Effort**: Slightly lower (single seam).
- **Rejection Reason**: Requires GDD revision; the Structural Decision 3 "separate interfaces, shared platform primitives" path achieves nearly the same code reuse (one shared `Platform/SleepAwareClock.h` header) without contradicting the GDD.

## Consequences

### Positive

- Session-state continuity is structural: `RunSeed` immutable across the entire game session, replay-coherent for Death Replay (Wave Spawner AC-WS-13 binding).
- Subsystem-collection dependency mechanism (`InitializeDependency`) gives Wave Spawner and DPC a clean ordering pin — no race-condition workarounds at the call site.
- Sleep-aware wall clock is shared with IS Seam 1's primitives — one platform-clock implementation pair, two seams.
- `ForceTickNow()` pull primitive guarantees ordering across ALL choices of RSM type (this ADR keeps `UGameInstanceSubsystem + FTickableGameObject`, but the same primitive works under `AActor`, `UWorldSubsystem`, or `UGameInstanceSubsystem` — robustness inherited from DPC GDD R6 Rule 3 unreal-specialist B-1 analysis).
- `ETickableTickType::Conditional` + `IsTickable()` returning `false` in `IDLE` eliminates RSM tick overhead during the main-menu surface.
- Test path uses `FakeTimeSource` injection at `Initialize()` — every AC from AC-04 onward becomes deterministically reproducible.

### Negative

- Multiple inheritance (`UGameInstanceSubsystem + FTickableGameObject`) is unconventional and the same trap as ADR-0005: if future engineers move admission or transition logic between `Tick()` and event handlers, the structural-ordering guarantee breaks. Code review gate required.
- `AsyncTask(ENamedThreads::GameThread)` wrap adds ~1-frame latency to background/foreground events. Acceptable for lifecycle events that are infrequent and player-visible as "the game went away and came back," but the latency is documented (Performance Implications).
- `RunSeed` immutability across replays means a player cannot "re-roll" a bad run by quickly restarting; this is a deliberate design property of Death Replay determinism, surfaced here for traceability.
- Two clock seams (`RunTimeSource` + `IMonotonicClock`) with one shared platform-clock primitive is duplication at the interface level. Justified by GDD authority but a future architect may want to revisit.
- `RunSeed` Blueprint exposure is via `UFUNCTION(BlueprintPure) GetRunSeedBP()` returning `int64`, not a literal `UPROPERTY(BlueprintReadOnly) uint64` as the GDD Rule 21 prose specifies. UHT rejects `uint64` with `BlueprintReadOnly` on UE 5.7; this is a documented brief deviation from GDD Rule 21's literal form. Semantic intent (Blueprint read-only access to the seed value) is preserved.
- `ForceTickNow()`'s non-callback invariant means subscriber-side state-transition visibility is split: poll consumers (DPC, Wave Spawner, Audio Controller via accessor reads) see transitions immediately; callback subscribers (`OnStateChanged` / `OnPausedChanged` bound systems) observe transitions on the next regular `Tick()`. The 1-frame latency is documented in the Key Interfaces comment and at DPC GDD R6 Rule 3 forward contract item 3. Wave Spawner explicitly accepts this latency in its R3a binding to `OnPausedChanged` (the pause-flush contract).
- `EDPCAbortReason` enum is owned by DPC module — a slight ownership inversion (RSM exposes an entry point typed on a DPC concept). Justified by EC-13's async-load watchdog being a DPC concern; RSM is the abort sink, not the abort source. Forward declaration of the enum in RSM's header keeps the cross-module include graph clean.

### Neutral

- `UGameInstanceSubsystem` discovery pattern is Blueprint-accessible via `UGameInstance::GetSubsystem<>()` if a future Blueprint-side debug tool is needed; not currently required.
- `STATGROUP_RunStateMachine` adds a new Unreal Insights profiling surface (sibling to `STATGROUP_WaveSpawner`).

## Risks

| Risk | Probability | Impact | Mitigation |
|------|------------|--------|-----------|
| `FSubsystemCollectionBase::InitializeDependency()` guarantees construction order but NOT `Initialize()` completion order in UE 5.7 | Low | High — Wave Spawner / DPC binds to a partially-initialized RSM | Source-verify before shipping. If verification fails, fallback: RSM emits `OnInitialized` (one-shot delegate), Wave Spawner / DPC `Initialize()` defers `RSM->OnPausedChanged.AddUObject(...)` to that handler. |
| `AsyncTask(ENamedThreads::GameThread)` wrap creates 1-frame latency that violates `PAUSED_TIMEOUT_S` semantics in an edge case; also: a foreground event arriving within that one-frame window can produce interleaved background/foreground handler execution | Very Low | Low | `PAUSED_TIMEOUT_S` minimum is 60s (GDD Tuning Knobs); one-frame delay (~16.6 ms) is 0.03% of the threshold. Interleaving is mitigated by the `bIsPaused` state check at the top of each handler (background no-ops if already paused; foreground no-ops if not paused). No measurable effect on AC outcomes. |
| GConfig salt write is lost mid-flight (app crash between `SetUInt64` and `Flush`) | Low | Low | `Flush()` immediately after write. If still lost: next launch regenerates and writes again. Salt loss is equivalent to a fresh installation (per-installation entropy resets); RunSeed determinism within a session is unaffected. |
| Engine ticks RSM regular `Tick()` AFTER DPC's `Tick()` calls `ForceTickNow()` — double-advance via the second tick path | Low | High | `bHasTickedThisFrame` flag is GUARDED on (early-return if true) AS FIRST STATEMENT of both `Tick()` and `ForceTickNow()` paths (Implementation Guideline 5; symmetric guard-then-set pattern; INT-005 amendment 2026-06-26). The set happens immediately after the guard, before any body logic. Engine's later regular `Tick()` call short-circuits because the flag is already true. Flag resets at `OnEndFrame`. |
| `ForceTickNow()` invocation re-entrantly inside RSM's own `Tick()` body produces infinite recursion or stack overflow | Very Low | High | `ForceTickNow()` early-returns when `bHasTickedThisFrame == true`. The flag is set inside `Tick()` BEFORE any logic runs that might re-call `ForceTickNow()`. Structural; not a runtime check. |
| `EDPCAbortReason` enum is owned by DPC module — RSM forward-declares it; build-order or include-graph circular dependency | Low | Low | DPC declares `EDPCAbortReason` in a shared header (`DPCAbortReason.h`) that RSM includes. No circular dependency: RSM does NOT include DPC's other headers — only the enum declaration. |
| `mach_continuous_time()` returns a different epoch than `mach_absolute_time()` — switching base mid-session would be catastrophic | Very Low | Low | Switch happens at compile time (no runtime mode switch). `t_run_start` is sampled with the same source as later `time_source.GetCurrentTime()` calls; epoch consistency holds. |
| Re-entrant `OnStateChanged` triggers from inside a subscriber log floods the output | Low | Low | Re-entrancy guard logs at `Error` level and returns. One log per re-entry. Code review gate: subscribers must not trigger transitions. |
| Salt read from GConfig returns valid value but tied to a previously-uninstalled app version (Android app data preserved across reinstall) | Medium | Negligible | This is the intended behavior — the salt is per-installation, and Android's "preserve app data" treats reinstall as the same installation. No correctness issue. |

## Performance Implications

| Metric | Before | Expected After | Budget |
|--------|--------|---------------|--------|
| Per-tick RSM `Tick()` CPU | N/A | State transition eval + double subtraction (F-1) — small fraction of frame budget | < 0.05 ms p99 mid-tier mobile (placeholder; AC for Alpha-gate Unreal Insights profile) |
| `ForceTickNow()` cost when already ticked | N/A | One bool read + one early return | < 0.001 ms (single integer compare + return) |
| Lifecycle delegate callback (background/foreground) | N/A | `AsyncTask` dispatch + 1-frame latency | ~16.6 ms one-shot latency per pause/resume edge — acceptable for a session-format-with-app-suspension event |
| `FAppTimeSource` Salt read | N/A | One-shot at `Initialize()` (GConfig hit cached for life of subsystem) | < 1 ms one-shot |
| `FAppTimeSource::GetCurrentTime()` per call | N/A | `mach_continuous_time()` + 2 multiplies + 1 divide (iOS); `clock_gettime()` + 1 add + 1 divide (Android) | < 0.001 ms per call (well below noise) |
| Idle (IDLE state) tick overhead | N/A | 0 — `IsTickable()` returns false | 0 ms; structural guarantee via `ETickableTickType::Conditional` |

## Migration Plan

Greenfield system — no existing RSM implementation to migrate.

1. Author `URunStateMachineSubsystem` class with the signature and split described in Key Interfaces. Verify `Initialize()` contains no `SpawnActor` calls.
2. Author `Platform/SleepAwareClock.h` shared header (`GetSleepAwareSeconds()` / `GetSleepAwareMilliseconds()` — platform-conditional). Refactor IS Seam 1's `FiOSContinuousTimeClock` / `FAndroidBootTimeClock` to delegate to the shared header at the same time (small, mechanical change to keep IS Seam 1 in sync).
3. Implement `FAppTimeSource` as a `RunTimeSource` adapter over `GetSleepAwareSeconds()`.
4. Implement `URunStateMachineSubsystem::Initialize()` with GConfig salt read/generate path and `AsyncTask`-wrapped lifecycle subscriptions.
5. Implement state transitions per GDD Detailed Rules + Edge Cases + Acceptance Criteria. AC-01 through AC-33 are the implementation gates.
6. Author RSM unit tests using `FakeTimeSource` (mirror of Seam 1's `FFakeMonotonicClock`).
7. Register `URunStateMachineSubsystem_Initialize_SpawnActor`, `URunStateMachineSubsystem_FApp_GetCurrentTime_usage`, `URunStateMachineSubsystem_FPlatformTime_Seconds_direct`, `URunStateMachineSubsystem_FTimerManager_usage`, `URunStateMachineSubsystem_ApplicationWillDeactivateDelegate_usage` in the Forbidden Patterns registry.
8. ~~Amend ADR-0005 with one-line Implementation Guideline registering the `Collection.InitializeDependency(URunStateMachineSubsystem::StaticClass())` requirement at the top of `UWaveSpawnerSubsystem::Initialize()`.~~ **DONE 2026-06-26 via ADR-0005 amendment INT-004** — ADR-0005 Implementation Guideline 8 + Initialize() skeleton inscribe both `InitializeDependency(URunStateMachineSubsystem)` and `InitializeDependency(UDPCSubsystem)` calls immediately after `Super::Initialize(Collection)`, plus Forbidden Pattern `WaveSpawner_Initialize_without_InitializeDependency_on_RSM_and_DPC` registered.

**Rollback plan**: If `InitializeDependency` ordering semantics in UE 5.7 prove insufficient (Risk 1), promote `OnInitialized` to the public interface and route Wave Spawner / DPC subscription through it. Subsystem class choice itself remains correct under that fallback.

## Validation Criteria

- [ ] AC-01 through AC-33 (RSM GDD lines 425–524) — every AC executable against `FakeTimeSource`-injected RSM.
- [ ] AC-08 timer accuracy: `remaining_time` within ±0.017s of expected after 30s advance.
- [ ] AC-11 / AC-12 / AC-25: `is_paused = true` correctly set on `ApplicationWillEnterBackgroundDelegate`; cleared on `ApplicationHasEnteredForegroundDelegate`; correct ABORTED transition on `PAUSED_TIMEOUT_S` exceeded.
- [ ] AC-29 `resume_grace` suppression: `death_confirmed` discarded during grace window.
- [ ] AC-30 `OnPausedChanged` fires exactly once per `is_paused` change.
- [ ] No `FApp::GetCurrentTime()`, `FPlatformTime::Seconds()`, or `FTimerManager` calls in RSM source (grep gate in code review).
- [ ] `bHasTickedThisFrame` GUARDED (early-return if true) as FIRST STATEMENT and SET to true as SECOND STATEMENT of both `Tick()` and `ForceTickNow()` paths (grep gate in code review); reset by `OnEndFrame` lambda. (INT-005 amendment 2026-06-26 — guard-then-set pattern.)
- [ ] `ForceTickNow()` idempotency: integration test calls `ForceTickNow()` 1000 times within one frame; `Tick()` body executes exactly once.
- [ ] `ForceTickNow()` non-callback invariant: integration test wires an `OnStateChanged` subscriber that asserts `NOT bInsideForceTickNow`; calls `ForceTickNow()` near a transition boundary; assertion never fires.
- [ ] `RequestAbort(EDPCAbortReason::AsyncLoadTimeout)` mid-RUNNING → ABORTED → IDLE (mirrors AC-11 / AC-12 pattern for the external-entry path).
- [ ] Wave Spawner integration: `UWaveSpawnerSubsystem::Initialize()` successfully binds to `RSM->OnPausedChanged` without observable race.
- [ ] Per-tick subsystem CPU ≤ 0.05 ms p99 on iPhone XR + Pixel 5 / Galaxy A52 under Unreal Insights (Alpha gate).
- [ ] Sleep-aware clock validation: device-sleep 5+ minute soak test — `total_paused_duration` accumulates correctly; F-2 stale detection fires on resume past `PAUSED_TIMEOUT_S`.

## Forbidden Patterns

Register the following at `docs/registry/architecture.yaml` Forbidden Patterns registry on write approval (skill Phase 6).

| Pattern | Reason |
|---------|--------|
| `RunStateMachine_SpawnActor_at_Initialize` | UWorld unavailable at `UGameInstanceSubsystem::Initialize()` in UE 5.7 (mirror of ADR-0005). RSM has no entity references per GDD Rule 12; pattern is registered defensively against future regression. |
| `RunStateMachine_FApp_GetCurrentTime_usage` | `FApp::GetCurrentTime()` is frame-cached (set once per GameThread tick) and does not advance during lifecycle callbacks; using it in F-3 silently corrupts the run timer across pauses (GDD line 64 + EC-9). |
| `RunStateMachine_FPlatformTime_Seconds_direct` | `FPlatformTime::Seconds()` is not sleep-aware on iOS (`mach_absolute_time` backing) or Android (`CLOCK_MONOTONIC` backing). Use `mach_continuous_time()` / `clock_gettime(CLOCK_BOOTTIME)` via `RunTimeSource` (GDD EC-9). |
| `RunStateMachine_FTimerManager_usage` | `FTimerManager` is incompatible with `FakeTimeSource`-based unit tests; AC-15, AC-18, AC-23, AC-27, AC-29 cannot be exercised against `FTimerManager`-driven timers (GDD Clock Injection Timer Implementation Policy). |
| `RunStateMachine_ApplicationWillDeactivateDelegate_usage` | `ApplicationWillDeactivateDelegate` fires on transient interruptions (notification banners, Control Center, incoming-call overlays); using it for run-pause semantics produces false ABORTED outcomes (GDD Rule 13). Bind to `ApplicationWillEnterBackgroundDelegate` instead. |
| `RunStateMachine_check_or_ensure_for_reentrancy` | `check()` and `ensure()` are gated by `DO_CHECK`, default 0 in Shipping. Re-entrancy guards using them are no-ops in production (GDD Rule 15). Use runtime conditional + `UE_LOG(Error)`. |
| `RunStateMachine_DECLARE_DYNAMIC_MULTICAST_for_state_or_paused` | Dynamic multicast requires `UFUNCTION`-marked handlers + reflection overhead + cannot bind lambdas (Wave Spawner R2a-2 binds `RSM->OnPausedChanged` via lambda inside `UWaveSpawnerSubsystem::Initialize()`). Use `DECLARE_MULTICAST_DELEGATE_*` (non-dynamic) per GDD Rule 15 + 18. |

## GDD Requirements Addressed

Maps to all 34 RSM technical requirements registered in `docs/architecture/tr-registry.yaml` (TR-RSM-001 through TR-RSM-034). Per-requirement attribution below.

| GDD Document | TR-ID | Requirement | How This ADR Addresses It |
|--------------|-------|-------------|---------------------------|
| `design/gdd/run-state-machine.md` | TR-RSM-001 | RSM owns exclusive lifecycle authority over run state enum and canonical run timer | `URunStateMachineSubsystem` declares `CurrentState` (`ERunState`) and timer state as private; no setter API on `CurrentState`; only `RequestRunStart()` / `ReportDeathConfirmed()` / `RequestResolvingDismiss()` event entry points expose write paths gated by the state machine internals |
| `design/gdd/run-state-machine.md` | TR-RSM-002 | Run suspension as orthogonal `bool is_paused` flag, not discrete state | `bIsPaused` is a private bool member, distinct from `CurrentState`; per-state invariant table in GDD enforced by `OnApplicationWillEnterBackground()` gating on `CurrentState` ∈ {COUNTDOWN, RUNNING} |
| `design/gdd/run-state-machine.md` | TR-RSM-003 | Broadcast `OnStateChanged(prev, new, outcome, timestamp)` on every state transition | `FOnStateChanged` declared 4-param; broadcast site is a private helper called by every transition path |
| `design/gdd/run-state-machine.md` | TR-RSM-004 | `OnStateChanged` declared via `DECLARE_MULTICAST_DELEGATE` (non-dynamic, C++-only) | `DECLARE_MULTICAST_DELEGATE_FourParams` per Key Interfaces (non-dynamic) |
| `design/gdd/run-state-machine.md` | TR-RSM-005 | Re-entrancy guard for `OnStateChanged` via runtime conditional `bool bBroadcastingState` | Private `bBroadcastingState` bool + runtime conditional in broadcast helper (Implementation Guideline 6) |
| `design/gdd/run-state-machine.md` | TR-RSM-006 | `OnPausedChanged` broadcast separately from `OnStateChanged` when `is_paused` changes | `FOnPausedChanged` declared separately; broadcast at end of `OnApplicationWillEnterBackground()` / `OnApplicationHasEnteredForeground()` |
| `design/gdd/run-state-machine.md` | TR-RSM-007 | `OnPausedChanged` declared as `DECLARE_MULTICAST_DELEGATE_TwoParams` (non-dynamic) | Key Interfaces matches GDD Rule 18 Wave Spawner R3a FC-2 pin verbatim |
| `design/gdd/run-state-machine.md` | TR-RSM-008 | Resume grace window (`resume_grace`) suppresses collision, wave spawning, movement input | `bResumeGrace` exposed via `IsResumeGrace()` direct accessor; sampled at foreground edge in RUNNING; expires on per-tick clock comparison (Implementation Guideline 7); polled by Collision + Wave Spawner + Player Movement after `ForceTickNow()` returns |
| `design/gdd/run-state-machine.md` | TR-RSM-009 | `RunTimeSource` injected dependency for all wall-clock access via `TUniquePtr<RunTimeSource>` | Private `TUniquePtr<RunTimeSource> TimeSource` member; `SetTimeSourceForTesting()` for test fixtures; production `FAppTimeSource` constructed in `Initialize()` |
| `design/gdd/run-state-machine.md` | TR-RSM-010 | `RunTimeSource` is non-UObject pure C++ abstract class, not UInterface | Key Interfaces declares plain C++ abstract `class RunTimeSource` (no `UCLASS`, no `UINTERFACE`) — preserved per Structural Decision 3 |
| `design/gdd/run-state-machine.md` | TR-RSM-011 | `FAppTimeSource` production impl uses sleep-aware iOS `mach_continuous_time()` | `Platform/SleepAwareClock.h` `GetSleepAwareSeconds()` implementation under `#if PLATFORM_IOS` uses `mach_continuous_time()` (shared with IS Seam 1) |
| `design/gdd/run-state-machine.md` | TR-RSM-012 | `FAppTimeSource` production impl uses sleep-aware Android `clock_gettime(CLOCK_BOOTTIME)` | Same shared header under `#elif PLATFORM_ANDROID` uses `clock_gettime(CLOCK_BOOTTIME, ...)` |
| `design/gdd/run-state-machine.md` | TR-RSM-013 | No `FTimerManager` usage; all timer checks use per-tick `time_source.GetCurrentTime()` comparisons | Implementation Guideline 7 + Forbidden Pattern `RunStateMachine_FTimerManager_usage` |
| `design/gdd/run-state-machine.md` | TR-RSM-014 | Run timer monotonic countdown computed from wall clock, not UE game ticks, using `double` precision | `TimeSource->GetCurrentTime()` is the sole wall-clock source; all timer state members typed `double`; `GetRemainingTime()` implements F-1 |
| `design/gdd/run-state-machine.md` | TR-RSM-015 | `FAppTimeSource` uses `#if PLATFORM_IOS / #elif PLATFORM_ANDROID` branching | `Platform/SleepAwareClock.h` is the platform-branched site |
| `design/gdd/run-state-machine.md` | TR-RSM-016 | Death takes priority over simultaneous timer expiry; RUNNING → DEAD evaluated before timer check | Implementation Guideline 5 fixes tick body order; AC-13 is the validation gate |
| `design/gdd/run-state-machine.md` | TR-RSM-017 | Terminal event (DEAD/COMPLETE) takes priority over simultaneous app background event | `OnApplicationWillEnterBackground` early-returns when `CurrentState` ∈ {DEAD, COMPLETE, ABORTED, RESOLVING, IDLE} (GDD Rule 20 + EC-3) |
| `design/gdd/run-state-machine.md` | TR-RSM-018 | RSM idempotent in DEAD/COMPLETE; subsequent collision/timer events in same frame are no-ops | `ReportDeathConfirmed()` early-returns when `CurrentState != RUNNING`; tick body skips timer check after terminal transition |
| `design/gdd/run-state-machine.md` | TR-RSM-019 | Bind to `FCoreDelegates::ApplicationWillEnterBackgroundDelegate` and `ApplicationHasEnteredForegroundDelegate` | `Initialize()` subscribes both via `AddLambda` with `AsyncTask` wrap; `Deinitialize()` removes both via stored handles |
| `design/gdd/run-state-machine.md` | TR-RSM-020 | Do NOT bind to `ApplicationWillDeactivateDelegate` or `ApplicationHasReactivatedDelegate` | Forbidden Pattern `RunStateMachine_ApplicationWillDeactivateDelegate_usage` (code-review gate) |
| `design/gdd/run-state-machine.md` | TR-RSM-021 | `RunSeed:uint64` captured atomically at COUNTDOWN → RUNNING transition | `RunSeed` write site is the COUNTDOWN → RUNNING transition path; immutable thereafter for the run |
| `design/gdd/run-state-machine.md` | TR-RSM-022 | `RunSeed` source is `FPlatformTime::Cycles64()` XOR'd with per-installation salt | COUNTDOWN → RUNNING handler: `RunSeed = FPlatformTime::Cycles64() ^ InstallationSalt;` |
| `design/gdd/run-state-machine.md` | TR-RSM-023 | `RunSeed` exposed as `UPROPERTY(BlueprintReadOnly)` for Wave Spawner + Death Replay | UE 5.7 UHT rejects `uint64 UPROPERTY(BlueprintReadOnly)`. Documented brief deviation: Blueprint exposure via `UFUNCTION(BlueprintPure, Category="RSM\|Run") int64 GetRunSeedBP()` wrapping native `uint64 GetRunSeed()` C++ accessor. Internal storage retains GDD-pinned `uint64`. Semantic intent (Blueprint read-only access) preserved. |
| `design/gdd/run-state-machine.md` | TR-RSM-024 | RSM must tick before downstream systems reading `remaining_time` or `current_state` | Structural Decision 2: `ForceTickNow()` pull primitive — DPC calls as first statement of its tick body; idempotent within frame via `bHasTickedThisFrame` (DPC GDD R6 Rule 3 binding) |
| `design/gdd/run-state-machine.md` | TR-RSM-025 | `TG_PrePhysics` tick group with `AddTickPrerequisite*` for Actor-based RSM | This ADR rejects Actor-based hosting (Alternative 1); GDD Rule 16 alternative (pull primitive) is provided per Structural Decision 2 — TR-RSM-025's branch becomes inapplicable under the chosen hosting |
| `design/gdd/difficulty-phase-controller.md` | TR-DPC-024 | Support `RSM.ForceTickNow()` idempotent entry point before reading `remaining_time` | Key Interfaces declares `void ForceTickNow()` public method; Structural Decision 2 specifies idempotency via `bHasTickedThisFrame`; Implementation Guideline 5 pins the guard-then-set pattern (early-return guard FIRST STATEMENT, flag set SECOND STATEMENT; INT-005 amendment 2026-06-26) |
| `design/gdd/difficulty-phase-controller.md` | DPC GDD R6 Rule 3 FC-1 | `ForceTickNow()` primitive signature `void ForceTickNow()`, no parameters; invokes RSM tick body with `DeltaTime = FApp::GetDeltaTime()` IF not already ticked; idempotent within frame; game-thread only | Key Interfaces declares public method matching signature verbatim; ForceTickNow() skeleton in Initialize() block implements the guard + body invocation |
| `design/gdd/difficulty-phase-controller.md` | DPC GDD R6 Rule 3 FC-2 | `bHasTickedThisFrame` flag set by BOTH regular `Tick` path AND `ForceTickNow` path; reset at frame boundary via `FCoreDelegates::OnEndFrame`; handle stored + Removed in `Deinitialize` | Implementation Guideline 5 pins the guard-then-set pattern in both paths (early-return guard FIRST STATEMENT, flag set SECOND STATEMENT — INT-005 amendment 2026-06-26 closes Risk 4 mitigation code/prose contradiction in the engine-entry path); Initialize() subscribes OnEndFrame with handle stored as `OnEndFrameHandle`; Deinitialize() removes the handle |
| `design/gdd/difficulty-phase-controller.md` | DPC GDD R6 Rule 3 FC-3 | Non-callback invariant: `ForceTickNow()` MUST NOT invoke subscriber callbacks; state-transition visibility deferred 1 frame for callback subscribers; immediate for poll consumers | Structural Decision 2 + Implementation Guideline 5c document the invariant; implementation pattern (queued-event list drained on next regular Tick) called out; pollers (Wave Spawner, Audio Controller) recommended over callbacks |
| `design/gdd/difficulty-phase-controller.md` | DPC GDD R6 Rule 3 FC-4 | External `RequestAbort(reason: EDPCAbortReason)` accepted mid-RUNNING; transitions to ABORTED → IDLE | Key Interfaces declares `void RequestAbort(EDPCAbortReason Reason)` public method; mirrors PAUSED_TIMEOUT_S-exceeded ABORTED entry path; idempotent in terminal states |
| `design/gdd/difficulty-phase-controller.md` | DPC GDD R6 Rule 3 FC-5 | All contract surface (ForceTickNow, RequestAbort) executes on game thread | Key Interfaces signatures document game-thread constraint; ForceTickNow body has `check(IsInGameThread())` first; RequestAbort body MUST add same check (Implementation Guideline 5c implied; not yet stated explicitly — code reviewer enforces) |
| `design/gdd/run-state-machine.md` | TR-RSM-026 | Clock injection enables `FakeTimeSource` test path; `FApp::GetCurrentTime()` never used directly | Forbidden Pattern `RunStateMachine_FApp_GetCurrentTime_usage`; `SetTimeSourceForTesting()` provides the fake-injection seam |
| `design/gdd/run-state-machine.md` | TR-RSM-027 | Pause accumulation formula uses `max(0.0, ...)` guard against backward clock movements | F-3 implementation in `OnApplicationHasEnteredForeground()` body applies `max(0.0, t_resume - t_pause_start)` |
| `design/gdd/run-state-machine.md` | TR-RSM-028 | Remaining time formula clamped to `[0, RUN_DURATION_S]` via `min()`/`max()` | `GetRemainingTime()` implements F-1 with both clamps |
| `design/gdd/run-state-machine.md` | TR-RSM-029 | Wave Spawner must flush all in-flight waves on `OnPausedChanged(is_paused=false)` during RUNNING | Cross-system contract registered as `OnPausedChanged` broadcast carrying `bIsPaused = false` payload; consumed by Wave Spawner per GDD Rule 19 + ADR-0005 line 254 forward contract |
| `design/gdd/run-state-machine.md` | TR-RSM-030 | Collision discards `death_confirmed` events during `resume_grace` window | `bResumeGrace` exposed via `IsResumeGrace()` direct accessor; Collision consumer reads (post-`ForceTickNow()`-return) and gates discards (cross-system contract) |
| `design/gdd/run-state-machine.md` | TR-RSM-031 | Player Movement freezes movement inputs during `resume_grace` window | Same `bResumeGrace` read path consumed by Player Movement (cross-system contract) |
| `design/gdd/run-state-machine.md` | TR-RSM-032 | Wave Spawner holds all new spawning during `resume_grace` window | Same `bResumeGrace` read path consumed by Wave Spawner (cross-system contract) |
| `design/gdd/run-state-machine.md` | TR-RSM-033 | `run_outcome` reset to NONE on RESOLVING → IDLE (after broadcast) and ABORTED → IDLE | Transition path orders: (a) capture outcome for broadcast payload; (b) broadcast `OnStateChanged`; (c) reset `RunOutcome = ERunOutcome::NONE` (matches AC-26 exception clause) |
| `design/gdd/run-state-machine.md` | TR-RSM-034 | Separate re-entrancy guard `bool bBroadcastingPaused` for `OnPausedChanged` | Private `bBroadcastingPaused` bool, independent of `bBroadcastingState` (Implementation Guideline 6 cites GDD Rule 18 explicit constraint) |

## Related

- **ADR-0001**: Palm rejection R_max + `FTouchRadiusBridgePlugin` — establishes the native-plugin pattern; no direct dependency. RSM does not interact with touch input.
- **ADR-0002**: Haptic platform bridge — independent; RSM does not trigger haptics.
- **ADR-0003**: 60Hz drain queue + `FInputSystem` — establishes Seam 1 `IMonotonicClock` whose platform implementations are reused by `FAppTimeSource` per Structural Decision 3.
- **ADR-0004**: R11a dual-grep methodology — applies to this ADR's oracle-site sweep for `RunSeed` / `ForceTickNow` / `bResumeGrace` / `EDPCAbortReason` if future GDD revisions touch these surfaces.
- **ADR-0005**: Wave Spawner `UGameInstanceSubsystem` hosting — direct hosting-pattern precedent. ADR-0005 §line 254–258 cites RSM's `OnPausedChanged` and DPC's `OnPostTickFrameStatePublished` as forward contracts; this ADR closes the RSM half. ADR-0005 SHOULD be amended (Implementation Guideline 3) to register the `Collection.InitializeDependency(URunStateMachineSubsystem::StaticClass())` requirement.
- **ADR-0006**: Pull-Wave instanced renderer — independent; no overlap.
- **ADR-0008 (forthcoming)**: DPC Hosting and `FDPCFrameState` Atomic Snapshot Publication — DPC binds RSM forward contracts decided here (`ForceTickNow`, `RequestAbort`, `EDPCAbortReason`); DPC's own production outer is decided by ADR-0008 per DPC GDD R6 Rule 3 ("OQ-7 ADR MUST specify DPC's production outer"). ADR-0008 produces `OnPostTickFrameStatePublished` consumed by Wave Spawner per ADR-0005 R2a-2.
- **GDD Open Question 2**: ADR for `ERunState + bool is_paused` architecture — closed by this ADR.
- **GDD Open Question 6**: Platform clock source implementation prerequisite — closed by Structural Decision 3 + `Platform/SleepAwareClock.h` reuse with IS Seam 1.
- **GDD Open Question 7**: RSM object type + delegate binding + Android thread safety + re-entrancy guard — all four sub-questions closed by Structural Decisions 1, 5 + Implementation Guideline 6 + Key Interfaces.
- **GDD `design/gdd/run-state-machine.md` Rule 16**: Tick prerequisite mechanism for `UGameInstanceSubsystem + FTickableGameObject` — alternative provided per Structural Decision 2.
- **Seam 1** (`docs/architecture/platform-seam-interfaces.md` lines 31–170): `IMonotonicClock` + `FiOSContinuousTimeClock` + `FAndroidBootTimeClock` + `FFakeMonotonicClock` — shared platform primitives reused by `FAppTimeSource`.

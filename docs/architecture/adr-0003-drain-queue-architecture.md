# ADR-0003: 60Hz Drain Queue Architecture and FInputSystem Class Pattern

## Status
Proposed

## Date
2026-05-15

## Engine Compatibility

| Field | Value |
|-------|-------|
| **Engine** | Unreal Engine 5.7 |
| **Domain** | Input |
| **Knowledge Risk** | HIGH — UE 5.4–5.7 are post-LLM-cutoff (May 2025) |
| **References Consulted** | `docs/engine-reference/unreal/VERSION.md`, `docs/engine-reference/unreal/modules/input.md` |
| **Post-Cutoff APIs Used** | `FSlateApplication::Get().GetPlatformApplication()->SetMessageHandler()`, `FTSTicker::GetCoreTicker().AddTicker()` — verify against UE 5.7 source before implementing |
| **Verification Required** | Confirm `IInputProcessor` Slate registration pattern unchanged in UE 5.7; confirm `FTSTicker` fires on game thread in UE 5.7 mobile |

## ADR Dependencies

| Field | Value |
|-------|-------|
| **Depends On** | ADR-0001 (establishes plugin pattern; FInputSystem follows same non-UObject approach) |
| **Enables** | F-4 collision detection; AC-10, AC-11, AC-16; all IS unit tests that call `DrainTick()` |
| **Blocks** | IS implementation — F-4 `drain_tick_index` comparison, AC-10/11/16, and any AC using `SimulateSameTickTouches()` are unimplementable until this ADR is Accepted |
| **Ordering Note** | Must be Accepted before IS or PM enters the implementation sprint. The `FInputSystem` class pattern (plain C++, not UObject) decided here constrains all IS code. |

## Context

### Problem Statement

The Input System GDD's F-4 collision detection requires that two touch events arriving
within the same 60Hz input tick are treated as simultaneous. Unreal Engine 5.7's
Enhanced Input system processes touch events at the display refresh rate (60Hz, 90Hz,
or 120Hz depending on device). On a 120Hz display, two taps arriving in adjacent display
frames (8.33ms apart) should be treated as simultaneous by the IS, but Enhanced Input
delivers them as distinct events 8.33ms apart — below the 60Hz boundary.

Without a 60Hz-decoupled drain queue:
1. F-4's collision window cannot be enforced uniformly across all display refresh rates.
2. Two contacts arriving in the same "real" 60Hz tick may be processed 8.33ms apart on
   a 120Hz device, appearing as distinct ticks to a timestamp-delta comparison.
3. Timestamp-delta comparisons are vulnerable to OS scheduling jitter: if DrainTick()
   fires 0.5ms late on a loaded device, a legitimately-same-tick pair exceeds
   `T_frame_ms = 16.67ms` and is incorrectly treated as non-simultaneous.

Additionally, `UObject`-derived classes cannot be constructed with `MakeUnique<>` —
they must be created through UE's object factory and memory management subsystem.
Registering a `UObject` as a Slate `IInputProcessor` is a category error. The IS must
be a plain C++ class.

### Requirements

- IS must process touch events at exactly 60Hz, regardless of display refresh rate
- F-4 collision detection must be deterministic and unaffected by OS scheduling jitter
- Both `IInputProcessor` callbacks and `DrainTick()` run on the game thread; the queue decouples 90/120Hz Slate-frame event delivery from the 60Hz FTSTicker drain interval
- IS must be constructable with `MakeUnique<FInputSystem>(...)` — injectable interfaces
  require a non-UObject design
- The drain architecture must be unit-testable via `DrainTick()` direct calls in tests

## Decision

### FInputSystem as Plain C++ IInputProcessor

`FInputSystem` is a plain C++ class (no `UObject`, no `UCLASS`). It registers as a
Slate `IInputProcessor` to receive raw OS touch events before Enhanced Input processes
them. Events are not processed inline in the input callback — they are enqueued into a
thread-safe pending buffer and processed in `DrainTick()`.

### Architecture

```
OS touch event (Slate frame, display refresh rate: 60/90/120Hz)
         │
         ▼
FInputSystem::HandleTouchStartedEvent / HandleTouchEndedEvent   [IInputProcessor, game thread]
         │  Enqueues into PendingEvents (TQueue SPSC, lock-free)
         │  Records: x_native_px, radius_mm (from FTouchRadiusCache — read here,
         │            at enqueue time), is_direct (from bridge), OS_FingerIndex,
         │            event_type
         │
         ▼
PendingEvents  [thread-safe queue, game thread writes at Slate rate, game thread reads at drain rate]
         │
         ▼
FTSTicker::AddTicker(0.0f)  [game thread, fires every game frame — rate-relative, not fixed interval]
         │
         ▼
FInputSystem::DrainTick()   [game thread]
         │  Increments drain_tick_index (uint64, monotonically increasing)
         │  Dequeues all pending events from PendingEvents
         │  Assigns same drain_tick_index to all events dequeued in this call
         │  Evaluates F-1, F-2, F-3, F-4 per event
         │  Dispatches slip-left / slip-right to Player Movement
         │
         ▼
Player Movement  [game thread]
```

### drain_tick_index

`drain_tick_index` is a `uint64` monotonically incrementing counter owned by
`FInputSystem`. It is incremented exactly once at the start of each `DrainTick()` call,
before any events are dequeued.

All OS events dequeued in the same `DrainTick()` call share the same `drain_tick_index`.

**F-4 collision detection (revised from GDD):**

```
COLLISION(drain_tick_index_a, z_a, drain_tick_index_b, z_b)
    = (|drain_tick_index_a − drain_tick_index_b| ≤ 1) ∧ (z_a ≠ z_b)
```

Two contacts are simultaneous if they were processed in the same `DrainTick()` call
(index delta = 0) or in adjacent `DrainTick()` calls (index delta = 1). This replaces
the timestamp-delta approach in the GDD (`|t_a_ms − t_b_ms| ≤ T_frame_ms`).

**Why drain_tick_index instead of timestamp delta:**
- Immune to OS scheduling jitter: if DrainTick fires 0.5ms late, delta is still 0 or 1
- No floating-point comparison at tick boundaries (integer comparison is exact)
- Naturally quantizes the 60Hz invariant: events from adjacent ticks are always "adjacent
  ticks" regardless of wall-clock delta
- Eliminates the `T_frame_ms = 1000.0 / 60.0` fragility (truncation errors, thread scheduling)

### Key Interfaces

```cpp
// FInputSystem.h

class FInputSystem : public IInputProcessor
{
public:
    FInputSystem(
        TUniquePtr<IMonotonicClock>      InClock,
        TUniquePtr<ITouchRadiusProvider> InRadiusProvider,
        TUniquePtr<IRunStateProvider>    InRunStateProvider,
        TUniquePtr<IHapticDispatch>      InHapticDispatch,
        TUniquePtr<IVisualDispatch>      InVisualDispatch,
        TUniquePtr<IBootFlagStore>       InBootFlagStore   // first-run prompt persistence (FIRST_RUN_PROMPT_ENABLED)
    );

    // Called at 60Hz by FTSTicker — processes all pending OS events.
    // This IS the 60Hz drain tick. Exposed for unit tests via direct call.
    void DrainTick();

    // --- IInputProcessor interface ---
    // Called on game thread at Slate's frame rate (display refresh rate).
    // Per-finger touch events on iOS/Android with bUseMouseForTouch=false.
    // HandleMouseButtonDownEvent/Up receive ZERO touch events on mobile — use these instead.
    // Do not process inline — enqueue only. Read FTouchRadiusCache here (enqueue time).
    bool HandleTouchStartedEvent(FSlateApplication& SlateApp,
                                 const FPointerEvent& PointerEvent) override;
    bool HandleTouchEndedEvent(FSlateApplication& SlateApp,
                               const FPointerEvent& PointerEvent) override;

#if !UE_BUILD_SHIPPING
    int32          SimulateTouch(float x_native_px, float radius_mm);
    void           SimulateTouchUp(int32 contact_id);
    TArray<int32>  SimulateSameTickTouches(TArray<TPair<float, float>> ContactsXAndRadius);
#endif

private:
    uint64 drain_tick_index = 0;  // monotonically incremented each DrainTick()

    // Thread-safe SPSC queue: input thread enqueues, game thread dequeues
    TQueue<FPendingTouchEvent, EQueueMode::Spsc> PendingEvents;

    // ... injected interfaces, contact tracking, etc.
};
```

```cpp
// FPendingTouchEvent — enqueued by HandleTouchStartedEvent/HandleTouchEndedEvent,
// dequeued by DrainTick()
struct FPendingTouchEvent
{
    float   x_native_px;
    float   radius_mm;    // Read from FTouchRadiusCache at enqueue time (not at drain time).
                          // -1.0f sentinel if cache had no data for this FingerIndex.
    bool    is_direct;    // Read from FTouchRadiusCache at enqueue time. false = stylus/indirect.
    int32   OS_FingerIndex;
    bool    bIsTouchUp;   // true = touch-up event; false = touch-down
};
```

### FTSTicker Registration

```cpp
// Called once at IS initialization (on game thread):
// Interval = 0.0f: fires every game frame (rate-relative). Using 1.0f/60.0f causes
// double-fire per 33ms frame at 30 FPS — drain_tick_index advances by 2 per frame,
// breaking the delta≤1 collision window. 0.0f is authoritative.
TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
    FTickerDelegate::CreateRaw(this, &FInputSystem::OnTick),
    0.0f  // fire every game frame (rate-relative, not fixed interval)
);

bool FInputSystem::OnTick(float DeltaTime)
{
    DrainTick();
    return true; // keep ticking
}
```

### IInputProcessor Registration

`FInputSystem` does NOT inherit `IInputProcessor` directly. UE5's `TSharedPtr` does not
support custom deleters (unlike `std::shared_ptr`), so the TSharedPtr-with-no-op-deleter
pattern is not available. Instead, a thin non-owning proxy class forwards callbacks to
`FInputSystem`:

```cpp
// FInputSystem.h
class FInputProcessorProxy final : public IInputProcessor
{
    FInputSystem& Owner;
public:
    explicit FInputProcessorProxy(FInputSystem& InOwner) : Owner(InOwner) {}

    // Required: IInputProcessor::Tick is pure virtual in UE5
    // (Engine/Source/Runtime/Slate/Public/Framework/Application/IInputProcessor.h).
    // No-op here: FInputSystem's drain is driven by FTSTicker, not by Slate's per-frame Tick.
    // Do NOT forward to FInputSystem — doing so would double-drive the drain logic.
    virtual void Tick(const float /*DeltaTime*/, FSlateApplication& /*SlateApp*/,
                      TSharedRef<ICursor> /*Cursor*/) override {}

    bool HandleTouchStartedEvent(FSlateApplication& App, const FPointerEvent& Event) override
    { return Owner.HandleTouchStartedEvent(App, Event); }

    bool HandleTouchEndedEvent(FSlateApplication& App, const FPointerEvent& Event) override
    { return Owner.HandleTouchEndedEvent(App, Event); }
};

class FInputSystem  // does NOT inherit IInputProcessor
{
    // ... other members ...

    // Proxy is ref-counted by Slate; FInputSystem must unregister before destruction
    // to ensure Slate drops its TSharedPtr before FInputSystem memory is freed.
    TSharedPtr<FInputProcessorProxy> SlateRegistrationHandle;
};
```

```cpp
// On game thread, after IS construction and before first game frame.
// Slate may not yet be initialized if FSlipstormGameModule loads in an early
// LoadingPhase (PreDefault, PostConfigInit). Guard the registration call:
SlateRegistrationHandle = MakeShared<FInputProcessorProxy>(*this);
if (FSlateApplication::IsInitialized())
{
    FSlateApplication::Get().RegisterInputPreProcessor(SlateRegistrationHandle);
}
else
{
    // Defer registration until Slate is up. Subscribe to FCoreDelegates::OnPostEngineInit
    // (or use a one-shot tick) to retry. Do not Reset the handle here — the proxy must
    // remain alive for the eventual registration.
}
```

**Lifetime requirement:** `FInputSystem` must unregister the proxy in its destructor
before `SlateRegistrationHandle` goes out of scope. Failure results in UAF when Slate
fires an input callback after IS memory is freed.

**Destructor UAF window (RECOMMENDED guard):** In some UE versions
`FSlateApplication::UnregisterInputPreProcessor()` may defer removal to the next Slate
tick. A touch event arriving between `Unregister` and `SlateRegistrationHandle.Reset()`
will hit `FInputProcessorProxy::HandleTouchStartedEvent`, which forwards to `Owner`
(the `FInputSystem` mid-destruction). Mitigation: store `bOwnerValid = true` in
`FInputSystem` ctor; set it `false` at the top of `~FInputSystem()` before the
`Unregister` call; in each processor method, early-return false if `bOwnerValid ==
false`. Verify the deferral behavior in
`Engine/Source/Runtime/Slate/Private/Framework/Application/SlateApplication.cpp` for
the target UE 5.7 version before omitting this guard.

```cpp
// FInputSystem destructor — always call before releasing the unique owner:
FInputSystem::~FInputSystem()
{
    if (FSlateApplication::IsInitialized())
    {
        FSlateApplication::Get().UnregisterInputPreProcessor(SlateRegistrationHandle);
    }
    FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
    SlateRegistrationHandle.Reset();
}
```

### Thread Safety

| Operation | Thread | Mechanism |
|---|---|---|
| `HandleTouchStartedEvent` / `HandleTouchEndedEvent` | Game thread (Slate frame) | Enqueues to `TQueue<Spsc>` — decouples 90/120Hz display-rate delivery from 60Hz drain |
| `DrainTick()` | Game thread (FTSTicker) | Single consumer — dequeues all events accumulated since last tick |
| `FTouchRadiusCache` read | Game thread (inside `HandleTouchStartedEvent`) | Written by `FTouchRadiusBridgePlugin` native callback; **read at enqueue time** (inside the Slate callback, before the event is queued). The radius value is stored in `FPendingTouchEvent.radius_mm` and NOT re-read at drain time. |
| Injected interface calls | Game thread | All interface calls occur inside `DrainTick()`, except `FTouchRadiusCache` which is read at enqueue time |

**Thread model note:** `IInputProcessor` callbacks (`HandleTouchStartedEvent` /
`HandleTouchEndedEvent`) fire on the game thread — Slate's input processing is
game-thread-bound, not on a dedicated input thread. Both the enqueue and dequeue
operations occur on the game thread. The SPSC queue is retained because `IInputProcessor`
fires at the display refresh rate (60/90/120Hz per Slate frame), while `DrainTick()`
fires at the game frame rate (60Hz via FTSTicker). At 120Hz display, Slate delivers up
to 2 touch events per drain tick — the queue buffers them until the next drain. The SPSC
mode remains correct for single-producer/single-consumer even on the same thread;
`SimulateTouch*` test methods bypass the queue entirely.

**Why enqueue-time for FTouchRadiusCache:** The native bridge writes to `FTouchRadiusCache`
at OS touch-event time (the touchesBegan/dispatchTouchEvent callback). By drain time
(the next DrainTick() call), the bridge may have already overwritten the slot for that
FingerIndex with a subsequent touch event. Reading at enqueue time captures the correct
radius for this specific touch event. Reading at drain time could yield a stale or
wrong-contact value. See ADR-0001 §FTouchRadiusCache for the read-time decision and
atomic ordering specification.

### contact_id vs OS FingerIndex

OS `FingerIndex` values (from `FPointerEvent::GetPointerIndex()`) are OS-managed and
may be reused within a session. IS assigns its own monotonically increasing `contact_id`
at the moment each touch-down enters `DrainTick()`:

```cpp
// contact_id assignment in DrainTick() on touch-down:
const int32 contact_id = NextContactId++;  // NextContactId initialized to 0 at construction

// FingerIndex → contact_id mapping for this drain tick:
// IS maintains an active_contacts map: OS_FingerIndex → contact_id
// On touch-down: if OS_FingerIndex not in map → assign new contact_id, add to map
// On touch-up:   remove OS_FingerIndex from map; contact_id is retired (never reused)
```

`IS_TOUCH_RECEIVED` debug event logs both `OS_FingerIndex` and `contact_id`.

**FingerIndex sort verification gate:** Before bridge implementation, verify that UE 5.7
IOSView assigns `GetPointerIndex()` in the same ascending-pointer-address order as
`FTouchRadiusBridgePlugin`. Submit two simultaneous touches with distinct radii; verify
each contact_id receives the correct radius. This integration test is a required
pre-merge gate.

## Owner

`FInputSystem` is owned and constructed by the game module class, `FSlipstormGameModule`,
which inherits `IModuleInterface`:

```cpp
class FSlipstormGameModule : public IModuleInterface
{
public:
    void StartupModule() override
    {
        // FRSMRunStateProvider takes no ctor arg — UGameInstance does not exist
        // yet at StartupModule(); resolved lazily on first GetCurrentState() call.
        // See platform-seam-interfaces.md §FRSMRunStateProvider for the pattern.
        InputSystem = MakeUnique<FInputSystem>(
            MakePlatformClock(),
            MakeUnique<FTouchRadiusCache>(),
            MakeUnique<FRSMRunStateProvider>(),               // no arg — lazy
            MakeUnique<FHapticPlatformPlugin>(),
            MakeUnique<FWidgetVisualDispatch>(/* widget set later */),
            MakeUnique<FLocalStorageBootFlagStore>()
        );
        InputSystem->RegisterWithSlate();   // guarded internally — see §IInputProcessor Registration
    }

    void ShutdownModule() override
    {
        if (InputSystem) { InputSystem->UnregisterFromSlate(); }
        InputSystem.Reset();
    }

private:
    TUniquePtr<FInputSystem> InputSystem;
};
```

**Rationale:** `IModuleInterface` lifetime matches the game module — constructed once
before the first game frame and destroyed on module unload. This avoids `UObject` memory
management (incompatible with `MakeUnique<>`, see Alternative 2) and avoids coupling IS
lifetime to a `UGameInstance` subclass. `FSlipstormGameModule` is the natural
single-owner: no other system constructs `FInputSystem`.

**Rejected alternatives:**
- `UGameInstance` subclass — requires UObject factory construction (see Alternative 2)
- `UGameInstanceSubsystem` — subsystem factory does not support custom constructor args;
  `IInputProcessor` registration still requires a non-UObject proxy regardless

---

## Alternatives Considered

### Alternative 1: Enhanced Input at 60Hz (display tick cap)

- **Description:** Cap display framerate to 60Hz to force Enhanced Input to 60Hz
- **Cons:** Unacceptable visual quality trade-off; affects all game systems, not just input
- **Rejection Reason:** Blunt instrument; fixes the symptom not the cause

### Alternative 2: UObject + UGameInstanceSubsystem

- **Description:** Implement IS as `UGameInstanceSubsystem` (a UObject type)
- **Pros:** Native UE lifecycle management
- **Cons:** `MakeUnique<>` is invalid for UObjects; injectable constructor injection
  pattern requires TUniquePtr construction; Subsystem factory doesn't support custom
  constructor args; `IInputProcessor` registration still requires a non-UObject proxy
- **Rejection Reason:** Category error — UObject memory management is incompatible with
  dependency injection via constructor. Plain C++ class with manual lifetime management
  is the correct pattern for injectable interfaces.

### Alternative 3: Timestamp delta (original F-4 approach)

- **Description:** Compute collision as `|t_a_ms − t_b_ms| ≤ T_frame_ms`
- **Cons:** Vulnerable to OS scheduling jitter (DrainTick fires 0.5ms late →
  legitimate same-tick pair produces delta = 16.67 + 0.5 = 17.17ms, missed);
  floating-point comparison at boundary (16.6667ms) requires careful constant
  definition to avoid rounding errors
- **Rejection Reason:** `drain_tick_index` integer comparison is immune to both issues.

## Consequences

### Positive

- F-4 collision detection is deterministic and jitter-immune across all display rates
- `FInputSystem` constructable with `MakeUnique<>` — full dependency injection
- `DrainTick()` is directly callable in unit tests (no Slate app setup required)
- SPSC queue is lock-free — zero contention between input thread and game thread

### Negative

- IS must manually manage IInputProcessor registration/unregistration with Slate via the proxy
- `FTSTicker` registration must match IS lifetime exactly — leak risk if not unregistered
- `FInputProcessorProxy` is a thin forwarding class with no logic; keep it that way

### Neutral

- contact_id is an IS concept; OS FingerIndex is the platform concept; mapping is internal to IS

## Risks

| Risk | Probability | Impact | Mitigation |
|---|---|---|---|
| FTSTicker fires off-game-thread in UE 5.7 mobile | Low | High | Verify with UE source before implementation; add thread assertion in DrainTick() debug builds |
| IInputProcessor API changed in UE 5.7 | Low | Medium | Verify against `docs/engine-reference/unreal/modules/input.md`; flag as post-cutoff verification |
| SPSC queue overflow (events arrive faster than 60Hz drains) | Very Low | Medium | Queue is unbounded by default in TQueue; if profiling shows unbounded growth, add bounded variant with drop-oldest policy |

## Validation Criteria

- [ ] `FTSTicker` drain rate matches game frame rate: with interval=0.0f, DrainTick() fires once per game frame. Verify on iOS min-spec at 60 FPS (≈60 calls/sec); verify on Android min-spec at 30 FPS (≈30 calls/sec). `drain_tick_index` must advance by exactly 1 per frame.
- [ ] Two touches injected via `SimulateSameTickTouches()` produce `drain_tick_index_a == drain_tick_index_b` in the IS debug log
- [ ] Two touches injected in adjacent `DrainTick()` calls produce `|drain_tick_index_a − drain_tick_index_b| == 1`
- [ ] `FInputSystem` constructs and destructs cleanly without UObject registry involvement
- [ ] Unregistering from Slate on IS destruction produces no UE assertion in debug builds

## GDD Requirements Addressed

| GDD Document | System | Requirement | How This ADR Satisfies It |
|-------------|--------|-------------|--------------------------|
| `design/gdd/input-system.md` | Input System | F-4: two touches in same 60Hz tick treated as simultaneous | `drain_tick_index` integer comparison replaces timestamp-delta; immune to jitter |
| `design/gdd/input-system.md` | Input System | INPUT_TICK_RATE = 60Hz cap on all devices | FTSTicker interval=0.0f fires once per game frame; frame rate capped at 60 FPS by engine |
| `design/gdd/input-system.md` | Input System | AC-10, AC-11: same-frame collision tests | `SimulateSameTickTouches()` puts contacts in same DrainTick() call; deterministic |
| `design/gdd/input-system.md` | Input System | AC-16: 60Hz tick cap on 120Hz device | FTSTicker interval=0.0f; game frame rate capped at 60 FPS; `drain_tick_index` delta between adjacent ticks is always 1 |

## Related

- ADR-0001: establishes FTouchRadiusBridgePlugin native-plugin pattern; FInputSystem follows same non-UObject approach
- `docs/architecture/platform-seam-interfaces.md`: defines IMonotonicClock, SimulateTouch seam, IRunStateProvider
- `docs/architecture/visual-dispatch-contract.md`: defines IVisualDispatch, the fifth injectable interface

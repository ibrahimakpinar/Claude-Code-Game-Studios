# Visual Dispatch Contract: IVisualDispatch

**Last Updated:** 2026-06-03 (re-review 23: ContactRestingLeft/Right reclassified persistent→transient 50ms flash; Clear() now always an error; ClearContactRestingLeft/Right removed from UInputFeedbackOverlay contract)
**Relates to:** `design/gdd/input-system.md`, `docs/architecture/adr-0002-haptic-platform-bridge.md`
**Required Before:** Input System implementation sprint; IS acceptance criteria AC-19b, AC-19c

---

## Overview

The Input System owns two visual events that bypass `IHapticDispatch`: the NONE-tier
R-3 same-frame collision desaturation pulse, and the VISUAL-tier palm/stylus rejection
micro-flash. Both are IS direct visual responsibilities — not haptic fallbacks routed
through the haptic bridge.

`IVisualDispatch` is the fifth injectable interface provided to `FInputSystem` at
construction. Like the other four seams (`IMonotonicClock`, `ITouchRadiusProvider`,
`IRunStateProvider`, `IHapticDispatch`), it is injected once at construction and never
swapped at runtime. In test builds, `FVisualDispatchStub` records calls without touching
any widget; in shipping builds, `FWidgetVisualDispatch` forwards to the
`UInputFeedbackOverlay` widget.

---

## Visual Events

```cpp
// EVisualEvent.h
enum class EVisualEvent : uint8
{
    // NONE-tier R-3 same-frame collision: gray dual-zone-edge desaturation pulse.
    // Fired when two opposite-zone contacts arrive in the same drain tick and
    // HAPTIC_CAPABILITY == NONE. Suppressed in DEAD, RESOLVING, ABORTED, and COUNTDOWN
    // states. Fires in IDLE, RUNNING, and COMPLETE.
    // Visual spec: design/ux/input-feedback.md §NONE-Tier Visual
    R3CollisionDesaturation = 0,

    // VISUAL-tier palm/stylus rejection: warm gray center micro-flash.
    // Fired when a contact fails F-2 (r_mm > R_max) and HAPTIC_CAPABILITY == VISUAL.
    // Not fired on AUDIO or NONE tiers — palm rejection is typically unnoticed.
    // Visual spec: design/ux/input-feedback.md §Palm/Stylus Rejection Signal
    InputRejectedMicroFlash = 1,

    // NONE-tier ContactResting hold-deactivation signal, LEFT zone (Pillar 5 requirement).
    // Transient (50ms flash, self-clears) — no Clear() call needed or valid.
    // Fired when the 180ms cancel timer expires for a LEFT-zone contact and
    // HAPTIC_CAPABILITY == NONE. IS selects Left or Right based on the contact's
    // originating zone. IHapticDispatch::Fire(ContactResting) is a no-op on NONE tier;
    // these visuals provide the mandatory Pillar 5 hold-deactivation signal.
    // Suppressed in DEAD, RESOLVING, and ABORTED. Fires in IDLE, RUNNING, and COMPLETE.
    // Effectively suppressed in COUNTDOWN via cancel-timer guard (IS-21-A1 tracking reset).
    // Visual spec: design/ux/input-feedback.md §NONE-Tier Visual
    ContactRestingLeft = 2,

    // NONE-tier ContactResting hold-deactivation signal, RIGHT zone.
    // Transient (50ms flash, self-clears) — same conditions as ContactRestingLeft.
    // Visual spec: design/ux/input-feedback.md §NONE-Tier Visual
    ContactRestingRight = 3,

    // NONE-tier dead-band contact flash (transient). Fired when a touch lands in the
    // 8mm exclusion band, HAPTIC_CAPABILITY == NONE, and DEAD_BAND_FEEDBACK_ENABLED == true.
    // Replaces IHapticDispatch::Fire(DeadBandContact) on NONE tier.
    // Suppressed in DEAD, RESOLVING, and ABORTED. Fires in IDLE, COUNTDOWN, RUNNING, COMPLETE.
    // Visual spec: design/ux/input-feedback.md §NONE-Tier Dead-Band Visual
    DeadBandContactFlash = 4,
};
```

---

## Interface

```cpp
// IVisualDispatch.h

class IVisualDispatch
{
public:
    // Fire the visual event on the input feedback overlay.
    // MUST be called from the game thread only.
    // Implementations must not call this from input thread, OS callback, or FTSTicker.
    // Non-blocking: implementation queues an animation frame, returns immediately.
    virtual void Fire(EVisualEvent Event) = 0;

    // Clear a persistent visual event.
    // After re-review 23, all EVisualEvent values are transient — calling Clear()
    // for any currently-defined event is a programming error. IS never calls Clear().
    // This method is preserved in the interface for forward compatibility (future
    // persistent events may be added). Implementations must log an error and no-op
    // for all current EVisualEvent values.
    // MUST be called from the game thread only.
    virtual void Clear(EVisualEvent Event) = 0;

    virtual ~IVisualDispatch() = default;
};
```

---

## Production Implementation

```cpp
// FWidgetVisualDispatch.h

class FWidgetVisualDispatch final : public IVisualDispatch
{
    TWeakObjectPtr<UInputFeedbackOverlay> Overlay;

public:
    explicit FWidgetVisualDispatch(UInputFeedbackOverlay* InOverlay)
        : Overlay(InOverlay)
    {
        check(InOverlay != nullptr);
    }

    void Fire(EVisualEvent Event) override
    {
        if (!Overlay.IsValid())
        {
            UE_LOG(LogInputSystem, Warning,
                TEXT("IVisualDispatch::Fire called with invalid Overlay — skipping"));
            return;
        }
        switch (Event)
        {
        case EVisualEvent::R3CollisionDesaturation:
            Overlay->PlayR3CollisionDesaturation();
            break;
        case EVisualEvent::InputRejectedMicroFlash:
            Overlay->PlayInputRejectedMicroFlash();
            break;
        case EVisualEvent::ContactRestingLeft:
            Overlay->PlayContactRestingLeft();
            break;
        case EVisualEvent::ContactRestingRight:
            Overlay->PlayContactRestingRight();
            break;
        case EVisualEvent::DeadBandContactFlash:
            Overlay->PlayDeadBandContactFlash();
            break;
        }
    }

    void Clear(EVisualEvent Event) override
    {
        if (!Overlay.IsValid())
        {
            UE_LOG(LogInputSystem, Warning,
                TEXT("IVisualDispatch::Clear called with invalid Overlay — skipping"));
            return;
        }
        switch (Event)
        {
        default:
            // All EVisualEvent values are transient after re-review 23 — Clear() is always an error
            UE_LOG(LogInputSystem, Error,
                TEXT("IVisualDispatch::Clear called on transient EVisualEvent %d — no persistent events exist"), (int32)Event);
            break;
        }
    }
};
```

### `UInputFeedbackOverlay` contract

`UInputFeedbackOverlay` is a `UUserWidget` subclass owned by the game's UI layer — not
by IS. IS holds only a `TWeakObjectPtr<UInputFeedbackOverlay>`; it does not create,
destroy, or manage the widget's lifetime.

Required `UFUNCTION` methods that IS calls via `FWidgetVisualDispatch`:

```cpp
// UInputFeedbackOverlay.h (UUserWidget subclass — authored by UI team)
UFUNCTION(BlueprintImplementableEvent, Category = "InputFeedback")
void PlayR3CollisionDesaturation();

UFUNCTION(BlueprintImplementableEvent, Category = "InputFeedback")
void PlayInputRejectedMicroFlash();

UFUNCTION(BlueprintImplementableEvent, Category = "InputFeedback")
void PlayContactRestingLeft();    // NONE-tier hold-deactivation, LEFT zone — 50ms transient flash, self-clears

UFUNCTION(BlueprintImplementableEvent, Category = "InputFeedback")
void PlayContactRestingRight();   // NONE-tier hold-deactivation, RIGHT zone — 50ms transient flash, self-clears

UFUNCTION(BlueprintImplementableEvent, Category = "InputFeedback")
void PlayDeadBandContactFlash();  // NONE-tier dead-band acknowledgement; transient (no Clear needed)
```

Each Play* method starts the corresponding animation (or Blueprint event) as specified
in `design/ux/input-feedback.md`. All events are transient — animations self-terminate
after their specified duration. IS does not call `Clear()` for any current event, and no
Clear* methods are defined in this contract. The overlay handles its own animation queue
and blending.

**Lifetime requirement:** `UInputFeedbackOverlay` must be valid for the entire
lifetime of `FInputSystem`. If the overlay is destroyed while IS is alive:
`FWidgetVisualDispatch` detects this via `TWeakObjectPtr::IsValid()` and logs a warning
rather than crashing. This is a defensive guard; destroying the overlay while IS is
active is a programming error and must not happen in production.

---

## Test Stub

```cpp
// FVisualDispatchStub.h  (#if !UE_BUILD_SHIPPING)

class FVisualDispatchStub final : public IVisualDispatch
{
    bool bFired             = false;
    EVisualEvent LastEvent  = EVisualEvent::R3CollisionDesaturation;
    int32 FireCount         = 0;
    bool bCleared           = false;
    EVisualEvent LastClearedEvent = EVisualEvent::R3CollisionDesaturation;
    int32 ClearCount        = 0;

    // Per-event fire and clear counts — populated unconditionally (no clock required).
    // Use WasFired(Event) / GetClearCount(Event) for event-specific assertions.
    TMap<EVisualEvent, int32> FireCountsMap;
    TMap<EVisualEvent, int32> ClearCountsMap;

    // Optional clock for rate-window queries (GetFireCountInWindowMs).
    // If null, GetFireCountInWindowMs() always returns 0 — don't pass null when
    // writing rate-window tests. Not needed for WasFired(Event) or GetClearCount(Event).
    TSharedPtr<IMonotonicClock> Clock;

    struct FFireRecord
    {
        EVisualEvent Event;
        int64        TimestampMs;
    };
    TArray<FFireRecord> FireLog;  // populated only when Clock is non-null

public:
    // Default constructor — rate-window queries unavailable
    FVisualDispatchStub() = default;

    // Constructor with clock — enables GetFireCountInWindowMs()
    explicit FVisualDispatchStub(TSharedPtr<IMonotonicClock> InClock)
        : Clock(MoveTemp(InClock))
    {}

    void Fire(EVisualEvent Event) override
    {
        bFired    = true;
        LastEvent = Event;
        ++FireCount;
        FireCountsMap.FindOrAdd(Event)++;
        if (Clock)
        {
            FireLog.Add({ Event, Clock->NowMs() });
        }
    }

    void Clear(EVisualEvent Event) override
    {
        bCleared         = true;
        LastClearedEvent = Event;
        ++ClearCount;
        ClearCountsMap.FindOrAdd(Event)++;
    }

    // Test inspection methods — aggregate (any event)
    bool         WasFired()           const { return bFired; }
    EVisualEvent GetLastEvent()        const { return LastEvent; }
    int32        GetFireCount()        const { return FireCount; }
    bool         WasCleared()          const { return bCleared; }
    EVisualEvent GetLastClearedEvent() const { return LastClearedEvent; }
    int32        GetClearCount()       const { return ClearCount; }

    // Per-event overloads — use for event-specific assertions (AC-NONE-TIER-DB-01/02, AC-FORCE-EXPIRE-CLEAR)
    bool  WasFired(EVisualEvent Event)      const { const int32* P = FireCountsMap.Find(Event); return P && *P > 0; }
    int32 GetClearCount(EVisualEvent Event) const { const int32* P = ClearCountsMap.Find(Event); return P ? *P : 0; }

    // Returns the number of Fire(Event) calls recorded within the last WindowMs
    // milliseconds of stub-clock time. Requires clock injection — returns 0 if no
    // clock was provided. Use for IS firing-frequency assertions (not for widget
    // rate-limit testing — see AC-WCAG-01 for why FWidgetVisualDispatch must be
    // tested directly with a mock overlay instead).
    int32 GetFireCountInWindowMs(EVisualEvent Event, int64 WindowMs) const
    {
        if (!Clock) return 0;
        const int64 NowMs    = Clock->NowMs();
        const int64 CutoffMs = NowMs - WindowMs;
        int32 Count = 0;
        for (const FFireRecord& Record : FireLog)
        {
            if (Record.Event == Event && Record.TimestampMs >= CutoffMs)
            {
                ++Count;
            }
        }
        return Count;
    }

    void Reset()
    {
        bFired            = false;
        FireCount         = 0;
        LastEvent         = EVisualEvent::R3CollisionDesaturation;
        bCleared          = false;
        ClearCount        = 0;
        LastClearedEvent  = EVisualEvent::R3CollisionDesaturation;
        FireLog.Reset();
        FireCountsMap.Reset();
        ClearCountsMap.Reset();
    }
};
```

### Example: AC-19b test (NONE-tier R-3 visual fires in RUNNING)

```cpp
// tests/unit/input-system/test_none_tier_visual.cpp

// MakeUnique<> + raw observer pointers (TUniquePtr ownership model)
auto ClockOwned      = MakeUnique<FFakeMonotonicClock>();      auto* Clock      = ClockOwned.Get();
auto RadiusOwned     = MakeUnique<FTouchRadiusProviderStub>(); auto* RadiusStub = RadiusOwned.Get();
auto HapticOwned     = MakeUnique<FHapticDispatchStub>();      auto* HapticStub = HapticOwned.Get();
auto VisualOwned     = MakeUnique<FVisualDispatchStub>();      auto* VisualStub = VisualOwned.Get();
auto RunStateOwned   = MakeUnique<FRunStateProviderStub>();    auto* RunStateStub = RunStateOwned.Get();

HapticStub->SetCapability(EHapticCapability::NONE);
RunStateStub->SetState(ERunState::RUNNING);
RadiusStub->SetRadius(1.5f); // under R_max — passes PALM_PASS

auto IS = MakeUnique<FInputSystem>(
    MoveTemp(ClockOwned), MoveTemp(RadiusOwned), MoveTemp(RunStateOwned),
    MoveTemp(HapticOwned), MoveTemp(VisualOwned)
);

// Inject two opposite-zone contacts in the same drain tick
IS->SimulateSameTickTouches({{ 200.0f, 1.5f }, { 900.0f, 1.5f }});

// Assert
check(VisualStub->WasFired());
check(VisualStub->GetLastEvent() == EVisualEvent::R3CollisionDesaturation);
// Haptic stub must NOT have fired (NONE tier)
check(!HapticStub->WasFired(EHapticEvent::R3Collision));
```

### Example: AC-19c test (NONE-tier R-3 visual suppressed in DEAD/RESOLVING)

```cpp
for (ERunState SuppressedState : {
    ERunState::DEAD,
    ERunState::RESOLVING,
    ERunState::ABORTED,
    ERunState::COUNTDOWN
})
{
    VisualStub->Reset();
    RunStateStub->SetState(SuppressedState);

    IS->SimulateSameTickTouches({{ 200.0f, 1.5f }, { 900.0f, 1.5f }});

    // Visual must NOT fire in any of the four suppressed states
    check(!VisualStub->WasFired());
}
```

---

## Thread Safety Contract

- `IVisualDispatch::Fire()` is game-thread-only. `FInputSystem::DrainTick()` runs on
  the game thread (guaranteed by `FTSTicker`). This contract is satisfied automatically
  as long as `DrainTick()` is not called from any other thread.
- `FVisualDispatchStub::Fire()` is also game-thread-only. It does not use any lock.
  Calling it from multiple threads in tests is a test authoring error.
- `FWidgetVisualDispatch` calls `UObject` methods — UObject methods must not be called
  from outside the game thread. This invariant is maintained by the FTSTicker contract.

---

## WCAG 2.3.1 Compliance Note

Both visual events use brief flashes (80–150ms) and low opacity (10–45%). WCAG 2.3.1
prohibits content that flashes more than 3 times per second AND covers more than 25%
of the screen. IS fires at natural cadence (at most once per drain tick per event type);
IS does not implement WCAG rate-limiting. **Rate-limiting is owned by `FWidgetVisualDispatch`**
— it must enforce the ≤3 flashes/sec threshold per event type before forwarding calls to
the overlay. In practice, R-3 collision requires simultaneous opposite-zone contacts (a
rare gesture) and the VISUAL-tier micro-flash covers only the center dead-band strip
(~8mm wide, <2% of screen area), so the threshold is unlikely to be approached. However,
`FWidgetVisualDispatch` must implement the guard unconditionally.

UI team must verify at implementation: neither event exceeds the 3-flash/s threshold at
any expected usage frequency, using the target device at full brightness.

---

## Wiring in FInputSystem Constructor

The wiring below is **pseudocode** — exact construction site and HUD widget access
pattern are TBD; do not copy-paste as compilable code.

**OQ-7 resolved (2026-06-01):** RSM type is `URunStateMachineSubsystem : UGameInstanceSubsystem`.
`FRSMRunStateProvider` holds `TWeakObjectPtr<URunStateMachineSubsystem>`; resolved via
`UGameInstance::GetSubsystem<URunStateMachineSubsystem>()`.

**GC-safety contract:** Any UObject pointer held by IS must be wrapped in
`TWeakObjectPtr<T>` and checked with `.IsValid()` at every read call site. Raw UObject
pointers in long-lived non-UObject owners (such as `FInputSystem`) will be silently
invalidated by the GC without a crash guard.

```
// Pseudocode — shipping build (RSM OQ-7 resolved)
UGameInstance* GI = GetWorld()->GetGameInstance();
auto IS = MakeUnique<FInputSystem>(
    MakePlatformClock(),
    MakeUnique<FTouchRadiusCache>(),
    MakeUnique<FRSMRunStateProvider>(
        GI->GetSubsystem<URunStateMachineSubsystem>()
        // Exact construction site TBD — must occur after GameInstance subsystems initialize
    ),
    MakeUnique<FHapticPlatformPlugin>(),
    MakeUnique<FWidgetVisualDispatch>(InputFeedbackOverlayWidget)
    // InputFeedbackOverlayWidget: obtained from the active HUD — exact site TBD per UI subsystem init order
);
```

```cpp
// Test build (compilable) — MakeUnique<> + raw observer pointers
auto VisualStubOwned = MakeUnique<FVisualDispatchStub>(); auto* VisualStub = VisualStubOwned.Get();
// ... (other stubs similarly) ...
auto IS = MakeUnique<FInputSystem>(
    MoveTemp(ClockStub), MoveTemp(RadiusStub), MoveTemp(RunStateStub),
    MoveTemp(HapticStub), MoveTemp(VisualStubOwned)
);
```

## GDD ACs Unblocked by This Document

| AC | What was blocking it | What this doc provides |
|---|---|---|
| AC-19b | No `IVisualDispatch` contract — `FVisualDispatchStub` undefined | `FVisualDispatchStub.WasFired()` and `GetLastEvent()` |
| AC-19c | Same | `FVisualDispatchStub.Reset()` for per-state isolation; now covers all 4 suppression states (DEAD, RESOLVING, ABORTED, COUNTDOWN) |
| AC-06d | No VISUAL-tier InputRejected AC existed | `EVisualEvent::InputRejectedMicroFlash` now in enum; `FVisualDispatchStub` inspection methods usable |
| AC-21-NONE-ContactResting | No EVisualEvent for ContactResting existed; no zone discrimination | `EVisualEvent::ContactRestingLeft = 2` and `ContactRestingRight = 3` now defined; `PlayContactRestingLeft()` and `PlayContactRestingRight()` in overlay contract; zone verified via `FVisualDispatchStub.GetLastEvent()` |
| AC-21-NONE-ContactRestingFlash (supersedes AC-21-NONE-ContactRestingClear) | ContactResting reclassified persistent→transient in re-review 23; Clear() must NOT be called; overlay no longer has Clear* methods | `FVisualDispatchStub.WasFired(ContactRestingLeft/Right) == true` and `WasCleared() == false` via existing stub inspection methods |
| AC-WCAG-01 | No rate-window query on stub — integration test required a separate spy with no defined interface | `FVisualDispatchStub(TSharedPtr<IMonotonicClock>)` constructor + `GetFireCountInWindowMs(Event, WindowMs)` — unit-level rate-window assertions now possible alongside the integration test |
| AC-NONE-TIER-DB-01 | No `EVisualEvent::DeadBandContactFlash` existed; `WasFired(EVisualEvent)` overload missing | `DeadBandContactFlash = 4` added to enum; `PlayDeadBandContactFlash()` added to overlay contract; `FVisualDispatchStub.WasFired(EVisualEvent::DeadBandContactFlash)` now available via per-event overload |
| AC-NONE-TIER-DB-02 | Same; `WasFired(EVisualEvent)` missing | Same — `WasFired(EVisualEvent::DeadBandContactFlash) == false` confirms visual suppressed when knob off |
| AC-FORCE-EXPIRE-CLEAR | `GetClearCount(EVisualEvent)` per-event overload missing — `ClearCountsMap` did not exist | `FVisualDispatchStub.GetClearCount(EVisualEvent::ContactRestingLeft/Right)` now available via `ClearCountsMap` |

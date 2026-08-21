# Platform Seam Interfaces (Multi-System Catalog)

**Last Updated:** 2026-06-05
**Relates to:** `design/gdd/input-system.md`, `design/gdd/difficulty-phase-controller.md`, `docs/architecture/adr-0001-palm-rejection-rmax-calibration.md`
**Required Before:** Input System implementation sprint; DPC implementation sprint. IS acceptance criteria AC-04, AC-07, AC-08, AC-09, AC-10, AC-11, AC-12, AC-13, AC-16, AC-19; DPC acceptance criteria AC-12, AC-13, AC-14b, AC-14c, AC-21, AC-NAN-GUARD, AC-RUN-DURATION-ZERO.

---

## Overview

This document catalogs every injectable seam used by SLIPSTORM systems to isolate
platform-specific behaviour from unit-tested logic. Two systems currently contribute
seams here:

- **Input System** (Seams 1–6): time source, touch event delivery path, RSM state read,
  haptic dispatch, visual dispatch, and boot-flag store. See `design/gdd/input-system.md`.
- **Difficulty & Phase Controller** (Seams 7–8): RSM time/state injection for DPC tests,
  and a consumer-side subscription stub for Rule 17 mid-run subscription tests. See
  `design/gdd/difficulty-phase-controller.md`.

When a new system needs an injectable seam, append it as a new Seam N (do not renumber
existing seams — downstream stories cite seam numbers).

Each seam has three forms:
- **Interface** — the abstract contract the consuming system depends on (compiled everywhere)
- **Production implementation** — the platform-specific or runtime-resolved concrete class (compiled for shipping)
- **Test stub** — a controllable fake (compiled in non-shipping builds)

---

## Seam 1: `IMonotonicClock`

### Why this seam exists

The Input System GDD (F-3, F-4) declares `t_elapsed`, `t_a`, and `t_b` in units of
milliseconds. The only correct platform clock for the cancel timer is one that
continues counting across app suspension (to detect held fingers through a phone
call resume). `FPlatformTime::Seconds()` returns **seconds** (not milliseconds) and
uses `mach_absolute_time()` on iOS, which **pauses during background suspension** —
incorrect for both reasons. This injectable interface fixes both issues.

**Unit mismatch root cause:** If the IS stores `t_start = FPlatformTime::Seconds()`
and later evaluates `t_elapsed = current_time - t_start`, the result is in seconds.
Comparing against `CANCEL_TIMER_MS = 180.0` then requires 180 seconds to elapse, not
180ms. The cancel timer never fires under a naïve implementation. `IMonotonicClock`
returns ms directly, eliminating this class of error.

### Interface

```cpp
// IMonotonicClock.h
class IMonotonicClock
{
public:
    // Returns elapsed time in milliseconds since an arbitrary but consistent epoch.
    // Guaranteed to be monotonically non-decreasing.
    // MUST continue counting during app background suspension.
    virtual double NowMs() const = 0;
    virtual ~IMonotonicClock() = default;
};
```

### Production Implementation — iOS

```cpp
// FiOSContinuousTimeClock.h / .mm (ObjC++ compilation unit)
//
// Uses mach_continuous_time(), which continues counting during suspend.
// Do NOT use mach_absolute_time() (used by FPlatformTime::Seconds()) —
// it pauses when the device sleeps, producing negative t_elapsed on resume.

#include <mach/mach_time.h>

class FiOSContinuousTimeClock final : public IMonotonicClock
{
    mach_timebase_info_data_t TimebaseInfo;  // cached in constructor — mach_timebase_info() is not free
public:
    FiOSContinuousTimeClock()
    {
        mach_timebase_info(&TimebaseInfo);
    }

    double NowMs() const override
    {
        const uint64_t Ticks = mach_continuous_time();
        // Convert ticks → nanoseconds → milliseconds
        return static_cast<double>(Ticks) * TimebaseInfo.numer / TimebaseInfo.denom / 1'000'000.0;
    }
};
```

### Production Implementation — Android

```cpp
// FAndroidBootTimeClock.h
//
// Uses CLOCK_BOOTTIME, which continues counting during deep sleep.
// Do NOT use CLOCK_MONOTONIC — it pauses during Android deep sleep,
// producing incorrect t_elapsed for contacts held across screen-off events.

#include <time.h>

class FAndroidBootTimeClock final : public IMonotonicClock
{
public:
    double NowMs() const override
    {
        struct timespec Ts;
        clock_gettime(CLOCK_BOOTTIME, &Ts);
        return static_cast<double>(Ts.tv_sec) * 1000.0
             + static_cast<double>(Ts.tv_nsec) / 1'000'000.0;
    }
};
```

### Test Stub

```cpp
// FFakeMonotonicClock.h  (#if !UE_BUILD_SHIPPING)
class FFakeMonotonicClock final : public IMonotonicClock
{
    double CurrentMs = 0.0;
public:
    double NowMs() const override { return CurrentMs; }

    // Test control methods
    void AdvanceMs(double DeltaMs) { CurrentMs += DeltaMs; }
    void SetMs(double Ms)          { CurrentMs = Ms; }
    void Reset()                   { CurrentMs = 0.0; }
};
```

### Wiring in the Input System

```cpp
// IS constructor — injected at construction; never swapped after construction
FInputSystem::FInputSystem(TUniquePtr<IMonotonicClock> InClock)
    : Clock(MoveTemp(InClock))
{}

// Cancel timer usage (F-3)
const double t_start_ms = Clock->NowMs();  // store at dispatch

// At drain tick evaluation:
const double t_elapsed_ms = Clock->NowMs() - t_start_ms;
const bool cancel = (t_elapsed_ms > CANCEL_TIMER_MS) && !touch_up_received;

// Collision timestamp (F-4)
const double t_a_ms = Clock->NowMs();  // assigned at 60Hz processing boundary
```

### Default construction (shipping — platform-selected at build time)

```cpp
TUniquePtr<IMonotonicClock> MakePlatformClock()
{
#if PLATFORM_IOS
    return MakeUnique<FiOSContinuousTimeClock>();
#elif PLATFORM_ANDROID
    return MakeUnique<FAndroidBootTimeClock>();
#else
    // Editor / non-mobile fallback — FPlatformTime::Seconds() × 1000 is acceptable
    // in editor where suspend behavior is irrelevant
    struct FEditorClock final : public IMonotonicClock {
        double NowMs() const override { return FPlatformTime::Seconds() * 1000.0; }
    };
    return MakeUnique<FEditorClock>();
#endif
}
```

---

## Seam 2: `SimulateTouch` Test Seam

### Why this seam exists

Multiple IS acceptance criteria require programmatic touch injection:
- **AC-04**: Band-edge zone classification — human touch cannot target a specific pixel
- **AC-07/AC-08**: Cancel timer — requires holding a contact for exactly 180ms+
- **AC-10/AC-11**: Same-frame collision — requires simultaneous or precisely timed multi-touch
- **AC-12**: Three rapid left-zone taps with distinct contact IDs — requires controlled timing
- **AC-16**: 60Hz tick cap verification — requires measuring delta_ms between injected events

These ACs are automated unit tests, not manual test cases. `SimulateTouch` provides
the injection path that bypasses OS touch delivery while routing through the same
IS internal processing logic as real touches.

### Contract

```cpp
// Exposed in non-shipping builds on FInputSystem
// (#if !UE_BUILD_SHIPPING)

class FInputSystem /* : ... */
{
public:

#if !UE_BUILD_SHIPPING

    // Injects a synthetic touch-down through the full IS processing path.
    // - x_native_px: horizontal coordinate in native device pixels (0 = left edge)
    // - radius_mm: simulated contact radius in mm (passed to ITouchRadiusProvider stub)
    // Returns: the contact_id assigned to this synthetic contact (for SimulateTouchUp)
    //
    // The call goes through: PALM_PASS(radius_mm) → ZONE(x_native_px) → collision check
    // → cancel timer start → debug log. Identical to the real touch path except event
    // delivery is synchronous (no OS event queue) and uses the injected clock.
    int32 SimulateTouch(float x_native_px, float radius_mm);

    // Injects a synthetic touch-up for the given contact_id.
    // Resolves the cancel timer and removes the contact from the active tracking set.
    // No-op if contact_id is not in the active tracking set.
    void SimulateTouchUp(int32 contact_id);

    // Injects multiple touch-downs at the same clock snapshot (simulates same-tick arrival).
    // All contacts share the same t_a_ms (sampled once before the batch is processed).
    // Used to test same-frame collision detection (AC-10, AC-11, AC-16).
    TArray<int32> SimulateSameTickTouches(TArray<TPair<float, float>> ContactsXAndRadius);

#endif

private:
    // NextContactId is the single shared counter for all contact IDs (real and synthetic).
    // SimulateTouch() uses NextContactId++ — same namespace as real touch-down processing.
    // Do NOT introduce a separate NextSyntheticContactId counter: that would reintroduce
    // the namespace collision bug (a synthetic ID could alias a real ID already in tracking_set).
};
```

### Invariants

- `SimulateTouch` is synchronous — it processes the event before returning. The IS debug
  log must contain the relevant entries (`IS_TOUCH_RECEIVED`, `IS_SLIP_DISPATCHED`, etc.)
  immediately after the call returns.
- `SimulateTouch` uses the injected `IMonotonicClock` for timestamps — tests advance
  `FFakeMonotonicClock` before calling `SimulateTouch` to control t_a/t_b values.
- `SimulateTouch` uses the injected `ITouchRadiusProvider` for radius data — tests
  set `FTouchRadiusProviderStub.SetRadius(radius_mm)` before the call.
- The `SimulateTouch` path is compiled out in `UE_BUILD_SHIPPING`. The IS must not expose
  this surface in shipping binaries.

### Example: AC-04 band-edge test

```cpp
// tests/unit/input-system/test_zone_classification.cpp

// Setup — use MakeUnique<> + raw observer pointers (TUniquePtr ownership model)
auto ClockOwned = MakeUnique<FFakeMonotonicClock>();
auto* Clock = ClockOwned.Get();
auto StubOwned = MakeUnique<FTouchRadiusProviderStub>();
auto* Stub = StubOwned.Get();
Stub->SetRadius(1.5f); // well under R_max — passes PALM_PASS
auto IS = MakeUnique<FInputSystem>(MoveTemp(ClockOwned), MoveTemp(StubOwned), /* RunStateProvider */ ...);

const float W = 1080.0f;
const float B = 145.0f;  // at 460 PPI, 8mm exclusion band (460÷25.4=18.11 px/mm; 8×18.11=144.9≈145)
const float LeftEdge  = W / 2.0f - B / 2.0f; // = 467.5
const float RightEdge = W / 2.0f + B / 2.0f; // = 612.5

// AC-04a: left band edge → LEFT
IS->SimulateTouch(LeftEdge, 1.5f);
// Assert: debug log contains IS_ZONE_CLASSIFIED zone=LEFT, IS_SLIP_DISPATCHED direction=left

// AC-04b: right band edge → RIGHT
IS->SimulateTouch(RightEdge, 1.5f);
// Assert: debug log contains IS_ZONE_CLASSIFIED zone=RIGHT, IS_SLIP_DISPATCHED direction=right
```

### Example: AC-07 cancel timer test

```cpp
Clock->SetMs(0.0);
int32 ContactId = IS->SimulateTouch(200.0f, 1.5f); // left zone tap at t=0

// Advance to just before cancel threshold
Clock->AdvanceMs(179.0);
IS->DrainTick(); // explicit tick — cancel must NOT fire yet
// Assert: no IS_CANCEL_DISPATCHED in log

// Advance past threshold
Clock->AdvanceMs(2.0); // now at 181ms
IS->DrainTick();
// Assert: IS_CANCEL_DISPATCHED fires with t_elapsed_ms > 180
// Assert: IS_CONTACT_RESTING fires for ContactId
```

---

## Seam 3: `IRunStateProvider`

### Why this seam exists

The Input System GDD (R-5) states the IS has no game-state awareness: it dispatches
events regardless of run state. However, one narrow exception exists: the NONE-tier
R-3 collision visual (zone-edge desaturation pulse) is suppressed during DEAD and
RESOLVING states (to avoid overlapping with the death/score UI). This single RSM
read would require a live RSM in all IS unit tests without an injectable stub.

`IRunStateProvider` makes that read injectable so IS tests set RSM state without
constructing or running the actual Run State Machine.

### Interface

```cpp
// IRunStateProvider.h
//
// ERunState must match the RSM GDD exactly.
// Source of truth: design/gdd/run-state-machine.md
enum class ERunState : uint8
{
    IDLE       = 0,
    COUNTDOWN  = 1,
    RUNNING    = 2,
    DEAD       = 3,
    COMPLETE   = 4,
    RESOLVING  = 5,
    ABORTED    = 6,
};

class IRunStateProvider
{
public:
    virtual ERunState GetCurrentState() const = 0;
    virtual bool      GetIsPaused()     const = 0;
    virtual ~IRunStateProvider() = default;
};
```

### Production Implementation

```cpp
// FRSMRunStateProvider.h
//
// Lazy-resolution pattern: the subsystem cannot be resolved at FInputSystem
// construction time because IS is constructed in FSlipstormGameModule::StartupModule(),
// which runs *before* UEngine::Init() creates UGameInstance. Eager resolution returns
// a permanently-null TWeakObjectPtr. Instead, we resolve on first GetCurrentState()
// call — by then DrainTick has started, UGameInstance exists, and the subsystem is
// either available (resolve) or not yet ready (return ABORTED fallback; retry next
// call). The mutable state is internal — the IRunStateProvider interface remains
// `const`-correct from the caller's perspective.
class FRSMRunStateProvider final : public IRunStateProvider
{
    // TWeakObjectPtr — safe across GC cycles. Raw UObject* in a non-UObject owner
    // (FInputSystem is not a UObject) would be silently invalidated by GC, causing
    // a dangling pointer dereference on any state read after a GC pass.
    mutable TWeakObjectPtr<URunStateMachineSubsystem> RSM;
    mutable bool bResolutionAttempted = false;

    void EnsureResolved() const
    {
        if (RSM.IsValid()) return;          // already resolved
        // bResolutionAttempted is informational — we retry every call until valid,
        // because UGameInstance may take several frames to come up on cold boot.
        bResolutionAttempted = true;
        if (!GEngine) return;
        for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
        {
            if (UGameInstance* GI = Ctx.OwningGameInstance)
            {
                RSM = GI->GetSubsystem<URunStateMachineSubsystem>();
                if (RSM.IsValid()) return;
            }
        }
    }

public:
    // No constructor argument — eager resolution is impossible at StartupModule() time
    // (UGameInstance does not exist yet). The previous `check(InRSM != nullptr)` ctor
    // hard-crashed Debug/Development builds at module load.
    FRSMRunStateProvider() = default;

    ERunState GetCurrentState() const override
    {
        EnsureResolved();
        // Return ABORTED (not IDLE) when the pointer is invalid: IDLE would suppress
        // the ABORTED-transition flush in DrainTick, leaving ContactResting state
        // on-screen permanently if the RSM is torn down mid-run. ABORTED is also the
        // safe early-boot fallback before UGameInstance is created.
        return RSM.IsValid() ? RSM->GetCurrentState() : ERunState::ABORTED;
    }

    bool GetIsPaused() const override
    {
        EnsureResolved();
        return RSM.IsValid() ? RSM->GetIsPaused() : false;
    }
};
```

### Test Stub

```cpp
// FRunStateProviderStub.h  (#if !UE_BUILD_SHIPPING)
class FRunStateProviderStub final : public IRunStateProvider
{
    ERunState State  = ERunState::IDLE;
    bool bPaused     = false;

public:
    ERunState GetCurrentState() const override { return State; }
    bool      GetIsPaused()     const override { return bPaused; }

    void SetState(ERunState NewState)  { State   = NewState; }
    void SetPaused(bool bNewPaused)    { bPaused = bNewPaused; }
};
```

### Usage in the Input System

```cpp
// IS reads RSM state for R-3 haptic + visual suppression on all capability tiers:
bool FInputSystem::ShouldSuppressR3Signal() const
{
    const ERunState State = RunStateProvider->GetCurrentState();
    return State == ERunState::DEAD
        || State == ERunState::RESOLVING
        || State == ERunState::ABORTED
        || State == ERunState::COUNTDOWN;
}
```

### Example: AC-13 no-run-state-gating test

```cpp
// Confirm IS dispatches slip events in IDLE and DEAD states

auto RunStateStubOwned = MakeUnique<FRunStateProviderStub>();
auto* RunStateStub = RunStateStubOwned.Get();
auto IS = MakeUnique<FInputSystem>(MoveTemp(ClockOwned), MoveTemp(TouchRadiusStubOwned), MoveTemp(RunStateStubOwned));

// Test in IDLE state
RunStateStub->SetState(ERunState::IDLE);
IS->SimulateTouch(200.0f, 1.5f); // left zone
// Assert: IS_SLIP_DISPATCHED fires — IS does not gate on IDLE

// Test in DEAD state
RunStateStub->SetState(ERunState::DEAD);
IS->SimulateTouch(800.0f, 1.5f); // right zone
// Assert: IS_SLIP_DISPATCHED fires — IS does not gate on DEAD
```

---

---

## Seam 4: `IVisualDispatch`

`IVisualDispatch` is the fifth injectable interface. Its full contract, production
implementation (`FWidgetVisualDispatch`), and test stub (`FVisualDispatchStub`) are
specified in `docs/architecture/visual-dispatch-contract.md`.

**Summary for wiring purposes:**
- `IVisualDispatch::Fire(EVisualEvent)` — game thread only
- `FVisualDispatchStub::WasFired()`, `GetLastEvent()`, `Reset()` — for AC-19b, AC-19c
- Injected at `FInputSystem` construction alongside the other four interfaces

---

## Seam 6: `IBootFlagStore`

### Why this seam exists

IS reads `FIRST_RUN_PROMPT_ENABLED` at COUNTDOWN entry to determine whether to show
first-run labels, and writes it to `false` on the first successful RUNNING slip.
Without an injectable interface, this read/write cannot be controlled in unit tests —
testers cannot reset the flag between runs, verify exactly when GetFlag/SetFlag are
called, or simulate a first-run vs. returning-user scenario.

### Interface

Defined inline in `design/gdd/input-system.md` §Dependencies:

```cpp
// IBootFlagStore.h
class IBootFlagStore {
public:
    virtual bool GetFlag(FName FlagName) const = 0;  // returns false if key absent
    virtual void SetFlag(FName FlagName, bool Value) = 0;
    virtual ~IBootFlagStore() = default;
};
```

### Production Implementation

```cpp
// FLocalStorageBootFlagStore.h
class FLocalStorageBootFlagStore final : public IBootFlagStore
{
public:
    bool GetFlag(FName FlagName) const override
    {
        // Read from UE5 game-user settings or platform key-value store.
        // UConfigCacheIni::GetBool signature: bool GetBool(const TCHAR* Section,
        // const TCHAR* Key, bool& Value, const FString& Filename) const.
        // Return value indicates whether the key was found; the read value is
        // an out-parameter. Returns false (the default) if the key has never
        // been written.
        bool Value = false;
        GConfig->GetBool(TEXT("BootFlags"), *FlagName.ToString(), Value,
                         GGameUserSettingsIni);
        return Value;
    }

    void SetFlag(FName FlagName, bool Value) override
    {
        GConfig->SetBool(TEXT("BootFlags"), *FlagName.ToString(), Value,
                         *GGameUserSettingsIni);
        GConfig->Flush(false, *GGameUserSettingsIni);
    }
};
```

### Test Stub

```cpp
// FBootFlagStoreStub.h  (#if !UE_BUILD_SHIPPING)
class FBootFlagStoreStub final : public IBootFlagStore
{
    TMap<FName, bool>   Flags;
    mutable int32       GetFlagCallCount_Val = 0;
    int32               SetFlagCallCount_Val = 0;

public:
    // IBootFlagStore implementation:
    bool GetFlag(FName FlagName) const override
    {
        ++GetFlagCallCount_Val;
        const bool* Found = Flags.Find(FlagName);
        return Found ? *Found : false;
    }

    void SetFlag(FName FlagName, bool Value) override
    {
        ++SetFlagCallCount_Val;
        Flags.Add(FlagName, Value);
    }

    // Test observer methods:
    int32 GetFlagCallCount() const { return GetFlagCallCount_Val; }
    int32 SetFlagCallCount() const { return SetFlagCallCount_Val; }

    // Direct flag seeding (does not increment call counters):
    void InitFlag(FName FlagName, bool Value) { Flags.Add(FlagName, Value); }

    // Reset all flags and call counters between test cases:
    void Reset()
    {
        Flags.Empty();
        GetFlagCallCount_Val = 0;
        SetFlagCallCount_Val = 0;
    }
};
```

**Call-count semantics:**
- `GetFlagCallCount()` — cumulative count of `GetFlag()` calls since construction or last `Reset()`. Increments on every call regardless of which `FlagName` is queried.
- `SetFlagCallCount()` — cumulative count of `SetFlag()` calls since construction or last `Reset()`.
- Use `Reset()` between test cases that need independent call-count tracking.

**Usage in AC-FRP-GETFLAG-COUNTDOWN:**
```cpp
auto BootFlagOwned = MakeUnique<FBootFlagStoreStub>();
auto* BootFlag = BootFlagOwned.Get();
// ...construct IS...
// Step 1: construct IS — GetFlagCallCount == 0
EXPECT_EQ(BootFlag->GetFlagCallCount(), 0);
// Step 3: first COUNTDOWN DrainTick — GetFlagCallCount must equal 1
RunState->SetState(ERunState::COUNTDOWN);
IS->DrainTick();
EXPECT_EQ(BootFlag->GetFlagCallCount(), 1);
```

**Usage in AC-UX4-COUNTDOWN-NO-SETFLAG:**
```cpp
BootFlag->InitFlag(TEXT("FIRST_RUN_PROMPT_ENABLED"), true);
RunState->SetState(ERunState::COUNTDOWN);
IS->SimulateTouch(200.0f, 1.5f);  // LEFT zone
IS->DrainTick();  // receipt tick
IS->DrainTick();  // dispatch tick — IS_SLIP_DISPATCHED fires
EXPECT_EQ(BootFlag->SetFlagCallCount(), 0);  // SetFlag must NOT be called during COUNTDOWN
```

### GDD ACs Unblocked by This Stub

| AC | What was blocking it |
|---|---|
| AC-UX4-COUNTDOWN-NO-SETFLAG | `FBootFlagStoreStub.SetFlagCallCount` not specified |
| AC-FRP-GETFLAG-COUNTDOWN | `FBootFlagStoreStub.GetFlagCallCount` not specified |
| AC-21e (COUNTDOWN→RUNNING pre-positioning) | Required `FBootFlagStoreStub` to control `FIRST_RUN_PROMPT_ENABLED` |

---

## Seam 7: `IRSMTimeStateProvider` (DPC)

### Why this seam exists

The Difficulty & Phase Controller (DPC) reads `current_state`, `is_paused`,
`remaining_time`, and `RUN_DURATION_S` from the Run State Machine each tick (DPC
Rule 4 + Rule 14). DPC's ACs require driving these values directly without
running a real RSM:

- AC-12: 60-tick monotone `remaining_time` walk from `60.0` to `0.0`
- AC-13: single-tick `remaining_time` jump simulating frame-hitch boundary crossing
- AC-14c: `is_paused` toggle mid-test to validate Rule 16 sting suppression
- AC-21: `current_state` transition from RUNNING to ABORTED mid-run
- AC-NAN-GUARD: `remaining_time = NaN` injection
- AC-RUN-DURATION-ZERO: `RUN_DURATION_S = 0.0` injection at `Initialize()` time

Seam 3 (`IRunStateProvider`) is the IS-narrow read with only `GetCurrentState()`
and `GetIsPaused()` — it exposes neither `remaining_time` nor `RUN_DURATION_S`,
so DPC cannot use Seam 3 directly. Rather than expand Seam 3 (which would
require all IS consumers to handle two new methods they do not use), DPC declares
a separate `IRSMTimeStateProvider` interface that is a strict superset of the RSM
read surface DPC needs.

### Interface

```cpp
// IRSMTimeStateProvider.h
//
// ERunState and the {RUN_DURATION_S, remaining_time} semantics must match
// the RSM GDD exactly. Source of truth: design/gdd/run-state-machine.md.
class IRSMTimeStateProvider
{
public:
    virtual ERunState GetCurrentState()       const = 0;
    virtual bool      GetIsPaused()           const = 0;

    // Returns the canonical RSM run timer in seconds. Per RSM F-2, `remaining_time`
    // is monotonically non-increasing during RUNNING (modulo pause-freeze via
    // `cached_remaining_at_pause_entry`) and is frozen while `is_paused == true`.
    // Range `[0.0, RUN_DURATION_S]` under nominal contract; DPC F-1 clamp guards
    // overshoot.
    virtual double    GetRemainingTime()      const = 0;

    // Returns the configured run length in seconds. RSM owns the canonical
    // `[10.0, 300.0]s` range assertion per its Tuning Knobs. DPC adds a
    // defensive `> 0.0` guard at Initialize() (Rule 14) because F-1 divides
    // by this value.
    virtual double    GetRunDurationS()       const = 0;

    virtual ~IRSMTimeStateProvider() = default;
};
```

### Production Implementation

```cpp
// FRSMTimeStateProvider.h
//
// Lazy-resolution pattern (same as FRSMRunStateProvider in Seam 3 — DPC is
// constructed before UGameInstance exists). The mutable state is internal —
// the IRSMTimeStateProvider interface remains `const`-correct from DPC's
// perspective.
class FRSMTimeStateProvider final : public IRSMTimeStateProvider
{
    mutable TWeakObjectPtr<URunStateMachineSubsystem> RSM;
    mutable bool bResolutionAttempted = false;

    void EnsureResolved() const
    {
        if (RSM.IsValid()) return;
        bResolutionAttempted = true;
        if (!GEngine) return;
        for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
        {
            if (UGameInstance* GI = Ctx.OwningGameInstance)
            {
                RSM = GI->GetSubsystem<URunStateMachineSubsystem>();
                if (RSM.IsValid()) return;
            }
        }
    }

public:
    FRSMTimeStateProvider() = default;

    // Return ABORTED (not IDLE) when the pointer is invalid: matches the
    // FRSMRunStateProvider pattern (Seam 3) so DPC sees a safe non-RUNNING
    // fallback during cold boot.
    ERunState GetCurrentState() const override
    {
        EnsureResolved();
        return RSM.IsValid() ? RSM->GetCurrentState() : ERunState::ABORTED;
    }

    bool GetIsPaused() const override
    {
        EnsureResolved();
        return RSM.IsValid() ? RSM->GetIsPaused() : false;
    }

    // Return RUN_DURATION_S as the safe pre-resolution remaining_time —
    // matches the RSM IDLE-state contract (t_norm = 0.0, OPENER published).
    double GetRemainingTime() const override
    {
        EnsureResolved();
        // When RSM is not yet resolved (cold-boot: UGameInstance not yet initialized,
        // or GC collected RSM), return the run duration as the safe fallback via the
        // self-call to GetRunDurationS() — which returns 60.0 when unresolved and
        // never dereferences RSM. The previous false-branch called RSM->GetRunDurationS()
        // on the same TWeakObjectPtr that just failed IsValid(), which was a guaranteed
        // null-deref (R5 Cluster B Item 1).
        return RSM.IsValid() ? RSM->GetRemainingTime() : GetRunDurationS();
    }

    double GetRunDurationS() const override
    {
        EnsureResolved();
        // Default to 60.0 (RSM canonical default) if RSM not yet resolved.
        // DPC's Rule 14 `check(RUN_DURATION_S > 0.0)` is satisfied by this default.
        return RSM.IsValid() ? RSM->GetRunDurationS() : 60.0;
    }
};
```

### Test Stub

```cpp
// FRSMTestStub.h  (#if !UE_BUILD_SHIPPING)
//
// Drives DPC's RSM read surface directly. DPC AC-12 / AC-13 / AC-14c / AC-21 /
// AC-NAN-GUARD / AC-RUN-DURATION-ZERO all depend on this stub.
// Pull-Wave AC-PW-16 / AC-PW-17a / AC-PW-17b additionally depend on this stub
// via the OnPausedChanged delegate and the atomic ABORTED-transition helper
// (R2 Cluster F extension).
class FRSMTestStub final : public IRSMTimeStateProvider
{
    ERunState State           = ERunState::IDLE;
    bool      bPaused         = false;
    double    RemainingTime   = 60.0;
    double    RunDurationS    = 60.0;

public:
    // --- IRSMTimeStateProvider interface ---
    ERunState GetCurrentState()  const override { return State; }
    bool      GetIsPaused()      const override { return bPaused; }
    double    GetRemainingTime() const override { return RemainingTime; }
    double    GetRunDurationS()  const override { return RunDurationS; }

    // --- Basic setters (drive AC scenarios) ---
    void SetCurrentState(ERunState NewState)   { State         = NewState; }
    void SetRemainingTime(double NewRemaining) { RemainingTime = NewRemaining; }
    void SetRunDurationS(double NewDuration)   { RunDurationS  = NewDuration; }

    // Convenience for AC-12 (60-tick monotone walk): advance by step seconds.
    // No clamp — tests intentionally drive past 0.0 to exercise F-1's lower clamp.
    void AdvanceRemainingTime(double DeltaSeconds) { RemainingTime -= DeltaSeconds; }

    // --- OnPausedChanged delegate (R2 Cluster F — Pull-Wave AC-PW-16 / 17 / 25 / 26) ---
    //
    // Pull-Wave's pause-flush logic subscribes to RSM.OnPausedChanged (conceptually).
    // This stub exposes the delegate so tests can drive the same signal path.
    //
    // Broadcast semantics (CF-A3 contract):
    //   OnPausedChanged fires ONLY when SetIsPaused() is called AND the value
    //   actually changes AND current_state == RUNNING at the moment of the call.
    //   State transitions that incidentally clear is_paused as a side effect
    //   (e.g. RUNNING+paused → ABORTED via SetCurrentStateWithPauseClear()) do
    //   NOT broadcast OnPausedChanged — the wave must NOT route to DESPAWNING
    //   via PauseFlush in that path (EC-STALE-PAUSE-ABORTED semantic).
    DECLARE_MULTICAST_DELEGATE_OneParam(FOnPausedChangedDelegate, bool /*bNewPaused*/);
    FOnPausedChangedDelegate OnPausedChanged;

    // SetIsPaused — broadcasts OnPausedChanged if and only if current_state == RUNNING
    // and the value changes. Use this to drive AC-PW-16 (pause → freeze) and the
    // PauseFlush unfreeze (false branch → despawn_reason == PauseFlush).
    void SetIsPaused(bool bNewPaused)
    {
        if (bPaused != bNewPaused)
        {
            bPaused = bNewPaused;
            if (State == ERunState::RUNNING)
            {
                OnPausedChanged.Broadcast(bNewPaused);
            }
        }
    }

    // --- Atomic ABORTED + is_paused → false helper (CF-A3 — AC-PW-17a / 17b) ---
    //
    // Drives the PAUSED → ABORTED path in which RSM transitions current_state to
    // ABORTED while simultaneously clearing is_paused in the same tick, WITHOUT
    // broadcasting OnPausedChanged. This matches the RSM spec: the ABORTED-transition
    // is not a user-visible "unpause" event; it is a run-terminal state change.
    //
    // Test contract: after this call, GetCurrentState() == ABORTED and
    // GetIsPaused() == false, and no OnPausedChanged broadcast has fired.
    // Pull-Wave must NOT treat this as a PauseFlush trigger.
    void SetCurrentStateWithPauseClear(ERunState NewState)
    {
        State   = NewState;
        bPaused = false;
        // Intentionally does NOT call SetIsPaused() or broadcast OnPausedChanged.
    }
};
```

### Usage in DPC Tests

```cpp
// AC-12: OPENER → MID → PEAK transition observability across a 60-tick walk
// Ownership model (R5 Cluster B Item 8 + R5 re-review 5 teardown fix closing
// unreal-specialist BLOCKING B-2): UDPCController holds IRSMTimeStateProvider*
// as a raw observer pointer (non-owning borrow). The stub is owned by this test
// function via TUniquePtr (StubOwned). DPC must not outlive the stub. Passing
// MoveTemp(StubOwned) into the DPC constructor is incorrect and was removed in R5.
//
// CRITICAL teardown ordering (R5 re-review 5 fix): if DPC is rooted via AddToRoot()
// and the test function returns without explicitly RemoveFromRoot()-ing DPC, the
// stack-owned StubOwned destructs FIRST (LIFO on stack) and DPC's raw IRSMTimeStateProvider*
// pointer dangles. The first GC pass or any access to DPC after teardown is a
// use-after-free. The canonical test pattern below explicitly orders teardown:
// (1) clear DPC's provider pointer via Initialize(nullptr, nullptr) OR DPC->RemoveFromRoot(),
// (2) StubOwned naturally destructs at function exit.
auto StubOwned = MakeUnique<FRSMTestStub>();
auto* Stub = StubOwned.Get();
Stub->SetCurrentState(ERunState::RUNNING);
Stub->SetRunDurationS(60.0);
Stub->SetRemainingTime(60.0);

// UDPCController is UObject + FTickableGameObject (R5 Cluster B Item 7 + R5 re-review 5
// tick mechanism resolution); use NewObject<>, not MakeUnique<>. The FTickableGameObject
// mixin self-registers on construction. AddToRoot() keeps the UObject portion alive against
// GC for the test's duration.
UDPCController* DPC = NewObject<UDPCController>(GetTransientPackage());
DPC->AddToRoot();
DPC->Initialize(Stub, CanonicalCurves);

for (int32 i = 0; i < 60; ++i)
{
    Stub->AdvanceRemainingTime(1.0);
    DPC->Tick(0.0f);  // DeltaTime unused inside DPC body; param mandated by FTickableGameObject signature
    const FDPCFrameState& Snapshot = DPC->GetCurrentFrameState();
    // Assert phase transitions at the expected t_norm boundaries
}

// === Teardown (R5 re-review 5 BLOCKING fix) ===
// Order matters: remove DPC's GC root BEFORE StubOwned destructs at function exit,
// so DPC is eligible for GC collection while Stub is still valid. If GC runs during
// world transition, DPC's destructor (or any GC-triggered access) sees a valid Stub.
DPC->RemoveFromRoot();
// StubOwned destructs here at function exit. By this point, DPC is no longer rooted;
// GC may have collected it already, OR if a future GC pass collects it, the Stub*
// pointer is still valid because StubOwned has not yet destructed.
```

```cpp
// AC-RUN-DURATION-ZERO: Initialize() with RUN_DURATION_S = 0.0 logs Error
// and publishes inactive snapshot — NO Fatal (process must survive)
Stub->SetRunDurationS(0.0);
FTestLogCapture LogCapture;
UDPCController* DPC = NewObject<UDPCController>(GetTransientPackage());
DPC->AddToRoot();
DPC->Initialize(Stub, CanonicalCurves);
// Assert: LogCapture.CountMessages(LogDPC, ELogVerbosity::Error) == 1
// Assert: DPC->GetCurrentFrameState().is_active == false (still alive)
DPC->RemoveFromRoot();  // R5 re-review 5 fix: explicit teardown before StubOwned destructs
```

### GDD ACs Unblocked by This Stub

| AC | What was blocking it |
|---|---|
| DPC AC-12 | No `remaining_time` step injection |
| DPC AC-13 | No frame-hitch jump simulation |
| DPC AC-14c | No `is_paused` mid-test toggle |
| DPC AC-21 | No `current_state` transition control |
| DPC AC-NAN-GUARD | No NaN `remaining_time` injection path |
| DPC AC-RUN-DURATION-ZERO | No `RUN_DURATION_S = 0.0` injection path |
| Seam 7 AC-RSM-COLD-BOOT-FALLBACK (R5 Item 1) | Cold-boot fallback validation requires constructing `FRSMTimeStateProvider` against a null `WorldContextObject` |

**AC-RSM-COLD-BOOT-FALLBACK** (Seam 7 contract): When `FRSMTimeStateProvider::GetRemainingTime()` is called before `URunStateMachineSubsystem` is available (cold-boot, GC-after-PIE-end, or subsystem-not-registered paths), it must return the same value as `GetRunDurationS()` and must not dereference RSM. Test: construct `FRSMTimeStateProvider` with a null `WorldContextObject`, call `GetRemainingTime()`; assert return value equals `GetRunDurationS()` (which returns `60.0` when unresolved) and no crash. This AC validates the R5 Cluster B Item 1 fix that replaced the prior null-deref `RSM->GetRunDurationS()` false-branch with the self-call `GetRunDurationS()`.

---

## Seam 8: `IDPCSnapshotConsumer` Test Stub (DPC)

### Why this seam exists

DPC Rule 17 specifies a mid-run consumer subscription protocol: a consumer
registering as a snapshot reader after DPC has been ticking must read
`GetCurrentFrameState()` into its local cache and skip its diff handler for
one tick (or set a `bFirstReadComplete = false` sentinel). This protocol
prevents spurious phase-transition behavior fires on the subscription tick.

DPC AC-14b validates this protocol by spawning a fresh consumer at a phase
boundary tick and asserting the consumer fires no transition behavior on its
first read. The consumer cannot be a real Wave Spawner or Audio Controller
(those GDDs are not yet authored). It must be a test stub that exposes the
Rule 17 sentinel and counts transition-handler invocations.

This is a consumer-side seam — there is no production implementation. Real
consumers (Wave Spawner, Audio Controller) implement the protocol themselves
in their respective production code; the test stub exists only to validate
the protocol contract that DPC imposes on those consumers.

### Interface (test-only)

```cpp
// IDPCSnapshotConsumer.h  (#if !UE_BUILD_SHIPPING — test-only abstraction)
//
// The interface a Rule 17-compliant consumer presents to a test harness.
// Real consumers (Wave Spawner, Audio Controller) need not inherit from this
// interface; they only need to implement the same protocol internally.
class IDPCSnapshotConsumer
{
public:
    // Called once per DPC tick by the test harness. Implementations must follow
    // Rule 17: skip the diff handler on the first call after construction.
    virtual void ReadSnapshot(const FDPCFrameState& Current,
                              const FDPCFrameState& Previous) = 0;

    // Test-side observation: how many times has the consumer fired its
    // phase-transition handler? AC-14b asserts this == 0 across the first-call
    // and first-tick-post-subscription boundary.
    virtual int32 GetTransitionHandlerFireCount() const = 0;

    virtual ~IDPCSnapshotConsumer() = default;
};
```

### Test Stub

```cpp
// FConsumerTestStub.h  (#if !UE_BUILD_SHIPPING)
//
// Implements Rule 17's consumer-side TWO-LAYER initialization protocol
// (R5 Cluster B Item 9): distinguishes `is_active` rising-edge handler
// (always fires on first-tick-with-active-snapshot) from phase-diff handler
// (skipped on first tick). Used by AC-14a (mid-PEAK subscription, no diff),
// AC-14b (boundary-tick subscription, diff suppressed by Rule 17), and
// AC-RISING-EDGE (R5 Cluster B Item 9 validation).
class FConsumerTestStub final : public IDPCSnapshotConsumer
{
    bool        bFirstReadComplete           = false;
    ERunPhase   CachedPhase                  = ERunPhase::OPENER;
    int32       TransitionFireCount          = 0;
    // R5 re-review 5 fix — closes qa-lead BLOCKING BLK-1: this field is
    // referenced by AC-RISING-EDGE in the DPC GDD but was absent in the
    // prior Seam 8 spec, making the AC unverifiable. Field semantics:
    // set TRUE on the first tick where the consumer's `bFirstReadComplete`
    // transition observes `Current.is_active == true` (i.e., the rising
    // edge of is_active from the consumer's perspective). The rising-edge
    // handler is invoked exactly once on the tick this field flips false→true.
    bool        bIsActiveRisingEdgeObserved  = false;
    // Tracks rising-edge handler invocations for AC-RISING-EDGE assertions
    // (test verifies handler fires exactly once at the correct tick).
    int32       RisingEdgeFireCount          = 0;

public:
    void ReadSnapshot(const FDPCFrameState& Current,
                      const FDPCFrameState& Previous) override
    {
        // --- Layer 1: is_active rising-edge detection (R5 re-review 5 + Cluster B Item 9) ---
        // The rising-edge handler MUST fire on the first tick where Current.is_active==true,
        // even if it is the consumer's first observed tick (this is the rising edge from
        // the consumer's perspective). This is distinct from phase-diff behavior, which IS
        // skipped on the first tick.
        if (!bIsActiveRisingEdgeObserved && Current.is_active)
        {
            // Invoke the rising-edge handler (Audio Controller mix-resume,
            // visual fade-in, etc.) — stubbed here as a counter increment.
            ++RisingEdgeFireCount;
            bIsActiveRisingEdgeObserved = true;
        }
        // If Current.is_active goes false (pause-entry) and later returns to true,
        // the rising-edge handler fires again on that recovery tick — reset on the
        // falling edge to enable subsequent rising-edge detection.
        if (!Current.is_active)
        {
            bIsActiveRisingEdgeObserved = false;
        }

        // --- Layer 2: phase-diff handler with first-tick suppression ---
        if (!bFirstReadComplete)
        {
            // Rule 17: first read sets the cache and skips the phase-diff handler.
            // Whatever Previous contains is ignored on this tick for phase-diff purposes.
            // The is_active rising-edge handler above is NOT skipped — it ran above.
            CachedPhase         = Current.current_phase;
            bFirstReadComplete  = true;
            return;
        }

        // Rule 16 gate (active-state diff guard). If either side of the diff
        // is inactive, this is a pause-boundary / non-RUNNING boundary diff;
        // silently update the cache, do not fire the transition handler.
        if (!Previous.is_active || !Current.is_active)
        {
            CachedPhase = Current.current_phase;
            return;
        }

        // Real phase transition: cached_phase != current.
        if (CachedPhase != Current.current_phase)
        {
            ++TransitionFireCount;
            CachedPhase = Current.current_phase;
        }
    }

    int32 GetTransitionHandlerFireCount() const override
    {
        return TransitionFireCount;
    }

    // Test-side reset for re-running scenarios within a single test file.
    void Reset()
    {
        bFirstReadComplete           = false;
        CachedPhase                  = ERunPhase::OPENER;
        TransitionFireCount          = 0;
        bIsActiveRisingEdgeObserved  = false;
        RisingEdgeFireCount          = 0;
    }

    // Test-side accessors for assertions.
    bool      GetFirstReadComplete()           const { return bFirstReadComplete; }
    ERunPhase GetCachedPhase()                 const { return CachedPhase; }
    // R5 re-review 5 fix — used by AC-RISING-EDGE assertions:
    bool      GetIsActiveRisingEdgeObserved()  const { return bIsActiveRisingEdgeObserved; }
    int32     GetRisingEdgeFireCount()         const { return RisingEdgeFireCount; }
};
```

### Usage in DPC Tests

```cpp
// AC-14b: fresh consumer subscribes on a phase boundary tick — no spurious
// transition fire on the consumer's first read.
auto RSMStub = MakeUnique<FRSMTestStub>();
RSMStub->SetCurrentState(ERunState::RUNNING);
RSMStub->SetRunDurationS(60.0);
RSMStub->SetRemainingTime(15.0); // t_norm = 0.75 exactly — PEAK_START boundary tick

// Ownership (R5 Item 8): raw observer pointer; RSMStub owned by enclosing test scope.
UDPCController* DPC = NewObject<UDPCController>(GetTransientPackage());
DPC->AddToRoot();
DPC->Initialize(RSMStub.Get(), CanonicalCurves);
DPC->Tick(0.0f); // PEAK enters here; Previous (cached) was MID (DeltaTime param mandated by FTickableGameObject)

FConsumerTestStub Consumer;
Consumer.ReadSnapshot(DPC->GetCurrentFrameState(), DPC->GetPreviousFrameState());

// Rule 17 contract: no transition fired despite previous.current_phase == MID
// and current.current_phase == PEAK.
// Assert: Consumer.GetTransitionHandlerFireCount() == 0
// Assert: Consumer.GetCachedPhase() == ERunPhase::PEAK
// AC-RISING-EDGE assertion (R5 re-review 5): is_active==true on first tick during
// RUNNING fires the rising-edge handler exactly once:
// Assert: Consumer.GetIsActiveRisingEdgeObserved() == true
// Assert: Consumer.GetRisingEdgeFireCount() == 1

DPC->RemoveFromRoot(); // R5 re-review 5: explicit teardown before RSMStub destructs at scope exit
```

### GDD ACs Unblocked by This Stub

| AC | What was blocking it |
|---|---|
| DPC AC-14a | No Rule 17-compliant consumer stub |
| DPC AC-14b | No `bFirstReadComplete` sentinel + transition counter |
| DPC AC-14c | Used with `FRSMTestStub` (Seam 7) for pause/resume cycle |
| DPC AC-RISING-EDGE (R5 Cluster B Item 9 + R5 re-review 5) | No `bIsActiveRisingEdgeObserved` field + rising-edge counter |

---

## Seam 9: `IDPCAbortDelegate` (DPC)

### Why this seam exists

DPC owns no game-state authority and does not transition RSM directly. When DPC must signal a run-fatal condition (async-load watchdog expiry per EC-13, or cap-vs-barrage incompatibility per Rule 14), it raises an abort request through this delegate. The production implementation forwards the request to RSM, which transitions to `ABORTED` per its state machine contract. The seam is the OQ-7-agnostic boundary between DPC and the RSM subsystem type chosen by the OQ-7 ADR — DPC does not depend on RSM's UE object type to issue an abort.

Authored R7 (2026-06-06) to close DPC R7 BLOCKING qa-lead findings 1 and 4 — the interface was referenced extensively across DPC GDD lines 82, 238, 576, 835, 838, 845, 857, 863, 868 and used by AC-CAP-BARRAGE-COMPATIBILITY (R6 NEW) and AC-ASYNC-LOAD-RACE (R5) but was undefined in this seam document until R7.

### Interface

```cpp
// Abort reasons enumerated to match all DPC abort entry points.
// Add new entries here when DPC introduces new run-fatal conditions.
// Names match the EDPCInitOutcome failure variants where they overlap.
UENUM()
enum class EDPCAbortReason : uint8
{
    // EC-13 watchdog expiry — curve assets did not complete loading within
    // DPC_ASYNC_LOAD_WATCHDOG_S = 5.0s of RUNNING entry (foreground unpaused tick time).
    AsyncLoadTimeout,

    // Rule 14 cap-vs-barrage check — MaxConcurrentWavesCurve PEAK key < MAX_PULLS_PER_BARRAGE
    // when the Wave Spawner PEAK pool contains is_barrage=true patterns. Mathematically
    // unsatisfiable barrage admission gate — silent deadlock prevented by the runtime check.
    ThermalFallbackBarrageIncompatible,
};

// Abort delegate — DPC raises run-fatal conditions through this interface.
// Production implementation forwards to RSM's ABORTED transition.
// Test implementation (FDPCAbortTestStub) records the last reason for AC assertions.
class IDPCAbortDelegate
{
public:
    virtual ~IDPCAbortDelegate() = default;

    // Request RSM transition to ABORTED with the supplied reason.
    // Game-thread only (called from DPC::Tick or DPC::OnCurvesLoaded).
    // Production implementation MUST be idempotent if invoked more than once during the
    // same run (defense-in-depth against future DPC bugs); the R7 Q2=A fix sets
    // bInitialized=true on validation completion (success OR failure), preventing the
    // double-abort race at the source, but the delegate idempotency is a belt-and-suspenders
    // contract for future safety.
    virtual void RequestAbort(EDPCAbortReason Reason) = 0;
};
```

### Production Implementation

Production forwards the request to RSM through its subsystem wrapper. The implementation lives alongside `FRSMTimeStateProvider` (Seam 7) and shares its OQ-7-resolution logic for finding the RSM instance.

```cpp
class FDPCAbortDelegate_Production final : public IDPCAbortDelegate
{
public:
    void RequestAbort(EDPCAbortReason Reason) override
    {
        check(IsInGameThread());
        URunStateMachineSubsystem* RSM = ResolveRSMSubsystem();  // shared resolution with Seam 7
        if (!RSM)
        {
            UE_LOG(LogDPC, Warning, TEXT("DPC abort request dropped — RSM subsystem not resolvable. Reason=%s"),
                   *UEnum::GetValueAsString(Reason));
            return;
        }
        RSM->RequestAbort(static_cast<ERSMAbortReason>(Reason));  // RSM 5-item forward contract item 4
    }
};
```

### Test Stub

```cpp
class FDPCAbortTestStub final : public IDPCAbortDelegate
{
public:
    void RequestAbort(EDPCAbortReason Reason) override
    {
        check(IsInGameThread());
        ++AbortCallCount;
        LastAbortReason = Reason;
        AllAbortReasons.Add(Reason);
    }

    // AC assertion accessors.
    int32 GetAbortCallCount() const { return AbortCallCount; }
    EDPCAbortReason GetLastAbortReason() const
    {
        check(AbortCallCount > 0 && "GetLastAbortReason() called before any abort recorded");
        return LastAbortReason;
    }
    const TArray<EDPCAbortReason>& GetAllAbortReasons() const { return AllAbortReasons; }

    void Reset()
    {
        AbortCallCount = 0;
        AllAbortReasons.Reset();
        // LastAbortReason intentionally not reset — GetLastAbortReason()'s check catches "before any abort recorded"
    }

private:
    int32 AbortCallCount = 0;
    EDPCAbortReason LastAbortReason{};
    TArray<EDPCAbortReason> AllAbortReasons;
};
```

### GDD ACs Unblocked by This Stub

| AC | What was blocking it | What this stub provides |
|---|---|---|
| **DPC AC-ASYNC-LOAD-RACE** | No `IDPCAbortDelegate` to record the watchdog-expiry RequestAbort | `FDPCAbortTestStub.GetLastAbortReason()` returns `AsyncLoadTimeout` after watchdog fires |
| **DPC AC-CAP-BARRAGE-COMPATIBILITY** | No way to assert the cap-vs-barrage check's abort signaling | `FDPCAbortTestStub.GetAbortCallCount() == 1` AND `GetLastAbortReason() == ThermalFallbackBarrageIncompatible` |

---

## Seam 10: `IWaveSpawnerPoolMetadataProvider` (DPC + Wave Spawner)

### Why this seam exists

DPC's Rule 14 cap-vs-barrage compatibility check (R6 BLOCKING fix, CD-bound) requires DPC to know whether the Wave Spawner's PEAK pattern pool contains any `is_barrage=true` patterns at the moment of validation (post-`OnCurvesLoaded`). Without this metadata, DPC cannot decide whether `MaxConcurrentWavesCurve.GetFloatValue(PEAK_START_NORMALIZED) ≥ MAX_PULLS_PER_BARRAGE` is a hard requirement (when barrages exist in the pool — silent-deadlock prevention) or an optional configuration (when no barrages exist — thermal fallback path (a) is valid: ship at cap=2 with no PEAK barrages).

The seam keeps DPC and Wave Spawner decoupled: DPC asks "do you have any barrage patterns in PEAK?" and Wave Spawner answers with a boolean, without exposing the full pool data structure. The production implementation reads from Wave Spawner's content-cooked pool manifest; the test stub returns a test-fixture-configured boolean for AC-CAP-BARRAGE-COMPATIBILITY.

Authored R7 (2026-06-06) to close DPC R7 BLOCKING qa-lead finding 2 and systems-designer B-1.

### Interface

```cpp
// Lightweight pool-metadata read interface. DPC queries this once at
// post-OnCurvesLoaded validation to decide whether the cap-vs-barrage check applies.
// Future expansion (Wave Spawner GDD scope): per-pool barrage count, per-pattern slot
// requirements, cook-time spatial-separation validation hooks.
class IWaveSpawnerPoolMetadataProvider
{
public:
    virtual ~IWaveSpawnerPoolMetadataProvider() = default;

    // Returns true if the PEAK pattern pool contains at least one pattern flagged
    // is_barrage=true. Game-thread only. Result must be stable by the time DPC's
    // OnCurvesLoaded callback fires — Wave Spawner GDD must guarantee pool composition
    // is finalized at that point (the content-cooked pool manifest is immutable at runtime).
    // Returns false if pool metadata is not yet available — DPC treats false as a
    // successful validation (no barrage carve-out concern, no abort).
    virtual bool HasBarrageInPeakPool() const = 0;
};
```

### Production Implementation

Production reads from Wave Spawner's content-cooked pool manifest. The implementation lives in the Wave Spawner module (when authored) and is injected into DPC at construction.

```cpp
class FWaveSpawnerPoolMetadataProvider_Production final : public IWaveSpawnerPoolMetadataProvider
{
public:
    explicit FWaveSpawnerPoolMetadataProvider_Production(UWaveSpawnerPoolManifest* InManifest)
        : Manifest(InManifest) {}

    bool HasBarrageInPeakPool() const override
    {
        check(IsInGameThread());
        if (!Manifest) { return false; }  // manifest not loaded — treat as "no barrages", validation passes
        return Manifest->PeakPoolContainsBarrage();
    }

private:
    TWeakObjectPtr<UWaveSpawnerPoolManifest> Manifest;
};
```

### Test Stub

```cpp
class FWaveSpawnerPoolMetadataTestStub final : public IWaveSpawnerPoolMetadataProvider
{
public:
    // Test-controllable return value. Default false (no barrages — validation passes).
    void SetHasBarrageInPeakPool(bool bValue) { bHasBarrageInPeakPool = bValue; }

    bool HasBarrageInPeakPool() const override
    {
        check(IsInGameThread());
        return bHasBarrageInPeakPool;
    }

    void Reset() { bHasBarrageInPeakPool = false; }

private:
    bool bHasBarrageInPeakPool = false;
};
```

### GDD ACs Unblocked by This Stub

| AC | What was blocking it | What this stub provides |
|---|---|---|
| **DPC AC-CAP-BARRAGE-COMPATIBILITY** | No mock for `HasBarrageInPeakPool()` query | `FWaveSpawnerPoolMetadataTestStub.SetHasBarrageInPeakPool(true)` — drives the cap-vs-barrage check |
| **DPC AC-CAP-BARRAGE-COMPATIBILITY (positive sub-case)** | No mock for "no barrages" path | `SetHasBarrageInPeakPool(false)` — verifies validation passes when no barrages exist |

---

## Seam 11: `ICurveProvider` (DPC)

### Why this seam exists

DPC's Rule 8 output-side IsFinite guard catches `UCurveFloat::GetFloatValue()` returns that are NaN or non-finite (corrupted asset binary, designer-authored `+Inf` keys, malformed RichCurve interpolation). AC-08c sub-case (iv) validates this guard fires by feeding DPC a curve that returns NaN at a valid finite `t_norm` input — but UE5's `UCurveFloat` does not expose a programmatic way to make `GetFloatValue()` return NaN from a finite input without authoring a malformed .uasset binary (a brittle test fixture).

`ICurveProvider` is a thin test seam that substitutes for the production `UCurveFloat` reference for a specific test scenario, allowing the test stub to return NaN deterministically. Production code uses `UCurveFloat` directly (no seam overhead); the seam is only injected during AC-08c sub-case (iv) execution.

Authored R7 (2026-06-06) to close DPC R7 BLOCKING qa-lead finding 3.

### Interface

```cpp
// Test-only curve evaluation seam. Production code calls UCurveFloat::GetFloatValue
// directly; this seam exists so AC-08c sub-case (iv) can inject NaN returns without
// authoring a malformed .uasset binary.
class ICurveProvider
{
public:
    virtual ~ICurveProvider() = default;

    // Equivalent to UCurveFloat::GetFloatValue(t_norm). Game-thread only.
    virtual float GetFloatValue(float t_norm) const = 0;
};
```

### Production Implementation

Production wraps a `UCurveFloat*` reference. Used only when DPC is constructed with the seam-injection path (test builds); shipping builds use `UCurveFloat` directly without the seam wrapper.

```cpp
class FCurveProvider_UCurveFloat final : public ICurveProvider
{
public:
    explicit FCurveProvider_UCurveFloat(TObjectPtr<UCurveFloat> InCurve) : Curve(InCurve) {}

    float GetFloatValue(float t_norm) const override
    {
        check(IsInGameThread());
        return Curve ? Curve->GetFloatValue(t_norm) : 0.0f;
    }

private:
    TObjectPtr<UCurveFloat> Curve;
};
```

### Test Stub

```cpp
class FCurveProviderTestStub final : public ICurveProvider
{
public:
    // Configure the return value for all GetFloatValue calls.
    // Setting to NaN: `SetReturnValue(std::numeric_limits<float>::quiet_NaN())`
    // Setting to +Inf: `SetReturnValue(std::numeric_limits<float>::infinity())`
    void SetReturnValue(float InValue) { ReturnValue = InValue; }

    float GetFloatValue(float t_norm) const override
    {
        check(IsInGameThread());
        ++CallCount;
        return ReturnValue;
    }

    int32 GetCallCount() const { return CallCount; }
    void Reset() { ReturnValue = 0.0f; CallCount = 0; }

private:
    float ReturnValue = 0.0f;
    mutable int32 CallCount = 0;  // mutable because GetFloatValue is const
};
```

### GDD ACs Unblocked by This Stub

| AC | What was blocking it | What this stub provides |
|---|---|---|
| **DPC AC-08c sub-case (iv)** | No way to make `UCurveFloat::GetFloatValue()` return NaN from a finite t_norm input | `FCurveProviderTestStub.SetReturnValue(std::nan(""))` — drives Rule 8 output-side guard |

---

## Seam 12: `IPlayerMovementProvider` (Pull-Wave)

### Why this seam exists

Pull-Wave's ACs (AC-PW-21a/b/c, AC-PW-22, AC-PW-28) need to inject PM state (`current_lane`, `target_lane`, `movement_state`) without instantiating the full Player Movement subsystem. Pull-Wave reads PM state at LANDED-entry tick (Rule 10 hit resolution; Rule 11 near-miss direct-read of `current_lane` during SLIPPING). This seam abstracts the PM-side reads and `TriggerNearMissBeat()` call behind a test-injectable interface.

Pull-Wave does NOT subscribe to `OnSlipMidpoint` — removed R1 RC-A (near-miss now uses direct `PM.current_lane` read; PM Rules 4 + 7 guarantee `current_lane` returns the SOURCE lane throughout SLIPPING). PM continues to fire `OnSlipMidpoint` for other consumers; this seam does not expose that delegate to Pull-Wave. `FireSlipMidpoint` has been removed from the test stub (R2 Cluster D — no current consumer); re-introduce if a future consumer needs it.

Authored 2026-06-06 (Pull-Wave GDD authoring) to close paper-only-seam patterns flagged by the R7 design-review skill Phase 2b grep amendment.

### Interface

```cpp
// Movement state enum — pinned ordinals match PM's ERunSlipState exactly for ABI-safe static_cast.
// R7-PM-PROPAGATION-REVIEW (2026-06-11): pruned from 4 members {SETTLED, SLIPPING, BUFFERED, DEAD}
// to 2 members {SETTLED, SLIPPING} matching PM Rules 5 (buffering is an orthogonal flag, not a
// state) and 7 (DEAD freeze sets movement_state = SETTLED — see PM GDD R7-PM-PROPAGATION-REVIEW
// Rule 7 update). Adding members to either enum without updating the other is forbidden.
UENUM()
enum class EMovementState : uint8
{
    SETTLED  = 0,    // Player at rest in current_lane (also the state during DEAD freeze)
    SLIPPING = 1,    // Player mid-tween between current_lane and target_lane
};

class IPlayerMovementProvider
{
public:
    virtual ~IPlayerMovementProvider() = default;

    // Read accessors — called by Pull-Wave at LANDED-entry tick (Rule 10).
    // Game-thread only.
    virtual int32 GetCurrentLane() const = 0;
    virtual int32 GetTargetLane() const = 0;
    virtual EMovementState GetMovementState() const = 0;

    // Call from Pull-Wave on qualifying near-miss (Rule 11).
    // Production implementation forwards to PM's TriggerNearMissBeat().
    virtual void TriggerNearMissBeat() = 0;

    // NOTE: Pull-Wave does NOT subscribe to OnSlipMidpoint (R1 RC-A — removed).
    // PM fires OnSlipMidpoint for other consumers. If a future Pull-Wave consumer
    // needs this delegate, re-introduce it then. See Pull-Wave GDD R1 RC-A.
};
```

### Production Implementation

Production forwards to the actual Player Movement subsystem.

```cpp
class FPlayerMovementProvider_Production final : public IPlayerMovementProvider
{
public:
    explicit FPlayerMovementProvider_Production(UPlayerLaneMovementComponent* InPM) : PM(InPM) {}

    int32 GetCurrentLane() const override
    {
        check(IsInGameThread());
        return PM.IsValid() ? PM->GetCurrentLane() : 0;
    }
    int32 GetTargetLane() const override
    {
        check(IsInGameThread());
        return PM.IsValid() ? PM->GetTargetLane() : 0;
    }
    EMovementState GetMovementState() const override
    {
        check(IsInGameThread());
        return PM.IsValid() ? static_cast<EMovementState>(PM->GetMovementState()) : EMovementState::SETTLED;
    }
    void TriggerNearMissBeat() override
    {
        check(IsInGameThread());
        if (PM.IsValid()) { PM->TriggerNearMissBeat(); }
    }
private:
    TWeakObjectPtr<UPlayerLaneMovementComponent> PM;
};
```

### Test Stub

```cpp
class FPlayerMovementTestStub final : public IPlayerMovementProvider
{
public:
    // Test setters — configure state before Pull-Wave reads.
    void SetCurrentLane(int32 Lane) { CurrentLane = Lane; }
    void SetTargetLane(int32 Lane) { TargetLane = Lane; }
    void SetMovementState(EMovementState State) { MovementState = State; }

    // AC assertion accessor — counts TriggerNearMissBeat() calls.
    int32 GetNearMissBeatCount() const { return NearMissBeatCount; }

    void Reset()
    {
        CurrentLane = 0;
        TargetLane = 0;
        MovementState = EMovementState::SETTLED;
        NearMissBeatCount = 0;
    }

    // IPlayerMovementProvider interface
    int32 GetCurrentLane() const override { check(IsInGameThread()); return CurrentLane; }
    int32 GetTargetLane() const override { check(IsInGameThread()); return TargetLane; }
    EMovementState GetMovementState() const override { check(IsInGameThread()); return MovementState; }
    void TriggerNearMissBeat() override { check(IsInGameThread()); ++NearMissBeatCount; }

private:
    int32 CurrentLane = 0;
    int32 TargetLane = 0;
    EMovementState MovementState = EMovementState::SETTLED;
    int32 NearMissBeatCount = 0;
};
```

### GDD ACs Unblocked by This Stub

| AC | What was blocking it | What this stub provides |
|---|---|---|
| **PW AC-PW-21a/b/c** | No PM state injection for hit resolution tests | `SetMovementState`, `SetCurrentLane`, `SetTargetLane` drive Rule 10's branch coverage |
| **PW AC-PW-22** | No PM state injection for near-miss direct-read (R1 RC-A — `OnSlipMidpoint` subscription removed) | `SetMovementState(SLIPPING)`, `SetCurrentLane`, `SetTargetLane` drive Rule 11's direct `current_lane` read; `FireSlipMidpoint` is NOT invoked in this path |
| **PW AC-PW-23** | DELETED (R1 RC-A) — pre-midpoint fallback superseded by direct `current_lane` read | AC ID retained for traceability; no stub method required |
| **PW AC-PW-28** | Same-tick LANDED for multiple waves | Stub's state is independent of wave count; multiple `OnWaveHit` events validated against stub |

---

## Seam 13: `IWaveSpawnerCallback` (Pull-Wave)

### Why this seam exists

Pull-Wave's despawn pipeline (Rule 13) ordering is load-bearing: Collision unregister MUST fire before `OnWaveDespawned` broadcasts. AC-PW-15 verifies this ordering invariant; AC-PW-16/17/18/25/26 verify `OnWaveDespawned` events by `despawn_reason`. This seam captures the broadcasts in tests without requiring the real Wave Spawner instance.

Authored 2026-06-06 (Pull-Wave GDD authoring) — second seam in the Pull-Wave R7 set.

### Interface

```cpp
// Despawn reason enum (mirrors Pull-Wave's internal enum).
UENUM()
enum class EWaveDespawnReason : uint8
{
    NaturalLanding,
    RunTermination,
    PauseFlush,
};

class IWaveSpawnerCallback
{
public:
    virtual ~IWaveSpawnerCallback() = default;

    // Called by Pull-Wave at DESPAWNING entry, step 3 of the despawn pipeline.
    // Game-thread only.
    virtual void OnWaveDespawned(int32 WaveId, EWaveDespawnReason Reason, int32 FinalLane) = 0;

    // Called by Pull-Wave's Collision-unregister step (despawn pipeline step 1).
    // Production implementation forwards to Collision GDD's unregister; test stub records
    // the timestamp/order for the ordering invariant check.
    virtual void OnCollisionUnregistered(int32 WaveId) = 0;

    // Called by Pull-Wave's Telegraph-unregister step (despawn pipeline step 2).
    // R4 RC-R3-4 extension — closes the R3 RC-R3-4 finding that step (ii) was
    // unobservable from the test stub (a reversed (ii)/(iii) implementation passed
    // all prior assertions because the stub only saw steps (i) and (iii)).
    // Production implementation forwards to Telegraph System's unregister; test
    // stub records the event in the unified ordered log so AC-PW-15 can assert
    // [CollisionUnregistered, TelegraphUnregistered, WaveDespawned] sequence directly.
    virtual void OnTelegraphUnregistered(int32 WaveId) = 0;
};
```

### Production Implementation

Production forwards `OnWaveDespawned` to Wave Spawner's pool-return logic and `OnCollisionUnregistered` to Collision GDD's unregister.

```cpp
class FWaveSpawnerCallback_Production final : public IWaveSpawnerCallback
{
public:
    explicit FWaveSpawnerCallback_Production(UWaveSpawnerSubsystem* InSpawner,
                                              UCollisionSubsystem* InCollision,
                                              UTelegraphSubsystem* InTelegraph)
        : Spawner(InSpawner), Collision(InCollision), Telegraph(InTelegraph) {}

    void OnWaveDespawned(int32 WaveId, EWaveDespawnReason Reason, int32 FinalLane) override
    {
        check(IsInGameThread());
        if (Spawner.IsValid()) { Spawner->ReturnToPool(WaveId, Reason, FinalLane); }
    }

    void OnCollisionUnregistered(int32 WaveId) override
    {
        check(IsInGameThread());
        if (Collision.IsValid()) { Collision->UnregisterWave(WaveId); }
    }

    void OnTelegraphUnregistered(int32 WaveId) override
    {
        // R4 RC-R3-4: synchronous in-tick forward to Telegraph.
        // Telegraph->UnregisterWave is forbidden from deferring to next tick
        // (Pull-Wave Rule 13 same-tick atomicity contract).
        check(IsInGameThread());
        if (Telegraph.IsValid()) { Telegraph->UnregisterWave(WaveId); }
    }

private:
    TWeakObjectPtr<UWaveSpawnerSubsystem> Spawner;
    TWeakObjectPtr<UCollisionSubsystem> Collision;
    TWeakObjectPtr<UTelegraphSubsystem> Telegraph;
};
```

### Test Stub

```cpp
class FWaveSpawnerCallbackTestStub final : public IWaveSpawnerCallback
{
public:
    // --- IWaveSpawnerCallback (despawn pipeline events) ---

    void OnWaveDespawned(int32 WaveId, EWaveDespawnReason Reason, int32 FinalLane) override
    {
        check(IsInGameThread());
        ++DespawnEventCount;
        LastDespawnReason = Reason;
        LastFinalLane = FinalLane;
        DespawnEventLog.Add({WaveId, Reason, FinalLane});
        EventLog.Add({WaveId, EWaveEventType::Despawned, FinalLane, -1});
        // Capture whether OnCollisionUnregistered fired BEFORE this broadcast for the same WaveId.
        bCollisionUnregisteredBeforeBroadcast = CollisionUnregisteredWaveIds.Contains(WaveId);
        // R10c — Pull-Wave R9 qa-lead F1: fire registered user callback AFTER all standard
        // recording above is complete. Re-entrant calls from inside the user callback (e.g.
        // RSMStub.SetIsPaused(false) firing OnPausedChanged mid-tick) do not corrupt the
        // recording invariants because event log + counters + the before-broadcast capture
        // are already finalized for this wave_id at this point.
        if (OnDespawnedUserCallback)
        {
            OnDespawnedUserCallback(WaveId, Reason, FinalLane);
        }
    }

    void OnCollisionUnregistered(int32 WaveId) override
    {
        check(IsInGameThread());
        ++CollisionUnregisterCount;
        CollisionUnregisteredWaveIds.Add(WaveId);
        EventLog.Add({WaveId, EWaveEventType::CollisionUnregistered, -1, -1});
    }

    // R4 RC-R3-4 extension — Telegraph-unregister step independently observable.
    void OnTelegraphUnregistered(int32 WaveId) override
    {
        check(IsInGameThread());
        ++TelegraphUnregisterCount;
        TelegraphUnregisteredWaveIds.Add(WaveId);
        EventLog.Add({WaveId, EWaveEventType::TelegraphUnregistered, -1, -1});
    }

    // --- Pull-Wave multicast delegate handlers (R2 Cluster F extension) ---
    //
    // OnWaveHit and OnNearMiss are NOT IWaveSpawnerCallback virtual methods.
    // They are Pull-Wave-published multicast delegates that the test harness
    // binds to this stub during test setup (see Usage below).
    // The stub records them in the same ordered event log as despawn events,
    // enabling AC-PW-10's per-WaveId ordering assertion across all event types.
    //
    // Binding in test setup:
    //   PullWave->OnWaveHit.AddRaw(Stub, &FWaveSpawnerCallbackTestStub::HandleWaveHit);
    //   PullWave->OnNearMiss.AddRaw(Stub, &FWaveSpawnerCallbackTestStub::HandleNearMiss);

    void HandleWaveHit(int32 WaveId, int32 TargetLane, int32 SourceLane)
    {
        check(IsInGameThread());
        ++WaveHitCount;
        LastWaveHitId = WaveId;
        EventLog.Add({WaveId, EWaveEventType::WaveHit, TargetLane, SourceLane});
    }

    void HandleNearMiss(int32 WaveId, int32 TargetLane, int32 SlippedFromLane)
    {
        check(IsInGameThread());
        ++NearMissCount;
        LastNearMissId = WaveId;
        EventLog.Add({WaveId, EWaveEventType::NearMiss, TargetLane, SlippedFromLane});
    }

    // --- Re-entrant user callback slot (R10c — Pull-Wave R9 qa-lead F1 2026-06-17) ---
    //
    // Test-only secondary-callback slot for mid-tick cross-stub orchestration scenarios
    // (e.g. AC-PW-MID-TICK-PAUSE-DEFERRAL): the test harness registers a TFunction here
    // and Pull-Wave's despawn pipeline triggers it AFTER all standard event recording
    // (event log push, counters, bCollisionUnregisteredBeforeBroadcast capture) completes
    // for the wave, so re-entrant calls from inside the user callback (e.g. calling
    // FRSMTestStub.SetIsPaused(false) to fire OnPausedChanged mid-tick) do not corrupt
    // the recording invariants.
    //
    // NOT on the IWaveSpawnerCallback interface — production has no use for a re-entrant
    // user callback (FWaveSpawnerCallback_Production forwards to Spawner->ReturnToPool;
    // re-entrant pause toggles are a test-harness construct only).
    //
    // Binding in test setup (example — AC-PW-MID-TICK-PAUSE-DEFERRAL):
    //   Stub.SetOnDespawnedUserCallback(
    //       [&RSMStub](int32 WaveId, EWaveDespawnReason Reason, int32 FinalLane)
    //       {
    //           if (WaveId == 1 && Reason == EWaveDespawnReason::NaturalLanding)
    //           {
    //               RSMStub.SetIsPaused(false);  // fires OnPausedChanged(false) mid-tick
    //           }
    //       });
    void SetOnDespawnedUserCallback(TFunction<void(int32 /*WaveId*/, EWaveDespawnReason /*Reason*/, int32 /*FinalLane*/)> InCallback)
    {
        OnDespawnedUserCallback = MoveTemp(InCallback);
    }
    bool HasOnDespawnedUserCallback() const { return static_cast<bool>(OnDespawnedUserCallback); }

    // --- Despawn-pipeline accessors (unchanged) ---
    int32 GetDespawnEventCount() const { return DespawnEventCount; }
    EWaveDespawnReason GetLastDespawnReason() const
    {
        check(DespawnEventCount > 0 && "GetLastDespawnReason() called before any despawn recorded");
        return LastDespawnReason;
    }
    int32 GetLastFinalLane() const { return LastFinalLane; }
    bool WasCollisionUnregisteredBeforeBroadcast() const { return bCollisionUnregisteredBeforeBroadcast; }
    int32 GetCollisionUnregisterCount() const { return CollisionUnregisterCount; }

    // R4 RC-R3-4 — Telegraph-unregister observability
    int32 GetTelegraphUnregisteredCount() const { return TelegraphUnregisterCount; }

    struct FDespawnEvent { int32 WaveId; EWaveDespawnReason Reason; int32 FinalLane; };
    const TArray<FDespawnEvent>& GetDespawnEventLog() const { return DespawnEventLog; }

    // --- Hit/near-miss accessors (R2 Cluster F addition) ---
    int32 GetWaveHitCount()  const { return WaveHitCount; }
    int32 GetNearMissCount() const { return NearMissCount; }

    // Ordered event log across ALL event types (WaveHit, NearMiss, Despawned,
    // CollisionUnregistered, TelegraphUnregistered).
    // Required by AC-PW-10: broadcast order of OnWaveHit / OnNearMiss /
    // OnWaveDespawned for same-tick waves must be WaveId ASCENDING.
    // Required by AC-PW-15 (R4 RC-R3-4): despawn pipeline order
    // [CollisionUnregistered, TelegraphUnregistered, Despawned] per wave_id.
    enum class EWaveEventType : uint8 { WaveHit, NearMiss, Despawned, CollisionUnregistered, TelegraphUnregistered };
    struct FWaveEvent
    {
        int32 WaveId;
        EWaveEventType Type;
        int32 LaneA;  // TargetLane (WaveHit/NearMiss) or FinalLane (Despawned)
        int32 LaneB;  // SourceLane (WaveHit) / SlippedFromLane (NearMiss) / -1 (Despawned)
    };
    const TArray<FWaveEvent>& GetEventLogInOrder() const { return EventLog; }

    void Reset()
    {
        DespawnEventCount = 0;
        CollisionUnregisterCount = 0;
        TelegraphUnregisterCount = 0;
        LastFinalLane = -1;
        bCollisionUnregisteredBeforeBroadcast = false;
        CollisionUnregisteredWaveIds.Reset();
        TelegraphUnregisteredWaveIds.Reset();
        DespawnEventLog.Reset();
        WaveHitCount  = 0;
        NearMissCount = 0;
        LastWaveHitId   = -1;
        LastNearMissId  = -1;
        EventLog.Reset();
        // R10c — Pull-Wave R9 qa-lead F1: clear re-entrant user callback so successive test
        // cases do not inherit a stale callback from a prior case.
        OnDespawnedUserCallback = nullptr;
    }

private:
    // Despawn-pipeline fields
    int32 DespawnEventCount = 0;
    int32 CollisionUnregisterCount = 0;
    int32 TelegraphUnregisterCount = 0;  // R4 RC-R3-4
    EWaveDespawnReason LastDespawnReason{};
    int32 LastFinalLane = -1;
    bool bCollisionUnregisteredBeforeBroadcast = false;
    TSet<int32> CollisionUnregisteredWaveIds;
    TSet<int32> TelegraphUnregisteredWaveIds;  // R4 RC-R3-4
    TArray<FDespawnEvent> DespawnEventLog;

    // Hit/near-miss fields (R2 Cluster F addition)
    int32 WaveHitCount  = 0;
    int32 NearMissCount = 0;
    int32 LastWaveHitId   = -1;
    int32 LastNearMissId  = -1;
    TArray<FWaveEvent> EventLog;  // ordered across all event types

    // R10c — Pull-Wave R9 qa-lead F1: re-entrant user callback fired from inside
    // OnWaveDespawned AFTER standard recording. Test-only; nullptr by default.
    TFunction<void(int32, EWaveDespawnReason, int32)> OnDespawnedUserCallback;
};
```

### GDD ACs Unblocked by This Stub

| AC | What was blocking it | What this stub provides |
|---|---|---|
| **PW AC-PW-15** | No way to verify 3-step despawn pipeline ordering (R3 RC-R3-4: step (ii) Telegraph unregister was unobservable) | `GetEventLogInOrder()` records `[CollisionUnregistered, TelegraphUnregistered, Despawned]` directly per wave_id (R4 RC-R3-4 ext.); plus `WasCollisionUnregisteredBeforeBroadcast()` + counters for backward compat |
| **PW AC-PW-16** | No way to verify PauseFlush despawn_reason | `GetLastDespawnReason() == PauseFlush` after `OnPausedChanged(false)` |
| **PW AC-PW-17** | No way to distinguish RunTermination from PauseFlush on stale-pause-ABORTED | `GetLastDespawnReason() == RunTermination` after ABORTED unfreeze drain |
| **PW AC-PW-25** | No way to verify forced-despawn = no hit events | Combined with delta against Pull-Wave's OnWaveHit count |
| **PW AC-PW-26** | Same as AC-PW-17 but for distinguishability AC | Same |
| **PW AC-PW-28** | Same-tick concurrent landing event count | `GetDespawnEventCount()` reaches 2; `GetWaveHitCount()` reaches 2 (R2 ext.) |
| **PW AC-PW-21a/b/c** | No OnWaveHit / OnNearMiss observation infrastructure | `GetWaveHitCount()` / `GetNearMissCount()` via delegate binding (R2 ext.) |
| **PW AC-PW-10** | Broadcast-order assertion across event types required ordered log | `GetEventLogInOrder()` captures WaveHit / NearMiss / Despawned in arrival order (R2 ext.) |
| **PW AC-PW-MID-TICK-PAUSE-DEFERRAL** | No way to fire a re-entrant `OnPausedChanged(false)` mid-tick using only declared seam ops (the R8 RC-A rewrite cited a binding mechanism that was paper-only — R9 qa-lead F1) | `SetOnDespawnedUserCallback(TFunction<...>)` slot fired from inside `OnWaveDespawned` after standard recording (R10c ext.); test registers a callback that calls `FRSMTestStub.SetIsPaused(false)` on wave_id 1 `NaturalLanding`, producing the mid-tick `OnPausedChanged(false)` fire AFTER wave_id 1's despawn pipeline completes but BEFORE waves 2 and 3 are iterated |

---

## Wiring Summary

All six **Input System** interfaces are injected at IS construction and never swapped at runtime:

```cpp
// Shipping build — called from FSlipstormGameModule::StartupModule() (see ADR-0003 §Owner).
// IModuleInterface has no GetWorld() — and UGameInstance does not exist yet at module
// load time anyway. FRSMRunStateProvider resolves the subsystem lazily on first
// GetCurrentState() call (see its definition above); no UGameInstance pointer is
// required at construction.
auto IS = MakeUnique<FInputSystem>(
    MakePlatformClock(),                                  // IMonotonicClock
    MakeUnique<FTouchRadiusCache>(),                      // ITouchRadiusProvider
    MakeUnique<FRSMRunStateProvider>(),                   // IRunStateProvider (no arg — lazy)
    MakeUnique<FHapticPlatformPlugin>(),                  // IHapticDispatch
    MakeUnique<FWidgetVisualDispatch>(InputFeedbackOverlayWidget), // IVisualDispatch
    MakeUnique<FLocalStorageBootFlagStore>()              // IBootFlagStore
);

// Test build — MakeUnique<> + raw observer pointers (TUniquePtr ownership model)
auto ClockOwned    = MakeUnique<FFakeMonotonicClock>();      auto* Clock    = ClockOwned.Get();
auto RadiusOwned   = MakeUnique<FTouchRadiusProviderStub>(); auto* Radius   = RadiusOwned.Get();
auto RunStateOwned = MakeUnique<FRunStateProviderStub>();    auto* RunState = RunStateOwned.Get();
auto HapticOwned   = MakeUnique<FHapticDispatchStub>();      auto* Haptic   = HapticOwned.Get();
auto VisualOwned   = MakeUnique<FVisualDispatchStub>();      auto* Visual   = VisualOwned.Get();
auto BootFlagOwned = MakeUnique<FBootFlagStoreStub>();       auto* BootFlag = BootFlagOwned.Get();
auto IS = MakeUnique<FInputSystem>(
    MoveTemp(ClockOwned), MoveTemp(RadiusOwned), MoveTemp(RunStateOwned),
    MoveTemp(HapticOwned), MoveTemp(VisualOwned), MoveTemp(BootFlagOwned)
);
// Use raw observer pointers (Clock, Radius, BootFlag, etc.) to control stubs after construction
```

## ADR Dependencies

| Seam | Depends on |
|---|---|
| `IMonotonicClock` (iOS) | `mach_continuous_time()` — system API, no ADR |
| `IMonotonicClock` (Android) | `CLOCK_BOOTTIME` — system API, no ADR |
| `ITouchRadiusProvider` | ADR-0001 (palm rejection plugin) — must be implemented first |
| `SimulateTouch` seam | ADR-0003 (drain queue architecture) — part of IS implementation scope |
| `IRunStateProvider` | RSM GDD interface contract — no ADR required; follows RSM GDD spec |
| `IHapticDispatch` | ADR-0002 (haptic platform bridge) — must be implemented first |
| `IVisualDispatch` | `docs/architecture/visual-dispatch-contract.md` — full contract there |
| `IRSMTimeStateProvider` (Seam 7, DPC) | RSM GDD interface contract — no ADR required; follows RSM `remaining_time` + `RUN_DURATION_S` contract |
| `IDPCSnapshotConsumer` (Seam 8, DPC) | DPC GDD Rule 17 (mid-run consumer subscription protocol) — no ADR required; test-only abstraction |
| `IDPCAbortDelegate` (Seam 9, DPC) | DPC GDD Rule 14 cap-vs-barrage + EC-13 watchdog; RSM 5-item forward contract item 4 (RequestAbort external entry); no ADR required |
| `IWaveSpawnerPoolMetadataProvider` (Seam 10, DPC + Wave Spawner) | DPC GDD Rule 14 cap-vs-barrage check; Wave Spawner GDD pool composition contract — no ADR required; follows Wave Spawner pool manifest contract when Wave Spawner GDD is authored |
| `ICurveProvider` (Seam 11, DPC) | Test-only abstraction for AC-08c sub-case (iv); no ADR required |
| `IPlayerMovementProvider` (Seam 12, Pull-Wave) | Pull-Wave GDD Rule 10 hit resolution + Rule 11 near-miss source-lane recording; follows PM GDD interface contract (post 6-lane revision) — no ADR required |
| `IWaveSpawnerCallback` (Seam 13, Pull-Wave) | Pull-Wave GDD Rule 13 despawn pipeline (Collision-unregister-before-broadcast ordering) + OnWaveDespawned dispatch; no ADR required |

## GDD ACs Unblocked by This Document

| AC | What was blocking it | What this doc provides |
|---|---|---|
| AC-04 | No programmatic touch injection path | `SimulateTouch(x_native_px, radius_mm)` |
| AC-07 | No way to advance clock 180ms in test | `FFakeMonotonicClock.AdvanceMs()` |
| AC-08 | No way to control touch timing | `FFakeMonotonicClock` + `SimulateTouchUp()` |
| AC-09 | No way to sequence hold → release → new tap | `FFakeMonotonicClock` + `SimulateTouchUp()` + `SimulateTouch()` |
| AC-10 | No same-tick touch injection | `SimulateSameTickTouches()` |
| AC-11 | No same-tick touch injection | `SimulateSameTickTouches()` |
| AC-12 | No controlled rapid-tap injection | `SimulateTouch()` × 3 with `FFakeMonotonicClock` |
| AC-13 | No RSM state control in tests | `FRunStateProviderStub.SetState()` |
| AC-16 | No 60Hz tick delta measurement control | `FFakeMonotonicClock` + `SimulateSameTickTouches()` |
| AC-17 | No way to simulate suspend and 180ms elapsed | `FFakeMonotonicClock.AdvanceMs(200)` after app-resume signal |
| AC-19 | RSM state needed to test NONE-tier suppression | `FRunStateProviderStub.SetState(ERunState::DEAD)` |
| AC-19b | No `IVisualDispatch` contract or stub | See `docs/architecture/visual-dispatch-contract.md` |
| AC-19c | No `IVisualDispatch` contract or stub | `FVisualDispatchStub.Reset()` for per-state isolation |
| **DPC AC-12** | No `remaining_time` step injection | `FRSMTestStub.AdvanceRemainingTime()` (Seam 7) |
| **DPC AC-13** | No frame-hitch jump simulation | `FRSMTestStub.SetRemainingTime()` (Seam 7) |
| **DPC AC-14a** | No mid-run Rule 17-compliant consumer | `FConsumerTestStub` (Seam 8) |
| **DPC AC-14b** | No `bFirstReadComplete` sentinel + transition counter | `FConsumerTestStub.GetTransitionHandlerFireCount()` (Seam 8) |
| **DPC AC-14c** | No `is_paused` mid-test toggle | `FRSMTestStub.SetIsPaused()` (Seam 7) + Seam 8 consumer |
| **PW AC-PW-16** | No `OnPausedChanged` delegate for pause-freeze drive | `FRSMTestStub.SetIsPaused()` broadcasts `OnPausedChanged` only when `current_state == RUNNING` (Seam 7 R2 ext.) |
| **PW AC-PW-17a/b** | No atomic ABORTED + is_paused-clear without delegate fire | `FRSMTestStub.SetCurrentStateWithPauseClear(ABORTED)` (Seam 7 R2 ext.) |
| **DPC AC-21** | No `current_state` mid-run transition control | `FRSMTestStub.SetCurrentState(ABORTED)` (Seam 7) |
| **DPC AC-NAN-GUARD** | No NaN `remaining_time` injection | `FRSMTestStub.SetRemainingTime(std::nan(""))` (Seam 7) |
| **DPC AC-RUN-DURATION-ZERO** | No `RUN_DURATION_S = 0.0` injection at Initialize() | `FRSMTestStub.SetRunDurationS(0.0)` (Seam 7) |
| **DPC AC-ASYNC-LOAD-RACE** | No `IDPCAbortDelegate` mock for watchdog abort | `FDPCAbortTestStub.GetLastAbortReason() == AsyncLoadTimeout` (Seam 9) |
| **DPC AC-CAP-BARRAGE-COMPATIBILITY** | No `IWaveSpawnerPoolMetadataProvider` mock + no abort recorder | `FWaveSpawnerPoolMetadataTestStub.SetHasBarrageInPeakPool(true)` (Seam 10) + `FDPCAbortTestStub` (Seam 9) |
| **DPC AC-08c sub-case (iv)** | No `UCurveFloat::GetFloatValue() → NaN` injection from finite t_norm | `FCurveProviderTestStub.SetReturnValue(std::nan(""))` (Seam 11) |
| **PW AC-PW-21a/b/c** | No PM state injection for hit resolution | `FPlayerMovementTestStub.SetMovementState/SetCurrentLane/SetTargetLane` (Seam 12) |
| **PW AC-PW-22** | No PM state injection for near-miss direct-read (R1 RC-A — OnSlipMidpoint subscription removed) | `FPlayerMovementTestStub.SetMovementState/SetCurrentLane/SetTargetLane` (Seam 12) |
| **PW AC-PW-23** | No way to verify TriggerNearMissBeat invocation | `FPlayerMovementTestStub.GetNearMissBeatCount()` (Seam 12) |
| **PW AC-PW-15** | No way to verify Collision-unregister-before-broadcast ordering | `FWaveSpawnerCallbackTestStub.WasCollisionUnregisteredBeforeBroadcast()` (Seam 13) |
| **PW AC-PW-16/17/26** | No PauseFlush vs RunTermination despawn_reason distinguishability | `FWaveSpawnerCallbackTestStub.GetLastDespawnReason()` (Seam 13) |
| **PW AC-PW-25/28** | No way to count concurrent despawns / verify forced-despawn no-event | `FWaveSpawnerCallbackTestStub.GetDespawnEventCount()` (Seam 13) |
| **PW AC-PW-21a/b/c** | No OnWaveHit / OnNearMiss observation infrastructure | `FWaveSpawnerCallbackTestStub.GetWaveHitCount()` / `GetNearMissCount()` via Pull-Wave delegate binding (Seam 13 R2 ext.) |
| **PW AC-PW-10** | Broadcast-order assertion needs ordered cross-event log | `FWaveSpawnerCallbackTestStub.GetEventLogInOrder()` (Seam 13 R2 ext.) |

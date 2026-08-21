# ADR-0001: Palm Rejection Threshold (R_max) and Raw Touch Contact Radius Access

## Status
Proposed

## Date
2026-05-14

## Engine Compatibility

| Field | Value |
|-------|-------|
| **Engine** | Unreal Engine 5.7 |
| **Domain** | Input |
| **Knowledge Risk** | HIGH — UE 5.4–5.7 are post-LLM-cutoff (May 2025) |
| **References Consulted** | `docs/engine-reference/unreal/VERSION.md`, `docs/engine-reference/unreal/modules/input.md`, `docs/engine-reference/unreal/deprecated-apis.md` |
| **Post-Cutoff APIs Used** | Platform bridge APIs for iOS (`UITouch.majorRadius`) and Android (`MotionEvent.getTouchMajor()`) — verify against UE 5.7 mobile platform source before implementing |
| **Verification Required** | Confirm `FIOSPlatformInputInterface` subclassing pattern is unchanged in UE 5.7; confirm `GameActivity` override path is unchanged in UE 5.7 Android SDK |

## ADR Dependencies

| Field | Value |
|-------|-------|
| **Depends On** | None |
| **Enables** | Input System GDD rewrite (unblocks prerequisite 1 of 4 for IS Review 6) |
| **Blocks** | Input System implementation — F-2 (PALM_REJECTION) cannot be implemented without an Accepted R_max |
| **Ordering Note** | Must be Accepted before the Input System GDD can be marked Accepted |

## Context

### Problem Statement

The Input System GDD's palm rejection formula (F-2) uses `R_max = 2mm` as the threshold separating palm touches from finger touches. With the corrected platform-specific formulas, `R_max = 2mm` rejects 100% of typical finger contacts on both iOS and Android, making the game unplayable as specified. Additionally, Unreal Engine 5.7's Enhanced Input system does not propagate touch contact radius through `FPointerEvent`, so raw radius data must be accessed via a platform-specific bridge before it reaches the UE5 input pipeline.

### Constraints

- Target platforms: iOS (iPhone) and Android (flagship tier); portrait orientation only
- SLIPSTORM's only gameplay verb is slip — palm rejection correctness is critical; false positives block gameplay entirely
- UE 5.7 Enhanced Input does not expose per-finger contact radius — platform bridge required
- No engine source modification allowed (forked engine creates unsustainable maintenance burden across minor version updates)
- The Input System GDD rewrite is blocked until R_max is an Accepted ADR decision

### Requirements

- R_max must pass all typical index finger contacts (radius ≤ 5.5mm on target devices)
- R_max must reject palm-edge contacts (radius ≥ 8.5mm on target devices)
- Contact radius must be readable per-finger per-frame without engine source modification
- The bridge interface must be injectable for unit testing without real hardware
- The threshold must be configurable as a tuning knob post-ship without code changes

## Decision

### Threshold Value

**R_max = 6.0mm**

Derived from published iOS/Android touch research:

| Contact type | Radius range | Percentile basis |
|---|---|---|
| Adult index finger tip | 3.0 – 5.5mm | 95th percentile upper bound |
| Palm-edge contact | 8.5 – 14.0mm | 5th percentile lower bound |
| **Safety gap** | **3.0mm** | (8.5 − 5.5) |

`R_max = 6.0mm` sits at the midpoint of the safety gap: 0.5mm above the finger 95th percentile and 2.5mm below the palm 5th percentile.

Platform-specific formulas for computing `r_mm` from raw platform values (matches GDD F-2):

```
iOS:     r_mm = majorRadius × nativeScale ÷ screen_ppi × 25.4
Android: r_mm = getTouchMajor() ÷ 2 ÷ screen_dpi_per_mm
```

### Contact Radius Access Mechanism

**Platform bridge via UE5 native plugin — no engine source modification:**

```
FTouchRadiusBridgePlugin
├── iOS:     Method swizzling on IOSView (UIView subclass that receives touch events)
│            swizzles touchesBegan:withEvent: — touch events go to UIView, NOT
│            UIApplicationDelegate; intercepting on IOSAppDelegate silently misses all events
│            reads UITouch.majorRadius and UITouch.type (UITouchTypeDirect check)
│            writes {radius_mm, is_direct} to FTouchRadiusCache
├── Android: Java GameActivity subclass
│            intercepts dispatchTouchEvent
│            MUST call super.dispatchTouchEvent(event) FIRST before reading the event;
│            return the super's return value — do not return true unconditionally
│            reads MotionEvent.getTouchMajor() and MotionEvent.getToolType()
│            writes {radius_mm, is_direct} to FTouchRadiusCache
└── Editor:  FTouchRadiusProviderStub (injectable fixed-radius stub for tests)
```

**`FTouchRadiusCache` slot schema:** Each slot stores `{float radius_mm, bool is_direct}`.

| Field | iOS source | Android source | Sentinel |
|---|---|---|---|
| `radius_mm` | `UITouch.majorRadius × nativeScale ÷ screen_ppi × 25.4` | `MotionEvent.getTouchMajor() ÷ 2 ÷ screen_dpi_per_mm` | `−1.0f` if no data |
| `is_direct` | `UITouch.type == UITouchTypeDirect` | `MotionEvent.getToolType() == TOOL_TYPE_FINGER ∥ TOOL_TYPE_UNKNOWN` | `false` if no data |

**Stylus rejection using `is_direct`:** The IS checks `is_direct` before evaluating F-2.
If `is_direct == false`, the contact is rejected silently and `IS_STYLUS_REJECTED` is logged.
This cannot be done by comparing `UITouch.type` inside `FPointerEvent` — UE5's
`FPointerEvent` does not carry the iOS touch type. The bridge must write `is_direct` to
the cache alongside `radius_mm` and read both per contact.

The Input System reads from `FTouchRadiusCache` (lock-free ring buffer keyed by `OS_FingerIndex`)
**at enqueue time** — inside `FInputSystem::HandleTouchStartedEvent()`, before the event
is pushed to `PendingEvents`. The radius value is stored in `FPendingTouchEvent.radius_mm`
and is NOT re-read at drain time. Reading at enqueue time ensures each event captures the
radius value written by the native bridge for that specific touch contact. By drain time
(the next `DrainTick()` call), the bridge may have overwritten the slot for the same
`OS_FingerIndex` with a subsequent touch event, yielding stale or wrong-contact data.

**FingerIndex mapping contract:** `FingerIndex` in `GetRadiusMM(int32 FingerIndex)` must
match `FPointerEvent::GetPointerIndex()` exactly — this is the same integer Unreal uses
to identify concurrent touch points in Enhanced Input. On iOS, `UITouch*` pointers have
no numeric ID; the bridge assigns indices by sorting the active `UITouch*` set by pointer
address (`NSSet` sorted ascending), matching the order Unreal's IOSView assigns
`GetPointerIndex()`. The bridge and IS must use identical sort logic; any divergence
silently reads wrong-finger radius.

**FingerIndex sort verification gate (pre-implementation):** Before bridge implementation,
verify on UE 5.7 IOSView that `GetPointerIndex()` uses ascending-pointer-address sort for
concurrent touches. Inject two simultaneous touches with distinct radii (5.0mm and 2.0mm)
via `SimulateSameTickTouches()`; verify in the `IS_TOUCH_RECEIVED` debug log that each
`contact_id` receives the correct radius. This gate must pass before the bridge can be
merged. If the sort order differs from ascending pointer address, update the bridge to
match the UE 5.7 actual sort order and document the divergence here.

### Key Interfaces

```cpp
// Cache slot — written by FTouchRadiusBridgePlugin (native ObjC/Java callback, game thread
// in iOS, UI thread in Android JNI path), read by FInputSystem::HandleTouchStartedEvent
// (game thread, Slate callback). Because the writer and reader may run on different threads
// (Android bridge calls can arrive from the JNI/Java thread), each slot must use
// std::atomic with at least memory_order_release (write) / memory_order_acquire (read)
// to prevent the reader from observing a partial write. On iOS the bridge fires on the
// game thread via IOSView dispatch queue, but the atomic remains for correctness across
// platform variation.
//
// IMPORTANT: do NOT use relaxed ordering — the composite {radius_mm, is_direct} update
// is not a single machine word on all architectures; a raw struct copy without atomics
// risks a torn read where radius_mm is new and is_direct is stale (or vice versa).
// Use per-field atomics or a lock to guarantee both fields are read consistently.
struct FTouchRadiusSlot
{
    std::atomic<float>    radius_mm{-1.0f};       // -1.0f = sentinel (no data). Acquire/release.
    std::atomic<bool>     is_direct{false};        // false = no data (treat as stylus → reject)
    std::atomic<uint64>   last_write_time_ms{0};   // IMonotonicClock::NowMs() at bridge write time.
                                                   // 0 = slot never written. Used for 2s TTL check.
};

// Public injectable interface — stub in editor/test, platform impl in shipping
class ITouchRadiusProvider
{
public:
    // Returns {radius_mm, is_direct} for the given OS FingerIndex.
    // radius_mm = -1.0f if FTouchRadiusCache has no entry (bridge race or uninitialized).
    // is_direct = false if cache has no entry (treat as stylus → reject).
    virtual FTouchRadiusSlot GetRadiusAndDirect(int32 OS_FingerIndex) const = 0;
    virtual ~ITouchRadiusProvider() = default;
};

// IS member (injected at construction)
TUniquePtr<ITouchRadiusProvider> TouchRadiusProvider;

// Tuning knob — set via config, not hardcoded
static constexpr float PALM_REJECTION_R_MAX_MM = 6.0f; // safe range [4.0, 8.0]

// F-2 evaluation inside IS drain tick
const auto [r_mm, is_direct] = TouchRadiusProvider->GetRadiusAndDirect(contact.OS_FingerIndex);

// Stylus/indirect rejection (before radius check)
if (!is_direct)
{
    LogStylusRejected(contact.contact_id);  // IS_STYLUS_REJECTED, silent to player
    return;
}

// Radius rejection (-1.0f = no data → treat as pass)
const bool is_palm = (r_mm >= 0.0f) && (r_mm > PALM_REJECTION_R_MAX_MM);

// Test stub
class FTouchRadiusProviderStub : public ITouchRadiusProvider
{
    float FixedRadiusMM = 3.5f;
    bool  bIsDirect     = true; // default: simulate finger contact
public:
    FTouchRadiusSlot GetRadiusAndDirect(int32) const override
    {
        return { FixedRadiusMM, bIsDirect };
    }
    void SetRadius(float mm) { FixedRadiusMM = mm; }
    void SetIsDirect(bool bDirect) { bIsDirect = bDirect; }
};
```

### Architecture Diagram

```
iOS UITouch.majorRadius / Android MotionEvent.getTouchMajor()
         │
         ▼
FTouchRadiusBridgePlugin (UE5 native plugin — ObjC + Java)
         │  writes per-frame, per-finger (atomic, lock-free)
         ▼
FTouchRadiusCache (ring buffer, 10 slots × 4 bytes)
         │  read at drain tick start
         ▼
ITouchRadiusProvider (injected into IS constructor)
         │
         ▼
Input System F-2 (PALM_REJECTION) ← R_max = 6.0mm
         │
         ▼
Slip-left / Slip-right dispatch
```

## Alternatives Considered

### Alternative 1: Empirical-First (block GDD rewrite until hardware tested)

- **Description**: Do not set R_max until measured on real target devices; this ADR remains Proposed until measurements are taken
- **Pros**: Maximum accuracy — no population-level assumptions
- **Cons**: Indefinitely blocks the Input System GDD rewrite and all downstream systems; requires hardware availability before design proceeds; the bottleneck is hardware access, not design confidence
- **Rejection Reason**: Published touch research provides sufficient confidence for a design-phase decision. Hardware verification is captured as a mandatory ship gate — the decision is deferred to shipping, not to authoring

### Alternative 2: Dynamic Calibration at Session Start

- **Description**: Sample the first detected touch contact radius at `RUNNING` entry; set `R_max = first_contact_radius × 1.8`
- **Pros**: Self-calibrates per-user and per-device; handles OEM variability automatically
- **Cons**: First touch must not be a palm (edge case: user picks up phone with palm); adds 1–3 frame warm-up period; R_max changes if user switches hands mid-run; couples the Input System to RSM state; significantly increases IS complexity and test surface
- **Rejection Reason**: Complexity disproportionate to the problem; static threshold with 3mm safety margin is sufficient, simpler to specify, and simpler to test

### Alternative 3: Engine Source Modification

- **Description**: Modify UE5 source to propagate `UITouch.majorRadius` / `MotionEvent.getTouchMajor()` through `FPointerEvent` to Enhanced Input
- **Pros**: Cleanest integration — no separate cache or bridge; works transparently with Enhanced Input's existing data path
- **Cons**: Forked engine requires re-applying the patch on every UE minor version update; blocks any future Epic-initiated touch input changes; Epic PR process for upstreaming is uncertain
- **Rejection Reason**: Long-term maintenance cost is prohibitive; native plugin achieves the same result without forking

## Consequences

### Positive

- Unblocks Input System GDD rewrite (prerequisite 1 of 4 for IS Review 6)
- `R_max = 6.0mm` passes all typical index finger contacts; palm rejection becomes functional
- `ITouchRadiusProvider` injection makes the Input System fully unit-testable without real hardware
- 3.0mm safety gap handles moderate outlier users without hardware-specific tuning
- Plugin approach avoids engine fork and associated version-lock risk

### Negative

- Users with index finger tip radius > 6.0mm (outside the 95th percentile) will have all touches rejected as palm — unplayable for this population (~5%)
- Requires a native plugin with platform-specific code (ObjC + Java); increases build complexity when adding new platform targets
- `FTouchRadiusCache` introduces cross-thread state requiring careful synchronization

### Risks

- **Risk**: `UITouch.majorRadius` API behavior changed in a post-cutoff iOS version; bridge produces wrong values
  **Mitigation**: Mandatory hardware verification gate (see Validation Criteria); bridge ships with a diagnostic mode logging raw platform values vs. computed mm values

- **Risk**: Android OEM variability in `getTouchMajor()` calibration; some devices return larger-than-expected values for fingers
  **Mitigation**: Hardware gate tests ≥2 Android devices (flagship + mid-range); OEM-specific `R_max` override table documented as an extension point in `FTouchRadiusCache`

- **Risk**: `FTouchRadiusCache` race condition corrupts radius data during rapid multi-touch
  **Mitigation**: Cache uses `std::atomic` per-field writes with `memory_order_release` / `memory_order_acquire` — one writer per finger index (platform bridge), one reader per IS enqueue call (game thread Slate callback). Both fields (`radius_mm`, `is_direct`) are updated independently; tests must verify both are read correctly for the same touch event.

- **Risk**: Missed OS touch-up leaves stale radius data in `FTouchRadiusCache` slot permanently
  **Mitigation**: Each cache slot stores a `last_write_time_ms` (from `IMonotonicClock::NowMs()`, written alongside `radius_mm` and `is_direct` at bridge callback time). Any slot not updated within **2000ms** is invalidated at enqueue time (sentinel values restored: `radius_mm = -1.0f`, `is_direct = false`). The IS treats the invalidated slot as a cache miss (pass — never reject on absence of data). Stale invalidation is logged as `IS_STALE_SLOT_INVALIDATED` in debug builds.

## GDD Requirements Addressed

| GDD System | Requirement | How This ADR Addresses It |
|---|---|---|
| input-system.md | F-2 (PALM_REJECTION): `r_mm > R_max` → reject contact | Sets `R_max = 6.0mm` with corrected formula for each platform |
| input-system.md | Tuning Knob: `PALM_REJECTION_R_MAX_MM`, safe range [4.0, 8.0] | Documents calibration rationale for the safe range |
| input-system.md | F-2 contact radius source on iOS and Android | Documents `FTouchRadiusBridgePlugin` as the only viable path without engine fork |
| input-system.md | Palm rejection testability (AC requirement) | `ITouchRadiusProvider` injection enables stub-based unit tests without hardware |

## Performance Implications

- **CPU**: `FTouchRadiusCache` read is O(1) atomic load per contact per drain tick — negligible
- **Memory**: Ring buffer: 10 slots × 4 bytes = 40 bytes — negligible
- **Load Time**: Plugin startup registers the bridge during `Module::StartupModule()` — <1ms
- **Network**: N/A

## Migration Plan

Applies to new implementation only — no existing shipping code to migrate.

Implementation sequence:
1. Create `FTouchRadiusBridgePlugin` as a UE5 native plugin (ObjC + Java + C++ header)
2. Register platform bridges via `Module::StartupModule()`
3. Inject `ITouchRadiusProvider` into IS constructor (use `FTouchRadiusProviderStub` in editor builds)
4. Replace any hardcoded radius logic in F-2 with `TouchRadiusProvider->GetRadiusAndDirect(contact.OS_FingerIndex)` — note the correct method name is `GetRadiusAndDirect` (returns `FTouchRadiusSlot`), not `GetRadiusMM`; read at enqueue time inside `HandleTouchStartedEvent`, not at drain time

## Validation Criteria

### Hardware Verification Gate (required before Status → Accepted)

Run on each target device before shipping:

| Device | PPI | nativeScale | Test: 20 finger taps | Test: 10 palm presses | Pass condition |
|---|---|---|---|---|---|
| iPhone 16 | 460 | 3 | measure r_mm each | measure r_mm each | 0% fingers rejected, 100% palms rejected |
| Samsung Galaxy S24 | 500 | — | measure r_mm each | measure r_mm each | 0% fingers rejected, 100% palms rejected |

If any finger contact produces `r_mm > 6.0mm`: revise `R_max` upward before shipping.
If any palm contact produces `r_mm < 6.0mm`: revise `R_max` downward before shipping.

### Unit Test Gate (required before implementation begins)

```
FTouchRadiusProviderStub.SetRadius(5.9f) → F-2 passes contact  (expected: slip dispatched)
FTouchRadiusProviderStub.SetRadius(6.1f) → F-2 rejects contact (expected: no dispatch)
FTouchRadiusProviderStub.SetRadius(0.0f) → F-2 passes contact  (zero radius = no data → don't reject)
```

## Related Decisions

- `design/gdd/input-system.md` — Formula F-2 (PALM_REJECTION), Tuning Knob PALM_REJECTION_R_MAX_MM
- Input System prerequisites 2–4: platform seam interfaces, `design/ux/input-feedback.md`, haptic degradation ADR (parallel tracks)

---

*Status becomes Accepted when the hardware verification gate passes and results are appended to this file. Until then: Proposed.*

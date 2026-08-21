# ADR-0002: Haptic Platform Bridge and HAPTIC_CAPABILITY Detection

## Status
Proposed

## Date
2026-05-14

## Last Verified
2026-06-26 (amendment INT-002 — `EHapticEvent::NearMiss` enum value + platform patterns inscribed; PM R11a-12 forward contract propagated; architecture-review 2026-06-25)

## Engine Compatibility

| Field | Value |
|-------|-------|
| **Engine** | Unreal Engine 5.7 |
| **Domain** | Input |
| **Knowledge Risk** | HIGH — UE 5.4–5.7 are post-LLM-cutoff (May 2025) |
| **References Consulted** | `docs/engine-reference/unreal/VERSION.md`, `docs/engine-reference/unreal/modules/input.md`, `docs/engine-reference/unreal/deprecated-apis.md` |
| **Post-Cutoff APIs Used** | iOS `CHHapticEngine` (iOS 13+), `UIImpactFeedbackGenerator` (iOS 10+); Android `VibrationEffect` (API 26+), `Vibrator.hasAmplitudeControl()` (API 26+) — verify against UE 5.7 iOS/Android module source before implementing |
| **Verification Required** | Confirm `UHapticFeedbackComponent` is confirmed absent from UE5 mobile stack; confirm `VibrationEffect` JNI access pattern is unchanged in the UE 5.7 Android NDK toolchain |

## ADR Dependencies

| Field | Value |
|-------|-------|
| **Depends On** | ADR-0001 (establishes FTouchRadiusBridgePlugin native-plugin pattern; `FHapticPlatformPlugin` follows the same structural pattern) |
| **Enables** | Input System GDD rewrite (unblocks prerequisite 4 of 4 for IS Review 6); Player Movement implementation (PM fires slip-confirmed and buffer-drop via this bridge) |
| **Blocks** | IS implementation — HAPTIC_CAPABILITY detection cannot proceed without an Accepted bridge design; PM implementation — same |
| **Ordering Note** | This ADR must be Accepted before IS or PM enters the implementation sprint. `input-feedback.md` (prerequisite 3) specifies the audio/visual fallback specs that this bridge dispatches. |

## Context

### Problem Statement

The Input System and Player Movement both fire haptic feedback events on mobile, but
Unreal Engine 5.7 has no C++ surface for mobile haptics: `UHapticFeedbackComponent`
is a gamepad force-feedback abstraction that does not exist as a mobile API.
Additionally, Android haptic capability varies widely across OEM devices — detecting
the correct tier (`FULL/DURATION/AUDIO/VISUAL/NONE`) requires JNI calls with no UE5
equivalent. Without a shared platform bridge, both IS and PM would duplicate platform-
specific code, and the capability detection logic would have no single authoritative source.

### Constraints

- Target platforms: iOS and Android (mobile only); no desktop haptics required
- `UHapticFeedbackComponent` must not be used for mobile haptics — it is a gamepad API
- IS and PM both fire haptics; the bridge must be shared between them (no duplication)
- The bridge interface must be injectable for unit testing (consistent with ADR-0001)
- `HAPTIC_CAPABILITY` must be determined once at app init and cached — not queried per-event
- Android minimum API level: **26 (Android 8.0)** — UE 5.7 enforced minimum (per `docs/engine-reference/unreal/breaking-changes.md` §Mobile). `VibrationEffect` and `hasAmplitudeControl()` are available from API 26, so the bridge can rely on them unconditionally.
- iOS minimum: **iOS 14** — UE 5.7 enforced minimum (per `docs/engine-reference/unreal/breaking-changes.md` §Mobile). `CHHapticEngine.capabilitiesForHardware()` has been available since iOS 13; iOS 14 inherits unchanged.

### Requirements

- Must fire the correct platform haptic pattern for each of the four haptic events
- Must degrade gracefully across all `HAPTIC_CAPABILITY` tiers (FULL → DURATION → AUDIO → VISUAL → NONE)
- Must detect `HAPTIC_CAPABILITY` at app init using available platform APIs
- Must dispatch audio fallback cues and visual fallback indicators as specified in `design/ux/input-feedback.md`
- Must be injectable as a test stub in non-shipping builds (IS and PM unit tests)
- Must not query `HAPTIC_CAPABILITY` per-event — detection result is cached at init

## Decision

### Architecture: Shared UE5 Native Plugin (`FHapticPlatformPlugin`)

A shared UE5 native plugin provides:

1. **`IHapticDispatch`** — injectable abstract interface consumed by IS and PM
2. **`FHapticPlatformPlugin`** — platform-specific implementations (iOS ObjC + Android Java/JNI)
3. **`FHapticDispatchStub`** — test stub with call recording for unit tests

```
FHapticPlatformPlugin
├── iOS:     ObjC wrapper around UIImpactFeedbackGenerator + UINotificationFeedbackGenerator
│            CHHapticEngine.capabilitiesForHardware() for hardware detection
├── Android: JNI wrapper around android.os.Vibrator + VibrationEffect
│            Vibrator.hasAmplitudeControl() + API level check for detection
└── Stub:    FHapticDispatchStub — records calls; returns configurable fake capability
```

### `HAPTIC_CAPABILITY` Detection Logic

Detection runs once at `Module::StartupModule()` and caches the result:

```
iOS detection:
  1. CHHapticEngine.capabilitiesForHardware().supportsHaptics == false → AUDIO
     (No haptic hardware — iPhones lacking Taptic Engine; CHHapticEngine itself is iOS 13+, well under UE 5.7's iOS 14 minimum)
  2. supportsHaptics == true → FULL
     Note: UIFeedbackGenerator silently no-ops when user disables system haptics at
     Settings > Sounds & Haptics > System Haptics. iOS does not expose this preference
     to apps via public API. When user-disabled, FULL is reported but feedback is silent.
     This is an accepted limitation — the UIFeedbackGenerator contract handles it
     transparently. Workaround: not available without private API access.

Android detection (evaluated in order). UE 5.7's enforced minimum is API 26, so an
`API < 26` branch is unreachable in shipping and is omitted:
  1. Accessibility override (user disabled haptics) → NONE
     Check: android.provider.Settings.System.HAPTIC_FEEDBACK_ENABLED == 0
  2. Vibrator.hasAmplitudeControl() == false → DURATION
     (Hardware present but no amplitude control — ERM actuators, budget devices on API 26+)
  3. hasAmplitudeControl() == true → FULL
     (LRA actuator with amplitude control — flagship and mid-range from ~2019+)
  4. Vibrator.hasVibrator() == false → check audio → AUDIO or VISUAL
     (No vibration motor — uncommon on phones; more common on some tablets)
     4a. AudioManager.isStreamMute(STREAM_MUSIC) == false → AUDIO
     4b. Otherwise → VISUAL
```

### Key Interfaces

```cpp
// IHapticDispatch.h
enum class EHapticEvent : uint8
{
    R3Collision     = 0,   // IS-owned: two opposite-zone taps in same tick
    DeadBandContact = 1,   // IS-owned: touch in exclusion band — zone-classification only
                           //           Gated by DEAD_BAND_FEEDBACK_ENABLED
    SlipConfirmed   = 2,   // PM-owned: SETTLED → SLIPPING accepted
    BufferDrop      = 3,   // PM-owned: buffer full on second slip
    InputRejected   = 4,   // IS-owned: contact rejected by radius filter (r_mm > R_max)
                           //           FULL/DURATION: sub-light haptic pulse
                           //           VISUAL: micro-flash via IVisualDispatch (not this path)
                           //           AUDIO/NONE: silent — palm rejection unnoticed by player
    ContactResting  = 5,   // IS-owned: held finger transitions to resting (cancel timer expired)
                           //           Distinct from DeadBandContact — independently tunable
                           //           Fires unconditionally (not gated by DEAD_BAND_FEEDBACK_ENABLED)
                           //           FULL/DURATION/AUDIO/VISUAL: same tier-appropriate path as DeadBandContact
    NearMiss        = 6,   // PM-owned: Pull-Wave near-miss confirmation (added 2026-06-26 per
                           //           amendment INT-002 closing PM R11a-12 forward contract).
                           //           Opt-in: PM checks IGameSettings::IsNearMissHapticEnabled()
                           //           BEFORE calling Fire() — the bridge has no awareness of the
                           //           accessibility setting. Default off; closes the
                           //           deaf-in-speaker-mode accessibility gap when on.
                           //           Soft sub-50ms low-amplitude pulse — below the conscious
                           //           "you were hit" threshold; deliberately quieter than
                           //           SlipConfirmed. Synchronized within ±50ms of the visual
                           //           near-miss Y-dip onset.
                           //           Target intensity: ≤0.4 normalized (PM GDD AC-NEARMISS-HAPTIC).
};

enum class EHapticCapability : uint8
{
    FULL     = 0,
    DURATION = 1,
    AUDIO    = 2,
    VISUAL   = 3,
    NONE     = 4,
};

class IHapticDispatch
{
public:
    // Fire the haptic event (or its fallback) for the current capability tier.
    // Implementations must not call this from any thread other than the game thread.
    virtual void Fire(EHapticEvent Event) = 0;

    // Returns the cached capability tier determined at init.
    virtual EHapticCapability GetCapability() const = 0;

    virtual ~IHapticDispatch() = default;
};
```

### iOS Platform Patterns (FULL Tier)

```
R3Collision:     UINotificationFeedbackGenerator.notificationOccurred(.warning)
                 (double-pulse characteristic of .warning)
DeadBandContact: UIImpactFeedbackGenerator(style: .soft).impactOccurred(intensity: 0.25)
                 (ultra-light; sub-light relative to all other events; zone-classification only)
ContactResting:  UIImpactFeedbackGenerator(style: .soft).impactOccurred(intensity: 0.25)
                 (same intensity class as DeadBandContact; distinct event type for independent tuning)
SlipConfirmed:   UIImpactFeedbackGenerator(style: .light).impactOccurred()
                 (per PM GDD: UIImpactFeedbackStyleLight equivalent)
BufferDrop:      UIImpactFeedbackGenerator(style: .rigid).impactOccurred(intensity: 0.5)
                 (shorter, crisper than SlipConfirmed; distinct character)
NearMiss:        UIImpactFeedbackGenerator(style: .soft).impactOccurred(intensity: 0.4)
                 (opt-in only — PM gates on IGameSettings::IsNearMissHapticEnabled();
                  sub-light, low-amplitude soft pulse — below SlipConfirmed in salience
                  by design. .soft baseline matches DeadBandContact/ContactResting; the
                  0.4 intensity keeps it under the PM GDD AC-NEARMISS-HAPTIC ≤0.4
                  normalized intensity ceiling and noticeably above DeadBandContact's
                  0.25 so the cue is felt without crossing the "you were hit" threshold.)
```

### Android Platform Patterns (FULL Tier, API ≥ 26)

```
R3Collision:     VibrationEffect.createWaveform(
                   timings=[0, 15, 30, 15], amplitudes=[0, 150, 0, 150], repeat=-1)
                 (double-pulse: two 15ms bursts, 30ms apart)
DeadBandContact: VibrationEffect.createOneShot(duration=10, amplitude=40)
                 (10ms, amplitude 40/255 — sub-light; zone-classification only)
ContactResting:  VibrationEffect.createOneShot(duration=10, amplitude=40)
                 (same as DeadBandContact; distinct event type for independent tuning)
SlipConfirmed:   VibrationEffect.createOneShot(duration=30, amplitude=140)
                 (30ms, amplitude 140/255 — light single pulse)
BufferDrop:      VibrationEffect.createOneShot(duration=20, amplitude=110)
                 (20ms, amplitude 110/255 — shorter and crisper than SlipConfirmed)
NearMiss:        VibrationEffect.createOneShot(duration=45, amplitude=100)
                 (opt-in only — PM gates on IGameSettings::IsNearMissHapticEnabled();
                  45ms duration stays under the sub-50ms PM GDD AC-NEARMISS-HAPTIC
                  bound; amplitude 100/255 ≈ 0.39 normalized, under the ≤0.4 ceiling.
                  NOTE: PM GDD line 1748 mentions VibrationEffect.EFFECT_TICK as an
                  *intensity reference*, NOT a binding API choice; EFFECT_TICK is
                  API 29+ and would break this ADR's API 26 baseline. createOneShot
                  with explicit amplitude is the conservative path that respects
                  both bounds.)
```

### Android DURATION Tier Patterns (no amplitude control — ERM actuators on API 26+ budget devices)

```
R3Collision:     Vibrator.vibrate(pattern=[0, 15, 30, 15], repeat=-1)
                 (timing structure preserved; amplitude ignored)
DeadBandContact: Vibrator.vibrate(10)  (shortest; zone-classification only)
ContactResting:  Vibrator.vibrate(10)  (same duration; distinct event type)
SlipConfirmed:   Vibrator.vibrate(30)
BufferDrop:      Vibrator.vibrate(18)  (between dead-band and slip-confirmed)
NearMiss:        Vibrator.vibrate(45)  (opt-in; sub-50ms; amplitude not controllable
                                       on DURATION tier — duration alone carries the
                                       softness signal vs SlipConfirmed's 30ms)
```

### AUDIO / VISUAL Tier Dispatch

On AUDIO tier: `Fire(event)` plays the audio cue specified in `design/ux/input-feedback.md`.
On VISUAL tier: `Fire(event)` triggers the visual indicator specified in `design/ux/input-feedback.md`.
Both dispatches are routed via `UGameplayStatics::PlaySound2D` (audio) and a UI overlay widget (visual).
The `IHapticDispatch` implementations handle the tier-appropriate dispatch internally — callers (IS, PM) always call `Fire(event)` with no tier awareness.

### NONE Tier Behaviour

`Fire()` is a no-op for all events on NONE tier.
**Exception:** `EHapticEvent::R3Collision` on NONE tier triggers the zone-edge desaturation pulse
(see `design/ux/input-feedback.md` NONE-tier spec) if RSM state is not DEAD/RESOLVING.
This is the only Pillar 5 minimum on NONE tier. The visual dispatch is handled by the IS — not
by `IHapticDispatch` — because the IS already has an `IRunStateProvider` injected and knows
whether to suppress the visual. The NONE-tier visual is not routed through `IHapticDispatch`.

### Architecture Diagram

```
IS  ─── Fire(R3Collision)              ──┐
IS  ─── Fire(DeadBandContact)          ──┤    IHapticDispatch (injected)
IS  ─── Fire(ContactResting)           ──┤         │
IS  ─── Fire(InputRejected)            ──┤         │
PM  ─── Fire(SlipConfirmed)            ──┤         │
PM  ─── Fire(BufferDrop)               ──┤         │
PM  ─── Fire(NearMiss) [opt-in; PM-gate]──┘         │
        ↑ PM checks IGameSettings::IsNearMissHapticEnabled() BEFORE Fire().
        │ Default: off. Setting lives in HUD/Accessibility Settings GDD.
        │ Bridge has no awareness of the gate — it dispatches whatever
        │ events it receives. (INT-002 amendment 2026-06-26.)
                                                    ▼
                                         FHapticPlatformPlugin
                                         ├── iOS:     UIFeedbackGenerator (ObjC bridge)
                                         ├── Android: Vibrator JNI bridge
                                         └── Fallback: audio / visual dispatch
```

### Test Stub

```cpp
// FHapticDispatchStub.h  (#if !UE_BUILD_SHIPPING)
class FHapticDispatchStub final : public IHapticDispatch
{
    EHapticCapability FakeCapability = EHapticCapability::FULL;
    TArray<EHapticEvent> FiredEvents;

public:
    void Fire(EHapticEvent Event) override { FiredEvents.Add(Event); }
    EHapticCapability GetCapability() const override { return FakeCapability; }

    void SetCapability(EHapticCapability Cap) { FakeCapability = Cap; }
    bool WasFired(EHapticEvent Event) const { return FiredEvents.Contains(Event); }
    int32 FireCount(EHapticEvent Event) const { return FiredEvents.Filter(...).Num(); }
    void Reset() { FiredEvents.Reset(); }
};
```

## Alternatives Considered

### Alternative 1: Inline JNI/ObjC in Each System (IS and PM Independently)

- **Description**: IS implements its own iOS/Android haptic bridge; PM implements its own independently
- **Pros**: No shared plugin dependency; each system is self-contained
- **Cons**: Platform code duplicated in two systems; `HAPTIC_CAPABILITY` detection runs twice with potential for divergence; maintaining two bridges doubles the effort when platform APIs change; `HAPTIC_CAPABILITY` value may differ between IS and PM if detection is non-deterministic (edge case on Android)
- **Rejection Reason**: Code duplication and risk of divergence. Shared bridge is strictly better.

### Alternative 2: UGameInstanceSubsystem

- **Description**: Implement as a `UGameInstanceSubsystem` that IS and PM call via `GetSubsystem<UHapticSubsystem>()`
- **Pros**: UE5-idiomatic; automatic lifetime managed by UGameInstance
- **Cons**: Couples IS and PM to `UGameInstance` — unit tests must construct a real `UGameInstance` or mock it, which is heavy; `GetSubsystem<>()` is not injectable without additional scaffolding; inconsistent with `IHapticDispatch` injection pattern established by this ADR
- **Rejection Reason**: Makes unit testing significantly harder; `IHapticDispatch` injection is cleaner and consistent with ADR-0001's `ITouchRadiusProvider` pattern.

### Alternative 3: Use UHapticFeedbackComponent for Mobile

- **Description**: Use UE5's existing `UHapticFeedbackComponent` to drive mobile haptics
- **Pros**: No custom code required
- **Cons**: `UHapticFeedbackComponent` is a gamepad force-feedback abstraction that does not route to `UIFeedbackGenerator` or Android `Vibrator` on mobile. Using it produces no mobile haptic output.
- **Rejection Reason**: Wrong API for this purpose. `UHapticFeedbackComponent` must not be used for mobile haptics.

## Consequences

### Positive

- Unblocks IS and PM implementation (prerequisite 4 of 4 for IS Review 6)
- Single authoritative `HAPTIC_CAPABILITY` detection — no divergence between IS and PM
- `IHapticDispatch` injection makes IS and PM unit-testable without real haptic hardware
- Shared bridge reduces platform code to one location; one update when platform APIs change
- IS and PM callers are tier-agnostic — they call `Fire(event)` and the bridge handles dispatch

### Negative

- iOS user haptic preference (`Settings > Sounds & Haptics > System Haptics`) is not detectable via public API. When user disables iOS haptics, `UIFeedbackGenerator` is a silent no-op and we report FULL but dispatch nothing. The AUDIO fallback does not activate on iOS haptic-disabled. This is an accepted limitation for MVP.
- Android NONE-tier detection via `Settings.System.HAPTIC_FEEDBACK_ENABLED` reads a System Settings value — this is a public API but OEM-variable; some Android skins expose this differently. Test on Samsung One UI, Xiaomi MIUI, and stock Android before shipping.

### Risks

- **Risk**: `VibrationEffect` JNI access pattern changed in a UE 5.7 NDK update
  **Mitigation**: Verify NDK JNI access pattern in UE 5.7 Android module source before implementing; bridge has a diagnostic mode

- **Risk**: iOS `UIImpactFeedbackGenerator` API changed in a post-cutoff iOS version
  **Mitigation**: `CHHapticEngine.capabilitiesForHardware()` is the detection path — engine knowledge gap; verify against iOS 18+ release notes

- **Risk**: `HAPTIC_CAPABILITY` caches at init; if the user changes haptic settings while the app runs, the cached value is stale
  **Mitigation**: Re-detect on `applicationWillEnterForeground` delegate — app foreground is the natural re-detection point; user would have changed settings via Settings.app which requires backgrounding the game

## GDD Requirements Addressed

| GDD System | Requirement | How This ADR Addresses It |
|---|---|---|
| input-system.md | `HAPTIC_CAPABILITY` enum: FULL/DURATION/AUDIO/VISUAL/NONE | Defines detection logic and caching strategy |
| input-system.md | R-3 collision haptic (IS-owned) | `IHapticDispatch.Fire(EHapticEvent::R3Collision)` |
| input-system.md | Dead-band contact haptic — zone-classification (IS-owned) | `IHapticDispatch.Fire(EHapticEvent::DeadBandContact)` |
| input-system.md | Resting-finger transition haptic (IS-owned) | `IHapticDispatch.Fire(EHapticEvent::ContactResting)` — distinct event; fires unconditionally |
| input-system.md | `UHapticFeedbackComponent` must not be used | Explicitly rejected in Alternatives |
| input-system.md | Android JNI bridge for `hasAmplitudeControl()` | `FHapticPlatformPlugin` Android implementation |
| player-movement-presentation.md | Slip-confirmed haptic (PM-owned) | `IHapticDispatch.Fire(EHapticEvent::SlipConfirmed)` |
| player-movement-presentation.md | Buffer-drop haptic (PM-owned) | `IHapticDispatch.Fire(EHapticEvent::BufferDrop)` |
| player-movement-presentation.md (R11a-12 / DR-D.4) | Opt-in near-miss haptic (PM-owned); AC-NEARMISS-HAPTIC; sub-50ms low-amplitude soft pulse; ≤0.4 normalized intensity; gated on `IGameSettings::IsNearMissHapticEnabled()` (default off) | `IHapticDispatch.Fire(EHapticEvent::NearMiss)` — PM checks the setting before calling; bridge dispatches via the iOS / Android FULL or DURATION patterns above. (INT-002 amendment 2026-06-26.) |
| player-movement-presentation.md (R11a-12 forward contract) | HUD/Accessibility Settings GDD hosts the `near_miss_haptic_enabled` toggle (default false); copy: "Near-Miss Haptic Feedback — Adds a soft vibration when waves pass close." | Out of this ADR's scope — this ADR provides only the platform dispatch. The setting persistence + UI lives in the HUD/Accessibility Settings GDD when authored. |
| input-system.md §Haptic Vocabulary (R11a-12 forward contract) | `EHapticEvent::NearMiss` listed in haptic vocabulary table on first revision after PM R11a-12 propagation | Enum value added here authoritatively; IS GDD haptic vocabulary table inherits via `/propagate-design-change` whenever IS GDD is next revised. |

## Performance Implications

- **CPU**: `Fire()` is an immediate platform call or a no-op; no deferred work — negligible
- **Memory**: Capability enum (1 byte) cached at init — negligible
- **Load Time**: Detection runs once at `Module::StartupModule()` — JNI call overhead < 5ms
- **Network**: N/A

## Migration Plan

Applies to new implementation only. When implementing:
1. Create `FHapticPlatformPlugin` UE5 native plugin (ObjC + Java/JNI + C++ header)
2. Call `FHapticPlatformPlugin::DetectCapability()` at `Module::StartupModule()`
3. Inject `IHapticDispatch` into IS and PM at construction (use `FHapticDispatchStub` in editor builds)
4. Replace any haptic call site in IS or PM with `HapticDispatch->Fire(event)`

## Validation Criteria

### Unit Tests (before implementation sprint)

```
FHapticDispatchStub.SetCapability(FULL):
  Fire(R3Collision)     → WasFired(R3Collision) == true
  FireCount(R3Collision) == 1

FHapticDispatchStub.SetCapability(NONE):
  Fire(SlipConfirmed)   → WasFired(SlipConfirmed) == true (Fire() still called)
  → No platform haptic dispatch (validated by observing no actual vibration)

FHapticDispatchStub:
  IS AC-18: IS_HAPTIC_FIRED fires → verify FHapticDispatchStub.WasFired(R3Collision)
  IS AC-19: HAPTIC_CAPABILITY=NONE → verify IS still calls Fire() but platform dispatch is no-op
```

### Hardware Verification Gate (before shipping)

- iOS: Verify FULL-tier UIFeedbackGenerator patterns are distinct on target devices
- iOS: Verify that the IS NONE-tier R-3 visual fires when app is in NONE tier (not via IHapticDispatch)
- iOS: Verify `EHapticEvent::NearMiss` `.soft + intensity 0.4` is **perceptibly softer** than `EHapticEvent::SlipConfirmed` `.light` on iPhone 16; subjective sign-off by ux-designer + accessibility-specialist on cue character.
- Android: Verify FULL-tier VibrationEffect patterns are distinct on flagship + mid-range + an API 26 budget device with no amplitude control (DURATION-tier path)
- Android: Verify NONE detection logic fires correctly on a device with haptics disabled in Accessibility
- Android: Verify `EHapticEvent::NearMiss` `createOneShot(45, 100)` on FULL tier and `Vibrator.vibrate(45)` on DURATION tier are both perceptibly distinct from `SlipConfirmed`; subjective sign-off as above.
- Integration: Verify that with `IGameSettings::IsNearMissHapticEnabled() == false` (default) and 10 consecutive near-miss triggers, the bridge records ZERO `EHapticEvent::NearMiss` dispatches — matches PM GDD AC-NEARMISS-HAPTIC Setup A. (The bridge dispatches whatever it is told; this is a PM-side gate verification.)

## Related Decisions

- ADR-0001: `docs/architecture/adr-0001-palm-rejection-rmax-calibration.md` — establishes native plugin pattern this ADR follows
- `design/ux/input-feedback.md` — AUDIO/VISUAL fallback specs dispatched by this bridge
- `design/gdd/input-system.md` — HAPTIC_CAPABILITY enum and haptic vocabulary (NearMiss propagation queued for next IS revision per INT-002 forward contract row above)
- `design/gdd/player-movement-presentation.md` — slip-confirmed, buffer-drop, and (per R11a-12) near-miss haptic ownership; PM gates NearMiss on `IGameSettings::IsNearMissHapticEnabled()` before calling `Fire()`
- **architecture-review 2026-06-25** (`docs/architecture/architecture-review-2026-06-25.md`) — INT-002 surfacing. The NearMiss enum value, platform patterns, Architecture diagram update, GDD Requirements rows, and Hardware Verification Gate items added 2026-06-26 are the resolution artifacts for that finding.

---

*Status becomes Accepted when hardware verification gate passes. Until then: Proposed.*

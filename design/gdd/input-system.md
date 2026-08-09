# Input System

> **Status**: Approved (re-review 27, 2026-06-04)
> **Author**: ibrahimakpinar + agents
> **Last Updated**: 2026-06-04 (re-review 27: APPROVED. 0 blocking; 10 polish items in backlog; 7 items routed to technical-director as ADR amendments parallel to epic work — see review log for full classification. Trajectory: 10→16→22→23→25→29→18→26→7→9→12→9→9→5→6→14→14→15→10→5→18→10→15→5→2→8→0 across 27 reviews.)
> **Implements Pillar**: Pillar 1 (Slip Is the Only Verb), Pillar 5 (Skill Is Visible — input layer only; full Pillar 5 chain includes Telegraph System + Player Movement)
> **Rewrite Source**: ADR-0001, ADR-0002, ADR-0003, platform-seam-interfaces.md, design/ux/input-feedback.md
> **Prior status**: In Review — 16 reviews; 12 blockers in re-review 11 revision (2026-05-31); 9 blockers in re-review 12 revision (2026-05-31); 5 blockers in re-review 14 revision (2026-05-31); 14 blockers in re-review 16 revision (2026-06-01): fast-tap rescue cut, DrainTick rewritten, dead-band NONE-tier visual added, first-run prompt gated to COUNTDOWN

## Overview

The Input System is the translation layer between the player's touch events and
SLIPSTORM's single gameplay verb. It receives raw touch input from the mobile screen
and dispatches one of two discrete events: `slip-left` or `slip-right`. It maintains
no game-state awareness — events fire regardless of whether a run is active, paused,
or ended — but maintains bounded per-contact state: cancel timers, zone
classifications, and active touch tracking, all scoped to each touch's lifetime.

The Player Movement system is the sole consumer of these events and decides whether
to act on them based on current run state. The IS never queries Player Movement or
the Run State Machine. One narrow exception: the NONE-tier R-3 collision visual is
suppressed during DEAD, RESOLVING, ABORTED, and COUNTDOWN states via a single injectable RSM state read.

Six platform interfaces are injected at construction and never swapped at runtime:
a millisecond monotonic clock (for cancel-timer accuracy across app suspension), a
touch-radius provider (enabling palm rejection without engine-source modification),
an RSM state reader (for R-3 suppression during DEAD/RESOLVING/ABORTED/COUNTDOWN states and
NONE-tier visual gate), a haptic dispatch bridge (enabling graceful degradation across
device capability tiers), a visual dispatch bridge (for NONE-tier visual events: the R-3 collision zone-edge
overlay and the ContactResting hold-deactivation signal on NONE-tier devices — both
are direct IS visual responsibilities not routed through IHapticDispatch), and a
boot-flag store (for `FIRST_RUN_PROMPT_ENABLED` persistence across app restarts — IS reads
this flag at COUNTDOWN entry and clears it on the first successful slip in RUNNING state).
Canonical specifications:
ADR-0001
(`docs/architecture/adr-0001-palm-rejection-rmax-calibration.md`), ADR-0002
(`docs/architecture/adr-0002-haptic-platform-bridge.md`), and
`docs/architecture/platform-seam-interfaces.md`.

## Player Fantasy

The Input System has no fantasy of its own. Its entire purpose is to disappear —
the moment between the player's thumb committing and the world bending must feel
like the same moment.

This system exists in service of Pillar 5 (Skill Is Visible). The skill-visibility
guarantee is scoped: every input the player intends and successfully delivers within
one game frame produces feedback within 33ms on 60 FPS hardware — 16ms above the
one-tick hold buffer, below the ~80ms perceptual threshold. On 30 FPS devices, the
hold buffer occupies the full 33ms frame budget. Three edge cases fall outside this
guarantee: sub-frame taps arriving and lifting within the same drain tick (<16ms at
60 FPS, <33ms at 30 FPS) produce no dispatch — this is the minimum supported
skill-expression envelope; rapid two-thumb alternation within the one-frame collision
window (~16–66ms depending on frame rate) produces a discard rather than two events;
and the ContactResting hold-deactivation tint on bright voxels (luminance > 0.50) on
NONE-tier devices may fall below perceptual threshold. These are declared exceptions,
not silent failures — each has a documented rationale in the relevant rule. Each tap
is held for one drain tick before dispatch to enable adjacent-tick collision detection
(R-3, F-4) — without this one-tick hold, same-frame collisions are detectable but
adjacent-frame collisions cannot be retracted after dispatch.

Disappearance and skill-visibility are the same goal stated twice: the system
disappears precisely because it never adds noise to the skill signal. The player
reads the storm correctly, or they do not — the IS has no opinion on the outcome.

Every rule in this system — palm rejection, cancel timer, dead-band exclusion —
exists to ensure that events the player intended always fire, and events the player
did not intend never reach Player Movement.

## Detailed Design

### Core Rules

**R-1: Zone Definition**
The game runs in portrait orientation only. W in all zone formulas refers to the
short dimension of the device screen (width in portrait). The screen divides into
two halves at the horizontal midpoint with an 8mm exclusion band (≈145px on a 3×
460 PPI display) centered on the midpoint.

- Touches left of the band dispatch `slip-left`
- Touches right of the band dispatch `slip-right`
- Touches within the band are classified DEAD_BAND: discarded silently.
  `IS_DEAD_BAND_DISCARDED` is always logged in debug builds regardless of the
  feedback knob. `DEAD_BAND_FEEDBACK_ENABLED` (default **on**): when enabled, a
  sub-light haptic fires via `IHapticDispatch::Fire(EHapticEvent::DeadBandContact)`
  to acknowledge the contact without implying a game verb.

No visual zone marker is displayed during an active run. On the player's first run
only, three zone labels appear during the **COUNTDOWN** run state only:
- **Left zone label:** "Tap left." — lower-middle of the left tap zone at ~78% screen height
- **Center zone label:** "No slip" — lower-middle of the center exclusion band at ~78% screen height
- **Right zone label:** "Tap right." — lower-middle of the right tap zone at ~78% screen height

All three labels are shown only during **COUNTDOWN**; they dismiss together at the
COUNTDOWN → RUNNING transition (before the player can act). If the player has not
slipped before a run ends, the prompt reappears on the next run's COUNTDOWN.
**Persistence:** `FIRST_RUN_PROMPT_ENABLED` is a per-install flag stored in save data.
The prompt reappears every run until the player completes their first successful slip
in any direction on any run. After the first slip, the flag is cleared permanently and
the prompt never re-appears.

**Accepted trade-off — sub-frame tap detection:** A touch-down and touch-up arriving
within the same drain tick (sub-16.67ms at 60 FPS / sub-33ms at 30 FPS) produces no
slip dispatch. This is the minimum supported skill-expression envelope; such taps fall
below the intentional reflex pattern this game targets. See DrainTick pseudocode Step 1
for the touch-up path. (Pillar 5 note: the 1-tick hold buffer is a Pillar 5 choice —
the "game's reflex pattern is tap-based" — not an implementation constraint; the cut
was a deliberate scope decision in re-review 16.)

**R-2: Valid Tap**
A touch qualifies as a valid slip input if all of the following are true:
1. Trigger is touch-down (Pointer Pressed). Dispatches on finger-down, not finger-up.
2. Touch ID is new — not currently in the active tracking set.
3. Contact radius ≤ `PALM_REJECTION_R_MAX_MM` (6.0mm), evaluated via
   `ITouchRadiusProvider::GetRadiusAndDirect(int32 OS_FingerIndex)` — returns
   `FTouchRadiusReading { float radius_mm; bool is_direct }` (a plain snapshot struct;
   see ADR-0001 for the `FTouchRadiusSlot` / `FTouchRadiusReading` type split — `FTouchRadiusSlot`
   holds `std::atomic<>` fields and cannot be returned by value). Use `reading.radius_mm` for
   the filter. Contacts with radius_mm > 6.0mm are discarded as palm or accidental.
   See ADR-0001 for the platform formula and plugin specification.
4. Position falls outside the 8mm center exclusion band.

**Resting-finger protection (cancel timer):** For each touch-down that passes PALM_PASS
(F-2) and zone classification (F-1), a 180ms cancel timer starts using
`IMonotonicClock::NowMs()` (returns elapsed milliseconds; continues counting across app
suspension). The timer anchors to touch-down receipt in DrainTick() phase 1 — NOT to
the drain tick when the slip event later dispatches. The 1-tick hold buffer does not
delay `t_start_ms`. If no touch-up arrives within 180ms, the contact is marked "resting" — an IS-internal state transition only. When a contact
transitions to resting, IS fires
`IHapticDispatch::Fire(EHapticEvent::ContactResting)` — a sub-light signal distinct
from the dead-band acknowledgement — to notify the player that their held finger has
been deactivated without triggering a game verb (Pillar 5 minimum). **Not gated by
`DEAD_BAND_FEEDBACK_ENABLED`. Run-state gating: suppressed during DEAD, RESOLVING,
and ABORTED; fires in IDLE, RUNNING, and COMPLETE; **effectively suppressed during
COUNTDOWN via two cooperating mechanisms**: (1) cancel-timer guard in Step 2 prevents
the timer from firing while `current_state == COUNTDOWN`; (2) IS-21-A1 tracking reset
clears all contacts at COUNTDOWN→RUNNING transition, so no pre-positioned contact
survives to fire ContactResting on the first RUNNING tick — the player must lift and
re-press after run start to register a new contact. See DrainTick pseudocode
COUNTDOWN→RUNNING reset block and Haptic Vocabulary for rationale.** The knob gates only
zone-classification dead-band taps (R-1); the resting-transition signal is a mandatory
Pillar 5 notification using a distinct event type so the two signals are independently
tunable and Pillar-5-readable. A resting contact generates no further events until the finger
lifts and re-touches. No slip or cancel event is dispatched to Player Movement; this
protection is IS-internal.

**Stylus scope note:** iOS `UITouch.type == UITouchTypeDirect` must be checked before
the radius filter — non-Direct touch types (Pencil, indirect) are discarded before
F-2 evaluates. Android: accept `TOOL_TYPE_FINGER` and `TOOL_TYPE_UNKNOWN`; explicitly
reject `TOOL_TYPE_STYLUS`, `TOOL_TYPE_MOUSE`, `TOOL_TYPE_ERASER`. Do not reject
`TOOL_TYPE_UNKNOWN` — some OEM devices report it for legitimate finger contacts.

**R-3: Simultaneous Input**
Two touch-downs on opposite halves in the same or adjacent drain tick are a
same-frame collision — both events are discarded. IS fires a double-pulse haptic via
`IHapticDispatch::Fire(EHapticEvent::R3Collision)` — **suppressed during `DEAD`,
`RESOLVING`, `ABORTED`, and `COUNTDOWN` states** (via `IRunStateProvider::GetCurrentState()`;
applies to all capability tiers). Suppressing during `COUNTDOWN` prevents a
pre-run dual-thumb contact from conditioning the wrong association (double-pulse = discard)
before the first run frame. NONE-tier exception: IS additionally fires the zone-edge
desaturation pulse directly via `IVisualDispatch`; also suppressed during `DEAD`,
`RESOLVING`, `ABORTED`, and `COUNTDOWN`. The NONE-tier visual fires in all other run
states: `IDLE`, `RUNNING`, and `COMPLETE`.

Two touch-downs on the same half in the same tick are not a collision — each fires
independently. Player Movement handles duplicate events via its own buffering policy.

**Design note — 33ms collision window:** The delta ≤ 1 rule means two touches arriving
in adjacent drain ticks (~16–33ms apart) are treated as simultaneous. This is
intentional: rapid two-thumb alternation within ~33ms is not a supported skill
expression in SLIPSTORM. Players who alternate thumbs quickly will occasionally collide
— this is expected behavior, not a bug.

**Pillar 5 Exception — frame-rate-dependent collision window:** The same physical tap
timing that succeeds on a 60 FPS device (e.g., 20ms between taps → delta=1 → collision)
will also produce a collision on a 30 FPS device (66ms window at 30 FPS). The window is
explicitly frame-rate-dependent: a skill pattern learned on 60 FPS hardware may produce
mystery discards on a 30 FPS device running the same game. This is a known Pillar 5
exception, accepted for the following reason: using fixed-millisecond timestamps instead
of drain_tick_index would restore device-independence but reintroduces OS scheduling
jitter — two touches 16ms apart under load arrive as delta=0 or delta=2 non-deterministically.
The jitter-immune drain-tick model is the superior trade-off. **Minimum supported frame
rate: 30 FPS.** At 30 FPS the collision window is ~66ms — above 33ms but below the
~80ms floor where most players consciously attempt rapid two-thumb alternation. Below
30 FPS the window becomes perceptibly wrong and is not a supported operating condition.

`FInputSystem::DrainTick()` is called by `FTSTicker` once per game frame. The drain rate
therefore matches the game's frame rate — approximately 60 Hz on 60 FPS devices, 30 Hz
on 30 FPS devices. The collision window (delta ≤ 1 drain tick) equals one game-frame
interval, not a fixed 16.67ms. Two touches co-arriving in the same drain tick have
`drain_tick_index` delta=0 (always collide); touches in adjacent drain ticks have delta=1
(also collide). Only touches two or more drain ticks apart proceed. See ADR-0003 for the
drain queue architecture and `drain_tick_index` contract.

**R-4: Input During an Active Slip**
The Input System has no game-state awareness. It dispatches on every valid tap
regardless of whether a slip is in progress. Input buffering is Player Movement's
responsibility. The Input System must never query Player Movement state.

**R-5: Run-State Gating**
Events dispatch regardless of run state (idle, running, dead, end-screen). Gating is
Player Movement's responsibility. The Input System must not subscribe to or query the
Run State Machine, except via `IRunStateProvider` for the NONE-tier R-3 visual
suppression described in R-3.

**One IS-level exception — ABORTED state:** The ABORTED-state `tracking_set` flush
in `DrainTick()` prevents contacts from dispatching while `current_state ==
ERunState::ABORTED`. This is IS-level cleanup (clearing stale contacts accumulated
during a background suspend whose touch-up events will never arrive), not PM gating.
Dispatch is prevented by two cooperating mechanisms: (1) the 1-tick hold buffer
prevents same-tick dispatch — a contact received in Step 1 has
`drain_tick_index == current_drain_tick_index`, so the Step 3 predicate
(`drain_tick_index < current_drain_tick_index`) fails and no dispatch occurs in
that tick; (2) the ABORTED flush runs at the end of each ABORTED DrainTick (after
Step 3), clearing the entire tracking set — the contact cannot survive to the next
tick's dispatch window. Any contact received during ABORTED is logged
(`IS_TOUCH_RECEIVED`) but is cleared by the end-of-tick flush before it can ever
reach the dispatch predicate on a subsequent tick.
Player Movement is not involved in this suppression.

**R-6: Accidental Input Rejection**
Three layers applied in order:
1. OS-level palm suppression (iOS UIKit / Android MotionEvent) — relied upon, not
   duplicated in application code.
2. Contact radius filter: `ITouchRadiusProvider::GetRadiusAndDirect(OS_FingerIndex).radius_mm >
   PALM_REJECTION_R_MAX_MM` (6.0mm) → discard before zone check. See ADR-0001 for
   the `FTouchRadiusBridgePlugin` specification. R_max = 6.0mm sits 0.5mm above the
   finger 95th percentile and 2.5mm below the palm 5th percentile.
3. Resting-finger protection via 180ms cancel timer using `IMonotonicClock::NowMs()`
   (R-2).

---

### States and Transitions

The Input System has one lifecycle state: **ACTIVE**. It initializes at app start
and never transitions.

| State | Description | Entry | Exit |
|---|---|---|---|
| ACTIVE | Polls touch events; dispatches valid slip events | App start | App termination only |

All run-lifecycle state is owned by the Run State Machine. The Input System maintains
no game-state awareness — any dependency on run state would violate Foundation layer
isolation. It does maintain bounded per-contact state scoped to each touch's lifetime:

| Per-Contact State | Lifetime | Purpose |
|---|---|---|
| Active touch ID set | Touch-down → touch-up | R-2: new-touch check |
| Zone classification | Touch-down → touch-up | R-1 + R-3: zone retained across drag |
| Drain tick index (`drain_tick_index`) | Touch-down → removal | F-4 + 1-tick hold buffer: assigned at dequeue time; contacts dequeued in the same DrainTick share the same index; a contact is eligible for dispatch only when `current_drain_tick_index > drain_tick_index AND b_dispatched == false` — prevents same-tick self-dispatch, ensures adjacent-tick arrivals can pair via F-4, and prevents already-dispatched contacts from re-dispatching |
| Cancel timer start time | Touch-down → cancel or touch-up | R-2: resting-finger protection |
| Resting flag | Cancel → touch-up | R-2: suppress further events |
| Dispatched flag (`b_dispatched`) | Dispatch tick → removal | F-4: dispatched-contact exclusion; set `true` when the contact's slip event dispatches; contact removed from tracking set at touch-up. NOT cleared on cancel/deactivation — re-press after lift correctly re-enters F-4 pairing |
| OS Finger Index (`os_finger_index`) | Touch-down → removal | Stored at touch-down to enable reverse-lookup in the `pending_removals` drain: when a contact is discarded by F-4 collision, the drain reads `tracking_set[id].os_finger_index` to immediately clean `fingerindex_to_contactid_map` without waiting for a touch-up event (DEFECT-1 fix). The map entry is keyed by `os_finger_index`, not `contact_id`, so the struct must carry this value. |
| Contact ID (`contact_id`) | Touch-down → removal | IS-assigned identifier; same value as the key used to store this entry in `tracking_set`. Stored as a struct field so that pseudocode can reference `C.contact_id` in Step 3 loop bodies (collision pairing, dispatch log, pending_removals) without requiring a separate loop-key variable at every reference site. |

**Construction invariant:** At IS construction, the active contact tracking set is
empty, `drain_tick_index` is 0, `prev_state` is `ERunState::IDLE`,
`b_first_run_prompt` is `false` (read from `IBootFlagStore` at the first COUNTDOWN entry,
not at construction), and `bViewportChecked` is `false` (the viewport state machine at
the top of `DrainTick()` resolves it on the first tick where `GEngine->GameViewport` is
non-null — see OQ-4 and DrainTick pseudocode). Any OS state from before IS construction
is not inherited — contacts held before IS construction are untracked, and their touch-up
events are logged as `IS_TOUCH_UP_ORPHAN` and discarded. IS must be constructed before
any touch events can arrive (typically at subsystem init before the first game frame).

**Construction lifecycle (resolved by ADR-0003 Owner section):**
`FInputSystem` is owned by `FSlipstormGameModule : public IModuleInterface`.
`StartupModule()` constructs `FInputSystem` (via `MakeUnique<>`) and calls
`InputSystem->RegisterWithSlate()` — which creates `FInputProcessorProxy` and calls
`FSlateApplication::Get().RegisterInputPreProcessor()`. `ShutdownModule()` calls
`InputSystem->UnregisterFromSlate()` then resets the `TUniquePtr`. The `FInputSystem`
destructor additionally calls `UnregisterInputPreProcessor()` and `FTSTicker::RemoveTicker()`
as a safety net. `IModuleInterface` lifetime predates `UGameInstance` and outlasts all touch
events — the recommended owner. See `docs/architecture/adr-0003-drain-queue-architecture.md`
§Owner for the full class pattern, rejected alternatives, and the `FTSTicker` registration
ordering requirement (ticker must register before the first game frame).

**Cancel timer implementation requirement:** Use `IMonotonicClock::NowMs()` — returns
elapsed milliseconds and continues counting across app suspension. Do NOT use
`FPlatformTime::Seconds()` (returns **seconds**, pauses on iOS via
`mach_absolute_time()`) or `FApp::GetCurrentTime()` (pauses during backgrounding).
The unit mismatch — `Seconds()` returning seconds vs. a 180ms threshold — causes the
cancel timer to never fire under a naïve implementation. See
`docs/architecture/platform-seam-interfaces.md` for iOS (`mach_continuous_time`) and
Android (`CLOCK_BOOTTIME`) production implementations and the `FFakeMonotonicClock`
test stub.

**Cancel timer evaluation frequency:** F-3 is evaluated on every 60Hz drain tick
(`DrainTick()` call), not only when new OS events arrive. On each drain tick, IS
checks all contacts currently in the active tracking set whose resting flag is not yet
set. This ensures a finger held without generating further events (no drag, no
additional touches) transitions to resting at the correct wall-clock time. The
"first frame tick after resume" language in Edge Cases refers to the first
`DrainTick()` call after app resume — the drain tick IS the frame-tick evaluation
unit. There is no separate game-thread evaluation path.

---

### Interactions with Other Systems

| System | Direction | Data | Notes |
|---|---|---|---|
| Player Movement | Outbound → | `slip-left`, `slip-right` | Only consumer. Events are fire-and-forget; IS does not wait for acknowledgement. Resting-finger cancel is IS-internal — not dispatched to PM. |
| Player Movement | None ← | — | PM must not call into IS (R-4). PM fires its own haptics (SlipConfirmed, BufferDrop) via `IHapticDispatch::Fire()` directly — not through IS. |
| Run State Machine | Narrow read ← | `ERunState` | Via `IRunStateProvider::GetCurrentState()` only. IS reads RSM state to suppress the R-3 haptic and visual during DEAD, RESOLVING, ABORTED, and COUNTDOWN states (all capability tiers). No other RSM dependency. |
| OS Touch Layer | Inbound ← | `TouchStarted`, `TouchEnded`, contact radius | Contact radius is not available via `FPointerEvent` — retrieved via `ITouchRadiusProvider` (backed by `FTouchRadiusBridgePlugin` in shipping). OS palm suppression is a prerequisite, not duplicated here. |

> **Implementation note:** Zone-tap implementation path resolved by ADR-0003:
> `FInputSystem` implements `IInputProcessor` (Slate input preprocessor), not a
> UMG overlay. `FTSTicker` registers `DrainTick()` per game frame. See
> `docs/architecture/adr-0003-drain-queue-architecture.md`.

---

### Platform Injection Interfaces

All six interfaces are injected at IS construction and never swapped at runtime.
In shipping builds, platform implementations are selected at compile time. In
non-shipping builds, injectable stubs enable unit testing without real hardware.

| Interface | Role | Shipping Implementation | Test Stub |
|---|---|---|---|
| `IMonotonicClock` | Cancel timer and collision timestamps (ms, suspension-safe) | `FiOSContinuousTimeClock` / `FAndroidBootTimeClock` | `FFakeMonotonicClock` (advanceable) |
| `ITouchRadiusProvider` | Per-finger contact radius in mm | `FTouchRadiusCache` (backed by `FTouchRadiusBridgePlugin`) | `FTouchRadiusProviderStub` (fixed radius) |
| `IRunStateProvider` | RSM state read for NONE-tier visual gate only | `FRSMRunStateProvider` (holds `TWeakObjectPtr<URunStateMachineSubsystem>` — resolved via `UGameInstance::GetSubsystem<URunStateMachineSubsystem>()`; TWeakObjectPtr prevents dangling pointer if subsystem is destroyed before IS) | `FRunStateProviderStub` |
| `IHapticDispatch` | R-3 collision and dead-band haptic dispatch | `FHapticPlatformPlugin` | `FHapticDispatchStub` (records calls) |
| `IVisualDispatch` | NONE-tier visual events: R-3 zone-edge desaturation pulse and ContactResting hold-deactivation signal (both bypass haptic path) | `FWidgetVisualDispatch` | `FVisualDispatchStub` (records calls) |
| `IBootFlagStore` | First-run prompt state persistence — reads/writes `FIRST_RUN_PROMPT_ENABLED` flag across app restarts | `FLocalStorageBootFlagStore` (platform key-value store) | `FBootFlagStoreStub` (in-memory, resets each test) |

**`IBootFlagStore` interface contract:**
```cpp
class IBootFlagStore {
public:
    virtual bool  GetFlag(FName FlagName) const = 0;   // returns false if key absent
    virtual void  SetFlag(FName FlagName, bool Value) = 0;
    virtual ~IBootFlagStore() = default;
};
```
IS calls `GetFlag(TEXT("FIRST_RUN_PROMPT_ENABLED"))` at COUNTDOWN entry (via prev_state
edge detection in DrainTick) to determine whether to show labels. IS calls
`SetFlag(TEXT("FIRST_RUN_PROMPT_ENABLED"), false)` when the first successful slip is
dispatched **and `current_state == RUNNING`** — a slip in COUNTDOWN state does not dismiss
the prompt. IS owns this interface; Score Persistence is not a dependency.

Full interface contracts and stub implementations:
`docs/architecture/platform-seam-interfaces.md` (IMonotonicClock, SimulateTouch, IRunStateProvider),
ADR-0001 (`docs/architecture/adr-0001-palm-rejection-rmax-calibration.md`) (ITouchRadiusProvider),
ADR-0002 (`docs/architecture/adr-0002-haptic-platform-bridge.md`) (IHapticDispatch),
`docs/architecture/visual-dispatch-contract.md` (IVisualDispatch — FWidgetVisualDispatch, FVisualDispatchStub),
ADR-0003 (`docs/architecture/adr-0003-drain-queue-architecture.md`) (FInputSystem class pattern, drain_tick_index, FTSTicker registration).

## Formulas

The **Zone Classification** formula (F-1) is defined as:

`ZONE(x, W, B) = LEFT if x ≤ (W/2 − B/2) | RIGHT if x ≥ (W/2 + B/2) | DEAD_BAND otherwise`

**Variables:**
| Variable | Symbol | Type | Range | Description |
|---|---|---|---|---|
| Touch X position | `x` | float | 0–W | Horizontal native-pixel coordinate from screen left edge. Do not apply DPI down-scaling. |
| Screen width | `W` | float | > 0 | Total screen width in native pixels. Must satisfy W > B. |
| Band width | `B` | float | 0 < B < W | `EXCLUSION_BAND_WIDTH_MM × screen_dpi_per_mm`. On a 3× 460 PPI display, B ≈ 145px. (Derivation: 460 PPI ÷ 25.4 = 18.11 px/mm; 8mm × 18.11 = 144.9px ≈ 145px.) **⚠ `screen_dpi_per_mm` must be native physical pixel density — NOT logical/points density.** On iOS, the logical density is ~6.4 px/mm (163 pt/in ÷ 25.4); the physical density on iPhone 16 is ~18.1 px/mm (460 PPI ÷ 25.4). Using logical density produces B ≈ 48px instead of 145px — a 3× underestimate that misclassifies most center-tap contacts as LEFT or RIGHT. Use the same physical-PPI source as F-2 (see F-2 note). |

**Output Range:** {LEFT, RIGHT, DEAD_BAND} — every input maps to exactly one value.
**Boundary convention:** Band edges are inclusive to the zone side. x exactly at the
left edge → LEFT; x exactly at the right edge → RIGHT.
**Initialization guards:**
- If B ≤ 0: log error and halt initialization — band width must be positive.
- If B ≥ W: log error and halt initialization — no reachable LEFT or RIGHT zone.
- If `(W_mm − EXCLUSION_BAND_WIDTH_MM) / 2 < 2.2mm` (one finger width — minimum
  reliably tappable zone): log error and halt. Equivalently in pixels:
  `(W − B) / 2 < 2.2 × screen_dpi_per_mm`. The mm expression is the authoritative
  check — the px equivalent is device-specific and must be recomputed from the mm
  threshold, not hardcoded (40px is only correct at 460 PPI; it under-enforces at
  higher densities).
- `screen_dpi_per_mm` must be non-zero before B is computed; assert at init.

**Out-of-bounds coordinates:** If x < 0 or x > W, clamp x to [0, W] before evaluating
ZONE() — this guarantees x always maps to a defined output (LEFT, RIGHT, or DEAD_BAND)
rather than undefined behavior at the formula boundary. Log a debug warning
(`IS_OUT_OF_BOUNDS_COORDINATE`) when clamping occurs. Normal device use cannot produce
out-of-bounds values — they indicate an upstream platform bug or malformed event.
**Do not treat out-of-range coordinates as DEAD_BAND without clamping** — a value of
x = −1 would satisfy `x ≤ (W/2 − B/2)` and classify LEFT, dispatching an unintended
slip if the clamp is omitted. Clamping and logging is the correct response.

**Example:** W=1080, B=145 (at 460 PPI). Left boundary=467.5, Right boundary=612.5.
x=300 → LEFT. x=467.5 → LEFT (band edge, inclusive). x=540 → DEAD_BAND. x=612.5 → RIGHT
(band edge, inclusive). x=700 → RIGHT.

---

The **Palm Rejection Filter** formula (F-2) is defined as:

`PALM_PASS(r_mm, R_max) = (r_mm ≤ R_max)`

Where `r_mm = ITouchRadiusProvider::GetRadiusAndDirect(int32 OS_FingerIndex).radius_mm` —
computed by `FTouchRadiusBridgePlugin` from native platform values:

| Platform | Derivation |
|---|---|
| iOS | `r_mm = UITouch.majorRadius × nativeScale ÷ screen_ppi × 25.4` |
| Android | `r_mm = MotionEvent.getTouchMajor() ÷ 2 ÷ screen_dpi_per_mm` |

`UITouch.majorRadius` is in UIKit points; `× nativeScale` converts to pixels before
dividing by `screen_ppi`. **`screen_ppi` must be native physical pixel density in
pixels per inch (e.g., 460 for iPhone 16) — NOT logical/points density (~153pt/in on
iPhone 16). Using logical PPI inflates `r_mm` by 3× on a 3× Retina device, causing
false palm rejections for normal finger contacts.**
`MotionEvent.getTouchMajor()` is a diameter-class pixel value; `÷ 2` converts to radius.

**Variables:**
| Variable | Symbol | Type | Range | Description |
|---|---|---|---|---|
| Contact radius | `r_mm` | float | ≥ 0 or −1.0f | From `ITouchRadiusProvider::GetRadiusAndDirect(OS_FingerIndex).radius_mm`. Sentinel value `−1.0f` when `FTouchRadiusCache` had no data for this FingerIndex (cache slot uninitialized, or bridge write raced with drain read). Any r_mm < 0 → treated as pass (no data — never reject on absence of data); if the value is not exactly `−1.0f`, log a debug warning (unexpected negative from bridge) — the `IS_TOUCH_RECEIVED` log entry is the appropriate location for this warning (IS is the first consumer of the radius value; the bridge does not log it). **Non-finite handling (NaN and ±Inf):** `ITouchRadiusProvider` is required by its contract to return a finite float — neither NaN nor ±Inf is a valid sentinel. IS uses `!std::isfinite(r_mm)` (not `std::isnan` alone) to catch all non-finite values, treats the value as `−1.0f` (pass, no data), and logs a debug warning capturing the raw value. `std::isnan` alone misses `+Inf`, which would silently fail `PALM_PASS(+Inf, R_max) = false` and reject the contact as a palm — indistinguishable from a real palm rejection. The provider contract must be updated to assert finite return. r_mm ≥ 0 is a measured contact radius. |
| Max allowed radius | `R_max` | float | > 0 | `PALM_REJECTION_R_MAX_MM = 6.0mm`. Device-independent. Safe tuning range [4.0, 8.0]. See ADR-0001 for derivation from published finger/palm population data. **Initialization guard:** If `PALM_REJECTION_R_MAX_MM ≤ 0` at IS construction, log error and halt — every real contact (`r_mm > 0`) would fail `PALM_PASS` (`r_mm ≤ 0` is false), triggering `IS_PALM_REJECTED` on every touch-down and making the IS non-functional. The `-1.0f` sentinel still passes (no-data semantic preserved), but the game is unplayable on real input. This is analogous to the halting guards on F-1 (`B ≥ W`) and F-3 (`CANCEL_TIMER_MS ≤ 0`). |

**R_max glossary note:** `R_max` is calibrated against *dynamic tap contact radius* — the
instantaneous contact footprint at touch-down impact, not the resting or fully-pressed
contact area. These are distinct measurements: a resting thumb may spread 7–10mm, but
its dynamic tap radius at impact is typically 3–5mm. The 6.0mm threshold is derived from
the latter. See ADR-0001 §Population Data for source distributions.

**Output Range:** Boolean. `true` = event proceeds to zone check. `false` = discarded.
**Initialization guard:** Check `screen_ppi >= 200` and `nativeScale > 0` (iOS) before
the first touch event — values below 200 PPI indicate the editor/PIE context (PIE
reports ~72 PPI) or a device reporting logical rather than physical PPI. Check
`screen_dpi_per_mm >= 7.87` (Android — the mm equivalent of 200 PPI: 200 ÷ 25.4 ≈ 7.87
px/mm); values below this indicate editor/PIE or logical-density mismatch. If the check
fails: log a warning (do not halt — halting bricks the IS on devices where logical PPI
is returned before physical PPI is resolved, e.g., some iPad models) and fall back to
`UE::Input::GetDPIScaleByOS()` to obtain a corrected density value. Log the fallback
path with the raw and corrected values. Zero values remain a hard error — assert
`nativeScale > 0` and `screen_dpi_per_mm > 0`; zero produces division-by-zero in the
radius formula and there is no valid fallback.
**Target hardware gate:** The physical-PPI sourcing must be verified on the minimum-spec
device matrix (AC-15b) before IS implementation begins. If the density source cannot
return physical PPI on a required device, the sourcing strategy must be updated in
ADR-0001 before shipping.

**Example:** R_max = 6.0mm.
r=3.0mm → true (finger-sized, passes). r=6.0mm → true (at threshold — only r > R_max
is rejected). r=6.1mm → false (palm, discarded). r=−1.0mm → true (sentinel: no data,
cache miss or bridge race — never reject on absence of data).

---

The **Cancel Timer Check** formula (F-3) is defined as:

`CANCEL(t_elapsed_ms, t_threshold_ms, touch_up_received) = (t_elapsed_ms > t_threshold_ms) ∧ ¬touch_up_received`

**Variables:**
| Variable | Symbol | Type | Range | Description |
|---|---|---|---|---|
| Elapsed time | `t_elapsed_ms` | double (ms) | ≥ 0 | `(IMonotonicClock::NowMs() < t_start_ms) ? 0.0 : (double)(IMonotonicClock::NowMs() − t_start_ms)`, where `t_start_ms` was recorded when the touch-down entered DrainTick() phase 1 (before the 1-tick hold buffer — NOT at the drain tick when slip-left/right fires to PM). **Clock-stall guard:** `IMonotonicClock::NowMs()` returns `double` (not `uint64`). The `now < start` branch guards against clock stall or NTP adjustment causing `now` to precede `start` — a real but rare event. Do NOT use `max(0.0, NowMs() − t_start_ms)` — while `double` subtraction does not wrap, `max(0.0, ...)` silently truncates negative values and masks the clock-stall condition, whereas the branch form explicitly evaluates the ordering and returns 0.0 on stall. The branch form is authoritative. Both values are in milliseconds. **Non-finite guard:** `IMonotonicClock::NowMs()` is contractually required to return a finite double. DrainTick Step 2 must check `std::isfinite(NowMs())` once per tick before iterating contacts; if the value is non-finite, log `IS_CLOCK_NON_FINITE` and skip the cancel-timer loop for this tick (treats the tick as a clock-stall, no resting transitions). `+Inf` from a buggy clock would otherwise immediately resting-transition every active contact (a silent total cancel-system failure in one tick). |
| Cancel threshold | `t_threshold_ms` | double (ms) | > 0 | `CANCEL_TIMER_MS = 180ms`. **Initialization guard:** If `CANCEL_TIMER_MS ≤ 0` at IS construction, log error and halt — a value of 0 causes every contact to immediately expire on the first DrainTick, making the IS non-functional. This is analogous to the halting guards on F-1 (`B ≥ W`) and F-2 (`R_max ≤ 0`). Safe tuning range: [80, 400]ms. |
| Touch-up received | `touch_up_received` | bool | {true, false} | `true` if touch-up for this contact ID has been received. |

**Clock requirement:** Both `t_start_ms` and the per-tick `NowMs()` call must come from
`IMonotonicClock::NowMs()`. Do NOT use `FPlatformTime::Seconds()` (returns **seconds**
— comparing seconds against 180ms requires 180 seconds to elapse; the timer never
fires under a naïve implementation) or `FApp::GetCurrentTime()` (pauses during
backgrounding).

**Output Range:** Boolean. `true` = mark contact resting (IS-internal state transition
only). No external event dispatched. Cancel-slip is not emitted to Player Movement.

**Example:** t_threshold_ms = 180ms.
(t_elapsed=200, touch_up=false) → true — contact resting; `IS_CANCEL_DISPATCHED` and
`IS_CONTACT_RESTING` fire; no PM event. (t_elapsed=180, touch_up=false) → false —
exactly at threshold; strict greater-than required. (t_elapsed=200, touch_up=true) →
false — touch ended cleanly. (t_elapsed=90, touch_up=false) → false — not yet expired.

**COUNTDOWN guard:** The cancel timer check is suspended during `COUNTDOWN` state (see
DrainTick Step 2). A pre-positioned finger must not be deactivated mid-countdown. The
timer accumulates from `t_start_ms` through COUNTDOWN but cannot fire. At the COUNTDOWN→RUNNING
transition, the IS-21-A1 tracking reset clears all contacts (including any whose timers
have already exceeded 180ms), so no pre-positioned contact can fire ContactResting on the
first RUNNING tick — that contact no longer exists in the tracking set. Only contacts
introduced after the run starts are eligible for cancel-timer evaluation in RUNNING.

**DrainTick phase sequence:** Each `DrainTick()` call executes in four ordered steps.
Step 1 processes events in one pass, FIFO order (OS-delivery order). Touch-up events
for contacts not in the active tracking set are orphan events (pre-IS-construction
contacts or duplicates); they are logged as `IS_TOUCH_UP_ORPHAN` and discarded.

```
DrainTick():

  // Viewport portrait assertion (one-shot, deferred from StartupModule)
  // FInputSystem members: bViewportChecked (initialized false at construction).
  // Per OQ-4 — assertion cannot run at module load (GEngine->GameViewport is null
  // until the first level loads). Three-state machine:
  //   (a) viewport pending (null) → no-op early-return BEFORE counter increment,
  //       BEFORE Steps 1–3, do NOT set flag. Pending events remain in the queue
  //       until the next DrainTick when viewport is available.
  //   (b) viewport present + W >= H → misconfiguration (landscape on portrait-only
  //       game). Log error, unregister FTSTicker, halt.
  //   (c) viewport present + W <  H → set flag, fall through to normal body.
  // This same flag also gates the first-run prompt creation in §Visual Elements
  // (the prompt widget can only be sized once viewport dimensions are known).
  if not bViewportChecked:
    viewport = GEngine->GameViewport
    if viewport == nullptr:
      return   // pending: do NOT increment current_drain_tick_index; do NOT process events
    W, H = GetViewportSize(viewport)
    if W >= H:
      log IS_VIEWPORT_ASPECT_WRONG { W: W, H: H }
      UnregisterFTSTicker()
      return   // misconfigured: halt (will not retry; ticker removed)
    bViewportChecked = true
    // fall through — viewport valid; normal DrainTick body proceeds this tick

  // Step 0: advance drain tick counter
  current_drain_tick_index++

  // Snapshot run state once — all gate decisions this tick use this value
  current_state = IRunStateProvider::GetCurrentState()

  // State-transition edge detection (FInputSystem member: prev_state, initialized to IDLE at construction)

  // IS-21-A1: COUNTDOWN→RUNNING tracking reset
  // Pre-positioned contacts accumulated during COUNTDOWN are cleared before the first RUNNING step.
  // Prevents spurious ContactResting + double-pulse on the first RUNNING drain tick for
  // fingers already held when the countdown ends (Pillar 5: only player-initiated RUNNING
  // actions generate feedback).
  if prev_state == ERunState::COUNTDOWN AND current_state == ERunState::RUNNING:
    // ContactRestingLeft/Right are transient (50ms flash) — no Clear() needed or valid
    clear tracking_set entirely
    clear fingerindex_to_contactid_map entirely
    log IS_COUNTDOWN_RESET

  // First-run prompt: read flag at COUNTDOWN entry to decide whether to show zone labels
  if prev_state != ERunState::COUNTDOWN AND current_state == ERunState::COUNTDOWN:
    b_first_run_prompt = IBootFlagStore::GetFlag(TEXT("FIRST_RUN_PROMPT_ENABLED"))
    // (prompt widget creation is UI team responsibility — IS passes b_first_run_prompt)

  prev_state = current_state  // update before Steps 1–3; stored as FInputSystem member

  // Formula helpers — exact implementations required; alternatives that omit the
  // ordering branch are incorrect (see F-3 and F-4 variable tables):
  //
  //   safe_elapsed_ms(now_ms, start_ms):         // [F-3] clock-stall guard
  //     return (now_ms >= start_ms) ? (double)(now_ms - start_ms) : 0.0
  //
  //   safe_delta(a, b):                          // [F-4] uint64 unsigned-wrap guard
  //     return (a >= b) ? (a - b) : (b - a)

  // Step 1: dequeue + classify [F-1, F-2]
  // One-pass FIFO in OS-delivery order. Touch-up events for contacts not in
  // tracking_set are orphan events; logged as IS_TOUCH_UP_ORPHAN and discarded.
  for each event E dequeued from OS queue in arrival order:

    if E is touch-down:
      if not E.is_direct:                          // iOS: UITouchTypeDirect; Android: TOOL_TYPE_FINGER/UNKNOWN
        log IS_STYLUS_REJECTED; continue           // stylus rejection is always silent — no haptic or visual fires

      if not std::isfinite(E.radius_mm):           // NaN OR ±Inf — both are ITouchRadiusProvider contract violations; treat as -1.0f (pass, no data)
        log IS_RADIUS_NAN_GUARD { OS_FingerIndex: E.OS_FingerIndex, raw_radius_mm: E.radius_mm }  // contact_id not yet assigned; log by OS_FingerIndex. Raw value captured for bridge-bug diagnosis (NaN vs +Inf vs -Inf are distinguishable in the log).
        E.radius_mm = -1.0f

      if not PALM_PASS(E.radius_mm, R_max):        // [F-2] PALM_PASS(r_mm, R_max) = (r_mm ≤ R_max)
        log IS_PALM_REJECTED
        if HAPTIC_CAPABILITY == VISUAL:
          IVisualDispatch::Fire(EVisualEvent::InputRejectedMicroFlash)
        elif HAPTIC_CAPABILITY IN {FULL, DURATION}:
          IHapticDispatch::Fire(EHapticEvent::InputRejected)
        // AUDIO and NONE tiers: no signal — palm contacts are typically unnoticed by the player
        continue

      if E.OS_FingerIndex in fingerindex_to_contactid_map:   // OEM edge: OS re-delivers touch-down for active index
        log IS_DUPLICATE_CONTACT_ID_REJECTED { OS_FingerIndex: E.OS_FingerIndex }; continue
        // contact_id not yet assigned at this site — log by OS_FingerIndex only (see schema)

      contact_id = NextContactId++                 // IS-assigned; never reused within session
      fingerindex_to_contactid_map[E.OS_FingerIndex] = contact_id
      log IS_TOUCH_RECEIVED { OS_FingerIndex: E.OS_FingerIndex, contact_id: contact_id, radius_mm: E.radius_mm }

      zone = ZONE(E.x_native_px, W, B)            // [F-1] clamp x to [0,W] before call; log IS_OUT_OF_BOUNDS_COORDINATE if clamped
      log IS_ZONE_CLASSIFIED

      if zone == DEAD_BAND:
        log IS_DEAD_BAND_DISCARDED
        if DEAD_BAND_FEEDBACK_ENABLED:
          if HAPTIC_CAPABILITY == NONE:
            if current_state NOT IN {DEAD, RESOLVING, ABORTED}:  // NONE-tier: state-gated per EVisualEvent::DeadBandContactFlash spec
              IVisualDispatch::Fire(EVisualEvent::DeadBandContactFlash)
              log IS_NONE_TIER_DEAD_BAND_VISUAL_FIRED { contact_id: contact_id, x_native_px: E.x_native_px, run_state: current_state }
          else:
            IHapticDispatch::Fire(EHapticEvent::DeadBandContact)  // no state gate (Haptic Vocabulary §2 + R-1)
        fingerindex_to_contactid_map.remove(E.OS_FingerIndex)     // DEAD_BAND: undo map entry (contact not tracked)
        continue

      tracking_set[contact_id] = {
        drain_tick_index : current_drain_tick_index,
        t_start_ms       : IMonotonicClock::NowMs(),  // cancel timer anchors at receipt [F-3], NOT at dispatch
        zone             : zone,
        b_dispatched     : false,
        resting          : false,
        os_finger_index  : E.OS_FingerIndex,           // stored for pending_removals drain reverse-lookup [B1]
        contact_id       : contact_id,                 // stored so Step 3 can reference C.contact_id [B2]
      }

    if E is touch-up:
      contact_id = fingerindex_to_contactid_map.get(E.OS_FingerIndex)
      if contact_id == null OR contact_id not in tracking_set:
        fingerindex_to_contactid_map.remove(E.OS_FingerIndex)   // no-op if null; Case B (map-entry-but-not-in-tracking-set) is dead code post-DEFECT-1 — pending_removals drain removes both maps simultaneously, so the map entry is already gone when the touch-up arrives
        log IS_TOUCH_UP_ORPHAN; continue           // pre-IS contact or post-collision-discard orphan; discard
      C = tracking_set[contact_id]
      // ContactRestingLeft/Right are transient (50ms flash) — no Clear() needed or valid
      remove contact_id from tracking_set
      fingerindex_to_contactid_map.remove(E.OS_FingerIndex)
      log IS_TOUCH_UP

  // Step 2: cancel timer check [F-3]
  // CANCEL(t_elapsed_ms, t_threshold_ms, touch_up_received) = (t_elapsed_ms > t_threshold_ms) ∧ ¬touch_up_received
  // touch_up_received is always false here: touch-up removes the contact from
  // tracking_set in Step 1, so any contact still present has not received touch-up.
  // Resting flags written before Step 3 — a contact marked resting this tick is
  // excluded from F-4 candidate selection, preventing phantom collision with a held finger.
  // COUNTDOWN guard: cancel timer suspended during COUNTDOWN — a pre-positioned finger
  // must not be deactivated mid-countdown. Timer does NOT reset; it resumes from the
  // original t_start_ms on the first RUNNING tick (contact deactivates immediately if
  // already past 180ms by then).
  // Non-finite NowMs guard: sample once per tick and validate. +Inf from a buggy clock
  // would otherwise immediately resting-transition every active contact in one tick.
  now_ms_snapshot = IMonotonicClock::NowMs()
  if not std::isfinite(now_ms_snapshot):
    log IS_CLOCK_NON_FINITE { raw_now_ms: now_ms_snapshot }
    // skip Step 2 this tick — treat as clock stall, no resting transitions
  else:
    for each C in tracking_set where C.resting == false AND current_state != COUNTDOWN:
      t_elapsed_ms = safe_elapsed_ms(now_ms_snapshot, C.t_start_ms)             // [F-3] clock-stall guard, snapshotted NowMs
      if CANCEL(t_elapsed_ms, t_threshold_ms, touch_up_received=false):         // [F-3]
        C.resting = true
        log IS_CANCEL_DISPATCHED, IS_CONTACT_RESTING
        if current_state NOT IN {DEAD, RESOLVING, ABORTED}:
          if HAPTIC_CAPABILITY == NONE:
            IVisualDispatch::Fire(C.zone == LEFT
                                  ? EVisualEvent::ContactRestingLeft
                                  : EVisualEvent::ContactRestingRight)
            log IS_NONE_TIER_CONTACT_RESTING_VISUAL_FIRED { contact_id: C.contact_id, zone: C.zone, run_state: current_state }
          else:
            IHapticDispatch::Fire(EHapticEvent::ContactResting)

  // Step 3: hold-expiry dispatch + F-4 collision (deferred removal)
  // COLLISION(idx_a, z_a, idx_b, z_b) = (safe_delta(idx_a, idx_b) ≤ 1) ∧ (z_a ≠ z_b)  [F-4]
  // Outer loop iterates in ascending contact_id order — required for deterministic
  // three-touch pairwise resolution (lowest-ID pair always selected first).
  // Implementation requirement: TMap<int32, FContactState> does NOT guarantee ascending
  // key order. Use TSortedMap<int32, FContactState> or extract into TArray and Sort().
  pending_removals = []
  for each C in tracking_set (ascending contact_id order)
      where C.contact_id NOT IN pending_removals
        AND C.drain_tick_index < current_drain_tick_index   // 1-tick hold: received in a prior tick
        AND C.b_dispatched == false
        AND C.resting == false:

    collider = first D in tracking_set (ascending contact_id order)
               where D.contact_id != C.contact_id
                 AND D.contact_id NOT IN pending_removals
                 AND D.drain_tick_index <= current_drain_tick_index  // inner: D may be current-tick; outer hold guard (C) ensures C is prior-tick
                 AND D.b_dispatched == false
                 AND D.resting == false
                 AND COLLISION(C.drain_tick_index, C.zone, D.drain_tick_index, D.zone)  // [F-4]

    if collider != null:
      log IS_COLLISION_DETECTED result=COLLISION
      if current_state NOT IN {DEAD, RESOLVING, ABORTED, COUNTDOWN}:
        if HAPTIC_CAPABILITY == NONE:
          IVisualDispatch::Fire(EVisualEvent::R3CollisionDesaturation)
          log IS_NONE_TIER_VISUAL_FIRED { contact_id_a: C.contact_id, contact_id_b: collider.contact_id, run_state: current_state }
        else:
          IHapticDispatch::Fire(EHapticEvent::R3Collision)
      pending_removals.add(C.contact_id)
      pending_removals.add(collider.contact_id)
    else:
      log IS_COLLISION_DETECTED result=PASS
      dispatch slip-left or slip-right for C.zone
      C.b_dispatched = true
      log IS_SLIP_DISPATCHED
      // COUNTDOWN acknowledgement: PM ignores COUNTDOWN slips (no gameplay response);
      // IS fires a sub-light acknowledgement so the player receives Pillar 5 feedback
      // confirming their tap registered. State gate: COUNTDOWN only (not IDLE/RUNNING).
      //
      // Tier-specific signal (re-review 26 adjudication):
      //   FULL/DURATION/AUDIO/VISUAL → reuses EHapticEvent::DeadBandContact — the
      //     transition channel argument holds: amplitude/audio step-change at
      //     COUNTDOWN→RUNNING (DeadBandContact → SlipConfirmed) teaches the state
      //     transition. Same "receipt without game verb" semantic class.
      //   NONE                       → fires zone-targeted ContactRestingLeft/Right
      //     instead of the center-strip DeadBandContactFlash. Rationale: on NONE-tier
      //     no amplitude step-change exists (no haptic, visual-only). Reusing
      //     DeadBandContactFlash would (a) render a center-strip flash for a
      //     correctly-labeled left/right tap (spatial lie at first contact), and
      //     (b) collapse the COUNTDOWN-ack signal into the RUNNING dead-band-miss
      //     signal with no differentiator. Zone-targeted ContactRestingLeft/Right
      //     fires at the actual tap zone, preserves the Pillar 5 acknowledgement,
      //     and reuses an existing event.
      if current_state == ERunState::COUNTDOWN:
        if HAPTIC_CAPABILITY == NONE:
          IVisualDispatch::Fire(C.zone == LEFT
                                ? EVisualEvent::ContactRestingLeft
                                : EVisualEvent::ContactRestingRight)
          log IS_NONE_TIER_COUNTDOWN_ACK_VISUAL_FIRED { contact_id: C.contact_id, zone: C.zone }
        else:
          IHapticDispatch::Fire(EHapticEvent::DeadBandContact)
          log IS_HAPTIC_FIRED { haptic_type: dead_band_feedback, capability_tier: HAPTIC_CAPABILITY }
          log IS_COUNTDOWN_ACK_HAPTIC_FIRED { contact_id: C.contact_id, capability_tier: HAPTIC_CAPABILITY }
      // UX-4: first-run prompt dismissal — only in RUNNING; COUNTDOWN taps do not dismiss
      if current_state == ERunState::RUNNING AND b_first_run_prompt:
        IBootFlagStore::SetFlag(TEXT("FIRST_RUN_PROMPT_ENABLED"), false)
        b_first_run_prompt = false
        log IS_FRP_DISMISSED

  // Apply deferred removals — idempotent; a contact may appear in two pairs.
  // Also remove from fingerindex_to_contactid_map immediately: if the OS omits
  // the touch-up for this contact (documented OEM behavior), a later tap on the
  // same OS_FingerIndex would otherwise hit IS_DUPLICATE_CONTACT_ID_REJECTED.
  for each id in pending_removals:
    if id in tracking_set:
      fingerindex_to_contactid_map.remove(tracking_set[id].os_finger_index)
      remove id from tracking_set

  // ABORTED level-detection flush: runs on EVERY DrainTick where state == ABORTED,
  // not only on first transition. Stale contacts (e.g. held when app was suspended)
  // are cleared each tick — prevents any touch received during ABORTED from ever
  // reaching the dispatch predicate on the following tick.
  if current_state == ERunState::ABORTED:
    // ContactRestingLeft/Right are transient (50ms flash) — no Clear() needed or valid
    clear tracking_set entirely
    clear fingerindex_to_contactid_map entirely                // stale entries cause IS_DUPLICATE_CONTACT_ID_REJECTED on reused OS_FingerIndex after resume
```

---

The **Same-Frame Collision Detection** formula (F-4) is defined as:

`COLLISION(drain_tick_index_a, z_a, drain_tick_index_b, z_b) = (delta ≤ 1) ∧ (z_a ≠ z_b)`

where `delta = (a ≥ b) ? (a − b) : (b − a)` — safe unsigned subtraction. Both indices
are `uint64`; naive `|a − b|` wraps to ~1.84×10^19 when b > a, causing adjacent-tick
collisions to be silently missed. Always use the branch form above.
**Implementor warning:** Do not use `std::abs(int64(a) − int64(b))` as an alternative — the
`int64` cast is undefined behavior if `a − b` overflows a signed 64-bit integer (possible
when indices are far apart late in a session). The branch form is both correct and branchless
to optimize.

**Precondition:** `z_a`, `z_b` ∈ {LEFT, RIGHT}. DEAD_BAND contacts are filtered from the
active tracking set before F-4 is evaluated and must never appear as `z_a` or `z_b`.

**Variables:**
| Variable | Symbol | Type | Range | Description |
|---|---|---|---|---|
| Drain tick index A | `drain_tick_index_a` | uint64 | ≥ 0 | Monotonic counter value at the time touch A was processed. Assigned by `FInputSystem::DrainTick()` — all contacts dequeued in the same `DrainTick()` call share the same index. |
| Zone A | `z_a` | enum | {LEFT, RIGHT} | From ZONE() for touch A. DEAD_BAND contacts discarded before this check. |
| Drain tick index B | `drain_tick_index_b` | uint64 | ≥ 0 | Monotonic counter value when touch B was processed. |
| Zone B | `z_b` | enum | {LEFT, RIGHT} | From ZONE() for touch B. |

**drain_tick_index contract:** `drain_tick_index` is incremented exactly once at the
start of each `DrainTick()` call. Two contacts are simultaneous (delta = 0) if they were
dequeued in the same drain tick; adjacent (delta = 1) if in consecutive drain ticks. This
integer comparison is immune to OS scheduling jitter and floating-point rounding at tick
boundaries. See ADR-0003 for drain queue architecture and FTSTicker registration.

**Output Range:** Boolean. `true` = discard both events. `false` = both proceed
independently.

Co-arriving touches in the same `DrainTick()` call have `drain_tick_index_a ==
drain_tick_index_b` (delta=0, always collide); touches in adjacent drain ticks have
delta=1 (also collide). Only touches two or more drain ticks apart proceed. The
collision window equals one game-frame interval — approximately 16.67ms at 60 FPS,
33.33ms at 30 FPS — not a fixed real-time value. This is intentional: the mechanic
window scales with frame rate rather than creating ambiguous cross-frame edge cases.

**Dispatched-contact exclusion:** Once a contact has dispatched its slip event, it is
excluded from F-4 collision evaluation starting from the drain tick on which it
dispatches (inclusive) and for all subsequent drain ticks while it remains held
(`b_dispatched == true`). If finger A dispatches `slip-left` at tick N+1 and remains
held, and finger B arrives in the right zone at tick M (even if M − N ≤ 1), finger B
is NOT paired with finger A for collision — finger B dispatches independently as
`slip-right`. Finger A's cancel timer is NOT reset by B's arrival or dispatch —
A's `t_start_ms` remains unchanged from A's touch-down. A deactivates normally at
180ms from A's own touch-down if it remains held past the threshold. This rule enables
rapid left-then-right correction bursts (<250ms) without silent discards; the cancel
timer continues so long-held post-dispatch contacts still deactivate.

**Three-touch pairwise resolution:** Find the contact with the lowest `contact_id`; find
the contact with the lowest `contact_id` in the opposite zone; if both exist and their
`drain_tick_index` delta ≤ 1, discard that pair. Apply this rule recursively to the
remaining contacts until no opposite-zone pair shares adjacent drain tick indices. **Base case:** If the remaining set contains
no LEFT-RIGHT pair with `drain_tick_index` delta ≤ 1 (all remaining contacts share the
same zone, only one contact remains, or remaining opposite-zone pairs all have delta > 1),
all remaining contacts proceed independently — no collision. Tie-breaking
is deterministic within a session regardless of OS event delivery order.

**contact_id assignment:** IS assigns its own monotonic `contact_id` (type: `int32`) at
the moment each OS touch event enters `DrainTick()`. Each new contact receives
`NextContactId++`, where `NextContactId` is initialized to 0 at IS construction and the
active contact tracking set is empty. IDs are session-scoped and never reused within a
session, regardless of OS pointer-ID reuse.

**FingerIndex → contact_id mapping:** `contact_id` is an IS concept; OS `FingerIndex`
(from `FPointerEvent::GetPointerIndex()`) is the platform concept. IS maintains an
internal map `OS_FingerIndex → contact_id` for the duration of each contact's lifetime.

| Step | What happens |
|---|---|
| Touch-down enters DrainTick() | If OS_FingerIndex not in map → assign new `contact_id`; add to map |
| FTouchRadiusCache read | Keyed by `OS_FingerIndex` (same as GetPointerIndex()) |
| Touch-up enters DrainTick() | Remove OS_FingerIndex from map; `contact_id` retired |
| IS debug log | Both `OS_FingerIndex` and `contact_id` logged in `IS_TOUCH_RECEIVED` |

For contacts delivered simultaneously in the same `touchesBegan:` call (iOS), the bridge
iterates the touch set in a deterministic order (sorted by `UITouch*` pointer address,
ascending) before assigning `contact_id` values. This ensures ascending ID assignment
order is consistent regardless of OS event delivery sequence.

**Example:** drain_tick_index collisions.
(idx_a=5 LEFT, idx_b=5 RIGHT) → |0|≤1 ∧ LEFT≠RIGHT → **true** (same tick, both
discarded). (idx_a=5 LEFT, idx_b=6 RIGHT) → |1|≤1 ∧ LEFT≠RIGHT → **true** (adjacent
ticks). (idx_a=5 LEFT, idx_b=7 RIGHT) → |2| > 1 → **false** (two ticks apart).
(idx_a=5 LEFT, idx_b=5 LEFT) → LEFT=LEFT → **false** (same zone, no collision).

## Edge Cases

**Boundary convention (applies to all formulas except F-4 at the T_frame boundary):**
All formula boundaries are inclusive on the valid side. x at the exact band edge
classifies as a zone (not DEAD_BAND). r = R_max exactly passes PALM_PASS. t_elapsed =
t_threshold exactly does NOT fire cancel (cancel requires t > t_threshold). Convention:
when in doubt, the tap was intentional.

**F-4 boundary — adjacent drain ticks:** `drain_tick_index` delta = 1 (adjacent
`DrainTick()` calls) → COLLISION = true — both events are discarded. This is the
intended behavior: two touches arriving in adjacent drain ticks (one game-frame interval
apart) are treated as simultaneous. The `≤ 1` boundary is deliberate — both inputs
arrived in the same or immediately adjacent game-mechanic window. The interval is
frame-rate-dependent (~16.67ms at 60 FPS, ~33ms at 30 FPS), not a fixed real-time
threshold.

- **Touch-down lands exactly on the inner edge of the exclusion band
  (x = W/2 − B/2 or x = W/2 + B/2):** Classifies as LEFT or RIGHT respectively —
  not DEAD_BAND. The boundary belongs to the zone.

- **Contact radius equals R_max exactly (r = R_max = 6.0mm):** PALM_PASS returns
  true. Event proceeds. Only r > R_max is rejected.

- **t_elapsed equals t_threshold exactly (t_elapsed_ms = 180ms):** CANCEL returns
  false. Cancel fires only at t > 180ms.

- **Device has a 90Hz or 120Hz display (ProMotion):** `DrainTick()` fires once per game
  frame, not once per display frame. SLIPSTORM targets 60 FPS; on a ProMotion device,
  drain rate is 60 Hz regardless of the 120Hz display refresh rate. ProMotion-adaptive
  gameplay (120 FPS game loop) is **out of scope for MVP** — the IS is designed around
  a 60 Hz FTSTicker interval. If the game loop is run at 120 FPS on a ProMotion device,
  `drain_tick_index` advances twice per 60Hz period, halving the effective collision
  window to ≤8ms; this is not a supported operating condition. OS touch events arriving
  between game frames are queued and processed on the next `DrainTick()`. Collision
  behavior (delta ≤ 1 drain tick) is equal across all display refresh rates at a given
  game frame rate.

- **COUNTDOWN→RUNNING transition with a finger already held (IS-21-A1):** Any contacts
  accumulated during COUNTDOWN (including pre-positioned fingers and any whose cancel
  timers have already exceeded 180ms) are cleared at the COUNTDOWN→RUNNING transition
  by the IS-21-A1 tracking reset in DrainTick (before Step 1 of the first RUNNING tick).
  The player's finger must lift and re-press after run start to register a contact in
  RUNNING. This prevents spurious ContactResting + double-pulse haptic on the first
  RUNNING drain tick from a finger held through the countdown (Pillar 5: only
  player-initiated RUNNING actions generate feedback). `IS_COUNTDOWN_RESET` is logged
  when the flush runs.

- **Cancel timer fires while a second valid touch is simultaneously active in the
  opposite zone:** Each contact is processed independently. The held contact
  transitions to resting (IS-internal); the valid opposite-zone touch fires its slip
  event normally. These are unrelated IS-internal state transitions.

- **Cancel timer fires on a contact while a valid slip arrives from the opposite zone
  within T_frame:** The resting-flag transition is excluded from F-4 collision
  evaluation. The valid slip proceeds normally. The resting-flag transition does not
  count as a collision partner and does not suppress the opposite-zone slip.

- **App is backgrounded mid-touch (incoming call, home button):** OS delivers a
  synthetic touch-cancel, treated as touch-up — cancel timer resolves normally. If the
  OS does not deliver touch-cancel (OEM edge case): cancel timer fires on the first
  frame tick after resume if t_elapsed_ms > 180ms. `IMonotonicClock::NowMs()` continues
  counting across suspension on both platforms (iOS: `mach_continuous_time()`; Android:
  `CLOCK_BOOTTIME`) — no additional platform bridge required. A finger held through a
  phone call resume will be resting-flagged on the first post-resume frame — the first
  intentional tap after resume may be silently blocked; this is an accepted edge-case
  outcome.

- **Run transitions to ABORTED:** On every `DrainTick()` call where
  `current_state == ERunState::ABORTED` (level detection — runs each tick in ABORTED state,
  not only on first transition; see DrainTick pseudocode ABORTED flush section), IS must
  flush the entire active contact tracking set — all tracked contacts are removed, since OS
  touch state after a background-suspend abort is stale and no matching touch-up events will
  arrive. The `fingerindex_to_contactid_map` is also cleared to prevent stale FingerIndex
  entries blocking the first post-resume touch. `IVisualDispatch::Clear()` is NOT called —
  `ContactRestingLeft/Right` are transient (50ms flash); any in-progress flash self-clears
  within 50ms and does not require explicit teardown. `Clear()` on a transient event is a
  programming error.
  **Step 2 in ABORTED:** The cancel timer (Step 2) runs before the end-of-tick flush.
  Contacts whose 180ms timer has already expired log `IS_CANCEL_DISPATCHED` and
  `IS_CONTACT_RESTING` before being removed by the flush — these log entries are expected
  in ABORTED ticks and do not indicate incorrect behavior; no haptic or visual fires
  (suppressed by the ABORTED run-state gate). QA should not flag these as phantom events.
  **COUNTDOWN→ABORTED transition:** If the app suspends during COUNTDOWN, the ABORTED flush
  clears both maps correctly, but `IS_COUNTDOWN_RESET` does NOT fire (IS-21-A1 is
  COUNTDOWN→RUNNING only). This is intentional — ABORTED is a terminal cleanup path;
  IS_COUNTDOWN_RESET is reserved for the specific COUNTDOWN→RUNNING pre-positioning
  scenario.

- **Touch held when the 60-second run timer expires:** IS continues emitting events
  normally (R-5). Player Movement gates all events once the run state transitions.

- **Finger rolls across the zone boundary mid-hold:** Touch retains its original zone
  classification for its entire lifetime. Zone is evaluated at touch-down only; drag
  and boundary-crossing generate no new events.

- **Thumb slides from LEFT zone to RIGHT zone without lifting (correction attempt):**
  The contact is still the same touch ID — R-2 rule 2 ("Touch ID is new") fails. No
  new slip-right fires. The original slip-left has already been dispatched; the held
  contact continues aging toward the cancel timer. Re-tap (lift + new touch-down) is
  the only way to issue a correction slip in the opposite direction. This is intentional:
  the game's reflex pattern is tap-based, not swipe-based.

- **Three or more touches arrive within the same drain-tick window:** Pairwise resolution
  — find the contact with the lowest `contact_id`; find the contact with the lowest
  `contact_id` in the **opposite zone**; if both exist and their `drain_tick_index`
  delta ≤ 1, discard that pair. Apply this rule recursively on the remaining contacts
  until no opposite-zone pair shares an adjacent drain_tick_index. No full-frame
  contamination. Tie-breaking is deterministic within a session regardless of OS event
  delivery order.
  *Example: 4 contacts [L:1, R:2, L:3, R:4] in same tick → pair (1,2) discarded → pair
  (3,4) discarded → no events dispatch. [L:1, L:2, R:3, L:4] → pair (1,3) discarded →
  remaining [L:2, L:4] — same zone, no collision → both proceed.*

- **Touch-down classifies as DEAD_BAND:** No event emitted, no touch ID registered,
  no cancel timer started. The contact does not affect any subsequent valid contacts.

- **Contact radius grows beyond R_max after a valid touch-down:** Contact remains
  valid. PALM_PASS is evaluated once at touch-down only.

- **OS reuses a touch ID immediately after the previous contact with that ID was
  released:** New contact is valid if the previous contact is no longer in the active
  tracking set. "New touch ID" means not currently active, not historically unique.

- **Touch-up and cancel timer fire in the same drain tick:** Touch-up takes
  priority. IS_TOUCH_UP is dispatched; IS_CANCEL_DISPATCHED does NOT fire. The
  cancel timer is cancelled before any dispatch occurs. Touch-up always wins
  regardless of where the timer stood in its countdown.

- **Contact ID cleanup at touch-up boundary:** The contact_id is removed from the
  active tracking set immediately upon touch-up receipt within the drain tick that
  processed the touch-up event. Any subsequent drain tick that references that
  contact_id treats it as a new, independent contact (subject to full PALM_PASS
  evaluation on next touch-down).

- **Missed touch-up (OS drops the touch-up event):** If the OS fails to deliver a
  touch-up event (background interrupt, OEM edge case), the `FTouchRadiusCache` slot
  for that `OS_FingerIndex` remains occupied with stale radius data. To prevent
  permanent slot poisoning, any `FTouchRadiusCache` slot that has not been updated for
  **2 seconds** is invalidated at **enqueue time** — inside `HandleTouchStartedEvent()`
  when the next touch arrives on that slot. The age is measured by `IMonotonicClock::NowMs()`
  at enqueue time (not drain time — the bridge may overwrite the slot between enqueue and
  drain). The 2-second window is deliberately conservative — a shorter TTL risks clearing
  a genuinely-held finger on a slow device. Stale slot invalidation is logged as
  `IS_STALE_SLOT_INVALIDATED { OS_FingerIndex, age_ms }` in debug builds. This TTL is
  specified in ADR-0001; IS owns this cleanup inside `HandleTouchStartedEvent()`.

## Dependencies

**Hard dependencies (upstream — this system cannot function without these):**

*Game-system dependencies:* None. The Input System has no upstream game-system
dependencies. It depends only on the OS touch layer and the UE5.7 Enhanced Input
plugin, both of which are platform prerequisites, not designed game systems.

*Implementation prerequisites (must be Accepted/complete before IS enters the
implementation sprint):*

| Document | Type | Status | What it specifies |
|---|---|---|---|
| ADR-0001 (`docs/architecture/adr-0001-palm-rejection-rmax-calibration.md`) | ADR | Proposed | R_max=6.0mm, `FTouchRadiusBridgePlugin`, `ITouchRadiusProvider`. Must be Accepted before IS can be marked Accepted. |
| ADR-0002 (`docs/architecture/adr-0002-haptic-platform-bridge.md`) | ADR | Proposed | `FHapticPlatformPlugin`, `IHapticDispatch`, `HAPTIC_CAPABILITY` detection. Must be Accepted before IS or PM enters implementation. |
| `docs/architecture/platform-seam-interfaces.md` | Spec | Complete | `IMonotonicClock`, `SimulateTouch` seam, `IRunStateProvider`. Required before IS unit tests can be written. |
| `design/ux/input-feedback.md` | UX spec | STUB — producer action required | Audio/visual fallback specs for all four haptic events across AUDIO/VISUAL/NONE tiers. Stub created 2026-05-31 (re-review 11). Audio-director + UX designer sign-off required before IS implementation sprint. Escalated to producer — see open items in the stub file. |
| ADR-0003 (`docs/architecture/adr-0003-drain-queue-architecture.md`) | ADR | Proposed | 60Hz drain queue architecture: `FInputSystem` as `IInputProcessor`, `FTSTicker` at 60Hz, `drain_tick_index` contract, thread safety. Must be Accepted before IS implementation begins. **IInputProcessor method name — RESOLVED (re-review 11, 2026-05-31):** `HandleTouchStartedEvent` / `HandleTouchEndedEvent` are the correct virtual methods for per-finger mobile touch events. `HandleMouseButtonDownEvent/Up` receive zero touch events on iOS/Android with `bUseMouseForTouch=false`. ADR-0003 code samples updated accordingly. **Remaining pre-Accepted hardware verification gate** (not a design question — must be satisfied before ADR-0003 moves to Accepted): verify that UE 5.7 IOSView assigns `FPointerEvent::GetPointerIndex()` in the same ascending-pointer-address order as `FTouchRadiusBridgePlugin`; submit two simultaneous touches with distinct radii and verify each `contact_id` receives the correct radius. Required pre-merge integration test — see ADR-0003 §contact_id vs OS FingerIndex. |
| OQ-7 (RSM UE object type) | Design decision | **RESOLVED 2026-06-01** | `URunStateMachineSubsystem : UGameInstanceSubsystem`. `FRSMRunStateProvider` holds `TWeakObjectPtr<URunStateMachineSubsystem>`, resolved via `UGameInstance::GetSubsystem<URunStateMachineSubsystem>()`. GC-safety contract: TWeakObjectPtr prevents dangling pointer if subsystem outlives IS. Required before IS construction site is implemented. See `docs/architecture/platform-seam-interfaces.md` for `IRunStateProvider` interface contract. |

**Required Before Implementation — UE 5.7 API Verification Gates:**

These are not design questions — they are post-cutoff API verification tasks (UE 5.7
released after LLM training cutoff). All must be checked before IS implementation begins.

- **UE-1 (IInputProcessor method names):** Verify `HandleTouchStartedEvent` and
  `HandleTouchEndedEvent` exist as virtual methods in UE 5.7's `IInputProcessor`. If
  names differ, a compile error occurs immediately (loud failure — not a silent divergence).
  Fallback: if method names changed in 5.7, update IS to override the correct virtual(s).
  See ADR-0003 code samples for current usage.

- **UE-2 (FingerIndex → contact_id ordering):** Verify that UE 5.7 `IOSView` assigns
  `FPointerEvent::GetPointerIndex()` in the same ascending-pointer-address order as
  `FTouchRadiusBridgePlugin`'s touch-set iteration. Submit two simultaneous touches with
  distinct radii and confirm each `contact_id` receives the correct radius. Required
  pre-merge integration test — see ADR-0003 §contact_id vs OS FingerIndex for details.
  ADR-0003 cannot move to Accepted until UE-2 passes.

- **UE-ADR-1 (FTSTicker registration interval):** ADR-0003 code samples must be
  verified: `FTSTicker::GetCoreTicker().AddTicker()` must be called with interval
  `0.0f` (fire every frame, frame-rate-relative), NOT `1.0f/60.0f` (would hardcode
  60 Hz regardless of actual frame rate). If `1.0f/60.0f` is used, DrainTick() fires
  at a fixed 60 Hz on 30 FPS devices — the collision window halves to ~8ms, breaking
  the design intent. Verify ADR-0003 samples and correct before IS implementation.

- **UE-ADR-2 (GetSubsystem call syntax):** `platform-seam-interfaces.md` code samples
  must be verified: `UGameInstance::GetSubsystem<URunStateMachineSubsystem>()` is a
  non-static method and cannot be called as a static. The correct call site is
  `GetGameInstance()->GetSubsystem<URunStateMachineSubsystem>()` (instance call via
  a UGameInstance* pointer). Review all `IRunStateProvider` wiring samples and correct
  any static-call pattern before IS implementation.

- **UE-ADR-3 (TUniquePtr / TSharedPtr type mismatch):** Code samples using
  `MakeShared<>()` produce `TSharedRef<>`, not `TSharedPtr<>`. Passing `TSharedRef`
  to a `TUniquePtr` constructor is a type mismatch that will not compile. Review
  `platform-seam-interfaces.md` and `visual-dispatch-contract.md` injection wiring
  samples — any stub passed to `FInputSystem` constructor must match the expected
  pointer type (`TUniquePtr<IMonotonicClock>`, etc.). Use `MakeUnique<>()` for
  TUniquePtr construction in test wiring.

- **UE-ADR-4 (contact_id namespace boundary):** Any existing documentation using a
  "synthetic contact_id ≥ 10,000" namespace boundary for test injection is incorrect.
  IS assigns `contact_id` via `NextContactId++` starting at 0 with no reserved range.
  After 10,000 real touches in a session, IS-assigned IDs would collide with any
  "≥ 10,000" test IDs. Test stubs must use a separate injection path (e.g.,
  `SimulateTouch()` seam defined in `platform-seam-interfaces.md`) rather than
  injecting raw contact_id values into the tracking set.

**Downstream consumers (systems that depend on this one):**

| System | Interface | Dependency Type | GDD Status |
|---|---|---|---|
| Player Movement (Slip) | Receives `slip-left`, `slip-right` events | Hard — Player Movement's only input source | Approved |

**Interface contracts:**
- **Input System → Player Movement:** Emits `slip-left` or `slip-right` on every
  valid tap (R-2). Events are fire-and-forget; IS does not wait for acknowledgement.
  Cancel-slip is IS-internal only — never dispatched to PM.
- **Player Movement → Input System:** No data flows from PM to IS. PM must not call
  into IS (R-4). PM fires its own haptics (SlipConfirmed, BufferDrop) via
  `IHapticDispatch::Fire()` directly — not through IS.
- **Run State Machine → Input System:** One-way read via
  `IRunStateProvider::GetCurrentState()` — for NONE-tier R-3 visual gate only.
- **Input System → IBootFlagStore:** IS owns the `IBootFlagStore` interface for
  `FIRST_RUN_PROMPT_ENABLED` persistence. No dependency on Score Persistence. The
  shipping implementation (`FLocalStorageBootFlagStore`) uses the platform key-value
  store. IS reads the flag at COUNTDOWN entry; writes it false on the first successful
  slip dispatched while `current_state == RUNNING` (not in COUNTDOWN — see UX-4).

**OQ-1 resolution (2026-05-07; binding ADR resolved 2026-05-15, re-review 7):** Event
interface shape confirmed: `slip-left` and `slip-right` only. Cancel-slip is IS-internal
— never dispatched to PM. The binding implementation is resolved by ADR-0003:
`FInputSystem` implements `IInputProcessor` with an `FTSTicker` drain queue (UMG
transparent button overlay path not taken).

## Tuning Knobs

| Knob | Symbol | Default | Safe Range | If Too Low | If Too High |
|---|---|---|---|---|---|
| Exclusion band width | `EXCLUSION_BAND_WIDTH_MM` | 8mm | [3, 20] mm | Midline-ambiguity taps may fire wrong direction; lower bound ≥ 3mm (minimum finger-contact radius) | Reduces effective tap area; center feels dead |
| Palm rejection radius | `PALM_REJECTION_R_MAX_MM` | **6.0mm** | **[4.0, 8.0] mm** | Legitimate large-finger contacts (≥ R_max) rejected; false rejection rate rises | Palm contacts pass the radius filter; accidental input rate rises |
| Cancel timer window | `CANCEL_TIMER_MS` | 180ms | [80, 400] ms | Short holds or heavy-press taps enter resting state before intended touch-up | Resting thumbs generate accidental inputs; contacts stay active too long |
| Dead-band haptic feedback | `DEAD_BAND_FEEDBACK_ENABLED` | **on** (FULL/DURATION/VISUAL/NONE); **off** (AUDIO — pending sign-off) | {true, false} | Players cannot distinguish dead-band from input non-receipt — silent skill misread for all players (Pillar 5) | Sub-light haptic fires on every center contact; toggle via "Haptic Feedback" settings if distracting. **AUDIO tier implementation gate:** audio-director sign-off required (`design/ux/input-feedback.md` open item 2) before this defaults to `on` on AUDIO tier. Until sign-off, the AUDIO-tier default is `off`. |
| First-run prompt enabled | `FIRST_RUN_PROMPT_ENABLED` | true | {true, false} | New players receive no zone instruction | N/A — prompt auto-dismisses after first successful slip |
| ContactResting flash peak opacity | `CONTACT_RESTING_FLASH_PEAK_OPACITY` | 20% | [10%, 40%] | Flash barely visible; Pillar 5 hold-deactivation signal imperceptible — players cannot tell their finger was deactivated | Flash draws excessive visual attention to finger state during active run — Pillar 4 risk at high opacity |

**Derived constants (not direct knobs — computed at runtime from MM values and device DPI):**
```
EXCLUSION_BAND_WIDTH_PX = EXCLUSION_BAND_WIDTH_MM × screen_dpi_per_mm
PALM_REJECTION_R_MAX_PX  = PALM_REJECTION_R_MAX_MM × screen_dpi_per_mm
```
Always tune the MM values; never tune px values directly — px derivation ensures
physical consistency across screen densities.

**Non-tunable system constant:**
`INPUT_TICK_RATE` — `DrainTick()` fires once per game frame via `FTSTicker`. The drain
rate matches the game frame rate (60 Hz on 60 FPS devices, 30 Hz on 30 FPS devices).
T_frame = 1 / current_frame_rate. Changing the FTSTicker registration interval
requires an architecture change (not a knob change).

**Hard platform requirement — Minimum FPS = 30:**
Target hardware must sustain ≥ 30 FPS during RUNNING state. This is a go/no-go
certification requirement: below 30 FPS the collision window (~66ms+) exceeds the
tolerable range for skill-based play and IS behavior becomes undefined relative to
the design intent. Platform certification must verify ≥ 30 FPS on all minimum-spec
devices in the supported device matrix (AC-15b). This is not a tunable value — it
is a constraint on the supported operating envelope.

**HAPTIC_CAPABILITY** — detected at `Module::StartupModule()` and cached; not a design
knob. See ADR-0002 for detection logic (iOS `CHHapticEngine.capabilitiesForHardware()`,
Android `Vibrator.hasAmplitudeControl()` + API level). Re-detected on
`FCoreDelegates::ApplicationHasEnteredForegroundDelegate` to catch in-session changes from Settings.

**Knob interactions:**
- `CANCEL_TIMER_MS` and `EXCLUSION_BAND_WIDTH_MM` are independent — they filter at
  different stages (hold duration vs. spatial classification).
- `PALM_REJECTION_R_MAX_MM` and `EXCLUSION_BAND_WIDTH_MM` are independent — they filter
  at different layers (radius at receipt; zone at classification).
- T_frame (16.67ms) is non-configurable without an architecture change.

**PALM_REJECTION_R_MAX_MM calibration note:** The 6.0mm value is a design-phase
decision from published touch research (ADR-0001). Hardware verification gate required
before Status → Accepted: 0% finger rejection and 100% palm rejection on iPhone 16 and
Samsung Galaxy S24. If any finger tap produces r_mm > 6.0mm: revise R_max upward
before shipping.

## Visual/Audio Requirements

### Haptic Vocabulary

IS-owned and PM-owned haptic events route through `IHapticDispatch::Fire(EHapticEvent)`
(ADR-0002). IS and PM are tier-agnostic — they call `Fire(event)` and the bridge handles
dispatch. Exception: on `VISUAL` tier, palm/stylus rejection fires a micro-flash via
`IVisualDispatch` instead of `IHapticDispatch` (no audio equivalent for that event).

**IS-owned haptics (input-mechanic signals):**

1. **R-3 collision:** `IHapticDispatch::Fire(EHapticEvent::R3Collision)`. Double-pulse
   pattern. **Suppressed during DEAD, RESOLVING, ABORTED, and COUNTDOWN states** (same
   gate as the NONE-tier visual; see R-3 for rationale). Gated by HAPTIC_CAPABILITY;
   see NONE-tier exception below.

2. **Dead-band contact (Pillar 5 signal):** `IHapticDispatch::Fire(EHapticEvent::DeadBandContact)`.
   Sub-light amplitude — perceptually below R-3 collision and below slip-confirmed.
   Single ultra-brief tap, no rhythmic structure. Does not imply a game verb.
   Fires when a touch is zone-classified as DEAD_BAND (R-1), gated by
   `DEAD_BAND_FEEDBACK_ENABLED == true`.

3. **Resting-finger transition (Pillar 5 signal):** `IHapticDispatch::Fire(EHapticEvent::ContactResting)`.
   Sub-light amplitude — **distinct double-pulse pattern**: two pulses separated by a
   60ms gap (iOS: `UIImpactFeedbackGenerator(.soft, 0.2)` fired twice with 60ms delay;
   Android: `VibrationEffect.createWaveform([8, 60, 8], [35, 0, 35])`). Distinguishable
   from the single-tap `DeadBandContact` and `InputRejected` events so players can
   identify resting-state deactivation vs. dead-band vs. palm rejection. Fires when the
   180ms cancel timer expires and a contact transitions to resting (R-2 cancel timer).
   `DEAD_BAND_FEEDBACK_ENABLED` does not suppress this. **Run-state gating (asymmetric
   from R-3):** Suppressed during DEAD, RESOLVING, and ABORTED states — a sub-light
   haptic during death/resolution is noise, not a Pillar 5 signal. **Suppressed during
   COUNTDOWN by two cooperating mechanisms**: (1) the Step 2 predicate excludes contacts
   while `current_state == COUNTDOWN`; (2) the IS-21-A1 tracking reset at COUNTDOWN→RUNNING
   clears all pre-positioned contacts, so no contact from COUNTDOWN survives to fire
   ContactResting on the first RUNNING tick (Pillar 5: only player-initiated RUNNING taps
   generate feedback). Fires in IDLE, RUNNING, and COMPLETE for contacts initiated in those
   states. Distinct event type from `DeadBandContact` so the two signals are independently
   tunable.
   **NONE-tier fallback (Pillar 5 requirement):** On NONE-tier devices, `IHapticDispatch`
   is a no-op. To preserve the Pillar 5 hold-deactivation signal, IS additionally fires
   `IVisualDispatch::Fire(EVisualEvent::ContactRestingLeft)` or
   `IVisualDispatch::Fire(EVisualEvent::ContactRestingRight)` on NONE-tier when the cancel
   timer expires, targeting the zone in which the contact originated. Run-state gating is
   identical: suppressed during DEAD, RESOLVING, and ABORTED; fires in IDLE, RUNNING,
   and COMPLETE; effectively suppressed during COUNTDOWN via cancel-timer guard (same
   mechanism as haptic path). The flash is **transient** (50ms, non-persistent) — it
   self-clears after display; no `IVisualDispatch::Clear()` call is required or valid.
   `Clear()` must NOT be called for `ContactRestingLeft` or `ContactRestingRight` — these
   are transient events, and calling `Clear()` on any transient event is a programming error.
   After re-review 23, no currently-defined `EVisualEvent` is persistent; `Clear()` is never
   called in the IS pseudocode. Pillar 4 compliance: a 50ms transient flash is below the
   perceptual persistence threshold for an active run and does not constitute "chrome during
   motion." Pillar 5 delivery: the brief flash is perceptible across the full voxel luminance
   range without relying on contrast ratio (transient motion is detected by a different visual
   pathway than static overlay contrast).
   Spec: `design/ux/input-feedback.md` §NONE-Tier Visual.

4. **Palm rejection (radius filter) (Pillar 5 signal):**
   `IHapticDispatch::Fire(EHapticEvent::InputRejected)` when a contact fails the
   radius filter (`r_mm > R_max`). Sub-light amplitude — same intensity class as
   dead-band signal; below R-3 and slip-confirmed. Single ultra-brief tap, no
   rhythmic structure. Available on `FULL` and `DURATION` tiers. On `VISUAL` tier:
   IS fires a micro-flash directly via `IVisualDispatch`. On `AUDIO` and `NONE`
   tiers: no signal — palm contacts are typically unnoticed by the player.
   **Stylus rejection is always silent** — the `IS_STYLUS_REJECTED` path (non-direct
   touch) exits before any haptic or visual dispatch; this entry covers only the
   radius-filter rejection path.

5. **COUNTDOWN slip acknowledgement (Pillar 5 signal):**
   `IHapticDispatch::Fire(EHapticEvent::DeadBandContact)` on FULL/DURATION/AUDIO/VISUAL
   tiers. Fires when IS dispatches a valid slip during `COUNTDOWN` state — PM ignores
   the verb (COUNTDOWN is a no-verb state), but Pillar 5 requires the player receive
   acknowledgement that their gesture registered. Reuses `EHapticEvent::DeadBandContact`
   because the semantics are the same class on tiers where an amplitude/audio step-change
   exists between COUNTDOWN-ack (`DeadBandContact`) and RUNNING slip-confirmed
   (`SlipConfirmed`) — the step-change at COUNTDOWN→RUNNING teaches the player the run
   has started. Fires unconditionally once per dispatched slip in COUNTDOWN (no knob
   gate), immediately after `IS_SLIP_DISPATCHED` is logged. Logged as
   `IS_COUNTDOWN_ACK_HAPTIC_FIRED`.
   **NONE-tier fallback (tier-specific, per re-review 26):** IS fires
   `IVisualDispatch::Fire(EVisualEvent::ContactRestingLeft)` or
   `IVisualDispatch::Fire(EVisualEvent::ContactRestingRight)` targeting the zone in
   which the contact originated — *not* `DeadBandContactFlash`. Two reasons the
   `DeadBandContact` reuse from the non-NONE path does not extend to NONE:
   (a) on NONE-tier the haptic path is a no-op, so no amplitude step-change exists at
   COUNTDOWN→RUNNING to disambiguate COUNTDOWN-ack from RUNNING dead-band-miss;
   (b) `DeadBandContactFlash` renders at the center-strip (dead-band) location, which
   would be a spatial lie when the player tapped a correctly-labeled left or right
   zone (first contact with the game's only mechanic on first run). Zone-targeted
   `ContactRestingLeft/Right` fires at the actual tap zone and reuses an existing
   event. Logged as `IS_NONE_TIER_COUNTDOWN_ACK_VISUAL_FIRED { contact_id, zone }`.
   See §Per-Tier Teaching Channel Matrix in §Haptic Capability Tiers for the full
   tier-by-tier signal layout.

**Haptic amplitude rationale:** `DeadBandContact` and `InputRejected` use the same
sub-light amplitude class on FULL and DURATION tiers (iOS `UIImpactFeedbackGenerator(.soft, 0.25)`,
Android `VibrationEffect.createOneShot(10, 40)`). `ContactResting` uses a **distinct
double-pulse** (iOS two `.soft` pulses at 0.2 amplitude separated by 60ms; Android
`VibrationEffect.createWaveform([8, 60, 8], [35, 0, 35])`) — distinguishable from the
single tap so players can identify resting-state deactivation without visual attention.
These events are contextually distinct — `DeadBandContact` fires immediately on
zone-boundary contact, `ContactResting` fires after 180ms of holding, and `InputRejected`
fires when a contact is oversized. In SLIPSTORM's interaction model, these contexts do
not overlap. Future engineers must not collapse `DeadBandContact` and `InputRejected`
onto a distinct pattern without playtesting — amplitude differentiation may draw
inappropriate attention to mechanical scaffolding.

**PM-owned haptics (specified in PM GDD; listed here for vocabulary completeness):**

6. **Slip-confirmed:** PM fires `IHapticDispatch::Fire(EHapticEvent::SlipConfirmed)`
   on SETTLED→SLIPPING transition.

7. **Buffer-drop:** PM fires `IHapticDispatch::Fire(EHapticEvent::BufferDrop)` when
   buffer is full on second slip arrival.

---

### Haptic Capability Tiers

`HAPTIC_CAPABILITY` is detected once at `Module::StartupModule()` and cached.
Re-detected on `FCoreDelegates::ApplicationHasEnteredForegroundDelegate` to catch
in-session changes from device Settings. See ADR-0002 for detection logic and
platform haptic patterns (iOS UIFeedbackGenerator, Android Vibrator JNI). See
`design/ux/input-feedback.md` for audio and visual fallback specs.

**Detection failure fallback:** If the capability detection call throws or returns
an unexpected value, IS silently downgrades to `NONE`. Detection failure must be
logged as a warning in debug builds. No user-visible error is shown — the NONE-tier
visual path provides a minimum-viable signal on all devices.

| Tier | Capability | Behaviour | Fallback Source |
|---|---|---|---|
| `FULL` | Full amplitude + pattern control | All six `EHapticEvent` patterns as specified in ADR-0002. R-3 collision haptic suppressed during DEAD, RESOLVING, ABORTED, COUNTDOWN. | ADR-0002 iOS/Android FULL patterns |
| `DURATION` | Duration-only vibration | Duration sequences only; pulse character lost. R-3 suppressed during DEAD, RESOLVING, ABORTED, COUNTDOWN. | ADR-0002 DURATION patterns |
| `AUDIO` | No vibration hardware | `Fire()` plays audio cue per event. R-3 suppressed during DEAD, RESOLVING, ABORTED, COUNTDOWN. | `design/ux/input-feedback.md` audio spec |
| `VISUAL` | No audio either | `Fire()` triggers visual indicator per event. R-3 suppressed during DEAD, RESOLVING, ABORTED, COUNTDOWN. | `design/ux/input-feedback.md` visual spec |
| `NONE` | User-disabled or accessibility override | `Fire()` is a no-op for all events except: (1) R-3 collision → IS fires zone-edge desaturation pulse **directly** via `IVisualDispatch`; suppressed during DEAD, RESOLVING, ABORTED, and COUNTDOWN; fires in IDLE, RUNNING, and COMPLETE. (2) ContactResting → IS fires `IVisualDispatch::Fire(EVisualEvent::ContactRestingLeft)` or `IVisualDispatch::Fire(EVisualEvent::ContactRestingRight)` targeting the contact's originating zone (Pillar 5 hold-deactivation); suppressed during DEAD, RESOLVING, and ABORTED; fires in IDLE, RUNNING, and COMPLETE; suppressed during COUNTDOWN via cancel-timer guard + IS-21-A1 tracking reset at COUNTDOWN→RUNNING (no pre-positioned contact survives to fire on first RUNNING tick). (3) Dead-band tap (`DEAD_BAND_FEEDBACK_ENABLED == true`) → IS fires `IVisualDispatch::Fire(EVisualEvent::DeadBandContactFlash)` in place of the haptic; suppressed during DEAD, RESOLVING, and ABORTED; fires in IDLE, COUNTDOWN, RUNNING, and COMPLETE. (4) COUNTDOWN slip acknowledgement → IS fires `IVisualDispatch::Fire(EVisualEvent::ContactRestingLeft)` or `IVisualDispatch::Fire(EVisualEvent::ContactRestingRight)` zone-targeted to the dispatched contact's zone (NOT `DeadBandContactFlash` — would be a spatial lie on a correctly-labeled left/right tap; re-review 26 binding). Fires when a valid slip dispatches during COUNTDOWN on NONE-tier; reuses the ContactResting visual event but is gated to COUNTDOWN state only. | `design/ux/input-feedback.md` NONE-tier spec |

The NONE-tier R-3 visual is not routed via `IHapticDispatch` because IS already has
`IRunStateProvider` injected and knows the suppress condition, and the visual is a
direct IS responsibility under Pillar 5 — not a haptic fallback. The R-3 haptic
suppression during DEAD/RESOLVING/ABORTED/COUNTDOWN applies on all tiers; IS reads
`IRunStateProvider::GetCurrentState()` before dispatching either the haptic or
the NONE-tier visual.

---

### Visual Elements

1. **First-run prompt:** On the player's first run only, three zone labels appear:
   - **Left zone label:** "Tap left." — lower-middle of the left tap zone at ~78% screen height
   - **Center zone label:** "No slip" — lower-middle of the center exclusion band at ~78% screen height
   - **Right zone label:** "Tap right." — lower-middle of the right tap zone at ~78% screen height

   Labels are shown **only during the COUNTDOWN run state** (Pillar 4: no text chrome
   during active motion). They dismiss together at the COUNTDOWN → RUNNING transition.
   If the player has not yet slipped when the run ends, they reappear on the next run's
   COUNTDOWN. All three labels dismiss together — independent per-label dismissal is not
   used.

   This placement sits within the natural thumb-reach arc, above the thumb-rest area,
   where the labels are visible before the first tap without requiring the player to look
   down. No background panel; voxel backgrounds shift across the full luminance range, so
   no single static fill color can guarantee 4.5:1 contrast against arbitrary backgrounds
   (a drop shadow alone does not satisfy WCAG 1.4.3 — the standard measures contrast at
   the text body, not at glyph edges). The spec therefore mandates a **white fill +
   black outline** combination: white covers dark and mid-luminance voxels, the outline
   covers bright and white voxels.
   **Minimum label spec:**
   - **Fill color:** `LABEL_TEXT_FILL_COLOR = #FFFFFF` (white, L = 1.0). Achieves 4.5:1
     contrast against any background with L ≤ 0.183 (dark/mid voxels).
   - **Outline:** `LABEL_TEXT_OUTLINE_WIDTH = 1.5 logical px`, `LABEL_TEXT_OUTLINE_COLOR
     = #000000`, `LABEL_TEXT_OUTLINE_OPACITY = 100%`. The black outline produces a dark
     surround around each glyph, achieving ≥ 21:1 contrast against pure-white voxels
     (`(1.05) / (0.05) = 21`) — covering the bright background case the fill cannot.
   - **Drop shadow (supplemental, not load-bearing for WCAG):** `1 logical px` offset at
     45°, `2 logical px` blur radius, 70% opacity black. UI team may increase but must
     not reduce below this minimum. No animation. *Both blur and offset are specified
     in logical pixels — on a 3× Retina device, 2 native px would be a sub-pixel blur.*
   Gated by `FIRST_RUN_PROMPT_ENABLED` (per-install flag — see R-1 Persistence note).
   Implementation/font selection is delegated to UI team; the fill color, outline, and
   drop shadow floors above are binding.
   **Viewport bounds gate:** The pixel y-coordinate for 78% screen height must be
   resolved after viewport size is available. IS must not create the prompt widget
   until the DrainTick viewport state machine has reached the valid state
   (`bViewportChecked == true`); the same one-shot flag that gates portrait assertion
   also gates prompt sizing — single source of truth, no parallel gates. Assert prompt
   y-coordinate > 0 at creation. Font and weight are UI team responsibility (see UI
   Requirements below for minimum contrast spec).

2. **NONE-tier R-3 collision desaturation pulse:** IS fires a brief gray zone-edge
   flash on same-frame collision at NONE tier; suppressed during DEAD, RESOLVING,
   ABORTED, and COUNTDOWN states via `IRunStateProvider`. This is the minimum Pillar 5 signal on NONE tier
   — without it, simultaneous taps disappear silently with no indication. Spec:
   `design/ux/input-feedback.md` NONE-tier section.

3. **AUDIO/VISUAL tier fallback indicators:** Four audio cues (AUDIO tier) and four
   visual patterns (VISUAL tier) for all haptic events. Full specs in
   `design/ux/input-feedback.md`. Require audio-director + UX designer sign-off
   before IS implementation sprint.

**WCAG 2.3.1 photosensitivity note:** The NONE-tier R-3 desaturation pulse and the
VISUAL-tier `InputRejectedMicroFlash` are both brief (80–150ms) and low-opacity (10–45%).
WCAG 2.3.1 prohibits content flashing more than 3 times per second over more than 25%
of screen area. IS fires at natural cadence (at most once per drain tick per event type)
— IS does not implement rate-limiting. **WCAG 2.3.1 rate-limiting is owned entirely by
`FWidgetVisualDispatch`**, not IS. UI team must verify at implementation that neither
event exceeds 3 flashes/sec at any expected usage frequency. See
`docs/architecture/visual-dispatch-contract.md` §WCAG 2.3.1 Compliance Note for analysis.

No zone boundary lines, highlights, or indicators are displayed during an active run
beyond the above.

## UI Requirements

- First-run prompt labels ("Tap left." / "No slip" / "Tap right.") are
  positioned at the lower-middle of each zone at ~78% screen height from top. "Tap left."
  and "Tap right." anchor to the left and right tap zones respectively; "No slip"
  anchors to the center of the 8mm exclusion band. This keeps all three labels
  within the thumb-reach arc (visible without repositioning the hand) while avoiding the
  thumb-rest area (bottom ~20%) where occlusion risk is highest. Labels must not obscure
  gameplay content at this position — confirm with UI team during implementation.
- All three labels are visible only during the COUNTDOWN run state and dismiss together at
  the COUNTDOWN → RUNNING transition (Pillar 4: no text chrome during motion).
  Independent per-label dismissal is not used.
- **Contrast and font requirement (WCAG 2.1 AA):** Label text must meet a minimum
  contrast ratio of 4.5:1 against the game background at the label position. Because
  there is no background panel and voxel backgrounds shift across the full luminance
  range, no single static fill color satisfies WCAG 1.4.3 — and a drop shadow alone
  does not constitute a WCAG contrast mechanism (the standard measures contrast at the
  text body, not at glyph edges). The label spec therefore mandates a **fill +
  outline** combination: `LABEL_TEXT_FILL_COLOR = #FFFFFF` plus
  `LABEL_TEXT_OUTLINE_WIDTH = 1.5 logical px` `#000000` 100% opacity, with a
  supplemental drop shadow at the floor specified in Visual Elements item 1. Font
  family, weight, and size are delegated to the UI team; the fill color, outline, and
  drop shadow floors are binding. IS specifies position, copy, fill, outline, and
  drop shadow floors only. UI team verifies contrast compliance on all supported
  device brightness settings.
- The dead-band area has no persistent zone marker. A brief visual flash fires on
  dead-band tap on NONE-tier when `DEAD_BAND_FEEDBACK_ENABLED` is true — see NONE-tier
  capability table exception (3) and `design/ux/input-feedback.md` NONE-tier dead-band
  section for spec.
- No UI elements from the Input System are rendered during RUNNING state beyond the
  NONE-tier visual events described above (Pillar 4).

## Acceptance Criteria

### Debug Event Log Schema

All `[DEBUG BUILD]` acceptance criteria observe the IS debug event log. This log
must be implemented before any `[DEBUG BUILD]` AC can be tested.

| Event | Required Fields | When Fired |
|---|---|---|
| `IS_TOUCH_RECEIVED` | `contact_id`, `OS_FingerIndex`, `x_native_px`, `radius_mm`, `is_direct`, `timestamp_ms` | OS touch-down received |
| `IS_PALM_REJECTED` | `OS_FingerIndex`, `radius_mm`, `R_max_mm` | Contact rejected by radius filter (r_mm > R_max) — `contact_id` not yet assigned at this point; use `OS_FingerIndex` to correlate |
| `IS_STYLUS_REJECTED` | `OS_FingerIndex`, `tool_type` (iOS: UITouchType / Android: TOOL_TYPE_*) | Contact rejected as stylus/indirect before radius filter — silent to player, logged for observability; `contact_id` not yet assigned |
| `IS_RADIUS_NAN_GUARD` | `OS_FingerIndex` | `ITouchRadiusProvider` returned NaN for radius_mm — contract violation; treated as −1.0f (pass, no data); `contact_id` not yet assigned at this point |
| `IS_COUNTDOWN_RESET` | `tick_index` | COUNTDOWN→RUNNING tracking reset fired (IS-21-A1); tracking_set and fingerindex_to_contactid_map cleared |
| `IS_FRP_DISMISSED` | `contact_id` | First-run prompt dismissed — `IBootFlagStore::SetFlag(FIRST_RUN_PROMPT_ENABLED, false)` called; fires only on first slip in RUNNING state |
| `IS_OUT_OF_BOUNDS_COORDINATE` | `contact_id`, `OS_FingerIndex`, `x_raw_native_px`, `x_clamped_native_px` | `x_native_px` was outside [0, W] and was clamped before ZONE() evaluation. `x_raw_native_px` is the original unclamped value; `x_clamped_native_px` is the value passed to ZONE(). Normal device use cannot produce this event — it indicates an upstream platform bug or malformed touch event. Fires before `IS_ZONE_CLASSIFIED` on the same contact. |
| `IS_ZONE_CLASSIFIED` | `contact_id`, `x_native_px`, `zone` (LEFT/RIGHT/DEAD_BAND) | Zone check result |
| `IS_SLIP_DISPATCHED` | `contact_id`, `direction` (left/right), `timestamp_ms` | slip-left or slip-right emitted |
| `IS_CANCEL_DISPATCHED` | `contact_id`, `t_elapsed_ms` | Resting-finger timer expired; contact marked resting (IS-internal) |
| `IS_COLLISION_DETECTED` | `contact_id_a`, `contact_id_b`, `drain_tick_delta` (|index_a − index_b|), `result` (COLLISION/PASS) | F-4 collision check evaluated |
| `IS_CONTACT_RESTING` | `contact_id` | Contact marked resting after cancel timer |
| `IS_DEAD_BAND_DISCARDED` | `contact_id`, `x_native_px` | Touch classified DEAD_BAND; fires always regardless of `DEAD_BAND_FEEDBACK_ENABLED` |
| `IS_HAPTIC_FIRED` | `haptic_type` (r3_collision / dead_band_feedback / contact_resting / input_rejected), `capability_tier` | IS-owned haptic dispatched (synchronous, before platform call). PM haptics appear in PM debug log. |
| `IS_TOUCH_UP` | `contact_id`, `timestamp_ms` | OS touch-up received for this contact |
| `IS_TOUCH_UP_ORPHAN` | `OS_FingerIndex`, `timestamp_ms` | Touch-up received for a contact not in the tracking set (pre-IS-construction, duplicate, or post-collision-discard); silently discarded. `contact_id` is absent — the map lookup returns null at this log site (post-DEFECT-1, the pending_removals drain has already removed the map entry before any touch-up arrives). Correlate by `OS_FingerIndex` only, consistent with IS_PALM_REJECTED, IS_STYLUS_REJECTED, and IS_DUPLICATE_CONTACT_ID_REJECTED. |
| `IS_DUPLICATE_CONTACT_ID_REJECTED` | `OS_FingerIndex` | Touch-down received for an `OS_FingerIndex` already present in `fingerindex_to_contactid_map` (OEM edge case — OS re-delivers touch-down for an active finger index); silently discarded to protect the existing contact's cancel-timer anchor. `contact_id` is not yet assigned at this log site — correlate by `OS_FingerIndex`. |
| `IS_COUNTDOWN_ACK_HAPTIC_FIRED` | `contact_id`, `capability_tier` | COUNTDOWN-state slip acknowledgement haptic fired (reuses `EHapticEvent::DeadBandContact`); fires on non-NONE tiers when a valid slip dispatches during COUNTDOWN. |
| `IS_NONE_TIER_COUNTDOWN_ACK_VISUAL_FIRED` | `contact_id`, `zone` (LEFT/RIGHT) | NONE-tier COUNTDOWN-state slip acknowledgement visual fired. Per re-review 26 adjudication, fires `EVisualEvent::ContactRestingLeft` or `EVisualEvent::ContactRestingRight` zone-targeted to the contact's zone — NOT `DeadBandContactFlash` (avoids spatial lie). Fires when a valid slip dispatches during COUNTDOWN on NONE-tier devices. |
| `IS_NONE_TIER_VISUAL_FIRED` | `contact_id_a`, `contact_id_b`, `run_state` | NONE-tier R-3 zone-edge desaturation pulse dispatched via IVisualDispatch |
| `IS_NONE_TIER_CONTACT_RESTING_VISUAL_FIRED` | `contact_id`, `zone` (LEFT/RIGHT), `run_state` | NONE-tier ContactResting hold-deactivation signal dispatched via IVisualDispatch |
| `IS_NONE_TIER_DEAD_BAND_VISUAL_FIRED` | `contact_id`, `x_native_px`, `run_state` | NONE-tier dead-band contact flash dispatched via IVisualDispatch (only when `DEAD_BAND_FEEDBACK_ENABLED == true`) |
| `IS_VIEWPORT_ASPECT_WRONG` | `W` (viewport width, px), `H` (viewport height, px) | Viewport state machine reached the misconfigured-aspect state — `W >= H` (landscape on portrait-only game). Fires from the DrainTick viewport state machine before any event processing. After this event, IS unregisters its FTSTicker and halts — no further DrainTick calls will occur. |
| `IS_CLOCK_NON_FINITE` | `raw_now_ms` (double, the offending value) | `IMonotonicClock::NowMs()` returned a non-finite double (NaN or ±Inf) — contract violation. Step 2 cancel-timer loop is skipped for this tick to prevent `+Inf` from immediately resting-transitioning every active contact. Indicates an `IMonotonicClock` implementation bug; contract must be tightened. |

All timestamps use `IMonotonicClock::NowMs()` (milliseconds, suspension-safe). Log
is accessible via console output in debug builds; readable as a queryable in-game
overlay or log file.

**OS coalescing — monitoring gap:** OS touch-event coalescing (iOS: `coalescedTouchesForTouch`,
Android: `getHistoricalSize()`) can batch multiple touch events before IS receives them.
There is no reliable way to detect coalescing from within `IInputProcessor` — the
`FPendingTouchEvent` struct has no enqueue timestamp, so the coalescing window cannot
be evaluated. This is an accepted monitoring gap: AC-15b verifies the end-to-end
dispatch latency contract (delta ≤ 1 tick) holds on real hardware, which subsumes any
coalescing effect. If AC-15b fails on a specific device, investigate OS coalescing as
a contributing factor.

---

### Property Tests — Formula-Level (Formula Ground-Truth, No DrainTick)

These tests verify each formula function in isolation. They do not test DrainTick
orchestration. An implementation where all four pass but a DrainTick AC fails has
a pseudocode drift site, not a formula bug.

**AC-PROP-F1 — Zone Classification (F-1) boundary values [UNIT TEST]:**
Call `ZONE(x, W, B)` directly (W=1080, B=145; left edge=467.5, right edge=612.5):

- x=467.5 (exact left band edge) → LEFT. Fail: DEAD_BAND or RIGHT (band edge is inclusive to zone side, not to band).
- x=612.5 (exact right band edge) → RIGHT. Fail: DEAD_BAND or LEFT.
- x=540.0 (midpoint — fully inside band) → DEAD_BAND. Fail: LEFT or RIGHT.
- x=−1.0 (below zero — out-of-bounds) → clamped to 0.0 before ZONE(); result=LEFT;
  `IS_OUT_OF_BOUNDS_COORDINATE` logged. Fail: ZONE called with −1.0 unclamped (may produce
  LEFT silently without log); or call aborted without producing a zone result.
- x=1081.0 (above W — out-of-bounds) → clamped to W=1080.0 before ZONE(); result=RIGHT;
  `IS_OUT_OF_BOUNDS_COORDINATE` logged. Fail: ZONE called with 1081.0 unclamped.

*Five automated unit tests — no DrainTick involvement.*

**AC-PROP-F2 — Palm Rejection (F-2) boundary values [UNIT TEST]:**
Call `PALM_PASS(r_mm, R_max)` directly (R_max=6.0mm):

- r_mm=6.0 (exactly at threshold) → true. Fail: false (only r_mm > R_max is rejected; strict greater-than required).
- r_mm=6.001 (just above threshold) → false. Fail: true (under-rejects palms at the boundary).
- r_mm=−1.0 (sentinel: cache miss or bridge race) → true (never reject on absence of data). Fail: false.
- r_mm=NaN → treated as −1.0f sentinel; result=true; debug warning logged. Fail: crash,
  unhandled NaN comparison, or false (NaN > R_max evaluates false in IEEE 754, which would
  silently pass — but the NaN must be explicitly detected and logged as an `ITouchRadiusProvider`
  implementation bug).

*Four automated unit tests — no DrainTick involvement.*

**AC-PROP-F3 — Cancel Timer / safe_elapsed_ms (F-3) boundary values [UNIT TEST]:**
Call `safe_elapsed_ms(now_ms, start_ms)` and `CANCEL(t_elapsed_ms, t_threshold_ms, false)` directly:

- now=start+180.0ms (exactly at threshold) → safe_elapsed_ms=180.0; CANCEL returns false
  (strict > threshold required — at-threshold does not fire). Fail: CANCEL returns true
  (≥ instead of > implemented).
- now=start+180.001ms (1µs above threshold) → safe_elapsed_ms=180.001; CANCEL returns true.
  Fail: false.
- now=start−1.0ms (clock stall — now precedes start) → safe_elapsed_ms returns 0.0 (not a
  large positive or wrap value); CANCEL returns false. Fail: return value is huge positive
  (unsigned wrap if uint64 subtraction used), or negative (signed underflow), or crash.
- now=DBL_MAX, start=0.0 → safe_elapsed_ms=DBL_MAX; CANCEL returns true (elapsed massively
  exceeds threshold). Fail: overflow, NaN, or false.

*Four automated unit tests — no DrainTick involvement. These four cases catch the three most
common F-3 implementation bugs: ≥ instead of >, omitted clock-stall guard, and wrong time units.*

**AC-PROP-F4 — Collision Detection / safe_delta (F-4) boundary values [UNIT TEST]:**
Call `COLLISION(idx_a, z_a, idx_b, z_b)` and `safe_delta(a, b)` directly:

- idx_a=5, z_a=LEFT, idx_b=5, z_b=RIGHT (same tick, different zones) → COLLISION true (delta=0 ≤ 1, zones differ). Fail: false.
- idx_a=5, z_a=LEFT, idx_b=6, z_b=RIGHT (adjacent ticks, different zones) → COLLISION true (delta=1 ≤ 1). Fail: false.
- idx_a=5, z_a=LEFT, idx_b=7, z_b=RIGHT (two ticks apart, different zones) → COLLISION false (delta=2 > 1). Fail: true (collision window too wide).
- idx_a=5, z_a=LEFT, idx_b=5, z_b=LEFT (same tick, same zone) → COLLISION false (zone constraint). Fail: true.
- safe_delta(a=0, b=UINT64_MAX) → returns UINT64_MAX (no unsigned wrap producing a small value).
  COLLISION with these indices and different zones → false (delta massively > 1). Fail:
  safe_delta returns 1 or 0 (unsigned wrap: `0 − UINT64_MAX` wraps to 1 in unsigned arithmetic,
  which is the exact bug safe_delta guards against).

*Five automated unit tests — no DrainTick involvement. The fifth case is the canonical uint64
wrap test; without it, the `std::abs(int64)` or bare-subtraction alternative silently passes
all other cases but produces wrong results late in a session.*

---

**AC-01 — Left-zone slip dispatched [DEBUG BUILD]:** Tap left of the exclusion band →
IS dispatches `slip-left`. Pass: `IS_SLIP_DISPATCHED` shows `direction=left` in the
drain tick following `IS_TOUCH_RECEIVED` (delta = 1 tick — one-tick hold for collision
evaluation). Fail: no entry, wrong direction, or dispatch in the same tick as receipt
(delta = 0, one-tick hold not implemented).

**AC-02 — Right-zone slip dispatched [DEBUG BUILD]:** Tap right of the exclusion band →
IS dispatches `slip-right`. Pass: `IS_SLIP_DISPATCHED` shows `direction=right` in the
drain tick following receipt (delta = 1). Fail: no entry, wrong direction, or same-tick
dispatch.

**AC-03 — Dead-band tap discarded [DEBUG BUILD]:** Tap within the 8mm exclusion band →
no slip fires. Pass: `IS_ZONE_CLASSIFIED` shows `zone=DEAD_BAND`; `IS_DEAD_BAND_DISCARDED`
fires; no `IS_SLIP_DISPATCHED`; no `IS_PALM_REJECTED` (test contact radius must be <
R_max). Fail: `IS_SLIP_DISPATCHED` fires, or `IS_PALM_REJECTED` fires (test radius was
too large).

**AC-03b — DEAD_BAND contact starts no cancel timer [DEBUG BUILD]:** Tap within the 8mm
exclusion band, then advance `FFakeMonotonicClock` by 200ms and call `DrainTick()`. Pass:
`IS_ZONE_CLASSIFIED` shows `zone=DEAD_BAND`; `IS_DEAD_BAND_DISCARDED` fires; no
`IS_CANCEL_DISPATCHED` fires at 200ms (DEAD_BAND contacts are never added to the active
tracking set and are never evaluated by F-3). Fail: `IS_CANCEL_DISPATCHED` fires for a
dead-band contact.
*Automated unit test.*

**AC-03c — DEAD_BAND contact cannot suppress valid slip via F-4 [DEBUG BUILD]:** Inject a
DEAD_BAND contact and a valid LEFT-zone contact in the same `DrainTick()` call. Pass:
`IS_DEAD_BAND_DISCARDED` fires for the dead-band contact; `IS_SLIP_DISPATCHED` fires for
the LEFT-zone contact in the following drain tick; `IS_COLLISION_DETECTED` does NOT fire
(DEAD_BAND contact is never in the F-4 candidate set). Fail: the DEAD_BAND contact
prevents the valid LEFT-zone slip from dispatching (would indicate DEAD_BAND contacts
incorrectly enter the active tracking set).
*Automated unit test using `SimulateSameTickTouches()` with one contact in the dead band.*

**AC-04 — Band-edge zone classification [DEBUG BUILD]:** Touch at exactly x = W/2 − B/2
→ classified LEFT. Touch at exactly x = W/2 + B/2 → classified RIGHT. Neither classifies
as DEAD_BAND. Pass: debug log shows `zone=LEFT` / `zone=RIGHT` at both boundaries.
Fail: either boundary logs DEAD_BAND.
*Requires programmatic touch injection — use `SimulateTouch(x_native_px, radius_mm)`.
File as automated unit test under `tests/unit/input-system/`.*

**AC-05 — Palm rejection threshold (pass) [DEBUG BUILD]:** Touch with contact radius =
5.9mm (just below `PALM_REJECTION_R_MAX_MM = 6.0mm`) → PALM_PASS = true; event proceeds
to zone check. Pass: `IS_ZONE_CLASSIFIED` entry in log (no `IS_PALM_REJECTED`). Fail:
`IS_PALM_REJECTED` fires at r = 5.9mm.
*Use `FTouchRadiusProviderStub.SetRadius(5.9f)` — stub provided by ADR-0001. Automated
unit test.*

**AC-06 — Palm rejection threshold (fail) [DEBUG BUILD]:** Touch with contact radius =
6.1mm (just above R_max) → PALM_PASS = false; event discarded before zone check. Pass:
`IS_PALM_REJECTED` entry in log; no `IS_SLIP_DISPATCHED`. Fail: slip fires.
*Use `FTouchRadiusProviderStub.SetRadius(6.1f)`. Automated unit test.*

**AC-07 — Cancel timer fires [DEBUG BUILD]:** Touch held > 180ms without lift → contact
marked resting. Pass: `IS_CANCEL_DISPATCHED` appears with `t_elapsed_ms > 180ms`;
`IS_CONTACT_RESTING` fires for the same `contact_id`; no further `IS_SLIP_DISPATCHED`
fires for that contact_id while held. Fail: no `IS_CANCEL_DISPATCHED`, or
`IS_SLIP_DISPATCHED` fires for the resting contact while still held.
*Use `FFakeMonotonicClock.AdvanceMs(181)` then `DrainTick()` — see
`docs/architecture/platform-seam-interfaces.md` AC-07 example.*

**AC-08 — Clean lift suppresses cancel [DEBUG BUILD]:** Touch held ≤ 180ms then lifted →
no resting transition. Pass: `IS_TOUCH_UP` fires within the 180ms window; no
`IS_CANCEL_DISPATCHED`; no `IS_CONTACT_RESTING`. Fail: `IS_CANCEL_DISPATCHED` fires
despite clean lift.
*Use `FFakeMonotonicClock.AdvanceMs(179)`, then `SimulateTouchUp(contact_id)`, then
`DrainTick()` — lift occurs at 179ms, before the 180ms threshold. Automated unit test.*

**AC-09 — Post-resting re-tap valid [DEBUG BUILD]:** After a resting contact lifts and
a new contact touches a valid zone → IS dispatches slip event. Pass: `IS_CONTACT_RESTING`
fires for contact_id A; `IS_TOUCH_UP` fires for contact_id A; a new `IS_TOUCH_RECEIVED`
for contact_id B (distinct) in a valid zone fires `IS_SLIP_DISPATCHED` within the same
input tick. Fail: `IS_SLIP_DISPATCHED` does not fire for the re-tap.
*Use `FFakeMonotonicClock.AdvanceMs(181)`, `DrainTick()`, `SimulateTouch(touch_up)`,
`SimulateTouch(new_left_tap)` — fully injectable; file as automated unit test under
`tests/unit/input-system/`.*

**AC-10 — Same-drain-tick opposite-zone collision [DEBUG BUILD]:** Two touch-downs on
opposite halves within the same `DrainTick()` call → both discarded; no slip fires.
Pass: `IS_COLLISION_DETECTED` shows `result=COLLISION`, `drain_tick_delta=0`; no
`IS_SLIP_DISPATCHED`. Fail: one or both slips fire.
*Use `SimulateSameTickTouches()` for deterministic same-tick injection. See
`docs/architecture/platform-seam-interfaces.md` §SimulateTouch for the seam spec.*

**AC-11 — Same-drain-tick same-zone no collision [DEBUG BUILD]:** Two touch-downs on the
same half within the same `DrainTick()` call → both proceed independently. Pass:
`IS_COLLISION_DETECTED` shows `result=PASS`; two `IS_SLIP_DISPATCHED` entries. Fail:
either event discarded.

**AC-12 — Independent dispatch per contact (R-4) [DEBUG BUILD]:** Three sequential valid
taps in the left zone, each in a separate pair of `DrainTick()` calls (inject touch,
call `DrainTick()` — receipt tick; call `DrainTick()` again — dispatch tick; repeat).
Each tap has a distinct `contact_id`. Pass: `IS_SLIP_DISPATCHED` shows three
`direction=left` entries with distinct `contact_id` values; each dispatch appears in
drain_tick_index N+1 relative to its `IS_TOUCH_RECEIVED` in tick N (delta = 1).
Fail: fewer than three entries, any dispatch not in the tick following receipt, or any
sharing a `contact_id`.
*File as automated unit test under `tests/unit/input-system/`.*

**AC-13 — No run-state gating (R-5) [DEBUG BUILD]:** Valid taps during any run state
dispatch slip events. Pass: `IS_SLIP_DISPATCHED` fires in every state below. Fail:
events suppressed in any state.

Required states to verify (six `ERunState` values — ABORTED excluded, see note):
- `IDLE` — before run begins
- `COUNTDOWN` — pre-run countdown active
- `RUNNING` — active gameplay
- `COMPLETE` — run ended normally
- `RESOLVING` — post-run resolution phase
- `DEAD` — player killed

*Use `FRunStateProviderStub.SetState(ERunState::<value>)` for each — see
`docs/architecture/platform-seam-interfaces.md` for stub implementation and AC-13 example.
File as six automated unit tests (one per state) under `tests/unit/input-system/`.*

**AC-DUPLICATE-ID — Duplicate OS_FingerIndex touch-down rejected [DEBUG BUILD]:**
Step 1: Inject finger A (valid left zone, OS_FingerIndex = 0). Step 2: Call `DrainTick()`
— A received and tracked (`fingerindex_to_contactid_map[0] = contact_id_A`). Step 3: Without
injecting a touch-up for A, inject a second touch-down for OS_FingerIndex = 0 (OEM edge case
— OS re-delivers touch-down for an active FingerIndex). Step 4: Call `DrainTick()`.
Pass: `IS_DUPLICATE_CONTACT_ID_REJECTED` fires; A's original entry in `tracking_set` is
unmodified (cancel timer anchor `t_start_ms` unchanged, zone unchanged);
`fingerindex_to_contactid_map` still contains only one entry for OS_FingerIndex = 0.
Fail: `IS_DUPLICATE_CONTACT_ID_REJECTED` does not fire, or A's tracking state is overwritten
by the duplicate touch-down (would corrupt the cancel timer anchor, causing incorrect resting
transition timing).
*Automated unit test using `SimulateTouch()` with the same OS_FingerIndex twice without an
intervening touch-up.*

**AC-13-ABORTED — ABORTED state flush discards in-flight contacts [DEBUG BUILD]:**
ABORTED is intentionally excluded from the no-suppression check above. The ABORTED
level-detection flush (see DrainTick pseudocode) clears `tracking_set` at the end of
every DrainTick where `current_state == ABORTED`. A contact added in Step 1 of an ABORTED
tick is flushed before it reaches the dispatch predicate on the next tick. This is correct
behavior: ABORTED = background-suspend abort; OS touch state is stale and must not dispatch.

Test: Step 1: Set stub to `ERunState::RUNNING`. Step 2: Inject a valid left-zone touch
(OS_FingerIndex=0) and call `DrainTick()` twice — contact dispatches normally
(`IS_SLIP_DISPATCHED`). Step 3: Call `DrainTick()` with stub switched to
`ERunState::ABORTED` — flush runs, `tracking_set` cleared, `fingerindex_to_contactid_map`
cleared. Step 4: Inject a new valid left-zone touch **using the same OS_FingerIndex=0**
and call `DrainTick()` (ABORTED, receipt tick) — `IS_TOUCH_RECEIVED` fires, contact added
to tracking_set in Step 1, flush runs at end of tick. Step 5: Call `DrainTick()` again
(ABORTED) — tracking_set empty, no dispatch.
Pass: `IS_TOUCH_RECEIVED` fires in Step 4; `IS_DUPLICATE_CONTACT_ID_REJECTED` does NOT fire
in Step 4 (confirms `fingerindex_to_contactid_map` was cleared alongside `tracking_set`);
`IS_SLIP_DISPATCHED` does NOT fire in Steps 4 or 5 (contact cleared by flush before dispatch
opportunity). Fail: slip dispatches in ABORTED state; or `IS_DUPLICATE_CONTACT_ID_REJECTED`
fires in Step 4 (`fingerindex_to_contactid_map` not cleared — first post-resume tap silently
blocked on reused finger index).
*Automated unit test.*

**AC-14 — Resting-flag transition excluded from R-3 collision logic [DEBUG BUILD]:** If
the cancel timer fires on a contact while a valid slip arrives from the opposite zone
within T_frame, the resting-flag transition must not count as a collision partner — the
valid slip must proceed. Pass: `IS_CANCEL_DISPATCHED` fires for the held contact;
`IS_COLLISION_DETECTED` shows no pairing between the cancel event and the valid slip;
`IS_SLIP_DISPATCHED` fires for the valid slip. Fail: `IS_COLLISION_DETECTED` implicates
the cancel-timer event, suppressing the valid slip.

**AC-15a — Input System dispatch latency: deterministic unit test [DEBUG BUILD]:**
Using `SimulateTouch()` and `FFakeMonotonicClock` in a drain-tick harness, inject a
valid left-zone touch and call `DrainTick()` once (receipt tick), then call
`DrainTick()` a second time (dispatch tick — hold resolves with no collision).
Verify that `IS_SLIP_DISPATCHED` appears in drain tick N+1, where `IS_TOUCH_RECEIVED`
appeared in drain tick N (delta = 1 tick — one-tick hold for adjacent-frame collision
detection). Pass: `IS_SLIP_DISPATCHED` fires in the drain tick immediately following
receipt. Fail: dispatch fires in the same tick as receipt (delta = 0 — hold not
implemented), or deferred beyond N+1 (delta > 1). File as automated unit test under
`tests/unit/input-system/`.

**AC-15b — Input System dispatch latency: device performance characterization [ON-DEVICE]:**
Measure elapsed drain ticks between `IS_TOUCH_RECEIVED` and `IS_SLIP_DISPATCHED` for
the same contact on real hardware. **Hard contract: 100% of measured dispatches must
occur in the drain tick immediately following their touch receipt (drain_tick_index
delta == 1 exactly). Delta = 0 (dispatch in the same tick as receipt) indicates the
1-tick hold buffer is not implemented — a design-breaking failure, not a latency pass.
Delta > 1 indicates the dispatch was deferred beyond the adjacent tick.**

Pass: zero dispatch failures (delta > 1 drain tick) across N ≥ 300 taps per device in
≥ 3 sessions, on all four devices in the required matrix below. Fail: any slip
dispatch deferred by more than 1 drain tick (delta > 1).

**Required device matrix (all four must pass):**
- iOS min-spec device (oldest supported iOS hardware)
- iOS mid-tier device
- Android min-spec device (oldest supported Android hardware)
- Android mid-tier device

**Tap injection mechanism:** Use `SimulateTouch()` as defined in
`docs/architecture/platform-seam-interfaces.md` to inject synthetic touch events into the IS
OS queue for device-side validation. The `SimulateTouch()` seam must be compiled into the
device test build; it is NOT available in shipping builds. Do not rely on physical finger
taps for the 300-tap sample — `SimulateTouch()` provides deterministic timing and repeatability
across sessions.

*OS coalescing note:* A dispatch delayed by exactly 1 tick due to OS touch-event
coalescing (~16ms) is not a Pillar 5 violation — single-tick coalescing is below the
perceptual threshold for SLIPSTORM's 600ms+ telegraph window. See "OS coalescing —
monitoring gap" in the Debug Event Log Schema section for the monitoring strategy.

**AC-16a — Adjacent drain-tick collision arithmetic [DEBUG BUILD]:** Using
`SimulateTouch()` and the drain-tick harness, inject a LEFT touch in `DrainTick()` N
and a RIGHT touch in `DrainTick()` N+1. Verify `IS_COLLISION_DETECTED` records
`drain_tick_delta = 1` and `result = COLLISION`. Pass: both events discarded;
`IS_SLIP_DISPATCHED` does not fire for either. Fail: events proceed independently.
*File as automated unit test under `tests/unit/input-system/`.*

**AC-16b — Drain-tick quantization: two events between drain calls land in the same tick [DEBUG BUILD]:**
Using the test harness, call `DrainTick()` once to establish tick N. Then, WITHOUT calling
`DrainTick()` again, inject a LEFT touch and a RIGHT touch in sequence via `SimulateTouch()`
(both enqueued to the OS queue between drain calls). Call `DrainTick()` once — this is tick
N+1; both events are dequeued in the same drain call and receive `drain_tick_index = N+1`
(delta = 0). Pass: `IS_COLLISION_DETECTED` fires with `drain_tick_delta = 0` and
`result = COLLISION`; no `IS_SLIP_DISPATCHED` — confirming the drain queue batches events
arriving between DrainTick() calls into a single tick rather than delivering each injection
as a separate tick. Fail: taps treated as independent (two separate drain ticks — delta ≥ 1
— indicates events were not queued between drain calls as intended, or DrainTick() was called
twice).
*Automated unit test. The 120Hz device requirement is eliminated: this test verifies the
batching contract without requiring physical 8.33ms tap timing. On-device coverage is
subsumed by AC-15b (end-to-end latency characterization).*

**AC-17 — Background/resume cancel resolution [DEBUG BUILD]:** With a finger held
mid-touch, advance `FFakeMonotonicClock` by ≥ 200ms (simulating app suspension), then
call `DrainTick()` — cancel fires and contact is marked resting. Pass:
`IS_CANCEL_DISPATCHED` fires with `t_elapsed_ms > 180ms`; `IS_CONTACT_RESTING` fires
in the same frame. Fail: no `IS_CANCEL_DISPATCHED`, or `t_elapsed_ms ≤ 180ms`.
*On device: background and resume after ≥ 180ms — `IMonotonicClock::NowMs()` continues
counting across suspension (`mach_continuous_time` on iOS / `CLOCK_BOOTTIME` on Android).*

**AC-18 — IS_HAPTIC_FIRED fires exactly once on R-3 collision [DEBUG BUILD]:** Two
opposite-zone touches in the same `DrainTick()` call → `IS_HAPTIC_FIRED` fires exactly
once. Pass: `IS_HAPTIC_FIRED` log entry with `haptic_type=r3_collision` and
`capability_tier=FULL` appears in the same evaluation pass as
`IS_COLLISION_DETECTED (result=COLLISION)`;
`FHapticDispatchStub.WasFired(EHapticEvent::R3Collision) == true`; and
`FHapticDispatchStub.FireCount(EHapticEvent::R3Collision) == 1` (exactly one dispatch,
not double-fired). Fail: no `IS_HAPTIC_FIRED`, stub reports `WasFired() == false`, or
`FireCount != 1`.

**AC-18-DURATION — IS_HAPTIC_FIRED fires on R-3 collision with DURATION tier [DEBUG BUILD]:**
With `FHapticDispatchStub.SetCapability(EHapticCapability::DURATION)`, trigger R-3 collision
(two opposite-zone touches in the same `DrainTick()` call). Pass: `IS_HAPTIC_FIRED` log
entry with `haptic_type=r3_collision` and `capability_tier=DURATION`;
`FHapticDispatchStub.WasFired(EHapticEvent::R3Collision) == true`. Fail: haptic not fired
on DURATION tier, or `capability_tier` logged as FULL.
*Automated unit test. Without this, a DURATION-tier `Fire()` no-op silently passes AC-18.*

**AC-19 — IS_HAPTIC_FIRED suppressed when HAPTIC_CAPABILITY=NONE [DEBUG BUILD]:** With
`FHapticDispatchStub.SetCapability(EHapticCapability::NONE)`, R-3 collision fires → no
haptic platform dispatch. Pass: `IS_COLLISION_DETECTED` fires;
`FHapticDispatchStub.WasFired(EHapticEvent::R3Collision) == false`. Fail: stub reports
`WasFired() == true` on NONE tier.

**AC-19b — NONE-tier R-3 visual fires in RUNNING [DEBUG BUILD]:** With
`FHapticDispatchStub.SetCapability(EHapticCapability::NONE)` and
`FRunStateProviderStub.SetState(ERunState::RUNNING)`, trigger R-3 collision → zone-edge
desaturation pulse fires via `IVisualDispatch`. Pass: `IS_NONE_TIER_VISUAL_FIRED` log
entry appears; `FVisualDispatchStub.WasFired() == true`. Fail: no visual entry, or stub
reports `WasFired() == false`.

**AC-19c — NONE-tier R-3 visual suppressed in DEAD, RESOLVING, ABORTED, and COUNTDOWN [DEBUG BUILD]:**
With `FHapticDispatchStub.SetCapability(EHapticCapability::NONE)`, trigger R-3 collision
with state set to `ERunState::DEAD`, `ERunState::RESOLVING`, `ERunState::ABORTED`, and
`ERunState::COUNTDOWN` separately → no visual fires in any suppressed state. Pass:
`IS_NONE_TIER_VISUAL_FIRED` does NOT appear in the log for all four states;
`FVisualDispatchStub.WasFired() == false` in each case. Fail: visual dispatch fires in
any suppressed state.

**AC-04b — Palm rejection exact threshold [DEBUG BUILD]:** Touch with contact radius =
exactly 6.0mm → PALM_PASS = true; event proceeds to zone check. Pass:
`IS_ZONE_CLASSIFIED` fires; no `IS_PALM_REJECTED`. Fail: contact rejected at exactly
R_max. *Use `FTouchRadiusProviderStub.SetRadius(6.0f)`. Automated unit test.*

**AC-06b — InputRejected haptic fires on palm rejection [DEBUG BUILD]:** Touch with
r_mm = 6.1mm (above R_max) on FULL tier → `IS_PALM_REJECTED` fires and haptic
dispatches. Pass: `IS_HAPTIC_FIRED` entry with `haptic_type=input_rejected`;
`FHapticDispatchStub.WasFired(EHapticEvent::InputRejected) == true`. Fail: no haptic
fired, or wrong event type.
*Use `FTouchRadiusProviderStub.SetRadius(6.1f)`, `FHapticDispatchStub.SetCapability(FULL)`.
Automated unit test.*

**AC-07b — Cancel timer does NOT fire at exactly t=180ms [DEBUG BUILD]:** Advance
`FFakeMonotonicClock` to exactly 180ms from touch-down, then call `DrainTick()`. Pass:
no `IS_CANCEL_DISPATCHED` fires; contact not marked resting. Fail:
`IS_CANCEL_DISPATCHED` fires at exactly 180ms (cancel requires t > 180ms, not t ≥ 180ms).
*Automated unit test.*

**AC-20 — Stylus rejection fires IS_STYLUS_REJECTED [DEBUG BUILD]:** Simulate a contact
with `tool_type = TOOL_TYPE_STYLUS` (Android) or `UITouchTypeStylus` (iOS). Pass:
`IS_STYLUS_REJECTED` fires; no `IS_PALM_REJECTED`; no `IS_SLIP_DISPATCHED`. Fail: stylus
contact proceeds past the stylus-type check, or `IS_PALM_REJECTED` fires instead.
*Automated unit test using `SimulateTouch()` with stylus tool-type flag.*

**AC-20d — TOOL_TYPE_UNKNOWN NOT rejected [DEBUG BUILD]:** Simulate a contact with
`tool_type = TOOL_TYPE_UNKNOWN` (Android OEM compatibility — some devices report this
for legitimate finger contacts). Pass: no `IS_STYLUS_REJECTED` fires; `IS_TOUCH_RECEIVED`
fires; contact proceeds to PALM_PASS check; if radius ≤ R_max and zone valid,
`IS_SLIP_DISPATCHED` fires. Fail: `IS_STYLUS_REJECTED` fires for `TOOL_TYPE_UNKNOWN`
(indicates the is_direct / tool-type check incorrectly rejects unknown types).
*Automated unit test using `SimulateTouch()` with `TOOL_TYPE_UNKNOWN` flag.*

**AC-21 — ContactResting fires in IDLE, RUNNING, COMPLETE regardless of knob [DEBUG BUILD]:**
With `DEAD_BAND_FEEDBACK_ENABLED = false` and `FHapticDispatchStub.SetCapability(FULL)`,
advance clock past 180ms cancel threshold for an active contact with state set to
`ERunState::IDLE`, `ERunState::RUNNING`, and `ERunState::COMPLETE` separately. Call
`DrainTick()` in each case. Pass: `IS_CANCEL_DISPATCHED` fires;
`FHapticDispatchStub.WasFired(EHapticEvent::ContactResting) == true` in all three states
(fires despite knob being off — uses distinct `EHapticEvent::ContactResting`, not
`DeadBandContact`). Fail: haptic not fired in any of the three states, or
`EHapticEvent::DeadBandContact` fired instead.
*Three automated unit tests — one per state. See AC-21d/AC-21e for COUNTDOWN-specific tests.*

**AC-21-NONE-ContactResting — NONE-tier ContactResting fires IVisualDispatch with correct zone in IDLE, RUNNING, COMPLETE [DEBUG BUILD]:**
With `FHapticDispatchStub.SetCapability(EHapticCapability::NONE)` and state set to
`ERunState::IDLE`, `ERunState::RUNNING`, and `ERunState::COMPLETE` separately, advance
clock past 180ms cancel threshold for a **LEFT-zone** active contact and call `DrainTick()`.
Pass: `IS_NONE_TIER_CONTACT_RESTING_VISUAL_FIRED` log entry fires with `zone=LEFT`;
`FVisualDispatchStub.WasFired() == true`;
`FVisualDispatchStub.GetLastEvent() == EVisualEvent::ContactRestingLeft` in all three
states. Repeat with a **RIGHT-zone** contact: `GetLastEvent() ==
EVisualEvent::ContactRestingRight` in all three states. Fail: visual not fired in any
state, wrong event type (`ContactRestingLeft` fired for RIGHT-zone contact or vice versa),
or log zone field does not match contact origin.
*Six automated unit tests — three per zone (one per state). See AC-21d/AC-21e for COUNTDOWN-specific tests.*

**AC-21-NONE-ContactResting-Suppressed — NONE-tier ContactResting visual suppressed in DEAD, RESOLVING, ABORTED [DEBUG BUILD]:**
With `FHapticDispatchStub.SetCapability(EHapticCapability::NONE)` and state set to
`ERunState::DEAD`, `ERunState::RESOLVING`, and `ERunState::ABORTED` separately, advance
clock past 180ms cancel threshold and call `DrainTick()`. Pass: `IS_CANCEL_DISPATCHED`
fires (contact transitions to resting internally); `FVisualDispatchStub.WasFired() ==
false` in all three states (visual suppressed). Fail: visual fires during any suppressed state.
*Three automated unit tests. Note: COUNTDOWN suppression uses a different mechanism —
the cancel-timer guard prevents timer evaluation entirely (Step 2 predicate excludes
`current_state == COUNTDOWN`). The states tested here use the explicit run-state gate
in the ContactResting dispatch path. See AC-21d for the COUNTDOWN timer-guard test.*

**AC-21-NONE-ContactRestingFlash — NONE-tier ContactResting visual fires on cancel timer and does NOT call Clear() [DEBUG BUILD]:**
*(Supersedes AC-21-NONE-ContactRestingClear — re-review 23: ContactResting changed from persistent tint to 50ms transient flash; Clear() is no longer valid for ContactRestingLeft/Right.)*
With `FHapticDispatchStub.SetCapability(EHapticCapability::NONE)`, advance clock past the
180ms cancel threshold for a **LEFT-zone** active contact and call `DrainTick()`.
Pass: `IS_NONE_TIER_CONTACT_RESTING_VISUAL_FIRED` fires; `FVisualDispatchStub.WasFired(EVisualEvent::ContactRestingLeft) == true`.
Then inject touch-up for that contact and call `DrainTick()`.
Pass: `FVisualDispatchStub.WasCleared() == false` — Clear() must NOT be called for a
transient event. Fail: `WasCleared() == true` (would indicate implementation still treats
ContactRestingLeft as persistent, a programming error post-re-review 23).
Repeat with a **RIGHT-zone** contact: `WasFired(EVisualEvent::ContactRestingRight) == true`; `WasCleared() == false`.
*Two automated unit tests — one per zone.*

**AC-FORCE-EXPIRE-CLEAR — NONE-tier ContactResting visual fires and tracking set flushed on ABORTED transition; Clear() NOT called [DEBUG BUILD]:**
*(Updated re-review 23: ContactResting is transient; Clear() no longer required or valid on ABORTED transition. Test now verifies Clear() is NOT called.)*
With `FHapticDispatchStub.SetCapability(EHapticCapability::NONE)` and initial state
`ERunState::RUNNING`: Step 1: Inject finger A in LEFT zone.
Step 2a: Call `DrainTick()` — A received into `tracking_set`.
Step 2b: Call `DrainTick()` again — A's 1-tick hold expires; `IS_SLIP_DISPATCHED` fires
(`A.b_dispatched = true`; A remains held).
Step 2c: Advance `FFakeMonotonicClock` by 181ms. Call `DrainTick()` — A's cancel timer
expires; `IVisualDispatch::Fire(EVisualEvent::ContactRestingLeft)` fires
(`IS_NONE_TIER_CONTACT_RESTING_VISUAL_FIRED` logged). The flash is transient (50ms self-clear).
Step 3: Switch stub to `ERunState::ABORTED`. Step 4: Call `DrainTick()` — ABORTED flush runs.
Pass: `FVisualDispatchStub.GetClearCount(EVisualEvent::ContactRestingLeft) == 0` (Clear() must
NOT fire — ContactRestingLeft is transient; calling Clear() on a transient event is a
programming error); tracking set is empty after the tick (verified by injecting a touch-up
for the same contact ID — logs `IS_TOUCH_UP_ORPHAN`).
Step 5b (fingerindex_to_contactid_map cleared): inject a new touch-DOWN for **the same
OS_FingerIndex=0** used in Step 1, and call `DrainTick()`. Pass: `IS_DUPLICATE_CONTACT_ID_REJECTED`
does NOT fire (confirms `fingerindex_to_contactid_map[0]` was cleared by the ABORTED flush).
Fail: `IS_DUPLICATE_CONTACT_ID_REJECTED` fires in Step 5b (stale map entry remains after
ABORTED flush — first post-resume tap on reused finger index is silently blocked).
Fail: `GetClearCount(ContactRestingLeft) > 0` after ABORTED transition (Clear() incorrectly
called on transient event — programming error).
*Automated unit test.*

**AC-21c — ContactResting suppressed in DEAD, RESOLVING, ABORTED [DEBUG BUILD]:**
With `FHapticDispatchStub.SetCapability(FULL)` and state set to `ERunState::DEAD`,
`ERunState::RESOLVING`, and `ERunState::ABORTED` separately, advance clock past 180ms
cancel threshold and call `DrainTick()`. Pass: `IS_CANCEL_DISPATCHED` fires (contact
transitions to resting internally); `FHapticDispatchStub.WasFired(EHapticEvent::ContactResting)
== false` in all three states (haptic suppressed by run state). Fail: ContactResting
haptic fires during any suppressed state.
*Three automated unit tests — confirms asymmetric suppression (DEAD/RESOLVING/ABORTED
suppressed via run-state gate; COUNTDOWN suppressed via cancel-timer guard — see AC-21d/AC-21e).*

**AC-21d — ContactResting NOT dispatched during COUNTDOWN (timer guard) [DEBUG BUILD]:**
With `FHapticDispatchStub.SetCapability(FULL)` and state set to `ERunState::COUNTDOWN`,
advance `FFakeMonotonicClock` by 181ms and call `DrainTick()`. Pass: `IS_CANCEL_DISPATCHED`
does NOT fire; `FHapticDispatchStub.WasFired(EHapticEvent::ContactResting) == false` (the
Step 2 predicate excludes contacts while `current_state == COUNTDOWN` — timer accumulates
but cannot fire). Fail: `IS_CANCEL_DISPATCHED` fires during COUNTDOWN (indicates cancel-timer
guard is missing or inverted).
*Automated unit test.*

**AC-21e — Contact pre-positioned during COUNTDOWN is cleared at COUNTDOWN→RUNNING; no slip or ContactResting on first RUNNING tick (IS-21-A1) [DEBUG BUILD]:**
With `FHapticDispatchStub.SetCapability(FULL)`: Step 1: set state to `ERunState::COUNTDOWN`;
inject a LEFT-zone contact via `SimulateTouch()` (OS_FingerIndex=0); call `DrainTick()` —
`IS_TOUCH_RECEIVED` fires; contact in tracking_set; 1-tick hold applies (drain_tick_index
equals current_drain_tick_index so Step 3 outer predicate excludes it); `IS_SLIP_DISPATCHED`
does NOT fire. Step 2: switch stub to `ERunState::RUNNING`. Step 3: call `DrainTick()`.
Pass: `IS_COUNTDOWN_RESET` fires at the start of Step 3 tick (before any Step 1 event
processing); tracking_set is empty after Step 3; `IS_SLIP_DISPATCHED` does NOT fire (pre-positioned
contact was cleared before Step 3 outer loop ran); `IS_CANCEL_DISPATCHED` does NOT fire;
`FHapticDispatchStub.WasFired(EHapticEvent::ContactResting) == false`. Confirm: inject
touch-up for OS_FingerIndex=0 and call `DrainTick()` — `IS_TOUCH_UP_ORPHAN` fires (contact
no longer in tracking_set). Fail: `IS_COUNTDOWN_RESET` does not fire; or `IS_SLIP_DISPATCHED`
fires (pre-positioned contact dispatched as slip on first RUNNING tick — IS-21-A1 reset not
implemented); or ContactResting fires; or contact survives in tracking_set.
*Automated unit test. Directly exercises the first-run pre-positioning scenario: a contact
placed during COUNTDOWN must be cleared cleanly at COUNTDOWN→RUNNING with no spurious slip
or haptic on the first RUNNING tick.*

**AC-COLLISION-REUSE — fingerindex_to_contactid_map cleared after collision-discard, new touch on same FingerIndex dispatches [DEBUG BUILD]:**
Step 1: inject two contacts on opposite zones (LEFT: FingerIndex=0, RIGHT: FingerIndex=1)
in the same drain tick. Step 2: call `DrainTick()` — collision detected; both contacts
discarded (`IS_COLLISION_DETECTED` fires with `result=COLLISION`); neither dispatches.
Step 3: inject touch-up events for both FingerIndex=0 and FingerIndex=1. Step 4: call
`DrainTick()` — orphan path handles both touch-ups (contacts removed from tracking_set
during collision; map entries already removed by pending_removals drain; `IS_TOUCH_UP_ORPHAN`
fires for each). Step 5: inject new touch-down events on FingerIndex=0 (LEFT zone) and
FingerIndex=1 (RIGHT zone) as separate drain ticks.
Pass: `IS_SLIP_DISPATCHED` fires for both new contacts (`IS_DUPLICATE_CONTACT_ID_REJECTED`
must NOT fire) — confirms fingerindex_to_contactid_map was cleaned up.
Fail: `IS_DUPLICATE_CONTACT_ID_REJECTED` fires for FingerIndex=0 or FingerIndex=1.
*Automated unit test using `SimulateTouch()` seam.*

**AC-DEFECT1 — fingerindex_to_contactid_map cleaned in pending_removals drain without requiring touch-up [DEBUG BUILD]:**
Step 1: inject two contacts on opposite zones (LEFT: FingerIndex=0, RIGHT: FingerIndex=1)
in the same drain tick. Step 2: call `DrainTick()` — collision detected; both contacts
discarded; `IS_COLLISION_DETECTED result=COLLISION` fires. No touch-up events are injected
for either FingerIndex (simulating an OEM device that omits touch-up for discarded contacts).
Step 3: inject a new touch-down on FingerIndex=0 (LEFT zone) only. Step 4: call `DrainTick()`.
Pass: `IS_SLIP_DISPATCHED` fires for the new contact (direction=left);
`IS_DUPLICATE_CONTACT_ID_REJECTED` does NOT fire — confirms `fingerindex_to_contactid_map`
was cleared during the pending_removals drain in Step 2, without waiting for a touch-up.
Fail: `IS_DUPLICATE_CONTACT_ID_REJECTED` fires (stale map entry was not removed during
the pending_removals drain — DEFECT-1 not fixed).
*Automated unit test using `SimulateTouch()` seam.*

**AC-21-DURATION — ContactResting haptic fires on DURATION tier [DEBUG BUILD]:**
With `DEAD_BAND_FEEDBACK_ENABLED = false` and `FHapticDispatchStub.SetCapability(EHapticCapability::DURATION)`,
advance clock past 180ms cancel threshold for an active contact in `ERunState::RUNNING`
and call `DrainTick()`. Pass: `IS_CANCEL_DISPATCHED` fires;
`FHapticDispatchStub.WasFired(EHapticEvent::ContactResting) == true` on DURATION tier.
Fail: haptic not fired on DURATION tier (would indicate ContactResting incorrectly
no-ops on DURATION, silently violating Pillar 5 for players on DURATION-tier devices).
*Automated unit test. Without this, a DURATION-tier `Fire()` no-op silently passes AC-21.*

**AC-22 — Cancel timer fires on DrainTick() without new OS events [DEBUG BUILD]:**
Inject one left-zone touch, call `DrainTick()` (slip dispatched). Then advance clock by
200ms and call `DrainTick()` again with no new OS events queued. Pass:
`IS_CANCEL_DISPATCHED` and `IS_CONTACT_RESTING` fire on the second `DrainTick()`.
Fail: cancel does not fire unless a new OS event triggers evaluation.
*Automated unit test — confirms cancel evaluation is tick-driven, not event-driven.*

**AC-23 — Three-touch pairwise resolution — 4-contact scenario A [DEBUG BUILD]:**
Four contacts [L:1, R:2, L:3, R:4] in the same drain tick. Pass: pairs (1,2) and (3,4)
both discarded; `IS_COLLISION_DETECTED` fires twice; no `IS_SLIP_DISPATCHED`.
Fail: any slip fires, or fewer than two collision detections logged.
*Automated unit test using `SimulateSameTickTouches()`.*

**AC-24 — Three-touch pairwise resolution — 4-contact scenario B [DEBUG BUILD]:**
Four contacts [L:1, L:2, R:3, L:4] in the same drain tick. Pass: pair (1,3) discarded;
contacts 2 and 4 (both LEFT) proceed independently — two `IS_SLIP_DISPATCHED` entries
with `direction=left`. Fail: any LEFT contact discarded, or RIGHT contact proceeds.
*Automated unit test using `SimulateSameTickTouches()`.*

**AC-TMAP-ORDER — Ascending contact_id iteration enforced; `TSortedMap`/sorted array required [DEBUG BUILD]:**
Inject three contacts in a single drain tick via `SimulateSameTickTouches()`: [LEFT, RIGHT,
LEFT] → assigned contact_ids 0, 1, 2 in order (`NextContactId` starts at 0 per construction
invariant — first session contact is always 0). Call `DrainTick()` (receipt tick; all held
under 1-tick hold). Call `DrainTick()` (dispatch tick).
Pass: contact 0 (LEFT) pairs with contact 1 (RIGHT) — both discarded (`IS_COLLISION_DETECTED
result=COLLISION` for pair (0, 1)); contact 2 (LEFT) dispatches independently (`IS_SLIP_DISPATCHED
direction=left`); exactly one `IS_COLLISION_DETECTED result=COLLISION` entry; exactly one
`IS_SLIP_DISPATCHED` entry.
Fail (incorrect pairing): contact 2 pairs with contact 1 instead of contact 0 (ascending order
violated); `IS_SLIP_DISPATCHED` fires for contact 0 instead of 2, or two collision entries and
no slip.
**Implementation requirement:** Use `TSortedMap<int32, FContactState>` or extract into `TArray`
and `Sort()` before the Step 3 loop. `TMap` does not guarantee ascending key order — this test
will produce a deterministic failure against a `TMap` implementation (first run, every run) once
ordered iteration is required.
*Automated unit test using `SimulateSameTickTouches()`. Run once — the test is deterministic
when `TSortedMap` or sorted `TArray` is used. A `TMap` implementation produces a consistent
incorrect-pairing failure (not a flaky test).*

**AC-25 — Touch-up wins cancel timer race in same drain tick [DEBUG BUILD]:** With state
set to `ERunState::RUNNING`, advance clock to 200ms past touch-down (cancel would fire),
then inject the touch-up event and call `DrainTick()` once. Pass: `IS_TOUCH_UP` fires;
`IS_CANCEL_DISPATCHED` does NOT
fire; contact removed from tracking set. Fail: both `IS_TOUCH_UP` and
`IS_CANCEL_DISPATCHED` fire in the same drain tick, or cancel fires despite touch-up.
*Automated unit test.*

**AC-26a — Correction burst: dispatched-contact exclusion fires at delta=1 [DEBUG BUILD]:**
Step 1: Inject finger A in LEFT zone. Step 2: Call `DrainTick()` — A received (drain tick
1; `A.drain_tick_index = 1`); 1-tick hold applies, A not yet dispatched. Step 3: Inject
finger B in RIGHT zone (still before next DrainTick). Step 4: Call `DrainTick()` — A's
hold expires; `IS_SLIP_DISPATCHED` fires for A (`direction=left`, `A.b_dispatched = true`);
B received (drain tick 2; `B.drain_tick_index = 2`); delta = |2 − 1| = 1 ≤ 1; F-4 check:
A has `b_dispatched == true` → A excluded from F-4; B enqueued under 1-tick hold.
Step 5: Call `DrainTick()` — B's hold expires; `IS_SLIP_DISPATCHED` fires for B
(`direction=right`). Pass: both `IS_SLIP_DISPATCHED` events fire independently with no
`IS_COLLISION_DETECTED result=COLLISION` for the (A, B) pair; `IS_NONE_TIER_CONTACT_RESTING_VISUAL_FIRED`
does not fire prematurely for A (A's cancel timer is running from A's touch-down, not reset).
Fail: `IS_COLLISION_DETECTED` shows `result=COLLISION` suppressing B; or B fails to dispatch.
*Automated unit test. This test exercises the dispatched-contact exclusion specifically
(delta=1, which would collide without the exclusion rule).*

**AC-26b — Correction burst: delta=2 passes without dispatched-contact exclusion [DEBUG BUILD]:**
Step 1: Inject finger A in LEFT zone. Step 2: Call `DrainTick()` — A received (drain tick
1). Step 3: Call `DrainTick()` — A dispatches (`IS_SLIP_DISPATCHED`, `direction=left`,
`A.b_dispatched = true`). Step 4: Inject finger B in RIGHT zone. Step 5: Call `DrainTick()`
— B received (drain tick 3; `B.drain_tick_index = 3`); delta = |3 − 1| = 2 > 1; F-4
distance check: delta > 1, no collision regardless of dispatched flag; B enqueued. Step 6:
Call `DrainTick()` — B dispatches. Pass: `IS_SLIP_DISPATCHED` fires for both A and B
independently; no `IS_COLLISION_DETECTED result=COLLISION`. Fail: collision detected (would
indicate the distance check is broken).
*Automated unit test. Confirms delta=2 is already handled by the standard distance check,
independent of the new exclusion rule.*

**AC-26c — Cancel timer anchors to contact A's own touch-down time, not reset by B's arrival or dispatch [DEBUG BUILD]:**
Step 1: Inject finger A in LEFT zone. Step 2: Call `DrainTick()` (clock = 0ms) — A received
(drain tick 1); 1-tick hold; A not dispatched. Step 3: Call `DrainTick()` (clock = 50ms) —
A's hold expires; `IS_SLIP_DISPATCHED` fires for A (`direction=left`, `A.b_dispatched = true`).
A remains tracked (still held). Step 4: Inject finger B in RIGHT zone. Step 5: Call
`DrainTick()` (clock = 90ms) — B received (drain tick 3; `B.drain_tick_index = 3`); delta
= |3 − 1| = 2 > 1; no F-4 collision; B enqueued under 1-tick hold. Step 6: Call `DrainTick()`
(clock = 120ms) — B's hold expires; `IS_SLIP_DISPATCHED` fires for B (`direction=right`).
Step 7: Call `DrainTick()` (clock = 181ms from A's touch-down, i.e. `FFakeMonotonicClock` set
to 181ms) — F-3 evaluates A (still held, resting flag not set); `t_elapsed = 181ms > 180ms`
→ A enters resting; `IS_CANCEL_DISPATCHED` and `IS_CONTACT_RESTING` fire.
Pass: `IS_CONTACT_RESTING` fires at clock = 181ms (relative to A's touch-down); A's cancel
timer was not reset by B's arrival at 90ms (which would incorrectly delay the fire to
~270ms). Fail: `IS_CONTACT_RESTING` does not fire at clock = 181ms, or fires only after
clock = 270ms (indicating timer was reset to B's arrival time or B's dispatch time).
*Automated unit test using `FFakeMonotonicClock`. Verifies the cancel timer anchors to the
contact's own `touch_down_time_ms`, recorded once at touch-down and never updated.*

**AC-HOLD-01 — Held dispatched contact does not re-dispatch [DEBUG BUILD]:**
Step 1: Inject finger A in LEFT zone. Step 2: Call `DrainTick()` (tick 1) — A assigned
index 1; 1-tick hold; not dispatched. Step 3: Call `DrainTick()` (tick 2) — A's hold
expires; `IS_SLIP_DISPATCHED` fires (`direction=left`); `A.b_dispatched = true`. Step 4:
Call `DrainTick()` (tick 3) — A still held, no new events. Step 5: Call `DrainTick()` (tick 4).
Pass: `IS_SLIP_DISPATCHED` fires exactly once across ticks 2–4.
Fail: `IS_SLIP_DISPATCHED` fires again in tick 3 or tick 4 (re-dispatch due to missing
`b_dispatched == false` guard in step 3 predicate).
*Automated unit test. Verifies the step 3 predicate `b_dispatched == false` blocks
already-dispatched contacts from re-evaluating for dispatch on subsequent ticks.*

**AC-14b — F-3/F-4 ordering invariant [DEBUG BUILD]:** In a `DrainTick()` where the
cancel timer fires on a contact AND a valid opposite-zone touch arrives simultaneously,
`IS_CONTACT_RESTING` must appear in the log before `IS_COLLISION_DETECTED` for the same
drain tick. Pass: `IS_CONTACT_RESTING` log sequence index precedes `IS_COLLISION_DETECTED`;
`IS_SLIP_DISPATCHED` fires for the valid opposite-zone touch; the resting contact does not
appear as a collision partner. Fail: `IS_COLLISION_DETECTED` appears before
`IS_CONTACT_RESTING`, or the resting contact implicates as a collision partner.
*Automated unit test.*

**AC-19e — R-3 haptic suppressed on FULL tier in all four suppression states [DEBUG BUILD]:**
With `FHapticDispatchStub.SetCapability(EHapticCapability::FULL)`, trigger R-3 collision
with state set to `ERunState::DEAD`, `ERunState::RESOLVING`, `ERunState::ABORTED`, and
`ERunState::COUNTDOWN` separately. Pass: `IS_COLLISION_DETECTED` fires with
`result=COLLISION` in all four states; `FHapticDispatchStub.WasFired(EHapticEvent::R3Collision)
== false` in all four states; no `IS_HAPTIC_FIRED` entry. Fail: R-3 haptic fires on FULL
tier in any suppressed state.
*Use `FRunStateProviderStub.SetState(ERunState::<value>)` for each. Four automated unit tests.*

**AC-19f — R-3 haptic suppressed on DURATION tier in all four suppression states [DEBUG BUILD]:**
Same as AC-19e with `FHapticDispatchStub.SetCapability(EHapticCapability::DURATION)`.
Pass: `FHapticDispatchStub.WasFired(EHapticEvent::R3Collision) == false` in all four
states (DEAD, RESOLVING, ABORTED, COUNTDOWN). Fail: haptic fires on DURATION tier in
any suppressed state.
*Four automated unit tests.*

**AC-19d — NONE-tier R-3 visual fires in IDLE, RUNNING, and COMPLETE [DEBUG BUILD]:**
With `FHapticDispatchStub.SetCapability(EHapticCapability::NONE)`, trigger R-3 collision
with state set to `ERunState::IDLE`, `ERunState::RUNNING`, and `ERunState::COMPLETE`
separately → zone-edge desaturation pulse fires in all three states. Pass:
`IS_NONE_TIER_VISUAL_FIRED` appears; `FVisualDispatchStub.WasFired() == true` for each
state. Fail: visual does not fire in any of the three states.
*Use `FRunStateProviderStub.SetState(ERunState::<value>)` for each. Three automated unit
tests.*

**AC-21b — DeadBandContact haptic fires on zone-classification dead-band tap (ENABLED) [DEBUG BUILD]:**
With `DEAD_BAND_FEEDBACK_ENABLED = true` and `FHapticDispatchStub.SetCapability(FULL)`,
inject a touch within the 8mm exclusion band, then `DrainTick()`. Pass:
`IS_DEAD_BAND_DISCARDED` fires; `IS_HAPTIC_FIRED` entry with `haptic_type=dead_band_feedback`;
`FHapticDispatchStub.WasFired(EHapticEvent::DeadBandContact) == true`. Fail: no haptic
fired despite knob being on, or `EHapticEvent::ContactResting` fired instead of
`EHapticEvent::DeadBandContact`. *Automated unit test.*

**AC-NONE-TIER-DB-01 — NONE-tier dead-band visual fires on dead-band tap when knob enabled [DEBUG BUILD]:**
With `DEAD_BAND_FEEDBACK_ENABLED = true`, `FHapticDispatchStub.SetCapability(EHapticCapability::NONE)`,
and `ERunState::RUNNING`: inject a touch within the 8mm exclusion band, then `DrainTick()`.
Pass: `IS_DEAD_BAND_DISCARDED` fires; `IS_NONE_TIER_DEAD_BAND_VISUAL_FIRED` fires;
`FVisualDispatchStub.WasFired(EVisualEvent::DeadBandContactFlash) == true`;
`FHapticDispatchStub.WasFired(EHapticEvent::DeadBandContact) == false` (no haptic on NONE tier).
Fail: visual not fired; or haptic fires on NONE tier; or `IS_NONE_TIER_DEAD_BAND_VISUAL_FIRED`
not logged.
*Automated unit test. Run with `ERunState::RUNNING`. Note: COUNTDOWN is also a positive (firing)
state for NONE-tier dead-band visual (see NONE-tier capability table exception 3 — "fires in IDLE,
COUNTDOWN, RUNNING, and COMPLETE"). Repeat this test with `ERunState::COUNTDOWN` to verify the
visual also fires during COUNTDOWN — an implementation that accidentally suppresses COUNTDOWN
would pass the RUNNING fixture but fail the COUNTDOWN fixture. See AC-NONE-TIER-DB-03 for
suppressed states (DEAD, RESOLVING, ABORTED).*

**AC-NONE-TIER-DB-02 — NONE-tier dead-band visual suppressed when knob disabled [DEBUG BUILD]:**
With `DEAD_BAND_FEEDBACK_ENABLED = false`, `FHapticDispatchStub.SetCapability(EHapticCapability::NONE)`,
and `ERunState::RUNNING`: inject a touch within the 8mm exclusion band, then `DrainTick()`.
Pass: `IS_DEAD_BAND_DISCARDED` fires (always logged); `FVisualDispatchStub.WasFired(EVisualEvent::DeadBandContactFlash) == false`;
no `IS_NONE_TIER_DEAD_BAND_VISUAL_FIRED` log entry.
Fail: visual fires despite knob being off.
*Automated unit test.*

**AC-NONE-TIER-DB-03 — NONE-tier dead-band visual suppressed in DEAD, RESOLVING, and ABORTED [DEBUG BUILD]:**
With `DEAD_BAND_FEEDBACK_ENABLED = true`, `FHapticDispatchStub.SetCapability(EHapticCapability::NONE)`,
and state set to `ERunState::DEAD`, `ERunState::RESOLVING`, and `ERunState::ABORTED` separately:
inject a touch within the 8mm exclusion band, then `DrainTick()`. Pass: `IS_DEAD_BAND_DISCARDED`
fires in all three states (always logged regardless of run state); `FVisualDispatchStub.WasFired(EVisualEvent::DeadBandContactFlash)
== false` in all three states; no `IS_NONE_TIER_DEAD_BAND_VISUAL_FIRED` log entry in any
suppressed state. Fail: visual fires in any of the three suppressed states (DEAD, RESOLVING,
ABORTED) — indicates the run-state gate in the DEAD_BAND path of Step 1 does not match the
`NOT IN {DEAD, RESOLVING, ABORTED}` predicate specified in the DrainTick pseudocode.
*Three automated unit tests — one per suppressed state.*

**AC-06c — InputRejected silent on AUDIO and NONE tiers [DEBUG BUILD]:** Touch with
radius = 6.1mm (above R_max). (a) `FHapticDispatchStub.SetCapability(AUDIO)` →
`IS_PALM_REJECTED` fires; `FHapticDispatchStub.WasFired(EHapticEvent::InputRejected) == false`.
(b) `FHapticDispatchStub.SetCapability(NONE)` → same. Pass: no haptic dispatch on either
tier. Fail: `InputRejected` fires on AUDIO or NONE tier.
*Use `FTouchRadiusProviderStub.SetRadius(6.1f)`. Two automated unit tests.*

**AC-06d — InputRejected fires IVisualDispatch on VISUAL tier [DEBUG BUILD]:** Touch
with radius = 6.1mm on VISUAL tier → `IS_PALM_REJECTED` fires and IS dispatches
`IVisualDispatch::Fire(EVisualEvent::InputRejectedMicroFlash)` in place of the haptic.
Pass: `IS_PALM_REJECTED` log entry fires; `FVisualDispatchStub.WasFired() == true`;
`FVisualDispatchStub.GetLastEvent() == EVisualEvent::InputRejectedMicroFlash`;
`FHapticDispatchStub.WasFired(EHapticEvent::InputRejected) == false` (no haptic on
VISUAL tier). Fail: visual not fired, or haptic fired instead.
*Use `FTouchRadiusProviderStub.SetRadius(6.1f)`, `FHapticDispatchStub.SetCapability(VISUAL)`.
Automated unit test.*

**AC-06e — InputRejected fires haptic on DURATION tier [DEBUG BUILD]:** Touch with
radius = 6.1mm on DURATION tier → haptic dispatches (duration-only pattern). Pass:
`IS_HAPTIC_FIRED` entry with `haptic_type=input_rejected` and `capability_tier=DURATION`;
`FHapticDispatchStub.WasFired(EHapticEvent::InputRejected) == true`. Fail: no haptic
fired on DURATION tier.
*Use `FTouchRadiusProviderStub.SetRadius(6.1f)`, `FHapticDispatchStub.SetCapability(DURATION)`.
Automated unit test.*

**AC-WCAG-01 — FWidgetVisualDispatch enforces WCAG 2.3.1 rate-limit [INTEGRATION TEST]:**
Drive `FWidgetVisualDispatch::Fire(EVisualEvent::R3CollisionDesaturation)` directly at a
rate exceeding 3 calls/sec (e.g., 10 calls in 500ms using a real `FWidgetVisualDispatch`
instance with a real-time clock). Use a mock `UInputFeedbackOverlay` subclass (counter only —
no UMG dependency) that increments a call count on `PlayR3CollisionDesaturation()`. Pass:
mock overlay call count ≤ 3 **per rolling second window** despite `FWidgetVisualDispatch::Fire()`
being called at > 3/sec — confirming the widget dispatch layer enforces the rate limit
before forwarding to the overlay. The "3/sec" threshold means at most 3 overlay calls may
occur in any 1000ms sliding window, not just in the first second of the test. Fail: mock
overlay receives > 3 calls in any 1000ms window (rate-limiting not enforced or uses a
fixed-epoch window instead of a rolling window).

Note: this test drives `FWidgetVisualDispatch` directly — **not IS and not
`FVisualDispatchStub`**. `FVisualDispatchStub` has no rate-limiter; using the stub here
would always fail the > 3/sec check regardless of `FWidgetVisualDispatch` behavior.
`FVisualDispatchStub.GetFireCountInWindowMs()` is for testing IS's firing frequency
(i.e., that IS fires at most once per tick), not for testing widget-layer rate-limiting.
File under `tests/integration/visual-dispatch/`. See `docs/architecture/visual-dispatch-contract.md`
§WCAG 2.3.1 Compliance Note for the rate-limit contract.

**AC-FRP-01 — First-run prompt visible during COUNTDOWN only [BEHAVIORAL]:**
New-install, first run. Pass: "Tap left.", "No slip", and "Tap right." labels
are all visible at ~78% screen height during COUNTDOWN state — left label in the left zone,
center label over the exclusion band, right label in the right zone. No labels are visible
during RUNNING state. No labels visible during COMPLETE, DEAD, or IDLE states. Fail: any
label missing during COUNTDOWN, or any label visible during RUNNING.

**AC-FRP-02 — First-run prompt dismisses at COUNTDOWN→RUNNING transition [BEHAVIORAL]:**
New-install, first run. Allow COUNTDOWN to transition to RUNNING without tapping. Pass:
all three labels dismiss simultaneously at the COUNTDOWN→RUNNING transition — none persist
into RUNNING. Fail: any label persists into RUNNING, or labels dismiss independently of each
other.

**AC-FRP-03 — First-run prompt reappears if no slip completed [BEHAVIORAL]:**
New-install. Complete two runs: first run ends without any slip; second run's COUNTDOWN begins.
Pass: all three labels reappear during second run's COUNTDOWN. After completing the first
successful slip (any direction, any run), complete another run — labels must NOT appear on
that run's COUNTDOWN. Fail: labels missing on second run's COUNTDOWN before first slip, or
labels appear after first slip has been completed.

**AC-FRP-04 — Viewport bounds gate — widget not created before viewport ready [DEBUG BUILD]:**
In test harness: set `GetViewportSize()` to return (0, 0), then set run state to
`ERunState::COUNTDOWN`. Pass: no widget creation call is made (prompt widget creation is
deferred until `GetViewportSize()` returns non-zero height). Switch to `GetViewportSize()`
returning (1080, 1920) — widget creation proceeds; assert that the computed y-coordinate
for the 78% label anchor is > 0 pixels. Fail: widget created while viewport height is zero
(would place labels at y=0); or y-coordinate asserted as 0 at creation time.
*Automated unit test using mock viewport size provider.*

**AC-FRP-05 — First-run flag persists across app restart [DEBUG BUILD]:**
Using `FBootFlagStoreStub`: Step 1: call `GetFlag("FIRST_RUN_PROMPT_ENABLED")` — returns
true (first run, flag absent defaults to true). Step 2: dispatch one successful slip —
IS calls `SetFlag("FIRST_RUN_PROMPT_ENABLED", false)`. Step 3: destroy IS instance and
create a new IS instance using the **same `FBootFlagStoreStub` instance** (simulates app
restart with persisted store). Step 4: advance to COUNTDOWN and call `DrainTick()`.
Pass: `GetFlag("FIRST_RUN_PROMPT_ENABLED") == false` on the new instance; no first-run
labels created. Fail: labels created on second instance (flag was not persisted, or new IS
instance re-initialised the flag to true).
*Automated unit test using `FBootFlagStoreStub` as shared persistent store.*

**AC-UX4-COUNTDOWN-NO-SETFLAG — COUNTDOWN slip does not call SetFlag [DEBUG BUILD]:**
Step 1: configure `FBootFlagStoreStub` so `GetFlag("FIRST_RUN_PROMPT_ENABLED")` returns true;
set state to `ERunState::COUNTDOWN`. Step 2: inject a LEFT-zone contact via `SimulateTouch()`;
call `DrainTick()` (1-tick hold). Step 3: call `DrainTick()` — `IS_SLIP_DISPATCHED` fires
(slip dispatched; Player Movement receives the event but ignores it during COUNTDOWN).
Pass: `IS_FRP_DISMISSED` does NOT fire; `FBootFlagStoreStub.SetFlagCallCount == 0`;
`b_first_run_prompt` remains true (no dismissal during COUNTDOWN). Fail: `IS_FRP_DISMISSED`
fires; or `FBootFlagStoreStub.SetFlagCallCount > 0` (SetFlag called despite UX-4 guard).
*Automated unit test. Without this AC, a buggy implementation that ignores the
`current_state == RUNNING` guard in the UX-4 path would pass all other first-run ACs.*

**AC-FRP-GETFLAG-COUNTDOWN — IBootFlagStore::GetFlag called exactly at COUNTDOWN entry, not at construction or RUNNING entry [DEBUG BUILD]:**
Using `FBootFlagStoreStub` with call-count tracking: Step 1: construct IS —
`FBootFlagStoreStub.GetFlagCallCount == 0` (GetFlag not called at construction). Step 2:
set state to `ERunState::IDLE`; call `DrainTick()` — GetFlag call count remains 0.
Step 3: switch to `ERunState::COUNTDOWN`; call `DrainTick()` — GetFlag call count must
equal 1 (`b_first_run_prompt` loaded at the first COUNTDOWN DrainTick via `prev_state`
transition detection). Step 4: switch to `ERunState::RUNNING`; call `DrainTick()` —
GetFlag call count remains 1 (not re-read at RUNNING entry).
Pass (Steps 1–4): exactly one GetFlag call, occurring on the first COUNTDOWN DrainTick. Fail: GetFlag
not called by COUNTDOWN entry (b_first_run_prompt uses stale construction-time default
and misses inter-session flag changes); or GetFlag called at RUNNING entry; or called more
than once before COUNTDOWN.
Step 5 (second COUNTDOWN re-entry — verifies re-read on every COUNTDOWN entry, not only the first):
switch stub to `ERunState::RUNNING`; call `DrainTick()`. Switch to `ERunState::COMPLETE`;
call `DrainTick()`. Switch to `ERunState::IDLE`; call `DrainTick()`. Switch to
`ERunState::COUNTDOWN`; call `DrainTick()` — the edge `prev_state != COUNTDOWN AND current_state == COUNTDOWN` fires again.
Pass: `GetFlagCallCount == 2` after Step 5. Fail: `GetFlagCallCount == 1` (b_first_run_prompt
cached from first load and never re-read — a player who re-enables the prompt between runs
would never see labels again because the flag is not re-read at subsequent COUNTDOWN entries).
Step 6 (inter-run flag mutation — verifies a re-enabled flag actually drives the prompt UI
state on the next COUNTDOWN entry, not just that GetFlag was called):
between Step 5's RUNNING and the next COUNTDOWN, call `FBootFlagStoreStub.InitFlag(TEXT("FIRST_RUN_PROMPT_ENABLED"), true)`
(simulating a re-enable, e.g., via Settings reset). Switch stub through `ERunState::RUNNING`
→ `ERunState::COMPLETE` → `ERunState::IDLE` → `ERunState::COUNTDOWN`; call `DrainTick()` on
each. Assert that on the final COUNTDOWN tick: `b_first_run_prompt == true` is observable
via the prompt-widget creation path (UI integration probe: either `FVisualDispatchStub`
records a label-creation call, or the IS exposes a `b_first_run_prompt` accessor in
non-shipping builds). `GetFlagCallCount == 3`.
Pass: prompt is enabled on the third COUNTDOWN entry after re-enable. Fail: prompt remains
suppressed (IS read the new flag value but didn't propagate it to the widget creation
path — i.e., GetFlag is called but its return value is ignored, which would silently
break re-enable).
*Automated unit test.*

**AC-COUNTDOWN-ACK-HAPTIC — COUNTDOWN slip fires haptic acknowledgement on non-NONE tiers; RUNNING slip does not [DEBUG BUILD]:**
With `FHapticDispatchStub.SetCapability(EHapticCapability::FULL)`:
Step 1: configure `FBootFlagStoreStub.InitFlag("FIRST_RUN_PROMPT_ENABLED", true)`;
set state to `ERunState::COUNTDOWN`. Step 2: inject a LEFT-zone contact via `SimulateTouch()`;
call `DrainTick()` (receipt tick). Step 3: call `DrainTick()` (dispatch tick).
Pass: `IS_SLIP_DISPATCHED` fires with `direction=left`;
`IS_HAPTIC_FIRED` fires with `haptic_type=dead_band_feedback` and `capability_tier=FULL`;
`IS_COUNTDOWN_ACK_HAPTIC_FIRED` fires with `contact_id` and `capability_tier=FULL`;
`IS_NONE_TIER_COUNTDOWN_ACK_VISUAL_FIRED` does NOT fire;
`FHapticDispatchStub.WasFired(EHapticEvent::DeadBandContact) == true`;
`IS_FRP_DISMISSED` does NOT fire.
Step 4: switch to `ERunState::RUNNING`; inject a new LEFT-zone contact and call `DrainTick()` twice.
Pass: `IS_SLIP_DISPATCHED` fires; `IS_COUNTDOWN_ACK_HAPTIC_FIRED` does NOT fire
(acknowledgement is COUNTDOWN-scoped only; RUNNING slips produce PM-visible events via PM's own SlipConfirmed haptic path).
Step 5 (RIGHT-zone symmetric verification): reset stubs; set state back to `ERunState::COUNTDOWN`;
inject a RIGHT-zone contact via `SimulateTouch()`; call `DrainTick()` twice.
Pass (Step 5): `IS_SLIP_DISPATCHED` fires with `direction=right`;
`IS_COUNTDOWN_ACK_HAPTIC_FIRED` fires with `contact_id` and `capability_tier=FULL`;
`FHapticDispatchStub.WasFired(EHapticEvent::DeadBandContact) == true` (haptic is zone-agnostic
on non-NONE tiers — same event class fires for left and right COUNTDOWN-ack; the zone is
encoded only on NONE-tier via `IS_NONE_TIER_COUNTDOWN_ACK_VISUAL_FIRED.zone`).
Fail: acknowledgement fires in RUNNING state; acknowledgement does not fire in COUNTDOWN state
on either zone; or `IS_HAPTIC_FIRED` does not fire alongside `IS_COUNTDOWN_ACK_HAPTIC_FIRED`.
*Automated unit test.*

**AC-COUNTDOWN-ACK-VISUAL-NONE — COUNTDOWN slip fires zone-targeted Resting flash on NONE tier [DEBUG BUILD]:**
With `FHapticDispatchStub.SetCapability(EHapticCapability::NONE)`:
Step 1: set state to `ERunState::COUNTDOWN`. Step 2: inject a LEFT-zone contact via `SimulateTouch()`;
call `DrainTick()` (receipt tick). Step 3: call `DrainTick()` (dispatch tick).
Pass: `IS_SLIP_DISPATCHED` fires with `direction=left`;
`IS_NONE_TIER_COUNTDOWN_ACK_VISUAL_FIRED` fires with `contact_id` and `zone=LEFT`;
`FVisualDispatchStub.WasFired(EVisualEvent::ContactRestingLeft) == true`;
`FVisualDispatchStub.WasFired(EVisualEvent::ContactRestingRight) == false`;
`FVisualDispatchStub.WasFired(EVisualEvent::DeadBandContactFlash) == false` (zone-targeted, not center-strip);
`IS_COUNTDOWN_ACK_HAPTIC_FIRED` does NOT fire;
`FHapticDispatchStub.WasFired(EHapticEvent::DeadBandContact) == false` (no haptic on NONE tier).
Step 4 (RIGHT-zone symmetric verification): reset `FVisualDispatchStub`; inject a RIGHT-zone
contact; call `DrainTick()` twice.
Pass (Step 4): `IS_NONE_TIER_COUNTDOWN_ACK_VISUAL_FIRED` fires with `zone=RIGHT`;
`FVisualDispatchStub.WasFired(EVisualEvent::ContactRestingRight) == true`;
`FVisualDispatchStub.WasFired(EVisualEvent::ContactRestingLeft) == false`.
Fail: visual does not fire on NONE tier; or wrong zone fires (LEFT contact → ContactRestingRight
or vice versa — would indicate a spatial lie); or `DeadBandContactFlash` fires (center-strip
flash for left/right tap — pre-re-review-26 behavior); or haptic fires on NONE tier; or
`IS_COUNTDOWN_ACK_HAPTIC_FIRED` fires (indicates NONE-tier path was not taken).
*Automated unit test.*

**AC-IS21A1 — COUNTDOWN→RUNNING tracking reset fires and clears both maps [DEBUG BUILD]:**
Step 1: set state to `ERunState::COUNTDOWN`; inject a LEFT-zone contact (OS_FingerIndex=0);
call `DrainTick()` — `IS_TOUCH_RECEIVED` fires; contact in tracking_set;
`fingerindex_to_contactid_map[0]` populated. Step 2: switch stub to
`ERunState::RUNNING`; call `DrainTick()`.
Pass: `IS_COUNTDOWN_RESET` fires before any Step 1 event processing in this tick;
tracking_set is empty after the tick (confirmed by injecting touch-up for the original
contact_id — `IS_TOUCH_UP_ORPHAN` fires); `fingerindex_to_contactid_map[0]` is cleared
(confirmed by injecting a new touch-DOWN for OS_FingerIndex=0 — `IS_DUPLICATE_CONTACT_ID_REJECTED`
does NOT fire). `IS_CANCEL_DISPATCHED` and `IS_HAPTIC_FIRED` do NOT fire.
Fail: `IS_COUNTDOWN_RESET` does not fire; or contact survives in tracking_set; or
`fingerindex_to_contactid_map` retains stale entry; or any haptic fires on first RUNNING tick.
*Automated unit test.*

**AC-DRAINTICK-E2E — End-to-end DrainTick pipeline: touch-down to slip dispatch [DEBUG BUILD]:**
Verify the full DrainTick pipeline in one test: inject a valid left-zone touch with
`radius_mm = 3.0` and `OS_FingerIndex = 0` via `SimulateTouch()`. Call `DrainTick()`
(tick N — receipt tick). Call `DrainTick()` (tick N+1 — dispatch tick).
Pass: (1) `IS_TOUCH_RECEIVED` appears in tick N with correct `OS_FingerIndex=0`,
`contact_id` assigned, `radius_mm=3.0`, `zone=LEFT`; (2) `IS_ZONE_CLASSIFIED` fires in
tick N with `zone=LEFT`; (3) `IS_COLLISION_DETECTED` fires in tick N+1 with
`result=PASS` (no collider); (4) `IS_SLIP_DISPATCHED` fires in tick N+1 with
`direction=left`, `drain_tick_index = N+1` (delta = 1 from receipt); (5) no
`IS_PALM_REJECTED`, `IS_DEAD_BAND_DISCARDED`, or `IS_CANCEL_DISPATCHED` fires.
Fail: any log entry missing, wrong tick, wrong direction, or delta ≠ 1.
*Automated unit test using `FFakeMonotonicClock` and `SimulateTouch()`. This test
catches pseudocode drift: a GDD where all formula property tests pass but the
orchestration wiring is broken will fail here.*

---

*Flagged — not formal ACs (untestable without specialized hardware or user-state setup):*
- *Haptic calibration: IS_HAPTIC_FIRED log enables automated event verification. Physical-device calibration required for pattern distinctiveness and amplitude.*
- *Android capability tiers (DURATION/AUDIO/VISUAL): require OEM device coverage. Fallback specs in `design/ux/input-feedback.md` (audio-director + UX designer sign-off pending).*

## Open Questions

**1. Haptic intensity calibration (OPEN):** R-3 collision pattern needs final calibration
against PM's slip-confirmed pulse on target devices to confirm distinct perception.
Dead-band haptic (when enabled) needs calibration below both. Physical-device validation
required — automated tests cannot verify amplitude or perception.

**2. 180ms cancel timer empirical basis (OPEN):** The 180ms value is set by design
intuition. HCI literature puts intended mobile tap durations at 60–150ms under cognitive
load; 180ms sits at the upper edge. Under SLIPSTORM's 600ms telegraph window, a rapid
double-tap (slip + immediate correction) risks the second contact arriving while the
first is resting — a Pillar 5 failure mode. **Gate: validate via internal playtest
telemetry before implementation merge.** Target: ≥95% of intended taps complete within
the cancel window across N ≥ 100 internal playtest sessions on target devices.

**3. design/ux/input-feedback.md sign-off (OPEN):** Audio and visual tier fallback specs
require audio-director and UX designer approval before IS implementation sprint. Open items
in `design/ux/input-feedback.md`: (1) who authors WAV assets, (2)
`DEAD_BAND_FEEDBACK_ENABLED` default on AUDIO tier, (3) 50ms layering window
confirmation, (4) visual indicator render priority vs. In-Run HUD.

**4. Screen orientation (RESOLVED — re-review 23; pseudocode hoisted re-review 26):** Portrait-only lock is enforced at the OS manifest level. iOS: `UIInterfaceOrientationMask = .portrait` (set in `Info.plist` and `AppDelegate`). Android: `android:screenOrientation="portrait"` in `AndroidManifest.xml` for the game activity. Additionally, IS runs a viewport state machine at the top of every `DrainTick()` call — see DrainTick pseudocode at §Formulas (before Step 0 counter increment). The state machine has three branches: (a) `GEngine->GameViewport == nullptr` → no-op early-return (do not advance counter or process events); retried each tick until viewport is available — this covers cold-boot ticks before the first level loads. (b) viewport present and `W >= H` → log `IS_VIEWPORT_ASPECT_WRONG`, unregister FTSTicker, halt — catches simulator/misconfiguration. (c) viewport present and `W < H` → set `bViewportChecked = true` and fall through; the flag also gates first-run prompt widget creation (single source of truth, no parallel viewport gates). Landscape mode is not supported; if added in future, W and B derivations in F-1 must be revisited.

---

*Resolved (documented for traceability):*
- *Screen orientation (OQ-4, re-review 23/24/26, 2026-06-03): Portrait-only enforced via iOS `UIInterfaceOrientationMask = .portrait` + Android `android:screenOrientation="portrait"`. IS runs a 3-state viewport state machine at the top of every `DrainTick()` — pending (null viewport, retry), misconfigured (W ≥ H, halt), valid (W < H, set flag and fall through). Pseudocode hoisted into §Formulas DrainTick (re-review 26). See OQ-4 above for full spec.*
- *RSM UE object type (OQ-7, 2026-06-01): `URunStateMachineSubsystem : UGameInstanceSubsystem`. `FRSMRunStateProvider` holds `TWeakObjectPtr<URunStateMachineSubsystem>`, resolved via `UGameInstance::GetSubsystem<URunStateMachineSubsystem>()`. IRunStateProvider table and wiring pseudocode updated accordingly.*
- *IInputProcessor method names (re-review 11, 2026-05-31): `HandleTouchStartedEvent` / `HandleTouchEndedEvent` are correct for per-finger mobile touch events on iOS/Android. `HandleMouseButtonDownEvent/Up` receive zero touch events with `bUseMouseForTouch=false`. ADR-0003 updated.*
- *Event interface shape: `slip-left` / `slip-right` confirmed; cancel-slip removed from PM contract (IS-internal only).*
- *Contact radius access: resolved by ADR-0001 (`FTouchRadiusBridgePlugin` + `ITouchRadiusProvider`).*
- *Haptic suppression: resolved — IS owns R-3 collision + dead-band; PM owns slip-confirmed + buffer-drop.*
- *Android haptic equivalents: resolved by ADR-0002 (`HAPTIC_CAPABILITY` enum + `FHapticPlatformPlugin`).*
- *F-4 pairwise tie-breaking: resolved — ascending contact_id ordering, deterministic within session.*
- *Implementation path (OQ-1): resolved by ADR-0003 — `FInputSystem` implements `IInputProcessor`; `FTSTicker` drain queue. UMG overlay path not taken.*

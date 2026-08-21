# HUD Design

> **Status**: In Design
> **Author**: user + ux-designer
> **Last Updated**: 2026-06-27
> **Template**: HUD Design

> **Scope note**: This document specifies the in-run HUD only. End-Run (RESOLVING), Interrupted-Run (ABORTED), Main Menu, and Accessibility Settings are separate UX specs (future). First-run input prompts during COUNTDOWN are owned by Input System per `design/gdd/input-system.md` §UI Requirements and are NOT a HUD concern.

> **Pillar binding**: Pillar 4 ("Surreal in Motion, Not in Menus") and Pillar 5 ("Skill Is Visible") are the two binding pillars for this HUD. Every section below should pass a Pillar-4 minimality check and a Pillar-5 attributability check.

---

## HUD Philosophy

The HUD is **"Minimal but present"** — only system-mandated information appears on screen during a run. The game's three load-bearing reads (where the player is, where the waves are, where the body is leaning) all happen in the 3D world, not in chrome. The HUD's only job is to surface the two pieces of information no system-of-the-world can communicate diegetically:

1. **How much run time remains** (RSM Pillar 5 attributability: in the final 1-2 seconds the player must be able to confirm "I survived the timer" vs "I died to a wire just before time" — without this, wire-deaths cannot be distinguished from completion).
2. **Whether Performance mode is active** (PM R11a-9 inheritance: when the watchdog suppresses M=3 PEAK barrages, the player must be informed so the difficulty shift is attributable rather than mysterious).

Everything else is forbidden. The HUD never shows wave counts, phase indicators, lane indicators, telegraph meters, score-during-run, or any element listed under "Forbidden HUD elements" in the dependency GDDs. If a future system wants HUD chrome, that system must amend this philosophy first.

**Binding tests** for every subsequent HUD element decision:

- **Pillar 4 minimality test**: Can the player do this read from the 3D world alone? If yes — no HUD element.
- **Pillar 5 attributability test**: If this information is absent, can the player still attribute outcomes correctly? If yes — no HUD element.
- **Diegetic-alternative test**: Is there a world-state read (audio mix, lighting, character pose, voxel behavior) that conveys this without screen chrome? If yes — prefer the diegetic channel.

The two mandated elements above passed all three tests with no diegetic substitute. Any future proposed addition must be evaluated against the same three tests, on record.

---

## Information Architecture

### Full Information Inventory

Every piece of information the in-run HUD could conceivably communicate is listed below. The inventory is closed — additions require amending §HUD Philosophy first.

| # | Item | Source GDD | When | Source mandate |
|---|---|---|---|---|
| 1 | `remaining_time` (timer) | RSM | RUNNING | "Must display every frame" (`run-state-machine.md` §UI Requirements) |
| 2 | COUNTDOWN visual (3-2-1 or instant) | RSM | COUNTDOWN | "Or during COUNTDOWN if a countdown display is desired" — explicitly optional |
| 3 | Performance-mode banner | PM R11a-9/10 | RUNNING, when watchdog suppresses M=3 PEAK | Copy locked: `"Performance mode — hardest barrage suppressed."`; safe-area anchored below OS top inset |
| 4 | NONE-tier haptic-fallback visuals | IS / `design/ux/input-feedback.md` | RUNNING, when device haptic-capability tier = NONE | R-3 zone-edge pulse / ContactResting / dead-band flash (delegated from IS; HUD owns render-priority stack) |
| 5 | Pause / app-background indicator | RSM `is_paused` | RUNNING + `is_paused == true` | Derived from RSM pause semantics — needed so player can distinguish paused-by-OS from frozen-by-bug |

**Forbidden items** (explicitly listed in dependency GDDs — recorded here so the inventory cannot be silently expanded):

- Wave-count counter, per-wave targeting indicators / arrows, "barrage incoming" warning, lane occupancy indicators, approach-distance / threat-radius visualization — per `pull-wave-behavior.md` §UI Requirements
- Current-phase text label, phase progress bar, difficulty-tier icons or stars, run-completion timer visualization that exposes phase boundaries — per `difficulty-phase-controller.md` §UI Requirements (the 60s run timer itself is exempt — Pillar 5)
- Lane indicator UI — per `player-movement-presentation.md` §UI Requirements (Pillar 1: the slip is the only verb; lane readout would teach the player to watch the HUD instead of the wave field)
- Any UI flash or overlay on edge-absorb — per `player-movement-presentation.md` §UI Requirements
- Any Input System chrome during RUNNING beyond the NONE-tier visual events listed at item 4 — per `input-system.md` §UI Requirements
- In-run score / health / lives / combo counter — no GDD calls these out; single-life 60s run has no score-during-run concept; introducing one would require a new GDD + a §HUD Philosophy amendment

### Categorization

Per the 4-category scheme: **Must Show** (always visible during applicable state) / **Contextual** (visible only when a specific condition is true) / **On Demand** (player must actively request) / **Hidden** (communicated through world/audio/diegetic channel, never on-screen text).

| # | Item | Category | Gating condition |
|---|---|---|---|
| 1 | Timer | **Must Show** (RUNNING) | RSM mandates per-frame display during RUNNING; Pillar 5 attributability requires it for wire-death distinguishability in the final 1–2 s |
| 2 | COUNTDOWN visual | **Hidden** (diegetic-alternative satisfied by IS first-run prompts) | Considered as Contextual; rejected by §HUD Philosophy's diegetic-alternative test — IS first-run prompts ("Tap left." / "No slip" / "Tap right.") already give the player something to read during COUNTDOWN. Adding a 3-2-1 numeral is chrome for chrome's sake. RSM marks the COUNTDOWN visual as explicitly optional, so the HUD declines. See §Open Questions for the re-evaluation criterion (if playtest reveals IS prompts alone don't convey "imminent start", revisit). |
| 3 | Performance-mode banner | **Contextual** (watchdog-gated) | Visible only while the hardware-tier watchdog is actively suppressing M=3 PEAK barrages; absent otherwise; absent during COUNTDOWN/RESOLVING/IDLE/ABORTED |
| 4 | NONE-tier haptic-fallback visuals | **Contextual** (device-tier gated) | Visible only on NONE-tier devices (no haptic hardware AND no audio output, OR user-disabled feedback); event-driven (not persistent); HUD layer owns render priority above gameplay actors per `input-feedback.md` open item 4 |
| 5 | Pause indicator | **Contextual** (`is_paused` gated) | Visible only when `RSM.is_paused == true` (typically app-background); dismisses on `OnPausedChanged → false`; coexists with the timer freeze (timer remains visible but holds its value) |

**Categorization counts**: Must Show = 1, Contextual = 3, On Demand = 0, Hidden (diegetic alternative OR Pillar-4 prohibition) = COUNTDOWN visual + score/health/lives/combo/wave-count/phase/etc.

**Conflict check vs §HUD Philosophy**: passes. The HUD is chrome-free or near-chrome-free for the majority of every run — only the timer is permanently on-screen, and the contextual items each appear under tightly scoped conditions.

---

## Layout Zones

Arrangement: **Top-banded** (selected over corner-asymmetric and floating/diegetic alternatives — see §HUD Philosophy binding tests and PM R11a-10 banner-anchor mandate).

### Zone definitions

- **Zone L0 — Safe-area reserve** (OS-managed; HUD never paints here). The OS top inset (status bar / notch / dynamic island on iOS; status bar on Android). Anchor reference for all top-band positions: `safe_area_top_inset_px` reported by the platform at run start and on viewport-change events. HUD must subscribe to viewport-change events and reflow L1 if the inset changes (e.g., iOS interface-rotation; though SLIPSTORM is portrait-locked, rotation-lock-failure modes must still leave L1 anchored correctly).
- **Zone L1 — Top band** (HUD-owned permanent chrome strip). Directly below the OS top inset; height ~10 % of screen height (precise device-class numerics in §Platform & Input Variants). Holds the timer (always present during RUNNING) and the Performance-mode banner (present only when watchdog-gated). Only permanent HUD zone in the run.
- **Zone L2 — Gameplay viewport**. Between L1 (top) and L3 (bottom). **No HUD chrome painted here except transient overlays** (see "Transient element placement" below). This is the "Surreal in Motion" sanctum — Pillar 4 binding.
- **Zone L3 — Touch input region** (bottom ~75 % of viewport, partitioned by Input System into LEFT tap zone / 8 mm center dead-band exclusion / RIGHT tap zone). **HUD never paints here.** Owned by IS for touch input + COUNTDOWN first-run prompts at ~78 % screen height.
- **Zone L4 — Thumb-rest reserve** (bottom ~20 % of screen). HUD never paints here — high-occlusion area noted by IS GDD §UI Requirements item 1.

### Transient element placement

Transient overlays use other zones temporarily but never become permanent zone owners.

| Element | Active when | Position | Render priority |
|---|---|---|---|
| NONE-tier haptic-fallback visuals (R-3 zone-edge pulse, ContactResting, dead-band flash) | Device haptic-tier = NONE AND triggering event fires | Per-cue positions defined in `design/ux/input-feedback.md` — HUD inherits, does not redefine | Highest — above all gameplay actors AND above the pause overlay (haptic-fallback is safety-critical accessibility) |
| Pause indicator | `RSM.is_paused == true` | Centered overlay on L2 with semi-transparent backing | Above gameplay actors; below NONE-tier haptic-fallback visuals |

(COUNTDOWN visual was considered as a transient overlay but rejected per §HUD Philosophy diegetic-alternative test — IS first-run prompts during COUNTDOWN are the only COUNTDOWN UI. See §Open Questions for re-evaluation criterion.)

### Banner-timer coexistence rule (binding)

When the Performance-mode banner appears mid-run, the timer reflows from center alignment to right alignment within L1. The timer:

- **Does NOT disappear** — Pillar 5 attributability still requires timer visibility throughout RUNNING.
- **Does NOT shrink below legibility** — minimum font size held per §Platform & Input Variants device-class table.
- **Does NOT animate** the reposition — banner is the salient mid-run event; an animated timer reflow would pull focus from active gameplay. The reflow must be a single-frame snap on the same frame the banner appears.

The banner itself MAY animate in (see §Dynamic Behaviors for the entrance/exit treatment) since it is the salient event being announced.

### ASCII wireframe (portrait, 6-inch screen reference)

Default state (no banner, RUNNING):

```
+----------------------------+
|   [OS safe-area inset]     |  ← L0 (OS, not painted by HUD)
+----------------------------+
|        00:42               |  ← L1 (top band, ~10% screen h)
|     timer center-aligned   |     timer-only state
+----------------------------+
|                            |
|                            |
|         (character)        |  ← L2 (gameplay viewport)
|        ↑ waves approach    |     NO permanent chrome
|                            |     transient overlays only
|                            |
+----L-----+ |c| +-----R-----+  ← L3 (IS touch region;
|   tap     dead    tap      |     LEFT / dead-band / RIGHT
|   zone    band    zone     |     first-run prompts at 78%
+----------------------------+     during COUNTDOWN only)
|    [thumb-rest reserve]    |  ← L4 (no HUD paint)
+----------------------------+
```

Banner-active variant (watchdog suppressing M=3 PEAK):

```
+----------------------------+
|   [OS safe-area inset]     |
+----------------------------+
| Performance mode...  00:42 |  ← L1: banner left, timer right
+----------------------------+
| (L2 / L3 / L4 unchanged)   |
```

Pause-active variant (overlays L2; L1 unchanged; timer holds its value per RSM AC-09):

```
+----------------------------+
|   [OS safe-area inset]     |
+----------------------------+
|        00:42               |  ← L1 unchanged; timer frozen
+----------------------------+
|   +--------------------+   |
|   |                    |   |
|   |     [paused]       |   |  ← Pause overlay on L2
|   |                    |   |     semi-transparent backing
|   +--------------------+   |
|   (rest of L2 visible      |
|    through semi-trans)     |
+----------------------------+
| (L3 / L4 unchanged but     |
|  IS touch is suspended)    |
+----------------------------+
```

---

## HUD Elements

Four elements total (per §Information Architecture categorization).

### Element 1 — Timer

| Field | Spec |
|---|---|
| Category | Must Show (during RUNNING) |
| Content | `remaining_time` from RSM, in seconds |
| Format | `SS` with leading zero from 60→2 (e.g., `60`, `42`, `09`, `02`); switches to `S.t` from <2.0→0 (e.g., `1.9`, `0.5`, `0.0`) |
| Visual form | Numeric text glyph; font family/weight/size delegated to UI team (per IS-pattern delegation in `input-system.md` §UI Requirements); fill `#FFFFFF` + outline 1.5 logical px `#000000` + drop shadow at floor matching IS labels for WCAG 2.1 AA contrast |
| Color — default (`remaining_time ≥ 2.0`) | White per above |
| Color — final 2 s (`remaining_time < 2.0`) | Red — `#FF3030` provisional, final swatch TBD by art-director |
| Scale — default | 1.0× |
| Scale — final 2 s | Ramps 1.0× → 1.4× linearly over the 2.0 s window |
| Update behavior | Real-time; updated every frame from RSM `remaining_time` |
| Visibility | Driven by `OnStateChanged`. Enters on COUNTDOWN→RUNNING; exits on RUNNING→{DEAD, COMPLETE, ABORTED}. Not shown during COUNTDOWN itself (COUNTDOWN visual is rejected per §Information Architecture). |
| Animation — entrance | Fade-in over 150 ms at COUNTDOWN→RUNNING transition |
| Animation — decimal regime entry | Single-frame snap from `02` to `1.9`; color/scale ramp begins on the same frame |
| Animation — per-tick | None (display value steps; no tween between integer values) |
| Animation — exit | DEAD: freezes at current value for `DEAD_SNAP_DURATION_S`, then dismisses on DEAD→RESOLVING. COMPLETE: snaps to `00`, holds 1 frame, dismisses on RUNNING→COMPLETE. ABORTED: dismisses immediately on transition |
| Banner-coexistence | When the Performance-mode banner appears, reflows center→right alignment within L1 in a single-frame snap (no tween — per §Layout Zones binding rule) |

**Pillar 5 attribution rationale**: the SS.t decimal regime + color/scale modulation in the final 2 s give the player two redundant attribution channels — numeric precision (player can read `0.4` vs `0.0`) and peripheral salience (red+enlarged glyph is visible without direct foveation while the player is reading the wave field). Either channel alone suffices for wire-death distinguishability; together they pass the Pillar 5 attributability test for both attentive and distracted-final-second cases.

### Element 2 — Performance-mode banner

| Field | Spec |
|---|---|
| Category | Contextual (PM watchdog gated) |
| Content | Locked copy: `"Performance mode — hardest barrage suppressed."` (PM R11a-9 binding) |
| Visual form | Horizontal text bar within L1; left/center-aligned (timer reflows right); font TBD by UI team; color palette TBD by art-director; must satisfy WCAG 2.1 AA contrast against game background using IS-pattern fill `#FFFFFF` + outline 1.5 px `#000000` + drop shadow at floor |
| Update behavior | Event-driven; appears once on PM watchdog M=3 PEAK suppression engagement event; **persists until run ends** (appear-once-stick model — selected over real-time mirroring and hysteresis alternatives) |
| Contextual trigger | PM watchdog reports M=3 PEAK suppression actively engaged at least once during the run |
| Animation — entrance | Fade-in over 300 ms (slow enough to register as "something happened" without pulling focus from active gameplay; coordinated with the timer's same-frame reflow snap — banner fades in while the timer is already in its right-aligned position) |
| Animation — during run | Static (no animation; banner is reference info, not an attention-pull) |
| Animation — exit | DEAD: banner remains visible during the DEAD freeze and dismisses on DEAD→RESOLVING (alongside the timer). COMPLETE: dismisses on RUNNING→COMPLETE alongside the timer. ABORTED: dismisses immediately on transition alongside the timer. No dedicated fade in any case — RSM state exit drives dismissal. |
| Vocabulary dependency | `"barrage"` must be established as player-facing in the Main Menu UX spec before the banner can ship — PM R11a-9 CONTINGENT BLOCKING; forward contract recorded in §Open Questions |

**Persistence-model rationale**: appearance-once-stick aligns with Pillar 5 attributability — the player knows the *entire remainder* of the run was suppressed, not just the trigger moment. Real-time mirroring risks watchdog flicker (banner visibly appearing/disappearing on threshold oscillation) which would itself become a focus-pull. Hysteresis is a compromise but adds complexity without solving a problem the appearance-once-stick model has.

### Element 3 — NONE-tier haptic-fallback visuals

| Field | Spec |
|---|---|
| Category | Contextual (device haptic-tier = NONE gated) |
| Content | HUD does **NOT** own content. Per-cue visuals (R-3 zone-edge pulse, ContactResting deactivation cue, dead-band flash) are specified in `design/ux/input-feedback.md` |
| HUD responsibility | Hosting + render priority only. HUD layers these at highest priority above gameplay actors AND above the pause overlay (haptic-fallback is safety-critical accessibility — must remain visible even when paused) |
| `input-feedback.md` open item 4 resolution | This spec proposes the resolution at §Layout Zones (transient element placement table) and at this element's Render priority field. Cross-doc sync needed in `input-feedback.md` to mark open item 4 RESOLVED — recorded in §Open Questions |
| Update / animation | Event-driven from IS; per-event animation specified in `input-feedback.md` — HUD inherits, does not redefine |

### Element 4 — Pause indicator

| Field | Spec |
|---|---|
| Category | Contextual (`RSM.is_paused == true` gated) |
| Content | Provisional text label `"Paused"` — alternative pause-glyph icon is acceptable; final choice TBD by art-director |
| Visual form | Centered on L2 with semi-transparent dark backing (`#000000` @ 50 % alpha provisional; final values per art-director) |
| Update behavior | Event-driven; subscribed to `OnPausedChanged`. Appears on `→ true`; dismisses on `→ false` |
| Contextual trigger | `RSM.is_paused == true` (any RSM state where pause is permitted; primarily RUNNING) |
| Animation — entrance | Fade-in over 150 ms |
| Animation — during pause | Static |
| Animation — exit | Fade-out over 150 ms |
| Coexistence with timer | Timer remains visible in L1 (frozen at its pause-onset value per RSM AC-09 timer-freeze contract); pause overlay sits on L2 and does NOT obscure L1 |

---

## Dynamic Behaviors

### HUD density timeline (per RSM state cycle)

| Moment | Trigger | HUD density change |
|---|---|---|
| RSM `IDLE → COUNTDOWN` | Player taps start from Main Menu | IS first-run prompts appear (not HUD-owned); HUD remains empty |
| RSM `COUNTDOWN → RUNNING` | COUNTDOWN settles (~`COUNTDOWN_DURATION_S`) | Timer fades in (150 ms); IS prompts dismiss (per IS GDD) |
| `remaining_time` crosses `< 2.0` | RSM timer tick | Timer format `SS` → `S.t` single-frame snap; color/scale ramp begins (linear over 2.0 s) |
| PM watchdog engagement (first M=3 PEAK suppression of the run) | PM watchdog event | Performance-mode banner fades in (300 ms); timer single-frame reflows center → right |
| `OnPausedChanged → true` | App-background OR manual pause | Pause overlay fades in (150 ms); timer + banner remain in L1 (timer frozen per RSM AC-09) |
| Device-tier evaluated NONE | At app start (rare runtime change) | NONE-tier haptic-fallback visuals become eligible to fire (event-driven, not always-on) |
| RSM `RUNNING → DEAD` | `death_confirmed` event | Timer freezes at value; banner remains; all dismiss on DEAD→RESOLVING |
| RSM `RUNNING → COMPLETE` | Timer expires | Timer snaps `00`; banner remains; all dismiss on RUNNING→COMPLETE |
| RSM `RUNNING → ABORTED` | `PAUSED_TIMEOUT_S` exceeded | All HUD elements dismiss immediately |

### Density-shift rules (binding)

1. **No HUD element animates IN during active gameplay EXCEPT the banner.** Timer entry happens at the COUNTDOWN→RUNNING boundary (player attention shift); decimal-regime entry is a single-frame snap (not animated); pause overlay only appears when input is suspended. The banner is the sole mid-RUNNING animated entrance, and its 300 ms fade is calibrated to register-without-distract.
2. **No two HUD elements animate simultaneously during RUNNING.** The banner-and-timer-reflow rule (§Layout Zones binding): banner fades IN while the timer SNAPs (no tween) to its new position on the same frame. Prevents competing focus pulls during active gameplay.
3. **Density never decreases mid-run except through state transitions.** The banner does not auto-dismiss mid-run (appear-once-stick model — §HUD Elements Element 2). The pause overlay only dismisses on `OnPausedChanged → false`. NONE-tier haptic visuals are event-driven and self-dismissing per `design/ux/input-feedback.md`.
4. **Cross-state HUD reset**: every RUNNING-exit transition (DEAD / COMPLETE / ABORTED) dismisses all HUD elements at once. The next COUNTDOWN→RUNNING starts fresh (timer at the configured `RUN_DURATION_S` value, banner hidden, pause hidden). Watchdog state persistence across runs is unresolved — see §Open Questions.

### App-background / app-foreground recovery

| Scenario | HUD behavior |
|---|---|
| App backgrounds during COUNTDOWN | IS first-run prompts hidden by OS; HUD remains empty |
| App backgrounds during RUNNING | RSM sets `is_paused = true` → pause overlay fades in (150 ms); timer freezes per RSM AC-09; banner state preserved |
| App foregrounds with `paused_duration < PAUSED_TIMEOUT_S` | Pause overlay fades out (150 ms); timer resumes from frozen value (per RSM AC-09); banner state preserved |
| App foregrounds with `paused_duration > PAUSED_TIMEOUT_S` (RUNNING source) | RSM transitions RUNNING→ABORTED→IDLE (per RSM AC-11); HUD dismisses immediately; Interrupted-Run screen (separate UX spec) takes over |
| Device-tier change mid-run (e.g., haptics revoked) | Out of scope. NONE-tier evaluation happens at app start; mid-run tier changes are deferred — see §Open Questions |

---

## Platform & Input Variants

### Platform support matrix

| Platform | Status | Notes |
|---|---|---|
| iOS | First-class | iPhone 8 (5.5" / 16:9 / no notch) through iPhone 16 Pro Max (6.7" / 19.5:9 / Dynamic Island) |
| iPadOS | **Out of scope** | Game is portrait-mobile-phone; iPad-specific layouts deferred to a future UX spec amendment |
| Android | First-class | Android API 24+ (Nougat); 5"–6.7" portrait phones; notched + non-notched both supported |

### L1 height + font sizing

L1 has a target visual height of **56 logical points** (≈ 9.5 % of screen height on a 6.1" portrait phone; absolute pixel value scales with screen height).

| Element | Min legible size | Target size | Notes |
|---|---|---|---|
| Timer (default `SS` format) | 24 pt | 36 pt | Final size by UI team; legibility floor binding |
| Timer (`S.t` final-2 s, scaled 1.4×) | 33.6 pt (= 24 × 1.4) | 50.4 pt | Scale ramp anchored on default size |
| Banner copy | 16 pt | 20 pt | Must fit on one line on the narrowest supported screen (iPhone 8 / 750 px wide); copy character-count budget = 50 chars (locked copy = 47 chars including period — within budget) |

### Safe-area handling

L1 is positioned by `safe_area_top_inset_px + safe_area_padding`, where:

- `safe_area_top_inset_px` — platform-reported safe-area inset top (UIKit `safeAreaInsets.top` on iOS; `WindowInsets.systemBars().top` on Android)
- `safe_area_padding = 4 logical px` — constant; provides visual breathing room between OS chrome and HUD content

**L1 reflow triggers** — the HUD MUST reflow L1 when any of the following fires:

1. Interface rotation event (game is portrait-locked but rotation-lock-failure mode must still leave L1 anchored correctly)
2. `safeAreaInsetsDidChange` / `WindowInsets` change
3. **iOS Dynamic Island state change** (live activity / music playback / call indicator — Dynamic Island can resize and reposition dynamically during a run; HUD must subscribe and reflow within 1 frame of the change)
4. App foreground (after background-restore — safe-area may have changed during background)

**L1 reflow is a single-frame snap. No tween.** Mid-run Dynamic Island expansion is already visually jarring (it is the OS doing its thing); the HUD adding an animated reflow on top would compound the disruption.

### Input handling

HUD elements are **display-only during RUNNING** — no interactive elements:

- Timer: read-only display
- Banner: read-only display
- NONE-tier haptic visuals: read-only display
- Pause indicator: read-only display

Pause input (manual pause-by-tap) is owned by IS, not HUD. IS dispatches the pause event → RSM → fires `OnPausedChanged → true` → triggers the HUD pause overlay.

No gamepad support, no keyboard support, no mouse/cursor (per `technical-preferences.md` — touch-only mobile).

### Performance constraint

HUD must contribute ≤ **0.5 ms** to frame time on a mid-tier mobile target (16.6 ms total budget per `technical-preferences.md`). Implications:

- **Timer**: cache the glyph atlas; only re-render when display value changes (most 60 Hz ticks, the integer second is unchanged). During the final-2 s decimal regime, value changes ~10×/sec (one tenth per 100 ms) — still well under per-frame re-render.
- **Banner**: static once rendered; render priority above gameplay but no per-frame update cost.
- **Pause overlay**: rendered once on entrance, dismissed on exit; no per-frame update during pause.
- **NONE-tier haptic visuals**: event-driven; per-event GPU cost specified in `design/ux/input-feedback.md`.

The 0.5 ms budget covers **HUD-owned elements only** (timer, banner, pause overlay). NONE-tier haptic-fallback visual GPU cost is budgeted separately in `design/ux/input-feedback.md` (the HUD hosts these at top render priority but does not pay their cost from its own budget — they are accessibility-substitute renderings owed by IS).

The 0.5 ms budget is provisional; final budget set in `performance-budget.md` (future doc).

### Visual budget

| Budget axis | Limit | Worst-case observed |
|---|---|---|
| **Max simultaneous HUD elements** | 4 (cap) | 4 — timer + banner + pause overlay + 1 NONE-tier haptic visual coexisting (e.g., player backgrounds the app on a NONE-tier device with the banner already active and a haptic-substitute visual mid-animation) |
| **Max screen coverage (% of viewport)** | 60 % (cap) | ~50 % — L1 (~10 %) + pause overlay over most of L2 (~30 %) + zone-edge / dead-band NONE-tier visuals at peripheral edges (~10 %) |
| **Max ACTIVE-during-RUNNING elements** | 2 (cap) | 2 — timer + banner (the pause overlay and NONE-tier visuals are either suspended-input states or event-driven flashes; they don't compete for player attention during continuous play) |

The Max ACTIVE-during-RUNNING cap is the load-bearing one — it enforces the §HUD Philosophy "Minimal but present" intent. A third permanent on-screen element during continuous RUNNING would violate philosophy and require an explicit §HUD Philosophy amendment.

### Tuning knobs

**No HUD-owned tuning knobs.** The HUD exposes no player-adjustable settings of its own. All player-adjustable behavior affecting the HUD is honored from upstream sources:

- **Reduced-motion alternative** — read from system Reduce Motion setting (iOS `UIAccessibility.isReduceMotionEnabled`; Android `Settings.Global.TRANSITION_ANIMATION_SCALE`)
- **`near_miss_haptic_enabled`** — referenced from the future `design/ux/accessibility-settings.md` UX spec (see §Open Questions FC-HUD-2)
- **Text-scale** — explicitly NOT honored for HUD (exempted per §Accessibility §System text-scale rationale); out-of-game text in other UX specs honors system text-scale per their own specs

If a future requirement introduces a HUD-owned tuning knob (e.g., timer-numeric-vs-bar toggle, banner-mute toggle), it must be added to this list with rationale + a §HUD Philosophy amendment check.

---

## Accessibility

### Tier

No `design/ux/accessibility-requirements.md` exists at this spec's authoring time. The HUD aims for **WCAG 2.1 AA** as the baseline — this is the standard already referenced by `input-system.md` §UI Requirements contrast specification and is the most defensible baseline for a commercial mobile release. Final tier commitment is flagged in §Open Questions.

### Contrast

All HUD text uses the IS-pattern combination to satisfy WCAG 1.4.3 (text contrast) against the shifting voxel background:

| Layer | Spec |
|---|---|
| Fill | `#FFFFFF` |
| Outline | 1.5 logical px `#000000` 100 % opacity |
| Drop shadow | Floor specified per `input-system.md` Visual Elements item 1 (HUD inherits the same floor) |

UI team verifies 4.5:1 contrast on all supported device brightness settings during implementation.

### Color-independent communication

The timer's final-2 s state uses **two redundant attribution channels** (per §HUD Elements Element 1):

- Numeric precision channel (`S.t` decimal format)
- Visual modulation channel (red color + 1.4× scale)

Players who cannot perceive the red color shift (deuteranopia / protanopia / monochrome vision) still receive the attribution via numeric precision AND scale. **No information is conveyed by color alone.**

Banner copy is text-only; no color-dependent information.

NONE-tier haptic-fallback visuals — color-independence per `design/ux/input-feedback.md`.

### Motion / animation

The HUD has four animations during a run:

- Timer entrance fade-in (150 ms) at COUNTDOWN→RUNNING
- Banner fade-in (300 ms) on watchdog engagement
- Timer color/scale ramp (2 s linear) in the final-2 s regime
- Pause overlay fade-in / fade-out (150 ms each)

**Reduced-motion alternative (binding)**: when the system "Reduce Motion" setting is enabled (iOS: `UIAccessibility.isReduceMotionEnabled`; Android: `Settings.Global.TRANSITION_ANIMATION_SCALE == 0`), all HUD animations become single-frame snaps:

- Timer entrance: instant
- Banner: instant
- Timer color/scale: instant snap at the 2 s threshold (no linear ramp)
- Pause overlay: instant

**The color/scale state ITSELF is preserved** (still red + scaled in final 2 s) — only the transition animation is removed. Pillar 5 attributability is preserved under reduced-motion.

### System text-scale (WCAG 1.4.4 Resize Text) — exempted with rationale

WCAG 2.1 AA criterion 1.4.4 requires text to scale to 200 % without loss of content or functionality. The HUD **does NOT honor iOS Dynamic Type or Android system font-scale** for the following reasons:

1. **Pillar 5 attribution depends on a designer-controlled scale ramp.** The timer's 1.0× → 1.4× final-2 s scale is a binding attribution channel (per §HUD Elements Element 1 + §Accessibility color-independent communication). System text-scale of 200 % would compound unpredictably with the ramp (2.0× × 1.4× = 2.8× — exceeds L1 height, breaks layout, distorts attribution).
2. **L1 layout breakage.** L1 has a fixed 56 logical-point height (§Platform & Input Variants). System text-scale of 200 % applied to a 36 pt timer (= 72 pt) exceeds L1 height and would force L1 to dynamically grow — disrupting the gameplay viewport (L2) mid-run.
3. **Gameplay-critical readability is designer-controlled, not user-controlled.** The HUD's legibility floors (24 pt timer, 16 pt banner per §Platform & Input Variants) already exceed WCAG 1.4.4's minimum-legibility intent. The minimum legible size is selected to be readable at default device settings without user scaling.

**Out-of-game text** (Main Menu, End-Run, Interrupted-Run, Accessibility Settings) will honor system text-scale; that obligation lives in their respective UX specs, not here. The HUD exemption is scoped to the in-RUNNING surface only.

Forward-contract: revisit at `/ux-review` and at `design/ux/accessibility-requirements.md` authoring time. If WCAG 2.1 AAA or accessibility certification (e.g., GVAP) is targeted later, a separate "accessibility timer" treatment may need to be designed (e.g., a corner-tucked high-contrast variant that honors system scale, swappable via Accessibility Settings).

### Screen reader considerations

Out of scope for a reflex-driven 60-second mobile runner. Screen reader users are not a viable audience for the core gameplay loop. If a future Accessibility Settings spec defines a screen-reader-compatible game mode, the HUD will need to expose ARIA-equivalent labels for the timer + banner — flagged in §Open Questions.

### `near_miss_haptic_enabled` reference

PM R11a-12 introduced an opt-in `near_miss_haptic_enabled` accessibility setting (default off). The HUD does NOT own this toggle UI — the toggle lives in the future Accessibility Settings UX spec. The HUD references the setting only insofar as the near-miss haptic event itself may be present (PM-owned haptic event; not a HUD render). Forward contract recorded in §Open Questions for the Accessibility Settings UX spec.

---

## Acceptance Criteria

All ACs are testable without reading any other design document. Each AC's pass condition is verifiable by a human QA tester or a UI automation test. Default gate level **BLOCKING** for `/story-done`.

### Core purpose — timer

- [ ] **AC-HUD-01 (Timer format)**: At `remaining_time = 42.0`, timer displays `42`. At `remaining_time = 2.5`, displays `02`. At `remaining_time = 1.9`, displays `1.9`. At `remaining_time = 0.5`, displays `0.5`. At `remaining_time = 0.0` (COMPLETE entry), displays `00` for exactly 1 frame, then dismisses.
- [ ] **AC-HUD-02 (Timer color/scale ramp — Pillar 5)**: At `remaining_time = 2.0` exactly, timer renders in default white at 1.0× scale. At `remaining_time = 1.0` (50 % through ramp), timer renders in color interpolated 50 % toward red AND at scale 1.2×. At `remaining_time = 0.0`, timer renders red at 1.4× scale. Color and scale change in lockstep — neither can fire without the other.
- [ ] **AC-HUD-03 (Timer visibility — state machine correctness)**: When `RSM.current_state ∈ {IDLE, COUNTDOWN, RESOLVING, ABORTED}`, timer is NOT rendered. When `RSM.current_state == RUNNING`, timer IS rendered. Visibility transitions occur on `OnStateChanged` events, not per-frame polling (verifiable by removing per-frame visibility check in implementation review).
- [ ] **AC-HUD-04 (Timer pause-freeze — RSM AC-09 coordination)**: When `OnPausedChanged → true` fires mid-run with `remaining_time = 35.7`, timer displays `35` and freezes. After 10 s of paused wall-clock time, still displays `35`. On `OnPausedChanged → false`, timer resumes from `35` and continues decrementing.

### Core purpose — banner

- [ ] **AC-HUD-05 (Banner copy + persistence — PM R11a-9 binding)**: When PM watchdog M=3 PEAK suppression engages for the first time in a run, banner appears with verbatim copy `"Performance mode — hardest barrage suppressed."`. Banner remains visible for the rest of the run regardless of subsequent watchdog state changes (appear-once-stick). Banner dismisses on `RSM RUNNING → {DEAD, COMPLETE, ABORTED}` exit. For DEAD specifically, banner remains visible through the DEAD freeze and dismisses on DEAD→RESOLVING.
- [ ] **AC-HUD-06 (Banner-timer reflow — single-frame snap)**: On the frame the banner first appears, the timer position changes from L1-center to L1-right in a single frame. No intermediate positions; no tween. The timer's display value continues uninterrupted.
- [ ] **AC-HUD-07 (Banner entrance animation timing)**: Banner fade-in completes within 300 ms ± 16 ms (1 frame at 60 Hz). Under reduced-motion mode (see AC-HUD-10), banner appears in a single frame instead.

### State coexistence

- [ ] **AC-HUD-08 (L1 reflow on safe-area change)**: When `safeAreaInsetsDidChange` fires (or iOS Dynamic Island state changes), L1's vertical position recalculates from `safe_area_top_inset_px + 4 logical px` within 1 frame. No tween. L1 contents (timer + banner) reflow to the new position in the same frame.
- [ ] **AC-HUD-09 (Pause overlay coexistence)**: When `RSM.is_paused == true`, pause overlay renders on L2 with semi-transparent backing. L1 (timer + banner if active) remains visible above L2 and is NOT obscured by the pause overlay. NONE-tier haptic-fallback visuals (if any are firing) render above the pause overlay.

### Accessibility

- [ ] **AC-HUD-10 (Reduced-motion compliance)**: With system Reduce Motion enabled (iOS `UIAccessibility.isReduceMotionEnabled == true`; Android `Settings.Global.TRANSITION_ANIMATION_SCALE == 0`), no HUD animation tweens fire. Timer entrance is instant. Banner entrance is instant. Timer color/scale change at the 2.0 s threshold is an instant snap. The end-state color (red) and end-state scale (1.4×) ARE STILL applied during the final 2 s — only the transition animation is removed. Pause overlay entrance/exit is instant.
- [ ] **AC-HUD-11 (Color-independent attribution)**: A player viewing the timer through a desaturation filter (simulating monochrome vision) at `remaining_time = 0.5` can distinguish the final-2 s state from the default state via the format change (`0.5` vs `42`) AND the scale change (1.4× vs 1.0×). Attribution does not depend on the red color alone — at least 2 of the 3 attribution channels (format, scale, color) must convey the state.

### Performance

- [ ] **AC-HUD-12 (Frame-budget compliance)**: HUD rendering contributes ≤ 0.5 ms to per-frame GPU time on the mid-tier mobile reference device (per `.claude/docs/technical-preferences.md`). Measured via per-frame profiler over a representative 60-second run with one watchdog engagement (banner appears mid-run). NONE-tier haptic-fallback visual cost is excluded from this measurement (budgeted separately in `input-feedback.md`).

---

## Open Questions

### Forward contracts (cross-spec dependencies)

| ID | Contract | Direction | Status |
|---|---|---|---|
| FC-HUD-1 | `"barrage"` vocabulary established player-facing before banner ships | HUD → Main Menu UX spec (future) | **BLOCKING (CONTINGENT)** per PM R11a-9 |
| FC-HUD-2 | `near_miss_haptic_enabled` accessibility toggle UI ownership | HUD → Accessibility Settings UX spec (future) | Advisory; HUD only references the setting (PM owns the event) |
| FC-HUD-3 | Watchdog state persistence model across runs | HUD ← PM Watchdog Spec (pending PM decomposition execution) | Advisory; affects §Dynamic Behaviors density-shift rule 4 wording |
| FC-HUD-4 | Screen-reader ARIA labels for timer + banner | HUD → Accessibility Settings UX spec (future) | Advisory; only required if a screen-reader game mode is defined |
| FC-HUD-5 | Performance-budget formalization | HUD → `performance-budget.md` (future) | Advisory; 0.5 ms is provisional pending the budget table |

### Cross-doc syncs needed

| ID | Sync | Action |
|---|---|---|
| SYNC-1 | `design/ux/input-feedback.md` open item 4 ("visual-indicator render priority vs In-Run HUD") | Edit `input-feedback.md` to mark item 4 RESOLVED with reference to `hud.md` §Layout Zones transient table + §HUD Elements Element 3 |
| SYNC-2 | `design/ux/input-feedback.md` DRAFT-status header | Per its header: audio-director + UX-designer sign-off pending. HUD authorship completes the UX-designer side; flag the audio-director side as remaining |
| SYNC-3 | `design/gdd/run-state-machine.md` §UI Requirements timer behavior | RSM §UI Requirements does not specify the SS↔S.t format switch + color/scale ramp; this HUD spec is the authoritative source. RSM cross-ref optional but recommended |
| SYNC-4 | `design/gdd/player-movement-presentation.md` R11a-9 banner copy | Verify verbatim match (no edit) on `/ux-review` |

### Design deferrals to art-director

| Item | Deferred | Required by |
|---|---|---|
| Timer final-2 s color swatch | `#FF3030` provisional; final red TBD | Implementation sprint |
| Banner color palette | Final palette TBD | Implementation sprint |
| Pause indicator design | Text `"Paused"` vs pause-glyph icon | Implementation sprint |
| Font family / weight / size for L1 | Final font within legibility floors (24 pt min for timer, 16 pt min for banner) | Implementation sprint |

### Design deferrals — open / TBD

| Item | Reason |
|---|---|
| Accessibility tier formal commitment | WCAG 2.1 AA is targeted but not formally committed — requires `design/ux/accessibility-requirements.md` to exist |
| Mid-run device-tier change handling (haptics revoked mid-run) | No hardware case currently requires it; revisit if hardware behavior changes |
| iPad / tablet layout variant | Out of scope per §Platform & Input Variants; revisit if iPad support is added |
| COUNTDOWN visual re-evaluation criterion | If first-run playtest reveals IS prompts alone do not convey "imminent start", re-examine §Information Architecture categorization of the COUNTDOWN visual |
| iOS Dynamic Island reflow behavior | Implementation detail; needs first-run testing on iPhone 14 Pro+ devices |

### Context-doc gaps surfaced during this spec's authoring

| Missing doc | Impact on this spec |
|---|---|
| `design/player-journey.md` | Designed without explicit player-journey mapping; assumed player emotional context (focused / reactive / time-pressured) without journey-map verification |
| `design/art/art-bible.md` | All visual choices (color swatches, font, pause overlay treatment) deferred to art-director without an art-bible reference |
| `design/ux/accessibility-requirements.md` | No formal tier committed; WCAG 2.1 AA assumed as defensible baseline |

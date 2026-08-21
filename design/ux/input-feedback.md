# Input Feedback Spec: Haptic Fallback (AUDIO / VISUAL / NONE Tiers)

> **Audio section owner:** audio-director — must approve before IS implementation sprint
> **Visual section owner:** ux-designer — must approve before IS implementation sprint
> **Last Updated:** 2026-05-31
> **Status:** DRAFT — escalated to producer (re-review 11, 2026-05-31). Audio-director + UX designer sign-off required. See Open Items.
> **Blocks:** Input System GDD (prerequisite unblocked by stub; full IS implementation blocked until sign-offs obtained)
>
> **PRODUCER ACTION REQUIRED:** This spec is an unsigned dependency of Input System. Assign to audio-director + UX designer for review before IS enters the implementation sprint. Open items: (1) WAV asset authorship, (2) DEAD_BAND_FEEDBACK_ENABLED default on AUDIO tier, (3) 50ms layering window confirmation, (4) visual indicator render priority vs. In-Run HUD.

---

## Overview

SLIPSTORM's Input System and Player Movement system each own haptic feedback for
their respective events. On Android devices where haptic capability is degraded,
platform-appropriate fallbacks replace vibration signals. This document specifies
the audio and visual fallbacks for all four haptic events across the three degraded
tiers.

**Tiers where this spec applies:**

| Tier | Condition | Fallback Mode |
|---|---|---|
| `AUDIO` | No vibration hardware | Audio cues replace all haptic signals |
| `VISUAL` | No vibration AND no audio output | Visual indicators replace all haptic signals |
| `NONE` | User-disabled OR accessibility override | All feedback suppressed **except** R-3 collision NONE-tier visual, ContactResting NONE-tier visual, and Dead-band NONE-tier visual (all three are Pillar 5 minimums; dead-band visual only fires when `DEAD_BAND_FEEDBACK_ENABLED = true`) |

**Tiers NOT covered here** (haptic hardware present):

| Tier | Condition | Specified In |
|---|---|---|
| `FULL` | Full amplitude + pattern control | IS GDD (R-3 collision, dead-band), PM GDD (slip-confirmed, buffer-drop) |
| `DURATION` | Duration-only vibration | IS GDD, PM GDD |

---

## Haptic Event Reference

| Event | System Owner | Trigger Condition | IS GDD Rule |
|---|---|---|---|
| **R-3 collision** | Input System | Two touches on opposite zones in same 60Hz tick — both discarded | R-3 |
| **Dead-band contact** | Input System | Touch within 8mm exclusion band (when `DEAD_BAND_FEEDBACK_ENABLED = true`) | R-1 |
| **Slip-confirmed** | Player Movement | Valid slip accepted: SETTLED → SLIPPING transition | PM GDD F-1 |
| **Buffer-drop** | Player Movement | Buffer full when second slip arrives | PM GDD Rule 5 |

**Ownership note:** IS fires R-3 and dead-band. PM fires slip-confirmed and buffer-drop. All four
events must have audio/visual fallback specs here because on AUDIO and VISUAL tiers, a shared
audio/visual dispatch layer is used regardless of originating system. The audio and visual
fallbacks are implementation concerns of the shared platform bridge, not of IS or PM individually.

---

## Audio Fallback Spec (AUDIO Tier)

> **Owner: audio-director** — parameters below are design proposals pending audio director approval.
> Sign-off required before IS Review 6.

**Design constraints (from IS and PM GDDs):**
- Dead-band must be perceptually below R-3 collision in salience (it is a sub-light signal)
- R-3 collision must be clearly distinct from slip-confirmed (different game meaning)
- Buffer-drop must be distinct from slip-confirmed (opposite valence — input not acted on)
- The four cues must be distinguishable at mobile speaker volume with background ambient noise
- Cues must not compete with SLIPSTORM's music/SFX track — attack must be sharp to pierce mixing

**Audio cue parameters:**

| Event | Pattern | Frequency (Hz) | Duration (ms) | Amplitude | Timbre | Axes of Distinction |
|---|---|---|---|---|---|---|
| Dead-band contact | Single click | 250 | 15 | 25% | Sine (clean) | Softest; shortest; lowest pitch |
| Slip-confirmed | Single pop | 700 | 35 | 60% | Slight transient click | Mid-pitch; clear and satisfying |
| Buffer-drop | Single pop | 450 | 25 | 50% | Slightly duller than slip-confirmed | Lower pitch than slip-confirmed; shorter |
| R-3 collision | Double click (30ms gap) | 300 | 15 + 15 | 55% | Sine (clean, rhythmic) | Only event with double-beat pattern |
| Contact resting | Double click (60ms gap) — matches full-tier haptic double-pulse semantics | ~400 | 15 + 15 | 35% | Sine (clean) | Double-beat distinguishes from all single-beat events; lower amplitude than R-3 collision; longer gap (60ms vs 30ms) than R-3 to convey "hold expired" not "collision" | **[PENDING audio-director sign-off]** |

**Ordering by salience (low → high):**
`Dead-band < Buffer-drop < Slip-confirmed < R-3 collision`

**Axes of distinction summary:**

| Axis | Dead-band | Buffer-drop | Slip-confirmed | R-3 collision |
|---|---|---|---|---|
| Pattern | Single | Single | Single | **Double** |
| Frequency | **250Hz** (lowest) | 450Hz | **700Hz** (highest) | 300Hz |
| Duration | **15ms** (shortest) | 25ms | **35ms** (longest single) | 30ms total |
| Amplitude | **25%** (softest) | 50% | 60% | 55% |

**Implementation notes:**
- All cues use PCM synthesis or short WAV assets — no procedural audio required
- Cues must not duck the music track — they sit above the mix as transients
- The double-beat pattern of R-3 collision is the primary distinguisher from all three single-beat events
- If the audio director adjusts frequency values: maintain the salience ordering in the table above — `Dead-band < Buffer-drop < Slip-confirmed < R-3 collision`. The frequency axis must not invert this ordering (lower frequency = more neutral/passive).

**Audio Director sign-off block:**
```
Approved by: ____________________  Date: ________
Notes: ____________________________________________
```

---

## Visual Fallback Spec (VISUAL Tier)

> **Owner: ux-designer** — parameters below are design proposals pending UX designer approval.
> Sign-off required before IS Review 6.

**Design constraints (from IS and PM GDDs):**
- Visual indicators must not obscure gameplay (voxel track, hazards, player)
- Indicators must be brief enough to not be distracting at run pace
- Four patterns must be distinguishable without color alone (accessibility)
- Dead-band indicator must not look like a zone activation (must not imply a game verb)
- R-3 collision indicator must look like an error/rejection (both zones implicated)

**Visual pattern parameters:**

| Event | Location | Shape | Color | Opacity | Duration | Shape Variant |
|---|---|---|---|---|---|---|
| Dead-band contact | Center vertical strip (band width) | Vertical bar flash | White (#FFFFFF) | 15% → 0% | 80ms fade | Width = band width (8mm); height = 30% of screen height centered |
| Slip-confirmed | Full left or right edge (slipped side) | Edge glow | Cyan (#00E5FF) | 40% → 0% | 100ms single smooth fade-out | 4px edge glow along full zone edge |
| Buffer-drop | Full left or right edge (attempted slip side) | Edge double-pulse | Amber (#FFA000) | 30% → 0% | 80ms total: 30ms on → 20ms off → 30ms on | Same shape as slip-confirmed; amber instead of cyan; double-pulse distinguishes rejection from acceptance |
| R-3 collision | Both zone edges simultaneously | Dual edge glow | Red (#F44336) | 45% → 0% | 120ms fade | Both left and right edges; slightly wider (6px) |
| Contact resting | Left or right zone edge (zone of resting contact) | Zone-edge brief desaturation flash | Gray (#808080) | 20% → 0% | 50ms fade | Single-sided (unlike R-3 collision which is both-sided); gray (neutral, no game-verb implication); brief 50ms duration (Pillar 4: non-persistent during RUNNING); distinguishable from slip-confirmed (different color + duration) and R-3 collision (single-sided, shorter) | **[PENDING ux-designer sign-off]** |

**Distinguishing without color (for monochrome accessibility):**

| Event | Shape alone | Duration alone |
|---|---|---|
| Dead-band | Center strip (unique shape) | Short |
| Slip-confirmed | One-sided edge glow (single smooth fade) | Medium |
| Buffer-drop | One-sided edge glow (**double-pulse**) | Short |
| R-3 collision | Both-sided edge glow (unique coverage) | Longest |

R-3 collision is uniquely identifiable by shape alone (both edges). Dead-band is uniquely
identifiable by location alone (center, not edge). Slip-confirmed vs. buffer-drop are
distinguished by animation pattern (single smooth fade vs. double-pulse) and color —
the double-pulse pattern is the primary distinguisher and works in monochrome.

**Render layer:** All indicators render above the gameplay layer but below the HUD.
They must not trigger any camera reaction or gameplay event. Pure cosmetic overlay.

**UX Designer sign-off block:**
```
Approved by: ____________________  Date: ________
Notes: ____________________________________________
```

---

## NONE-Tier Visual: R-3 Collision Only

The `NONE` tier suppresses all feedback except one: the R-3 same-frame collision
zone-edge desaturation pulse. This is the minimum Pillar 5 signal for the NONE tier —
without it, simultaneous taps disappear silently and the player has no indication
that both were discarded.

**Spec:**

| Property | Value |
|---|---|
| Trigger | R-3 collision (same-frame opposite-zone simultaneous taps) |
| Location | Both zone edges (same coverage as VISUAL-tier R-3) |
| Effect | Brief desaturation pulse: zone edges briefly flash gray |
| Color | #9E9E9E (neutral gray — not "error red", to minimize game-feel intrusion) |
| Opacity | 35% → 0% |
| Duration | 150ms fade |
| Suppressed when | RSM state is `DEAD`, `RESOLVING`, `ABORTED`, or `COUNTDOWN` (IS reads RSM for this gate only) |

**Difference from VISUAL-tier R-3 collision:** The NONE-tier version uses gray
(desaturation) instead of red to signal "input received but cancelled" rather than
"error". The VISUAL-tier version uses red because on VISUAL tier the user has audio
context to separate the events by sound. On NONE tier, all feedback is visual-only,
so the gray desaturation is chosen to be interpretable without the prior context.

**UX Designer sign-off:** NONE-tier R-3 spec is covered by the visual section sign-off above.

---

## NONE-Tier Visual: ContactResting Hold-Deactivation Signal

On NONE-tier devices, the 180ms hold-deactivation signal (ContactResting) must have a
visual fallback. Without it, a player holding their finger too long receives no feedback
that their finger has been deactivated — a direct Pillar 5 violation. This signal was
added in IS re-review 11 (2026-05-31).

**Spec:**

| Property | Value |
|---|---|
| Trigger | Cancel timer fires (finger held > `CANCEL_TIMER_MS = 180ms`) on NONE-tier |
| Event fired | `EVisualEvent::ContactRestingLeft` or `EVisualEvent::ContactRestingRight` — IS selects based on contact's originating zone; overlay implements each as a zone-targeted indicator |
| Location | Full width of the zone in which the contact originated (LEFT or RIGHT half) |
| Effect | 50ms transient flash; self-clears after display — no persistent state |
| Color | #808080 (mid gray) |
| Peak opacity | 20% at peak; fades to 0% over 50ms |
| Clear trigger | None required — flash is transient and self-clears; `IVisualDispatch::Clear()` must NOT be called for this event |
| Suppressed when | RSM state is `DEAD`, `RESOLVING`, or `ABORTED` (same gate as ContactResting haptic; effectively suppressed during COUNTDOWN via cancel-timer guard — no new contact can hold long enough to time out in COUNTDOWN, and IS-21-A1 tracking reset removes pre-positioned contacts at COUNTDOWN→RUNNING) |

**Pillar 4 / Pillar 5 balance:** A 50ms transient flash is below the perceptual persistence threshold during an active run (Pillar 4 compliance). Transient motion signals are detected via the luminance-change pathway, which is luminance-independent — the flash remains perceptible across the full voxel luminance range without relying on contrast ratio (Pillar 5 delivery).

**Distinguishing from NONE-tier R-3 desaturation:**
- R-3 pulse covers BOTH zone edges simultaneously (both halves); fades fully to 0%
- ContactResting covers only the ONE zone where the held finger is located; 50ms transient flash then fully gone
- Players learn: both-sides flash = two-thumb cancel; one-side brief flash = my finger timed out and is deactivated

**UX Designer sign-off required:** This section was added in re-review 11; updated re-review 12 (persistent tint, zone-specific enum variants); updated re-review 23 (reclassified persistent tint → 50ms transient flash, color #BDBDBD→#808080, removed contrast-floor requirement, removed Clear trigger). Requires separate sign-off from the visual section above.

```
Approved by: ____________________  Date: ________
Notes: ____________________________________________
```

---

## NONE-Tier Visual: Dead-Band Contact Signal

On NONE-tier devices, `IHapticDispatch::Fire(EHapticEvent::DeadBandContact)` is a no-op.
Without a visual fallback, a player tapping the 8mm center exclusion band receives
zero feedback — a Pillar 5 violation during onboarding when the player is still learning
zone boundaries. A minimal center-strip flash provides the required signal.

This fallback fires when `DEAD_BAND_FEEDBACK_ENABLED = true` (default) on NONE-tier.
When `DEAD_BAND_FEEDBACK_ENABLED = false`, no visual fires on any tier for dead-band
contacts.

**Spec:**

| Property | Value |
|---|---|
| Trigger | Dead-band contact (touch within 8mm exclusion band) on NONE-tier with `DEAD_BAND_FEEDBACK_ENABLED = true` |
| Location | Center vertical strip (dead-band width = 8mm, full strip height) |
| Effect | Brief center-strip flash: same shape as VISUAL-tier dead-band indicator |
| Color | #BDBDBD (light gray; sub-light signal) |
| Opacity | 20% → 0% |
| Duration | 60ms fade |
| Contrast floor | At 20% opacity, #BDBDBD over a mid-gray voxel background (~0.50 luminance) yields approximately 1.8:1 — acceptable for a sub-light signal that supplements haptic feedback, not a standalone WCAG requirement. Against pure white or very bright backgrounds (luminance > 0.75), contrast falls below 1.2:1 — accepted trade-off. Verify visibility on all target voxel variants at minimum brightness. |
| Suppressed when | `DEAD_BAND_FEEDBACK_ENABLED = false`; RSM state is `DEAD`, `RESOLVING`, or `ABORTED` |

**Distinguishing from VISUAL-tier dead-band:** NONE-tier uses lower opacity (20% vs 15% [transient peak])
and shorter duration (60ms vs 80ms) — the NONE-tier version is intentionally more subtle while
still legible against typical voxel backgrounds.
The center-strip location uniquely identifies it as a dead-band event regardless of tier.

**NONE-tier dead-band vs. NONE-tier R-3 desaturation:** Both fire on the center region,
but R-3 desaturation covers the zone edges bilaterally while dead-band flash covers the
center strip. The two are spatially distinct.

**UX Designer sign-off required:**

```
Approved by: ____________________  Date: ________
Notes: ____________________________________________
```

---

## Palm/Stylus Rejection Signal (InputRejected Event)

The IS fires `EHapticEvent::InputRejected` when a contact fails the palm rejection
radius filter (`r_mm > R_max`). This is a sub-light signal in the same amplitude
class as the dead-band signal.

| Tier | Signal |
|---|---|
| `FULL` / `DURATION` | Sub-light haptic pulse via `IHapticDispatch::Fire(EHapticEvent::InputRejected)` |
| `VISUAL` | Micro-flash via `IVisualDispatch` — see spec below |
| `AUDIO` | No signal — palm contacts are typically unnoticed by the player |
| `NONE` | No signal |

**VISUAL-tier micro-flash spec:**

| Property | Value |
|---|---|
| Location | Center dead-band vertical strip (same position as dead-band contact indicator) |
| Shape | Same vertical bar as dead-band indicator |
| Color | Warm gray (#BDBDBD) — distinct from dead-band white to signal "rejected input" not "center contact" |
| Opacity | 10% → 0% |
| Duration | 50ms fade |

The micro-flash is intentionally below the salience of all four primary events. It
signals "something was blocked" without implying a game mechanic.

**UX Designer sign-off:** Covered by the visual section sign-off above.

---

## Haptic Layering Policy

**Layering rule:** If two haptic events trigger within the same 50ms window, only the
highest-salience event plays. Ordering: R-3 collision > Slip-confirmed > Buffer-drop > Dead-band.

**Rationale:** At SLIPSTORM's tempo, rapid tap sequences can trigger simultaneous events
(e.g., valid slip dispatched + dead-band feedback from a second finger in the exclusion band).
Without a layering policy, overlapping audio/visual cues are confusing. Only the most
salient event is presented per 50ms window; the lower-salience event is silently dropped.

**Implementation note:** This policy applies to AUDIO and VISUAL tiers only. On FULL and
DURATION tiers, haptic hardware can overlap signals (multiple actuations) — PM and IS fire
independently. The layering policy is a software gate in the shared audio/visual dispatch layer.

---

## Integration Requirements

### For the AUDIO Tier Implementation

1. Assets: four WAV/PCM assets at the specified parameters, loaded at IS and PM init
2. Playback: the shared platform haptic bridge checks `HAPTIC_CAPABILITY == AUDIO` before playing
3. Mixing: audio cues are played as UI audio (not spatialized; same volume regardless of player position)
4. No ducking required: the cues are transients; they sit above the music without requiring a duck

### For the VISUAL Tier Implementation

1. Overlay: a transparent fullscreen overlay widget renders all four indicator types
2. The overlay is always active (rendered every frame) but invisible when no indicator is showing
3. Fade-out is linear; easing may be added by the UX designer at sign-off
4. The overlay must be a separate draw call from gameplay — do not modify any existing material

### Test Seam

Audio and visual dispatch must be observable in debug builds:
- `IS_HAPTIC_FIRED` log event (defined in IS GDD) already logs `capability_tier` — use this for AUDIO/VISUAL verification
- AC-18 and AC-19 use this log for automated test coverage
- Manual playtest required for amplitude calibration and visual salience on target devices

---

## Open Items (requires owner input before closing)

1. **Audio cue assets:** Who authors the WAV assets? Should these be sourced from a sound library, synthesized procedurally, or authored by the audio director directly?
2. **DEAD_BAND_FEEDBACK_ENABLED default for AUDIO tier:** **RESOLVED (2026-06-01, re-review 19):** The authoritative default is `off` on AUDIO tier. The GDD (`design/gdd/input-system.md` Tuning Knobs) is the source of truth; this open item is closed. Audio-director sign-off on the audio cue design (Open Item 1) remains required before IS enters the implementation sprint.
3. **Layering window (50ms):** The 50ms layering window is a design proposal. Audio director should confirm whether a shorter or longer window is appropriate given the 600ms telegraph window (canonical: game-concept.md Pillar 2 — "at least 0.6s before contact").
4. **Visual indicator render priority:** Confirm that the overlay widget renders below the in-run HUD (score, timer) but above gameplay geometry. The In-Run HUD GDD (not yet authored) may override this.

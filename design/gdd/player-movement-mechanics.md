# Player Movement — Mechanics

**Status**: Draft (post-decomposition skeleton; bodies filled in Step 2 of decomposition execution)
**Date**: 2026-06-28
**Decomposed from**: `design/gdd/player-movement.md` (1823-line monolith, archived to redirect notice 2026-06-28)
**Decomposition plan**: `design/gdd/reviews/player-movement-decomposition-plan-2026-06-16.md`
**Scope**: PM's mechanical contract — formulas, state machine, edge cases, tuning knobs governing core motion. The "math + state" sub-GDD.
**Primary owners**: game-designer, systems-designer
**Sibling sub-GDDs**: `player-movement-presentation.md`, `player-movement-platform.md`
**Pre-decomposition review history**: `design/gdd/reviews/player-movement-review-log.md` (R1 through R11, marked closed at decomposition)
**Sub-GDD review log**: `design/gdd/reviews/player-movement-mechanics-review-log.md`
**Inherited R11 BLOCKING items (per decomposition plan §5)**: B-F6-1, B-F6-2, **B-F6-3 (caller side) CLOSED at Step 2 PASS 7 2026-06-29** (F-2 pseudo-code local clamp retired; F-6 comment rewritten; cross-sub-GDD invariant note in §4 F-2 prose updated to "B-F6-3 CLOSED"; definition site lives at `player-movement-platform.md` §4 F-PROLOGUE), B-F6-4 — remaining items to be addressed at R12a per `design/gdd/reviews/player-movement-mechanics-r12-author-brief-2026-06-28.md`
**Cross-sub-GDD forward contracts**: see §6 Dependencies — 7 mechanics↔presentation + 8 mechanics↔platform contracts authored per decomposition plan §4 (Step 3 execution 2026-07-01 closed plan §4.1 + §4.2 canonical coverage; additional-beyond-plan item 5 in each side retained from prior PASSes)

> **R10a binding decisions inherited from PM monolith (2026-06-14 — in-session author revision pass per brief `design/gdd/reviews/player-movement-r10-author-brief-2026-06-11.md` + B-LEAN-2026-06-12-1 fold-in; this sub-GDD inherits only the R10a decisions affecting its scope per decomposition plan §3.1)**:
> (R10a-1) **§5.1 F-6 timer-collision = (a) Override** — F-6 timer killed on SETTLED→SLIPPING; F-5 takes over cleanly. F-6 fully re-specified with triple-output spec (body/head/arm) closing the pre-R10a head/arm decouple defect. 4 new ACs (AC-F6-A/B/C/D) + EDGE_ABSORB_CURVE_ASSET added.
> (R10a-4) **§B-LEAN-tension SLIP_TWEEN range = Hold [0.10, 0.15]s per brief §6.1** — survivability arithmetic now permits widening to ~0.225s under MIN_ESCAPE_SLIPS=2, but safe range held on Player Fantasy + camera/shader/thermal grounds. Pre-R10a "above 0.15s violates the invariant immediately" claim RETRACTED. Frame-quantized verification table regenerated against MIN_ESCAPE_SLIPS=2 — all four supported framerates (60/50/30/20) now hold with 117–150 ms margin (previously 30 fps + 50 fps failed under M=3). (Post-R10a footnote: R10d 2026-06-17 raised platform-side TELEGRAPH_WINDOW_FLOOR_S from 0.65→0.70s; SLIP_TWEEN safe range unchanged; survivability margins re-derived at R10d's new FLOOR in `player-movement-platform.md` §4 F-BARRAGE-SURVIVABILITY-INVARIANT-CORNERS.)
> (R10a-7) **§5.6 Rule 7 vs Public Interface contradiction = (i) Update Public Interface row text** — `lateral_world_position` row acknowledges fractional-when-SETTLED carve-out under Rule 7 DEAD freeze. Folded with §2.7 Option (c) track-space semantics.
> (R10a-8) **§5.7 LANE_WIDTH "400cm preserved" prose = (ii) Rewrite the prose** — total track span scales with LANE_WIDTH_M × 4 (3.40–6.00 m across safe range). LANE_WIDTH_M safe-range floor justification rewritten on perceptual readability grounds, NOT thumb-target (R9 category-error finding).
> (R10a-11) **§2.1 + §2.2 AC bundle** — AC-34 sample set extended to {0.0, 0.1, 0.5, 0.99}; AC-34b NEW for buffer-flush execution tick; AC-14 boundary samples at TweenProgress 0.001 / 0.999; new "Counter Persistence" AC section (AC-COUNTER-DEAD / -COMPLETE / -PAUSE-RESUME).
> (R10a-12) **§2.6 Delegate Handler Bodies NEW** — HandleStateChanged + HandlePausedChanged bodies fully specified as authoritative pseudo-code under Cross-Component Interfaces. Closes R9 "unimplementable: method bodies unspecified" finding.

> **R11a binding decisions inherited from PM monolith (2026-06-15 — in-session author revision pass per brief `design/gdd/reviews/player-movement-r11-author-brief-2026-06-15.md`; this sub-GDD inherits only the R11a decisions affecting its scope per decomposition plan §3.1)**:
> (R11a-1) **§1 Cluster A — Internal Contradiction** — F-2 prologue rewritten to persistent clamp with `effective_slip_tween` local + `bSlipTweenClampActive` public flag + `TickClampLogCounter` private rate-limit driver. AC-21 rewritten to assert the persistent-clamp semantic. Cross-references added between F-2, AC-21, AC-SS-A, and §1.2 enforcement table for lock-step revision discipline. Pre-R10a one-shot `bFloorGuardFired` semantic is RETIRED everywhere except retirement markers. **Documented brief deviation preserved (per decomposition plan §6 Step 2 line 230)**: F-2 log idiom retained from brief; if a future re-review surfaces an alternative log idiom, AC-21 + AC-SS-A + §1.2 enforcement table update in lockstep.
> (R11a-2) **§2 Cluster E — F-6 Implementation Pathway** — `HandleStateChanged` COMPLETE/ABORTED/COUNTDOWN/IDLE dispatch updated to reset `edge_absorb_active` + `edge_absorb_local_timer` (DEAD correctly preserves per Rule 7). New `Slip Input Dispatch — HandleSlipTransition` subsection authored under Cross-Component Interfaces with full method signature + call site + body pseudo-code + 5 private helper-method declarations. §5.1 (a) Override pseudo-code replaced with reference to declared method body. New AC-COUNTER-F6-RESET. Closes pre-R10a "phantom method" finding. **Documented brief deviation preserved (per decomposition plan §6 Step 2 line 230)**: RSM gate property-read pattern retained from brief over alternative direct-subsystem-call form; lock-step closure with §3 Detailed Rules RSM Storage Contract.
> (R11a-3) **§6.1 DR-F.1 = (a) 2-frame fade-out** — §5.1 (a) Override timer-collision behavior reconciled with LEAN_CURVE_ASSET anti-snap contract. F-6 contributions decay over 2 ticks (1.0 → 0.5 → 0.0 multiplier) via snapshot capture at Override fire + linear ramp in F-6 lean formula. 4 new private members (`f6_override_fadeout_ticks_remaining` + 3 snapshot floats). AC-F6-B rewritten to assert tick-by-tick fade behavior.
> (R11a-4) **§6.2 DR-F.2 = (a) Proportional F-6 cap during F-5 Phase 3** — F-6 contribution scaled by `(1.0 − TweenProgress × EC15_F6_DECAY_COEFFICIENT)` when `movement_state == SLIPPING`. Default coefficient 0.7 (tuned upward from brief baseline 0.5 to produce unambiguous clamp-avoidance margin at the EC-15 boundary). New AC-F6-E asserts no clamp engagement at TweenProgress=0.85 + edge_absorb_progress=0.10.

---

## 1. Overview

Player Movement (Slip) — Mechanics is the mechanical contract for the translation layer between the Input System's discrete `slip-left` and `slip-right` events and the player character's lateral lane position on the run track. The player occupies one of **five** fixed lanes — FarLeft, Left, Center, Right, FarRight (indices 0..4; Center = 2) — and a slip event moves them one lane in the direction of the input; slips at the track edge are no-ops. (R7-PM-PROPAGATION binding: 3-lane model widened to 5 lanes per Pull-Wave R7 Rule 2 NUM_LANES=5 forward contract.) This sub-GDD owns the state machine, formulas (F-1 through F-6), authored asset contracts, and the mechanics-side public interface: `current_lane`, `target_lane`, `lateral_world_position`, `movement_state`, `lean_angle`, `head_lean_angle`, `arm_lean_angle`, `TweenProgress`, the diagnostic counters, and `bSlipTweenClampActive`. The mechanics sub-GDD gates all input against the current RSM state (`RUNNING`, not paused, not in resume grace), translates valid events into lane transitions, buffers rapid same-direction inputs, and locks position on DEAD or COMPLETE entry. On each SETTLED→SLIPPING transition, PM commits the player's collision geometry to the destination lane immediately, before any translation arc begins; the perception layer (visual commitment-tell flash + audio cue + haptic) is owned by `player-movement-presentation.md`. The hardware contract, watchdog, and Shipping-safety enforcement that gate this mechanics surface are owned by `player-movement-platform.md`. Downstream, the mechanics interface is what Pull-Wave targeting, Collision hit volumes, and Camera framing all depend on. The player never thinks about lane indices. They think about the pull-wave that's about to reach them, the half-second window they were given, and which way to lean. At 5 lanes, reading the wave is reading a direction — not counting steps.

## 2. Player Fantasy

The slip is not a button. It's a bend. A pull-wave leans toward you — iron deciding which lane you're in — and before the decision lands, your character is already in the next lane. The body moves before the mind finishes the sentence. You are watching the one soft thing in a world of pulling iron, and the slip is the moment it refuses to be pulled. When the wave slams into the space your character just left, that is the fantasy: *I read it, and they were already gone.*

The player's relationship to the avatar is observational and proprioceptive at once — they watch the voxel character bend, but they also *are* the bending. The avatar's lane transition must read as a lean: the body committing to a direction before the move completes. This is not a teleport. It is not a swipe. The animation team's brief is a single word: *bend*.

**What this framing demands of implementation:**
- The tween must be long enough to express the arc of a body committing to direction (see §7 Tuning Knobs — `SLIP_TWEEN_DURATION_S`).
- The avatar's animation during the tween must communicate lean-and-arrive, not snap-and-appear.
- When a slip is a no-op (at the track edge), the avatar should absorb the input — a small lean that doesn't complete — rather than produce no response at all. The body tried. The world stopped it.
- Near-misses should have a readable visual beat: the wave passing close confirms the read was right, the margin was thin, the skill was real. (Visual + audio + opt-in haptic owned by `player-movement-presentation.md`; the mechanics trigger contract is the `TriggerNearMissBeat()` call site authored by Pull-Wave.)
- **5-lane decision texture (R7-PM-PROPAGATION-REVIEW)**: at 5 lanes with M=3 PEAK barrages, the player sometimes faces a worst-case "no neutral safe lane" configuration where one direction reaches safety in one slip and the other reaches safety in two. The fantasy still works at this complexity because the player's read is *which way to lean*, not *which lane index to count to*. The skill visible under Pillar 5 is direction-anticipation, not position-indexing — the body bends toward the read.

## 3. Detailed Rules

### Core Rules

1. **Lane model (R7-PM-PROPAGATION — widened from 3 to 5 lanes per Pull-Wave R7 Rule 2 NUM_LANES=5 forward contract).** The player occupies one of **five fixed lanes** named by position: **FarLeft (index 0), Left (1), Center (2), Right (3), FarRight (4)**. Each lane maps to a fixed world X position at `lane_world_x_cm(lane_index) = (lane_index − 2) × LANE_WIDTH_CM`, where `LANE_WIDTH_CM = LANE_WIDTH_M × 100` (default `LANE_WIDTH_M = 1.0 m → LANE_WIDTH_CM = 100`). At default tuning (`LANE_WIDTH_M = 1.0 m`), lane centers sit at world-X positions −200 cm (FarLeft), −100 cm (Left), 0 cm (Center), +100 cm (Right), +200 cm (FarRight) — total track span 400 cm = 4.0 m at this tuning, coincidentally matching the prior 3-lane span at the default but with finer-grained intermediate lane positions. **R10a §2.4 correction**: total track span scales with `LANE_WIDTH_M × 4` and is NOT preserved across the safe range — the safe range [0.85, 1.5] m yields span 3.40 – 6.00 m. See §7 Tuning Knobs for the span table. `EPlayerLane` enum widens from prior `{Left, Center, Right}` to `{FarLeft, Left, Center, Right, FarRight}` (5 members; ordinal values 0..4 explicit). A `slip-left` event moves the player one lane left (decrement index by 1); `slip-right` moves one lane right (increment index by 1). A slip that would leave the track is a **no-op**: PM stays SETTLED, `current_lane` is unchanged, and the avatar plays the edge-absorb animation (a lean that doesn't complete). **The edge-absorb animation is driven by F-6 with `edge_absorb_active = true` and `edge_absorb_sign` set from the dropped input direction** (R10a §1.1 — see §4 Formulas → F-6, path (2) Rule 1 SETTLED). Rule 1 is one of two entry paths into F-6; the other (path (1)) is F-4's mid-tween buffer-discard edge no-op. PM does NOT discard the event silently — the animation response is required. Each Rule 1 edge-absorb trigger increments `edge_absorb_trigger_count` by 1 (same counter used by F-4 buffer pre-validation discards — the counter is the union of both paths). **Track-edge identity (R7-PM-PROPAGATION)**: at 5 lanes, the edge no-op condition is `(current_lane == FarLeft AND input == slip-left)` OR `(current_lane == FarRight AND input == slip-right)`. Slips from FarLeft to Left, Left to FarLeft, Right to FarRight, FarRight to Right are all valid 1-lane transitions (no edge no-op).

2. **Tween is non-interruptible; collision commits on SETTLED→SLIPPING.** Once a slip begins executing, it runs to completion. A new `slip-left` or `slip-right` event received while a tween is in progress goes into the single-slot buffer (Rule 3); it does not interrupt or redirect the active tween. **On the SETTLED→SLIPPING transition frame:** (a) the player's collision geometry immediately commits to `lane_world_x(target_lane)`; (b) the commitment-tell perception signal fires — the `LeadingFaceFlash` material parameter is set to ±0.80 and begins its 2-frame hold + 50ms decay (visual contract owned by `player-movement-presentation.md`; the mechanics responsibility is to fire the trigger at this exact moment). Fires for direct inputs and buffer-flush alike. Both happen before any tween progress has been accumulated. The visual mesh continues to interpolate via F-3 (`lateral_world_position`). Collision and visual positions are decoupled during the tween; they converge at TweenProgress = 1.0. The implementation mechanism (root capsule at target lane with mesh offset, or separate collision volume tracked independently) is resolved in the OQ-1 ADR. Invariant: collision responds to the player's input commitment, not to the visual position.

3. **Single-slot input buffer.** While a tween is executing, the first subsequent `slip-left` or `slip-right` event fills the single-slot buffer (depth 1). A second event while the buffer is full is **dropped** (first queued wins); when a buffer drop occurs, PM fires a distinct haptic pulse through the Input System's haptic interface **and** a sub-80ms "click-blocked" audio sting (presentation contract — see `player-movement-presentation.md` Audio § Buffer-drop cue). The buffer is pre-validated at fill time: if the buffered input would be an edge no-op given `target_lane` (the in-flight tween's destination), it is discarded immediately and the edge-absorb animation plays. Opposite-direction inputs are accepted into the buffer and executed when the active tween completes. **If a drop occurs while the buffer is already full** (including a drop that would have been a track-edge no-op), no edge-absorb animation fires — the buffer-drop haptic and audio sting are the only feedback. The existing queued input is unchanged. This is intentional: the buffer is the firewall; edge-absorb feedback is subordinate to buffer state.

4. **Buffer flush at tween completion (R7-PM-PROPAGATION — source-lane semantic during SLIPPING explicitly affirmed as BINDING on PM).** When a tween completes: `current_lane = target_lane`; `TweenProgress` is reset to `0.0`. PM becomes SETTLED. If the buffer holds a slip, PM immediately begins executing it (no idle frame required), subject to Rules 1–3. **Source-lane semantic invariant (BINDING per Pull-Wave R7 Rule 11 forward contract on PM)**: the reassignment `current_lane = target_lane` happens EXCLUSIVELY at this point — the tween-completion frame. Throughout the SLIPPING state (any tick where `TweenProgress ∈ (0.0, 1.0)`), `current_lane` MUST return the SOURCE lane (the lane the player slipped from). This single invariant IS the entire interface that Pull-Wave's near-miss detection consumes (Pull-Wave R7 Rule 11: "PM Rules 4 + 7 guarantee `current_lane` returns the SOURCE lane throughout the SLIPPING state — reassignment to `target_lane` at SLIPPING → SETTLED completion only"). Any future PM revision that changes the reassignment timing (e.g., reassigning `current_lane` at tween midpoint or at SLIPPING entry) breaks Pull-Wave's near-miss detection — a cross-system regression that MUST be flagged for Pull-Wave coordination via `/propagate-design-change` before implementation lands.

5. **RSM gating.** PM executes slip inputs only when `RSM.current_state == RUNNING`, `RSM.is_paused == false`, and `RSM.resume_grace == false`. PM subscribes to `OnStateChanged` and `OnPausedChanged`; it reads `resume_grace` each tick. Inputs received while any gate condition fails are discarded (not buffered).

6. **Pause mid-tween.** If `is_paused` becomes `true` while a tween is in progress, PM freezes `TweenProgress` at its current value. The buffer is preserved. On foreground resume, the game enters `resume_grace`; PM remains frozen during grace (`resume_grace == true` gates tween progress). At grace expiry, the tween resumes from its frozen `TweenProgress` value (picking up the arc, not restarting). If the buffer holds a slip at grace expiry, it fires immediately after the resumed tween completes.

7. **Terminal state — DEAD (R7-PM-PROPAGATION-REVIEW — `movement_state = SETTLED` assignment added; `TweenProgress` deliberately preserved at fractional value for visual freeze).** On `RSM.current_state == DEAD`: PM freezes in place at the current fractional tween position. `TweenProgress` is **preserved** at its current fractional value (NOT reset to 0.0 — unlike Rules 8/9 COMPLETE/ABORTED which snap visual to destination; DEAD freeze must hold the mid-arc visual pose for Death Replay). `current_lane` is set to the in-flight destination (or the current settled lane if not tweening). **`movement_state` is set to `ERunSlipState::SETTLED` (R7-PM-PROPAGATION-REVIEW NEW)**. Rationale: the SLIPPING flag must not persist across DEAD freeze because in-flight Pull-Wave LANDED ticks that fire during run-termination drain read `PM.movement_state` and `PM.current_lane`; with `movement_state` still SLIPPING after `current_lane = target_lane` reassignment, Pull-Wave Rule 11 source-lane semantic would return the wrong value (target lane masquerading as source lane). R7-PM-PROPAGATION-REVIEW closes this cross-system defect by transitioning PM out of SLIPPING at DEAD entry while preserving the mid-arc visual frozen pose. Once PM is SETTLED, subsequent Pull-Wave LANDED reads see `movement_state == SETTLED` and `current_lane == target_lane` — Pull-Wave Rule 10/11 must treat post-DEAD reads as terminal (the wave's hit/near-miss decision should already have completed before DEAD fired; any post-DEAD LANDED is a run-termination drain artifact). After this rule's `movement_state = SETTLED` assignment, F-2 / F-3 / F-5 / F-6 no longer advance (DEAD gates them via RSM check). `lateral_world_position` is the actual visual mesh position at freeze — which may be between lanes (the last F-3 output before DEAD). Collision geometry is at `lane_world_x(target_lane)` (committed at tween start per Rule 2), not at the fractional visual position. The buffer is discarded. No further slip inputs are accepted. PM's `OnStateChanged` handler must be registered before Death Replay's handler so that `current_lane`, `movement_state`, and `lateral_world_position` are frozen and stable when Death Replay reads them from the broadcast. If an edge-absorb or near-miss beat animation is in progress at DEAD entry, it freezes immediately at its current pose — body rotation and mesh transforms hold their mid-animation values. See §5 EC-14.

8. **Terminal state — COMPLETE.** On `RSM.current_state == COMPLETE`: any in-flight tween snaps instantly to `target_lane`. `lateral_world_position` resolves to `lane_world_x(target_lane)`. `TweenProgress` is reset to `0.0`. Body lean angle resets to 0° in the same frame (`lean_angle = head_lean_angle = arm_lean_angle = 0.0`; `SetRelativeRotation(FRotator::ZeroRotator)` called on mesh component). The buffer is discarded. PM transitions to `ERunSlipState::SETTLED` in the same frame. No further slip inputs are accepted.

9. **Terminal state — ABORTED.** Identical to COMPLETE: snap to destination (`lateral_world_position = lane_world_x(target_lane)`), `TweenProgress = 0.0`, lean reset to 0° (`lean_angle = head_lean_angle = arm_lean_angle = 0.0`; `SetRelativeRotation(FRotator::ZeroRotator)` called), PM transitions to SETTLED, buffer discarded, no further inputs accepted.

10. **Lane reset on COUNTDOWN.** On `IDLE → COUNTDOWN` transition (via `OnStateChanged`): `current_lane` and `target_lane` are both reset to Center (lane index 2 — R7-PM-PROPAGATION: Center remains the canonical track-X origin under the 5-lane widening). Any in-flight tween is cancelled. `TweenProgress = 0.0`. The buffer is cleared. `slip_complete_count` and `edge_absorb_trigger_count` both reset to 0.

11. **Buffer discarded at any non-RUNNING state entry.** Whenever `RSM.current_state` leaves RUNNING (to any state), the buffer is cleared. This subsumes the terminal-state and COUNTDOWN rules above for the buffer.

### Mechanics-Driven Tells

These two perception signals are immediate consequences of mechanics state transitions; the mechanics layer owns both the trigger and the underlying animation contract. (Visual/audio rendering details are split — final material/cue specs are catalogued in `player-movement-presentation.md` per the Audio-Visual Ownership Split table there.)

#### Lane-Settle Tell

When a tween completes (SLIPPING→SETTLED), at the destination lane:

- **Visual:** Lane-arrival dust puff (see `player-movement-presentation.md` § VFX). 4–6 voxel-square sprites at ground contact, alpha-fade over 80ms.
- **Audio:** Slip whoosh cue ends naturally (120–180ms total; no separate landing sound — see `player-movement-presentation.md` Audio table).
- **Owned by:** Mechanics triggers (transition is a mechanics event); presentation renders the dust puff + plays the cue end.

#### Edge-Absorb Tell

When a slip input is a no-op (PM stays SETTLED at the track edge):

- **Visual:** Edge-absorb animation (partial lean, damped return — driven by F-6 path (2) Rule 1 SETTLED; see §4 Formulas → F-6). No flash. No UI response.
- **Audio:** Edge-absorb audio cue (damped whoosh variant, dull thud at hold — see `player-movement-presentation.md` Audio table).
- **Owned by:** Mechanics entirely (transition + F-6 timer); no commitment-tell fires (PM never left SETTLED).

### States and Transitions

PM has two movement states, orthogonal to RSM state:

| PM State | Description | Accepts |
|---|---|---|
| **SETTLED** | Player is stationary in `current_lane` | `slip-left`, `slip-right` (if RSM gating passes) → SLIPPING |
| **SLIPPING** | Tween in progress toward `target_lane` | `slip-left`/`slip-right` → buffer or drop |

`is_paused` and `resume_grace` are orthogonal flags read from RSM each tick — they are not PM states. They gate input acceptance and freeze tween progress but do not add new PM state nodes.

Tween completion always transitions SLIPPING → SETTLED (in `target_lane`). If the buffer holds a slip at that moment, PM immediately re-enters SLIPPING for the buffered event.

### Public Interface (Mechanics-Side)

Mechanics owns the lane / tween / lean / counter surface. The platform-owned watchdog broadcast surface (`is_hw_performance_degraded` + `OnHardwarePerformanceBreach`) lives in `player-movement-platform.md` §3 Public Interface.

| Property / Method | Type | Description |
|---|---|---|
| `current_lane` | `EPlayerLane` (R7-PM-PROPAGATION — widened to `{FarLeft, Left, Center, Right, FarRight}` = indices 0..4) | Lane the player is settled in (or snapped to on terminal entry). **Source-lane semantic during SLIPPING (BINDING per Pull-Wave R7 Rule 11 forward contract; preserved by PM Rules 4 + 7)**: throughout the SLIPPING state, `current_lane` returns the SOURCE lane (the lane the player slipped from); reassignment to `target_lane` happens only at SLIPPING → SETTLED completion (PM Rule 4). Pull-Wave's near-miss detection reads `current_lane` directly during SLIPPING expecting source-lane semantic; revising this would force Pull-Wave Rule 11 to be re-derived. |
| `target_lane` | `EPlayerLane` (5-member widened — R7-PM-PROPAGATION) | Destination lane of the active tween; equals `current_lane` when SETTLED |
| `lateral_world_position` | `float` (cm) | Actual lateral position of the player avatar — interpolated during tween, lane-anchored when SETTLED **except when entered via Rule 7 DEAD freeze** (R10a §2.3 carve-out — Rule 7 leaves TweenProgress at its fractional value while transitioning movement_state to SETTLED for the source-lane-semantic exit; under this entry path `lateral_world_position` may be a between-lane fractional value while SETTLED — this is the only SETTLED state with a non-lane-anchored position, by design, so Death Replay can read the mid-arc visual pose). **Reference frame (R10a §2.7 clarification under Option (c) stationary-player / moving-world model — recommended in Forward Motion Dual-Writer)**: under Option (c), the player actor is stationary in *world* space and the track + hazards move toward the camera. In that frame, `lateral_world_position` describes the avatar's lateral position in *track space* (the reference frame in which lane centers are defined), NOT world space — the property name is a legacy of the pre-Option-(c) authoring and is retained for downstream interface stability. The OQ-2 ADR is the authoritative resolution of the semantic; consumers (Pull-Wave targeting, Camera framing, Collision geometry) MUST read this property as track-space when Option (c) is selected. If a non-(c) option is selected by the ADR, the property reverts to actor world X. |
| `tween_progress` | `float` [0.0, 1.0] | Current tween completion fraction; 0.0 when SETTLED |
| `movement_state` | `ERunSlipState` (SETTLED/SLIPPING; pinned `UENUM(uint8)` ordinals `SETTLED=0, SLIPPING=1` — R7-PM-PROPAGATION-REVIEW; see Movement State Enum subsection below) | Current PM state. At RSM DEAD entry, set to SETTLED per Rule 7 (R7-PM-PROPAGATION-REVIEW) even though TweenProgress is preserved at its fractional value. |
| `lean_angle` | `float` (degrees) | Current body lean angle. **Co-written by F-5 and F-6 per the R10a §1.1 F-5 / F-6 co-write contract** (see §4 Formulas → F-6 → "F-5 / F-6 co-write contract" table for all four state combos). 0.0 when SETTLED **and** F-6 tail is inactive; non-zero during SLIPPING (F-5 contribution) and during F-6 tail (F-6 contribution, optionally summed with F-5 during EC-15 mid-tween edge-absorb). Positive = left lean, negative = right lean. Final value is clamped to `±(MAX_LEAN_ANGLE_DEG × 1.2)`. |
| `head_lean_angle` | `float` (degrees) | Head lean angle. **Co-written by F-5 and F-6 per the R10a §1.1 F-5 / F-6 co-write contract.** Lags the body's `lean_angle` by `HEAD_LAG_PROGRESS` in both F-5 and F-6 sampling (head reads earlier progress samples of both curves). 0.0 when SETTLED **and** F-6 tail is inactive — pre-R10a this row claimed "0.0 when SETTLED" unconditionally, which broke during F-6 SETTLED-tail phase (head decoupled from body). R10a §1.1 closes the defect: head stays aligned with body throughout the F-6 tail. Final value clamped to `±(MAX_LEAN_ANGLE_DEG × 1.2)`. |
| `arm_lean_angle` | `float` (degrees) | Leading arm/shoulder lean angle. **Co-written by F-5 and F-6 per the R10a §1.1 F-5 / F-6 co-write contract.** Leads the body's `lean_angle` by `ARM_LEAD_PROGRESS` in both F-5 and F-6 sampling (arm reads later progress samples of both curves). 0.0 when SETTLED **and** F-6 tail is inactive — pre-R10a unconditional-zero claim corrected per the head_lean_angle note. R10a §1.1 closes the same defect for arm: arm stays aligned with body throughout the F-6 tail. Final value clamped to `±(MAX_LEAN_ANGLE_DEG × 1.2)`. |
| `has_queued_input` | `bool` | True if the single-slot buffer holds a pending slip |
| `queued_input_direction` | `ESlipDirection` (Left/Right/None) | Direction of the buffered slip; None when `has_queued_input == false` |
| `collision_world_x` | `float` (cm) | World X position of the collision geometry, updated synchronously each frame. Equals `lane_world_x(target_lane)` while SLIPPING; `lane_world_x(current_lane)` when SETTLED. Diagnostic-read-only. |
| `slip_complete_count` | `int32` | Increments each time a tween completes naturally (SETTLED→SLIPPING→SETTLED). Does NOT increment on DEAD freeze, COMPLETE snap, or ABORTED snap. Reset to 0 on COUNTDOWN. |
| `edge_absorb_trigger_count` | `int32` | Increments each time the edge-absorb animation triggers (Rule 1 and F-4 paths combined). Reset to 0 on COUNTDOWN. |
| `commitment_tell_fire_count` | `int32` | Increments each time the commitment-tell fires (every SETTLED→SLIPPING transition). Reset to 0 on COUNTDOWN. |
| `OnSlipMidpoint` | `DECLARE_MULTICAST_DELEGATE_TwoParams(FOnSlipMidpoint, EPlayerLane /*source*/, EPlayerLane /*target*/)` (5-member widened — R7-PM-PROPAGATION) | Fires once per tween when TweenProgress first crosses 0.5. Does not fire for edge-absorb or during DEAD/COMPLETE/ABORTED. **R7-PM-PROPAGATION — no longer required by Pull-Wave**: Pull-Wave R1 RC-A superseded the `OnSlipMidpoint`-based near-miss detection with a direct read of `PM.current_lane` during SLIPPING (works because PM Rules 4 + 7 guarantee source-lane semantic — see `current_lane` row above). PM retains the delegate at its own discretion; no other documented consumer. If a future PM revision removes the delegate, no downstream consumer is broken. |
| `TriggerNearMissBeat()` | `void` | Called by Pull-Wave System when a qualifying near-miss is detected. PM forwards to the presentation layer (`player-movement-presentation.md` § Near-Miss Beat) which plays the avatar animation + audio swell + optional haptic. Ignored if PM is DEAD/COMPLETE/ABORTED. |
| `bSlipTweenClampActive` (R11a §1 NEW) | `bool` (public read-only) | True on any tick where F-2's `effective_slip_tween` was clamped to the safe-range floor (0.10) or ceiling (0.15) because `SLIP_TWEEN_DURATION_S` drifted outside [0.10, 0.15]. False when the configured value is within safe range. Visible to telemetry for runtime config-drift detection; replaces the retired one-shot `bFloorGuardFired` flag with persistent per-tick state. Backed by a private `TickClampLogCounter` (int32) that rate-limits the F-2 error log to once per 600 ticks (~10s at 60fps) — `TickClampLogCounter` is internal to F-2 and not part of the public contract. See §4 F-2 prologue and `player-movement-platform.md` § Shipping-Safety Enforcement Policy. |

### Cross-Component Interfaces

This section formalizes the contracts between PM (mechanics) and every system it exchanges data with at the mechanical layer. These are the authoritative interface definitions — downstream GDDs must reference these, not PM's internal implementation. Platform-layer interface surfaces (Tick Ordering, Hardware Contract, Shipping-Safety policy, watchdog broadcast row of the Cross-System Interface Table) are owned by `player-movement-platform.md` §3 Detailed Rules; presentation-layer interface surfaces (perception signal triggers) are owned by `player-movement-presentation.md` §3 Detailed Rules.

#### RSM Storage Contract

PM holds a cached pointer to RSM, injected at `BeginPlay` (not retrieved via singleton or `GetSubsystem` each tick):

```cpp
// BeginPlay: resolve RSM reference once
// If OQ-7 ADR selects UActorComponent RSM:
RSMComponent = GetOwner()->FindComponentByClass<URunStateMachineComponent>();
// If OQ-7 ADR selects UWorldSubsystem or UGameInstanceSubsystem:
// RSMComponent = GetWorld()->GetSubsystem<URunStateMachineSubsystem>();
// (Update this block when OQ-7 is finalized)

if (!IsValid(RSMComponent)) {
    UE_LOG(LogPlayerMovement, Error, TEXT("PlayerLaneMovementComponent: RSM not found — disabling component"));
    PrimaryComponentTick.bCanEverTick = false;
    return;
}
```

`checkf()` is NOT used here — it is a no-op in Shipping builds (`DO_CHECK = 0`). The runtime `IsValid()` guard is the only Shipping-safe path. Per-tick reads (`current_state`, `is_paused`, `resume_grace`) are direct property reads on the GameThread. PM takes no independent thread-safety responsibility for these reads — RSM OQ-7 ADR must confirm that `ApplicationWillEnterBackgroundDelegate` (which fires from the Android event thread) marshals all RSM state writes to the GameThread before PM's tick executes.

#### Rotation Implementation

F-5 drives body lean via rotation. Implementation constraint:

- **Use `SetRelativeRotation()`** on the skeletal/voxel mesh component (or lean socket). This rotates the visual mesh only.
- **Do NOT use `SetActorRotation()`** for lean — it rotates the root actor, which moves the collision capsule with it and breaks the collision-commitment contract (Rule 2).
- Lean is applied to the mesh component's local transform. Collision geometry position is set independently via actor root movement (OQ-1 ADR mechanism).

#### Delegate Binding Contract

All `OnStateChanged` / `OnPausedChanged` subscriptions:

```cpp
// BeginPlay:
StateChangedHandle  = RSMComponent->OnStateChanged.AddUObject(this, &UPlayerLaneMovementComponent::HandleStateChanged);
PausedChangedHandle = RSMComponent->OnPausedChanged.AddUObject(this, &UPlayerLaneMovementComponent::HandlePausedChanged);

// EndPlay:
if (IsValid(RSMComponent)) {
    RSMComponent->OnStateChanged.Remove(StateChangedHandle);
    RSMComponent->OnPausedChanged.Remove(PausedChangedHandle);
}
```

`AddRaw` and lambda bindings are prohibited. Store both handles as member variables. Guard `IsValid(RSMComponent)` in `EndPlay` — PIE shutdown fires `EndPlay` in arbitrary order.

#### Delegate Handler Bodies (R10a §2.6 NEW — closes R9 "unimplementable: method bodies unspecified" finding)

PM's two `HandleStateChanged` / `HandlePausedChanged` handlers are bound via the Delegate Binding Contract above. The R7-PM-PROPAGATION-REVIEW pass surfaced the handler signatures but left the bodies prose-only ("dispatches to Rule 7 on DEAD" etc.). R9 flagged the implementations as unimplementable without explicit dispatch tables. This subsection specifies both bodies as authoritative pseudo-code.

##### `HandleStateChanged(ERSMState OldState, ERSMState NewState)`

Dispatches PM's response to every RSM state transition. Called synchronously from RSM's broadcast when `OnStateChanged` fires. PM's handler MUST be registered before Death Replay's handler (see Death Replay Registration Order — counters and lane snapshot must be stable before Death Replay reads them).

```cpp
void UPlayerLaneMovementComponent::HandleStateChanged(ERSMState OldState, ERSMState NewState)
{
    // Rule 11: buffer is discarded at any non-RUNNING state entry.
    // Subsumes Rules 7/8/9/10 buffer-discard semantics.
    if (NewState != ERSMState::RUNNING) {
        ClearInputBuffer();   // sets has_queued_input = false; queued_input_direction = None
    }

    switch (NewState) {
        case ERSMState::COUNTDOWN:
            // Rule 10: lane reset. IDLE→COUNTDOWN is the normal trigger; EC-13
            // (RUNNING→COUNTDOWN) is architecturally impossible but defended here.
            CancelInFlightTween();                  // TweenProgress = 0.0
            current_lane = EPlayerLane::Center;     // index 2 — track-X origin
            target_lane  = EPlayerLane::Center;
            lateral_world_position = 0.0f;
            ResetLeanOutputs();                     // lean_angle/head/arm = 0
            edge_absorb_active      = false;        // R11a §2 — F-6 terminal table parity (Reset semantics on terminal states)
            edge_absorb_local_timer = 0.0f;
            // R11a §6.1 — fade-out state reset alongside edge_absorb_active (same rationale as COMPLETE/ABORTED).
            f6_override_fadeout_ticks_remaining = 0;
            f6_override_fadeout_snapshot_body   = 0.0f;
            f6_override_fadeout_snapshot_head   = 0.0f;
            f6_override_fadeout_snapshot_arm    = 0.0f;
            slip_complete_count       = 0;
            edge_absorb_trigger_count = 0;
            commitment_tell_fire_count = 0;
            // movement_state already SETTLED via CancelInFlightTween
            break;

        case ERSMState::DEAD:
            // Rule 7: freeze in place at mid-arc; preserve TweenProgress fractional;
            // exit SLIPPING (movement_state = SETTLED per R7-PM-PROPAGATION-REVIEW);
            // commit current_lane to in-flight target_lane; lean outputs frozen at
            // their current values (no ease, no snap); buffer already cleared above.
            if (movement_state == ERunSlipState::SLIPPING) {
                current_lane = target_lane;         // in-flight destination per Rule 7
                // TweenProgress deliberately PRESERVED at fractional — see AC-14.
            }
            movement_state = ERunSlipState::SETTLED;
            // lean_angle / head_lean_angle / arm_lean_angle are NOT zeroed
            // (Rule 7: freeze pose for Death Replay). Tick gating via RSM check
            // prevents F-5/F-6 advance after DEAD.
            break;

        case ERSMState::COMPLETE:
        case ERSMState::ABORTED:
            // Rules 8 / 9: snap to target. Identical bodies.
            lateral_world_position = LaneWorldX(target_lane);
            current_lane           = target_lane;
            TweenProgress          = 0.0f;
            ResetLeanOutputs();                     // lean_angle/head/arm = 0
            edge_absorb_active      = false;        // R11a §2 — F-6 terminal table parity (without this, mid-tail F-6 leaks past run-end and violates Rules 8/9 lean=0 invariant on the next run's SETTLED frames)
            edge_absorb_local_timer = 0.0f;
            // R11a §6.1 — fade-out state must reset alongside edge_absorb_active.
            // If Override fired just before COMPLETE/ABORTED, fade-out is mid-progress
            // (ticks_remaining > 0, non-zero snapshots) and would emit phantom lean on
            // the next run's first frame.
            f6_override_fadeout_ticks_remaining = 0;
            f6_override_fadeout_snapshot_body   = 0.0f;
            f6_override_fadeout_snapshot_head   = 0.0f;
            f6_override_fadeout_snapshot_arm    = 0.0f;
            movement_state = ERunSlipState::SETTLED;
            // Mesh SetRelativeRotation(FRotator::ZeroRotator) called per Rule 8/9 prose.
            SetMeshRelativeRotationToZero();
            // counter values UNCHANGED per AC-COUNTER-COMPLETE — do NOT reset here.
            break;

        case ERSMState::RUNNING:
            // No PM-side action on RUNNING entry. Tween (if any) resumes naturally
            // via per-tick RSM gating. Buffer state preserved from before only if
            // OldState == IDLE → COUNTDOWN → RUNNING (impossible at PM level —
            // COUNTDOWN cleared buffer in the prior transition).
            break;

        case ERSMState::IDLE:
            // Defensive reset matching COUNTDOWN — IDLE entry from any state must
            // leave PM in a clean state. Rare path: post-COMPLETE / post-ABORTED
            // return to menu. Same body as COUNTDOWN case minus the counter resets
            // (counters are run-scoped and persist into the menu state for telemetry
            // until the next COUNTDOWN starts a new run).
            CancelInFlightTween();
            current_lane = EPlayerLane::Center;
            target_lane  = EPlayerLane::Center;
            lateral_world_position = 0.0f;
            ResetLeanOutputs();
            edge_absorb_active      = false;        // R11a §2 — F-6 terminal table parity with COUNTDOWN
            edge_absorb_local_timer = 0.0f;
            // R11a §6.1 — fade-out state reset (same parity rationale as COUNTDOWN).
            f6_override_fadeout_ticks_remaining = 0;
            f6_override_fadeout_snapshot_body   = 0.0f;
            f6_override_fadeout_snapshot_head   = 0.0f;
            f6_override_fadeout_snapshot_arm    = 0.0f;
            SetMeshRelativeRotationToZero();
            // counters preserved — IDLE is between-run, not new-run
            break;

        default:
            // Unknown state — Shipping-safe path per platform-side Shipping-Safety
            // Enforcement Policy (defensive default: log error, leave PM untouched).
            UE_LOG(LogPlayerMovement, Error,
                   TEXT("HandleStateChanged: unknown ERSMState ordinal %u"), (uint8)NewState);
            break;
    }
}
```

**Notes:**
- `ResetLeanOutputs()` sets all three lean outputs (`lean_angle`, `head_lean_angle`, `arm_lean_angle`) to 0.0 in one call.
- `CancelInFlightTween()` sets `movement_state = SETTLED`, `TweenProgress = 0.0`, and stops F-2 / F-5 advance for the current tick.
- `ClearInputBuffer()` sets `has_queued_input = false` and `queued_input_direction = ESlipDirection::None`.
- `LaneWorldX(lane)` is the F-1 lookup helper.
- `SetMeshRelativeRotationToZero()` calls `MeshComponent->SetRelativeRotation(FRotator::ZeroRotator)` per Rotation Implementation contract.
- `ERSMState` is the run-state-machine.md enum; PM imports it. The ordinal pinning contract is owned by RSM's GDD.

##### `HandlePausedChanged(bool bNewPaused)`

Dispatches PM's pause-mid-tween behavior per Rule 6. Pause and resume are orthogonal to RSM state — `is_paused` may toggle within RUNNING state.

```cpp
void UPlayerLaneMovementComponent::HandlePausedChanged(bool bNewPaused)
{
    // Rule 6: pause freezes TweenProgress at its current value; buffer is
    // preserved; on resume_grace expiry, tween resumes from frozen value.
    //
    // The handler itself does NOT freeze TweenProgress — per-tick reads of
    // RSM.is_paused gate the F-2 progress accumulator (see F-2 prose:
    // "Progress is NOT advanced when is_paused == true or resume_grace == true").
    // The handler exists for two purposes:
    //   (1) future-extension hook for pause-entry / pause-exit side effects
    //       (e.g., audio bus mute, animation freeze on non-PM mesh components);
    //   (2) explicit logging for telemetry / debug observability.
    //
    // No state mutation in the body itself — F-2 is the source of truth for
    // the frozen TweenProgress invariant.

    if (bNewPaused) {
        // Pause entry. F-2 will halt next tick via is_paused gate.
        // Buffer preserved per Rule 6.
        UE_LOG(LogPlayerMovement, Verbose,
               TEXT("Pause entered. TweenProgress=%.3f, has_queued_input=%d"),
               TweenProgress, has_queued_input ? 1 : 0);
    } else {
        // Pause exit. resume_grace gate (per-tick) keeps tween frozen until
        // grace expires; PM resumes naturally at grace expiry.
        UE_LOG(LogPlayerMovement, Verbose,
               TEXT("Pause released. resume_grace will gate tween until expiry."));
    }

    // Counter values are UNCHANGED across pause / grace per AC-COUNTER-PAUSE-RESUME.
}
```

**Notes:**
- The pause/resume mechanism is intentionally split: the **handler logs**, the **per-tick gate freezes**. This split keeps the freeze deterministic (always per-tick, never event-ordering-dependent) and the handler future-extensible (additional side effects can be added without re-deriving the freeze invariant).
- The handler MUST NOT mutate `TweenProgress`, `current_lane`, `target_lane`, or counter values. Any state change belongs in F-2 (gated by per-tick `is_paused` / `resume_grace`) or in Rule 7/8/9/10 (gated by `HandleStateChanged`).
- If a pause occurs mid-tween while a buffered input is queued, the queued input is preserved (Rule 6 + Rule 11 only discards buffer on non-RUNNING state entries; pause inside RUNNING does not discard).

#### Slip Input Dispatch — `HandleSlipTransition` (R11a §2 NEW — closes Cluster E.2 "phantom method" finding)

The §4 Formulas → F-6 → "§5.1 (a) Override — timer-collision on new SLIPPING transition" decision references a method named `HandleSlipTransition` as the SETTLED→SLIPPING entry path. Pre-R11a the method was named in the Override pseudo-code but never declared, signed, or specified — the binding §5.1 (a) decision sat on a phantom method. This subsection declares the method authoritatively.

##### Method signature

```cpp
void UPlayerLaneMovementComponent::HandleSlipTransition(ESlipDirection InDirection);
```

Private member of `UPlayerLaneMovementComponent`. Called once per accepted slip input event.

##### Call site

Invoked from PM's input-event handler (the binding mechanism resolved by OQ-3 / Input System OQ-1; currently unbound at the Input System ↔ PM seam, but PM's input-event handler will route here). Each `slip-left` or `slip-right` event from Input System dispatches to exactly one call. `cancel-slip` is NOT routed here (removed from PM contract per Cross-System Interface Table).

##### Body specification (authoritative pseudo-code)

```cpp
void UPlayerLaneMovementComponent::HandleSlipTransition(ESlipDirection InDirection)
{
    // RSM gating (Rule 5) — read RSM state via per-tick property reads per the
    // Cross-System Interface Table (RSM → PM: current_state, is_paused,
    // resume_grace). Method-call form in the R11a brief verbatim is reconciled
    // here to property-read form to match the interface contract; see the
    // R11a §2 brief-deviation note at the end of this subsection.
    if (RSMComponent->current_state != ERSMState::RUNNING ||
        RSMComponent->is_paused ||
        RSMComponent->resume_grace) {
        return;  // event discarded; not buffered (per Rule 5)
    }

    // SETTLED path: begin tween or fire edge-absorb
    if (movement_state == ERunSlipState::SETTLED) {
        const EPlayerLane intended_target = ComputeTargetLane(current_lane, InDirection);
        if (intended_target == current_lane) {
            // Rule 1 edge no-op
            FireEdgeAbsorb(InDirection);
            return;
        }
        // §5.1 (a) Override — transition F-6 tail into 2-frame fade-out if active
        // (BINDING — locks the R10a §5.1 author decision; R11a §6.1 reconciliation
        // applies a 2-frame fade-out instead of instant zero to satisfy the
        // LEAN_CURVE_ASSET anti-snap contract; see §4 Formulas → F-6 "§5.1 (a)
        // Override — timer-collision on new SLIPPING transition" for the decision
        // record, fade-out semantics, and lock-step canary discipline).
        if (edge_absorb_active) {
            // Snapshot the current F-6 output values BEFORE zeroing edge_absorb_*.
            // F-6 will continue to contribute the snapshot scaled by a linear ramp
            // (1.0 → 0.5 → 0.0) over the next 2 ticks.
            f6_override_fadeout_snapshot_body = f6_lean_body;
            f6_override_fadeout_snapshot_head = f6_lean_head;
            f6_override_fadeout_snapshot_arm  = f6_lean_arm;
            f6_override_fadeout_ticks_remaining = 2;

            edge_absorb_active      = false;
            edge_absorb_local_timer = 0.0f;
            edge_absorb_sign        = 0.0f;
        }
        target_lane    = intended_target;
        TweenProgress  = 0.0f;
        movement_state = ERunSlipState::SLIPPING;
        CommitCollisionToTargetLane();    // Rule 2
        FireCommitmentTell();              // Rule 2 + commitment_tell_fire_count++
        return;
    }

    // SLIPPING path: buffer (Rule 3) or pre-validate edge (F-4)
    if (movement_state == ERunSlipState::SLIPPING) {
        if (has_queued_input) {
            // Buffer full — drop with haptic + audio (Rule 3)
            FireBufferDropHapticAndAudio();
            return;
        }
        const EPlayerLane projected_target = ComputeTargetLane(target_lane, InDirection);
        if (projected_target == target_lane) {
            // F-4 buffer pre-validation: edge no-op
            FireEdgeAbsorb(InDirection);
            return;
        }
        // Accept into buffer
        has_queued_input        = true;
        queued_input_direction  = InDirection;
    }
}
```

##### Helper-method declarations (private members)

The body references five helpers. All are private members of `UPlayerLaneMovementComponent`; signatures are declared here, bodies are specified by the rule / formula they implement:

| Helper | Signature | Implements | Side effects |
|---|---|---|---|
| `ComputeTargetLane` | `EPlayerLane ComputeTargetLane(EPlayerLane source, ESlipDirection direction) const` | F-1 + Rule 1 edge clamp | Pure function; returns `source` lane when the direction would step off-track (FarLeft + slip-left → FarLeft; FarRight + slip-right → FarRight); otherwise returns adjacent lane. |
| `FireEdgeAbsorb` | `void FireEdgeAbsorb(ESlipDirection direction)` | Rule 1 + F-4 edge no-op path; activates F-6 | Sets `edge_absorb_active = true`, `edge_absorb_local_timer = 0.0f`, `edge_absorb_sign = (direction == ESlipDirection::Left ? +1.0f : -1.0f)`. Increments `edge_absorb_trigger_count`. Fires edge-absorb audio / haptic per `player-movement-presentation.md` § Edge-Absorb Tell. |
| `CommitCollisionToTargetLane` | `void CommitCollisionToTargetLane()` | Rule 2 — synchronous collision commit | Sets `collision_world_x = LaneWorldX(target_lane)` on the same event-handler call (synchronous; no defer). Diagnostic-readable via Public Interface `collision_world_x`. |
| `FireCommitmentTell` | `void FireCommitmentTell()` | Rule 2 — commitment-tell + counter | Fires commitment-tell VFX/audio per `player-movement-presentation.md` § Commitment-Tell (presentation owns rendering + the 200ms cadence cap; mechanics owns the trigger contract + counter). Increments `commitment_tell_fire_count` exactly once per SETTLED→SLIPPING transition (regardless of whether the visual flash is suppressed by the cadence cap). |
| `FireBufferDropHapticAndAudio` | `void FireBufferDropHapticAndAudio()` | Rule 3 — buffer-drop feedback | Fires buffer-drop haptic + audio sting in the same event-handler call as the drop (synchronous per AC-25). Does NOT mutate buffer state, counter values, or movement state. |

##### Cross-references

- **§5.1 (a) Override decision**: see §4 Formulas → F-6 subsection "§5.1 (a) Override — timer-collision on new SLIPPING transition". The decision body now points HERE for the implementation; this method is the load-bearing call site.
- **AC-F6-B**: exercises the §5.1 (a) Override path via this dispatch — becomes implementable against this method body.
- **AC-19 / AC-23 / AC-24 / AC-25**: all assert behaviors triggered through this dispatch (buffer discard, collision commit, slip-midpoint delegate, buffer-drop feedback respectively). Implementations of those ACs read this body for the source-of-truth call ordering.
- **Rule 5 (RSM gating)**: the early return at top of the body IS the Rule 5 dispatch — event discarded, not buffered, when RSM is not RUNNING or is paused / in resume_grace.

##### Documented deviation from R11a brief verbatim (R11a §2 — API-surface reconciliation)

The R11a brief's HandleSlipTransition body specified the Rule 5 gate as `if (!RSMComponent->IsRunning() || RSMComponent->IsPaused() || RSMComponent->IsInResumeGrace()) return;` — method-call form. The Cross-System Interface Table below (RSM → PM row) documents RSM exposing `current_state`, `is_paused`, `resume_grace` as **per-tick property reads**, and F-2 prose reads them as properties (`is_paused == true`). The body above is reconciled to property-read form so the dispatch matches the documented RSM ↔ PM interface contract. Brief authority is preserved on intent (early return on non-RUNNING / paused / grace); only the read mechanism differs from method-call to property. Rationale logged here because Cluster A is the internal-contradiction cluster — landing the §2 fix with an unreconciled RSM API-surface mismatch would re-introduce the failure pattern §1 is closing.

#### Death Replay Registration Order

RSM fires `OnStateChanged` delegates in bind order. PM's `HandleStateChanged` must be registered **before** Death Replay's handler — `current_lane` and `lateral_world_position` must be frozen and stable when Death Replay reads them from the broadcast. The enforcement mechanism (explicit bind-order guarantee in a shared initialization sequence, or a `BeginPlay` execution order guarantee via Actor component priority) must be resolved in the PM or Death Replay ADR.

#### Cross-System Interface Table (Mechanics-Side)

The mechanics-side rows are listed here. The Wave Spawner row (hardware-gate forward contract under platform watchdog) and HUD row (banner forward contract under platform watchdog) are owned by `player-movement-platform.md` §3 Cross-System Interface Table.

| System | Direction | What flows | Contract |
|---|---|---|---|
| **Input System** | → PM | `slip-left`, `slip-right` | Fire-and-forget; IS has no knowledge of lane state. `cancel-slip` removed from PM contract. |
| **Run State Machine** | → PM | `current_state`, `is_paused`, `resume_grace` (per-tick reads); `OnStateChanged`, `OnPausedChanged` (delegates) | PM subscribes via `AddUObject` + stored handles. RSM OQ-7 ADR must confirm GameThread marshalling. |
| **Pull-Wave System** | PM → | `lateral_world_position` (wave targeting); `current_lane` + `movement_state` (read directly during SLIPPING by Pull-Wave Rule 11 near-miss detection at LANDED tick — R7-PM-PROPAGATION-REVIEW corrected from stale `OnSlipMidpoint` reference) | Pull-Wave GDD R7 specifies `TELEGRAPH_WINDOW_FLOOR_S = 0.65s` (raised from 0.6s at R2 Cluster E). PM Rules 4 + 7 guarantee source-lane semantic during SLIPPING (BINDING per Pull-Wave R7 Rule 11). |
| **Pull-Wave System** | → PM | `TriggerNearMissBeat()` | Pull-Wave calls after detecting qualifying near-miss. PM forwards to presentation layer for animation playback (see `player-movement-presentation.md` § Near-Miss Beat). |
| **Collision System** | PM → | Collision geometry at `lane_world_x(target_lane)` while SLIPPING; `lane_world_x(current_lane)` when SETTLED | Decoupled from `lateral_world_position` during tween. Implementation via OQ-1 ADR. |
| **Camera System** | PM → | `lateral_world_position` | Read each tick. |
| **Death Replay** | PM → | `current_lane` + `lateral_world_position` snapshot at DEAD entry | PM's handler registered first (see Death Replay Registration Order above). |

#### Movement State Enum — Pinned Ordinal Contract (R7-PM-PROPAGATION-REVIEW)

**BINDING — `ERunSlipState` ordinals are pinned for ABI safety:**

```cpp
UENUM(BlueprintType, meta = (ScriptName = "RunSlipState"))
enum class ERunSlipState : uint8
{
    SETTLED  = 0,
    SLIPPING = 1
};
```

The seam doc (`docs/architecture/platform-seam-interfaces.md` — Seam 12 `IPlayerMovementProvider`) declares `EMovementState` with matching members and pinned ordinals `{SETTLED = 0, SLIPPING = 1}`. The production implementation of `IPlayerMovementProvider::GetMovementState()` performs `static_cast<EMovementState>(PM->GetMovementState())`; this cast is ABI-safe only because both enums are `uint8` with matching pinned ordinals. **Adding members to either enum without updating the other and without bumping the seam contract version is forbidden** (see R7-PM-PROPAGATION-REVIEW Finding 4). PM Rules 5 (buffering as orthogonal flag, not state) and 7 (DEAD freeze sets `movement_state = SETTLED`, does not add a DEAD state) are the rationale for the 2-member surface.

#### Forward Motion Dual-Writer

`UPlayerLaneMovementComponent` manages lateral lane movement only. Forward motion is not owned by PM. Two components cannot both call `SetActorLocation` in the same tick without clobbering each other. Three candidate resolutions (OQ-2 ADR must select one before implementation):

- **(a)** Shared movement struct — PM writes X, forward-motion writes Z, a single `SetActorLocation` call integrates both per tick.
- **(b)** PM writes world X only; forward-motion is contractually prohibited from calling `SetActorLocation`.
- **(c) Recommended:** Stationary-player / moving-world — player actor is stationary in world space, track and hazards move toward the camera, PM writes world X only. Eliminates the conflict entirely; reduces forward-motion draw calls; preferred for mobile thermal budget.

Option (c) must be selected before OQ-1 is finalized.

## 4. Formulas

```cpp
DECLARE_LOG_CATEGORY_EXTERN(LogPlayerMovement, Log, All);
// In PlayerLaneMovementComponent.cpp:
DEFINE_LOG_CATEGORY(LogPlayerMovement);
```

### F-1 — Lane World Position

Maps a lane enum to a world X coordinate:

```
lane_world_x(lane) =
  FarLeft  (0) → −2 × LANE_WIDTH_CM   = −200.0 cm at LANE_WIDTH_M = 1.0 default
  Left     (1) → −1 × LANE_WIDTH_CM   = −100.0 cm
  Center   (2) →  0 × LANE_WIDTH_CM   =    0.0 cm  (track-X origin)
  Right    (3) → +1 × LANE_WIDTH_CM   = +100.0 cm
  FarRight (4) → +2 × LANE_WIDTH_CM   = +200.0 cm

  Generalized: lane_world_x_cm(lane_index) = (lane_index − 2) × LANE_WIDTH_CM
                                            where LANE_WIDTH_CM = LANE_WIDTH_M × 100
```

`LANE_WIDTH_M` (= `LANE_WIDTH_CM / 100`) is the tuning knob (see §7 Tuning Knobs). Center (lane index 2) is the track's X origin under the R7-PM-PROPAGATION 5-lane widening. Previously `LANE_OFFSET_CM` was the per-edge-offset tuning knob on the 3-lane model; the new `LANE_WIDTH_M` knob has DIFFERENT semantics — it measures per-lane width, not per-edge offset. At the prior default `LANE_OFFSET_CM = 200` (per-edge offset, 3 lanes), inter-lane distance was 200 cm; under the new default `LANE_WIDTH_M = 1.0 m → LANE_WIDTH_CM = 100`, inter-lane distance is 100 cm (half the prior value). This is intentional: at 5 lanes, finer-grained lane spacing at default tuning (`LANE_WIDTH_M = 1.0 m`) keeps the default total track span at 400 cm = 4.0 m, coincidentally matching the prior 3-lane default, without making the track wider on a 6-inch mobile screen. **R10a §2.4 correction**: total track span scales with `LANE_WIDTH_M × 4` across the safe range [0.85, 1.5] m → 3.40 – 6.00 m and is NOT preserved at all tunings; the 400 cm value applies only at the default. See §7 Tuning Knobs for the safe-range span table.

**Example (R7-PM-PROPAGATION update for 5-lane widening):** With `LANE_WIDTH_M = 1.0 m → LANE_WIDTH_CM = 100`: FarLeft (0) = −200.0, Left (1) = −100.0, Center (2) = 0.0, Right (3) = +100.0, FarRight (4) = +200.0.

---

### F-2 — Tween Progress Accumulator

Each `TickComponent`, while SLIPPING and not paused or in resume_grace:

```cpp
// R11a §1 — Persistent clamp: every tick, clamp to nearest safe-range bound.
// bFloorGuardFired semantics RETIRED per `player-movement-platform.md` § Shipping-Safety Enforcement Policy.
// Log idiom (log-on-counter-zero, modulo-600 increment) deviates from the
// R11a brief's verbatim "++counter >= 600" form to align with AC-21 + AC-SS-A's
// "first log fires on tick 1; second on tick 601" assertions. See the cross-
// references block below for the documented deviation rationale.
float effective_slip_tween = SLIP_TWEEN_DURATION_S;
if (effective_slip_tween < 0.10f) {
    effective_slip_tween = 0.10f;
    bSlipTweenClampActive = true;
    if (TickClampLogCounter == 0) {  // log first violation of each 600-tick window
        UE_LOG(LogPlayerMovement, Error, TEXT("SLIP_TWEEN_DURATION_S=%.4f below safe floor 0.10; persistently clamped"), SLIP_TWEEN_DURATION_S);
    }
    TickClampLogCounter = (TickClampLogCounter + 1) % 600;  // rate-limit: 1 log per 10s at 60fps
} else if (effective_slip_tween > 0.15f) {
    effective_slip_tween = 0.15f;
    bSlipTweenClampActive = true;
    if (TickClampLogCounter == 0) {
        UE_LOG(LogPlayerMovement, Error, TEXT("SLIP_TWEEN_DURATION_S=%.4f above safe ceiling 0.15; persistently clamped"), SLIP_TWEEN_DURATION_S);
    }
    TickClampLogCounter = (TickClampLogCounter + 1) % 600;
} else {
    bSlipTweenClampActive = false;
    TickClampLogCounter = 0;  // new violation window starts fresh on next breach
}

// effective_dt is defined ONCE at the top of TickComponent by the prologue in
// `player-movement-platform.md` §4 F-PROLOGUE (B-F6-3 closure CLOSED).
// F-2 consumes the upstream value; the pre-PASS-7 local clamp here is RETIRED.
TweenProgress += effective_dt / effective_slip_tween;  // divides by CLAMPED value — clamp is load-bearing
TweenProgress  = min(TweenProgress, 1.0f);
```

`effective_dt` (defined upstream by `player-movement-platform.md` §4 F-PROLOGUE as `clamp(FApp::GetDeltaTime(), 0.0f, MAX_SLIP_DT_S)`) caps at `MAX_SLIP_DT_S` to prevent tween-skip on hitched frames. `effective_slip_tween` is the persistently-clamped local copy of `SLIP_TWEEN_DURATION_S` — the division uses the clamped value, so a config violation never produces divide-by-zero or runaway progress. The clamp is the load-bearing safety mechanism; the rate-limited log is observability only. Progress is **not** advanced when `is_paused == true` or `resume_grace == true`.

**Cross-sub-GDD invariant on `effective_dt` — B-F6-3 CLOSED (PASS 7)**: F-2 and F-6 both consume `effective_dt` from a single upstream TickComponent prologue defined at `player-movement-platform.md` §4 F-PROLOGUE. The pre-PASS-7 local clamp `effective_dt = clamp(DeltaTime, 0.0f, MAX_SLIP_DT_S)` shown in earlier drafts of F-2 is RETIRED — replaced by a one-line consume-from-upstream comment in the F-2 pseudo-code above. Platform owns the canonical definition (single-source invariant: same value fed to the §3 hardware watchdog AND to F-2/F-6, so a future world-time-dilation feature cannot desynchronize the watchdog from the gameplay tween advance); mechanics consumes. The DT source is `FApp::GetDeltaTime()` (engine raw frame DT) — full rationale in `player-movement-platform.md` §4 F-PROLOGUE > "DT source rationale".

**Cross-references (R11a §1 — coordinated edit; consistency canary):** this F-2 prologue is the implementation side of the `player-movement-platform.md` § Shipping-Safety Enforcement Policy SLIP_TWEEN row and the AC-21 fold-in (platform-owned per §5 BLOCKING matrix → see `player-movement-platform.md` §8 Acceptance Criteria). The floor-violation assertion lives in **AC-21** (platform); the 100-tick ceiling-violation assertion lives in **AC-SS-A** (platform). The pre-R10a one-shot `bFloorGuardFired` semantic is retired everywhere; future revisions touching any one of these four locations (F-2 prologue in mechanics / platform §3 Shipping-Safety table / platform §8 AC-21 / platform §8 AC-SS-A) MUST update the other three in lock-step.

**Documented deviation from R11a brief verbatim (R11a §1 — internal-consistency reconciliation):** The brief's F-2 pseudo-code used `if (++TickClampLogCounter >= 600) { log; counter = 0; }` — under that form the first log fires on tick 600, not tick 1. AC-21 and AC-SS-A both assert "first log on tick 1, second log on tick 601" (two-source agreement on the tick-1 semantic). The F-2 prologue above uses the log-on-counter-zero + modulo-600-increment idiom so the implementation matches the assertions. Brief authority is preserved on intent ("rate-limit: 1 log per 10s at 60fps"); only the log-then-suppress ordering differs from "suppress-then-log". Rationale logged here because Cluster A is the internal-contradiction cluster — landing the §1 fix with an unreconciled F-2-vs-AC contradiction would defeat the cluster's purpose.

On tween start (SETTLED → SLIPPING), `TweenProgress` is explicitly reset to `0.0` before the first tick advances it.

**Completion condition:** `TweenProgress >= 1.0` → tween complete; PM enters SETTLED in `target_lane`; `TweenProgress` reset to `0.0`.

**Example (normal tick):** `SLIP_TWEEN_DURATION_S = 0.15`, `raw_dt = FApp::GetDeltaTime() = 0.016`, `MAX_SLIP_DT_S = 0.05`: prologue yields `effective_dt = 0.016`; F-2 reads it and `TweenProgress += 0.107`.

**Example (hitched frame):** `raw_dt = FApp::GetDeltaTime() = 2.0` (engine paused for 2 s on a load), `MAX_SLIP_DT_S = 0.05`: prologue yields `effective_dt = 0.05` (clamped); F-2 reads it and `TweenProgress += 0.333` — no single-frame tween skip. Watchdog separately consumes the unclamped `raw_dt = 2.0` (see `player-movement-platform.md` §4 F-PROLOGUE Example).

---

### F-3 — Lateral World Position (Output)

Computes the actor's current world X each tick:

```cpp
if (!SlipCurve) {
    // Null fallback: linear lerp — no authored curve shape
    lateral_world_position = Lerp(source_x, target_x, TweenProgress);
    return;
}

source_x = lane_world_x(current_lane)
target_x = lane_world_x(target_lane)
raw_t    = SlipCurve->GetFloatValue(TweenProgress)
curve_t  = clamp(raw_t, 0.0f, 1.0f)   // guard against authored-curve overshoot

lateral_world_position = Lerp(source_x, target_x, curve_t)
```

`SlipCurve` is the `UCurveFloat` referenced by `SLIP_CURVE_ASSET` — a **hard reference** (`TObjectPtr<UCurveFloat>`, `EditAnywhere`). The null guard prevents a Shipping-build null dereference; it is not a graceful fallback for missing assets — see Authored Asset Contracts below for the BeginPlay check that catches null at startup. When SETTLED, `TweenProgress == 0.0` and `source_x == target_x`, so `lateral_world_position == lane_world_x(current_lane)`.

**Example (R7-PM-PROPAGATION updated for 5-lane widening):** With `LANE_WIDTH_M = 1.0 m → LANE_WIDTH_CM = 100`, slipping Left (1) → Center (2), `TweenProgress = 0.5`, `SlipCurve` returns 0.62 → `curve_t = 0.62` → `lateral_world_position = Lerp(lane_world_x_cm(Left)=−100, lane_world_x_cm(Center)=0, 0.62) = −38.0 cm`. (Pre-R7-PM-PROPAGATION worked example was based on 3-lane `LANE_OFFSET_CM = 200`: Lerp(−200, 0, 0.62) = −76 cm; under the 5-lane widening, lane-adjacent slips traverse half the cm distance because per-lane width is half.)

---

### F-4 — Buffer Pre-Validation (Edge Check)

Before accepting a slip event into the buffer while a tween is in progress:

```
projected_lane = target_lane  // the in-flight tween's destination

if (input == slip-left  AND projected_lane == FarLeft)  → discard + edge-absorb animation   // R7-PM-PROPAGATION: track-edge identity is FarLeft (lane 0) under 5-lane widening
if (input == slip-right AND projected_lane == FarRight) → discard + edge-absorb animation   // R7-PM-PROPAGATION: track-edge identity is FarRight (lane 4) under 5-lane widening
otherwise → accept into buffer
```

Prevents buffering a slip that will be a no-op the moment it executes. Cross-reference §3 Detailed Rules Rule 3: a second input while the buffer is already occupied is dropped before F-4 is reached.

---

### F-5 — Rotation Output (Body Lean)

**Actor orientation:** The character faces world **-Y** (track runs along world Y; camera follows from world +Y). Lateral lean = rotation about the **actor local X axis**. Right-slip: negative local-X rotation. Left-slip: positive local-X rotation. Applied via `SetRelativeRotation()` on the mesh component (see §3 Cross-Component Interfaces > Rotation Implementation).

**lean_angle is NOT clamped** in F-5. The settle dip (Phase 3 overshoot) is authored directly into `LeanCurve` as a small negative excursion below 0.0 at t≈0.85–0.95 (see Authored Asset Contracts below). The EC-15 additive clamp (`±MAX_LEAN_ANGLE_DEG × 1.2`) is a separate, edge-absorb-specific guard and does not apply to normal F-5 output.

```cpp
if (!LeanCurve) {
    lean_angle      = 0.0f;
    head_lean_angle = 0.0f;
    arm_lean_angle  = 0.0f;
    return;
}

lean_sign       = (target_lane is to the right of current_lane) ? -1.0f : +1.0f

lean_t          = LeanCurve->GetFloatValue(TweenProgress)
lean_angle      = lean_t * MAX_LEAN_ANGLE_DEG * lean_sign   // not clamped — see above

head_progress   = max(TweenProgress - HEAD_LAG_PROGRESS, 0.0f)
head_lean_t     = LeanCurve->GetFloatValue(head_progress)
head_lean_angle = head_lean_t * MAX_LEAN_ANGLE_DEG * lean_sign

arm_progress    = min(TweenProgress + ARM_LEAD_PROGRESS, 1.0f)   // arm leads body
arm_lean_t      = LeanCurve->GetFloatValue(arm_progress)
arm_lean_angle  = arm_lean_t * MAX_LEAN_ANGLE_DEG * lean_sign
```

`lean_angle`, `head_lean_angle`, and `arm_lean_angle` are written to the public interface each tick (see §3 Public Interface (Mechanics-Side)). All three share `LeanCurve` — only the sampling offset differs. `ARM_LEAD_PROGRESS` controls how far ahead in the tween arc the arm/shoulder reads, producing the "body commits, arm initiates" timing.

**Voxel scaling:** At peak lean (lean_t = 1.0), apply 5–8% X-axis compression to the torso voxel block as a child-node transform: `torso_scale_x = 1.0 - (lean_t * 0.065)`.

When SETTLED (TweenProgress = 0.0): F-5 contributes `lean_angle = 0`, `head_lean_angle = 0`, `arm_lean_angle = 0` (neutral pose). When DEAD fires mid-tween: F-5 contribution freezes at its current value — no ease to neutral. When COMPLETE or ABORTED fires: F-5 contribution resets to 0.0 in the same frame as the snap (per §3 Detailed Rules Rules 8–9).

**F-5 / F-6 co-write contract (R10a §1.1 — closes head/arm decouple defect):** F-5 produces *contributions* to the three lean outputs, not final values. When F-6 is active (`edge_absorb_active == true`), F-6 also contributes to all three outputs; the **final** `lean_angle`, `head_lean_angle`, `arm_lean_angle` are the sum of F-5 + F-6 contributions, clamped to `±(MAX_LEAN_ANGLE_DEG × 1.2)`. See F-6 → "F-5 / F-6 co-write contract" table below for the full per-state matrix. Pre-R10a, F-6 wrote `lean_angle` only (head/arm decoupled from body during the SETTLED F-6 tail) — R10a §1.1 fixes the contract so F-6 writes all three with the same head/arm staggering F-5 uses.

---

### Boundary Value Summary

| Formula | Min output | Max output | Degenerate risk |
|---|---|---|---|
| F-1 | `−2 × LANE_WIDTH_CM` (= −200 at default; FarLeft lane center) | `+2 × LANE_WIDTH_CM` (= +200 at default; FarRight lane center) | None — R7-PM-PROPAGATION 5-lane widening: bounds widen to 5 discrete lane centers at multiples of LANE_WIDTH_CM, but the FarLeft/FarRight extremes preserve the prior 3-lane span. |
| F-2 | 0.0 | 1.0 (clamped) | `DeltaTime` clamped to `[0, MAX_SLIP_DT_S]`; floor prevents division by near-zero `SLIP_TWEEN_DURATION_S` |
| F-3 | `source_x` | `target_x` | `curve_t` clamped to [0,1]; null guard returns linear lerp; no Lerp overflow |
| F-4 | accept / discard | accept / discard | None |
| F-5 | `< -MAX_LEAN_ANGLE_DEG` (authored undershoot possible) | `> +MAX_LEAN_ANGLE_DEG` (authored undershoot possible) | Not clamped by design — settle overshoot is authored into LeanCurve; null guard returns 0.0 |
| F-6 (R10a §1.1 — body/head/arm triple output) | 0.0 (at `edge_absorb_progress = 1.0`, fully decayed) | `MAX_LEAN_ANGLE_DEG × 0.35` per output (body at `edge_absorb_progress = 0.0`; head and arm at their staggered offset extremes — head reads earlier samples, arm reads later samples; before final co-write clamp) | Additive sum with F-5 contribution on each of the three outputs (body / head / arm), final clamp `±MAX_LEAN_ANGLE_DEG × 1.2`. §5.1 (a) Override resets all three to 0 on SETTLED→SLIPPING transition. EdgeAbsorbCurve null-fallback to linear decay. |

---

### F-6 — Edge-Absorb Tail Phase (R10a §1.1 — full re-specification per brief; SUPERSEDES prior R5-era partial spec)

**Scope:** F-6 is the edge-absorb tail phase animation. It activates in two paths: (1) **F-4 mid-tween path** — F-4 discards a buffer input as an edge no-op while PM is SLIPPING (additive lean contribution composites with the in-flight F-5 lean per EC-15); (2) **Rule 1 SETTLED path** — a slip input at SETTLED at FarLeft / FarRight produces an edge no-op (F-6 drives the lean response without F-5 contribution, since no tween starts). In both paths, F-6 produces a transient lean toward the dropped-input direction that decays back to neutral over `EDGE_ABSORB_DURATION_S`.

**Per-tick state (R10a §1.1 NEW; extended by R11a §6.1 with 4 fade-out members):**

| Member | Type | Initialized at | Reset at |
|---|---|---|---|
| `edge_absorb_active` | `bool` | `false` | F-6 fires → `true`; F-6 timer expires OR §5.1 (a) Override fires (transitions to fade-out, see below) OR terminal state → `false` |
| `edge_absorb_local_timer` | `float` (s) | `0.0` when F-6 fires | F-6 timer expires OR §5.1 (a) Override fires |
| `edge_absorb_sign` | `float` (±1) | `+1.0` (left) or `−1.0` (right) captured at F-6 start from the dropped-input direction (NOT from in-flight tween direction — see "lean_sign convention" below) | F-6 timer expires OR §5.1 (a) Override fires |
| `EDGE_ABSORB_DURATION_S` | tuning knob (default 0.27s) | constant | — |
| `edge_absorb_progress` | derived `float` ∈ [0.0, 1.0] | `edge_absorb_local_timer / EDGE_ABSORB_DURATION_S` clamped | — |
| `f6_override_fadeout_ticks_remaining` (R11a §6.1 NEW) | `int8` | `0` | §5.1 (a) Override fires → `2` (2-frame fade-out at 60 fps = 33 ms); decremented at end of each tick while > 0; back to `0` after fade-out completes |
| `f6_override_fadeout_snapshot_body` (R11a §6.1 NEW) | `float` (degrees) | `0.0` | §5.1 (a) Override fires → captured `f6_lean_body` at the moment of Override; held constant during fade-out; back to `0.0` after fade-out completes |
| `f6_override_fadeout_snapshot_head` (R11a §6.1 NEW) | `float` (degrees) | `0.0` | Same as `snapshot_body` for `f6_lean_head` |
| `f6_override_fadeout_snapshot_arm` (R11a §6.1 NEW) | `float` (degrees) | `0.0` | Same as `snapshot_body` for `f6_lean_arm` |

**lean_sign convention (R10a §1.1 — clarified from pre-R10a spec):** `edge_absorb_sign` is captured from the **dropped input direction** at F-6 fire time, NOT from the in-flight tween direction (the prior R7-PM-PROPAGATION-REVIEW example "use the lean_sign of the in-flight tween" was incorrect — under §1.1 path (2) Rule 1 SETTLED, there is no in-flight tween, so the dropped-input direction is the only available sign source; path (1) F-4 mid-tween case is dominated by §5.1 (a) Override timer-collision behavior — see below — so the prior dual-source confusion is retired). `slip-left` → `edge_absorb_sign = +1.0` (left lean); `slip-right` → `edge_absorb_sign = −1.0` (right lean).

#### F-6 lean formula — body, head, arm (R10a §1.1 — new triple-output spec; closes head/arm decouple defect)

F-6 has its own internal curve `EdgeAbsorbCurve` (linear decay by default — see Authored Asset Contracts below) that maps `edge_absorb_progress` ∈ [0.0, 1.0] → lean fraction ∈ [0.0, 0.35] (peak 35% of `MAX_LEAN_ANGLE_DEG`, decaying to 0). The head/arm offset semantics from F-5 (`HEAD_LAG_PROGRESS = 0.10` lag, `ARM_LEAD_PROGRESS = 0.05` lead) apply identically:

```cpp
// F-6 ticks regardless of SETTLED / SLIPPING state, provided edge_absorb_active == true
// OR a §5.1 (a) Override fade-out is in progress.
// edge_absorb_local_timer advances each tick by effective_dt — the SAME upstream variable F-2
// consumes (defined ONCE per tick by `player-movement-platform.md` §4 F-PROLOGUE;
// B-F6-3 closure CLOSED at PASS 7). F-6 does NOT recompute the clamp.

// R11a §6.1 — §5.1 (a) Override fade-out branch (anti-snap reconciliation).
// During fade-out, output the snapshotted F-6 lean values scaled by a linear
// ramp-down: tick 1 of 2 → multiplier 1.0; tick 2 of 2 → multiplier 0.5; tick 3 → 0.
// This eliminates the 1-frame discontinuity that contradicted the LEAN_CURVE_ASSET
// anti-snap contract. After 2 ticks the snapshot is hard-zeroed.
if (f6_override_fadeout_ticks_remaining > 0) {
    float fade_mult = static_cast<float>(f6_override_fadeout_ticks_remaining) / 2.0f;
    f6_lean_body = f6_override_fadeout_snapshot_body * fade_mult;
    f6_lean_head = f6_override_fadeout_snapshot_head * fade_mult;
    f6_lean_arm  = f6_override_fadeout_snapshot_arm  * fade_mult;
    --f6_override_fadeout_ticks_remaining;
    if (f6_override_fadeout_ticks_remaining == 0) {
        f6_override_fadeout_snapshot_body = 0.0f;
        f6_override_fadeout_snapshot_head = 0.0f;
        f6_override_fadeout_snapshot_arm  = 0.0f;
    }
    return;
}

if (!edge_absorb_active) {
    f6_lean_body = 0.0f;
    f6_lean_head = 0.0f;
    f6_lean_arm  = 0.0f;
    return;
}

edge_absorb_progress = clamp(edge_absorb_local_timer / EDGE_ABSORB_DURATION_S, 0.0f, 1.0f);

// Body samples the F-6 curve at the current progress.
body_f6_t = EdgeAbsorbCurve->GetFloatValue(edge_absorb_progress);   // ∈ [0.0, 0.35]
f6_lean_body = body_f6_t * MAX_LEAN_ANGLE_DEG * edge_absorb_sign;

// Head LAGS the body (reads earlier progress sample).
head_f6_progress = clamp(edge_absorb_progress - HEAD_LAG_PROGRESS, 0.0f, 1.0f);
head_f6_t = EdgeAbsorbCurve->GetFloatValue(head_f6_progress);
f6_lean_head = head_f6_t * MAX_LEAN_ANGLE_DEG * edge_absorb_sign;

// Arm LEADS the body (reads later progress sample).
arm_f6_progress = clamp(edge_absorb_progress + ARM_LEAD_PROGRESS, 0.0f, 1.0f);
arm_f6_t = EdgeAbsorbCurve->GetFloatValue(arm_f6_progress);
f6_lean_arm = arm_f6_t * MAX_LEAN_ANGLE_DEG * edge_absorb_sign;

// R11a §6.2 — EC-15 head-clamp avoidance: scale F-6 contribution during SLIPPING.
// When F-5 is co-active (mid-tween edge-absorb path), F-6's additive contribution
// is multiplied by (1.0 - TweenProgress × EC15_F6_DECAY_COEFFICIENT) so the sum
// f5_head + f6_head stays under the ±MAX_LEAN_ANGLE_DEG × 1.2 clamp at the
// TweenProgress=0.85 + edge_absorb_progress=0.10 EC-15 boundary. The default
// coefficient 0.7 is tuned upward from the R11a brief baseline 0.5 (which left
// the head sum at 12.01° — at the clamp boundary, not under it). See AC-F6-E
// and DR-F.2 rationale in the §6 brief. SETTLED + F-6 active is unchanged — the
// multiplier applies only when movement_state == SLIPPING.
if (movement_state == ERunSlipState::SLIPPING) {
    const float slipping_mult = max(0.0f, 1.0f - TweenProgress * EC15_F6_DECAY_COEFFICIENT);
    f6_lean_body *= slipping_mult;
    f6_lean_head *= slipping_mult;
    f6_lean_arm  *= slipping_mult;
}

// Timer advance for next frame.
edge_absorb_local_timer += effective_dt;
if (edge_absorb_local_timer >= EDGE_ABSORB_DURATION_S) {
    edge_absorb_active = false;
    edge_absorb_local_timer = 0.0f;
}
```

`EdgeAbsorbCurve` defaults to a linear decay (`y = 0.35 × (1 − x)`) if not assigned — the previous F-6 spec hard-coded this. As a tuning knob (`EDGE_ABSORB_CURVE_ASSET`, new — see Authored Asset Contracts below) the curve can be authored with a different envelope (e.g., quick decay early, slow decay late) to taste.

`EC15_F6_DECAY_COEFFICIENT` (R11a §6.2 NEW; default 0.7) is a private const float. Tuning rationale: at TweenProgress=0.85 + edge_absorb_progress=0.10 with default 0.27s EDGE_ABSORB_DURATION_S, F-5 head ≈ 10° and pre-multiplier F-6 head ≈ 3.5°. With coefficient 0.7, slipping_mult = 1.0 − 0.85 × 0.7 = 0.405, scaled F-6 head = 1.42°, sum = 11.42° < 12.0° clamp (margin = 0.58° = 4.8% of clamp ceiling — ~6× the ±0.1° AC-F6-E measurement tolerance). The brief baseline coefficient 0.5 would yield slipping_mult = 0.575, scaled F-6 head = 2.01°, sum = 12.01° — at the clamp boundary, not under it; AC-F6-E would assert at-tolerance pass which is not "unambiguously under clamp". The 0.7 default is the smallest coefficient that produces a clamp-avoidance margin ≥ 5× the measurement tolerance at the EC-15 worst case while preserving meaningful F-6 contribution at early TweenProgress (TP=0.0 → multiplier 1.0; TP=0.5 → multiplier 0.65). **Safe tuning range**: lower bound determined by clamp arithmetic at TP=0.85 — coefficient must satisfy `1 − 0.85 × c ≤ 0.571` (the F-6 head multiplier that brings sum to exactly 12°) → `c ≥ 0.505`; conservatively, lower bound is **0.6** (≥0.29° margin). Upper bound determined by non-negative output at TP=1.0 — `1 − 1.0 × c ≥ 0` → `c ≤ 1.0`; conservatively, upper bound is **1.0**. Below 0.6 reintroduces clamp risk; above 1.0 produces zero F-6 contribution at TP=1.0 (effectively disabling F-6 at tween completion, which is unintended — the `max(0, ...)` guard prevents negative output if a future tuner overshoots, but the design intent is preserved by keeping `c ≤ 1.0`).

#### F-5 / F-6 co-write contract (R10a §1.1 NEW — closes head/arm decouple defect; BINDING)

The final `lean_angle`, `head_lean_angle`, `arm_lean_angle` outputs depend on the state combination. F-6 contribution ADDS to F-5 contribution on all three angles; the head/arm staggering is applied within each formula independently and the SUM produces the final output. Final values are clamped to `±(MAX_LEAN_ANGLE_DEG × 1.2)` — the only place a clamp is applied to lean angles.

| State combo | `lean_angle` (body) | `head_lean_angle` | `arm_lean_angle` |
|---|---|---|---|
| SETTLED + F-6 inactive | 0 | 0 | 0 |
| SETTLED + F-6 active (tail phase) | `clamp(0 + f6_lean_body, ±MAX_LEAN×1.2)` = `f6_lean_body` | `f6_lean_head` | `f6_lean_arm` |
| SLIPPING + F-6 inactive (normal tween) | `f5_lean_body` (from F-5) | `f5_lean_head` | `f5_lean_arm` |
| SLIPPING + F-6 active (EC-15 mid-tween edge-absorb) | `clamp(f5_lean_body + f6_lean_body, ±MAX_LEAN×1.2)` | `clamp(f5_lean_head + f6_lean_head, ±MAX_LEAN×1.2)` | `clamp(f5_lean_arm + f6_lean_arm, ±MAX_LEAN×1.2)` |
| DEAD entry under any of the above | freeze at current value | freeze at current value | freeze at current value |
| COMPLETE / ABORTED / COUNTDOWN entry | 0 | 0 | 0 |

This contract resolves the pre-R10a head/arm decouple defect: in the prior spec, F-6 wrote `lean_angle` only, leaving `head_lean_angle` and `arm_lean_angle` driven solely by F-5 (which during the F-6 SETTLED-tail phase is producing 0). The head/arm-snap-to-zero while the body still leaned via F-6 was the visual contradiction R9 flagged. Under the §1.1 spec, F-6 writes all three angles with the same staggered offsets F-5 uses; head and arm stay aligned with the body throughout the tail.

#### §5.1 (a) Override — timer-collision on new SLIPPING transition (R10a NEW — author decision recorded)

**Decision (R10a §5.1 = (a) Override; locked 2026-06-14; implementation declared R11a §2; anti-snap reconciliation applied R11a §6.1):** When PM transitions SETTLED→SLIPPING while F-6 tail is active (`edge_absorb_active == true`), F-6 is transitioned into a **2-frame linear fade-out** (33 ms at 60 fps). The fade-out reconciles the Override author decision with the LEAN_CURVE_ASSET anti-snap contract (which specifically forbids 1-frame discontinuous lean-angle jumps at tween completion); without fade-out, the Override would zero F-6 contributions in one frame — structurally identical to the head-snap defect the anti-snap contract was authored to prevent. The Override author decision (`§5.1 = (a) Override`) remains binding on the SETTLED→SLIPPING semantic; the fade-out is the implementation that satisfies BOTH the Override intent (F-5 takes over quickly) AND the anti-snap contract (no instant jump).

**Fade-out semantics** (R11a §6.1 — DR-F.1 = (a) 2-frame fade-out):

1. **At Override fire** (inside `HandleSlipTransition` SETTLED-path branch — see §3 Cross-Component Interfaces > Slip Input Dispatch): snapshot the current F-6 lean values into private state:
   - `f6_override_fadeout_snapshot_body = f6_lean_body` (at this moment, before zeroing)
   - `f6_override_fadeout_snapshot_head = f6_lean_head`
   - `f6_override_fadeout_snapshot_arm  = f6_lean_arm`
   - `f6_override_fadeout_ticks_remaining = 2`
   Then hard-zero the F-6 timer state so normal F-6 sampling stops contributing:
   - `edge_absorb_active = false`
   - `edge_absorb_local_timer = 0.0f`
   - `edge_absorb_sign = 0.0f`

2. **On the 2 fade-out ticks** (per the F-6 formula's fade-out branch): F-6 contributes the snapshot values scaled by `(f6_override_fadeout_ticks_remaining / 2.0)`:
   - Tick 1 of 2 (immediately after Override fires, on the SLIPPING transition frame): multiplier = 1.0 → F-6 still contributes its snapshotted value. F-5 also contributes from `TweenProgress = 0.0`. Sum is the additive composite on all three lean angles.
   - Tick 2 of 2: multiplier = 0.5 → F-6 contributes half the snapshot.
   - Tick 3 (after fade-out completes): F-6 contributes 0.

3. **After fade-out completes**: snapshot values are hard-zeroed; F-6 is fully retired until the next edge no-op fires it again.

**Why 2 frames and not more**: the brief specifies "1-2 frame fade-out (33ms at 60fps)". 2 frames is the upper bound; 1 frame would still produce a 50% step on the second frame's transition to 0, which approaches the discontinuity defect the contract was authored to prevent. 2 frames produces a smooth linear ramp (1.0 → 0.5 → 0.0) with no single-frame jump exceeding 50% of the snapshot value. Going beyond 2 frames would dilute the Override author decision intent ("F-5 takes over cleanly from TweenProgress = 0.0" — at 3+ frames the F-6 tail visibly lingers into the new slip and the input feels less responsive).

**Implementation sites** (lock-step canary per R11a §1 discipline):
- F-6 lean formula pseudo-code (this subsection above) — the fade-out branch at the top of the function consumes the snapshot state and decrements `ticks_remaining`.
- `HandleSlipTransition` body (§3 Cross-Component Interfaces > Slip Input Dispatch) — the SETTLED-path Override block writes the snapshot state and sets `ticks_remaining = 2`.
- `HandleStateChanged` dispatch (§3 Cross-Component Interfaces > Delegate Handler Bodies) — COMPLETE/ABORTED/COUNTDOWN/IDLE cases reset all 4 fade-out members; DEAD case preserves them (Rule 7 parity). See §8 AC-COUNTER-F6-RESET Case B (reset) and AC-F6-D fade-out preservation block (DEAD).
- AC-F6-B (§8 Acceptance Criteria > F-6) — the assertion-side contract verifies the fade-out tick-by-tick.

Any change to one of these FOUR sites MUST be coordinated with the other three. The pre-R11a "kill block" semantic is RETIRED; the snapshot-and-fade semantic replaces it.

Rationale: Player Fantasy ("the body moves before the mind finishes the sentence") prioritizes input responsiveness over animation continuity. F-6's tail is decorative; SLIPPING is mechanical. A player who edge-absorbs then immediately slips the other direction should see the new slip with the dropped-input F-6 contribution fading rapidly (not lingering, not snapping). Alternatives (b) Kill (graceful fade over 100+ms) and (c) Accumulate (sum F-6 with new F-5 throughout the new tween) were considered and rejected — (b) would dilute the input-responsiveness intent, (c) would produce additive lean angles that fight the new slip direction. The 2-frame fade is the minimum that satisfies both constraints.

#### Reset semantics on terminal states (R10a §1.1 NEW — parity with existing terminal rules)

| Terminal state | `edge_absorb_active` | `edge_absorb_local_timer` | F-6 output | Notes |
|---|---|---|---|---|
| COUNTDOWN (§3 Rule 10) | reset to `false` | reset to `0.0` | 0 on all three angles | Parity with COUNTDOWN's counter resets — full state reset for new run |
| DEAD (§3 Rule 7) | preserved at current value (e.g., `true`) | preserved at current value | preserved at current frozen value | Parity with Rule 7's TweenProgress / F-5 lean preservation for Death Replay |
| COMPLETE (§3 Rule 8) | reset to `false` | reset to `0.0` | 0 on all three angles | Parity with Rule 8's snap-to-target + lean reset |
| ABORTED (§3 Rule 9) | reset to `false` | reset to `0.0` | 0 on all three angles | Same as COMPLETE |
| Pause (`is_paused = true`, §3 Rule 6) | preserved | preserved (timer halted via per-tick gate) | preserved at frozen value | Parity with F-2 freeze; resume picks up tail from frozen progress |

These are folded into the `HandleStateChanged` body (§3 Cross-Component Interfaces > Delegate Handler Bodies) — each switch case sets the F-6 state per the table.

**Example (R10a §1.1 — worked example for F-6 path (2), Rule 1 SETTLED edge no-op):** Player is SETTLED at FarLeft (lane 0) and inputs `slip-left`. Rule 1: edge no-op. F-6 activates: `edge_absorb_active = true`, `edge_absorb_local_timer = 0.0`, `edge_absorb_sign = +1.0`. On the first tick (0.0167s later at 60fps), `edge_absorb_progress = 0.0167 / 0.27 = 0.062`. `body_f6_t = EdgeAbsorbCurve(0.062) ≈ 0.328` (linear default), `f6_lean_body ≈ 0.328 × 10° × +1.0 = +3.28°`. `head_f6_progress = max(0.062 − 0.10, 0) = 0.0` → `head_f6_t = 0.35` → `f6_lean_head = 0.35 × 10° × +1.0 = +3.5°`. `arm_f6_progress = 0.062 + 0.05 = 0.112` → `arm_f6_t ≈ 0.311` → `f6_lean_arm ≈ +3.11°`. Body and head and arm all lean LEFT (positive sign); arm slightly trails the head's full-peak value because the body is past the peak. After 270 ms (`edge_absorb_local_timer == 0.27`), `edge_absorb_active = false`; all three angles return to 0. F-5 is not active throughout (PM stayed SETTLED).

**Example (R10a §1.1 — worked example for F-6 path (1), EC-15 mid-tween edge-absorb):** Player is mid-tween FarLeft→Left, `TweenProgress = 0.5`, `lean_sign = −1.0` (rightward tween). F-5 produces `f5_lean_body = -8°` (peak left-to-right body lean). Player inputs another `slip-left`. F-4: projected_lane = Left, slip-left = NOT an edge no-op (Left → FarLeft is valid). F-6 does NOT fire on this path. (The prior example "SLIPPING Left→FarLeft + slip-left → F-4 fires F-6" remains correct for that specific scenario; F-4 only fires F-6 when the projected_lane equals an edge AND the input is the off-track direction.) Repeating with corrected scenario: SLIPPING Left→FarLeft, `TweenProgress = 0.5`, `lean_sign = +1.0` (leftward tween). Player inputs slip-left again. F-4: projected_lane = FarLeft, slip-left = edge no-op → F-6 fires with `edge_absorb_sign = +1.0`. F-5 contributes `f5_lean_body = +8°`. F-6 first tick: `f6_lean_body ≈ +3.28°` (per math above). Combined: `lean_angle = clamp(8 + 3.28, ±12) = +11.28°`. Head and arm composited similarly. The §5.1 (a) Override does NOT fire here because PM stays SLIPPING (no new SETTLED→SLIPPING transition occurred — the slip-left was dropped at F-4, not buffered and not committed).

### Authored Asset Contracts

This subsection defines the authoring requirements for the three `UCurveFloat` assets that drive PM's formulas. Implementers must not assign curves that violate these contracts — BeginPlay verifications catch violations at startup in non-Shipping builds. The Shipping-safe guard family for these contracts (curve null fallback flag visibility) is owned by `player-movement-platform.md` § Shipping-Safety Enforcement Policy AC-SS-D.

#### SLIP_CURVE_ASSET (`SlipCurve`)

**Purpose:** Drives `lateral_world_position` via F-3. Maps TweenProgress [0.0, 1.0] → translation fraction [0.0, 1.0].

**Required shape:**

| TweenProgress | Required output | Rationale |
|---|---|---|
| 0.0 | 0.0 ± 0.01 | No translation on frame 1 — lean-in is rotation-only |
| [0.0, 0.20] | ≤ 0.01 | Phase 1 shelf — body pivots before translating |
| 0.19 | ≤ 0.01 | Shelf does not end early |
| 1.0 | ≥ 0.99 | Player arrives at destination lane center |

**Authoring tolerance:** The curve may slightly exceed 1.0 (up to +0.05 for aesthetic ease-out) — F-3 clamps `curve_t` to [0.0, 1.0], so the visual will not overshoot the destination lane. Do not exceed 1.05; larger overshoot defeats the clamp intent.

**Debug baseline:** A linear curve (no shelf) is acceptable for automated tests only. Never ship the linear baseline — it eliminates the Phase 1 lean-in tell.

**BeginPlay verification (logs `Warning` — no-op in Shipping; use non-Shipping builds to catch violations):**

```cpp
if (SlipCurve) {
    if (SlipCurve->GetFloatValue(0.0f) > 0.01f)
        UE_LOG(LogPlayerMovement, Warning, "SLIP_CURVE_ASSET non-zero at t=0 — tween snaps on frame 1");
    if (SlipCurve->GetFloatValue(0.10f) > 0.01f)
        UE_LOG(LogPlayerMovement, Warning, "SLIP_CURVE_ASSET missing Phase 1 shelf — lean-in will be invisible");
    if (SlipCurve->GetFloatValue(0.19f) > 0.01f)
        UE_LOG(LogPlayerMovement, Warning, "SLIP_CURVE_ASSET Phase 1 shelf ends early — translation begins before lean completes");
    if (SlipCurve->GetFloatValue(1.0f) < 0.99f)
        UE_LOG(LogPlayerMovement, Warning, "SLIP_CURVE_ASSET does not reach 1.0 at t=1 — player lands short of lane center");
} else {
    UE_LOG(LogPlayerMovement, Error, "SLIP_CURVE_ASSET is null — assign asset in editor; falling back to linear lerp");
}
```

---

#### LEAN_CURVE_ASSET (`LeanCurve`)

**Purpose:** Drives body lean angle via F-5. Maps TweenProgress [0.0, 1.0] → lean fraction [−0.05, 1.0] (the small negative excursion below 0.0 is the Phase 3 settle overshoot — by design).

**Required shape:**

| TweenProgress | Required output | Rationale |
|---|---|---|
| 0.0 | 0.0 ± 0.01 | Neutral pose at rest |
| [0.0, 0.20] | rises 0→1 | Phase 1 lean-in — body commits to direction |
| [0.20, 0.80] | 1.0 sustained | Phase 2 — body holds lean during translation |
| [0.80, 0.90] | drops 1→0 | Phase 3 — body returning to neutral |
| 0.90 | 0.0 ± 0.01 | **Head-snap prevention** — head reads curve at TweenProgress-0.10; at tween completion (TweenProgress=1.0) head reads t=0.90; must be ~0.0 so head arrives at neutral without a discontinuous jump |
| [0.90, 0.95] | dips to ≈−0.05 | Phase 3 settle overshoot — body rotates past neutral before catching |
| [0.95, 1.00] | returns to 0.0 | Body fully neutral |
| 1.0 | 0.0 ± 0.01 | Settled neutral |

**Settle overshoot:** The curve dipping below 0.0 (negative excursion) produces a brief reverse lean — "body caught itself." This is rotation-only; `lateral_world_position` does not overshoot (F-3 clamp). The negative excursion must not exceed −0.08 or the reverse lean reads as a visual glitch.

**HEAD_LAG_PROGRESS contract:** The head reads `LeanCurve` at `TweenProgress − HEAD_LAG_PROGRESS` (default 0.10). The constraint `LeanCurve(0.90) ≈ 0.0` is load-bearing: it ensures that when the body completes its tween (TweenProgress resets to 0.0), the head's last sampled value was already at neutral — no discontinuous jump to 0.0 on the next frame. If `LeanCurve(0.90)` is non-zero, the head will visibly snap to neutral on every slip completion.

**BeginPlay verification:**

```cpp
if (LeanCurve) {
    if (fabsf(LeanCurve->GetFloatValue(0.0f)) > 0.01f)
        UE_LOG(LogPlayerMovement, Warning, "LEAN_CURVE_ASSET non-zero at t=0 — neutral pose has residual lean");
    if (fabsf(LeanCurve->GetFloatValue(0.90f)) > 0.01f)
        UE_LOG(LogPlayerMovement, Warning, "LEAN_CURVE_ASSET non-zero at t=0.90 — head will snap to neutral on every tween completion");
    if (fabsf(LeanCurve->GetFloatValue(1.0f)) > 0.01f)
        UE_LOG(LogPlayerMovement, Warning, "LEAN_CURVE_ASSET non-zero at t=1.0 — settled pose has residual lean");
} else {
    UE_LOG(LogPlayerMovement, Error, "LEAN_CURVE_ASSET is null — assign asset in editor; falling back to zero lean");
}
```

---

#### EDGE_ABSORB_CURVE_ASSET (`EdgeAbsorbCurve`) — R10a §1.1 NEW

**Purpose:** Drives F-6 edge-absorb tail body / head / arm lean angles via the same staggered-sampling pattern F-5 uses (`HEAD_LAG_PROGRESS` / `ARM_LEAD_PROGRESS` offsets applied to `edge_absorb_progress`). Maps `edge_absorb_progress` ∈ [0.0, 1.0] → lean fraction ∈ [0.0, 0.35].

**Required shape:**

| `edge_absorb_progress` | Required output | Rationale |
|---|---|---|
| 0.0 | 0.35 ± 0.01 | Peak attempted lean at F-6 fire — the "body tried" moment |
| [0.0, 0.10] | sustained near 0.35 | Brief hold at peak — body presses against the invisible wall |
| [0.10, 0.95] | smooth decay 0.35 → 0.0 | Damped return — visibly slower than slip Phase 3 (effort withdrawn, not snapped) |
| 0.95 | ≤ 0.05 | Tail nearly complete |
| 1.0 | 0.0 ± 0.01 | Fully decayed to neutral |

**Default fallback:** if `EdgeAbsorbCurve` is null, F-6 uses a linear decay `y = 0.35 × (1 − x)` (this is the pre-R10a hard-coded behavior). Curve assignment is a polish-phase task; the linear fallback is acceptable for prototype and integration testing.

**BeginPlay verification (logs `Warning` — non-Shipping only; falls back to linear if violated):**

```cpp
if (EdgeAbsorbCurve) {
    if (EdgeAbsorbCurve->GetFloatValue(0.0f) < 0.34f || EdgeAbsorbCurve->GetFloatValue(0.0f) > 0.36f)
        UE_LOG(LogPlayerMovement, Warning, "EDGE_ABSORB_CURVE_ASSET t=0 not at peak 0.35 — F-6 fire frame will not register as peak attempted lean");
    if (EdgeAbsorbCurve->GetFloatValue(1.0f) > 0.01f)
        UE_LOG(LogPlayerMovement, Warning, "EDGE_ABSORB_CURVE_ASSET t=1 non-zero — F-6 tail will not decay fully to neutral");
} else {
    UE_LOG(LogPlayerMovement, Warning, "EDGE_ABSORB_CURVE_ASSET is null — F-6 falls back to linear decay y = 0.35 × (1 − x). Authored curve recommended for polish.");
}
```

## 5. Edge Cases

EC-1 through EC-15 are mechanics-owned. EC-16 (triple-overlap audio: buffer-drop + slip + near-miss simultaneous dispatch) routes to `player-movement-presentation.md` §5 per the audio-cue-interaction policy split.

**EC-1 — Double-tap same direction while SLIPPING.**
Player taps right twice rapidly. First slip begins tween. Second slip arrives while tween is active → fills buffer (`has_queued_input = true`, `queued_input_direction = Right`). A third slip-right → dropped (buffer full, first queued wins); haptic pulse fires. On tween completion, buffer fires. If the second slip-right would be an edge no-op (already at Right), it was pre-validated at buffer-fill time via F-4 and discarded then — not at execution time.

**EC-2 — DEAD during tween (mid-arc).**
RSM transitions to DEAD while TweenProgress = 0.47. PM freezes: TweenProgress held at 0.47. `current_lane` is set to `target_lane`. `lateral_world_position` reflects the fractional interpolated position. Collision geometry is at `lane_world_x(target_lane)` (committed at tween start per Rule 2). `lean_angle` and `head_lean_angle` freeze at their current mid-arc values — no ease to neutral. Buffer discarded. Death Replay snapshots `lateral_world_position` and collision position separately — these are decoupled.

**EC-3 — DEAD with TweenProgress = 0 (slip accepted, no tick advance yet).**
A slip event has been accepted (PM is SLIPPING) and collision geometry has already committed to `lane_world_x(target_lane)` (Rule 2 commits on SETTLED→SLIPPING). TickComponent has not yet fired. RSM transitions to DEAD. PM freezes: TweenProgress = 0, `lateral_world_position = lane_world_x(current_lane)` (source lane, no visual translation yet). Collision geometry is at `lane_world_x(target_lane)`. `lean_angle = 0`, `head_lean_angle = 0` (F-5 has not fired). Maximum collision-visual separation of `LANE_WIDTH_CM` (one lane width, ≤100 cm at R7-PM-PROPAGATION default; was up to `LANE_OFFSET_CM = 200 cm` under the prior 3-lane model where a Center→Right slip moved 200 cm) for one frame — acceptable by design. Buffer discarded.

**EC-4 — COMPLETE during tween.**
RSM transitions to COMPLETE while TweenProgress = 0.6. PM snaps `lateral_world_position` to `lane_world_x(target_lane)` instantly. `current_lane = target_lane`. `TweenProgress = 0.0`. `lean_angle = 0`, `head_lean_angle = 0` (per §3 Detailed Rules Rule 8). Buffer discarded. No partial arc visible — the snap is frame-exact.

**EC-5 — Pause mid-tween, long background duration.**
TweenProgress = 0.3 when device backgrounds. `is_paused → true`; PM freezes. On foreground resume, `resume_grace` begins. PM remains frozen during grace. At grace expiry, tween resumes: F-2 picks up from 0.3, F-3 and F-5 output the interpolated position and lean. The avatar completes the remaining 70% of its arc.

**EC-6 — Slip input received during resume_grace.**
RSM gating: `resume_grace == true` → input discarded (§3 Rule 5). Not buffered. If a tween was mid-arc before pause, it resumes at grace expiry unaffected by the discarded tap.

**EC-7 — Buffer holds slip at grace expiry after pause mid-tween.**
Sequence: (a) tween at TweenProgress 0.6, (b) pause → freeze, (c) buffer holds a queued slip from before the pause, (d) resume → grace. Buffer is preserved through grace. At grace expiry: frozen tween resumes from 0.6 and completes. Buffer fires immediately after tween completion, initiating the next slip.

**EC-8 — Buffer holds slip when ABORTED fires.**
Player has a slip queued mid-tween. RSM transitions to ABORTED. PM snaps to `target_lane`; `TweenProgress = 0.0`; lean reset to 0°; buffer is discarded. The queued slip never executes.

**EC-9 — Slip input received outside RUNNING.**
All non-RUNNING states (COUNTDOWN, DEAD, COMPLETE, RESOLVING, ABORTED, IDLE): RSM gating fails. Input discarded immediately. Not buffered. No animation response. The avatar does not react to inputs outside RUNNING.

**EC-10 — Rapid opposite-direction inputs: Left then Right.**
Player slips Left→Center (tween in progress, `target_lane = Center`). Before completion, player slips Right. F-4: `projected_lane = Center`, slip-right from Center is valid → buffer accepts (`has_queued_input = true`, `queued_input_direction = Right`). Tween completes at Center. Buffer fires → second tween Center→Right. Net result: Left → Center → Right. Two full bend arcs play in sequence.

**EC-11 — Edge no-op: player at FarRight, slips Right (R7-PM-PROPAGATION updated for 5-lane widening — edge identity is FarRight not Right).**
§3 Rule 1 applies: PM stays SETTLED at FarRight. Edge-absorb animation plays. `movement_state` stays SETTLED. `lateral_world_position` unchanged. `edge_absorb_trigger_count` increments. `has_queued_input` remains false (SETTLED — buffer is only used during SLIPPING). Mirror case: player at FarLeft slipping Left produces identical edge-absorb response. Slips from Right (lane 3) to FarRight (lane 4) are valid 1-lane transitions, NOT edge no-ops.

**EC-12 — Buffer edge pre-validation: tween destination is FarRight, buffered slip-right (R7-PM-PROPAGATION updated for 5-lane widening — edge identity is FarRight not Right).**
Player slips Right→FarRight (tween in progress; was Center→Right under 3-lane). Player immediately taps slip-right. F-4: `projected_lane = FarRight`, slip-right → edge no-op. Input discarded immediately. Edge-absorb animation plays now (not deferred to tween completion). `edge_absorb_trigger_count` increments. `has_queued_input` remains false. PM settles at FarRight with no queued input.

**EC-13 — IDLE→COUNTDOWN during a tween (safety rule).**
Architecturally impossible under RSM rules (RUNNING cannot transition to COUNTDOWN). Defensive invariant: if PM receives `OnStateChanged` with `new_state == COUNTDOWN` while SLIPPING: cancel tween, reset `current_lane` and `target_lane` to Center (lane index 2 — R7-PM-PROPAGATION: Center remains canonical reset lane under 5-lane widening), `TweenProgress = 0.0`, clear buffer, `lateral_world_position = 0.0`, `lean_angle = 0`, `head_lean_angle = 0`. Avatar snaps to Center.

**EC-14 — DEAD during edge-absorb or near-miss beat animation.**
RSM transitions to DEAD while a non-tween animation is in progress.

- **Edge-absorb:** Always fires during SETTLED (`ERunSlipState::SETTLED`). TweenProgress is 0.0. On DEAD entry: `lean_angle` and `head_lean_angle` freeze at current animation values. Mesh transform holds mid-animation pose. No ease-out, no snap to neutral. PM's `current_lane` is unchanged.
- **Near-miss beat** (via `TriggerNearMissBeat()`): May fire during SETTLED or during an active SLIPPING tween. (R7-PM-PROPAGATION-REVIEW — stale `OnSlipMidpoint` detection-path language corrected: Pull-Wave Rule 11 reads `PM.current_lane` directly at the wave's LANDED-entry tick — see `player-movement-presentation.md` § Near-Miss Beat → Trigger API.) On DEAD entry during a near-miss beat: lean values freeze at their current composite value (tween lean + near-miss dip overlay). The near-miss animation does **not** reset tween lean; both contributions are present in the frozen pose.

In all cases: no ease-out, no snap to neutral. Death Replay receives the frozen visual state.

**EC-15 — Edge-absorb animation fires while body is mid-lean from an active tween (F-4 path).**
F-4 discards a buffer input and fires edge-absorb now (not deferred). The body is already rotating via F-5. The edge-absorb lean is **additive** on top of the active tween lean, computed via F-6: `edge_absorb_raw = lerp(MAX_LEAN_ANGLE_DEG × 0.35, 0.0, edge_absorb_t) × lean_sign`, where `lean_sign` is the in-flight tween direction and `edge_absorb_t` advances from 0.0 over `EDGE_ABSORB_DURATION_S`. The additive sum is clamped to `±(MAX_LEAN_ANGLE_DEG × 1.2)` — the only place in PM where a clamp is applied to lean angle. The active tween continues unaffected. The edge-absorb damped return (via F-6 timer) plays simultaneously with the tween's Phase 2/3 translation.

## 6. Dependencies

### Upstream (systems PM mechanics depends on)

| System | What PM needs | Source |
|---|---|---|
| **Input System** | `slip-left` and `slip-right` events delivered to PM while RUNNING | input-system.md — `cancel-slip` removed from PM's contract; provisional interface otherwise confirmed unchanged (see §9 OQ-3) |
| **Run State Machine** | `current_state`, `is_paused`, `resume_grace` per tick; `OnStateChanged` and `OnPausedChanged` delegates | run-state-machine.md — Rule 19 (resume_grace contract), RESUME_GRACE_S knob |

### Downstream (systems that depend on PM mechanics)

| System | What they consume | PM's obligation |
|---|---|---|
| **Pull-Wave System** | `lateral_world_position` — wave targeting; `current_lane` + `movement_state` — read directly at LANDED tick by Pull-Wave Rule 11 (R7-PM-PROPAGATION-REVIEW corrected: `OnSlipMidpoint` is NOT the near-miss detection input; PM may retain or remove the delegate at its discretion); `TriggerNearMissBeat()` — near-miss animation trigger (Pull-Wave → PM, forwarded to presentation). **Pull-Wave R7 specifies `TELEGRAPH_WINDOW_FLOOR_S = 0.65s`** (raised from 0.6s at R2 Cluster E). | `lateral_world_position` accurate each tick; `current_lane` returns SOURCE lane throughout SLIPPING (§3 Rules 4 + 7 — BINDING per Pull-Wave R7 Rule 11); `OnSlipMidpoint` is optional under R7+ (no Pull-Wave consumer; if retained, fires once per tween at TweenProgress ≥ 0.5); `TriggerNearMissBeat()` method available at runtime |
| **Collision System** | Actor world position (via APawn transform) | PM moves the actor's collision geometry to `lane_world_x(target_lane)` on tween start; visual mesh position is separate (see §3 Cross-Component Interfaces > Rotation Implementation) |
| **Camera System** | `lateral_world_position` — lateral framing | Accurate each tick |
| **Death Replay** | `current_lane` + `lateral_world_position` snapshot at DEAD entry | Values must be frozen before Death Replay reads them — PM's `OnStateChanged` handler must register first (see §3 Cross-Component Interfaces > Death Replay Registration Order) |

### Bidirectional Notes

- RSM GDD (Rule 19) requires PM to subscribe to `OnPausedChanged` to freeze/resume movement. See §3 Cross-Component Interfaces > Delegate Binding Contract for the required pattern.
- Input System GDD (Interactions section) lists PM as the consumer of slip events. `cancel-slip` removed from PM's contract. The Input System haptic vocabulary has expanded past the original 3-state set; PM is responsible only for its own two events (`SlipConfirmed` and `BufferDrop`), fired directly via `IHapticDispatch::Fire()` without routing through IS. See `input-system.md` §Haptic Vocabulary for the full IS-owned haptic taxonomy. The Near-Miss haptic event `EHapticEvent::NearMiss` (R11a §5 DR-D.4 NEW; sub-50 ms low-amplitude soft pulse; gated on `near_miss_haptic_enabled` setting) is owned by `player-movement-presentation.md` § Near-Miss Beat at the dispatch site.
- Pull-Wave, Collision, Camera, and Death Replay GDDs are not yet authored. When designed, each must reference the §3 Cross-Component Interfaces (Mechanics-Side) section above and must not assume internal PM structure.
- **Forward motion ownership gap.** See §3 Cross-Component Interfaces > Forward Motion Dual-Writer. Resolve via OQ-2 ADR before implementation.
- **Tick ordering.** See `player-movement-platform.md` § Tick Ordering (engine-level concern routed to platform). Dependency on RSM OQ-7 resolution.
- **Thread-safety contract for `is_paused` / `resume_grace`.** PM reads as per-tick polls from RSM on GameThread. RSM OQ-7 ADR must confirm GameThread marshalling of Android event-thread writes. PM takes no independent responsibility.
- **`resume_grace` timing authority.** PM reads `resume_grace` as an opaque boolean from RSM. PM does not independently time the grace window and must not call `FPlatformTime::Seconds()` for this purpose.
- **OQ-6 — 0.6s telegraph window is a blocking pre-condition for the Pull-Wave GDD.** Value propagated to `game-concept.md` Pillar 2 (done). The Pull-Wave GDD must not use any other telegraph value. If 0.6s changes, skill window math must be recalculated and the §3 Cross-System Interface Table updated.

### Cross-sub-GDD forward contracts (mechanics ↔ presentation + mechanics ↔ platform)

Per decomposition plan §4, the mechanics sub-GDD has 7 forward contracts on `player-movement-presentation.md` (all 5 plan §4.1 canonical rows covered — some rows spread across multiple items — + 1 additional beyond plan §4.1) and 8 forward contracts on `player-movement-platform.md` (all 6 plan §4.2 canonical rows covered + 1 additional beyond plan §4.2). Per Step 3 execution (2026-07-01), plan §4.1 / §4.2 canonical contracts are enumerated with the plan §4.4 four-element discipline (source, consumed, binding invariant, propagation); additional-beyond-plan items (mech↔pres item 5 Slip cue duration + mech↔plat item 5 Hardware Contract gating context) pre-date Step 3 verification and retain their lighter format from PASS 4-8 authoring.

**Mechanics ↔ Presentation (7)**:
1. **Commitment-tell trigger + SlipConfirmed haptic co-dispatch** (plan §4.1 rows #2 + #5 SlipConfirmed portion): mechanics fires `FireCommitmentTell()` on every SETTLED→SLIPPING transition (§3 Cross-Component Interfaces > Slip Input Dispatch); presentation owns the `LeadingFaceFlash` material parameter rendering + 200 ms cadence cap. The SETTLED→SLIPPING transition ALSO dispatches `IHapticDispatch::Fire(EHapticEvent::SlipConfirmed)` at the same site — the haptic is a distinct contract face from the visual flash: presentation's cadence-cap suppresses the visual flash but NOT the haptic (cadence-cap is visual-only per presentation §4 F-COMMIT-CADENCE-CAP). **Binding invariant**: `commitment_tell_fire_count` increments per SETTLED→SLIPPING transition; presentation may render or suppress (cadence cap) but MUST NOT modify the counter. **Co-ownership with Input System**: `EHapticEvent::SlipConfirmed` enum membership is co-owned per `input-system.md` §Haptic Vocabulary — PM declares SlipConfirmed as a downstream-consumer obligation on IS's vocabulary taxonomy; IS lists PM as the consumer. **Propagation**: counter is mechanics-owned (presentation reads only); if IS revises the enum, mechanics dispatch site re-emits under new membership; if presentation retunes the haptic profile, platform's `IHapticDispatch` API surface inherits the change (see plat↔pres item 2 in `player-movement-platform.md` §6 for the OS-state gate composition path).
2. **Edge-absorb trigger + EDGE_ABSORB_DURATION_S lockstep** (plan §4.1 row #3): mechanics fires `FireEdgeAbsorb()` on Rule 1 / F-4 paths (§3 Slip Input Dispatch) and drives the `edge_absorb_active` state flag from F-6; presentation owns the audio cue rendering (§3 Audio > Edge-absorb row) + visual envelope read (§3 Edge-Absorb Animation). **Binding invariant**: when `edge_absorb_active == true`, presentation fires the edge-absorb animation + cue; presentation's Edge-Absorb Tell duration MUST match mechanics-owned `EDGE_ABSORB_DURATION_S` (§7 default 0.27 s, safe range [0.20, 0.35]). **Propagation**: if mechanics changes `EDGE_ABSORB_DURATION_S`, presentation re-validates its Edge-Absorb Tell timing envelope; F-6 animation timing remains in mechanics §4 as the source of truth.
3. **Buffer-drop feedback** (plan §4.1 row #5 BufferDrop portion): mechanics fires `FireBufferDropHapticAndAudio()` on Rule 3 path; presentation owns the `IHapticDispatch::Fire(EHapticEvent::BufferDrop)` haptic dispatch + audio sting rendering. `EHapticEvent::BufferDrop` enum membership is co-owned per `input-system.md` §Haptic Vocabulary (see item 1 for the co-ownership framing that applies uniformly across the three PM-owned haptic events).
4. **Near-miss beat forwarding** (plan §4.1 row #5 NearMiss portion): mechanics `TriggerNearMissBeat()` call site (received from Pull-Wave) forwards to presentation animation playback (avatar Y-dip + audio swell + opt-in `IHapticDispatch::Fire(EHapticEvent::NearMiss)` haptic dispatch). `EHapticEvent::NearMiss` enum membership is co-owned per `input-system.md` §Haptic Vocabulary (see item 1 for the co-ownership framing).
5. **Slip cue duration contract** *(additional beyond plan §4.1)*: mechanics owns `SLIP_TWEEN_DURATION_S` (§7); presentation owns `audio_cue_ratio` (§7); cue duration = `SLIP_TWEEN_DURATION_S × audio_cue_ratio`.
6. **F-6 fade-out ≥ commitment-tell hold** (plan §4.1 row #1): mechanics §4 F-6 owns the edge-absorb tail fade-out duration; presentation §3 Commitment-Tell + §4 F-COMMIT-CADENCE-CAP owns the flash hold-then-decay envelope. **Binding invariant**: F-6 fade-out duration (post-hold decay phase) MUST be ≥ commitment-tell hold duration (2 frames minimum ≥ 33 ms at 60 fps; ≥ 66 ms at 30 fps) so no Override mid-fade-out coincides with a new commitment-tell flash. **Propagation**: if mechanics retunes fade-out frames (F-6 curve or `EDGE_ABSORB_DURATION_S` adjustment), presentation re-validates AC-COMMIT-FLASH-CADENCE setup — closes the timing-collision window where a stale F-6 decay tail masks a fresh commitment-tell peak.
7. **TweenProgress Phase-1 shelf visibility at 60 fps** (plan §4.1 row #4): mechanics §4 F-2 owns the tween accumulator + F-3 owns `lateral_world_position` output driven by SLIP_CURVE sampled at TweenProgress; presentation §3 Slip Tween Animation reads the sampled position for the bend animation. **Binding invariant**: SLIP_CURVE Phase 1 shelf `[0.0, 0.20]` MUST remain visible for at least one rendered frame at 60 fps at the `SLIP_TWEEN_DURATION_S` safe-range floor (0.10 s) — first-tick TweenProgress = `raw_dt_60 / 0.10 = 0.0167 / 0.10 = 0.167`, inside the shelf. **Propagation**: mechanics-owned timing (SLIP_CURVE authored asset + `SLIP_TWEEN_DURATION_S` safe-range floor jointly enforce the shelf); presentation reads only — no independent re-sampling. If mechanics widens `SLIP_TWEEN_DURATION_S` safe range below 0.10 s or re-authors SLIP_CURVE to compress the shelf below `[0.0, 0.20]`, presentation re-validates the bend arc against the new shelf envelope.

**Mechanics ↔ Platform (8)**:
1. **`effective_dt` definition (B-F6-3 CLOSED at PASS 7)**: mechanics §4 F-2/F-6 consume; platform §4 F-PROLOGUE owns the canonical TickComponent prologue definition (`raw_dt = FApp::GetDeltaTime()` + `effective_dt = clamp(raw_dt, 0.0f, MAX_SLIP_DT_S)` — single-source for both watchdog and tween advance; full DT-source rationale in `player-movement-platform.md` §4 F-PROLOGUE).
2. **Shipping-Safety policy enforcement**: mechanics §4 F-2 prologue implements the SLIP_TWEEN persistent clamp; platform §3 Shipping-Safety Enforcement Policy owns the policy + AC family.
3. **`SLIP_TWEEN_DURATION_S` safe range**: mechanics §7 declares the knob + safe range [0.10, 0.15]; platform §3 enforcement table consumes it.
4. **`ERunSlipState` ↔ `EMovementState` ordinal lockstep**: mechanics §3 Movement State Enum declares; platform §3 Shipping-Safety table enforces via static_assert.
5. **Hardware Contract gating context** *(additional beyond plan §4.2)*: mechanics §3 Cross-System Interface Table inherits the platform watchdog broadcast (`is_hw_performance_degraded`); Pull-Wave consumption of mechanics-side `current_lane` + `movement_state` is unaffected by the gate.
6. **Tick Ordering**: mechanics's RSM dependency (§3 RSM Storage Contract) requires the Tick Ordering OQ-1 resolution authored in platform §3 Tick Ordering.
7. **F-BARRAGE-SURVIVABILITY-INVARIANT input-flow** (plan §4.2 row #1): mechanics §7 owns the input knobs `SLIP_TWEEN_DURATION_S` (safe range [0.10, 0.15]) + `MAX_SLIP_DT_S` (safe range [0.020, 0.06]) + `REACTION_BUDGET` (per §7); platform §4 F-BARRAGE-SURVIVABILITY-INVARIANT-CORNERS owns the frame-quantized derivation matrix; Wave Spawner M=3 PEAK gate is the downstream consumer of the derived envelope. **Binding invariant**: `SLIP_TWEEN × MIN_ESCAPE_SLIPS + REACTION_BUDGET ≤ TELEGRAPH_WINDOW_FLOOR_S` MUST hold at all supported framerates × MAX_SLIP_DT_S safe-range × SLIP_TWEEN safe-range corners (see platform §4 F-BARRAGE-SURVIVABILITY-INVARIANT-CORNERS sensitivity matrix). **Propagation**: if mechanics changes any input knob's safe range, platform re-derives the frame-quantized survivability table + re-validates the Wave Spawner M=3 PEAK gate contract; breach at any corner triggers mechanics + platform + Wave Spawner lockstep re-tuning per R10a §B-LEAN-tension precedent.
8. **`MIN_ESCAPE_SLIPS = 2` constant lockstep** (plan §4.2 row #6): mechanics §7 F-BARRAGE math consumes the registry-bound `MIN_ESCAPE_SLIPS = 2` (see R10a §B-LEAN-tension binding); platform §3 Hardware Contract math consumes the same constant; platform §8 AC-SS-E (compile-time `static_assert`) enforces that the local C++ value matches design intent. **Binding invariant**: `MIN_ESCAPE_SLIPS` constant value MUST match between mechanics F-BARRAGE math + platform Hardware Contract math + registry declaration; any drift triggers AC-SS-E static_assert failure at PM compile unit. **Propagation**: registry → mechanics + platform lockstep; OQ-7 future yaml→C++ pipeline (see mechanics §9 OQ-7) closes the registry→code drift gap by generating the local C++ constant from the registry entry (pre-OQ-7 closure, drift is caught only at PM compile time via AC-SS-E).

Step 3 execution (2026-07-01) closed the plan §4.1 canonical coverage (2 NEW items 6 + 7 for plan rows #1 + #4; 2 ENHANCED items 1 + 2 for plan rows #3 + #5 SlipConfirmed portion) + plan §4.2 canonical coverage (2 NEW items 7 + 8 for plan rows #1 + #6) per the 2026-06-30 verification report (`design/gdd/reviews/player-movement-decomposition-step3-verification-2026-06-30.md`). Plan §4.3 (Presentation ↔ Platform) is out-of-scope for this sub-GDD — see `player-movement-presentation.md` §6 + `player-movement-platform.md` §6 for the reciprocal paragraphs.

## 7. Tuning Knobs

Mechanics owns the lane / tween / lean / edge-absorb / EC-15 tuning surface. The `audio_cue_ratio` row (slip cue duration anchoring) routes to `player-movement-presentation.md` §7 Tuning Knobs per the audio-perception ownership split. Platform-specific tuning knobs (watchdog parameters) are scoped PM-internal per R10a-9 and declared in `player-movement-platform.md` §7.

| Knob | Type | Default | Safe Range | Affects |
|---|---|---|---|---|
| `LANE_WIDTH_M` (R7-PM-PROPAGATION — REPLACES `LANE_OFFSET_CM` retired; R7-PM-PROPAGATION-REVIEW safe-range floor 0.75 → 0.85; R10a §2.5 — floor justification rewritten on perceptual readability grounds, not thumb-target [Input System uses a left/right dead-band split with no per-lane tap target — thumb-target geometry never applied to lane spacing; R9 category-error finding]) | float (m) | 1.0 | 0.85 – 1.5 | Per-lane width in world meters (canonical knob — matches Pull-Wave R7 GDD's shared constant). Derived: `LANE_WIDTH_CM = LANE_WIDTH_M × 100`. At 5 lanes, total track span = `4 × LANE_WIDTH_M` (which at default 1.0 m yields 4.0 m; safe range yields 3.4 – 6.0 m). Lane centers at `(lane_index − 2) × LANE_WIDTH_M`. Pull-Wave targeting width; camera framing. **Semantic shift from prior `LANE_OFFSET_CM`**: prior knob measured per-edge offset (200 cm), new knob measures per-lane width (100 cm at default). Track span scales with the knob (see prose below for safe-range span values). **Perceptual readability floor (R10a §2.5 — replaces R7-PM-PROPAGATION-REVIEW thumb-target floor)**: safe-range lower 0.85 m is set by the minimum readable inter-lane visual gap on a 6-inch portrait mobile under the default camera framing calibration. At default camera framing (track centerlines occupy the central ~70% of horizontal viewport on a 6-inch portrait device), `LANE_WIDTH_M = 0.85` produces a per-lane on-screen gap of ≈ 12 mm — at the perceptual floor for distinguishing adjacent lanes at a glance during 60-second high-speed play. Below 0.85, adjacent lanes blur perceptually under speed; the player loses the ability to read `which lane the wave targets` and reverts to position-counting instead of direction-anticipation (Pillar 5 / Pillar 1 break). Above 1.5 m, FarLeft/FarRight lane centers exit the central framing region and require concurrent camera GDD recalibration — see camera framing upper-bound note. **Camera framing upper-bound note (R7-PM-PROPAGATION-REVIEW carry-over)**: values above 1.2 m widen FarLeft/FarRight centers past the default camera framing calibration (≥ ±2.4 m world) — requires camera framing recalibration before use. Values 1.2 – 1.5 m are valid only with concurrent camera GDD update. |
| `SLIP_TWEEN_DURATION_S` | float (s) | 0.15 | **0.10 – 0.15** (R10a §B-LEAN-tension Hold — safe-range upper bound retained at 0.15s on Player Fantasy + camera/shader/thermal grounds, NOT on survivability arithmetic which now permits widening to ~0.225s under MIN_ESCAPE_SLIPS=2; supersedes R7-PM-PROPAGATION-REVIEW zero-margin survivability framing which was mathematically false under Option (c)) | Duration of a full lane transition; core "bend" expressiveness. **R10a §B-LEAN-tension binding (locked 2026-06-14)**: under the registry-bound `MIN_ESCAPE_SLIPS = 2`, the F-BARRAGE-SURVIVABILITY-INVARIANT now has 150 ms continuous-form margin at default SLIP_TWEEN = 0.15s (see `player-movement-platform.md` § Hardware Contract frame-quantized table — all four supported framerates hold). The arithmetic permits SLIP_TWEEN up to ~0.225s before the invariant breaches. The safe-range upper bound is HELD at 0.15s on three non-survivability grounds: (1) **Player Fantasy "body before mind"** — above 0.15s the bend reads as a deliberate lean rather than a reflex; the slip stops feeling like the body moving ahead of the mind; (2) **camera + shader coherence** — track-following shader timing budgets are calibrated against 100–150 ms tween bands; above 150 ms the shader artifacts (motion-trail, dust-puff timing) decouple from the slip arc; (3) **thermal sustainability of the per-tween workload** — longer tweens hold the high-update tick density across more frames, marginally increasing the per-run thermal budget on min-spec devices. Any future widening past 0.15s requires creative-director adjudication. The 0.10s lower bound is preserved (Phase 1 shelf invisibility floor at 30 fps — `0.0333 / 0.10 = 0.333` shelf endpoint is past TweenProgress=0.20, but `0.0333 / 0.15 = 0.222` is still past — the shelf endpoint visibility depends on framerate not just SLIP_TWEEN; the 0.10 floor is a defensive margin for designer override). **R10a §1.2 Shipping-Safety enforcement (platform-owned)**: `ensureMsgf(SLIP_TWEEN_DURATION_S >= 0.10f && SLIP_TWEEN_DURATION_S <= 0.15f, ...)` at BeginPlay (dev-build); persistent F-2 clamp + per-N-ticks rate-limited log in Shipping. The R7-PM-PROPAGATION-REVIEW one-shot `bFloorGuardFired` semantic is RETIRED per `player-movement-platform.md` § Shipping-Safety Enforcement Policy. See platform §8 AC-21 (updated semantic) and AC-SS-A. |
| `SLIP_CURVE_ASSET` | UCurveFloat hard reference | ease-in-out with Phase 1 shelf | — | Shape of lateral motion arc; see §4 Authored Asset Contracts |
| `MAX_SLIP_DT_S` | float (s) | 0.05 | **0.020 – 0.06** | DeltaTime cap in F-2; prevents tween-skip on hitched frames. **Constraint: `MAX_SLIP_DT_S < SLIP_TWEEN_DURATION_S × 0.80` must hold.** Safe range max (0.06) satisfies this at minimum tween duration (0.10 × 0.80 = 0.08 > 0.06 ✓). |
| `LEAN_CURVE_ASSET` | UCurveFloat hard reference | shape per §4 Authored Asset Contracts | — | Body lean angle over tween (F-5); drives the "bend" expressiveness |
| `MAX_LEAN_ANGLE_DEG` | float (°) | 10.0 | 8.0 – 12.0 | Peak body lean angle (actor local X axis); below 8° unreadable on mobile; above 12° reads as exaggerated fall |
| `HEAD_LAG_PROGRESS` | float [0.0, 1.0] | 0.10 | **0.08 – 0.12** | TweenProgress offset for head secondary motion in F-5; controls "body leads, head follows" lag. **Constraint: `LeanCurve(1.0 − HEAD_LAG_PROGRESS)` must be ≈ 0.0 to prevent head-snap on tween completion (see §4 Authored Asset Contracts). Values outside [0.08–0.12] violate this constraint against the required LeanCurve shape and require LeanCurve re-authoring.** |
| `ARM_LEAD_PROGRESS` | float [0.0, 1.0] | 0.05 | 0.03 – 0.08 | TweenProgress advance for arm/shoulder secondary motion in F-5; controls "arm initiates, body follows" lead. Arm reads LeanCurve at `TweenProgress + ARM_LEAD_PROGRESS` (clamped to 1.0). No LeanCurve authoring constraint beyond t=1.0 ≈ 0.0 (already required). |
| `EDGE_ABSORB_DURATION_S` | float (s) | 0.27 | 0.20 – 0.35 | Total duration of the edge-absorb lean arc (F-6) including hold and return phases; used as the time denominator in F-6's `edge_absorb_progress` computation. |
| `EDGE_ABSORB_CURVE_ASSET` (R10a §1.1 NEW) | UCurveFloat soft reference | linear decay `y = 0.35 × (1 − x)` fallback | — | Shape of F-6 edge-absorb tail (body / head / arm via staggered sampling) — see §4 Authored Asset Contracts > EDGE_ABSORB_CURVE_ASSET. Authoring is polish-phase; null-asset linear fallback is acceptable for prototype + integration testing. Replaces the pre-R10a hard-coded linear lerp in F-6's body. |
| `EC15_F6_DECAY_COEFFICIENT` (R11a §6.2 NEW) | float (private const) | **0.7** | **0.6 – 1.0** | F-6 contribution scaling multiplier during SLIPPING (EC-15 mid-tween edge-absorb path). Multiplier formula: `slipping_mult = max(0, 1 − TweenProgress × c)`. Default 0.7 produces clamp-avoidance margin ≥ 5× the AC-F6-E measurement tolerance at the EC-15 worst case. Below 0.6 reintroduces clamp risk at TP=0.85; above 1.0 zeroes F-6 contribution at TP=1.0 (defeats F-6 intent at tween completion). See §4 F-6 lean formula for the tuning rationale derivation. |

**LANE_WIDTH_M (R7-PM-PROPAGATION — REPLACES `LANE_OFFSET_CM`; R10a §2.4 + §2.5 — total-track-span prose rewritten to scale with the knob, thumb-target justification removed and replaced with perceptual readability floor):** Narrower values tighten risk geometry (waves threaten more lanes simultaneously); wider values give breathing room but reduce tension. Changes must be cross-referenced with Pull-Wave targeting radius, Collision capsule width, and the camera framing calibration. **R7-PM-PROPAGATION migration note**: prior `LANE_OFFSET_CM` (per-edge-offset semantics; default 200 cm; safe range 120–300 cm on the 3-lane model) is RETIRED. New `LANE_WIDTH_M` (per-lane-width semantics; default 1.0 m = 100 cm; safe range 0.85–1.5 m) measures a different quantity. Tuners migrating projects from the prior knob must NOT directly substitute `LANE_OFFSET_CM` for `LANE_WIDTH_CM` — the prior value would yield a 5-lane span of 1000 cm = 10 m (impractical on mobile). The recommended mapping at migration time: previous 3-lane track with `LANE_OFFSET_CM = 200` (span 400 cm) → new 5-lane track with `LANE_WIDTH_M = 1.0` (= LANE_WIDTH_CM 100; span 400 cm at the default tuning). **Total track span scales with the knob (R10a §2.4 — supersedes prior "400 cm preserved at all tunings" claim, which was an arithmetic error introduced when the R7-PM-PROPAGATION-REVIEW pass tightened LANE_WIDTH_M safe range without re-deriving span)**: total span between FarLeft and FarRight lane centers = `4 × LANE_WIDTH_M`. Safe-range span boundaries:

| `LANE_WIDTH_M` | Track span (FarLeft → FarRight center distance) | Notes |
|---|---|---|
| 0.85 m (safe floor) | 3.40 m | Perceptual-readability floor (see below) |
| 1.00 m (default) | 4.00 m | Original 3-lane LANE_OFFSET_CM=200 span preserved at default tuning |
| 1.20 m | 4.80 m | Camera framing calibration upper bound (see camera framing note) |
| 1.50 m (safe ceiling) | 6.00 m | Requires concurrent camera GDD recalibration |

The "400 cm preserved at all tunings" claim from the R7-PM-PROPAGATION revision pass was a leftover from when the original 3-lane track happened to coincide with the default 5-lane default tuning; under the tunable knob, span is a derived value, not an invariant. Pull-Wave targeting, Collision, and Camera consumers MUST read the derived `4 × LANE_WIDTH_M` value at tune time, not assume a constant 4.0 m span. **Perceptual readability floor binding (R10a §2.5 — replaces former thumb-target floor binding)**: the 0.85 m floor is set by minimum readable inter-lane visual gap on a 6-inch portrait mobile under the default camera framing — at the floor, per-lane on-screen width ≈ 12 mm, the lower bound for distinguishing adjacent lanes at a glance during 60-second high-speed play. Below 0.85 m, lane discrimination collapses perceptually under speed, breaking Pillar 5 (skill-as-visible direction-anticipation). The prior R7-PM-PROPAGATION-REVIEW thumb-target argument (5 lanes × per-lane width vs. an 11 mm capacitive-touch reliable-target floor) was a category error: the Input System uses a left/right dead-band split with no per-lane tap target — thumb-target geometry never applied to lane spacing. The arithmetic floor (0.85 m) is unchanged but the perceptual justification supersedes the thumb-target rationale, which is retracted.

**SLIP_TWEEN_DURATION_S (R10a §B-LEAN-tension binding — full rewrite; SUPERSEDES R7-PM-PROPAGATION zero-margin framing which is mathematically false under MIN_ESCAPE_SLIPS=2):** Target range for the "bend" fantasy is 120–180ms; the BINDING safe range is **[0.10, 0.15]s** (per R10a §B-LEAN-tension = Hold). Below 100ms (safe-range minimum), the Phase 1 shelf becomes invisible at 30fps under any tween duration — but at the documented 60fps lock, the shelf is visible at 0.10s (first tick at TweenProgress = 0.167, still inside the [0.0, 0.20] shelf). The lower bound is a defensive margin for designer override scenarios. **Upper-bound rejustification (R10a §B-LEAN-tension)**: above 0.15s, the survivability arithmetic under MIN_ESCAPE_SLIPS=2 still holds up to ~0.225s — but the upper bound is held at 0.15s on three non-survivability grounds:

1. **Player Fantasy** — the "body before mind" beat reads as a reflexive lean below 150ms; above 150ms it reads as a deliberate lean. The mechanical commitment moment shifts perceptually.
2. **Camera + shader coherence** — track-following shader timings (motion-trail, dust-puff fade) are calibrated to 100–150ms tween bands. Above 150ms the shader timings decouple from the slip arc.
3. **Thermal sustainability** — longer tweens hold the high-update tick density across more frames, marginally increasing per-run thermal budget on min-spec devices.

**Pre-R10a "above 0.15s violates the invariant immediately" claim is RETRACTED** — under MIN_ESCAPE_SLIPS=2 the arithmetic permits widening. Any future widening past 0.15s is OUT-of-scope for R10a and requires creative-director adjudication. If set below 0.001s at runtime, F-2 logs an error and applies the persistent clamp (per platform §8 AC-SS-A) — pre-R10a one-shot reset semantic is RETIRED.

**MAX_SLIP_DT_S:** Default 0.05s. Safe range 0.020–0.06. The constraint `MAX_SLIP_DT_S < SLIP_TWEEN_DURATION_S × 0.80` must hold at all times — violations allow a single-frame tween collapse that skips the Phase 1 shelf entirely. The prior safe-range maximum of 0.08 violated this constraint at minimum tween duration (0.10); corrected to 0.06.

**LEAN_CURVE_ASSET / HEAD_LAG_PROGRESS / ARM_LEAD_PROGRESS:** Calibrate together. `HEAD_LAG_PROGRESS` controls how far behind the head reads the LeanCurve; safe range is **[0.08–0.12]** — values outside this range violate the head-snap prevention constraint (`LeanCurve(1.0 − HEAD_LAG_PROGRESS) ≈ 0.0`) against the required LeanCurve shape and would require LeanCurve re-authoring. `ARM_LEAD_PROGRESS` controls how far ahead the arm/shoulder reads the LeanCurve (default 0.05; safe range [0.03–0.08]); no additional LeanCurve authoring constraint beyond t=1.0 ≈ 0.0. Default 10° / HEAD_LAG_PROGRESS 0.10 / ARM_LEAD_PROGRESS 0.05 was calibrated at 150ms tween and 60fps.

**RESUME_GRACE_S:** Owned by RSM (`RESUME_GRACE_S = 0.5s`). Not configurable by PM. Documented here for cross-reference — PM's freeze/resume behavior depends on this value.

## 8. Acceptance Criteria

> **Test strategy.** PM exposes the following properties for unit and integration test inspection (no internal mocking required):
> - `current_lane`, `target_lane` — lane state
> - `lateral_world_position` — visual mesh position
> - `tween_progress` — current tween fraction
> - `movement_state` — SETTLED / SLIPPING
> - `lean_angle`, `head_lean_angle`, `arm_lean_angle` — body lean outputs
> - `collision_world_x` — collision geometry position (diagnostic-read-only)
> - `has_queued_input`, `queued_input_direction` — buffer state
> - `slip_complete_count`, `edge_absorb_trigger_count`, `commitment_tell_fire_count` — event counters
>
> **Unit tests** read these properties directly — no event subscription required. **Integration tests** may additionally subscribe to `OnSlipMidpoint` (see AC-24). **Test curves:** unit tests must pin `SlipCurve` and `LeanCurve` to known authored shapes (not the shipped production curves) — use a linear baseline for `SlipCurve` and a unit-ramp-then-plateau for `LeanCurve` so `GetFloatValue` returns predictable values. Baseline: reset all counters to 0 and buffer to empty via RSM COUNTDOWN transition before each test.

Mechanics owns the lane / tween / lean / counter / F-6 / source-lane AC family. Per decomposition plan §5 BLOCKING-ownership matrix, three AC families route to siblings:

- **AC-21 (SLIP_TWEEN persistent clamp) + AC-SS-A through E (Shipping-Safety) + AC-HW-A/B/C (Hardware Contract)** → `player-movement-platform.md` §8 (B-QA-1 + B-PERF-1 + B-SHIP-1 ownership).
- **AC-29 (Commitment-Tell) + AC-COMMIT-FLASH-CADENCE + AC-NEARMISS-HAPTIC + AC-AUDIO-CUE-PROPORTIONALITY + AC-AUDIO-CUE-DUCKING + AC-AUDIO-PAN-NEUTRALITY-RETIRED** → `player-movement-presentation.md` §8 (B-BANNER-1 + B-CERT-1 + B-AUDIO-1/2 ownership).

### Movement Model

**AC-01 (R7-PM-PROPAGATION updated for 5-lane widening):** Given SETTLED at Center (lane index 2) and `slip-right` received, when tween completes, then `current_lane == Right` (lane index 3 — one lane right of Center under 5-lane widening), `lateral_world_position == LANE_WIDTH_CM` (= +100 cm at default, NOT +200 cm under the prior 3-lane LANE_OFFSET_CM model) ± 0.1cm, `movement_state == SETTLED`, `slip_complete_count == 1`.

**AC-02 (R7-PM-PROPAGATION updated for 5-lane widening — edge identity is FarRight not Right):** Given SETTLED at FarRight (lane index 4 — the track-edge identity under 5-lane widening) and `slip-right` received (§3 Rule 1 edge no-op), then `current_lane` remains FarRight, `lateral_world_position` unchanged, `movement_state == SETTLED`, `slip_complete_count == 0`, `edge_absorb_trigger_count == 1`, `has_queued_input == false`. Mirror case: SETTLED at FarLeft (lane index 0) + slip-left produces identical assertion. SETTLED at Right (lane index 3) + slip-right is NOT an edge no-op — it produces a valid Right→FarRight tween.

**AC-03 (R7-PM-PROPAGATION updated — Lerp arguments unchanged in form; values update under 5-lane widening):** Given a tween executing Left→Center with `TweenProgress == 0.5`, when sampled, then `lateral_world_position == Lerp(lane_world_x(Left), lane_world_x(Center), SlipCurve->GetFloatValue(0.5))` ± 1cm. At default `LANE_WIDTH_M = 1.0` → `lane_world_x(Left) = −100 cm`, `lane_world_x(Center) = 0`.

### Input Buffer

**AC-04:** Given a tween executing Center→Right, when `slip-left` is received and the buffer is empty, then `has_queued_input == true` and `queued_input_direction == Left`. When the Center→Right tween completes, `movement_state == SLIPPING` (the buffered Right→Center slip has started) in the **same tick** as the first tween's completion — no idle tick separates them. `slip_complete_count == 2` after the second tween completes. `has_queued_input == false` once the buffer flushes.

**AC-05:** Given a tween executing and `has_queued_input == true`, when a second slip event arrives, then the second slip is dropped. `has_queued_input` remains true and `queued_input_direction` is unchanged.

**AC-06 (R7-PM-PROPAGATION updated — edge identity changed Left → FarLeft under 5-lane widening):** Given a tween executing Left→FarLeft (`target_lane = FarLeft`), when `slip-left` is received (F-4 edge pre-validation: projected FarLeft + slip-left = edge no-op — FarLeft (lane 0) is the left track edge under R7-PM-PROPAGATION 5-lane widening, NOT Left (lane 1) which was the edge under the prior 3-lane model), then `has_queued_input == false`, `edge_absorb_trigger_count == 1`, and buffer is empty at tween completion.

**AC-07:** Given a tween executing Center→Right, when `slip-left` is received into the buffer (`has_queued_input == true`, `queued_input_direction == Left`), then on tween completion PM enters SETTLED at Right and immediately begins the buffered slip (Right→Center). `slip_complete_count == 2` after both tweens complete.

### RSM Gating

**AC-08:** Given `RSM.current_state != RUNNING`, when any slip event is received, then the event is discarded immediately. `movement_state` remains SETTLED. `has_queued_input` unchanged.

**AC-09:** Given `RSM.current_state == RUNNING` and `RSM.is_paused == true`, when a slip event is received, then the event is discarded. `has_queued_input` is not affected. `movement_state` unchanged.

**AC-10:** Given `RSM.current_state == RUNNING`, `RSM.is_paused == false`, and `RSM.resume_grace == true`, when a slip event is received, then the event is discarded and `has_queued_input` is unchanged.

### Pause and Grace

**AC-11:** Given a tween executing with `TweenProgress = 0.4`, when `is_paused` becomes true, then `TweenProgress` stops advancing. Sampled on 10 consecutive ticks while paused: `TweenProgress` remains 0.4 ± 0.001. `has_queued_input` preserved through pause.

**AC-12:** Given a tween executing with `TweenProgress = 0.4`, when `RSM.resume_grace == true`, then `TweenProgress` stops advancing. Sampled on 5 consecutive ticks during grace: `TweenProgress` remains 0.4 ± 0.001.

**AC-13:** Given a paused mid-tween state (`TweenProgress = 0.4`, `has_queued_input == true`), when `resume_grace` expires, then the tween resumes from 0.4 and completes. The buffered slip begins in the **same tick** as tween completion — no idle tick separates them. `slip_complete_count == 2` after both tweens complete.

### Terminal States

**AC-14 (R10a — boundary samples TweenProgress=0.001 / 0.999 added per brief §2.1; pre-R10a R7-PM-PROPAGATION-REVIEW — `movement_state == SETTLED` assertion added per §3 Rule 7 parity fix; `TweenProgress` preserved at fractional value per DEAD freeze design):** Given a tween executing (Left→Center, `current_lane = Left`, `target_lane = Center`) with `TweenProgress = 0.55`, when RSM transitions to DEAD, then: `TweenProgress` is **preserved** at 0.55 (NOT reset — DEAD freeze preserves mid-arc visual pose; this differs from §3 Rules 8/9 which reset TweenProgress to 0.0 and snap visual to destination); `lateral_world_position == Lerp(lane_world_x(Left), lane_world_x(Center), SlipCurve->GetFloatValue(0.55))` ± 1cm; `current_lane == Center` (set to in-flight destination per §3 Rule 7); **`movement_state == ERunSlipState::SETTLED` (R7-PM-PROPAGATION-REVIEW NEW assertion — closes cross-system source-lane semantic defect against in-flight Pull-Wave LANDED ticks during run-termination drain; PM exits SLIPPING at DEAD entry even though TweenProgress is preserved)**; `lean_angle`, `head_lean_angle`, and `arm_lean_angle` all freeze at their TweenProgress=0.55 values — they are NOT reset to 0.0; `slip_complete_count == 0`; `has_queued_input == false`. Sampled on 5 consecutive ticks after DEAD entry: all frozen values unchanged.

**AC-14 boundary samples (R10a NEW per brief §2.1):** Repeat the AC-14 assertion at two additional TweenProgress boundary values to catch implementation paths that special-case mid-tween freeze versus near-start / near-end freeze:

- **TweenProgress = 0.001 (near-start boundary):** Same lane-transition setup as AC-14. RSM transitions to DEAD on the first tick after SETTLED→SLIPPING (one tick of progress accumulated). Assertions identical to AC-14 with `TweenProgress` preserved at 0.001 (NOT snapped to 0.0; NOT reset). `lateral_world_position == Lerp(lane_world_x(Left), lane_world_x(Center), SlipCurve->GetFloatValue(0.001))` ± 1cm — effectively at source lane but the assertion must use the Lerp form. `current_lane == Center` per §3 Rule 7. `movement_state == SETTLED`. Catches an implementation that special-cases "if TweenProgress < threshold, treat as SETTLED-pre-freeze" and resets to source.
- **TweenProgress = 0.999 (near-end boundary):** Same setup. RSM transitions to DEAD on the tick before SLIPPING→SETTLED would naturally fire. Assertions identical with `TweenProgress` preserved at 0.999 (NOT snapped to 1.0; NOT auto-completed). `lateral_world_position == Lerp(lane_world_x(Left), lane_world_x(Center), SlipCurve->GetFloatValue(0.999))` ± 1cm. `current_lane == Center`. `movement_state == SETTLED`. Catches an implementation that special-cases "if TweenProgress ≥ 0.99, treat as complete and snap to target" — this snap would be incorrect under §3 Rule 7 DEAD-freeze semantics.

**AC-15:** Given RSM transitions to DEAD while PM is SETTLED, then `current_lane`, `target_lane`, and `lateral_world_position` are unchanged. `has_queued_input == false`. `slip_complete_count` unchanged.

**AC-16:** Given a tween executing with `TweenProgress = 0.55`, when RSM transitions to COMPLETE, then in the **same frame**: `lateral_world_position` snaps to `lane_world_x(target_lane)`, `TweenProgress == 0.0`, `lean_angle == 0.0`, `head_lean_angle == 0.0`, `arm_lean_angle == 0.0`, `has_queued_input == false`, and `slip_complete_count == 0`.

**AC-17:** Given RSM transitions to ABORTED while a tween is executing, then in the **same frame**: `lateral_world_position` snaps to `lane_world_x(target_lane)`, `TweenProgress == 0.0`, `lean_angle == 0.0`, `head_lean_angle == 0.0`, `arm_lean_angle == 0.0`, `slip_complete_count == 0`, and `has_queued_input == false`.

### Lane Reset

**AC-18:** Given `slip_complete_count == 2`, `edge_absorb_trigger_count == 1`, and `commitment_tell_fire_count == 2`, when RSM transitions to COUNTDOWN, then: `current_lane == Center`, `target_lane == Center`, `lateral_world_position == 0.0`, `lean_angle == 0.0`, `head_lean_angle == 0.0`, `arm_lean_angle == 0.0`, `movement_state == SETTLED`, `has_queued_input == false`, `queued_input_direction == None`, `slip_complete_count == 0`, `edge_absorb_trigger_count == 0`, and `commitment_tell_fire_count == 0`.

### Buffer Discard on Non-RUNNING Exit

**AC-19:** Given `has_queued_input == true` and RSM leaves RUNNING (to any state), then `has_queued_input == false` after the state transition. The queued slip does not execute.

### Tween Formula

**AC-20:** (a) `SLIP_TWEEN_DURATION_S = 0.15`, `MAX_SLIP_DT_S = 0.05`, `DeltaTime = 0.016`, `TweenProgress_prev = 0.0`: one tick → `TweenProgress == 0.107` ± 0.001. (b) `DeltaTime = 0.0`: `TweenProgress` unchanged. (c) `DeltaTime = 2.0` (hitch): `effective_dt = 0.05`, `TweenProgress_delta == 0.333` ± 0.001 — no single-frame tween skip; clamped to 1.0 at completion.

**AC-22 (R7-PM-PROPAGATION updated — edge identity changed Right → FarRight under 5-lane widening; pre-conditions adjusted to reach FarRight from Center via 2-step sequence):** Given SETTLED at Right (lane 3), receives `slip-right` (Right→FarRight tween starts), then while executing receives another `slip-right` (F-4: projected FarRight + slip-right = edge no-op — FarRight (lane 4) is the right track edge under R7-PM-PROPAGATION 5-lane widening, NOT Right (lane 3) which was the edge under the prior 3-lane model), then `has_queued_input` remains false, tween completes: `slip_complete_count == 1`, `edge_absorb_trigger_count == 1`, `has_queued_input == false`.

### Collision Commitment

**AC-23 (R7-PM-PROPAGATION updated — target lane corrected to Center under 5-lane widening; latent inconsistency in original 3-lane spec also resolved):** Given SETTLED at Left (lane 1) and `slip-right` received (SETTLED→SLIPPING), when sampled **in the same event-handler call** as the transition (synchronous guarantee — no async or next-tick deferral), then: `collision_world_x == lane_world_x(Center)` (= 0 cm at default; lane 2 — one-lane-right of Left), `lateral_world_position == lane_world_x(Left)` (= −100 cm at default), and `TweenProgress == 0.0`. Verifies §3 Rule 2: collision commits synchronously to target lane on the transition frame, decoupled from visual position, before any tween progress has accumulated.

### Delegate Firing

**AC-24** *(integration test — subscribe to delegate):* Given a tween executing Center→Right, when `TweenProgress` first crosses 0.5, then `OnSlipMidpoint` fires with `source_lane == Center` and `target_lane == Right`. On subsequent ticks of the same tween, `OnSlipMidpoint` does not fire again. Does not fire for edge-absorb animations or during DEAD/COMPLETE/ABORTED.

### Buffer-Drop Feedback (mechanics-side counter assertion; presentation rendering AC lives in presentation §8)

**AC-25:** Given a tween executing and `has_queued_input == true`, when a second slip event arrives (buffer full), then the buffer-drop haptic fires and the buffer-drop audio sting fires **in the same event-handler call** as the drop (synchronous — no deferral to next tick). `has_queued_input` remains true; `queued_input_direction` unchanged. `slip_complete_count` unchanged. Presentation rendering specifics (haptic + audio sting profile) are owned by `player-movement-presentation.md` § Audio cue table > Buffer-drop row.

### Body Lean (F-5)

**AC-26 (R10a §1.1 — head/arm exception folded in alongside body):** Given PM is SETTLED (`TweenProgress = 0.0`) **with no active F-6 edge-absorb timer (`edge_absorb_active == false`)**, then `lean_angle == 0.0` ± 0.01°, `head_lean_angle == 0.0` ± 0.01°, and `arm_lean_angle == 0.0` ± 0.01° (see also AC-30). **During an active F-6 tail, all three lean angles may be non-zero while SETTLED — this is by design per the R10a §1.1 F-5/F-6 co-write contract** (see §4 F-6 → "F-5 / F-6 co-write contract" table). AC-F6-A asserts the head/arm values explicitly during F-6 tail.

**AC-27:** Given a tween executing with `TweenProgress = 0.20` and `LeanCurve->GetFloatValue(0.20) == 1.0`, then `|lean_angle| == MAX_LEAN_ANGLE_DEG` ± 0.1°. For right-slip: `lean_angle < 0`. For left-slip: `lean_angle > 0`. Additionally: `|head_lean_angle| == MAX_LEAN_ANGLE_DEG × LeanCurve->GetFloatValue(max(0.20 - HEAD_LAG_PROGRESS, 0.0))` ± 0.1° (head reads lagged TweenProgress — must be strictly less than peak at this moment). `|arm_lean_angle| == MAX_LEAN_ANGLE_DEG × LeanCurve->GetFloatValue(min(0.20 + ARM_LEAD_PROGRESS, 1.0))` ± 0.1° (arm reads ahead — at peak lean if curve is already at 1.0 by t=0.25).

**AC-28:** Given a tween executing with `TweenProgress = 0.5` (non-zero lean), when RSM transitions to COMPLETE, then in the **same frame** as the snap: `lean_angle == 0.0` ± 0.01°, `head_lean_angle == 0.0` ± 0.01°, and `arm_lean_angle == 0.0` ± 0.01° (see also AC-32).

### Arm Lean (F-5)

**AC-30 (R10a §1.1 — F-6 tail exception explicit):** Given PM is SETTLED (`TweenProgress = 0.0`) **and `edge_absorb_active == false`**, then `arm_lean_angle == 0.0` ± 0.01°. During an active F-6 tail, `arm_lean_angle` may be non-zero per the R10a §1.1 co-write contract — AC-F6-A asserts the explicit value.

**AC-31:** Given a tween executing with `TweenProgress = 0.5` (non-zero lean), when RSM transitions to DEAD, then `arm_lean_angle` freezes at its TweenProgress=0.5 value — it is NOT reset to 0.0 (matches AC-14 freeze-not-reset contract for all three lean outputs).

**AC-32:** Given a tween executing with `TweenProgress = 0.5` (non-zero lean), when RSM transitions to COMPLETE, then `arm_lean_angle == 0.0` ± 0.01° in the same frame as the snap (matches AC-16 reset contract).

### F-6 Edge-Absorb Tail Phase (R10a §1.1 NEW — closes head/arm decouple defect, §5.1 (a) Override timer-collision, 5-lane edge coverage, DEAD-mid-tail freeze parity)

**AC-F6-A — Head and arm alignment with body during F-6 tail (closes pre-R10a head/arm decouple defect):** Given PM is SETTLED at FarRight (lane 4), inputs `slip-right`, §3 Rule 1 fires edge no-op → F-6 activates with `edge_absorb_sign = −1.0`, `edge_absorb_local_timer = 0.0`. Sampled on the **F-6 tail tick** at `edge_absorb_progress = 0.5` (about 135 ms into the tail at default `EDGE_ABSORB_DURATION_S = 0.27s`):

- `lean_angle == EdgeAbsorbCurve(0.5) × MAX_LEAN_ANGLE_DEG × −1.0` ± 0.1° (body sample, default linear curve yields ≈ −1.75°)
- `head_lean_angle == EdgeAbsorbCurve(max(0.5 − HEAD_LAG_PROGRESS, 0.0)) × MAX_LEAN_ANGLE_DEG × −1.0` ± 0.1° (head reads progress 0.40 → ≈ −2.10°)
- `arm_lean_angle == EdgeAbsorbCurve(min(0.5 + ARM_LEAD_PROGRESS, 1.0)) × MAX_LEAN_ANGLE_DEG × −1.0` ± 0.1° (arm reads progress 0.55 → ≈ −1.575°)
- All three signs agree (all negative — right lean). The ordering `|head| > |body| > |arm|` holds at this sample point because the curve is decaying (head's earlier sample is closer to the peak).
- Critical assertion: `head_lean_angle != 0.0` and `arm_lean_angle != 0.0` — under the pre-R10a spec, both would be 0 (decoupled from body during SETTLED F-6 tail). The R10a §1.1 co-write contract closes this gap.

Mirror case: SETTLED at FarLeft + slip-left, all three angles positive (left lean) with `edge_absorb_sign = +1.0`. Same magnitude assertions.

**AC-F6-B — §5.1 (a) Override timer-collision boundary case (BINDING — locks the R10a §5.1 author decision; R11a §6.1 fade-out rewrite — asserts 2-frame linear ramp-down instead of instant zero):** Given PM is in F-6 tail phase with `edge_absorb_active == true`, `edge_absorb_local_timer = 0.135s` (mid-tail at default duration 0.27s), and is currently SETTLED at FarRight (entered F-6 via §3 Rule 1 edge no-op from prior slip-right; `edge_absorb_sign = −1.0`). On the next frame, a `slip-left` input arrives — this is a valid input (FarRight + slip-left → FarRight→Right transition is NOT an edge no-op). PM transitions SETTLED→SLIPPING for the FarRight→Right tween.

Pre-Override F-6 output at the snapshot moment (`edge_absorb_progress = 0.5`, per AC-F6-A math): `f6_lean_body ≈ −1.75°`, `f6_lean_head ≈ −2.10°`, `f6_lean_arm ≈ −1.575°`.

**Asserted on the SLIPPING transition frame (tick 1 of 2 — Override just fired)**:
- `edge_absorb_active == false` (Override zeroed it)
- `edge_absorb_local_timer == 0.0` (Override zeroed it)
- `edge_absorb_sign == 0.0` (Override zeroed it)
- `f6_override_fadeout_ticks_remaining == 1` (the formula tick decremented it from 2 → 1 after applying multiplier 1.0)
- Fade-out snapshot members are non-zero: `f6_override_fadeout_snapshot_body ≈ −1.75°`, `_head ≈ −2.10°`, `_arm ≈ −1.575°`
- F-6 contributes the snapshot at multiplier 1.0 on THIS frame (the snapshot tick): `f6_lean_body ≈ −1.75°`, `f6_lean_head ≈ −2.10°`, `f6_lean_arm ≈ −1.575°`
- F-5 contribution starts cleanly at `TweenProgress = 0.0`: F-5 body ≈ 0.0°, F-5 head ≈ 0.0°, F-5 arm ≈ `LeanCurve(ARM_LEAD_PROGRESS) × MAX_LEAN × +1.0` (small leftward, ≈ +0.5° depending on curve)
- Sum (per F-5 / F-6 co-write contract, clamped to ±MAX_LEAN_ANGLE_DEG × 1.2 = ±12°): `lean_angle ≈ −1.75°`, `head_lean_angle ≈ −2.10°`, `arm_lean_angle ≈ −1.07°` (sum of rightward F-6 and leftward F-5 arm — does not clamp).

**Asserted on tick 2 of 2 (next frame)**:
- `f6_override_fadeout_ticks_remaining == 0` (decremented from 1 → 0 after applying multiplier 0.5)
- F-6 contributes the snapshot at multiplier 0.5: `f6_lean_body ≈ −0.875°`, `f6_lean_head ≈ −1.05°`, `f6_lean_arm ≈ −0.7875°`
- Fade-out snapshot members are hard-zeroed at end of this tick (`_body = _head = _arm = 0.0`)

**Asserted on tick 3 (fade-out complete)**:
- `f6_override_fadeout_ticks_remaining == 0`
- All fade-out snapshot members == 0.0
- F-6 contribution to all three lean angles is 0
- Sum is purely F-5 from `TweenProgress = 2 × DT / SLIP_TWEEN_DURATION_S` (≈ 0.22 at 60 fps default)

**Critical assertions (anti-snap reconciliation)**:
- NO single-frame discontinuity **exceeds 50%** of the snapshot value on ANY of the three lean outputs. The two per-frame F-6 contribution drops are both exactly 50% of the snapshot: tick 1 → tick 2 (multiplier 1.0 → 0.5) and tick 2 → tick 3 (multiplier 0.5 → 0.0). The 50% ceiling matches the LEAN_CURVE_ASSET anti-snap envelope (`LeanCurve` natural-decay steps are bounded at ~50% per frame at default tuning), so the fade-out is within the existing acceptable envelope. The pre-R11a kill block produced a 100% step (snapshot → 0 in one frame) which exceeded the anti-snap envelope — that path is RETIRED.
- Alternatives (b) Kill / (c) Accumulate would either snap to 0 in one frame (b) or sum indefinitely (c) — both fail one constraint. The 2-frame fade satisfies BOTH the Override author decision AND the anti-snap contract.

**AC-F6-C — 5-lane edge-absorb path coverage (closes pre-R10a gap on FarLeft / FarRight 5-lane edge identity):** Verifies F-6 fires correctly at BOTH track edges under the 5-lane widening. Two cases:

- **Case A — FarLeft edge:** Given PM is SETTLED at FarLeft (lane 0), inputs `slip-left`. §3 Rule 1 edge no-op fires; F-6 activates with `edge_absorb_sign = +1.0` (leftward); `edge_absorb_active == true`; `edge_absorb_local_timer = 0.0`. On the next 10 ticks, all three lean angles are positive (leftward) per AC-F6-A's curve math. After `EDGE_ABSORB_DURATION_S` elapses, `edge_absorb_active == false` and all three angles return to 0.
- **Case B — FarRight edge:** Mirror — given PM is SETTLED at FarRight (lane 4), inputs `slip-right`. F-6 activates with `edge_absorb_sign = −1.0` (rightward); same assertions as Case A but with negative angles.
- Verifies the lane model widening did not break F-6's edge-detection logic for the new FarLeft / FarRight edges. Tested as a counter-case within AC-F6-C: SETTLED at Left + slip-left → valid Left→FarLeft tween, F-6 does NOT fire, `edge_absorb_active == false` throughout.

**AC-F6-E — EC-15 head-clamp avoidance via proportional F-6 cap during F-5 Phase 3 (R11a §6.2 NEW — closes DR-F.2 head-stuck-body-settling defect):** Given PM is SLIPPING with `TweenProgress = 0.85` (F-5 Phase 3 settle dip) and F-6 active with `edge_absorb_progress = 0.10` (early tail, ≈ 27 ms into the F-6 tail at default 0.27s duration). This is the EC-15 worst-case boundary identified in DR-F.2: without the §6.2 multiplier, F-5 head ≈ 10° + F-6 head ≈ 3.5° = 13.5° → clamps to 12° (head stuck at clamp while body settles to neutral).

With the §6.2 `EC15_F6_DECAY_COEFFICIENT = 0.7` multiplier applied:
- `slipping_mult = 1.0 − 0.85 × 0.7 = 0.405`
- Pre-multiplier F-6 head ≈ +3.5° (per AC-F6-A math at edge_absorb_progress=0.10 with default 0.35 peak)
- Scaled F-6 head ≈ +3.5° × 0.405 = +1.42°
- F-5 head ≈ +10° (LeanCurve(0.75) at Phase 3 dip)
- Sum: `head_lean_angle = clamp(10.0 + 1.42, ±12.0) = 11.42°` — **UNDER** the ±12.0° clamp ceiling, margin = 0.58°.

**Asserted**:
- `head_lean_angle ≤ 11.5° ± 0.1°` at the EC-15 boundary sample point (no clamp engagement).
- `lean_angle` (body sum) likewise below clamp; `arm_lean_angle` likewise below clamp.
- The clamp's `max()` / `min()` operations DO NOT modify the head, body, or arm output at this sample point.

**Counter-case (sanity)**: with `EC15_F6_DECAY_COEFFICIENT = 0.0` (multiplier disabled), at the same EC-15 boundary sample point: head sum = 13.5°, post-clamp head_lean_angle = 12.0° (clamp engaged). This is the pre-R11a defect path.

**Mirror case (left edge)**: SLIPPING at `TweenProgress = 0.85` toward FarLeft direction with F-6 active at `edge_absorb_progress = 0.10` and `edge_absorb_sign = +1.0` (leftward). Same magnitudes, mirrored signs: head sum = +11.42°, under +12.0° clamp.

This AC complements AC-F6-A (F-6 alignment during SETTLED-tail; multiplier does NOT apply there because movement_state == SETTLED, not SLIPPING).

**AC-F6-D — DEAD-entry freeze pose during F-6 tail (parity with AC-14 mid-tween-freeze contract):** Given PM is in F-6 tail at `edge_absorb_local_timer = 0.135s` (mid-tail at default 0.27s duration) — entered via §3 Rule 1 SETTLED edge no-op at FarRight + slip-right with `edge_absorb_sign = −1.0`. When RSM transitions to DEAD:

- `edge_absorb_active == true` (preserved — same parity rule as F-2 TweenProgress preservation under DEAD per §3 Rule 7)
- `edge_absorb_local_timer == 0.135` (preserved — NOT reset)
- `lean_angle`, `head_lean_angle`, `arm_lean_angle` all freeze at their F-6 contribution values at edge_absorb_progress = 0.5 (per AC-F6-A math)
- Sampled on 5 consecutive ticks after DEAD entry: all four values (`edge_absorb_active`, `edge_absorb_local_timer`, and all three lean angles) remain unchanged — F-6 timer does NOT advance under DEAD (RSM tick gate halts F-6 along with F-5)
- `movement_state == ERunSlipState::SETTLED` (§3 Rule 7 sets SETTLED; F-6 tail does NOT change the movement state because it operates orthogonally)
- Death Replay receives the frozen mid-tail pose — Pillar 5 readability.

**Fade-out preservation under DEAD (R11a §6.1 NEW — §3 Rule 7 parity for the 4 new fade-out members):** Mirror case — given PM is mid-Override-fade with `f6_override_fadeout_ticks_remaining == 1`, `f6_override_fadeout_snapshot_body == −1.75°`, `_head == −2.10°`, `_arm == −1.575°`. When RSM transitions to DEAD:
- `f6_override_fadeout_ticks_remaining == 1` (preserved — NOT decremented)
- All three fade-out snapshot members preserved at their captured values
- The three lean angle outputs include the fade-out contribution at multiplier 0.5
- Sampled on 5 consecutive ticks after DEAD entry: all four fade-out members remain unchanged

This AC complements AC-14 (DEAD mid-tween-freeze) and §5 EC-14.

### Commitment-Tell Counter (counter logic — visual rendering AC lives in presentation §8)

**AC-33 (R7-PM-PROPAGATION updated — edge identity changed Right → FarRight under 5-lane widening):** Given PM is SETTLED and receives 3 valid `slip-right` inputs (each beginning a new tween that completes before the next input), then `commitment_tell_fire_count == 3` after all three tweens complete. Given PM then receives a `slip-right` that is a buffer-flush execution (a buffered slip that fires on tween completion), `commitment_tell_fire_count` increments to 4 — buffer-flush is a SETTLED→SLIPPING transition and fires the tell. Given PM then receives a `slip-right` input while SETTLED at FarRight (edge no-op), `commitment_tell_fire_count` remains 4 — edge-absorb does not fire the tell (PM stays SETTLED, no transition occurs).

### Counter Persistence Across Non-COUNTDOWN State Entries (R10a §2.2 NEW — closes silent-zero-on-DEAD invisibility gap; BINDING)

**AC-COUNTER-DEAD (R10a §2.2 NEW):** Given `slip_complete_count == 5`, `edge_absorb_trigger_count == 2`, `commitment_tell_fire_count == 5` (achieved by running 5 successful slips with 2 intervening edge-absorbs while SETTLED at FarLeft or FarRight), when RSM transitions to DEAD, then **on the DEAD-entry frame**: counters are `5 / 2 / 5` (unchanged — consistent with AC-15). **Sampled on 10 consecutive ticks after DEAD entry**: all three counter values remain `5 / 2 / 5` — they MUST NOT be zeroed, reset, or otherwise mutated by the DEAD state itself or by any per-tick code path executing under DEAD. Verifies counters durable across run-termination and remain readable by post-run telemetry, Death Replay summary screens, and end-of-run analytics.

**AC-COUNTER-COMPLETE (R10a §2.2 NEW):** Same setup as AC-COUNTER-DEAD with terminal state COMPLETE instead. Given `slip_complete_count == 5`, `edge_absorb_trigger_count == 2`, `commitment_tell_fire_count == 5`, when RSM transitions to COMPLETE, then on the COMPLETE-entry frame: counters are `5 / 2 / 5` (unchanged — consistent with AC-16). **Sampled on 10 consecutive ticks after COMPLETE entry**: counters remain `5 / 2 / 5`. ABORTED follows the same pattern by symmetry with AC-17; tested as a mirror-case assertion within this AC (sample 10 ticks under ABORTED → counters remain `5 / 2 / 5`).

**AC-COUNTER-PAUSE-RESUME (R10a §2.2 NEW):** Given `slip_complete_count == 5`, `edge_absorb_trigger_count == 2`, `commitment_tell_fire_count == 5` and PM is mid-tween at `TweenProgress = 0.4`, when `is_paused` becomes true, then **on the pause-entry frame** counters are `5 / 2 / 5`. **Sampled on 10 consecutive ticks during pause**: counters remain `5 / 2 / 5`. When pause is released and `resume_grace == true`, **sampled on 5 consecutive ticks during grace**: counters remain `5 / 2 / 5`. At grace expiry, the tween resumes from 0.4 and completes; `slip_complete_count == 6` (incremented by the natural completion path, NOT reset). `edge_absorb_trigger_count == 2` and `commitment_tell_fire_count == 5` unchanged.

**AC-COUNTER-F6-RESET (R11a §2 NEW; R11a §6.1 extended to cover fade-out members — verifies §3 HandleStateChanged COMPLETE/ABORTED/COUNTDOWN/IDLE dispatch matches the §4 F-6 terminal table for BOTH the F-6 timer state AND the §5.1 (a) Override fade-out state; closes Cluster E.1):**

**Case A — F-6 timer state reset (Cluster E.1 original):** Given PM is in F-6 tail with `edge_absorb_active == true` and `edge_absorb_local_timer == 0.135s` (mid-tail at default duration 0.27s), when RSM transitions to COMPLETE, then **on the same frame** `edge_absorb_active == false` and `edge_absorb_local_timer == 0.0`. **Sampled on 10 consecutive ticks after COMPLETE entry**: both values remain `false / 0.0`. Mirror case: same setup with RSM transition to ABORTED — same assertions hold. Also verifies parity-case for COUNTDOWN entry: setup with same mid-tail F-6 + RSM transition IDLE→COUNTDOWN → same assertions.

**Case B — fade-out state reset (R11a §6.1 NEW):** Given PM is mid-Override-fade with `f6_override_fadeout_ticks_remaining == 1` (one fade-out tick remaining), `f6_override_fadeout_snapshot_body == −1.75°`, `_head == −2.10°`, `_arm == −1.575°` (snapshot captured from a prior FarRight edge no-op + Override fire), when RSM transitions to COMPLETE, then **on the same frame**: `f6_override_fadeout_ticks_remaining == 0` AND `f6_override_fadeout_snapshot_body == 0.0`, `_head == 0.0`, `_arm == 0.0`. **Sampled on 10 consecutive ticks after COMPLETE entry**: all four fade-out members remain at zero / false. Mirror cases hold for ABORTED, IDLE→COUNTDOWN, and (any state) → IDLE entries — all four cases reset the fade-out state.

Verifies that the `HandleStateChanged` COMPLETE/ABORTED/COUNTDOWN/IDLE dispatch bodies match the §4 F-6 terminal-state reset table for BOTH F-6 timer state (Case A) AND fade-out state (Case B). Companion to AC-F6-D (DEAD-entry preservation case).

### Source-Lane Semantic During SLIPPING (R7-PM-PROPAGATION-REVIEW — BINDING for Pull-Wave R7 Rule 11)

**AC-34 (R10a — sample set extended to include TweenProgress=0.0 transition-tick sample per brief §2.1; pre-R10a R7-PM-PROPAGATION-REVIEW NEW; BINDING):** Verifies the §3 Rule 4 / §3 Rule 7 / Public Interface `current_lane` BINDING invariant — `current_lane` MUST return the SOURCE lane throughout the SLIPPING state — at multiple TweenProgress sample points across three lane-transition cases covering the FarLeft / Left / Center / Right / FarRight enum span. Sample set is `{0.0, 0.1, 0.5, 0.99}` — **TweenProgress=0.0 is the transition-tick sample** (the frame on which SETTLED→SLIPPING occurs and `target_lane` is committed but no tween progress has accumulated yet); it is the highest-probability Pull-Wave LANDED-coincidence frame because Pull-Wave's near-miss detection may run in the same tick PM commits a new SLIPPING transition.

- **Case A — Left → Center:** Given a tween executing Left (lane 1) → Center (lane 2), then sampled at TweenProgress ∈ {0.0, 0.1, 0.5, 0.99}, `current_lane == Left` at every sample (including the transition-tick sample at 0.0). At the SLIPPING→SETTLED tween-completion frame, `current_lane == Center`. No sampled TweenProgress value during SLIPPING returns Center.
- **Case B — FarLeft → Left (enum-boundary case):** Given a tween executing FarLeft (lane 0) → Left (lane 1), then sampled at TweenProgress ∈ {0.0, 0.1, 0.5, 0.99}, `current_lane == FarLeft` at every sample. At tween completion, `current_lane == Left`.
- **Case C — Right → FarRight (opposite-boundary case):** Given a tween executing Right (lane 3) → FarRight (lane 4), then sampled at TweenProgress ∈ {0.0, 0.1, 0.5, 0.99}, `current_lane == Right` at every sample. At tween completion, `current_lane == FarRight`.

**Cross-system rationale:** Pull-Wave R7 Rule 11 near-miss detection reads `PM.current_lane` directly at the wave's LANDED-entry tick. The pre-R7-PM-PROPAGATION-REVIEW AC set tested `lateral_world_position` correctness (AC-03) and delegate args (AC-24) but never observed `current_lane` directly during SLIPPING. AC-34 closes this gap.

**AC-34b (R10a — NEW per brief §2.1; BINDING — buffer-flush execution tick coverage):** Verifies the §3 Rule 4 source-lane invariant across the **buffer-flush execution tick** (the compound tick where a prior tween completes and a buffered tween begins in the same frame — §3 Rule 4 buffer flush + §3 Rule 2 collision commit happen in one tick). Given a tween executing Left (lane 1) → Center (lane 2) with `has_queued_input == true` and `queued_input_direction == Right`, on the buffer-flush execution tick, sampled in the buffer-flush handler call after §3 Rule 4 reassignment + §3 Rule 2 collision commit, `current_lane == Center` (the prior `target_lane`, which is the new `source_lane` for the buffered tween — NOT the original `source_lane` of `Left`). On the next tick with `TweenProgress > 0.0` on the buffered Center→Right tween, `current_lane == Center` continues to return the new source lane until that tween completes. **Cross-system rationale**: the buffer-flush tick is the second-highest LANDED-coincidence frame after the transition tick.

**Recommended implementation aid (RECOMMENDED, not BLOCKING — from gameplay-programmer review)**: capture `TweenSourceLane` as a `EPlayerLane` debug-only member at SETTLED→SLIPPING entry (guard `#if !UE_BUILD_SHIPPING`); add `checkf(current_lane == TweenSourceLane, ...)` at SLIPPING→SETTLED completion before the reassignment. This converts the source-lane contract from "passively observable via AC-34 / AC-34b" to "actively enforced at runtime in non-Shipping builds" — a single point of failure surfacing rather than a silent cross-system regression. The `TweenSourceLane` member is implementation-only and is NOT part of the Public Interface contract.

## 9. Open Questions

OQ-1 through OQ-4 are mechanics-owned. OQ-5 and OQ-6 are RESOLVED — preserved here for revision-history traceability. OQ-7 (registry → C++ generator pipeline) routes to `player-movement-platform.md` §9 (architecture seam concern + B-SHIP-1 closure path).

**OQ-1 — PM object type ADR (blocking implementation prerequisite).**
What UE object type implements `UPlayerLaneMovementComponent`? Candidates: `UActorComponent` (simplest, direct `SetActorLocation` via actor ref), `UMovementComponent` subclass (provides `SafeMoveUpdatedComponent` with `bSweep` for geometry separation), `UPawnMovementComponent` (integrates with `APawn::SetMovementComponent`). The choice affects tick registration, physics integration, position update method, and whether sweep-on-move is needed. Also determines which tick-ordering mechanism is available for RSM→PM ordering (see `player-movement-platform.md` § Tick Ordering). Required before implementation begins.

**OQ-2 — Tween implementation ADR.**
`UCurveFloat` in `TickComponent` is the recommended approach. The ADR must additionally confirm: whether to use `SafeMoveUpdatedComponent` vs. direct `SetActorLocation`; the `DeltaTime` source; and that all delegate subscriptions use `AddUObject` + stored handle + `Remove` in `EndPlay`. Must also select the forward motion resolution — option (c) stationary-player/moving-world is recommended (see §3 Cross-Component Interfaces > Forward Motion Dual-Writer). Required before implementation begins.

**OQ-3 — Input event binding ADR (shared with Input System OQ-1).**
How does PM receive `slip-left` and `slip-right` from the Input System? The Input System GDD defers this to an ADR (UMG overlay vs. Slate `IInputProcessor`). PM is blocked on the same ADR for its binding implementation. One ADR resolves both.

**OQ-4 — `lateral_world_position` authority during DEAD freeze.**
The Collision System's hit detection may continue to tick after DEAD entry. At DEAD, `lateral_world_position` is a fractional between-lane position — inconsistent with `current_lane`. Who owns the DEAD-frame position, and can Collision safely ignore events after DEAD entry? Resolve when the Collision GDD is authored.

**OQ-5 — Near-miss detection ownership. [RESOLVED]**
PM declares `OnSlipMidpoint` (multicast delegate) — Pull-Wave subscribes and uses it for near-miss detection. PM exposes `TriggerNearMissBeat()` — Pull-Wave calls it after confirming a wave occupied `source_lane` within the past 0.6s. **R7-PM-PROPAGATION supersedes**: Pull-Wave R1 RC-A 2026-06-07 superseded the `OnSlipMidpoint`-based detection with a direct read of `PM.current_lane` during SLIPPING (§3 Rules 4 + 7 source-lane invariant guarantees source-lane semantic). PM may retain or remove `OnSlipMidpoint` at its discretion; no downstream consumer remains.

**OQ-6 — Telegraph window. [RESOLVED]**
0.6s telegraph window confirmed and propagated to `game-concept.md` Pillar 2 (done). Pull-Wave GDD must use this value. Reaction window = 0.6s − 0.15s tween = 450ms viable skill window. **Note**: Pull-Wave R7 subsequently raised `TELEGRAPH_WINDOW_FLOOR_S` to 0.65s; reaction window math updated accordingly elsewhere in the doc set (mechanics §3 Cross-System Interface Table > Pull-Wave row).

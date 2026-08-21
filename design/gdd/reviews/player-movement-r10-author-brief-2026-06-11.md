# PM R10 Scoped Revision Brief

**Date**: 2026-06-11
**Target document**: `design/gdd/player-movement.md` (currently at R9 NEEDS REVISION; was R7-PM-PROPAGATION-REVIEW state pre-R9)
**Authoring mode**: scoped author revision (NOT in-session patch — CD-mandated handoff)
**Authority**: CD synthesis on PM R9 fresh-context re-review (`design/gdd/reviews/player-movement-review-log.md` line 106) + cross-system survivability coordination resolution (`design/gdd/reviews/cross-system-survivability-coordination-2026-06-11.md`)
**Pre-revision gate**: cross-system coordination CLOSED 2026-06-11 (Option (c) bound); registry + DPC + Pull-Wave forward contracts in place

---

## 0. Why this brief exists

PM R9 verdict was MAJOR REVISION NEEDED with 22 BLOCKING / 23 RECOMMENDED / 8 NICE-TO-HAVE. The R8→R9 BLOCKING spike (4→22) was the propagation-depth tax from the 5-lane widening exceeding what in-session revision could absorb. CD explicitly ruled out another in-session patch:

> "Hand back to author with scoped revision brief, NOT another in-session patch."

This brief is the scoped handoff. The cross-system survivability coordination (Cluster 1) has already been resolved at the registry / Pull-Wave / DPC layer — see §6 *Out of scope* — so SLIP_TWEEN does not change and the survivability invariant is not load-bearing for this pass. PM R10 is therefore a focused structural cleanup on Clusters 2–5 + a small set of editorial fixes.

The author should treat this brief as a self-contained scope. Open questions are surfaced as numbered decision points; the author MUST resolve them in the revision pass and document the decision in the GDD, not leave them for R10 re-review.

---

## 1. IN scope — Top 3 structural priorities

### 1.1 F-6 full re-specification (CD-escalated from RECOMMENDED to BLOCKING)

**Defect**: F-6 (edge-absorb tail) was authored at R5 to solve EC-15. The 5-lane widening (R7-PM-PROPAGATION) created additive edge-absorb paths and chained-mash timer collisions F-6 was never spec'd against. Three concrete defects:

- **Dual-writer with F-5**: F-6 writes `lean_angle` only. F-5 continues driving `head_lean_angle` and `arm_lean_angle` during the F-6 tail phase. Head/arm decouple from body during edge-absorb — a visual contradiction that breaks the lean-as-commitment fantasy.
- **Timer collision behavior undefined**: when a new `slip-left` / `slip-right` SLIPPING transition arrives mid-tail, three plausible behaviors are all defensible: (override) F-6 timer is killed and the new tween starts cleanly; (kill) F-6 finishes its tail before the new tween starts; (accumulate) the new tween's F-5 lean composites additively with the in-flight F-6 lean. The GDD does not pick. Each choice has different ACs and different player-feel implications.
- **Zero ACs cover F-6 timer behavior**. The acceptance-criteria matrix has no row for any F-6 boundary condition.

**Fix scope**:

1. Author a complete F-6 specification covering:
   - Formula body for `lean_angle`, `head_lean_angle`, `arm_lean_angle` during the tail phase
   - Explicit co-write contract with F-5: which formula owns which property under SETTLED + tail-active state
   - Timer-collision behavior on new SLIPPING transition (DECISION required — see §5.1)
   - Reset semantics on COUNTDOWN / DEAD / COMPLETE / ABORTED (parity with existing terminal rules)
2. Add 4 new ACs minimum:
   - AC-F6-A: head/arm angle alignment with body during tail phase (no visual decoupling)
   - AC-F6-B: timer-collision boundary case (asserts the chosen behavior from §5.1)
   - AC-F6-C: 5-lane edge-absorb path coverage (FarLeft and FarRight slip-into-edge cases)
   - AC-F6-D: terminal-state freeze pose during F-6 tail (DEAD entry mid-tail)
3. Update Public Interface table `lean_angle` / `head_lean_angle` / `arm_lean_angle` row text to reflect F-5/F-6 co-write contract.
4. Update Cross-Component Interfaces if the chosen timer-collision behavior affects any downstream consumer (Camera reads `lateral_world_position` which is F-3 — likely no downstream change).

**Deliverable**: new `### F-6 — Edge-Absorb Tail Phase` subsection under Formulas + 4 new ACs + Public Interface row updates + one new Tuning Knob if a tail-duration knob is introduced.

---

### 1.2 Shipping-Safety Enforcement Policy (NEW subsection)

**Defect (cross-cutting)**: PM R9 surfaced a pattern across multiple findings — assertions that strip in Shipping cannot be the only line of defense for BLOCKING invariants. Specific examples:
- `ensureMsgf(SLIP_TWEEN_DURATION_S ≤ 0.15f)` BeginPlay assertion is no-op in Shipping; a designer can ship `SLIP_TWEEN = 0.16s` silently breaking the invariant.
- `ERunSlipState` / `EMovementState` lockstep relies on prose only — no `static_assert` adjacent to the seam cast site.
- `AddTickPrerequisiteComponent` Option (b) silent-double-tick has no `check()` guard.
- Null curve guards are `Warning`-only; Shipping silently falls back to linear/zero.
- AC-21 one-shot floor-guard `bFloorGuardFired` bypasses subsequent violations silently.

These are the same failure class throughout. The fix is structural, not a list of point patches.

**Fix scope**: author a new GDD subsection titled `### Shipping-Safety Enforcement Policy` (placement: under Cross-Component Interfaces, after Hardware Contract). The subsection establishes a binding policy:

> Every BLOCKING invariant in this GDD MUST have BOTH:
> 1. An `ensure` / `ensureMsgf` / `check` at the defect site for dev-build visibility.
> 2. A runtime clamp, guard, or fallback that holds in Shipping builds.
>
> Examples (non-exhaustive) below; the policy applies to every invariant added in this GDD or in any future revision.

Then provide an enforcement table mapping each BLOCKING invariant to its dev assertion + Shipping guard. At minimum:

| Invariant | Dev assertion | Shipping guard |
|---|---|---|
| `SLIP_TWEEN_DURATION_S ∈ [0.10, 0.15]` | `ensureMsgf` at BeginPlay | Persistent clamp + `LogPlayerMovement Error` (preserve current AC-21 path) |
| `ERunSlipState ↔ EMovementState ordinal lockstep` | `static_assert` adjacent to seam cast | Defensive `default:` branch in switch → `LogPlayerMovement Error` + return SETTLED |
| `AddTickPrerequisiteComponent Option (b) double-tick` | `check(!bManualTickEnabled)` in TickComponent prologue | Guard early-out via `bManualTickEnabled` flag |
| `TelegraphWindowCurve` / `SlipCurveAsset` non-null at tick | `ensure(IsValid(Curve))` once per occurrence | Linear-time fallback (existing) + `bCurveFallbackActive` flag → debug overlay |
| `F-2 floor guard one-shot bypass` | (existing `bFloorGuardFired`) | Replace one-shot bypass with persistent clamp + per-N-ticks rate-limited log |

The policy itself does NOT require code changes — it establishes the contract every BLOCKING invariant in PM must follow. The R9 findings that are instances of this class (B-2, B-5, B-6 from PM R9 cluster 2) close as a class once the policy is authored.

**Deliverable**: new `### Shipping-Safety Enforcement Policy` subsection (≥1 page) + the enforcement table populated for every existing BLOCKING invariant + a one-paragraph statement that future PM revisions adding a BLOCKING invariant MUST extend the table.

---

### 1.3 Hardware Contract enforcement mechanism

**Defect**: PM R7-PM-PROPAGATION-REVIEW locked the 60fps minimum hardware contract but the enforcement mechanism is conceptually broken. Specific issues:

- `t.MaxFPS = 60` is a UE *cap*, not a *floor*. It limits the maximum framerate; it does not guarantee a minimum. The current GDD prose treats it as a floor — wrong engine semantic.
- The `<55fps-for-2s-sustained` degradation check ignores isolated transient spikes — the exact failure class Pillar 5 was authored against.
- The watchdog conflates a BeginPlay one-shot check with rolling-window measurement. Rolling window size, threshold, sample location all unspec'd.
- The "debug overlay" graceful-degradation path is a developer artifact, not a shipping-build fallback UX. No recovery path. No named min-spec devices.
- Min-spec device list absent. "60fps mobile" is unverifiable without naming reference hardware (e.g., iPhone XR / Pixel 5 / Samsung A52 baseline) for performance-analyst to audit against.

**Fix scope**:

1. **Replace the `t.MaxFPS = 60` prose** in the Hardware Contract subsection (`design/gdd/player-movement.md` lines ~212–234) with the actual UE mechanism:
   - Project settings target framerate + vsync
   - Specific UE config keys (e.g., `MobileContentScaleFactor`, `r.MobileEarlyZPass`, `FrameRateLowerBound` if applicable)
   - Whatever is the canonical UE 5.6+ mobile framerate-floor declaration (TD or engine-specialist confirms)
2. **Author a runtime DT watchdog specification** covering:
   - Sample location (single `FApp::GetDeltaTime()` read on GameThread; document tick ordering)
   - Rolling-window size (recommendation: 1.0s window with per-tick sampling)
   - Breach threshold (recommendation: ≥3 consecutive ticks above 18ms `DT` = sustained sub-55fps, or 30% of window-ticks above 16.67ms = transient hitch cluster)
   - Action on breach (DECISION required — see §5.2)
3. **Author a named min-spec device list** as a new Tuning Knobs row or a dedicated subsection:
   - 2 mobile reference devices the PM-relevant performance audit must pass on (recommendation: 1 iOS + 1 Android baseline at ~3-year-old hardware; TD + performance-analyst pick specific models)
   - Sustained 60fps under PEAK density on these devices is the Polish-phase gate
4. **Author shipping-build fallback UX** (DECISION required — see §5.3). Not a debug overlay. Must be a player-facing experience.
5. New ACs:
   - AC-HW-A: rolling-window watchdog triggers at threshold + action fires
   - AC-HW-B: min-spec device PEAK-density framerate measurement (Polish-phase gate)
   - AC-HW-C: shipping-build fallback UX appears under sustained sub-55fps + survivability promise is or is not relaxed per §5.3 decision

**Deliverable**: full rewrite of the Hardware Contract subsection (lines ~212–234) + new shipping-build fallback UX subsection (likely under Player-Perceivable State) + named min-spec device list + 3 new ACs.

---

## 2. IN scope — Additional R9 BLOCKING items to bundle

These are point fixes from R9 Clusters 4 and 5 that bundle into the same revision pass. They are smaller in scope but BLOCKING.

### 2.1 AC-34 coverage gaps (Cluster 4)

AC-34 (added in the R7-PM-PROPAGATION-REVIEW in-session pass) tests `current_lane == source_lane` at `TweenProgress ∈ {0.1, 0.5, 0.99}` across 3 lane-transition cases. Missing cases:

- **TweenProgress = 0.0** (transition-tick sample). Highest-probability Pull-Wave LANDED-coincidence frame. AC-34 must extend its sample set to `{0.0, 0.1, 0.5, 0.99}`.
- **Buffer-flush execution tick** (compound tick where prior tween completes and new tween begins in the same tick — Rule 4 buffer flush + Rule 1 collision commit happen in one frame). New AC required (call it AC-34b): assert `current_lane == new_source_lane` (the prior `target_lane`, not the original `source_lane`) at the buffer-flush tick.
- **AC-14 boundary moments**: `TweenProgress = 0.001` and `TweenProgress = 0.999` not covered. Add boundary sample to existing AC-14.

### 2.2 Counter-reset assertions (Cluster 4)

No ACs assert that `slip_complete_count`, `edge_absorb_trigger_count`, `commitment_tell_fire_count` are NOT reset on DEAD / COMPLETE / ABORTED / pause / grace-expiry. The R9 reviewer flagged this as a "silent zero-on-DEAD bug invisible" — counters could be wiped without any test catching it. Add 3 ACs:

- AC-COUNTER-DEAD: assert all 3 counters retain pre-DEAD values after DEAD entry
- AC-COUNTER-COMPLETE: same for COMPLETE
- AC-COUNTER-PAUSE-RESUME: counters retain values across pause + grace expiry

### 2.3 Rule 7 vs Public Interface contradiction (Cluster 5)

Rule 7 (DEAD) leaves `TweenProgress` at its fractional value with `movement_state = SETTLED`. Public Interface row for `lateral_world_position` says "lane-anchored when SETTLED" but Rule 7 leaves it at the fractional visual mesh position. This is effectively a third "SETTLED-but-fractional" state the 2-member enum cannot represent.

**Fix**: pick one of:
- (i) Update Public Interface `lateral_world_position` row to acknowledge "may be fractional when SETTLED if entered via Rule 7 DEAD freeze"
- (ii) Add Rule 7 carve-out language to the row explicitly

Recommendation: (i). Cleaner.

### 2.4 LANE_WIDTH_M = 0.85 floor breaks "400cm preserved at all tunings" claim (Cluster 5)

The R7-PM-PROPAGATION-REVIEW pass tightened `LANE_WIDTH_M` safe-range lower from 0.75 → 0.85 m. At 0.85 × 4 lane-widths = 340 cm total track span, NOT the 400 cm the GDD repeatedly claims is "preserved at all tunings."

**Fix**: either (i) restore the upper bound that ensures 400 cm at all safe-range values (requires LANE_WIDTH_M = 1.0 m as a fixed value, not a knob with a safe range), OR (ii) rewrite the "400 cm preserved" prose to acknowledge that total track span scales with `LANE_WIDTH_M × 4`.

Recommendation: (ii). Cheaper, preserves the knob.

### 2.5 Thumb-target floor is wrong constraint for LANE_WIDTH_M safe range (Cluster 5)

The R7-PM-PROPAGATION-REVIEW pass justified the 0.85 m floor with a "6-inch thumb-target" argument. The R9 reviewer flagged this as a category error — Input System uses a dead-band split with no per-lane tap target; thumb-target geometry does not apply to PM's lane spacing.

**Fix**: rewrite the LANE_WIDTH_M safe-range justification to use a perceptually-grounded constraint (e.g., camera framing on 6-inch mobile + minimum readable inter-lane visual gap), NOT thumb-target.

### 2.6 HandleStateChanged / HandlePausedChanged method bodies unspecified (Cluster 5)

The GDD names these delegate handlers but does not specify what their bodies do. R9 reviewer flagged as unimplementable.

**Fix**: add a `### Delegate Handler Bodies` subsection under Cross-Component Interfaces specifying:
- `HandleStateChanged(EPMState OldState, EPMState NewState)` — dispatch table (COUNTDOWN → Rule 10; DEAD → Rule 7; COMPLETE → Rule 8; ABORTED → Rule 9; etc.)
- `HandlePausedChanged(bool bNewPaused)` — Rule 6 freeze / unfreeze gating

### 2.7 lateral_world_position semantics under Option (c) stationary-player model (Cluster 5)

Forward Motion Dual-Writer section recommends Option (c) (stationary-player / moving-world). Under (c), the player actor is stationary in world space — but Public Interface still describes `lateral_world_position` as actor world X. Semantic is undefined under (c).

**Fix**: clarify Public Interface row for `lateral_world_position`: under Option (c), this property describes the player's lateral position in *track-space* (or whatever the moving-world reference frame is), not world space. Note the OQ-2 ADR resolution will lock the semantic.

### 2.8 Slip cue proportional scaling is informal comment (Cluster 5)

Audio slip cue duration is documented as "scaling proportionally with SLIP_TWEEN_DURATION_S" in an informal comment but not enforced as a contract. R9 reviewer flagged as unverifiable.

**Fix**: promote to formal contract: audio slip cue duration = `SLIP_TWEEN_DURATION_S × audio_cue_ratio` where `audio_cue_ratio` is a new Tuning Knob with default = 0.85 (yielding 127ms at default 0.15s — matches the 100–150ms safe range). Add AC asserting the proportionality.

### 2.9 Near-miss audio suppressed under slip priority breaks Pillar 5 (Cluster 5)

R9 reviewer flagged: when a slip is in-flight, near-miss audio is suppressed by the slip cue priority. This breaks Pillar 5 for the case where the player slips AND a near-miss occurs in the same beat.

**Fix**: either (i) duck the slip cue under the near-miss swell rather than suppressing, OR (ii) author concurrent playback explicitly with documented mixing.

DECISION required — author should pick (i) or (ii) and document.

### 2.10 Mono-front edge-absorb pan-absence as implicit directional signal (Cluster 5)

The pan-locked-mono edge-absorb audio cue produces an implicit directional signal via pan-absence (every other audio cue pans; the mono cue stands out as "edge"). R9 reviewer flagged this as contradicting the locked decision banning pan as a direction signal.

**Fix**: either (i) author a center-pan slip variant for consistency (no mono cue stands out), OR (ii) accept that pan-absence is itself an aesthetic signal and document that it is intentional + does not violate the pan-as-direction ban (since the player cannot tell direction from absence-of-pan, only from visual cues).

DECISION required — author should pick (i) or (ii) and document.

---

## 3. RECOMMENDED items (bundle if cost ≤1 hour each)

The R9 review surfaced 23 RECOMMENDED items. The author may bundle these opportunistically but they do NOT gate R10 verdict. CD downgraded two RECOMMENDED items to NICE-TO-HAVE explicitly:

- systems-designer R-3 (1-frame shelf at lower bound)
- gameplay-programmer R-1 (UMovementComponent threading)

These two are NICE-TO-HAVE and may be deferred past PM approval. The other 21 RECOMMENDED items live in the R9 review log entry — author should skim and bundle whatever is cheap.

---

## 4. NICE-TO-HAVE items (defer)

8 NICE-TO-HAVE items from R9, plus the two downgrades above. Do not bundle into R10. Defer to R10+1 or post-Approval polish pass.

---

## 5. Open decisions the author MUST resolve in this revision pass

These are design decisions the author cannot defer to a downstream reviewer. Each requires a documented decision in the GDD.

### 5.1 F-6 timer-collision behavior on new SLIPPING mid-tail

Choose ONE:
- **(a) Override**: F-6 timer is killed when a new tween starts. F-5 takes over cleanly.
- **(b) Kill**: F-6 finishes its tail before the new tween starts. New tween is queued in the buffer (treating the F-6 tail as occupying the SLIPPING-equivalent slot).
- **(c) Accumulate**: F-5 lean composites additively with the in-flight F-6 lean during the overlap window.

CD does NOT take a position. Pick based on player feel — (a) prioritizes input responsiveness, (b) prioritizes animation continuity, (c) blends both at the cost of visual readability.

Recommended: **(a)**. Player Fantasy ("the body moves before the mind finishes the sentence") favors input responsiveness over animation continuity. F-6's tail is decorative; SLIPPING is mechanical.

### 5.2 Hardware Contract — action on sustained framerate breach

Choose ONE:
- **(a) Survivability margin relaxation**: under sustained <55fps the F-BARRAGE-SURVIVABILITY-INVARIANT M=3 PEAK barrage is suppressed (Wave Spawner gate), preserving the M=2 surviving triplet set as the only PEAK barrage class.
- **(b) PEAK-density gate**: under sustained <55fps the entire PEAK phase is gated to MID-density (DPC reads the watchdog and skips PEAK transitions).
- **(c) Player-visible warning + run-end**: under sustained <55fps the current run is gracefully ended with a "device performance warning" message. Player is shunted back to the menu.

CD does NOT take a position. (a) and (b) preserve Pillar 5 by keeping the survivability invariant whole; (c) preserves Pillar 5 by stopping play before it lies. (c) is the most player-honest; (a) and (b) are more elegant.

Recommendation deferred to TD + performance-analyst (CD scope is to flag, not to specify the mechanism).

### 5.3 Shipping-build fallback UX shape

When the Hardware Contract watchdog fires, what does the player see? This is NOT a debug overlay. Choices:
- **(a) Banner notification**: a thin warning banner at the top of the HUD reading "Performance reduced — survivability adjustments active."
- **(b) Pause + dialog**: pause the run, show a modal explaining the device is below min-spec.
- **(c) Silent + telemetry**: no player-facing UX; degraded mode is silent; telemetry logs the breach for post-launch analysis.

DECISION required. (c) ships the cleanest experience but is the least Pillar-5-honest. (a) is a middle ground.

### 5.4 Near-miss audio under slip priority (§2.9)

Pick (i) duck slip cue under near-miss swell, OR (ii) concurrent playback with documented mixing.

### 5.5 Mono-front edge-absorb pan-absence (§2.10)

Pick (i) author center-pan slip variant for consistency, OR (ii) accept pan-absence as intentional aesthetic.

### 5.6 Rule 7 vs Public Interface contradiction (§2.3)

Pick (i) update `lateral_world_position` row text, OR (ii) add Rule 7 carve-out language. Recommendation: (i).

### 5.7 LANE_WIDTH_M "400cm preserved" prose (§2.4)

Pick (i) restore LANE_WIDTH_M as a fixed value, OR (ii) rewrite the prose. Recommendation: (ii).

---

## 6. OUT of scope — what this revision MUST NOT touch

### 6.1 Survivability invariant (resolved at cross-system layer)

`SLIP_TWEEN_DURATION_S` stays at **0.15s default** with the existing **[0.10, 0.15]s safe range**. Do NOT tighten it. Do NOT widen it.

The F-BARRAGE-SURVIVABILITY-INVARIANT is now closed by:
- `MIN_ESCAPE_SLIPS = 2` (new registry constant; see `design/registry/entities.yaml`)
- Pull-Wave R7 B8 promoting ADVISORY → BLOCKING at Pull-Wave R8 (queued)
- Wave Spawner cook-time exclusion of the three all-consecutive M=3 triplets

PM's contribution to the invariant is the 60fps Hardware Contract + the existing default tuning. No PM tween change is in scope.

If the author has an argument for tightening SLIP_TWEEN anyway (e.g., F-6 timing makes 0.15s feel sluggish), that is a SEPARATE design question that goes to creative-director adjudication — NOT to be folded into R10.

### 6.2 5-lane widening (already applied at R7-PM-PROPAGATION)

The 5-lane lane model, `EPlayerLane` enum widening, `LANE_WIDTH_M` knob, `LANE_WIDTH_CM` alias, F-1 formula restructure are all settled. Do NOT re-open. R10 may polish prose around these (e.g., the "400cm preserved" claim in §2.4) but the mechanical model is locked.

### 6.3 EMovementState pruning (already applied at R7-PM-PROPAGATION-REVIEW)

The 2-member pinned-ordinal enum + Seam 12 lockstep is locked. Do NOT add states.

### 6.4 Cross-system contracts (queued for downstream GDD revisions)

The 5 forward contracts on Pull-Wave are queued for Pull-Wave R8 substantive revision. PM R10 does NOT author them — Pull-Wave R8 does. PM R10 may *reference* them (e.g., the Hardware Contract subsection can mention that the survivability invariant is closed via Pull-Wave R7 B8 BLOCKING + MIN_ESCAPE_SLIPS=2) but MUST NOT make PM-side edits that pre-empt Pull-Wave R8.

### 6.5 DPC TELEGRAPH_WINDOW_FLOOR_S

Unchanged at 0.65s. DPC GDD content unchanged. PM R10 may reference the DPC resolution note added to the TELEGRAPH_WINDOW_FLOOR_S Tuning Knob row but does not modify DPC.

---

## 7. Files this revision will touch

Author should expect to edit these (and ONLY these) in the R10 revision pass:

- `design/gdd/player-movement.md` — primary target. Substantive growth expected (new F-6 subsection + new Shipping-Safety subsection + Hardware Contract rewrite + ~10 ACs added + ~5 prose fixes). Estimate: +200 to +400 lines.
- `design/registry/entities.yaml` — possibly 1–2 new constants if §1.3 introduces a `MIN_SPEC_DEVICE_LIST` or `AUDIO_CUE_RATIO` constant. NOT for `SLIP_TWEEN` changes (out of scope).
- `docs/architecture/platform-seam-interfaces.md` — possibly Seam 12 polish if the F-6 spec affects the `IPlayerMovementProvider` interface. Unlikely.
- `design/gdd/reviews/player-movement-review-log.md` — append a `## In-Session R10 Revision Pass` entry once the revision is complete (NOT during; the entry is the receipt of the work).

Files the author MUST NOT touch in this pass:
- `design/gdd/pull-wave-behavior.md`
- `design/gdd/difficulty-phase-controller.md`
- `design/gdd/wave-spawner-pattern-library.md` (does not exist)
- `design/gdd/systems-index.md` (gets updated by `/design-review` or `/story-done`, not by the author)

---

## 8. Verification path

When the author believes the revision is complete:

1. Self-check against §1 and §2 — every BLOCKING item has a specific GDD edit that closes it.
2. Self-check against §5 — every open decision has a documented choice in the GDD.
3. `/clear` → fresh-session `/design-review design/gdd/player-movement.md` for R10 fresh-context re-review.
4. R10 forecast (creative-director synthesis): 0–4 BLOCKING. Main residual risk: F-6 timer-collision behavior decision (§5.1) may surface downstream consumer interactions that the author cannot anticipate without specialist consultation.

R10 verdict will be APPROVED if BLOCKING ≤ 2 AND no Cluster 1 (Pillar 5 + survivability) regression. Convergence trajectory expectation: R9 22 BLOCKING → R10 0–4 BLOCKING is a sharp drop because the cross-system layer absorbed Cluster 1 entirely.

---

## 9. Estimated effort

Brief-scope assessment (not a producer estimate):

- §1.1 F-6 re-specification: 3–5 hours (DECISION + spec authoring + 4 ACs)
- §1.2 Shipping-Safety Enforcement Policy: 2–3 hours (subsection authoring + enforcement table population)
- §1.3 Hardware Contract enforcement: 3–4 hours (TD consultation + prose rewrite + 3 ACs + min-spec device list research)
- §2.x bundle (10 point fixes): 3–4 hours total
- §5.x decisions: 1–2 hours of designer time (the decisions are small; the consequences are folded into §1.x and §2.x)

Total: ~12–18 hours of focused author work. Plus an R10 fresh-context re-review.

---

## 10. Companion artifacts

- `design/gdd/reviews/player-movement-review-log.md` — R9 entry (line 106) + cross-system coordination resolution entry (most recent)
- `design/gdd/reviews/cross-system-survivability-coordination-2026-06-11.md` — full creative-director adjudication of Options (a/b/c)
- `design/gdd/reviews/pull-wave-behavior-review-log.md` — cross-system coordination entry queuing 5 forward contracts for Pull-Wave R8
- `design/registry/entities.yaml` — `MIN_ESCAPE_SLIPS = 2` (new), `MIN_BARRAGE_LANE_SEPARATION` notes (updated), `TELEGRAPH_WINDOW_FLOOR_S` notes (Option-(c) provenance), `REACTION_BUDGET` notes (Option (c) math)

End of brief.

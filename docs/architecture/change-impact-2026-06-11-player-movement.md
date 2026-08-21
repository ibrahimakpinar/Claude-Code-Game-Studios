# Change Impact: PM R9 + Survivability Invariant Cross-System Coordination

**Date**: 2026-06-11
**Trigger**: `design/gdd/player-movement.md` R9 fresh-context re-review (verdict: MAJOR REVISION NEEDED; 22 BLOCKING / 23 RECOMMENDED / 8 NICE-TO-HAVE) + deferred 60fps Hardware Contract propagation from PM R7-PM-PROPAGATION-REVIEW (2026-06-11 earlier in day)
**Skill invocation**: `/propagate-design-change design/gdd/player-movement.md`
**Review mode**: full
**Supersedes (for survivability-invariant deltas only)**: `docs/architecture/change-impact-2026-06-10-player-movement.md` — the Lane-model and SLIP_TWEEN safe-range deltas from 2026-06-10 remain valid; this doc adds the survivability resolution + Hardware Contract propagation that the prior pass deferred.

---

## Executive Summary

PM R9 review surfaced **two cross-system items requiring multi-GDD coordination**:

1. **Deferred 60fps Hardware Contract propagation** — PM unilaterally declared a 60fps minimum target framerate Hardware Contract in its R7-PM-PROPAGATION-REVIEW pass earlier on 2026-06-11; cross-system propagation to Pull-Wave + DPC was deferred to this skill invocation.
2. **Zero-margin F-BARRAGE-SURVIVABILITY-INVARIANT cliff** — PM R9 review found the invariant `SLIP_TWEEN × M + REACTION_BUDGET ≤ TELEGRAPH_WINDOW_FLOOR_S` sits at `0.45 + 0.20 = 0.65 ≤ 0.65` (exact equality). A single transient frame hitch or GC pause at 60fps produces unescapable M=3 PEAK barrage — Pillar 5 violation (mystery death attributable to device, not skill). The 60fps Hardware Contract degradation check (`<55fps-for-2s-sustained`) catches sustained throttle but not isolated spikes.

**User adjudication (2026-06-11)**: Resolution **Path (b)** — raise `TELEGRAPH_WINDOW_FLOOR_S` from 0.65s to **0.68s** in DPC. Preserves PM "bend" Player Fantasy at 0.15s SLIP_TWEEN; produces 30ms (1.8 frames at 60fps) margin to absorb transient hitches.

**Cross-GDD scope**: 5 files (DPC + Pull-Wave + game-concept + entity registry + PM). No ADRs affected (all 3 existing ADRs scanned clean against the propagation surface). No new ADRs required (this is a tuning-knob coordination, not an architectural decision).

---

## Change Summary

### Resolution Path (b) — Adopted

| Constant | Before | After | Margin calculation |
|---|---|---|---|
| `TELEGRAPH_WINDOW_FLOOR_S` | 0.65 s (provisional, R2 Cluster E path b) | **0.68 s** (provisional, this pass) | `0.15 × 3 + 0.20 = 0.65 ≤ 0.68` → **30ms (1.8 frames at 60fps) margin** |
| `BARRAGE_SIMULTANEITY_WINDOW_S` | 0.325 s (= FLOOR / 2) | **0.34 s** (= FLOOR / 2) | Derived; cascades from FLOOR change |
| `SLIP_TWEEN_DURATION_S` safe range | `[0.10, 0.15]` s | **`[0.10, 0.15]` s — unchanged** | Path b preserves PM expressiveness |
| 60fps Hardware Contract | Declared in PM only | **Mirrored in Pull-Wave + DPC** | F-BARRAGE-SURVIVABILITY-INVARIANT verifies frame-quantized at 60fps |

### Why Path (b) Over Alternatives

| Path | Outcome | Reason rejected |
|---|---|---|
| (a) Tighten SLIP_TWEEN to ≤0.13s | PM-only; 60ms margin | Trade-off: Player Fantasy regression — animation team must re-time SlipCurve shape; lower bound 0.10s already at Phase 1 shelf invisibility edge |
| **(b) Raise FLOOR to 0.68s** | DPC + Pull-Wave + game-concept; **30ms margin** | **Adopted**: preserves PM "bend" fantasy at 0.15s tween; largest blast radius but smallest mechanical-design impact; explicitly listed as DPC GDD Rule 9 fail-timing resolution path in entity registry note (lines 167–168) |
| (c) Wave Spawner barrage restriction | Wave Spawner forward contract change | Wave Spawner GDD currently Not Started; would lock a forward contract on an unauthored downstream system; risks difficulty curve regression (PEAK becomes mechanically easier) |
| (d) Hybrid (SLIP_TWEEN 0.14s + FLOOR 0.66s) | PM + DPC + Pull-Wave | Touches more files for similar margin (40ms vs Path b's 30ms); each system absorbs disruption without clear benefit |

### 60fps Hardware Contract Propagation

PM Cross-Component Interfaces section (lines 212–234) declares: "BINDING — Minimum supported target framerate: 60 fps mobile" with frame-quantized verification table and degradation check. This contract must be referenced (not duplicated) in:
- **Pull-Wave behavior.md F-BARRAGE-SURVIVABILITY-INVARIANT** context — cite PM's Hardware Contract subsection as the framerate-floor authority; note that the invariant is verified at 60fps frame-quantization (post-Path-b: at zero-margin continuous form, 30ms margin frame-quantized at 60fps).
- **DPC TELEGRAPH_WINDOW_FLOOR_S Tuning Knob rationale** — cite PM's Hardware Contract subsection; new FLOOR value 0.68s is calibrated against 60fps verified margin.

---

## ADR Impact Analysis

ADRs scanned: 3 (`adr-0001-palm-rejection-rmax-calibration.md`, `adr-0002-haptic-platform-bridge.md`, `adr-0003-drain-queue-architecture.md`).

### ADR-0001: Palm-Rejection R_max Calibration
**Status**: ✅ **Not affected** — no PM, DPC, or Pull-Wave references; touches input-system only.

### ADR-0002: Haptic Platform Bridge
**Status**: ✅ **Still Valid** — PM consumes `IHapticDispatch::Fire(EHapticEvent::SlipConfirmed | BufferDrop)` per existing contract. Path (b) does not alter the haptic taxonomy or dispatch interface. R9 finding qa-lead R-1 ("AC-25 should mock IHapticDispatch") is an AC-scoping note for the PM GDD revision — not an ADR-0002 change.

### ADR-0003: Drain Queue Architecture
**Status**: ✅ **Not affected** — zero PM/DPC/Pull-Wave references against the propagation surface.

### `platform-seam-interfaces.md`
**Status**: ✅ **Still Valid** — Seam 12 (`IPlayerMovementProvider`) `EMovementState` was pruned to 2 members with pinned ordinals in PM R7-PM-PROPAGATION-REVIEW (2026-06-11); no further seam changes from Path (b). Seam 14 (`IDPCFrameStateProvider` — if present) would need to surface the new FLOOR value but only if the seam exposes it; verify during DPC revision.

### Other ADR observations
- **No new ADR required** for Path (b). The change is a tuning-knob coordination (FLOOR safe-range + default value), not an architectural decision. The 60fps Hardware Contract is itself a design-level invariant (lives in PM GDD), not an ADR-class decision.
- **Future ADR candidate**: If PM R10 fresh-context re-review surfaces additional Hardware Contract enforcement-mechanism complexity (e.g., the rolling-window watchdog gameplay-programmer flagged in R9), a dedicated ADR may be warranted to lock the mechanism. For now, deferred.

---

## Cross-GDD Propagation Plan

| # | File | Change | Section / Line refs (approx) | Status |
|---|---|---|---|---|
| 1 | `design/registry/entities.yaml` | `TELEGRAPH_WINDOW_FLOOR_S` value `0.65 → 0.68`; update note to record Path-b adjudication; `revised: 2026-06-11`; BARRAGE_SIMULTANEITY_WINDOW_S note updated if registered (= FLOOR / 2 = 0.34s) | lines 143–174 + any BARRAGE_SIMULTANEITY entry | Pending |
| 2 | `design/gdd/difficulty-phase-controller.md` | `TELEGRAPH_WINDOW_FLOOR_S` knob safe-range + default `0.65 → 0.68`; AC-PILLAR-2-CONCURRENT re-validation note; BARRAGE_SIMULTANEITY_WINDOW_S derived `0.325 → 0.34`; new "60fps Hardware Contract reference" note in Tuning Knob rationale | knob table + Tuning Knobs § + relevant ACs | Pending |
| 3 | `design/gdd/pull-wave-behavior.md` | All `TELEGRAPH_WINDOW_FLOOR_S = 0.65s` references → `0.68s`; F-BARRAGE-SURVIVABILITY-INVARIANT recalculation under Path (b); AC-PW-PEAK-NO-ADJACENT-CLUSTER REACTION_BUDGET-Hick's-Law analysis updated for Path (b) margin (note that R7 B8 Hick's Law analysis assumed 0s margin; now 30ms margin softens the all-adjacent-cluster exclusion rationale but the exclusion remains BINDING for OPENER tier-0 first-encounter readability); new "60fps Hardware Contract reference" note; EC-INTER-SLIP-GATE-INTRODUCTION recomputation at FLOOR=0.68s (60fps `0.20 + 0.45 + 3 × 0.017 = 0.701 > 0.68 still violated — inter-slip gate still forbidden at default; under Path-a-combined: `0.20 + 0.30 + 3 × 0.017 = 0.55 ≤ 0.68 ✓` with 130ms margin if SLIP_TWEEN tightens) | 28 references to `0.65` across header binding decisions + Rule 5 + ACs + worked examples | Pending |
| 4 | `design/gdd/game-concept.md` | Pillar 2 — "at least 0.6s before contact" reference; raise to 0.68s if Path (b) FLOOR is the Pillar 2 floor, OR explicitly note that game-concept Pillar 2 is the human-readability minimum (0.6s) while the FLOOR for F-BARRAGE-SURVIVABILITY is the survivability-minimum (now 0.68s) — these are different invariants serving different constraints. The 0.6s reference may stay as the Pillar 2 perceptual floor while 0.68s becomes the additional survivability floor. **CD adjudication needed** during structural revision; default plan = retain Pillar 2 at 0.6s; surface 0.68s as derived survivability constraint. | Line 49 + line 142 Pillar 2 design test | Pending (CD adjudication during revision) |
| 5 | `design/gdd/player-movement.md` | Cross-Component Interfaces Pull-Wave row: `TELEGRAPH_WINDOW_FLOOR_S = 0.65s` → `0.68s`; Tuning Knobs SLIP_TWEEN note recalculated under new FLOOR (margin now 30ms not 0ms); Hardware Contract subsection table updated (the verified margin column shifts from "0ms / zero margin" to "30ms / 1.8 frames"); Hardware Contract degradation rationale strengthened: the new 30ms margin absorbs sub-30ms transient hitches without invariant violation, but the `<55fps-for-2s` watchdog still catches sustained throttle. Also: structural revision pass items (F-6 re-spec, AC-34 missing cases, Shipping-Safety Enforcement Policy subsection, etc.) per R9 BLOCKING list — these are SEPARATE from this propagation but should be applied in the same revision session to minimize churn. | 10 references to `0.65` + Hardware Contract subsection + Tuning Knobs SLIP_TWEEN row + Cross-Component Interfaces table | Pending |

---

## Resolution Decisions Recorded

- **Survivability invariant resolution path**: (b) — raise TELEGRAPH_WINDOW_FLOOR_S 0.65 → 0.68s in DPC. Rationale: preserves PM "bend" Player Fantasy at 0.15s SLIP_TWEEN; largest cross-GDD blast radius but smallest design-disruption per system; explicitly listed in entity registry note as a valid resolution path.
- **PM R9 structural revision scope**: NOT applied in this propagation pass. The PM R9 BLOCKING list (22 items across 5 clusters) requires a focused structural revision session AFTER this propagation lands. R9 review log entry already captures the full scope; PM systems-index status remains NEEDS REVISION until structural pass + fresh-context R10 re-review.
- **No ADR changes**: All 3 ADRs Still Valid against the propagation surface. No new ADRs needed.
- **Game-concept Pillar 2 0.6s reference**: deferred to CD adjudication during structural revision. Default plan: retain Pillar 2 perceptual floor at 0.6s; surface 0.68s as derived survivability floor (two different constraints serving different concerns).

---

## ADRs To Be Written / Updated

**None required by this pass.** Future candidate: if PM R10 surfaces Hardware Contract enforcement-mechanism complexity (rolling-window watchdog, debug overlay player-facing UX), a dedicated ADR may be warranted then.

---

## Follow-Up Workstreams

### Status at end of 2026-06-11 session

**Completed in this session:**
1. ✅ Registry update — `TELEGRAPH_WINDOW_FLOOR_S: 0.65 → 0.68s` (revised 2026-06-11); BARRAGE_SIMULTANEITY_WINDOW_S note updated with derived 0.34s value (value-update deferred to DPC author per prior convention)
2. ✅ DPC canonical Tuning Knobs row (line 717) — `0.6s` → `0.68s` with deferral note flagging that the 34 in-document FLOOR refs remain stale and need bulk reconciliation in a focused pass
3. ✅ Change-impact doc written (this file)
4. ✅ Systems-index PM Status updated to NEEDS REVISION with R9 verdict + cross-system coordination context
5. ✅ PM R9 review log entry appended

**Discovery during DPC propagation**: the prior R2 Cluster E `0.6 → 0.65s` registry update (2026-06-08) was never propagated to the DPC GDD body — DPC still references `0.6s` throughout (34 refs). Path B now requires reconciling both updates simultaneously (`0.6 → 0.68`) rather than only the new `0.65 → 0.68`. Scope is larger than originally framed.

**Deferred to fresh session (focused reconciliation pass)**:

| # | Workstream | Files | Estimated scope |
|---|---|---|---|
| **DEFERRED-1** | DPC bulk reconciliation | `design/gdd/difficulty-phase-controller.md` | 34 in-document references: (a) ~25 simple narrative `0.6s` → `0.68s` replacements (Rule 9 definition, F-3b output range, Output Range table, narrative references); (b) Canonical TelegraphWindowCurve keys at lines 710 and 775 — re-derive at FLOOR=0.68: `(0.0, 0.92), (0.333, 0.80), (0.75, 0.68), (1.0, 0.68)` per the General Rule at line 48 (OPENER ≥ f + 0.24, MID ≥ f + 0.12, PEAK ≥ f, where `intra_phase_overlap_s = 0.12s` is design-bound); (c) AC-07a, AC-07b (lines 793-794) test fixture values 0.6 → 0.68; (d) Math worked examples — line 622 `0.45s reaction window` → `0.53s` (= FLOOR - SLIP_TWEEN at 0.68 - 0.15); line 661 same recalculation; line 350 PEAK telegraph_window_s table column 0.6 → 0.68; (e) Path B hypothetical walkthrough lines 38-48 — these reference the prior 0.6s and a hypothetical 0.9s; UPDATE framing to note Path B has been resolved at 0.68s (between the two), preserve the math walkthrough as illustrative |
| **DEFERRED-2** | Pull-Wave propagation | `design/gdd/pull-wave-behavior.md` | 28 references to `0.65s` → `0.68s` across header binding decisions + Rule 5 + ACs + worked examples; F-BARRAGE-SURVIVABILITY-INVARIANT margin recalculation (zero → 30ms); AC-PW-PEAK-NO-ADJACENT-CLUSTER Hick's Law analysis update (R7 B8 assumed 0ms margin; now 30ms softens but preserves exclusion); EC-INTER-SLIP-GATE-INTRODUCTION recomputation at FLOOR=0.68; new "60fps Hardware Contract reference" note |
| **DEFERRED-3** | PM in-document propagation | `design/gdd/player-movement.md` | 10 references to `0.65s` → `0.68s`; Cross-Component Interfaces Pull-Wave row updated; Tuning Knobs SLIP_TWEEN note recalculated (margin now 30ms not 0ms); Hardware Contract verification table updated (margin column shifts 0ms → 30ms / 1.8 frames at 60fps); Hardware Contract degradation rationale strengthened |
| **DEFERRED-4** | game-concept Pillar 2 | `design/gdd/game-concept.md` | CD adjudication needed: retain 0.6s Pillar 2 perceptual floor + surface 0.68s as derived survivability floor (default plan) OR raise Pillar 2 to 0.68s as the canonical floor. Either decision then propagates to Pillar 2 design test text |
| **DEFERRED-5** | PM structural revision pass | `design/gdd/player-movement.md` | After DEFERRED-3 lands: apply R9 BLOCKING list — F-6 full re-specification (head/arm/body co-write, timer-collision behavior, dedicated ACs); AC-34 missing cases (TweenProgress=0.0 + buffer-flush tick + boundary moments); Shipping-Safety Enforcement Policy subsection; Rule 7 / Public Interface contract reconciliation for `lateral_world_position`; `LANE_WIDTH_M` 340cm span correction; HandleStateChanged body specification; Option (c) `lateral_world_position` semantic; Hardware Contract enforcement mechanism rewrite (iOS CADisplayLink + Android Choreographer + Swappy; player-facing fallback UX); etc. Reference R9 review log entry in `design/gdd/reviews/player-movement-review-log.md` for the full 22-BLOCKING + 23-RECOMMENDED + 8-NICE-TO-HAVE punch list |
| **DEFERRED-6** | DPC fresh-context re-review | `design/gdd/difficulty-phase-controller.md` | After DEFERRED-1 lands: focused re-review to confirm AC-PILLAR-2-CONCURRENT survives at the new FLOOR. Specialists: game-designer + systems-designer + qa-lead |
| **DEFERRED-7** | Pull-Wave R7 fresh-context re-review | `design/gdd/pull-wave-behavior.md` | Was already queued independently. Apply DEFERRED-2 first, then run R7 re-review |
| **DEFERRED-8** | PM R10 fresh-context re-review | `design/gdd/player-movement.md` | Terminal step. After DEFERRED-1 through DEFERRED-5 complete. Confirms convergence |

### Recommended Next Session Order

1. DEFERRED-1 (DPC bulk reconciliation) — start fresh, allocate ~30 min, full focus on the 34 refs
2. DEFERRED-2 (Pull-Wave propagation) — can run in parallel session if multi-stream
3. DEFERRED-3 (PM in-document propagation) — after DEFERRED-1+2 (PM references DPC and Pull-Wave values)
4. DEFERRED-4 (game-concept Pillar 2) — surface CD adjudication question, then update
5. DEFERRED-5 (PM structural revision pass) — large dedicated session for R9 BLOCKING list
6. DEFERRED-6/7/8 (re-reviews) — fresh-context per system after deferred-1..5 lands

### Risk Notes

- **Compounding deferred propagations**: this is the second time the DPC FLOOR registry-vs-GDD propagation has been deferred (first deferral 2026-06-08, now also 2026-06-11). The next propagation pass MUST land DEFERRED-1 fully or risk a third deferral.
- **PM remains BLOCKED** until DEFERRED-3 + DEFERRED-5 both land. R10 cannot run until then.
- **Wave Spawner GDD (Not Started)** inherits the FLOOR change when it's authored — no propagation needed today, but worth noting in the Wave Spawner kickoff brief.

---

## Sign-off

| Role | Decision | Date |
|---|---|---|
| User | Path (b) chosen via AskUserQuestion adjudication | 2026-06-11 |
| Creative Director | (Synthesis at R9 mandated cross-system coordination FIRST — verdict honored by this pass) | 2026-06-11 |
| Producer | (Coordination of per-GDD edits + R10 re-review scheduling) | Pending |

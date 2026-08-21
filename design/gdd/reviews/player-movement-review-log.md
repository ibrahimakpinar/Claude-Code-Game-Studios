# Review Log: Player Movement (Slip)

> **⚠ LOG CLOSED AT DECOMPOSITION 2026-06-16.** This log preserves the pre-decomposition R1 through R11 review history for `player-movement.md` (the monolith, now a redirect stub — see `design/gdd/player-movement.md` for the redirect notice + section-to-sub-GDD mini-map). Post-decomposition R12+ review history for each sub-GDD lives in dedicated per-sub-GDD logs:
> - Mechanics R12+: [`player-movement-mechanics-review-log.md`](player-movement-mechanics-review-log.md)
> - Presentation R12+: [`player-movement-presentation-review-log.md`](player-movement-presentation-review-log.md)
> - Platform R12+: [`player-movement-platform-review-log.md`](player-movement-platform-review-log.md)
>
> Decomposition plan: [`player-movement-decomposition-plan-2026-06-16.md`](player-movement-decomposition-plan-2026-06-16.md).
> Do not append new entries below — the R11 entry is the terminal entry for this log.

---

## Review — 2026-05-07 — Verdict: MAJOR REVISION NEEDED
Scope signal: L
Specialists: game-designer, systems-designer, qa-lead, gameplay-programmer, unreal-specialist, creative-director
Blocking items: 17 | Recommended: 18
Summary: The core design vision was solid and the Player Fantasy was strong, but the GDD had significant specification defects. cancel-slip (reaction window ~16ms — below human motor response) was deleted entirely; the telegraph window was expanded to 0.6s to give a viable 450ms skill window. Thirteen technical spec defects were fixed: curve_t clamping, TweenProgress reset at tween start, DeltaTime cap (MAX_SLIP_DT_S), SLIP_TWEEN_DURATION_S floor validation, Death Replay handler ordering, delegate binding pattern (AddUObject + FDelegateHandle), and a full AC rewrite replacing "fires exactly once" with testable trigger hook counters (slip_complete_count, edge_absorb_trigger_count). ABORTED visual/audio description was corrected (removed false mid-arc blend description). OQ-6 added to propagate 0.6s telegraph to game-concept.md and Pull-Wave GDD.
Prior verdict resolved: No — first review

---

## Review — 2026-05-07 — Verdict: MAJOR REVISION NEEDED (re-review 2)
Scope signal: L
Specialists: game-designer, systems-designer, qa-lead, gameplay-programmer, unreal-specialist, creative-director
Blocking items: 32 | Recommended: resolved in-session
Summary: Second adversarial review identified 32 blockers across 5 domains. The core mechanic remained sound but three major gaps were closed: (1) Collision commitment was undefined — Rule 2 now specifies hitbox commits to target_lane on the SETTLED→SLIPPING frame, decoupled from visual lateral_world_position throughout the tween; (2) Body rotation was mechanically unspecified — F-5 (Rotation Output) added as a full formula, using a second UCurveFloat (LEAN_CURVE_ASSET) driving actor local X axis rotation (actor faces world -Y), with HEAD_LAG_PROGRESS secondary motion and voxel scaling at peak lean; (3) Near-miss detection interface was undeclared — OnSlipMidpoint multicast delegate added to public interface, OQ-5 marked RESOLVED. 12 ACs were strengthened or added (collision commitment AC-23, delegate firing AC-24, buffer-drop AC-25, counter semantics in AC-02/14/16/17/18, preconditions in AC-09/10). Visual/Audio section fixed rotation axis error, converted 7 frame-count timing references to ms, added buffer-drop audio cue, clarified COMPLETE snap as hard position+rotation reset. Dependencies section expanded with forward motion dual-writer conflict (option c recommended), RSM subsystem tick ordering constraint, and thread-safety contract. SLIP_TWEEN_DURATION_S minimum raised from 0.08s to 0.10s for shelf visibility at 30fps.
Prior verdict resolved: Yes — 17 blockers from Review 1 were revised in-session before this review; this review identified 32 new blockers all revised in-session.

---

## Review — 2026-05-07 — Verdict: MAJOR REVISION NEEDED (re-review 3)
Scope signal: XL
Specialists: game-designer, systems-designer, qa-lead, gameplay-programmer, unreal-specialist, audio-director, creative-director
Blocking items: 28 | Recommended: 20
Summary: Third review found 28 deduplicated blockers across 6 domains. No prior blockers were re-discovered; all findings are new. Four problem clusters dominate: (1) Spec drift — F-5 settle overshoot contradicts itself in three places (boundary table claims clamped, prose says remove clamp, pseudocode has no clamp; three specialists flagged independently); (2) Buffer testability — 13 ACs assert buffer state that is unobservable through the public interface; AC-24 contradicts the test strategy; AC-16 uses wrong TweenProgress sentinel (1.0 vs. 0.0); F-5 has zero ACs; (3) New creative-director finding — collision commitment model is architecturally defensible but a commitment-tell is missing, leaving Pillar 5 unearned; (4) Audio direction signal — panning is the sole direction cue but collapses for speaker-mode mobile players (the majority target audience). Additional blockers: safe-range corner violation in MAX_SLIP_DT_S constraint; null checks absent from F-3/F-5 pseudocode; EC-15 edge-absorb during SLIPPING entirely unspecified (creative-director recommends MVP cut); near-miss beat trigger interface missing; log category declarations commented out (compile break); OQ-6 not marked as Pull-Wave authoring blocker. Creative-director recommends out-of-session structural rewrite of the four clusters, not another in-session patch session.
Prior verdict resolved: Yes — 32 blockers from Review 2 were resolved in-session before this review; this review identified 28 new blockers, not resolved in-session.

---

## Review — 2026-05-13 — Verdict: MAJOR REVISION NEEDED (re-review 4)
Scope signal: XL
Specialists: game-designer, systems-designer, qa-lead, gameplay-programmer, audio-director, unreal-specialist, creative-director (senior synthesis)
Blocking items: 23 | Recommended: 8
Summary: Fourth review found 23 blockers against the GDD unchanged since Review 2. The blocker pattern (17→32→28→23) matches the Input System trajectory that triggered a canonical-source rewrite recommendation. Three structural deficits drive the majority of findings: (1) Player-perceivability is not a first-class section — commitment-tell absent (Pillar 5 violated: hitbox commits invisibly before any animation evidence), audio-vs-visual responsibility split undeclared, near-miss trigger API missing from PM's public interface; (2) Authored-asset contracts not specified — SlipCurve/LeanCurve have no boundary behavior or authoring tolerance spec, producing HEAD_LAG_PROGRESS head-snap on every slip completion and three-way F-5 settle-overshoot contradiction; (3) Cross-component interface surface incomplete — no RSM storage contract, AddTickPrerequisiteComponent API incompatible with subsystem RSM, SetRelativeRotation vs. SetActorRotation unspecified (silent behavioral error). Additional blockers: 14 ACs assert unobservable buffer state; F-5 has zero ACs and no public interface observability; AC-24 contradicts test strategy; Rule 8/9 TweenProgress sentinel gap; MAX_SLIP_DT_S safe-range corner violation; pan direction encoding functionally vs. aesthetically ambiguous; overlapping slip cue layering unspecified; log macros commented out; null dereference in Shipping tick. Creative-director recommends structural rewrite adding three new sections (Player-Perceivable State, Authored Asset Contracts, Cross-Component Interfaces). Pre-rewrite design decisions resolved: commitment-tell = Option A (1-frame lock-in flash on SETTLED→SLIPPING); game-concept.md 0.4s→0.6s update required; pan declared aesthetic-only.
Prior verdict resolved: No — GDD not revised since Review 2. 28 blockers from Review 3 remain open. Not revised in-session — structural rewrite approach taken per CD recommendation.

---

## Review — 2026-05-14 — Verdict: NEEDS REVISION (re-review 6, lean)
Scope signal: XL
Specialists: None (--depth lean)
Blocking items: 3 | Recommended: 4
Summary: Lean re-review confirming all 21 re-review 5 blockers were resolved. Three new blockers found and resolved in-session: (1) F-6 example specified wrong tween direction — "Left→Center (lean_sign = +1.0)" is inconsistent with F-5's lean_sign formula (Left→Center = lean_sign -1.0); corrected to "Center→Left" with an explanation that lean_sign must be captured as a member variable for the F-6 tail phase; (2) Visual/Audio Near-Miss Beat said "No translation / 33ms micro-hold at current position" contradicting the authoritative Player-Perceivable State ("tween does NOT pause"); reworded to "No additional lateral translation — active tween continues via F-2 normally"; (3) public interface described lean_angle as "0.0 when SETTLED" without accounting for F-6 tail phase (edge-absorb timer continuing after tween completes produces non-zero lean_angle during SETTLED); interface entry and AC-26 now clarify the F-6 tail exception. Convergence trajectory: 17→32→28→23→21→3→0.
Prior verdict resolved: Yes — all 21 re-review 5 blockers resolved in-session before this review; 3 new blockers identified and resolved in-session.

---

## Review — 2026-05-13 — Verdict: NEEDS REVISION (re-review 5)
Scope signal: XL
Specialists: game-designer, systems-designer, qa-lead, unreal-specialist, audio-director, creative-director (senior synthesis)
Blocking items: 21 | Recommended: 15
Summary: First review of the structural rewrite. The rewrite succeeded architecturally — all three ordered sections (Player-Perceivable State, Authored Asset Contracts, Cross-Component Interfaces) are present and correctly shaped; the trajectory is converging (17→32→28→23→21 unique after dedup). The Creative Director issued two binding reversals: (1) Option A commitment-tell duration reversed from 1-frame to 2-frame minimum + 50ms decay tail (1-frame flash is sub-perceptual under mobile thermal throttle, violating Pillar 5); (2) edge-absorb audio revised to mono-front with no pan (pan contradiction reintroduced — same class of error as Review 2; process note: add a "Locked Decisions" header to the audio section). Top clusters: commitment-tell prose self-contradiction (3 agents flagged independently); near-miss beat composition with active SLIPPING tween undefined; edge-absorb additive lean has no formula (EC-15 clamp has no operand); HEAD_LAG_PROGRESS safe range [0.05–0.15] falsified at both endpoints (only 0.10 satisfies the constraint — tighten to [0.08–0.12]); AC systemic testability (7 properties must be added to the exposed interface, test curves must be pinned); 3 UE Shipping-safety fixes (checkf→runtime guard, FindComponentByClass+subsystem reconciliation, option-b double-tick warning); 5 audio spec fixes. No structural rewrite needed — one focused authoring session plus a lean re-review to confirm.
Prior verdict resolved: Yes — structural rewrite completed per Review 4 CD recommendation. Prior 23 blockers addressed by rewrite; 21 new blockers identified.

---

## Review — 2026-05-14 — Verdict: APPROVED (re-review 7, lean)
Scope signal: XL
Specialists: None (--depth lean)
Blocking items: 0 | Recommended: 2
Summary: Fresh-session re-review confirming all three Review 6 in-session fixes are correctly applied: F-6 example direction (Center→Left, lean_sign = +1.0 ✓), near-miss beat translation note consistent with Player-Perceivable State ✓, lean_angle F-6 tail exception in public interface and AC-26 ✓. No new blockers found. Two advisory recommendations: review log chronological ordering (Review 6 entry precedes Review 5 in the file), and a non-blocking implementation note that SETTLED Rule-1 edge-absorb lean is animation-driven (not formula-driven) and is not reflected in lean_angle. All 8 required sections present; 33 acceptance criteria all independently testable; formula boundary protection complete. Full convergence: 17→32→28→23→21→3→0.
Prior verdict resolved: Yes — all 3 Review 6 in-session blockers confirmed resolved in the current document.

---

## Review — 2026-06-11 — Verdict: MAJOR REVISION NEEDED (re-review 8, R7-PM-PROPAGATION re-review, full)
Scope signal: L
Specialists: qa-lead, systems-designer, gameplay-programmer, game-designer, audio-director, creative-director (senior synthesis)
Blocking items: 4 | Recommended: 9 | Nice-to-have: 7
Summary: First fresh-context re-review of the 2026-06-10 R7-PM-PROPAGATION 17-edit delta applied via `/propagate-design-change` after Pull-Wave R7 approval. The deltas (3→5 lanes, LANE_OFFSET_CM→LANE_WIDTH_M, SLIP_TWEEN safe range tightening, source-lane semantic affirmation, OnSlipMidpoint optional, F-1 restructure) were mechanically clean at the propagation level but surfaced four BLOCKING defects at the implementability surface. (B1) qa-lead: no AC observes `current_lane == source_lane` during SLIPPING despite Rule 4/Pull-Wave R11 marking it BINDING — AC-23 tests `lateral_world_position` on transition frame, AC-24 tests delegate args, but the property itself goes unobserved across the full SLIPPING window. (B2) systems-designer: Rule 7 (DEAD) sets `current_lane = target_lane` but doesn't set `movement_state` (Rules 8/9 do); during run-termination wave drain, in-flight Pull-Wave LANDED ticks read both fields and would see source-lane semantic broken if movement_state still SLIPPING. (B3) systems-designer: F-BARRAGE-SURVIVABILITY-INVARIANT is continuous-time only; at 30fps the frame-quantized per-tween becomes 5×0.0333=0.167s; M=3 chain = 0.501+0.20 = 0.701 > 0.65 floor (fails by 51ms). PM is a load-bearing party because SLIP_TWEEN safe-range upper is one variable in the inequality. (B4) gameplay-programmer: seam doc EMovementState has 4 members {SETTLED, SLIPPING, BUFFERED, DEAD} but PM's ERunSlipState has 2 — static_cast across them is silently wrong without ordinal pinning. Tests can exercise unreachable branches. Game-designer (alone) issued APPROVED — scoped narrowly to fantasy delivery — but creative-director overrode to REJECT given the four orthogonal implementability defects. CD ruling: Finding 3 BLOCKS PM (cannot approve past invariant failure within PM's own MAX_SLIP_DT_S envelope); in-session revision pass is correct (not hand-back). Critical RECOMMENDED bundled: audio slip cue 120-180→100-150ms; stale OnSlipMidpoint refs (line 61, EC-14, Dependencies); LANE_WIDTH_M floor 0.75→0.85; Player Fantasy 5-lane acknowledgment.
Prior verdict resolved: No — the prior APPROVED verdict was for the pre-R7-PM-PROPAGATION document; this re-review evaluates the propagation delta itself and finds 4 new BLOCKING defects introduced by integration-level surface exposure.

---

## Revision Pass — 2026-06-11 — R7-PM-PROPAGATION-REVIEW in-session revision applied
All 4 BLOCKING + 4 critical RECOMMENDED resolved via user adjudication on 2 design decisions and 6 mechanical fixes.

**User design decisions:**
- D1 (Finding 3 invariant fix path): **Lock 60fps minimum hardware contract** — F-BARRAGE-SURVIVABILITY-INVARIANT verified at 60fps: per-tween = 9×0.01667 = 0.150s; chain = 0.45+0.20 = 0.65 ≤ 0.65 ✓. New Hardware Contract section added under Cross-Component Interfaces with frame-quantized verification table. Cross-system propagation to Pull-Wave + DPC required via `/propagate-design-change`.
- D2 (Finding 4 EMovementState resolution): **Prune EMovementState to {SETTLED=0, SLIPPING=1} with pinned ordinals**. Seam 12 (`platform-seam-interfaces.md`) updated. PM Cross-Component Interfaces gains Movement State Enum subsection declaring `ERunSlipState` pinned ordinals matching the seam.

**Mechanical fixes (no user decision needed):**
- B1: AC-34 NEW asserts `current_lane == source_lane` at TweenProgress ∈ {0.1, 0.5, 0.99} across Case A (Left→Center), Case B (FarLeft→Left enum boundary), Case C (Right→FarRight opposite boundary). RECOMMENDED implementation aid added: `TweenSourceLane` debug-only member + `checkf` at SLIPPING→SETTLED.
- B2: Rule 7 explicitly sets `movement_state = SETTLED` (TweenProgress deliberately preserved at fractional value for visual freeze pose). AC-14 updated to assert `movement_state == SETTLED`. Public Interface `movement_state` row updated.
- R5 (audio): Slip cue duration range 120–180ms → 100–150ms.
- R6 (stale OnSlipMidpoint refs): corrected at line 61 Near-Miss Beat Trigger API, EC-14 near-miss beat bullet, Dependencies table downstream row, Cross-System Interface Table Pull-Wave row.
- R7 (LANE_WIDTH_M floor): Tuning Knobs table safe range 0.75 → 0.85; upper-bound camera framing constraint note added.
- R8 (Player Fantasy 5-lane): one-sentence acknowledgment in Overview ("reading the wave is reading a direction — not counting steps") and Player Fantasy implementation-demands bullet ("5-lane decision texture").
- R9 (SLIP_TWEEN BeginPlay assertion): documented in Tuning Knobs row.

**Files modified by R7-PM-PROPAGATION-REVIEW revision pass:**
- `design/gdd/player-movement.md` (header status block + 11 substantive edits)
- `docs/architecture/platform-seam-interfaces.md` (Seam 12 EMovementState pruned to 2 members with pinned ordinals)
- `design/gdd/reviews/player-movement-review-log.md` (this file — review 8 + revision pass entry)

**Files NOT modified by this pass (deferred):**
- `design/gdd/pull-wave-behavior.md` (60fps hardware contract propagation deferred to `/propagate-design-change` post-R8-APPROVED)
- `design/gdd/difficulty-phase-controller.md` (same — 60fps propagation queued)
- `design/registry/entities.yaml` (no new shared constants; existing entries unchanged by this pass)
- `design/gdd/systems-index.md` (PM Status update at re-review widget after this revision pass)

---

## Review — 2026-06-11 — Verdict: MAJOR REVISION NEEDED (re-review 9, full, fresh-context post-R7-PM-PROPAGATION-REVIEW revision pass)
Scope signal: L (with cross-system dependency)
Specialists: game-designer, systems-designer, qa-lead, gameplay-programmer, audio-director, ux-designer, creative-director (senior synthesis)
Blocking items: 22 | Recommended: 23 | Nice-to-have: 8
Summary: Fresh-context re-review of the 2026-06-11 R7-PM-PROPAGATION-REVIEW in-session revision pass (which had addressed 4 BLOCKING + 4 critical RECOMMENDED items from R8). Convergence trajectory 17→32→28→23→21→3→0→4→**26** — sharp R8→R9 spike from 4→26 indicates propagation depth from the 5-lane widening exceeded what in-session revision can absorb. Phase 2/2b checks pass: 8/8 required sections present; all project-authored interfaces (`IPlayerMovementProvider`, `IHapticDispatch`) authored in seam doc / ADR-0002 with matching pinned ordinals; all dependency GDDs exist.

**Five problem clusters surfaced** (22 BLOCKING total):

1. **Pillar 5 + Hardware Contract (cross-system, load-bearing)** — multi-specialist convergence (game-designer + ux-designer + gameplay-programmer): zero-margin F-BARRAGE-SURVIVABILITY-INVARIANT (`SLIP_TWEEN × 3 + REACTION_BUDGET = 0.45 + 0.20 = 0.65 ≤ 0.65` exact equality) means a single transient frame hitch or GC pause at 60fps kills the player on M=3 PEAK barrage — Pillar 5 violation (mystery death attributable to device, not skill). 60fps Hardware Contract lock does NOT close cliff — `<55fps-for-2s-sustained` degradation check ignores isolated spikes. `t.MaxFPS = 60` is a UE cap, NOT a floor — Hardware Contract enforcement mechanism conceptually broken. Watchdog mechanism conflates BeginPlay one-shot check with rolling-window measurement; rolling window location unspec'd; "debug overlay" is developer concept shipped to players with no fallback UX, no recovery path, no named min-spec devices.

2. **Shipping-build enforcement gap (cross-cutting)** — `ensureMsgf(SLIP_TWEEN ≤ 0.15f)` BeginPlay assertion is no-op in Shipping; designer can ship 0.16s silently breaking invariant. `ERunSlipState`/`EMovementState` lockstep relies on prose only — no `static_assert` adjacent to seam cast site. `AddTickPrerequisiteComponent` option (b) silent-double-tick has no `check()` guard. Null curve guards Warning-only; Shipping silently falls back to linear/zero. AC-21 one-shot floor-guard `bFloorGuardFired` bypasses subsequent violations silently. Same failure class throughout: assertions that strip in Shipping cannot be the only line of defense for BLOCKING invariants.

3. **F-6 systemic underspecification** — F-6 decouples head/arm from body during edge-absorb tail (dual-writer defect: F-6 writes `lean_angle` only, F-5 still drives `head_lean_angle` and `arm_lean_angle`). F-6 timer behavior when new SLIPPING transition arrives mid-tail is fully undefined (three plausible behaviors: override / kill / accumulate). Zero ACs cover F-6 timer behaviors. F-6 was authored at R5 of the original review to solve EC-15; the 5-lane widening created additive edge-absorb paths and chained-mash timer collisions that the original F-6 spec never anticipated.

4. **AC-34 + AC coverage gaps** — AC-34 (added in R8 revision pass) tested only TweenProgress ∈ {0.1, 0.5, 0.99}; missing TweenProgress=0.0 transition-tick sample (highest-probability Pull-Wave LANDED-coincidence frame); missing buffer-flush execution tick case (compound tick where prior tween completes and new tween begins). No ACs assert counters are NOT reset on DEAD/COMPLETE/ABORTED/pause/grace-expiry (silent zero-on-DEAD bug invisible). Hardware Contract degradation path has zero ACs (unenforceable per CLAUDE.md "AC must be testable"). AC-14 boundary moments TweenProgress=0.001 / 0.999 not covered.

5. **Other contract gaps** — Rule 7 contradicts Public Interface (`lateral_world_position` is "lane-anchored when SETTLED" per Public Interface but Rule 7 leaves it fractional with `movement_state == SETTLED`; effectively a third SETTLED-but-fractional state the 2-member enum cannot represent). `LANE_WIDTH_M = 0.85` floor produces 340cm track span, contradicting the GDD's "400cm preserved at all tunings" claim throughout. Thumb-target floor is the WRONG constraint for `LANE_WIDTH_M` safe-range justification (input system uses dead-band split with no per-lane tap target — category error). `HandleStateChanged` and `HandlePausedChanged` method bodies completely unspecified. `lateral_world_position` semantics undefined under recommended Option (c) stationary-player model. Slip cue proportional scaling is informal comment not enforced contract. Near-miss audio suppressed under slip priority breaks Pillar 5. Mono-front edge-absorb creates implicit directional signaling via pan-absence (contradicts locked decision banning pan as direction signal).

**Specialist convergences (zero active disagreements at R9)**:
- game-designer B-1 + gameplay-programmer B-1/B-2 + ux-designer B-2/B-3: Pillar 5 + Hardware Contract broken
- game-designer B-2 + gameplay-programmer B-5 + qa-lead B-6: Shipping enforcement gap
- qa-lead B-1/B-2 + systems-designer B-4: AC-34 missing cases
- systems-designer B-1/B-2 + qa-lead B-5: F-6 systemic gaps

**CD senior verdict**: MAJOR REVISION NEEDED with cross-system propagation gate. NOT a cluster spike (analogous to R6→R7 lean→APPROVED — that was single-domain polish); NOT addressable by another in-session pass (R8 in-session pass added one AC that missed 3 of 4 required cases). Pattern is mixed structural + cross-system cascade; cross-system dependency is load-bearing. **R7-PM-PROPAGATION sequence has NOT converged.**

**Three things must happen in order (CD-mandated)**:

1. **Cross-system coordination FIRST (before any PM revision)** — Pillar 5 zero-margin invariant cannot be resolved at the PM layer alone. PM cannot unilaterally tighten SLIP_TWEEN without DPC FLOOR coordination; DPC cannot unilaterally raise FLOOR without PM confirmation. Three options to resolve survivability margin ≥1 frame at 60fps: (a) tighten `SLIP_TWEEN_DURATION_S` to ≤0.13s (PM-side); (b) raise `TELEGRAPH_WINDOW_FLOOR_S` to ≥0.68s (DPC-side, requires DPC GDD revision); (c) restrict M=3 PEAK barrages to player-adjacent-safe-lane configurations (Wave Spawner forward contract). Open coordination session: DPC + PM + Pull-Wave authors.

2. **Structural revision pass after cross-system decision lands** — Hand back to author with scoped revision brief, NOT another in-session patch. Top 3 priority items for author: (i) Pillar 5 survivability margin — apply whichever fix cross-system session chose with explicit margin calculation; (ii) F-6 full re-specification covering head/arm/body co-write contract with F-5, timer-collision behavior (override/kill/accumulate decision), dedicated ACs; (iii) new "Shipping-Safety Enforcement Policy" subsection: every BLOCKING invariant requires BOTH an `ensure` (dev visibility) AND a runtime clamp/guard (Shipping safety) — closes B4/B5/B14 as a class.

3. **Fresh-context R10 re-review** — only after (1) and (2) complete.

**CD severity recalibration applied (vs R9 specialist verdicts)**:
- Escalated to BLOCKING: systems-designer R-2 (F-5/F-6 write order) — part of F-6 re-specification, not optional
- Downgrade RECOMMENDED → NICE-TO-HAVE: systems-designer R-3 (1-frame shelf at lower bound), gameplay-programmer R-1 (UMovementComponent threading) — deferrable past PM approval

Prior verdict resolved: Yes — R8's 4 BLOCKING items WERE mechanically closed by the R7-PM-PROPAGATION-REVIEW revision pass (AC-34 added, Rule 7 movement_state=SETTLED added, 60fps Hardware Contract section added, EMovementState pruned). However, R8's in-session revision surfaced 22 new BLOCKING items at the integration surface (cross-system, Shipping enforcement, F-6 systemic, AC coverage matrix) — these are NOT regressions of R8 fixes but propagation-depth defects R8 did not have time to expose.

**User-chosen path (2026-06-11)**: Open cross-system coordination first. PM revision deferred until DPC + PM + Pull-Wave session resolves the survivability invariant.

**Files modified by this review pass:**
- `design/gdd/systems-index.md` (PM Status updated to NEEDS REVISION with R9 verdict and 5-cluster summary)
- `design/gdd/reviews/player-movement-review-log.md` (this file — R9 entry appended)

**Files NOT modified by this pass:**
- `design/gdd/player-movement.md` (no in-session revision applied — CD escalation queued)
- `design/gdd/pull-wave-behavior.md` (cross-system coordination session will determine if changes needed)
- `design/gdd/difficulty-phase-controller.md` (same — coordination session may raise TELEGRAPH_WINDOW_FLOOR_S)
- `docs/architecture/platform-seam-interfaces.md` (no seam changes — Seam 12 still valid)
- `design/registry/entities.yaml` (no new shared constants from R9 review)

---

## Cross-System Coordination Resolution — 2026-06-11 — Verdict: Option (c) BOUND

Scope signal: L (cross-system, multi-GDD)
Specialists: game-designer, systems-designer, creative-director (senior adjudication)
Originating review: R9 (above) — survivability invariant zero-margin cliff

**Decision**: Option (c) bound — Wave Spawner cook-time consecutive-triplet exclusion (Pull-Wave R7 B8 ADVISORY → BLOCKING; `MIN_ESCAPE_SLIPS` reduced from 3 to 2 as a new registry constant). PM `SLIP_TWEEN_DURATION_S = 0.15s` UNCHANGED. TELEGRAPH_WINDOW_FLOOR_S = 0.65s UNCHANGED.

**Why (c) over (a) or (b)** — adjudication summary:
- (a) tighten SLIP_TWEEN to 0.13s: at REACT=0.25s registered ceiling, `0.13 × 3 + 0.25 = 0.64` → 10ms (0.6 frame) margin — cliff RELOCATED, not eliminated.
- (b) raise TELEGRAPH_WINDOW_FLOOR_S to 0.68s: at REACT=0.25s, `0.15 × 3 + 0.25 = 0.70 > 0.68` → BREAKS invariant.
- (c) cook-time triplet exclusion: at REACT=0.25s, `0.15 × 2 + 0.25 = 0.55 ≤ 0.65` → 100ms (6 frames) margin. Survives every boundary combination.
- Pillar-5 premise accepted: invariant must hold across the FULL REACTION_BUDGET safe range [0.20, 0.25], no degraded-attention carve-out.

**Specialist disagreement adjudicated**: game-designer recommended (b), arguing (c) was broken by voluntary slipping during the telegraph window. CD enumeration showed they were reasoning against (c-naive) (spawn-time check against player's current lane). systems-designer proposed (c-actual) — pure geometric cook-time exclusion with no player-position input. Safety property is uniform over all 5 player lanes; voluntary slipping moves the player from one ≤2-slip-escapable position to another ≤2-slip-escapable position. CD confirmed empirically by enumerating all 7 surviving M=3 triplets × 5 player lanes.

**Implication for PM R10 structural revision (NEXT)**: SLIP_TWEEN_DURATION_S is NOT changed by this coordination. PM R10 author brief is now scoped to the THREE structural issues orthogonal to survivability:
1. **F-6 systemic re-specification** — head/arm/body co-write contract with F-5; timer-collision behavior on new SLIPPING transition mid-tail (override / kill / accumulate decision); dedicated ACs. Forward decision required from author.
2. **Shipping-Safety Enforcement Policy** — new GDD subsection requiring every BLOCKING invariant to have BOTH `ensure` (dev visibility) AND runtime clamp/guard (Shipping safety). Closes cluster-2 failure class.
3. **Hardware Contract enforcement mechanism** — `t.MaxFPS = 60` is a UE cap not a floor (wrong engine semantic for the use case); rolling-window FPS watchdog location + threshold + action-on-breach unspec'd; "debug overlay" is a developer artifact not shipping UX; min-spec device list absent. CD scope is to flag; technical-director + performance-analyst own the resolution mechanism. Minimum acceptable PM R10 outcome: a Shipping-build runtime DT watchdog with explicit fallback UX OR a PEAK-density gate, plus named min-spec device list, plus replacement of the `t.MaxFPS = 60` prose with the actual UE mechanism.

**Forward contracts queued for Pull-Wave R8 substantive revision** (recorded in `design/gdd/reviews/pull-wave-behavior-review-log.md` 2026-06-11 cross-system entry):
1. R7 B8 status ADVISORY → BLOCKING
2. New AC-WS-COOK-EXCLUDE-CONSECUTIVE-M3 (BLOCKING)
3. F-BARRAGE-SURVIVABILITY-INVARIANT M reduced to 2 (with R7 B8 BLOCKING annotation)
4. AC-PILLAR-2-BARRAGE-SPATIAL-K re-verify: 7 surviving / 1.75× margin
5. Wave Spawner authoring guide listing the 7 surviving triplets explicitly: `{0,1,3}, {0,1,4}, {0,2,3}, {0,2,4}, {0,3,4}, {1,2,4}, {1,3,4}`

**Companion artifact**: full adjudication at `design/gdd/reviews/cross-system-survivability-coordination-2026-06-11.md` (16KB; durable record of game-designer, systems-designer, creative-director synthesis).

**Files modified by this resolution** (2026-06-11):
- `design/registry/entities.yaml` (TELEGRAPH_WINDOW_FLOOR_S reverted 0.68→0.65 + Option-(c) note; BARRAGE_SIMULTANEITY_WINDOW_S withdrew speculative 0.34s derivation; MIN_BARRAGE_LANE_SEPARATION annotated with 2026-06-11 resolution; new constant MIN_ESCAPE_SLIPS=2 added; REACTION_BUDGET closing paragraph updated to Option (c) math)
- `design/gdd/difficulty-phase-controller.md` (single resolution note on TELEGRAPH_WINDOW_FLOOR_S Tuning Knob row)
- `design/gdd/reviews/pull-wave-behavior-review-log.md` (cross-system entry — Pull-Wave R8 forward contracts queued)
- `design/gdd/reviews/player-movement-review-log.md` (this entry)
- `design/gdd/reviews/cross-system-survivability-coordination-2026-06-11.md` (durable adjudication artifact, written by creative-director)

**Files NOT modified by this resolution** (queued for next revision cycle):
- `design/gdd/player-movement.md` — PM R10 structural revision (F-6 + Shipping-Safety + Hardware Contract enforcement). SLIP_TWEEN unchanged.
- `design/gdd/pull-wave-behavior.md` — Pull-Wave R8 substantive revision applies the 5 forward contracts above.

**Prior verdict resolved**: R9 verdict (MAJOR REVISION NEEDED, 22 BLOCKING) addressed for Cluster 1 (Pillar 5 + Hardware Contract) at the cross-system layer. Clusters 2–5 remain open for PM R10 structural revision. PM R10 forecast unchanged from R9 CD synthesis (NOT addressable by in-session pass; hand back to author with scoped brief).

---

## Review — 2026-06-12 — Verdict: MAJOR REVISION NEEDED (lean re-review, no specialists, post-cross-system-resolution drift sweep)
Scope signal: L
Specialists: None (`--depth lean`)
Blocking items: 23 (22 carried from R9 + 1 NEW) | Recommended: 23 (carried from R9) | Nice-to-have: 8 (carried from R9)

**Trigger**: `/clear` → `/design-review design/gdd/player-movement.md` invoked. Session-state plan was for the PM author to apply the R10 brief at `design/gdd/reviews/player-movement-r10-author-brief-2026-06-11.md` independently, then this re-review would be a true R10 fresh-context pass. Phase 1 verification (header inspection + grep for R10 markers) showed the GDD body has NOT been edited since R9 — header still reads `R7-PM-PROPAGATION re-review revision pass`; none of the R10 distinctive markers (`shipping-safety`, `min-spec`, `audio_cue_ratio`, `AC-COUNTER`, `AC-HW-`, `AC-F6-`, F-6 re-spec subsection, `HandleStateChanged` method body) appear in the file. Cross-checked against R9 entry line 153 and Cross-System Coordination Resolution line 199, both of which state `design/gdd/player-movement.md` was NOT modified. User was surfaced this with an `AskUserQuestion` widget and chose to proceed with `--depth lean` as a drift sweep (single-session, no specialist token burn).

**Phase 2 / 2b**:
- 8/8 required sections present ✓
- Phase 2b seam-doc grep clean: project-authored interfaces (`IPlayerMovementProvider`, `IHapticDispatch`) both authored in `docs/architecture/platform-seam-interfaces.md` Seam 12 + ADR-0002; no paper-only-seam findings; no paper-only-stub-field findings.
- Dependency graph: `input-system.md` ✓, `run-state-machine.md` ✓, `pull-wave-behavior.md` ✓ (R8 in-session revision applied; R9 fresh-context queued). Downstream collision-system.md / camera-system.md / death-replay.md ✗ — all acknowledged as "not yet authored" in PM Dependencies; queued post-PM-Approval, not a blocker.

**Outcome — all R9 BLOCKING / RECOMMENDED / NICE-TO-HAVE items remain open**: document unchanged; R9 findings durable. Will not re-enumerate — see R9 entry above.

**NEW BLOCKING (post-R9, propagation-staleness drift introduced by 2026-06-11 Cross-System Coordination Resolution)**:

- **B-LEAN-2026-06-12-1 — Hardware Contract + SLIP_TWEEN Tuning Knobs prose stale against `MIN_ESCAPE_SLIPS=2` binding.** The Cross-System Coordination Resolution (2026-06-11) bound Option (c) and added `MIN_ESCAPE_SLIPS=2` to `design/registry/entities.yaml` (with the 7-surviving M=3-triplet authoritative list deferred to Wave Spawner cook-time exclusion). PM's Hardware Contract section (lines 212–234) and SLIP_TWEEN Tuning Knobs row + prose (lines 592, 603) still cite the pre-Option-(c) invariant form `SLIP_TWEEN × M=3 + REACTION_BUDGET=0.20 ≤ FLOOR=0.65` throughout, including the frame-quantized verification table (60fps zero margin / 50fps fails by 30ms / 30fps fails by 51ms) and the "60fps mobile minimum or the body literally cannot move fast enough at 30fps" justification.

  Under the bound `MIN_ESCAPE_SLIPS=2`:
  - 60fps: `0.150 × 2 + 0.20 = 0.500 ≤ 0.65` → 150ms margin (9 frames)
  - 50fps: `0.160 × 2 + 0.20 = 0.520 ≤ 0.65` → 130ms margin
  - 30fps: `0.167 × 2 + 0.20 = 0.533 ≤ 0.65` → 117ms margin

  The R7-PM-PROPAGATION-REVIEW survivability arithmetic justification for the 60fps lock is mathematically false under Option (c). The 60fps lock may still be defensible on Player Fantasy "body moves before mind finishes the sentence" feel grounds, camera shader coherence, or other non-survivability reasons — but those reasons must be re-articulated separately. The frame-quantized verification table is also stale and must be regenerated against MIN_ESCAPE_SLIPS=2.

  **Required fix scope (extends R10 brief §1.3 Hardware Contract enforcement rewrite — was implicit, surfacing explicitly)**:
  1. Replace `M=3` literal in Hardware Contract invariant statement (line 214) with `MIN_ESCAPE_SLIPS` registry reference.
  2. Regenerate frame-quantized verification table at MIN_ESCAPE_SLIPS=2 (60/50/30/20fps columns) showing the invariant holds with comfortable margin at all supported framerates.
  3. Re-derive the 60fps lock on its actual surviving justification (Player Fantasy / camera / shader timing) OR relax the framerate floor if no non-survivability justification holds at the author/CD level.
  4. Update SLIP_TWEEN Tuning Knobs row text (line 592) and SLIP_TWEEN prose paragraph (line 603) to cite `MIN_ESCAPE_SLIPS` form (currently cite `M=3` explicitly).
  5. Update SLIP_TWEEN_DURATION_S "above 0.15s violates the invariant immediately" prose — under MIN_ESCAPE_SLIPS=2, the upper bound on SLIP_TWEEN can in principle relax (per the same arithmetic). Author/CD must decide whether the [0.10, 0.15]s safe range is still binding or whether it can widen back toward the pre-R7-PM-PROPAGATION [0.10, 0.25]s range. This is a Player Fantasy + tuning-headroom design decision, not an arithmetic finding.

**Phase 3 — Consistency and Implementability**: no other new findings beyond B-LEAN-2026-06-12-1. The other 22 R9 BLOCKING items + 23 RECOMMENDED items remain open. (Lean mode = no specialist deep dives; further drift in non-Hardware-Contract regions cannot be ruled out, but the targeted grep for new specialist-domain-relevant markers (HandleStateChanged bodies, audio_cue_ratio, AC-* additions, etc.) showed no R10 work has been applied anywhere.)

**Senior Verdict**: N/A — lean mode, no creative-director synthesis. The R9 CD synthesis stands: NOT addressable by another in-session pass; hand back to author with scoped revision brief. The 1 new BLOCKING above extends R10 brief §1.3 scope.

**CD recommendation respected**: user explicitly chose "Stop — author revises in separate session" at this lean re-review's verdict-stage widget, matching the R9 CD ruling exactly.

**Scope signal**: L — unchanged from R9.

**Files modified by this pass**:
- `design/gdd/systems-index.md` (PM row appended with 2026-06-12 lean re-review note)
- `design/gdd/reviews/player-movement-review-log.md` (this file — this entry appended)

**Files NOT modified by this pass**:
- `design/gdd/player-movement.md` (lean re-review is read-only on the target GDD)
- `design/gdd/reviews/player-movement-r10-author-brief-2026-06-11.md` (author should fold B-LEAN-2026-06-12-1 into the brief in their next revision pass; this lean re-review entry stays in the review log as the authoritative source of the new finding)
- `design/registry/entities.yaml` (no new shared constants — MIN_ESCAPE_SLIPS=2 already bound 2026-06-11)
- `docs/architecture/platform-seam-interfaces.md` (no seam changes)

**NEXT (user-chosen 2026-06-12)**: Stop here. Author works through R10 brief (12–18 hours per brief estimate) plus the new B-LEAN-2026-06-12-1 Hardware Contract / SLIP_TWEEN prose update. Then `/clear` → fresh-session `/design-review design/gdd/player-movement.md` for true R10 fresh-context re-review.

**Prior verdict resolved**: No — R9 verdict carries unchanged because the GDD body was not modified. This entry adds 1 new BLOCKING introduced by post-R9 cross-system registry drift; total BLOCKING count moves 22 → 23.

---

## In-Session R10a Revision Pass — 2026-06-14 (author revision applying R10 brief + B-LEAN-2026-06-12-1 fold-in)

**Trigger**: User opened a fresh `/clear` session and instructed "let continue R10a". Per active.md session-state plan, R10a is the first author revision pass on `design/gdd/player-movement.md` against the scoped brief at `design/gdd/reviews/player-movement-r10-author-brief-2026-06-11.md` (drafted 2026-06-11) plus the post-brief extension B-LEAN-2026-06-12-1 from the 2026-06-12 lean drift re-review (entry above). Pre-R10a GDD state: 853 lines, R7-PM-PROPAGATION-REVIEW state with R9 22 BLOCKING + 1 lean re-review BLOCKING = 23 BLOCKING items + 23 RECOMMENDED + 8 NICE-TO-HAVE all open.

**Author decisions locked at session open (8 decisions; surfaced via AskUserQuestion in 2 batches of 4)**:

| ID | Decision | Choice |
|---|---|---|
| §5.1 | F-6 timer-collision behavior on new SLIPPING mid-tail | (a) Override — F-6 timer killed when new tween starts; F-5 takes over cleanly. Input responsiveness wins over animation continuity per Player Fantasy. |
| §5.2 | Hardware Contract action on sustained <55fps DT watchdog breach | (a) Survivability margin relaxation — Wave Spawner gate suppresses M=3 PEAK barrage class; M=2 surviving triplet set remains the only PEAK barrage class. |
| §5.3 | Shipping-build fallback UX shape when watchdog fires | (a) Banner notification — thin warning banner at top of HUD: "Performance reduced — survivability adjustments active." Player keeps playing. |
| §B-LEAN-2026-06-12-1 | SLIP_TWEEN_DURATION_S upper bound under MIN_ESCAPE_SLIPS=2 | Hold [0.10, 0.15]s per brief §6.1 — survivability arithmetic permits widening to ~0.225s but brief §6.1 binding is explicit. Rejustify upper bound on Player Fantasy "body before mind" + camera/shader coherence + thermal sustainability, NOT survivability margin. |
| §5.4 | Near-miss audio under slip cue priority | (i) Duck slip cue under near-miss swell — slip cue auto-attenuates -6dB / 50ms attack when near-miss swell is active. Both cues play. |
| §5.5 | Mono-front edge-absorb pan-absence as implicit signal | (i) Author center-pan slip variant for consistency — eliminates pan-absence-as-direction signal. |
| §5.6 | Rule 7 vs Public Interface contradiction | (i) Update Public Interface row text — acknowledge "may be fractional when SETTLED if entered via Rule 7 DEAD freeze." |
| §5.7 | LANE_WIDTH_M "400cm preserved at all tunings" prose | (ii) Rewrite the prose — span scales with LANE_WIDTH_M × 4 (3.40–6.00 m across safe range). |

**Implementation order**: §2.x point fixes first (lowest-risk; built momentum) — folded §2.1/§2.2/§2.3/§2.4/§2.5/§2.6/§2.7/§2.8/§2.9/§2.10 into one consolidated batch since the items overlapped heavily in the audio table, Public Interface table, Tuning Knobs, and AC block. Then §1.2 Shipping-Safety Enforcement Policy (structural but self-contained). Then §1.1 F-6 full re-specification (under §5.1 (a) Override). Then §1.3 Hardware Contract enforcement rewrite + B-LEAN-2026-06-12-1 fold-in (under §5.2 / §5.3 / §B-LEAN-tension decisions). Subagent strategy: stub-and-confirm (NOT spawn) per advisor — canonical UE 5.6+ framerate-floor mechanism + named min-spec device list both stubbed for Polish-phase TD + performance-analyst audit.

**Files modified by this pass**:

- `design/gdd/player-movement.md` — primary target. **Substantive growth: 853 → 1418 lines** (+ 565 lines). All changes summarized below by section.
- `design/gdd/reviews/player-movement-review-log.md` — this file — this entry appended.
- `production/session-state/active.md` — full R10a decision sheet + implementation-plan checkpoint + final completion update.
- `design/gdd/systems-index.md` — PM row updated to reflect R10a in-session revision applied + R10 awaiting fresh-context re-review (queued).

**Files NOT modified by this pass (intentional)**:

- `design/registry/entities.yaml` — no new shared constants. The Hardware Contract DT watchdog parameters (60-sample window, 18ms / 16.67ms thresholds, 3.0s hysteresis) are PM-internal implementation details. `audio_cue_ratio` is a PM Tuning Knob, NOT a cross-system shared constant. `MIN_ESCAPE_SLIPS = 2` was already bound 2026-06-11.
- `docs/architecture/platform-seam-interfaces.md` — no seam changes. R10a touches PM-internal contracts and forward contracts on Wave Spawner (which doesn't yet exist); no `IPlayerMovementProvider` Seam 12 modification required.
- `design/gdd/pull-wave-behavior.md` / `design/gdd/difficulty-phase-controller.md` — propagation is queued for `/propagate-design-change` post-R10-APPROVED, not in this pass.

**GDD changes by section**:

- **Status / header block** (lines ~1–12 — pre-R10a was 9 lines, now ~12 lines): Status bumped to R10a, last-updated 2026-06-14, new R10a binding decisions block listing all 12 decision IDs (8 §5 + B-LEAN + §1.2 + §1.3 + §2.1/§2.2 bundle + §2.6 delegate bodies).
- **Player Fantasy** (lines ~16–28): unchanged in body; R7-PM-PROPAGATION-REVIEW M=3 reference at line ~28 remains as historical record (still factually relevant — M=3 PEAK barrages still exist in the dispatch pool, just suppressed when watchdog breaches).
- **Player-Perceivable State** (lines ~30–90): NEW Hardware-Performance Banner subsection under §1.3 + §5.3 (a). NEW Audio-Visual Ownership Split row for the banner.
- **Detailed Rules** (lines ~95–125): line ~95 Rule 1 prose qualified ("400 cm at default tuning, coincidentally matching prior 3-lane span; total span scales with LANE_WIDTH_M × 4 per R10a §2.4 correction").
- **Public Interface** (lines ~127–158): `lateral_world_position` row updated for §2.3 Rule 7 fractional-when-SETTLED carve-out + §2.7 Option (c) track-space semantics. `lean_angle` / `head_lean_angle` / `arm_lean_angle` rows updated for F-5/F-6 co-write contract per §1.1. NEW rows: `is_hw_performance_degraded` + `OnHardwarePerformanceBreach` multicast delegate (§1.3).
- **Cross-Component Interfaces** (lines ~160–540): NEW Delegate Handler Bodies subsection (§2.6 — `HandleStateChanged` + `HandlePausedChanged` full pseudo-code dispatch tables). Hardware Contract subsection fully rewritten per §1.3 (frame-quantized verification regenerated at MIN_ESCAPE_SLIPS=2, 60 fps lock re-derived on non-survivability grounds, DT watchdog spec with rolling window + hysteresis, stub-and-confirm UE 5.6+ engine mechanism + min-spec device list, cross-system forward contract on Wave Spawner). NEW Shipping-Safety Enforcement Policy subsection (§1.2 — enforcement table + 5 new BLOCKING ACs AC-SS-A through AC-SS-E + AC-21 semantic fold-in retiring one-shot `bFloorGuardFired` flag in favor of persistent F-2 clamp).
- **Formulas** (lines ~545–725): F-5 prose adds F-5 / F-6 co-write contract callout. F-6 fully re-specified per §1.1 — triple-output spec (body / head / arm via staggered EdgeAbsorbCurve sampling using `HEAD_LAG_PROGRESS` + `ARM_LEAD_PROGRESS` offsets), explicit per-state co-write table, §5.1 (a) Override timer-collision behavior, terminal-state reset semantics table, worked examples for path (1) F-4 mid-tween and path (2) Rule 1 SETTLED. Boundary Value Summary F-6 row updated for triple-output.
- **Authored Asset Contracts** (lines ~727–775): NEW EDGE_ABSORB_CURVE_ASSET section with required shape table + BeginPlay verification.
- **Edge Cases** (lines ~777–810): EC-15 / EC-14 prose unchanged in body — already references F-6 "additive" semantic which still holds; the §1.1 fix is structural (F-6 now writes all three angles) which is captured in F-6 + Public Interface, not in EC bodies.
- **Tuning Knobs** (lines ~857–878): `LANE_WIDTH_M` row safe-range floor justification rewritten on perceptual readability grounds (§2.5). `SLIP_TWEEN_DURATION_S` row fully rewritten on Player Fantasy + camera/shader/thermal grounds — pre-R10a "above 0.15s violates the invariant immediately" claim RETRACTED. NEW knobs: `EDGE_ABSORB_CURVE_ASSET` (§1.1) + `audio_cue_ratio` (§2.8). Tuning Knobs prose paragraph below table: LANE_WIDTH_M paragraph rewritten with safe-range span table (§2.4 + §2.5); SLIP_TWEEN_DURATION_S paragraph rewritten on non-survivability grounds (§B-LEAN-tension).
- **Visual/Audio Requirements** (lines ~885–945): Audio overlapping cues priority hierarchy rewritten with §2.9 + §5.4 ducking carve-out for near-miss. Slip cue row promoted to formal duration contract (`SLIP_TWEEN × audio_cue_ratio`). NEW center-pan slip variant cue row (§2.10 + §5.5). Edge-absorb cue row updated with pan-neutrality note.
- **Acceptance Criteria** (lines ~975–1395): pre-R10a AC count was 34 (AC-01 through AC-34). R10a additions:
  - AC-34 sample-set extended ({0.0, 0.1, 0.5, 0.99}) per §2.1.
  - AC-14 boundary samples (TweenProgress 0.001 / 0.999) per §2.1.
  - AC-26 / AC-30 F-6 tail exception folded in.
  - AC-21 semantic updated per §1.2 (persistent clamp; pre-R10a one-shot `bFloorGuardFired` RETIRED).
  - NEW AC-34b (buffer-flush execution tick per §2.1).
  - NEW Counter Persistence section: AC-COUNTER-DEAD / -COMPLETE / -PAUSE-RESUME per §2.2.
  - NEW F-6 section: AC-F6-A (head/arm alignment) / AC-F6-B (§5.1 Override boundary) / AC-F6-C (5-lane edge-absorb coverage) / AC-F6-D (DEAD-entry freeze pose) per §1.1.
  - NEW Audio Cue Contracts section: AC-AUDIO-CUE-PROPORTIONALITY / AC-AUDIO-CUE-DUCKING / AC-AUDIO-PAN-NEUTRALITY per §2.8 + §2.9 + §2.10.
  - NEW Hardware Contract Enforcement section: AC-HW-A (DT watchdog state machine) / AC-HW-B (min-spec device PEAK-density audit) / AC-HW-C (Banner + Wave Spawner gate integration) per §1.3.
  - NEW Shipping-Safety BLOCKING ACs (in §1.2 subsection): AC-SS-A through AC-SS-E.
  - **Post-R10a AC count: 34 → 53** (+ 19 new ACs across 4 new subsections + 4 substantive updates to existing ACs). Pending R10 fresh-context re-review to confirm count + AC ID consistency.

**Outcome — pre-R10a R9 + R10-lean BLOCKING items addressed**: all 22 R9 BLOCKING items + 1 R10-lean BLOCKING item are addressed in the GDD body. Not all are independently verified — the verification happens at R10 fresh-context re-review.

- **Cluster 1 (Pillar 5 / Hardware Contract / SLIP_TWEEN)**: closed via §1.3 Hardware Contract rewrite + B-LEAN fold-in + SLIP_TWEEN rejustification.
- **Cluster 2 (Shipping-Safety failures)**: closed as a class via §1.2 Shipping-Safety Enforcement Policy.
- **Cluster 3 (F-6 head/arm decouple + timer collision)**: closed via §1.1 F-6 full re-specification + §5.1 (a) Override.
- **Cluster 4 (AC-34 gaps + counter-reset assertions)**: closed via §2.1 sample extension + AC-34b + AC-14 boundary samples + §2.2 Counter Persistence section.
- **Cluster 5 (multi-item: Rule 7 contradiction, LANE_WIDTH prose, thumb-target, HandleStateChanged bodies, lateral_world_position Option (c) semantics, audio cue proportionality, near-miss audio under slip, mono pan-absence)**: all 8 items addressed (§2.3 / §2.4 / §2.5 / §2.6 / §2.7 / §2.8 / §2.9 / §2.10).
- **B-LEAN-2026-06-12-1 (Hardware Contract + SLIP_TWEEN prose stale against MIN_ESCAPE_SLIPS=2)**: closed via §1.3 fold-in items 1-4 (replace M=3 literal, regenerate frame-quantized table, re-derive 60 fps lock on non-survivability grounds, update SLIP_TWEEN row + prose). Item 5 (whether to widen [0.10, 0.15]s upper bound) deferred per §B-LEAN-tension = Hold per brief §6.1; widening is OUT-of-scope for R10a and requires creative-director adjudication if pursued.

**Senior Verdict**: N/A — this is an author revision pass, not a review. R10 fresh-context re-review will issue the verdict.

**In-session-override risk note (per R7 same-session-bias precedent)**: R10a is an in-session author revision pass driven by the pre-drafted brief; it does NOT carry the same risks as an in-session reviewer override of CD-binding "NOT in-session" recommendation (the Pull-Wave R8 in-session revision precedent). The CD's R9 ruling was "hand back to author with scoped revision brief" — R10a IS the scoped author revision per that exact ruling, executed in a fresh `/clear` session after the brief was drafted. The in-session-override-risk dimension that would apply to a reviewer override does NOT apply here.

**R10 fresh-context re-review pre-conditions** (NEXT step):
1. `/clear` → `/design-review design/gdd/player-movement.md` for R10 fresh-context.
2. R10 reviewer should weight: (a) F-5/F-6 co-write contract internal consistency (per-state matrix table vs. F-6 body pseudo-code vs. AC-F6-A through AC-F6-D math); (b) Hardware Contract DT watchdog state-machine correctness (60-sample rolling window + breach + hysteresis + flap-prevention); (c) §1.2 Shipping-Safety Enforcement Policy completeness (does every BLOCKING invariant added in this pass appear in the enforcement table?); (d) cross-system forward contract on Wave Spawner is documented in both Hardware Contract subsection and Cross-System Interface Table; (e) AC count + ID mechanical consistency (no duplicate IDs, no orphaned AC references); (f) all pre-R10a "M=3" / "violates invariant immediately" prose retracted or qualified.
3. R10 forecast (per brief §8): 0–4 BLOCKING. Main residual risk: F-6 timer-collision §5.1 (a) Override may surface downstream consumer interactions (camera framing, near-miss detection during the SLIPPING-after-F-6-override frame) that R10a author could not anticipate without specialist consultation. R10 reviewer may flag those for §1.1 extension.

**Files queued for downstream propagation (post-R10-APPROVED)**:
- `design/gdd/pull-wave-behavior.md` — Hardware Contract context inherits §5.2 (a) gate semantic. Pull-Wave R9 fresh-context re-review (already queued independently) may surface coordination work.
- `design/gdd/difficulty-phase-controller.md` — TELEGRAPH_WINDOW_FLOOR_S Tuning Knob context inherits the Hardware Contract.
- `design/gdd/wave-spawner-pattern-library.md` (NEW post-R10 addition — does not yet exist) — inherits the M=3 PEAK suppression gate as a BINDING forward contract on first authoring.
- Producer queues `/propagate-design-change` after R10 APPROVED.

**Prior verdicts resolved**: R9 verdict (MAJOR REVISION NEEDED, 22 BLOCKING) carries pending R10 fresh-context re-review to confirm closure. R10-lean B-LEAN-2026-06-12-1 BLOCKING addressed via §1.3 fold-in.

---

## Review — 2026-06-15 — Verdict: MAJOR REVISION NEEDED (R10, full, fresh-context post-R10a author revision pass)

Scope signal: XL
Specialists: game-designer, systems-designer, qa-lead, gameplay-programmer, audio-director, ux-designer, performance-analyst, creative-director (senior synthesis)
Blocking items: 19 (in 7 clusters) | Recommended: 23 | Nice-to-have: 5

**Phase 2 / 2b**: 8/8 required sections present ✓. Phase 2b seam-doc grep clean: `IPlayerMovementProvider` (Seam 12) + `IHapticDispatch` (ADR-0002) both authored with matching pinned ordinals. AC count mechanically verified at **53** by qa-lead (matches R10a author claim of 34→53; +19 new ACs across AC-F6-A/B/C/D, AC-COUNTER-DEAD/COMPLETE/PAUSE-RESUME, AC-AUDIO-CUE-PROPORTIONALITY/DUCKING/PAN-NEUTRALITY, AC-HW-A/B/C, AC-SS-A/B/C/D/E; +4 substantive updates). Dependency graph: `input-system.md` ✓, `run-state-machine.md` ✓, `pull-wave-behavior.md` ✓ (R9 queued). Downstream `collision-system.md` / `camera-system.md` / `death-replay.md` / `wave-spawner-pattern-library.md` / HUD GDD all acknowledged "not yet authored" — forward-contract pattern per R9 CD precedent (not blockers).

**Summary**: The R10a brief executed substantially (closed 22 R9 + 1 R10-lean BLOCKING; added 19 ACs; restructured F-6 / §1.2 / §1.3) but the R10a forecast (0–4 BLOCKING) was off by ~4-5x. R10 returns **19 BLOCKING in 7 failure clusters** — mostly NEW failure-class items the R10a brief did not scope (audio identity, UX safe-areas, watchdog correctness, anti-snap coherence) plus 3 triple-specialist-convergence items in Cluster A (text-survival from pre-R10a) that the brief targeted but missed. Convergence trajectory: 17→32→28→23→21→3→0→4→26→22+1→**19 (R10)**.

**Seven failure clusters** (BLOCKING tally in parentheses):

1. **Cluster A — Internal Contradiction (3 items, triple-specialist convergence: systems-designer/qa-lead/gameplay-programmer)**: F-2 pseudo-code (lines 618-622) + AC-21 body (line 1244) still hold pre-R10a one-shot `bFloorGuardFired` reset semantic despite §1.2 enforcement table + AC-SS-A explicitly retiring it. AC-SS-A test setup (`SLIP_TWEEN=0.20`) is unimplementable against current F-2 (only checks `<0.001`, never catches ceiling violations). Three independently inconsistent descriptions for the same runtime path.

2. **Cluster B — Watchdog Specification Defects (3 items, performance-analyst BLOCKING + 2 RECOMMENDED echoes)**: (B.1) `TickDTRollingBuffer[60]` zero-init blinds watchdog for first ~60 frames during highest-hitch-exposure window (startup) — AC-HW-A passes while defect is live; (B.2) AC-HW-B math+units error — 17.67ms = 56.6 fps not 60 fps, 1% of 3600 frames = 36 hitches per 60s inconsistent with "sustained 60 fps" header; (B.3) In-flight M=3 PEAK during gate engagement window — Wave Spawner does NOT mid-flight-cancel, AC-HW-C unconditional survivability promise cannot be delivered, Pillar-5 honesty failure.

3. **Cluster C — Audio Acceptance & Identity (5 items, audio-director domain authority)**: (C.1) Playback-rate scaling at ratio 0.70 produces ~+6 semitone pitch shift → cue identity destroyed (audio-director domain authority over silent systems-designer/gameplay-programmer); (C.2) AC-AUDIO-CUE-DUCKING vacuous at defaults (slip 127.5ms ends before swell 200-300ms); (C.3) Triple-overlap (buffer-drop + slip + near-miss) undefined; (C.4) Center-pan slip variant schedule undefined — headphone players hear two cues for same event; (C.5) AC-AUDIO-PAN-NEUTRALITY statistically invalid (N=40, 95% CI [44%, 74%]).

4. **Cluster D — UX / Accessibility / Platform Certification (4 items)**: Banner copy is dev jargon (Pillar-5 violation); top-edge banner placement vs iOS notch/Dynamic Island + Android status bar (silent failure on 60%+ iOS fleet); commitment-tell flash cadence at SLIP_TWEEN=0.10s floor exceeds IEC PEAT 3/sec (App Store + Google Play cert risk); near-miss visual beat sub-perceptual for deaf-in-speaker-mode players (GDD line 42 acknowledges this class) + no haptic alternative.

5. **Cluster E — F-6 Implementation Pathway (2 items, gameplay-programmer)**: `HandleStateChanged` COMPLETE/ABORTED dispatch missing F-6 reset (terminal-state contract violation); `HandleSlipTransition` is referenced as home for §5.1 (a) Override (PRIMARY R10a §1.1 decision) but never declared — phantom method anchoring the load-bearing edit.

6. **Cluster F — Player Fantasy / Coherence (3 items, game-designer vision-class)**: (F.1) §5.1 (a) Override zeros F-6 contributions (~3.5°) in 1 frame, structurally identical to the head-snap discontinuity that LEAN_CURVE_ASSET contract was specifically authored to prevent — GDD simultaneously holds anti-snap principle as load-bearing AND violates it; (F.2) EC-15 worked math at TweenProgress=0.85 + edge_absorb_progress=0.10 produces head=13.5° → clamps to 12° while body settles, reading as "stuck head + settling body" (player-reachable artifact); (F.3) §5.2 (a) M=3 PEAK suppression creates content-opaque difficulty reduction — banner doesn't say WHAT changed, PB calibrated against wrong difficulty class.

7. **Cluster G — Pipeline / Tooling (1 item)**: AC-SS-E assumes a yaml→C++ header generator pipeline (`design/registry/entities.yaml` → PM compile unit) that is nowhere documented. Unimplementable without pipeline, meaningless with locally-defined constants.

**Specialist disagreements adjudicated by creative-director**:
- C.1 playback-rate pitch shift — audio-director BLOCKING vs systems-designer/gameplay-programmer silent. CD ruling: audio-director's domain authority controls timbral identity; the other specialists had no domain hook to detect cue-identity loss. BLOCKING confirmed.
- D.13 near-miss haptic — ux-designer BLOCKING (accessibility gap) vs GDD-documented no-haptic intent. CD ruling: design intent ≠ accessibility justification when the GDD itself acknowledges deaf-in-speaker-mode players (line 42). BLOCKING confirmed; author chooses among 3 resolution paths (enlarge visual beat, opt-in haptic, or Pillar-5-honest documentation of the inaudible-and-invisible cost).
- Cluster F vision-class findings vs implementability findings. CD ruling: vision findings are BLOCKING when they cause documented self-contradiction (F.1: anti-snap principle vs Override) or pillar violation (F.3: Pillar-5 invisible skill ceiling). Confirmed BLOCKING.

**CD Senior Verdict**: **MAJOR REVISION NEEDED — hand back to author via R11a scoped brief (R10→R11a pattern, NOT in-session revision).**

R7 in-session-bias risk applies HERE MORE THAN at R10a. R10a was authored under a scoped brief I issued after R9 with explicit decision-bindings; the brief's coverage model was incomplete (off by 4-5x), not wrong. A fresh in-session revision pass now, with 19 BLOCKING in active context and the author co-located, would recreate exactly the R7 failure mode. Cluster A is the canary: pre-R10a F-2 text survived three locations despite three sections of new text being written that explicitly contradicted it — the R10a author had every BLOCKING item in context and still missed it. That is the R7 failure mode in miniature.

**R11a brief structure (CD-prescribed)** — to be authored before R11a session opens — organized around the 7 clusters, not the 19 items, because within each cluster items share root cause and benefit from coordinated edits:
1. **§1 Cluster A — Internal Contradiction (PRIMARY)**: locate-and-replace operation on F-2 pseudo-code + AC-21 body + AC-SS-A setup to harmonize on persistent-clamp semantic.
2. **§2 Cluster E — F-6 Implementation Pathway (PRIMARY)**: declare `HandleSlipTransition` with signature + call sites; add F-6 reset to COMPLETE/ABORTED dispatch.
3. **§3 Cluster B — Watchdog**: buffer-init policy; AC-HW-B math/units correction; M=3 PEAK in-flight policy (author chooses grace window OR documented caveat OR survivability proof).
4. **§4 Cluster C — Audio**: playback-rate scaling redesign (variant assets per ratio band OR time-stretch); AC-AUDIO-CUE-DUCKING re-spec; triple-overlap priority; center-pan variant author commits; AC-AUDIO-PAN-NEUTRALITY statistical re-spec.
5. **§5 Cluster D — UX / Platform Cert**: banner copy rewrite to player vocabulary; banner placement to respect safe-areas; flash-cadence floor recompute against PEAT; near-miss perceptibility resolution (author chooses among 3 paths).
6. **§6 Cluster F — Player Fantasy / Coherence**: Override path redesign to honor anti-snap contract (decay over N frames OR documented exception clause); EC-15 head-clamp behavior; PEAK suppression transparency to player.
7. **§7 Cluster G — Pipeline**: document yaml→header generator pipeline as architecture seam OR reframe AC-SS-E.
8. **§8 R11 forecast**: **3-8 BLOCKING** (widened from R10a's 0-4 because R10a forecast was off 4-5x; new failure surface is more cross-cutting UX/audio/accessibility/cert). **Decomposition trigger**: if R11 returns >8 BLOCKING, CD recommends GDD decomposition rather than another revision round.

**Validation criteria — we'll know this verdict was right if**:
- R11 fresh-context re-review returns BLOCKING count within forecast band (3-8).
- Cluster A does not recur (the canary).
- No new domain surfaces appear (4th BLOCKING domain cluster would trigger decomposition recommendation).

**Pillar-5 honesty note (CD)**: R10a forecasting model was wrong (predicted 0-4, actual ~19). R11a forecast 3-8 reflects calibrated humility, not confidence.

**Files modified by this review pass**:
- `design/gdd/systems-index.md` — PM row updated with R10 verdict + 7-cluster summary + CD ruling + decomposition trigger.
- `design/gdd/reviews/player-movement-review-log.md` — this file — this entry appended.

**Files NOT modified by this pass**:
- `design/gdd/player-movement.md` — fresh-context review is read-only on the target GDD.
- R11a brief — not yet drafted; user-chosen path is "Stop — author revises via R11a scoped brief".
- `design/registry/entities.yaml` — no new constants.
- `docs/architecture/platform-seam-interfaces.md` — no seam changes from this review.

**NEXT (user-chosen 2026-06-15)**: Stop here. Author drafts R11a scoped brief at `design/gdd/reviews/player-movement-r11-author-brief-2026-06-15.md` organized around the 7 clusters. Then author executes R11a in a `/clear` session. Then `/clear` → fresh-session `/design-review design/gdd/player-movement.md` for R11 fresh-context re-review. Forecast 3-8 BLOCKING per §8.

**Prior verdict resolved**: R9 verdict (MAJOR REVISION NEEDED, 22 BLOCKING) — R10a in-session author revision pass closed 22+1 R9 items; R10 confirms closure of those items. **However, R10 surfaces 19 NEW BLOCKING items** mostly in failure classes the R10a brief did not scope (Clusters B-G new surface) plus 3 triple-convergence items in Cluster A the brief targeted but missed (text-survival of pre-R10a F-2 pseudo-code + AC-21 body). Net effect: R9 closed, R10 opens new revision cycle.

---

## R11a In-Session Author Revision Pass — 2026-06-16 (author revision applying R11a brief)

**Trigger**: Per active.md session-state plan, R11a is the second author revision pass on `design/gdd/player-movement.md` against the R10 scoped brief at `design/gdd/reviews/player-movement-r11-author-brief-2026-06-15.md` (drafted 2026-06-15 per R10 CD ruling). Pre-R11a GDD state: 1418 lines (post-R10a), 19 R10 BLOCKING items + 23 RECOMMENDED + 5 NICE-TO-HAVE all open.

**Execution summary**: 17 R11a header decisions appended (R11a-1 through R11a-17). 12 binding author decisions taken across 7 clusters (DR-F.1/F.2/F.3, DR-B.3, DR-D.1/D.2/D.3/D.4, DR-C.1/C.4, AC-AUDIO-PAN-NEUTRALITY retirement, DR-G.1). 3 documented brief deviations: R11a-1 (F-2 log idiom — log-on-counter-zero matches AC-21 + AC-SS-A tick-1/tick-601 assertions over brief's `++counter >= 600` form); R11a-2 (RSM gate property-read form matches Cross-System Interface Table vs brief's method-call form); R11a-13 (audio_cue_ratio range [0.89, 1.12] honors brief intent over literal [0.79, 1.26] which was math-inconsistent with brief's own ±2-semitone claim).

**GDD growth**: 1418 → 1823 lines (+405).

**All 7 clusters addressed**:
- §1 Cluster A (Internal Contradiction): F-2 persistent clamp + AC-21 rewrite + AC-SS-A cross-ref + Public Interface `bSlipTweenClampActive` row.
- §2 Cluster E (F-6 Implementation Pathway): HandleSlipTransition declared with full signature + body + 5 helper declarations; HandleStateChanged COMPLETE/ABORTED/COUNTDOWN/IDLE dispatch resets F-6 state.
- §3 Cluster B (Watchdog): BeginPlay sentinel pre-fill + AC-HW-A Setup G two-part + AC-HW-B three orthogonal pass criteria rewrite + 3.0 s grace window forward contract on Wave Spawner.
- §4 Cluster C (Audio): audio_cue_ratio safe range tightened to [0.89, 1.12] with default raised to 0.93; panned variant RETIRED; triple-overlap resolution table; AC-AUDIO-CUE-DUCKING rewritten with Setup A/B/C; AC-AUDIO-PAN-NEUTRALITY RETIRED.
- §5 Cluster D (UX/Cert): banner copy locked = "Performance mode — hardest barrage suppressed."; safe-area binding; commitment-tell ±0.80 peak + 200ms cooldown; opt-in near-miss haptic.
- §6 Cluster F (Player Fantasy): 2-frame fade-out on F-6 Override; EC15_F6_DECAY_COEFFICIENT=0.7 proportional cap; banner disclosure contract closed by R11a-9.
- §7 Cluster G (Pipeline): AC-SS-E reframed to test local C++ static_assert; OQ-7 filed for future yaml→C++ generator pipeline.

**Files modified by R11a pass**: `design/gdd/player-movement.md` (primary target), `production/session-state/active.md` (R11a checkpoint).
**Files NOT modified by R11a (intentional)**: `design/registry/entities.yaml`, `docs/architecture/platform-seam-interfaces.md`, `design/gdd/pull-wave-behavior.md`, `design/gdd/difficulty-phase-controller.md` (propagation queued for `/propagate-design-change` post-R11-APPROVED).

---

## Review — 2026-06-16 — Verdict: MAJOR REVISION NEEDED (R11, full, fresh-context post-R11a author revision) — DECOMPOSITION TRIGGER FIRED

**Scope signal**: XL
**Specialists**: game-designer, systems-designer, qa-lead, ux-designer, audio-director, performance-analyst, unreal-specialist, creative-director (senior synthesis)
**Blocking items**: **11 consolidated (naive 17 across 7 specialists)** | Recommended: 23 | Nice-to-have: ≥2

**Phase 2 / 2b**: 8/8 required sections present ✓. Phase 2b seam-doc grep: `IPlayerMovementProvider` (Seam 12) + `IHapticDispatch` (ADR-0002) both authored. `IGameSettings` referenced (R11a-12 line 93 `IsNearMissHapticEnabled()`) but NOT in seam doc — RECOMMENDED finding; PM hedges with "or equivalent settings-bridge interface" and forward-contracts to HUD/Accessibility Settings GDD (forward-contract pattern grandfathered per R9 CD precedent). `EHapticEvent::NearMiss` not in IS haptic vocabulary — explicit R11a-12 forward contract; not BLOCKING. Seam doc line 1772 says "post 6-lane revision" while PM is 5-lane — documentation drift on seam doc, not PM. Dependency graph: `input-system.md` ✓, `run-state-machine.md` ✓ (property-read form for `current_state`/`is_paused`/`resume_grace` verified matches R11a-2 documented deviation), `pull-wave-behavior.md` ✓ (R1 CD adjudication direct-read of `PM.current_lane` confirms PM Rule 4/7 source-lane semantic), `difficulty-phase-controller.md` ✓. Downstream `collision-system.md` / `camera-system.md` / `death-replay.md` / `wave-spawner-pattern-library.md` / HUD GDD all acknowledged "not yet authored" — forward-contract pattern per R9 CD precedent.

**Cluster A canary (R11a brief's primary structural test): CLEAN ✓** — All 11 `bFloorGuardFired` references in the GDD are explicit retirement markers (R11a-1 header, Public Interface row, §1.2 enforcement table, AC-21 fold-in, AC-SS-A, F-2 prologue cross-reference, Tuning Knobs prose). Zero pre-R10a "resets to 0.15" semantic survivors. F-2 prologue + AC-21 + AC-SS-A + §1.2 enforcement table four-site lock-step coherent. **R11a brief execution model validated.**

**Summary**: R11a executed substantially well — all 7 R10 clusters closed; 17 R11a header decisions + 12 binding author decisions landed; GDD grew 1418 → 1823 lines. The R11a brief's structural test (Cluster A non-recurrence canary) passed cleanly. Yet R11 surfaced **11 NEW BLOCKING items** in failure classes the R11a brief did not scope: F-6 implementation depth (4), cert/accessibility (2), audio precision (2), profiler measurement (1), QA infrastructure (1), Shipping-guard body completeness (1). Convergence trajectory: 17→32→28→23→21→3→0→4→26→22+1→19 (R10) →**11 (R11)**. **R11 forecast (3-8 BLOCKING per brief §8) EXCEEDED; >8 decomposition trigger CD-set at R10 has FIRED.**

**11 R11 BLOCKINGs (CD-consolidated from 17 naive across 7 specialists)**:

1. **B-F6-1 [game-designer]**: F-6 fade-out tick-1 multiplier=1.0 reintroduces additive-opposition that alt (c) Accumulate was explicitly rejected for. AC-F6-B itself documents "sum of rightward F-6 and leftward F-5 arm" at tick 1 (lines 1038-1072, 1591-1593). Brief's anti-snap rationale and game-designer's anti-additive critique are both correct against different criteria — fix needs to satisfy both.
2. **B-F6-2 [systems-designer]**: EC15_F6_DECAY_COEFFICIENT=0.7 derivation anchored on TweenProgress=0.85 (line 1021) but worst case is TP=0.30-0.65 where F-5 head is at PEAK (LeanCurve plateau [0.20, 0.80]=1.0). At TP=0.30: scaled F-6 head 2.765° + F-5 head 10° = 12.765° → clamp engages. No coefficient in [0,1] satisfies the invariant at TP=0.30. Folds sub-finding: MAX_SLIP_DT_S=0.020 safe-range floor × 30fps breaks F-BARRAGE-SURVIVABILITY-INVARIANT (M=2 chain 0.733s > 0.65 FLOOR).
3. **B-F6-3 [systems-designer]**: F-6 references `effective_dt` (lines 1011-1012, 951) defined only in F-2 prologue which runs only while SLIPPING. SETTLED + F-6 active path (Rule 1 edge no-op) leaves `effective_dt` stale or zero-initialized → perpetual edge-absorb lean risk.
4. **B-F6-4 [unreal-specialist]**: F-6 references `EDGE_ABSORB_CURVE_ASSET` as **soft reference** (line 1293) but pseudo-code does raw `EdgeAbsorbCurve->GetFloatValue(...)` at lines 982/987/992. F-3 and F-5 have null guards; F-6 does not. Linear fallback promised at line 1181 but unimplemented. Shipping-build null deref crash on unloaded asset.
5. **B-BANNER-1 [ux + game-designer collapsed]**: Banner copy "Performance mode — hardest barrage suppressed." (R11a-9) framed as CLOSED while CONTINGENT on unauthored HUD GDD establishing "barrage" as player-facing vocabulary. Plus "suppressed" is dev jargon independent of "barrage" contingency. Disclosure contract cannot be simultaneously closed and conditionally open.
6. **B-CERT-1 [ux-designer]**: No player-controllable flash/motion-reduction setting (e.g., `commitment_tell_flash_enabled`). Photosensitive players cannot disable the flash. App Store cert risk; opt-in haptic precedent (DR-D.4) makes this gap structurally identical and unacceptable.
7. **B-CERT-2 [ux-designer]**: Haptic dispatch at line 93 (`IHapticDispatch::Fire(EHapticEvent::NearMiss)`) has no OS-state gate. iOS Focus modes + Android DND expected to suppress non-critical haptics per Apple HIG and Android accessibility guidance. App Store rejection risk.
8. **B-AUDIO-1 [audio-director]**: AC-AUDIO-CUE-DUCKING (line 1707) tests only one direction (near-miss triggered during active slip). Reverse case (slip dispatched during active near-miss swell — reachable gameplay) is unspecified. Worst-case compliant implementation produces silent slip cue = Pillar 5 violation.
9. **B-AUDIO-2 [audio-director]**: Triple-overlap "5 ms HARD-CUT ramp" (line 1402) specifies duration but not curve shape. Linear ramp on 400-1200Hz percussive whoosh produces click. Industry standard is raised-cosine; spec's own correctness claim depends on unspecified parameter.
10. **B-QA-1 [qa-lead collapsed]**: Two issues with same root cause: (i) AC-21 / AC-SS-A "tick 601" assertion unverifiable inside 10/100 tick test windows; (ii) Test strategy preamble (lines 1438-1449) lists only pre-R11a public properties — does not expose R11a-added F-6 fade-out members (4), shipping-safety flags, hardware watchdog state. Test apparatus cannot verify R11a invariants.
11. **B-PERF-1 [performance-analyst]**: AC-HW-B (line 1784) 0.5ms measurement tolerance is BELOW the named profiler tools' actual noise floor: Unreal Insights 0.3-1.2ms jitter, Android Profiler 0.5-1.5ms (Choreographer onset jitter), Apple Instruments ~1ms quantization. **The Polish-gate-BLOCKING AC is unmeasurable with its own apparatus.**
12. **B-SHIP-1 [unreal-specialist]**: AC-SS-C Shipping guard body unspecified. §1.2 policy (line 695-699) requires runtime guard body for every BLOCKING invariant. TickComponent pseudo-code (lines 603-641) shows only DT watchdog prologue — no `bManualTickEnabled` early-out, no 1Hz rate-limited log. Recreates the R9 "assertions-strip-in-Shipping" failure mode §1.2 was authored to close.

(CD numbered "11 BLOCKING" but the table contains 12 items — either count exceeds the >8 decomposition trigger.)

**Specialist disagreements adjudicated by creative-director**:
1. **F-6 fade-out intent (game-designer vs brief)**: game-designer claims tick-1 multiplier=1.0 reintroduces the additive-opposition (c) Accumulate was rejected for; brief's stated rationale was anti-*snap*, not anti-*additive*. CD ruling: **both correct against different criteria.** Fix must satisfy both anti-snap (smooth ramp) AND anti-additive (no first-frame opposition spike). B-F6-1 stands BLOCKING.
2. **unreal-specialist B3 (naming)**: unreal-specialist self-noted that RSM GDD uses snake_case `is_paused` / `current_state` consistent with PM's expectation. Downgraded to RECOMMENDED (project-wide convention drift vs `bIsXxx` UE idiom in technical-preferences.md, not PM defect).
3. **audio-director R3 (wrong Pillar)**: Audio Locked Decision cites Pillar 2 when it should cite Pillar 5 (or 1). Confirmed RECOMMENDED. Author should sweep all "Per Pillar X" citations.
No active cross-specialist contradictions.

**CD Senior Verdict (2026-06-16)**: **MAJOR REVISION NEEDED — DECOMPOSITION RECOMMENDED.**

Per CD R10 ruling: ">8 BLOCKING from R11 = recommend GDD decomposition rather than another revision round." Condition met. CD honors the threshold as committed at R10.

**Forecast model status: BROKEN.** R10a forecast 0-4 / actual 19. R11 forecast 3-8 / actual ~11-17 (consolidated to 11). Two consecutive 2-4× misses is not noise. Per Pillar 5 commitment-honesty, CD declares the forecast model broken and issues no third forecast. Future R*a→R* cycles on a doc this large should be forecast-free.

**Honest reading of the pattern**: GDD is at the wrong scope for the depth of detail being demanded. 1823 lines spanning mechanics + presentation + platform with 4-site lock-step canaries is past the coherence ceiling of a single-author doc. Each R*a pass exposes new surface as detail deepens — normal verification deepening that doesn't converge unless scope is bounded. Decomposition resolves this.

**Recommended split — three sub-GDDs along R11's empirical failure-domain seams**:
- **`player-movement-mechanics.md`** — F-1 through F-6, state machine, EC-15 boundary, invariant, mechanics test strategy. Owners: game-designer + systems-designer.
- **`player-movement-presentation.md`** — Banner copy, commitment-tell flash, audio cues + ducking, haptics, near-miss feedback. Owners: ux-designer + audio-director + narrative-director.
- **`player-movement-platform.md`** — Hardware tier gates AC-HW-A/B, thermal budgets, Shipping guards AC-SS-A through E, iOS/Android frame-pacing, OS Focus/DND integration, profiler measurement methodology. Owners: performance-analyst + unreal-specialist + qa-lead.

Each sub-GDD cleanly maps to one R11 cluster. Cross-sub-GDD contracts (e.g., F-6 fade-out timing → audio ramp timing) authored explicitly as forward contracts. Removes the cross-domain entanglement R11 keeps surfacing.

**R11a process assessment**: R11a executed substantially well. Cluster A canary CLEAN. All 7 R10 clusters closed. 17 R11a header decisions + 12 binding author decisions landed. Three documented brief deviations all rationalized. Three advisor-flagged in-session blocker fixes during R11a (R11a-9 vocabulary contingency surfaced; R11a stale 0.85 ratio in AC-COMMIT-FLASH-CADENCE Setup A updated to 0.93; R11a-15 stale slip duration range updated). **This is not an author-capability problem. It is a scope-vs-detail-depth problem.**

**R7 in-session-bias risk**: Does NOT reapply at R11 (fresh-context review; specialists spawned independently in fresh contexts; no in-session anchoring to prior verdict). CD self-consistency caveat: CD set the >8 threshold at R10 and is now ruling on whether it fires — honoring as committed. If user wants to override and try R12a, CD supports.

**User-chosen path (2026-06-16)**: **Decompose** (CD recommendation accepted). CD authors decomposition plan + cross-sub-GDD contract list at `design/gdd/reviews/player-movement-decomposition-plan-2026-06-16.md`. Author executes structural split in a later /clear session. Pull-Wave R9 + Telegraph prototype proceed in parallel.

**Files modified by this review pass**:
- `design/gdd/systems-index.md` (PM row updated with R11 verdict + decomposition trigger + 11 BLOCKING consolidation + CD decomposition recommendation)
- `design/gdd/reviews/player-movement-review-log.md` (this file — R11a entry + R11 entry appended)

**Files NOT modified by this pass**:
- `design/gdd/player-movement.md` (fresh-context review is read-only on the target GDD)
- `design/registry/entities.yaml` (no new constants)
- `docs/architecture/platform-seam-interfaces.md` (no seam changes; line 1772 "post 6-lane revision" documentation drift flagged as separate RECOMMENDED at seam-doc cleanup pass)

**Prior verdict resolved**: R10 verdict (MAJOR REVISION NEEDED, 19 BLOCKING in 7 clusters) — R11a in-session author revision pass closed all 7 R10 clusters; R11 confirms closure (Cluster A canary CLEAN; all 7 cluster surfaces addressed by R11a header decisions). **However, R11 surfaces 11 NEW BLOCKING items** in deeper-surface failure classes the R11a brief did not scope (F-6 implementation depth, accessibility cert, audio precision, profiler measurement methodology, QA infrastructure refresh, Shipping-guard body completeness). Net effect: R10 closed, R11 trips decomposition trigger.

---

> **⚠ END OF PRE-DECOMPOSITION LOG.** R11 above is the terminal entry — this log is CLOSED at decomposition 2026-06-16. Continuation review history lives in per-sub-GDD logs; see banner at top of this file for the 3 pointer paths.




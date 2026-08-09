# Pull-Wave Behavior — Review Log

Reviews of `design/gdd/pull-wave-behavior.md`. Each entry summarises a single `/design-review` invocation.

---

## Review — 2026-06-07 — Verdict: NEEDS REVISION (R1, first review)
Scope signal: L
Specialists: game-designer, systems-designer, level-designer, ai-programmer, performance-analyst, qa-lead, creative-director (synthesis)
Blocking items: 23 | Recommended: ~22 | Nice-to-have: ~17 | Specialist disagreements: **0** (first time in project history with 6+ specialists)
Same-session bias caveat: GDD was authored 2026-06-06 in the same session that spawned this review. Specialists ran in fresh contexts (no bias); main-session synthesis layer carried bias and was flagged.
Prior verdict resolved: First review.

Summary: 23 BLOCKING items concentrate into **7 root causes** per CD synthesis. This matches **DPC R6 (18 BLOCKING / 5 roots — concentration) NOT DPC R3 (21 BLOCKING / sprawl)**. Phase 2b seam-doc grep CLEAN PASS — the R7 design-review skill amendment did its job; none of the 23 are paper-only-seam class. Findings are concentrated on design coherence + second-order interactions, which is the higher-value level for a first review.

### Root cause clustering (per CD synthesis)

**RC-A — EC-NEARMISS-PREMIDPOINT fallback structurally wrong** (2 BLOCKING, strongest signal):
- game-designer B-1 + ai-programmer B-4 independently converged on the same one-line fix: replace inference branch with `slipped_from_lane = PM.current_lane` (PM already holds source lane during SLIPPING per PM Rule 4/7).
- Cascades to Rule 11 + EC text + Forward Contract item 4 (drop `TweenProgress = 0.5` lock per ai-prog B-5 — replace with "PM guarantees `current_lane` returns source-lane until SETTLED").

**RC-B — Lane-bleed geometric contradiction** (1 BLOCKING):
- level-designer B-1: Visual/Audio Section says voxel cluster is 1.5–2.0 units wide AND "no bleed into adjacent lanes" at LANE_WIDTH_M=1.0m. Mathematically incompatible.
- Independent of lane count — both lane choices have this problem.

**RC-C — Float determinism / boundary arithmetic precision** (4 BLOCKING):
- systems-designer B-2 (CRITICAL): `floor(0.6f / 0.15f) = 3` in IEEE 754 (not 4 as documented). F-SLIP-REACH "1-slip margin" claim collapses to ZERO. F-BARRAGE-SURVIVABILITY-INVARIANT margin is zero.
- systems-designer B-1: F-TRAJ-LATERAL `world_x` declared [-2.5, 2.5] but actual range at endpoint tolerance is [-2.525, 2.525].
- ai-programmer B-2: `FPullWaveSpawnParams` stores `TObjectPtr<UCurveFloat>` (mutable ref), not snapshot. Rule 14 determinism + Death Replay across asset edits diverges.
- ai-programmer B-3: Tick order across active waves unspecified — Rule 14 bit-identical claim collapses without `wave_id ASC` iteration order spec.

**RC-D — PEAK identity unbound at wave level** (2 BLOCKING, CD re-attributed):
- game-designer B-2 + B-4 merged: Phase-independent Rule 7 means PEAK waves could be mechanically identical to OPENER waves. **CD adjudication**: this is Wave Spawner's domain. Add forward contract on Wave Spawner GDD: "PEAK avg velocity ≥ 1.5× OPENER avg." Do NOT change Pull-Wave Rule 7.

**RC-E — Seam plumbing gaps post-Phase-2b** (6 BLOCKING):
- qa-lead B-1: `FPullWaveInstanceState` phantom struct name — only appears in AC-PW-22b grep requirement; never defined in GDD.
- qa-lead B-2: AC-PW-15 seam-bridge gap — Rule 13 step 1 doesn't bind a call through `IWaveSpawnerCallback::OnCollisionUnregistered`. Production calls Collision DIRECTLY; stub never observes ordering. (Ironic: qa-lead authored Seam 13 in this session, then their adversarial pass caught its own production-side bridge gap.)
- qa-lead B-3: AC-PW-23 negative branch unreachable — two outcomes promised, one GIVEN provided.
- qa-lead B-4: AC-PW-13 authoring-error sub-case mechanism unspecified — both cook-time AND runtime reject the scenario.
- ai-programmer B-1: LANDED hold self-contradiction — Rule 3 says "one-tick hold" but Tuning Knobs `WAVE_DESPAWN_HOLD_S=0.15s` (9 ticks at 60fps).
- performance-analyst B1: AC-PW-22a scope too narrow — measures TRAVERSING-only. Real PEAK frame mix exceeds 0.25ms budget.

**RC-F — Lane count anchoring weakness** (1 BLOCKING + 1 merged):
- game-designer B-3 + level-designer R-4 + systems-designer B-3 merged: 6-lane choice has no justification over 5 lanes. **CD adjudication**: lane count → **5 lanes** (CD recommended; user ratified). C(5,3)=10 configs ≥ 4 ✓, mobile-UX wins (lane width ~14mm at 6-inch screen vs ~11.7mm at 6 lanes — above thumb-target floor).
- Plus systems-designer B-3 PM enum mismatch — no AC blocks story-Done on PM enum widening; add AC-PW-LANE-ENUM-CONSISTENCY.

**RC-G — Mobile rendering budget gaps** (3 BLOCKING):
- performance-analyst B2: Object pool sizing unspecified. Fix: explicit formula `cap + ceil(cap × max_despawn_hold × spawn_rate)`.
- performance-analyst B3: Per-instance material scalars (`LeanChargeIntensity`, `NearMissEdgeFlash`, `VoxelDissolveFade`) break ISM batching. 128-224 draw calls without `PerInstanceCustomData`.
- performance-analyst B4: 16 translucent trail ribbons on mobile tile-based GPU = bandwidth/thermal concern.

### Re-attributed (CD downgraded from BLOCKING)

- game-designer B-5 ("RSM Rule 19 from UNWRITTEN GDD"): CD verified RSM is Approved and Rule 19 IS authored at RSM line 113. Downgraded to **RECOMMENDED** wording clarification.

### Cross-cluster convergence patterns (specialists independently surfaced same root)

- EC-NEARMISS-PREMIDPOINT fix (game B-1 + ai-prog B-4) — both converge on `PM.current_lane`
- 5-lane vs 6-lane (game B-3 + level R-4)
- Per-barrage velocity (game R-3 + level R-3 + ai-prog R-3)
- Float determinism cross-platform (systems R-4 + ai-prog R-1 + qa-lead R-6)
- Synchronous broadcast budget (systems R-3 + perf B1)
- 6-lane lane-bleed × axis ambiguity (level B-1 + B-2)

### CD adjudications (binding)

1. **RC-A — EC-NEARMISS-PREMIDPOINT**: ADOPT `PM.current_lane` direct read. Cascades to Rule 11 + EC text + Forward Contract item 4 (drop TweenProgress=0.5 lock).
2. **RC-F — Lane count**: 6 → **5 lanes** (user ratified). Cascades to Pull-Wave Rule 2, entity registry NUM_LANES=5, AC-PILLAR-2-BARRAGE-SPATIAL-K K=10, PM revision scope.
3. **RC-D — PEAK identity**: Wave Spawner forward contract `PEAK avg velocity ≥ 1.5× OPENER avg`, NOT Pull-Wave fix. Rule 7 phase-independence preserved.

### R2 Forecast

**8–12 BLOCKERS at R2** if all 7 root causes land cleanly.

Assumptions: all 7 roots fixed; RC-A applies simple `PM.current_lane` fix (not re-derivation); RC-B fixed via cluster width tightening OR LANE_WIDTH_M widening with mobile re-verify; lane count decided once (no re-iteration); same 6 specialists fresh contexts at R2; no new architectural surfaces.

Anchor: DPC pattern R3=21 → R4=16 (≈25% drop). Pull-Wave should outperform (50% drop, 23 → ~11) because of root-cause concentration + Phase 2b already clean.

### Strategic recommendation

**Revise OUT-OF-SESSION + run Telegraph prototype + PM revision in parallel** (CD recommended; user ratified).

Parallel workstreams:
- **WS1 (Pull-Wave R2 prep, out-of-session)**: Fresh session, address 7 roots, request R2.
- **WS2 (PM revision, out-of-session)**: `/propagate-design-change` for PM 3→5 lane widening + tween range tighten + `current_lane`-during-SLIPPING semantic documentation.
- **WS3 (Telegraph prototype, can begin now)**: Prototype `lean_duration_s` readability at 0.6s floor on 6-inch screen. Highest-risk bet per systems-index; doesn't depend on Pull-Wave R2 verdict.

### Forward-imposed contracts on downstream GDDs

- **Player Movement (BLOCKING for Pull-Wave impl)**: 5-lane (revised from 6) + SLIP_TWEEN_DURATION_S range tighten + OnSlipMidpoint signature/timing preserved + `current_lane` returns source-lane during SLIPPING (per ai-prog B-4 R1 finding).
- **Wave Spawner (R1 NEW per RC-D)**: PEAK avg velocity ≥ 1.5× OPENER avg in per-phase pattern pools.
- **Wave Spawner (R1 NEW per qa-lead B-2)**: Pull-Wave production must invoke `IWaveSpawnerCallback::OnCollisionUnregistered(wave_id)` in Rule 13 step 1 OR Seam 13 production class must wrap the Collision direct-call with self-notification.

### Test infrastructure (R1 verified)

- Seam 7 IRSMTimeStateProvider ✓ (verified by Phase 2b grep)
- Seam 11 ICurveProvider ✓
- Seam 12 IPlayerMovementProvider + FPlayerMovementTestStub ✓ (all referenced fields declared)
- Seam 13 IWaveSpawnerCallback + FWaveSpawnerCallbackTestStub ✓ (all fields declared; production-side bridge gap is a separate finding)

### Editorial defects flagged (non-blocking)

- None flagged this round (Pull-Wave benefits from DPC's 7-round editorial decisions; no carryover).

### Separate flags for producer (not GDD revision items)

- Pull-Wave is the project's 5th MVP system; 8 downstream systems depend on it. R2 convergence cost matters for sprint planning.
- PM revision (WS2) is load-bearing for Pull-Wave R2 to evaluate against Approved PM. If WS2 stalls, R2 will block.
- Telegraph prototype (WS3) is independent — recommend starting immediately to parallelize convergence.
- Same-session bias caveat: future review-skill amendment may consider enforcing a `/clear` boundary between /design-system and /design-review on the same GDD.

### Convergence trajectory note

This is the project's first first-review with 6 specialists and zero disagreements. The R7 design-review skill amendment (Phase 2b seam-doc grep) is verified working — none of the 23 BLOCKING items are paper-only-seam class. Convergence concentration matches DPC R6 not DPC R3, suggesting the project's review process has matured significantly through the DPC 7-round path. Pull-Wave should benefit from these learnings throughout its convergence.

---

## In-Session Revision Pass — 2026-06-07 — Verdict: R1 BLOCKERS RESOLVED (pending R2)
Scope signal: L
Specialists: (none — in-session author + advisor; R2 will spawn full panel)
Blocking items addressed: 17 distinct fixes covering all 23 R1 BLOCKING items across 7 root causes | Recommended deferred to R2
Summary: User chose option [A] — revise in-session — after `/clear` recovery surfaced that R1 GDD was unrevised since the R1 review log was written. All 7 R1 root causes (RC-A through RC-G) addressed via cascading edits. GDD grew 675 → 739 lines. AC count 38 → 39 (1 deleted-placeholder + 2 new). PM forward contract revised; new Wave Spawner forward contract authored; new Implementation Resource Budgets sub-section. Same-session bias caveat from R1 still applies to this revision pass; R2 must be run in a fresh session against clean context.
Prior verdict resolved: R1 NEEDS REVISION (23 BLOCKING) → resolved in-session pending R2 verification.

### Root causes resolved

- **RC-A — EC-NEARMISS-PREMIDPOINT fallback structurally wrong**: Rule 11 rewritten to direct `PM.current_lane` read at LANDED entry (PM Rules 4 + 7 guarantee source-lane semantic throughout SLIPPING). EC-NEARMISS-PREMIDPOINT deleted. AC-PW-23 retained as DELETED placeholder. AC-PW-22 rewritten to test direct-read mechanism (no FireSlipMidpoint invocation). PM Forward Contract items 3 + 4 collapsed into single `current_lane` source-semantic guarantee. Dependencies + Bidirectional + OQ-PW-1 updated. PM author notified that `OnSlipMidpoint` is no longer required by Pull-Wave (PM may keep or drop at its discretion).
- **RC-B — Lane-bleed geometric contradiction**: HITBOX vs VISUAL SILHOUETTE separation introduced. Hitbox exactly `LANE_WIDTH_M`, never bleeds. Silhouette core ≤ LANE_WIDTH_M, sparse trailing voxels ≤ 0.25m bleed acknowledged. Pillar 5 binding preserved (gameplay-safe-lane semantic).
- **RC-C — Float determinism / boundary arithmetic precision (4 items)**: (C-1) F-SLIP-REACH deleted entirely — multiplicative F-BARRAGE-SURVIVABILITY-INVARIANT is float-stable and is the sole binding form. (C-2) F-TRAJ-LATERAL `world_x` range corrected to `[-2.02, 2.02]m` (5-lane × 1.0m × CURVE_ENDPOINT_TOLERANCE). (C-3) `FPullWaveCurveSnapshot` immutable per-instance struct introduced; `FPullWaveSpawnParams` carries snapshot value, not `TObjectPtr<UCurveFloat>` reference. (C-4) Rule 14 (b) binds active-wave list iteration to `wave_id ASC`, monotonic non-reusable ids. AC-PW-10 strengthened.
- **RC-D — PEAK identity unbound**: Wave Spawner forward contract introduced — PEAK pool avg `ForwardVelocityMs` ≥ 1.5 × OPENER pool avg. NEW AC-PW-WAVE-SPAWNER-PEAK-VELOCITY-FLOOR. Pull-Wave Rule 7 phase-independence preserved (CD-bound).
- **RC-E — Seam plumbing gaps (6 items)**: (E-1) `FPullWaveInstanceState` fully defined in Data Structures block. (E-2) Rule 13 step 1 binds production to call BOTH `Collision.UnregisterWave` AND `IWaveSpawnerCallback::OnCollisionUnregistered` in sequence; AC-PW-15 strengthened. (E-3) AC-PW-23 deletion (covered under RC-A). (E-4) AC-PW-13 stripped to threshold-cross only; authoring-error sub-case lives in cook-time AC-PW-30. (E-5) Rule 3 LANDED row corrected (state persists for `WAVE_DESPAWN_HOLD_S` ≈ 9 ticks). (E-6) AC-PW-22a broadened to mixed-state PEAK frame (2 LEANING + 10 TRAVERSING + 3 LANDED + 1 DESPAWNING).
- **RC-F — Lane count 6 → 5**: NUM_LANES = 5 propagated throughout (header, Rule 2, Rule 4, formulas, examples, ACs, lean magnitude tiers 3-4-lane, knob interactions). PM forward contract item 1 = 5-lane. AC-PW-PILLAR-2-BARRAGE-SPATIAL-K → C(5,3)=10. NEW AC-PW-LANE-ENUM-CONSISTENCY. NUM_LANES non-tunable entry locked to 5 with mobile-thumb-target rationale.
- **RC-G — Mobile rendering budget gaps (3 items)**: New Implementation Resource Budgets sub-section in Tuning Knobs binds (G-1) object pool sizing formula `pool_size ≥ MAX_CONCURRENT_WAVES_CAP + ceil(cap × hold_s × spawn_rate) + safety` (provisional default 24); (G-2) all 4 per-instance material scalars (LeanChargeIntensity, NearMissEdgeFlash, VoxelDissolveFade, TrailAlpha) routed through `PerInstanceCustomData[0..3]` on the wave-mass ISMC — MaterialInstanceDynamic forbidden; (G-3) translucent ribbon trail REPLACED with alpha-tested cutout voxel-ghost meshes (3 cubes at half-unit intervals) — aesthetic shift flagged for art-director sign-off; NEW OQ-PW-7 tracks the validation gate at prototype.

### New surface introduced (R2 specialists should scrutinize)

- `FPullWaveCurveSnapshot` struct — sample count, evaluation semantics, AC-PW-22b 256-byte size budget implications.
- `FPullWaveInstanceState` struct — field set, alignment, total size; SAMPLE_COUNT may need to shrink to fit budget.
- `OnCollisionUnregistered` callback binding in Rule 13 step 1 — production implementation choice (Pull-Wave invokes both, or Collision invokes the callback before returning).
- Alpha-tested cutout voxel-ghost trail — read as "approaching motion" vs "static after-image" is unverified; OQ-PW-7 tracks.
- Mixed-state AC-PW-22a frame mix — distribution may need refinement against real PEAK frame stats from prototype profiling.
- Wave Spawner forward contracts proliferating: PEAK velocity floor + wave_id monotonic + curve snapshot at admission + OnCollisionUnregistered observability. Risk of contract over-specification when Wave Spawner GDD is eventually authored.

### Same-session bias caveat

This revision pass was authored in the same session as the R1 review log was read. The author of this revision pass IS the agent that read the R1 verdict and applied the fixes; no independent specialist re-evaluated the fixes. R2 MUST be run in a fresh session against clean context, ideally with the same 6+ specialist panel as R1 (game-designer, systems-designer, level-designer, ai-programmer, performance-analyst, qa-lead, creative-director).

### R2 forecast

**4–8 BLOCKERS at R2** (revised from R1's 8–12 forecast). Reasoning: all 7 R1 roots addressed cleanly + new structures introduce moderate surface area for second-order findings. Risk areas: FPullWaveInstanceState size budget vs SAMPLE_COUNT (sys-design likely); cutout-trail aesthetic readability (art/level-designer); Wave Spawner forward contract overcommit (level-designer / game-designer); OnCollisionUnregistered implementation ambiguity (ai-programmer / qa-lead); mixed-state AC-PW-22a frame distribution (perf-analyst).

### Files modified this pass

- `design/gdd/pull-wave-behavior.md` (675 → 739 lines; 35+ edits)
- `design/gdd/systems-index.md` (Pull-Wave row + header + tracker checkboxes)
- `design/gdd/reviews/pull-wave-behavior-review-log.md` (this entry)
- `production/session-state/active.md` (R2-prep checkpoint)

### Files NOT modified this pass (deliberately deferred)

- `design/registry/entities.yaml` (NUM_LANES, C(N,3) value — to be updated at R2 acceptance or in Phase 5)
- `docs/architecture/change-impact-2026-06-06-player-movement.md` (6-lane-based, stale; will be replaced by fresh report from `/propagate-design-change`)
- `docs/architecture/platform-seam-interfaces.md` Seam 12 reverse map note (cosmetic — points to FireSlipMidpoint for AC-PW-22; can be updated at R2 acceptance)
- `design/gdd/player-movement.md` (PM GDD revision deferred to `/propagate-design-change` per skill flow)

---

## Review — 2026-06-07 — Verdict: MAJOR REVISION NEEDED (R2, fresh-context)
Scope signal: L (matches R1)
Specialists: game-designer, systems-designer, level-designer, ai-programmer, qa-lead, art-director, creative-director (synthesis). **performance-analyst output LOST** (~89K tokens, 22 tool uses, no structured findings returned — advisor-preseeded perf concerns surfaced as UNRESOLVED).
Blocking items: 18 | Recommended: 19 | Nice-to-have: 9 | Specialist disagreements: 3 (all CD-adjudicated)
Phase 2b seam-doc grep: CLEAN PASS (all 4 interfaces — IRSMTimeStateProvider, ICurveProvider, IPlayerMovementProvider, IWaveSpawnerCallback — and all referenced stub fields present in docs/architecture/platform-seam-interfaces.md).
Prior verdict resolved: R1 NEEDS REVISION (23 BLOCKING / 7 roots) → in-session revision pass. **Only 2 of 18 R2 blockers are pure-introductions from R1 revision** (SAMPLE_COUNT=64 default; Rule 13 "two valid implementations" + Seam 13 production no-op claim). **16 were R1-missed** despite R1 claiming all 7 roots addressed — empirical confirmation of same-session bias caveat R1 reviewers warned about.

Summary: R2 surfaces 18 BLOCKING / 6 root causes (vs R1 forecast 4–8). Trajectory pattern matches DPC R6 (concentrated, not sprawled) in cluster count but exceeds it in discipline breadth — every reviewing discipline (6/6) found BLOCKINGs vs DPC R6's 5 roots clustered in 3 disciplines. Three roots (A: time-source, D: contract incoherence, E: reaction budget) are foundational — they break ACs and downstream authoring before any visual or perf polish matters. One root (C: lean magnitude collapse) is a Pillar 2 fantasy-promise breach.

### Root cause clustering (per CD synthesis)

**Cluster A — Time-Source Incoherence** (3 BLOCKING):
- F-TRAJ-TNORM uses wall-clock `(GameTime - SpawnTimeS - LeanDurationS) / TravelDurationS` while `FPullWaveInstanceState.TraverseElapsedS` accumulator is unused — two time models in one struct.
- AC-PW-12's `t_norm == 0.0` at TRAVERSING entry unreachable. EC-STALE-PAUSE-ABORTED teleports wave (pause duration included in numerator). Frame hitches teleport ≤90% of trajectory in one tick.
- Convergence: systems-designer B-1 + ai-programmer B-T10 independently identified same wall-clock-vs-accumulator contradiction.
- Fix: Rewrite F-TRAJ-TNORM as `clamp(TraverseElapsedS / TravelDurationS, 0, 1)` per tick during TRAVERSING.

**Cluster B — Compile-Time Self-Contradiction (Curve Snapshot)** (2 BLOCKING, 4-specialist convergence):
- `SAMPLE_COUNT = 64` (line 128) → struct ~316 bytes vs BLOCKING `static_assert(sizeof ≤ 256)`. "Tune at prototype" deferral incoherent for compile-time constant.
- `FPullWaveCurveSnapshot.EvaluateAt(1.0)` reads `Samples[SAMPLE_COUNT]` — OOB at routine input.
- Convergence: game-designer B-1 + systems-designer B-2 + ai-programmer B-T1 + B-T4 — 4 specialists.
- Fix: Lock SAMPLE_COUNT=32 (interpolation error ~0.001, 5× under CURVE_ENDPOINT_TOLERANCE=0.005 per systems-designer math); remove SAMPLE_COUNT=16 option (unsafe margin); add endpoint clamp; correct line 152 scalar count (15 fields × 4 = 60 bytes, not 16 × 4 = 64).

**Cluster C — Lean Magnitude Readability Collapse** (2 BLOCKING + 1 partial dissent, Pillar 2 breach):
- 3-lane and 4-lane shifts both map to tier-3 = 28° lean. Player at lane 0 seeing right-lean cannot disambiguate target lane 3 (lane 4 safe) from target lane 4 (lane 3 safe) — 50/50 lottery with zero pre-slip information.
- Player Fantasy line 18 promises destination-from-lean. Broken.
- Specialist disagreement: game-designer B-2 + level-designer B-2 call BLOCKING (Pillar 2 violation); art-director Finding-3 offers softer take (Pillar 5 preserved because target-lane matters not delta).
- **CD adjudication: BLOCKING.** game-designer + level-designer correct. Art-director's defense applies to a fantasy this game doesn't claim. 4-tier lean (17°/22°/25°/28°) is the path; alternatives require pillar-touching change requiring CD-PILLARS re-spawn, not approved at R2.

**Cluster D — Production/Seam Contract Incoherence** (3+ BLOCKING):
- GDD line 70 says production `OnCollisionUnregistered` is no-op; Seam 13 production class (lines 1498-1502) calls `Collision->UnregisterWave` → with Rule 13 step 1's direct call, `UnregisterWave` fires TWICE → AC-PW-15 mechanically impossible.
- GDD line 77 "two valid implementations" framing: convergence ai-programmer B-T2 + qa-lead B-1 agree this must collapse to ONE canonical.
- Seam 12 "Why this seam exists" (line 1308) and AC table (1437-1441) still describe Pull-Wave subscribing to OnSlipMidpoint — removed R1 RC-A. Test authors following seam doc will write wrong test. R1 flagged as "cosmetic — defer to R2"; at R2 acceptance it is a hard gate.
- **CD adjudication: collapse to ONE canonical pattern.** Optionality in a contract is a defect, not flexibility.

**Cluster E — Survivability Invariant Omits Reaction Budget + Lane Semantic Ambiguity** (2 BLOCKING):
- F-BARRAGE-SURVIVABILITY-INVARIANT counts only slip locomotion. Residual 0.15s budget must absorb acquisition + parse + decision + initiation on 6-inch mobile. Applied with realistic ~0.20s mobile recognition floor: `0.45 ≤ 0.60 - 0.20 = 0.40` → FAILS. Worst-case adjacent M=3 `{0,1,2}` may be structurally unescapable.
- `MIN_BARRAGE_LANE_SEPARATION` "distinct lane indices" semantically ambiguous — AC-PW-PILLAR-2-BARRAGE-SPATIAL-K's C(5,3)=10 assumes target-lane; OQ-PW-5 convergence assumes source-lane. Wave Spawner authoring blocked.
- **CD adjudication: lock semantic to target-lane distinct.** Rewrite invariant as `SLIP_TWEEN × M + REACTION_BUDGET ≤ TELEGRAPH_FLOOR` with REACTION_BUDGET = 0.20s provisional.

**Cluster F — Test Infrastructure Missing + Visual Contract Self-Contradictions** (4 BLOCKING):
- `FRSMTestStub.SetIsPaused()` is a field setter with no `OnPausedChanged` delegate — AC-PW-16/17/25/26 cannot drive pause-flush logic.
- `FWaveSpawnerCallbackTestStub` has no `OnWaveHit`/`OnNearMiss` capture — 6 ACs (AC-PW-21a/b/c, 25, 26, 28) assert event counts with no observation infrastructure.
- RC-G G-3 BINDING (line 527: "cutout… NOT translucent… not optional") directly contradicted by OQ-PW-7 fallback (b) (line 739: "re-evaluate selective translucency for closest 4 waves"). BINDING + escape-hatch = defect.
- `LeanChargeIntensity` brightness ramp not bound to ease curve → at lean_progress=0.4, rotation barely 4.2° but brightness already +32% → punch-before-windup (Pillar 2 ordering inversion).
- **CD adjudication on OQ-PW-7 fallback (b): STRIP.** Selective translucency would create perceptual inconsistency between identical waves at different distances — itself a Pillar 5 violation.

### Cross-cluster CD adjudications (binding)

1. **Cluster C — Lean tier**: 4-tier lean (1-lane=17°, 2-lane=22°, 3-lane=25°, 4-lane=28°). Alternatives require Player Fantasy revision; not approved at R2.
2. **Cluster D — Rule 13 implementations**: collapse to ONE canonical pattern. Either production class is genuinely no-op + direct call from despawn driver, OR callback IS the unregister + direct call removed. Pick.
3. **Cluster E — Lane semantic**: `MIN_BARRAGE_LANE_SEPARATION` = target-lane distinct. OQ-PW-5 convergence must adapt.
4. **Cluster F — OQ-PW-7 fallback (b)**: STRIP. Only fallbacks (a) animate per-instance offsets and (c) revisit ribbon with aggressive overdraw budget retained.
5. **OQ-PW-2 mixed-velocity barrages**: CLOSE AT PULL-WAVE LAYER (not defer to Wave Spawner). Convergence game-designer R-3 + level-designer R-1: mixed velocity invalidates F-BARRAGE-SURVIVABILITY-INVARIANT timing model.

### Cross-cluster convergence patterns (specialists independently surfaced same root)

- SAMPLE_COUNT=64 vs static_assert ≤256: game-designer + systems-designer + ai-programmer (3-way) + qa-lead (implicit)
- F-TRAJ-TNORM wall-clock teleport: systems-designer + ai-programmer (2-way, distinct angles — frame hitch vs EC-STALE-PAUSE-ABORTED)
- Rule 13 production/Seam 13 contradiction: ai-programmer + qa-lead (2-way)
- 3-4 lane lean tier collapse: game-designer + level-designer (2-way BLOCKING; art-director softer take)
- 2-frame edge flash inadequacy: game-designer + level-designer + art-director (3-way)
- OQ-PW-2 close at Pull-Wave layer: game-designer + level-designer (2-way)
- 1.5× PEAK velocity ratio unanchored: game-designer + systems-designer (2-way)

### Unresolved (performance-analyst gap)

Performance-analyst specialist exhausted ~89K tokens + 22 tool uses without returning structured findings. Advisor-preseeded concerns that remain unverified:
- AC-PW-22a mixed-state distribution (2 LEANING / 10 TRAVERSING / 3 LANDED / 1 DESPAWNING) under-counts LEANING vs ~27% steady-state proportion at default tuning
- Pool sizing burst-rate (3 spawns/0.3s = 10/s instantaneous) vs MAX_SPAWN_RATE_PER_S=1.0 sustained-rate assumption (covered partially by systems-designer Finding-5)
- HISM vs ISMC ambiguity in G-2 (HISM hierarchical culling overhead at 16 instances)
- G-3 "zero overdraw cost" rhetorical overreach (fragments still shaded before discard in mobile tile-GPU)
- AC-PW-22a PNG profiler output has no CI auto-pass criterion

Recommend re-spawning performance-analyst with fresh context after Cluster A (time-source rewrite) lands — perf budget may shift materially.

### Editorial defects flagged (non-blocking)

- Line 152 scalar count comment incorrect (16 × 4 = 64 actually 15 × 4 = 60 bytes)
- AC summary count claim "39 = 30 BLOCKING + 9 ADVISORY" does not enumerate (qa-lead R-1)
- `lateral_offset` declared range (line 258) `[-4.0, 4.0]` should be `[-4.02, 4.02]` for tolerance consistency
- PM forward-contract cites "PM Rule 4 line 84" — line numbers drift across PM revision
- Seam 12 stub retains dead code post-R1 RC-A (FireSlipMidpoint, OnSlipMidpointDelegate field)
- `WaveId` type mismatch: FPullWaveSpawnParams uint32 vs IWaveSpawnerCallback int32 (qa-lead R-2)

### R3 forecast

**3–6 BLOCKERS at R3** (revised from R2's 4-8 forecast based on observed convergence pattern).

Assumptions: all 6 roots fixed atomically in fresh sessions; cross-cluster interactions (e.g., OQ-PW-5 + tier collapse, mixed-velocity barrage + survivability invariant) addressed during their cluster sessions; performance-analyst re-spawned after Cluster A; same 7-specialist panel at R3 in fresh contexts; no new architectural surfaces.

Risk areas: emergent issues from Cluster F's seam stub extensions (new test surfaces); Cluster E's reaction budget tuning may interact with Cluster C's 4-tier lean readability; OQ-PW-7 fallback rewrite may surface art-director additional perceptual concerns at prototype.

### Strategic recommendation

**Atomic-by-cluster revision in fresh sessions per root cause** (DPC R5 model; user ratified).

Reasoning: R1's in-session revision missed 16 of 18 R2 blockers — empirically the in-session model under-detects on this GDD. The 6 roots are largely independent (Time-Source ≠ Lean Tier; Survivability Invariant ≠ Visual Contracts). Parallel sessions without merge conflicts. Atomic-by-cluster lets each cluster get fresh-context specialist re-check before bundling — the model that converged DPC.

Concrete sequence (parallelizable):
1. **Cluster A (Time-Source)** — systems-designer + ai-programmer fresh session; rewrite F-TRAJ-TNORM + EC-STALE-PAUSE-ABORTED.
2. **Cluster B (Sample Curve)** — systems-designer fresh session; lock SAMPLE_COUNT=32; EvaluateAt boundary clamp; kill SAMPLE_COUNT=16 option.
3. **Cluster C (Lean Tier)** — game-designer leads fresh session; 4-tier lean spec. Touches Player Fantasy — must echo to Telegraph GDD when authored.
4. **Cluster D (Production/Seam Contracts)** — ai-programmer + qa-lead fresh session; collapse Rule 13 to one canonical; fix double-fire; refresh Seam 12 OnSlipMidpoint removal.
5. **Cluster E (Survivability Invariant)** — level-designer + game-designer fresh session; rewrite invariant with reaction budget; lock MIN_BARRAGE_LANE_SEPARATION target-lane semantic.
6. **Cluster F (Test Infra + Visual Contracts)** — qa-lead + art-director fresh session; extend stubs (OnPausedChanged delegate, OnWaveHit/OnNearMiss capture + ordering log); strip OQ-PW-7 fallback (b); bind LeanChargeIntensity to ease curve.
7. **Re-spawn performance-analyst** after Cluster A lands (perf budget may shift materially with TraverseElapsedS rewrite).
8. **R3 fresh-context design-review** to validate convergence.

**Rejected paths:**
- In-session revision: R1 demonstrated same-session bias undetects.
- Single bundled fresh-session pass: bundles too much; correlates misses across clusters.
- Accept-and-proceed-to-Telegraph: rejected. Cluster C is fantasy breach; Cluster D breaks ACs; cannot ship.
- Stop Pull-Wave entirely: roots are tractable; momentum is real; 8 downstream systems waiting.

### Forward-imposed contracts on downstream GDDs (R2 additions to R1 set)

- **Player Movement** (R1 contracts unchanged; R2 adds nothing new for PM since `current_lane` source-lane semantic from R1 RC-A remains the binding interface)
- **Wave Spawner** (R2 NEW): OQ-PW-2 mixed-velocity barrage policy must reflect Pull-Wave's binding (uniform velocity within barrage OR per-onset velocity bounded such that all M land within BARRAGE_SIMULTANEITY_WINDOW_S)
- **Wave Spawner** (R2 NEW): OPENER barrage admissibility (level-designer R-2) — Pull-Wave must declare whether OPENER patterns admit `is_barrage = true`
- **Wave Spawner** (R2 NEW): `MIN_BARRAGE_LANE_SEPARATION = 1` semantic = target-lane distinct (binding per CD adjudication)
- **Telegraph System** (R2 NEW): LEAN_ANGLE_MAX_DEG world-space spec authored in Pull-Wave Visual section — art-director R-6 flags structural authoring overlap; Telegraph must either accept world-space or own screen-space verification
- **Death Replay** (R2 NEW): `wave_id` verbatim replay forward contract — `FPullWaveSpawnParams` must capture WaveId; replay must inject explicit ID rather than regenerate (ai-programmer R-T3)
- **Camera System** (R2 NEW per art-director Finding-7): FOV + depth setup required before Pull-Wave's screen-space readability claims (lean angle visibility, trail depth, near-miss flash perceptibility) are verifiable

### Test infrastructure additions required (Cluster F)

- **Seam 7 IRSMTimeStateProvider**: add `OnPausedChanged` multicast delegate (virtual accessor); FRSMTestStub.SetIsPaused must broadcast on value change OR provide FirePausedChanged(bool) helper
- **Seam 13 IWaveSpawnerCallback**: add `OnWaveHit(wave_id, target_lane, source_lane, spawn_time)` and `OnNearMiss(wave_id, target_lane, slipped_from_lane)` virtual methods + FWaveSpawnerCallbackTestStub counters + ordered event log (AC-PW-10 ordering invariant requires log not just count)
- **Seam 12 IPlayerMovementProvider**: update "Why this seam exists" prose + AC table to remove OnSlipMidpoint references for Pull-Wave consumers; map AC-PW-22 to direct `SetMovementState/SetCurrentLane/SetTargetLane` injection
- **Type unification**: `WaveId` type across `FPullWaveSpawnParams` (uint32) and `IWaveSpawnerCallback` signatures (int32) — pick one

### Separate flags for producer

- Pull-Wave is project's 5th MVP system; 8 downstream systems depend on it. R3 convergence cost matters for sprint planning. Atomic-by-cluster estimated 3–5 working days.
- Performance-analyst output loss is a tooling gap, not a content gap — re-spawn after Cluster A lands.
- Same-session bias caveat now empirically confirmed (16 of 18 R2 blockers were R1-missed despite R1's "all 7 roots addressed" claim). Consider design-review skill amendment: enforce `/clear` boundary between /design-system and /design-review on same GDD.
- PM revision (via `/propagate-design-change`) remains load-bearing for Pull-Wave R3 evaluation against Approved-revised PM. If PM revision stalls, R3 will block on AC-PW-LANE-ENUM-CONSISTENCY and AC-PW-SLIP-TWEEN-CONSISTENCY.

### Convergence trajectory

R1 = 23 BLOCKING / 7 roots (sprawl-then-revised in-session) → R2 = 18 BLOCKING / 6 roots (concentrated; breadth concern — 6/6 disciplines hit). Pattern matches DPC R6 in cluster count, exceeds in discipline breadth. Atomic-by-cluster fix path should converge R3 to 3–6 BLOCKING range (the forecast R2 originally promised). The "trajectory healthy in shape, unhealthy in process" diagnosis (CD) applies: roots are tractable, but in-session revision masked structural defects.

### Files NOT modified this review (deliberately deferred to cluster sessions)

- `design/gdd/pull-wave-behavior.md` (revisions belong in cluster sessions, not review)
- `docs/architecture/platform-seam-interfaces.md` (Seam 7/12/13 amendments are Cluster F deliverables)
- `design/registry/entities.yaml` (REACTION_BUDGET = 0.20s provisional from Cluster E)
- `design/gdd/systems-index.md` (will be updated in Phase 5 post-this-review)

---

## Cluster A Revision Pass — 2026-06-08 — Verdict: CLUSTER A RESOLVED (pending R3)
Scope signal: M (focused single-cluster pass; affects 9 sections in the GDD)
Specialists: systems-designer + ai-programmer (parallel fresh-context subagents) + creative-director adjudication via session author (Option C + tighten EC user-ratified)
Blocking items addressed: 3 R2 BLOCKING items resolved (F-TRAJ-TNORM wall-clock / accumulator contradiction; AC-PW-12 reachability; EC-STALE-PAUSE-ABORTED teleport semantic)
Frame-hitch teleport context: still possible under any time-source model when DeltaTime is uncapped; the accumulator model uniquely fixes the PAUSE case (not non-pause hitches, which remain Rule 9 threshold-cross's responsibility per EC-LANDING-OVERSHOOT-FORWARD).

Summary: First atomic-by-cluster pass per R2 strategic recommendation. Both specialists converged on the accumulator-as-canonical rewrite. Two editorial decisions resolved by user: (D-1) F-TRAJ-FORWARD form → **Option C** (simplified lerp `world_z = SpawnPlaneZ − t_norm × SPAWN_PLANE_Z_OFFSET_M`); (D-2) EC-STALE-PAUSE-ABORTED wording → **tighten** to explicit no-teleport semantic. 9 sections edited; GDD 739 → 763 lines.

### Edits applied

1. **Rule 8** (line 48) — dropped inline wall-clock derivation; reference F-TRAJ-TNORM by symbol; explicitly name TraverseElapsedS as canonical time-source.
2. **FPullWaveInstanceState struct field comments** (lines 145–146 pre-edit) — both LeanProgress and TraverseElapsedS gained explicit init/freeze/clamp/canonical-source semantics. Both fields now describe the pause-freeze invariant symmetrically.
3. **F-TRAJ-TNORM** (lines 205–236 pre-edit) — full replacement. Formula = `clamp(TraverseElapsedS / TravelDurationS, 0.0, 1.0)`. New sub-sections: tick-order contract (eval-before-increment) for AC-PW-12 reachability; determinism note (identical inputs → identical outputs, not bit-equivalence under variable frame timing); pause correctness note. Variable table reduced to 3 vars (was 5). Boundary check table rewritten with structural impossibility framing.
4. **F-TRAJ-LATERAL** — no change (uses t_norm symbol, no wall-clock references).
5. **F-TRAJ-FORWARD** (lines 279–311 pre-edit) — Option C: `world_z = SpawnPlaneZ − t_norm × SPAWN_PLANE_Z_OFFSET_M`. Algebraically equivalent via F-TRAVERSE-DURATION identity. Removed TravelDurationS + ForwardVelocityMs from this section's variable table (they stay in F-TRAVERSE-DURATION). New "LANDED hold correctness" sub-section. `t_norm = 1.0 → world_z = 0.0` cancels exactly.
6. **F-TRAVERSE-DURATION** — no change.
7. **EC-STALE-PAUSE-ABORTED** (line 408 pre-edit) — tightened from vague "accumulators resume" to explicit no-teleport: names both accumulators, asserts pause duration NOT included, asserts trajectory continuity preserved.
8. **AC-PW-12** (line 659 pre-edit) — uses observable-focused wording ("first observable t_norm reading equals 0.0 within 1e-9"); asserts both TraverseElapsedS and t_norm.
9. **AC-PW-16** (line 663 pre-edit) — four invariants: (a) TraverseElapsedS underlying, (b) t_norm derived, (c) world_z, (d) state. Both (a) and (b) must be asserted independently.
10. **AC-PW-17** (line 664 pre-edit) — split into 17a (LEANING-state wave) + 17b (TRAVERSING-state wave). 17b is the primary regression guard: arithmetic assertion `|TraverseElapsedS_observed − (TraverseElapsedS_pre_pause + N × DeltaTime)| ≤ 1e-9`.
11. **AC-PW-22b** (lines 713–718 pre-edit) — added 5th grep pattern: (a) FORBID `GameTime.*SpawnTimeS` in trajectory source (regression guard); (b) ALLOW `TraverseElapsedS +=` (accumulator increment exists). Combined FORBID + ALLOW is stronger than either alone.

### Advisor corrections applied post-initial-edits

After the 11 initial edits, advisor surfaced two spec defects (both verified, both fixed in-pass):

- **AC-PW-17b float tolerance**: initial draft used `1e-9`, but `TraverseElapsedS` is declared `float` (single precision) in FPullWaveInstanceState. Single-precision epsilon at 1–4s values is ~1e-7, so `Σ DeltaTime` rounding error would exceed `1e-9` and spuriously fail correct implementations. Fix: changed to `1e-4` with explicit rationale (teleport bug produces ms-scale difference, comfortably above `1e-4`). Verified against struct line 150.
- **AC-PW-22b FORBID grep**: initial draft `GameTime.*SpawnTimeS` matched on any line containing both names, including legitimate logging like `UE_LOG(TEXT("SpawnTimeS=%f GameTime=%f"))`. Fix: scoped to subtraction expressions via `[^"]*-[^"]*` between the names, excludes string-literal occurrences.

Lower-priority advisor flag (not corrected; surfaced here for future review): the LeanProgress field-comment rewrite introduced an explicit increment-rate clause ("Advances by `DeltaTime / LeanDurationS`") that was not part of the R2 Cluster A finding. The rate is correct (standard LEANING accumulator pattern; matches Rule 5's "lasts exactly `lean_duration_s`"), but Cluster C (lean tier) will revisit LeanProgress semantics — that session should sanity-check this clause against any tier-curve work it introduces.

### Cluster F handoff list (seam concerns surfaced, NOT fixed here)

These are second-order issues from the Cluster A rewrite that require test-infrastructure changes (Cluster F scope). Each must be addressed when Cluster F session opens — they are prerequisites for AC-PW-12 / 16 / 17 to be mechanically testable:

- **CF-A1**: `TraverseElapsedS` must be readable as test observable on the wave instance. Existing seams (Seam 7 IRSMTimeStateProvider, Seam 12 IPlayerMovementProvider) do NOT expose Pull-Wave instance state. A new seam (Seam 14 or extension of an existing seam) is required. Without it, AC-PW-12 / 16 / 17b can only assert derived outputs (t_norm, world_z), not the underlying accumulator invariant they're designed to validate.
- **CF-A2**: Pull-Wave tick harness must accept injected `DeltaTime` for AC-PW-16's 20-tick assertion and AC-PW-17b's N-tick arithmetic check. If existing harness uses real clock, these ACs are nondeterministic.
- **CF-A3**: `FRSMTestStub` must support `SetCurrentState(ABORTED)` with `is_paused` transitioning to false on the same tick WITHOUT firing `RSM.OnPausedChanged`. Required for AC-PW-17a/b to drive the no-teleport assertion. If stub does not currently support this atomic transition, AC-PW-17 has no valid test path.
- **CF-A4**: Cross-reference from AC-PW-26 to AC-PW-17b (low priority, editorial).

### Cross-cluster dependencies surfaced

- **Death Replay** (post-MVP, not yet authored): under wall-clock, replay used SpawnTimeS + reconstructed GameTime; under accumulator, replay must either capture DeltaTime sequences or re-simulate at fixed step. Flag when Death Replay GDD is authored. Not Cluster A scope.

### R3 forecast

Cluster A alone closes 3 of 18 R2 BLOCKING items. R3 cannot run until Clusters B–F are addressed (each in fresh sessions per the atomic-by-cluster strategy). If all 6 clusters land cleanly, R3 forecast is 3–6 BLOCKING per the R2 review's own projection — unchanged by this pass.

### Next cluster recommendation

Per R2 sequence: Cluster B (SAMPLE_COUNT=64 vs static_assert ≤256 + EvaluateAt OOB). Cluster B is independent of Cluster A's edits — can be opened in a fresh session immediately. Performance-analyst should re-spawn after Cluster A AND Cluster B both land (perf budget may shift materially from both: Cluster A's accumulator increment changes hot-loop math; Cluster B's SAMPLE_COUNT change affects per-wave memory footprint).

### Files modified this pass

- `design/gdd/pull-wave-behavior.md` (739 → 763 lines; 11 edits across 9 sections)
- `design/gdd/reviews/pull-wave-behavior-review-log.md` (this entry)
- `production/session-state/active.md` (Cluster A checkpoint + Cluster B as next)

### Files NOT modified this pass (deliberately deferred)

- `docs/architecture/platform-seam-interfaces.md` (Seam 14 / extension for CF-A1 belongs to Cluster F)
- `design/gdd/systems-index.md` (will be updated once all clusters land and R3 passes)
- `design/registry/entities.yaml` (no Cluster A constants to register)

---

## Clusters B+C+D+E+F Parallel Revision Pass — 2026-06-08 — Verdict: ALL R2 CLUSTERS RESOLVED (pending R3)

Specialists: systems-designer (Cluster B), game-designer (Cluster C), ai-programmer (Cluster D), level-designer (Cluster E), qa-lead (Cluster F) — all spawned in parallel as fresh-context subagents returning structured edit proposals. Main session applied serially to avoid Edit-tool race-clobber on shared files. Performance-analyst NOT re-spawned this pass (deferred per active.md — should run after Cluster A + this pass land so perf budget can be re-assessed against both accumulator hot-loop and SAMPLE_COUNT changes).

Blocking items addressed: all 15 remaining R2 BLOCKING items (3 already closed by Cluster A) across 5 root cause clusters.

Summary: User selected "Parallel" execution model deviating from the R2-recommended atomic-by-cluster-in-fresh-sessions but preserving the fresh-context property via subagent isolation. Three integrator decisions ratified mid-apply: (1) Cluster E BOQ resolved via path (b) — TELEGRAPH_WINDOW_FLOOR_S 0.6 → 0.65s; (2) Cluster F CF-A1 resolved via path (A) — test accessor on FPullWaveSubsystem (not a new injectable seam); (3) Cluster D FireSlipMidpoint stripped from stub (no current consumer). Advisor flagged that Cluster E's proposed new_strings were drafted pre-BOQ; remediated in-flight by rewriting bodies to commit to path (b) before applying each Edit. Cross-cluster touch points (C↔F lean visual section overlap; D↔F seam-doc table adjacency) verified disjoint after apply.

GDD 763 → 825 lines (+62). Seam doc ~1666 → 1769 lines (+103). Entity registry ~471 → 614 lines (+143).

### Cluster B — Sample Curve (RESOLVED, 4 edits)

- SAMPLE_COUNT 64 → 32 (locked); interpolation-error math 5× under CURVE_ENDPOINT_TOLERANCE
- EvaluateAt boundary-clamp documented (`i1 = min(i0+1, SAMPLE_COUNT-1)`) preventing Samples[SAMPLE_COUNT] OOB at TNormClamped=1.0
- Struct size comment corrected (`16 × 4 → 15 × 4 = 60 bytes`); total 188 bytes (68 byte headroom)
- F-TRAJ-LATERAL guard updated `SAMPLE_COUNT > 1 → == 32`
- SAMPLE_COUNT=16 option ruled out (66% of tolerance, unsafe margin)

### Cluster C — Lean Tier (RESOLVED, 4 edits)

- 4-tier lean mapping `17°/22°/25°/28°` replaces broken 3-tier collapse (Pillar 2 destination-from-lean restored)
- `lean_magnitude_tier` data contract widened `{1,2,3} → {0,1,2,3,4}` (tier 0 = straight-forward wave; minor implicit → explicit extension)
- 4 new `LEAN_ANGLE_TIER{1,2,3,4}_DEG` feel knobs added to Tuning Knobs + entity registry
- Telegraph GDD forward contract authored: must consume tier mapping as authoritative angular source + AC asserting ±0.1° + tier-3 vs tier-4 (3° gap) perceptibility prototype validation
- `LEAN_ANGLE_MAX_DEG` retired from GDD + registry

### Cluster D — Production/Seam Contract β (RESOLVED, 10 edits — 3 GDD + 7 seam doc)

- Pattern β chosen: production `OnCollisionUnregistered` forwards to `Collision->UnregisterWave`; despawn driver makes ONE call, holds no Collision reference
- Rule 13 step 1 collapsed; "two valid implementations" framing retired
- AC-PW-15 ordering rewritten (steps i/ii/iii — single callback fire)
- Seam 12 OnSlipMidpoint cleanup: stripped FOnSlipMidpoint declaration, GetOnSlipMidpointDelegate from interface + production override + stub override + Reset comment + delegate field
- FireSlipMidpoint stub method STRIPPED (user decision: no current consumer; review log line 250 editorial defect closed)
- Seam 12 AC table + global reverse-map AC-PW-22 row updated to direct-read semantics
- Seam 13 production class UNCHANGED (already correct under β)

### Cluster E — Survivability Invariant (RESOLVED, 14 edits — 11 from proposal + 3 cross-cluster extras)

- F-BARRAGE-SURVIVABILITY-INVARIANT rewritten: `SLIP_TWEEN × M + REACTION_BUDGET ≤ TELEGRAPH_FLOOR`
- **Path (b) committed** throughout: TELEGRAPH_WINDOW_FLOOR_S `0.6 → 0.65s` (Rule 5 line 42 extra; PM forward contract line 186 extra; Telegraph dependency row line 476 extra; Variables table; Tuning Knobs row; boundary check primary row); zero margin at default tuning (0.15×3+0.20=0.65≤0.65)
- `MIN_BARRAGE_LANE_SEPARATION` semantic locked **target-lane distinct** (Rule 2; Tuning Knobs; AC-PW-PILLAR-2-BARRAGE-SPATIAL-K)
- REACTION_BUDGET decomposition added (acquisition 0.07s + parse/decision 0.08s + motor initiation 0.05s)
- Tuning Knobs: REACTION_BUDGET row added with safe range [0.20, 0.25]s and rationale
- EC-TELEGRAPH-FLOOR-LOWERED-BELOW-0.45S → EC-TELEGRAPH-FLOOR-LOWERED-BELOW-SURVIVABILITY-THRESHOLD (path (b) threshold 0.65s)
- EC-INTER-SLIP-GATE-INTRODUCTION re-derived (zero gate budget at default tween under path (b); 0.10s margin under path (a)+(b) combined)
- AC-PW-32 re-derived under new invariant
- AC-PW-SLIP-TWEEN-CONSISTENCY path-conditional, commits to `[0.10, 0.15]s` range preserved
- AC summary updated to log R2 Cluster E revision (AC count unchanged)
- entities.yaml REACTION_BUDGET entry + TELEGRAPH_WINDOW_FLOOR_S value + MIN_BARRAGE_LANE_SEPARATION/NUM_LANES/LANE_WIDTH_M cleanup

### Cluster F — Test Infra + Visual Contracts (RESOLVED, 14 edits — 5 seam doc + 9 GDD; Edit 6 confirmed no-change)

- FRSMTestStub extended (Seam 7): `OnPausedChanged(bool)` multicast delegate with CF-A3 semantics (fires only on RUNNING state + value-change); `SetCurrentStateWithPauseClear(ERunState)` atomic helper (CF-A3: ABORTED + is_paused→false same-tick WITHOUT OnPausedChanged fire)
- FWaveSpawnerCallbackTestStub extended (Seam 13): `HandleWaveHit(WaveId, TargetLane, SourceLane)` + `HandleNearMiss(WaveId, TargetLane, SlippedFromLane)` delegate-listener methods; `GetWaveHitCount()` + `GetNearMissCount()` counters; `GetEventLogInOrder()` unified ordered event log (AC-PW-10 ordering)
- Master AC-Unblocked tables in seam doc updated for AC-PW-16/17/21a/b/c/10
- GDD LeanChargeIntensity bound to `EaseCurve(lean_progress)` (Pillar 2 windup→punch ordering fix); phase verification table; LEAN_BRIGHTNESS_PEAK_RATIO=1.8 inlined as constant; brightness peak uniform across tiers
- OQ-PW-7 fallback (b) STRIPPED (Pillar 5 violation per CD adjudication); (a) + (c) retained with clarified rationale
- Test Infrastructure Dependencies updated: Seam 7 R2 ext + Seam 13 R2 ext + Pull-Wave instance test accessor (CF-A1 path A: `#if !UE_BUILD_SHIPPING` test accessor `GetTraverseElapsedS_ForTest(WaveId)` on `FPullWaveSubsystem`) + DeltaTime injection contract (CF-A2: harness must accept injected delta)
- AC-PW-16 strengthened (OnPausedChanged drive, injected delta, GetTraverseElapsedS_ForTest, Seam 13 hit/near-miss counts)
- AC-PW-17a strengthened (SetCurrentStateWithPauseClear + injected delta + RunTermination check)
- AC-PW-17b strengthened (full no-teleport via accessor + Seam 13 observation + AC-PW-26 cross-ref)
- AC-PW-21a/b/c strengthened (GetWaveHitCount/GetNearMissCount via delegate binding)
- AC-PW-25 strengthened (OnPausedChanged drive + Seam 13 forced-despawn no-event)
- AC-PW-26 strengthened (CF-A4 cross-reference to AC-PW-17b + Seam 7/13 dependency note)
- AC-PW-28 strengthened (WaveHitCount==2 + GetEventLogInOrder ordering)
- Edit 6 NO CHANGE confirmed: line 547 RC-G G-3 BINDING text was already correct

### entities.yaml cleanup (7 edits)

- `last_updated 2026-06-06 → 2026-06-08` + R2 cluster notes
- `TELEGRAPH_WINDOW_FLOOR_S` value `0.6 → 0.65` + extensive note rewrite (path (b) rationale + DPC propagation pending)
- `BARRAGE_SIMULTANEITY_WINDOW_S` notes flagged for DPC re-evaluation at 0.325s (value 0.3 left in place until DPC author confirms)
- `MIN_BARRAGE_LANE_SEPARATION` notes: target-lane distinct semantic + NUM_LANES=5 + C(5,3)=10
- `NUM_LANES` value `6 → 5` + R1 CD catch-up notes (registry was stale 2 days; corrected)
- `LANE_WIDTH_M` notes corrected `6 lanes × 1.0m → 5 lanes × 1.0m`
- `LEAN_ANGLE_MAX_DEG` RETIRED with header comment; 4 new entries `LEAN_ANGLE_TIER{1,2,3,4}_DEG`; 1 new entry `REACTION_BUDGET`

### Forward contracts newly imposed on downstream GDDs

- **DPC** (R2 Cluster E path (b) → forward propagation; NOT edited this pass): TELEGRAPH_WINDOW_FLOOR_S 0.6 → 0.65s; re-validate AC-PILLAR-2-CONCURRENT; re-evaluate BARRAGE_SIMULTANEITY_WINDOW_S = 0.325s. Propagation via `/propagate-design-change` after R3 lands.
- **RSM** (R2 Cluster F forward contract; joins R5 ForceTickNow primitive as pre-implementation requirement): expose `OnPausedChanged(bool)` multicast delegate with same semantics as FRSMTestStub (fires only on RUNNING + value-change; PAUSED→ABORTED atomic does NOT fire).
- **Wave Spawner** (R2 Cluster E + F): MIN_BARRAGE_LANE_SEPARATION = 1 enforces **target-lane** distinct; K ≥ 4 spatial enumeration is target-lane triplets; OnWaveHit + OnNearMiss multicast delegates on Pull-Wave subsystem (delegate-listener pattern, not IWaveSpawnerCallback virtuals).
- **Telegraph** (R2 Cluster C): consume 4-tier LEAN_ANGLE_TIER{1,2,3,4}_DEG as sole authoritative angular source; AC ±0.1° per tier; prototype tier-3/tier-4 (3° gap) discriminability validation.
- **PM revision** (preserved under path (b)): SLIP_TWEEN_DURATION_S range `[0.10, 0.15]s` (not collapsed to `{0.10s}` per path (a) rejection); 5-lane; current_lane source-lane semantic during SLIPPING.
- **Death Replay** (post-MVP): accumulator-replay vs wall-clock-rederivation flagged.

### Editorial defects flagged (post-pass; NOT applied)

- **WaveId type unification** (R2 review log line 251): `FPullWaveSpawnParams uint32` vs `IWaveSpawnerCallback int32`. Cluster D's pattern β work did not resolve this; defer to dedicated editorial pass or first implementation story.

### Same-session bias risk this pass

This pass was authored in the same session as the R2 review log was read. **However**, the actual edit proposals were generated by 5 parallel fresh-context specialist subagents (each with own context window), preserving the fresh-context property R2 mandated. The main-session integrator's role was strictly serial application + path-(b) commitment of pre-BOQ E proposals + cross-cluster deconflict — no design re-litigation. The integration risk is therefore lower than DPC R7's single-session revise-after-review pattern that triggered R2's "same-session bias confirmed" finding.

### R3 forecast

Unchanged from R2 projection: **3–6 BLOCKERS at R3** if all 6 root causes (Cluster A + B/C/D/E/F) addressed cleanly. Cluster A landed clean; this pass closes the remaining 15 R2 BLOCKING. Risk areas for R3: (a) Cluster E's zero-margin tuning under path (b) — any subsequent constraint adjustment (frame rate, REACTION_BUDGET measurement evidence) reopens; (b) Cluster F's CF-A1 test accessor approach may surface architectural review concerns at engine-specialist level (UObject + non-shipping accessor pattern); (c) LEAN_BRIGHTNESS_PEAK_RATIO=1.8 inlined-constant choice (vs registry entry) is a minor style choice that may surface at R3.

### Files modified this pass

- `design/gdd/pull-wave-behavior.md` (763 → 825 lines)
- `docs/architecture/platform-seam-interfaces.md` (~1666 → 1769 lines)
- `design/registry/entities.yaml` (~471 → 614 lines)
- `design/gdd/reviews/pull-wave-behavior-review-log.md` (this entry)
- `production/session-state/active.md` (R2 cluster pass checkpoint + R3 readiness)

### Files NOT modified this pass (deliberately deferred)

- `design/gdd/difficulty-phase-controller.md` (DPC mid R5 Cluster A+B+C re-review pending; do not edit cross-GDD until DPC R6 lands)
- `design/gdd/run-state-machine.md` (RSM forward contracts queued for R5/R7 revision pass: ForceTickNow + OnPausedChanged)
- `design/gdd/player-movement.md` (PM revision via `/propagate-design-change` post Pull-Wave R3)
- `design/gdd/systems-index.md` (update on R3 PASS)

---

## Review — 2026-06-08 — Verdict: NEEDS REVISION (R3, fresh-context)
Scope signal: L (matches R1 + R2)
Specialists: game-designer, systems-designer, ai-programmer, level-designer, qa-lead, performance-analyst, art-director, creative-director (synthesis). **Performance-analyst output successfully returned this pass (R2 token-budget failure not repeated)** — tight prompt with structured output format + max-finding cap worked.
Blocking items: 12 (concentrating to 6 root causes) | Recommended: 14 | Nice-to-have: 11 | Specialist disagreements: **0** (first time at this GDD; 7-way consensus)
Phase 2b seam-doc grep: **CLEAN PASS** — all 4 actively-referenced interfaces (ICurveProvider, IPlayerMovementProvider, IRSMTimeStateProvider, IWaveSpawnerCallback) defined in `docs/architecture/platform-seam-interfaces.md`; Cluster F additions (Seam 7 OnPausedChanged delegate + SetCurrentStateWithPauseClear helper; Seam 13 HandleWaveHit/HandleNearMiss + counters + GetEventLogInOrder) all present; Cluster D strips (FireSlipMidpoint/GetOnSlipMidpointDelegate/FOnSlipMidpoint) verified — only prose references explaining removal remain; IPullWaveInstanceObserver explicit deferral acknowledged at GDD line 715.
Prior verdict resolved: R2 MAJOR REVISION (18 BLOCKING / 6 roots) → Cluster A standalone + Clusters B+C+D+E+F parallel revision passes 2026-06-08 → R3 fresh-context surfaces 12 BLOCKING / 6 roots. R2 forecast was 3–6 BLOCKING; actual ~12 tagged items but with high cross-cluster convergence collapse to 6 roots, near forecast bottom.

Summary: GDD is structurally sound — pillars hold, encoding contracts hold, seam interfaces verified clean. The remaining issues are **integration debt from the parallel-cluster pass, not foundational design problems**. Root-cause count stuck at 6 (R2 also 6) is the warning signal: BLOCKING drops but roots stick, matching DPC R5→R6 pattern (needed one more pass to break the floor), not DPC R6→R7. Four R2 items survived "all 18 BLOCKING addressed": G-3 "zero overdraw cost" overreach (perf B-2), 2-frame near-miss flash (3-way 2026-06-07 finding), LEANING undercount in AC-PW-22a state mix (perf R-1), HISM/ISMC ambiguity in G-2 (perf R-3). Plus a partial: OQ-PW-2 close-at-Pull-Wave (R2 CD adjudication #5 never cascaded into the parallel-cluster pass — RC-R3-5). **Empirical confirmation that parallel-cluster revision has a known failure mode: cross-cluster defects and cascading CD adjudications fall between cluster owners.** When Clusters B+C+D+E+F ran in parallel, items spanning clusters or requiring re-cascade of prior CD adjudications had no single owner.

### Root cause clustering (per CD synthesis)

**RC-R3-1 — Cluster A wall-clock residue in AC-PW-01/02/03** (3-way convergence: game-designer B-2 + systems-designer B-1 + qa-lead B-1)
- AC-PW-01 GIVEN specifies `GameTime = 11.8`, `SpawnTimeS = 10.0`, `LeanDurationS = 0.6` — but F-TRAJ-TNORM post-Cluster-A reads `clamp(TraverseElapsedS / TravelDurationS, 0.0, 1.0)`. `GameTime` is no longer a formula input.
- AC-PW-02/03 reference `GameTime < SpawnTimeS + LeanDurationS` and `GameTime > SpawnTimeS + LeanDurationS + TravelDurationS` — guards the implementation no longer checks.
- A test engineer cannot construct the GIVEN through Seam 7. **Three ACs are structurally untestable as written.**
- Cluster A pass fixed F-TRAJ-TNORM but did not sweep the AC block that depended on it. **Same defect class as R2's stale Seam-12 OnSlipMidpoint references — deferred-as-cosmetic, caught late.**

**RC-R3-2 — Tier-0 (straight-forward) wave undefined behavior** (4-way convergence: systems-designer B-2 + game-designer R-3 + level-designer R-4 + ai-programmer N-2)
- Data Contract (line 649) acknowledges `lean_magnitude_tier = 0` for straight-forward waves where `abs(target_lane − source_lane) == 0`.
- LEAN_ANGLE_DEG_BY_TIER table (line 625) maps tiers 1–4 only; `LEAN_ANGLE_DEG_BY_TIER[0]` either reads OOB or requires nowhere-specified special case.
- `LeanChargeIntensity` formula (line 658) ungated on tier: a tier-0 wave charges to 1.8× brightness with zero rotation, producing "charge without direction" signal that contradicts Pillar 2's encoding contract (brightness = magnitude → destination distance).
- Telegraph forward contract AC (line 638) asserts coverage for tiers `{1,2,3,4}` only; tier-0 entirely excluded.
- Tier-0 wave holds LEANING state for 0.65s communicating zero spatial information; no AC covers this case.

**RC-R3-3 — CF-A1 sentinel `-1.0f` ambiguity creates AC false-positive trap** (2-way convergence: ai-programmer R-1 + qa-lead B-3)
- `GetTraverseElapsedS_ForTest(WaveId)` (GDD line 715) returns `-1.0f` for both "WaveId not found" AND "wave found but not TRAVERSING".
- AC-PW-16 freeze assertion (`elapsed_at_t0 == elapsed_at_t20`, "does not advance") passes when a pause-flush bug wrongly transitions the wave out of TRAVERSING during the 20 ticks — both reads return `-1.0f`, assertion passes while the wave is actually gone.
- **This is the exact false-positive pattern AC-PW-17b was designed to prevent — Cluster F installed the accessor but reproduced the failure mode at the sentinel layer.**

**RC-R3-4 — AC-PW-15 ordering claim exceeds stub observability + active-wave list container unbound** (2-way convergence: ai-programmer B-1/B-2 + qa-lead B-4)
- AC-PW-15 asserts 3-step despawn ordering: (i) `OnCollisionUnregistered` FIRST, (ii) Telegraph unregister SECOND, (iii) `OnWaveDespawned` THIRD. Seam 13 stub observes (i) and (iii) only — step (ii) is in Telegraph System's domain, unobservable from current test harness. A conforming implementation reversing (ii) and (iii) passes all assertions.
- Pattern β's production-side `Collision->UnregisterWave` ordering relative to `OnWaveDespawned` is not validated either — a production bug deferring `Collision->UnregisterWave` to next-tick queue passes AC-PW-15 entirely (stub records despawn-driver-side call sequence, not production-side internal forwarding).
- Active-wave list container type is unbound. Rule 14(b) requires `wave_id ASC` iteration, but UE5's standard `TArray::RemoveAtSwap` (O(1) idiom) silently breaks the ascending invariant after the first natural despawn. AC-PW-10's 100-tick GIVEN doesn't stipulate any mid-window despawn; broadcast-order assertion in AC-PW-28 also passes because both waves are alive simultaneously. **Bit-identical determinism guarantee relies entirely on this unbound contract.**

**RC-R3-5 — OQ-PW-2 mixed-velocity barrage + OPENER admissibility = R2 CD adjudication never cascaded** (3-way convergence: game-designer B-1 + level-designer B-2 + level-designer B-4)
- R2 CD adjudication #5 (review-log line 221) bound mixed-velocity barrage policy AT PULL-WAVE LAYER: "mixed velocity invalidates F-BARRAGE-SURVIVABILITY-INVARIANT timing model."
- GDD line 820 still reads "Owner: game-designer + level-designer | Target Resolution: Wave Spawner GDD authoring." Wave Spawner forward contract (lines 190-194) contains no uniform-velocity clause. **R2 cluster deliverable was never applied** — parallel-cluster pass missed this cross-cluster CD cascade.
- Similarly, OPENER `is_barrage=true` admissibility (R2 review-log line 288) is deferred to Wave Spawner despite Pull-Wave's F-BARRAGE-SURVIVABILITY-INVARIANT owning the survivability claim. Decision must live at Pull-Wave layer.

**RC-R3-6 — Ghost-trail fake-acceleration is Pillar 5 violation + persisted visual defects** (single + 3-way persisted)
- GDD line 667: ghost-trail final-25% scale-up "makes the mass appear to accelerate into the player plane without altering the simulation." Pillar 5 ("Skill Is Visible — no mystery deaths") forbids perceptual deception. If a player learns the cue and times dodges to it, they read a false signal. (art-director B-1)
- Ghost-trail spacing produces visual fusion at 18 m/s: 3 cubes × 0.5m / 18 m/s = 28ms each = 5-frame total trail depth. On 6-inch mobile, produces fused stamps rather than legible directional streak. At PEAK 16 waves × 3 cubes = 48 fused trail cubes. (art-director B-2)
- Near-miss edge flash 2-frame (33ms) below reliable attribution threshold at 6-inch mobile. **Persisted defect from R2** — R2 review-log line 229 records 3-way convergence (game-d + level-d + art-d); Cluster F did not fix. 50–80ms is the threshold for reliable conscious perception of a chromatic event on peripheral visual element. Frame-rate-dependent: 30fps = 66ms, 120Hz = 16ms. (3-way R3: art-d B-3 + game-d R-1 + level-d R-2)

### CD adjudications (binding)

1. **Ghost-trail fake-acceleration (RC-R3-6)**: REMOVE OR REWORK. Pillar 5 violation. Defer perspective-projection growth (math-only truth) to post-MVP Visual Polish. Add to Pillar 5 design test: "If the player can learn the cue and dodge to it, the cue must correspond to simulation state."
2. **Brightness uniform across lean tiers (art-d B-4)**: ADOPT, BUT DOCUMENT. Angle carries magnitude; brightness carries phase (CHARGING → PEAK). Dual-channel would create conflict-resolve dilution. Add explicit rationale paragraph to GDD.
3. **OQ-PW-2 mixed-velocity barrage (RC-R3-5)**: BIND AT PULL-WAVE LAYER. "All waves in a barrage MUST share velocity tier." Add to Data Contract + Wave Spawner forward contract. Update GDD line 820 to mark CLOSED.
4. **OPENER `is_barrage=true` admissibility (RC-R3-5)**: OWNED BY PULL-WAVE. "OPENER waves cannot be `is_barrage=true`; first wave must be solo." Add to Data Contract + Wave Spawner forward contract.
5. **K=10 spatial diversity (level-d B-1)**: STRENGTHEN THE AC. Bind to pattern-class count, not triplet count. AC-PW-PILLAR-2-BARRAGE-SPATIAL-K must require ≥6 mechanically-distinct escape strategies validated by pattern-class taxonomy.
6. **Ghost-trail readability + lateral containment (art-d B-2 + R-1)**: DEFER TO PROTOTYPE WITH GUARDRAILS. Keep trail spec PROVISIONAL; add new prototype validation AC-PW-22c (perceptible as distinct cubes not fused mass AND lateral containment ≤ lane width).

### Cross-cluster convergence patterns (specialists independently surfaced same root)

- AC-PW-01/02/03 wall-clock residue: game-d + sys-d + qa-l (3-way)
- Tier-0 undefined behavior: sys-d + game-d + level-d + ai-p (4-way)
- CF-A1 sentinel ambiguity: ai-p + qa-l (2-way)
- AC-PW-15 Telegraph step ii unverifiable: ai-p + qa-l (2-way)
- OQ-PW-2 close-at-Pull-Wave never applied: game-d + level-d (2-way) + OPENER admissibility level-d (related)
- 2-frame near-miss flash inadequate: art-d + game-d + level-d (3-way; persisted from R2)
- AC count claim wrong (39 vs actual 40 active / 36 BLOCKING vs claimed 30): qa-l + sys-d + game-d (3-way)
- LEAN_BRIGHTNESS_PEAK_RATIO=1.8 inlined not registry: game-d + art-d (2-way; predicted at R2 review-log line 482)

### Persisted-from-R2 defects (process diagnosis)

Four R2 items survived "all 18 BLOCKING addressed": G-3 "zero overdraw cost" overreach, 2-frame near-miss flash, AC-PW-22a LEANING undercount, HISM/ISMC ambiguity. Plus partial OQ-PW-2. **Diagnosis (per CD synthesis)**: parallel-cluster revision has a now-empirically-confirmed failure mode — when Clusters B+C+D+E+F ran in parallel, each owner addressed items tagged inside their cluster. Items spanning clusters (2-frame flash spans game-d + level-d + art-d) and items requiring re-cascade of prior CD adjudications (OQ-PW-2 was CD adjudication #5) had no single cluster owner. **The parallel structure resolved local issues efficiently but did not handle convergent or cascading items.** This is not a process failure — it's an expected residual when trading cross-cluster fidelity for throughput. R2 strategy was correct given 18 blockers; R4 needs a different strategy.

### Forward-imposed contracts on downstream GDDs (R3 additions)

- **Wave Spawner**: (a) MUST NOT emit a barrage with mixed velocity tiers (CD-R3-3); (b) MUST NOT emit OPENER waves with `is_barrage=true` (CD-R3-4); (c) MUST validate pattern-class diversity (≥6 mechanically-distinct escape strategies), not raw triplet count, in barrage configuration authoring (CD-R3-5); (d) MUST allocate `wave_id` deterministically and document initial allocator state (qa-l R-2).
- **Telegraph**: (e) MUST handle `lean_magnitude_tier = 0` as a first-class case with explicit visual spec; coverage AC must extend to `{0,1,2,3,4}` (RC-R3-2); (f) Telegraph register/unregister ordering MUST be testable through a Telegraph-side observability hook, not only via downstream side effects (RC-R3-4).
- **Player Movement**: (g) Near-miss edge flash duration MUST be specified as `NEAR_MISS_FLASH_DURATION_S = 0.066s` registry constant (4 frames @ 60Hz minimum), not frame count (3-way convergence).
- **Collision**: (h) `Collision->UnregisterWave` MUST execute in the same tick as despawn event broadcast, not deferred (RC-R3-4 production-side ordering).

### New registry entries imposed by R3

- `LEAN_BRIGHTNESS_PEAK_RATIO = 1.8` (currently inlined, promote per art-d R-2 + game-d N-2; predicted at R2 review-log line 482)
- `NEAR_MISS_FLASH_DURATION_S = 0.066` (replacing frame-count spec per 3-way convergence)

### R4 forecast

**2–3 BLOCKERS at R4** (CD-synthesized), converging to 0–1 at R5.

Reasoning: 6 R3 roots are tractable single-pass fixes. 5 of 6 roots are R2-known issues that parallel-cluster pass missed or partially fixed. No new architectural defects. No specialist disagreements. RC-R3-1 (AC sweep), RC-R3-2 (tier-0 spec), RC-R3-5 (OQ-PW-2 contract cascade) are mechanical fixes — propagating already-decided semantics. RC-R3-6 (ghost-trail Pillar 5) requires a single CD-binding decision on cue removal. RC-R3-3 (sentinel) and RC-R3-4 (container binding + stub gap) are local code-level decisions. **Convergence trajectory R1=23/7 → R2=18/6 → R3=12/6 is healthy convergence in BLOCKING count but unhealthy in root-count plateau** — matches DPC R5→R6 pattern (needed one more pass to break the floor), not DPC R6→R7. Pull-Wave is one revision pass away from APPROVED.

### Strategic recommendation (R4 strategy CD-binding)

**Single-session sequential bundle, NOT parallel clusters.** R3 empirically confirmed parallel clustering has a cross-cluster cascade failure mode.

The 6 R3 roots split into:
- **Cascade group** (RC-R3-1 AC sweep, RC-R3-2 tier-0 spec, RC-R3-5 OQ-PW-2 + OPENER, RC-R3-6 ghost-trail Pillar 5): need single-owner sequential handling. One CD pass with the GDD open, working through cascades in dependency order.
- **Local group** (RC-R3-3 sentinel CF-A1 redesign, RC-R3-4 stub observability + Telegraph step ii + container binding): isolated, could parallelize, but at 2 items the overhead isn't worth it.

Bundle all 6 roots into one revision session, owned by creative-director + game-designer + qa-lead in sequence (not parallel). Estimated 1 working session. **Spawn fresh — do not continue current session.** Re-review after.

**Rejected paths**:
- Parallel-cluster again: R3 demonstrated cross-cluster cascade failure mode at 18→12 BLOCKING; another parallel pass would replay the residual problem.
- In-session revision in current session: same-session bias caveat (R2 empirically confirmed). 16 of 18 R2 blockers were R1-missed despite "all 7 roots addressed" claim.
- Accept and proceed to Telegraph: rejected. RC-R3-6 is Pillar 5 violation; RC-R3-5 leaks unbound contract to Wave Spawner; cannot ship.
- Stop Pull-Wave entirely: roots are tractable; trajectory is healthy in BLOCKING shape; 8 downstream systems waiting.

### Strategic flags for producer

- Pull-Wave is project's 5th MVP system; 8 downstream systems depend on it. R4 convergence cost matters for sprint planning. Sequential bundle: 1 working session estimated.
- Performance-analyst returned this pass (R2 failure not repeated) — tight prompt with structured output format and max-finding cap worked. Apply same pattern in future reviews.
- PM revision (via `/propagate-design-change`) remains load-bearing for Pull-Wave story-Done evaluation against Approved-revised PM. Can begin in parallel with R4 revision (independent workstream).
- Telegraph prototype is independent of R4 — start now to parallelize convergence. Tier-3 vs tier-4 (3° gap at 25°/28°) discriminability + ghost-trail readability at 18 m/s (RC-R3-6 prototype gate) + near-miss flash perceptibility at PEAK density should all be validated in the same prototype session.
- Engine version still not pinned (Unreal Engine version TBD). Does not gate R4; recommend pinning before downstream GDDs begin.

### Editorial defects flagged (carry to R4)

- Header "Last Updated: 2026-06-07" stale (actually edited 2026-06-08)
- AC summary count mismatch: GDD claims 39 total / 30 BLOCKING / 9 ADVISORY; actual 40 active (excluding deleted AC-PW-23), 36 BLOCKING, 4 named ADVISORY + "5 sub-cases" unidentified
- `lateral_offset` declared range `[-4.0, 4.0]` stale (should be `[-4.02, 4.02]` per R2 editorial defect line 248; persisted)
- F-TRAVERSE-DURATION worked example uses default 0.6s not 0.65s (sys-d N-1)
- WaveId uint32 vs int32 type mismatch (R2 editorial defect line 251; persisted)
- AC-PW-23 DELETED placeholder occupies numbered slot — move to "Retired ACs" appendix
- LEAN_ANGLE_TIER safe ranges overlap; need cross-knob ordering invariant `TIER1 < TIER2 < TIER3 < TIER4` (sys-d R-1)
- REACTION_BUDGET decomposition (0.07 + 0.08 + 0.05 = 0.20s) lacks source citation (sys-d R-2)
- Float epsilon `1e-9` in AC-PW-01..09 wrong for single-precision floats (sys-d B-3 — promoted to BLOCKING-class but tractable; sweep to `1e-5`)

### Files modified this review

- `design/gdd/systems-index.md` (R3 verdict appended to Pull-Wave row + header Last Updated + next-steps checklist updated)
- `design/gdd/reviews/pull-wave-behavior-review-log.md` (this entry)

### Files NOT modified this review (deliberately deferred to R4 revision pass)

- `design/gdd/pull-wave-behavior.md` (revisions belong in R4 sequential bundle session, not in this review)
- `docs/architecture/platform-seam-interfaces.md` (no seam doc changes required by R3; CF-A1 sentinel redesign is on FPullWaveSubsystem accessor, not a seam)
- `design/registry/entities.yaml` (LEAN_BRIGHTNESS_PEAK_RATIO + NEAR_MISS_FLASH_DURATION_S additions deferred to R4 revision pass)
- `design/gdd/player-movement.md` (PM revision via `/propagate-design-change`; independent workstream from R4)
- `design/gdd/difficulty-phase-controller.md` (DPC TELEGRAPH_WINDOW_FLOOR_S 0.65s + BARRAGE_SIMULTANEITY_WINDOW_S 0.325s propagation queued for post-R4-APPROVED via `/propagate-design-change`)
- `design/gdd/run-state-machine.md` (RSM OnPausedChanged + ForceTickNow forward contracts still queued)

---

## R4 Revision Pass — 2026-06-08 — Single-Session Sequential Bundle (CD-recommended strategy)

**Trigger**: R3 verdict NEEDS REVISION carried forward (GDD byte-unchanged since R3); user selected "Run R4 sequential revision bundle" via `/design-review`'s post-revision widget.

**Strategy used**: CD-recommended single-session sequential bundle (NOT parallel clusters — R3 review-log line 587 documented parallel-cluster cross-cluster cascade failure mode). One owner, all 6 RC-R3 roots addressed in dependency order: RC-R3-1 (AC sweep) → RC-R3-2 (tier-0) → RC-R3-5 (OQ-PW-2 + OPENER cascade) → RC-R3-6 (Pillar 5 + registry) → RC-R3-3 (CF-A1 sentinel) → RC-R3-4 (container + Telegraph step ii). Editorial sweep applied as a closing pass.

**User design decisions taken (consolidated AskUserQuestion before any edits per skill protocol)**:

| Question | User decision | Rationale captured |
|---|---|---|
| Tier-0 wave brightness policy | Tier-0 ramps brightness 1.0× → 1.8× (same eased curve as tiers 1–4) | Brightness carries phase, not magnitude (R3 CD adjudication #2); tier-0 visually mute during LEANING would degrade read-and-react loop |
| CF-A1 sentinel redesign shape | State-aware struct `FTraverseElapsedQuery { bool bWaveFound; EPullWaveState State; float ElapsedS; }` | Most ergonomic for the false-positive guard AC-PW-16/17b was designed to be; tests can assert State independently of value |

**Blocker → fix mapping (R4 closure)**:

| R3 Root | Fix Applied | GDD Location |
|---|---|---|
| RC-R3-1 (3-way) | AC-PW-01/02/03 GIVEN clauses swept to TraverseElapsedS/TravelDurationS semantics; AC-PW-12 updated to reference CF-A1 struct return | AC-PW-01..03 + AC-PW-12 |
| RC-R3-2 (4-way) | LEAN_ANGLE_DEG_BY_TIER table extended with tier-0 row (locked 0.0°); Channel-separation rationale documented (angle = magnitude, brightness = phase); Telegraph forward contract AC coverage extended to {0,1,2,3,4}; LEAN_ANGLE_TIER0_DEG added to Tuning Knobs table; cross-knob ordering invariant added; new AC-PW-TIER0-LEANING | Magnitude Encoding sub-section + Tuning Knobs + AC list |
| RC-R3-3 (2-way) | CF-A1 accessor redesigned to FTraverseElapsedQuery state-aware struct; AC-PW-16 + AC-PW-17b updated with three independent assertions per query (bWaveFound, State, ElapsedS) | Test Infrastructure Dependencies + AC-PW-16 + AC-PW-17b |
| RC-R3-4 (2-way) | Active-wave list bound to TArray<FPullWaveInstanceState> with RemoveAt (forbid RemoveAtSwap); container-type binding documented in Rule 14(b); Rule 13 despawn pipeline step 2 explicitly calls `IWaveSpawnerCallback::OnTelegraphUnregistered(WaveId)`; Seam 13 extended in seam doc; AC-PW-15 rewritten to assert unified event log `[CollisionUnregistered, TelegraphUnregistered, Despawned]` | Rule 13 + Rule 14(b) + AC-PW-15 + `docs/architecture/platform-seam-interfaces.md` Seam 13 |
| RC-R3-5 (3-way) | OQ-PW-2 marked CLOSED at Pull-Wave layer (uniform ForwardVelocityMs literal equality within any barrage); OPENER `is_barrage=true` forbidden at Pull-Wave layer; new AC-PW-BARRAGE-UNIFORM-VELOCITY (BLOCKING); new AC-PW-OPENER-NO-BARRAGE (BLOCKING); AC-PW-PILLAR-2-BARRAGE-SPATIAL-K strengthened to require pattern-class diversity ≥6 with explicit taxonomy of 6 mechanically-distinct escape-strategy classes; Wave Spawner forward contracts (d) + (e) + (f) added to Dependencies | Open Questions table + Interactions Wave Spawner section + AC-PW-PILLAR-2-BARRAGE-SPATIAL-K + Dependencies Wave Spawner row |
| RC-R3-6 (single + 3-way persisted) | Ghost-trail final-25% scale-up cue REMOVED (Pillar 5 binding; Pillar 5 design-test addition documented for future cues); near-miss flash duration replaced with `NEAR_MISS_FLASH_DURATION_S = 0.066s` registry constant (time-based, not frame-count); `LEAN_BRIGHTNESS_PEAK_RATIO = 1.8` promoted to registry; new AC-PW-22c (ADVISORY at story-Done, BLOCKING at Alpha — ghost-trail readability + lateral containment prototype gate) | Traverse/Landing visuals + Near-Miss visual + AC-PW-22c + `design/registry/entities.yaml` |

**Persisted-from-R2 defects (RESOLVED in R4)**:
- "Zero overdraw cost" G-3 overreach — addressed via AC-PW-22b pattern 10 (FORBID fake-acceleration regression guard) + AC-PW-22c prototype gate (validates trail reads as distinct cubes at PEAK velocity).
- 2-frame near-miss flash — replaced with `NEAR_MISS_FLASH_DURATION_S = 0.066s` time-based registry constant.
- LEAN_BRIGHTNESS_PEAK_RATIO inlined — promoted to registry; AC-PW-22b pattern 8 requires source to reference registry constant.
- HISM/ISMC ambiguity — retained as OQ-PW-3 (technical-director + performance-analyst at prototype milestone); not a Pull-Wave-layer decision.
- AC-PW-22a LEANING undercount — addressed in mixed-state mix wording from R1 RC-E E-6; R4 retains the wording.

**Editorial sweep (R3-tagged + R4-found)**:
- Header status + Last Updated date refreshed to 2026-06-08 / R4 prep.
- AC summary count rewritten: 43 active ACs (35 BLOCKING / 8 ADVISORY); AC-PW-23 retained as DELETED placeholder in dedicated "Retired ACs" sub-section (not counted in active total).
- `lateral_offset` declared range corrected: `[−4.0, 4.0]` → `[−4.02, 4.02]` (R3 editorial persisted from R2 — now resolved).
- `WaveId` type unified to `int32` throughout (FPullWaveSpawnParams + FPullWaveInstanceState + Seam 13 + delegate signatures + test stub fields).
- Float epsilon `1e-9` → `1e-5` swept across AC-PW-01..09 + AC-PW-12 + test infrastructure note (1e-9 is below IEEE 754 single-precision epsilon ~1.19e-7).
- LEAN_ANGLE_TIER cross-knob ordering invariant (`TIER0 (0) < TIER1 < TIER2 < TIER3 < TIER4`) documented in Magnitude Encoding + Tuning Knobs row for TIER1 + AC-PW-22b grep pattern 7.
- REACTION_BUDGET 0.20s decomposition citations added (Carpenter 1988, Rayner 1998, Hick 1952, Welford 1980, Wood & Zaichkowsky 2002, Mark et al. 2008, Mathôt et al. 2014).
- `LEAN_BRIGHTNESS_PEAK_RATIO` safe range tightened in registry: `[1.4, 2.2]` → `[1.5, 2.2]` (the 1.4 floor is below the readable mobile minimum per the same Pillar-2 read-distance constraint that locked the 1.8 default).

**Files modified this revision pass**:
- `design/gdd/pull-wave-behavior.md` (R4 sequential bundle — all 6 RC-R3 roots + editorial sweep)
- `docs/architecture/platform-seam-interfaces.md` (Seam 13 extended with `OnTelegraphUnregistered` + counter + event log enum + Reset)
- `design/registry/entities.yaml` (NEW: `LEAN_ANGLE_TIER0_DEG = 0.0` active; NEW: `NEAR_MISS_FLASH_DURATION_S = 0.066` active; UPDATED: `LEAN_BRIGHTNESS_PEAK_RATIO` notes for 5-tier inclusion + channel-separation rationale; safe range `[1.4, 2.2]` → `[1.5, 2.2]`; revised: 2026-06-08; header `last_updated` notes refreshed)
- `design/gdd/reviews/pull-wave-behavior-review-log.md` (this entry)
- `design/gdd/systems-index.md` (R4 verdict to be updated via skill Phase 5 widget)

**Files NOT modified by R4 revision (deliberately deferred)**:
- `design/gdd/player-movement.md` (PM revision via `/propagate-design-change` — independent workstream from R4; PM forward contracts unchanged)
- `design/gdd/difficulty-phase-controller.md` (DPC TELEGRAPH_WINDOW_FLOOR_S 0.65s + BARRAGE_SIMULTANEITY_WINDOW_S 0.325s propagation queued for post-R4-APPROVED via `/propagate-design-change`)
- `design/gdd/run-state-machine.md` (RSM OnPausedChanged + ForceTickNow forward contracts; queue position unchanged by R4)

**Strategic flags**:
- The R4 revision pass introduces THREE new BLOCKING ACs (TIER0-LEANING, BARRAGE-UNIFORM-VELOCITY, OPENER-NO-BARRAGE) — each closes an R3-identified gap and adds Wave Spawner forward contracts that the Wave Spawner GDD (not yet authored) will inherit.
- AC-PW-22c is the only new ADVISORY-at-story-Done AC; it gates Alpha. The Telegraph prototype is the natural validation venue (parallel workstream).
- Convergence trajectory: R1=23/7 → R2=18/6 → R3=12/6 → R4=(pending re-review). CD R3 forecast: 2–3 BLOCKING at R4 re-review, converging to 0–1 at R5.
- Recommend `/clear` + fresh-session R4 fresh-context design-review (CD strategy carry-forward; same-session bias caveat applies to in-session re-review of just-applied revisions).

---

## Review — 2026-06-08 — Verdict: NEEDS REVISION (R4 fresh-context re-review)

Scope signal: M
Specialists: systems-designer, qa-lead, ai-programmer, level-designer, art-director, creative-director (synthesis). game-designer agent returned an unfinished thinking statement only; brief items independently covered by other specialists.
Blocking items: 6 | Recommended: 10 | Nice-to-have: 4
Phase 2b seam-doc grep: CLEAN PASS (all 4 actively-referenced project interfaces + Cluster F additions + R4 RC-R3-4 OnTelegraphUnregistered extension authored; all test-stub methods referenced in ACs verified present in seam doc).

Summary: R4's "single-session sequential bundle" was designed to prevent R3's parallel-cluster cascade-dropout failure mode. It solved coordination but traded it for confirmation bias — the author trusted the binding-decisions paragraph at GDD line 7 instead of re-grepping the body, producing 6+ header-body desynchronizations. The AC count summary editorial defect (3-way R3 finding) re-appeared with new wrong numbers (claim 35/8/43 vs actual 39/5/44) — R3's same-session-bias caveat empirically reconfirmed at a new failure shape. Substantive R4 changes (RC-R3-1 through RC-R3-6 root cause fixes) all landed mechanically; the dropouts are downstream instances of those fixes that the sequential-bundle author missed. Plus three single-source structural defects: safe-range overlap breaks cross-knob ordering invariant claim (sys-d); trail spacing rationale geometrically false after R4 removed approach-intensity signal (art-d); WAVE_DESPAWN_HOLD_S floor below NEAR_MISS_FLASH_DURATION_S.

Prior verdict resolved: Partial. R3's 6 root causes (RC-R3-1..6) all received mechanical fixes that landed correctly — but R3's editorial-defects list and persisted-from-R2 defects were only partially swept. R3 forecast 2–3 BLOCKING; actual 6.

### 6 BLOCKING items (collapsed cross-specialist tags)

1. **[sys-d + qa-l + ai-p — 3-way cascade-dropout cluster — R3 failure mode recurrent]** Header-body desynchronizations from binding-decisions paragraph (GDD line 7): F-TRAVERSE-DURATION worked example still uses 0.6s lean at lines 365/369/371 (should be 0.65s post-floor-raise — R3 flagged); Near-Miss Visual Juice section line 703 still says "2-frame cool-highlight pulse" (contradicts NEAR_MISS_FLASH_DURATION_S=0.066s binding at line 696); inherited-knob table line 559 shows BARRAGE_SIMULTANEITY_WINDOW_S=0.3s (R4 implies 0.325s per line 442); AC-PW-20 line 795 still uses 1e-9 epsilon (R4 binding-decisions claimed sweep across formula ACs; AC summary line 867 silently narrowed scope); AC-PW-22b header line 844 says "all five patterns match" but body lists ten (R4 added patterns 6–10).

2. **[qa-l + sys-d + ai-p — 3-way persisted-from-R3 editorial]** AC count summary self-inconsistent (line 861). Enumerated BLOCKING = 39 from line 869 list, claim = 35. Named ADVISORY = 5 (22a, 22c, 29, 32, WAVE-SPAWNER-PEAK-VELOCITY-FLOOR), claim = 8 with "+3 sub-cases tagged advisory" that cannot be located by name in the document. Total active = 44, claim = 43. Same root cause as R3 finding; R4 editorial sweep produced new wrong numbers instead of mechanical regeneration.

3. **[sys-d — single source, structural]** Safe-range overlap makes cross-knob invariant claim at line 643 factually false. TIER1 [14,20] ∩ TIER2 [18,24] = [18,20] — TIER1=20, TIER2=18 violates `TIER1 < TIER2` while both values are in their "safe" ranges. Same for TIER2↔TIER3 ([21,24] overlap) and TIER3↔TIER4 ([25,27] overlap). AC-PW-22b pattern 7 static_assert catches at build time, but the design-layer claim "safe ranges configured to preserve invariant" is mathematically wrong. A tuner reading the ranges will infer any value within them is safe.

4. **[qa-l + level-d — 2-way]** Wave-Spawner-dependent ACs apply inconsistent gate level. Three R4-introduced ACs (PILLAR-2-BARRAGE-SPATIAL-K at line 825, BARRAGE-UNIFORM-VELOCITY at line 827, OPENER-NO-BARRAGE at line 829) marked BLOCKING-at-story-Done; the identical-precondition AC-PW-WAVE-SPAWNER-PEAK-VELOCITY-FLOOR at line 837 is ADVISORY-until-WS-GDD-authored. Gate level inconsistent across the precondition class.

5. **[art-d — single source, Pillar 5 binding]** Trail spacing rationale geometrically false (lines 685 vs 689). Line 685 specifies ghost cubes at "half-unit intervals" — fixed object-space offsets, wave-local frame, constant regardless of velocity. Line 689 claims "faster waves produce more spread-out ghost cubes at the same forward step." Mutually exclusive. R4 RC-R3-6 removed the only approach-intensity signal (final-25% scale-up cue) under Pillar 5 binding and replaced it with this geometrically-impossible rationale. Pillar 5 contract ("no mystery deaths" + honest perceptual cues) is now under-served at PEAK velocity — the trail communicates direction but not differential velocity, contrary to the replacement's stated rationale.

6. **[sys-d — single source]** WAVE_DESPAWN_HOLD_S safe range floor [0.05, 0.5] (line 534) is below NEAR_MISS_FLASH_DURATION_S = 0.066s. At floor configuration, the near-miss flash is truncated mid-animation — Pillar 5 readability regression. Line 696 mentions the interaction; line 534 declaration site provides no cross-reference or floor note. Either raise floor to ≥0.075s OR add inline cross-knob note.

### Cross-cluster convergence patterns (R3-style multi-specialist alignment)

- AC count summary wrong: qa-l F1 + sys-d R2 + ai-p F5 (3-way; persisted from R3 with new wrong numbers)
- AC-PW-PILLAR-2-BARRAGE-SPATIAL-K taxonomy "(example) MAY refine downstream" makes K_class threshold renegotiable: qa-l F3 + level-d F1 (2-way)
- Cascade-dropout pattern (R4 fixed root, missed downstream instances): sys-d B2 + B3 + R3 + R1; ai-p F1 + F5 (6+ instances)
- AC-PW-22c prototype gate gaps: qa-l F4 methodology ambiguity + level-d F3 velocity range stops at 12 m/s (max is 18) + art-d F4 near-miss flash camera-frustum coverage (3-way)
- Wave-Spawner-dependent AC gate level inconsistent: qa-l F2 + level-d F1 (2-way)

### CD synthesis (binding)

R4's sequential-bundle strategy solved R3's parallel-cluster cascade-dropout problem and traded it for confirmation bias. The author trusted the binding-decisions paragraph as a change-log instead of re-grepping the body. Header-body desynchronization is the recurring failure shape — three of six BLOCKING items are header-says-X/body-says-Y. The safe-range overlap (B3) is a boundary-value-not-checked failure that the systems-designer adversarial protocol catches but wasn't re-run after R4 edits. The trail rationale substitution (B5) is a different class — single-session sequential authoring is *worse* than parallel for substitution failures because there's no second voice to challenge.

Without a structural change, R5 will plateau at 2–4 BLOCKING. The cascade-dropout class is on its third manifestation (R3 found it; R4 missed downstream instances; R5 will find new ones unless verification is mechanized).

### R5 strategy (CD-binding)

**Switch from "single-session sequential bundle" to "sequential bundle + automated verification pass."**

1. Before R5 review, run a mechanical grep pass comparing every claim in the binding-decisions paragraph (line 7) against the body. Script-able, not human-judgment. Catches header-body desyncs structurally.
2. AC count summary regenerated mechanically from body — never hand-edited.
3. For every safe-range or invariant claim, plug boundary values (mini sys-d protocol). Catches B3-class defects.
4. R5 specialist review switches back to parallel-cluster mode but runs verification pass FIRST — cascading editorial defects don't burn specialist tokens, freeing them for genuine design judgment.

**R5 success criteria**: zero header-body desynchronizations; zero binding-vs-body drift; AC count mechanically verified; specialist findings concentrate on genuine design questions (camera-frustum perceptibility, taxonomy lock-in, scope reservations) not cascade residue.

### R5 forecast

**2–4 BLOCKING with HIGH plateau risk.** Convergence to 0–1 conditional on structural change to revision workflow. Trajectory R1=23/7 → R2=18/6 → R3=12/6 → R4=6/3 (root-count condensed but cascade class persistent). Matches DPC R4→R5 pattern (BLOCKING drops, root-class plateaus) rather than DPC R6→R7 (convergence).

### Specialist disagreements

None substantive. Five specialists converged independently on AC count summary, cascade-dropout pattern, and AC-PW-PILLAR-2-BARRAGE-SPATIAL-K taxonomy renegotiability. CD down-weighted three findings from BLOCKING to RECOMMENDED (art-d F2 tier-0 camera occlusion, ai-p F2 LeanProgress observability gap, level-d F5 OPENER duration knob scope) on grounds of "measurement question not design defect" or "scope-not-defect" — adopted in this entry's BLOCKING/RECOMMENDED/NICE-TO-HAVE split.

### Process flags for producer

- R5 strategy CD-binding above includes structural workflow change. Producer should verify the verification-pass step is added to the R5 owner's tasking before R5 revision begins.
- game-designer agent returned unfinished thinking-statement only (Task tool quirk, not a process failure). Domain coverage compensated by 4 other specialists touching the same items. Re-spawn pattern: if a specialist returns mid-thought without findings, check whether the brief items have cross-coverage from other specialists before re-spawning.
- 8 downstream systems depend on Pull-Wave. R5 convergence cost matters for sprint planning. CD forecast 2–4 BLOCKING at R5 with verification-pass structural change; without it, plateau risk.
- PM revision (via `/propagate-design-change`) remains load-bearing for Pull-Wave story-Done evaluation. Can begin in parallel with R5 revision. Telegraph prototype also independent of R5 verdict.

### Files modified this review

- `design/gdd/systems-index.md` (Pull-Wave row R4 verdict appended + header Last Updated)
- `design/gdd/reviews/pull-wave-behavior-review-log.md` (this entry)

### Files NOT modified this review

- `design/gdd/pull-wave-behavior.md` (revisions belong in R5 revision pass, not in this review)
- `docs/architecture/platform-seam-interfaces.md` (no seam doc changes required by R5 — all referenced infrastructure verified present)
- `design/registry/entities.yaml` (no new constants required; B6 fix may tune WAVE_DESPAWN_HOLD_S floor)

---

## R5 Revision Pass — 2026-06-09 — In-Session Mechanical Verification-Pass Bundle (CD-recommended strategy)

**Trigger**: R4 fresh-context re-review verdict NEEDS REVISION carried forward (6 BLOCKING / 10 RECOMMENDED / 4 NICE-TO-HAVE). GDD byte-unchanged since R4 (in-session grep verified all 6 R4 BLOCKERS present verbatim before edits began). User invoked `/design-review` on the R4-state document; after surfacing the unchanged-document situation, user selected "Apply R5 revisions first" (skipping in-session specialist spawn) — matching R4 CD-binding strategy: *"Switch from 'single-session sequential bundle' to 'sequential bundle + automated verification pass.'"*

**Strategy used**: In-session mechanical edits against the enumerated R4 BLOCKERS — no specialist spawn (R4 already provided 5-specialist convergence on the 6 items, and the GDD is unchanged so spawning again would re-confirm). Verification grep run before AND after every edit cluster. Advisor consulted after the substantive edits landed and BEFORE declaring done.

**Advisor catch (R5 strategy proved itself on first application)**: AC count summary's "mechanically derived" arithmetic equation contained two extra `+ 1` terms (12 terms summing to 38, claiming 36). This is precisely the failure class R4 B2 identified — hand-edited "mechanically derived" claim that doesn't survive the addition check. Advisor flagged it; one-edit fix applied; verification-pass strategy demonstrated its own value before the revision exited the authoring session.

**Blocker → fix mapping (R5 closure)**:

| R4 BLOCKER | Fix Applied | GDD Location |
|---|---|---|
| **B1 (3-way sys-d + qa-l + ai-p — cascade-dropout)** Header-body desynchronizations from binding-decisions paragraph: F-TRAVERSE-DURATION worked example 0.6s residue at lines 365/369/371; Near-Miss Visual Juice line 703 "2-frame" prose; BARRAGE_SIMULTANEITY 0.3s at line 559 (R4 implied 0.325s); AC-PW-20 1e-9 at line 795; AC-PW-22b header "all five patterns" at line 844 vs body 10 patterns. | Five sites swept 0.6 → 0.65 + (R5 in-session catch) AC-PW-12 + AC-PW-TIER0-LEANING GIVEN clauses updated (would be floored at SPAWNED per Rule 5 otherwise; were structurally impossible at runtime); Near-Miss prose rewritten to `NEAR_MISS_FLASH_DURATION_S = 0.066s`; BARRAGE_SIMULTANEITY row annotated for pending DPC propagation at 0.325s with cross-reference to F-BARRAGE-SURVIVABILITY-INVARIANT closing paragraph; AC-PW-20 epsilon 1e-9→1e-5; AC-PW-22b heading "all five → all ten" with R2/R4 history tag. | lines 7, 44, 366, 370, 372, 485, 506, 534, 560, 573, 704, 781, 794, 796, 845 |
| **B2 (3-way qa-l + sys-d + ai-p — persisted from R3 editorial)** AC count summary self-inconsistent at line 861: enumerated 39 BLOCKING / 5 ADVISORY / 44 active; claim 35/8/43. Same root cause as R3 finding; R4 editorial sweep produced new wrong numbers. | AC count summary regenerated mechanically by section (10 + 9 + 3 + 4 + 1 + 1 + 5 + 1 + 1 + 1 = 36 BLOCKING; 2 + 3 + 1 + 2 = 8 ADVISORY; 36 + 8 = 44 active). Per-section breakdown shown explicitly. Process binding paragraph added: "Any future AC add/remove/gate-change MUST regenerate this block by re-counting sections, NOT by editing the numbers in place." Advisor post-edit catch resolved an arithmetic slip in the equation (12-term form summing to 38) — confirms verification-pass strategy's value. | lines 859–887 |
| **B3 (single source sys-d — structural)** Safe-range overlap makes cross-knob invariant claim at line 643 factually false. TIER1=[14,20] ∩ TIER2=[18,24] = [18,20]; analogous overlaps TIER2/3 and TIER3/4. A tuner reading the ranges would infer any value within them is safe. | Rewritten: per-knob safe ranges declared INDIVIDUAL bounds (not jointly-safe); overlap zones enumerated explicitly; cross-knob ordering invariant declared a SEPARATE joint constraint enforced via compile-time `static_assert` per AC-PW-22b pattern 7. Tuner-facing guidance added: any tier-angle change MUST re-verify joint ordering, not just individual knob's safe range. Authoring tool SHOULD surface warnings at overlap zones; cook-time static_assert is the long-stop. | line 644 |
| **B4 (2-way qa-l + level-d)** Three R4-introduced ACs (PILLAR-2-BARRAGE-SPATIAL-K, BARRAGE-UNIFORM-VELOCITY, OPENER-NO-BARRAGE) labeled BLOCKING-at-story-Done while identical-precondition AC-PW-WAVE-SPAWNER-PEAK-VELOCITY-FLOOR is ADVISORY-until-WS-authored. | All four ACs unified on PEAK-VELOCITY-FLOOR's wording: "BLOCKING at story-Done once Wave Spawner GDD + pattern pool are authored; until then ADVISORY (forward contract on a not-yet-written downstream GDD; same precondition class as AC-PW-WAVE-SPAWNER-PEAK-VELOCITY-FLOOR)." AC count breakdown reflects: 3 BLOCKING → ADVISORY (forward-contract class). | lines 826, 828, 830 |
| **B5 (single source art-d — Pillar 5 binding)** Trail spacing rationale geometrically false (lines 685 vs 689). Line 685 specifies "half-unit intervals" (fixed object-space offsets); line 689 claims "faster waves produce more spread-out ghost cubes at the same forward step." Mutually exclusive. R4 RC-R3-6 removed the only approach-intensity signal under Pillar 5 binding and replaced with this geometrically-impossible rationale. | Rewritten: removed false "more spread-out ghost cubes" claim; trail communicates direction + "wave-in-motion" identity only; speed cue is forward perspective foreshortening alone (math-only Pillar 5 truth — direct projection of simulation state); fast and slow waves produce IDENTICAL world-space trail geometry at any given instant, differing only in approach rate (math-only foreshortening). Velocity-cued trail (offset spacing as function of ForwardVelocityMs through PerInstanceCustomData) deferred to post-MVP Visual Polish under Pillar 5 design-test gate ("cue must correspond exactly to simulation state if learnable"). | line 690 |
| **B6 (single source sys-d)** WAVE_DESPAWN_HOLD_S safe-range floor [0.05, 0.5] below NEAR_MISS_FLASH_DURATION_S = 0.066s — flash truncated at floor configuration, Pillar 5 readability regression. | Floor raised `[0.05, 0.5] → [0.075, 0.5]` so the floor strictly exceeds NEAR_MISS_FLASH_DURATION_S (9ms margin tolerates registry-side rounding). Cross-knob note added inline at line 535: floor MUST stay strictly greater than NEAR_MISS_FLASH_DURATION_S; future raise of NEAR_MISS_FLASH_DURATION_S past 0.075s requires raising this floor in lockstep. Entity registry updated in lockstep (`design/registry/entities.yaml` line 469; revised 2026-06-09; same notes + lockstep rule). | line 535 + registry line 469 |

**Persisted from prior reviews (RESOLVED in R5)**:
- B1 cascade-dropout cluster: same defect class as R3 RC-R3-1 → R4 RC-R3-1 fix (which left residue at unrelated sites). R5 sweep covers all `0.6s` residue + "2-frame" prose + BARRAGE_SIMULTANEITY annotation + AC-PW-20 epsilon + AC-PW-22b heading.
- B2 AC count summary: third manifestation of count drift (R3 39/30/9 wrong → R4 35/8/43 wrong → R5 mechanical regenerate 36/8/44 correct + arithmetic slip caught by advisor + process binding paragraph). The verification-pass strategy's first test is its own AC count regeneration.

**R5 editorial closure**:
- Header status + Last Updated date refreshed to 2026-06-09 / R5 prep, with R5 binding-decisions paragraph (B1–B6) added beneath the R4 paragraph.
- All 5 R4 BLOCKER references in this entry tagged with `R5 B1` etc. inline at the GDD edit site for traceability — matches the R4 pattern.
- WAVE_DESPAWN_HOLD_S notes in `design/registry/entities.yaml` updated to carry the cross-knob lockstep rule; revised 2026-06-09.

**Strategic flags**:
- The R5 revision pass introduces **zero new BLOCKING ACs**. Gate-level reshuffle moves 3 BLOCKING → ADVISORY (Wave-Spawner-dependent precondition class). Net AC change: 44 total (unchanged); BLOCKING 39→36; ADVISORY 5→8.
- No specialist spawn was used this pass. The R4 5-specialist convergence already enumerated the 6 BLOCKERS; the verification-pass-first strategy (R4 CD-binding) was honored.
- Convergence trajectory: R1=23/7 → R2=18/6 → R3=12/6 → R4=6/3 → R5=(advisor 1 arithmetic; expected 0–1 at R6 OR APPROVED).
- The advisor catch on the AC count arithmetic is empirical evidence that the verification-pass strategy works — and that the failure class would have surfaced again at R6 without a structural change. The R6 reviewer should NOT need to re-grep binding-decisions vs body if R5's process binding paragraph holds; if R6 surfaces a new instance, the binding paragraph itself needs strengthening.
- Recommend `/clear` + fresh-session R6 fresh-context design-review (CD strategy carry-forward; same-session bias caveat applies to in-session re-review of just-applied revisions). R6 may safely return to the original Phase 3b adversarial specialist spawn since the verification-pass step has now run successfully — specialists' attention will not be burned on cascade residue.

**Files modified this revision pass**:
- `design/gdd/pull-wave-behavior.md` (R5 in-session verification-pass sweep — 14 edits across all 6 BLOCKERS + advisor-found arithmetic fix; ~887 → ~908 lines)
- `design/registry/entities.yaml` (`WAVE_DESPAWN_HOLD_S` safe-range [0.05, 0.5] → [0.075, 0.5] + cross-knob lockstep notes; revised 2026-06-09)
- `design/gdd/systems-index.md` (Pull-Wave row R5 status appended + header Last Updated)
- `production/session-state/active.md` (R4 revision + R4 re-review + R5 revision DONE; next step is R6 fresh-context re-review — staleness flag from advisor resolved)
- `design/gdd/reviews/pull-wave-behavior-review-log.md` (this entry)

**Files NOT modified by R5 revision (deliberately deferred)**:
- `docs/architecture/platform-seam-interfaces.md` (no seam doc changes required by R5 — R4 RC-R3-4 OnTelegraphUnregistered extension is the most recent seam doc edit and remains correct)
- `design/gdd/player-movement.md` (PM revision via `/propagate-design-change` — independent workstream from R5; PM forward contracts unchanged by R5)
- `design/gdd/difficulty-phase-controller.md` (DPC TELEGRAPH_WINDOW_FLOOR_S 0.65s + BARRAGE_SIMULTANEITY_WINDOW_S 0.325s propagation queued for post-R6-APPROVED via `/propagate-design-change`)
- `design/gdd/run-state-machine.md` (RSM OnPausedChanged + ForceTickNow forward contracts; queue position unchanged by R5)

---

## Review — 2026-06-09 — In-Session R5 Verdict: REVISIONS APPLIED (awaiting R6 fresh-context re-review)

Scope signal: M (matches R4)
Specialists: (none — R4 5-specialist enumeration carried forward; verification-pass-first strategy honored per R4 CD-binding)
Blocking items resolved: 6 (all 6 R4 BLOCKERS) | Advisor-found post-edit: 1 (arithmetic slip in AC count totals, fixed in place)
Phase 2b seam-doc grep: CLEAN PASS (R4 finding still holds; no seam doc changes by R5)

Summary: R5 verification-pass-first strategy honored R4 CD-binding recommendation. All 6 R4 BLOCKERS resolved via mechanical in-session edits with grep verification before/after. Advisor consulted post-substantive-edits caught an arithmetic slip in the AC count totals equation — exactly the failure class R4 B2 was designed to prevent at R6 — and the catch was applied before declaring done. Verification-pass strategy demonstrated value on first application. Convergence: R1=23/7 → R2=18/6 → R3=12/6 → R4=6/3 → R5=1 (advisor-caught; resolved); R6 forecast 0–1 BLOCKING or APPROVED.

Prior verdict resolved: R4's 6 BLOCKERS all resolved per the blocker → fix mapping above. R4 B2 (cascade-dropout failure class) addressed structurally via process binding paragraph in AC summary — future AC counts must be regenerated from sections, not edited in place.

Files modified: see "Files modified this revision pass" above.
Files NOT modified: see "Files NOT modified by R5 revision" above.

R6 strategy (carrying R4 CD-binding forward): fresh-context re-review. The verification-pass step has now run successfully (B2 arithmetic class caught + fixed pre-review); R6 specialists can be spawned in the original Phase 3b pattern without the cascade-residue tax.

---

## Review — 2026-06-10 — Verdict: MAJOR REVISION NEEDED (R6, fresh-context)
Scope signal: XL
Specialists: game-designer, systems-designer, qa-lead, ai-programmer, level-designer, performance-analyst, art-director, creative-director (synthesis)
Blocking items: 8 | Recommended: ~14 | Nice-to-have: ~6 | Specialist disagreements: 0 substantive
Phase 2b seam-doc grep: FAILED — surfaced `GetCollisionUnregisteredCount`/`GetCollisionUnregisterCount` paper-only-seam-field mismatch at main-review layer; qa-lead independently corroborated AND found a second instance (`EWaveEventType::WaveDespawned` vs `Despawned`). Same defect class as DPC R3 / R5 / R7 historical recurrences — surfaced at THIS GDD for the first time after 5 prior reviews.
Prior verdict resolved: R5's 6 R4 BLOCKERS all resolved per R5 review-log entry; R5 advisor-caught arithmetic slip resolved pre-declaration-done. R6's 8 BLOCKERS are NEW-class findings (cross-document drift, perceptual contract anchored to physical reality, invariant-test coverage gaps) — not regression on R1-R5 surface.

Summary: 8 BLOCKING items concentrate into 3 root causes per CD synthesis: (RC-A) cross-document drift — seam doc and GDD not co-edited, surfaced 2 paper-only-seam-field naming mismatches at AC-PW-15. (RC-B) Pillar 2 perceptual contract not anchored to physical reality — OPENER tier-0 first-encounter scaffolding missing, LEAN_BRIGHTNESS_PEAK_RATIO floor not derived from base material luminance. (RC-C) invariants stated but not contract-tested — AC-PW-15 contiguity ambiguous, AC-PW-22b tier-ordering grep covers 1 of 4 terms, Telegraph double-unregister contract ambiguous, container ADD ordering not bound. Convergence trajectory R1=23/7 → R2=18/6 → R3=12/6 → R4=6/3 → R5=1 was healthy on the in-document review surface; R6 surfaced a new failure CLASS (cross-document verification) that R1-R5 specialists could not have surfaced from the GDD alone.

### Root cause clustering (per CD synthesis)

**RC-A — Cross-document drift (2 BLOCKING)**:
- BLOCKING (main + qa-lead + ai-programmer corroborated): GDD AC-PW-15 line 784 references `GetCollisionUnregisteredCount()`; seam doc `docs/architecture/platform-seam-interfaces.md` line 1640 defines `GetCollisionUnregisterCount()` (no "ed"). Paper-only-seam-field — programmer implementing AC-PW-15 verbatim fails compile.
- BLOCKING (qa-lead): GDD AC-PW-15 line 784 references `EWaveEventType::WaveDespawned`; seam doc line 1658 enumerator is `Despawned` (no "Wave" prefix). Same defect class. (Note: `EWaveEventType::WaveHit` and `::NearMiss` ARE correct against the enum — only the despawn enumerator drifted.)

**RC-B — Pillar 2 perceptual contract not anchored to physical reality (2 BLOCKING)**:
- BLOCKING (game-designer): No Wave Spawner forward contract binds tier-0 introduction in OPENER. GDD argues tier-0 brightness ramp is the core "no wave is mute" invariant, but OPENER pool authoring doesn't require any tier-0 pattern in player's first N waves. First tier-0 encounter at MID/PEAK density reads ambiguously as "no wave / mute" against the player's untrained intuition.
- BLOCKING (art-director): `LEAN_BRIGHTNESS_PEAK_RATIO` safe-range floor 1.5× silently violates Pillar 2 on dark iron-gray base material (RGB 55-65/50-60/45-55). At 1.5× peak the lit-face lift is ~28 RGB points — below reliable non-foveal threshold at 6-inch mobile non-foveal viewing. The 1.5× floor was set without reference to the actual base material; tuner can pick a "safe" value that doesn't deliver the phase signal.

**RC-C — Invariants stated but not contract-tested (4 BLOCKING)**:
- BLOCKING (qa-lead): AC-PW-15 ordering assertion is not contiguity-scoped per `wave_id`. Two-same-tick despawn could interleave events; a buggy "all step-(i) then all step-(ii)" implementation passes "all present in order per wave_id" while violating same-tick atomicity Rule 13 declares.
- BLOCKING (systems-designer): AC-PW-22b pattern 7 grep covers only `TIER0 < TIER1`. Static_assert prose requires full four-term chain; misconfig like TIER1=20 + TIER2=18 (both individually within per-knob safe ranges) ships undetected.
- BLOCKING (ai-programmer): Telegraph double-unregister contract ambiguous. Line 204 ("Telegraph unsubscribes at LEANING exit") vs Rule 13 step 2 (`OnTelegraphUnregistered` at DESPAWNING) admits two interpretations — single Unregister call vs idempotent double-call. Currently undefined behavior at the boundary.
- BLOCKING (ai-programmer): Container ADD ordering not bound + AC-PW-10 doesn't exercise same-tick multi-admission. Rule 14(b) bound `RemoveAt` but says nothing about insertion idiom; AC-PW-10's GIVEN uses "identical wave_id order" without exercising same-tick admit-two case where pool-slot ordering could differ from wave_id ordering.

### Notable RECOMMENDED findings

- performance-analyst: ISMC/TrailAlpha architectural contradiction — TrailAlpha PerInstanceCustomData[3] on wave-mass ISMC vs trail cubes as separate cube meshes (different mesh instances); single-batched-draw-call claim collapses.
- art-director: Minimum tier-gap invariant absent — TIER3=26, TIER4=27 (both in safe ranges) yields 1° gap below perceptibility threshold.
- game-designer: Tier-3/Tier-4 3° step has no owning AC at Pull-Wave layer — only a forward promise to Telegraph GDD.
- game-designer: F-BARRAGE-SURVIVABILITY-INVARIANT REACTION_BUDGET (0.20s) derived from solo-wave eye-tracking literature; no AC validates the budget at M=3 PEAK barrage density.
- level-designer: Within-barrage lean-tier spread unconstrained; same barrage can mix tiers 0-4.
- level-designer: Source-lane multiplicity within barrages unconstrained; three waves spawning from same source-lane creates SPAWNED-frame visual pile.
- level-designer: K_raw=4 floor vestigial under K_class≥6; raise K_raw≥8 to prevent thin PEAK pools.
- qa-lead: AC-PW-22b pattern 5(a) regex false-positives on legitimate diagnostic comments.
- qa-lead: AC-PW-17b `1e-4` tolerance is sound at N≤375 ticks but GIVEN has no max N; harness running N=1000 would spurious-fail.
- ai-programmer: No prohibition on `RSM.current_state == RUNNING` tick gate — defensive guard could silently break ABORTED drain.
- ai-programmer: `FTraverseElapsedQuery.ElapsedS == 0.0f` returns valid-looking value for SPAWNED/LEANING/DESPAWNING — attractive nuisance.
- art-director: 48 ghost cubes at PEAK density with 1.25× lateral containment can co-occupy escape lane between adjacent-lane barrage waves.
- art-director: Near-miss flash at 66ms sits at peripheral-perception threshold floor; split-attention during mid-slip yields ~70% capture rate.

### CD adjudications (binding)

1. **RC-A (#1, #2 naming corrections)**: ADOPT mechanical renames to match seam doc. The seam doc is source of truth (already implemented at lines 1640 + 1658); rename in GDD AC-PW-15.
2. **RC-B (#3 tier-0 OPENER intro)**: ADOPT first-5-waves binding as a Wave Spawner forward contract + new AC-PW-OPENER-TIER0-INTRO (ADVISORY-until-WS-authored class, matching sibling forward-contract ACs).
3. **RC-B (#4 brightness floor)**: ADOPT floor raise 1.5 → 1.7 + material-dependency note. Conditional on current dark base palette; floor may be re-evaluated downward if art-bible-lock raises base luminance to RGB ≥100.
4. **RC-C (#5 AC-PW-15 contiguity)**: ADOPT strengthen-assertion path — contiguous triple per wave_id with no entries for other wave_ids between them. Closes the multi-despawn interleave bug.
5. **RC-C (#6 four-grep tier ordering)**: ADOPT 4 separate adjacent-pair greps (7a-7d), all required for green CI.
6. **RC-C (#7 Telegraph unregister)**: ADOPT single-call-site interpretation — per-tick reads cease at LEANING exit, UnregisterWave called once at DESPAWNING. No idempotence contract needed.
7. **RC-C (#8 container ADD)**: ADOPT `TArray::Add` (append-to-tail) binding at Rule 14(b); add AC-PW-22b pattern 11 forbidding `TArray::Insert` on active-wave-list source; extend AC-PW-10 GIVEN to exercise same-tick multi-admission.

### R7 forecast

**0-2 BLOCKING, dominated by RC-C residual.** Reasoning: RC-A is mechanical (2 renames, low residual risk); RC-B closes the Pillar 2 perceptual contract gap structurally; RC-C is the highest-residual class because invariant-test coverage tends to surface new gaps when the implementation surfaces. Trajectory: R1=23/7 → R5=1 (in-document) + R6=8 (cross-document NEW class) → R7 expected 0-2 (cross-document NEW class largely closed; residual = RC-C). Recommend `/clear` + fresh-session R7 fresh-context design-review per Phase 3b adversarial spawn pattern.

### Files modified this review

- `design/gdd/systems-index.md` (Pull-Wave row R6 verdict + R6 revision-applied status appended + header Last Updated)
- `design/gdd/reviews/pull-wave-behavior-review-log.md` (this entry + R6 revision-pass entry below)

### Files NOT modified this review

- `design/gdd/pull-wave-behavior.md` (R6 revisions are below in the R6 Revision Pass entry)
- `docs/architecture/platform-seam-interfaces.md` (no seam doc changes required by R6 — seam doc is source-of-truth for naming; GDD aligned to seam doc, not vice versa)
- `design/registry/entities.yaml` (R6 revision pass updates LEAN_BRIGHTNESS_PEAK_RATIO safe range; see R6 Revision Pass entry below)

---

## R6 Revision Pass — 2026-06-10 — In-Session Sequential Bundle (R6 findings + 4 user design decisions)

**Trigger**: R6 fresh-context re-review verdict MAJOR REVISION NEEDED (8 BLOCKING / 3 root causes / CD synthesis). User invoked `/design-review` then selected "Revise the GDD now" — fresh-session R7 re-review queued post-revision.

**Strategy used**: In-session sequential bundle. 4 design decisions grouped into a single AskUserQuestion before any edits (per skill's mid-revision-no-interrupt protocol). 4 mechanical fixes applied without user decision. All R6 BLOCKERS resolved in-session.

**User design decisions (2026-06-10)**:
- Q1 (B2 tier-0 OPENER intro): chose "At least one tier-0 pattern in first 5 OPENER waves" (Recommended)
- Q2 (B3 brightness floor): chose "Raise floor to 1.7× + add material-dependency note" (Recommended)
- Q3 (B4 AC-PW-15 contiguity): chose "Strengthen assertion to contiguous triple per wave_id" (Recommended)
- Q4 (B5 Telegraph unregister): chose "Clarify line 204: 'unsubscribes' = per-tick read cessation only; UnregisterWave called exactly once at DESPAWNING" (Recommended)

**Blocker → fix mapping (R6 closure)**:

| R6 BLOCKER | Fix Applied | GDD Location |
|---|---|---|
| **B1 (RC-A — cross-doc drift, 2 instances)** | AC-PW-15 paper-only-seam-field corrections: `GetCollisionUnregisteredCount` → `GetCollisionUnregisterCount` (no "ed" — matches seam doc line 1640); `EWaveEventType::WaveDespawned` → `EWaveEventType::Despawned` (no "Wave" prefix — matches seam doc line 1658). | line 787 (AC-PW-15) |
| **B2 (RC-B — Pillar 2 OPENER tier-0 scaffolding)** | New Wave Spawner forward contract paragraph in Dependencies section: OPENER pool MUST include ≥1 tier-0 pattern AND ≥1 tier-0 pattern MUST appear in player's first 5 OPENER waves (`OPENER_TIER0_INTRO_MAX_WAVE_INDEX = 5`). New AC-PW-OPENER-TIER0-INTRO (ADVISORY-until-WS-authored class). | Wave Spawner forward contracts section + R7 Forward Contract Closure (new AC) |
| **B3 (RC-B — Pillar 2 brightness floor)** | `LEAN_BRIGHTNESS_PEAK_RATIO` safe range tightened `[1.5, 2.2] → [1.7, 2.2]`. Material-dependency note added: floor revises if art-bible-lock raises base luminance to RGB ≥100. Body refs updated × 2 (Color and material shift during lean + Ease-curve binding). Registry updated in lockstep (`design/registry/entities.yaml` line 614; revised 2026-06-10). | line 670 (Color/material section) + line 685 (Ease-curve binding) + registry line 614 |
| **B4 (RC-C — AC-PW-15 contiguity)** | AC-PW-15 THEN clause strengthened: the three entries for `wave_id = W` appear as a CONTIGUOUS RUN in `GetEventLogInOrder()` with no entry for any other `wave_id` (or other `EWaveEventType`) between them. Catches same-tick-multi-despawn step-(i)-then-step-(ii) interleave bug. | line 787 (AC-PW-15) |
| **B5 (RC-C — Telegraph unregister contract)** | Line 204 rewritten: per-tick read cessation at LEANING exit explicitly separated from UnregisterWave call. Rule 13 step 2 (`OnTelegraphUnregistered → Telegraph->UnregisterWave`) declared the SINGLE call site; no idempotence required because the call fires exactly once per wave_id at DESPAWNING-entry tick. | line 207 (Downstream — Telegraph System) |
| **B6 (RC-C — tier ordering grep coverage)** | AC-PW-22b pattern 7 expanded from single TIER0<TIER1 grep into four separate adjacent-pair greps (7a TIER0<TIER1, 7b TIER1<TIER2, 7c TIER2<TIER3, 7d TIER3<TIER4); all four required for green CI. Closes misconfig TIER1=20+TIER2=18 (both within per-knob safe ranges) escape. | lines 858-861 (AC-PW-22b pattern 7) |
| **B7 (RC-C — container ADD binding + AC-PW-10 same-tick coverage)** | Rule 14(b) extended: active-wave list MUST be updated by `TArray::Add` (append-to-tail) at `Construct()`; `TArray::Insert` is forbidden. New AC-PW-22b pattern 11 (FORBID `TArray::Insert` on active-wave list source; ALLOW `TArray::Add`). AC-PW-10 GIVEN extended to exercise same-tick multi-admission (two waves admitted in the same tick must preserve `wave_id ASC` iteration order). | line 88 (Rule 14(b)) + line 866 (AC-PW-22b pattern 11) + AC-PW-10 GIVEN extension |

**Files modified this revision pass**:
- `design/gdd/pull-wave-behavior.md` (903 → 914 lines; 8 edits across R6 B1-B7 + header status block + AC count summary regenerated mechanically per R5 B2 process binding)
- `design/registry/entities.yaml` (`LEAN_BRIGHTNESS_PEAK_RATIO` safe range `[1.5, 2.2] → [1.7, 2.2]` + material-dependency note; revised 2026-06-10)
- `design/gdd/systems-index.md` (Pull-Wave row R6 verdict + R6 revision-applied status appended; header Last Updated refreshed to 2026-06-10)
- `design/gdd/reviews/pull-wave-behavior-review-log.md` (this entry + the R6 fresh-context review verdict entry above)

**Files NOT modified by R6 revision (deliberately deferred)**:
- `docs/architecture/platform-seam-interfaces.md` (seam doc is source-of-truth for naming; GDD aligned TO seam doc per CD adjudication #1; no seam doc changes required)
- `design/gdd/player-movement.md` (PM revision via `/propagate-design-change` — independent workstream; PM forward contracts unchanged by R6)
- `design/gdd/difficulty-phase-controller.md` (DPC TELEGRAPH_WINDOW_FLOOR_S 0.65s + BARRAGE_SIMULTANEITY_WINDOW_S 0.325s propagation queued for post-R7-APPROVED via `/propagate-design-change`)
- `design/gdd/run-state-machine.md` (RSM OnPausedChanged + ForceTickNow forward contracts; queue position unchanged by R6)

**Strategic flags**:
- R6 surfaced a NEW failure CLASS not previously found in R1-R5: cross-document verification (seam doc field naming, OPENER pool authoring contract, material-anchored perceptibility). R6 specialists effectively expanded the review surface beyond the GDD alone. Future R7+ should retain this expanded surface; the Phase 2b seam-doc grep step (already in design-review skill) caught one instance, but field-level naming check (vs interface-level existence check) was not previously done — recommend updating skill to add field-level naming grep to Phase 2b.
- Convergence trajectory: R1=23/7 → R2=18/6 → R3=12/6 → R4=6/3 → R5=1 (in-document) → R6=8 (cross-document NEW class). The convergence is INTACT for in-document defects; R6's 8 are net-new from review surface expansion. R7 forecast 0-2 BLOCKING.
- The 4 user design decisions all chose Recommended options; no specialist findings were overridden.
- AC count summary regenerated mechanically per R5 B2 process binding: 44→45 active (added AC-PW-OPENER-TIER0-INTRO); BLOCKING unchanged at 36; ADVISORY 8→9 (the new AC enters the R7-Forward-Contract section as ADVISORY-until-WS, raising that section's count from 3→4).

**R7 strategy (carrying R6 CD-binding forward)**: `/clear` → fresh-session R7 fresh-context design-review per Phase 3b adversarial spawn pattern. The R6 revision-pass step has now run; R7 specialists should focus on RC-C residual (most likely class to surface in R7) and any cross-document verification gaps R6 missed. PM revision (`/propagate-design-change`) and Telegraph prototype remain independent workstreams.

---

## Review — 2026-06-10 — In-Session R6 Revision Verdict: REVISIONS APPLIED (awaiting R7 fresh-context re-review)

Scope signal: XL (matches R6)
Specialists: (R6 7-specialist enumeration carried forward; no R7-specific spawn this entry — R6 specialists' findings drove the in-session bundle)
Blocking items resolved: 8 (all 8 R6 BLOCKERS) | User design decisions: 4 (all Recommended options chosen)
Phase 2b seam-doc grep: PASS at R6 revision-pass output (GDD aligned to seam doc names; no further mismatch)

Summary: R6 in-session sequential bundle resolved all 8 R6 BLOCKERS across 3 root causes (RC-A cross-doc drift × 2, RC-B Pillar 2 perceptual × 2, RC-C invariant-test coverage × 4). 4 user design decisions chose Recommended options. AC count regenerated mechanically per R5 B2 process binding: 44→45 active / 36 BLOCKING / 8→9 ADVISORY. Registry updated in lockstep. R7 forecast 0-2 BLOCKING dominated by RC-C residual.

Prior verdict resolved: R6's 8 BLOCKERS all resolved per the blocker → fix mapping above. RC-A class closed via mechanical seam-doc-aligned renames (no inverse risk because seam doc is source-of-truth). RC-B class closed via Wave Spawner forward contract + material-anchored safe-range floor. RC-C class closed via 4 structural fixes (contiguity, four-grep ordering, single-unregister site, container ADD binding).

Files modified: see "Files modified this revision pass" above.
Files NOT modified: see "Files NOT modified by R6 revision" above.

R7 strategy: `/clear` → fresh-session R7 fresh-context design-review per Phase 3b adversarial spawn pattern (R6 specialists effectively expanded review surface to cross-document verification; R7 should retain that surface). PM revision and Telegraph prototype remain independent workstreams.


---

## Review — 2026-06-10 — Verdict: MAJOR REVISION NEEDED (R7, fresh-context)
Scope signal: XL
Specialists: game-designer, systems-designer, qa-lead, ai-programmer, level-designer, performance-analyst, art-director, creative-director (synthesis)
Blocking items: 13 (+ 1 contingent on Cluster 3 fix path) | Recommended: ~17 | Nice-to-have: ~5 | Specialist disagreements: 3 (all adjudicated by creative-director)
Phase 2b seam-doc grep: CLEAN PASS at field-level (R6 B1 fixes verified — `GetCollisionUnregisterCount`, `EWaveEventType::Despawned`, `GetTelegraphUnregisteredCount` all match seam doc); registry constants present (LEAN_BRIGHTNESS_PEAK_RATIO safe range [1.7, 2.2] R6 B3 verified; NEAR_MISS_FLASH_DURATION_S=0.066; REACTION_BUDGET=0.20; MIN_BARRAGE_LANE_SEPARATION=1)
Prior verdict resolved: R6's 8 BLOCKERS all resolved per R6 revision-pass entry; verified clean at R7. R7 BLOCKERs are NEW-class findings (integration-level defects) — not regression on R6 surface.

Summary: 13 BLOCKING items concentrate into 5 root causes per CD synthesis: (RC-C-residual) AC contract self-inconsistency — 4 findings: TSortedMap vs pattern 9 ALLOW grep contradiction, AC-PW-15 contiguity GIVEN doesn't exercise multi-wave-same-tick, FTraverseElapsedQuery 0.0f sentinel collision, AC-PW-10 R6 B7 admit-tick visit-order vacuously satisfied. (RC-A-residual) AC-PW-17b N unbounded — 1 finding carried from R6 RECOMMENDED. (RC-D NEW) Perceptual-threshold margin failures — 3 findings: TIER3/TIER4 MIN_GAP absent (art-director R6 RECOMMENDED escalated to BLOCKING), ease curve not registry-backed (Pillar 2 phase-alignment silent failure), REACTION_BUDGET wrong literature class for M=3 choice-reaction (game-designer RECOMMENDED escalated to BLOCKING by creative-director). (RC-E NEW) Cross-system architectural contradiction — 2 findings: ISMC/TrailAlpha (R6 RECOMMENDED escalated; trails are separate meshes not wave-mass instances), AC-PW-22a LEANING undercount (PEAK mix stale against R2-path-b FLOOR raise). (RC-F NEW) Content-authoring forward contract gaps — 2 findings: within-barrage lean-tier spread unconstrained, source-lane multiplicity unconstrained (both level-designer R6 RECOMMENDED escalated). (RC-G NEW) Concurrency / lifecycle race — 1 finding: pause-flush mid-tick delegate fire ambiguity. R6 forecast undershot by 6× because R6 measured local residuals while R7 surfaced integration-level defects at seams between Pull-Wave / registry / AC contract patterns / rendering architecture.

### Root cause clustering (per CD synthesis — 5 classes)

**RC-C-residual — AC contract self-inconsistency (4 BLOCKING)**:
- B1: TSortedMap vs AC-PW-22b pattern 9 ALLOW `RemoveAt\b` grep mismatch (main + ALL 7 specialists unanimous; line 88 vs line 864).
- B4: AC-PW-15 contiguity GIVEN scaffold is single-wave; contiguity is vacuously satisfied without multi-wave-same-tick despawn case (ai-programmer; qa-lead partial concur on assertion soundness; CD adjudicated as BLOCKING coverage gap).
- B5: FTraverseElapsedQuery.ElapsedS = 0.0f for SPAWNED/LEANING/DESPAWNING cases creates false-positive trap recreating exact RC-R3-3 sentinel ambiguity (ai-programmer).
- B11: AC-PW-10 R6 B7 admit-tick visit-order has no Seam 13 observable (SPAWNED state emits zero events; two identical buggy implementations compare-equal); qa-lead correct over main reviewer initial framing (CD adjudication).

**RC-A-residual — N-bound numerical margin (1 BLOCKING)**:
- B12: AC-PW-17b N unbounded; `1e-4` tolerance derived for N≤375 (qa-lead — LIKELY BLOCKING upgraded to BLOCKING by CD).

**RC-D — Perceptual-threshold margin failures (3 BLOCKING)**:
- B6: Minimum tier-gap invariant absent — TIER3/TIER4 1° gap admissible at TIER3=27+TIER4=28 (art-director R6 RECOMMENDED → R7 BLOCKING).
- B7: Ease curve not registry-backed — standard cubic ease-in-out preset substitution returns ~0.10 at t=0.4 instead of required 0.15, silently breaking R2 Cluster F Pillar 2 phase-alignment fix (art-director).
- B8: REACTION_BUDGET cited solo-stimulus literature; M=3 PEAK barrage is choice-reaction with Hick's Law load ~+50-80ms; invariant has 0 margin (game-designer RECOMMENDED → CD escalated to BLOCKING).

**RC-E — Cross-system architectural contradiction (2 BLOCKING + 1 contingent)**:
- B9: ISMC/TrailAlpha architectural contradiction — trail cubes are separate meshes, not wave-mass ISMC instances; single-batched-draw-call invariant structurally false (performance-analyst R6 RECOMMENDED → R7 BLOCKING).
- B9b (contingent): Trail ISMC PerInstanceCustomData budget undeclared (triggered when B9 fix path (a) taken — separate trail ISMC).
- B10: AC-PW-22a LEANING count undercount — PEAK-representative mix stated as 2 LEANING but at R2-path-b FLOOR=0.65s default, derived count is ~5 (performance-analyst).

**RC-F — Content-authoring forward contract gaps (2 BLOCKING)**:
- B11(F): Within-barrage lean-tier spread unconstrained — mixed-tier barrage breaks dual-channel read (level-designer R6 RECOMMENDED → R7 BLOCKING).
- B12(F): Source-lane multiplicity within barrages unconstrained — SPAWNED-frame visual pile violates Pillar 5 (level-designer R6 RECOMMENDED → R7 BLOCKING).

**RC-G — Concurrency / lifecycle race (1 BLOCKING)**:
- B13: Pause-flush mid-tick delegate fire ambiguity — Rule 19 silent on inline-vs-queued; mid-tick fire produces inconsistent per-wave frame state (ai-programmer).

### Notable RECOMMENDED findings (~17 total)

- game-designer R1: OPENER tier-0 lane placement not constrained (non-player-lane tier-0 is non-threatening; player ignores; scaffolding fails)
- game-designer R2: Tier-3/4 perceptibility contingency missing
- game-designer R4: Dual-channel temporal asymmetry (brightness readable before angle perceptible during first ~60% of LEAN window)
- game-designer N1: K_class frequency unbounded — Sirlin-class degenerate strategy
- systems-designer R1: Pattern 11 grep scope too narrow vs pattern 9
- systems-designer R2: OPENER_TIER0_INTRO_MAX_WAVE_INDEX missing from registry
- systems-designer R3: K_class ≥6 zero-margin against 6-class taxonomy (now ≥4 at R7 B8 — same concern at lower N)
- systems-designer N1: Registry last_updated header stale (resolved at R7 post-cluster)
- systems-designer N2: EC-WAVE-LIFETIME-OVERRUN warns at legal {25m, 4m/s} combo
- ai-programmer R2: Death Replay "(or asset identifier)" re-introduces asset load dependency
- ai-programmer R5: No Pull-Wave-side defensive `check(NewWaveId > LastAdmittedWaveId)` at Construct()
- ai-programmer R7: AC-PW-17b doesn't cover unpaused-at-ABORTED drain path
- qa-lead R1: Pattern 9 scope asymmetry vs pattern 11
- qa-lead R2: Pattern 5(a) FORBID regex doesn't exclude // comment lines
- art-director R1: AC-PW-22c missing tier discrimination criterion (forwarded to non-existent Telegraph GDD)
- art-director R2: 48 ghost cubes at PEAK + 1.25× lateral containment = adjacent-lane co-occupancy
- performance-analyst: G-3 "zero overdraw cost" rhetoric on tile-GPU is technically wrong
- performance-analyst: Pool formula misses DESPAWNING dissolve hold (+2 slots)
- level-designer R1: K_class=6 ceiling not floor — min-per-class needed
- level-designer R2: OPENER tier-0 window worst-case wave#5 defeats teaching intent
- level-designer R3: OPENER pool minimum size undocumented
- level-designer R4: Pool authoring order dependency (OPENER must lock before PEAK)
- level-designer N1: BARRAGE_SIMULTANEITY_WINDOW_S not confirmed
- level-designer N2: OPENER phase duration not cross-referenced to DPC

### Specialist disagreements adjudicated by CD

1. **AC-PW-15 contiguity GIVEN** — ai-programmer (BLOCKING) vs qa-lead (CONFIRMED CLEAN). CD: ai-programmer correct on coverage gap; qa-lead correct on assertion mechanism. Both true; BLOCKING upheld for the coverage gap.
2. **AC-PW-10 R6 B7 visit-order observability** — qa-lead (vacuously satisfied / BLOCKING) vs main reviewer (initially considered observable via later-tick broadcast trace). CD: qa-lead correct; no observable witness exists; BLOCKING upheld.
3. **REACTION_BUDGET literature class** — game-designer RECOMMENDED vs CD BLOCKING. CD escalated: same failure shape as F7 (tier-gap); wrong literature + zero margin + Pillar 5 stake.

### CD adjudications (binding)

1. **B1 (TSortedMap)**: ADOPT TArray-only path (simpler; GDD argues O(N) at N=16 negligible). Strike TSortedMap escape-hatch from Rule 14(b).
2. **B4 (AC-PW-15 multi-wave GIVEN)**: ADOPT GIVEN extension with multi-wave-same-tick scenario; preserves R6 B4 contiguity assertion.
3. **B5 (FTraverseElapsedQuery sentinel)**: ADOPT -1.0f sentinel for undefined-state cases.
4. **B6 (MIN_TIER_GAP)**: ADOPT LEAN_ANGLE_MIN_TIER_GAP_DEG=3.0 + AC-PW-22b sub-patterns 7e-7h + tighten TIER3/TIER4 safe ranges to non-overlapping.
5. **B7 (Ease curve)**: ADOPT registry-canonical asset + cook-time fingerprint check + AC-PW-22b pattern 12 grep.
6. **B8 (REACTION_BUDGET / PEAK barrage adjacent-cluster exclusion)**: ADOPT authoring constraint path (a) — exclude {0,1,2}/{1,2,3}/{2,3,4} from PEAK; preserves REACTION_BUDGET=0.20 + FLOOR=0.65; new AC-PW-PEAK-NO-ADJACENT-CLUSTER.
7. **B9 (Trail ISMC separation)**: ADOPT separate trail-cube ISMC + own PerInstanceCustomData budget; declare 2 batched draw calls at PEAK (not 1).
8. **B10 (AC-PW-22a mix)**: ADOPT mechanical derivation from tuning knobs; R7-derived mix 5/10/1/0-1.
9. **B11 (visit-order strike)**: ADOPT strike-and-rely-on-pattern-11; lower risk than introducing GetIterationOrderForTest accessor.
10. **B11(F) within-barrage uniform tier**: ADOPT new Wave Spawner forward contract (g) + new AC.
11. **B12 (AC-PW-17b N upper bound)**: ADOPT N ≤ N_MAX = 375 in GIVEN.
12. **B12(F) within-barrage distinct source lanes**: ADOPT new Wave Spawner forward contract (h) + new AC.
13. **B13 (pause-flush queued)**: ADOPT explicit "queued to next tick start" in Rule 19 + new AC-PW-MID-TICK-PAUSE-DEFERRAL.

### R8 forecast (CD-derived)

**3-5 BLOCKING expected, 60% confidence.** Reasoning: R7's defect distribution clustered into 5 root-cause classes; R8 residuals likely concentrate in Cluster 2 (perceptual margins — choice-reaction re-derivation may surface follow-on margin gaps in OPENER tier-0 timing) and Cluster 3 (cross-system — trail ISMC decision may trigger ADR-class questions). Possible 1 new RC-class if integrator pass surfaces a Pause × Despawn interaction (B13 plus B4 combined). Forecast undershot R6→R7 by 6× because R6 measured local residuals; R7→R8 explicitly anticipates integration-level residuals, so systematic blind spot of prior forecast is partially corrected. 3-5 BLOCKING weighted at 60%; 6-8 weighted at 30%; ≤2 weighted at 10%; ≥9 would indicate Cluster 3 spawned ADR-class re-architecture.

### Validation criteria for R8 APPROVAL

1. Zero findings of class RC-C-residual (AC body/contract drift) — Cluster 1 success metric.
2. Every Pillar 2 perceptual claim has margin constant + registry-backed artifact — Cluster 2 success metric.
3. No invariant in GDD makes a claim about another module's draw-call topology or tick budget without cross-doc gate — Cluster 3 success metric.
4. Wave Spawner forward contracts read as a closed set (a–h, no open holes) — Cluster 4 success metric.
5. Rule 19 has explicit "fires queued at next tick start" wording + AC exercising mid-tick OnPausedChanged — Cluster 5 success metric.

### Files modified this review

- `design/gdd/systems-index.md` (Pull-Wave row R7 verdict + R7 revision-applied status appended preserving R1-R6 context; header Last Updated refreshed to 2026-06-10)
- `design/gdd/reviews/pull-wave-behavior-review-log.md` (this entry + R7 revision-pass entry below)

### Files NOT modified this review

- `docs/architecture/platform-seam-interfaces.md` (no seam doc changes required by R7 — seam doc is source-of-truth for naming; GDD aligned to seam doc, not vice versa)
- `design/gdd/player-movement.md` (PM revision via `/propagate-design-change` — independent workstream; PM forward contracts unchanged by R7)
- `design/gdd/difficulty-phase-controller.md` (DPC propagation queued for post-R8-APPROVED via `/propagate-design-change`)
- `design/gdd/run-state-machine.md` (RSM OnPausedChanged + ForceTickNow forward contracts; queue position unchanged by R7)

---

## R7 Revision Pass — 2026-06-10 — In-Session Cluster-Based Bundle (R7 findings + 4 user design decisions)

**Trigger**: R7 fresh-context re-review verdict MAJOR REVISION NEEDED (13 BLOCKING + 1 contingent / 5 RC classes / CD synthesis). User invoked `/design-review`, then selected "Revise the GDD now — cluster-based bundle (Recommended)". CD-recommended cluster strategy adopted (5 clusters disjoint enough to revise in parallel; sequential-bundle would defeat tick-level coupling).

**Strategy used**: Cluster-based revision pass. 4 design decisions grouped into a single AskUserQuestion before any edits (per skill's mid-revision-no-interrupt protocol). 9 mechanical fixes applied without user decision.

**User design decisions (2026-06-10)**:
- Q1 (B9 ISMC/TrailAlpha): chose "Separate trail ISMC + own PerInstanceCustomData budget" (Recommended)
- Q2 (B8 REACTION_BUDGET): chose "Cap PEAK barrage authoring to exclude all-adjacent triplets" (Recommended)
- Q3 (B11(F) within-barrage lean-tier spread): chose "Uniform tier within barrage" (Recommended)
- Q4 (B7 Ease curve canonicalization): chose "Registry-accessible named curve asset + cook-time fingerprint check" (Recommended)

**Blocker → fix mapping (R7 closure)**:

| R7 BLOCKER | Cluster | Fix Applied | GDD Location |
|---|---|---|---|
| **B1 (RC-C — TSortedMap escape hatch)** | 1 | Rule 14(b) TSortedMap clause RETIRED; TArray locked as sole permitted container. AC-PW-22b pattern 9 ALLOW grep `RemoveAt\b` now unambiguous. | line 88 |
| **B4 (RC-C — AC-PW-15 multi-wave GIVEN)** | 1 | AC-PW-15 GIVEN extended with R7 B4 multi-wave-same-tick despawn subcase (pause-flush ≥3 waves or two-same-target same-velocity LANDED); contiguity assertion now applied per-wave_id in the despawning set. | line 787 (AC-PW-15) |
| **B5 (RC-C — FTraverseElapsedQuery sentinel)** | 1 | Accessor sentinel changed `0.0f` → `-1.0f` for undefined-state cases (SPAWNED, LEANING, DESPAWNING); unguarded reads now see visibly-wrong negative value. | line ~751 |
| **B11 (RC-C — AC-PW-10 visit-order)** | 1 | R6 B7 admit-tick visit-order assertion struck (vacuously satisfied — SPAWNED emits zero Seam 13 events); AC-PW-22b pattern 11 static FORBID is sufficient design-layer guard. | line 779 (AC-PW-10) |
| **B12 (RC-A — AC-PW-17b N upper bound)** | 1 | GIVEN binds `N ≤ N_MAX = 375`; tolerance derivation note added (at N=375, accumulated single-precision error ~9e-5 < 1e-4 ✓; at N=1000, ~2.4e-4 > 1e-4 spurious-fail). | line ~791 (AC-PW-17b) |
| **B6 (RC-D — MIN_TIER_GAP)** | 2 | LEAN_ANGLE_MIN_TIER_GAP_DEG=3.0 added to Non-tunable invariants + registry. TIER3 safe range upper tightened 27→25; TIER4 lower tightened 25→28 (defense-in-depth for TIER3/TIER4 pair). AC-PW-22b sub-patterns 7e-7h NEW (MIN_GAP static_assert chain). | lines 543-547, line 647, lines 857-867, registry |
| **B7 (RC-D — Ease curve)** | 2 | LeanEaseCurve_Canonical registry-canonical asset added. Cook-time fingerprint check at 5 reference t values. AC-PW-22b pattern 12 NEW (FORBID inlined UCurveFloat*). | line 633, line 679, AC-PW-22b pattern 12, registry |
| **B8 (RC-D — REACTION_BUDGET)** | 2 | NEW Wave Spawner forward contract: PEAK barrage adjacent-cluster exclusion ({0,1,2}/{1,2,3}/{2,3,4}). New AC-PW-PEAK-NO-ADJACENT-CLUSTER (ADVISORY-until-WS). AC-PW-PILLAR-2-BARRAGE-SPATIAL-K K_class floor revised ≥6→≥4; K_raw 10→7. | line ~203, AC-PW-PEAK-NO-ADJACENT-CLUSTER, AC-PW-PILLAR-2-BARRAGE-SPATIAL-K |
| **B9 (RC-E — Trail ISMC)** | 3 | Trail-cube ISMC separation declared. Wave-mass ISMC (3 scalars: LeanCharge/NearMissFlash/VoxelDissolve) + trail-cube ISMC (1 scalar: TrailAlpha) = 2 batched draw calls. Trail-cube ISMC sized to 48 instances at PEAK. | lines 591-593 (G-2), line 689 (TRAVERSING motion) |
| **B10 (RC-E — AC-PW-22a mix)** | 3 | PEAK-representative mix mechanically derived: 5 LEANING / 10 TRAVERSING / 1 LANDED / 0-1 DESPAWNING (vs R1 stale 2/10/3/1; LEANING fraction at FLOOR=0.65s ≈ 29%). | line ~847 (AC-PW-22a) |
| **B11(F) (RC-F — within-barrage uniform tier)** | 4 | NEW Wave Spawner forward contract (g): all M ≥ 2 barrage members share identical lean_magnitude_tier. New AC-PW-BARRAGE-UNIFORM-TIER (ADVISORY-until-WS). | line ~205, AC-PW-BARRAGE-UNIFORM-TIER |
| **B12(F) (RC-F — within-barrage distinct source lanes)** | 4 | NEW Wave Spawner forward contract (h): all barrage members have pairwise-distinct SourceLane. New AC-PW-BARRAGE-DISTINCT-SOURCE-LANES (ADVISORY-until-WS). | line ~206, AC-PW-BARRAGE-DISTINCT-SOURCE-LANES |
| **B13 (RC-G — pause-flush queued)** | 5 | Rule 19 pause-resume flush semantic explicit: QUEUED for NEXT TICK start, NOT applied inline mid-tick. `bPauseFlushPending` flag set in delegate handler; consumed at next tick top BEFORE per-wave iteration; batch flush of all paused waves in wave_id ASC. New AC-PW-MID-TICK-PAUSE-DEFERRAL (BLOCKING). | line ~104, AC-PW-MID-TICK-PAUSE-DEFERRAL |

**Files modified this revision pass**:
- `design/gdd/pull-wave-behavior.md` (914 → ~1050+ lines; 14 substantive edits across R7 B1-B13 + header status block + AC count summary regenerated mechanically per R5 B2: 49 active / 37 BLOCKING / 12 ADVISORY)
- `design/registry/entities.yaml` (LEAN_ANGLE_MIN_TIER_GAP_DEG NEW entry; LeanEaseCurve_Canonical NEW entry; LEAN_BRIGHTNESS_PEAK_RATIO revised date refreshed; last_updated header bumped 2026-06-08 → 2026-06-10)
- `design/gdd/systems-index.md` (Pull-Wave row R7 verdict + R7 revision-applied status appended; header Last Updated refreshed to 2026-06-10)
- `design/gdd/reviews/pull-wave-behavior-review-log.md` (this entry + R7 fresh-context review verdict entry above + R7 in-session verdict entry below)
- `production/session-state/active.md` (rewritten 2026-06-10 to reflect R7 done + R8 next)

**Files NOT modified by R7 revision (deliberately deferred)**:
- `docs/architecture/platform-seam-interfaces.md` (no seam doc changes required by R7 — seam doc is source-of-truth for naming; GDD aligned to seam doc per CD adjudication chain)
- `design/gdd/player-movement.md` (PM revision via `/propagate-design-change` — independent workstream)
- `design/gdd/difficulty-phase-controller.md` (DPC propagation queued for post-R8-APPROVED)
- `design/gdd/run-state-machine.md` (RSM forward contracts queued)

**Strategic flags**:
- R7 surfaced 5 NEW root-cause classes (RC-D perceptual margin, RC-E cross-system, RC-F forward contract gaps, RC-G concurrency) beyond R6's RC-A/B/C surface. Integration-level defect class.
- Convergence trajectory: R1=23/7 → R2=18/6 → R3=12/6 → R4=6/3 → R5=1 → R6=8 (cross-doc NEW) → R7=13 (integration NEW). R6→R7 expansion driven by escalating R6 RECOMMENDEDs to R7 BLOCKING (B6, B8, B9, B11(F), B12(F)) — half of R7 BLOCKERS are R6 findings that the in-session R6 revision didn't address.
- 4 user design decisions all chose Recommended options; no specialist findings overridden.
- Trail-cube ISMC separation (B9) may trigger ADR-class question at implementation — flagged at OQ-PW-3.

**R8 strategy (carrying R7 CD-binding forward)**: `/clear` → fresh-session R8 fresh-context design-review per Phase 3b adversarial spawn pattern. R8 should retain R7's expanded review surface (integration-level + cross-document). PM revision and Telegraph prototype remain independent workstreams.

---

## Review — 2026-06-10 — In-Session R7 Revision Verdict: REVISIONS APPLIED (awaiting R8 fresh-context re-review)

Scope signal: XL (matches R7)
Specialists: (R7 7-specialist enumeration carried forward; no R8-specific spawn this entry — R7 specialists' findings drove the in-session cluster bundle)
Blocking items resolved: 13 (all R7 BLOCKERS) | User design decisions: 4 (all Recommended options chosen)
Phase 2b seam-doc grep: PASS at R7 revision-pass output (GDD aligned to seam doc names; new R7 ACs reference declared seam infrastructure correctly)

Summary: R7 in-session cluster-based bundle resolved all 13 R7 BLOCKERS across 5 root-cause classes (RC-C-resid × 4, RC-A-resid × 1, RC-D × 3, RC-E × 2 [+ 1 contingent], RC-F × 2, RC-G × 1). 4 user design decisions chose Recommended options. AC count regenerated mechanically per R5 B2 process binding: 45→49 active / 36→37 BLOCKING / 9→12 ADVISORY. Registry updated in lockstep (LEAN_ANGLE_MIN_TIER_GAP_DEG + LeanEaseCurve_Canonical added). R8 forecast 3-5 BLOCKING dominated by Cluster 2 + Cluster 3 follow-on residuals.

Prior verdict resolved: R7's 13 BLOCKERS all resolved per the blocker → fix mapping above. RC-C-residual class closed via 4 structural fixes (TSortedMap retired, multi-wave GIVEN, sentinel sentinel-change, visit-order strike). RC-A-residual closed via N upper bound. RC-D closed via 3 perceptual-margin protections (MIN_GAP, registry-canonical curve, PEAK adjacent-cluster exclusion). RC-E closed via trail ISMC separation + AC-PW-22a tuning derivation. RC-F closed via 2 new Wave Spawner forward contracts (g, h). RC-G closed via Rule 19 queued-to-next-tick + new AC.

Files modified: see "Files modified this revision pass" above.
Files NOT modified: see "Files NOT modified by R7 revision" above.

R8 strategy: `/clear` → fresh-session R8 fresh-context design-review per Phase 3b adversarial spawn pattern. PM revision and Telegraph prototype remain independent workstreams.

---

## Cross-System Coordination Entry — 2026-06-11 — Source: PM R9 cross-system survivability coordination

This is NOT a Pull-Wave design review. This is a binding cross-system coordination decision originating from the PM R9 fresh-context re-review (see `design/gdd/reviews/player-movement-review-log.md` line 106, and the full adjudication at `design/gdd/reviews/cross-system-survivability-coordination-2026-06-11.md`). Recorded here because it imposes new forward contracts on Pull-Wave that will land at the next Pull-Wave revision cycle (R8 fresh-context re-review).

**Decision**: Option (c) bound — Wave Spawner cook-time consecutive-triplet exclusion. PM and DPC GDD content unchanged.

**Forward contracts queued for Pull-Wave R8 substantive revision:**

1. **R7 B8 status promotion ADVISORY → BLOCKING.** Pull-Wave R7 already excludes the three all-consecutive M=3 triplets {0,1,2}, {1,2,3}, {2,3,4} as ADVISORY. R9 coordination promotes this to BLOCKING.
2. **New AC: AC-WS-COOK-EXCLUDE-CONSECUTIVE-M3 (BLOCKING).** Wave Spawner cook-time tooling MUST reject any `is_barrage=true` PEAK pattern where `MAX_PULLS_PER_BARRAGE = 3` AND `target_lanes` form 3 consecutive integers (`{n, n+1, n+2}` for `n ∈ {0,1,2}`). Tooling error reports the pattern name, the offending triplet, and a suggested replacement from the 7 surviving configs.
3. **F-BARRAGE-SURVIVABILITY-INVARIANT update.** Multiplicand `M` reduced from 3 to `MIN_ESCAPE_SLIPS = 2` (new registry constant — see `design/registry/entities.yaml` line ~380). Updated form: `SLIP_TWEEN_DURATION_S × MIN_ESCAPE_SLIPS + REACTION_BUDGET ≤ TELEGRAPH_WINDOW_FLOOR_S` = `0.15 × 2 + 0.20 = 0.50 ≤ 0.65` → 150ms margin at REACT default; `0.15 × 2 + 0.25 = 0.55 ≤ 0.65` → 100ms margin at REACT safe-range ceiling. Annotation required: "R7 B8 (BLOCKING) eliminates all configurations requiring 3 consecutive escape slips — worst-case escape across all 7 surviving M=3 triplets × 5 player lanes = 2 slips (creative-director enumeration)."
4. **AC-PILLAR-2-BARRAGE-SPATIAL-K re-verify.** Surviving M=3 config count drops from C(5,3)=10 to **7** after R7 B8 BLOCKING promotion. AC ≥4 floor preserved with 1.75× margin (7/4 = 1.75).
5. **Wave Spawner authoring guide.** Pull-Wave R8 should explicitly list the 7 surviving triplets as authoritative: `{0,1,3}, {0,1,4}, {0,2,3}, {0,2,4}, {0,3,4}, {1,2,4}, {1,3,4}`. Wave Spawner & Pattern Library GDD (#7 in systems-index, Not Started) inherits this list as a binding pattern-authoring constraint.

**Specialist convergence path that produced this decision:**
- game-designer recommended Option (b) (raise TELEGRAPH_WINDOW_FLOOR_S to 0.68s) — responded to **(c-naive)** where spawn-time safe-lane check would be broken by voluntary slipping
- systems-designer recommended Option (c) — empirical boundary math showed Options (a) and (b) BOTH break at REACT=0.25s registered ceiling
- creative-director synthesis enumerated all 7 surviving triplets × 5 player lanes, confirmed worst-case = 2 slips regardless of voluntary slipping (uniform safety property), adjudicated (c-actual) — pure geometric cook-time exclusion with no player-position input

**Why this decision is recorded in the Pull-Wave log**: Pull-Wave is the owner of MIN_BARRAGE_LANE_SEPARATION and the R7 B8 exclusion rule. The status promotion + new AC + F-invariant edit land in Pull-Wave's next revision pass, not in this entry.

**Registry corrections applied 2026-06-11** (immediate, in lockstep with this entry):
- `TELEGRAPH_WINDOW_FLOOR_S` reverted from speculative 0.68 → 0.65 (stale R8 "Path b" registry drift corrected)
- `BARRAGE_SIMULTANEITY_WINDOW_S` notes withdraw the 0.34s derivation under rejected Option (b); 0.325s target from R2 Cluster E preserved
- `MIN_BARRAGE_LANE_SEPARATION` notes annotated with 2026-06-11 R9 coordination resolution
- `MIN_ESCAPE_SLIPS = 2` new constant added (provisional pending Pull-Wave R8 confirmation)
- `REACTION_BUDGET` closing paragraph updated to Option (c) math

**Files modified by this cross-system entry**: `design/registry/entities.yaml`, `design/gdd/difficulty-phase-controller.md` (single resolution note on TELEGRAPH_WINDOW_FLOOR_S Tuning Knob row), `design/gdd/reviews/pull-wave-behavior-review-log.md` (this entry), `design/gdd/reviews/player-movement-review-log.md` (companion entry).

**Files NOT modified by this cross-system entry** (queued for next revision cycle):
- `design/gdd/pull-wave-behavior.md` — R8 substantive revision will apply forward contracts 1–5 above. Pull-Wave R8 fresh-context re-review remains the gate.
- `design/gdd/player-movement.md` — PM R10 structural revision will address F-6 systemic underspecification + Shipping-Safety Enforcement Policy + Hardware Contract enforcement mechanism. SLIP_TWEEN_DURATION_S = 0.15s default UNCHANGED by this coordination.
- `design/gdd/wave-spawner-pattern-library.md` — does not yet exist; this entry imposes a pre-authoring forward contract.

**Cross-system coordination ledger**: this entry is the durable record that Pull-Wave R8 must apply these 5 contracts. PM R10 author brief lives in the survivability-coordination artifact.

---

## Review — 2026-06-11 — Verdict: NEEDS REVISION (R8, fresh-context)
Scope signal: L
Specialists: game-designer, systems-designer, level-designer, ai-programmer, qa-lead, performance-analyst, art-director, creative-director (synthesis). All 7 panel specialists returned structured findings — no panel gap this round (R2 performance-analyst token-budget failure mode did not recur; structured-output cap pattern worked).
Blocking items: 11 (post-CD adjudication; raw specialist count ~22 collapses via 5 convergence patterns) | Recommended: ~18 (including 5 CD-downgraded BLOCKING from perf-analyst + 2 from art-director + 1 from ai-prog + 2 from game-d/art-d cross-system) | Nice-to-have: ~13 | Specialist disagreements: **0 substantive** (only scope-adjudication on perf-analyst 7 BLOCKING → 2 BLOCKING / 5 RECOMMENDED).
Phase 2b seam-doc grep: **PARTIAL FAILURE** — 4 active interfaces (IRSMTimeStateProvider, IPlayerMovementProvider, IWaveSpawnerCallback, ICurveProvider) all defined; IPullWaveInstanceObserver explicitly deferred per R3 (no regression); BUT AC-PW-MID-TICK-PAUSE-DEFERRAL GIVEN at GDD line ~811 cites Seam 7 "fire on iteration index N" hook that does not exist in FRSMTestStub (RC-A; 2-way convergence ai-prog B-2 + qa-lead B-2). Same paper-only-seam-field class as historical DPC R3/R5/R7 — pattern recurred at a new AC site that names a per-iteration hook rather than a missing interface, slipping the standard regex.
Prior verdict resolved: R7's 13 BLOCKERS were all resolved in-session per R7 in-session revision pass. GDD is **byte-unchanged since R7** (verified). 2026-06-11 cross-system survivability coordination resolution bound Option (c) (Wave Spawner cook-time consecutive-target-triplet exclusion + MIN_ESCAPE_SLIPS=2); registry updated in lockstep, GDD body NOT updated — the 5 forward contracts queued for R8 substantive revision remain open and form the spine of RC-B below.

Summary: R8 is **not a regression**. Trajectory R1=23/7 → R2=18/6 → R3=12/6 → R4=6/3 → R5=1 → R6=8/3 → R7 in-session → **R8=11/5** is the predictable cascade tail of the 2026-06-11 cross-system coordination, plus one genuine new design finding (RC-E ease-curve back-loading) that the M=3→M=2 transition surfaced by removing 100ms of slack. Raw specialist blockers (~22 across 7 panels) collapse via convergence patterns to 11 binding blockers in 5 root causes — closer to R4 (6/3) than R6 (8/3) in shape.

### Root cause clustering (per CD synthesis)

**RC-A — Paper-only Seam 7 hook** (1 BLOCKING, 2-way convergence)
- `[ai-prog B-2 + qa-lead B-2]` AC-PW-MID-TICK-PAUSE-DEFERRAL GIVEN (line ~811): "harness wires the trigger via Seam 7's FRSMTestStub test hook that allows pre-registered 'fire on iteration index N' injection." `FRSMTestStub` declares only `SetCurrentState`, `SetRemainingTime`, `SetRunDurationS`, `AdvanceRemainingTime`, `SetIsPaused`, `SetCurrentStateWithPauseClear`, `OnPausedChanged`. No iteration-index hook exists. Implementation-blocked.
- Fix path: either (a) declare a new test-only seam (IWaveTickIndexProvider) authored in `docs/architecture/platform-seam-interfaces.md` with concrete stub, or (b) rewrite the GIVEN to use only declared seam operations (two sequential sub-tick calls with pause toggle between them).

**RC-B — 2026-06-11 cross-system coord cascade incomplete** (5 known queued + 4 derived = 9 items; coordinated single-pass revision)
- 5 pre-known queued forward contracts (from 2026-06-11 cross-system coord entry, not re-discovered): F-BARRAGE-SURVIVABILITY-INVARIANT body M=3 → M=2; MIN_ESCAPE_SLIPS GDD=3 vs registry=2 reconcile; Wave Spawner forward contract cook-time consecutive-target-triplet exclusion clause + new AC-WS-COOK-EXCLUDE-CONSECUTIVE-M3; AC-PW-PILLAR-2-BARRAGE-SPATIAL-K re-target C(5,3)=10 → 7 surviving / 1.75× margin; R7 B8 ADVISORY → BLOCKING annotation with R7-B8 tag.
- 4 derived findings (specialists independently surfaced cascade tail):
  - `[sys-d B-1]` EC-INTER-SLIP-GATE-INTRODUCTION (line ~497) + AC-PW-32 (line ~843) both still compute at M=3 and assert "any inter-slip gate at any frame rate immediately violates." Under (c) at M=2/REACT=0.25: `0.25 + 0.30 + 2×0.017 = 0.584s ≤ 0.65s` → 66ms margin at 60fps. **Conclusion INVERTED, actively misinforms PM authors** about inter-slip gate budget. Material misinformation, not stylistic drift.
  - `[sys-d B-2]` EC-TELEGRAPH-FLOOR-LOWERED-BELOW-SURVIVABILITY-THRESHOLD (line ~499) names threshold 0.65s; correct value under (c) at REACT_CEILING=0.25 is **0.55s**. Rewrite.
  - `[sys-d B-3]` F-BARRAGE-SURVIVABILITY-INVARIANT boundary table (lines ~459–466): "default" row hides known violation at REACT=0.25 (0.70 > 0.65); M=2 row falsely labeled "requires Phase Identity revision" — false under Option (c) which keeps MAX_PULLS_PER_BARRAGE=3 but reduces escape-slip count to 2. Rewrite primary row as M=2, add REACT=0.25 verification row, downgrade M=3 rows to "pre-(c) superseded."
  - `[sys-d B-4]` AC-PW-17b N_MAX = 375 rationale claims 6.25s coverage but `375 × 0.016 = 6.0s`; max TravelDurationS = 6.25s needs N=391. Raise to 391, or drop the "matching max TravelDurationS" rationale and state 6.0s coverage explicitly.

**RC-C — K_class structural fragility** (3 BLOCKING, 3-way convergence)
- `[qa-lead B-1]` Three-site self-contradiction on K_class: AC-PW-PILLAR-2-BARRAGE-SPATIAL-K (line ~847) says floor≥4 / classes {3,4,5,6} admissible; AC-PW-PEAK-NO-ADJACENT-CLUSTER paragraph (line ~857) says ≥5 / classes 2-6 admissible (re-admits {1,2,3} that its own THEN clause excludes); line 210 forward-contract prose says "minus the subclass" (singular, implying only 1 class retired not 2). Reconcile to canonical statement across all three sites: floor ≥4 / admissible {3,4,5,6}.
- `[level-d B-1]` Class-3 pool reduces to **exactly 1 authorable barrage** under combined R7 B8 (consecutive-target exclusion) + R7 B11 (uniform-tier) + R7 B12 (distinct-source). Enumeration of class-3 sole triplet {0,2,4}: tier-0 sources {0,2,4} distinct ✓; tier-1 target 0→s=1, target 4→s=3, target 2→s∈{1,3} both collide → 0; tier-2 sources collision → 0; tier-3/4 source OOB → 0. By contrast class-6 triplet {1,3,4} yields 6 authorable barrages across tiers. Starvation concentrates on class 3. **Design decision required**: (a) relax one of R7 B8/B11/B12 on class-3 only; (b) accept singleton + ADR + min-per-class≥2 floor (which forces re-design); (c) re-derive taxonomy at M=2; (d) NUM_LANES 5→6 (largest blast radius — reopens mobile thumb-target check).
- `[level-d B-2]` K_class floor=ceiling (4=4) → AC passes vacuously with ≥1 per class. No frequency or variety protection within a class. Add binding min_patterns_per_class ≥ 2 floor — collapses with class-3 finding above.

**RC-D — OPENER tier-0 teaching contract weak** (1 collapsed item, 3-way Pillar 2)
- `[game-d B-2 + level-d B-3 + art-d R-1]` AC-PW-OPENER-TIER0-INTRO + Wave Spawner forward contract together fail Pillar 2: (a) `OPENER_TIER0_INTRO_MAX_WAVE_INDEX = 5` allows first tier-0 at wave 5 — player has inferred "rotation=telegraph" encoding from prior 4 waves; (b) no lane-placement constraint — tier-0 on lane 0 with player at lane 2 is non-threatening, scaffolding invisible; (c) tier-1 order before tier-0 unspecified. Coordinated revision: tighten max wave index to ≤3 + lane-placement constraint targeting player's spawn lane + require waves 1-2 solo tier-1 (establish the rotation-as-commitment model the tier-0 will then bound).

**RC-E — Ease-curve back-loading breaks REACTION_BUDGET assumption** (1 BLOCKING, single-source high-leverage)
- `[game-d B-1]` `LeanEaseCurve_Canonical` registry-locked control points `(0.0→0.0, 0.4→0.15, 0.7→0.7, 1.0→1.0)` defer tier-1 visible rotation past LEAN_ANGLE_MIN_TIER_GAP_DEG=3° until lean_progress≈0.414 (~0.27s at FLOOR=0.65s). Under (c) at M=2/REACT=0.20: `0.27 (onset delay) + 0.20 (REACTION_BUDGET) + 0.30 (locomotion) = 0.77s > 0.65s` — F-BARRAGE-SURVIVABILITY-INVARIANT **fails by 120ms for tier-1 patterns at telegraph floor**. F-BARRAGE-SURVIVABILITY-INVARIANT implicitly assumed REACTION_BUDGET starts at lean_progress=0; ease-curve violates that assumption — hidden by M=3's prior 100ms slack until 2026-06-11 coord removed it. Pillar 2 (read-the-storm) and Pillar 5 (skill-is-visible) both violated for tier-1 patterns. Fix: (a) move 0.4→0.15 control point forward to deliver 3° rotation by t≈0.15 (e.g. 0.15→0.20) while preserving ease-in-out feel; or (b) explicitly scope the invariant to `lean_magnitude_tier ≥ 2` (tier-1 carries no PEAK survivability pressure at 0.65s floor) and add a tier-1-specific AC. Plus: add an AC asserting `TELEGRAPH_WINDOW_FLOOR_S − t_perceptible_onset ≥ REACTION_BUDGET + SLIP_TWEEN × MIN_ESCAPE_SLIPS` for any live tier.

**Plus 2 architectural/arithmetic BLOCKERS** (rolled into R9 cascade pass)
- `[perf-analyst B-1]` Pool sizing formula (line ~597) yields zero safety margin at WAVE_DESPAWN_HOLD_S safe-range ceiling 0.5s: `16 + ceil(16 × 0.5 × 1.0) = 24` consumes entire default 24-slot pool. Replace single `+5` magic constant with explicit `MAX_CONCURRENT_WAVES_CAP_CEIL_MARGIN`; re-derive against ceiling, not default.
- `[perf-analyst B-3]` OQ-PW-3 (HISM vs ISMC architectural ambiguity) must be **promoted from open question to required ADR** before implementation. Mobile occlusion benefit on 5m-wide fixed-camera track is implausible; recommend plain ISMC unless TD identifies benefit; HISM cull-tree rebuild at 64 moving instances/tick adds 0.05-0.2ms CPU per tick on mobile with no benefit.

### CD adjudications (binding)

1. **RC-A**: Author the Seam 7 hook OR rewrite AC-PW-MID-TICK-PAUSE-DEFERRAL GIVEN to declared seam operations. Pick one in R9.
2. **RC-B**: 5 known + 4 derived = 9 textual items, treated as ONE coordinated cascade pass through Formulas → Edge Cases → boundary table → ACs, arithmetic verified per section before moving on. Single-author single-session.
3. **RC-C class-3 path**: Design decision REQUIRED up-front (options a/b/c/d above). Cannot defer — affects forward-contract authoring at Wave Spawner.
4. **RC-D**: Tighten OPENER_TIER0_INTRO_MAX_WAVE_INDEX 5→3, add lane-placement constraint targeting player spawn lane, add tier-1 ordering constraint for waves 1-2.
5. **RC-E ease-curve vs invariant**: Design decision REQUIRED up-front. (a) revise ease curve, or (b) scope invariant to tier≥2 with tier-1 AC.
6. **RC-F visual lane bleed (game-d B-3 + art-d R-2)**: DOWNGRADE to RECOMMENDED. Cannot quantify without viewing-geometry resolution (RC-G); the Pillar 5 frame is correct but the AC fix is premature.
7. **RC-G art-d B-1 viewing geometry**: KEEP BLOCKING in project scope BUT reframe as project-level CD docket, not Pull-Wave R9 blocker. ACs depending on it get `[PROVISIONAL — pending viewing geometry]` annotation. Move to project-level art-direction debt.
8. **RC-H perf-analyst 7 BLOCKING**: BIND B-1 + B-3 as BLOCKING; downgrade B-2/B-4/B-5/B-6/B-7 to RECOMMENDED with explicit forward contracts requiring perf prototype on target hardware. Promote to Alpha gates, not R9.
9. **ai-prog B-1 LEANING tick-body ordering**: DOWNGRADE to RECOMMENDED. Symmetric to F-TRAJ-TNORM read-before-increment contract; can ride that revision in R9 RC-B cascade.
10. **art-d B-2 near-miss flash Pillar 5 redundancy**: DOWNGRADE to RECOMMENDED. Joint capture argument with PM Y-dip requires PM revision composition; defer.

### Convergence patterns (specialists independently surfaced same root)

- **RC-A paper-only Seam 7 hook**: ai-prog B-2 + qa-lead B-2 (2-way)
- **RC-B M=3→M=2 cascade**: sys-d B-1/B-2/B-3 + level-d R-1 (4-site reinforcement of 5 pre-known queued contracts)
- **RC-C K_class structural fragility**: qa-lead B-1 + level-d B-1 + level-d B-2 (3-way; different angles — AC self-contradiction, class-3 enumeration, floor=ceiling)
- **RC-D OPENER tier-0**: game-d B-2 + level-d B-3 + art-d R-1 (3-way Pillar 2 teaching)
- **RC-F visual lane bleed**: game-d B-3 + art-d R-2 (2-way; CD downgraded — pending viewing geometry)
- **AC-PW-22a state-mix concerns**: perf-analyst B-2 (single-source; advisor-preseeded at R2)

### Specialist disagreements

**None substantive.** Performance-analyst returned 7 BLOCKING; CD adjudicated 2 BLOCKING + 5 RECOMMENDED. This is scope adjudication (specialists cannot see GDD-vs-prototype boundary), not a domain disagreement. The 2-way RC-A and 3-way RC-C/RC-D convergences are unanimous within their scope.

### Forward-imposed contracts on downstream GDDs (R8 additions / clarifications)

- **Wave Spawner** (NEW per RC-B): cook-time consecutive-target-triplet exclusion as BLOCKING; 7 surviving triplets {0,1,3} {0,1,4} {0,2,3} {0,2,4} {0,3,4} {1,2,4} {1,3,4} as authoritative; min-per-class≥2 floor (collapses with class-3 design decision); OPENER tier-0 lane-placement constraint (player-spawn-lane targeting); OPENER waves 1-2 solo tier-1.
- **Telegraph** (NEW per RC-E): consume revised ease curve OR document tier-1 invariant-scoping decision; AC tier-1 perceptibility window asserts `TELEGRAPH_FLOOR − t_perceptible_onset ≥ REACTION + SLIP_TWEEN × MIN_ESCAPE_SLIPS`.
- **Camera** (REITERATED per art-d B-1): FOV + spawn-plane subtended-height contract dangling 6 revisions; CD-reframed as project-level docket; ACs depending on viewing geometry get `[PROVISIONAL]` annotation in Pull-Wave R9.
- **DPC** (CONFIRMED): BARRAGE_SIMULTANEITY_WINDOW_S 0.3 vs 0.325 still awaiting DPC R7+ author confirmation; level-d N-2 (NICE-TO-HAVE) flagged for `/propagate-design-change` scope post-R9-APPROVED.

### Pre-known queued forward contracts (verified NOT applied in R8-state GDD; spine of RC-B)

These were known going into R8 per 2026-06-11 cross-system coordination entry; R8 verifies they are still queued and not yet applied:
1. F-BARRAGE-SURVIVABILITY-INVARIANT body M=3 → M=2 (verified GDD lines ~408-425, ~445 still M=3)
2. MIN_ESCAPE_SLIPS GDD=3 vs registry=2 reconcile (verified GDD line ~419 still 3, registry has 2)
3. Wave Spawner forward contract cook-time consecutive-target-triplet exclusion + AC-WS-COOK-EXCLUDE-CONSECUTIVE-M3 (verified not present)
4. AC-PW-PILLAR-2-BARRAGE-SPATIAL-K re-target C(5,3)=10 → 7 surviving / 1.75× margin (verified still references C(5,3)=10)
5. R7 B8 ADVISORY → BLOCKING annotation with R7-B8 tag (verified absent)

### Editorial defects flagged (carry to R9)

- AC count summary regeneration required after RC-B + new ACs from RC-C/RC-D/RC-E land
- F-TRAJ-LATERAL `world_x` Range column needs `(at LANE_WIDTH_M=1.0m default)` qualifier (sys-d R-1)
- LEAN_BRIGHTNESS_PEAK_RATIO formula output range declaration missing (sys-d R-3)
- WAVE_DESPAWN_HOLD_S floor 0.075s vs NEAR_MISS_FLASH=0.066s margin = 9ms (<1 frame at 120Hz); consider raising floor to 0.10s in next pass (sys-d N-1)
- PerInstanceCustomData slot count drift: G-2 says 3 scalars, line 701 prose mentions [3] — strike or clarify (perf-analyst R-1)
- BARRAGE_SIMULTANEITY_WINDOW_S 0.3 vs 0.325 DPC pending (level-d N-2)

### R9 forecast

**Best case**: 3-4 BLOCKING / 1-2 RCs (residual ease-curve arithmetic edge case + one K_class downstream surprise).
**Expected**: 5-7 BLOCKING / 2-3 RCs (RC-B cascade tail finds missed AC, RC-C decision opens new K-class question, ease-curve revision exposes tier-2 boundary).
**Worst**: 10+ BLOCKING if class-3 pool decision is "accept singleton + ADR" and that ADR conflicts with Pillar 2 — requires additional cross-system coord round.

R9 should be approval-adjacent (R5-shaped, 1-3 blockers) if the three design decisions (RC-C class-3 path, RC-E ease-curve vs invariant, RC-D OPENER constraints) are made cleanly up front.

### Strategic recommendation (R9 strategy CD-binding)

**Single-session sequential bundle.** NOT parallel cluster, NOT in-session.

R7 was in-session and the result is the byte-unchanged-since-R7 GDD R8 just reviewed — same-session bias confirmed across multiple rounds. R3 demonstrated parallel-cluster's cross-cluster cascade failure mode. The R8 finding shape is many-small-mechanical-edits-spanning-Formulas-and-ACs-simultaneously plus two design decisions (RC-C class-3, RC-E ease-curve) — needs single owner with full GDD open, decisions made up front, then top-to-bottom propagation.

**R9 sequence:**
1. **Design decisions first** (~30 min): RC-C class-3 pool path; RC-E ease-curve vs invariant; RC-D OPENER tier-0 constraint set.
2. **RC-B cascade pass** (~90 min): top-to-bottom Formulas → Edge Cases → boundary table → ACs with arithmetic verified per section before moving on.
3. **Isolated fixes** (~30 min): RC-A Seam 7 hook authorship in `docs/architecture/platform-seam-interfaces.md`; sys-d B-4 N_MAX 375→391 or rationale drop; perf-analyst B-1 pool sizing formula; OQ-PW-3 ADR resolution decision.
4. **R9 fresh-context re-review** — full specialist panel.

### Strategic flags for producer

- Pull-Wave R8 verdict NEEDS REVISION; PM R10 revision (independent workstream per session state) and Telegraph prototype both unblocked.
- 8 downstream systems still depend on Pull-Wave; R9 convergence cost matters for sprint planning. Single-session sequential bundle estimate: ~3 hours of focused work + R9 re-review.
- 2026-06-11 cross-system coord cascade is the spine of R9 (9 of 11 BLOCKERs); this is well-understood scope, not new design risk.
- RC-C class-3 design decision is the highest-stakes item — likely to surface a new ADR if option (b) [accept singleton] is chosen.
- Wave Spawner GDD authoring should NOT begin until R9 APPROVED — forward contracts are still in flux (cook-time exclusion clause + 7-surviving triplet list + min-per-class floor + OPENER constraints all land in R9).

### Files modified this review

- `design/gdd/systems-index.md` (R8 verdict appended to Pull-Wave row + Last Updated header)
- `design/gdd/reviews/pull-wave-behavior-review-log.md` (this entry)

### Files NOT modified this review (deliberately deferred to R9 single-session sequential bundle)

- `design/gdd/pull-wave-behavior.md` (revisions belong in R9 pass)
- `docs/architecture/platform-seam-interfaces.md` (Seam 7 hook authorship for AC-PW-MID-TICK-PAUSE-DEFERRAL belongs in R9 RC-A fix)
- `design/registry/entities.yaml` (no R8 registry changes; 2026-06-11 cross-system coord already updated MIN_ESCAPE_SLIPS=2)
- `design/gdd/player-movement.md` (PM R10 revision is independent workstream per session state)
- `design/gdd/difficulty-phase-controller.md` (BARRAGE_SIMULTANEITY_WINDOW_S 0.3 vs 0.325 awaits DPC author; queued for post-R9-APPROVED via `/propagate-design-change`)
- `design/gdd/run-state-machine.md` (RSM OnPausedChanged multicast + ForceTickNow forward contracts still queued)

---

## Review — 2026-06-11 — In-Session R8 Revision Verdict: REVISIONS APPLIED (awaiting R9 fresh-context re-review)

In-session execution of the R8 revision plan, **against R8 CD-binding strategic recommendation** ("NOT in-session" — per R7 same-session bias precedent). Override acknowledged eyes-open by user (`a` selection at "Apply now"); R9 fresh-context re-review carries elevated burden of independent verification given same-session-bias risk.

### User design decisions (5 of 5 Recommended options chosen)

1. **RC-C class-3 pool** → `Relax B11 for class-3 only (Recommended)`. Class-3 triplet `{0,2,4}` admits mixed-tier barrages exclusively; all other K_classes retain R7 B11 uniform-tier as BLOCKING. Add `MIN_PATTERNS_PER_K_CLASS = 2` registry constant + AC sub-checks.
2. **RC-D OPENER tier-0** → `Apply CD-recommended 3-part fix (Recommended)`. `OPENER_TIER0_INTRO_MAX_WAVE_INDEX` 5→3; new `OPENER_TIER0_INTRO_TARGET_LANE = PLAYER_SPAWN_LANE = 2`; waves 1-2 must be solo tier-1.
3. **RC-E ease-curve vs invariant** — RE-POSED after advisor flagged math dishonesty in initial framing. Re-posed honest options. User chose `Tier≥2 scope + ban tier-1 PEAK + DOCUMENT gap as prototype risk (Recommended)`. Scope F-BARRAGE-SURVIVABILITY-INVARIANT to lean_magnitude_tier ≥ 2; Wave Spawner forward contract bans tier-1 PEAK barrages; explicitly document the 75–100ms onset-delay residual gap as prototype risk in EC-EASE-CURVE-ONSET-DELAY; closure depends on direction-detection (1°) being operative perceptibility floor; AC-PW-TIER-DIRECTION-DETECTION-PROTO-GATE is the validation gate.
4. **RC-A Seam 7 hook** → `Rewrite AC GIVEN to use declared seam ops only (Recommended)`. Zero seam-doc changes; AC-PW-MID-TICK-PAUSE-DEFERRAL GIVEN restructured to use OnWaveDespawned-handler-triggered SetIsPaused (mid-tick fire via despawn-event chain on declared Seam 7 + Seam 13 ops only).

### Files modified this in-session revision

- `design/gdd/pull-wave-behavior.md`:
  - Status / Last Updated headers bumped to R8 in-session revision 2026-06-11; new R8 binding-decisions header block enumerating all 5 RCs + isolated fixes + editorial
  - **Forward contracts (Detailed Design)**:
    - OPENER tier-0 forward contract extended with R8 RC-D 3-part teaching contract
    - NEW forward contract: PEAK barrage minimum-tier exclusion (lean_magnitude_tier ≥ 2 in PEAK pool only) — R8 RC-E
    - NEW forward contract: Class-3 B11 uniform-tier relaxation (mixed-tier admitted exclusively for triplet `{0,2,4}`) — R8 RC-C
    - NEW forward contract: Minimum patterns per K_class = 2 — R8 RC-C
    - R7 B8 PEAK barrage adjacent-cluster exclusion contract extended with R9 cross-system survivability binding rationale + authoritative 7-surviving-triplet list
  - **F-BARRAGE-SURVIVABILITY-INVARIANT**:
    - Body rewritten: MIN_ESCAPE_SLIPS=2 decoupled from MAX_PULLS_PER_BARRAGE=3; scope clarification (tier ≥ 2 + direction-detection vs magnitude-discrimination perceptibility model trade)
    - Derivation paragraph rewritten with 7 surviving triplets + creative-director enumeration
    - Variables table updated (MIN_ESCAPE_SLIPS, REACTION_BUDGET safe range, FLOOR registry-locked note)
    - Verification at default REACT=0.20 (150ms margin) + REACT=0.25 ceiling (100ms margin); pre-Option-(c) state documented as superseded
    - Cross-system coordination resolution paragraph added (Option a/b/c surveys + Option c bound)
    - Boundary table rewritten: Option (c) defaults + REACT ceiling verification row + pre-Option-(c) historical rows + Option (a)(b) rejected rows + 0.16 ceiling + 0.30 REACT extreme rows
  - **Edge Cases**:
    - EC-INTER-SLIP-GATE-INTRODUCTION rewritten — conclusion INVERTED ("any gate violates" → "1-frame gate survives at 60fps + REACT=0.25 ceiling 66ms margin"); per-framerate worked examples updated; binding ceiling case = 30fps + REACT=0.25 ceiling 1-frame margin
    - EC-TELEGRAPH-FLOOR-LOWERED-BELOW-SURVIVABILITY-THRESHOLD rewritten — threshold = 0.55s under Option (c) at REACT=0.25 ceiling; old "BELOW-0.45S" and pre-Option-(c) "BELOW-0.65S" names retired
    - **NEW EC-EASE-CURVE-ONSET-DELAY** documents the R8 RC-E residual gap: under back-loaded LeanEaseCurve_Canonical, F-invariant fails for tier-1 by 120ms at telegraph floor; magnitude-discrimination model vs direction-detection model; if magnitude-discrimination operative → escalation to FLOOR raise or front-load curve or PM SLIP_TWEEN tightening
  - **Tuning Knobs**:
    - DPC inherited TELEGRAPH_WINDOW_FLOOR_S row updated (unchanged by Option (c) + residual prototype risk note)
    - DPC inherited MAX_PULLS_PER_BARRAGE row updated (unchanged by Option (c) + MIN_ESCAPE_SLIPS decoupling note)
    - DPC inherited BARRAGE_SIMULTANEITY_WINDOW_S row updated (0.325s derivation preserved; speculative 0.34s under Option (b) withdrawn)
    - Non-tunable invariants table: MIN_ESCAPE_SLIPS, MIN_PATTERNS_PER_K_CLASS, OPENER_TIER0_INTRO_MAX_WAVE_INDEX, OPENER_TIER0_INTRO_TARGET_LANE — 4 new rows added
    - F-TRAJ-LATERAL world_x range qualifier added (at LANE_WIDTH_M=1.0m default; scales with knob)
  - **Implementation Resource Budgets**:
    - Object pool sizing formula re-derived against WAVE_DESPAWN_HOLD_S safe-range ceiling 0.5s; pool_size default 24 → 29; explicit MAX_CONCURRENT_WAVES_CAP_CEIL_MARGIN constant replaces `+5` magic number (R8 perf-analyst B-1)
    - PerInstanceCustomData[3] slot-count drift corrected (wave-mass ISMC has 3 slots, not 4; TrailAlpha moved to trail-cube ISMC under R7 B9) (R8 perf-analyst R-1)
    - LEAN_BRIGHTNESS_PEAK_RATIO formula output range declared (R8 sys-d R-3)
  - **Acceptance Criteria**:
    - AC-PW-PEAK-NO-ADJACENT-CLUSTER gate ADVISORY-until-WS → BLOCKING (R9 coord promotion 2026-06-11); rationale extended with dual binding (R7 B8 Hick's Law + R9 cross-system survivability)
    - AC-PW-PILLAR-2-BARRAGE-SPATIAL-K extended from 2 to 5 sub-checks (K_class ≥ 4 + K_raw=7/1.75× margin + min_patterns_per_class ≥ 2 + class-3 mixed-tier admission + R9 cross-system survivability binding)
    - AC-PW-OPENER-TIER0-INTRO extended from 2 to 4 sub-checks (R8 RC-D 3-part teaching contract)
    - AC-PW-32 (EC-INTER-SLIP-GATE-INTRODUCTION invariant alert) rewritten — conclusion inverted with per-framerate × REACT-endpoint margin table
    - AC-PW-17b N_MAX 375 → 391 (R8 sys-d B-4 — cover max TravelDurationS=6.25s)
    - AC-PW-MID-TICK-PAUSE-DEFERRAL GIVEN fully rewritten — uses only declared Seam 7 + Seam 13 ops; OnWaveDespawned-handler-triggered SetIsPaused replaces "fire on iteration index N" hook (R8 RC-A)
    - 3 NEW ACs in R7 Forward Contract Closure section:
      - AC-PW-PEAK-BARRAGE-MIN-TIER (ADVISORY-until-WS; R8 RC-E PEAK pool tier-1 barrage exclusion)
      - AC-PW-TIER1-PERCEPTIBILITY (BLOCKING; R8 RC-E tier-1 solo survivability bound + verification table)
      - AC-PW-TIER-DIRECTION-DETECTION-PROTO-GATE (ADVISORY at story-Done, BLOCKING at Alpha; R8 RC-E perceptibility-floor prototype validation)
    - AC count summary regenerated mechanically per R5 B2: 49 → 52 active / 37 → 39 BLOCKING / 12 → 13 ADVISORY
  - **Open Questions**:
    - OQ-PW-3 PROMOTED from open question to REQUIRED ADR (HISM vs ISMC architectural decision; technical-director ownership; story-Done BLOCKING) — R8 perf-analyst B-3
    - NEW OQ-PW-EASE-CURVE-ONSET — perceptibility-floor prototype validation gate (game-designer + ux-designer + level-designer; Telegraph + Pull-Wave prototype milestone before PEAK-density content production)

### Files NOT modified this in-session revision (deliberately deferred)

- `design/registry/entities.yaml` (all R8 cross-system coordination already applied 2026-06-10; new constants MIN_PATTERNS_PER_K_CLASS / OPENER_TIER0_INTRO_MAX_WAVE_INDEX / OPENER_TIER0_INTRO_TARGET_LANE registered at Phase 5 with R9 acceptance)
- `docs/architecture/platform-seam-interfaces.md` (RC-A user decision = rewrite AC GIVEN, NOT author new seam — zero seam-doc changes needed)
- `design/gdd/difficulty-phase-controller.md` (DPC GDD unchanged; BARRAGE_SIMULTANEITY_WINDOW_S 0.325s derivation preserved at FLOOR=0.65s; 0.3 vs 0.325 author confirmation still pending; queued for `/propagate-design-change` post-R9-APPROVED)
- `design/gdd/player-movement.md` (PM R10 revision independent workstream; PM `SLIP_TWEEN = 0.15s` default unchanged by Option (c))
- `design/gdd/run-state-machine.md` (RSM OnPausedChanged multicast + ForceTickNow forward contracts still queued — RSM pre-implementation requirements; not Pull-Wave-blocking)
- `design/gdd/wave-spawner-pattern-library.md` (#7 in systems-index, Not Started — does not yet exist; R8 forward contracts continue to accrete for that GDD's pre-authoring constraints)

### R9 fresh-context re-review (next gate)

R9 must verify (1) RC-B cascade arithmetic per section (Formulas → Edge Cases → boundary table → ACs); (2) RC-A AC-PW-MID-TICK-PAUSE-DEFERRAL GIVEN rewrite is correctly grounded in declared seam ops only; (3) RC-C/D/E forward contracts are internally consistent across forward-contract block + AC sub-checks + non-tunable invariants table; (4) AC count regenerated mechanically matches body enumeration; (5) the in-session same-session-bias risk (R7 precedent) did not silently regress any prior fix. R9 forecast (in-session author estimate): 3–5 BLOCKING / 1–2 RCs (the R8 review log forecast was "approval-adjacent if design decisions land cleanly" — design decisions did land cleanly, so the upper bound of the forecast is the expected case).

The R8 strategic recommendation "single-session sequential bundle, NOT in-session" was deliberately overridden; the R9 reviewer should weight this in their re-review with extra independent verification of the F-invariant cascade arithmetic and the AC count regeneration.

---

## Review — 2026-06-12 — Verdict: MAJOR REVISION NEEDED

**Scope signal**: XL (multi-system implications; RC-D touches Wave Spawner pattern library; RC-E may cascade to DPC FLOOR raise; AC contradiction requires structural rewrite; PROTO-GATE needs methodology rebuild; Seam 13 extension required; perf pool derivation rebuild)
**Specialists**: game-designer, systems-designer, qa-lead, level-designer, performance-analyst, ux-designer, creative-director (senior synthesis)
**Blocking items**: 9 | **Recommended**: 10 | **Nice-to-have**: 2
**Prior verdict resolved**: NO — R8 in-session revision over CD-binding "NOT in-session" recommendation produced new structural defects caught at R9 fresh-context

### Phase 2b seam-doc verification

**PASS for declared interfaces, PARTIAL FAIL on stub field surface**:
- All 4 actively-referenced project interfaces (`IRSMTimeStateProvider`, `ICurveProvider`, `IPlayerMovementProvider`, `IWaveSpawnerCallback`) declared in `docs/architecture/platform-seam-interfaces.md`.
- `IPullWaveInstanceObserver` explicitly deferred per R3 (no regression).
- Seam 7 ops cited by AC-PW-MID-TICK-PAUSE-DEFERRAL R8 RC-A rewrite (`SetCurrentState`, `SetRemainingTime`, `SetIsPaused`, `SetCurrentStateWithPauseClear`, `OnPausedChanged`) all declared on `FRSMTestStub` at seam doc lines 727-820.
- **HOWEVER**: AC-PW-MID-TICK-PAUSE-DEFERRAL R8 RC-A rewrite cites a `FWaveSpawnerCallbackTestStub` `OnWaveDespawned` listener that synchronously calls `FRSMTestStub.SetIsPaused(false)` from inside the listener — this re-entrant callback mechanism requires a secondary-callback slot on `FWaveSpawnerCallbackTestStub` (`SetOnWaveDespawnedCallback(TFunction<void(...)>)` or `OnDespawnedDelegate`) that is **NOT declared in Seam 13**. The R8 RC-A rewrite eliminated the "fire on iteration index N" paper-only-seam-OP gap that R8 fresh-context surfaced, but introduced a new paper-only-seam-FIELD gap. qa-lead F1 BLOCKING.

### AC count regeneration verification

Mechanically recomputed from body section headings + per-section AC enumeration:
- Formula Correctness: 10 (BLOCKING)
- State Machine Transitions: 10 (BLOCKING; includes AC-PW-MID-TICK-PAUSE-DEFERRAL)
- Core Rule Behaviors: 3 (BLOCKING)
- Hit / Near-Miss Resolution: 4 (BLOCKING; AC-PW-23 retired)
- Rule 12 — Independence: 1 (BLOCKING)
- Rule 13 — Forced Despawn: 1 (BLOCKING)
- Edge Case Coverage: 5 BLOCKING + 2 ADVISORY = 7
- R8 Forward Contract Closure: 3 BLOCKING + 7 ADVISORY-until-WS + 1 ADVISORY-at-Alpha = 11
- Cross-System Consistency: 1 BLOCKING + 1 ADVISORY-until-WS = 2
- Performance Budget: 1 BLOCKING + 2 ADVISORY = 3

**Totals**: 39 BLOCKING + 13 ADVISORY = 52 active. **MATCHES** the AC Summary block at lines 970 / 1004. AC count mechanical regeneration verification PASSED.

### Specialists consulted (parallel, 6 panels + senior synthesis)

- **game-designer**: 4 BLOCKING (RC-D self-defeating teaching contract; RC-E line 543 self-admission; AC-PW-BARRAGE-UNIFORM-TIER vs AC-PW-PILLAR-2-BARRAGE-SPATIAL-K (iv) contradiction; cascade onset value RECOMMENDED) + 2 RECOMMENDED (Player Fantasy back-loaded curve contradiction; class-3 mixed-tier trade-off vs 3-class alternative)
- **systems-designer**: 1 BLOCKING (LeanEaseCurve_Canonical interpolation mode unspecified; cliff magnitude is interpretation-dependent — cubic ~30ms / linear 21ms margin; confirms onset value defect direction at higher severity than game-designer) + 1 RECOMMENDED (AC-PW-17b N_MAX=391 tolerance C-factor unproven). All other formula boundary math CONFIRMED CLEAN.
- **qa-lead**: 3 BLOCKING (AC-PW-MID-TICK-PAUSE-DEFERRAL paper-only-seam-FIELD gap; AC-PW-TIER1-PERCEPTIBILITY gate level mismatch; AC-PW-TIER-DIRECTION-DETECTION-PROTO-GATE p95(t_decision)≤0.15s ceiling incompatible with REACTION_BUDGET=0.20s floor) + 4 RECOMMENDED (cross-sys survivability sub-check (v) algorithm unspecified; OPENER waves 1-2 lane constraint missing; AC-PW-32 multi-frame gate math 30fps vs 60fps confusion; AC-PW-22b Pattern 11 `active_wave_list*` glob undeclared)
- **level-designer**: 2 BLOCKING ({0,2,4} has ZERO uniform-tier authorable patterns — RC-C relaxation is STRUCTURALLY MANDATORY not "convenience exception"; class-4 sits at MIN_PATTERNS floor with ZERO MARGIN) + 1 RECOMMENDED (tier-4 unreachable in PEAK barrage authoring — `LEAN_ANGLE_TIER4_DEG` inert for barrage identity) + 1 RECOMMENDED (OPENER waves 1-2 lane constraint missing — confirms game-designer F1) + 1 NICE-TO-HAVE (PLAYER_SPAWN_LANE coupling not formalized). WP3 K_class taxonomy distinctness REJECTED (class-5 vs class-6 distinction IS real).
- **performance-analyst**: 1 BLOCKING (R8 B-1 pool_size=29 OVER-DERIVED — formula middle term inert under cap-includes-LANDED semantics; correct pool = 23) + 2 RECOMMENDED (trail-cube ISMC at 48 has no DESPAWNING-overlap headroom; OQ-PW-3 ADR scope missing slot-count consistency clause). R-1 (slot-count drift correction) and R-3 (output range declaration) CONFIRMED CLEAN.
- **ux-designer**: 2 BLOCKING (AC-PW-TIER-DIRECTION-DETECTION-PROTO-GATE pass condition unmeasurable — 3 fix paths offered; OPENER 3-part teaching contract player-position coherence break — 2 fix paths offered) + 2 RECOMMENDED (back-loaded curve defers tier-3/tier-4 magnitude discrimination to last 30%; Player Fantasy prose contradicts curve) + 1 NICE-TO-HAVE (LEAN_BRIGHTNESS_BASE_LUMINANCE_FLOOR_RGB invariant not registered).

### Cross-specialist convergence

- **4-way RC-D OPENER teaching contract self-defeating**: game-designer F1 + ux-designer F2 + qa-lead F3 + level-designer F2 all independently surfaced. Strongest convergence in this review.
- **2-way AC-PW-TIER-DIRECTION-DETECTION-PROTO-GATE methodology incoherent**: qa-lead F5 + ux-designer F1.
- **2-way EC-EASE-CURVE-ONSET-DELAY onset value defect**: game-designer F2 (RECOMMENDED) escalated to BLOCKING by systems-designer F1 with the load-bearing detail that interpolation mode is unspecified.
- **2-way AC-PW-BARRAGE-UNIFORM-TIER vs RC-C class-3 contradiction**: game-designer F4 + level-designer F1B (structural escalation).
- **2-way Player Fantasy vs back-loaded curve**: game-designer F5 + ux-designer F4.

### Specialist disagreements

- **EC-EASE-CURVE-ONSET-DELAY cliff severity**: game-designer reported RECOMMENDED; systems-designer escalated to BLOCKING with interpolation-mode argument. No directional disagreement on the defect itself; severity disagreement resolved at BLOCKING per CD synthesis. **No specialist claimed a 50ms figure** — that number appears only in the GDD's own line 543 self-admission.

### Senior verdict (creative-director synthesis)

The same-session-bias risk flagged at R8 is **confirmed**, not refuted. The R7→R8→R9 trajectory establishes that successive in-session revisions on this document accumulate defects faster than they retire them. R8's encoding of the 7-surviving-triplet enumeration introduced fresh contradictions inside the same document it was attempting to make consistent (AC-907 vs sub-check (iv)); contained a self-admission of cross-system invariant failure inside the RC that was supposed to close it (line 543); and amplified rather than corrected the perf finding (pool_size=29 vs correct 23). These are diagnostic signatures of author-blindness, not random defects.

### Recommended path forward

**R10 must be a separate-session revision sequenced in 4 sub-passes**:

1. **R10a (structural — must come first)**: AC-PW-BARRAGE-UNIFORM-TIER vs sub-check (iv) class-3 carve-out (level-designer F1B + game-designer F4) + OPENER waves 1-2 target-lane constraint (game-designer F1 / ux-designer F2 path (i): constrain waves 1-2 TargetLane ≠ PLAYER_SPAWN_LANE — cheaper than cook-time player-position simulation). These are load-bearing structural fixes the rest depend on.

2. **R10b (measurement — separate session, after R10a)**: Pin `LeanEaseCurve_Canonical` interpolation mode (systems-designer F1) + apply ux-designer F1 path (c) two-alternative forced-choice paradigm to AC-PW-TIER-DIRECTION-DETECTION-PROTO-GATE + reconcile AC-PW-TIER1-PERCEPTIBILITY gate level (qa-lead F4). All three touch the same measurement/perceptibility surface.

3. **R10c (seam and perf — separate session, can run parallel to R10b authoring)**: Extend Seam 13 with `OnDespawnedUserCallback` slot + add direct-into-TRAVERSING seam op (qa-lead F1) + correct pool_size to 23 (performance-analyst F1) + fix AC-PW-32 multi-frame gate math (qa-lead F6). Infrastructure fixes that don't touch design surface.

4. **R10d (RC-E adjudication — requires creative-director trade-off review before authoring)**: Line 543 self-admission is not closable by re-wording — either the cross-system invariant must be relaxed (requires pillar trade evaluation), or the REACT budget must move, or the tier-direction-detection design must change. Defer until R10a-c land and residual surface is visible.

**R10 forecast**: 2-4 BLOCKING / 1-2 RCs at R11 fresh-context. **R10 is approval-adjacent if R10a-d each land cleanly without introducing same-session-bias regressions in their own sub-passes.**

### Files modified this review

- `design/gdd/systems-index.md` — Pull-Wave row updated with R9 MAJOR REVISION NEEDED verdict + full finding manifest + R10 4-sub-pass strategy + same-session-bias confirmation note
- `design/gdd/reviews/pull-wave-behavior-review-log.md` — this entry appended

### Files NOT modified this review (deliberately deferred to separate-session R10)

- `design/gdd/pull-wave-behavior.md` — R10 revisions belong in separate sessions per CD strategic recommendation
- `docs/architecture/platform-seam-interfaces.md` — Seam 13 OnDespawnedUserCallback extension belongs in R10c
- `design/registry/entities.yaml` — no R9 registry changes; potential R10a changes (FLOOR raise, interpolation mode) deferred until decision
- `design/gdd/{difficulty-phase-controller,player-movement,run-state-machine,wave-spawner-pattern-library}.md` — independent workstreams; Pull-Wave R9 verdict does not block them

---

## Carry-forward note — 2026-06-12 (post-R9, same day)

`/design-review design/gdd/pull-wave-behavior.md` re-invoked in a new session after the R9 entry above. Document `mtime = 2026-06-11 19:05:56` (unchanged since R8 in-session revision); seam doc and registry also unchanged since R9. **No fresh specialist pass spawned** — running the same 6-specialist panel against a byte-identical document in the same day reproduces R9's findings and risks the same-session-bias defects R9's CD synthesis explicitly warned about. R9 verdict (MAJOR REVISION NEEDED, 9/10/2) and the R10 4-sub-pass plan (R10a structural → R10b measurement → R10c seam/perf → R10d RC-E adjudication) remain the operative guidance. User elected to begin R10a in this session; same-session-bias risk acknowledged eyes-open.

---

## Carry-forward note — 2026-06-16 (post-R9, +4 days)

`/design-review design/gdd/pull-wave-behavior.md` re-invoked again, 4 days after the 2026-06-12 carry-forward note. Pull-Wave GDD `mtime = 2026-06-11 19:05:56` (still byte-identical to R9-reviewed state — R10a was NOT actually executed in the 2026-06-12 session despite the intent recorded in that carry-forward note); seam doc (mtime 2026-06-11 00:11:43) and entity registry (mtime 2026-06-11 11:13:18) likewise unchanged since R9. PM has churned heavily since R9 (R10a 2026-06-15 + R10 2026-06-15 + R11a 2026-06-16 + R11 2026-06-16 with decomposition recommended) but PM itself is at MAJOR REVISION NEEDED with structural decomposition recommended — PM does not stabilize any Pull-Wave-load-bearing contract. DPC / RSM / input-system unchanged since R9. **Skill flow deviation**: per the 2026-06-12 precedent + advisor adjudication, no fresh specialist panel spawned this session — running the same panel against byte-identical content 4 days later reproduces R9's findings at full cost. User chose `[A] Accept R9 + start R10a structural` from the surfacing widget. R10a sub-pass executed in this session per R9 CD-binding plan §R10a (lines 1469-1470 above) — see R10a entry below.

---

## R10a Structural Sub-Pass — 2026-06-16 — Verdict: R10a STRUCTURAL EDITS APPLIED (awaiting R11 fresh-context re-review after R10b + R10c + R10d also land)

Scope signal: M (focused two-edit structural pass; AC count unchanged)
Specialists: (none — CD-binding scoped sub-pass per R9 review log lines 1469-1470; R9's specialist panel adjudication is the operative authority; R10a edits are mechanically prescribed by R9 CD synthesis, not subject to design re-litigation)
Blocking items addressed: 2 R9 BLOCKING items resolved (AC-PW-BARRAGE-UNIFORM-TIER vs AC-PW-PILLAR-2-BARRAGE-SPATIAL-K sub-check (iv) contradiction — level-designer F1B + game-designer F4 convergence; AC-PW-OPENER-TIER0-INTRO sub-check (d) non-player-lane constraint missing — game-designer F1 + ux-designer F2 path (i) convergence)
AC count post-R10a: 52 active / 39 BLOCKING / 13 ADVISORY (unchanged — both edits extend existing ACs; no add/remove)

Summary: First sub-pass of R9 4-sub-pass plan executed in-session 4 days after R9 verdict, against byte-identical R9-reviewed GDD. Two edits applied per R9 CD-binding prescription. R10a is structural foundation work the rest of R10b/c/d depend on — without the class-3 carve-out and the OPENER non-player-lane constraint landed, R10b (measurement) and R10c (seam/perf) would be auditing inconsistent contracts. Same-session-bias risk is structurally lower than R7/R8 broad in-session revisions because the edits are mechanically prescribed (no design ambiguity to introduce defects in).

Prior verdict resolved: R9 NEEDS REVISION (9 BLOCKING / 6 RCs in R9 CD adjudication framing) — R10a closes 2 of the 9 R9 BLOCKING items per R9 CD-binding R10a scope (lines 1469-1470). Remaining 7 R9 BLOCKING items distribute across R10b (measurement: LeanEaseCurve_Canonical interpolation mode pin + AC-PW-TIER-DIRECTION-DETECTION-PROTO-GATE forced-choice paradigm + AC-PW-TIER1-PERCEPTIBILITY gate level reconcile = 3 items) + R10c (seam and perf: Seam 13 `OnDespawnedUserCallback` extension + pool_size 29 → 23 correction + AC-PW-32 multi-frame gate math fix = 3 items) + R10d (RC-E adjudication: F-BARRAGE-SURVIVABILITY-INVARIANT line 543 self-admission CD trade-off ruling = 1 item).

### Edits applied

1. **AC-PW-BARRAGE-UNIFORM-TIER** (line ~907 in body): added explicit `K_class == 3` (triplet `{0,2,4}`) mixed-tier admission carve-out per the R8 RC-C class-3 B11 relaxation contract. New THEN clause structure: "all members share identical `lean_magnitude_tier` (integer equality) — EXCEPT for K_class == 3 barrages (sole admissible triplet `{0, 2, 4}` per AC-PW-PILLAR-2-BARRAGE-SPATIAL-K sub-check (iv)), which MAY admit mixed-tier members…". Rationale added: structural-mandatory carve-out because under combined R7 B8 + R7 B11 + R7 B12 the only uniform-tier authorable class-3 configuration is the tier-0 triplet at `{0,2,4}`; without this carve-out class-3 collapses to K=1 patterns, failing AC-PW-PILLAR-2-BARRAGE-SPATIAL-K sub-check (iii) `MIN_PATTERNS_PER_K_CLASS = 2`. New tooling log line `BARRAGE_TIER_CLASS3_MIXED_OK` for class-3 mixed-tier admission distinguishes the class-3 carve-out path from non-class-3 violation path. Pillar 2 trade preserved (mixed-tier degrades the collective dual-channel batch-read cue for class-3 specifically; per-wave angle = magnitude encoding remains correct).

2. **AC-PW-OPENER-TIER0-INTRO sub-check (d)** (line ~913 in body): extended waves 1–2 constraint with `TargetLane != PLAYER_SPAWN_LANE` requirement. New AND-clause: "(`is_barrage == false` AND `|TargetLane - SourceLane| == 1`) AND target a lane other than the player's spawn lane (`TargetLane != PLAYER_SPAWN_LANE`)". Rationale added: without this constraint, sub-check (c) (tier-0 introducer targets PLAYER_SPAWN_LANE at wave ≤3) AND sub-check (d) (waves 1–2 tier-1) interact incoherently when waves 1–2 happen to also target PLAYER_SPAWN_LANE — player observes three consecutive waves all landing on own lane and forms useless "every wave comes at me" heuristic; the tier-0 contrast at sub-check (c) becomes unreadable. R9 ux-designer F2 path (i): cheaper-than-cook-time-player-position-simulation path (the alternative path was rejected as out of scope). New tooling log line `OPENER_WAVES_1_2_NOT_PLAYER_LANE`. Failure-line wrap-up extended to reference R9 R10a non-player-lane convergence (game-designer F1 + ux-designer F2 path (i)) in addition to the R8 RC-D 3-way Pillar 2 teaching binding.

3. **Header status block**: status string updated `R8 in-session revision — 2026-06-11 → R10a structural sub-pass — 2026-06-16`; Last Updated `2026-06-11 → 2026-06-16`; new R10a binding-decisions block appended at top of GDD header documenting both edits + AC count unchanged + same-session-bias structural-lower argument + R10b/c/d still pending.

### Files modified this pass

- `design/gdd/pull-wave-behavior.md` (header status block + 2 AC body edits; ~1023 → ~1025 lines net — both AC edits extend within their AC bullet so line growth is minimal)
- `design/gdd/reviews/pull-wave-behavior-review-log.md` (this entry)
- `design/gdd/systems-index.md` (Pull-Wave row R10a status note)
- `production/session-state/active.md` (Pull-Wave NEXT step updated from stale "R9 fresh-context re-review" → "R10b measurement sub-pass / R10c seam-and-perf sub-pass / R10d RC-E adjudication" remaining work)

### Files NOT modified this pass (deliberately deferred per R9 4-sub-pass plan)

- `docs/architecture/platform-seam-interfaces.md` — Seam 13 `OnDespawnedUserCallback` extension is R10c scope, not R10a
- `design/registry/entities.yaml` — no new shared constants surfaced by R10a (`PLAYER_SPAWN_LANE = 2` already registered under `OPENER_TIER0_INTRO_TARGET_LANE` row at line ~610)
- `design/gdd/difficulty-phase-controller.md` — no DPC propagation required by R10a edits (both are Wave Spawner forward-contract refinements; DPC propagation queued for post-R11-APPROVED)
- `design/gdd/player-movement.md` — PM is independently in MAJOR REVISION NEEDED with decomposition recommended (R11 2026-06-16); Pull-Wave R10a does not impose new PM forward contracts
- `design/gdd/wave-spawner-pattern-library.md` — does not yet exist; R10a edits accrete onto the existing pre-authoring forward-contract bundle for that GDD

### R10b/c/d remaining work (per R9 4-sub-pass plan)

- **R10b (measurement, separate session)**: pin `LeanEaseCurve_Canonical` interpolation mode (R9 systems-designer F1 — load-bearing for the EC-EASE-CURVE-ONSET-DELAY math gap magnitude; cubic ~30ms / linear 21ms margin); apply ux-designer F1 path (c) two-alternative forced-choice paradigm to AC-PW-TIER-DIRECTION-DETECTION-PROTO-GATE (R9 qa-lead F5 + ux-designer F1); reconcile AC-PW-TIER1-PERCEPTIBILITY gate level (R9 qa-lead F4).
- **R10c (seam and perf, separate session, can parallel-run with R10b)**: extend Seam 13 with `OnDespawnedUserCallback` slot for AC-PW-MID-TICK-PAUSE-DEFERRAL re-entrant callback (R9 qa-lead F1) + correct pool_size formula to 23 (R9 performance-analyst F1 — middle term inert under cap-includes-LANDED semantics) + fix AC-PW-32 multi-frame gate math 30fps vs 60fps confusion (R9 qa-lead F6).
- **R10d (RC-E adjudication, requires CD trade-off review before authoring, after R10a/b/c land)**: F-BARRAGE-SURVIVABILITY-INVARIANT line 543 self-admission ("fails by 50ms at REACT=0.25 ceiling under direction-detection model") is not closable by re-wording — either the cross-system invariant must be relaxed (requires Pillar 2/5 trade), OR REACTION_BUDGET must move (cascade), OR the tier-direction-detection design must change (perceptibility model swap). Defer until R10b residual measurement data is available.

### R11 forecast (carried from R9 lines 1477)

R11 fresh-context (after R10a + R10b + R10c + R10d all land cleanly): 2–4 BLOCKING / 1–2 RCs. Conditional on each sub-pass NOT introducing same-session-bias regressions in its own scope. R10a same-session-bias risk profile is structurally lower than R7/R8 because the edits are mechanically prescribed (zero design decisions in scope); R10b/c risk profiles are moderate (measurement and seam contracts have design surface); R10d risk profile is high (CD trade-off ruling — requires careful single-session adjudication).

### Strategic note

R10a executed against R9's prescribed structural scope without expanding scope. Per the advisor's adjudication 2026-06-16, no fresh specialist panel was spawned because R9's verdict was already on file 4 days prior and the GDD was byte-identical — the cost of re-spawning would have reproduced R9 findings at full cost without surfacing new information. The skill's "MANDATORY Phase 3b" wording assumes content has changed since last review; the 2026-06-12 carry-forward note's precedent of explicit non-spawn against byte-identical content is the operative pattern here.

---

## Carry-forward note — 2026-06-16 (post-R10a, same day, second `/design-review` invocation)

`/design-review design/gdd/pull-wave-behavior.md` re-invoked the same day after the R10a entry above. Document `mtime` now reflects the 2026-06-16 R10a edits (2 ACs extended in-place: AC-PW-BARRAGE-UNIFORM-TIER + AC-PW-OPENER-TIER0-INTRO sub-check (d)); seam doc and entity registry unchanged since R9. **Phase 2b seam-doc grep PASS**: all 4 actively-referenced project interfaces (`IRSMTimeStateProvider`, `ICurveProvider`, `IPlayerMovementProvider`, `IWaveSpawnerCallback`) present in `docs/architecture/platform-seam-interfaces.md`; `IPullWaveInstanceObserver` explicitly deferred (no regression). R9's `[paper-only-seam-FIELD]` finding on the `FWaveSpawnerCallbackTestStub` re-entrant listener slot remains operative (queued for R10c per R9 4-sub-pass plan).

**Skill flow deviation (advisor-adjudicated)**: per the 2026-06-12 + 2026-06-16 carry-forward note precedent, no fresh specialist panel spawned. The R10a edit surface is 2 mechanically-prescribed AC extensions; the rest of the GDD is byte-identical to the R9-reviewed state, so the 6-specialist panel would reproduce 7 of R9's 9 BLOCKING items and all 10 of R9's RECOMMENDED items at full token cost without surfacing new information. The advisor reasoning: R10a being on file *strengthens* the non-spawn case versus the 2026-06-16 R10a-launch invocation, not weakens it (no new design surface to review).

**Phase 3 spot-check of R10a edits**:
- AC-PW-BARRAGE-UNIFORM-TIER `K_class == 3` (triplet `{0,2,4}`) carve-out + `BARRAGE_TIER_CLASS3_MIXED_OK` tooling log line: LANDED CLEAN; chains coherently with AC-PW-PILLAR-2-BARRAGE-SPATIAL-K sub-checks (iii)+(iv); structurally-mandatory rationale captured. No new defects.
- AC-PW-OPENER-TIER0-INTRO sub-check (d) `TargetLane != PLAYER_SPAWN_LANE` clause + `OPENER_WAVES_1_2_NOT_PLAYER_LANE` tooling log line: LANDED CLEAN; restores coherence loop with sub-check (c); R9 ux-designer F2 path (i) implemented as prescribed. No new defects.

**Verdict carried forward**: MAJOR REVISION NEEDED (R9 baseline). R10a closes 2 of 9 R9 BLOCKING. **Remaining 7 R9 BLOCKING items operative**: 3 R10b (LeanEaseCurve_Canonical interpolation-mode pin; AC-PW-TIER-DIRECTION-DETECTION-PROTO-GATE 2-AFC paradigm; AC-PW-TIER1-PERCEPTIBILITY gate level reconcile) + 3 R10c (Seam 13 `OnDespawnedUserCallback` slot; pool_size 29 → 23 correction; AC-PW-32 multi-frame math fix) + 1 R10d (F-BARRAGE-SURVIVABILITY-INVARIANT line 543 self-admission CD adjudication). All 10 R9 RECOMMENDED items operative.

**R11 forecast** (carried from R9 line 1477): 2–4 BLOCKING / 1–2 RCs at R11 fresh-context if R10b/c/d each land cleanly without same-session-bias regression.

**Operative next step**: `/clear` → fresh-session R10b measurement sub-pass per R9 4-sub-pass plan §R10b (review-log lines 1469-1471). R10b same-session-bias risk profile: moderate (more design surface than R10a's mechanically-prescribed edits, less than R7/R8 broad authoring). PM decomposition + Telegraph prototype remain independent parallel tracks per active.md.

### Files modified this carry-forward pass

- `design/gdd/systems-index.md` — Pull-Wave row note prefix added documenting carry-forward verdict + R10a clean validation
- `design/gdd/reviews/pull-wave-behavior-review-log.md` — this entry appended
- `production/session-state/active.md` — (pending user-facing widget update)

### Files NOT modified this pass

- `design/gdd/pull-wave-behavior.md` — read-only carry-forward; no GDD edits
- `docs/architecture/platform-seam-interfaces.md` — Seam 13 `OnDespawnedUserCallback` extension belongs in R10c
- `design/registry/entities.yaml` — no new shared constants surfaced
- All sibling GDDs — independent workstreams

---

## R10b measurement sub-pass — 2026-06-17 — Verdict: REVISIONS APPLIED (3 R9 BLOCKING closed; awaits R11 fresh-context re-review after R10c + R10d also land)

> **R10b measurement sub-pass (2026-06-17 in-session, per R9 CD-binding 4-sub-pass plan §R10b — review-log lines 1469-1471 prescription; measurement / perceptibility surface — all 3 edits mechanically prescribed by R9 specialist findings, zero design ambiguity in scope)**

> **Authoring timeline note**: R10b authoring straddled the 2026-06-16 → 2026-06-17 calendar boundary in one continuous session that started post-`/clear` after the 2026-06-16 R10a-launch carry-forward. The GDD's R10b binding-decisions block header is timestamped 2026-06-16 (R10b prescription date + work-start date per R9 plan §R10b); this review-log entry is timestamped 2026-06-17 (entry-creation date). The registry `revised` field is timestamped 2026-06-16 (lockstep with GDD binding-decisions block). No functional implication — internal date timestamps reflect work-start; ledger-style entries reflect entry-creation.

### Closure map: 3 R9 BLOCKING items

| R9 finding | R10b closure path | GDD edit site | Closure verification |
|---|---|---|---|
| **R9 systems-designer F1** — LeanEaseCurve_Canonical interpolation mode unspecified; cubic ~30ms / linear 21ms margin (escalation of R9 game-designer F2 RECOMMENDED to BLOCKING with load-bearing interpolation-mode argument) | Pin `InterpMode = RCIM_Cubic` + `TangentMode = RCTM_Auto` on every `FRichCurveKey` (the UE UCurveFloat editor default — pin makes implicit explicit) | GDD §curve description (line ~696); EC-EASE-CURVE-ONSET-DELAY paragraph (line ~544 — interpolation qualifier added); registry `LeanEaseCurve_Canonical` entry (interpolation_mode + tangent_mode notes added; revised: 2026-06-16); cook-time fingerprint check extended with NEW check (2) iterating all 4 keys and asserting cubic + auto-tangent metadata | The pin is mechanically prescribed (not a design decision): the existing R7 B7 fingerprint target at t=0.25→0.038 is only reachable under cubic Hermite interpolation; linear between (0.0, 0.0) and (0.4, 0.15) yields 0.094 at t=0.25 — already caught by the control-point check, but the interpolation-metadata pin is defense-in-depth + clearer error message + future-proofs against UCurveFloat editor default changes |
| **R9 qa-lead F5 + ux-designer F1 path (c)** (2-way convergence) — AC-PW-TIER-DIRECTION-DETECTION-PROTO-GATE `p95(t_decision) ≤ 0.15s` mathematically incoherent with `REACTION_BUDGET = 0.20s` floor (the prior RT paradigm conflated perceptibility-onset latency AND motor reaction time — demanding `t_decision ≤ 0.15s` when motor RT alone is 200ms is logically impossible) | Replace reaction-time paradigm with **two-alternative forced-choice adaptive staircase**: 12 staircases (4 tiers × 3 testers); each trial renders stimulus for duration `t_stimulus` (the staircase variable; start 0.20s, step 0.01s, floor 0.05s, ceiling 0.30s); response collected WITHOUT time pressure; 2-down-1-up rule converges on 70.7% direction-discrimination accuracy threshold (closest standard staircase target to 75% psychophysics convention); `t_threshold(tier, tester)` = mean of last 6 reversals; pass: (a) `median(t_threshold(tier-1)) ≤ 0.15s` AND (b) cross-tier threshold monotonicity; trial budget preserved at 360 trials total (≥30 per staircase × 12 staircases = same as the prior RT design's 360 total) | GDD AC-PW-TIER-DIRECTION-DETECTION-PROTO-GATE (line ~927) — WHEN/THEN clause fully rewritten; pass conditions (a/b) updated from `p95(t_decision)` to `median(t_threshold)`; tooling log strings updated from `TIER1_p95_t_decision` to `TIER1_t_threshold_median`; evidence path updated from "t_decision histograms" to "per-staircase trajectories + last-6-reversal threshold estimates + 3-tester median"; new R10b methodology rationale paragraph at end of AC explaining why 2-AFC | The methodology shift decouples perceptibility-onset latency from motor reaction time — the resulting `t_threshold` is pure perceptibility latency, directly comparable to `t_perceptible_direction` in EC-EASE-CURVE-ONSET-DELAY. The 2-AFC paradigm is psychophysics gold standard for perceptibility-floor measurement precisely because it isolates perceptibility from motor + decision noise. The prior RT paradigm was a measurement-design defect (not just a parameter mis-choice); the methodological correction was named explicitly by R9 ux-designer F1 path (c) and implicitly demanded by R9 qa-lead F5's incoherence detection |
| **R9 qa-lead F4** — AC-PW-TIER1-PERCEPTIBILITY gate level mismatch (Gate was "BLOCKING at story-Done" but the AC's closure condition explicitly depends on AC-PW-TIER-DIRECTION-DETECTION-PROTO-GATE's empirical determination of the operative perceptibility model; PROTO-GATE is BLOCKING-at-Alpha, not story-Done; at story-Done only the arithmetic structure is verifiable, not the closure) | Reconcile to **two-phase gate** matching PROTO-GATE: ADVISORY at story-Done (cross-system arithmetic table MUST be derivable + code paths MUST handle BOTH perceptibility-model outcomes; static structure is observable at story-Done) + BLOCKING at Alpha (empirical model confirmation via PROTO-GATE; if magnitude-discrimination is operative, escalates to EC-EASE-CURVE-ONSET-DELAY resolution paths) | GDD AC-PW-TIER1-PERCEPTIBILITY (line ~918) — Gate clause rewritten with two-phase structure + reconcile rationale paragraph; AC body unchanged below the Gate header (only the Gate header is reconciled — the verification table + closure condition + cross-reference remain valid) | The reconcile is mechanically prescribed: the AC's closure condition was already PROTO-GATE-dependent; making the Gate match PROTO-GATE's two-phase gate is the cleanest alignment. No content change to the verification table or closure conditions; only the Gate-level expression changes to match the empirical dependency that was already in the AC body |

### Edit log (5 edits across GDD + registry)

1. **GDD §curve description (line ~696)** — extended canonical-asset paragraph with explicit interpolation-mode pin (RCIM_Cubic + RCTM_Auto) + rationale (linear would contradict fingerprint at t=0.25 + would shift EC-EASE-CURVE-ONSET-DELAY arithmetic by ~30ms); extended cook-time fingerprint check with NEW check (2) iterating all 4 keys and asserting cubic + auto-tangent metadata.
2. **GDD EC-EASE-CURVE-ONSET-DELAY (line ~544)** — parenthetical insert noting "(under R10b-pinned cubic Hermite interpolation with auto tangents — see registry entry + AC-PW-22b pattern 12 check (2); linear interpolation is forbidden because it would shift the perceptibility-onset arithmetic below by ~30ms and contradict the fingerprint targets)"; arithmetic body unchanged.
3. **GDD AC-PW-TIER1-PERCEPTIBILITY (line ~918)** — Gate clause rewritten from "BLOCKING at story-Done" to two-phase ADVISORY-at-story-Done + BLOCKING-at-Alpha; added Gate-level reconcile rationale paragraph (R10b — 2026-06-16, per R9 qa-lead F4); AC body unchanged below the Gate header (Classification, GIVEN, WHEN, verification table, Closure condition, cross-reference all preserved).
4. **GDD AC-PW-TIER-DIRECTION-DETECTION-PROTO-GATE (line ~927)** — WHEN/THEN fully rewritten replacing RT paradigm with 2-AFC adaptive staircase (12 staircases = 4 tiers × 3 testers; 2-down-1-up; 70.7% threshold via mean-of-last-6-reversals; pass (a) median(t_threshold(tier-1))≤0.15s + (b) cross-tier monotonicity; trial budget preserved at 360 total); tooling log strings updated; evidence file content updated from `t_decision histograms` to per-staircase trajectories + threshold estimates; new R10b methodology rationale paragraph at end explaining why 2-AFC closes qa-lead F5.
5. **Registry `LeanEaseCurve_Canonical` entry (entities.yaml lines ~713-781)** — added explicit interpolation-mode + tangent-mode pins to notes block; extended cook-time fingerprint check description with check (2); updated revised field to 2026-06-16; closing paragraph extended explaining R10b rationale + that cubic ~30ms margin is the load-bearing reference figure for EC-EASE-CURVE-ONSET-DELAY math.
6. **GDD header (line 3 Status + line 5 Last Updated + new R10b binding-decisions block)** — Status line updated to reflect R10b sub-pass DONE + R10c/d still pending; Last Updated unchanged at 2026-06-16; new R10b binding-decisions block inserted ABOVE the existing R10a block (newest-at-top header pattern) enumerating all 3 closures with full mechanical rationale.

### Phase 2b seam-doc verification (R10b — same precondition as R10a)

PASS. R10b touches zero seams: the LeanEaseCurve_Canonical pin is a registry-asset metadata pin, not a seam declaration; the 2-AFC paradigm is a test methodology rewrite, not a runtime contract; the gate-level reconcile is internal to the GDD. No `paper-only-seam-*` regressions introduced. Phase 2b grep against `docs/architecture/platform-seam-interfaces.md` unchanged from R9 carry-forward (4 active interfaces declared; `IPullWaveInstanceObserver` deferred; R10c's Seam 13 `OnDespawnedUserCallback` extension still pending).

### AC count regeneration verification

| Section | Pre-R10b | Post-R10b | Delta |
|---|---|---|---|
| Formula Correctness | 10 BLOCKING | 10 BLOCKING | 0 |
| State Machine Transitions | 10 BLOCKING | 10 BLOCKING | 0 |
| Core Rule Behaviors | 3 BLOCKING | 3 BLOCKING | 0 |
| Hit / Near-Miss Resolution | 4 BLOCKING | 4 BLOCKING | 0 |
| Rule 12 — Independence | 1 BLOCKING | 1 BLOCKING | 0 |
| Rule 13 — Forced Despawn | 1 BLOCKING | 1 BLOCKING | 0 |
| Edge Case Coverage | 5 BLOCKING + 2 ADVISORY | 5 BLOCKING + 2 ADVISORY | 0 |
| R8 Forward Contract Closure | 3 BLOCKING + 7 ADVISORY-until-WS + 1 ADVISORY-at-Alpha = 11 | 2 BLOCKING + 7 ADVISORY-until-WS + 1 ADVISORY-at-Alpha + 1 ADVISORY-at-story-Done-BLOCKING-at-Alpha = 11 | 0 net (AC-PW-TIER1-PERCEPTIBILITY shifts from BLOCKING-at-story-Done to two-phase ADVISORY-at-story-Done + BLOCKING-at-Alpha — categorized in the new ADVISORY-at-story-Done bucket at story-Done) |
| Cross-System Consistency | 1 BLOCKING + 1 ADVISORY-until-WS = 2 | 1 BLOCKING + 1 ADVISORY-until-WS = 2 | 0 |
| Performance Budget | 1 BLOCKING + 2 ADVISORY = 3 | 1 BLOCKING + 2 ADVISORY = 3 | 0 |

**Totals**: 39 → 38 BLOCKING at story-Done + 1 ADVISORY-at-story-Done-promoted-to-BLOCKING-at-Alpha = 39 BLOCKING (story-Done effective); ADVISORY 13 → 14 at story-Done. Total active 52 unchanged. **Note on categorization**: AC-PW-TIER1-PERCEPTIBILITY's two-phase gate is categorized as ADVISORY at story-Done for the AC-count summary (the BLOCKING-at-Alpha aspect is the same precondition class as AC-PW-PEAK-BARRAGE-MIN-TIER which was already ADVISORY-until-WS / will be BLOCKING once Wave Spawner is authored — both ACs declare a story-Done observable + an Alpha closure check). Net: 52 active, 38 BLOCKING-at-story-Done (down 1), 14 ADVISORY-at-story-Done (up 1). Alpha gate level (PROTO-GATE + TIER1-PERCEPTIBILITY) = 2 BLOCKING-at-Alpha (up 1 from R10a's 1).

### R10b same-session-bias risk assessment

**Structurally lower than R7/R8** because all 3 edits are mechanically prescribed by R9 specialist findings with zero design ambiguity:

- **Edit 1 (interpolation pin)**: the pinned mode (cubic) is the ONLY interpolation mode consistent with the existing R7 B7 fingerprint target at t=0.25→0.038. Linear would give 0.094 — already caught. The pin is "make implicit explicit"; the design decision was already made at R7 B7. NO design surface.
- **Edit 2 (2-AFC paradigm)**: ux-designer F1 path (c) explicitly named "two-alternative forced-choice paradigm" as the prescribed fix. Adaptive staircase + 2-down-1-up rule + last-6-reversals threshold estimate are psychophysics gold-standard defaults (not novel design). Trial budget preserved at 360 (same as RT design). NO novel design decisions; only methodology shift.
- **Edit 3 (gate reconcile)**: matching PROTO-GATE's two-phase gate is the cleanest mechanical alignment — the AC's closure condition was already PROTO-GATE-dependent. NO new design surface.

**Comparable risk profile to R10a** (also mechanically prescribed; R11 forecast unchanged). **Higher than R10c** (pure infrastructure fixes — Seam 13 OnDespawnedUserCallback declaration, pool_size arithmetic, AC-PW-32 multi-frame math; no design surface at all). **Lower than R10d** (CD trade-off ruling — high design surface around F-BARRAGE-SURVIVABILITY-INVARIANT line 543 self-admission).

**Operative risk**: an R11 fresh-context reviewer COULD find that the 2-AFC adaptive staircase methodology defaults (2-down-1-up vs other staircase rules; mean-of-last-6 vs different reversal-count averaging; starting `t_stimulus = 0.20s` vs different starting value; step size 0.01s vs different step size) are tunable design parameters the R10b author chose without explicit user input. These choices are psychophysics-convention defaults — explicit user input would normally only be sought if the user has prior experience in perceptibility testing methodology. **R11 reviewer flag for adjudication**: if any of the 2-AFC staircase defaults are non-standard for game-perceptibility testing on mobile platforms, raise as RECOMMENDED at R11; the methodology defaults are documented in the new R10b methodology rationale paragraph at the end of AC-PW-TIER-DIRECTION-DETECTION-PROTO-GATE for review.

### R11 fresh-context re-review (next gate after R10c + R10d also land)

R11 forecast remains 2-4 BLOCKING / 1-2 RCs per R9 line 1477. R10b closes 3 of 9 R9 BLOCKING (combined with R10a's 2, R10c's expected 3, R10d's expected 1, all 9 R9 BLOCKING are scoped for closure across the 4 sub-passes). R11 will validate that R10a + R10b + R10c + R10d each landed cleanly without same-session-bias regressions in their own sub-passes — R11 BLOCKING budget is reserved for cross-sub-pass coordination defects + any genuinely new findings from fresh-context re-read.

### Files modified this sub-pass

- `design/gdd/pull-wave-behavior.md` — primary target. ~1024 → ~1032 lines net (R10b adds ~1 new line for the interpolation pin paragraph extension; the AC rewrites are in-place expansions of existing AC bodies; the header R10b binding-decisions block is 1 new line of dense prose).
- `design/registry/entities.yaml` — `LeanEaseCurve_Canonical` entry extended with interpolation-mode + tangent-mode notes + revised: "2026-06-16"; header `last_updated: "2026-06-10" → "2026-06-16"` per R10b lockstep.
- `design/gdd/systems-index.md` — Pull-Wave row prefix with R10b sub-pass DONE block; header Last Updated bumped to 2026-06-17.
- `design/gdd/reviews/pull-wave-behavior-review-log.md` — this entry appended.
- `production/session-state/active.md` — R10b completion checkpoint (pending edit; see below).

### Files NOT modified this sub-pass (deliberately deferred to R10c / R10d)

- `docs/architecture/platform-seam-interfaces.md` — Seam 13 `OnDespawnedUserCallback` extension remains R10c scope (R10b touches zero seams).
- `design/gdd/difficulty-phase-controller.md` — no DPC propagation required by R10b edits (interpolation mode pin + 2-AFC paradigm + gate reconcile are all Pull-Wave-internal; DPC FLOOR/REACT/SLIP_TWEEN forward contracts unchanged).
- `design/gdd/player-movement.md` — PM is independently in MAJOR REVISION NEEDED with decomposition recommended (R11 2026-06-16); Pull-Wave R10b does not impose new PM forward contracts.
- `design/gdd/wave-spawner-pattern-library.md` — does not yet exist; R10b edits accrete onto the existing pre-authoring forward-contract bundle for that GDD (the PROTO-GATE methodology rewrite affects Telegraph System prototype harness more than Wave Spawner pattern authoring; the interpolation pin affects Wave Spawner cook-time tooling per AC-PW-22b pattern 12 check (2)).
- All sibling GDDs — independent workstreams.

### R10c / R10d remaining work (per R9 4-sub-pass plan + R10a + R10b residual)

- **R10c (seam and perf, separate session, can run parallel to ANY R10 sub-pass — pure infrastructure, no design surface)**: extend Seam 13 `FWaveSpawnerCallbackTestStub` with `OnDespawnedUserCallback` slot for AC-PW-MID-TICK-PAUSE-DEFERRAL re-entrant callback (R9 qa-lead F1) + correct `pool_size` formula to 23 (R9 performance-analyst F1 — middle term inert under cap-includes-LANDED semantics) + fix AC-PW-32 multi-frame gate math 30fps vs 60fps confusion (R9 qa-lead F6).
- **R10d (RC-E adjudication, requires CD trade-off review before authoring, after R10a/b/c land)**: F-BARRAGE-SURVIVABILITY-INVARIANT line 543 self-admission ("fails by 50ms at REACT=0.25 ceiling under direction-detection model") is not closable by re-wording — either the cross-system invariant must be relaxed (requires Pillar 2/5 trade), OR REACTION_BUDGET must move (cascade), OR the tier-direction-detection design must change (perceptibility model swap), OR FLOOR must rise (DPC cascade). Defer until R10b residual measurement data is available + R10c infrastructure fixes land cleanly.

R11 fresh-context re-review is the gate after R10c + R10d both land. R11 forecast 2-4 BLOCKING per R9 line 1477 unchanged by R10b's mechanical-prescription closures.

### Strategic note

R10b executed against R9's prescribed measurement-surface scope without expanding scope. Same precedent as R10a (advisor-adjudicated mechanical-prescription scope discipline; structural lower risk than R7/R8). The 2-AFC paradigm shift in PROTO-GATE is the largest single edit of R10b — substantial WHEN/THEN rewrite — but the methodology was explicitly named by R9 ux-designer F1 path (c); the rewrite is mechanical translation, not novel methodology design. The 2-AFC adaptive staircase defaults (2-down-1-up, mean-of-last-6, starting 0.20s, step 0.01s) are psychophysics-convention; documented in the new R10b methodology rationale paragraph at end of the AC for R11 reviewer flag-if-non-standard.

---

## R10c seam-and-perf sub-pass — 2026-06-17 — Verdict: REVISIONS APPLIED (3 R9 BLOCKING closed; awaits R11 fresh-context re-review after R10d also lands)

> **R10c seam-and-perf sub-pass (2026-06-17 in-session, per R9 CD-binding 4-sub-pass plan §R10c — review-log lines 1469-1473 prescription; pure-infrastructure surface — all 3 edits mechanically prescribed by R9 specialist findings, zero design ambiguity in scope; structurally lowest same-session-bias risk of the four sub-passes)**

### Closure map: 3 R9 BLOCKING items

| R9 finding | R10c closure path | Edit site | Closure verification |
|---|---|---|---|
| **R9 qa-lead F1** — AC-PW-MID-TICK-PAUSE-DEFERRAL R8 RC-A rewrite cites a `FWaveSpawnerCallbackTestStub.OnWaveDespawned` re-entrant listener that the stub had no declared slot to bind to (paper-only-seam-FIELD gap — the inverse of the paper-only-seam-OP gap R8 RC-A closed) | Extend Seam 13 `FWaveSpawnerCallbackTestStub` with a `SetOnDespawnedUserCallback(TFunction<void(int32, EWaveDespawnReason, int32)>)` setter + private `OnDespawnedUserCallback` member + invocation from inside `OnWaveDespawned` AFTER all standard recording (event log push, counters, `bCollisionUnregisteredBeforeBroadcast` capture) is complete + `Reset()` clears the slot. NOT on `IWaveSpawnerCallback` interface — production has no use for a re-entrant user callback. New row added to "GDD ACs Unblocked by This Stub" table for AC-PW-MID-TICK-PAUSE-DEFERRAL. GDD AC-PW-MID-TICK-PAUSE-DEFERRAL parenthetical extended to declare the R10c slot; GIVEN body updated to bind via `SetOnDespawnedUserCallback(...)` rather than the prior handwave "binds an OnWaveDespawned listener"; R8 RC-A rewrite rationale paragraph extended with the R10c amendment noting that R10c closes the paper-only-seam-FIELD gap that R9 qa-lead F1 surfaced (inverse defect class to R8's paper-only-seam-OP closure) | Seam doc edit (`docs/architecture/platform-seam-interfaces.md` Seam 13 §Test Stub block) declares the slot; GDD AC-PW-MID-TICK-PAUSE-DEFERRAL prose now binds via the declared slot in lockstep with the seam edit. AC body unchanged below the parenthetical + GIVEN — 5 assertions (i)-(v), failure modes, and pause-flush-batch contiguity all unchanged. Recording invariants are preserved across re-entrant callback because the slot fires AFTER event log push + counter updates + before-broadcast capture are finalized |
| **R9 performance-analyst F1** — R8 B-1 `pool_size = 29` OVER-DERIVED; middle term `ceil(MAX_CONCURRENT_WAVES_CAP × WAVE_DESPAWN_HOLD_S_CEIL × MAX_SPAWN_RATE_PER_S)` inert under cap-includes-LANDED semantics (Rule 12 enforces cap on LEANING + TRAVERSING + LANDED active states; LANDED-hold is in-cap, not above-cap); correct pool = 23 | Refactor §Implementation Resource Budgets §Object pool sizing formula: drop the inert middle term, introduce explicit `DESPAWNING_RETURN_LATENCY_SLOTS = 2` for above-cap DESPAWNING-state pool-return-deferral (Rule 13 step 5-6 MAY defer one frame; the 2-slot budget covers expected per-tick despawn at MAX_SPAWN_RATE_PER_S=1.0/s plus 1-slot deferred-frame overlap). Strike margin rationale item (b) "one tick of DESPAWNING + pool-return latency" from the (a)+(b)+(c) breakdown to prevent double-counting with the new explicit term; margin = (a) lifetime overrun + (c) prototype iteration headroom only. New formula: `pool_size ≥ MAX_CONCURRENT_WAVES_CAP + DESPAWNING_RETURN_LATENCY_SLOTS + MAX_CONCURRENT_WAVES_CAP_CEIL_MARGIN = 16 + 2 + 5 = 23`. R10c prose adds a traceability note that `WAVE_DESPAWN_HOLD_S_CEIL = 0.5s` no longer participates in pool sizing post-R10c (preserved in §Tuning Knobs as the per-wave LANDED-hold tuning bound for designer-error detection) | Pre-R10c: 16 + 8 + 5 = 29. Post-R10c: 16 + 2 + 5 = 23. The 6-slot reduction reflects (a) the removed inert middle term (8 slots double-counted with the cap's LANDED accounting) and (b) the new explicit 2-slot DESPAWNING-deferral term replacing the implicit margin-(b) accounting. Net pool reduction: 29 − 23 = 6 slots; net safety preserved because (a) the LANDED-hold the middle term was nominally pricing is in-cap and never demanded extra slots in the first place, and (b) the deferred-ReturnToPool latency is now explicit at 2 rather than buried in the margin |
| **R9 qa-lead F6** — AC-PW-32 "Multi-frame inter-slip gates" paragraph muddles 30fps and 60fps frame_dt: labels "a 2-frame gate at 60fps" but uses `2 × 0.033 = 0.066` (0.033 is 30fps frame_dt 1/30, not 60fps frame_dt 1/60 ≈ 0.0167); coincidentally yields a result matching the K=1 30fps ceiling case (0.616s) and hides the actual K=2/60fps margin behind the wrong number | Restate the formula as `REACT + SLIP_TWEEN × M + M × K × frame_dt` (K = inter-slip gate frames-per-slip; frame_dt per fps declared explicitly: 60fps → 0.0167s, 30fps → 0.0333s). Replace the muddled prose paragraph with a 4×3 K-vs-fps margin table covering K=1 baseline + K=2/K=3/K≥4 at both 60fps and 30fps under REACT=0.25 ceiling against FLOOR=0.65s. Update the K=1 baseline values throughout the AC to consistent 4-decimal frame_dt (0.0167 / 0.0333) for arithmetic precision (replacing pre-R10c 3-decimal 0.017 / 0.033 which introduced ≤1ms cumulative error in the per-row totals). Restate the practical binding: 30fps survives only K=1 baseline; ANY multi-frame gate fails at 30fps under REACT=0.25 ceiling. 60fps survives K=1 and K=2 with measurable margin, sits at zero margin at K=3, fails at K≥4 | Pre-R10c arithmetic in the multi-frame paragraph: "2-frame gate at 60fps → `0.25 + 0.30 + 2 × 0.033 = 0.616s`" — wrong frame_dt label. Post-R10c: K=2/60fps row uses `2 × 2 × 0.0167 = 0.0668` for the frame term → total 0.617s → 33ms margin. K=2/30fps row uses `2 × 2 × 0.0333 = 0.1332` → total 0.683s → FAILS by 33ms. K=3/60fps row uses `2 × 3 × 0.0167 = 0.1002` → total 0.650s → 0ms margin (right at FLOOR). K=3/30fps row uses `2 × 3 × 0.0333 = 0.1998` → total 0.750s → FAILS by 100ms. All cells arithmetically grounded in row's K and column's frame_dt. PM authors now have unambiguous per-fps verification cases at REACT=0.25 ceiling |

### Edit log (5 edits across GDD + seam doc)

1. **Seam doc `docs/architecture/platform-seam-interfaces.md` Seam 13 §Test Stub** — `FWaveSpawnerCallbackTestStub` extended with: (a) new `SetOnDespawnedUserCallback(TFunction<void(int32, EWaveDespawnReason, int32)>)` setter declaration + `HasOnDespawnedUserCallback()` query, (b) invocation of the slot from inside `OnWaveDespawned` AFTER standard recording (event log push, counters, `bCollisionUnregisteredBeforeBroadcast` capture), (c) Reset() clears the slot (`OnDespawnedUserCallback = nullptr`), (d) private `OnDespawnedUserCallback` member of type `TFunction<void(int32, EWaveDespawnReason, int32)>`. ~32 lines added to the stub class body. Test-only — NOT on `IWaveSpawnerCallback` interface (production class `FWaveSpawnerCallback_Production` unchanged).
2. **Seam doc §GDD ACs Unblocked by This Stub table** — new row appended for AC-PW-MID-TICK-PAUSE-DEFERRAL describing the slot's binding role.
3. **GDD AC-PW-MID-TICK-PAUSE-DEFERRAL (line ~867)** — parenthetical at AC head extended with the R10c amendment block (R10c — Pull-Wave R9 qa-lead F1 2026-06-17); GIVEN body Seam 13 declarations extended with the new slot (`+ SetOnDespawnedUserCallback(TFunction<void(int32, EWaveDespawnReason, int32)>) re-entrant user callback slot fired from inside OnWaveDespawned AFTER standard recording — R10c ext.`); test setup binding rewritten to bind via `FWaveSpawnerCallbackTestStub.SetOnDespawnedUserCallback(...)` rather than the prior handwave "binds an OnWaveDespawned listener"; added the recording-invariant-preservation note (user callback fires after event log push + counters + before-broadcast capture are finalized for wave_id 1); R8 RC-A rewrite rationale paragraph extended with R10c amendment noting that R10c closes the paper-only-seam-FIELD gap that R9 qa-lead F1 surfaced (the prior "Zero seam-doc changes required" claim is now scoped to R8 RC-A's Seam-7-op closure; R10c adds the Seam 13 stub field extension). 5 GIVEN assertions (i)-(v) unchanged below the seam-declarations clause; failure modes paragraph unchanged.
4. **GDD §Tuning Knobs §Implementation Resource Budgets §Object pool sizing (line ~641)** — formula refactored from `MAX_CONCURRENT_WAVES_CAP + ceil(cap × hold_ceil × rate) + margin = 16 + 8 + 5 = 29` to `MAX_CONCURRENT_WAVES_CAP + DESPAWNING_RETURN_LATENCY_SLOTS + MAX_CONCURRENT_WAVES_CAP_CEIL_MARGIN = 16 + 2 + 5 = 23`. R10c amendment paragraph documents: (a) why the middle term is inert under cap-includes-LANDED semantics (Rule 12 cap covers LEANING + TRAVERSING + LANDED active states); (b) the new explicit `DESPAWNING_RETURN_LATENCY_SLOTS = 2` term carries above-cap DESPAWNING-state pool-return-deferral; (c) margin rationale item (b) "one tick of DESPAWNING + pool-return latency" struck to prevent double-counting; (d) `WAVE_DESPAWN_HOLD_S_CEIL = 0.5s` no longer participates in pool sizing — preserved in §Tuning Knobs as the per-wave LANDED-hold tuning bound. Header parenthetical extended with R10c attribution.
5. **GDD AC-PW-32 (EC-INTER-SLIP-GATE-INTRODUCTION — line ~899)** — parenthetical at AC head extended with R10c amendment block (R10c — Pull-Wave R9 qa-lead F6); formula header restated as the K-frame general form `REACT + SLIP_TWEEN × M + M × K × frame_dt` with K and frame_dt declared per fps; K=1 baseline arithmetic restated with 4-decimal frame_dt (0.0167 / 0.0333) for ≤1ms-precision totals; pre-R10c "Multi-frame inter-slip gates" paragraph replaced with a 4×3 K-vs-fps margin table at REACT=0.25 ceiling (K=1 / K=2 / K=3 / K≥4 rows × 60fps / 30fps columns); practical binding restated; R10c closure rationale paragraph identifies the pre-R10c 30/60fps frame_dt confusion and shows how the table grounds each cell in its row's K and column's frame_dt.
6. **GDD header (line 3 Status + line 5 Last Updated + new R10c binding-decisions block)** — Status line updated to reflect R10c sub-pass DONE + R10d still pending; Last Updated 2026-06-16 → 2026-06-17; new R10c binding-decisions block inserted ABOVE the R10b block (newest-at-top header pattern) enumerating all 3 closures with full mechanical rationale.

### Phase 2b seam-doc verification (R10c — touches seam doc directly)

R10c is the first R10 sub-pass that touches `docs/architecture/platform-seam-interfaces.md` directly (R10a / R10b touched only the GDD + registry). Verification: the new `SetOnDespawnedUserCallback` / `HasOnDespawnedUserCallback` / private `OnDespawnedUserCallback` member additions land within Seam 13 §Test Stub (between the existing `HandleNearMiss` body and the `Despawn-pipeline accessors` block); the invocation in `OnWaveDespawned` lands after all existing recording (event log push, counters, `bCollisionUnregisteredBeforeBroadcast` capture) so recording invariants are preserved; the `Reset()` clear lands at the end of the Reset() body alongside other field clears. No new IWaveSpawnerCallback interface methods. The 4 actively-referenced project interfaces (`IRSMTimeStateProvider`, `ICurveProvider`, `IPlayerMovementProvider`, `IWaveSpawnerCallback`) remain declared and unchanged. `IPullWaveInstanceObserver` deferral preserved (no regression). R10c introduces zero new `paper-only-seam-*` gaps — every GDD seam citation post-R10c references a declared op or field in the seam doc.

### AC count regeneration verification

All 3 R10c edits are in-place extensions of existing constructs (Seam 13 stub body extension + pool sizing formula refactor + AC-PW-32 multi-frame paragraph rebuild); no AC added, no AC removed, no Gate-level shifts. AC count unchanged from R10b state: **52 active / 38 BLOCKING-at-story-Done + 14 ADVISORY-at-story-Done = 52 (with AC-PW-TIER1-PERCEPTIBILITY's two-phase gate adding +1 BLOCKING at Alpha)**. Section breakdown unchanged from R10b regeneration table.

### R10c same-session-bias risk assessment

**Structurally LOWEST of the four sub-passes** because R10c is pure-infrastructure with zero design surface:

- **Edit 1 (Seam 13 slot)**: the slot mechanics are mechanical translation of R9 qa-lead F1's prescribed contract — `TFunction<...>` member + setter + invocation site + Reset() clear. The invocation-after-recording ordering is the only design-shaped choice and it is the only safe choice (any earlier ordering would let re-entrant calls corrupt the recording invariants R9 qa-lead F1 explicitly identified as needing protection). The decision to keep the slot stub-only (NOT on the production-side interface) is the only architectural choice and it is forced by R9 qa-lead F1's framing: production has no use for a re-entrant user callback.
- **Edit 2 (pool_size refactor)**: R9 performance-analyst F1 gave the conclusion (pool = 23) and the diagnostic reasoning (middle term inert under cap-includes-LANDED). The R10c derivation (drop inert middle, introduce explicit DESPAWNING_RETURN_LATENCY_SLOTS = 2, strike margin (b) to prevent double-counting, keep margin = 5 with (a) + (c)) is the cleanest reading that yields 23 while preserving the formula's 3-term structure and making the post-cap-LANDED accounting explicit. The 2-slot DESPAWNING budget is justified by the expected per-tick despawn count at MAX_SPAWN_RATE_PER_S=1.0/s plus 1-slot deferred-frame overlap; an R11 reviewer could flag this as a design parameter requiring measurement, but it's a defensible conservative ceiling that doesn't change the binding 23-pool answer at typical tuning.
- **Edit 3 (AC-PW-32 multi-frame math)**: pure arithmetic rebuild against the R9 qa-lead F6 finding. The K-frame formula `REACT + SLIP_TWEEN × M + M × K × frame_dt` is the natural generalization of the K=1 baseline formula already in the GDD; the per-fps frame_dt values (60fps → 0.0167s, 30fps → 0.0333s) are platform constants. The 4×3 margin table is mechanical multiplication. Zero new design parameters introduced.

**Comparable risk profile to R10a + R10b** (both mechanically prescribed); **lower than R10d** (CD trade-off ruling on F-BARRAGE-SURVIVABILITY-INVARIANT line 543 self-admission — high design surface). **R11 reviewer flag (R10c)**: the only R10c parameter R11 could surface as design-shaped rather than mechanically-prescribed is `DESPAWNING_RETURN_LATENCY_SLOTS = 2`. The R10c amendment paragraph documents the derivation rationale (expected per-tick despawn + 1-slot deferred-frame overlap at MAX_SPAWN_RATE_PER_S=1.0/s) AND the prototype-measurement closure path (re-derive with measured spawn-rate envelope or sustained DESPAWNING-deferral profiling). If R11 wants a more conservative budget, raise the constant to 3-4 — the additional pool slots are cheap (1 wave-mass instance + 3 trail-cube instances per slot).

### R11 fresh-context re-review (next gate after R10d also lands)

R11 forecast remains 2-4 BLOCKING / 1-2 RCs per R9 line 1477. R10c closes 3 of 9 R9 BLOCKING (combined with R10a's 2 + R10b's 3 + R10c's 3 = 8 of 9 R9 BLOCKING closed); only R10d's 1 remains (RC-E F-BARRAGE-SURVIVABILITY-INVARIANT line 543 self-admission). R11 will validate that R10a + R10b + R10c + R10d each landed cleanly without same-session-bias regressions in their own sub-passes — R11 BLOCKING budget is reserved for cross-sub-pass coordination defects + any genuinely new findings from fresh-context re-read.

### Files modified this sub-pass

- `docs/architecture/platform-seam-interfaces.md` — Seam 13 `FWaveSpawnerCallbackTestStub` extended (~32 lines added: SetOnDespawnedUserCallback setter + HasOnDespawnedUserCallback query + invocation in OnWaveDespawned + Reset() clear + private member); §GDD ACs Unblocked by This Stub table extended (1 row added for AC-PW-MID-TICK-PAUSE-DEFERRAL).
- `design/gdd/pull-wave-behavior.md` — primary target. 3 GDD edits + 1 header binding-decisions block: (1) AC-PW-MID-TICK-PAUSE-DEFERRAL parenthetical + GIVEN seam declarations + GIVEN test setup binding + R8 RC-A rewrite rationale R10c amendment; (2) §Implementation Resource Budgets §Object pool sizing formula refactor + R10c amendment paragraph; (3) AC-PW-32 parenthetical + formula header + K=1 baseline arithmetic precision + multi-frame paragraph replaced with K×fps table + practical binding restatement + R10c closure rationale; (4) header Status + Last Updated + new R10c binding-decisions block above the R10b block.
- `design/gdd/systems-index.md` — Pull-Wave row prefix with R10c sub-pass DONE block; header Last Updated bumped to 2026-06-17.
- `design/gdd/reviews/pull-wave-behavior-review-log.md` — this entry appended.
- `production/session-state/active.md` — R10c completion checkpoint (Status block updated; Session Extract appended).

### Files NOT modified this sub-pass (deliberately deferred to R10d / out of R10c scope)

- `design/registry/entities.yaml` — no new shared constants introduced by R10c (DESPAWNING_RETURN_LATENCY_SLOTS = 2 and the corrected pool_size = 23 are PM-internal Implementation Resource Budget constants, not cross-system registry constants; same shelf as MAX_CONCURRENT_WAVES_CAP_CEIL_MARGIN which lives in the GDD §Implementation Resource Budgets section).
- `design/gdd/difficulty-phase-controller.md` — no DPC propagation required by R10c edits (seam stub extension is test-infrastructure-only; pool_size correction is PM-internal implementation budget; AC-PW-32 multi-frame math is restatement at higher precision of the same K=1 baseline budget DPC R7 forward contract already references). DPC's R7 forward-contract on REACTION_BUDGET safe range + MIN_ESCAPE_SLIPS = 2 cascade is unchanged.
- `design/gdd/player-movement.md` — PM is independently in MAJOR REVISION NEEDED with decomposition recommended (R11 2026-06-16); Pull-Wave R10c does not impose new PM forward contracts (AC-PW-32's K-frame formula and per-fps frame_dt restatement are clarifications of the existing R8 RC-B cross-system coordination, not new contracts).
- `design/gdd/wave-spawner-pattern-library.md` — does not yet exist; R10c edits accrete onto the existing pre-authoring forward-contract bundle for that GDD (the new pool_size = 23 binding is the operative implementation-budget figure Wave Spawner authors should reference; the AC-PW-32 K-frame formula is the operative inter-slip gate budget Wave Spawner pattern authors should verify against).
- All sibling GDDs — independent workstreams.

### R10d remaining work (per R9 4-sub-pass plan + R10a + R10b + R10c residual)

**R10d (RC-E adjudication, requires creative-director trade-off review before authoring, after R10a/b/c land)**: F-BARRAGE-SURVIVABILITY-INVARIANT line 543 self-admission ("fails by 50ms at REACT=0.25 ceiling under direction-detection model") is not closable by re-wording — four trade paths are on the table: (1) relax the cross-system invariant (requires Pillar 2/5 trade-off), OR (2) move REACTION_BUDGET safe range (cascade across PM + DPC), OR (3) change the tier-direction-detection design (perceptibility model swap — operative perceptibility floor would shift from direction-detection 1° to magnitude-discrimination 3°, with cascading effect on the AC-PW-TIER-DIRECTION-DETECTION-PROTO-GATE 2-AFC measurement that R10b just authored), OR (4) raise DPC FLOOR (cascade to DPC GDD R7 forward contract). R10d is the only R10 sub-pass with substantive design surface and is the only sub-pass whose same-session-bias risk profile R9 CD called out as "high — requires careful single-session adjudication."

R11 fresh-context re-review is the gate after R10d lands. R11 forecast 2-4 BLOCKING per R9 line 1477 unchanged by R10c's mechanical-prescription closures (the R10c arithmetic-table rebuild and seam-slot extension introduce no new design surface for R11 to surface BLOCKING items against).

### Strategic note

R10c executed against R9's prescribed seam-and-perf scope without expanding scope. Same precedent as R10a + R10b (advisor-adjudicated mechanical-prescription scope discipline; structurally lower risk than R7/R8). The 4×3 K-vs-fps margin table in AC-PW-32 is the largest visual change of R10c — replaces a single muddled prose sentence with a multi-row table — but the table is mechanical multiplication of the K-frame formula against per-fps frame_dt; zero new design parameters introduced. The Seam 13 stub extension follows the same "test-stub-only re-entrant callback" pattern that FRSMTestStub's `OnPausedChanged` multicast delegate established at Seam 7 R2 ext. (R2 Cluster F), so the seam pattern is precedented rather than novel. The pool_size 29 → 23 refactor is the only R10c edit that involves a small judgment call (the choice to refactor the formula's middle term rather than collapse it to a single constant) — the R10c amendment paragraph documents the derivation rationale to make the R11 reviewer's audit cheap.

R10c completes the R9 4-sub-pass plan's mechanical-prescription budget (R10a + R10b + R10c). Only R10d remains, requiring creative-director adjudication. PM decomposition + Telegraph prototype remain independent parallel tracks per active.md.

---

## R10d RC-E Adjudication Sub-Pass — 2026-06-17 — Verdict: R10d CD RULING APPLIED (9 of 9 R9 BLOCKING items closed across R10a+b+c+d; awaiting R11 fresh-context re-review)

### Context

Final R9 4-sub-pass closure. R9 line 1547 explicitly flagged R10d's same-session-bias risk as HIGH and prescribed routing through creative-director fresh-context delegation BEFORE authoring. This sub-pass complied: R10d's binding ruling was issued by a fresh-context CD invocation (zero in-session deliberation context); the in-session work was mechanical translation of CD's ruling to 14 GDD edits + 1 OQ resolution + 1 review-log entry + 1 systems-index update + 1 session-state checkpoint.

### Creative-Director Trade-off Ruling Summary

**Question presented to CD** (via fresh-context agent invocation): adjudicate F-BARRAGE-SURVIVABILITY-INVARIANT line 546 self-admission (the R9 BLOCKING surface) — under direction-detection (1°) perceptibility model at REACT=0.25 ceiling, `0.15 + 0.25 + 0.30 = 0.70s > 0.65s` FAILS by 50ms; 4 enumerated paths to closure (i) FLOOR 0.65→0.70s; (ii) front-load ease curve; (iii) tighten PM SLIP_TWEEN safe range; (iv) demonstrate sub-1° perceptibility at prototype.

**CD ruling (binding)**: PATH (i) — raise `TELEGRAPH_WINDOW_FLOOR_S` from 0.65s to 0.70s.

**CD rationale (verbatim from agent transcript)**:
- Pillar 5 binding "no deaths attributable to degraded attention" applies across full REACT safe range [0.20, 0.25]s (R8 RC-B made the range the invariant, not the centerline); Pillar 5 cannot be relaxed.
- Path (ii) front-load reverses R2 Cluster F Pillar 2 binding (windup-before-punch is the "tell" feel — reversing it re-fires Cluster F defects). High regression cost.
- Path (iii) collapses PM tuning headroom 60% on a doc currently in MAJOR REVISION with decomposition pending (active.md 2026-06-16) — setting a moving target on restructuring system trades short-term math closure for medium-term churn. Wrong tradeoff while PM is mid-rewrite.
- Path (iv) is identical in shape to R8 RC-E "document as prototype risk" outcome which R9 BLOCKING already rejected — moving the gap to a future empirical bet is not closure.
- Path (i) is GDD line 546's own endorsement as "cheapest cross-system option"; single-parameter raise on a DPC-owned, registry-backed value; FLOOR ownership cleanly seated in DPC (difficulty-phase-controller.md owns FLOOR; Pull-Wave only reads `telegraph_window_s`); preserves both PM tuning safe range and the Cluster F ease curve; ~7.7% telegraph-slowdown player-facing cost is the smallest pillar trade in the option set — Pillar 5 (survivability) survives intact at REACT ceiling with zero margin; Pillar 2 (challenge density) takes a sub-perceptible telegraph stretch at PEAK boundary; Pillars 1/3/4 untouched.

**CD ruling forward contracts implied (per Section 3 of CD transcript)**:
- DPC GDD: `TELEGRAPH_WINDOW_FLOOR_S` 0.65 → 0.70s; AC-PILLAR-2-CONCURRENT re-verify against new FLOOR; Tuning Knobs + binding-value table update.
- `design/registry/entities.yaml`: FLOOR registry-backed value update.
- Pull-Wave GDD: F-invariant boundary table re-derive; EC-EASE-CURVE-ONSET-DELAY self-admission flipped to closure under R10d FLOOR; EC-TELEGRAPH-FLOOR-LOWERED-BELOW-SURVIVABILITY-THRESHOLD cushion-margin updates; EC-INTER-SLIP-GATE-INTRODUCTION per-fps margins widened 50ms; AC-PW-32 K-frame table re-derived; AC-PW-TIER-DIRECTION-DETECTION-PROTO-GATE reworded so closure not conditioned on prototype outcome; EC-WAVE-LIFETIME-OVERRUN lifetime arithmetic update.
- Wave Spawner: pool-lifetime ceiling +50ms (informational; no Wave Spawner GDD exists yet).
- PM: NO contract change — SLIP_TWEEN safe range unchanged.

**CD R11 sequencing recommendation (Section 5 flag)**: Pull-Wave R11 + DPC mini-review should be BATCHED, not Pull-Wave-only — otherwise Pull-Wave's boundary table at FLOOR=0.70s will conflict with DPC GDD's still-stated FLOOR=0.65s and R11 will flag the cross-doc inconsistency as BLOCKING. **Adopted as binding sequencing for R11.**

### Application of the CD Ruling — 14 GDD Edits

Per established R10a/b/c pattern: CD's ruling translated to mechanical in-GDD updates. Edits applied in order of dependency:

| # | Site | Change |
|---|---|---|
| 1 | Header (lines 3-6) | Status updated; new R10d binding-decisions block inserted ABOVE existing R10c block (newest-at-top per R10b precedent); enumerates all 13 in-GDD edit sites + forward contracts + same-session-bias mitigation via CD delegation. |
| 2 | Rule 5 (line 51) | `TELEGRAPH_WINDOW_FLOOR_S = 0.65s` → `0.70s` with R10d binding annotation + propagation note. |
| 3 | PM forward contract item 2 (line 197) | Cross-system invariant updated: MIN_ESCAPE_SLIPS=2 (post-R8 RC-B) + R10d FLOOR=0.70s worked examples (was stale at M=3 + FLOOR=0.65s — R8 RC-B residual). |
| 4 | Wave Spawner forward contract — PEAK barrage min-tier (line 210) | FLOOR context updated 0.65→0.70s; tier-1 PEAK exclusion preserved as INDEPENDENT of R10d FLOOR raise (tier-1's smaller max angle reaches direction-detection too late even under R10d's wider budget). |
| 5 | Path comparison (lines 221-227) | Pre-R10d Options (a)(b)(c) path comparison retained as historical record (the Options were rejected/adopted under FLOOR=0.65s era); CD ruling path (i) supplements Option (c) by adding 50ms perceptibility-onset safety net atop Option (c)'s locomotion-only closure. |
| 6 | F-TRAVERSE-DURATION worked examples (lines 393, 397, 399) | Lifetime arithmetic updated 0.65→0.70s base + total lifetime re-derived (1.48-4.40s → 1.53-4.45s default offset; 2.15s → 2.20s example; 0.65s lean phase → 0.70s lean phase). |
| 7 | F-invariant scope-clarification paragraph (line 444) | Stale "fails by 50ms at REACT=0.25 ceiling" + "FLOOR must be raised to ≥0.82s" branches retired in favor of R10d closure under 0.70s under binding direction-detection model. |
| 8 | F-invariant body block (lines 470-489) | Tuning Knobs row TELEGRAPH_WINDOW_FLOOR_S 0.65→0.70 R10d binding; Verification subsection re-derived (REACT=0.20 margin 150→200ms; REACT=0.25 ceiling 100→150ms); Cross-system coordination resolution extended with R10d path (i) as supplemental closure adopted alongside Option (c). |
| 9 | F-invariant boundary table (lines 491-507) | All rows re-derived under FLOOR=0.70s; new rows added documenting pre-R10d state at FLOOR=0.65s for traceability; new pre-Option-(c)+R10d-FLOOR row documenting that M=3 + REACT=0.25 mathematically holds at zero margin under R10d FLOOR=0.70 BUT MIN_ESCAPE_SLIPS=2 remains the binding closure (Option (c) + AC-PW-PEAK-NO-ADJACENT-CLUSTER); Option (a)/(b) rejection rows preserved with R10d cross-references. |
| 10 | "Valid only when" caveat (line 509) | Per-fps K=1 baseline margin 67→117ms at 60fps + REACT=0.25 ceiling under R10d FLOOR=0.70s. |
| 11 | EC-WAVE-LIFETIME-OVERRUN (line 536) | Wave lifetime ceiling 6.90s → 6.95s (= 0.70 + 25/4); Wave Spawner pool-sizing forward contract +50ms tag added (informational). |
| 12 | EC-INTER-SLIP-GATE-INTRODUCTION (lines 538-549) | Per-fps K=1 baseline margins re-derived under FLOOR=0.70s: 60fps REACT=0.20 117→167ms; 30fps REACT=0.20 83→133ms; 60fps REACT=0.25 67→117ms; 30fps REACT=0.25 33→83ms. Pre-R10d "30fps + REACT=0.25 has only 1-frame margin" K=1 binding ceiling case retired; new K=2 binding ceiling case at 30fps=17ms practical-zero. |
| 13 | EC-TELEGRAPH-FLOOR-LOWERED-BELOW-SURVIVABILITY-THRESHOLD (line 551) | SURVIVABILITY threshold values 0.55s/0.50s UNCHANGED (FLOOR-independent function of SLIP_TWEEN/M/REACT); cushion margins widened 100→150ms (ceiling) + 150→200ms (default); "raising FLOOR back toward 0.65s" branch marked historical with R10d binding note "FLOOR locked at 0.70s per R10d." |
| 14 | EC-EASE-CURVE-ONSET-DELAY (line 553) | THE primary R10d edit. 4-path enumeration deleted; self-admission "FAILS at REACT=0.25 ceiling by 50ms" flipped to "holds with zero margin at REACT=0.25 under FLOOR=0.70s" (`0.15 + 0.25 + 0.30 = 0.70 ≤ 0.70s`); binding direction-detection model declared as R10d design-time choice; PROTO-GATE reframed from closure-gate to perceptibility-model confidence check; PROTO-GATE failure handling moved to Alpha-phase prototype-found defect class (NOT a design-time admitted residual gap). |
| 15 | Tuning Knobs inherited row TELEGRAPH_WINDOW_FLOOR_S (line 625) | 0.65s → 0.70s R10d binding annotation; "Residual prototype risk" language fully retired in favor of R10d closure documentation. |
| 16 | Tuning Knobs inherited row BARRAGE_SIMULTANEITY_WINDOW_S (line 627) | DPC re-evaluation target updated 0.325s → 0.35s (= TELEGRAPH_WINDOW_FLOOR_S / 2 at R10d FLOOR=0.70s). |
| 17 | Telegraph downstream dependency (line 568) | "TELEGRAPH_WINDOW_FLOOR_S = 0.65s" → "0.70s" with R10d binding annotation. |
| 18 | AC-PW-12 GIVEN (line 859) | `lean_duration_s = 0.65` → `0.70` (R10d FLOOR test input). |
| 19 | AC-PW-TIER0-LEANING GIVEN (line 874) | `LeanDurationS = 0.65` → `0.70` (R10d FLOOR test input). |
| 20 | AC-PW-32 K-frame table (lines 897-906) | 4×2 K-vs-fps table re-derived under FLOOR=0.70s: K=1 (both PASS — 30fps was 33ms, now 83ms); K=2 (both PASS — 30fps was FAIL by 33ms, now 17ms PASS practical-zero); K=3 (60fps PASS 50ms, was 0ms; 30fps still FAIL); K=4 (60fps PASS 17ms practical-zero, was FAIL; 30fps still FAIL by 117ms); ≥5 added row (60fps FAIL by ≥17ms). Practical-binding restated: 30fps K=1 + K=2 both survive but K=2 at practical-zero; 60fps K=1 through K=4 survive with K=4 at practical-zero. |
| 21 | AC-PW-PEAK-BARRAGE-MIN-TIER rationale (line 929) | Tier-1 perceptibility-onset reference at FLOOR=0.65s updated to FLOOR=0.70s; tier-1 PEAK exclusion preserved (rationale is independent of FLOOR — tier-1's 17° max angle reaches direction-detection late under R10d budget too; arithmetic: 0.21 + 0.25 + 0.30 = 0.76 > 0.70). |
| 22 | AC-PW-TIER1-PERCEPTIBILITY (lines 931-938) | Gate at story-Done remains ADVISORY (R10b state preserved); Alpha-gate label retained as BLOCKING per advisor 2026-06-17 (initial R10d draft moved to ADVISORY-at-Alpha but advisor flagged this exceeded CD ruling authorization — CD said "role downgraded to confidence check" but didn't authorize Alpha-label downgrade); R10d semantic clarification adds that BLOCKING-at-Alpha applies to must-run prototype validation rather than F-invariant closure-conditioning. Verification table re-derived under FLOOR=0.70s — magnitude-discrimination row now PASSES at REACT=0.25 ceiling with 30ms margin (was FAIL by 20ms under pre-R10d FLOOR=0.65s); direction-detection row widens to 150ms margin at ceiling (was 100ms). Closure condition rewritten: tier-1 solo survives under BOTH perceptibility models under R10d FLOOR=0.70s at design-time. |
| 23 | AC-PW-TIER-DIRECTION-DETECTION-PROTO-GATE (line 941) | Role reframed from "load-bearing closure gate" to "perceptibility-model confidence check on the binding direction-detection design decision"; Alpha-gate label retained as BLOCKING per advisor 2026-06-17 with R10d semantic clarification (BLOCKING applies to must-run prototype validation; failure triggers separate CD ruling per Alpha-phase prototype-found defect class, NOT direct F-invariant closure-block). GIVEN updated FLOOR=0.65 → 0.70s LeanDurationS rendering; R10d rationale section appended; rationale section corrected to remove "ADVISORY-at-Alpha downgrade" framing. |
| 24 | OQ-PW-EASE-CURVE-ONSET (line 1038) | OQ scope NARROWED from "what perceptibility floor is operative" + "R8 RC-E user choice document gap as prototype risk" to perceptibility-model confidence check on the R10d-binding design decision; pre-R10d framing preserved as historical for traceability; decision tree restructured: design-time closure landed via R10d, prototype outcome (a)(b)(c) tree retained but reframed as Alpha-phase post-closure confidence checks. |

**Total edits: 24 sites updated** (vs. R10d header block's "13 edits" enumeration — the header block was authored before the full edit scope was discovered; the 11 additional sites are lockstep numeric updates uncovered during the systematic FLOOR=0.65 grep sweep at the end of the edit pass).

### AC count regeneration verification (advisor-corrected 2026-06-17)

Per R5 B2 binding process: all 24 sites are in-place updates of existing constructs; no AC added, no AC removed. **Advisor-flagged correction (2026-06-17)**: R10b's reported "unchanged 52/39/13" count after AC-PW-TIER1-PERCEPTIBILITY's story-Done-gate downgrade from BLOCKING → ADVISORY was a carry-forward error (the count should have moved to 52/38/14 at story-Done at R10b); R10c carried the error forward; R10d corrects to **52 active / 38 BLOCKING / 14 ADVISORY at story-Done** per mechanical regeneration from current AC body Gate headers.

**R10d Gate-level shifts (corrected post-advisor 2026-06-17)**: NONE. Initial R10d drafting moved AC-PW-TIER1-PERCEPTIBILITY and AC-PW-TIER-DIRECTION-DETECTION-PROTO-GATE Alpha gates from BLOCKING → ADVISORY based on the design-time closure under R10d FLOOR=0.70s, but advisor correctly observed this exceeded CD's explicit ruling authorization. CD said "retained as prototype validation (we still want the 1° threshold empirically confirmed) but its closure-gating role is downgraded" — that authorizes the role shift (closure-gate → confidence check) but does NOT authorize moving the Alpha label from BLOCKING to ADVISORY. **R10d final state**: both ACs retain BLOCKING-at-Alpha labels with R10d semantic clarification — BLOCKING applies to must-run prototype validation, NOT to F-invariant closure-conditioning. Failure at Alpha triggers a separate CD ruling per the Alpha-phase prototype-found defect class noted in EC-EASE-CURVE-ONSET-DELAY, NOT a direct closure-block on R10d's design-time invariant closure.

**Net AC count at story-Done: 52 active / 38 BLOCKING / 14 ADVISORY** (R10d corrects R10b's carry-forward error of 52/39/13 → 52/38/14). **Net AC count at Alpha**: unchanged from R10b — +2 BLOCKING-at-Alpha (PROTO-GATE + TIER1-PERCEPTIBILITY both BLOCKING-at-Alpha per R10b's two-phase gate; R10d preserves the labels with semantic clarification). R10d introduces NO Alpha-gate downgrades.

### R10d same-session-bias risk assessment

**R9 line 1547 declared R10d's risk profile HIGH** — explicitly prescribing creative-director fresh-context delegation before authoring. R10d complied via the `creative-director` agent invocation pattern: CD operated outside this session's deliberation context, with a complete trade-off briefing assembled by the parent, and returned a binding ruling that was mechanically translated to GDD edits.

**Risk-mitigation evidence**:
1. **Fresh-context decision authority**: the path-(i) choice was issued by CD's fresh-context invocation, NOT by in-session author deliberation. The parent's role was scoping the question (4-path enumeration + cross-system blast radius for each + recommendation per GDD line 546's own endorsement) and applying the ruling — NOT making the design choice.
2. **Mechanical translation discipline**: the 24 in-GDD edits are numeric updates against a single binding parameter (FLOOR 0.65 → 0.70s). No new design parameters introduced; no AC added/removed; no formula structure changed. Cluster-of-edits diff is shallow.
3. **CD-prescribed R11 sequencing adopted as binding**: the CD's flagged "Pull-Wave R11 + DPC mini-review batched, not Pull-Wave-only" recommendation is propagated to active.md as the next operational step. The risk of R11 flagging cross-doc inconsistency is mitigated by sequencing.

**Comparable risk profile to R10c** (R10c was also mechanical translation of R9 specialist findings); **higher than R10a/b** in raw decision surface (R10a/b applied small-magnitude prescribed edits) but mitigated to comparable level by the CD delegation.

### R11 fresh-context re-review (next gate)

**R11 fresh-context re-review is the gate after R10d lands.** R11 forecast remains 2-4 BLOCKING / 1-2 RCs per R9 line 1477 — unchanged by R10d's mechanical-prescription closures.

**R11 BLOCKING budget reserved for**:
- Cross-sub-pass coordination defects (any disagreement between R10a/b/c/d's outputs)
- Residual stale FLOOR=0.65s references not caught by the R10d sweep (the systematic grep sweep at the end of the R10d edit pass caught all known sites — but R11's fresh-context grep is the audit)
- Cross-doc inconsistency between Pull-Wave's FLOOR=0.70s and DPC GDD's still-stated FLOOR=0.65s — **MITIGATED by CD's R11 sequencing recommendation (batched Pull-Wave R11 + DPC mini-review)** but R11 will still flag if DPC update is not also queued
- Genuinely new findings from fresh-context re-read

**R11 flagged surface from R10d (per CD Section 5)**:
1. EC-EASE-CURVE-ONSET-DELAY rewrite must cleanly remove the 4-path enumeration — verified during R10d sweep; no residual "if prototype reveals the cliff is fatal" hedging language remains.
2. 30fps K=2 17ms practical-zero warning preserved at AC-PW-32 — R11 should verify this practical-binding language is loud enough.
3. ~7.7% PEAK telegraph slowdown — flagged for art-director / game-designer feel-test awareness when PEAK is play-tested. Cosmetically subtle but documented.

### Files modified this sub-pass

- `design/gdd/pull-wave-behavior.md` — primary target. 24 edit sites (see table above) + 1 header binding-decisions block.
- `design/gdd/systems-index.md` — Pull-Wave row prefix with R10d sub-pass DONE block; header Last Updated bumped to 2026-06-17.
- `design/gdd/reviews/pull-wave-behavior-review-log.md` — this entry appended.
- `production/session-state/active.md` — R10d completion checkpoint (Status block updated; Session Extract appended).

### Files NOT modified this sub-pass (deliberately deferred to `/propagate-design-change` post-R11-APPROVED, per CD ruling Section 4)

- `design/gdd/difficulty-phase-controller.md` — DPC GDD's `TELEGRAPH_WINDOW_FLOOR_S` value flip 0.65 → 0.70s + AC-PILLAR-2-CONCURRENT re-verify + Tuning Knobs update. **Critical sequencing dependency**: must land in same R11 review cycle per CD recommendation, otherwise R11 flags cross-doc inconsistency as BLOCKING.
- `design/registry/entities.yaml` — FLOOR registry-backed value update (if FLOOR is registry-backed).
- `design/gdd/reviews/cross-system-survivability-coordination-2026-06-11.md` — R10d resolution entry to append.
- `design/gdd/player-movement.md` — PM is independently in MAJOR REVISION NEEDED with decomposition recommended (R11 2026-06-16); R10d imposes NO new PM forward contracts per CD ruling (SLIP_TWEEN safe range unchanged).
- `docs/architecture/platform-seam-interfaces.md` — R10d is pure-parameter / no seam changes.
- `design/gdd/wave-spawner-pattern-library.md` — does not yet exist; R10d's pool-lifetime ceiling +50ms tag accretes onto the existing pre-authoring forward-contract bundle.
- All sibling GDDs — independent workstreams.

### R10 sub-pass closure summary

R10a (structural) + R10b (measurement) + R10c (seam/perf) + R10d (CD adjudication) closes **9 of 9 R9 BLOCKING items** combined. R9 verdict MAJOR REVISION NEEDED is fully addressed in surface; the test is R11's fresh-context audit.

Sub-pass closure map:
- **R10a** (2 R9 BLOCKING closed): AC-PW-BARRAGE-UNIFORM-TIER class-3 carve-out; AC-PW-OPENER-TIER0-INTRO sub-check (d) non-player-lane constraint.
- **R10b** (3 R9 BLOCKING closed): LeanEaseCurve_Canonical interpolation-mode pin; AC-PW-TIER-DIRECTION-DETECTION-PROTO-GATE 2-AFC paradigm; AC-PW-TIER1-PERCEPTIBILITY gate-level reconcile.
- **R10c** (3 R9 BLOCKING closed): Seam 13 OnDespawnedUserCallback slot; pool_size 29 → 23; AC-PW-32 K-frame multi-frame math.
- **R10d** (1 R9 BLOCKING closed): F-BARRAGE-SURVIVABILITY-INVARIANT line 546 self-admission via FLOOR=0.65→0.70s CD ruling path (i).

### Strategic note

R10d's compliance with R9's CD-delegation prescription is the procedural success of this sub-pass: the binding decision was made by CD operating in fresh context, not by in-session deliberation. R9's "R10d risk profile is high" warning is materially mitigated when the path choice itself is outsourced to CD — the in-session work is reduced to scope-disciplined translation of CD's ruling.

The R10d edit count (24 sites) exceeded R10d's header-block enumeration (13 sites) because the systematic FLOOR=0.65 grep sweep at the end of the edit pass uncovered 11 additional lockstep sites (e.g., AC test-input GIVENs, Telegraph dependency annotation, Tuning Knobs inherited rows, F-TRAVERSE-DURATION worked examples). The header-block enumeration is preserved as authored; this review-log entry's edit table is the canonical full enumeration. R11 will audit against the review-log table.

R10 (all 4 sub-passes) completes the R9 fresh-context re-review's 4-sub-pass plan. R11 is the next gate. The R10 plan's success will be judged at R11: forecast 2-4 BLOCKING per R9 line 1477; will land below or at if R10d's CD-delegation discipline transfers to R11's fresh-context audit (no new BLOCKING surface from R10d's design-time closure introduction) and the CD-prescribed Pull-Wave R11 + DPC mini-review sequencing lands as recommended.

---

## R11 Fresh-Context Re-Review — 2026-06-17 — Verdict: MAJOR REVISION NEEDED

**Scope signal**: L (multi-system integration; 5+ formulas re-derived under R10d; downstream forward contracts to 6 unauthored GDDs; ADR-class methodology gap surfaced)
**Specialists**: game-designer + systems-designer + qa-lead + performance-analyst + ux-designer + creative-director (senior synthesis)
**Counts**: 8 BLOCKING / 16 RECOMMENDED / 6 NICE-TO-HAVE
**Forecast vs actual**: R9 line 1547 forecast 2-4 BLOCKING. **Actual 8 = 2× overshoot.** Not the 4-5× margin that triggered PM decomposition. Decomposition trigger ARMED at 8 BLOCKING but NOT FIRED (PM decomposed at 9).

### Context

R11 is the gate after R10 4-sub-pass plan completed. All 9/9 R9 BLOCKING items were claimed closed by R10a (structural 2026-06-16) + R10b (measurement 2026-06-16) + R10c (seam/perf 2026-06-17) + R10d (CD-ruled FLOOR raise 2026-06-17). R10d CD ruling raised `TELEGRAPH_WINDOW_FLOOR_S` from 0.65s to 0.70s via PATH (i) and explicitly mandated batched Pull-Wave R11 + DPC mini-review (Section 5 sequencing recommendation). R11 ran as the Pull-Wave half of that batch.

### Three failure modes

#### (A) R10d sweep methodology gap — 4 stale-residue lines NOT in R10d's 24-site edit table

Convergence: 4-of-4 specialists flagged lines 596 + 640 + 953 as BLOCKING. Line 408 caught alone by systems-designer.

| # | Site | Stale value | Correct value under R10d | Discovery |
|---|---|---|---|---|
| B1 | Line 408 boundary table note | "Waves survive longer than 4.35s" | `0.70 + 6.25 + 0.15 + 0.016 ≈ 7.12s` at warn boundary | **Pre-R2 residue** — derives as `0.60 + 15.0/4.0` = pre-R2 FLOOR=0.60s + default offset 15.0m (wrong offset for row which is >25.0m); survived R3/R5/R7/R9/R10a-d (5 sequential reviews). |
| B2 | Line 596 PULL_WAVE_VELOCITY_MAX_MS cook-time warning | `LeanDurationS < 0.65s` | `LeanDurationS < 0.70s` | Implementation contract — tooling author reading this writes the wrong threshold. Survivability hole: pattern with velocity > 14 m/s and LeanDurationS in [0.65s, 0.70s] passes cook-time but violates invariant at runtime. |
| B3 | Line 640 knob-interaction prose | "0.65s lean + 0.83s = 1.48s" | "0.70s + 0.83s = 1.53s" | Internal contradiction with F-TRAVERSE-DURATION line 393 which is correctly 1.53s. CD overruled performance-analyst's RECOMMENDED tag → BLOCKING majority (3-of-4 specialists agree). |
| B4 | Line 953 AC-PW-22a state-mix derivation | `TELEGRAPH_WINDOW_FLOOR_S = 0.65s` + total lifetime `2.25s` + LEANING `29%` + annotation citing "R2-path-b FLOOR raise 0.6s→0.65s" as most recent raise | Total lifetime `0.70 + 1.43 + 0.15 + 0.016 = 2.30s`; LEANING `30.4%`; annotation should cite R10d. Integer state counts 5/10/1/0-1 unchanged but derivation arithmetic violates AC's own Tuning-dependence note. |

**CD methodology diagnosis**: token-grep catches named-constant references (e.g. `TELEGRAPH_WINDOW_FLOOR_S` symbol) but misses derived arithmetic literals (e.g. `4.35`, `1.48`, `2.25` — results of arithmetic where the inputs include the floor). Line 408 contains no `FLOOR`, no `0.65`, no `0.70` — it's the result of an arithmetic and the inputs rotated three times. Pull-Wave at 1039 lines past eyeball-sweep limits. **Methodology ADR needed** for dual-grep sweep technique (named-reference + derived-literal re-evaluation against current parameter values) before next FLOOR-touching parameter change.

#### (B) Cross-doc FLOOR batch BLOCKING

CD's own R10d Section 5 sequencing recommendation was violated by running Pull-Wave R11 before propagation:

- `entities.yaml` line 150: `TELEGRAPH_WINDOW_FLOOR_S value: 0.65`
- `entities.yaml` lines 163, 233-243: rationale text references `0.55 ≤ 0.65` survivability calc + `BARRAGE_SIMULTANEITY_WINDOW_S` derives `TELEGRAPH_WINDOW_FLOOR_S / 2 = 0.325s` (should be 0.35s under R10d)
- `difficulty-phase-controller.md` lines 32-33: PEAK Phase Identity `telegraph_window_s = TELEGRAPH_WINDOW_FLOOR_S = 0.6s` (two raises behind: pre-R2 0.6 → R2 0.65 → R10d 0.70)
- `difficulty-phase-controller.md` line 478: PROVISIONAL `0.6s` + R9 coordination note pinning `0.65s` (two raises behind)
- `cross-system-survivability-coordination-2026-06-11.md` line 12: sealed verdict asserts `TELEGRAPH_WINDOW_FLOOR_S = 0.65s` — now contradicts R10d ruling

**AC-PW-22a profiling gated on registry update** — test harness pulling from `entities.yaml` constructs the wrong derivation trace.

#### (C) 3 NEW UX 2-AFC measurement-protocol gaps

Entirely missing from R9 forecast because ux-designer was not in R9 specialist set — forecast-construction error not forecast-magnitude error:

| # | Site | Gap | Required fix |
|---|---|---|---|
| B6 | AC-PW-TIER-DIRECTION-DETECTION-PROTO-GATE line 941 | 2-AFC apparatus condition unspecified — "typical mobile viewing distance" not numeric | At 30cm vs 50cm a 1° tier-1 lean subtends 5.2mm vs 8.7mm — 67% variance dominates 0.15s threshold. Required: viewing distance (35-40cm), display brightness lock (80%), ambient lighting (~500 lux), True Tone disabled. Currently not falsifiable. |
| B7 | AC-PW-TIER-DIRECTION-DETECTION-PROTO-GATE line 941 | LEFT/RIGHT trial randomization (50/50) unspecified | Standard 2-AFC requirement missing. Without 50/50 randomization, response bias inflates correct-response rate, corrupting 0.15s pass condition. |
| B8 | AC-PW-TIER-DIRECTION-DETECTION-PROTO-GATE line 941 + AC-PW-TIER1-PERCEPTIBILITY | Alpha PROTO-GATE failure has NO pre-authorized escalation path | R10d rescoped PROTO-GATE from closure-gate to confidence-check but didn't pre-author what happens if Alpha confidence check reveals magnitude-discrimination operative (30ms margin at REACT=0.25 = narrowest case). CD recommends pre-authorizing PATH (i) FLOOR raise as preferred response. |

### Specialist disagreements (CD-adjudicated)

- **Line 640 BLOCKING vs RECOMMENDED**: 3 specialists (GD/SD/UX) tagged BLOCKING; perf-analyst tagged RECOMMENDED. **CD overruled to BLOCKING** — the prose is the only human-readable pacing description tuners read first when adjusting `SPAWN_PLANE_Z_OFFSET_M × PULL_WAVE_VELOCITY_MAX_MS`; 50ms residue at fastest-tuning boundary IS implementation-affecting.
- **game-designer B5 zero-margin Pillar 5 fragility**: only game-designer raised this. **CD overruled to RECOMMENDED** — re-litigation of R10d CD adjudication without new evidence (no 60fps frame-hitch frequency data). The ≥33ms minimum-margin annotation to AC-PW-22a is genuinely worth adding as a "below this margin escalate to CD" trigger but is not BLOCKING.

### RECOMMENDED items (16)

PEAK phase identity erosion (DPC mini-review attention); Player Fantasy language drift (`game-concept.md` Core Fantasy "half-second" vs FLOOR=0.70s = 40% discrepancy; Pull-Wave Pillar 2 still claims "≥0.6s before contact"); direction-detection (1°) binding without pre-authored rollback design (overlaps with B8); PM decomposition SLIP_TWEEN pointer migration; F-invariant boundary table missing M=3 + onset-inclusive form row as "reversion trigger" reference (`0.85s > 0.70s` FAILS by 150ms); DESPAWNING_RETURN_LATENCY_SLOTS=2 pause-flush burst defense (Rule 13 atomicity guarantee); SPAWN_PLANE_Z_OFFSET_M cook-time row incomplete (no cross-reference to EC-WAVE-LIFETIME-OVERRUN 6.95s ceiling); 0.25ms AC-PW-22a budget lacks derivation; thermal budget claim missing for sustained 60s run; Tier-0 brightness-ramp perceptibility untested; 2-AFC staircase blocking order unspecified; frozen-frame vs animated-motion measurement direction undocumented; "barrage" player-facing vocabulary forward contract on HUD GDD (extends PM R11a-9); reduced-motion accommodation absent; methodology ADR for dual-grep sweep technique; game-designer B5 zero-margin annotation (downgrade per CD).

### NICE-TO-HAVE (6)

BARRAGE_SIMULTANEITY_WINDOW_S derivation target propagation (entities.yaml 0.325s → 0.35s); AC-PW-22a heading annotation stale ("R2-path-b FLOOR raise 0.6s→0.65s" one revision behind); Pool sizing formula has no explicit note about EC-WAVE-LIFETIME-OVERRUN's 6.95s ceiling impact; LEAN_BRIGHTNESS_PEAK_RATIO=1.7× no mobile bloom-path guard documented; Brightness ramp photosensitive-epilepsy profile uncharacterized (defer to Telegraph GDD); HoH audio fallback not forward-contracted (defer to Audio Controller GDD).

### CD verdict synthesis (verbatim)

> "REJECT — MAJOR REVISION NEEDED. 8 BLOCKING including 4 stale-residue lines (one pre-R2), one cross-doc batch that violates my own prior ruling, and 3 measurement-protocol gaps that make Alpha's defining gate unfalsifiable. This is not a margin-of-error miss — it's a structural failure of the R10d landing. The headline finding (line 408 pre-R2 residue surviving 5 reviews) is a methodology failure: token-grep catches named-constant references but misses derived arithmetic literals."

### CD routing recommendation (binding for R11a)

**Split R11a into two parallel workstreams**:

1. **R11a-arithmetic** (fresh-context, same author): scoped to the 4 stale-residue lines (408, 596, 640, 953) + cross-doc batch (entities.yaml FLOOR + BARRAGE_SIMULTANEITY_WINDOW_S derivation + DPC GDD lines 32-33 + line 478 + survivability-coordination-2026-06-11.md R10d resolution append + Pull-Wave Pillar 2 language "≥0.6s" two raises behind + game-concept.md Core Fantasy "half-second" 40% drift) + methodology ADR for dual-grep sweep. Fresh context required — in-session is contaminated by R10d edit history and will re-make the same blind spots.
2. **R11a-measurement** (route to ux-designer fresh-context): authors the 2-AFC measurement protocol section into the GDD addressing B6 apparatus (35-40cm distance, 80% brightness, ~500 lux ambient, True Tone disabled), B7 LEFT/RIGHT randomization (50/50 with blocking specification), and B8 Alpha PROTO-GATE escalation path. Measurement-spec authoring, not stale-residue cleanup — different skill, different specialist, must be parallel not serial.

**Batched with DPC mini-review per CD R10d Section 5**: DPC mini-review folds into R11a-arithmetic's cross-doc batch — same fresh context, same FLOOR=0.70 propagation pass, single coherent edit set.

**Decomposition trigger ARMED but NOT FIRED** — PM decomposed at 9 BLOCKING; Pull-Wave at 8 = one below trigger. CD recommends drafting decomposition skeleton during R11a (likely split: telegraph/lean mechanics + F-invariant/survivability + spawning/pooling + AC/measurement protocol) gated on R12 outcome:
- If R12 returns ≤3 BLOCKING: hold decomposition; Pull-Wave stays monolithic through Alpha
- If R12 returns ≥4 BLOCKING: trigger decomposition immediately

### R12 forecast

**1-2 BLOCKING / 2-3 RECOMMENDED** assuming both R11a workstreams land cleanly with dual-grep methodology catching what eyeball-sweep missed. If R11a-measurement comes back with new methodology questions that ux-designer surfaces during authoring, expect +1-2 BLOCKING in the measurement section that cannot be foreseen from R11.

### Validation criteria for R11a closure

- Lines 408, 596, 640, 953 all read consistent under FLOOR=0.70s with re-derived arithmetic
- entities.yaml `TELEGRAPH_WINDOW_FLOOR_S=0.70` + `BARRAGE_SIMULTANEITY_WINDOW_S` updated derivation
- cross-system-survivability-coordination-2026-06-11.md superseded by 2026-06-17 entry citing R10d
- DPC GDD lines 32-33 + 478 updated from `0.6s PROVISIONAL` to `0.70s` with R10d citation
- Pull-Wave Pillar 2 language updated from "≥0.6s" to "≥0.70s"
- game-concept.md Core Fantasy updated from "half-second" to ~"two-thirds of a second" (or equivalent that doesn't lie about the FLOOR)
- 2-AFC protocol section authored in Pull-Wave GDD with falsifiable apparatus + randomization + Alpha escalation path
- Methodology ADR authored documenting dual-grep sweep technique
- R12 returns ≤3 BLOCKING

### Files modified by R11 pass

- `design/gdd/systems-index.md` — Pull-Wave row R11 verdict prefix (R10d block moved to [Prior retained])
- `design/gdd/reviews/pull-wave-behavior-review-log.md` — this entry appended
- `production/session-state/active.md` — R11 checkpoint

### Files NOT modified by R11 pass (intentional — fresh-context re-review is read-only on target GDD; propagation deferred to R11a)

- `design/gdd/pull-wave-behavior.md` — target GDD, read-only during fresh-context review
- `design/registry/entities.yaml` — propagation queued for R11a-arithmetic
- `docs/architecture/platform-seam-interfaces.md` — no seam changes by R11
- `design/gdd/difficulty-phase-controller.md` — addressed by batched DPC mini-review (next operative step)
- `design/gdd/reviews/cross-system-survivability-coordination-2026-06-11.md` — R10d resolution append queued for R11a-arithmetic

### Operative next step (BATCHED per CD R10d Section 5)

Invoke `/design-review design/gdd/difficulty-phase-controller.md` for **DPC mini-review** in this session (the second half of the batched review). Scope: TELEGRAPH_WINDOW_FLOOR_S 0.6/0.65 → 0.70s value flip + AC-PILLAR-2-CONCURRENT playtest gate re-verify against new FLOOR + Tuning Knobs safe-range table + cross-system-survivability-coordination-2026-06-11.md R10d resolution append + entities.yaml FLOOR + PEAK Phase Identity language re-examination per game-designer R1. After DPC mini-review lands, both verdicts inform the combined R11a-arithmetic + R11a-measurement plan (executed in fresh `/clear` sessions).

### Strategic note

R11 surfaced what R10's mechanical-prescription scope discipline could not catch: a methodology gap in the sweep technique that has accumulated stale-residue across 5 prior reviews without anyone noticing. The pre-R2 residue at line 408 is the clearest evidence — it's not an R10d failure, it's an R3/R5/R7/R9 + R10a-d compound failure that R11's fresh-context dual-grep approach uncovered. The R12 forecast (1-2 BLOCKING / 2-3 RECOMMENDED) is conservative: with the methodology ADR in place and ux-designer authoring the 2-AFC protocol section, R12 should be APPROVED-adjacent. The decomposition trigger arming is a procedural hedge — Pull-Wave has been at the coherence ceiling for 3+ revision cycles and the safety margin to single-author stewardship is thin.

---

## R11a-arithmetic — 2026-06-18 (Mechanical Cross-Doc Propagation Pass)

**Trigger**: BATCHED Pull-Wave R11 fresh-context re-review (2026-06-17 — 8 BLOCKING / 16 RECOMMENDED / 6 NICE-TO-HAVE) + DPC R-Updated mini-review (2026-06-18 — 6 BLOCKING / 5 RECOMMENDED / 3 NICE-TO-HAVE) both adjudicated to require an R11a-arithmetic propagation pass per CD R10d Section 5 sequencing recommendation. User-directed sequential R11a-arithmetic execution in a fresh `/clear` session (separate from the R11a-measurement workstream which runs in parallel for the 2-AFC apparatus + LEFT/RIGHT randomization + Alpha PROTO-GATE escalation path authoring).

**Mode**: same-author fresh-context mechanical propagation pass. No specialist panel; no CD adjudication on the propagation surface itself (the TelegraphWindowCurve cascade collapse Path B reauthoring is FLAGGED FOR CD POST-PASS REVIEW per DPC R-Updated B1 framing but the propagation pass applied Path B mechanically per DPC's existing R6 Q7 binding rule — the design-intent ratification is the CD post-pass call).

**Methodology**: dual-grep technique per the new methodology ADR (`docs/architecture/adr-0004-r11a-dual-grep-methodology.md`) — literal grep for stale FLOOR values (0.6, 0.65, 0.68) + computed-result grep for known derived literals (4.35, 1.48, 2.25, 0.325, etc.) + annotation grep for stale provenance references (R2-path-b, PROVISIONAL, R9 coordination). Hit list captured pre-edit; deltas applied per-hit.

### Pull-Wave R11 BLOCKING items closed (B1-B4 — 4 stale-residue lines)

- **B1 — Line 408 boundary table residue**: pre-R2 stale value `4.35s` (= `0.60 + 15.0/4.0`) → corrected to `~7.12s` (= `0.70 + 6.25 + 0.15 + 0.016` at R10d FLOOR=0.70s, min velocity 4.0 m/s, max offset 25.0 m). Annotation added documenting the pre-R2 stale residue and the dual-grep methodology that surfaced it.
- **B2 — Line 596 cook-time warning**: `LeanDurationS < 0.65s` → `LeanDurationS < 0.70s` per R10d FLOOR. Implementation contract — closes the tooling-author writes-wrong-threshold defect.
- **B3 — Line 640 knob-interaction prose**: `0.65s lean + 0.83s = 1.48s` → `0.70s lean + 0.83s = 1.53s`. CD-overruled upgrade from perf-analyst's RECOMMENDED to BLOCKING per R11 synthesis. Now matches F-TRAVERSE-DURATION line 393 worked example.
- **B4 — Line 953 AC-PW-22a state-mix derivation**: literal `TELEGRAPH_WINDOW_FLOOR_S = 0.65s` → `0.70s`; total lifetime `2.25s` → `2.30s`; state fractions re-derived (LEANING 29% → 30.4%; TRAVERSING 63% → 62.2%; LANDED 7% → 6.5%; DESPAWNING 1% → 0.7%); integer state counts unchanged (5/10/1/0-1 — the integer-rounding boundary is invariant under this perturbation); annotation updated to cite R10d as most recent raise (was citing R2-path-b).

### Pull-Wave R11 RECOMMENDED items (Pillar 2 language)

- Audited Pillar 2 references in Pull-Wave GDD for "≥0.6s" / "0.65s" two-raises-behind drift. The historical R9-coordination derivations at lines 224-226 (paths (a)/(b)/(c) enumeration) are intentionally preserved as contemporaneous historical context — they accurately describe what was true at R9-coordination time. The R10d ruling supersedes via a different mechanism (CD-ruled FLOOR raise to 0.70s under direction-detection model), not by invalidating the R9 trade-off enumeration. EC-TELEGRAPH-FLOOR-LOWERED-BELOW-SURVIVABILITY-THRESHOLD (line 549) already updated at R10d. AC-PW-22a state-mix derivation updated above as B4.

### Cross-doc batch (DPC R-Updated mini-review BLOCKING B1-B6)

- **DPC B1 — TelegraphWindowCurve cascade collapse (HIGHEST-IMPACT)**: Path B reauthoring applied per DPC's own line 38-50 R6 Q7 binding rule. New curve keys `(0.0, 0.94), (0.333, 0.82), (0.75, 0.70), (1.0, 0.70)` replace pre-R10d keys `(0.0, 0.9), (0.333, 0.72), (0.75, 0.6), (1.0, 0.6)`. Restores the 0.12s intra-phase overlap window (MID = FLOOR + 0.12; OPENER = MID + 0.12; PEAK = FLOOR) under R10d FLOOR=0.70s; pre-R10d keys under R10d FLOOR would have collapsed MID's overlap from 0.12s to 0.02s (83% reduction = silent Phase Identity collapse). **FLAGGED FOR CD POST-PASS REVIEW**: confirm Path B (re-author for overlap preservation, applied mechanically per the existing binding rule) vs Path C (bind 0.02s overlap as new design intent / Pillar 3 trade). The R11a-arithmetic pass did not make a new design judgment — it applied the existing binding rule that R10d failed to trigger. CD post-pass decision ratifies the design-intent direction.
- **DPC B2 — 14 FLOOR=0.6s sites + 1 line-717 dual-grep surfaced (15 total)**: all sites updated to R10d-bound 0.70s with consistent R11a-arithmetic 2026-06-18 annotation tagging. The 15th site (line 717 — R8-era 0.68s PROVISIONAL stale residue in the Non-tunable runtime invariants table) was surfaced by dual-grep beyond the mini-review's 14-site enumeration; documented as a methodology validation case.
- **DPC B3 — Tuning Knobs row line 479 R9 coordination note inversion**: existing "FLOOR stays at the R2 Cluster E value of 0.65s. No DPC GDD content change required." text replaced with full R10d resolution note citing CD PATH (i) ruling + R9 Option (c) Wave Spawner cook-time exclusion + MIN_ESCAPE_SLIPS=2 REMAINS BINDING + cross-reference to coordination doc R10d resolution append.
- **DPC B4 — Rule 9 PROVISIONAL framing restructure**: design-time bound at FLOOR=0.70s (no longer PROVISIONAL for design-time scope); two-phase gate pattern applied per Pull-Wave R10b precedent (ADVISORY-at-story-Done static arithmetic verification + BLOCKING-at-Alpha empirical AC-PILLAR-2-CONCURRENT closure). Resolution paths restructured: Path A pass, Path B fail-timing requires CD ruling per R10d precedent (the original automatic "raise to 0.7-0.9s" path retired), Path C fail-perceptual-load unchanged.
- **DPC B5 — AC-PILLAR-2-CONCURRENT validation target update**: "survivability of 0.6s" → "survivability of 0.70s" with two-phase gate pattern applied to the AC's Gate header.
- **DPC B6 — cross-system-survivability-coordination-2026-06-11.md R10d resolution append**: new "## 2026-06-18 R10d Resolution Entry" section appended near end of doc, documenting R9 verdict superseded + R9 Option (c) REMAINS BINDING + mechanism summary + propagation pass coverage + forward contracts still operative.

### DPC RECOMMENDED items addressed

- DPC GDD status header (line 3) fixed: "Pending Re-Review 7" → "Approved (R7 2026-06-06); R-Updated mini-review 2026-06-18 NEEDS REVISION ... ; R11a-arithmetic propagation pass DONE 2026-06-18".
- BARRAGE_SIMULTANEITY_WINDOW_S derivation target update — entities.yaml notes updated from "target W=0.325s at FLOOR=0.65" to "target W=0.35s at R10d FLOOR=0.70s; current value 0.3 held pending DPC R-Updated re-review confirmation". The value flip itself (0.3 → 0.35) deferred to DPC R-Updated re-review per the existing deferral pattern in the entities.yaml note.

### game-concept.md Core Fantasy "half-second" drift (Pull-Wave R11 RECOMMENDED)

- Three sites updated from "half-second" → "seven-tenths of a second" (matches R10d FLOOR=0.70s): Elevator Pitch ("survival means reading the pull seven-tenths of a second before it lands"); Core Fantasy ("I read the storm seven-tenths of a second before it hits, and I bend through it."); Unique Hook ("giving skilled players a seven-tenths-of-a-second window to slip through").

### entities.yaml registry update

- `TELEGRAPH_WINDOW_FLOOR_S value: 0.65` → `0.70`; notes block restructured to retire R9-coordination "FLOOR stays at 0.65" framing in favor of R10d resolution narrative; PROVISIONAL marker scope clarified (now applies to empirical-confirmation phase only); revised date 2026-06-11 → 2026-06-18.
- `BARRAGE_SIMULTANEITY_WINDOW_S` notes updated with R10d derivation target W=0.35s (= FLOOR/2 at R10d FLOOR=0.70s); current value 0.3 held pending DPC author confirmation; revised date 2026-06-18.
- Header `last_updated:` field updated to 2026-06-18 with R11a-arithmetic narrative.

### Methodology ADR created

- `docs/architecture/adr-0004-r11a-dual-grep-methodology.md` created — Status: Proposed. Documents the dual-grep technique (literal grep + computed-result grep + annotation grep + manual review of arithmetic expressions) prescribed for all future cross-system constant propagation passes. Addresses the methodology gap that allowed Pull-Wave's 4-line stale-residue to survive 5 sequential reviews under single-pattern grep. Future tooling direction (automated dual-grep via registry parsing) filed as out-of-scope follow-up.

### Files modified by R11a-arithmetic

- `design/gdd/pull-wave-behavior.md` — 4 stale-residue lines (408, 596, 640, 953) updated; ~+8 lines net (annotations).
- `design/gdd/difficulty-phase-controller.md` — 15 FLOOR sites updated + Rule 9 restructure + Tuning Knobs row R9-note replacement + AC-PILLAR-2-CONCURRENT body update + TelegraphWindowCurve Path B reauthoring + status header fix + Last Updated date; ~+60 lines net (annotations + Rule 9 restructure + Path B reauthoring note).
- `design/registry/entities.yaml` — TELEGRAPH_WINDOW_FLOOR_S value + notes restructure; BARRAGE_SIMULTANEITY_WINDOW_S notes update; header last_updated; ~+30 lines net.
- `design/gdd/game-concept.md` — 3 "half-second" → "seven-tenths of a second" prose updates; 0 net line change.
- `design/gdd/reviews/cross-system-survivability-coordination-2026-06-11.md` — R10d resolution entry appended; +~35 lines.
- `docs/architecture/adr-0004-r11a-dual-grep-methodology.md` — NEW; ~200 lines.
- `design/gdd/reviews/pull-wave-behavior-review-log.md` — this entry; +~70 lines.
- `design/gdd/reviews/difficulty-phase-controller-review-log.md` — R11a-arithmetic entry appended (companion to this entry); +~50 lines.
- `design/gdd/systems-index.md` — Pull-Wave + DPC rows updated with R11a-arithmetic prefix; ~+4 lines.
- `production/session-state/active.md` — R11a-arithmetic checkpoint extract; ~+30 lines.

### Files NOT modified by R11a-arithmetic (intentional)

- `design/gdd/player-movement.md` — independently in decomposition state per `design/gdd/reviews/player-movement-decomposition-plan-2026-06-16.md`; R10d imposes NO new PM forward contracts per CD ruling; SLIP_TWEEN safe range unchanged.
- `design/gdd/input-system.md`, `design/gdd/run-state-machine.md` — sibling GDDs not in R11 + DPC mini-review scope.
- `docs/architecture/platform-seam-interfaces.md` — no seam changes by R11a-arithmetic (R10d is pure-parameter; DPC mini-review Phase 2b seam grep was CLEAN per the mini-review verdict).
- `design/gdd/wave-spawner-pattern-library.md` — does not yet exist; R10d's pool-lifetime ceiling +50ms tag accretes onto the existing pre-authoring forward-contract bundle.

### R12 forecast

**Pull-Wave R12 fresh-context re-review forecast** (post-R11a-arithmetic landing + post-R11a-measurement landing): **1-2 BLOCKING / 2-3 RECOMMENDED** per R11 line 1960 — UNCHANGED. The R11a-arithmetic pass closed **5/8 Pull-Wave R11 BLOCKING (B1-B5 — 4 stale-residue lines B1-B4 + cross-doc FLOOR batch B5)** + audited Pillar 2 language. **B6-B8 (2-AFC apparatus + LEFT/RIGHT randomization + Alpha PROTO-GATE escalation path) are R11a-measurement scope per CD routing and remain operative against the R11a-measurement workstream — NOT closed by R11a-arithmetic**. The residual R12 surface is therefore primarily R11a-measurement scope (B6-B8 closures) plus the CD post-pass review of the TelegraphWindowCurve Path B reauthoring (if CD ratifies Path B unchanged, zero additional BLOCKING; if CD requests Path C / Pillar 3 trade direction, +1-2 BLOCKING for the curve key revision pass).

### R11a-measurement workstream — separate (parallel)

R11a-measurement runs separately in a parallel fresh-context ux-designer session per the CD-binding routing decision in the R11 fresh-context re-review entry above. Scope: 2-AFC apparatus condition (35-40cm viewing distance + 80% brightness + ~500 lux ambient + True Tone disabled) + LEFT/RIGHT trial randomization 50/50 + Alpha PROTO-GATE failure escalation path pre-authoring (recommend PATH (i) FLOOR raise per CD R10d precedent). NOT addressed by this R11a-arithmetic entry.

### Strategic note

The R11a-arithmetic pass is the first real-world application of the dual-grep methodology newly authored as ADR-0004. The pass surfaced one site beyond the DPC R-Updated mini-review's 14-site enumeration (line 717 — the R8-era "0.68s PROVISIONAL" stale residue in the Non-tunable runtime invariants table that the mini-review missed because its scope was the Tuning Knob row, not the parallel runtime-invariants table). This is the methodology validating itself: a 15th site surfaced by the dual-grep that would have been a future stale-residue finding under single-pattern grep. The Path B reauthoring of TelegraphWindowCurve is the only design-shaped edit in the pass; everything else is mechanical propagation of the R10d FLOOR raise plus annotation cleanup. The cumulative effect closes the 15 FLOOR=0.6→0.70 propagation sites + 4 Pull-Wave stale-residue lines + game-concept.md Core Fantasy drift + entities.yaml registry update + cross-system-survivability-coordination append + methodology ADR — **11 BLOCKING items closed by R11a-arithmetic** (5 Pull-Wave R11 B1-B5 + 6 DPC R-Updated B1-B6) across 5 docs + 1 new ADR. The remaining 3 Pull-Wave R11 BLOCKING (B6-B8) are R11a-measurement workstream scope per CD routing; combined R11a (both workstreams) totals 14 BLOCKING when R11a-measurement lands.

The CD post-pass review of TelegraphWindowCurve Path B reauthoring is the only open design-surface item from R11a-arithmetic. R12 fresh-context re-review of Pull-Wave + DPC R-Updated re-review is the next gate after R11a-measurement lands.

---

## R11a-measurement — 2026-06-18 (Psychophysics Methodology Author Pass)

### Trigger

R11a-measurement workstream per CD routing in the R11 fresh-context re-review entry above and the R11a-arithmetic entry above. User directed sequential R11a execution within a single Claude Code session (R11a-arithmetic first, R11a-measurement second) to avoid same-file write race risk on `pull-wave-behavior.md`, this review log, `systems-index.md`, and `active.md` that literal-parallel agent execution would have produced. Scope: close the 3 remaining Pull-Wave R11 BLOCKING items (B6 2-AFC apparatus + B7 LEFT/RIGHT randomization + B8 Alpha PROTO-GATE escalation path) — psychophysics measurement-protocol gaps that R11a-arithmetic's mechanical propagation scope intentionally excluded per the CD-binding R11a split.

### Routing

Routed to the `ux-designer` specialist (fresh-context Agent invocation) per CD routing in R11 entry. The specialist read `active.md`, the R11 review log entry's ux-designer specialist findings (apparatus details + randomization rationale + Alpha escalation context), and the existing R10b 2-AFC adaptive-staircase methodology in AC-PW-TIER-DIRECTION-DETECTION-PROTO-GATE as the prose-density template before authoring.

### BLOCKING items closed (3 of 3)

**B6 — 2-AFC apparatus specification (closed)**

Authored an in-AC **Apparatus Specification** sub-section inside AC-PW-TIER-DIRECTION-DETECTION-PROTO-GATE. Locked parameters:

- **Viewing distance**: 35–40cm — tighter than the gameplay arm's-length target; rationale: at 30cm a 1° lean subtends ~5.2mm on a 6-inch screen; at 50cm it subtends ~8.7mm — 67% variance in retinal projection size would dominate the 0.15s threshold measurement. Tester confirms with ruler before each session.
- **Display brightness**: 80% — device-locked for staircase session to cap per-device backlight variance.
- **Ambient lighting**: ~500 lux ±100 lux — standard office lighting; target with a lux meter before each session; avoid windows or direct overhead lights creating screen hotspots.
- **True Tone**: disabled (iOS) / equivalent adaptive display modes off (Android) — True Tone warms display color temperature and shifts perceived contrast on the OPENER/MID/PEAK lean brightness tells.
- **Display orientation**: portrait (per game spec).
- **Same device per tester across all 4 tiers** — within-subject device consistency; variance across testers captured by the 3-tester n.

Reproducibility bar: a fresh tester picking up the AC must be able to configure the test rig from this specification alone without ambiguity.

**B7 — LEFT/RIGHT trial randomization (closed)**

Authored a **Trial randomization** sub-clause inside the WHEN clause of AC-PW-TIER-DIRECTION-DETECTION-PROTO-GATE. Locked protocol:

- Pre-seeded 50/50 balanced pseudo-random shuffle per staircase — NOT independent Bernoulli per trial (Bernoulli admits runs of 4-5 same-direction trials, which allow motor-anticipation bias to inflate apparent correct-response rate above true perceptibility threshold).
- No-runs-of-3 constraint enforced during shuffle; if 3 or more consecutive same-direction trials appear at any position, reshuffle until constraint is met.
- Per-staircase random seed logged for reproducibility (alongside the staircase trajectory in the evidence file).
- Same protocol applied independently for each of the 12 staircases (4 tiers × 3 testers); no sequence reused across staircases.
- Log line per staircase: `TRIAL_RAND_SEED: tier=T tester=N seed=XXXXXXXX balance=50/50 max_run=2 ✓`.

**B8 — Alpha PROTO-GATE failure escalation paths (closed)**

Authored an **Alpha PROTO-GATE Failure Escalation** sub-section inside AC-PW-TIER-DIRECTION-DETECTION-PROTO-GATE; SUPERSEDES the R10d-era inline escalation list (the retired paths "front-load ease curve" and "tighten SLIP_TWEEN" are explicitly retired by this section). Pre-authorized paths in priority order:

- **PATH (i) — RECOMMENDED: raise `TELEGRAPH_WINDOW_FLOOR_S` by one increment** per the R10d precedent (CD PATH (i) raised FLOOR 0.65s → 0.70s; the same mechanism applies if magnitude-discrimination proves operative at the Alpha confidence-check). Lower bound: **0.82s** — derivable under the magnitude-discrimination (3°) model at REACT=0.25 ceiling, `t_perceptible_onset = 0.27s + REACT = 0.25s + SLIP_TWEEN × MIN_SOLO_ESCAPE_SLIPS = 0.30s = 0.82s` (from AC-PW-TIER1-PERCEPTIBILITY magnitude-discrimination row at REACT=0.25 ceiling, the narrowest case). Upper bound: **TBD — pending systems-designer derivation in R12 prep** (bounded by cross-system survivability ceiling under MIN_ESCAPE_SLIPS=2 + REACT=0.20s + SLIP_TWEEN safe-range upper 0.15s; requires cross-system arithmetic against DPC FLOOR constraints not computable within this AC's scope). PATH (i) is preferred — single-parameter raise on a DPC-owned value with smallest blast radius, following the R10d precedent.
- **PATH (ii) — CD-handoff: enlarge lean angular magnitude per tier** (revise tier angle schedule, e.g., 17°/22°/25°/28° → enlarged spread). Pillar 3 trade (changes lean-visual character of all tiers) requiring creative-director adjudication before adoption; pre-authorized here as a CD-handoff path so R12 planning does not require a new routing decision.
- **PATH (iii) — fail-safe**: if both PATH (i) and PATH (ii) are blocked, ship MID and PEAK tier discrimination as ADVISORY. Tier-1 direction-detection is conceded to be marginal; the cadence change between OPENER/MID/PEAK is still felt by the player via wave velocity and frequency even if fine per-tier angle discrimination is not reliable. MID and PEAK tier discrimination gates downgraded BLOCKING → ADVISORY under this fail-safe, pending a post-ship tuning pass.

These escalations are NOT design-time hedges; R10d's binding direction-detection decision means design-time closure does not depend on PROTO-GATE's outcome. These paths are only relevant if the Alpha confidence check fails.

### Cross-AC impact

AC-PW-TIER1-PERCEPTIBILITY: no body change required — the two-phase ADVISORY-at-story-Done + BLOCKING-at-Alpha gate established at R10b (per R9 qa-lead F4 closure) remains valid; the cross-reference from AC-PW-TIER-DIRECTION-DETECTION-PROTO-GATE's escalation paths to AC-PW-TIER1-PERCEPTIBILITY's pre-authorized paths is now bidirectionally coherent.

### Files modified by R11a-measurement

- `design/gdd/pull-wave-behavior.md` — AC-PW-TIER-DIRECTION-DETECTION-PROTO-GATE body extended in place with Apparatus Specification + Trial randomization + Alpha PROTO-GATE Failure Escalation sub-sections; AC header annotation updated to enumerate R11a-measurement BLOCKING closures (B6/B7/B8); ~+30 lines net within the AC body.
- `design/gdd/reviews/pull-wave-behavior-review-log.md` — this entry; +~120 lines.
- `design/gdd/systems-index.md` — Pull-Wave row + header `Last Updated` extended with R11a-measurement workstream DONE prefix on top of R11a-arithmetic entry; ~+10 lines.
- `production/session-state/active.md` — STATUS block updated to BOTH R11a workstreams DONE; new Session Extract appended; ~+50 lines.

### Files NOT modified by R11a-measurement (intentional)

- `design/gdd/difficulty-phase-controller.md`, `design/registry/entities.yaml`, `design/gdd/game-concept.md`, `design/gdd/reviews/cross-system-survivability-coordination-2026-06-11.md`, `docs/architecture/adr-0004-r11a-dual-grep-methodology.md`, `design/gdd/reviews/difficulty-phase-controller-review-log.md` — all addressed by R11a-arithmetic earlier in the session; R11a-measurement scope is the AC-PW-TIER-DIRECTION-DETECTION-PROTO-GATE block + cross-AC coherence + housekeeping only.
- `design/gdd/player-movement.md`, `design/gdd/input-system.md`, `design/gdd/run-state-machine.md` — sibling GDDs out of scope.
- `docs/architecture/platform-seam-interfaces.md` — no seam changes from psychophysics methodology authoring.

### TBD flagged for R12 prep

PATH (i) **upper-bound derivation** for `TELEGRAPH_WINDOW_FLOOR_S` further-raise ceiling — requires cross-system arithmetic against DPC FLOOR constraints under MIN_ESCAPE_SLIPS=2 + REACT=0.20s + SLIP_TWEEN safe-range upper 0.15s. Routed to systems-designer at R12 prep. Not blocking R12 re-review (the lower bound 0.82s is the operative number for the Alpha confidence-check escalation; the upper bound only matters if PATH (i) needs multiple iterative raises).

### R12 forecast

**Pull-Wave R12 fresh-context re-review forecast** (post-R11a-arithmetic + post-R11a-measurement landing): **1-2 BLOCKING / 2-3 RECOMMENDED** UNCHANGED per R11 line 1960. Both R11a workstreams landed cleanly within their CD-routed scopes; residual R12 surface is primarily:
- CD post-pass review of TelegraphWindowCurve Path B reauthoring (if CD ratifies Path B unchanged, 0 BLOCKING addition; if CD requests Path C / Pillar 3 trade direction, +1-2 BLOCKING for revision pass).
- R11a-measurement's PATH (i) upper-bound TBD — if R12 reviewer wants it derived before R12 closure, +0-1 BLOCKING for systems-designer pass.
- Any newly surfaced methodology questions during R12 specialist review (not foreseeable; +0-1 RECOMMENDED upper bound).

The decomposition trigger remains ARMED at 8 BLOCKING (per R11 line 280) but unfired pending R12 verdict.

### Combined R11+DPC closure tally

- **R11a-arithmetic**: 11 BLOCKING closed (5 of 8 Pull-Wave R11 B1-B5 + 6 of 6 DPC R-Updated mini-review B1-B6).
- **R11a-measurement**: 3 BLOCKING closed (3 of 8 Pull-Wave R11 B6-B8).
- **Combined R11a (both workstreams)**: **14 BLOCKING closed across the batched Pull-Wave R11 + DPC R-Updated mini-review verdicts** — 8 of 8 Pull-Wave R11 BLOCKING + 6 of 6 DPC R-Updated mini-review BLOCKING.

### Operative next step

Either (a) R12 fresh-context BATCHED re-review of Pull-Wave + DPC R-Updated (per CD R10d Section 5 batching pattern established by R11) — the next gate in the convergence trajectory; OR (b) CD post-pass review of TelegraphWindowCurve Path B reauthoring (the only open design-surface item from R11a-arithmetic) — can run in parallel with (a) or sequentially before it. If CD reviews Path B first and confirms, R12 surface tightens to R11a-measurement TBD + procedural verification; if CD requests Path C / Pillar 3 trade, +1-2 BLOCKING gets folded into R12 surface.

Parallel tracks unchanged: PM decomposition (per `design/gdd/reviews/player-movement-decomposition-plan-2026-06-16.md`) + Telegraph prototype.

### Strategic note

R11a-measurement validated the CD's R11a split routing: the apparatus + randomization + escalation-path surface required psychophysics literacy + reproducibility-first authoring discipline that the same-author arithmetic context structurally cannot provide. Routing to `ux-designer` fresh-context produced a single-session clean landing on all 3 BLOCKINGs with zero new methodology questions surfaced beyond the documented TBD (PATH (i) upper-bound derivation, which is a systems-designer surface not a ux-designer surface). The combined R11a-arithmetic + R11a-measurement execution closes the full R11 + DPC mini-review surface in one Claude Code session — sequential rather than literal-parallel for file-race safety, but functionally equivalent to the "two parallel fresh-context sessions" the CD-binding routing envisioned.

---


## Review — 2026-06-18 — Verdict: NEEDS REVISION (R12 fresh-context BATCHED re-review of Pull-Wave + DPC; revisions applied in-session)

Scope signal: M
Specialists: game-designer, systems-designer, qa-lead, ux-designer, performance-analyst, unreal-specialist, creative-director (synthesis)
Re-review type: R12 fresh-context BATCHED re-review per CD R10d Section 5 batching pattern + R11a-arithmetic/R11a-measurement landing precedent
Prior verdict resolved: Yes — R11a-arithmetic closed 5/8 Pull-Wave R11 B1-B5 + R11a-measurement closed 3/3 Pull-Wave R11 B6-B8; combined R11a closed 8 of 8 Pull-Wave R11 BLOCKING + 6 of 6 DPC R-Updated mini-review BLOCKING (14 total)
Forecast: 1-2 BLOCKING / 2-3 RECOMMENDED (per R11 line 1960)
Actual surface: **4 Pull-Wave BLOCKING + 3 DPC BLOCKING = 7 BLOCKING / ~17 RECOMMENDED total combined** — forecast missed by ~4× on BLOCKING

### Root cause of forecast miss (advisor-flagged + CD-ratified)

R11a-arithmetic patched **formula sites** but did NOT sweep **oracle sites**. The dual-grep methodology in ADR-0004 catches mechanical FLOOR=0.65→0.70 propagation in formulas and worked examples, but did not extend to: (a) AC body pass-condition values (e.g., `THEN telegraph_window_s == 0.6`); (b) test fixture preamble keys; (c) anchor reference tables read by downstream authors. The forecast assumed dual-grep was exhaustive; it was exhaustive over the formula-site class but blind to the oracle-site class. **Lesson for future re-review forecasting: arithmetic patches require a separate oracle-site sweep before forecast publication.**

### CD R12 binding rulings

**Ruling 1 — Path B is binding** (resolves game-designer BLOCKING-1 design-intent flag). Path B keys `(0.0, 0.94), (0.333, 0.82), (0.75, 0.70), (1.0, 0.70)` for `TelegraphWindowCurve` are confirmed as canonical, NOT Path C (which would collapse MID-PEAK cadence-character gap to 2.8%, well below JND, defeating Phase Identity Differentiator 1 / Pillar 3). Path B preserves three distinguishable cadence bands (0.94 / 0.82 / 0.70 — gaps of 14.6% and 14.6%). The R11a-arithmetic Path B commit is ratified; the "FLAGGED FOR CD POST-PASS REVIEW" annotation in DPC line 717 Tuning Knobs row is hereby resolved Path B. Cascade implication: AC-PILLAR-2-CONCURRENT's PROTO-GATE must measure MID-band discrimination (at 0.82s) in addition to PEAK-band (at 0.70s) per game-designer RECOMMENDED-6.

**Ruling 2 — 7.7% PEAK slowdown wording retracted (ux-designer BLOCKING-1 closure)**. The R10d header line 6 phrase "sub-perceptible single-feature change" is overconfident — Weber fraction for duration discrimination in the 0.5–1.0s range is 5–10% (Getty 1975; Grondin 2010 review), placing 7.7% AT the JND boundary, not below it. Retracted in favor of **"near-JND telegraph slowdown ... boundary of felt-but-not-named for trained players."** The underlying R10d PATH (i) decision survives reframing — PATH (i) is still the lowest-impact fail-safe vs PATH (ii)/(iii)/(iv) structural alternatives even if the cost is felt-not-named rather than truly sub-threshold. **Edited at Pull-Wave line 6 in-session.**

**Ruling 3 — PATH (i) iteration cap = 1 raise** (ux-designer BLOCKING-3 closure). The PROTO-GATE escalation path PATH (i) is capped at exactly ONE FLOOR raise per PROTO-GATE failure. If a second confidence-check failure occurs after a single PATH (i) raise has been applied, the team MUST escalate to PATH (ii) or PATH (iii) before any further PATH (i) iteration. Cap is binding regardless of headroom under the 0.76s ceiling. Prevents indefinite FLOOR raises with diminishing returns while deferring the underlying design question. **Edited at Pull-Wave line 941 in-session.**

### Pull-Wave R12 BLOCKING items (4) — ALL CLOSED in-session

1. **[qa-lead BLOCKING-1 + systems-designer R-D]** PATH (i) upper bound was TBD. **R12 systems-designer derivation 2026-06-18: PATH (i) upper bound = 0.76s** under Path B + 0.12s overlap cascade ceiling (Constraint A: `OPENER_key = FLOOR + 0.24 ≤ 1.0s`, where 1.0s is the practical "generous-but-not-lethargic" perceptibility ceiling). Current 0.70s has 60ms headroom before Constraint A. Constraint B (`FLOOR < 1.0s`, PEAK spawn-interval floor) is non-binding under Constraint A. Inscribed at line 941.

2. **[qa-lead BLOCKING-2]** PATH (iii) referenced non-existent ACs (`AC-PW-MID-TIER-DISCRIMINATION` / `AC-PW-PEAK-TIER-DISCRIMINATION`). Paper-only-AC class. **Rewrite at line 941**: PATH (iii) downgrades **sub-check (b) cross-tier threshold monotonicity** of THIS AC (AC-PW-TIER-DIRECTION-DETECTION-PROTO-GATE) from BLOCKING-at-Alpha to ADVISORY-at-Alpha; sub-check (a) direction-detection threshold floor remains BLOCKING-at-Alpha. PATH (iii) concedes the per-tier ordering above tier-1, NOT tier-1 itself.

3. **[ux-designer BLOCKING-3]** PATH (i) iteration cap missing. **Inscribed at line 941 per CD Ruling 3 above** — one raise authorized; second failure forces PATH (ii) or PATH (iii).

4. **[ux-designer BLOCKING-1]** "Sub-perceptible" claim at line 6 was overconfident relative to Weber JND. **Retracted per CD Ruling 2 above**, reworded to "near-JND telegraph slowdown." PROTO-GATE consequence noted (MID-band discrimination must be measured).

**Notation fix (RECOMMENDED → folded in-session)**: qa-lead RECOMMENDED-5 caught that PATH (i) lower bound derivation used `MIN_SOLO_ESCAPE_SLIPS = 1` but `0.15 × 1 = 0.15s`, not the cited 0.30s. The 0.30s product is correct under `MIN_ESCAPE_SLIPS = 2` (barrage invariant). Symbol corrected in line 941.

### Pull-Wave R12 RECOMMENDED items (12) — deferred to R13 surface

- 2-AFC tier-order counterbalancing across testers (qa-lead RECOMMENDED-6)
- PROTO-GATE n=3 testers/staircase undersized for population threshold (ux-designer)
- 35-40cm apparatus vs 50-60cm gameplay ecological-validity gap (ux-designer)
- 5-lane thumb-target Fitts framing should be retired in favor of lane-discrimination perceptibility (ux-designer)
- 2-AFC convergence "OR 30 trials OR 10 reversals" can terminate prematurely (ux-designer)
- Pool pause-flush burst undersized (performance-analyst)
- K=2 30fps 17ms cell perf-gate elevation (performance-analyst)
- UCurveFloat cook-time key-count warning at >10 keys (performance-analyst)
- TFunction capture-lifetime annotation on FWaveSpawnerCallbackTestStub (unreal-specialist)
- Substrate vs legacy material declaration absent (unreal-specialist)
- Cross-doc Player Fantasy reference to DPC (game-designer)
- BARRAGE_SIMULTANEITY_WINDOW_S 0.3 vs target 0.35 (systems-designer; deferral-acceptable, propagate via `/propagate-design-change` post-R13)

### DPC R12 BLOCKING items (3) — ALL CLOSED in-session (companion edits applied to DPC GDD same session)

1. **[systems-designer Finding 1 + qa-lead Findings 3+4 convergent]** **Test-as-oracle inversion**: DPC AC-07a/AC-07b (lines 800-801) + AC-09a (line 820) + canonical test fixture (line 782) bound FLOOR=0.6, the pre-R10d value. A correct R10d implementation that clamps to 0.70 would have FAILED these ACs as written; a regression to 0.6 would have PASSED. Convergent finding from two specialists — counted as one BLOCKING. **Edits applied in-session**:
   - Line 782 canonical fixture: `(0.0, 0.9), (0.333, 0.72), (0.75, 0.6), (1.0, 0.6)` → `(0.0, 0.94), (0.333, 0.82), (0.75, 0.70), (1.0, 0.70)`
   - Line 800 AC-07a: expected value `0.6` → `0.70`
   - Line 801 AC-07b: both test input AND expected value `0.6` → `0.70`
   - Line 820 AC-09a inactive snapshot: OPENER `0.9` → `0.94` (Path B key)

2. **[game-designer BLOCKING-1 companion]** **Stale per-phase default target values anchor table at line 357**: `telegraph_window_s | OPENER: 0.9 | MID: 0.72 | PEAK: 0.6`. The first numeric anchor a new team member reads; downstream authors use it as reference. R11a-arithmetic missed this. **Edit applied in-session**: updated to Path B `0.94 / 0.82 / 0.70` with R12 annotation.

3. **[ux-designer BLOCKING-2]** AC-PILLAR-2-CONCURRENT pass criterion ambiguous between point estimate and Wilson LCB. At n=30 testers, 80% point estimate corresponds to only ~62% Wilson LCB — barely above chance for a Pillar 2 safety gate. **Edit applied in-session at line 921**: pass condition now specifies BOTH a ≥80% sample point estimate AND a one-sided 95% Wilson LCB ≥ 0.62, evaluated across the per-PEAK-exposure observation aggregate (n_observations ≥ 30 × 5 = 150; LCB ≈ 0.73 at observed 80%). Both gates must hold.

### Verified PASS items (not findings)

- **R10c OnDespawnedUserCallback Seam 13 slot**: confirmed present at `docs/architecture/platform-seam-interfaces.md` lines 1591, 1657-1669, 1725, 1749 (qa-lead R-H sweep)
- **Cross-doc FLOOR=0.70 consistency**: Pull-Wave + DPC fully aligned post-R11a-arithmetic (qa-lead R-J)
- **Vibe-language sweep**: no ACs contain "feels balanced", "works correctly", "performs well" etc. as pass conditions (qa-lead)
- **R-H paper-only-seam-FIELD audit**: clean (qa-lead)
- **Pull-Wave line 908 K=3 table values**: live K-frame formula outputs at FLOOR=0.70, NOT stale FLOOR=0.65 residue (game-designer + systems-designer convergent; R-C explicitly CLOSED)
- **K-frame inter-slip arithmetic at FLOOR=0.70**: rounding-error-free (systems-designer R-F)
- **Pool sizing = 23 derivation**: arithmetically correct (performance-analyst R-N)
- **ISMC 2-draw-call architecture**: sound on Mobile Forward (performance-analyst R-O)
- **DPC tick cost on ARM**: <3μs estimated, well within budget (performance-analyst R-Q)
- **LeanEaseCurve_Canonical RCIM_Cubic / RCTM_Auto API**: stable in UE 5.7 (unreal-specialist R-R)
- **bDPCAbortPending name**: brief-side artifact only; actual mechanism is `bInitialized` + `IDPCAbortDelegate` idempotency (unreal-specialist)
- **IPullWaveInstanceObserver "deferred seam"**: correct call; stub-now-extend-later would be premature scope (unreal-specialist)

### Story-Done blockers (not GDD-revision; out of R12 GDD verdict scope)

- **`docs/architecture/adr-NNNN-pullwave-instanced-renderer.md` does not exist**: OQ-PW-3 was promoted to REQUIRED ADR at R8 per perf-analyst B-3. Story-Done blocker for Pull-Wave implementation. Author before story-Done attempt; assign to unreal-specialist. Not a GDD revision item — the GDD content is sound; the missing artifact is downstream.

### Files modified in R12 in-session pass

- `design/gdd/pull-wave-behavior.md` — line 6 header retract + line 941 (PATH (i) upper bound 0.76s + PATH (iii) rewrite + PATH (i) iteration cap + MIN_ESCAPE_SLIPS notation fix); ~+20 lines net
- `design/gdd/difficulty-phase-controller.md` — line 357 (per-phase default target values anchor table 0.9/0.72/0.6 → 0.94/0.82/0.70) + line 782 (canonical fixture keys) + line 800-801 (AC-07a/b FLOOR 0.6 → 0.70) + line 820 (AC-09a OPENER 0.9 → 0.94) + line 921 (AC-PILLAR-2-CONCURRENT Wilson LCB gate-criterion sharpening); ~+30 lines net
- `design/gdd/reviews/pull-wave-behavior-review-log.md` — this entry; ~+80 lines

### Files NOT modified by R12 (intentional)

- `design/gdd/reviews/difficulty-phase-controller-review-log.md` — user opted not to append a parallel DPC R12 entry; the DPC closures are documented in this Pull-Wave log entry as the canonical R12 record
- `design/gdd/systems-index.md` — user opted not to update; deferred
- `design/registry/entities.yaml` — no registry-backed value changes (PATH (i) upper bound 0.76s is a derived ceiling, not a registry constant; if/when codified it would belong in entities.yaml as `TELEGRAPH_WINDOW_FLOOR_MAX_ADMISSIBLE_S`)
- `docs/architecture/platform-seam-interfaces.md` — no seam changes
- `design/gdd/player-movement.md`, `design/gdd/input-system.md`, `design/gdd/run-state-machine.md` — sibling GDDs out of scope
- `design/gdd/wave-spawner-pattern-library.md` — does not yet exist

### R13 forecast (per CD synthesis)

**0-1 BLOCKING / 2-4 RECOMMENDED.** Reasoning:
- All R12 BLOCKING items closed in-session via inscription-class edits + 3 CD rulings; no remaining derivations open
- Open RECOMMENDED cluster is PROTO-GATE methodology refinements (counterbalancing, n=3 sizing, ecological validity, convergence criterion) — review-able but not load-bearing on GDD approval; can land in a single PROTO-GATE protocol sprint
- HISM vs ISMC ADR is downstream (Story-Done blocker), does not need R13 attention
- The R10d→R11a→R12 trajectory shows decreasing structural surface and increasing inscription-density; R13 should be the final convergence gate

**Recommendation: R13 runs in `lean` mode** (no specialist agents; CD + qa-lead + systems-designer only) unless ux-designer's PROTO-GATE methodology fixes land between now and then, in which case spawn ux-designer once more to verify.

### Operative next step

R13 fresh-context BATCHED re-review of Pull-Wave + DPC. Schedule after PROTO-GATE methodology refinements land (or after explicit user decision to defer RECOMMENDED items to a PROTO-GATE sprint). Parallel tracks unchanged: PM decomposition + Telegraph prototype + HISM/ISMC ADR authoring.

### Strategic note

R12 is the first review in the R7→R8→R9→R10→R11→R11a→R12 trajectory where the surface was **methodology-failure-class** (oracle-site sweep gap in R11a-arithmetic) rather than design-defect-class. The oracle-site sweep gap is documented above and should be folded into ADR-0004's methodology section as a corollary rule: "Dual-grep targets formula sites; oracle-site sweep (AC pass conditions, test fixture preambles, anchor reference tables) requires a separate scan against the parameter being propagated." With the methodology gap closed and 7 BLOCKING items resolved in a single in-session pass, R12 converges the Pull-Wave + DPC GDDs to a near-APPROVED state with the residual surface being methodology-refinement and downstream artifact authorship (HISM ADR).

---

## R13 Lean BATCHED Re-Review 2026-06-19 (Pull-Wave + DPC, canonical entry per R12 housekeeping pattern)

### Trigger + mode

User typed `next` post-`/clear`; recovered state surfaced operative next step = R13 fresh-context BATCHED re-review of Pull-Wave + DPC per CD R10d Section 5 batching pattern (established by R11 + R12 precedent). User confirmed "go" after a one-line scope confirmation. Mode = **lean panel** (CD + qa-lead + systems-designer only — no game-designer / performance-analyst / unreal-specialist / ux-designer this round, by CD R12 synthesis prescription). PROTO-GATE methodology refinements have not landed; ux-designer remains held back until they do.

### Specialist composition + verdicts

| Specialist | Pull-Wave | DPC | Notes |
|---|---|---|---|
| qa-lead | APPROVED (0 findings) | NEEDS REVISION (1 BLOCKING + 1 RECOMMENDED) | Found DPC lines 165 + 385 oracle-site residue; verified R12 closures held |
| systems-designer | APPROVED (0 findings) | NEEDS REVISION (1 BLOCKING confirmed from qa-lead; 0 new arithmetic findings) | Confirmed all R12 inscription edits + key derivations arithmetically clean |
| creative-director (synthesis) | APPROVED | NEEDS REVISION (closure path (a): in-session inscription) | Verdict-stamped; ADR-0004 corollary extension prescribed; decomposition trigger ARMED on retirement watch |

### The 1 R13 BLOCKING (closed in-session)

**DPC oracle-site residue: stale `0.9f` literal at struct default + pre-init constant + paired prose example** — closed in-session per CD R13 closure path (a).

- **DPC line 165**: `FDPCFrameState` struct default initializer `float telegraph_window_s = 0.9f;` — stale pre-Path-B. A default-constructed `FDPCFrameState` would have FAILED AC-09a's `telegraph_window_s == 0.94` assertion (R12-fixed at line 820). Edit applied: `0.9f` → `0.94f`.
- **DPC line 385**: `INACTIVE_SNAPSHOT_TW_DEFAULT = 0.9f` pre-init constant + paired stale prose example "Path B floor rise to 0.9s would produce 1.35s ≠ authored OPENER 0.9s". Triple-stale: (i) literal `0.9f` should be `0.94f` (mirrors Path B OPENER key at `t_norm=0.0`); (ii) example FLOOR `0.9s` should be R10d FLOOR `0.70s`; (iii) example arithmetic `1.35s` should be `1.05s` (= `0.70 × 1.5`); (iv) authored OPENER `0.9s` should be `0.94s` (Path B). Edit applied: `0.9f` → `0.94f`, prose updated to "under R10d FLOOR=0.70s that derivation produces 1.05s ≠ authored OPENER 0.94s", + R13 inline annotation explaining root cause.

Root cause: oracle-site sweep gap — exactly the methodology-failure class CD R12 synthesis predicted. R11a-arithmetic's dual-grep patched formula sites; R12 oracle sweep covered (a) AC pass-condition values, (b) test fixture preambles, (c) anchor reference tables; R13 caught the residual sub-classes (d) struct-default initializers + (e) pre-init constant definitions. R12 enumerated 3 oracle sub-classes; R13 added 2. Both sub-classes folded into ADR-0004 §Oracle-Site Sweep Corollary as binding methodology.

### CD R13 binding elements (no design-shaped rulings; methodology-hygiene only)

1. **Closure path (a)**: in-session inscription edit. Two literal sites + paired prose; no design judgment required; faster than hand-back to author. Pattern matches R12 precedent.
2. **ADR-0004 §Oracle-Site Sweep Corollary extension**: appended sub-classes (d) struct-default initializers + (e) pre-init constant definitions, with worked examples from R13 DPC residue. ADR-0004 now has two corollary expansions (R12 + R13) — the oracle-site concept is still being charted; treat future sub-class discoveries as expected.
3. **Decomposition trigger**: ARMED per R11 line 280 (>8 BLOCKING fires). R13 surface = 1 BLOCKING. NOT FIRED. Trajectory R10(19)→R11(8)→R12(7)→R13(1) is monotonically decreasing. CD ruling: **do not retire ARMED status yet** — retirement requires R14 APPROVED on first pass to confirm trajectory holds through inscription closure. Premature retirement loses the safety net during propagation cascade phase, where late-discovered cross-doc drift typically reappears.

### R13 RECOMMENDED (folded into BLOCKING fix at same site)

The paired prose example update at DPC line 385 is folded into the BLOCKING fix; no separate RECOMMENDED action item. The "FLAGGED FOR CD POST-PASS REVIEW" prose at DPC line 717 (TelegraphWindowCurve tuning knob row) is stale — CD R12 Ruling 1 ratified Path B; numerics at line 717 are correct. Prose flag removal deferred to R14 housekeeping or `/propagate-design-change` post-R14.

### Verified PASS items (qa-lead + systems-designer convergent)

- **All R12 inscription edits hold**: DPC lines 782 (canonical fixture Path B keys), 800 (AC-07a 0.70), 801 (AC-07b 0.70), 820 (AC-09a 0.94), 921 (Wilson LCB gate). Pull-Wave line 6 ("near-JND telegraph slowdown") + line 941 (PATH (i) upper bound 0.76s + iteration cap = 1 raise + PATH (iii) sub-check (b) ADVISORY-at-Alpha downgrade + MIN_ESCAPE_SLIPS notation fix).
- **AC-PW-22a state-mix arithmetic** (Pull-Wave line 953): total lifetime 2.30s = 0.70 + 1.43 + 0.15 + 0.016 ✓; LEANING fraction 30.4% ✓; integer state counts 5/10/1/0-1 ✓.
- **Boundary table line 408** (Pull-Wave): `~7.12s = 0.70 + 6.25 + 0.15 + 0.016` at max offset 25.0m + min velocity 4.0 m/s ✓.
- **PATH (i) lower bound 0.82s** (Pull-Wave line 941): `0.27 + 0.25 + (0.15 × 2) = 0.82s` with MIN_ESCAPE_SLIPS=2 (R12 notation fix verified inscribed) ✓.
- **Seam 13 OnDespawnedUserCallback** at `platform-seam-interfaces.md` lines 1591/1657-1669/1725/1749 (R10c closure held).
- **Cross-doc FLOOR=0.70 coherence**: Pull-Wave + DPC + entities.yaml + cross-system-survivability-coordination + game-concept all aligned via R11a-arithmetic. R13 found no new drift.
- **Pool sizing = 23** (Pull-Wave §Implementation Resource Budgets): `16 + 2 + 5 = 23` (R10c closure held).
- **AC-PW-32 K-frame multi-frame math** with per-fps frame_dt explicit (R10c closure held).
- **`0.9f` sweep across DPC GDD**: post-edit grep confirms zero active oracle sites at `0.9f` (only historical R13-annotation reference remains in line 385's "was 0.9f" provenance prose).
- **`= 0.9f` sweep across Pull-Wave GDD**: zero hits (Pull-Wave was clean before R13).
- **Stale `0.65f` / `0.6f` literals across DPC**: zero hits (R12 + R11a-arithmetic closed these).

### Forecast vs observed (calibration data point)

| Review | Forecast BLOCKING | Observed BLOCKING | Calibration |
|---|---|---|---|
| R10a | 0–4 | 19 | 4–5× miss (forecast model declared broken at R11) |
| R11 | 3–8 | 8 (Pull-Wave) + 6 (DPC) = 14 across batch | 2× miss on Pull-Wave; decomposition trigger ARMED but unfired |
| R12 | 1–2 (Pull-Wave) + 0–1 (DPC) = 1–3 | 4 (Pull-Wave) + 3 (DPC) = 7 | ~4× miss; root cause = oracle-site sweep gap |
| **R13** | **0–1 (per CD R12 synthesis)** | **1 (DPC; Pull-Wave clean)** | **CALIBRATED — inside band** |

**R13 is the first calibrated forecast since R7.** The forecast model recovered after the R12 root-cause attribution (oracle-site sweep gap) folded into ADR-0004 as a binding corollary. R13's panel applied the lesson preemptively and caught the residual sub-class within the predicted band.

### R14 forecast (post-R13 in-session closure)

**Terminal review preferred, but minimal R14 confirmation pass recommended** per CD R13 synthesis. R14 scope = solo-mode qa-lead grep verification of the three edit sites + the `0.9f` sweep + ADR-0004 corollary extension landing. Forecast = APPROVED, 0 BLOCKING / 0 RECOMMENDED. Single-specialist pass; not a full lean panel. ~5-minute mechanical confirmation. Trade-off: preserves forecast feedback loop (just recovered) vs adds one cycle of overhead. CD recommends spawn; user can override to terminal-now.

### Files modified by R13 in-session pass

- `design/gdd/difficulty-phase-controller.md` — 2 BLOCKING fixes: line 165 (`0.9f` → `0.94f` struct default) + line 385 (`0.9f` → `0.94f` pre-init constant + paired prose example update + R13 inline annotation). ~+3 lines net.
- `docs/architecture/adr-0004-r11a-dual-grep-methodology.md` — Oracle-Site Sweep Corollary subsection added with R12 + R13 enumerations. ~+50 lines net.
- `design/gdd/reviews/pull-wave-behavior-review-log.md` — this R13 entry. ~+90 lines net.
- `production/session-state/active.md` — STATUS block + Progress + Session Extract update.

### Files NOT modified by R13 (intentional — per R12 housekeeping pattern, user opted out 2026-06-18; same pattern held for R13)

- `design/gdd/reviews/difficulty-phase-controller-review-log.md` — no parallel DPC R13 entry; this Pull-Wave log is the canonical R13 record (R12 housekeeping precedent).
- `design/gdd/systems-index.md` — Pull-Wave / DPC rows not updated this pass (R12 precedent).
- `design/gdd/pull-wave-behavior.md` — Pull-Wave R13 = APPROVED, no edits.
- `design/registry/entities.yaml` — no registry-backed value changes (the R13 fixes were in-GDD constants; entities.yaml `TELEGRAPH_WINDOW_FLOOR_S = 0.70` confirmed clean).
- `docs/architecture/platform-seam-interfaces.md` — no seam changes.
- `design/gdd/player-movement.md` / `design/gdd/input-system.md` / `design/gdd/run-state-machine.md` — sibling GDDs out of R13 scope.
- `design/gdd/wave-spawner-pattern-library.md` — does not yet exist.

### Operative next step (post-R13)

If user accepts terminal-now: **`/propagate-design-change`** to land R10d + R12 + R13 cascade into `design/registry/entities.yaml` (PATH (i) upper bound `TELEGRAPH_WINDOW_FLOOR_MAX_ADMISSIBLE_S = 0.76s` codification if desired; `BARRAGE_SIMULTANEITY_WINDOW_S` 0.3 → 0.35 target alignment; + R13 closure note appended to `cross-system-survivability-coordination-2026-06-11.md`).

If user accepts minimal R14 confirmation: spawn qa-lead solo to grep-verify the 3 sites + ADR-0004 landing; expected APPROVED on first pass; then proceed to `/propagate-design-change` immediately after.

PARALLEL TRACKS unchanged: PM decomposition (per `design/gdd/reviews/player-movement-decomposition-plan-2026-06-16.md`) + Telegraph prototype (highest-risk bet per systems-index) + HISM/ISMC ADR authoring (`docs/architecture/adr-NNNN-pullwave-instanced-renderer.md` — Pull-Wave story-Done blocker, downstream artifact, not GDD-revision item).

### Strategic note

R13 is the **first review in the entire R7→R13 trajectory** to land with (a) calibrated forecast, (b) single BLOCKING within band, (c) closed in-session via inscription, and (d) methodology corollary expanded under control rather than under failure-mode emergency. The R12→R13 transition validates that the methodology-failure-class root-cause attribution (oracle-site sweep gap) was correct: applying the corollary preemptively at R13 caught the next residual sub-class within forecast bounds. The R13 sub-class additions to ADR-0004 are themselves the empirical proof that the corollary structure is the right place to accumulate this kind of methodology evolution.

Pull-Wave + DPC GDDs are now at near-terminal state pending R14 minimal confirmation (recommended) or terminal-now declaration (acceptable). Decomposition trigger ARMED on retirement watch — retirement gated on R14 APPROVED-on-first-pass. Downstream propagation cascade and Wave Spawner GDD authoring become the next operative theaters after R14 (or after terminal-now declaration).

---

## R14 Solo qa-lead Confirmation Pass 2026-06-19

### Trigger + mode

User typed `go` post-R13 housekeeping completion; CD R13 synthesis recommended R14 minimal confirmation over terminal-now to preserve the just-recovered forecast feedback loop. Mode = **solo qa-lead grep verification** — single specialist Agent spawn, no full lean panel, ~5-minute mechanical pass. Forecast = APPROVED, 0 BLOCKING / 0 RECOMMENDED.

### Verdict

**APPROVED** — all 8 checklist items PASS. R13 closure is clean. Forecast confirmed.

### Checklist results

| # | Check | Result |
|---|---|---|
| 1 | DPC:165 `telegraph_window_s` default | PASS — `0.94f` |
| 2 | DPC:385 `INACTIVE_SNAPSHOT_TW_DEFAULT` | PASS — `0.94f` |
| 3 | DPC:385 prose example + R13 annotation | PASS — "1.05s ≠ authored OPENER 0.94s" + R13 inline annotation present |
| 4 | DPC `0.9f` sweep | PASS — 0 active oracle sites (line 32 hit is `0.82s` MID value; line 165 hit is corrected `0.94f`; sole `0.9f` reference is historical-provenance backtick-quoted text in R13 annotation at line 385) |
| 5 | DPC `1.35s` sweep | PASS — 1 hit at line 385 inside R13 historical annotation only |
| 6 | ADR-0004 §Oracle-Site Sweep Corollary | PASS — subsection at correct location (lines 144-194) between §Capture in Review Logs and §Future Tooling Direction; all 5 sub-classes (a)/(b)/(c) R12 + (d)/(e) R13 present with worked examples; (d) attributed to DPC:165 + (e) attributed to DPC:385 |
| 7 | Pull-Wave `= 0.9f` sweep | PASS — 0 hits |
| 8 | DPC:181 semantic consistency | PASS — qualitative prose, no numeric literals; post-R13 default 0.94f matches inactive snapshot `max(0.70, 0.94) = 0.94` |

### Findings

**None.** R14 forecast (APPROVED 0/0) confirmed empirically. No new BLOCKING / RECOMMENDED / NICE-TO-HAVE surfaced.

### Decomposition trigger RETIRED

Per CD R13 binding ruling: "do not retire ARMED status yet — retirement requires R14 APPROVED on first pass to confirm trajectory holds through inscription closure." **R14 landed APPROVED on first pass.** Decomposition trigger status is now **RETIRED**.

Final trajectory: R10(19) → R11(8) → R12(7) → R13(1) → R14(0). Monotonically decreasing across five reviews; terminal at R14. The R11 line 280 trigger threshold (>8 BLOCKING) was approached only at R11 (= 8, did not fire). Subsequent reviews stayed below threshold with decreasing margin. Inscription-density grew across the same trajectory.

### Final R13 + R14 forecast calibration record

| Review | Forecast | Observed | Status |
|---|---|---|---|
| R10a | 0-4 | 19 | MISS 4-5× (model broken) |
| R11 | 3-8 | 8 + 6 = 14 batch | MISS 2× |
| R12 | 1-2 + 0-1 = 1-3 | 4 + 3 = 7 batch | MISS ~4× |
| **R13** | **0-1** | **1** | **CALIBRATED ✓** |
| **R14** | **0** | **0** | **CALIBRATED ✓** |

Two consecutive calibrated forecasts. Forecast model recovered + held through R14. R12 root-cause attribution (oracle-site sweep gap → ADR-0004 corollary) was correct.

### Files modified by R14 pass

- `design/gdd/reviews/pull-wave-behavior-review-log.md` — this R14 confirmation entry; ~+50 lines.
- `production/session-state/active.md` — STATUS + Progress + Session Extract update.

### Files NOT modified by R14 (intentional — confirmation pass, no GDD changes)

- `design/gdd/difficulty-phase-controller.md` — APPROVED at R13; R14 confirmation only.
- `design/gdd/pull-wave-behavior.md` — APPROVED at R13; R14 confirmation only.
- `docs/architecture/adr-0004-r11a-dual-grep-methodology.md` — R13 corollary extension verified at R14.
- `design/gdd/systems-index.md`, `design/registry/entities.yaml`, `docs/architecture/platform-seam-interfaces.md`, sibling GDDs — out of R14 scope.

### Terminal status

**Pull-Wave + DPC both APPROVED as of R14 2026-06-19.** Decomposition trigger RETIRED. Forecast model CALIBRATED (two consecutive). Methodology corollary EVOLVED under control (ADR-0004 sub-classes (d) + (e) added at R13; R14 confirmed no further escalation). The R7→R14 review trajectory is closed for Pull-Wave + DPC.

### Operative next step

**`/propagate-design-change`** to land R10d + R12 + R13 cascade in:
- `design/registry/entities.yaml` — `BARRAGE_SIMULTANEITY_WINDOW_S` 0.3 → 0.35 target alignment (registry note at line 40 confirms value 0.3 still held; R14 APPROVED is the release condition for the hold per R11a-arithmetic 2026-06-18 note "held pending DPC R-Updated re-review confirmation"); optionally codify PATH (i) upper bound `TELEGRAPH_WINDOW_FLOOR_MAX_ADMISSIBLE_S = 0.76s`.
- `design/gdd/reviews/cross-system-survivability-coordination-2026-06-11.md` — R13 + R14 closure note appended.
- `design/gdd/difficulty-phase-controller.md` — line 717 "FLAGGED FOR CD POST-PASS REVIEW" prose retirement (CD R12 Ruling 1 ratified Path B; numerics correct; prose flag stale).
- `design/gdd/wave-spawner-pattern-library.md` (when authored) — forward contract inheritance from Pull-Wave + DPC.

PARALLEL TRACKS unchanged: PM decomposition (per `design/gdd/reviews/player-movement-decomposition-plan-2026-06-16.md`) + Telegraph prototype + HISM/ISMC ADR authoring (`docs/architecture/adr-NNNN-pullwave-instanced-renderer.md` — Pull-Wave story-Done blocker).

## Downstream Artifact — ADR-0006 (Pull-Wave Instanced Renderer — Plain ISMC) AUTHORED 2026-06-24

- **Path**: `docs/architecture/adr-0006-pullwave-instanced-renderer.md`
- **Status**: Accepted (closes OQ-PW-3 / R8 perf-analyst B-3 ADR promotion; Pull-Wave story-Done blocker NOW UNBLOCKED).
- **Trigger**: User typed `next` post-`/clear`; recovered state surfaced operative next step = HISM/ISMC ADR authoring per active.md primary lane after ADR-0005 closure. User chose lane via AskUserQuestion. Authored via `/architecture-decision` skill in lean review mode (TD-ADR Step 5.6 skipped per lean precedent BUT user chose "Delegate Decision to TD" at the procedural pre-check, satisfying GDD line 1034's adjudication-class TD-authority requirement via TD-as-author rather than TD-as-reviewer).
- **Decision (reproduced)**: Plain `UInstancedStaticMeshComponent` (ISMC) for both Pull-Wave render components on ALL mobile platform tiers — no HISM, no platform-tier conditional logic. `WaveMassISMC` (16 instances at PEAK, 3 PerInstanceCustomData slots: LeanChargeIntensity / NearMissEdgeFlash / VoxelDissolveFade). `TrailCubeISMC` (48 instances at PEAK, 1 slot: TrailAlpha). `SetCullDistances(0, 3500 cm)` on both (35 m end, 10 m margin past `SPAWN_PLANE_Z_OFFSET_M` safe upper of 25 m). `SetCastShadow(false)` on both (mobile forward). Wave-mass: `SetCollisionEnabled(ECollisionEnabled::QueryOnly)` for overlap events. Trail-cube: `SetCollisionEnabled(ECollisionEnabled::NoCollision)` — pure visual. Per-tick update path: `UpdateInstanceTransform(..., bMarkRenderStateDirty=false, bTeleport=true)` + `SetCustomDataValue(..., bMarkRenderStateDirty=false)` for all dirty waves, then `MarkRenderStateDirty()` once at end-of-tick per component. `bTeleport=true` per unreal-specialist Risk 2 (mobile forward has TAA + motion blur disabled; motion vectors unused).
- **TD adjudication**: perf-analyst R8 B-3 recommendation (plain ISMC for both) accepted in full. HISM rejected because (i) no occluders exist in the fixed-camera 5m-wide mobile track scene, (ii) all instances are inside the frustum every frame, (iii) all 16 + 48 = 64 instances move every tick (worst case for HISM cull-tree rebuild). HISM rebuild estimate 0.05–0.2 ms/tick = 20–80% of AC-PW-22a's 0.25 ms total Pull-Wave per-tick budget with zero rendering benefit.
- **Alternatives documented + rejected**: (Alt 2) HISM for both — rejected on AC-PW-22a budget; (Alt 3) mixed HISM+ISMC — rejected (inherits HISM's tick-budget cost on whichever component receives it + doubles verification surface); (Alt 4) platform-tier conditional — rejected (no tier-specific HISM benefit on a no-occluder scene; doubles maintenance + verification + test matrix).
- **Engine specialist gate (Step 5.5 — unreal-specialist) verdict**: CONCERNS. Architecturally affirms plain ISMC for both. Surfaced 1 Decision-level change (`bTeleport=false` → `bTeleport=true` for constant-velocity waves on mobile forward; Risk 2) + 6 supplementary risks (1 HIGH on physics-body sync, 2 MEDIUM on multi-despawn index cascading + bTeleport safety, 4 LOW on QueryOnly/NoCollision/GPU upload/shader register pressure) + 4 additional UE 5.7 source verifications (items 6–9 added to TD's list of 5, total 9). User chose "Apply all" — all specialist findings folded into ADR's Engine Compatibility + Consequences → Risks + Key Interfaces code block (`bTeleport=true` + `INDEX_NONE` sentinel guards + multi-despawn comment block).
- **GDD sync landed in same pass (Step 5.7 of skill)**: `TrailCube_ISMC` → `TrailCubeISMC` across 5 occurrences in 3 sites in `design/gdd/pull-wave-behavior.md` (lines 658 × 2, 756 × 1, 1034 × 2). Pre-rename GDD prose used PascalCase-with-underscore (`TrailCube_ISMC`); UE naming convention for UPROPERTY members is PascalCase-no-underscore (matches `WaveMassISMC`, `UWaveSpawnerSubsystem` style). Sub-class (b) narrative drift per ADR-0004 §Oracle-Site Sweep Corollary — caught at ADR authoring time. Approved by user via AskUserQuestion.
- **Architecture registry updated**: 4 new stances appended to `docs/registry/architecture.yaml` per Step 6 user approval — (i) `api_decision: pullwave_wave_renderer → UInstancedStaticMeshComponent`; (ii) `forbidden: HISM_for_pullwave_renderers`; (iii) `forbidden: MaterialInstanceDynamic_per_pullwave_instance`; (iv) `forbidden: bTeleport_false_for_constant_velocity_pullwave_update`. User declined the optional renderer-subset performance_budget entry (overlapped with AC-PW-22a ownership; Pull-Wave GDD already canonical). Registry `last_updated` already at 2026-06-24 from ADR-0005 pass; no further bump needed.
- **Files modified by ADR-0006 authoring pass**:
  - `docs/architecture/adr-0006-pullwave-instanced-renderer.md` (NEW, ~430 lines)
  - `design/gdd/pull-wave-behavior.md` (5-occurrence sync `TrailCube_ISMC` → `TrailCubeISMC`)
  - `docs/registry/architecture.yaml` (4 stances appended)
  - `design/gdd/reviews/pull-wave-behavior-review-log.md` (this entry)
  - `production/session-state/active.md` (STATUS block update — pending)
- **Files NOT modified by ADR-0006 authoring pass (intentional)**: `design/gdd/systems-index.md` (no Pull-Wave row change — verdict + closure trajectory unchanged; Pull-Wave + DPC R14-closed cascade NOT re-opened); `docs/architecture/platform-seam-interfaces.md` (no new seams introduced by renderer-class decision; Seam 13 unaffected); `design/gdd/wave-spawner-pattern-library.md` (no Wave Spawner-side cascade — ADR-0006 affects per-actor renderer components but does not change pool sizing, lifecycle, or admission semantics); `design/gdd/difficulty-phase-controller.md` / `design/gdd/run-state-machine.md` / sibling GDDs (out of scope); `design/registry/entities.yaml` (no registry-backed value changes — `SPAWN_PLANE_Z_OFFSET_M` safe range cited as input, not modified; `MAX_CONCURRENT_WAVES_CAP` cited as input, not modified).
- **Pull-Wave Epic story-Done blocker (OQ-PW-3) NOW UNBLOCKED.** Both Pull-Wave story-Done blockers (OQ-PW-3 HISM/ISMC + ADR-0005 Wave Spawner hosting sibling) now Accepted.
- **Strategic notes**:
  - ADR-0006 establishes the precedent for **adjudication-class** downstream artifact authoring (distinct from ADR-0005's downstream-artifact-codification). Process change: TD spawned via Agent as Decision-section AUTHOR (not just reviewer at the lean-mode-skipped Step 5.6). Satisfies CLAUDE.md §1 vertical-delegation rule for adjudication-class decisions even in lean review mode.
  - `bTeleport=true` Decision-level change surfaced by engine-specialist gate is a textbook example of why Step 5.5 should never be skipped regardless of review mode. TD's draft was sound at the architecture level but missed a mobile-forward-specific optimization that the engine specialist caught. The `forbidden_pattern: bTeleport_false_for_constant_velocity_pullwave_update` registry entry preserves this knowledge so future Pull-Wave-touching code is constrained at the registry-check layer.
  - The `TrailCube_ISMC` → `TrailCubeISMC` GDD rename is the second instance of ADR-0004 §Oracle-Site Sweep Corollary sub-class (b) drift caught at ADR authoring time (first was ADR-0005's `OnPostLoadMap` → `PostLoadMapWithWorld` 8-site sync). Pattern: Step 5.7 GDD sync check has now caught 2 sub-class (b) drifts in 2 ADRs, validating it as a high-yield late-catch surface.
- **Operative next step (recommended primary)**: `/architecture-review` in a fresh `/clear` session — validates ADR-0005 + ADR-0006 coverage against the Pull-Wave GDD + Wave Spawner GDD. Skill explicitly forbids running `/architecture-review` in the same session as `/architecture-decision`. Parallel tracks unchanged: PM decomposition execution (per `design/gdd/reviews/player-movement-decomposition-plan-2026-06-16.md` §6, separate `/clear` session, ~3-5 h); Telegraph System prototype (highest-risk bet per systems-index); Wave Spawner R4 (terminal-now per qa-lead R3 recommendation, OR solo qa-lead grep confirmation per Pull-Wave R14 precedent); RSM `/consistency-check` pass (Wave Spawner R2a-2 DPC `OnPostTickFrameStatePublished` forward contract + FC-1/FC-2/FC-3 from 2026-06-23 propagation pass).

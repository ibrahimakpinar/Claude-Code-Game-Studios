# PM-Mechanics R12a Scoped Revision Brief

**Date**: 2026-07-01 (filename dated 2026-06-28 to match sub-GDD `[R12a-PENDING]` pointer references authored during Step 2 PASS 4–8; internal Date field reflects actual authoring session)
**Target document**: `design/gdd/player-movement-mechanics.md` (post-decomposition; currently at Step 3 forward-contracts COMPLETE 2026-07-01)
**Authoring mode**: scoped author revision (NOT in-session patch — CD-mandated handoff per R7 same-session-bias precedent)
**Authority**: PM decomposition plan §7.1 (`design/gdd/reviews/player-movement-decomposition-plan-2026-06-16.md`) + R11 BLOCKING assignment matrix §5
**Pre-revision gate**: PM decomposition Steps 1–3 COMPLETE (structural split + monolith-lift content + cross-sub-GDD forward contracts). Step 4 R12a briefs authored 2026-07-01 (this document). Step 5 systems-index PM row split queued same session.

---

## 0. Why this brief exists

The PM monolith `design/gdd/player-movement.md` R11 fresh-context re-review (2026-06-16) returned **11 BLOCKING** — exceeding the CD-set decomposition trigger (`>8`). CD recommended decomposition into 3 sub-GDDs by failure-domain seams. Mechanics sub-GDD inherits **4 R11 BLOCKING items** originally scoped as mechanics-owned: B-F6-1, B-F6-2, B-F6-3, B-F6-4 (per decomposition plan §5). **B-F6-3 was CLOSED at PM decomposition Step 2 PASS 7 (2026-06-29)** via the TickComponent-prologue definition-site relocation to `player-movement-platform.md` §4 F-PROLOGUE — mechanics-side caller updates authored at PASS 7 (F-2 pseudo-code local clamp retired + F-6 comment rewritten + cross-sub-GDD invariant note in F-2 prose). The remaining 3 mechanics R11 BLOCKINGs (B-F6-1, B-F6-2, B-F6-4) require R12a author decisions per plan §7.1.

This brief is the scoped R12a author handoff for the mechanics sub-GDD. It enumerates the DR-* author decisions the R12a pass must close, cites the extensive `[R12a-PENDING]` context already embedded in `player-movement-mechanics.md` (authored during Step 2 PASS 4–8 to preserve monolith prose lift + R11a §6.1 / §6.2 interim solutions), and sets a calibrated R12 fresh-review forecast.

**Format discipline**: this brief is **light-with-pointers** by design. The sub-GDD already carries the full defect context, option enumeration, R11a-interim solutions, tuning-knob derivations, and worked-example arithmetic for each DR-* — in the F-6 formula subsection + Tuning Knobs + Authored Asset Contracts + AC-F6-A/B/C/D/E. Duplicating that content in this brief would create divergence-risk against the sub-GDD authoritative text. The brief's job is to enumerate what R12a MUST decide + where the decision lands + what verification the decision requires. Rationale confirmed by pre-authoring advisor round (2026-07-01).

**R12 fresh-review forecast**: **1–3 BLOCKING** (single-domain sub-GDD; per plan §7.1 calibrated forecast band). Decomposition trigger per sub-GDD: `>5 BLOCKING`.

**Validation criteria (CD-set)** — R12 succeeds if:
- BLOCKING count within 1–3 forecast band.
- All 3 open (B-F6-1, B-F6-2, B-F6-4) inherited R11 BLOCKINGs have documented closure paths in the sub-GDD (via DR-* decisions authored below).
- **B-F6-3 CLOSED framing survives fresh-context R12 review** (grep gate: sub-GDD §4 F-2 prologue + F-6 comment + Migration Plan cross-references remain word-consistent with `player-movement-platform.md` §4 F-PROLOGUE single-source contract).
- Every `[R12a-PENDING]` marker in `player-movement-mechanics.md` is either resolved (decision authored, marker removed) or explicitly deferred with an updated fallback-behavior stance and traceability trail.
- No new failure-domain surface appears (a 4th BLOCKING cluster within mechanics scope would trigger further scope tightening).

---

## 1. In-scope R12a author decisions (3 DR-*)

Each decision below carries: the R11 BLOCKING it closes, the sub-GDD landing site(s), the option enumeration (cross-referenced from the sub-GDD — not duplicated), the domain authority, and CD recommendations where applicable.

### 1.1 DR-F6-FADE — F-6 fade-out tick-1 multiplier curve (closes B-F6-1)

**Defect**: R11a §6.1 (R11a-3 header decision) authored a 2-frame fade-out with linear ramp `1.0 → 0.5 → 0.0` multiplier on Override capture (SETTLED→SLIPPING triggers F-6 timer kill; F-6 contributions decay over 2 ticks). Tick-1 multiplier = 1.0 means 100 % of the frozen F-6 magnitude is still applied on the tick immediately after Override fires — this is the additive-opposition failure mode the "Override" (§5.1 (a)) choice was authored to avoid. New tween's F-5 opening lean composites against a full-magnitude frozen F-6 lean in the wrong direction for a full tick. Sub-GDD line 1294 confirms "The three lean angle outputs include the fade-out contribution at multiplier 0.5" (tick-2 behavior is correct); tick-1 remains at 1.0.

**Author decision required**: select one of the following fade-out curves:
- **(a) Cosine ramp** `1.0 → cos(π/4) ≈ 0.707 → 0` over 2 ticks. Reduces tick-1 multiplier to ~0.71 (~29 % magnitude reduction on the additive-opposition-critical tick). Preserves the anti-snap contract (LEAN_CURVE_ASSET load-bearing principle: no head-snap discontinuity via zero-multiplier jump).
- **(b) Shorter 1-frame fade with smaller magnitude** — cap F-6 magnitude at 50 % on the Override-fire tick, drop to 0 next tick. Halves tick-1 additive contribution; may reintroduce a smaller snap at tick 2.
- **(c) Frame-1 multiplier hard-cap based on F-6 magnitude threshold** — if `|f6_lean_body| > threshold_deg` on Override fire, apply an additional cap multiplier `threshold_deg / |f6_lean_body|`; otherwise pass through at 1.0. Preserves anti-snap for small F-6 magnitudes; caps additive-opposition for large ones.
- **(d) Accept the pre-R12a `1.0 → 0.5 → 0.0` linear ramp** and re-scope the anti-additive contract as "≤50 % additive-opposition budget" — documenting the tick-1 100 % contribution as within tolerance. Requires rewriting AC-F6-B to assert the budget rather than eliminate the additive-opposition.

**CD recommendation**: **(a) — cosine ramp**. Rationale: two-frame smoothness preserved (2-tick decay to zero); tick-1 multiplier reduced from 1.0 → ~0.71 without introducing a new hard-cap tuning-knob; the anti-snap contract holds (no zero-multiplier jump between tick-0 and tick-1). Advisor and R12 reviewer should verify the AC-F6-B rewrite exactly asserts the tick-by-tick behavior for the chosen curve.

**Sub-GDD landing sites**:
- §4 F-6 lean formula subsection (`player-movement-mechanics.md` — pseudo-code snippet around lines 763–770 uses `slipping_mult` for the EC-15 damping; the Override fade-out multiplier is a separate factor and lives in the same F-6 body).
- §4 Authored Asset Contracts — verify EDGE_ABSORB_CURVE_ASSET t=0 constraint remains at 0.35 (peak) if the fade-out curve interacts with the asset curve sampling.
- §8 Acceptance Criteria — AC-F6-B (currently asserts tick-by-tick fade behavior for the linear ramp; rewrite for the chosen curve).
- §7 Tuning Knobs — if a new fade-curve tuning knob is introduced (e.g., `F6_OVERRIDE_FADEOUT_CURVE_ASSET` UCurveFloat), add a Tuning Knobs row.
- Header decision block — R12a-1 or similar to record the closed decision + brief-deviation marker if the chosen curve departs from CD recommendation.

**Domain authority**: game-designer + systems-designer (co-sign — game-designer owns Player Fantasy anti-snap contract; systems-designer owns tick-by-tick arithmetic).

**Post-decision propagation**: Presentation sub-GDD `player-movement-presentation.md` §6 forward contract "F-6 fade-out ≥ commitment-tell hold" (mechanics §6 line 1047 lockstep) re-validates against the chosen curve — if fade-out effective duration changes, presentation re-runs AC-COMMIT-FLASH-CADENCE setup.

### 1.2 DR-F6-EC15 — EC-15 formula re-derivation at correct worst case (closes B-F6-2)

**Defect**: R11a §6.2 (R11a-4 header decision) authored `EC15_F6_DECAY_COEFFICIENT = 0.7` (safe range 0.6–1.0) with derivation anchored on `TweenProgress = 0.85 + edge_absorb_progress = 0.10` (F-5 Phase 3 settle dip boundary). AC-F6-E asserts no clamp engagement at that boundary sample. R11 review found the derivation targeted the wrong worst case — LeanCurve plateau region `TweenProgress ∈ [0.30, 0.65]` produces higher F-5 head values that the current coefficient does NOT protect against. **Folded-in**: MAX_SLIP_DT_S = 0.020 × 30 fps survivability invariant violation (F-BARRAGE-SURVIVABILITY-INVARIANT at 30 fps with MAX_SLIP_DT_S at safe-range floor 0.020 produces `SLIP_TWEEN × MIN_ESCAPE_SLIPS + REACTION_BUDGET > TELEGRAPH_WINDOW_FLOOR_S` — mechanics-owned constant range needs constraint against platform-owned frame-quantized table).

**Author decision required** (compound — pick BOTH an EC15 solution AND a MAX_SLIP_DT_S constraint):

**Part (i) — EC15 re-derivation**: select one of:
- **(a) Non-linear multiplier curve** — replace `1.0 − TP × c` with `1.0 − TP^k × c` for some `k > 1` (steeper at low TP). Preserves shape at high TP (current AC-F6-E boundary); adds protection at low-TP plateau. Requires new tuning-knob `EC15_F6_DECAY_EXPONENT` (default `k = 2` candidate); new AC-F6-F asserts no clamp engagement at TP=0.30 + TP=0.65 boundary samples.
- **(b) Restrict F-6 firing on F-4 path to TP ≥ 0.50** — eliminates the low-TP worst case by not firing F-6 during F-5 plateau region. Preserves R10a §1.1 F-6 dual-path (path 1 F-4 mid-tween + path 2 Rule 1 SETTLED); path 2 SETTLED is unaffected. Simpler than (a) but changes F-4 → F-6 conditional; risks reducing edge-absorb feel in the mid-tween window.
- **(c) Accept clamp engagement at TP=0.30–0.65 as design-acceptable head-snap-under-load** — tighten AC-F6-E to assert clamp engagement at TP=0.30 + TP=0.65 as expected behavior (not a defect); document the design-acceptable head-snap in Player Fantasy prose. Requires narrative-director sign-off on the "brief head snap under simultaneous slip + tween-plateau edge-absorb" being acceptable.

**Part (ii) — MAX_SLIP_DT_S safe-range constraint**: select one of:
- **(x) Tighten MAX_SLIP_DT_S safe-range floor** from 0.020 → 0.030 (or another value derived from F-BARRAGE-SURVIVABILITY-INVARIANT at 30 fps). Trade: less tight DT clamp at 30 fps → slightly more visible frame-hitch impact; but survivability arithmetic holds.
- **(y) Document Hardware Contract dependency** — accept MAX_SLIP_DT_S = 0.020 floor AND declare 30 fps as an "advisory-only" framerate (60 fps hard minimum for the survivability invariant). Requires cross-sub-GDD update to `player-movement-platform.md` §3 Hardware Contract framerate matrix.
- **(z) Both (x) tightening AND (y) 60 fps hard-min** — most conservative; least tuning flexibility.

**CD recommendation**: **Part (i) = (a) non-linear multiplier**; **Part (ii) = (x) tighten MAX_SLIP_DT_S floor**. Rationale: (a) preserves Player Fantasy (edge-absorb tell fires on F-4 path regardless of TP; no reduced feel); (x) preserves 30 fps advisory-support at the cost of a slightly looser DT clamp. Combined margin at TP=0.30–0.65 + tightened MAX_SLIP_DT_S restores the ≥5× measurement-tolerance clamp-avoidance margin authored at R11a §6.2 for TP=0.85.

**Sub-GDD landing sites**:
- §4 F-6 lean formula subsection (pseudo-code lines 753–770 — `slipping_mult` formula updates).
- §7 Tuning Knobs — `EC15_F6_DECAY_COEFFICIENT` row (update safe range if (a) chosen); new `EC15_F6_DECAY_EXPONENT` row if (a) chosen; `MAX_SLIP_DT_S` row (tighten floor if (x) chosen).
- §8 Acceptance Criteria — AC-F6-E (current TP=0.85 boundary) preserved; new AC-F6-F asserts TP=0.30 + TP=0.65 boundary if (a) chosen; if (c) chosen, AC-F6-E rewritten to assert clamp-engagement-as-expected.
- Cross-sub-GDD forward contract — mechanics §6 line 1057 (F-BARRAGE-SURVIVABILITY-INVARIANT input-flow) lockstep with `player-movement-platform.md` §4 F-BARRAGE-SURVIVABILITY-INVARIANT-CORNERS matrix if (x) or (y) chosen.

**Domain authority**: systems-designer + game-designer + (if (y) chosen) technical-director (co-sign on 60 fps hard-min declaration).

**Post-decision propagation**: platform sub-GDD survivability table re-tuning per R10a §B-LEAN-tension precedent. Wave Spawner + mechanics + platform lockstep re-tuning if MAX_SLIP_DT_S safe-range changes.

### 1.3 DR-F6-ASSET — EDGE_ABSORB_CURVE_ASSET reference type (closes B-F6-4)

**Defect**: R10a §1.1 authored `EDGE_ABSORB_CURVE_ASSET` as a `UCurveFloat` **soft reference** (Tuning Knobs row line 1077). Soft ref + raw dereference + insufficient null guard was the pre-R11a Shipping crash defect. R11a authored a warning-level null guard + linear-decay fallback (Authored Asset Contracts lines 946–950 log warnings; F-6 body falls back to `y = 0.35 × (1 − x)`). R11 review flagged that: (a) the reference type remains `soft` (per line 1077) — mismatched against SLIP_CURVE_ASSET + LEAN_CURVE_ASSET which are hard references; (b) the fallback is warning-level, but a Shipping build with a null curve + no debug console will silently degrade F-6 to linear decay (acceptable but undocumented in AC-F6-A/B/C/D).

**Author decision required**: select one of:
- **(a) Upgrade to hard reference** matching SLIP_CURVE / LEAN_CURVE pattern. **CD recommendation** (per plan §5 B-F6-4 row + §7.1 recommendation). Closes the Shipping crash defect cleanly (hard ref cannot be null at runtime — asset load guaranteed at cook time; missing asset is a cook-time error). Matches the asset-contract pattern already established for the other two curve assets.
- **(b) Retain soft reference + `LoadSynchronous` at BeginPlay + null guard + linear fallback** (current state). Preserves lazy-load benefit (asset not loaded until PM tick begins); requires explicit `LoadSynchronous` in BeginPlay + null-check discipline throughout F-6 body. Higher runtime complexity; matches no other PM curve asset.

**Post-decision propagation on choice (a)**:
- Update Tuning Knobs row line 1077: `UCurveFloat soft reference` → `UCurveFloat hard reference` (or the appropriate `TObjectPtr<UCurveFloat>` UE 5.7 pattern per `.claude/docs/technical-preferences.md`).
- Update Authored Asset Contracts EDGE_ABSORB_CURVE_ASSET subsection: retire the null-guard prose; keep the t=0 peak-at-0.35 + t=1 zero validation checks (author-side asset contract still holds).
- Retire the "linear fallback" prose in the F-6 body — hard ref cannot be null, so no fallback path executes. Preserve the linear-fallback prose only as historical footnote if desired.
- Sub-GDD Migration Plan (if one exists) documents the soft-→hard-ref migration.

**Post-decision propagation on choice (b)**: preserve current sub-GDD text; AC-F6-A/B/C/D setup notes explicitly reference the fallback path as a tested branch (add an AC-F6-A-FALLBACK sub-case asserting linear-fallback correctness).

**Sub-GDD landing sites**:
- §4 Authored Asset Contracts EDGE_ABSORB_CURVE_ASSET subsection (~lines 925–955).
- §7 Tuning Knobs — EDGE_ABSORB_CURVE_ASSET row (line 1077).
- §4 F-6 lean formula — null-guard prose retirement or preservation.
- §8 Acceptance Criteria — AC-F6-A/B/C/D setup notes.

**Domain authority**: systems-designer + unreal-specialist (co-sign — asset-reference pattern is UE-idiom-owned; systems-designer owns the F-6 contract that depends on the asset being present).

---

## 2. Out of scope for R12a (mechanics)

- **B-F6-3 (F-6 effective_dt SETTLED-path leak)** — **CLOSED at PM decomposition Step 2 PASS 7 (2026-06-29)**. Caller-side edits authored: (a) F-2 pseudo-code local clamp `effective_dt = clamp(DeltaTime, 0.0f, MAX_SLIP_DT_S)` RETIRED (sub-GDD line ~571 shows the retirement + upstream-consumption comment); (b) F-6 comment rewritten to reference the platform-side definition (sub-GDD line 709 references B-F6-3 closure); (c) cross-sub-GDD invariant note in §4 F-2 prose updated to "B-F6-3 CLOSED" framing (sub-GDD line 572). Definition site lives at `player-movement-platform.md` §4 F-PROLOGUE (single-source `raw_dt = FApp::GetDeltaTime()` + `effective_dt = clamp(raw_dt, 0.0f, MAX_SLIP_DT_S)`). R12 fresh-context reviewer verifies the closure via grep gate: mechanics §4 F-2 + F-6 prose + platform §4 F-PROLOGUE + cross-sub-GDD forward-contract Step 3 execution (2026-07-01) all word-consistent on the single-source contract. See `player-movement-platform-r12-author-brief-2026-06-28.md` §2 for the platform-side CLOSED framing.
- **Cross-system propagation to Pull-Wave / DPC / Wave Spawner / registry**: forward contracts documented in sub-GDD §6 Dependencies. Cross-system revisions triggered by DR-* closure land as `/propagate-design-change` runs after R12 APPROVED.
- **Polish-phase items**: EDGE_ABSORB_CURVE_ASSET authored curve tuning (polish-phase per Tuning Knobs "Authoring is polish-phase"); Player Fantasy playtest validation of edge-absorb feel post-DR-F6-EC15 decision.
- **Mechanics implementation code** — Sprint deliverable, not R12a scope. R12a produces the design contract; ADR-0009 (PM hosting) + implementation ADR (if any) come later.
- **Registry OQ-7 yaml→C++ generator pipeline** — MIN_ESCAPE_SLIPS + related constants; scoped platform-side per `player-movement-platform.md` §9. Mechanics inherits at platform-side landing.

---

## 3. Verification approach

R12 fresh-context `/design-review design/gdd/player-movement-mechanics.md` performed in a `/clear` session AFTER this brief's DR-* decisions are authored into the sub-GDD. Reviewer weights:

- **Every `[R12a-PENDING]` marker resolved** — grep the sub-GDD post-R12a; markers should be absent (or explicitly deferred with a documented traceable rationale).
- **B-F6-3 CLOSED framing survives grep gate**: mechanics §4 F-2 prologue + F-6 comment + Migration Plan cross-references remain word-consistent with `player-movement-platform.md` §4 F-PROLOGUE single-source contract (see §2 above).
- **DR-F6-FADE tick-by-tick arithmetic**: AC-F6-B setup + expected result asserts the chosen curve's tick-1 + tick-2 multipliers with explicit numeric values; F-6 pseudo-code body implements the same values byte-for-byte.
- **DR-F6-EC15 worst-case coverage**: AC-F6-E preserved for TP=0.85; new AC-F6-F (if (a) chosen) covers TP=0.30 + TP=0.65 boundary; MAX_SLIP_DT_S safe-range change (if (x) chosen) propagates lockstep to platform §4 F-BARRAGE-SURVIVABILITY-INVARIANT-CORNERS matrix + Wave Spawner forward contracts.
- **DR-F6-ASSET reference-type consistency**: Tuning Knobs row line 1077 reference type matches SLIP_CURVE + LEAN_CURVE pattern (if (a) chosen); Authored Asset Contracts subsection prose matches; F-6 pseudo-code body matches (no dead null-guard code if (a) chosen).
- **§8 AC count consistency**: post-R12a mechanics AC count reflects DR-F6-FADE (AC-F6-B rewrite), DR-F6-EC15 (AC-F6-E preserve + AC-F6-F new if (a)), DR-F6-ASSET (AC-F6-A/B/C/D setup notes update).
- **Forecast honesty**: if R12 exceeds 3 BLOCKING, invoke plan §10 decomposition-trigger-per-sub-GDD (`>5 BLOCKING`). If R12 returns 0 BLOCKING with the mechanics sub-GDD demonstrably calibrated (all DR-* closed with traceable rationale), mechanics reaches APPROVED and ADR-0009 (PM hosting) unblocks for the mechanics-scope surface.

---

## 4. Reference chain

- **Decomposition plan**: `design/gdd/reviews/player-movement-decomposition-plan-2026-06-16.md` §3.1 (mechanics scope) + §5 (BLOCKING assignment matrix — 4 mechanics rows, B-F6-3 CLOSED per §2 above) + §7.1 (this brief's outline authority).
- **Monolith history**: `design/gdd/player-movement.md` (R11 review at `design/gdd/reviews/player-movement-review-log.md` — R11 entry) + R10 brief `design/gdd/reviews/player-movement-r10-author-brief-2026-06-11.md` + R11 brief `design/gdd/reviews/player-movement-r11-author-brief-2026-06-15.md` (format precedent).
- **Sub-GDD authoritative text**: `design/gdd/player-movement-mechanics.md` (Step 2 PASS 4–8 authored; PASS 7 F-2/F-6 B-F6-3 closure caller-side) + Step 3 forward-contracts `design/gdd/reviews/player-movement-decomposition-step3-verification-2026-06-30.md` (verification of §6 subsection) + 2026-07-01 Step 3 execution (this session's mechanics §6 line 1042–1064 + presentation §6 line 392–411 + platform §6 line 453–473 authoring).
- **Cross-sub-GDD lockstep briefs** (same-day 2026-07-01):
  - `player-movement-presentation-r12-author-brief-2026-06-28.md` (presentation DR-* including DR-PRES-DUCK ducking timing that reads mechanics EDGE_ABSORB_DURATION_S)
  - `player-movement-platform-r12-author-brief-2026-06-28.md` (platform DR-* + B-F6-3 CLOSED framing on definition side + F-BARRAGE-SURVIVABILITY-INVARIANT-CORNERS lockstep if DR-F6-EC15 (x) chosen)

---

**Brief authored by**: PM decomposition Step 4 execution session (2026-07-01) per plan §6 Step 4 authority.
**Brief authority**: binding for the R12a author revision pass on `player-movement-mechanics.md`. Author may override any option enumeration or CD recommendation with documented rationale recorded in the sub-GDD's header decision block.
**Brief execution gate**: R12a authoring occurs in a `/clear` fresh-context session AFTER this brief is durable on disk. R7 same-session-bias precedent applies — do not fold R12a authoring into the same session that produced the brief.

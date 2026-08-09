# Cross-System Coordination — F-BARRAGE-SURVIVABILITY-INVARIANT Adjudication

*Date: 2026-06-11*
*Adjudicator: creative-director*
*Gate: CD-CROSS-SYSTEM-COORDINATION*
*Triggered by: PM R9 review log line 132–134 — user-chosen path "Open cross-system coordination first"*

---

## Verdict: BIND (c) — Wave Spawner cook-time triplet exclusion

`SLIP_TWEEN_DURATION_S = 0.15s`, `TELEGRAPH_WINDOW_FLOOR_S = 0.65s`, `MIN_ESCAPE_SLIPS = 2`.

Worst-case margin under (c): `0.15 × 2 + 0.25 = 0.55 ≤ 0.65 → 100 ms = 6 frames @ 60 fps`.

---

## 1. Adjudication of the empirical disagreement

**Claim under test (game-designer)**: voluntary player slip during 0.65s telegraph window can move the player into a configuration where the spawn-time-chosen barrage requires 3 escape slips.

**Counter-claim (systems-designer)**: after excluding the 3 all-consecutive triplets, NO surviving triplet requires >2 escape slips from ANY player lane on a 5-lane track. Voluntary slipping does not break the property because the property is global over player lanes.

**Enumeration** (5 lanes; player escapes by adjacent slips; occupied lanes are not passable as a destination):

| Triplet | Safe lanes | Worst-case escape distance from any of 5 lanes |
|---|---|---|
| {0,1,3} | {2,4} | 2 (from lane 0) |
| {0,1,4} | {2,3} | 2 (from lane 0) |
| {0,2,3} | {1,4} | 1 |
| {0,2,4} | {1,3} | 1 |
| {0,3,4} | {1,2} | 2 (from lane 4) |
| {1,2,4} | {0,3} | 1 |
| {1,3,4} | {0,2} | 2 (from lane 4) |

**Worst case across all 7 surviving triplets × 5 player lanes = 2 slips.**

**systems-designer is empirically correct. game-designer's objection conflates two different versions of (c):**

- **(c-naive)**: spawn-time check verifies the player's current lane is safe. This IS broken by voluntary slipping — game-designer's logic holds against this version.
- **(c-actual)** as systems-designer proposed: pure geometric cook-time triplet exclusion (no player-position input at all). The safety property is uniform over all 5 player lanes, so voluntary slipping moves the player from one ≤2-slip-escapable position to another ≤2-slip-escapable position. Pull-Wave Rule 4 (target_lane locked at SPAWNED) is irrelevant — the check has no runtime input.

The "the pull-wave never lies — its curve is published before its motion begins" Player Fantasy is unaffected. The Wave Spawner authors the spatial configuration at cook time; player position never enters the selection logic.

**game-designer's reasoning failure is the design lesson here, not the verdict.** When evaluating a Wave Spawner forward contract, the question to ask first is "is this contract a function of player state, or a function of authored content only?" (c-actual) is the latter. The 0.25 minute spent verifying which version was on the table would have surfaced the convergence.

---

## 2. Boundary-value math and the REACTION_BUDGET ceiling

systems-designer's boundary table is the discriminating evidence. I verified each at REACTION_BUDGET = 0.25s (the registry's documented safe-range upper bound):

| Option | Math @ REACT=0.25 | Margin | Verdict |
|---|---|---|---|
| (a) SLIP_TWEEN=0.13, FLOOR=0.65 | 0.13×3 + 0.25 = 0.64 | 10ms (0.6 frame) | sub-1-frame cliff — relocates problem |
| (b) SLIP_TWEEN=0.15, FLOOR=0.68 | 0.15×3 + 0.25 = 0.70 | −20ms | BREAKS invariant |
| (c) SLIP_TWEEN=0.15, FLOOR=0.65, M=2 | 0.15×2 + 0.25 = 0.55 | 100ms (6 frames) | survives with margin |

**Is REACT=0.25 a legitimate operating point that must hold?**

The registry constant `REACTION_BUDGET` entry (lines 774–800) explicitly defines `[0.20, 0.25]` as the safe range and states "Upper bound (0.25s) covers degraded-attention scenarios; raising past 0.20s forces TELEGRAPH_WINDOW_FLOOR_S raise." Pillar 5 ("Skill Is Visible — no deaths attributable to latency, unclear collision, or the device") points the same direction: the player at degraded attention deserves the same survivability guarantee as the player at peak focus. A game that says "we promise survivability when you're sharp; we silently revoke it when you're tired" is a Pillar 5 violation by construction.

**Premise I am binding this decision on**: the invariant must hold across the FULL REACTION_BUDGET safe range, not only at the default 0.20. If a future reviewer disagrees and treats 0.25 as a degraded-scenario allowance, (a) returns to viability and the decision should be re-opened. I am flagging this premise explicitly so it can be challenged once.

Under this premise, (c) is the only option that does not relocate the cliff. (a) and (b) both move it within the safe range; (c) eliminates it.

---

## 3. Registry drift — confirmed, requires correction regardless of outcome

`design/registry/entities.yaml` line 150: `TELEGRAPH_WINDOW_FLOOR_S value: 0.68` with provenance note "user adjudication chose Path b."

`design/gdd/reviews/player-movement-review-log.md` line 146: "User-chosen path (2026-06-11): Open cross-system coordination first. PM revision deferred until DPC + PM + Pull-Wave session resolves the survivability invariant."

systems-designer's hidden-defect catch is correct. The R8 in-session revision pass advanced the registry to 0.68; the R9 fresh-context re-review reopened that adjudication, and the user explicitly chose coordination over committing to Path B. The registry is stale by two reviews.

**Correction required regardless of which option is bound**:
- Revert `TELEGRAPH_WINDOW_FLOOR_S` to 0.65 with the note text rewritten to reflect R9's "coordination chosen" state.
- Revert `BARRAGE_SIMULTANEITY_WINDOW_S` provenance text to drop the 0.34 derivation (which was downstream of the now-reverted 0.68).
- After this adjudication binds: re-set `MIN_ESCAPE_SLIPS = 2` (which is currently encoded as `MIN_BARRAGE_LANE_SEPARATION` notes referencing M=3); add a new explicit constant `MIN_ESCAPE_SLIPS = 2` with the derivation pinned to (c) — separation=1 + R7 B8 BLOCKING + 5 lanes.

---

## 4. The hybrid option

game-designer's proposal: `SLIP_TWEEN=0.14s, FLOOR=0.66s`. At REACT=0.25: `0.14 × 3 + 0.25 = 0.67 > 0.66`. **Hybrid breaks the invariant at REACT=0.25.**

To survive REACT=0.25, the hybrid would need to add a third side-move — either MIN_ESCAPE_SLIPS reduction (which is just option (c) with extra steps) or REACTION_BUDGET ceiling tightening to 0.20 (which dismisses the Pillar 5 concern above).

**The hybrid is dead.** Each side moving "<10% of current value" sounds politically attractive but produces a configuration that fails its own purpose. Bilateral compromise is not a virtue when the math is one-directional.

---

## 5. Binding recommendation

**Option (c) — Wave Spawner cook-time triplet exclusion** (status promotion: R7 B8 ADVISORY → BLOCKING; MIN_ESCAPE_SLIPS reduced from 3 to 2).

**Math at worst-case REACT=0.25**: `SLIP_TWEEN × M + REACTION_BUDGET = 0.15 × 2 + 0.25 = 0.55 ≤ 0.65 → 100 ms (6 frames @ 60 fps)`.
**Math at default REACT=0.20**: `0.15 × 2 + 0.20 = 0.50 ≤ 0.65 → 150 ms (9 frames @ 60 fps)`.

**Why (c) over the alternatives**:

1. **Only option that survives the full REACTION_BUDGET safe range.** (a) and (b) both relocate the zero-margin cliff; (c) eliminates it.
2. **Zero knob movement on either side.** SLIP_TWEEN stays 0.15s (PM Player Fantasy "bend" preserved — the 33ms 2-frame commitment-tell hold remains 22% of duration, not the 25%-of-shorter-duration the (a) path forced). FLOOR stays 0.65s (DPC Path B reauthoring not required).
3. **It's a status promotion, not new rule authoring.** R7 B8 already exists ADVISORY. The cook-time semantic `MIN_BARRAGE_LANE_SEPARATION = 1` on target lanes already implies the exclusion geometrically. Promoting to BLOCKING aligns the formal rule status with the math the registry already encodes.
4. **No runtime cost.** Pure cook-time assert. Wave Spawner rejects below-floor patterns at asset cook; runtime sees only valid configs. Pull-Wave Rule 4's target_lane-locked-at-SPAWNED contract is untouched.
5. **AC-PILLAR-2-BARRAGE-SPATIAL-K margin remains.** C(5,3) − 3 excluded = 7 surviving spatial configurations ≥ 4 required = 1.75× margin (down from 2.5×, still above the AC floor).
6. **Confirms a defect already latent in the registry.** `MIN_ESCAPE_SLIPS = 3` is internally inconsistent with R7 B8 BLOCKING because no surviving triplet requires 3 escape slips. (c) closes the inconsistency by retiring the over-specified constant.

### Cross-system propagation queue

| File | Change | Owner | Sequence |
|---|---|---|---|
| `design/registry/entities.yaml` | (i) Revert TELEGRAPH_WINDOW_FLOOR_S to 0.65 with R9-coordination provenance; (ii) revert BARRAGE_SIMULTANEITY_WINDOW_S text; (iii) add explicit `MIN_ESCAPE_SLIPS = 2` constant with (c)-binding provenance; (iv) update MIN_BARRAGE_LANE_SEPARATION note: M=3 worst-case under R7 B8 BLOCKING reduces to M=2 escape requirement | producer (registry steward) | First — before any GDD touches |
| `design/gdd/pull-wave-behavior.md` | (i) R7 B8 status ADVISORY → BLOCKING (cook-time assert in Wave Spawner: reject any is_barrage=true PEAK pattern where `max(target_lanes) − min(target_lanes) ≤ 2` AND lanes are 3 consecutive); (ii) F-BARRAGE-SURVIVABILITY-INVARIANT updated: `SLIP_TWEEN × 2 + REACTION_BUDGET ≤ FLOOR` (M reduced from 3 to 2); (iii) new AC on the BLOCKING cook-time assert — Wave Spawner authoring tooling MUST fail cook on any all-consecutive M=3 PEAK barrage triplet; (iv) AC-PILLAR-2-BARRAGE-SPATIAL-K re-verify text: "C(5,3)−3 = 7 surviving spatial configurations ≥ 4 required (1.75× margin)" | game-designer + systems-designer (paired authoring) | Second — after registry |
| `design/gdd/difficulty-phase-controller.md` | No GDD content change. TELEGRAPH_WINDOW_FLOOR_S stays 0.65; PROVISIONAL status unchanged. Add R9 coordination resolution note in the constant's Tuning Knob row referencing the Pull-Wave R10 revision. | game-designer (DPC owner) | Third — after Pull-Wave |
| `design/gdd/player-movement.md` | SLIP_TWEEN_DURATION_S stays 0.15s — no survivability-driven knob change. The PM R10 structural revision proceeds on the OTHER R9 defects (F-6 systemic, Shipping enforcement, Hardware Contract enforcement) per the R9 CD-mandated brief. | gameplay-programmer + game-designer | Fourth — orthogonal to survivability fix |
| `design/gdd/reviews/pull-wave-review-log.md` | Append R8 entry: "R7 B8 ADVISORY → BLOCKING; MIN_ESCAPE_SLIPS retired and replaced with M=2 derivation; F-BARRAGE-SURVIVABILITY-INVARIANT exponent reduced. Cross-system coordination resolution per `cross-system-survivability-coordination-2026-06-11.md`." | game-designer | Concurrent with Pull-Wave R10 |
| `design/gdd/reviews/player-movement-review-log.md` | Append R10 entry: "Cross-system survivability coordination resolved via option (c) — Wave Spawner cook-time exclusion. PM SLIP_TWEEN unchanged. PM structural revision proceeds on F-6 / Shipping enforcement / Hardware Contract enforcement." | (whoever runs the PM R10 review) | After PM R10 |

### New ACs / forward contracts

- **Pull-Wave AC-WS-COOK-EXCLUDE-CONSECUTIVE-M3 (NEW, BLOCKING)**: Wave Spawner cook-time tooling MUST reject any `is_barrage=true` PEAK pattern where `MAX_PULLS_PER_BARRAGE = 3` AND target lanes form 3 consecutive integers (i.e., `lanes = {n, n+1, n+2}` for any `n ∈ {0,1,2}`). Failing patterns produce a tooling error with the pattern name, triplet, and the recommended replacement triplet from the 7 surviving configs.
- **Pull-Wave forward contract on Wave Spawner (NEW)**: at M=3 PEAK barrages, target lanes are chosen from the 7-element surviving set, not the full C(5,3)=10. Wave Spawner pattern authoring guide must list the 7 surviving triplets explicitly.
- **DPC Tuning Knob row update**: TELEGRAPH_WINDOW_FLOOR_S note appends: "R9 coordination resolution 2026-06-11 — survivability margin obtained via Wave Spawner cook-time M=3 triplet exclusion (Pull-Wave R8). FLOOR stays 0.65s. DPC Path B reauthoring not required for survivability."

---

## 6. Companion work — Hardware Contract enforcement

**Endorsed as part of PM structural revision brief.** game-designer is correct: this is broken regardless of which survivability path was chosen.

Defects to address in PM R10:

1. **`t.MaxFPS = 60` is a CAP, not a FLOOR.** Unreal's engine setting clamps the upper bound; it has no semantics for "refuse to ship below this framerate." The PM Hardware Contract section (lines 229–232) treats it as a floor. This is engine-API-level wrong.
2. **Rolling-window watchdog unspec'd.** The BeginPlay one-shot check vs. per-frame rolling-window measurement is conflated in prose. The mechanism, the window size, the threshold, and the action on threshold breach are all undefined.
3. **"Debug overlay" graceful degradation is incoherent in production.** A debug overlay is a developer artifact, not a shipping-build fallback UX. There's no recovery path, no min-spec device list, no user-facing communication.
4. **Min-spec device list absent.** "60 fps mobile" must name specific reference devices (e.g., "iPhone XR / iPhone SE 2nd gen and newer; Pixel 5 / Samsung A52 and newer") that gate Polish phase performance audits. Otherwise the framerate floor is unverifiable.

**Scoped for PM R10 author, not specified here.** The CD scope is to flag the defect class. The technical-director and performance-analyst own the resolution mechanism. The minimum acceptable outcome is:
- A Shipping-build runtime check that detects sustained sub-60fps and either (a) triggers a survivability-margin-relaxed mode with explicit UX, or (b) prevents the player from entering PEAK density. Either is a Pillar 5 violation in isolation; the right answer requires technical-director judgment.
- A named min-spec device list that performance-analyst can audit against.
- A removal of `t.MaxFPS = 60` from the Hardware Contract enforcement prose; replace with the actual UE mechanism (likely `Engine.ini` framerate smoothing + a runtime DT watchdog).

---

## 7. Validation criteria — "we'll know this was right if"

- Pull-Wave R8 fresh-context re-review confirms AC-WS-COOK-EXCLUDE-CONSECUTIVE-M3 closes the invariant cliff at the registry-defined REACTION_BUDGET safe-range upper bound (0.25s).
- AC-PILLAR-2-BARRAGE-SPATIAL-K stays ≥4 at the new surviving-triplet count of 7.
- PM R10 fresh-context re-review proceeds on F-6 / Shipping enforcement / Hardware Contract enforcement without re-opening the survivability invariant (orthogonal).
- Prototype playtest at PEAK density (after PM R10 ships) shows zero "mystery deaths attributable to frame hitch" — Pillar 5 validated empirically.
- No registry constant remains internally inconsistent at the close of this propagation (`MIN_ESCAPE_SLIPS`, `MIN_BARRAGE_LANE_SEPARATION`, `MAX_PULLS_PER_BARRAGE`, `TELEGRAPH_WINDOW_FLOOR_S` all coherent under (c)).

**Failure conditions that would force re-opening this adjudication**:
- Playtest evidence that REACTION_BUDGET safe-range upper bound is actually 0.30 or higher in degraded-attention scenarios (would force a further survivability margin).
- A future Pull-Wave design adding M=4 or higher PEAK barrages (which would re-introduce the same class of problem on a different scale and require re-derivation).
- A Wave Spawner authoring decision to expand the pattern library such that the 7 surviving triplets cannot provide enough spatial variety for Pillar 2 read-ability — would force NUM_LANES > 5 and full re-verification.

---

## Decision premise — surfaced for one-time challenge

This adjudication binds on the premise that **F-BARRAGE-SURVIVABILITY-INVARIANT must hold across the full REACTION_BUDGET safe range [0.20, 0.25]**, not only at the default 0.20. If the user disagrees and treats 0.25 as a degraded-scenario allowance where the survivability promise can be silently relaxed, then option (a) returns to viability and this decision should be re-opened. I am flagging this premise so it can be challenged once at this gate rather than re-litigated later.

---

## 2026-06-18 R10d Resolution Entry

**Trigger**: Pull-Wave R8 RC-E onset-delay cliff resurfaced at R9 fresh-context re-review under the binding direction-detection (~1°) perceptibility-onset model; CD ruling on Pull-Wave R10d (2026-06-17) raised `TELEGRAPH_WINDOW_FLOOR_S` from 0.65s to 0.70s via PATH (i). This supersedes the R9 verdict sealed in this document above (which held FLOOR at 0.65s via Option (c) alone).

**R9 verdict status under R10d**: The R9 verdict at FLOOR=0.65s is superseded by R10d CD ruling PATH (i) raise to 0.70s. The full R9 reasoning above remains historically accurate to the R9-coordination context (premise: Pillar 5 holds across full REACT safe range; the R9 paths (a)/(b)/(c) enumeration correctly mapped the trade-off space available at R9). R10d revisits the FLOOR raise under a perceptibility-onset model that R9 did not yet have evidence for (R9's path (b) "raise FLOOR to 0.68" broke the invariant at REACT=0.25 under M=3 escape; R10d's PATH (i) "raise to 0.70" closes the residual onset-delay gap under M=2 escape via direction-detection onset).

**R9 Option (c) status**: **REMAINS BINDING.** The R9 Option (c) Wave Spawner cook-time consecutive-triplet exclusion (Pull-Wave R7 B8 ADVISORY → BLOCKING promotion) + `MIN_ESCAPE_SLIPS = 2` is the locomotion-only invariant closure mechanism, unchanged by R10d. At R10d FLOOR=0.70s: locomotion-only `0.15 × 2 + 0.25 = 0.55 ≤ 0.70` ✓ 150ms margin (widened from 100ms under pre-R10d FLOOR=0.65s). R10d adds an onset-inclusive safety net (`0.15 + 0.25 + 0.30 = 0.70 ≤ 0.70` ✓ zero margin at REACT=0.25 ceiling under direction-detection) **on top of** the R9 Option (c) closure — both mechanisms together provide structural margin. Neither supersedes the other.

**Mechanism summary** (current binding state, post-R10d + post-R11a-arithmetic propagation):
- `TELEGRAPH_WINDOW_FLOOR_S = 0.70s` (R10d-bound; design-time bound under R10d CD PATH (i))
- `MIN_ESCAPE_SLIPS = 2` (R9 Option (c) binding; registry constant; unchanged)
- Wave Spawner cook-time consecutive-triplet exclusion: `{0,1,2}, {1,2,3}, {2,3,4}` rejected at PEAK pool admission (R9 Option (c) binding; Pull-Wave R7 B8 BLOCKING; unchanged)
- 7 surviving M=3 triplets: `{0,1,3}, {0,1,4}, {0,2,3}, {0,2,4}, {0,3,4}, {1,2,4}, {1,3,4}` (R9 enumeration; unchanged)
- `BARRAGE_SIMULTANEITY_WINDOW_S` derivation target: 0.35s (= FLOOR/2 at R10d FLOOR=0.70s; current value 0.3 held pending DPC R-Updated re-review confirmation; R11a-arithmetic 2026-06-18 update)
- Path B reauthoring of `TelegraphWindowCurve` keys: applied at R11a-arithmetic 2026-06-18 per DPC R6 Q7 binding rule — new keys `(0.0, 0.94), (0.333, 0.82), (0.75, 0.70), (1.0, 0.70)` preserve the 0.12s intra-phase overlap window under R10d FLOOR=0.70s (pre-R10d keys would have collapsed MID's overlap to 0.02s); FLAGGED FOR CD POST-PASS REVIEW (Path B vs Path C / Pillar 3 trade)

**Propagation pass**: R11a-arithmetic 2026-06-18 propagated the R10d FLOOR raise across all binding sites:
- DPC GDD (14 FLOOR=0.6→0.70 mechanical sites per R-Updated mini-review BLOCKING B2 enumeration + 1 line-717 site dual-grep surfaced + Rule 9 PROVISIONAL framing restructure with two-phase gate + Tuning Knobs row R9-coordination-note inversion + AC-PILLAR-2-CONCURRENT validation target update + TelegraphWindowCurve Path B reauthoring)
- Pull-Wave GDD (4 stale-residue lines per R11 BLOCKING B1-B4: line 408 boundary table residue, line 596 cook-time warning, line 640 knob-interaction prose, line 953 AC-PW-22a state-mix derivation)
- `design/registry/entities.yaml` (`TELEGRAPH_WINDOW_FLOOR_S` value + provenance notes + `BARRAGE_SIMULTANEITY_WINDOW_S` derivation target + header `last_updated`)
- `design/gdd/game-concept.md` (Core Fantasy "half-second" prose updated to "seven-tenths of a second" — 40% drift correction)
- This coordination doc (R10d resolution entry — current section)
- Methodology ADR (`docs/architecture/adr-NNNN-r11a-dual-grep-methodology.md`) — dual-grep technique for future cross-system constant propagation passes (catches derived-arithmetic-literal residues like Pull-Wave's pre-R2 line 408 `4.35s` = `0.60 + 15.0/4.0` that survived 5 sequential reviews under single-pattern grep)

**Forward contracts still operative**:
- Wave Spawner pool-lifetime ceiling +50ms tag (R10d informational; no Wave Spawner GDD exists yet to propagate to — tag accretes onto the existing pre-authoring forward-contract bundle for when the GDD is authored)
- DPC TelegraphWindowCurve Path B reauthoring flagged for CD post-pass review (Path B per the R6 Q7 binding rule vs Path C bind 0.02s overlap as new design intent / Pillar 3 trade — the R11a-arithmetic pass applied Path B mechanically per the existing binding rule; the design-intent ratification is the CD post-pass call)
- Telegraph System GDD (when authored) and Wave Spawner pattern library GDD (when authored) inherit the R10d FLOOR=0.70s and the post-Path-B TelegraphWindowCurve key set — both must re-baseline against the new MID telegraph window 0.82s (not pre-R10d 0.72s)

**Validation outcome (from R11 fresh-context re-review 2026-06-17 + R-Updated DPC mini-review 2026-06-18)**: the R10d closure structurally addresses the R8 RC-E cliff; the R9 verdict's failure conditions enumerated above ("REACTION_BUDGET safe-range upper bound 0.30 or higher", "M=4 PEAK barrages", "pattern library expansion") remain the operative re-opening triggers. R10d resolves the R8 RC-E cliff under direction-detection — the empirical confirmation that direction-detection is the operative perceptibility floor is the BLOCKING-at-Alpha gate via Pull-Wave AC-PW-TIER-DIRECTION-DETECTION-PROTO-GATE (separate AC, retained as confidence check post-R10d).

---

## 2026-06-19 R13 + R14 Closure + /propagate-design-change Entry

**Status update**: R13 lean BATCHED re-review (Pull-Wave + DPC) DONE 2026-06-19 — Pull-Wave APPROVED; DPC NEEDS REVISION → 1 BLOCKING closed in-session (DPC oracle-site residue at lines 165 + 385 — `0.9f` → `0.94f` struct default + pre-init constant + paired prose example). R14 solo qa-lead confirmation pass DONE 2026-06-19 — APPROVED on first pass, all 8 checks PASS, 0 findings. **Pull-Wave + DPC both TERMINAL.** Decomposition trigger RETIRED per CD R13 binding ruling (gated on R14 APPROVED-on-first-pass).

**Final R7→R14 trajectory**: R10(19)→R11(8)→R12(7)→R13(1)→R14(0). Monotonically decreasing across five reviews. Decomposition trigger ARMED at >8 BLOCKING (R11 line 280) was approached only at R11 (= 8, did not fire); subsequent reviews stayed below threshold with decreasing margin and increasing inscription-density. Two consecutive calibrated forecasts (R13 1/1 + R14 0/0) confirm the forecast model recovered + held after R12 root-cause attribution (oracle-site sweep gap) was folded into ADR-0004 as a binding corollary.

**Methodology evolution under control**: ADR-0004 §Oracle-Site Sweep Corollary now enumerates 5 oracle sub-classes — (a) AC pass-condition values + (b) test fixture preambles + (c) anchor reference tables (R12 additions) + (d) struct default-value initializers + (e) pre-init constant definitions (R13 additions). Each with worked example from DPC. R13's preemptive application of the R12 corollary + R14 confirmation closed the methodology-evolution loop.

**/propagate-design-change pass 2026-06-19 — cascade landed**:
- **entities.yaml**: `BARRAGE_SIMULTANEITY_WINDOW_S` value flipped 0.3 → 0.35s (restores FLOOR/2 design ratio at FLOOR=0.70s; R11a-arithmetic hold-condition "DPC R-Updated re-review confirmation" satisfied by R14 APPROVED). NEW constant `TELEGRAPH_WINDOW_FLOOR_MAX_ADMISSIBLE_S = 0.76s` codified as derived ceiling under Constraint A (OPENER_key ≤ 1.0s ⇒ FLOOR ≤ 0.76s under Path B); current FLOOR=0.70s has 60ms headroom. Header `last_updated` synced 2026-06-19.
- **DPC oracle-site lock-step**: line 944 AC-PILLAR-2-BARRAGE Path A `BARRAGE_SIMULTANEITY_WINDOW_S = 0.3s` → `0.35s` per ADR-0004 dual-grep methodology.
- **Pull-Wave oracle-site lock-step**: line 535 EC-CONCURRENT-LANDING-ON-SAME-LANE `W = 0.3s` → `W = 0.35s` per ADR-0004 dual-grep methodology.
- **DPC line 717 prose retirement**: "FLAGGED FOR CD POST-PASS REVIEW" stale flag replaced with "CD R12 Ruling 1 (2026-06-18) RATIFIED: Path B canonical" + rationale citing Weber JND (Getty 1975 / Grondin 2010) + cascade reference to AC-PILLAR-2-CONCURRENT Wilson-LCB gate at line 921.

**Cross-system survivability invariant — UNAFFECTED by /propagate-design-change**: `MIN_ESCAPE_SLIPS × SLIP_TWEEN + REACT ≤ TELEGRAPH_WINDOW_FLOOR_S` still closes at `0.15 × 2 + 0.25 = 0.55 ≤ 0.70` ✓ (150ms locomotion-only margin); under direction-detection at REACT=0.25 ceiling `0.15 × 2 + 0.30 = 0.60 + perceptibility 0.27 = 0.82s ≤ FLOOR + headroom under PATH (i)`. `BARRAGE_SIMULTANEITY_WINDOW_S` is the simultaneity window (max time span across M onsets within a single barrage), NOT the escape budget — the value flip 0.3 → 0.35 widens the barrage authoring envelope (matches FLOOR/2 design ratio) but does not alter the cross-system survivability arithmetic.

**Forward contracts now operative for Wave Spawner GDD (when authored)**:
- R10d FLOOR=0.70s inheritance + Path B TelegraphWindowCurve key set (0.94/0.82/0.70)
- R10d pool-lifetime ceiling +50ms tag (informational)
- R10c pool_size = 23 derivation + DESPAWNING_RETURN_LATENCY_SLOTS = 2 explicit term
- R10c Seam 13 `FWaveSpawnerCallbackTestStub::SetOnDespawnedUserCallback` slot (test-stub only)
- R8 RC-E PEAK barrage minimum-tier exclusion (R9 Option (c) BINDING)
- R7 B8 7-surviving-triplet pattern library binding
- BARRAGE_SIMULTANEITY_WINDOW_S = 0.35s (R14 closure; matches FLOOR/2 design ratio)
- TELEGRAPH_WINDOW_FLOOR_MAX_ADMISSIBLE_S = 0.76s (PATH (i) raise ceiling, registry-codified)

**R9 Option (c) Wave Spawner cook-time consecutive-triplet exclusion + MIN_ESCAPE_SLIPS=2 REMAIN BINDING.** No mechanism change at R14; R14 confirmed R13 inscription-class closure of the DPC oracle-site residue + ADR-0004 corollary extension.

**Pull-Wave + DPC review cycle CLOSED.** Next theaters: PM decomposition + Telegraph prototype + HISM/ISMC ADR authoring (`docs/architecture/adr-NNNN-pullwave-instanced-renderer.md` — Pull-Wave story-Done blocker) + Wave Spawner & Pattern Library GDD authoring (inherits the forward contracts enumerated above).

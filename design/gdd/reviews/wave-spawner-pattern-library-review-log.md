# Wave Spawner & Pattern Library — Review Log

Canonical revision history for `design/gdd/wave-spawner-pattern-library.md` (system #7 in SLIPSTORM design order). Each entry records verdict, specialist findings, CD synthesis, applied closures, and forward-pointing forecast.

---

## Review R1 — 2026-06-21 — Verdict: NEEDS REVISION

**Scope signal**: L (multi-system integration; 7 formulas + 15 rules + 31 ACs + 18 ECs at R1; 1 new ADR required per OQ-WS-3 + 1 cross-system propagation pass to RSM per OQ-WS-4)
**Specialists**: game-designer, systems-designer, qa-lead, performance-analyst, unreal-specialist, creative-director (senior synthesis)
**Mode**: full (5 specialists + CD synthesis)
**Blocking items**: 9 (after de-dup from 19 raw specialist findings) | Recommended: 15 | Nice-to-have: 5
**Prior verdict resolved**: First review (no prior verdict)

### Summary

First-pass review on a 1099-line GDD inheriting 8 forward contracts from the closed Pull-Wave + DPC R10d→R14 cascade. All 8 required sections present + 7 OQs filed; Phase 2b seam grep CLEAN (only project-authored interface `IWaveSpawnerCallback` defined at platform-seam-interfaces.md:1482; all test-stub fields verified). Specialist convergence STRONG — 6 of 9 BLOCKING had multi-specialist convergence.

### The 9 BLOCKING items (consolidated)

| # | Item | Convergence | Class |
|---|---|---|---|
| B1 | AC-WS-07 wrong constant (0.35s should be 0.70s) | 4 specialists (game-designer B4, systems-designer B1, qa-lead B1, perf-analyst F9) | Inscription — 2× error allows covert sub-telegraph non-barrages to ship past cook (Pillar 2 violation) |
| B2 | Rule 15 ↔ H.1 coverage gap (3 missing ACs: OPENER_NO_BARRAGE, PRIMER_PATTERN, PILLAR_1_VERB_SLIP) | 3 specialists (game-designer B5, systems-designer B4, qa-lead B2) | AC expansion — H.1 self-claim "9 of 9 Rule 15 checks" doubly wrong (count 9 vs 10; mapping 5 vs 9) |
| B3 | `t_norm_phase` phantom field — DPC exposes only diagnostic `t_norm` | 2 specialists (game-designer B3, systems-designer B2) | Cross-doc contract — F-3 cadence governor uses DPC field marked "not a decision input" (DPC line 172) |
| B4 | AC-WS-29 fixture inverts Rule 9 (admits inside drain window) | 2 specialists (systems-designer B3, qa-lead B4) | Inscription — fixture `phase_remaining ≈ 0.5s` is inside Rule 9's 3.0s suppression window |
| B5 | AC-WS-13 BLOCKING-at-story-Done with OQ-WS-4 OPEN (Pillar 5 unverifiable) | 2 specialists (game-designer B2, qa-lead B3) | Contract closure — self-contradicting gate; Pillar 5 ("Skill Is Visible") cannot be honored without deterministic RNG replay |
| B6 | `UPROPERTY(EditAnywhere, Config)` on UDataAsset structurally invalid in UE | 2 specialists (unreal-specialist B1, perf-analyst R6) | UE-mechanics — `Config` requires `UCLASS(Config=GameName)` and `.ini`; UDataAsset serializes via asset pipeline. Specifier silently ignored. |
| B7 | Rule 11 pool pre-allocation timing ambiguous | 2 specialists (perf-analyst F1, unreal-specialist B3) | Inscription — "run start" vs "engine ready-state" vs "Cold entry" + replay path; potential per-replay 16.6ms hitch on death-retry |
| B8 | `UWave` naming + `SpawnActor<UWave>` contradiction | 1 specialist (unreal-specialist B2) | UE-mechanics — SpawnActor requires AActor; should be `AWave` per CLAUDE.md naming + TObjectPtr modernization (UE 5.0+) |
| B9 | F-10 says `T=0` valid in OPENER+MID; Rule 2 shape table lists OPENER `{1,2}` / MID `{1,2,3}` | 1 specialist (systems-designer B5) | Inscription — direct contradiction; no oracle |

### Demoted from BLOCKING (CD Ruling 1)

**game-designer B1 (7-triplet vocabulary collapse)** → RECOMMENDED-STRONG. CD demoted via lane-class-vs-pattern-instance disambiguation: Rule 4 enumerates lane-class signatures, not pattern instances; F-3 `[0.15, 0.40]` band + Rule 5 tier floor explicitly permit multiple patterns per triplet. Birthday-paradox math conflates the two units. Constructive closure: Player Fantasy precision edit + new Rule 15 cook check `MIN_BARRAGE_PATTERN_COUNT_PER_TRIPLET ≥ 2` + AC-WS-35.

### 5 CD R1 binding rulings

1. **Ruling 1** — 7-triplet concern resolution path: NOT a design pivot, NOT RC adjudication. Player Fantasy line 16 receives one-sentence precision edit; Rule 15 receives new cook check `MIN_BARRAGE_PATTERN_COUNT_PER_TRIPLET ≥ 2` + paired AC-WS-35.
2. **Ruling 2** — AC-WS-13 + OQ-WS-4 resolution: OQ-WS-4 closes in same revision pass. Rule 8 inscribes "per-run RNG seed supplied by RSM via `RunSeed : uint64` field captured at `Cold → Active`." Forward contract on RSM via Dependencies § Bidirectional consistency row 1. AC-WS-13 stays BLOCKING-at-story-Done (downgrade weakens Pillar 5).
3. **Ruling 3** — `t_norm_phase` fix scope: derive `t_norm_PEAK` locally in spawner from PEAK-entry timestamp + registered `PEAK_DURATION_S`. **Pull-Wave + DPC closed cascade is NOT re-opened.** DPC `t_norm` stays diagnostic.
4. **Ruling 4** — UWave → AWave verify-first: confirmed Pull-Wave has zero `UWave`/`AWave` class declarations; this GDD is the first authoritative naming site. Per unreal-specialist B2 + CLAUDE.md `A`-prefix-for-AActor convention, commit to `AWave`. 7 touch sites updated.
5. **Ruling 5** — Phase 2b seam grep mandated pre-revision (verified CLEAN in main-review Phase 2b before specialist spawn; only `IWaveSpawnerCallback` interface; all test-stub fields verified at Seam 13).

### User decisions (3 questions answered before R1a execution)

1. **CD Ruling 1 acceptance**: "Accept verbatim" (Player Fantasy edit + new cook check + AC-WS-35 applied).
2. **OQ-WS-3 close path**: "Lock as binding UGameInstanceSubsystem in GDD" (perf-driven; UWorldSubsystem rejected).
3. **R1a scope**: "BLOCKING + cheap mechanical RECOMMENDED only" (9 BLOCKING + 5 RECOMMENDED in R1a; 10 RECOMMENDED deferred to R2 surface per Pull-Wave R12→R13 precedent).

### Specialist disagreement on verdict

- MAJOR REVISION NEEDED: game-designer, unreal-specialist
- NEEDS REVISION: systems-designer, qa-lead, performance-analyst
- **CD adjudication**: NEEDS REVISION. MAJOR-drivers reduce on inspection — game-designer B1 (7-triplet vocabulary) conflated two units (constructively closed via CD Ruling 1); unreal-specialist B1/B2 are real defects but each closes via single-site fix or global find-replace. Architectural surface is sound; 9 BLOCKING are all inscription-class or single-site closure. Decomposition trigger UNARMED (Pull-Wave R11 fired at 11 BLOCKING / structural; this surface is 9 BLOCKING / inscription).

### Files modified by R1 pass

- (No files modified — R1 is the verdict-bearing review; closure was deferred to R1a in-session revision.)
- This file (review log) created at R1+R1a closure tally point (2026-06-22).

### Files NOT modified by R1 pass (intentional — review-only)

- `design/gdd/wave-spawner-pattern-library.md` — fresh-context review is read-only on target GDD; R1a applies the edits.
- `docs/architecture/platform-seam-interfaces.md` — no seam changes.
- `design/registry/entities.yaml` — no registry-backed value changes from R1.
- Sibling GDDs (PM, IS, RSM, DPC, Pull-Wave) — out of R1 scope.

---

## Review R1a — 2026-06-21 — In-session author revision

**Mode**: in-session author revision per CD R1 synthesis "R1a closure path" recommendation; same-session execution because all 9 BLOCKING were inscription-class or single-site closure (no design-shaped questions requiring fresh-context reflection).

### R1a closure tally

**17 R1a binding decisions** applied (R1a-1 through R1a-17). Header binding-decisions block prepended to GDD documenting all 17. All 9 R1 BLOCKING closed + 5 RECOMMENDED + 5 CD R1 rulings + 2 OQ closures (OQ-WS-3, OQ-WS-4).

| R1a | Closure | Source finding(s) | GDD touch sites |
|---|---|---|---|
| R1a-1 | Player Fantasy precision: vocabulary-at-lane-class-layer disambiguation + new Rule 15 `MIN_BARRAGE_PATTERN_COUNT_PER_TRIPLET ≥ 2` cook check + AC-WS-35 | CD Ruling 1 (was game-designer B1) | Player Fantasy + Rule 15 + AC-WS-35 |
| R1a-2 | OQ-WS-3 CLOSED — locked as `UGameInstanceSubsystem` | perf-analyst R7 + user Q2-locked | Rule 7 + Engine integration + E3 + OQ-WS-3 + Status header |
| R1a-3 | OQ-WS-4 CLOSED — RSM `RunSeed:uint64` forward contract inscribed in Rule 8 | CD Ruling 2 (B5 closure) | Rule 8 + AC-WS-13 + OQ-WS-4 + Dependencies §3 |
| R1a-4 | `t_norm_phase` phantom field removed — Rule 8 derives `t_norm_PEAK` locally | CD Ruling 3 (B3 closure) | Rule 8 + Dependencies §1 |
| R1a-5 | AC-WS-07 wrong constant 0.35s → 0.70s | 4-specialist convergence (B1) | AC-WS-07 |
| R1a-6 | Rule 15 ↔ H.1 coverage closed: AC-WS-32 OPENER_NO_BARRAGE + AC-WS-33 PRIMER_PATTERN + AC-WS-34 PILLAR_1_VERB_SLIP added; H.1 self-claim + H.8 cross-ref corrected | 3-specialist convergence (B2) | H.1 + H.8 |
| R1a-7 | AC-WS-29 fixture: `phase_remaining ≈ 0.5s` → `≈ 3.05s` (just outside Rule 9 suppression window) | 2-specialist convergence (B4) | AC-WS-29 |
| R1a-8 | `UPROPERTY(EditAnywhere, Config)` on UDataAsset: `Config` specifier removed; UDataAsset asset-pipeline serialization is binding | 2-specialist convergence (B6) | G.1 + G.2 |
| R1a-9 | Rule 11 pool pre-allocation locked to `UGameInstanceSubsystem::Initialize()` (once per session) | 2-specialist convergence (B7) | Rule 11 + AC-WS-20 + AC-WS-31 |
| R1a-10 | UWave → AWave global rename + TObjectPtr modernization | unreal-specialist B2 (B8) | Rule 11 + Engine integration + E2 + AC-WS-15/16/20 (7 touch sites) |
| R1a-11 | F-10 T=0 domain restricted to PEAK non-barrage only | systems-designer B5 (B9) | F-10 |
| R1a-12 | Section C.3 labeled anchors added (`### C.3` + `C.3 §9`) | 3-specialist convergence (RECOMMENDED-3) | Detailed Rules subsections (2 headings) |
| R1a-13 | AC-WS-12 reservation-fulfilment fixture added (4th scenario) | qa-lead R5 (RECOMMENDED-4) | AC-WS-12 |
| R1a-14 | AC-WS-15 LogActor watchpoint → `FWaveSpawnActorCounter` seam-injected counter | unreal-specialist R7 (RECOMMENDED-10) | AC-WS-15 |
| R1a-15 | AC-WS-30/31 device list: inherit Pull-Wave R10a placeholders (iPhone XR + Pixel 5 / Galaxy A52) | qa-lead R9 (RECOMMENDED-7) | AC-WS-30/31 |
| R1a-16 | Author note at line 22 stripped from canonical Player Fantasy | game-designer N10 (RECOMMENDED-15) | Player Fantasy |
| R1a-17 | OQ-WS resolution sequencing updated: 2 of 7 OQs closed | Process | OQ resolution table |

### GDD metrics post-R1a

- **Line count**: 1099 → 1129 (+30 net).
- **AC count**: 31 → 35 (+4: AC-WS-32/33/34/35 added per R1a-6 + R1a-1).
- **BLOCKING-at-story-Done**: 26 → 30 (+4 from new ACs).
- **ADVISORY-at-story-Done + BLOCKING-at-Alpha**: 5 (unchanged).
- **Cook-time checks (Rule 15)**: 10 → 11 (+1 MIN_BARRAGE_PATTERN_COUNT_PER_TRIPLET per R1a-1).
- **Open Questions**: 7 → 5 OPEN (2 CLOSED — OQ-WS-3 + OQ-WS-4).

### 10 RECOMMENDED items intentionally deferred to R2 surface

Per Pull-Wave R12→R13 precedent (defer non-blocking RECOMMENDED to next review surface):
1. `pool_size = 23` literal duplicated across F-2, Rule 11, AC-WS-09, AC-WS-20 — oracle-site drift risk.
2. AC-WS-21 `LeanEaseCurve_Canonical` asset-reference immutability half unverified.
3. AC-WS-22/23 telemetry apparatus underspecified for Alpha BLOCKING gate.
4. AC-WS-23 schema scope covers 5 of 9 named events.
5. `OnPausedChanged` delegate type (MULTICAST vs DYNAMIC_MULTICAST) unspecified.
6. F-2 DESPAWNING_RETURN_LATENCY_SLOTS rationale documents wrong scenario.
7. `pattern_admitted` telemetry rate documentation.
8. G.1 BlueprintReadOnly clarification.
9. Pause-flush `time_to_drain_ms` ceiling AC (perf-analyst R3).
10. Primer pedagogical AC (R8 RC-D 3-part OPENER teaching contract).

Plus 5 Nice-to-Have items unchanged: entities.yaml stale REACTION_BUDGET margin, W=0.35s barrage cluster experiential playtest AC, Rule 7 `live` counter implementation guidance, LeanEaseCurve_Canonical async-load failure path, EC-WS-7 RNG-on-primer wording clarification.

### Files modified by R1a pass

- `design/gdd/wave-spawner-pattern-library.md` — primary target. 1099 → 1129 lines (+30 net). 17 R1a binding-decisions header block prepended; 18 in-place edits applied across all 8 sections.
- `design/gdd/reviews/wave-spawner-pattern-library-review-log.md` — NEW; this file.
- `design/gdd/systems-index.md` — Wave Spawner row R1+R1a status update.
- `production/session-state/active.md` — STATUS block + Progress + Session Extract for R1+R1a.

### Files NOT modified by R1a (intentional)

- `docs/architecture/platform-seam-interfaces.md` — no seam changes; Seam 13 contract unchanged (R10c addition already in place).
- `design/registry/entities.yaml` — no registry-backed value changes. RSM `RunSeed` is a forward contract on RSM (not a registry constant); `PEAK_DURATION_S` is a DPC constant already registered (not a new entry).
- `design/gdd/difficulty-phase-controller.md` — Pull-Wave + DPC R14-closed cascade NOT re-opened per CD Ruling 3. DPC `t_norm` field semantics preserved (diagnostic-only).
- `design/gdd/pull-wave-behavior.md` — Pull-Wave + DPC cascade NOT re-opened.
- `design/gdd/run-state-machine.md` — RSM `RunSeed` forward contract filed for next `/consistency-check` pass; not landed in this pass.
- Sibling GDDs (PM, IS) — out of scope.
- `docs/architecture/adr-NNNN-wave-spawner-subsystem-hosting.md` — does not exist yet; downstream artifact authorship.

### R2 forecast (per CD R1 synthesis, applying ADR-0004 oracle-site corollary; biased high)

- **BLOCKING band**: 2-4 BLOCKING.
- **RECOMMENDED band**: 4-7 RECOMMENDED (largely the 10 deferred items above; some may auto-close if R2 reviewer agrees with deferral pattern).
- **Forecast rationale**: R12 oracle-site corollary applied preemptively to AC-WS-07, t_norm_phase, AC-WS-29 fixture, and pool-allocation timing — most likely oracle-sweep gaps already addressed. Residual surface: (a) AC-WS-07 fix may need lock-step sweep across F-6 prose + Rule 6 + Rule 15 table; (b) `t_norm_phase` deletion may leak; (c) Rule 15 → H.1 alignment may surface count drift in H.8; (d) UWave→AWave global rename may have missed dependent text. First R-cycle on Wave Spawner — forecast model uncalibrated; bias high (recall Pull-Wave R10a 0-4 forecast / 19 actual = 4-5× miss).
- **R2 mode recommendation**: lean panel (CD + qa-lead + systems-designer + unreal-specialist) per Pull-Wave R13 precedent; ux-designer + game-designer + performance-analyst spawn only if R2 surfaces methodology-shaped concerns.

### Strategic notes

- **R1 → R1a transition validated CD's "in-session inscription" closure-path recommendation**: 9 BLOCKING + 5 RECOMMENDED + 2 OQ closures all closed in one author pass; no design-shaped questions required fresh-context delegation.
- **Forecast model recalibration**: first R-cycle on this GDD; R2 will be the first calibration data point. If R2 lands within 2-4 BLOCKING band: forecast model successfully transfers from Pull-Wave to Wave Spawner. If R2 exceeds 4 BLOCKING: methodology corollary needs domain-specific extension.
- **Cross-system cascade unaffected**: Pull-Wave + DPC R14-closed cascade NOT re-opened per CD Ruling 3 (t_norm derived locally) + Ruling 2 (RunSeed forward contract on RSM, not DPC). Cross-system survivability invariant unchanged.
- **OQ closure pattern**: 2 of 7 OQs closed in R1a (OQ-WS-3 + OQ-WS-4). Remaining 5 OQs are forward-looking hand-offs that find resolution forum cleanly; none block R2.
- **Decomposition trigger UNARMED**: this surface (9 BLOCKING / inscription) does not approach the Pull-Wave R11 / Player-Movement R11 threshold (>8 BLOCKING + structural distribution).

### Operative next step

R2 fresh-context re-review per `/clear` → `/design-review design/gdd/wave-spawner-pattern-library.md`. Lean panel mode recommended.

**Parallel tracks unchanged from prior active.md state**:
- HISM/ISMC ADR authoring (Pull-Wave story-Done blocker; downstream artifact; assign unreal-specialist).
- `docs/architecture/adr-NNNN-wave-spawner-subsystem-hosting.md` (Wave Spawner story-Done blocker per OQ-WS-3 close; assign unreal-specialist; sibling to OQ-PW-3).
- PM decomposition execution per `design/gdd/reviews/player-movement-decomposition-plan-2026-06-16.md` (~3-5 hours, separate `/clear` session).
- Telegraph System prototype (highest-risk bet per systems-index).
- RSM `/consistency-check` pass to land `RunSeed:uint64` public-interface field + `OnPausedChanged` delegate type pin (R1a forward contract surface).

---

## Review R2 — 2026-06-22 — Verdict: NEEDS REVISION

**Scope signal**: L (unchanged from R1; multi-system integration; 8 formulas + 15 rules + 38 ACs + 18 ECs post-R2a; ADR sibling pending)
**Specialists**: qa-lead, systems-designer, unreal-specialist, creative-director (senior synthesis)
**Mode**: lean panel (3 specialists + CD) per R1a forecast recommendation
**Blocking items**: 8 (after CD synthesis dedup from 12 raw specialist findings; 3-spec convergence on 3 items + 5 single-spec domain-strong) | Recommended: 15 | Nice-to-have: 5
**Prior verdict resolved**: R1 NEEDS REVISION → R1a in-session closure (17 binding decisions); R2 is first fresh-context re-review.

### Summary

R1a's 17 binding decisions closed 9 R1 BLOCKING but introduced 2 regressions (AC-WS-29 fixture impossibility under Rule 2 MID cap; E1 lateral line 891 stale "Cold entry" wording contradicting R1a-9). R2 fresh-context surfaced 8 BLOCKING: 3 multi-spec convergence (tick host + F-prefix; tick ordering pin; `FWaveSpawnActorCounter` paper-only seam) + 5 single-spec domain-strong findings (UE 5.7 `Initialize()` lifecycle violation; F-3 `base_w < W_CEILING` invariant missing; AC-WS-29 fixture impossibility; AC-WS-23 schema 5-of-9 events; AC-WS-13 platform-scope contradiction). All inscription-class; no structural pivots. CD synthesis ruled: decomposition trigger DOES NOT FIRE (8 at threshold not >8; no structural distribution).

### The 8 BLOCKING items (consolidated)

| # | Item | Convergence | Class |
|---|---|---|---|
| B1 | Tick host + F-prefix (`FWaveSpawnerSubsystem` → `UWaveSpawnerSubsystem` + FTickableGameObject inscription) | 3-spec (qa B-R2-4 + sys B-SD-2 + unreal B-R2-2) | UE-mechanics inscription |
| B2 | Tick ordering pin (Wave Spawner's position in `RSM → DPC → Pull-Wave` chain unstated) | 3-spec (qa B-R2-5 + sys + unreal B-R2-3) | UE-mechanics inscription |
| B3 | `FWaveSpawnActorCounter` paper-only seam not in `platform-seam-interfaces.md` | 3-spec (qa + sys B-SD-3 + unreal B-R2-4) | Paper-only-seam (reframe to post-hoc pool count is unanimous) |
| B4 | `SpawnActor` at `Initialize()` structurally impossible in UE 5.7 (UWorld not yet exists) | unreal B-R2-1 | UE 5.7 lifecycle violation |
| B5 | F-3 `base_w < W_CEILING` cross-knob invariant missing (G.1 safe ranges overlap → silent λ-saturation) | systems-designer B-SD-1 | Formula domain |
| B6 | AC-WS-29 fixture impossibility (R1a `phase_remaining ≈ 3.05s` + Rule 2 MID cap unreachable) | qa-lead B-R2-1 | AC testability regression from R1a |
| B7 | AC-WS-23 schema scope 5-of-9 events (4 EC-class runtime defense events absent from C.3 §9) | qa-lead B-R2-2 | Telemetry inscription |
| B8 | AC-WS-13 missing same-platform constraint (contradicts EC-WS-15 platform-scope) | qa-lead B-R2-3 | Replay-determinism inscription |

### Specialist disagreements (CD-ruled)

**sys-des B-SD-3 over-claimed**: extended claim that `FWaveSpawnerCallbackTestStub` was also paper-only. Phase 2b grep evidence (main-review pass) confirmed `FWaveSpawnerCallbackTestStub` IS defined at `platform-seam-interfaces.md` line 1571 (Seam 13 test stub). CD ruled: drop the FWaveSpawnerCallbackTestStub half; only `FWaveSpawnActorCounter` is the legitimate paper-only seam. Surfaced in active body of GDD R2a header to keep the disagreement traceable.

### R2 forecast vs actual

- **R2 forecast (per R1a)**: 2-4 BLOCKING / 4-7 RECOMMENDED.
- **R2 actual**: 8 BLOCKING / 15 RECOMMENDED / 5 NICE-TO-HAVE.
- **Miss size**: 2× on BLOCKING band; 2-3× on RECOMMENDED.
- **CD assessment**: first-R-cycle calibration data point, NOT a forecast-model failure. R1a's forecast was explicitly UNCALIBRATED in the R1 review log. The 2× miss is informative for future calibration: forecast model implicitly assumed multi-spec convergence dominates; for inscription-heavy GDDs with deep cross-domain surfaces (telemetry, UE lifecycle, formulas), single-spec domain-strong finds were the dominant signal (5 of 8 BLOCKING). Compare: Player-Movement R11 model-breaking; DPC R10 in-band; Wave Spawner R2 moderate-2×-miss. Recommendation: default future first-R-cycle large-inscription GDD forecasts to 4-8 BLOCKING band.

### Decomposition trigger ruling (CD)

**DOES NOT FIRE.** Two-part rationale:
1. Threshold semantics: armed at *greater than* 8 BLOCKING; we are at exactly 8.
2. Structural distribution requirement not met: all 8 are inscription-class single-site closures; none require architectural pivots.

Splitting this GDD into sub-GDDs would NOT reduce BLOCKING count — it would distribute the same fixes across new artifacts. Decomposition pattern (Player-Movement R11) exists for GDDs whose problems span structurally independent seams; Wave Spawner's R2 surface is concentrated in inscription artifacts inside a single coherent system.

### Files modified by R2 pass

- (No files modified — R2 is the verdict-bearing review; closure deferred to R2a in-session revision.)
- This file (review log) appended with R2 + R2a entries at 2026-06-22 closure tally.

### Files NOT modified by R2 pass (intentional — review-only)

- `design/gdd/wave-spawner-pattern-library.md` — fresh-context review is read-only on target GDD; R2a applies the edits.
- `docs/architecture/platform-seam-interfaces.md` — no seam changes (CD-ruled R2a-3 reframe avoids the need for new seam authoring).
- `design/registry/entities.yaml` — no registry-backed value changes from R2.
- Sibling GDDs (PM, IS, RSM, DPC, Pull-Wave) — out of R2 scope. RSM `RunSeed` + `OpPausedChanged` delegate type still queued for next `/consistency-check`.

---

## Review R2a — 2026-06-22 — In-session author revision

**Mode**: in-session author revision per CD R2 synthesis "R2a closure path" recommendation; same-session execution because all 8 BLOCKING are inscription-class single-site closures (no design-shaped questions requiring fresh-context reflection). User chose "R2a in-session now (Recommended)" via AskUserQuestion.

### R2a closure tally

**12 R2a binding decisions** applied (R2a-1 through R2a-12). Header binding-decisions block prepended to GDD documenting all 12. All 8 R2 BLOCKING closed + 4 cheap mechanical RECOMMENDED applied + 3 new ACs added (AC-WS-36/37/38 for the new Rule 15 cook checks).

| R2a | Closure | Source finding(s) | GDD touch sites |
|---|---|---|---|
| R2a-1 | Tick host inscription: `UWaveSpawnerSubsystem` rename + FTickableGameObject mixin + ETickableTickType::Conditional + GetStatId() | 3-spec (qa B-R2-4 + sys B-SD-2 + unreal B-R2-2) | Engine subsystem integration + AC-WS-30 |
| R2a-2 | Tick ordering pin: DPC `OnPostTickFrameStatePublished` delegate subscription (structural ordering) | 3-spec (qa B-R2-5 + sys + unreal B-R2-3) | Engine subsystem integration |
| R2a-3 | AC-WS-15 reframe: post-hoc pool inspection (`Num() == 23` + world-actor census); FWaveSpawnActorCounter removed | 3-spec (qa + sys B-SD-3 + unreal B-R2-4) | AC-WS-15 |
| R2a-4 | Pool pre-allocation lifecycle fix: `Initialize()` → `FCoreUObjectDelegates::OnPostLoadMap` | unreal B-R2-1 | Rule 11 + E1 + Engine integration + AC-WS-20 + AC-WS-31 |
| R2a-5 | F-3 `base_w < W_CEILING` cross-knob invariant + Rule 15 `PEAK_BASE_W_BELOW_CEILING` cook check + AC-WS-36 | systems-designer B-SD-1 | F-3 cross-knob invariants + Rule 15 + AC-WS-36 |
| R2a-6 | AC-WS-29 fixture reframe: onset-2 at +2.90s (fires 150ms before swap, immutability claim now testable) | qa-lead B-R2-1 | AC-WS-29 |
| R2a-7 | C.3 §9 schema expansion to 9 events (4 EC-class events added with payload fields) + AC-WS-23 scope update + `run_termination_flush_executed` added to G.3 | qa-lead B-R2-2 | C.3 §9 + G.3 + AC-WS-23 |
| R2a-8 | AC-WS-13 same-platform constraint + forward-guard sentence | qa-lead B-R2-3 | AC-WS-13 |
| R2a-9 | [mechanical RECOMMENDED] G.1 UPROPERTY specifier: `EditAnywhere` → `EditDefaultsOnly` (matches G.2 + canonical UDataAsset pattern) | 3-spec (qa R-R2-3 + sys R-SD-3 + unreal R-R2-2) | G.1 |
| R2a-10 | [mechanical RECOMMENDED] `pool_size = 23` literal: `(per F-2)` annotations at 3 non-oracle sites; F-2 is sole oracle | 2-spec (qa R-R2-2 + unreal R-R2-5) | Rule 11 + AC-WS-09 (AC-WS-20 already annotated) |
| R2a-11 | [mechanical RECOMMENDED] Rule 15 `NON_BARRAGE_STAGGER` empty-set guard: `if onset_count ≤ 1: pass` branch encoded in expression | systems-designer R-SD-2 | Rule 15 |
| R2a-12 | [mechanical RECOMMENDED] Rule 15 added 2 cook checks: `BARRAGE_UNIFORM_TIER` + `BARRAGE_DISTINCT_SOURCE_LANES` (Pull-Wave R7 B11/B12 forward contract enforcement) + AC-WS-37 + AC-WS-38 | systems-designer R-SD-5 | Rule 15 + AC-WS-37 + AC-WS-38 |

### GDD metrics post-R2a

- **Line count**: 1129 → 1176 (+47 net).
- **AC count**: 35 → 38 (+3: AC-WS-36/37/38 added per R2a-5 + R2a-12).
- **BLOCKING-at-story-Done**: 30 → 33 (+3 from new ACs).
- **ADVISORY-at-story-Done + BLOCKING-at-Alpha**: 5 (unchanged).
- **Cook-time checks (Rule 15)**: 11 → 14 (+3: PEAK_BASE_W_BELOW_CEILING + BARRAGE_UNIFORM_TIER + BARRAGE_DISTINCT_SOURCE_LANES).
- **Open Questions**: 5 OPEN (unchanged from R1a).

### 11 RECOMMENDED + 5 NICE-TO-HAVE intentionally deferred to R3 surface

Per Pull-Wave R12→R13 precedent (defer non-blocking RECOMMENDED to next review surface):
1. `OnPausedChanged` delegate type MULTICAST vs DYNAMIC_MULTICAST pin (qa R-R2-1 + unreal R-R2-3 + R1a deferred #5).
2. AC-WS-13 mock-fixture story-sequencing clause (qa R-R2-2 EXTEND).
3. AC-WS-21 `LeanEaseCurve_Canonical` immutability second fixture (qa R-R2-5 + R1a deferred #2).
4. AC-WS-31 pool-init activation state `bHidden=true, SetActorEnableCollision(false)` (qa R-R2-9 / R-main-4).
5. Primer pedagogy AC absent (qa R-R2-6 + R1a deferred #10).
6. AC-WS-14 boundary fixtures enumeration (qa R-R2-8).
7. Pause-flush `time_to_drain_ms` ≤ 16.6ms ceiling AC (qa R-R2-4 + R1a deferred #9).
8. AC-WS-22 gate-level downgrade candidate (qa R-R2-10).
9. F-3b drift derivation (sys R-SD-1).
10. Rule 9 direct AC (sys R-SD-4).
11. AWave module ownership pin (unreal R-R2-4).

Plus 5 Nice-to-Have items: name drift `t_norm`/`t_norm_PEAK`/`τ` (R-main-1); F-2 DESPAWNING rationale (R1a deferred #6 + N-R2-2); F-5 ascending-onset assumption (N-SD-1); F-7 FLOOR/W ratio cross-ref to F-8 (N-SD-2); Rule 7 wording polish.

### Files modified by R2a pass

- `design/gdd/wave-spawner-pattern-library.md` — primary target. 1129 → 1176 lines (+47 net). 12 R2a binding-decisions header block prepended; in-place edits applied across Detailed Rules (Rule 11 + Rule 15 + Engine integration), Formulas (F-3 cross-knob invariants), Dependencies (E1 lateral), Tuning Knobs (G.1 specifier), Acceptance Criteria (AC-WS-13/15/20/23/29/30/31 + 3 new ACs WS-36/37/38), C.3 §9 + G.3 telemetry schema.
- `design/gdd/reviews/wave-spawner-pattern-library-review-log.md` — this file; R2 + R2a entries appended.

### Files NOT modified by R2a pass (intentional)

- `docs/architecture/platform-seam-interfaces.md` — no seam changes; R2a-3 reframe avoided the need for new seam authoring (post-hoc pool inspection instead of `FWaveSpawnActorCounter` seam).
- `design/registry/entities.yaml` — no registry-backed value changes; F-3 cross-knob invariant is on existing G.1 knobs.
- `design/gdd/difficulty-phase-controller.md` — `OnPostTickFrameStatePublished` delegate is a NEW forward contract on DPC; filed for next `/consistency-check` pass (sibling to RSM `RunSeed` + `OnPausedChanged` contracts). Pull-Wave + DPC R14-closed cascade NOT re-opened.
- `design/gdd/pull-wave-behavior.md` / `design/gdd/run-state-machine.md` — out of scope; forward contracts queued.
- `design/gdd/systems-index.md` — Wave Spawner row update at R2a closure (user-prompted).
- `production/session-state/active.md` — STATUS block + Progress + Session Extract for R2+R2a (user-prompted).
- `docs/architecture/adr-NNNN-wave-spawner-subsystem-hosting.md` — does not exist yet; downstream artifact authorship.

### R3 forecast (per CD R2 synthesis)

- **BLOCKING band**: 0-3 BLOCKING.
- **RECOMMENDED band**: 3-6 RECOMMENDED (subset of the 11 deferred items above; some may auto-close incidentally during R2a → R3).
- **Confidence**: MEDIUM. R1a→R2 had a 2× miss; we now have one calibration anchor for this GDD's behavior. Treat R3 band as informed-but-not-locked.
- **Forecast rationale**: R2a→R3 regression rate expected 0-2 similar to R1a→R2 (author now sensitized to Rule 2 cap + oracle-drift patterns). G.1/G.2 specifier inconsistency + OnPausedChanged delegate appeared in all 3 specialist reports — closing 3 of 11 deferred RECOMMENDED incidentally is plausible.
- **R3 mode recommendation**: solo qa-lead grep confirmation pass (Pull-Wave R14 precedent) IF the R2a closure trajectory is monotonically decreasing; else lean panel re-test (Pull-Wave R13 precedent).

### Strategic notes

- **R2 → R2a transition validated CD's "in-session inscription" closure-path recommendation again**: 8 BLOCKING + 4 RECOMMENDED + 3 new ACs all closed in one author pass; no design-shaped questions required fresh-context delegation.
- **Forecast calibration record (Wave Spawner)**:

  | Review | Forecast | Observed | Status |
  |---|---|---|---|
  | R1 | 8-15 | 9 | CALIBRATED ✓ |
  | R2 | 2-4 | 8 | 2× miss — first calibration data point (CD assessment: not a model failure) |
  | R3 | 0-3 | TBD | pending |

- **Cross-system cascade unaffected**: Pull-Wave + DPC R14-closed cascade NOT re-opened. The R2a-2 tick ordering fix uses DPC's existing post-tick semantics; the new `OnPostTickFrameStatePublished` delegate is a NEW forward contract on DPC (additive — no DPC GDD revision required; will be inscribed at next `/consistency-check`).
- **OQ closure pattern**: 5 OPEN OQs unchanged from R1a. Remaining 5 OQs are forward-looking hand-offs; none block R3.
- **Decomposition trigger UNARMED**: this surface (8 BLOCKING / inscription) is at threshold but CD-ruled DOES-NOT-FIRE per structural-distribution requirement.

### Operative next step

R3 fresh-context re-review per `/clear` → `/design-review design/gdd/wave-spawner-pattern-library.md`. Solo qa-lead grep verification recommended (Pull-Wave R14 precedent) if R2a trajectory holds; lean panel re-test (Pull-Wave R13 precedent) if user prefers higher confidence.

**Parallel tracks unchanged**:
- HISM/ISMC ADR authoring.
- `docs/architecture/adr-NNNN-wave-spawner-subsystem-hosting.md`.
- PM decomposition execution.
- Telegraph System prototype.
- RSM `/consistency-check` pass to land `RunSeed:uint64` + `OnPausedChanged` delegate type + DPC `OnPostTickFrameStatePublished` (R2a-2 forward contract).

---

## Review R3 — 2026-06-22 — Verdict: NEEDS REVISION

**Scope signal**: L (unchanged; 8 formulas + 15 rules + 38 ACs + 18 ECs; 1 cross-system propagation pending RSM `/consistency-check`)
**Specialists**: qa-lead (solo)
**Mode**: solo qa-lead grep verification (Pull-Wave R14 precedent) per user selection via AskUserQuestion; CD synthesis bypassed per mode definition
**Blocking items**: 1 | Recommended: 1 | Nice-to-have: 0
**Prior verdict resolved**: R2 NEEDS REVISION (8 BLOCKING) → R2a in-session closure (12 binding decisions) → R3 is first fresh-context re-review post-R2a.

### Summary

R2a's 12 binding decisions all verified PASS on grep-and-line evidence. One sub-class (e) oracle-site drift surfaced at E3 lateral row (line 936) — R2a-4 lifecycle correction reached 5 sibling sites (Rule 11, Engine integration, E1, AC-WS-20, AC-WS-31) but missed the adjacent E3 lateral-table row. One sub-class (b) narrative drift at H.1 intro (line 1054) — R2a-5/R2a-12 added 3 new Rule 15 checks + 3 new ACs but the H.1 preamble count "11 cook checks" / "AC-WS-01–35" was not refreshed (H.8 sibling counts at lines 1140/1145 were updated correctly). Both inscription-class single-site closures. Decomposition trigger UNARMED (1 BLOCKING ≪ >8 threshold).

### The 1 BLOCKING + 1 RECOMMENDED items

| # | Item | Class | Source | GDD site |
|---|---|---|---|---|
| B-R3-1 | E3 lateral row stale lifecycle reference: "Pool pre-allocation fires exactly once per session at `Initialize()`" contradicts R2a-4's fix moving pool pre-allocation to `FCoreUObjectDelegates::OnPostLoadMap`. Cross-system misread risk — implementer reading E3 verbatim hits UE 5.7 lifecycle violation. | Sub-class (e) oracle-site drift / inscription | qa-lead B-R3-1 | E3 lateral table row, line 936 |
| R-R3-1 | H.1 intro narrative count stale: "11 binding cook-time checks Rule 15 enumerates" + "AC-WS-01 through AC-WS-09 + AC-WS-32 through AC-WS-35" — both stale post-R2a-5 + R2a-12 (Rule 15 now has 14 checks; H.1 now has 16 ACs including AC-WS-36/37/38). H.8 lines 1140+1145 already correctly say "× 14" and "× 16 cook-time" — line 1054 contradicts its own sibling text 86 lines down. | Sub-class (b) narrative prose | qa-lead R-R3-1 | H.1 intro paragraph, line 1054 |

### Deferred-RECOMMENDED audit (11 items)

All 11 R2a-deferred RECOMMENDED items remain correctly deferred per CD R2 synthesis. None escalated to BLOCKING. No downstream AC blocked by absence of any deferred item. Items audited:

1. `OnPausedChanged` delegate type pin (MULTICAST vs DYNAMIC_MULTICAST) — queued for RSM `/consistency-check`
2. AC-WS-13 mock-fixture story-sequencing clause
3. AC-WS-21 LeanEaseCurve_Canonical immutability second fixture
4. AC-WS-31 pool-init activation state (bHidden=true, SetActorEnableCollision(false))
5. Primer pedagogy AC absent
6. AC-WS-14 boundary fixtures enumeration
7. Pause-flush `time_to_drain_ms` ≤ 16.6ms ceiling AC
8. AC-WS-22 gate-level downgrade candidate
9. F-3b drift derivation
10. Rule 9 direct AC
11. AWave module ownership pin

### Cross-system verification

- **DPC R13 closures (lines 165, 385, 944)**: HELD. `telegraph_window_s = 0.94f` struct default + `INACTIVE_SNAPSHOT_TW_DEFAULT = 0.94f` pre-init + `BARRAGE_SIMULTANEITY_WINDOW_S = 0.35s` all confirmed.
- **`design/registry/entities.yaml`**: `TELEGRAPH_WINDOW_FLOOR_S = 0.70`, `TELEGRAPH_WINDOW_FLOOR_MAX_ADMISSIBLE_S = 0.76`, `BARRAGE_SIMULTANEITY_WINDOW_S = 0.35` all present. Registry-side gap (`BARRAGE_SIMULTANEITY_WINDOW_S.referenced_by` does not list wave-spawner) is a `/consistency-check` pass item, not a Wave Spawner R3 finding.
- **`docs/architecture/platform-seam-interfaces.md`**: Seam 13 CLEAN. `IWaveSpawnerCallback` (line 1482) + `FWaveSpawnerCallbackTestStub` (line 1571) both authored. No paper-only-seam regression. R2a-3 reframe (post-hoc pool inspection) verified — no `FWaveSpawnActorCounter` authoring required.

### R3 forecast vs actual

- **R3 forecast (per R2a closure)**: 0-3 BLOCKING / 3-6 RECOMMENDED. Confidence MEDIUM.
- **R3 actual**: 1 BLOCKING / 1 RECOMMENDED / 0 NICE-TO-HAVE.
- **Status**: **CALIBRATED ✓** (first calibrated forecast for Wave Spawner; Pull-Wave R13 precedent matched).
- **Trajectory R1(9) → R2(8) → R3(1)**: monotonically decreasing across three reviews.

### Decomposition trigger ruling

UNARMED. R3 surface (1 BLOCKING) is well below the >8 BLOCKING + structural-distribution threshold. Trajectory monotonically decreasing across 3 reviews. CD R2 synthesis trigger semantics preserved.

### Files modified by R3 pass

- (No files modified — R3 is the verdict-bearing review; closure deferred to R3a in-session revision.)
- This file (review log) appended with R3 + R3a entries at 2026-06-22 closure tally.

### Files NOT modified by R3 pass (intentional — review-only)

- `design/gdd/wave-spawner-pattern-library.md` — fresh-context review is read-only on target GDD; R3a applies the edits.
- `docs/architecture/platform-seam-interfaces.md` — no seam changes; Seam 13 confirmed clean.
- `design/registry/entities.yaml` — no registry-backed value changes from R3.
- Sibling GDDs (PM, IS, RSM, DPC, Pull-Wave) — out of R3 scope. RSM `RunSeed` + `OnPausedChanged` delegate type + DPC `OnPostTickFrameStatePublished` still queued for next `/consistency-check`.

---

## Review R3a — 2026-06-22 — In-session author revision

**Mode**: in-session author revision per qa-lead R3 grep-verification synthesis "R3a closure path" recommendation (Pull-Wave R14 precedent); same-session execution because all R3 items are inscription-class single-site closures (no design-shaped questions requiring fresh-context reflection). User chose "R3a in-session now (Recommended)" via AskUserQuestion.

### R3a closure tally

**2 R3a binding decisions** applied (R3a-1 + R3a-2). Header binding-decisions block prepended to GDD documenting both. The 1 R3 BLOCKING closed + 1 RECOMMENDED applied. No new ACs added (both items are inscription edits to existing text).

| R3a | Closure | Source finding | GDD touch sites |
|---|---|---|---|
| R3a-1 | E3 lateral row lifecycle inscription fix: replaced "at `Initialize()`" with "at the first-world-load hook (`FCoreUObjectDelegates::OnPostLoadMap`)" + audit-trail annotation citing R2a-4 sibling fixes (Rule 11, Engine integration §, E1, AC-WS-20, AC-WS-31). | qa-lead B-R3-1 (sub-class (e) oracle-site drift) | E3 row, line 936 |
| R3a-2 | H.1 intro narrative-count refresh: "11 binding cook-time checks Rule 15 enumerates" → "14 binding cook-time checks"; AC range "AC-WS-01 through AC-WS-09 + AC-WS-32 through AC-WS-35" → "AC-WS-01 through AC-WS-09 + AC-WS-32 through AC-WS-38" + audit-trail annotation citing R2a-5 + R2a-12 sibling additions. | qa-lead R-R3-1 (sub-class (b) narrative drift) | H.1 intro paragraph, line 1054 |

### GDD metrics post-R3a

- **Line count**: 1176 → ~1188 (+~12 net from R3a header block + 2 in-place expansions).
- **AC count**: 38 (unchanged — R3a applied no AC additions).
- **BLOCKING-at-story-Done**: 33 (unchanged).
- **ADVISORY-at-story-Done + BLOCKING-at-Alpha**: 5 (unchanged).
- **Cook-time checks (Rule 15)**: 14 (unchanged — R3a applied no Rule 15 additions; only narrative refresh of stale "11" claim).
- **Open Questions**: 5 OPEN (unchanged from R1a).

### Files modified by R3a pass

- `design/gdd/wave-spawner-pattern-library.md` — primary target; R3a binding-decisions header block prepended (~+13 lines) + 2 in-place inscription edits at lines 936 (E3) + 1054 (H.1).
- `design/gdd/reviews/wave-spawner-pattern-library-review-log.md` — this file; R3 + R3a entries appended.

### Files NOT modified by R3a pass (intentional)

- `docs/architecture/platform-seam-interfaces.md` — no seam changes; Seam 13 unchanged.
- `design/registry/entities.yaml` — no registry-backed value changes; `referenced_by` gap for `BARRAGE_SIMULTANEITY_WINDOW_S` is queued for `/consistency-check`.
- `design/gdd/difficulty-phase-controller.md` — Pull-Wave + DPC R14-closed cascade NOT re-opened; DPC R13 closures verified held in R3 pass.
- `design/gdd/pull-wave-behavior.md` / `design/gdd/run-state-machine.md` — out of scope; forward contracts queued.
- `design/gdd/systems-index.md` — Wave Spawner row update at R3a closure (user-prompted).
- `production/session-state/active.md` — STATUS block + Progress + Session Extract for R3+R3a (user-prompted).
- `docs/architecture/adr-NNNN-wave-spawner-subsystem-hosting.md` — does not exist yet; downstream artifact authorship.

### R4 forecast (per qa-lead R3 synthesis)

- **BLOCKING band**: 0.
- **RECOMMENDED band**: 0-1 (only the Dependencies lateral row 9 "5 named events" prose at line 927 — sub-class (b) drift — could surface if R4 reviewer sweeps it; could also have been folded into R3a but was outside R3 surface).
- **Confidence**: HIGH. All oracle sites now accounted for; no open design questions remain; deferred items all correctly deferred; cross-system cascade clean.
- **R4 mode recommendation (per qa-lead R3 report)**: **terminal-now** (skip R4) OR solo qa-lead grep confirmation pass (Pull-Wave R14 precedent). The E3 fix is single-line lifecycle-anchor inscription; the H.1 prose fix is counter refresh. After these two edits, GDD is structurally sound with all R2a closures confirmed held + R3 closures applied.

### Strategic notes

- **R3 → R3a transition validated CD's "in-session inscription" closure-path pattern at a third cycle** (R1a + R2a + R3a). 1 BLOCKING + 1 RECOMMENDED closed in one author pass; no design-shaped questions required fresh-context delegation. Pattern is now strongly evidenced for inscription-class Wave Spawner closure.
- **Forecast calibration record (Wave Spawner — post-R3)**:

  | Review | Forecast | Observed | Status |
  |---|---|---|---|
  | R1 | 8-15 (pre-review) | 9 | CALIBRATED ✓ |
  | R2 | 2-4 (R1a forecast) | 8 | 2× miss — first calibration data point |
  | **R3** | **0-3 (R2a forecast)** | **1** | **CALIBRATED ✓** (first calibrated post-R1) |
  | **R4** | **0 BLOCKING / 0-1 RECOMMENDED** | TBD | pending (HIGH confidence) |

- **Cross-system cascade unaffected**: Pull-Wave + DPC R14-closed cascade NOT re-opened. The R3a edits are entirely within the Wave Spawner GDD's own surface; no propagation cascade triggered.
- **Oracle-site sweep methodology evidence**: ADR-0004's sub-class (e) "pre-init constant definitions" and sub-class (b) "narrative pass-condition prose" both caught a real-world R3 BLOCKING + RECOMMENDED finding. R3a closure adds two more empirical data points to the methodology's track record (after Pull-Wave R12 + R13 corollary extensions).
- **Decomposition trigger UNARMED + TRAJECTORY HEALTHY**: monotonic decrease across R1(9) → R2(8) → R3(1) confirms inscription-class closure pattern is converging. R4 = 0 BLOCKING projected (HIGH confidence) — would CONFIRM the decomposition-not-needed assessment from R2 CD synthesis.

### Operative next step

R4 confirmation pass per `/clear` → `/design-review design/gdd/wave-spawner-pattern-library.md`. Solo qa-lead grep confirmation mode recommended (Pull-Wave R14 precedent). OR — per qa-lead R3 explicit recommendation — **terminal-now**: skip R4 and proceed directly to `/propagate-design-change` + downstream work (ADR authoring + sibling systems).

**Parallel tracks unchanged**:
- HISM/ISMC ADR authoring (Pull-Wave story-Done blocker; downstream artifact; assign unreal-specialist).
- `docs/architecture/adr-NNNN-wave-spawner-subsystem-hosting.md` (Wave Spawner story-Done blocker per OQ-WS-3 close; assign unreal-specialist).
- PM decomposition execution per `design/gdd/reviews/player-movement-decomposition-plan-2026-06-16.md` (~3-5 hours, separate `/clear` session).
- Telegraph System prototype (highest-risk bet per systems-index).
- RSM `/consistency-check` pass to land `RunSeed:uint64` + `OnPausedChanged` delegate type + DPC `OnPostTickFrameStatePublished` (R2a-2 forward contract) + Dependencies row 9 "5 named events" → "9 named events" stale prose (R4-deferral candidate).

---

## Downstream Artifact — ADR-0005 (Wave Spawner Subsystem Hosting) AUTHORED 2026-06-24

- **Path**: `docs/architecture/adr-0005-wave-spawner-subsystem-hosting.md`
- **Status**: Accepted (decision was locked at R1a 2026-06-21 OQ-WS-3 closure; this ADR is downstream artifact authorship per active.md framing, NOT re-adjudication).
- **Trigger**: User typed `next` post-`/clear`; recovered state surfaced operative next step = downstream artifact authoring per qa-lead R3 terminal-now recommendation + active.md NEXT primary lane. User chose lane via AskUserQuestion. Authored via `/architecture-decision` skill in lean review mode (TD-ADR skipped; unreal-specialist authored draft satisfying Step 5.5 engine specialist validation).
- **Decision (reproduced)**: `UWaveSpawnerSubsystem : public UGameInstanceSubsystem, public FTickableGameObject`. Pool allocated once per session at `FCoreUObjectDelegates::PostLoadMapWithWorld`; reused across all replays. Admission decision in `OnDPCFrameReady()` (bound to `UDPCSubsystem::OnPostTickFrameStatePublished`), NOT in `Tick()` — tick ordering structural via DPC delegate subscription rather than tick-group dependency. `ETickableTickType::Conditional` + `IsTickable()` gates idle overhead in `Cold`/`Idle` states. `STATGROUP_WaveSpawner` for Unreal Insights scope. Pool storage `UPROPERTY() TArray<TObjectPtr<AWave>>` (UE 5.0+ GC-safe modernization).
- **Alternatives documented + rejected**: (1) `UWorldSubsystem` — per-replay 16.6 ms pool-alloc hitch; (2) `AActor` singleton in `PersistentLevel` — manual GC anchoring + same per-reload hitch risk; (3) `UEngineSubsystem` — outlives `GameInstance`, leaks game-session state across sessions.
- **GDD sync landed in same pass (Step 5.7 of skill)**: `OnPostLoadMap` → `PostLoadMapWithWorld` across 8 sites in `design/gdd/wave-spawner-pattern-library.md` (lines 9, 18, 157, 457, 940, 942, 1107, 1139). `OnPostLoadMap` was a stale UE API name (oracle-site sub-class (b) drift per ADR-0004 methodology); the correct UE 5.7 delegate is `FCoreUObjectDelegates::PostLoadMapWithWorld` (post-load hook with `UWorld*` parameter; the older `PostLoadMap` form was deprecated). GDD sync was approved by user via AskUserQuestion.
- **Architecture registry updated**: 5 new stances appended to `docs/registry/architecture.yaml` per Step 6 user approval — (i) `api_decision: wave_spawner_subsystem_hosting`; (ii) `performance_budget: wave-spawner-subsystem-admission-tick ≤ 0.30 ms p99 mobile`; (iii-v) 3 forbidden patterns: `WaveSpawner_as_UWorldSubsystem`, `WaveSpawner_SpawnActor_at_Initialize`, `WaveSpawner_per_run_pool_reallocation`. Registry `last_updated` bumped to 2026-06-24.
- **Files modified by ADR-0005 authoring pass**:
  - `docs/architecture/adr-0005-wave-spawner-subsystem-hosting.md` (NEW, ~340 lines)
  - `design/gdd/wave-spawner-pattern-library.md` (8-site sync `OnPostLoadMap` → `PostLoadMapWithWorld`)
  - `docs/registry/architecture.yaml` (5 stances appended + `last_updated` bumped)
  - `design/gdd/reviews/wave-spawner-pattern-library-review-log.md` (this entry)
  - `production/session-state/active.md` (STATUS block update)
- **Files NOT modified by ADR-0005 authoring pass (intentional)**: `design/gdd/systems-index.md` (no Wave Spawner row change — verdict + closure trajectory unchanged); `docs/architecture/platform-seam-interfaces.md` (no new seams introduced by hosting decision); `design/gdd/difficulty-phase-controller.md` (R2a-2 DPC `OnPostTickFrameStatePublished` forward contract still queued for next `/consistency-check`); `design/gdd/run-state-machine.md` (RSM forward contracts unchanged); sibling GDDs (out of scope).
- **Wave Spawner story-Done blocker (OQ-WS-3) NOW UNBLOCKED.** Sibling ADR (OQ-PW-3 HISM/ISMC at `docs/architecture/adr-NNNN-pullwave-instanced-renderer.md`) remains the Pull-Wave story-Done blocker; not authored in this pass.
- **Strategic notes**:
  - The `OnPostLoadMap` → `PostLoadMapWithWorld` sync is an instance of ADR-0004 §Oracle-Site Sweep Corollary sub-class (b) drift (narrative naming inaccuracy across multiple sites) that was caught at ADR authoring time rather than at implementation. This validates the cross-doc sync check (Step 5.7 of `/architecture-decision`) as an effective late-catch surface for oracle-site drift that survives R-cycle reviews.
  - ADR-0005 establishes the precedent for downstream artifact authoring on a CLOSED review cycle: skill workflow is `/architecture-decision` invocation → unreal-specialist draft → GDD sync as needed → registry update → review log entry → active.md sync. The same pattern will apply to OQ-PW-3 HISM/ISMC ADR.
- **Operative next step (recommended primary)**: HISM/ISMC ADR authoring (`docs/architecture/adr-NNNN-pullwave-instanced-renderer.md` — Pull-Wave story-Done blocker; assign unreal-specialist; sibling pattern to ADR-0005). Or — `/architecture-review` in a fresh `/clear` session to validate ADR-0005 coverage against Wave Spawner GDD before sibling ADR authoring.

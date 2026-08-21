# ADR-0004: Dual-Grep Methodology for Cross-Document Constant Propagation

## Status
Proposed

## Date
2026-06-18

## Engine Compatibility

| Field | Value |
|-------|-------|
| **Engine** | Unreal Engine 5.7 (project-wide; ADR is engine-agnostic) |
| **Domain** | Documentation / Cross-System Constant Propagation Methodology |
| **Knowledge Risk** | None — methodology ADR, not an engine API decision |
| **References Consulted** | Pull-Wave R11 fresh-context re-review entry (`design/gdd/reviews/pull-wave-behavior-review-log.md`); R10d cross-system survivability coordination resolution append (`design/gdd/reviews/cross-system-survivability-coordination-2026-06-11.md`); R11a-arithmetic propagation pass (this ADR's authoring trigger) |
| **Post-Cutoff APIs Used** | None |
| **Verification Required** | None — methodology ADR |

## ADR Dependencies

| Field | Value |
|-------|-------|
| **Depends On** | None |
| **Enables** | All future cross-system constant propagation passes (e.g., `/propagate-design-change` skill invocations; manual propagation triggered by CD rulings on registry-backed parameters) |
| **Blocks** | None — methodology is advisory, not gating |
| **Ordering Note** | Adopt before the next FLOOR-touching parameter change OR the next registry-backed constant raise. Pull-Wave R10d revealed the methodology gap; this ADR is the closure. |

## Context

### Problem Statement

Pull-Wave R11 fresh-context re-review (2026-06-17) surfaced 4 stale-residue lines in
`design/gdd/pull-wave-behavior.md` that had survived 5 sequential reviews
(R3 / R5 / R7 / R9 / R10a-d) despite each review including a propagation sweep of
`TELEGRAPH_WINDOW_FLOOR_S` references after the constant's prior raises (R2 Cluster E
0.6→0.65s; R10d 0.65→0.70s):

- Line 408 (boundary table note): `"Waves survive longer than 4.35 s"` — pre-R2
  residue derived as `0.60 + 15.0/4.0` (pre-R2 FLOOR=0.60s + wrong offset 15.0 m for a
  row whose offset row is `>25.0 m`). Should be ~7.12s under R10d FLOOR=0.70s.
- Line 596 (cook-time warning): `"LeanDurationS < 0.65s"` — stale at R10d=0.70s.
- Line 640 (knob-interaction prose): `"0.65s lean + 0.83s = 1.48s"` — stale; internal
  contradiction with line 393 which is correctly 1.53s under R10d.
- Line 953 (AC-PW-22a state-mix derivation): `TELEGRAPH_WINDOW_FLOOR_S = 0.65s` +
  total lifetime `2.25s` + annotation citing R2-path-b as most recent raise — three
  stale literals + outdated annotation.

### Root Cause

Prior propagation passes used **single-pattern grep** (e.g., `grep "0.65"` or
`grep "TELEGRAPH_WINDOW_FLOOR_S"`) which catches:

- **Named-constant token references** (`TELEGRAPH_WINDOW_FLOOR_S` symbol appearances)
- **Literal-value appearances of the current value** (`0.65` literal occurrences)

But misses:

- **Derived arithmetic literals** — values that are the *result* of arithmetic where
  the constant appears as an operand. Line 408's `4.35s` contains no `FLOOR`, no
  `0.65`, and no `0.70` — it is the result of `0.60 + 15.0/4.0` and the inputs
  rotated three times across prior raises without anyone re-evaluating the result.
- **Composite values where the constant is one factor** — e.g., line 640's `1.48`
  is `0.65 + 0.83` where 0.83 is itself `15.0/18.0`. Single-pattern grep on `0.65`
  catches the lean term but not the resulting sum.
- **Stale-annotation references** — e.g., line 953's "R2-path-b FLOOR raise
  0.6s→0.65s" annotation accurately cited R2 as the most-recent raise *at R7 time*
  but is stale after R10d. Annotation drift is invisible to constant-token grep.

The Pull-Wave GDD at 1039 lines is past the single-author eyeball-sweep coherence
ceiling for these residues. Five reviews missed them because each review's grep
sweep was constrained to named-constant + current-value forms — the **derived
arithmetic surface** was never within scope.

### Requirements

- Future cross-system constant propagation passes must catch derived-arithmetic-literal
  residues AND annotation drift, not just named-constant + current-value references.
- The methodology must be runnable manually by an author in a focused revision pass
  (no tooling dependency for adoption).
- The methodology must add bounded overhead — propagation passes are already
  multi-doc, multi-cluster, context-heavy; a "scan everything from scratch every
  time" approach is not viable.
- The methodology must be auditable in review logs — a reviewer must be able to
  verify the dual-grep sweep was actually performed against the relevant constants.

## Decision

### Dual-Grep Technique

Future cross-system constant propagation passes (whether triggered manually by a CD
ruling or via `/propagate-design-change` skill invocation) MUST perform a **dual-grep
sweep** across all target documents:

1. **Literal grep for the constant value** — grep for the current numeric literal
   (e.g., `grep -n "0.65" doc.md`) AND for the prior numeric literal (e.g.,
   `grep -n "0.6 " doc.md` and `grep -n "0.6s" doc.md` for the pre-R2 residue case).
   Pre-existing residues from prior raises that escaped earlier sweeps must be in
   scope; the propagation pass closes the cumulative debt, not just the latest delta.
2. **Computed-result grep for known derived values** — enumerate the arithmetic
   expressions that include the constant as an operand in the source document(s),
   compute the pre-raise and post-raise result for each, and grep for both forms.
   Example for R10d (FLOOR=0.65→0.70s on Pull-Wave):
   - Lifetime `FLOOR + travel + 0.15 + 0.016` at default travel `0.83s`:
     pre-R10d `2.05s`; pre-R2 `2.00s`; post-R10d `2.10s`. Grep `1\.48|1\.53|2\.0|2\.05|2\.10|2\.15|2\.20|2\.25|2\.30`.
   - Knob-interaction sum `FLOOR + 0.83`: pre-R10d `1.48s`; post-R10d `1.53s`.
   - Boundary-row lifetime at max offset / min velocity `FLOOR + 25/4 + 0.15 + 0.016`:
     pre-R2 `~6.91s` (with old offset `0.60 + 15/4`= `4.35s` is the actual bug);
     post-R10d `~7.12s`. Grep `4\.35|6\.90|6\.95|7\.10|7\.12`.
   - Half-floor (e.g., `BARRAGE_SIMULTANEITY_WINDOW_S = FLOOR/2`): pre-R10d
     `0.325s`; post-R10d `0.35s`. Grep `0\.325|0\.35`.
3. **Annotation grep for stale provenance references** — grep for phrases like
   `"R2-path-b"`, `"most recent raise"`, `"PROVISIONAL"`, `"R9 coordination"` to
   surface annotation prose whose accuracy depends on which raise is current. Each
   hit must be manually evaluated against the post-pass state.
4. **Manual review of arithmetic expressions** — any arithmetic expression in the
   document(s) that contains the constant as an operand must be re-evaluated
   against the new value. The dual-grep surfaces candidates; manual evaluation
   closes the loop.

### When to Apply

Apply the dual-grep technique on any of the following triggers:

- Registry-backed constant value change (any `constants:` entry in
  `design/registry/entities.yaml` whose `value` field changes)
- CD ruling on a cross-system parameter raise/lower
- `/propagate-design-change` skill invocation post-review-APPROVED
- Pre-author-pass setup for a fresh-context propagation pass (the propagation
  author's first action is to run the dual-grep and capture the hit list)

### Capture in Review Logs

Each propagation pass MUST capture in the review log entry:

- The constant(s) being propagated and the value delta
- The literal-grep patterns run (both current and prior values)
- The computed-result patterns derived from the source GDD's arithmetic
- The annotation-grep patterns run
- The total hit count per pattern and the deltas applied per hit
- Any residues left unfixed (e.g., historical-derivation prose intentionally
  preserved) with rationale

### Oracle-Site Sweep Corollary (R12 + R13 extensions)

**R12 lesson (2026-06-18)**: The dual-grep technique as originally specified targets
**formula sites** — arithmetic expressions whose operand is the propagated constant —
but does NOT cover **oracle sites**: locations where the constant's value appears as
a pass condition or reference fixture against which downstream code/ACs assert. R12
discovered that R11a-arithmetic's first deployment patched formula sites cleanly but
left 5 oracle sites in DPC stale (AC-07a/07b pass-condition values, canonical test
fixture preamble, AC-09a inactive snapshot OPENER key, line 357 anchor reference
table). A correct R10d-bound implementation would have FAILED these ACs; a regression
to pre-R10d would have PASSED.

**Required sweep extension** — in addition to the four steps in §Dual-Grep Technique,
the propagation author MUST sweep these oracle-site sub-classes:

(a) **AC pass-condition values** — `THEN <field> == <value>` and `EXPECT(<field>, <value>)`
forms in acceptance criteria bodies.
(b) **Test fixture preambles** — `GIVEN curve keys = (...)` or canonical fixture
declarations in AC GIVEN clauses.
(c) **Anchor reference tables** — per-phase / per-tier default value tables that
downstream authors read as numeric anchors (e.g., DPC's line-357 anchor table of
`OPENER / MID / PEAK` default targets).

**R13 lesson (2026-06-19)**: R13 caught a residual sub-class of oracle sites that
R12's corollary enumeration missed. The R11a-arithmetic dual-grep + R12 oracle sweep
both missed:

(d) **Struct default-value initializers** — e.g., `float telegraph_window_s = 0.94f;`
in `F*State` USTRUCT struct bodies. A default-constructed instance returns this
value, which downstream ACs assert against (DPC line 165 was R13's residual site:
`FDPCFrameState` default initializer at `0.9f` would have failed AC-09a's
`telegraph_window_s == 0.94` assertion).
(e) **Pre-init constant definitions** — e.g., `INACTIVE_SNAPSHOT_TW_DEFAULT = 0.94f`
constants paired with explanatory prose comments. These are oracle sites because they
define the value that a pre-initialization-phase consumer observes (e.g., between
`Initialize()` and async curve-load completion); downstream ACs assert against them.
The paired prose comment is itself oracle-adjacent — stale arithmetic examples in the
prose silently mis-document the contract (DPC line 385 was R13's residual site: the
prose example `floor rise to 0.9s would produce 1.35s ≠ authored OPENER 0.9s` was
double-stale post-Path-B + post-R10d).

**Methodology corollary as of R13**: the oracle-site sweep MUST also grep for
`= [VALUE]f;` patterns in struct bodies, `*_DEFAULT = [VALUE]` patterns in constant
blocks, and `*_DEFAULT.*mirrors authored.*[VALUE]` patterns in paired prose-comment
text, AND manually verify each paired prose example arithmetic is internally
consistent with the post-pass constant value.

**Note**: This is the second corollary expansion ADR-0004 has needed in two reviews.
The oracle-site concept is still being charted; treat further sub-class discoveries
as expected and append them as encountered, rather than treating the enumeration as
closed.

### Future Tooling Direction (Out-of-Scope for This ADR)

A future tooling pipeline could automate the dual-grep technique:

- Parse `design/registry/entities.yaml` to enumerate registry-backed constants
- Parse source GDDs to enumerate arithmetic expressions containing each constant
- Compute the result table per (constant, expression) under any proposed value
- Run the dual-grep automatically when a registry value changes

This is filed as future work (likely a `tools/registry-propagation/` script) and
is NOT a prerequisite for adoption of the dual-grep methodology itself. The
methodology runs by hand today; tooling reduces overhead later.

## Consequences

### Positive

- Eliminates the stale-residue survival pattern that R11 surfaced (4 lines that
  survived 5 sequential reviews); the methodology is the correct surface for
  catching derived-arithmetic-literal drift.
- Catches annotation drift (e.g., "R2-path-b FLOOR raise 0.6s→0.65s" stale
  citations) that single-pattern grep misses entirely.
- Forces explicit enumeration of arithmetic expressions, making the propagation
  surface auditable in review logs.

### Negative

- Adds ~15-20 minutes per cross-system propagation pass (the upfront grep
  enumeration + computed-result derivation). For a pass that already runs 90-120
  minutes (R11a-arithmetic scope), this is a ~15-20% overhead increase.
- The author must compute the derived-result table by hand until tooling lands;
  computation errors in the table create the risk of *introducing* stale residues
  while fixing prior ones. Mitigation: capture the derivation in the review log
  so a reviewer can audit the computation.
- Annotation grep is fuzzy (no canonical pattern set); the author must use
  judgment on which annotation phrases need re-evaluation.

### Neutral

- The methodology is engine-agnostic and applies to all `design/gdd/` and
  `design/registry/` propagation passes regardless of engine choice.
- The methodology does not constrain the *content* of design decisions — it
  constrains the *mechanics* of propagating those decisions across documents.

## Alternatives Considered

### Alternative A: Strict no-arithmetic-in-prose rule

Forbid all arithmetic literals in GDD prose; require constants to appear by name
only (e.g., `"Waves survive longer than TELEGRAPH_WINDOW_FLOOR_S + 25.0/PULL_WAVE_VELOCITY_MIN_MS + WAVE_DESPAWN_HOLD_S"` instead of `"7.12s"`).

**Rejected** — too costly for SLIPSTORM's derivation-heavy GDD style. Pull-Wave's
F-formula sections rely on worked-example arithmetic for designer + tuner
readability; replacing every literal with a symbolic expression makes the
documents unreadable as design references. Also doesn't help with annotation
drift (provenance citations still go stale regardless of arithmetic style).

### Alternative B: Automated symbol substitution pipeline

Build a tool that parses GDDs as templates with `{{TELEGRAPH_WINDOW_FLOOR_S}}` and
`{{LIFETIME(travel=0.83)}}` placeholders, renders to plain Markdown at build time,
and rejects any hand-edited rendered value that diverges from the template.

**Rejected for current scope** — out of scope for the current sprint; would
require a parser, template engine, build-pipeline integration, and per-GDD
template authoring. Filed as future work; the dual-grep methodology is the
manual interim solution that catches the same class of failure with bounded
process overhead.

### Alternative C: Author-level discipline ("just read more carefully")

Rely on the author's manual diligence to catch derived-arithmetic-literal drift
during propagation passes.

**Rejected** — R11 demonstrated empirically that author-level discipline does NOT
scale past the single-author eyeball-sweep coherence ceiling (~1000 lines per
doc, per the Pull-Wave R11 analysis). The 4-line residue surviving 5 reviews is
the falsification of this alternative. The dual-grep methodology is a
process-level closure for an attention-budget gap that author-level discipline
cannot close at the document scale SLIPSTORM has reached.

## Risks

| Risk | Probability | Impact | Mitigation |
|---|---|---|---|
| Author miscomputes the derived-result table and introduces new residues | Medium | Medium | Capture the derivation in the review log so a reviewer can audit. Two-author cross-check on the table for high-stakes propagations (CD rulings on Pillar invariants). |
| Dual-grep overhead becomes a blocker for small propagation passes | Low | Low | The methodology is advisory, not gating. For trivial single-site changes (e.g., a one-line value update in a single doc), the dual-grep step can be skipped if the author certifies the change has no derived-arithmetic surface. |
| Annotation grep is fuzzy and misses provenance references | Medium | Low | Annotation drift typically doesn't break invariants (it's traceability decay). Captured-in-review-log audit catches it on the next review pass even if the propagation pass misses it. |

## Validation Criteria

- [ ] Next cross-system constant propagation pass invokes the dual-grep technique and captures the literal/computed/annotation patterns in the review log entry.
- [ ] R12 fresh-context re-review of Pull-Wave + DPC R-Updated re-review confirms zero new stale-residue lines from the R11a-arithmetic propagation pass (the methodology's first real-world test).
- [ ] At least one future propagation pass after R11a-arithmetic explicitly cites this ADR in its review log as the methodology authority.

## GDD Requirements Addressed

| GDD Document | System | Requirement | How This ADR Satisfies It |
|---|---|---|---|
| `design/gdd/pull-wave-behavior.md` | Pull-Wave | F-BARRAGE-SURVIVABILITY-INVARIANT cross-system arithmetic must be re-evaluated against current `TELEGRAPH_WINDOW_FLOOR_S` value on every raise | Dual-grep technique forces re-evaluation of all derived arithmetic literals (e.g., `4.35s`, `1.48s`, `2.25s`) against the new value, not just the named-constant references |
| `design/gdd/difficulty-phase-controller.md` | DPC | Rule 9 PROVISIONAL framing under R10d two-phase gate restructure; TelegraphWindowCurve Path B reauthoring per R6 Q7 binding rule on FLOOR raise | Dual-grep surfaces the computed-result drift (e.g., MID overlap window 0.12s vs collapsed 0.02s under pre-R10d keys) that single-pattern grep misses; annotation-grep surfaces the stale "FLOOR stays at 0.65s per R9 verdict" Tuning Knob note that R10d invalidates |
| `design/gdd/game-concept.md` | Game Concept | Core Fantasy "half-second" prose must reflect current `TELEGRAPH_WINDOW_FLOOR_S` accurately (Pillar 2 communication contract) | Dual-grep's annotation pattern surfaces aesthetic-prose drift (40% discrepancy between "half-second" and R10d FLOOR=0.70s) that constant-grep cannot detect |
| `design/registry/entities.yaml` | Registry | Registry constants must be the single source of truth; downstream GDD references must propagate within one review cycle of a value change | Dual-grep technique is the propagation mechanism; this ADR formalizes it as binding methodology for registry-backed constants |

## Related

- ADR-0001, ADR-0002, ADR-0003: prior ADRs (not directly related; this ADR establishes a propagation methodology that future ADRs and propagation passes inherit)
- `design/gdd/reviews/pull-wave-behavior-review-log.md` R11 fresh-context re-review entry (2026-06-17): the empirical trigger for this ADR — the 4-line stale-residue finding that surfaced the methodology gap
- `design/gdd/reviews/difficulty-phase-controller-review-log.md` R-Updated mini-review entry (2026-06-18): the second-document confirmation of the methodology gap — the TelegraphWindowCurve cascade collapse and 14-site FLOOR drift were both caught by the dual-grep approach during R11a-arithmetic propagation
- `design/gdd/reviews/cross-system-survivability-coordination-2026-06-11.md` R10d resolution append (2026-06-18): cites this ADR as the methodology authority for the R11a-arithmetic propagation pass
- Future work: `tools/registry-propagation/` script to automate the dual-grep technique (out-of-scope for this ADR; filed as a follow-up tooling task)

# Change Impact: Pull-Wave + DPC R13/R14 Closure Cascade

**Date**: 2026-06-19
**Trigger**: Pull-Wave + DPC R13 lean BATCHED re-review (Pull-Wave APPROVED, DPC NEEDS REVISION → 1 BLOCKING closed in-session) + R14 solo qa-lead confirmation (APPROVED on first pass; 0 findings) — both DONE 2026-06-19. R7→R14 review trajectory CLOSED for both GDDs.
**Skill invocation**: `/propagate-design-change` (adapted — see §Skill Adaptation Note below)
**Review mode**: lean (TD-CHANGE-IMPACT gate skipped per skill Phase 6b lean-mode skip)
**Supersedes**: none (additive to `docs/architecture/change-impact-2026-06-11-player-movement.md` which propagated the parallel PM-side coordination; this doc closes the Pull-Wave + DPC side after the R10d → R11 → R12 → R13 → R14 trajectory).

---

## Executive Summary

The Pull-Wave + DPC R13/R14 cycle landed with Pull-Wave APPROVED + DPC APPROVED post-1-BLOCKING-inscription. Decomposition trigger RETIRED. Two consecutive calibrated forecasts (R13 1/1 + R14 0/0) confirmed forecast model recovery. ADR-0004's §Oracle-Site Sweep Corollary was extended at R13 with sub-classes (d) struct-default initializers + (e) pre-init constant definitions and held through R14.

This propagation pass closes the remaining cross-doc cascade deferred from R10d → R12 → R13: BARRAGE value flip (registry hold-condition satisfied by R14), TELEGRAPH_WINDOW_FLOOR_MAX_ADMISSIBLE_S codification (CD R12 derived ceiling promoted to registry), and DPC line 717 stale prose retirement (CD R12 Ruling 1 inscribed).

**User adjudication (2026-06-19)**: both decisions = recommended path.

1. **BARRAGE_SIMULTANEITY_WINDOW_S**: value flipped 0.3 → 0.35s (restores FLOOR/2 design ratio at FLOOR=0.70s).
2. **TELEGRAPH_WINDOW_FLOOR_MAX_ADMISSIBLE_S**: codified in registry at 0.76s as derived ceiling.

**Cross-GDD scope**: 5 files (entities.yaml + DPC GDD × 2 sites + Pull-Wave GDD × 1 site + cross-system-survivability-coordination doc). No ADRs require revision (ADR-0004 already extended at R13; ADRs 0001/0002/0003 do not reference Pull-Wave/DPC).

---

## Skill Adaptation Note

The `/propagate-design-change` skill's strict Phase 3 (`git show HEAD:design/gdd/[filename].md`) does not apply here because both target GDDs (`pull-wave-behavior.md` + `difficulty-phase-controller.md`) are uncommitted (staged-as-new only; no git history). The skill's diff mechanism short-circuits to "no previous version" for both.

**Adapted approach**: substituted the per-review change history (R10d → R12 → R13 → R14 entries in `pull-wave-behavior-review-log.md` + `difficulty-phase-controller-review-log.md` + this active.md trajectory) as the "diff input" — these documents canonically record what changed at each review pass and form the substantive equivalent of a git diff trace.

ADR impact analysis (skill Phase 5) was bounded by a `grep -l "pull-wave\|difficulty-phase\|TELEGRAPH_WINDOW_FLOOR\|BARRAGE_SIMULTANEITY"` across `docs/architecture/adr-*.md` which surfaced only ADR-0004 — already extended at R13 and held through R14. The skill's Phase 4-5 ADR-walk would find zero affected ADRs requiring new revision.

TD-CHANGE-IMPACT director gate (skill Phase 6b) skipped per lean-mode policy.

---

## Change Summary — Propagation Targets

| Target | Site | Before | After | Rationale |
|---|---|---|---|---|
| `entities.yaml` | `BARRAGE_SIMULTANEITY_WINDOW_S.value` | `0.3` | **`0.35`** | Restores FLOOR/2 design ratio (= 0.70/2) at FLOOR=0.70s; R11a-arithmetic hold-condition "DPC R-Updated re-review confirmation" satisfied by R14 APPROVED |
| `entities.yaml` | new constant `TELEGRAPH_WINDOW_FLOOR_MAX_ADMISSIBLE_S` | (did not exist) | **`0.76` seconds** | Codified derived ceiling under Constraint A (`OPENER_key ≤ 1.0s ⇒ FLOOR ≤ 0.76s` under Path B); registry-driven tooling/test-harness discoverability |
| `entities.yaml` | header `last_updated` | `"2026-06-18"` | `"2026-06-19"` | Sync to propagation pass date |
| `difficulty-phase-controller.md` | line 944 (AC-PILLAR-2-BARRAGE Path A) | `BARRAGE_SIMULTANEITY_WINDOW_S = 0.3s` | `BARRAGE_SIMULTANEITY_WINDOW_S = 0.35s` + 2026-06-19 closure annotation | Lock-step oracle-site update per ADR-0004 dual-grep methodology |
| `pull-wave-behavior.md` | line 535 (EC-CONCURRENT-LANDING-ON-SAME-LANE) | `M ≤ 3 onsets within W = 0.3s` | `M ≤ 3 onsets within W = 0.35s` + 2026-06-19 closure annotation | Lock-step oracle-site update per ADR-0004 dual-grep methodology |
| `difficulty-phase-controller.md` | line 717 (TelegraphWindowCurve Tuning Knob row) | `**FLAGGED FOR CD POST-PASS REVIEW**: confirm Path B over Path C ...` (stale, CD R12 Ruling 1 ratified Path B 2026-06-18) | `**CD R12 Ruling 1 (2026-06-18) RATIFIED**: Path B canonical ...` + rationale citing Weber JND (Getty 1975, Grondin 2010) + cascade reference to AC-PILLAR-2-CONCURRENT Wilson-LCB gate at line 921 | Stale flag retirement; CD ruling inscribed |
| `cross-system-survivability-coordination-2026-06-11.md` | end of doc | (R10d resolution entry was tail-end) | 2026-06-19 R13 + R14 Closure + /propagate-design-change Entry appended (final trajectory, forecast calibration record, propagation cascade list, forward contracts for Wave Spawner GDD) | Permanent record at the cross-system coordination authoritative location |

---

## ADR Impact Analysis

### Walked (4 ADRs)

| ADR | Title | References Pull-Wave / DPC? | Status |
|---|---|---|---|
| ADR-0001 | Palm rejection / R_max calibration | No (input-system scope) | ✅ Still Valid |
| ADR-0002 | Haptic platform bridge | No (input-system / accessibility scope) | ✅ Still Valid |
| ADR-0003 | Drain queue architecture | No (input-system scope; verified via `grep`) | ✅ Still Valid |
| ADR-0004 | R11a Dual-Grep Methodology | Yes (methodology ADR for Pull-Wave/DPC propagation) | ✅ Still Valid — extended at R13 with §Oracle-Site Sweep Corollary additions (sub-classes (d) struct-default initializers + (e) pre-init constant definitions); held through R14 |

### Needs Review

None.

### Likely Superseded

None.

### New ADRs Required

None. The propagation is tuning-knob coordination (registry value flip + derived ceiling codification + stale prose retirement), not architectural decisions. No new architectural surface introduced.

---

## Resolution Decisions

| # | Decision point | User selection (2026-06-19) | Why recommended |
|---|---|---|---|
| 1 | `BARRAGE_SIMULTANEITY_WINDOW_S` value | **Bump 0.3 → 0.35s** | Restores semantic intent (FLOOR/2 design ratio); entities.yaml line 269-271 flagged the 50ms shift as the R10d perceptibility-onset safety net that "may warrant the value flip rather than holding at 0.3"; aligns operational value with derivation target |
| 2 | `TELEGRAPH_WINDOW_FLOOR_MAX_ADMISSIBLE_S` codification | **Codify in entities.yaml at 0.76s** | Discoverability for tooling/test harnesses; matches R10c + R11a-arithmetic registry-canonicalization pattern; prevents indefinite PATH (i) FLOOR raises (paired with PATH (i) iteration cap = 1 raise at Pull-Wave line 941) |

Both decisions are recommended path; mechanical bundle (cross-system doc append + DPC line 717 retirement + this change-impact doc) approved alongside.

---

## Cross-System Invariant Verification

**F-BARRAGE-SURVIVABILITY-INVARIANT** (Pull-Wave) — unchanged by this propagation pass.

- Closure arithmetic (R10d locomotion-only invariant): `SLIP_TWEEN × MIN_ESCAPE_SLIPS + REACT ≤ TELEGRAPH_WINDOW_FLOOR_S`
- At REACT default (0.20s): `0.15 × 2 + 0.20 = 0.50 ≤ 0.70` ✓ **200ms locomotion-only margin**
- At REACT ceiling (0.25s): `0.15 × 2 + 0.25 = 0.55 ≤ 0.70` ✓ **150ms locomotion-only margin**
- Direction-detection extension (R10d perceptibility-onset model): the perceptibility-onset budget (~0.27s for 1°) is added on top of the locomotion-only invariant — under R10d the FLOOR=0.70s raise closes the cross-system cliff under the direction-detection model that was the empirical basis for the CD PATH (i) ruling.
- `BARRAGE_SIMULTANEITY_WINDOW_S` (W) is the **simultaneity window** (max time span across M onsets within a single barrage), **NOT** the escape budget. The W value flip 0.3 → 0.35 widens the barrage authoring envelope (allows more dispersed onset clustering within a single barrage) but does not alter the cross-system survivability arithmetic.

**R9 Option (c)** (Wave Spawner cook-time consecutive-triplet exclusion + `MIN_ESCAPE_SLIPS=2`) REMAINS BINDING. R14 confirmed no mechanism change.

---

## Forward Contracts Now Operative for Wave Spawner & Pattern Library GDD (when authored)

When `design/gdd/wave-spawner-pattern-library.md` is authored, it MUST inherit:

1. **R10d FLOOR=0.70s** TelegraphWindowCurve binding + Path B key set `(0.0, 0.94), (0.333, 0.82), (0.75, 0.70), (1.0, 0.70)` (CD R12 Ruling 1 canonical; Path C rejected)
2. **R10d pool-lifetime ceiling +50ms tag** (informational)
3. **R10c pool_size = 23 derivation** + `DESPAWNING_RETURN_LATENCY_SLOTS = 2` explicit term + `MAX_CONCURRENT_WAVES_CAP_CEIL_MARGIN = 5`
4. **R10c Seam 13 `FWaveSpawnerCallbackTestStub::SetOnDespawnedUserCallback`** slot (test-stub only; production `FWaveSpawnerCallback_Production` unchanged)
5. **R8 RC-E PEAK barrage minimum-tier exclusion** (R9 Option (c) BINDING)
6. **R7 B8 7-surviving-triplet pattern library** binding
7. **`BARRAGE_SIMULTANEITY_WINDOW_S = 0.35s`** (R14 closure 2026-06-19; restored FLOOR/2 ratio; barrage authoring constraint `M ≤ MAX_PULLS_PER_BARRAGE=3 onsets within W=0.35s`)
8. **`TELEGRAPH_WINDOW_FLOOR_MAX_ADMISSIBLE_S = 0.76s`** ceiling (registry-codified 2026-06-19; PATH (i) iteration cap = 1 raise from current FLOOR=0.70s; 60ms headroom)

---

## Files Modified by This Pass

1. `design/registry/entities.yaml` — header `last_updated` synced 2026-06-19; `BARRAGE_SIMULTANEITY_WINDOW_S.value` flipped 0.3 → 0.35; BARRAGE notes 2026-06-19 R14 closure entry appended; NEW constant `TELEGRAPH_WINDOW_FLOOR_MAX_ADMISSIBLE_S = 0.76s` entry added between TELEGRAPH_WINDOW_FLOOR_S and MAX_PULLS_PER_BARRAGE. ~+50 lines net.
2. `design/gdd/difficulty-phase-controller.md` — line 944 oracle-site lock-step update + line 717 stale flag retirement. ~+10 lines net.
3. `design/gdd/pull-wave-behavior.md` — line 535 oracle-site lock-step update. ~+1 line net.
4. `design/gdd/reviews/cross-system-survivability-coordination-2026-06-11.md` — 2026-06-19 R13 + R14 Closure entry appended (final trajectory + forecast calibration record + propagation cascade + forward contracts). ~+30 lines net.
5. `docs/architecture/change-impact-2026-06-19-pull-wave-dpc.md` — this document.
6. `production/session-state/active.md` — STATUS + Progress + Session Extract sync (next step after this doc is written).

---

## Files NOT Modified by This Pass (intentional)

- `docs/architecture/adr-0001-palm-rejection-rmax-calibration.md` — does not reference Pull-Wave/DPC; verified clean via grep.
- `docs/architecture/adr-0002-haptic-platform-bridge.md` — does not reference Pull-Wave/DPC.
- `docs/architecture/adr-0003-drain-queue-architecture.md` — does not reference Pull-Wave/DPC.
- `docs/architecture/adr-0004-r11a-dual-grep-methodology.md` — extended at R13 with §Oracle-Site Sweep Corollary; held through R14; no further revision needed at this pass.
- `docs/architecture/platform-seam-interfaces.md` — no seam changes from this propagation (Seam 13 OnDespawnedUserCallback closure at R10c held through R14).
- `design/gdd/reviews/difficulty-phase-controller-review-log.md` — DPC closures documented in Pull-Wave log entries as canonical record per user opt-out at R12; pattern held through R13 + R14.
- `design/gdd/systems-index.md` — Pull-Wave/DPC rows update deferred per R12 user opt-out; user can re-opt-in at any time.
- `design/gdd/player-movement.md` / `design/gdd/input-system.md` / `design/gdd/run-state-machine.md` — sibling GDDs; out of Pull-Wave + DPC propagation scope.
- `design/gdd/game-concept.md` — Core Fantasy "seven-tenths of a second" already updated at R11a-arithmetic; no further changes from this pass.
- `design/gdd/wave-spawner-pattern-library.md` — does not yet exist; forward contracts queued for when authored (see §Forward Contracts).

---

## Next Steps

1. **Pull-Wave + DPC review cycle CLOSED.** Both GDDs at APPROVED state. Decomposition trigger RETIRED. No further design-review passes required pending implementation surface.
2. **Wave Spawner & Pattern Library GDD authoring** (#7 in `design/gdd/systems-index.md`) — inherits the 8 forward contracts enumerated above. Recommended next operative GDD-authoring step.
3. **HISM vs ISMC ADR** (`docs/architecture/adr-NNNN-pullwave-instanced-renderer.md`) — Pull-Wave story-Done blocker per OQ-PW-3 promoted at R8 perf-analyst B-3. Assign to unreal-specialist. NOT a GDD revision item; downstream artifact authorship.
4. **PM decomposition** (per `design/gdd/reviews/player-movement-decomposition-plan-2026-06-16.md`) — independent parallel track; not blocked on Pull-Wave/DPC closure.
5. **Telegraph System prototype** — highest-risk bet per systems-index; independent parallel track.

---

## Validation Outcome

R7→R14 trajectory closed with calibrated forecast + retired decomposition trigger + methodology corollary evolved under control. Two consecutive calibrated forecasts (R13 1/1 + R14 0/0). Final trajectory R10(19)→R11(8)→R12(7)→R13(1)→R14(0) monotonically decreasing across five reviews.

This propagation pass is **terminal** for the Pull-Wave + DPC R10d cascade. The R10d FLOOR raise (Pull-Wave R10d 2026-06-17) + Path B TelegraphWindowCurve reauthoring (R11a-arithmetic 2026-06-18, CD R12 Ruling 1 ratified 2026-06-18) + ADR-0004 §Oracle-Site Sweep Corollary extension (R13 2026-06-19) + BARRAGE value flip + TELEGRAPH_WINDOW_FLOOR_MAX_ADMISSIBLE_S codification + DPC line 717 prose retirement (this pass) constitute the full cascade.

No re-opening triggers active under current invariant arithmetic. Re-opening conditions remain those documented in the R9 verdict (`REACTION_BUDGET safe-range upper bound 0.30 or higher`, `M=4 PEAK barrages`, `pattern library expansion`); none currently in scope.

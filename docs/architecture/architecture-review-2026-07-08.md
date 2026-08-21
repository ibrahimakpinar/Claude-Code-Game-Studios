# Architecture Review — 2026-07-08 (Delta)

**Mode**: `/architecture-review` (full — delta against 2026-07-03 baseline)
**Engine**: Unreal Engine 5.7 (pinned 2026-02-13)
**Baseline**: `architecture-review-2026-07-03.md` (CONCERNS; 125/207 covered = 60%)
**Delta window**: 5 days (2026-07-03 → 2026-07-08)
**Verified via mtime**: Zero ADR file changes, zero GDD file changes since baseline.
**Only new architectural input**: `prototypes/pull-wave-perf-spike-2026-07-03/SPIKE-NOTE.md` (2026-07-08 — Pull-Wave perf PARTIAL VERDICT).

---

## Verdict: **CONCERNS** (unchanged from 2026-07-03)

The Pull-Wave perf spike ran successfully on iPhone 17 physical hardware. Its outcome is *evidence* about ADR-0006's claims, not an architectural change. The verdict framework and coverage numbers are unmoved.

---

## Delta vs 2026-07-03

| Change | Impact |
|---|---|
| **Pull-Wave perf spike executed** (iPhone 17 flagship, Xcode thermal simulator, Development config with FrameRateLock=PUFRL_60 unlocking 60fps) | Evidence for ADR-0006; does NOT contradict any claim; does NOT close Sprint 1 gate |
| **SPIKE-NOTE outcome**: CPU sub-budget PASS decisively (0.01–0.06 ms vs 0.25 ms budget); 60fps AMBIGUOUS at Nominal (16.6↔18 ms variance across readings at same thermal state); Thermal NOT VALIDLY TESTED (Xcode simulator sends notifications but doesn't stress hardware — evidenced by GPU going DOWN Nominal→Fair→Serious: 11→9.6→7 ms, which is physically impossible if simulator were stressing hardware); Mid-tier follow-up STILL REQUIRED (iPhone 17 is flagship, not the mid-tier target from `technical-preferences.md`) | ADR-0006 CPU claim now has real-hardware validation; 60fps + thermal claims remain follow-up items; Pull-Wave itself contributes ~1 ms of the 7–11 ms observed GPU cost (scene shell dominates) |
| **INT-008 (NEW; documentation-quality; not blocking)**: SPIKE-NOTE recommends 3 ADR-0006 amendments (Last Verified bump with qualifier + Risks-table row for 60fps ceiling-with-variance + Migration Plan iOS-toolchain-precheck step) — not yet landed | Author task; same shape as INT-003 carry-over |
| **ADR-0009 promotion Proposed → Accepted**: 2026-07-03 baseline recommended this as next action; user elected perf spike path instead | Unblocked; recommendation carries forward unchanged; no new blockers surfaced |

---

## Confirmed unchanged from 2026-07-03

- **Coverage**: 125/207 (60% covered, 14% partial, 26% gap). No new/removed TRs.
- **Cross-ADR conflicts**: None.
- **GDD revision flags**: None. Spike VALIDATES ADR-0006 CPU claim; does not contradict any GDD assumption.
- **Dependency cycles**: None. Dependency order unchanged from baseline table (see 2026-07-03 §"ADR Dependency Order").
- **Engine compatibility audit**: Clean (unchanged; no new engine decisions since baseline).
- **ADR portfolio**: 9 ADRs (0001–0009), status unchanged. ADR-0004 methodology + ADR-0001/0002/0003/0009 Proposed; ADR-0005/0006/0007/0008 Accepted.
- **Coverage gaps**: ADR-0010 (Pull-Wave lifecycle), ADR-0011 (Wave Spawner pattern library), ADR-0012 (Telegraph — deferred until Telegraph GDD).
- **Pre-gate infrastructure**: ❌ `/test-setup` not run; ❌ `/ux-design` not run (all 4 checklist items unchanged).
- **INT-003 carry-over**: ADR-0003 `INPUT_TICK_RATE = 60Hz` naming nuance still open. Documentation-quality.

---

## SPIKE-NOTE — Interpretation for architectural review

**Not a new architectural decision.** The spike is an evidence artifact validating ADR-0006's claims to varying degrees. Structured impact per Verification concern from ADR-0006:

| ADR-0006 claim | Spike evidence | Verdict impact |
|---|---|---|
| AC-PW-22a Pull-Wave per-tick CPU budget ≤ 0.25 ms | 0.01–0.06 ms measured across every reading and every simulated thermal state; typical 0.01 ms (25× under budget) | **CPU claim validates decisively.** ADR-0006 CPU sub-budget promise holds. |
| Mobile-forward 60fps sustained on target hardware | Frame 16.6 ↔ 18 ms observed at Nominal on iPhone 17 (flagship — NOT mid-tier target) | **GPU claim partially validated.** 60fps achievable but not comfortably headroomed on flagship. Mid-tier follow-up required per SPIKE-NOTE Follow-up §2. |
| Thermal robustness | Xcode `Simulate → Thermal State` did not measurably stress workload (Serious GPU < Nominal GPU — reversal proves simulator invalidity) | **Thermal claim untested.** Physical soak required per SPIKE-NOTE Follow-up §1. |
| ISMC Verification concerns V3/V4/V6/V7 from ADR-0006 | V3-adjacent config validated as byproduct (Mobile Scene Render pass observed at 5.85 ms — proves Mobile Forward path activates correctly). V4/V6/V7 untested. | Partial; V3 partially covered; V4/V6/V7 still open per ADR-0006. |

**Sprint 1 gate for Pull-Wave wave-renderer story**: **STILL OPEN.** ADR-0006 Migration Plan explicitly states "no wave-renderer story may enter implementation until this ADR is Accepted" — the spike does not change ADR-0006's status. The three follow-up items (physical thermal + mid-tier spike + Nominal variance investigation) must complete before Sprint 1 gate can clear.

---

## Required ADR Amendments

### 🟡 INT-008 (NEW this pass) — ADR-0006 SPIKE-NOTE-driven amendments (pending)

**Author task; not architecturally blocking.** Same shape as INT-003 (documentation-quality carry-over).

1. **Bump ADR-0006 Last Verified stamp** to 2026-07-08 with qualifier: "Verified on iPhone 17 flagship (Xcode simulator only); CPU sub-budget claim validated decisively; 60fps + thermal claims validated *partially*; mid-tier + physical thermal follow-up REQUIRED."
2. **Add Risks-table row**: "iPhone 17 Nominal-thermal 60fps observed at ceiling with variance (16.6 ↔ 18 ms across readings); comfort headroom not confirmed. Mitigation: physical thermal soak + mid-tier follow-up before Sprint 1."
3. **Add Migration Plan step**: "Verify iOS toolchain installs cleanly in developer environment before packaging any story that depends on this ADR" (SPIKE-NOTE evidence: transient `OtherCompilationError` blocker consumed 2 days of the 5-day spike execution window).

### 🟡 INT-003 (carry-over from 2026-06-25/26) — ADR-0003 naming nuance

Unchanged. Documentation-quality only.

### Required NEW ADRs (carry-over from 2026-07-03; unchanged priorities)

- **ADR-0010** — Pull-Wave Object Pool, State Machine, Despawn Pipeline (Feature; est. ~15–20 TR closure).
- **ADR-0011** — Wave Spawner Pattern Library Data Model + Cadence Governor (Feature; est. ~15–20 TR closure).
- **ADR-0012** — Telegraph System (deferred until Telegraph GDD).

---

## Skipped Phases

- **Engine specialist consultation** — no new engine decisions since 2026-07-03. Same skip rationale as 2026-07-03. If the pending ADR-0006 amendments surface new engine questions (e.g. UE thermal-notification hooks), spawn `unreal-specialist` at that amendment time, not at this review time.
- **Phase 3b RTM** — `production/epics/` still does not exist; no stories yet.
- **Phase 4 dependency ordering** — unchanged from 2026-07-03 baseline table.
- **Phase 5b GDD Revision Flags** — none surfaced; spike outcome does not contradict any GDD.
- **Reflexion log** — `docs/consistency-failures.md` still does not exist; no CONFLICT entries to append (no conflicts detected).

---

## Pre-Gate Checklist (Phase 9 — unchanged from 2026-07-03)

- ❌ `tests/unit/` + `tests/integration/` MISSING → run `/test-setup`
- ❌ `.github/workflows/` MISSING → run `/test-setup`
- ❌ `design/ux/accessibility-requirements.md` MISSING → run `/ux-design`
- ❌ `design/ux/interaction-patterns.md` MISSING → run `/ux-design`

Cannot run `/gate-check pre-production` until all four resolve.

---

## Recommended Next Actions (updated priority)

1. **Land ADR-0006 amendments (INT-008)** — 3 changes per SPIKE-NOTE recommendations. Author-only edit; no new architecture-review required to apply.
2. **Promote ADR-0009 Proposed → Accepted** (carry-over from 2026-07-03; no new blockers). Feature-layer PM chain becomes Accepted.
3. **Physical thermal soak + mid-tier device follow-up spike** (per SPIKE-NOTE §Follow-up REQUIRED items 1 + 2) — real hardware perf validation before Sprint 1. Scaffold assets preserved at `~/Development/Games/PullWaveSpike/` so follow-up spikes only need deploy + measure.
4. **Author ADR-0010 (Pull-Wave lifecycle)** — highest-impact remaining Core-layer gap.
5. **Author ADR-0011 (Wave Spawner pattern library)** — Feature-layer closure.
6. **Run `/test-setup` and `/ux-design`** in parallel — Pre-Production gate infrastructure.
7. **Re-run `/architecture-review`** after each new ADR is written OR after ADR-0009 promotes to Accepted OR after ADR-0006 amendments land — verify coverage improves + validate cross-ADR integration surface.

---

## Sprint 1 Gate Summary

Pull-Wave wave-renderer story: **BLOCKED** (per ADR-0006 Migration Plan, unchanged).

Preconditions for gate clearance:
- [ ] Physical thermal soak on iPhone 17+ (15+ min warm-room / stress-warm silicon)
- [ ] Mid-tier device spike (iPhone 12/13/SE class or A15-tier Android)
- [ ] Nominal reading variance investigation (3–5 readings, p50/p95/p99)
- [ ] Critical GPU spike (28 ms momentary) root cause investigation
- [ ] ADR-0006 amendments landed (INT-008 items 1–3)

---

*This is a short delta review. The 2026-07-03 baseline remains authoritative for the full 207-requirement traceability analysis, full cross-ADR conflict sweep, engine compatibility audit, and dependency ordering. Re-derive from full baseline (not from this delta) when the next non-trivial architectural change lands.*

# Change Impact: Player Movement GDD — Pull-Wave-Triggered Revision

**Date**: 2026-06-06
**Trigger GDD**: `design/gdd/pull-wave-behavior.md` (authored 2026-06-06)
**Affected GDD**: `design/gdd/player-movement.md` (currently Approved — re-review 7 closed 2026-05-14)
**Skill invocation**: `/propagate-design-change design/gdd/player-movement.md`
**Review mode**: lean (TD-CHANGE-IMPACT director gate skipped)

---

## Change Summary

Pull-Wave Behavior GDD imposes 3 binding forward contracts that require Player Movement GDD revision. PM has NOT been revised yet — this document surfaces what would need to change BEFORE the revision is applied.

| Section | Current PM State | Required Post-Revision State | Source of Requirement |
|---|---|---|---|
| **Rule 1 — Lane model** | 3-lane track: `Left, Center, Right`. `EPlayerLane` enum has 3 members. `LANE_OFFSET_CM = 200cm` half-offset per lane. | 6-lane track: indices `[0, 5]`. `EPlayerLane` enum widens to 6 members or replaces with `int32` lane index. `LANE_OFFSET_CM` reorganizes into `LANE_WIDTH_M = 1.0m` (provisional) × signed offset from track center. | Pull-Wave Rule 2 — required to satisfy DPC R7 forward contract AC-PILLAR-2-BARRAGE-SPATIAL-K (≥4 distinct M=3 spatial configurations). At 6 lanes × MIN_BARRAGE_LANE_SEPARATION=1: C(6,3) = 20 configs ≥ 4 ✓. |
| **Tuning Knobs — `SLIP_TWEEN_DURATION_S`** | Safe range `[0.10, 0.25]s` (default 0.15s). | Safe range tightens to `[0.10, 0.15]s`. Default 0.15s preserved at new ceiling. | Pull-Wave F-BARRAGE-SURVIVABILITY-INVARIANT: `SLIP_TWEEN_DURATION_S × MIN_ESCAPE_SLIPS ≤ TELEGRAPH_WINDOW_FLOOR_S`, where `MIN_ESCAPE_SLIPS = MAX_PULLS_PER_BARRAGE = 3`. At `SLIP_TWEEN_DURATION_S > 0.15s`, player cannot escape worst-case M=3 barrage in 6-lane layout within `TELEGRAPH_WINDOW_FLOOR_S = 0.6s` budget. |
| **`OnSlipMidpoint` interface** | Signature `(source: int32, target: int32)`, fires at `TweenProgress = 0.5`. | UNCHANGED. Documented as binding (Pull-Wave Rule 11 subscribes to this delegate for near-miss source-lane recording). | Pull-Wave Rule 11. No change to PM; just locks the current contract. |

---

## ADR Reference Scan

5 architecture documents scanned for PM references (`docs/architecture/*.md`):

### ✅ Not Affected — 3 ADRs + 2 supporting docs

#### ADR-0001: Palm Rejection R_max Calibration
- **PM references**: None.
- **Assessment**: Pure Input System concern (palm rejection geometry threshold, R_max = 6.0mm). Lane count and tween duration do not affect palm rejection.
- **Recommended action**: Keep as-is.

#### ADR-0002: Haptic Platform Bridge
- **PM references**: PM fires `IHapticDispatch.Fire(EHapticEvent::SlipConfirmed)` and `IHapticDispatch.Fire(EHapticEvent::BufferDrop)` (mentioned in ADR's GDD Requirements Addressed table).
- **What the ADR assumed**: PM fires haptic events through the platform bridge. Specific lane count and tween duration are not part of the ADR's scope.
- **What the GDD now says (post-revision)**: PM still fires the same two haptic events. Event firing semantics unchanged by 6-lane track or tightened tween range.
- **Assessment**: Haptic event firing is orthogonal to lane geometry and tween timing. Still Valid.
- **Recommended action**: Keep as-is.

#### ADR-0003: Drain Queue Architecture
- **PM references**: PM mentioned in pipeline diagram as recipient of `slip-left`/`slip-right` event dispatch.
- **What the ADR assumed**: Input System dispatches discrete slip events to Player Movement; PM owns the response.
- **What the GDD now says (post-revision)**: PM still receives slip events; only the destination lane count widens (the dispatch pipeline doesn't care about lane count).
- **Assessment**: Dispatch architecture is independent of PM's internal lane geometry. Still Valid.
- **Recommended action**: Keep as-is.

#### platform-seam-interfaces.md (supporting doc, not an ADR)
- **PM references**: Seam 12 `IPlayerMovementProvider` (just authored 2026-06-06) abstracts PM state reads (`current_lane`, `target_lane`, `movement_state`) behind a test-injectable interface. The interface contract is lane-count-agnostic (`int32 GetCurrentLane()` returns any `int32`; not bound to a specific enum).
- **Assessment**: Seam author already accounted for PM revision. No update needed.
- **Recommended action**: Keep as-is.

#### visual-dispatch-contract.md (supporting doc, not an ADR)
- **PM references**: None.
- **Assessment**: Visual dispatch contract concerns Input System feedback overlays. No PM coupling.
- **Recommended action**: Keep as-is.

### ⚠️ Needs Review — 0 ADRs

None.

### 🔴 Likely Superseded — 0 ADRs

None.

---

## Director Gate

**TD-CHANGE-IMPACT skipped — Lean mode** (no `production/review-mode.txt`; default lean per coordination rules).

---

## Assessment

Impact at the ADR layer is **minimal**. All 3 ADRs survive PM revision unchanged. The propagation work is concentrated entirely within PM GDD itself + downstream test infrastructure (Seam 12 already accounts for it).

**Key risk**: PM is currently `Approved` (re-review 7 closed 2026-05-14, convergence 17→32→28→23→21→3→0 — among the project's most thoroughly-reviewed GDDs). Revising PM reopens its Approved status. The revision should be a **focused 2-section update** (Rule 1 + Tuning Knobs `SLIP_TWEEN_DURATION_S` row), not a full rewrite.

**Recommended PM revision scope**:
1. Rule 1 (lane model): widen 3-lane → 6-lane. Update `EPlayerLane` enum; replace `LANE_OFFSET_CM` with `LANE_WIDTH_M × signed offset`.
2. Tuning Knobs (`SLIP_TWEEN_DURATION_S` row): tighten safe range to `[0.10, 0.15]s`. Document the cross-system invariant linkage to Pull-Wave F-BARRAGE-SURVIVABILITY-INVARIANT.
3. (Optional) Add cross-system invariant note in Dependencies section pointing to Pull-Wave's F-BARRAGE-SURVIVABILITY-INVARIANT as the source of the tween-range tightening.

**Expected PM R8 review trajectory**: 0–3 blockers if revision is tightly scoped. PM's review history shows strong convergence on focused edits. Run `/design-review design/gdd/player-movement.md` in fresh session post-revision.

---

## Recommended Follow-Up Actions

1. **No ADR updates required** — all 3 ADRs remain Valid as-is.
2. **PM GDD revision** — focused 2-section update. Run `/design-system retrofit design/gdd/player-movement.md` OR perform the edit manually.
3. **PM R8 re-review** — `/design-review design/gdd/player-movement.md` in fresh session after revision lands.
4. **No traceability matrix update** — `docs/architecture/architecture-traceability.md` does not exist yet in this project.
5. **systems-index.md row 3 update** — already applied 2026-06-06 (marked PM as "Approved — REVISION QUEUED").

---

## Verdict: COMPLETE

Change impact report saved. 0 ADRs require modification. PM revision queued for a fresh session via `/design-system retrofit` or manual edit, followed by `/design-review` R8 pass.

**Cross-references**:
- `design/gdd/pull-wave-behavior.md` (trigger GDD; see Section C Interactions → PM forward contract)
- `design/gdd/systems-index.md` row 3 (PM revision queued status)
- `design/gdd/player-movement.md` (target of revision)
- `design/registry/entities.yaml` `LANE_WIDTH_M` entry (provisional; source ownership transfers PM → Pull-Wave temporarily until PM revision lands)

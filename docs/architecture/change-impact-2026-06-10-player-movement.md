# Change Impact: Player Movement GDD — Pull-Wave R7 Forward-Contract Propagation

**Date**: 2026-06-10
**Trigger GDD**: `design/gdd/pull-wave-behavior.md` (Pull-Wave R7 fresh-context re-review + in-session R7 cluster-based revision applied 2026-06-10; 13 BLOCKING + 1 contingent / 5 root-cause classes resolved)
**Affected GDD**: `design/gdd/player-movement.md` (Status before this pass: Approved — re-review 7 closed 2026-05-14)
**Skill invocation**: `/propagate-design-change design/gdd/player-movement.md`
**Review mode**: full (TD-CHANGE-IMPACT director gate skipped — gate spec not defined in `.claude/docs/director-gates.md` AND impact surface is trivial (1 ADR, Still Valid, zero changes))
**Supersedes**: `docs/architecture/change-impact-2026-06-06-player-movement.md` (prior pass was authored against Pull-Wave R1 draft which assumed 6-lane track, FLOOR=0.6s, locomotion-only invariant; all three of those values changed before R7. The 2026-06-06 doc remains in the repo for historical traceability but its requirements are superseded by this 2026-06-10 doc.)

---

## Change Summary

Pull-Wave Behavior R7 (post-revision, 2026-06-10) imposes 3 BINDING forward contracts on Player Movement plus 1 BINDING semantic invariant and 1 OPTIONAL interface relaxation. PM HAS been revised in this same propagation pass — this document records both what was required and what was applied.

| Section | Pre-R7-PM-PROPAGATION State | Post-R7-PM-PROPAGATION State (applied 2026-06-10) | Source of Requirement |
|---|---|---|---|
| **Rule 1 — Lane model** | 3-lane track: `Left, Center, Right`. `EPlayerLane` enum has 3 members. `LANE_OFFSET_CM = 200cm` half-offset per lane. | **5-lane track** (revised from R1-draft 6-lane per Pull-Wave R1 CD adjudication 2026-06-07): indices `[0, 4]`. `EPlayerLane` enum widens to 5 members `{FarLeft, Left, Center, Right, FarRight}` (ordinal 0..4; Center=2 preserves track-X-origin convention). | Pull-Wave R7 Rule 2 — `NUM_LANES = 5` locked. Satisfies DPC R7 forward contract AC-PILLAR-2-BARRAGE-SPATIAL-K with margin: at 5 lanes × `MIN_BARRAGE_LANE_SEPARATION = 1` (target-lane distinct) → C(5,3) = 10 admissible M=3 configurations ≥ 4 floor (R7 B8 retires 3 configs leaving 7 admissible × 4 classes — still ≥ floor). Mobile-thumb-target check: at `LANE_WIDTH_M = 1.0 m` on 6-inch screen ≈ 14 mm per lane, comfortably above 11 mm capacitive-touch reliable-target floor. |
| **Lane geometry knob** | `LANE_OFFSET_CM = 200.0` (per-edge offset; safe range 120-300 cm on 3-lane model). | `LANE_OFFSET_CM` **RETIRED**. New canonical knob `LANE_WIDTH_M = 1.0 m` (matches Pull-Wave R7 shared constant) with working cm alias `LANE_WIDTH_CM = LANE_WIDTH_M × 100 = 100`. Safe range 0.75–1.5 m. F-1 formula restructured: `lane_world_x_cm(lane_index) = (lane_index − 2) × LANE_WIDTH_CM`. Lane centers at −200, −100, 0, +100, +200 cm — same total track span 400 cm as prior 3-lane (preserves mobile-screen-fit) but with finer-grained intermediate lanes. | Pull-Wave R7 Dependencies section forward contract on PM: "LANE_OFFSET_CM reorganizes into LANE_WIDTH_M = 1.0m (provisional) × signed offset from track center." Semantic shift: prior knob measured per-edge offset (one half-distance value); new knob measures per-lane width (one inter-lane-distance value). |
| **`SLIP_TWEEN_DURATION_S` safe range** | Safe range `[0.10, 0.25]s` (default 0.15s). | Safe range **tightened to `[0.10, 0.15]s`** (default 0.15s preserved at new ceiling). | Pull-Wave R7 F-BARRAGE-SURVIVABILITY-INVARIANT: `SLIP_TWEEN_DURATION_S × MIN_ESCAPE_SLIPS + REACTION_BUDGET ≤ TELEGRAPH_WINDOW_FLOOR_S`, where `MIN_ESCAPE_SLIPS = MAX_PULLS_PER_BARRAGE = 3`, `REACTION_BUDGET = 0.20s` (mobile recognition floor — R2 Cluster F registry constant), `TELEGRAPH_WINDOW_FLOOR_S = 0.65s` (DPC; raised R2 Cluster E path b from 0.6s). At `SLIP_TWEEN = 0.15s`: `0.15 × 3 + 0.20 = 0.65 ≤ 0.65 ✓` holds at exact zero margin. At `SLIP_TWEEN = 0.16s`: `0.16 × 3 + 0.20 = 0.68 > 0.65 ✗` violates immediately. The upper-bound tightening is BINDING; prior safe range upper 0.25s would permit unescapable PEAK barrages. |
| **PM Rules 4 + 7 source-lane semantic during SLIPPING** | Rule 4 reassigns `current_lane = target_lane` at tween completion (no change). Rule 7 reassigns `current_lane = in-flight destination` at DEAD entry (no change). The invariant "current_lane returns SOURCE lane throughout SLIPPING" was implicit. | **No structural change** — invariant explicitly affirmed in Rule 4 as BINDING per Pull-Wave R7 Rule 11 forward contract. Cross-system regression flag added: any future PM revision that changes the reassignment timing (e.g., reassigning at tween midpoint) breaks Pull-Wave's near-miss detection and MUST be flagged via `/propagate-design-change` before implementation. | Pull-Wave R7 Rule 11 near-miss detection: reads `PM.current_lane` directly during SLIPPING expecting source-lane semantic. Pull-Wave R1 RC-A superseded the prior `OnSlipMidpoint`-based mechanism with the direct-read approach. The Pull-Wave Rule 11 contract is: "PM Rules 4 + 7 guarantee `current_lane` returns the SOURCE lane throughout the SLIPPING state — reassignment to `target_lane` at SLIPPING → SETTLED completion only." PM Rules 4 + 7 already guaranteed this; R7-PM-PROPAGATION affirms the invariant explicitly so future PM revisions can detect when they would break it. |
| **`OnSlipMidpoint` delegate** | Required (signature `(source: EPlayerLane, target: EPlayerLane)`, fires at TweenProgress=0.5; consumed by Pull-Wave Rule 11 for near-miss source-lane recording). | **OPTIONAL** — PM retains the delegate at its own discretion (no other documented consumer). Pull-Wave Rule 11 no longer consumes it (direct `current_lane` read supersedes per R1 RC-A). PM revision is free to remove the delegate without breaking downstream consumers, but it is preserved in the R7-PM-PROPAGATION revision for backwards compatibility. | Pull-Wave R7 forward contract on PM: "`OnSlipMidpoint` delegate optional (no longer consumed by Pull-Wave)." |

---

## Impact Analysis

ADRs scanned: 3 (`adr-0001-palm-rejection-rmax-calibration.md`, `adr-0002-haptic-platform-bridge.md`, `adr-0003-drain-queue-architecture.md`).

ADRs referencing `design/gdd/player-movement.md`: **1** (ADR-0002).

### ADR-0001: Palm-Rejection R_max Calibration
**Status**: ✅ **Not affected** (does not reference PM).

### ADR-0002: Haptic Platform Bridge
**Status**: ✅ **Still Valid**

**What the ADR assumed about PM** (lines 306-307, 354):
- "Slip-confirmed haptic (PM-owned) → `IHapticDispatch.Fire(EHapticEvent::SlipConfirmed)`"
- "Buffer-drop haptic (PM-owned) → `IHapticDispatch.Fire(EHapticEvent::BufferDrop)`"
- "`design/gdd/player-movement.md` — slip-confirmed and buffer-drop haptic ownership"

**What PM now says (post R7-PM-PROPAGATION)**:
- PM still owns the slip-confirmed haptic event (fired on SETTLED→SLIPPING transition — Commitment-Tell section unchanged by R7-PM-PROPAGATION).
- PM still owns the buffer-drop haptic event (fired when buffer is dropped — Buffer-Drop section unchanged by R7-PM-PROPAGATION).
- The `IHapticDispatch` interface, event vocabulary (`EHapticEvent::SlipConfirmed`, `EHapticEvent::BufferDrop`), and ownership boundary are all untouched.

**Assessment**: The R7-PM-PROPAGATION revisions touch lane geometry (3→5 lanes), the lane-width knob (LANE_OFFSET_CM → LANE_WIDTH_M), and the slip-tween safe range upper bound. None of these affect:
- Haptic event names or vocabulary
- Haptic ownership boundary (IS vs PM)
- `IHapticDispatch` interface
- When haptics fire (SETTLED→SLIPPING transition timing unchanged; buffer-drop trigger unchanged)
- Detection logic for haptic capability tiers
- Platform bridge implementation

ADR-0002's decision is orthogonal to lane geometry and slip timing.

**Recommended action**: Keep as-is. No revisions to ADR-0002 needed.

### ADR-0003: Drain Queue Architecture
**Status**: ✅ **Not affected** (does not reference PM).

---

## Resolution Decisions (Phase 7)

ADRs marked "Needs Review": **0**.
ADRs marked "Likely Superseded": **0**.
Resolution decisions required: **0**.

---

## Traceability Index Update (Phase 8)

`docs/architecture/architecture-traceability.md` **does not exist** in this project. Phase 8 traceability-index update is therefore a no-op for this propagation pass. If a traceability index is added in a future architecture-review pass, the R7-PM-PROPAGATION should be back-filled with:

```
| Date | GDD | Requirement | Changed To | ADRs Affected | Resolution |
|------|-----|-------------|------------|---------------|------------|
| 2026-06-10 | player-movement.md | Rule 1 — 3-lane track {Left, Center, Right} | 5-lane track {FarLeft, Left, Center, Right, FarRight} | (none — ADR-0002 still valid) | Updated (PM revised in place) |
| 2026-06-10 | player-movement.md | `LANE_OFFSET_CM = 200` per-edge offset | `LANE_WIDTH_M = 1.0 m` per-lane width (canonical) | (none — ADR-0002 still valid) | Updated (PM revised in place) |
| 2026-06-10 | player-movement.md | `SLIP_TWEEN_DURATION_S` safe range [0.10, 0.25]s | safe range [0.10, 0.15]s | (none — ADR-0002 still valid) | Updated (PM revised in place) |
```

---

## Follow-Up Actions (Phase 10)

- **No ADRs to write or update** — ADR-0002 is unaffected by the PM revisions.
- **PM GDD itself**: R7-PM-PROPAGATION revisions have been applied in-place. PM Status updated `Approved` → `In Revision`. PM should be re-reviewed (via `/design-review design/gdd/player-movement.md`) before being marked Approved again — the revisions are extensive (overview paragraph + Rule 1 + Rule 10 + Player-Perceivable State table + Cross-Component Interfaces + F-1 formula + multiple examples + EC-9 / EC-11 / EC-12 / EC-13 + Tuning Knobs (knob retirement + safe-range tighten) + AC-01 / AC-02 / AC-03 / AC-06 / AC-22 / AC-23 / AC-33).
- **`/consistency-check` cross-GDD pass**: recommended after PM re-review approves. Pull-Wave R7 binding decisions on PM are now mirrored in PM body; consistency-check verifies no other GDD (e.g., RSM, DPC) carries stale assumptions about PM's 3-lane model or LANE_OFFSET_CM knob.
- **No `/architecture-decision` invocations required** at this time — no ADR was marked Superseded.

---

## Files Modified by This Propagation Pass

- `design/gdd/player-movement.md` (R7-PM-PROPAGATION revisions applied: Overview + Rule 1 + Rule 4 + Rule 10 + Player-Perceivable State table + Cross-Component Interfaces (current_lane semantic + OnSlipMidpoint delegate note) + F-1 formula + Rule 1 example + F-4 examples + EC-9 + EC-11 + EC-12 + EC-13 + Tuning Knobs (LANE_OFFSET_CM → LANE_WIDTH_M; SLIP_TWEEN_DURATION_S safe range tighten) + AC-01 / AC-02 / AC-03 / AC-06 / AC-22 / AC-23 / AC-33. Header status block + R7-PM-PROPAGATION binding-decisions block added.)
- `docs/architecture/change-impact-2026-06-10-player-movement.md` (this file).

## Files NOT Modified by This Propagation Pass

- `docs/architecture/adr-0001-palm-rejection-rmax-calibration.md` (does not reference PM).
- `docs/architecture/adr-0002-haptic-platform-bridge.md` (references PM but haptic dispatch contract is unaffected by R7-PM-PROPAGATION revisions — Still Valid).
- `docs/architecture/adr-0003-drain-queue-architecture.md` (does not reference PM).
- `docs/architecture/architecture-traceability.md` (does not exist; no-op for this pass).
- `design/gdd/pull-wave-behavior.md` (Pull-Wave R7 in-session revision was completed in the upstream session; PM revision is the downstream-propagation half).
- `design/gdd/systems-index.md` (PM Status change captured in the R7-PM-PROPAGATION header on PM itself; systems-index will be updated when PM re-review closes).
- `production/session-state/active.md` (will be updated post-propagation with PM In-Revision state).

---

## Verdict: **COMPLETE** — change impact report saved.

R7-PM-PROPAGATION revisions applied to PM in-place; ADR impact analysis classified zero changes needed (only ADR-0002 references PM and its haptic dispatch contract is orthogonal to the R7-PM-PROPAGATION revisions). PM is now ready for `/design-review` to validate the revisions are internally consistent and complete.

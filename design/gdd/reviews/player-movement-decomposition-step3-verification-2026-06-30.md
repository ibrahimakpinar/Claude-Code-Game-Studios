# PM Decomposition — Step 3 Forward-Contract Verification (2026-06-30)

**Trigger**: User typed `next` post-`/clear` after PM Decomposition Step 2 closed 2026-06-29; selected "Step 3 forward-contract verify (Recommended)" via AskUserQuestion.

**Scope**: Verify that the §6 Dependencies "Cross-sub-GDD forward contracts" subsections inscribed across PASS 4-8 in the three PM sub-GDDs (`player-movement-mechanics.md` line 1037; `player-movement-presentation.md` line 392; `player-movement-platform.md` line 453) actually cover the plan §4.1 / §4.2 / §4.3 routing matrix in `design/gdd/reviews/player-movement-decomposition-plan-2026-06-16.md`. Gap-fill or close-out per the verification finding.

**Per session-state directive**: this verification is a fresh-context restart point; any Step 3 authoring beyond the verification report itself is deferred to the next `/clear` session (same-session-bias risk for Step 3 mirror-authoring matches prior PASSes).

---

## 0. §4.4 placement-interpretation question — RESOLVED

**Question**: Plan §4.4 prescribes forward contracts as "one-paragraph blocks at the top of the receiving section in the consumer sub-GDD." The §6 disclaimer text in each PM sub-GDD says "Step 3 authoring will produce reciprocal paragraphs in `[other sub-GDD]` §6." These disagree on placement: §3/§4 receiving-section tops vs. §6 inventory.

**Discriminator** (per plan §4.4: "the pattern is the same as R7-PM-PROPAGATION cross-system contracts (which propagated PM → Pull-Wave / DPC / Wave Spawner)"): inspect Pull-Wave / DPC / Wave Spawner GDDs to see where their PM-PROPAGATION contract paragraphs live structurally.

**Finding** (verified via grep + read of `design/gdd/pull-wave-behavior.md` §Dependencies lines 553-583):

- Pull-Wave's forward contracts on PM are embedded as **bolded binding paragraphs within the §Dependencies upstream/downstream tables** (line 561: "**PM forward contract (BINDING for PM revision)**: (1) 5-lane track; (2) `SLIP_TWEEN_DURATION_S` safe range tightened to `[0.10, 0.15]s`; (3) PM Rules 4 + 7 guarantee `current_lane` returns SOURCE lane throughout SLIPPING; (4) `OnSlipMidpoint` delegate optional ...").
- Pull-Wave's forward contracts on Wave Spawner sit similarly inside the Wave Spawner row (line 562: "**Wave Spawner forward contracts (BINDING per this GDD)**: (a) PEAK pool avg `ForwardVelocityMs` ≥ 1.5 × OPENER pool avg; ...").
- Pull-Wave's bidirectional consistency notes (lines 577-583) reciprocate the contracts pointing back at each upstream.

**Resolution**: §6 placement is correct per R7-PM-PROPAGATION precedent. Plan §4.4 "top of the receiving section" is interpreted as "top of the consumer's §6 Dependencies subsection." The PM sub-GDDs' current "Cross-sub-GDD forward contracts" subsection under §6 is the structurally correct landing zone for Step 3 paragraphs.

**Implication**: Step 3 execution scope is bounded to §6 paragraph authoring — NOT a full rewrite that propagates contracts to §3/§4 receiving-section tops. Lower scope than the alternative interpretation.

---

## 1. Gap analysis — plan §4 routing matrix vs §6 inventory contents

### 1.1 Plan §4.1 Mechanics ↔ Presentation (5 contracts)

| Plan # | Plan contract (canonical) | §6 inventory coverage | Gap status |
|---|---|---|---|
| 1 | F-6 fade-out 2-frame timing — F-6 fade-out duration ≥ commitment-tell hold duration (2 frames) so no Override mid-fade-out coincides with new commitment-tell flash | NOT enumerated in §6 inventory of either mechanics or presentation. F-6 timing referenced in mechanics §4 + presentation §3, but the cross-binding invariant linking fade-out to commitment-tell hold is not authored as a forward contract. | **MISSING** |
| 2 | Commitment-tell counter increment — counter increments per SETTLED→SLIPPING; presentation reads only, MUST NOT modify | Mechanics §6 inventory item 1 covers the trigger; presentation §6 inventory item 1 covers the read-only semantic + counter-vs-flash decoupling. | **COVERED** |
| 3 | edge_absorb_active flag — presentation fires edge-absorb animation + cue when flag==true; Edge-Absorb Tell duration MUST match F-6 EDGE_ABSORB_DURATION_S | Mechanics §6 inventory item 2 + presentation §6 inventory item 2 cover the trigger, but the duration-match invariant (`EDGE_ABSORB_DURATION_S` binding) is not explicit. | **PARTIAL** |
| 4 | TweenProgress sample for SLIP_CURVE rendering — TweenProgress drives F-3 lateral_world_position which presentation reads for the bend animation; Phase 1 shelf [0.0, 0.20] MUST remain visible at 60fps minimum | NOT enumerated in §6 inventory of either sub-GDD. Phase 1 shelf visibility constraint is referenced in mechanics §4 F-2 but NOT linked as a presentation forward contract. | **MISSING** |
| 5 | EHapticEvent::SlipConfirmed / BufferDrop / NearMiss dispatch — Mechanics dispatches; presentation owns the haptic-vocabulary spec (sub-50ms low-amplitude profile) | Mechanics §6 inventory items 3 (BufferDrop) + 4 (NearMiss) cover two of the three haptic events. SlipConfirmed is mentioned within inventory item 1 (Commitment-tell trigger) but not enumerated as a haptic-vocabulary forward contract distinct from the visual flash trigger. Presentation §6 inventory mirrors the same partial split. Co-ownership with Input System §Haptic Vocabulary is referenced in presentation §6 Bidirectional Notes (line 388) but not formalized in the Cross-sub-GDD forward contracts subsection. | **PARTIAL** (1 of 3 haptic events not enumerated as a standalone contract; co-ownership split not formalized in the forward-contracts subsection) |

**§4.1 gap count**: 2 MISSING + 2 PARTIAL = 4 gaps to close. (1 of 5 fully covered.)

### 1.2 Plan §4.2 Mechanics ↔ Platform (6 contracts)

| Plan # | Plan contract (canonical) | §6 inventory coverage | Gap status |
|---|---|---|---|
| 1 | F-BARRAGE-SURVIVABILITY-INVARIANT — `SLIP_TWEEN × MIN_ESCAPE_SLIPS + REACTION_BUDGET ≤ TELEGRAPH_WINDOW_FLOOR_S` MUST hold at all supported framerates × MAX_SLIP_DT_S safe-range × SLIP_TWEEN safe-range corners | Mechanics §6 inventory item 5 ("Hardware Contract gating context") + platform §6 inventory item 5 reference the `is_hw_performance_degraded` broadcast but NOT the F-BARRAGE invariant input-flow (mechanics provides SLIP_TWEEN + MIN_ESCAPE_SLIPS + REACTION_BUDGET; platform §4 F-BARRAGE-SURVIVABILITY-INVARIANT-CORNERS owns the frame-quantized derivation; cross-binding propagation discipline missing). Platform §4 line 321 has the corners-sensitivity matrix authored but no cross-reference back to mechanics §7 knobs as input sources. | **MISSING** (the framing — input-flow contract linking mechanics-owned knobs to platform-owned derivation — is absent; the underlying invariant IS authored in platform §4, but its inter-sub-GDD contract surface is not formalized) |
| 2 | effective_dt definition site (B-F6-3 closure) | Mechanics §6 inventory item 1 + platform §6 inventory item 1 both explicitly cite B-F6-3 closure at PASS 7. | **COVERED** |
| 3 | F-2 persistent clamp — mechanics implements; platform owns Shipping-Safety policy | Mechanics §6 inventory items 2 + 3 + platform §6 inventory items 2 + 3 cover the clamp + policy + safe range. | **COVERED** |
| 4 | ERunSlipState ordinal pinning — `static_assert` against seam doc EMovementState | Mechanics §6 inventory item 4 + platform §6 inventory item 4 cover the lockstep. | **COVERED** |
| 5 | HandleSlipTransition runtime invocation — mechanics dispatch site; platform owns tick ordering | Mechanics §6 inventory item 6 + platform §6 inventory item 6 cover the tick-ordering linkage; mechanics dispatch site referenced in §3 Cross-Component Interfaces. | **COVERED** |
| 6 | MIN_ESCAPE_SLIPS = 2 constant — must match between mechanics F-BARRAGE math + platform Hardware Contract math; AC-SS-E (compile-time static_assert) ensures local C++ value matches design intent; OQ-7 future yaml→C++ pipeline closes registry→code drift | NOT enumerated in §6 inventory of either sub-GDD. AC-SS-E reframe referenced in platform §3 (per R11a-17 closure) but the cross-sub-GDD constant-lockstep contract is not authored as a §6 line item. | **MISSING** |

**§4.2 gap count**: 2 MISSING = 2 gaps to close. (4 of 6 fully covered.)

### 1.3 Plan §4.3 Presentation ↔ Platform (4 contracts)

| Plan # | Plan contract (canonical) | §6 inventory coverage | Gap status |
|---|---|---|---|
| 1 | `is_hw_performance_degraded` broadcast — banner trigger; R11a-9 locked copy | Presentation §6 inventory item 1 + platform §6 inventory item 1 cover the broadcast + banner. | **COVERED** |
| 2 | Safe-area placement (iOS safeAreaInsets / Android WindowInsetsCompat) | Presentation §6 inventory item 4 + platform §6 inventory item 4 cover safe-area + AC-HW-C apparatus. | **COVERED** |
| 3 | OS haptic-state gate — `IHapticDispatch::IsSystemHapticsEnabled()` (R12a B-CERT-2) | Presentation §6 inventory item 2 + platform §6 inventory item 2 cover the gate API + call-site obligation. | **COVERED** |
| 4 | Player-controllable flash/reduce-motion setting (R12a B-CERT-1) | Presentation §6 inventory item 3 + platform §6 inventory item 3 cover the settings-bridge API. | **COVERED** |

**§4.3 gap count**: 0. (All 4 covered — clean.)

### 1.4 Aggregate gap summary

- **§4.1**: 2 MISSING + 2 PARTIAL = 4 gaps
- **§4.2**: 2 MISSING = 2 gaps
- **§4.3**: 0 gaps
- **Total**: 4 MISSING + 2 PARTIAL = 6 gap items across mechanics §6 + presentation §6 + platform §6 inventories.

---

## 2. Step 3 execution scope estimate (next `/clear` session)

Under the resolved §4.4 placement interpretation (§6 Cross-sub-GDD forward contracts subsection per R7-PM-PROPAGATION precedent), Step 3 execution requires:

### 2.1 Paragraph-insert count

| Sub-GDD | Section | Inserts | Notes |
|---|---|---|---|
| mechanics | §6 Cross-sub-GDD forward contracts | +4 lines (Mechanics ↔ Presentation: +2 NEW for §4.1 #1 + #4; +2 ENHANCED for §4.1 #3 + #5; Mechanics ↔ Platform: +2 NEW for §4.2 #1 + #6) | Inventory currently has 5 + 6 = 11 lines; will grow to ~15 lines. Renumber items so the canonical plan-§4 row count is preserved (5 + 6 = 11 distinct contracts, not 15 — but each contract gets explicit binding + propagation language per plan §4.4 discipline). |
| presentation | §6 Cross-sub-GDD forward contracts | +4 lines (Presentation ↔ Mechanics: +2 NEW + 2 ENHANCED mirrors of mechanics edits) | Same renumber discipline; Platform-side block untouched. |
| platform | §6 Cross-sub-GDD forward contracts | +2 lines (Platform ↔ Mechanics: +2 NEW mirrors of mechanics §4.2 #1 + #6) | Presentation-side block untouched. |

**Total edit surface**: ~10 paragraph inserts/enhancements across 3 sub-GDDs.

### 2.2 Time estimate

- 45–60 min focused work in a single fresh-context session.
- Same-session-bias risk: **LOW** — gaps are routed by plan §4 tables 1:1; full-verbatim contract framing is mechanically prescribed; no creative authoring surface.
- Advisor consultation: NOT required pre-authoring (mechanical scope) but RECOMMENDED post-authoring to verify renumbering preserves plan-§4 row count + propagation discipline matches plan §4.4 framing.

### 2.3 Discipline reminders for Step 3 execution session

Per plan §4.4:
- Each forward contract appears as a one-paragraph block at the top of the receiving section in the consumer sub-GDD (here: the §6 Cross-sub-GDD forward contracts subsection).
- A parallel reference is required in the source sub-GDD.
- The pattern matches R7-PM-PROPAGATION cross-system contracts — bolded **"X forward contract (BINDING for ...)"** preamble + numbered items (1), (2), (3) per contract row.
- Lock-step canary discipline applies: change one site, update the other (PM's R11a 4-site lock-step pattern generalizes to N-site lock-step across sub-GDDs).

---

## 3. Verification verdict

**Step 3 forward-contract authoring is NOT de-facto complete.** The §6 inventories cover 9 of 15 plan §4 contracts cleanly (60% by row count); the remaining 6 gaps (4 in §4.1, 2 in §4.2, 0 in §4.3) require explicit forward-contract paragraphs in §6 with the plan §4.4 binding + propagation discipline.

**Placement interpretation resolved**: §6 Cross-sub-GDD forward contracts subsection is the correct structural landing zone per R7-PM-PROPAGATION precedent. No §3/§4 receiving-section rewrite required.

**Recommended Step 3 execution**: fresh `/clear` session → 45–60 min focused authoring across mechanics §6 + presentation §6 + platform §6 to close the 6 gap items. Lock-step pairing discipline per plan §4.4.

**Files NOT modified this verification session** (intentional): all 3 sub-GDDs (verification deliverable is read-only); decomposition plan (read-only reference); registry; ADRs.

**Operative next step after this verification**: `/clear` → Step 3 execution. Steps 4-9 remain queued: Step 4 R12a author briefs for presentation + platform; Step 5 systems-index PM row 3 → 3a/3b/3c split; Step 6 monolith disposition; Step 7 per-sub-GDD review log files; Step 8 cross-ref grep against `design/` + `docs/architecture/` for stale `player-movement.md` references; Step 9 session-state final.

**Parallel-eligible workstreams remain unchanged**: `/ux-design` for the last unticked pre-gate UX item; first `/sprint-plan` for Pre-Production gate. PM decomposition closure is the highest-value sequential unblocker (gates ADR-0009 PM hosting).

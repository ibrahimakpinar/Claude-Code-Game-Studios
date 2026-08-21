# Player Movement GDD — Decomposition Plan

**Date**: 2026-06-16
**Trigger**: R11 fresh-context re-review (2026-06-16) returned 11 BLOCKING — exceeds CD-set decomposition threshold (`>8`) from R10 ruling.
**Authoring authority**: CD synthesis on R11 verdict (`design/gdd/reviews/player-movement-review-log.md` 2026-06-16 entry).
**Execution authority**: Binding for the next author session that executes the structural split. Author overrides any decision with documented rationale in the executed sub-GDD's header.
**Execution gate**: Author runs decomposition in a `/clear` fresh-context session after this plan is durable on disk.

---

## 1. Why decomposition (CD synthesis)

R11 returned 11 BLOCKING consolidated from 17 naive findings across 7 specialists. The 11 items span **four disjoint specialist domains**:

| Domain | BLOCKING count | Items |
|---|---|---|
| F-6 implementation depth | 4 | B-F6-1, B-F6-2, B-F6-3, B-F6-4 |
| UX / cert / accessibility | 3 | B-BANNER-1, B-CERT-1, B-CERT-2 |
| Audio precision | 2 | B-AUDIO-1, B-AUDIO-2 |
| Platform / QA / Shipping | 3 | B-QA-1, B-PERF-1, B-SHIP-1 |

Four disjoint domain clusters in one GDD is the textbook signal the doc is at the wrong scope. Each cluster has its own specialist tier of ownership, its own verification methodology, and its own forecast model. Forcing them through a single coherent revision pass produces the observed pattern: every R*a→R* round closes one cluster's prior surface and exposes another cluster's deeper surface.

**Convergence trajectory**: 17→32→28→23→21→3→0→4→26→22+1→19 (R10)→11 (R11). The trend is downward but not converging — it bounces with cluster surface.

**Forecast model declared BROKEN at R11 (Pillar 5 commitment-honesty)**: R10a forecast 0-4 / actual 19. R11 forecast 3-8 / actual ~11-17 (consolidated 11). Two consecutive 2-4× misses. No third forecast for the monolithic doc.

**Decomposition resolves this** by binding each sub-GDD's scope to one specialist tier of ownership + one cluster of failure surface. Each sub-GDD can have a tighter, calibrated forecast model (1-3 BLOCKING is the expected band for a single-domain GDD).

---

## 2. Decomposition principle

The split follows **R11's empirical failure-domain seams**, not a top-down taxonomy. The clusters that R11 exposed are the natural cut lines. Three sub-GDDs:

| Sub-GDD | Scope | Owners | R11 BLOCKINGs |
|---|---|---|---|
| `player-movement-mechanics.md` | F-1 through F-6 formulas, state machine, EC-15 boundary, F-BARRAGE-SURVIVABILITY-INVARIANT, mechanics ACs | game-designer (primary) + systems-designer | 4 (B-F6-1/2/3/4) |
| `player-movement-presentation.md` | Banner copy, commitment-tell flash, audio cues + ducking + triple-overlap, haptics, near-miss feedback, Player-Perceivable State, Audio-Visual Ownership Split | ux-designer (primary) + audio-director + narrative-director | 5 (B-BANNER-1, B-CERT-1, B-CERT-2, B-AUDIO-1, B-AUDIO-2) |
| `player-movement-platform.md` | Hardware Contract + watchdog, Shipping-Safety Enforcement Policy, AC-HW-A/B/C, AC-SS-A through E, frame-pacing, OS Focus/DND, profiler methodology, min-spec device list | performance-analyst (primary) + unreal-specialist + qa-lead | 3 (B-QA-1, B-PERF-1, B-SHIP-1) |

Each sub-GDD has its own 8 required sections (per `.claude/rules/design-docs.md`) and its own AC block. Sub-GDDs cite each other via **explicit forward contracts** (see §4) rather than via shared monolithic context.

---

## 3. Sub-GDD scopes (content boundaries)

### 3.1 `player-movement-mechanics.md`

**Scope**: PM's mechanical contract — formulas, state machine, edge cases, tuning knobs governing core motion. This is the "math + state" sub-GDD.

**Sections to move from current `player-movement.md`**:

| Section in monolith | Lines | Move to mechanics? |
|---|---|---|
| Overview | 41-43 | Split — mechanics gets a mechanics-scoped overview |
| Player Fantasy | 45-56 | **Mechanics owns** (the "bend" fantasy is mechanical) |
| Detailed Rules (Core Rules 1-11) | 126-150 | **All to mechanics** |
| States and Transitions | 152-163 | **Mechanics** |
| Public Interface | 165-187 | Split — properties owned by mechanics (current_lane, target_lane, lateral_world_position, lean_angle, movement_state, counters, bSlipTweenClampActive); broadcast delegates owned by platform (is_hw_performance_degraded, OnHardwarePerformanceBreach) |
| Cross-Component Interfaces — RSM Storage Contract | 193-212 | **Mechanics** |
| Cross-Component Interfaces — Tick Ordering | 214-219 | **Platform** (engine-level concern) |
| Cross-Component Interfaces — Rotation Implementation | 221-227 | **Mechanics** |
| Cross-Component Interfaces — Delegate Binding Contract | 229-245 | **Mechanics** |
| Cross-Component Interfaces — Delegate Handler Bodies (HandleStateChanged + HandlePausedChanged) | 247-412 | **Mechanics** (F-6 state reset table + tick handler dispatch) |
| Cross-Component Interfaces — Slip Input Dispatch (HandleSlipTransition) | 414-522 | **Mechanics** |
| Cross-Component Interfaces — Death Replay Registration Order | 524-526 | **Mechanics** |
| Cross-Component Interfaces — Cross-System Interface Table | 528-540 | Split — mechanics owns the upstream/downstream contracts; platform owns the Wave Spawner row (hardware-gate forward contract) |
| Cross-Component Interfaces — Hardware Contract | 542-681 | **Platform** |
| Cross-Component Interfaces — Shipping-Safety Enforcement Policy | 683-723 | **Platform** |
| Cross-Component Interfaces — Movement State Enum | 725-738 | **Mechanics** |
| Cross-Component Interfaces — Forward Motion Dual-Writer | 740-748 | **Mechanics** |
| Formulas (F-1 through F-6) | 750-1087 | **All to mechanics** |
| Authored Asset Contracts (SLIP_CURVE_ASSET, LEAN_CURVE_ASSET, EDGE_ABSORB_CURVE_ASSET) | 1089-1193 | **Mechanics** |
| Edge Cases (EC-1 through EC-16) | 1196-1248 | Split — EC-16 (triple-overlap audio) to **presentation**; rest to **mechanics** |
| Dependencies | 1251-1278 | Split — input/RSM upstream + pull-wave/collision/camera/death-replay downstream to **mechanics**; wave-spawner + HUD downstream to **platform/presentation** |
| Tuning Knobs (knobs governing mechanics: LANE_WIDTH_M, SLIP_TWEEN_DURATION_S, SLIP_CURVE_ASSET, MAX_SLIP_DT_S, LEAN_CURVE_ASSET, MAX_LEAN_ANGLE_DEG, HEAD_LAG_PROGRESS, ARM_LEAD_PROGRESS, EDGE_ABSORB_DURATION_S, EDGE_ABSORB_CURVE_ASSET, EC15_F6_DECAY_COEFFICIENT) | 1280-1294, except audio_cue_ratio row | **Mechanics** |
| Tuning Knobs (audio_cue_ratio row) | 1294 | **Presentation** |
| Player-Perceivable State (Lane-Settle Tell + Edge-Absorb Tell — mechanics-driven) | 71-86 | **Mechanics** (these tells are immediate consequences of mechanics state transitions) |
| Visual/Audio Requirements (Slip Tween Animation, Edge-Absorb Animation, DEAD Freeze, COMPLETE/ABORTED Snap) | 1323-1383 | Split — Slip Tween Animation (mechanics + presentation share) — **mechanics** authors the timing/phases, **presentation** authors the visual+audio + commitment-tell visual |
| VFX | 1420-1426 | **Presentation** |
| UI Requirements | 1428-1435 | **Mechanics** (lane indicator absence is a mechanics decision per Pillar 1) |
| Acceptance Criteria (AC-01 through AC-34, AC-COUNTER-DEAD/COMPLETE/PAUSE-RESUME, AC-COUNTER-F6-RESET, AC-F6-A through E, AC-21, AC-SS-A) | per section | Split per cluster ownership — see §5 BLOCKING assignment matrix |
| Open Questions | 1802-1823 | Split by topic ownership — OQ-1, OQ-2, OQ-3, OQ-4 stay with mechanics; OQ-7 to platform |

**Estimated mechanics sub-GDD final size**: ~800-1000 lines (down from monolith's 1823 spread across all three).

**Mechanics-specific BLOCKING items** (R11):
- B-F6-1 (F-6 fade-out tick-1 multiplier 1.0 reintroduces additive-opposition)
- B-F6-2 (EC15 coefficient derivation anchored on wrong worst case; folds MAX_SLIP_DT_S floor invariant violation)
- B-F6-3 (F-6 references effective_dt undefined in SETTLED path)
- B-F6-4 (EDGE_ABSORB_CURVE_ASSET soft-ref + raw deref + missing null guard)

### 3.2 `player-movement-presentation.md`

**Scope**: PM's player-facing perception surface — copy, visual flashes, audio cues, haptics, banner. This is the "what the player sees / hears / feels" sub-GDD.

**Sections to move from current `player-movement.md`**:

| Section in monolith | Lines | Move to presentation? |
|---|---|---|
| Player-Perceivable State — Commitment-Tell | 62-69 | **Presentation** |
| Player-Perceivable State — Near-Miss Beat | 87-95 | **Presentation** |
| Player-Perceivable State — Hardware-Performance Banner | 97-112 | **Presentation** |
| Player-Perceivable State — Audio-Visual Ownership Split | 113-124 | **Presentation** |
| Visual/Audio Requirements — Commitment-Tell Visual | 1341-1343 | **Presentation** |
| Visual/Audio Requirements — Edge-Absorb Animation | 1347-1357 | **Presentation** (animation timing + tell intensity) |
| Visual/Audio Requirements — Near-Miss Beat | 1361-1367 | **Presentation** |
| Visual/Audio Requirements — Audio (Locked Decision + cue table + ducking + triple-overlap) | 1387-1416 | **Presentation** |
| Visual/Audio Requirements — VFX | 1420-1426 | **Presentation** |
| Tuning Knobs — audio_cue_ratio | 1294 (one row) | **Presentation** |
| Acceptance Criteria — AC-29 (Commitment-Tell), AC-COMMIT-FLASH-CADENCE, AC-NEARMISS-HAPTIC, AC-AUDIO-CUE-PROPORTIONALITY, AC-AUDIO-CUE-DUCKING, AC-AUDIO-PAN-NEUTRALITY (retired marker) | per section | **Presentation** |
| EC-16 (triple-overlap audio) | 1248 | **Presentation** |
| R11a-9 banner copy + R11a-10 safe-area + R11a-11 cadence cap + R11a-12 opt-in haptic + R11a-13/14/15/16 audio decisions | Header decisions at lines 28-36 | **Presentation** |

**Estimated presentation sub-GDD final size**: ~500-700 lines.

**Presentation-specific BLOCKING items** (R11):
- B-BANNER-1 (banner copy contingent on unauthored HUD GDD + "suppressed" dev jargon)
- B-CERT-1 (no player-controllable flash/reduce-motion setting — App Store cert risk)
- B-CERT-2 (haptic dispatch ignores iOS Focus / Android DND — App Store cert risk)
- B-AUDIO-1 (AC-AUDIO-CUE-DUCKING trigger-order asymmetry — Pillar 5 violation)
- B-AUDIO-2 (5ms HARD-CUT ramp shape unspecified)

### 3.3 `player-movement-platform.md`

**Scope**: PM's hardware contract, Shipping-safety enforcement, watchdog, frame-pacing, OS state integration, test infrastructure for non-mechanics ACs. This is the "device-tier + engine-tier" sub-GDD.

**Sections to move from current `player-movement.md`**:

| Section in monolith | Lines | Move to platform? |
|---|---|---|
| Cross-Component Interfaces — Hardware Contract (full subsection) | 542-681 | **Platform** |
| Cross-Component Interfaces — Shipping-Safety Enforcement Policy (full subsection) | 683-723 | **Platform** |
| Cross-Component Interfaces — Tick Ordering | 214-219 | **Platform** |
| Cross-Component Interfaces — Cross-System Interface Table — Wave Spawner row | 539 | **Platform** (hardware-gate forward contract) |
| Cross-Component Interfaces — Cross-System Interface Table — HUD row | 540 | Split — banner copy/safe-area forward contracts to **presentation**; broadcast delegate to **platform** |
| Public Interface — is_hw_performance_degraded, OnHardwarePerformanceBreach | 185-186 | **Platform** |
| Tuning Knobs — N/A (no platform-specific tuning knobs in monolith currently; watchdog parameters are PM-internal implementation per R10a) | — | **Platform** if any added |
| Acceptance Criteria — AC-HW-A, AC-HW-B, AC-HW-C, AC-SS-A through E, AC-21 (fold-in semantics) | per section | **Platform** |
| Header decisions R11a-6/7/8 (watchdog), R11a-17 (compile-time constant) | Lines 26-28, 37 | **Platform** |
| Header decisions R10a-9/10 (Shipping-Safety policy, Hardware Contract) | Lines 16-17 | **Platform** |
| Open Question OQ-7 (yaml→C++ generator pipeline) | 1822-1823 | **Platform** |

**Estimated platform sub-GDD final size**: ~400-600 lines.

**Platform-specific BLOCKING items** (R11):
- B-QA-1 (AC-21/SS-A tick-601 unverifiable in 10/100 tick windows + test strategy preamble missing R11a private members)
- B-PERF-1 (AC-HW-B 0.5ms measurement tolerance below profiler noise floor)
- B-SHIP-1 (AC-SS-C Shipping guard body unspecified)

---

## 4. Cross-sub-GDD forward contracts

The decomposition is not free — sub-GDDs reference each other. Forward contracts must be authored at each seam as **one-paragraph blocks at the top of the relevant section** in each sub-GDD. Each contract states: the source sub-GDD, the consumed property/behavior, the binding invariant, and the propagation discipline (how a change in one propagates to the others).

### 4.1 Mechanics ↔ Presentation

| Contract | Source | Consumer | Binding | Propagation |
|---|---|---|---|---|
| F-6 fade-out 2-frame timing | mechanics F-6 formula | presentation Commitment-Tell Visual + Near-Miss Beat | F-6 fade-out duration MUST be ≥ commitment-tell hold duration (2 frames) so no Override mid-fade-out coincides with new commitment-tell flash | If mechanics retunes fade-out frames, presentation re-validates AC-COMMIT-FLASH-CADENCE setup |
| Commitment-tell counter increment | mechanics HandleSlipTransition.FireCommitmentTell() | presentation Commitment-Tell Visual | `commitment_tell_fire_count` increments per SETTLED→SLIPPING transition; presentation may render or suppress (cadence cap) but MUST NOT modify the counter | Counter is mechanics-owned; presentation reads only |
| edge_absorb_active flag | mechanics F-6 state | presentation Edge-Absorb Tell visual + audio | When `edge_absorb_active == true`, presentation fires the edge-absorb animation + cue. Edge-Absorb Tell duration MUST match F-6 EDGE_ABSORB_DURATION_S | If mechanics changes EDGE_ABSORB_DURATION_S, presentation re-validates timing |
| TweenProgress sample for SLIP_CURVE rendering | mechanics F-2/F-3 | presentation Slip Tween Animation (Phase 1 shelf visibility) | TweenProgress drives F-3 lateral_world_position which presentation reads for the bend animation. Phase 1 shelf [0.0, 0.20] MUST remain visible at 60fps minimum | Mechanics-owned timing; presentation reads only |
| EHapticEvent::SlipConfirmed / BufferDrop / NearMiss dispatch | mechanics (HandleSlipTransition.FireCommitmentTell + FireBufferDropHapticAndAudio + TriggerNearMissBeat) | presentation Audio-Visual Ownership Split | Mechanics dispatches the haptic event; presentation owns the haptic-vocabulary spec (sub-50ms low-amplitude profile) | Cross-contract on Input System §Haptic Vocabulary for the enum membership |

### 4.2 Mechanics ↔ Platform

| Contract | Source | Consumer | Binding | Propagation |
|---|---|---|---|---|
| F-BARRAGE-SURVIVABILITY-INVARIANT | mechanics SLIP_TWEEN_DURATION_S + REACTION_BUDGET | platform Hardware Contract frame-quantized table + Wave Spawner gate | `SLIP_TWEEN × MIN_ESCAPE_SLIPS + REACTION_BUDGET ≤ TELEGRAPH_WINDOW_FLOOR_S` MUST hold at all supported framerates × MAX_SLIP_DT_S safe-range × SLIP_TWEEN safe-range corners | If mechanics changes SLIP_TWEEN safe range or MAX_SLIP_DT_S safe range, platform re-derives frame-quantized table |
| effective_dt definition site | mechanics F-2 prologue | platform TickComponent prologue (watchdog precondition) | effective_dt MUST be defined in TickComponent prologue (before F-2 SLIPPING-gate AND before F-6) so both formulas have a consistent DT cap value | This was a R11 BLOCKING (B-F6-3); the decomposition migrates the definition site to platform's TickComponent prologue, fixing it |
| F-2 persistent clamp | mechanics F-2 prologue | platform Shipping-Safety Enforcement Policy | F-2 persistent clamp is the Shipping-safe guard for `SLIP_TWEEN_DURATION_S ∈ [0.10, 0.15]` invariant. Mechanics implements the clamp; platform owns the policy that requires it | If mechanics changes SLIP_TWEEN safe range, platform updates enforcement table |
| ERunSlipState ordinal pinning | mechanics Movement State Enum | platform Shipping-Safety Enforcement Policy (`static_assert` against seam doc EMovementState) | `ERunSlipState::SETTLED=0`, `ERunSlipState::SLIPPING=1` MUST match seam doc EMovementState ordinals; static_assert MUST guard at PM compile unit | If mechanics adds/removes a state, platform updates static_assert + seam contract |
| HandleSlipTransition runtime invocation | mechanics dispatch | platform Tick Ordering | HandleSlipTransition called from PM's input-event handler; platform owns the registration site that ensures RSM→PM tick ordering is honored | Mechanics-owned call site; platform-owned tick ordering |
| MIN_ESCAPE_SLIPS = 2 constant | (registry) → mechanics SLIP math + platform survivability table | both consumers | Constant value MUST match between mechanics F-BARRAGE math + platform Hardware Contract math. AC-SS-E (compile-time static_assert) ensures local C++ value matches design intent | OQ-7 future yaml→C++ pipeline closes registry→code drift gap |

### 4.3 Presentation ↔ Platform

| Contract | Source | Consumer | Binding | Propagation |
|---|---|---|---|---|
| is_hw_performance_degraded broadcast | platform Hardware Contract watchdog | presentation Hardware-Performance Banner | When watchdog fires `OnHardwarePerformanceBreach(true)`, presentation renders the banner. R11a-9 locked copy "Performance mode — hardest barrage suppressed." | If platform changes watchdog threshold, presentation re-validates banner trigger latency (AC-HW-C) |
| Safe-area placement (iOS safeAreaInsets / Android WindowInsetsCompat) | presentation (R11a-10) | platform AC-HW-C apparatus requirement | Banner anchored below OS safe-area inset. Platform's AC-HW-C apparatus MUST include a notched-iOS test device to verify the binding | Presentation-owned spec; platform-owned test apparatus |
| OS haptic-state gate (iOS Focus, Android DND) | platform `IHapticDispatch::IsSystemHapticsEnabled()` | presentation haptic dispatch sites | Presentation MUST query the platform gate before any `IHapticDispatch::Fire(...)` call. Closes R11 B-CERT-2 | Platform owns the API; presentation owns the gate-check call site |
| Player-controllable flash/reduce-motion setting | presentation accessibility settings spec | platform configuration-gate API (similar to `IGameSettings::IsNearMissHapticEnabled`) | Platform exposes the settings-bridge interface; presentation defines the user-facing setting + binds the gate to the LeadingFaceFlash dispatch | Closes R11 B-CERT-1 |

### 4.4 Forward-contract authoring discipline

Each forward contract appears as a **one-paragraph block** at the top of the receiving section in the consumer sub-GDD, with a parallel reference in the source sub-GDD. The pattern is the same as R7-PM-PROPAGATION cross-system contracts (which propagated PM → Pull-Wave / DPC / Wave Spawner) — but here it operates within a single GDD's own scope.

When a forward-contract block is updated, the lock-step canary discipline applies: change one site, update the other. R11a's 4-site lock-step pattern (F-2 prologue + AC-21 + AC-SS-A + §1.2 enforcement table) generalizes to N-site lock-step across sub-GDDs.

---

## 5. R11 BLOCKING assignment matrix

| ID | BLOCKING | Sub-GDD | R12a author decision required | Forward-contract impact |
|---|---|---|---|---|
| B-F6-1 | F-6 fade-out tick-1 multiplier=1.0 reintroduces additive-opposition | mechanics | New fade-out curve: tick-1 multiplier <1.0 satisfying anti-snap AND anti-additive | None — internal mechanics |
| B-F6-2 | EC15_F6_DECAY_COEFFICIENT worst case wrong; no coefficient in [0,1] satisfies invariant at TP=0.30. Folds MAX_SLIP_DT_S=0.020 × 30fps survivability violation | mechanics | (i) Re-derive EC15 formula at correct worst case TP=0.30-0.65; (ii) constrain MAX_SLIP_DT_S safe range against survivability invariant or document Hardware Contract dependency | Platform: F-BARRAGE-SURVIVABILITY-INVARIANT must re-validate against new EC15 coefficient + MAX_SLIP_DT_S range |
| B-F6-3 | F-6 effective_dt undefined in SETTLED path | mechanics (caller) + platform (definition site) | Move effective_dt definition to TickComponent prologue (platform), called by both F-2 and F-6 | Closed by decomposition itself; needs paragraph in both sub-GDDs |
| B-F6-4 | F-6 EDGE_ABSORB_CURVE_ASSET soft-ref + raw deref + no null guard | mechanics | (i) Upgrade to hard reference matching SLIP_CURVE/LEAN_CURVE pattern, OR (ii) add LoadSynchronous + null guard + linear fallback per asset contract | None — internal mechanics |
| B-BANNER-1 | Banner copy contingent on unauthored HUD GDD vocabulary + "suppressed" dev jargon | presentation | Decide vocabulary path: (a) revert to "waves" (already player-facing), (b) mark DR-D.1 OPEN pending HUD authoring, (c) other | HUD GDD forward contract retired or made explicit |
| B-CERT-1 | No player-controllable reduce-motion / flash-disable setting | presentation | Author `commitment_tell_flash_enabled` (or `reduce_motion`) accessibility setting + dispatch gate | Platform: configuration-gate API parallel to IGameSettings near-miss-haptic |
| B-CERT-2 | Haptic dispatch ignores OS Focus/DND state | presentation (call site) + platform (API) | Add `IHapticDispatch::IsSystemHapticsEnabled()` gate to all PM haptic dispatch sites | Platform owns the API; presentation owns the gate-check |
| B-AUDIO-1 | AC-AUDIO-CUE-DUCKING trigger-order asymmetry | presentation | Specify resolution for slip-dispatched-during-active-near-miss case (begin pre-ducked? begin at authored level + ramp? suppress?) | None — internal presentation |
| B-AUDIO-2 | 5ms HARD-CUT ramp shape unspecified | presentation | Append "raised-cosine (not linear) gain ramp" to triple-overlap table | None — internal presentation |
| B-QA-1 | AC-21/SS-A tick-601 unverifiable in 10/100 tick windows + test strategy preamble missing R11a private members | platform | (i) Extend test windows to ≥601 ticks OR pre-seed TickClampLogCounter state; (ii) refresh test strategy preamble with R11a-added members (4 F-6 fade-out members, watchdog state, shipping-safety flags) | None — internal platform |
| B-PERF-1 | AC-HW-B 0.5ms tolerance below profiler noise floor | platform | Either (a) rewrite apparatus to raw FApp::GetDeltaTime() log methodology, OR (b) widen tolerance to 1.5ms | None — internal platform |
| B-SHIP-1 | AC-SS-C Shipping guard body unspecified | platform | Author bManualTickEnabled early-out pseudo-code + 1Hz rate-limited log idiom in TickComponent prologue OR mark STUBBED pending OQ-1 ADR | None — internal platform |

---

## 6. Decomposition execution checklist

The author executes the split in a `/clear` fresh-context session. Estimated effort: **3-5 hours** focused work. No specialist subagents needed during execution (mechanical content move + forward-contract authoring).

**Step 1 — Create 3 sub-GDD skeletons (15 min)**:
- `design/gdd/player-movement-mechanics.md`
- `design/gdd/player-movement-presentation.md`
- `design/gdd/player-movement-platform.md`
- Each with 8 required section headers (per `.claude/rules/design-docs.md`) + empty bodies + status header block referencing this decomposition plan

**Step 2 — Move content (90-120 min)**:
- Work top-to-bottom through `player-movement.md` 1823 lines
- Each section header: per §3 of this plan, identify destination sub-GDD(s)
- Use Edit tool with replace_all=false to lift sections cleanly
- Preserve R10a + R11a header decision blocks in each sub-GDD (each sub-GDD's header block lists only the decisions affecting its scope)
- Preserve documented brief deviations (R11a-1, R11a-2, R11a-13) in their respective sub-GDDs

**Step 3 — Author cross-sub-GDD forward contracts (45-60 min)**:
- Per §4 of this plan, author one-paragraph contract blocks at each seam
- Each contract block: source / consumer / binding / propagation discipline
- Lock-step canary discipline: any future change to one contract site requires updating the other(s)

**Step 4 — Move BLOCKING items into per-sub-GDD R12a briefs (30-45 min)**:
- Create `design/gdd/reviews/player-movement-mechanics-r12-author-brief-2026-06-XX.md` (and similar for presentation + platform)
- Each brief lists only the BLOCKINGs assigned to its sub-GDD (per §5)
- Each brief gets a calibrated forecast band (1-3 BLOCKING per sub-GDD)

**Step 5 — Update systems-index.md (15 min)**:
- PM row #3 splits into 3 rows: 3a (mechanics), 3b (presentation), 3c (platform)
- Each row links to its sub-GDD + R12a brief
- Original PM row marked "DECOMPOSED 2026-06-XX → see rows 3a/3b/3c"
- Dependency columns updated for each sub-GDD (some inherit from monolith; some new)

**Step 6 — Disposition the monolith (5 min)**:
- Option A: rename `player-movement.md` to `player-movement-DECOMPOSED.md.archive` and add `.archive` to gitignore patterns
- Option B (recommended): leave `player-movement.md` in place, replace body with a single-screen redirect notice: "This GDD was decomposed on 2026-06-XX into 3 sub-GDDs — see `player-movement-mechanics.md`, `player-movement-presentation.md`, `player-movement-platform.md`." Preserve the header block as historical record. Any downstream GDD or seam doc that references `player-movement.md` continues to resolve to a valid (redirect) file.
- Option C (most aggressive): delete `player-movement.md`; rely on review log + this plan + decomposed sub-GDDs for all future reference. Risk: breaks any external link.
- **CD recommendation: Option B.** Lowest churn on downstream references.

**Step 7 — Author review log files for each sub-GDD (15 min)**:
- `design/gdd/reviews/player-movement-mechanics-review-log.md` (new)
- `design/gdd/reviews/player-movement-presentation-review-log.md` (new)
- `design/gdd/reviews/player-movement-platform-review-log.md` (new)
- Each new review log opens with a single entry: "Pre-decomposition history: see `design/gdd/reviews/player-movement-review-log.md` (R1 through R11). Decomposition executed 2026-06-XX per `design/gdd/reviews/player-movement-decomposition-plan-2026-06-16.md`. This sub-GDD inherits the R11 BLOCKING items assigned to its scope (see decomposition plan §5)."
- The pre-decomposition log `player-movement-review-log.md` is preserved and marked closed.

**Step 8 — Final cross-reference validation (30 min)**:
- Grep all files in `design/` and `docs/architecture/` for references to `player-movement.md` — update to point to the relevant sub-GDD or to the decomposed redirect notice
- Grep `design/registry/entities.yaml` for PM references
- Verify seam doc Seam 12 still resolves correctly (it points to the mechanics sub-GDD's Movement State Enum + Public Interface)

**Step 9 — Commit and update session-state**:
- Single commit titled "Decompose player-movement.md into 3 sub-GDDs per R11 decomposition trigger"
- Update `production/session-state/active.md` to reflect the decomposed structure + queue R12 sub-GDD reviews

---

## 7. R12a brief outlines per sub-GDD

Each sub-GDD gets its own R12a author brief drafted during decomposition execution. The briefs are scoped to the BLOCKINGs assigned in §5 + any forward-contract authoring outstanding from §4.

### 7.1 player-movement-mechanics R12a brief outline

**Scope**: Address B-F6-1, B-F6-2, B-F6-3 (caller side), B-F6-4 + author all mechanics-owned forward contracts.

**Author decisions required**:
- DR-F6-FADE: F-6 fade-out tick-1 multiplier curve satisfying both anti-snap AND anti-additive. Options: (a) cosine ramp 1.0 → cos(π/4) ≈ 0.707 → 0; (b) shorter 1-frame fade with smaller magnitude; (c) frame-1 multiplier hard-cap based on F-6 magnitude threshold. CD recommendation pending re-derivation.
- DR-F6-EC15: EC15 formula re-derivation at correct worst case TP=0.30-0.65. Options: (a) replace `1.0 − TP×c` with `1.0 − TP^k × c` for some k>1 (steeper at low TP); (b) restrict F-6 firing to TP ≥ 0.50 on F-4 path (eliminates worst case); (c) accept clamp engagement at TP=0.30-0.65 as design-acceptable head-snap-under-load behavior and tighten AC-F6-E.
- DR-F6-DT: F-6 effective_dt definition site. CD recommendation: move to TickComponent prologue (platform-owned definition; mechanics-consumer access).
- DR-F6-ASSET: EDGE_ABSORB_CURVE_ASSET reference type. CD recommendation: (a) hard reference matching SLIP_CURVE/LEAN_CURVE pattern (closes Shipping crash defect cleanly + matches similar curve pattern).

**Forecast**: 1-3 BLOCKING at R12 (single-domain doc).

**Decomposition trigger for mechanics sub-GDD**: >5 BLOCKING (tighter than monolith's >8 — single-domain doc should converge cleanly).

### 7.2 player-movement-presentation R12a brief outline

**Scope**: Address B-BANNER-1, B-CERT-1, B-CERT-2 (call site), B-AUDIO-1, B-AUDIO-2 + author all presentation-owned forward contracts.

**Author decisions required**:
- DR-PRES-VOCAB: Banner vocabulary path. CD recommendation: (a) revise to "Performance mode — hardest waves disabled" ("waves" is already player-facing).
- DR-PRES-FLASH: Player-controllable flash/reduce-motion setting design. Default state + setting copy + dispatch gate. Accessibility-specialist co-sign.
- DR-PRES-HAPTIC-GATE: Haptic OS-state gate semantics. Verify against Apple HIG + Android accessibility guidance.
- DR-PRES-DUCK: AC-AUDIO-CUE-DUCKING reverse-trigger-order resolution. Audio-director domain authority.
- DR-PRES-RAMP: HARD-CUT ramp shape (raised-cosine).

**Forecast**: 1-3 BLOCKING at R12.

### 7.3 player-movement-platform R12a brief outline

**Scope**: Address B-QA-1, B-PERF-1, B-SHIP-1, B-CERT-2 (API side), B-F6-3 (definition site) + author all platform-owned forward contracts.

**Author decisions required**:
- DR-PLAT-TEST: Test strategy refresh path. (a) Extend AC-21/SS-A windows to ≥601 ticks; (b) pre-seed TickClampLogCounter; (c) both. Plus: refresh test strategy preamble with R11a-added private members.
- DR-PLAT-HW-B: AC-HW-B measurement methodology. (a) Raw FApp::GetDeltaTime() log apparatus; (b) widen tolerance to 1.5ms; (c) both (raw apparatus + wider tolerance for cross-validation).
- DR-PLAT-SS-C: AC-SS-C Shipping guard body. Author full bManualTickEnabled early-out + 1Hz rate-limited log idiom, OR mark STUBBED pending OQ-1 ADR.
- DR-PLAT-EFFECTIVE-DT: effective_dt TickComponent prologue definition + usage propagation to F-2 and F-6 consumers.
- DR-PLAT-HAPTIC-API: `IHapticDispatch::IsSystemHapticsEnabled()` API spec + cross-platform behavior (iOS Focus vs Android DND).
- DR-PLAT-FLASH-API: configuration-gate API for flash/reduce-motion setting (parallel to IGameSettings near-miss-haptic).

**Forecast**: 1-3 BLOCKING at R12.

---

## 8. Out of scope for this plan

- **Actual content authoring of the sub-GDDs**: deferred to author execution (Steps 2-3 of §6).
- **Cross-system propagation**: `pull-wave-behavior.md`, `difficulty-phase-controller.md`, `wave-spawner-pattern-library.md`, HUD GDD, Input System §Haptic Vocabulary, `entities.yaml` — all queued for `/propagate-design-change` post-decomposition-execution-COMPLETE. No changes here.
- **Sub-GDD R12 reviews**: each sub-GDD gets independent /design-review cycles after decomposition lands. Pull-Wave R9 and Telegraph prototype can proceed in parallel.
- **Engine project bootstrap**: Sprint 1 dependency, not decomposition dependency. PM decomposition can complete and reach Approved without the UE project existing.
- **Polish-phase items**: stub-and-confirm placeholders in platform sub-GDD remain stubbed (canonical UE 5.6+ framerate-floor mechanism, named min-spec device list, Polish-phase device audit) — these are TD + performance-analyst Polish-phase responsibility, not decomposition responsibility.

---

## 9. Decomposition risk register

| Risk | Likelihood | Mitigation |
|---|---|---|
| Sub-GDD inherits a BLOCKING that turns out to be cross-sub-GDD (orphaned forward contract) | Medium | §4 cross-sub-GDD contract authoring is exhaustive; review log for each sub-GDD carries an inventory of inherited R11 BLOCKINGs |
| Author execution exceeds 3-5 hour estimate | Low-Medium | Mechanical work; subagents not required; estimate has 30% padding |
| Downstream GDD references break (Pull-Wave, DPC, seam doc, registry) | Low | Step 8 validation grep catches; Option B redirect in §6 Step 6 preserves path |
| One sub-GDD turns out to be too small (under 300 lines, scope thin) | Low | If platform sub-GDD comes in <400 lines post-split, fold platform into mechanics with a separate AC subsection. Reserve decision for execution time. |
| Specialist tiers don't map cleanly to sub-GDDs in practice (e.g., systems-designer needs to author presentation) | Medium | Forward-contract pattern allows cross-tier collaboration; sub-GDD owners are primary, not exclusive |
| Decomposition discovers a new R11 BLOCKING during content move (e.g., a section that doesn't fit cleanly in any sub-GDD) | Low-Medium | Document inline; surface to user before proceeding; may trigger a refinement of the 3-sub-GDD model |

---

## 10. Execution recommendation

CD recommends:
1. **Decomposition execution in a `/clear` session** (estimated 3-5 hours focused work).
2. **Pull-Wave R9 in parallel** (independent track; user already chose this as next operational step).
3. **Telegraph prototype in parallel** (highest-risk bet; can proceed without PM decomposition).
4. After decomposition: **R12 fresh-context reviews per sub-GDD** (3 independent /design-review calls).
5. **Decomposition trigger per sub-GDD**: >5 BLOCKING (tighter than the monolith's >8). If a sub-GDD trips >5 BLOCKING at R12, recommend further decomposition or scope tightening within that sub-GDD.

**Forecast for the 3 sub-GDDs combined at R12**: **3-9 BLOCKING total** (1-3 per sub-GDD × 3 sub-GDDs). This is a calibrated forecast band based on single-domain-doc convergence patterns observed in DPC (R7=10 → Approved) and Input System (review 27 → Approved 0 BLOCKING).

---

**Plan authored by**: creative-director synthesis on R11 fresh-context re-review (2026-06-16).
**Plan authority**: binding for the next author session executing decomposition. Override only with documented rationale recorded in the executed sub-GDD's status header.
**Plan execution gate**: a `/clear` fresh-context session AFTER this plan is durable on disk. R7 same-session-bias precedent does NOT apply — this is structural execution, not reviewer override.

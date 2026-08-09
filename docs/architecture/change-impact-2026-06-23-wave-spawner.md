# Change Impact Report — Wave Spawner R3a Closure

| Field | Value |
|-------|-------|
| **Date** | 2026-06-23 |
| **Source GDD** | `design/gdd/wave-spawner-pattern-library.md` |
| **Triggering event** | Wave Spawner R3a in-session closure (2 binding decisions: R3a-1 E3 lifecycle fix + R3a-2 H.1 narrative refresh) per qa-lead R3 solo grep verification + user "terminal-now: /propagate-design-change" selection |
| **Skill mode** | Adapted-flow (no git history on uncommitted GDD; review log + active.md substituted as diff input per 2026-06-19 Pull-Wave + DPC precedent) |
| **TD-CHANGE-IMPACT gate** | Skipped (lean-mode; matches solo qa-lead R3 verification spirit) |
| **Cross-system cascade impact** | Pull-Wave + DPC R10d→R14 closed cascade UNAFFECTED ✅ |

---

## 1. Executive Summary

Wave Spawner & Pattern Library GDD reached R3a closure on 2026-06-22 with 1 BLOCKING (E3 lateral row lifecycle) + 1 RECOMMENDED (H.1 narrative count) resolved via in-session inscription edits. Per qa-lead R3 terminal-now recommendation, this `/propagate-design-change` pass propagates 5 forward contracts surfaced during the R1→R3a review cycle. **Zero ADR revisions required** (no ADR references Wave Spawner). **5 forward-contract inscriptions applied** across 3 sibling GDDs + 1 registry update + 1 source-GDD prose refresh.

---

## 2. ADR Impact Analysis (Zero Revisions)

| ADR | Title | Wave Spawner reference | Status |
|-----|-------|----------------------|--------|
| ADR-0001 | Palm Rejection rmax Calibration | Zero references | ✅ Still Valid |
| ADR-0002 | Haptic Platform Bridge | Zero references | ✅ Still Valid |
| ADR-0003 | Drain Queue Architecture | Zero `wave/spawner/pool` references; Input System scope only | ✅ Still Valid |
| ADR-0004 | R11a Dual-Grep Methodology | References DPC + Pull-Wave as methodology illustrations; sub-classes (b) + (e) used by qa-lead R3 to surface R3 BLOCKING + RECOMMENDED but methodology unchanged | ✅ Still Valid |

**Rationale for zero revisions**: Wave Spawner is the youngest GDD; no architectural decisions yet written against it. The implementation-level ADR for subsystem hosting (per OQ-WS-3 close — locked `UGameInstanceSubsystem`) is a downstream artifact pending (`docs/architecture/adr-NNNN-wave-spawner-subsystem-hosting.md`; sibling to OQ-PW-3 HISM/ISMC ADR). Authoring that ADR does not re-adjudicate the decision; it documents the already-locked choice.

---

## 3. Forward-Contract Propagation (5 contracts applied)

### FC-5 — Wave Spawner Dependencies row 9 prose refresh (sub-class (b) drift)

- **Target**: `design/gdd/wave-spawner-pattern-library.md` line 933
- **Change**: "5 named events emitted" → "9 named events emitted" + audit-trail annotation
- **Source finding**: qa-lead R3 cross-system verification (sub-class (b) narrative drift caught outside R3 BLOCKING/RECOMMENDED surface)
- **Rationale**: R2a-7 expanded C.3 §9 schema from 5 to 9 events (added 4 EC-class runtime defense events). AC-WS-23 (line 1111) + C.3 §9 schema both already updated; Dependencies row 9 was the missed lock-step site.

### FC-4 — entities.yaml `referenced_by` registry update (4 constants)

- **Target**: `design/registry/entities.yaml` (4 entries + header `last_updated`)
- **Change**: Added `design/gdd/wave-spawner-pattern-library.md` to `referenced_by` lists of:
  - `BARRAGE_SIMULTANEITY_WINDOW_S` (qa-lead R3 explicit finding; Wave Spawner Rule 6 + Rule 15 BARRAGE_W_SPAN cook check + F-5 + F-7)
  - `TELEGRAPH_WINDOW_FLOOR_S` (Wave Spawner Rule 6 + Rule 15 NON_BARRAGE_STAGGER + F-6/F-7/F-8 + AC-WS-07 + AC-WS-21)
  - `TELEGRAPH_WINDOW_FLOOR_MAX_ADMISSIBLE_S` (Wave Spawner Rule 15 anchor + F-7 + AC-WS-21)
  - `MAX_CONCURRENT_WAVES_CAP` (Wave Spawner F-2 + AC-WS-09 + G.2 recompute trigger + F-1/F-1b)
- **Header**: `last_updated` advanced 2026-06-19 → 2026-06-23 with propagation note
- **Source finding**: qa-lead R3 cross-system verification surfaced the registry-side gap; extended to 4 constants Wave Spawner has direct cook-time or AC-binding dependencies on
- **Rationale**: Wave Spawner is now a registered consumer of these 4 registry constants; bidirectional consistency requires the consumer list to reflect that.

### FC-1 — RSM `RunSeed : uint64` public-interface field

- **Target**: `design/gdd/run-state-machine.md` new Rule 21 (inserted after Rule 20 before Clock Injection §)
- **Change**: Added new Core Rule defining `RunSeed : uint64` field captured atomically at `COUNTDOWN → RUNNING` transition
- **Source finding**: Wave Spawner R1a-3 + CD R1 Ruling 2 (closes OQ-WS-4); Wave Spawner Rule 8 binding consumer
- **Rationale**: Wave Spawner Rule 8 specifies "the per-run RNG is seeded at the `Cold → Active` transition from the RSM-supplied `RunSeed : uint64` field." Without the RSM-side inscription, Wave Spawner's AC-WS-13 deterministic-RNG-seeding AC has no consumable source for the seed. The new Rule 21:
  - Specifies seed source: `FPlatformTime::Cycles64()` XOR per-installation salt
  - Pins cross-device determinism precluded (consistent with EC-WS-15 platform-scoped Death Replay)
  - Pins UPROPERTY visibility: `BlueprintReadOnly` for Death Replay reflection access
  - Specifies read-edge contract: consumers read at/after `OnStateChanged(RUNNING)` callback edge
  - Specifies replay reuse: Death Replay restores captured seed before re-issuing pattern-admission decisions

### FC-2 — RSM `OnPausedChanged` delegate type pin

- **Target**: `design/gdd/run-state-machine.md` Rule 18 (appended to existing rule)
- **Change**: Pinned delegate as `DECLARE_MULTICAST_DELEGATE_TwoParams(FOnPausedChanged, bool, double)` — non-dynamic multicast
- **Source finding**: Wave Spawner R1a deferred → R2a R-R2-1 + R-R2-3 deferred → R3 deferred-RECOMMENDED (qa-lead R3 explicit deferral note)
- **Rationale**: All current subscribers (Audio Controller, Player Movement, Wave Spawner, Collision) bind in C++ via `AddRaw`/`AddUObject`/`AddLambda`; none require Blueprint exposure. Dynamic multicast rejected — requires `UFUNCTION()`-marked handlers + reflection overhead + cannot bind lambdas (Wave Spawner R2a-2 tick-ordering subscription uses lambda binding inside `UWaveSpawnerSubsystem::Initialize()`). Future Blueprint exposure (if needed) requires a parallel dynamic delegate `OnPausedChangedBP` rather than retrofitting this one.

### FC-3 — DPC `OnPostTickFrameStatePublished` multicast delegate

- **Target**: `design/gdd/difficulty-phase-controller.md` Rule 10 (appended to existing rule)
- **Change**: Inscribed post-tick publication delegate `DECLARE_MULTICAST_DELEGATE_OneParam(FOnPostTickFrameStatePublished, const FDPCFrameState&)` as sibling broadcast to existing polling-based `GetCurrentFrameState()` interface
- **Source finding**: Wave Spawner R2a-2 tick-ordering pin closure (3-spec convergence: qa B-R2-5 + sys + unreal B-R2-3)
- **Rationale**: Wave Spawner subscribes to this delegate inside `UWaveSpawnerSubsystem::Initialize()` and runs its admission decision inside the callback — makes tick ordering structural (`RSM → DPC → WaveSpawner`) rather than tick-group-dependent. Inscription specifies:
  - Fires at END of each DPC tick (after current/previous snapshot fields overwrite — consistent post-tick state)
  - Fires unconditionally each tick (active + inactive snapshots both publish; pre-init inactive snapshot during async-load window per EC-13 publishes)
  - Non-dynamic delegate (lambda-friendly for Wave Spawner; no Blueprint subscribers exist)
  - Re-entrancy: subscribers must not call DPC mutators (structurally precluded by DPC's no-public-mutator design)
  - Test stub: Seam 6 `IDPCFrameStateProvider` exposes `BroadcastOnPostTickFrameStatePublished(...)` method for test-side synthesis

---

## 4. Files Modified by This Pass

| File | Type of change | Lines added |
|------|----------------|-------------|
| `design/gdd/wave-spawner-pattern-library.md` | FC-5: Dependencies row 9 prose refresh | ~3 |
| `design/registry/entities.yaml` | FC-4: 4 referenced_by list extensions + header last_updated update | ~9 |
| `design/gdd/run-state-machine.md` | FC-1: Rule 21 inscription (RunSeed field) + FC-2: Rule 18 delegate-type pin extension | ~2 paragraphs |
| `design/gdd/difficulty-phase-controller.md` | FC-3: Rule 10 post-tick delegate inscription | ~1 paragraph |
| `docs/architecture/change-impact-2026-06-23-wave-spawner.md` | NEW; this file | ~200 |

---

## 5. Files NOT Modified (intentional)

- `docs/architecture/adr-0001-palm-rejection-rmax-calibration.md` — does not reference Wave Spawner ✓ Still Valid
- `docs/architecture/adr-0002-haptic-platform-bridge.md` — does not reference Wave Spawner ✓ Still Valid
- `docs/architecture/adr-0003-drain-queue-architecture.md` — does not reference Wave Spawner ✓ Still Valid
- `docs/architecture/adr-0004-r11a-dual-grep-methodology.md` — methodology unchanged; ADR-0004 §Oracle-Site Sweep Corollary sub-classes (b) + (e) used to surface R3 BLOCKING + RECOMMENDED, but the methodology itself held through R3 verification ✓ Still Valid
- `docs/architecture/platform-seam-interfaces.md` — no seam changes; Seam 13 (`IWaveSpawnerCallback` + `FWaveSpawnerCallbackTestStub`) clean per qa-lead R3 verification
- `design/gdd/pull-wave-behavior.md` — Pull-Wave + DPC R14-closed cascade NOT re-opened
- `design/gdd/player-movement.md` — out of scope; in independent MAJOR REVISION NEEDED + decomposition recommended track
- `design/gdd/input-system.md` — out of scope
- `design/gdd/game-concept.md` — Core Fantasy "seven-tenths" already updated at R11a-arithmetic 2026-06-18
- `docs/architecture/adr-NNNN-wave-spawner-subsystem-hosting.md` — does not yet exist (downstream artifact authorship per OQ-WS-3 close)
- `docs/architecture/adr-NNNN-pullwave-instanced-renderer.md` — does not yet exist (HISM/ISMC ADR per OQ-PW-3 close)

---

## 6. Cross-System Invariant Verification

All invariants verified UNAFFECTED by this propagation pass:

| Invariant | Value | Status |
|-----------|-------|--------|
| `SLIP_TWEEN × MIN_ESCAPE_SLIPS + REACT ≤ TELEGRAPH_WINDOW_FLOOR_S` (locomotion survivability) | 0.50 ≤ 0.70 (REACT=0.20) / 0.55 ≤ 0.70 (REACT=0.25) | ✅ HELD |
| `BARRAGE_SIMULTANEITY_WINDOW_S = FLOOR / 2` (perceptual identity ratio) | 0.35 = 0.70 / 2 | ✅ HELD |
| `TELEGRAPH_WINDOW_FLOOR_MAX_ADMISSIBLE_S` (PATH (i) ceiling per Constraint A) | 0.76s | ✅ HELD |
| Pull-Wave + DPC R10d→R14 closed cascade | Trajectory R10(19)→R11(8)→R12(7)→R13(1)→R14(0) | ✅ HELD |
| Wave Spawner R1→R3a closure trajectory | R1(9)→R2(8)→R3(1) monotonically decreasing | ✅ HELD |

---

## 7. Resolution Decisions (Phase 7 walkthrough)

User selection via AskUserQuestion: **Full scope (all 5 contracts) — Recommended**. All 5 forward contracts applied in this session. No ADRs required Mark-Superseded action (zero ADRs reference Wave Spawner).

---

## 8. Follow-Up Actions

- **Story-Done blockers (downstream artifact authoring)**:
  - `docs/architecture/adr-NNNN-wave-spawner-subsystem-hosting.md` — assign `unreal-specialist`; sibling to OQ-PW-3 HISM/ISMC ADR; documents the locked `UGameInstanceSubsystem` decision per OQ-WS-3 close.
  - `docs/architecture/adr-NNNN-pullwave-instanced-renderer.md` — Pull-Wave story-Done blocker per OQ-PW-3.
- **Independent tracks (parallel `/clear` sessions)**:
  - PM decomposition execution per `design/gdd/reviews/player-movement-decomposition-plan-2026-06-16.md` §6 (~3-5 hours).
  - Telegraph System prototype (highest-risk bet per `design/gdd/systems-index.md`).
- **Optional R4 confirmation** (per qa-lead R3 recommendation): solo qa-lead grep confirmation pass on Wave Spawner GDD. Forecast 0 BLOCKING / 0-1 RECOMMENDED HIGH confidence; would CONFIRM full R3a closure trajectory but is structurally optional.

---

## 9. Validation Outcome

**COMPLETE.** All 5 forward contracts inscribed; cross-system cascade unaffected; zero ADR revisions required; change-impact doc written. Wave Spawner GDD review cycle CLOSED at R3a per qa-lead R3 terminal-now recommendation.

Forecast calibration record (Wave Spawner — final):

| Review | Forecast | Observed | Status |
|---|---|---|---|
| R1 | 8-15 (pre-review) | 9 | CALIBRATED ✓ |
| R2 | 2-4 (R1a forecast) | 8 | 2× miss — first calibration data point |
| R3 | 0-3 (R2a forecast) | 1 | CALIBRATED ✓ |
| **R4** (optional) | **0 BLOCKING / 0-1 RECOMMENDED** | TBD | pending HIGH confidence |

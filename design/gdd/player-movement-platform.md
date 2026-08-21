# Player Movement — Platform

**Status**: Draft (post-decomposition skeleton; bodies filled in Step 2 of decomposition execution)
**Date**: 2026-06-28
**Decomposed from**: `design/gdd/player-movement.md` (1823-line monolith, archived to redirect notice 2026-06-28)
**Decomposition plan**: `design/gdd/reviews/player-movement-decomposition-plan-2026-06-16.md`
**Scope**: PM's hardware contract, Shipping-Safety enforcement, watchdog, frame-pacing, OS state integration (Focus / DND), test infrastructure for non-mechanics ACs, min-spec device list, profiler methodology. The "device-tier + engine-tier contract that keeps the game playable" sub-GDD.
**Primary owners**: performance-analyst, unreal-specialist, qa-lead
**Sibling sub-GDDs**: `player-movement-mechanics.md`, `player-movement-presentation.md`
**Pre-decomposition review history**: `design/gdd/reviews/player-movement-review-log.md` (R1 through R11, marked closed at decomposition)
**Sub-GDD review log**: `design/gdd/reviews/player-movement-platform-review-log.md`
**Inherited R11 BLOCKING items (per decomposition plan §5)**: B-QA-1, B-PERF-1, B-SHIP-1 + B-CERT-2 (API side — call site belongs to presentation) + **B-F6-3 CLOSED at Step 2 PASS 7 2026-06-29** (definition site authored at §4 F-PROLOGUE; caller-side consume edits applied to mechanics §4 F-2 + F-6) — remaining items to be addressed at R12a per `design/gdd/reviews/player-movement-platform-r12-author-brief-2026-06-28.md`
**Cross-sub-GDD forward contracts**: see §6 Dependencies — 8 platform↔mechanics + 4 platform↔presentation contracts authored per decomposition plan §4 (Step 3 execution 2026-07-01 closed §4.2 canonical coverage; additional-beyond-plan item 5 plat↔mech retained from prior PASSes; §4.3 was already fully covered at PASS 8)

> **R10a binding decisions inherited from PM monolith (2026-06-14 — in-session author revision pass per brief `design/gdd/reviews/player-movement-r10-author-brief-2026-06-11.md` + B-LEAN-2026-06-12-1 fold-in; this sub-GDD inherits only the R10a decisions affecting its scope per decomposition plan §3.3)**:
> (R10a-2) **§5.2 Hardware Contract action on breach = (a) Survivability margin relaxation** — Wave Spawner gate suppresses M=3 PEAK barrage class on watchdog breach; M=2 surviving triplet set remains. Forward-contracted on `wave-spawner-pattern-library.md`. (Cross-scope note: the player-facing surface of this breach action — the banner notification — is owned by `player-movement-presentation.md` per R10a-3 + R11a-9. R11a-8 below extends this Wave Spawner contract with a BINDING 3.0 s post-breach grace window.)
> (R10a-9) **§1.2 Shipping-Safety Enforcement Policy NEW** — cross-cutting binding policy: every BLOCKING invariant gets both dev-build assertion AND Shipping-safe guard. Enforcement table covers all existing BLOCKING invariants + new ones added by R10a §1.3. 5 new ACs (AC-SS-A through AC-SS-E). AC-21 one-shot floor-guard semantic RETIRED in favor of persistent F-2 clamp.
> (R10a-10) **§1.3 Hardware Contract enforcement rewrite + B-LEAN fold-in** — full DT watchdog spec (60-sample rolling window, sustained-sub-55 + hitch-cluster breach conditions, 3.0s clean hysteresis-release), stub-and-confirm placeholders for canonical UE 5.6+ framerate-floor mechanism + min-spec device list (TD + performance-analyst Polish-phase audit closes both stubs), 60 fps lock re-derived on Player Fantasy + camera/shader + thermal grounds. 3 new ACs (AC-HW-A/B/C). The pre-R10a `t.MaxFPS = 60` floor declaration RETRACTED as wrong engine semantic.

> **R11a binding decisions inherited from PM monolith (2026-06-15 — in-session author revision pass per brief `design/gdd/reviews/player-movement-r11-author-brief-2026-06-15.md`; this sub-GDD inherits only the R11a decisions affecting its scope per decomposition plan §3.3)**:
> (R11a-6) **§3 B.1 — Watchdog buffer sentinel pre-fill** — `TickDTRollingBuffer` is pre-filled with `0.01667f` (nominal 60 fps clean sample) at BeginPlay; `TickDTRingIndex`, `ContinuousCleanWindowTime`, `bHardwarePerformanceBreachActive` initialized explicitly. New "Initialization at BeginPlay" spec table row + BeginPlay pseudo-code block + AC-HW-A Setup G (two-part: sentinel-presence regression catch + degraded-startup detection latency). Closes the zero-init blindness defect during the 1.0 s startup hitch-exposure window.
> (R11a-7) **§3 B.2 — AC-HW-B math/units rewrite** — Pre-R11a "99% of frames within 17.67ms (= 60 fps ± 1 ms margin)" criterion rejected (units error: 17.67 ms ≠ 60 fps; hitch-budget error: 1% = 36 hitches/session ≈ 10× perceptible threshold; single-criterion conflation of steady-state and peak hitch). Replaced with three orthogonal pass criteria: (1) ≥ 99.9% of frames `DT ≤ 16.67ms + 0.5ms`, (2) NO single frame `DT > 33.33ms`, (3) `is_hw_performance_degraded == false` throughout the session. All three must hold. Parallel summary in Hardware Contract subsection updated.
> (R11a-8) **§3 DR-B.3 = (b) Grace window, N = 3.0 s** — Wave Spawner forward contract amended with a BINDING 3.0-second post-breach grace window during which NO new M=3 PEAK barrages may be dispatched, regardless of `is_hw_performance_degraded` sampling within the window. Closes the in-flight M=3 PEAK gap at gate engagement onset. Rationale: 3.0 s covers max plausible M=3 PEAK dispatch-to-arrival latency at degraded framerate AND matches the hysteresis-release window for design symmetry. AC-HW-C survivability promise re-qualified accordingly. (a) Documented caveat and (c) Survivability proof at degraded framerate options REJECTED — (a) breaks Pillar-5-honest framing the banner is supposed to deliver; (c) is too brittle at the input-latency boundary.
> (R11a-17) **§7 DR-G.1 = (b) Reframe AC-SS-E to test local C++ constant** — Pre-R11a AC-SS-E asserted a yaml→C++ header generator pipeline (`design/registry/entities.yaml` → C++ header import site for `MIN_ESCAPE_SLIPS`) that does NOT exist as of R11a authoring (no generator tool documented, no header location specified, no CI integration). Pre-R11a AC was unimplementable: the test mechanism ("CI variant build with the constant changed in entities.yaml") cannot run without the generator pipeline that produces the rebuilt header. R11a-17 reframes the AC to test what IS implemented: PM's compile unit defines the `MIN_ESCAPE_SLIPS` C++ constant locally (the constant's value MUST match the registry value 2; designer-author responsibility at the moment the value is set in code, NOT auto-synced from yaml), guarded by a `static_assert(MIN_ESCAPE_SLIPS == 2, ...)`. The rewritten AC asserts: if a future change edits the local C++ `MIN_ESCAPE_SLIPS` constant to any value other than 2, the PM compile unit fails to compile with the documented static_assert message. The trade-off (registry→code drift is NOT caught at compile time) is documented as a known gap; the future yaml→C++ generator pipeline is filed as **OQ-7** for a separate ADR (out of R11a / out of PM GDD scope — belongs in `docs/architecture/` as an architecture seam decision). §1.2 enforcement table SLIP_TWEEN-row note carried forward; the `MIN_ESCAPE_SLIPS` row was already worded compatibly with the (b) reframe ("static_assert in PM compile unit at the constant's import site" — under (b), "import site" = "header where the constant is declared in PM compile unit"). Documented brief deviation NOT required: (b) is the brief recommendation; same path. The registry remains the source of truth for design intent — drift is caught at code-review time when the local constant edit is made, not at compile time of the registry change.

---

## 1. Overview

Player Movement (Slip) — Platform owns the device-tier + engine-tier contract that keeps PM mechanics playable on the supported mobile hardware: the runtime DT watchdog (60-sample rolling buffer + sustained-sub-55 + hitch-cluster breach detection + 3.0 s hysteresis-release per R10a §1.3 + R11a-6/7/8), the Shipping-Safety Enforcement Policy (cross-cutting binding policy per R10a-9: every BLOCKING mechanics invariant gets both dev-build assertion AND Shipping-safe guard), the iOS / Android OS state integration (Focus / DND haptic gating per R12a B-CERT-2 + safe-area top-inset binding per R11a-10), the per-framerate F-BARRAGE-SURVIVABILITY-INVARIANT verification table (frame-quantized at 20 / 30 / 50 / 60 / 120 fps × SLIP_TWEEN safe-range corners — §3 tabulates 60/50/30/20 fps at default tuning; §4 F-BARRAGE-SURVIVABILITY-INVARIANT-CORNERS adds 120 fps for high-refresh future devices + cross-products with the SLIP_TWEEN safe-range corners under R10d FLOOR=0.70s), the platform-facing public interface (`is_hw_performance_degraded` + `OnHardwarePerformanceBreach` broadcast), the TickComponent prologue contract (where `raw_dt = FApp::GetDeltaTime()` and `effective_dt = clamp(raw_dt, 0.0f, MAX_SLIP_DT_S)` are defined per B-F6-3 closure — single-source for both the watchdog hardware feed AND the mechanics-side tween advance; consumed by §3 watchdog AND by F-2 and F-6 in `player-movement-mechanics.md`; full DT-source rationale in §4 F-PROLOGUE), and the AC-HW-A/B/C + AC-SS-A/B/C/D/E + AC-21 test infrastructure. The platform layer consumes mechanics-layer outputs (SLIP_TWEEN safe range, MAX_SLIP_DT_S, ERunSlipState ordinals, HandleSlipTransition call site registration) to derive its survivability table; it produces broadcasts to presentation (banner trigger via `OnHardwarePerformanceBreach` consumed by `player-movement-presentation.md`) and gating contracts to Wave Spawner (M=3 PEAK barrage suppression + 3.0 s grace window per R11a-8). The platform sub-GDD is the contract of trust between the player and the device: the game runs at the same difficulty on every supported phone, or it discloses honestly that it cannot.

## 2. Player Fantasy

The player buys a phone, downloads SLIPSTORM, and expects the game to keep its promise: the slip is responsive, the wave is fair, the run is honest. The platform contract is how the game keeps that promise across the heterogeneous landscape of mobile hardware that the player never thinks about.

When the device can hold 60 fps, the player feels the full design: M=3 PEAK barrages arrive on schedule, the slip tween settles cleanly, the survivability invariant holds with margin. When the device cannot — sustained framerate dips below the watchdog threshold, hitch clusters accumulate, thermal throttle engages — the platform layer detects this within a 60-sample rolling window and broadcasts the breach. The Wave Spawner gates the hardest barrage class. The Performance-mode banner appears below the OS safe-area inset, disclosing what changed. The player feels the relaxation as a *choice the game made on their behalf*, not as a silent erosion of the difficulty.

**The platform contract demands:**

- **The hardest barrage class either runs or is disclosed.** No silent M=3 PEAK suppression. If the device cannot deliver the survivability invariant at 60 fps, the watchdog broadcasts, the Wave Spawner gate engages, and the banner says so. The 3.0 s post-breach grace window per R11a-8 covers max-plausible in-flight M=3 PEAK dispatch-to-arrival latency at degraded framerate, so the player never experiences a half-suppressed difficulty.
- **The OS owns the device, not the game.** When the player turns on iOS Focus or Android DND, haptics shut up — the platform-owned device gate (`IHapticDispatch::IsSystemHapticsEnabled()` per B-CERT-2) takes precedence over any in-game accessibility setting. The banner respects the OS safe-area top inset because the OS owns the notch. App Store cert is the contract.
- **The Shipping build cannot lie.** Every BLOCKING mechanics invariant has both a dev-build assertion (catches drift in development) and a Shipping-safe guard (catches drift in production). The persistent F-2 clamp per R11a-1 is the canonical pattern; the §1.2 Shipping-Safety Enforcement Policy per R10a-9 is the cross-cutting enforcement contract. AC-SS-A through E verify the policy holds.
- **Watchdog warmth is the price of honesty.** The 60-sample rolling buffer is pre-seeded at BeginPlay with the nominal 60 fps clean sample (`0.01667f × 60` per R11a-6), so the watchdog can detect degradation during the 1.0 s startup hitch-exposure window — without false-positive on initialization noise. The two-part AC-HW-A Setup G verifies both sentinel presence and degraded-startup detection latency.
- **Profiler measurement is humble.** AC-HW-B's measurement tolerance budgets the profiler noise floor honestly (R12a DR-PLAT-HW-B chooses raw `FApp::GetDeltaTime()` log methodology + widened tolerance per B-PERF-1 closure). The three orthogonal pass criteria (frame-rate percentage + peak hitch + watchdog non-breach) all must hold — no single-criterion conflation.

## 3. Detailed Rules

### Public Interface (Platform-Side)

Platform owns the watchdog broadcast surface that gates the Wave Spawner barrage-class suppression and the Presentation banner. The mechanical surface (lane / tween / lean / counters / `bSlipTweenClampActive`) lives in `player-movement-mechanics.md` §3 Public Interface.

| Property / Method | Type | Description |
|---|---|---|
| `is_hw_performance_degraded` (R10a §1.3 NEW) | `bool` | True when PM's Hardware Contract DT watchdog is in a breach state (sustained sub-55 fps detected); false at startup and after watchdog hysteresis-release. **Consumers**: Wave Spawner gates M=3 PEAK barrage class when true (§5.2 (a) Survivability margin relaxation forward contract — see Cross-System Interface Table below + Hardware Contract subsection); HUD renders the §5.3 (a) Performance banner when true (see `player-movement-presentation.md` § Hardware-Performance Banner); telemetry samples the flag at run-end for post-launch device-quality analysis. **Broadcast**: PM fires `OnHardwarePerformanceBreach(bool bEntering)` multicast delegate alongside the property update so subscribers do not need to poll. |
| `OnHardwarePerformanceBreach` (R10a §1.3 NEW) | `DECLARE_MULTICAST_DELEGATE_OneParam(FOnHardwarePerformanceBreach, bool /*bEntering*/)` | Fires when `is_hw_performance_degraded` transitions in either direction. `bEntering = true` on watchdog breach entry; `bEntering = false` on hysteresis-release. Wave Spawner + HUD subscribe; telemetry may also subscribe. |

### Tick Ordering

Required order per tick: **RSM → PM → Pull-Wave / Camera**.

> **⚠ SUPERSEDED by ADR-0009 SD4 (2026-07-03)** — the (a) / (b) enumeration below was authored pre-ADR-0007 and pre-dates the `URunStateMachineSubsystem::ForceTickNow()` public pull primitive published by ADR-0007 SD2 (Accepted 2026-06-26, amended INT-005 2026-06-26). The canonical pattern for PM's tick ordering with RSM is **option (c): PM's `TickComponent(DeltaTime)` calls `RSMSubsystem->ForceTickNow()` as its first statement after `check(IsInGameThread())`** — engine-scheduler-independent; idempotent within-frame via RSM's `bHasTickedThisFrame` guard. This mirrors ADR-0008 SD1 for DPC's controller. See `docs/architecture/adr-0009-player-movement-hosting.md` Structural Decision 4 + Implementation Guideline 1 for the full rationale and the (a) / (b) rejection prose. Registry forbidden pattern `PlayerMovement_TickComponent_without_prior_ForceTickNow` (architecture.yaml v8, added 2026-07-03) enforces the canonical (c) pattern at code-review time. The (a) / (b) enumeration is preserved verbatim below for revision-history traceability; **do not adopt (a) or (b) for a new implementation**.

- **(SUPERSEDED)** If RSM resolves to a `UActorComponent`: use `AddTickPrerequisiteComponent(RSMComponent)` in PM's `BeginPlay`. *(Historically-relevant pre-ADR-0007 branch; ADR-0007 SD1 selected `UGameInstanceSubsystem` — this branch never applied.)*
- **(SUPERSEDED)** If RSM resolves to `UWorldSubsystem` or `UGameInstanceSubsystem`: `AddTickPrerequisiteComponent` is **not available** for subsystem object types. The OQ-1 ADR must adopt one of: (a) RSM writes cached output values that PM reads with one-frame lag (ordering relaxed); (b) PM's `TickComponent` is called manually from RSM's `Tick`. **If option (b) is chosen: PM's own tick registration MUST be disabled (`PrimaryComponentTick.bCanEverTick = false` in BeginPlay) before RSM calls TickComponent manually — otherwise PM ticks twice per frame, F-2 advances at double speed, tween duration halves, and OnSlipMidpoint may double-fire.** Do not assume UE default tick ordering in this case.

The Option (b) silent-double-tick failure mode is guarded by AC-SS-C (Shipping-Safety Enforcement) — see §8 Acceptance Criteria. **Post-ADR-0009 note (2026-07-03)**: under the canonical option (c) resolution in ADR-0009 SD4, PM never opts into manual-tick mode; `bManualTickEnabled` remains `false` and the AC-SS-C early-out is a defensive no-op. The AC is retained for forward-compatibility against a future RSM object-type change (per §6 Bidirectional Notes item 4) but is not the primary defense against tick-order corruption — the primary defense is the ForceTickNow pull primitive itself.

### Cross-System Interface Table (Platform-Side)

Platform owns the rows that flow off the watchdog broadcast — Wave Spawner's barrage-class suppression contract and HUD's banner rendering contract. The mechanics-side rows (Input System, RSM, Pull-Wave, Collision, Camera, Death Replay) live in `player-movement-mechanics.md` §3 Cross-System Interface Table.

| System | Direction | What flows | Contract |
|---|---|---|---|
| **Wave Spawner** (R10a §1.3 NEW — pre-authoring forward contract; `wave-spawner-pattern-library.md` does not yet exist; amended R11a §3 DR-B.3 = (b)) | PM → | `is_hw_performance_degraded` (read each dispatch tick); `OnHardwarePerformanceBreach(bool bEntering)` multicast delegate | When `is_hw_performance_degraded == true`, Wave Spawner MUST exclude M=3 PEAK barrage patterns from the dispatch pool, leaving only the M=2 surviving triplet set as the active PEAK barrage class (§5.2 (a) Survivability margin relaxation). When `false` (post-hysteresis-release), Wave Spawner returns to the full PEAK pool. The transition does NOT mid-flight-cancel in-progress waves; only the next dispatch decision honors the gate. **Plus 3.0 s post-breach grace window (R11a §3 DR-B.3 = (b)): Wave Spawner MUST NOT dispatch any new M=3 PEAK barrage for 3.0 s wall-clock after `OnHardwarePerformanceBreach(true)` fires, regardless of `is_hw_performance_degraded` sampling within the window; closes the in-flight M=3 PEAK gap at gate engagement onset.** See Hardware Contract → Cross-system forward contracts below for the full contract body + grace-window rationale. |
| **HUD** (R10a §1.3 + R11a §5 DR-D.1/D.2 NEW — pre-authoring forward contract; HUD GDD authoring still queued) | PM → | `OnHardwarePerformanceBreach(bool bEntering)` multicast delegate; reads `is_hw_performance_degraded` for current state | HUD subscribes; on `bEntering=true` renders the §5.3 (a) Performance Banner with R11a-9 locked copy **"Performance mode — hardest barrage suppressed."** (silent, ~24px tall, orange-on-dark; see `player-movement-presentation.md` § Hardware-Performance Banner); on `bEntering=false` fades the banner over 250 ms. **R11a §5 DR-D.2 BLOCKING forward contract**: HUD MUST anchor the banner's top edge to the OS safe-area top inset (iOS `safeAreaInsets.top` / Android `WindowInsetsCompat.Type.systemBars()`), NOT to screen-edge `y=0`. Rendering at `y=0` is silently occluded on notched iOS / Android-status-bar devices. The safe-area binding is a BLOCKING layout constraint on first HUD authoring (closes the silent-occlusion defect surfaced by R11a §5 DR-D.2). **R11a §5 DR-D.1 player-facing-vocabulary BLOCKING forward contract**: "barrage" is currently design-system vocabulary only (zero hits in any player-facing UI string as of R11a). HUD GDD MUST establish "barrage" as player-facing terminology — via wave-class readout, death-replay summary screen, or tutorial — BEFORE the banner copy ships. If HUD authoring elects different player-facing terminology, DR-D.1 must be re-opened and the banner copy revised. Polish-gate co-sign verifies the vocabulary contract was honored. |

### Hardware Contract — Minimum Target Framerate (R10a §1.3 — full rewrite per brief; SUPERSEDES R7-PM-PROPAGATION-REVIEW version + folds in B-LEAN-2026-06-12-1)

#### Survivability invariant (R10a §1.3 — regenerated against MIN_ESCAPE_SLIPS=2)

The F-BARRAGE-SURVIVABILITY-INVARIANT is `SLIP_TWEEN_DURATION_S × MIN_ESCAPE_SLIPS + REACTION_BUDGET ≤ TELEGRAPH_WINDOW_FLOOR_S`. The bound R10a values are:

- `MIN_ESCAPE_SLIPS = 2` (from `design/registry/entities.yaml`, bound by Cross-System Survivability Coordination 2026-06-11; was implicitly `M = 3` in the pre-R10a R7-PM-PROPAGATION-REVIEW prose)
- `REACTION_BUDGET = 0.20s`
- `TELEGRAPH_WINDOW_FLOOR_S = 0.70s` (DPC R10d CD ruling path (i) 2026-06-17; R11a-arithmetic propagation 2026-06-18; `/propagate-design-change` 2026-06-19. Was `0.65s` under R7-R10c; the PM monolith pre-decomposition retained the stale 0.65 value because R10d's `/propagate-design-change` 2026-06-19 noted "PM imposes NO new contracts" but PM consumes this value — PASS 7 cascade-corrects the stale baseline that the monolith carried forward into Step 2's §3 lift)
- `SLIP_TWEEN_DURATION_S = 0.15s` at default tuning (owned by `player-movement-mechanics.md` § Tuning Knobs)

Substituting at FLOOR=0.70s (R10d-bound): `0.15 × 2 + 0.20 = 0.50 ≤ 0.70` — **the invariant holds with a 200 ms continuous-form margin** (about 12 frames at 60 fps). This is a substantive change from the pre-R10a R7-PM-PROPAGATION-REVIEW arithmetic which used `M = 3` at the then-FLOOR=0.65s (verified at zero margin). Under MIN_ESCAPE_SLIPS=2 + R10d FLOOR=0.70s the margin is comfortable.

Frame-quantized verification at supported framerates (R10a §1.3 — regenerated at PASS 7 against R10d FLOOR=0.70s):

| Framerate | DT | Ticks per tween | Wall-clock per tween | × MIN_ESCAPE_SLIPS=2 | + REACTION_BUDGET | vs FLOOR=0.70 |
|---|---|---|---|---|---|---|
| 60 fps | 0.01667 s | ⌈0.15/0.01667⌉ = 9 | 9 × 0.01667 = 0.150 s | 0.300 s | 0.500 s | ✓ holds (200 ms margin) |
| 50 fps | 0.02000 s | ⌈0.15/0.020⌉ = 8 | 8 × 0.020 = 0.160 s | 0.320 s | 0.520 s | ✓ holds (180 ms margin) |
| 30 fps | 0.03333 s | ⌈0.15/0.0333⌉ = 5 | 5 × 0.0333 = 0.1665 s | 0.333 s | 0.533 s | ✓ holds (167 ms margin) |
| 20 fps (= MAX_SLIP_DT_S) | 0.05000 s | ⌈0.15/0.05⌉ = 3 | 3 × 0.05 = 0.150 s | 0.300 s | 0.500 s | ✓ holds (200 ms margin) |

**All four supported framerates now hold the invariant under MIN_ESCAPE_SLIPS=2 + FLOOR=0.70s**. The pre-R10d FLOOR=0.65 baseline (which the monolith inherited under PM Step 2 lift; carried staleness now PASS 7 corrects) reported smaller margins — 30 fps came in at 117 ms; 60 fps at 150 ms — but all four still held. Under R10d FLOOR=0.70 every row picks up 50 ms additional margin. The R7-PM-PROPAGATION-REVIEW 60 fps lock based on survivability arithmetic is therefore no longer arithmetically required at the default tuning. R10a §1.3 re-derives the 60 fps lock on non-survivability grounds (below).

#### 60 fps lock — re-derived justification (R10a §1.3 — replaces R7-PM-PROPAGATION-REVIEW survivability-arithmetic justification, which is mathematically false under MIN_ESCAPE_SLIPS=2)

PM's minimum-supported target framerate remains **60 fps mobile** — BINDING — but on three non-survivability grounds:

1. **Player Fantasy — "body before mind"**: the Phase 1 lean-in shelf (`SLIP_CURVE_ASSET` flat from `TweenProgress` ∈ [0, 0.20]) must be visible. At 30 fps with DT = 0.0333s, the first tween tick advances `TweenProgress` by `0.0333 / 0.15 = 0.222` — past the 0.20 shelf endpoint. The shelf is invisible. Player Fantasy explicitly demands the "bend before move" beat is registered visually; at 30 fps the beat is skipped, undermining the fantasy. Hard 60 fps requirement, not soft.
2. **Camera + shader coherence (forward contract on camera-system.md)**: PEAK barrage telegraph rendering uses motion-coherent shaders (per Pull-Wave R8 cross-system coordination). Shader timing budgets assume 60 fps tick cadence; at 30 fps the telegraph fade timing becomes coarse-grained and reads as a stutter rather than a continuous build. Camera-system GDD must inherit the 60 fps assumption — propagation queued.
3. **Thermal sustainability on min-spec hardware**: mobile devices that sustain 60 fps PEAK density without throttling are by definition above the "casual gameplay" thermal envelope (≈ 4 W package power on a typical mobile SoC). Devices that throttle to 30 fps within a 60s run are also approaching the operating-temperature limit for safe screen-touch contact — a UX failure independent of survivability arithmetic. The 60 fps lock is a thermal-safety floor for the device family.

The R7-PM-PROPAGATION-REVIEW "body literally cannot move fast enough at 30fps" justification is RETRACTED — it relied on M=3 survivability arithmetic that no longer holds.

#### Runtime DT watchdog specification (R10a §1.3 NEW per brief)

PM owns a per-tick rolling-window DT measurement that detects sustained sub-55 fps operation and dispatches the §5.2 (a) action-on-breach.

| Spec dimension | Value | Notes |
|---|---|---|
| Sample location | `raw_dt` consumed from §4 F-PROLOGUE (single-source invariant: F-PROLOGUE samples `FApp::GetDeltaTime()` once per tick; the watchdog reads the prologue variable, not the engine static) | After Tick Ordering (RSM → PM); single engine-static read per frame; the watchdog and mechanics F-2/F-6 read the same upstream value |
| Rolling window | 60 samples (= 1.0 s at nominal 60 fps; 1.0–3.0 s wall-clock at degraded framerates) | Ring buffer member `TickDTRollingBuffer[60]`; index modulo 60 |
| Initialization at BeginPlay (R11a §3 B.1 NEW) | All 60 slots of `TickDTRollingBuffer` pre-filled with `0.01667f` (nominal 60 fps clean sample); `TickDTRingIndex = 0`; `ContinuousCleanWindowTime = 0.0f`; `bHardwarePerformanceBreachActive = false` | Sentinel pre-fill is a **code-clarity invariant**, not a runtime correctness fix in the strict-comparison `>` regime: under the current breach math (`TickDTRollingBuffer[i] > 0.01818f` for sub-55 and `> 0.01667f` for sub-60), both `0.0f` (zero-init) and `0.01667f` (sentinel) evaluate to FALSE, so steady-state breach detection latency is identical between the two. The sentinel is load-bearing in three other ways: (1) it makes the buffer's "no data yet" state semantically equal to "nominal clean" rather than "undefined" — future refactors that switch to inclusive `>=` comparisons, add a `TickDTSampleCount` gate, or introduce moving-average math will remain correct; (2) it makes `ContinuousCleanWindowTime` accumulation at startup semantically defensible (the buffer claims clean data, so the release-path accumulator can start from t=0 without surprising a debug-trace reader); (3) it eliminates the "first ~60 frames blind" cognitive footgun for anyone reading the spec — AC-HW-A Setup G Part 1 asserts the sentinel directly as a code-level regression catch independent of runtime semantics. Single-line BeginPlay change; no per-tick branch added. |
| Breach condition (primary — sustained sub-55 fps) | ≥ 30 of last 60 samples have `DT > 18.18ms` (= sub-55 fps tick) | Detects degraded sustained framerate, not transient hitches |
| Breach condition (secondary — transient hitch cluster) | ≥ 18 of last 60 samples have `DT > 16.67ms` (= sub-60 fps tick) AND any single sample has `DT > 33ms` (= visible hitch ≥ 1 frame at 30 fps) | Detects hitch storms (many small misses + one big miss) which read as worse than a steady 50 fps |
| Action on breach | §5.2 (a) Survivability margin relaxation — Wave Spawner gate suppresses M=3 PEAK barrage class (with a 3.0 s post-breach grace window per R11a §3 DR-B.3 = (b)); M=2 surviving triplet set remains the only PEAK barrage class | Forward-contracted on Wave Spawner GDD (see below). Grace window closes the in-flight M=3 PEAK gap at gate engagement onset. |
| Hysteresis (release condition) | All 60 last samples have `DT ≤ 16.67ms` for 3.0 s continuous | Prevents flap between gated and ungated PEAK; release back to M=3 PEAK only after sustained recovery |
| Player UX on breach | §5.3 (a) Banner notification — see `player-movement-presentation.md` § Hardware-Performance Banner | Banner appears on breach, fades on release |

```cpp
// Pseudo-code for PM::BeginPlay DT watchdog initialization (R11a §3 B.1 NEW):
// Sentinel pre-fill — see "Initialization at BeginPlay" spec row above.
for (int32 i = 0; i < TickDTRollingBufferSize; ++i) {
    TickDTRollingBuffer[i] = 0.01667f;  // nominal 60 fps clean seed
}
TickDTRingIndex                  = 0;
ContinuousCleanWindowTime        = 0.0f;
bHardwarePerformanceBreachActive = false;
```

```cpp
// Pseudo-code for PM::TickComponent DT watchdog (R10a §1.3 NEW; PASS 7 — consumes §4 F-PROLOGUE).
// CurrentDT is sourced from the §4 F-PROLOGUE `raw_dt` variable — single-source invariant:
// F-PROLOGUE samples FApp::GetDeltaTime() once per tick at the very top of TickComponent;
// the watchdog reads the prologue value, not the engine static. Hitch-cluster detection
// (MaxSampleDT > 0.033f) requires the UNCLAMPED raw_dt — a clamped feed would mask the
// very hitches the watchdog defends against. See §4 F-PROLOGUE for the full rationale.
const float CurrentDT = raw_dt;
TickDTRollingBuffer[TickDTRingIndex] = CurrentDT;
TickDTRingIndex = (TickDTRingIndex + 1) % TickDTRollingBufferSize;  // 60

// Count breach samples in window:
int32 SamplesAbove18ms = 0;   // sub-55 fps
int32 SamplesAbove16ms = 0;   // sub-60 fps
float MaxSampleDT     = 0.0f; // max single-frame hitch
for (int32 i = 0; i < TickDTRollingBufferSize; ++i) {
    if (TickDTRollingBuffer[i] > 0.01818f) ++SamplesAbove18ms;
    if (TickDTRollingBuffer[i] > 0.01667f) ++SamplesAbove16ms;
    MaxSampleDT = FMath::Max(MaxSampleDT, TickDTRollingBuffer[i]);
}

const bool bSustainedSub55 = (SamplesAbove18ms >= 30);
const bool bHitchCluster   = (SamplesAbove16ms >= 18 && MaxSampleDT > 0.033f);

if (bSustainedSub55 || bHitchCluster) {
    if (!bHardwarePerformanceBreachActive) {
        bHardwarePerformanceBreachActive = true;
        BroadcastHardwarePerformanceBreach(/*entering=*/true);  // Wave Spawner gate + Banner UX
        UE_LOG(LogPlayerMovement, Warning, TEXT("Hardware Contract watchdog breach — entering M=2 PEAK gate"));
    }
} else {
    // Hysteresis: require ALL 60 samples ≤ 16.67ms for 3.0 s before release.
    // Track ContinuousCleanWindowTime; release when ≥ 3.0 s.
    if (SamplesAbove16ms == 0) {
        ContinuousCleanWindowTime += CurrentDT;
        if (ContinuousCleanWindowTime >= 3.0f && bHardwarePerformanceBreachActive) {
            bHardwarePerformanceBreachActive = false;
            BroadcastHardwarePerformanceBreach(/*entering=*/false);
            UE_LOG(LogPlayerMovement, Log, TEXT("Hardware Contract watchdog recovery — releasing PEAK gate"));
        }
    } else {
        ContinuousCleanWindowTime = 0.0f;
    }
}
```

`bHardwarePerformanceBreachActive` is published as a public read-only PM property (`is_hw_performance_degraded`) — see §3 Public Interface (Platform-Side) above. HUD / Wave Spawner / DPC consume it.

#### Engine-specific implementation note (R10a §1.3 — stub-and-confirm during Polish phase)

The pre-R10a prose used `t.MaxFPS = 60` as the framerate floor declaration. This is a UE *cap*, not a floor — wrong engine semantic for the use case (the cap limits maximum framerate; it does not guarantee a minimum). Under UE 5.6+ mobile, the canonical mechanism for declaring a target framerate floor + vsync coupling is:

> **STUB — to be confirmed by technical-director + engine-specialist during Polish-phase device audit**: the exact UE 5.6+ config keys (likely a combination of `[/Script/Engine.RendererSettings]` mobile target FPS, `r.MobileContentScaleFactor`, vsync mode, and potentially a custom `IFramePacer` if UE's default frame pacer is insufficient on iOS for sustained-60-fps thermal safety). The pre-R10a `t.MaxFPS = 60` is RETRACTED as it does not declare a floor.

The R10a §1.3 watchdog above is the runtime-observable floor: it does not depend on engine-level framerate-floor mechanisms. The engine-level declaration is a *correctness improvement* (allows the device to throttle to 60 not above) but not a survivability defense — the watchdog is the survivability defense.

#### Minimum-spec device list (R10a §1.3 NEW per brief)

Polish-phase performance-analyst gates SLIPSTORM's launch readiness on sustained 60 fps under PEAK density on the named min-spec devices below.

> **STUB — to be confirmed by performance-analyst + technical-director during Polish-phase device audit**: the brief recommends one iOS + one Android baseline at ~3-year-old hardware. Placeholder candidates:
> - **iOS min-spec (placeholder)**: iPhone XR (2018, A12 Bionic) — represents the late-life iOS device that should still sustain 60 fps PEAK; if the iPhone XR fails the audit, the floor moves to iPhone 11 (2019, A13).
> - **Android min-spec (placeholder)**: Pixel 5 (2020, Snapdragon 765G) OR Samsung Galaxy A52 (2021, Snapdragon 720G) — represents the mid-tier Android device that should sustain 60 fps PEAK; Snapdragon 6-series at 2-3 years old is the realistic floor for casual mobile.

The min-spec list is BINDING at the launch gate. Devices below the list do NOT receive a "performance reduced" mode that silently changes survivability — they receive the §5.3 (a) Banner Notification while in degraded mode but the underlying survivability invariant is preserved by the §5.2 (a) Wave Spawner gate (M=3 PEAK suppressed). Players on devices below the min-spec are warned that they are below the supported hardware.

Performance-analyst Polish-phase audit pass-criteria:
- **AC-HW-B target (R11a §3 B.2 rewrite)**: sustained 60 fps under PEAK barrage density for 60 seconds of gameplay on each min-spec device, jointly verified by three orthogonal criteria: (1) ≥ 99.9% of frames with `DT ≤ 16.67ms + 0.5ms`, (2) NO single frame with `DT > 33.33ms`, (3) `is_hw_performance_degraded == false` throughout the session. All three criteria must hold. See AC-HW-B in §8 Acceptance Criteria for the full rewrite + rationale.
- Failure on iPhone XR → move iOS min-spec to iPhone 11; re-audit.
- Failure on Pixel 5 → move Android min-spec up by one Snapdragon tier; re-audit.
- Audit failure on BOTH baseline devices → reopen Hardware Contract for creative-director adjudication (likely outcome: tighten min-spec, reduce PEAK density, or both).

#### Cross-system forward contracts (R10a §1.3 NEW — closes §5.2 (a) cross-system gap)

The §5.2 (a) Survivability margin relaxation requires a Wave Spawner GDD (#7 in systems-index, not yet authored as of R10a — queued for post-R10-APPROVED authoring) to honor the following BINDING forward contract:

> **Forward contract on `wave-spawner-pattern-library.md` (R10a §1.3 NEW, parallel to Pull-Wave R7's forward-contract pattern on PM; amended R11a §3 DR-B.3 = (b))**: Wave Spawner MUST consume PM's `is_hw_performance_degraded` public property at the per-wave dispatch decision tick. When `is_hw_performance_degraded == true`, Wave Spawner MUST exclude all M=3 PEAK barrage patterns from the dispatch pool, leaving only the M=2 surviving triplet set as the active PEAK barrage class. When `is_hw_performance_degraded == false` (post-hysteresis-release), Wave Spawner returns to the full PEAK barrage pool including M=3. The transition does NOT mid-flight-cancel in-progress waves; only the next dispatch decision honors the gate.
>
> **Post-breach grace window (R11a §3 DR-B.3 = (b) NEW — closes in-flight M=3 PEAK gap at gate engagement onset)**: Wave Spawner MUST additionally enforce a **3.0-second grace window** following any `OnHardwarePerformanceBreach(true)` event during which it dispatches NO new M=3 PEAK barrages, regardless of how `is_hw_performance_degraded` is subsequently sampled within that window (the grace window is unconditional and dominates any sample-race). The grace window timer starts at the moment of the `bEntering=true` broadcast and persists for 3.0 s wall-clock; new M=3 PEAK barrages are gated even if the watchdog were to spuriously release within the window (which the hysteresis design prevents, but the grace window does not depend on that). The grace window does NOT mid-flight-cancel any M=3 PEAK barrage already in flight at breach onset — those barrages complete their natural lifecycle. After 3.0 s, the `is_hw_performance_degraded` gate becomes the sole gating mechanism for M=3 PEAK dispatch decisions. **Rationale for N = 3.0 s**: (1) covers the maximum plausible M=3 PEAK dispatch-to-arrival latency at degraded framerate (~2.0–3.0 s at 50 fps including telegraph window + arrival travel time), so any M=3 PEAK dispatched a tick before breach reaches the player before the grace window expires; (2) matches the watchdog's hysteresis-release window (3.0 s of continuous-clean) for design symmetry — the device cannot exit breach until 3.0 s of clean samples, AND Wave Spawner cannot dispatch new M=3 PEAK until 3.0 s after breach onset. Wave Spawner GDD inherits this as a BINDING forward contract on first authoring.

Cross-system propagation list (R10a §1.3 — expanded from R7-PM-PROPAGATION-REVIEW Pull-Wave + DPC list):
- `pull-wave-behavior.md` — F-BARRAGE-SURVIVABILITY-INVARIANT context inherits the §5.2 (a) gate semantic. Pull-Wave R9 fresh-context re-review may surface coordination work.
- `difficulty-phase-controller.md` — TELEGRAPH_WINDOW_FLOOR_S Tuning Knob context inherits the hardware contract.
- `wave-spawner-pattern-library.md` (NEW addition R10a §1.3) — inherits the M=3 PEAK suppression gate as a BINDING forward contract on first authoring.
- Producer queues `/propagate-design-change` after R10a APPROVED.

### Shipping-Safety Enforcement Policy (R10a §1.2 NEW — binding cross-cutting policy)

R9 review surfaced a recurring failure mode across multiple findings: assertions that strip in Shipping cannot be the only line of defense for BLOCKING invariants in PM. Concrete pre-R10a examples:

- `ensureMsgf(SLIP_TWEEN_DURATION_S ≤ 0.15f)` BeginPlay assertion is no-op in Shipping; a designer can ship `SLIP_TWEEN = 0.16s` silently breaking the survivability invariant.
- `ERunSlipState` / `EMovementState` lockstep relies on prose only — no `static_assert` adjacent to the seam cast site to catch ordinal drift at compile time.
- The `AddTickPrerequisiteComponent` Option (b) silent-double-tick path (Tick Ordering) has no `check()` guard against PM ticking twice per frame.
- Null curve guards in F-3 / F-5 are `Warning`-only; Shipping silently falls back to linear / zero output.
- AC-21 one-shot floor-guard `bFloorGuardFired` flag prevents log spam but ALSO bypasses subsequent re-violations silently — the value can drift back to zero (e.g., via a corrupt config reload) and no further log fires.

These are the same failure class throughout: an invariant has a dev-time assertion that does not survive cooking. The fix is structural, not a list of point patches.

**Policy (BINDING — applies to every existing BLOCKING invariant in PM and to every new BLOCKING invariant added by future PM revisions):**

> Every BLOCKING invariant in this GDD MUST have BOTH:
> 1. An `ensure` / `ensureMsgf` / `check` / `static_assert` at the defect site for dev-build visibility.
> 2. A runtime clamp, guard, or fallback that holds in Shipping builds (i.e., compiled-in via `#if !UE_BUILD_SHIPPING` is NOT acceptable as the sole defense).
>
> Future PM revisions that add a BLOCKING invariant MUST extend the enforcement table below before the revision is considered complete. Reviewers verify table extension as part of design-review Phase 2.

**Enforcement table (R10a §1.2 — closes pre-R10a R9 cluster 2 findings B-2, B-5, B-6 + the brief §1.2 examples as a class):**

| Invariant | Dev assertion (non-Shipping) | Shipping-safe guard | AC reference |
|---|---|---|---|
| `SLIP_TWEEN_DURATION_S ∈ [0.10, 0.15]` (Tuning Knob safe range — owned by `player-movement-mechanics.md`) | `ensureMsgf(SLIP_TWEEN_DURATION_S >= 0.10f && SLIP_TWEEN_DURATION_S <= 0.15f, ...)` at BeginPlay | Persistent clamp in F-2 prologue (mechanics-owned formula): if outside safe range, log `Error`, clamp to nearest bound (NOT default — clamp preserves operator intent), set `bSlipTweenClampActive = true` flag visible to telemetry. Replaces AC-21's one-shot `bFloorGuardFired` bypass with persistent clamp. | AC-21 (updated semantic — see fold-in below); AC-SS-A NEW (Shipping clamp persistence) |
| `ERunSlipState ↔ EMovementState` ordinal lockstep (mechanics enum + seam doc) | `static_assert(static_cast<uint8>(ERunSlipState::SETTLED) == static_cast<uint8>(EMovementState::SETTLED), "ordinal drift")` adjacent to seam cast site, both members | Defensive `default:` branch in any switch over `EMovementState`: log `Error`, return SETTLED. Pull-Wave / Death Replay consumers receive a safe default rather than reading from a corrupted ordinal. | AC-SS-B NEW (default-branch fallback) |
| `AddTickPrerequisiteComponent` Option (b) silent-double-tick (Tick Ordering above) | `check(!bManualTickEnabled)` at TickComponent prologue when `bManualTickEnabled` is the Option (b) flag | Early-out on `bManualTickEnabled` flag set in BeginPlay (per Tick Ordering Option (b)): if PM's `bCanEverTick` is still true under Option (b), Shipping path early-outs and logs error per frame at 1Hz rate limit. | AC-SS-C NEW (Option (b) double-tick early-out) |
| `TelegraphWindowCurve` / `SlipCurve` / `LeanCurve` non-null at tick (Authored Asset Contracts in `player-movement-mechanics.md` §4) | `ensure(IsValid(Curve))` once per occurrence (idempotent rate-limited via a static flag per curve) | Linear-time fallback (F-3) / zero-lean fallback (F-5) already present; ADD `bCurveFallbackActive` flag visible to debug overlay and HUD telemetry path so the player-facing degradation is observable. | AC-SS-D NEW (fallback flag visibility) |
| `F-2 floor guard` (one-shot `bFloorGuardFired` bypass) — R10a updates from one-shot bypass to persistent clamp | (none — bypass is not assertion-class) | **Replace one-shot bypass with persistent clamp** per the SLIP_TWEEN row above. Per-N-ticks rate-limited log (1 log per 600 ticks = 10s at 60fps) prevents log spam without losing observability of repeated violations. | AC-21 (updated — see fold-in below) |
| `MIN_ESCAPE_SLIPS = 2` cross-system survivability constant (from registry; R11a §7 DR-G.1 = R11a-17 — value is hand-mirrored from registry to PM compile unit, NOT auto-generated; see §9 OQ-7 for future pipeline) | `static_assert` in PM compile unit at the constant's local declaration site (the line where PM declares its local `MIN_ESCAPE_SLIPS` C++ constant; registry → C++ propagation is a code-review responsibility, NOT an automated pipeline as of R11a): `static_assert(MIN_ESCAPE_SLIPS == 2, "PM Hardware Contract math assumes MIN_ESCAPE_SLIPS=2; if registry changes this value, update the PM compile unit's local constant and re-derive the F-BARRAGE-SURVIVABILITY-INVARIANT frame-quantized table")` | Compile-time only — the LOCAL constant cannot drift at runtime; if the local constant is edited to a non-2 value, recompile fails. **Known gap**: registry → local C++ drift is NOT auto-caught (registry value can change to 3 with PM's local constant still 2 → static_assert still passes; PM uses stale 2); closure path = OQ-7 yaml→C++ generator pipeline. Registry remains the source of truth for design intent; local mirror is the implementation. | AC-SS-E (R11a-17 REFRAMED — tests local static_assert; was pre-R11a yaml-generator-dependent) |
| Hardware Contract DT watchdog (R10a §1.3 — defined in this subsection above) | `ensureMsgf(WatchdogRollingWindow.IsValid(), ...)` at first watchdog tick | Action on watchdog breach is §5.2 = (a) survivability margin relaxation: Wave Spawner gate suppresses M=3 PEAK barrage class. Player-facing banner per §5.3 = (a) (see `player-movement-presentation.md` § Hardware-Performance Banner). See Hardware Contract subsection above for full spec. | AC-HW-A / AC-HW-B / AC-HW-C (see §8 Acceptance Criteria) |

**AC-21 fold-in (R10a §1.2 update — replaces one-shot `bFloorGuardFired` semantic with persistent clamp per the enforcement table SLIP_TWEEN row):** AC-21's pre-R10a body asserted that the floor guard fires once and SLIP_TWEEN_DURATION_S is reset to default 0.15. Under the §1.2 policy, the floor guard is now a persistent clamp — every tick where SLIP_TWEEN is outside [0.10, 0.15] gets clamped to the nearest bound, NOT reset to 0.15. The rate-limited log (1 per 600 ticks) replaces the one-shot flag. AC-21's pre-R10a wording remains valid for the FIRST violation (no crash, no divide-by-zero) but the post-R10a AC additionally asserts: (a) on the 601st violating tick (10 s of sustained violation), a second `Error` log fires; (b) the clamp persists across 100 consecutive violating ticks (no silent bypass); (c) `bSlipTweenClampActive` reads `true` for the duration. See AC-SS-A in §8 Acceptance Criteria.

**Cross-sub-GDD forward contract on `player-movement-mechanics.md`**: the F-2 prologue clamp pseudo-code (the implementation of this row of the enforcement table) lives in `player-movement-mechanics.md` §4 Formulas → F-2. The cross-doc invariant is: mechanics owns the F-2 implementation of the clamp; platform owns the enforcement contract (this table) + the AC family (AC-21, AC-SS-A through E) that verifies it. Any future revision to either side must update both files in lock-step — see §6 Dependencies → cross-sub-GDD forward contracts.

## 4. Formulas

Platform owns four named formula contracts:

- **F-PROLOGUE** — the TickComponent prologue that defines `raw_dt` (unclamped) and `effective_dt` (clamped) once per tick (B-F6-3 closure; consumed by §3 watchdog AND by mechanics §4 F-2 and F-6).
- **F-WATCHDOG-ROLLING-BUFFER** — the 60-tick rolling sample buffer math (sustained sub-55 detection, hitch cluster detection, hysteresis-release accumulator). The pseudo-code lives in §3 Hardware Contract > "Runtime DT watchdog specification"; §4 names the contract and the failure-mode set it defends against (no duplication).
- **F-BARRAGE-SURVIVABILITY-INVARIANT-CORNERS** — the per-framerate × SLIP_TWEEN safe-range sensitivity matrix that proves the invariant `SLIP_TWEEN_DURATION_S × MIN_ESCAPE_SLIPS + REACTION_BUDGET ≤ TELEGRAPH_WINDOW_FLOOR_S` holds beyond §3's default-tuning row at every corner a designer can legally reach.
- **F-HW-B-MEASUREMENT** — the raw `FApp::GetDeltaTime()` profiler measurement methodology AC-HW-B uses (R12a DR-PLAT-HW-B option (a) baseline assumption; B-PERF-1 closure pending).

Mechanical formulas the platform layer consumes (not redefines): **F-2** Tween Progress Accumulator and **F-6** Edge-Absorb Tail Phase timer — both lift `effective_dt` from F-PROLOGUE below. These live in `player-movement-mechanics.md` §4.

### F-PROLOGUE — TickComponent prologue: `raw_dt` + `effective_dt` (B-F6-3 closure; CANONICAL)

PM's `TickComponent` body runs a single-source DT prologue BEFORE the SLIPPING-gate (F-2 path), BEFORE the F-6 edge-absorb timer advance, AND BEFORE the watchdog sample push. The prologue defines two variables consumed by all three downstream sites:

```cpp
// PM::TickComponent — first statements, before any state-gate branches.
// B-F6-3 closure: single source of truth for both the engine-DT watchdog and the
// gameplay-DT tween advance. Reads the engine static once per tick; downstream
// sites consume the variables, not the static.
const float raw_dt       = FApp::GetDeltaTime();
const float effective_dt = FMath::Clamp(raw_dt, 0.0f, MAX_SLIP_DT_S);
```

**Variables**:

- `raw_dt` — the engine's last-frame DT in seconds, sampled once. **Unclamped**; fed to the §3 watchdog rolling buffer so the watchdog can detect hitch samples above `MAX_SLIP_DT_S` (the §3 secondary breach `MaxSampleDT > 0.033f` condition depends on raw values — a clamped feed would mask the very hitches the watchdog is designed to detect). Type `float`. Lifetime: per-tick local.
- `effective_dt` — the gameplay-DT consumed by mechanics F-2 (TweenProgress accumulator) and F-6 (edge_absorb_local_timer advance). **Clamped** to `[0.0, MAX_SLIP_DT_S]`. Type `float`. Lifetime: per-tick local. **Mechanics consumes; mechanics does not redefine.** See `player-movement-mechanics.md` §4 F-2 + F-6.
- `MAX_SLIP_DT_S` — owned by `player-movement-mechanics.md` §7 (default 0.05 s; declared as singleton safe range because the tween-skip-prevention math is calibrated to this exact bound — broadening it requires re-deriving F-2 worst-case progress per tick). Platform consumes this knob; does not redefine.

**DT source rationale (load-bearing — closes the three-candidate ambiguity that pre-PASS-7 prose left unresolved)**: three named candidates existed in pre-PASS-7 prose — `TickComponent`'s `DeltaTime` parameter (mechanics F-2 pseudo-code line 549 pre-PASS-7), `FApp::GetDeltaTime()` (§3 watchdog line 126), and `GetWorld()->GetDeltaSeconds()` (pre-PASS-7 platform §1 Overview + session-state resume pointer). The PASS 7 reconciliation chooses **`FApp::GetDeltaTime()`** on three grounds:

1. **Single-source invariant.** Both the watchdog (hardware contract) and the tween advance (gameplay) read the same value, so no "watchdog says 50 fps; tween says 60 fps" divergence is reachable from a future world-time-dilation feature. The watchdog block in §3 consumes `raw_dt` from this prologue (PASS 7 cascade edit) rather than re-sampling the static — one engine-static read per tick.
2. **Hardware-semantic correctness.** `FApp::GetDeltaTime()` is the engine's raw frame DT, ignoring `WorldTimeDilation` and `ActorTimeDilation`. If SLIPSTORM ever introduces a slow-motion debug mode, replay scrubbing, or any feature that adjusts world time, the watchdog must continue to read RAW hardware DT or it generates false-positive breach signals on a perfectly-performing device that has been deliberately slowed. The `DeltaTime` parameter and `GetWorld()->GetDeltaSeconds()` are both world-dilated and would not be forward-safe.
3. **Equivalence in SLIPSTORM today.** SLIPSTORM has no time-dilation system in its current GDD set, so all three candidates are arithmetically equivalent at this scope. The `FApp::GetDeltaTime()` choice is the only one that remains correct if a future dilation feature is added — the same arithmetic, the same downstream values, with the failure-mode protection intact.

The `GetWorld()->GetDeltaSeconds()` candidate that pre-PASS-7 `§1 Overview` inscribed is **superseded** by this section; §1 Overview is amended in PASS 7's cascade to align (single-source consistency).

**Cross-references** (PASS 7 cascade edit sites — all updated in lock-step with this section):

- `player-movement-mechanics.md` §4 F-2 — consumes `effective_dt` from this prologue. The pre-PASS-7 local clamp `effective_dt = clamp(DeltaTime, 0.0f, MAX_SLIP_DT_S)` is RETIRED; F-2's pseudo-code now reads the upstream variable.
- `player-movement-mechanics.md` §4 F-6 — `edge_absorb_local_timer += effective_dt` consumes the prologue's clamped value (no in-section recomputation; the F-2 / F-6 "same clamp semantics" comment is rewritten to "shared upstream definition").
- §3 Hardware Contract > "Runtime DT watchdog specification" pseudo-code — the standalone `const float CurrentDT = FApp::GetDeltaTime()` declaration is amended to `const float CurrentDT = raw_dt` (consumes the prologue variable; no double-read of the engine static per tick).
- §3 Hardware Contract spec table > "Sample location" row — amended to reference §4 F-PROLOGUE.
- §1 Overview prologue declaration — the pre-PASS-7 `effective_dt = min(GetWorld()->GetDeltaSeconds(), MAX_SLIP_DT_S)` is amended to `effective_dt = clamp(FApp::GetDeltaTime(), 0.0f, MAX_SLIP_DT_S)` to match this section.

**Example (normal tick, 60 fps clean)**: `FApp::GetDeltaTime() = 0.01667`, `MAX_SLIP_DT_S = 0.05`: `raw_dt = 0.01667`; `effective_dt = 0.01667` (no clamp). Watchdog samples `raw_dt = 0.01667` into ring buffer; F-2 reads `effective_dt = 0.01667` for `TweenProgress` advance.

**Example (hitched frame)**: `FApp::GetDeltaTime() = 2.0` (engine paused for 2 s on a load), `MAX_SLIP_DT_S = 0.05`: `raw_dt = 2.0`; `effective_dt = 0.05`. Watchdog samples `raw_dt = 2.0` into ring buffer (preserves hitch for `MaxSampleDT > 0.033f` breach detection); F-2 reads `effective_dt = 0.05` (no single-frame tween skip). The single-source invariant is held: the watchdog sees the raw hitch; the tween sees the clamped advance; both are consistent.

### F-WATCHDOG-ROLLING-BUFFER — 60-tick sample buffer math (named contract; pseudo-code lives in §3)

The DT watchdog rolling-buffer math is fully defined as pseudo-code in §3 Hardware Contract > "Runtime DT watchdog specification" subsection (60-tick ring buffer; sentinel pre-fill at BeginPlay per R11a-6; sustained-sub-55 breach trigger at `≥ 30/60 samples > 18.18 ms` per R11a-7; hitch-cluster trigger at `≥ 18/60 samples > 16.67 ms AND any single sample > 33 ms`; 3.0 s continuous-clean hysteresis-release per R10a §1.3). §4 names the contract identity and tabulates the failure-mode set the math defends against — so cross-document references (e.g., AC-HW-A Setup A-G in §8) can target the formula name.

| Failure mode | Watchdog field | Threshold (R11a-6/7 binding) |
|---|---|---|
| Sustained sub-55 fps (≥ 1 s wall-clock at degraded rate) | `SamplesAbove18ms` | `≥ 30/60` samples with `DT > 0.01818s` |
| Hitch-cluster storm (many small + one large) | `SamplesAbove16ms` AND `MaxSampleDT` | `≥ 18/60` samples with `DT > 0.01667s` AND any single sample `> 0.033s` |
| First-second-after-BeginPlay false positive | sentinel pre-fill (R11a-6 BeginPlay init) | all 60 slots `= 0.01667f` immediately after BeginPlay (asserted by AC-HW-A Setup G Part 1) |
| Flap between gated and ungated PEAK | `ContinuousCleanWindowTime` | `≥ 3.0 s` of continuous all-60-clean samples before release |
| In-flight M=3 PEAK at breach-onset gap | (NOT a watchdog field — Wave Spawner-side grace timer per R11a-8) | 3.0 s grace window from `OnHardwarePerformanceBreach(true)` broadcast |

The canonical reference code-review verifies is the pseudo-code at §3 lines 113-163. §4 here names the contract identity.

### F-BARRAGE-SURVIVABILITY-INVARIANT-CORNERS — sensitivity matrix across SLIP_TWEEN safe-range × framerate corners

The §3 Hardware Contract > "Survivability invariant" subsection (lines 66-86) tabulates the invariant `SLIP_TWEEN_DURATION_S × MIN_ESCAPE_SLIPS + REACTION_BUDGET ≤ TELEGRAPH_WINDOW_FLOOR_S` at the **default** tuning (`SLIP_TWEEN = 0.15 s`, `MIN_ESCAPE_SLIPS = 2`, `REACTION_BUDGET = 0.20 s`, `TELEGRAPH_WINDOW_FLOOR_S = 0.70 s` per DPC R10d binding 2026-06-17) across 4 framerates. §4 extends that table across the full SLIP_TWEEN safe-range × broader framerate corners, proving the invariant holds anywhere a designer can legally tune SLIP_TWEEN without re-opening the Hardware Contract.

**Variables** (all owned by mechanics §7 unless noted):

- `SLIP_TWEEN_DURATION_S` safe range — `[0.10, 0.15]` s (mechanics §7).
- `MIN_ESCAPE_SLIPS = 2` (entities.yaml registry; mirrored to PM compile unit via `static_assert` per §3 Shipping-Safety table > MIN_ESCAPE_SLIPS row).
- `REACTION_BUDGET = 0.20 s` (locked PM design constant — not a tuning knob).
- `TELEGRAPH_WINDOW_FLOOR_S = 0.70 s` (DPC GDD §7 owns under R10d CD ruling path (i) 2026-06-17 / R11a-arithmetic propagation 2026-06-18; `/propagate-design-change` 2026-06-19; PM forward-contract consumer per `design/gdd/reviews/cross-system-survivability-coordination-2026-06-11.md` R10d resolution append).
- `MAX_SLIP_DT_S = 0.05 s` (mechanics §7 — singleton safe range; corners reduce to one row per framerate).
- Framerate corners — 60 fps (target), 30 fps (watchdog breach band), 20 fps (`= 1 / MAX_SLIP_DT_S` clamp ceiling), 120 fps (high-refresh future).

**Sensitivity matrix** (full SLIP_TWEEN × framerate corners):

| Framerate | DT | SLIP_TWEEN | Ticks per tween | Wall-clock per tween | × MIN_ESCAPE_SLIPS=2 | + REACTION_BUDGET | vs FLOOR=0.70 s | Margin |
|---|---|---|---|---|---|---|---|---|
| 60 fps | 0.01667 s | 0.10 (floor) | ⌈0.10/0.01667⌉ = 6 | 6 × 0.01667 = 0.100 s | 0.200 s | 0.400 s | ✓ holds | 300 ms |
| 60 fps | 0.01667 s | 0.15 (ceiling = default) | ⌈0.15/0.01667⌉ = 9 | 9 × 0.01667 = 0.150 s | 0.300 s | 0.500 s | ✓ holds | 200 ms |
| 30 fps | 0.03333 s | 0.10 | ⌈0.10/0.03333⌉ = 3 | 3 × 0.03333 = 0.100 s | 0.200 s | 0.400 s | ✓ holds | 300 ms |
| 30 fps | 0.03333 s | 0.15 | ⌈0.15/0.03333⌉ = 5 | 5 × 0.03333 = 0.167 s | 0.333 s | 0.533 s | ✓ holds | 167 ms |
| 20 fps (= MAX_SLIP_DT_S ceiling) | 0.05000 s | 0.10 | ⌈0.10/0.05⌉ = 2 | 2 × 0.05 = 0.100 s | 0.200 s | 0.400 s | ✓ holds | 300 ms |
| 20 fps | 0.05000 s | 0.15 | ⌈0.15/0.05⌉ = 3 | 3 × 0.05 = 0.150 s | 0.300 s | 0.500 s | ✓ holds | 200 ms |
| 120 fps | 0.00833 s | 0.10 | ⌈0.10/0.00833⌉ = 12 | 12 × 0.00833 = 0.100 s | 0.200 s | 0.400 s | ✓ holds | 300 ms |
| 120 fps | 0.00833 s | 0.15 | ⌈0.15/0.00833⌉ = 18 | 18 × 0.00833 = 0.150 s | 0.300 s | 0.500 s | ✓ holds | 200 ms |

**Worst-case across corners**: minimum margin is **167 ms** at `(30 fps × SLIP_TWEEN = 0.15)`. Every corner clears `FLOOR=0.70 s` by ≥ 167 ms. The default tuning's 200 ms margin (60 fps × 0.15) is preserved as the central case. (Pre-PASS-7 PM monolith arithmetic carried a stale `FLOOR=0.65 s` baseline that produced 117 ms worst-case + 150 ms default-tuning margins; PASS 7 cascade-corrects against the R10d-bound canonical FLOOR=0.70 s.)

**Why corners proved** (sensitivity analysis):

- SLIP_TWEEN's floor 0.10 gives **more** margin than the ceiling 0.15 at every framerate — shorter tween → less wall-clock to complete MIN_ESCAPE_SLIPS escapes. So the ceiling row is the worst case at each framerate.
- MAX_SLIP_DT_S is singleton (not a range), so its "corner" reduces to one row per framerate. The 20 fps row exercises the clamp ceiling — i.e., the worst legal DT a tick can present to F-2 / F-6 under F-PROLOGUE clamping.
- The 30 fps row is the worst-margin row (167 ms) because `⌈0.15/0.03333⌉ = 5 ticks` produces 0.167 s wall-clock — 17 ms over nominal — while 60 fps and 20 fps both produce exactly 0.150 s wall-clock by integer-tick alignment with `0.15 s`.
- The 120 fps row is included for future high-refresh device support; the invariant holds with the same 200 ms margin as 60 fps (the `⌈⌉` ceiling produces an integer-tick alignment because `0.15 / 0.00833 = 18` cleanly).

**Re-derivation trigger condition**: the matrix above MUST be re-derived if any of `{SLIP_TWEEN safe-range bounds, MIN_ESCAPE_SLIPS, REACTION_BUDGET, TELEGRAPH_WINDOW_FLOOR_S, MAX_SLIP_DT_S}` changes. The `static_assert` in §3 Shipping-Safety > MIN_ESCAPE_SLIPS row catches the registry-vs-PM-local-constant drift for `MIN_ESCAPE_SLIPS` at compile time; the other variables are flagged by code-review per the Shipping-Safety Enforcement Policy. If DPC's `TELEGRAPH_WINDOW_FLOOR_S` is ever revised below 0.70 s, the 30 fps × 0.15 row's 167 ms margin is the first to compress and `/propagate-design-change` MUST fire to re-derive this matrix (precedent: the R10d 0.65 → 0.70 raise on 2026-06-17 propagated to DPC + Pull-Wave + entities.yaml on 2026-06-19 but did NOT update the PM monolith — PASS 7 cascade-corrected the stale baseline this matrix inherited from Step 2's §3 lift).

### F-HW-B-MEASUREMENT — AC-HW-B raw-log measurement methodology (R12a DR-PLAT-HW-B options (a)/(c); B-PERF-1 closure pending)

AC-HW-B verifies sustained 60 fps under PEAK density on min-spec devices via per-frame DT sampling. The R12a author decision **DR-PLAT-HW-B** will choose between three measurement options:

- **(a) raw `FApp::GetDeltaTime()` log** — write each frame's raw DT to a CSV at run-end; post-process to verify the three pass criteria (≥ 99.9 % of frames with `DT ≤ 16.67 ms + 0.5 ms`; no single frame with `DT > 33.33 ms`; `is_hw_performance_degraded == false` throughout). Lowest profiler-noise-floor footprint. Simplest test apparatus.
- **(b) Unreal Insights frame timing capture** — use the engine's built-in Insights tooling; verify the same three criteria against the captured trace. Higher footprint (Insights itself perturbs frame timing slightly, hence the 0.5 ms tolerance budget below).
- **(c) Hybrid (a) + (b)** — raw log for the assertion (low-noise), Insights for the post-hoc human investigation of failed runs (high-context). Acknowledged as the expected R12a outcome per architecture-review pre-flag.

**Pre-R12a fallback (this section ships with option (a) as the baseline assumption until DR-PLAT-HW-B is authored)**: raw `FApp::GetDeltaTime()` written to a CSV at run-end. §4 here names the measurement *apparatus* (raw-log CSV with the prologue's `raw_dt` as the sampled value); the assertion arithmetic is the three orthogonal pass criteria stated in §8 AC-HW-B.

**Noise-floor budget**: AC-HW-B pass criterion (1) "DT ≤ 16.67 ms + 0.5 ms tolerance" reserves 0.5 ms for measurement-apparatus jitter. This budget covers:

- **CSV write contention**: one `fwrite` per frame at 60 fps × 4 bytes per float = 240 B/s, well below the disk-write noise floor on iOS / Android.
- **Engine-tick clock-source jitter**: `FApp::GetDeltaTime()` derives from `FPlatformTime::Cycles64()` which is monotonic with sub-microsecond precision on both iOS (mach_absolute_time) and Android (clock_gettime CLOCK_MONOTONIC).
- **Unreal Insights overhead**: ~0.3-0.5 ms acknowledged on mobile if hybrid (c) is the R12a outcome; covered by the budget.

**Cross-reference**: §8 AC-HW-B names the three pass criteria the measurement supports. The DT source is THIS section's apparatus (`raw_dt` from F-PROLOGUE = raw `FApp::GetDeltaTime()`); the assertions are AC-HW-B's three orthogonal criteria.

---

### Mechanical formulas consumed (not redefined) by platform

| Formula | Owned by | Consumed how |
|---|---|---|
| F-2 — Tween Progress Accumulator | `player-movement-mechanics.md` §4 | F-PROLOGUE produces `effective_dt`; F-2 reads it for `TweenProgress += effective_dt / effective_slip_tween`. The pre-PASS-7 local clamp at F-2's pseudo-code line is RETIRED. |
| F-6 — Edge-Absorb Tail Phase timer | `player-movement-mechanics.md` §4 | F-PROLOGUE produces `effective_dt`; F-6 reads it for `edge_absorb_local_timer += effective_dt`. The "same clamp semantics as F-2" comment is rewritten to "shared upstream prologue definition". |

## 5. Edge Cases

Platform owns hardware-tier + OS-tier + watchdog edge cases. Mechanical ECs (lane / tween / lean / edge-absorb) live in `player-movement-mechanics.md` §5 (EC-1–EC-15). Presentation ECs (audio overlap / cadence-cap / haptic-OS-state-gate call site) live in `player-movement-presentation.md` §5 (EC-16–EC-22).

**EC-23 — App background / foreground transition (iOS Control Center / app switcher / Home button / iOS phone call / Android task switcher / Android Recents).**
The OS suspends SLIPSTORM's process. The UE engine stops ticking. `FApp::GetDeltaTime()` retains its last pre-suspension value while the process is suspended. RSM Rule 19 sets `is_paused = true` on the app-lifecycle-suspend hook and PM's logic-layer pause-gate freezes F-2 / F-6 advance independently of any DT-source semantics. On foreground resume: the first `TickComponent` invocation may carry a `DeltaTime` (and `FApp::GetDeltaTime()` value) reflecting the suspension duration (possibly multiple seconds). F-PROLOGUE (§4) clamps `effective_dt = clamp(raw_dt, 0.0f, MAX_SLIP_DT_S)` to 0.05 s — F-2 / F-6 see a single ≤ 50 ms tween advance, not a multi-second skip. The watchdog buffer ingests one large `raw_dt` sample (potentially several seconds) at the post-resume ring index. Spurious-breach prevention: secondary breach (hitch cluster) requires `SamplesAbove16ms ≥ 18` AND `MaxSampleDT > 0.033f` jointly — a single huge sample plus 59 pre-suspension clean samples yields `SamplesAbove16ms = 1 < 18`, so no breach fires. Primary breach (sustained sub-55) requires `SamplesAbove18ms ≥ 30` — also held. The huge sample rotates out of the 60-slot ring within 60 subsequent ticks; the buffer fully self-heals during normal post-resume play. RSM's resume_grace window (Rule 19; `RESUME_GRACE_S = 0.5 s`) continues to freeze F-2 / F-6 advance for the first 0.5 s after `is_paused → false`. No PM-side suspension hook is required; the platform layer relies on RSM's lifecycle integration.

**EC-24 — iOS Focus mode (Work / Sleep / Driving / Do Not Disturb) / Android DND active during play.**
The OS suppresses notification delivery and may suppress system haptics depending on the user's Focus configuration. Platform-side impact on PM Tick + watchdog: NONE — Focus and DND do not suspend the app process and do not alter engine tick cadence. The haptic OS-state gate is the only PM dispatch path that consults Focus / DND state; that gate is a B-CERT-2 author decision owned by `player-movement-presentation.md` §5 EC-19 (DR-PRES-HAPTIC-GATE call-site) + platform's `IHapticDispatch::IsSystemHapticsEnabled()` query API (`player-movement-platform.md` §3 Public Interface (Platform-Side) — R12a-pending). When the haptic gate evaluates `false`, PM haptic dispatch is suppressed but visual + audio cues continue normally. The Hardware-Performance Banner and watchdog operate identically regardless of Focus / DND state. No additional platform-layer logic is required.

**EC-25 — iOS Low Power Mode / Android Battery Saver active during play.**
The OS imposes a system-wide framerate cap (typically 30 fps on iOS Low Power Mode; varies by Android OEM but commonly 30 or 45 fps). The engine ticks at the OS-capped rate; `FApp::GetDeltaTime()` returns ~0.0333 s per tick on the iOS 30 fps cap. F-PROLOGUE clamps F-2 / F-6 normally (DT < `MAX_SLIP_DT_S = 0.05 s`, no clamp engagement). The watchdog interprets sustained 30 fps as a Hardware Contract breach — `SamplesAbove18ms ≥ 30/60` triggers within ~1 s of sustained 30 fps play; `OnHardwarePerformanceBreach(true)` fires; banner appears with the R11a-9 copy; Wave Spawner M=3 PEAK gate engages with 3.0 s grace per R11a-8. **This is correct design behavior** — Low Power Mode on a device that would otherwise run at 60 fps is, from the player's perspective, a hardware-degradation state, and the Survivability Margin Relaxation (§5.2 (a)) responds identically to the OS-imposed cap and a thermal-degradation cap. The banner discloses the gameplay change honestly per the §5.3 (a) Disclosure contract. The F-BARRAGE-SURVIVABILITY-INVARIANT-CORNERS matrix (§4) verifies the invariant still holds at 30 fps with 167 ms margin at SLIP_TWEEN=0.15 ceiling — the run remains survivable. Players who do not want the banner / M=3 PEAK gate can disable Low Power Mode / Battery Saver.

**EC-26 — Sustained thermal throttle during a 60-second run (mid-tier or sub-min-spec device).**
A min-spec or sub-min-spec device runs SLIPSTORM at peak load; the SoC's thermal envelope (≈ 4 W package power on a typical mobile platform) is exceeded around 45–60 s into sustained PEAK density play; iOS / Android thermal management throttles the CPU / GPU clock to maintain operating temperature; engine framerate degrades from 60 → 50 → 45 → 30 fps over a 5–10 s thermal-throttle ramp. F-PROLOGUE samples the degrading DT each tick; watchdog rolling buffer accumulates `SamplesAbove18ms`; sustained sub-55 breach (`≥ 30/60`) fires once `~30` ticks (~1 s) of sub-55 samples have accumulated. `OnHardwarePerformanceBreach(true)` broadcasts; banner appears; Wave Spawner M=3 PEAK gate engages with 3.0 s grace; HUD renders the Performance Banner. The §5.2 (a) Survivability Margin Relaxation engages while the player completes the remaining run on the degraded device. Hysteresis-release (`ContinuousCleanWindowTime ≥ 3.0 s` of all-60-clean samples) does NOT fire unless the device cools (typically requires backgrounding or run-end). The thermal-throttle EC is the canonical Hardware Contract trigger — every Hardware Contract decision is calibrated against this scenario. AC-HW-B Polish-phase device audit (§8) validates this EC by running 60 s under PEAK density and asserting `is_hw_performance_degraded == false` throughout the session on the min-spec device — failure → tighten min-spec.

**EC-27 — In-flight M=3 PEAK barrage at the moment of watchdog breach onset (R11a §3 DR-B.3 3.0 s grace-window closure).**
At t=0, Wave Spawner has dispatched an M=3 PEAK barrage; the three telegraphs are mid-flight (telegraph window 0.70 s; arrival latency ~1.5–2.0 s); at t=+0.5 s, watchdog breach fires (`OnHardwarePerformanceBreach(true)`); player has ≤ 1.5 s remaining to escape the in-flight M=3 PEAK on a degraded device. **Per R11a §3 DR-B.3 = (b)**: the in-flight M=3 PEAK is permitted to complete its natural lifecycle (NOT mid-flight-cancelled); the 3.0 s post-breach grace window starts at the moment of the `bEntering=true` broadcast and forbids any *new* M=3 PEAK dispatch for 3.0 s wall-clock, regardless of how `is_hw_performance_degraded` is subsequently sampled within that window. The grace window covers the maximum plausible M=3 PEAK dispatch-to-arrival latency at degraded framerate (~2.0–3.0 s at 50 fps including telegraph window + arrival travel time) plus matches the hysteresis-release window for symmetry. By 3.0 s post-breach, all M=3 PEAK barrages dispatched before breach onset have arrived and resolved (or killed the player). The §3 Hardware Contract > "Cross-system forward contracts" subsection codifies this as a BINDING contract on `wave-spawner-pattern-library.md`. Survivability promise: the player on a degraded device faces ONE final M=3 PEAK (the in-flight one) under suboptimal performance, then only M=2 PEAK class for the remainder of the run. The Wave Spawner gate AND the grace window together close the in-flight gap.

**EC-28 — Watchdog cold-start: first 1 s after BeginPlay before the rolling buffer is fully populated with live samples (R11a §3 B.1 NEW — sentinel pre-fill mitigation).**
At BeginPlay, the watchdog ring buffer is pre-filled with `0.01667f × 60` (nominal 60 fps clean sample); `TickDTRingIndex = 0`; `ContinuousCleanWindowTime = 0.0f`; `bHardwarePerformanceBreachActive = false`. The first 60 TickComponent invocations (1 s at 60 fps; up to 3 s at degraded framerate) write live samples into the ring, displacing the sentinels one-by-one. Under the current strict-comparison `>` breach math (`> 0.01818f` and `> 0.01667f`), the sentinel value 0.01667 evaluates `> 0.01667` as FALSE (and `> 0.01818` as FALSE), so the sentinel is semantically equivalent to a clean live sample for breach-detection purposes. AC-HW-A Setup G (§8) asserts: (Part 1) immediately after BeginPlay and before any TickComponent fires, `TickDTRollingBuffer[i] == 0.01667f` for all 60 slots; (Part 2) startup-onset degradation reaches breach within 30 frames (`SamplesAbove18ms ≥ 30` at t=30 frames into degraded play). The sentinel pre-fill is a code-clarity invariant that also makes `ContinuousCleanWindowTime` accumulation defensible at startup (the buffer claims clean data, so the release-path accumulator can start from t=0 without surprising a debug reader). Without the sentinel, the buffer's zero-init under the current strict-`>` regime produces identical runtime semantics — but a future refactor to inclusive `>=` comparisons, moving-average math, or a `TickDTSampleCount` gate would silently fail without the sentinel.

**EC-29 — 60 → 30 fps step-throttle during an in-flight slip tween (cross-`MAX_SLIP_DT_S`-bound DT crossing).**
A device sustains 60 fps for the first 8 ticks of a SLIPPING tween (TweenProgress reaches ~0.89 at SLIP_TWEEN=0.15); on tick 9, OS thermal throttle kicks in and `FApp::GetDeltaTime()` jumps from 0.01667 s to 0.0333 s (a single-tick framerate halving). F-PROLOGUE clamps unchanged (DT < `MAX_SLIP_DT_S = 0.05 s`, no clamp engagement); F-2 advances `TweenProgress += 0.0333 / 0.15 = 0.222` — large step but bounded; `TweenProgress` reaches 1.0 in 1 additional tick; tween completes in 10 total ticks (vs the nominal 9) with wall-clock 8 × 0.01667 + 1 × 0.0333 = 0.167 s (= the 30 fps × 0.15 row of the §4 F-BARRAGE-SURVIVABILITY-INVARIANT-CORNERS matrix). The invariant still holds at the 167 ms worst-case margin against FLOOR=0.70s. Watchdog sees one sample at 0.0333 s on tick 9; secondary breach requires `SamplesAbove16ms ≥ 18` so single-step-throttle does NOT breach. If the 30 fps regime is sustained, the watchdog breach + Wave Spawner M=3 PEAK gate engage within ~1 s per EC-26 normal flow. The single in-flight tween is unaffected by the gate engagement (gate applies to *new* barrage dispatches, NOT in-flight tween advances). **No PM-side step-throttle handling is required** — F-PROLOGUE's `MAX_SLIP_DT_S` clamp + the watchdog's compound-condition breach math jointly cover this scenario.

**EC-30 — [R12a-PENDING — B-QA-1] AC-21 tick-601 log-fire verifiability.**
AC-21 asserts that under sustained SLIP_TWEEN safe-range violation, "the first log fires on tick 1 and the second log fires on tick 601 (per the rate-limited log per AC-SS-A)." The pre-R12a F-2 prologue pseudo-code (mechanics §4) uses a `TickClampLogCounter = (TickClampLogCounter + 1) % 600` idiom with a log-on-counter-zero gate (`if (TickClampLogCounter == 0) { log; }`). The verifiability concern: under standard automated unit-test fixtures, advancing a single `TickComponent` 601 times and asserting exactly two log events fire is a long-running test (~10 s wall-clock if real-time, or ~10 ms if mocked clock). The R12a author decision (B-QA-1) closes the verifiability path: (a) **standardize on a mocked-clock test harness** that injects 601 TickComponent calls without wall-clock delay — preserves the existing tick-601 semantic AND honors the 4-site lockstep contract on tick-601 (see lockstep note below); (b) **re-author the log idiom to count violations rather than ticks** (e.g., 1 log per 600 violations regardless of tick interleave) — semantically equivalent at sustained-violation steady state; would require the 4-site lockstep update only if assertion language changes from "tick 601" to "violation 601". Options (a) and (b) preserve the existing tick-601 assertion language; option (a) is the lower-risk path. **Pre-R12a fallback behavior**: AC-21 and AC-SS-A assert the tick-601 semantic; the F-2 pseudo-code matches the assertion; verification methodology is "mocked-clock harness, 601 ticks injected synchronously". B-QA-1 closure path: qa-lead authors the explicit harness pattern in `tests/automation-cpp/` and the AC-21 fixture references it.

**4-site lockstep note (R10a §1.2 enforcement-table lockstep rule — DO NOT REVISE TICK-601 IN ISOLATION)**: the tick-601 assertion is inscribed in four lock-stepped locations per the R10a §1.2 enforcement-table policy: (1) `player-movement-mechanics.md` §4 F-2 pseudo-code comment + documented-deviation prose; (2) `player-movement-platform.md` §3 Shipping-Safety Enforcement Policy table > SLIP_TWEEN row > AC-21 fold-in prose; (3) `player-movement-platform.md` §8 AC-21 body; (4) AC-SS-A body (defined at the close of §3 Shipping-Safety Enforcement Policy). Any B-QA-1 closure that changes the assertion's tick threshold (i.e., changes "tick 601" to a different value) MUST update all four locations in lockstep — a closure path that touches only the F-2 prologue idiom without revising the AC-21 + AC-SS-A assertions silently breaks the contract. The pre-R10a F-2 `if (++TickClampLogCounter >= 600) { log; counter = 0; }` idiom suffered exactly this kind of internal-consistency contradiction (first log on tick 600, not tick 1 — see mechanics §4 F-2 "Documented deviation from R11a brief verbatim" paragraph at line 562); R10a Cluster A was the internal-contradiction cluster. Reviewers verify lockstep extension as part of design-review Phase 2 per the §3 Shipping-Safety policy paragraph 3.

**EC-31 — [R12a-PENDING — DR-PLAT-HW-B / B-PERF-1] AC-HW-B profiler noise floor cross-validation.**
AC-HW-B's pass criterion (1) "≥ 99.9 % of frames with `DT ≤ 16.67 ms + 0.5 ms`" budgets 0.5 ms for measurement-apparatus jitter. The B-PERF-1 concern: the 0.5 ms budget is documented in §4 F-HW-B-MEASUREMENT against three candidate noise-floor sources (CSV write contention; `FPlatformTime::Cycles64()` clock-source jitter; Unreal Insights overhead) but has NOT been empirically cross-validated on the named min-spec devices. If the actual noise floor on iPhone XR + Pixel 5 exceeds 0.5 ms under raw `FApp::GetDeltaTime()` log conditions, AC-HW-B fails with false-positive hitch-detection. R12a DR-PLAT-HW-B (TD + performance-analyst co-sign) chooses the measurement methodology (a/b/c — see §4) AND widens or tightens the 0.5 ms tolerance based on a pre-Polish-phase calibration run on the named min-spec devices. **Pre-R12a fallback behavior**: §4 ships with option (a) + 0.5 ms tolerance; Polish-phase device audit (AC-HW-B) is BLOCKING and the calibration run is implicit in the audit setup; if the noise floor is empirically wider than 0.5 ms, the audit re-opens this EC and the tolerance value before AC-HW-B can sign off.

**EC-32 — [R12a-PENDING — B-SHIP-1] Tick Ordering Option (b) `bManualTickEnabled` early-out under Shipping.**
The §3 Tick Ordering subsection documents Option (b): if RSM resolves to a `UWorldSubsystem` / `UGameInstanceSubsystem`, PM's `TickComponent` is called manually from RSM's `Tick` AND PM's own tick registration MUST be disabled (`PrimaryComponentTick.bCanEverTick = false` in BeginPlay). The Shipping-Safety policy AC-SS-C (§3 + §8) asserts: if `bManualTickEnabled == true` AND `PrimaryComponentTick.bCanEverTick != false`, the TickComponent prologue early-outs with a per-frame log at 1 Hz rate limit. The B-SHIP-1 concern: the AC-SS-C assertion mechanism in Shipping builds is currently an early-out + rate-limited log, but the failure-mode interaction with `OnHardwarePerformanceBreach` AND the F-PROLOGUE single-source invariant is not yet specified. If the early-out fires, F-PROLOGUE does NOT execute → no `raw_dt` / `effective_dt` defined for the tick → watchdog skips the sample → an Option (b)-broken Shipping build presents as a "watchdog never breaches" failure mode. R12a DR-PLAT-SHIP-1 (technical-director + unreal-specialist co-sign) closes this by specifying: whether the early-out path should still pump the watchdog (sampling raw `FApp::GetDeltaTime()` independently of F-PROLOGUE) OR whether the early-out path is intentionally a "no-watchdog" mode that escalates via a separate `LogPlayerMovement Fatal` in Shipping. **Pre-R12a fallback behavior**: AC-SS-C's early-out + per-frame log fires; watchdog does not sample on early-out ticks; the developer noticing the log catches the misconfiguration before it ships to production. The R12a closure tightens the Shipping semantic.

## 6. Dependencies

Platform consumes mechanics-layer values (SLIP_TWEEN safe range, MAX_SLIP_DT_S, ERunSlipState ordinals, HandleSlipTransition call-site registration) and produces watchdog broadcasts to Wave Spawner / HUD / presentation plus a haptic OS-state-gate API consumed by presentation. Platform has no upstream external GDD dependencies — the engine layer (UE 5.7 + iOS / Android OS) is platform's "upstream" but is documented as platform's substrate rather than as a sibling GDD.

### Upstream (systems platform depends on)

| System | What platform needs | Source |
|---|---|---|
| **`player-movement-mechanics.md`** | `SLIP_TWEEN_DURATION_S` safe range [0.10, 0.15] (consumed by §3 Shipping-Safety table + §4 F-BARRAGE-SURVIVABILITY-INVARIANT-CORNERS sensitivity matrix); `MAX_SLIP_DT_S` singleton (consumed by §4 F-PROLOGUE clamp); `ERunSlipState` ordinal contract (consumed by §3 Shipping-Safety table > AC-SS-B default-branch fallback row); `HandleSlipTransition` call site for engine-tick interleaving | mechanics §7 (knob declarations); mechanics §3 Movement State Enum (ordinal contract); mechanics §3 Slip Input Dispatch (HandleSlipTransition site) |
| **Run State Machine** (for Tick Ordering OQ-1 resolution + `is_paused` / `resume_grace` lifecycle integration) | RSM object type (UWorldSubsystem / UGameInstanceSubsystem / UActorComponent — affects Tick Ordering Option (a) vs (b) in §3); `OnPausedChanged` lifecycle integration for EC-23 (app background / foreground) | run-state-machine.md — Rule 19 (resume_grace contract); OQ-1 ADR pending (see mechanics §9 OQ-1) |
| **UE 5.7 engine** (substrate, not a sibling GDD) | `FApp::GetDeltaTime()` (canonical DT source per §4 F-PROLOGUE); `FPlatformTime::Cycles64()` (clock-source underlying `FApp::GetDeltaTime()`); `IFramePacer` (Polish-phase 60 fps target floor declaration per §3 Engine-specific implementation note); `AddTickPrerequisiteComponent` (Tick Ordering Option (a) registration); UE Mobile Application Lifecycle Events (EC-23 OS suspend / resume integration) | UE 5.7 engine reference: `docs/engine-reference/unreal/VERSION.md` + Misc/App.h + Misc/Parse.h |
| **iOS / Android OS** (substrate, not a sibling GDD) | iOS `safeAreaInsets.top` / Android `WindowInsetsCompat.Type.systemBars()` (banner safe-area binding per R11a-10 + AC-HW-C); iOS Low Power Mode / Android Battery Saver framerate cap signaling (EC-25 watchdog trigger path); thermal-throttle clock reduction (EC-26 watchdog trigger path); Focus / DND state (EC-24 — non-impactful at platform; haptic gate routed to presentation EC-19) | iOS HIG; Android accessibility guidance; UE Mobile platform abstraction layer |

### Downstream (systems that depend on platform)

| System | What they consume | Platform's obligation |
|---|---|---|
| **Wave Spawner System** (unauthored — BLOCKING forward contracts at first authoring per R10a §1.3 + R11a §3 DR-B.3) | `is_hw_performance_degraded` (per-dispatch-tick poll); `OnHardwarePerformanceBreach(bool bEntering)` (multicast subscription); 3.0 s post-breach grace window timer (Wave Spawner-side, started at `bEntering=true` broadcast) | Watchdog broadcast surface is API-stable as of R10a §1.3 + R11a §3; the M=3 PEAK suppression contract + 3.0 s grace window are codified in §3 Hardware Contract > Cross-system forward contracts subsection as BINDING on `wave-spawner-pattern-library.md` first authoring. Wave Spawner inherits these contracts on first authoring per the precedent that Pull-Wave R7 forward-contracted on PM |
| **HUD GDD** (unauthored — BLOCKING forward contracts at first authoring per R10a §1.3 + R11a §5 DR-D.1 + DR-D.2) | `OnHardwarePerformanceBreach` subscription for banner trigger; banner safe-area binding (top inset on iOS / Android per R11a-10); R11a-9 locked copy "Performance mode — hardest barrage suppressed." (or a HUD-renegotiated alternative if DR-D.1 vocabulary contract is not honored — see Bidirectional Notes below) | Banner trigger + safe-area binding contracts are API-stable; banner styling + rendering live in HUD GDD authoring; co-sign required at Polish gate for vocabulary verification |
| **`player-movement-presentation.md`** | `is_hw_performance_degraded` + `OnHardwarePerformanceBreach` (consumed for Hardware-Performance Banner trigger / release rendering — see presentation §3 Hardware-Performance Banner); `IHapticDispatch::IsSystemHapticsEnabled()` (consumed at every PM haptic dispatch site per B-CERT-2 / DR-PRES-HAPTIC-GATE — R12a-pending); `IGameSettings::IsCommitmentTellFlashEnabled()` (consumed at the `LeadingFaceFlash` dispatch site per B-CERT-1 / DR-PRES-FLASH — R12a-pending) | Public API surface is platform-owned; presentation owns the call-site gate-check. R12a-pending APIs (`IsSystemHapticsEnabled` / `IsCommitmentTellFlashEnabled`) become BLOCKING on platform's first revision after R12a APPROVED |
| **`player-movement-mechanics.md`** | `effective_dt` from §4 F-PROLOGUE (consumed by F-2 / F-6 — B-F6-3 CLOSED at PASS 7); `is_hw_performance_degraded` gating context for `current_lane` + `movement_state` reads (mechanics §3 Cross-System Interface Table) | F-PROLOGUE is the canonical definition; mechanics reads, never redefines (PASS 7 cascade) |
| **Input System GDD** (unauthored — informational forward contract; the haptic vocabulary surface is co-owned per `player-movement-presentation.md` §6 Bidirectional Notes) | Platform-owned `IHapticDispatch::IsSystemHapticsEnabled()` query API (cross-platform iOS Core Haptics + Android VibrationEffect query unification) | Input System inherits the OS-state query API contract on first revision after R12a B-CERT-2 closure |

### Bidirectional Notes

- The Wave Spawner forward contract is BINDING on `wave-spawner-pattern-library.md` per R10a §1.3 + R11a §3 DR-B.3. The Wave Spawner GDD authoring must reference §3 Hardware Contract > "Cross-system forward contracts" subsection and the §3 Public Interface (Platform-Side) table. Platform-side, if Wave Spawner authoring elects a different M=3 / M=2 partition (e.g., a finer 4-tier barrage classification), platform's `is_hw_performance_degraded` gate semantics MUST be co-revised in lockstep — the gate's coarse "M=3 vs everything else" carve-out is contingent on Wave Spawner's barrage-class taxonomy holding the M=3 distinction.
- The HUD safe-area binding (R11a-10) is BLOCKING on HUD authoring. If the HUD elects an in-game safe-area framework (e.g., a `USafeAreaWidget` parent) rather than direct OS-inset binding, platform's AC-HW-C verification setup must be updated to test against the HUD framework rather than the OS inset directly. The notched-iOS test apparatus (AC-HW-C Setup R11a §5 DR-D.2) is locked at the iOS `safeAreaInsets.top ≥ 30 pt` device contract; HUD framework changes must preserve this verification path.
- The R12a-pending `IsSystemHapticsEnabled()` / `IsCommitmentTellFlashEnabled()` APIs are platform-owned but presentation-consumed at the call sites. Closure of B-CERT-1 + B-CERT-2 requires lockstep authoring of (a) the platform API surface; (b) the presentation call-site gate; (c) the AC-CERT-1 / AC-CERT-2 verification setups in presentation §8. Platform exposes the API at first revision after R12a APPROVED; presentation inserts the gate at the same revision; ACs assert the composed behavior.
- The `near_miss_haptic_enabled` accessibility setting is AND-composed with `IsSystemHapticsEnabled()` (see presentation §3 Near-Miss Beat + presentation §6 Bidirectional Notes). Platform side: the OS-state query is the platform's only contribution to this composition; the setting toggle lives in HUD/Accessibility Settings GDD; presentation owns the call-site gate-check.
- The watchdog broadcast `OnHardwarePerformanceBreach` is consumed by Wave Spawner + HUD + presentation simultaneously (multicast). All three subscribers receive the same `bEntering` parameter; no subscriber-specific filtering at the platform layer. Subscribers are responsible for their own debouncing if needed (none required at MVP — single-broadcast-per-transition semantics are sufficient).
- **Tick Ordering OQ-1 is a cross-sub-GDD bidirectional dependency.** Mechanics §9 OQ-1 (PM object type ADR) determines the Tick Ordering option (a) vs (b) in platform §3. The Shipping-Safety AC-SS-C row (early-out on `bManualTickEnabled` flag) only applies under Option (b). If OQ-1 resolves to Option (a) (UActorComponent-based RSM), AC-SS-C becomes a defensive-no-op (the flag remains `false`); the AC is retained for forward-compatibility against a future RSM object-type change.

### Cross-sub-GDD forward contracts (platform ↔ mechanics + platform ↔ presentation)

Per decomposition plan §4, the platform sub-GDD has 8 reciprocal forward contracts on `player-movement-mechanics.md` (all 6 plan §4.2 canonical rows covered + 1 additional beyond plan §4.2) and 4 reciprocal forward contracts on `player-movement-presentation.md` (all 4 plan §4.3 canonical rows covered). Per Step 3 execution (2026-07-01), plan §4.2 canonical contracts are enumerated with the plan §4.4 four-element discipline (source, consumed, binding invariant, propagation); the additional-beyond-plan item 5 plat↔mech (Hardware Contract gating context) pre-dates Step 3 verification and retains its lighter format from PASS 7-8 authoring; the 4 plan §4.3 plat↔pres contracts were already covered at PASS 8.

**Platform ↔ Mechanics (8 — reciprocal to mechanics §6 inventory)**:
1. **`effective_dt` definition (B-F6-3 CLOSED at PASS 7)**: platform §4 F-PROLOGUE owns the canonical TickComponent prologue definition (`raw_dt = FApp::GetDeltaTime()` + `effective_dt = clamp(raw_dt, 0.0f, MAX_SLIP_DT_S)` — single-source for both watchdog and tween advance; full DT-source rationale in platform §4 F-PROLOGUE); mechanics §4 F-2 / F-6 consume the upstream value.
2. **Shipping-Safety policy enforcement**: platform §3 Shipping-Safety Enforcement Policy owns the policy + AC family (AC-21, AC-SS-A through E, AC-HW-A through C); mechanics §4 F-2 prologue implements the SLIP_TWEEN persistent clamp pseudo-code; mechanics §7 declares the knob + safe range that the policy enforces.
3. **`SLIP_TWEEN_DURATION_S` safe range**: platform §3 enforcement table consumes the mechanics-owned safe range [0.10, 0.15]; mechanics §7 declares the knob + safe range as the source of truth.
4. **`ERunSlipState` ↔ `EMovementState` ordinal lockstep**: platform §3 Shipping-Safety table enforces via `static_assert` adjacent to the seam cast site + a default-branch fallback in any switch over `EMovementState` (AC-SS-B); mechanics §3 Movement State Enum declares the ordinal contract as the source of truth.
5. **Hardware Contract gating context** *(additional beyond plan §4.2)*: platform §3 Hardware Contract owns the watchdog broadcast (`is_hw_performance_degraded` + `OnHardwarePerformanceBreach`) AND the §5.2 (a) Survivability Margin Relaxation forward contract; mechanics §3 Cross-System Interface Table inherits the broadcast as gating context for Pull-Wave's consumption of mechanics-side `current_lane` + `movement_state`. Pull-Wave reads remain unaffected by the gate (the gate is on barrage *dispatch*, not on PM mechanics output).
6. **Tick Ordering**: platform §3 Tick Ordering owns the Option (a) / (b) resolution path + the AC-SS-C early-out enforcement; mechanics §3 RSM Storage Contract subscribes to RSM's lifecycle delegates whose tick interleaving is governed by this ordering. Resolution depends on mechanics §9 OQ-1 (PM object type ADR).
7. **F-BARRAGE-SURVIVABILITY-INVARIANT input-flow** (plan §4.2 row #1): platform §4 F-BARRAGE-SURVIVABILITY-INVARIANT-CORNERS owns the frame-quantized derivation matrix consuming mechanics-owned knobs `SLIP_TWEEN_DURATION_S` + `MAX_SLIP_DT_S` + `REACTION_BUDGET` (mechanics §7); Wave Spawner M=3 PEAK gate is the downstream consumer of the derived envelope. **Binding invariant** (mirror of mechanics §6 item 7): `SLIP_TWEEN × MIN_ESCAPE_SLIPS + REACTION_BUDGET ≤ TELEGRAPH_WINDOW_FLOOR_S` MUST hold at all supported framerates × MAX_SLIP_DT_S safe-range × SLIP_TWEEN safe-range corners. **Propagation**: if mechanics changes any input knob's safe range, platform re-derives F-BARRAGE-SURVIVABILITY-INVARIANT-CORNERS + re-validates the Wave Spawner gate contract; breach at any corner triggers mechanics + platform + Wave Spawner lockstep re-tuning per R10a §B-LEAN-tension precedent.
8. **`MIN_ESCAPE_SLIPS = 2` constant lockstep** (plan §4.2 row #6): platform §3 Hardware Contract math consumes the registry-bound `MIN_ESCAPE_SLIPS = 2`; mechanics §7 F-BARRAGE math consumes the same constant; platform §8 AC-SS-E (compile-time `static_assert`) enforces that the local C++ value matches design intent. **Binding invariant** (mirror of mechanics §6 item 8): `MIN_ESCAPE_SLIPS` constant value MUST match between platform Hardware Contract math + mechanics F-BARRAGE math + registry declaration; any drift triggers AC-SS-E static_assert failure at PM compile unit. **Propagation**: registry → platform + mechanics lockstep; OQ-7 future yaml→C++ pipeline (see mechanics §9 OQ-7) closes the registry→code drift gap by generating the local C++ constant from the registry entry (pre-OQ-7 closure, drift is caught only at PM compile time via AC-SS-E).

**Platform ↔ Presentation (4 — reciprocal to presentation §6 inventory)**:
1. **`OnHardwarePerformanceBreach` broadcast surface**: platform §3 Public Interface (Platform-Side) declares the multicast delegate + `is_hw_performance_degraded` property; presentation §3 Hardware-Performance Banner consumes the broadcast for banner trigger / release rendering. HUD GDD authoring owns the actual banner rendering; presentation owns the trigger contract + visual envelope spec.
2. **Haptic OS-state gate API** (R12a B-CERT-2 — pending): platform owns the `IHapticDispatch::IsSystemHapticsEnabled()` query API (cross-platform unification of iOS Core Haptics + Android VibrationEffect OS-state queries); presentation §5 EC-19 owns the call-site gate-check at every PM haptic dispatch site (`SlipConfirmed` / `BufferDrop` / `NearMiss`). Platform exposes the API + presentation inserts the gate at first revision after R12a DR-PRES-HAPTIC-GATE APPROVED. AC-CERT-2 (R12a NEW; presentation §8) asserts the composed gate semantics.
3. **Flash settings-bridge API** (R12a B-CERT-1 — pending): platform owns the `IGameSettings::IsCommitmentTellFlashEnabled()` query API (analog to the existing `IsNearMissHapticEnabled()`); presentation §5 EC-18 owns the call-site gate-check at the `LeadingFaceFlash` dispatch site (within the cadence-cap gate). Platform exposes the API + presentation inserts the gate at first revision after R12a DR-PRES-FLASH APPROVED. AC-CERT-1 (R12a NEW; presentation §8) asserts the composed gate semantics.
4. **Banner safe-area surface** (R11a-10): platform's §3 HUD row of the Cross-System Interface Table declares the safe-area binding as a BLOCKING layout constraint on HUD authoring; presentation §3 Hardware-Performance Banner specifies the visual envelope (orange-on-dark, ~24 px, 250 ms fade); platform AC-HW-C (§8) verifies the safe-area binding holds on the notched-iOS device run. HUD inherits the safe-area binding constraint on first authoring.

**Intra-PM scope disambiguation** (per advisor pre-flag): these 12 cross-sub-GDD contracts are *intra-PM* per decomposition plan §4 — they coordinate the three sub-GDDs (mechanics / presentation / platform) that jointly compose the Player Movement system. They are NOT new external forward contracts on Pull-Wave / DPC / Wave Spawner / HUD / Input System / Audio System, which remain governed by R10d's `/propagate-design-change` 2026-06-19 framing of "PM imposes NO new contracts (SLIP_TWEEN safe range unchanged) — R10d does not block PM decomposition." External forward contracts (Wave Spawner M=3 PEAK gate + grace window per R10a §1.3 + R11a §3 DR-B.3; HUD banner per R11a §5 DR-D.1/D.2; Input System haptic vocabulary co-ownership per `player-movement-presentation.md` §6) pre-date PM decomposition and are inherited unchanged.

Step 3 execution (2026-07-01) closed the plan §4.2 canonical coverage (2 NEW items 7 + 8 for plan rows #1 + #6) per the 2026-06-30 verification report. Plan §4.3 contracts (items 1-4 in Platform ↔ Presentation) were already fully covered at PASS 8.

## 7. Tuning Knobs

**No platform-specific tuning knobs at MVP scope.** Per R10a-9, the runtime DT watchdog parameters (60-sample rolling buffer size, sustained-sub-55 threshold `> 18.18 ms` × 30 samples, hitch-cluster threshold `> 16.67 ms` × 18 samples + `MaxSampleDT > 0.033 s`, 3.0 s hysteresis-release window, 3.0 s post-breach grace window per R11a §3 DR-B.3, sentinel pre-fill value `0.01667f` per R11a §3 B.1) are PM-internal implementation constants — they are NOT exposed as designer-tunable knobs because: (a) any change risks invalidating the Hardware Contract math derived in §3 Hardware Contract subsection (re-derivation gate requires CD adjudication, not a runtime tuning operation); (b) the watchdog is calibrated against specific min-spec device thermal envelopes (Polish-phase audit binds the values per AC-HW-B Setup); (c) exposing watchdog tuning to designers risks accidental gameplay-difficulty changes via Hardware Contract gating side effects (the M=3 PEAK gate engagement is gated by watchdog state).

**Mechanical tuning knobs** (SLIP_TWEEN_DURATION_S / MAX_SLIP_DT_S / LEAN_CURVE_ASSET / etc.) live in `player-movement-mechanics.md` §7. **Presentation tuning knobs** (audio_cue_ratio / commitment_tell_flash_amplitude / near_miss_haptic_amplitude / etc.) live in `player-movement-presentation.md` §7.

**Polish-phase deferred candidates** (NOT MVP knobs):
- **Min-spec device list** (currently STUB at §3 Hardware Contract > "Minimum-spec device list" subsection — iPhone XR / Pixel 5 placeholders pending Polish-phase performance-analyst + TD confirmation). At Polish gate, the confirmed min-spec device list may be surfaced as a Polish-phase configuration knob (e.g., `MinSpecDeviceList` config table) for telemetry partitioning and store-listing device-support claims. This is documentation-tier, not gameplay-tier, tuning.

The design-docs.md per-system tuning-knob-presence rule requires every system GDD to declare its tuning knobs explicitly OR declare "no tuning knobs" with rationale. This section satisfies the rule with the "no platform-specific tuning knobs at MVP scope + Polish-phase deferred candidates" declaration.

## 8. Acceptance Criteria

Platform owns the SLIP_TWEEN clamp + Shipping-Safety + Hardware Contract AC family. AC-SS-A through E are defined in §3 Shipping-Safety Enforcement Policy (as the closing rows of the §3 Shipping-Safety subsection); §8 cross-references them. AC-21 + AC-HW-A/B/C are lifted here.

### Tween Formula — Shipping-Safety Clamp (SLIP_TWEEN persistent clamp)

**AC-21 (R11a §1 — rewrites pre-R10a one-shot reset semantic to persistent clamp; coordinated with §3 Shipping-Safety Enforcement Policy table SLIP_TWEEN row, AC-21 fold-in, AC-SS-A, and mechanics §4 F-2 prologue; classification = Logic — automated unit test; BLOCKING gate):** Given `SLIP_TWEEN_DURATION_S = 0.09` (below safe-range floor 0.10), when 10 consecutive ticks occur, then on every tick the effective tween duration is clamped to 0.10 (NOT reset to 0.15 — clamp preserves operator intent per §3 Shipping-Safety policy); `bSlipTweenClampActive == true` for all 10 ticks; F-2 logs an error on tick 1 and on tick 601 (per the rate-limited log per AC-SS-A). No crash, no divide-by-zero. The pre-R10a one-shot `bFloorGuardFired` semantic is RETIRED — no one-shot flag exists. See AC-SS-A (defined in §3 Shipping-Safety Enforcement Policy table) for the 100-tick ceiling-violation case (`SLIP_TWEEN = 0.20`). The F-2 prologue pseudo-code implementation lives in `player-movement-mechanics.md` §4 F-2; this AC verifies platform's enforcement policy contract.

### Shipping-Safety Acceptance Criteria

The full text of **AC-SS-A** (SLIP_TWEEN persistent clamp), **AC-SS-B** (Movement state default-branch fallback), **AC-SS-C** (Tick Ordering Option (b) double-tick early-out), **AC-SS-D** (Curve fallback flag visibility), and **AC-SS-E** (Compile-time constant lock) lives in §3 Shipping-Safety Enforcement Policy at the closing of that subsection (under "**New Shipping-Safety ACs**"). They are platform-owned per decomposition plan §5 BLOCKING-ownership matrix (B-QA-1 + B-PERF-1 + B-SHIP-1). All five are classified Logic — automated unit test; BLOCKING gate.

Summary table:

| AC | Verifies | F-2 / Mechanics dependency |
|---|---|---|
| AC-SS-A | SLIP_TWEEN persistent clamp (both floor + ceiling) + rate-limited log idiom | F-2 prologue clamp (mechanics §4 F-2) |
| AC-SS-B | EMovementState default-branch fallback returns SETTLED on corrupt ordinal | mechanics §3 Movement State Enum |
| AC-SS-C | Tick Ordering Option (b) silent-double-tick early-out | §3 Tick Ordering above |
| AC-SS-D | Curve fallback flag (`bCurveFallbackActive`) visibility | mechanics §4 F-3/F-5 null-curve guards |
| AC-SS-E | MIN_ESCAPE_SLIPS compile-time `static_assert` (local-constant drift catch) | §3 Shipping-Safety policy row + §9 OQ-7 known-gap closure path |

### Hardware Contract Enforcement (R10a §1.3 NEW — closes brief §1.3 + B-LEAN-2026-06-12-1)

**AC-HW-A — Runtime DT watchdog state machine (R10a §1.3 NEW; classification = Logic — automated unit test with mocked DT; BLOCKING gate):** Verifies the watchdog rolling-window + breach + hysteresis logic in isolation, using a test harness that injects `FApp::GetDeltaTime()` via dependency injection.

- **Setup A — clean baseline (no breach):** Inject 60 consecutive samples of `DT = 0.01667s` (60 fps). `bSustainedSub55 == false`; `bHitchCluster == false`; `is_hw_performance_degraded == false`; no `OnHardwarePerformanceBreach` broadcast fires.
- **Setup B — sustained sub-55 entry:** Continue from Setup A. Inject 30 samples of `DT = 0.020s` (50 fps). After the 30th sample, `SamplesAbove18ms == 30`; `bSustainedSub55 == true`; `is_hw_performance_degraded == true`; `OnHardwarePerformanceBreach(true)` fires exactly once on the transition.
- **Setup C — hitch cluster entry:** Reset rolling buffer to all 60-fps samples. Inject 17 samples of `DT = 0.018s` + 1 sample of `DT = 0.040s` (a 25 fps hitch). After the 18th sample, `SamplesAbove16ms == 18` AND `MaxSampleDT == 0.040 > 0.033`; `bHitchCluster == true`; `is_hw_performance_degraded == true`; broadcast fires.
- **Setup D — hysteresis-release boundary:** Continue from Setup B (degraded state). Inject 60 consecutive `DT = 0.01667s` samples plus 3.0 s × 60 fps = 180 additional clean samples. After 3.0 s of continuous-clean samples, `is_hw_performance_degraded == false`; `OnHardwarePerformanceBreach(false)` fires exactly once on the release.
- **Setup E — flap prevention:** Inject 60 clean samples (Setup A); then a single 0.025s hitch sample; then 60 clean samples. Verify `is_hw_performance_degraded` never becomes true (a single hitch is not enough — `SamplesAbove16ms == 1` is far below the 18-sample threshold). Verify `ContinuousCleanWindowTime` resets to 0 on the hitch sample and starts re-accumulating after it.
- **Setup F — broadcast subscriber correctness:** Set up a mock Wave Spawner subscriber + mock HUD subscriber to `OnHardwarePerformanceBreach`. Trigger Setup B. Verify Wave Spawner subscriber receives `bEntering=true` and HUD subscriber receives `bEntering=true`. Trigger Setup D. Verify both receive `bEntering=false`. No duplicate broadcasts.
- **Setup G — startup sentinel pre-fill coverage (R11a §3 B.1 NEW):** Two-part AC. **Part 1 (sentinel presence — the load-bearing regression catch):** construct PM and invoke `BeginPlay`. Immediately after `BeginPlay` returns and BEFORE any `TickComponent` invocation, assert `TickDTRollingBuffer[i] == 0.01667f` for all `i ∈ [0, 60)`, `TickDTRingIndex == 0`, `ContinuousCleanWindowTime == 0.0f`, and `bHardwarePerformanceBreachActive == false`. A regression that removes the BeginPlay pre-fill (returning to default zero-init) fails this assertion at the very first slot read. **Part 2 (degraded-startup detection latency):** continue from Part 1. Inject 30 consecutive samples of `DT = 0.020s` (50 fps) starting at the first `TickComponent` invocation (t=0). After the 30th injected sample, verify `SamplesAbove18ms == 30`, `bSustainedSub55 == true`, `is_hw_performance_degraded == true`, and `OnHardwarePerformanceBreach(true)` fires exactly once. The watchdog must reach breach within 30 frames of startup-onset degradation, with no warm-up delay attributable to buffer-fill semantics.

**AC-HW-B — Min-spec device PEAK-density framerate audit (R10a §1.3 NEW; classification = Visual/Feel — Polish-phase device measurement; ADVISORY gate; Polish-phase BLOCKING):** Verifies sustained 60 fps under PEAK barrage density on each named min-spec device.

- **Pass criteria (R11a §3 B.2 rewrite — closes pre-R11a math/units defect)**: three orthogonal criteria, all must hold for the device to pass.
  1. **Steady-state framerate**: ≥ 99.9% of frames have `DT ≤ 16.67ms + 0.5ms` (= exact 60 fps target, with 0.5ms measurement tolerance allowing for Unreal Insights / on-device profiler sample jitter). At 60 fps × 60 s = 3600 frames per session, the 0.1% threshold equals ≤ 3-4 frames above the ceiling per session.
  2. **Peak hitch tolerance**: NO single frame sample with `DT > 33.33ms` (= sub-30 fps tick — corresponds to the watchdog's secondary breach trigger condition `MaxSampleDT > 0.033f`). Any single hitch above 33.33ms fails the audit unconditionally.
  3. **Watchdog non-breach**: throughout the full 60 s session under PEAK barrage density, `is_hw_performance_degraded == false` at all times.
- **Why the pre-R11a "99% of frames within 17.67ms (= 60 fps ± 1 ms margin)" criterion was wrong (documented for revision-history traceability)**: (1) **units error** — 17.67 ms ≠ 60 fps; 17.67 ms = 1000/17.67 ≈ 56.6 fps. The "± 1 ms margin" label conflated a frame-time tolerance with a framerate tolerance. (2) **hitch budget error** — 1% of 3600 frames per 60 s session = 36 hitches above the ceiling, ≈ 10× the rate a mobile gameplay-targeting-sustained-60fps player would tolerate. (3) **single-criterion conflation** — combined steady-state framerate and peak-hitch tolerance into one number; the rewritten three-criterion structure separates them.
- **Device 1 (iOS — placeholder iPhone XR; TD + performance-analyst confirms during Polish-phase audit):** Run SLIPSTORM for a full 60-second session under PEAK barrage density. Sample `FApp::GetDeltaTime()` every frame. Apply all three pass criteria above. On fail (any criterion), move iOS min-spec to iPhone 11 and re-audit.
- **Device 2 (Android — placeholder Pixel 5 or Galaxy A52; TD + performance-analyst confirms):** Same test setup, same three pass criteria. On fail (any criterion), move Android min-spec up one Snapdragon tier and re-audit.
- **Failure outcome on BOTH baseline devices**: reopen Hardware Contract for creative-director adjudication (tighten min-spec, reduce PEAK density, or both).
- **Test apparatus**: real device required. Profiling tools (Apple Instruments, Android Studio Profiler, Unreal Insights on-device) capture frame timing for the run. Stress profile = PEAK density sustained for full 60s. The watchdog non-breach criterion (3) is observed by sampling `is_hw_performance_degraded` per frame OR by subscribing a test fixture to `OnHardwarePerformanceBreach` and asserting no `bEntering=true` event fires during the run.
- **Polish-phase BLOCKING note**: this AC is ADVISORY during MVP development and BLOCKING at the Polish gate. Pre-Polish, the AC is documented but not enforced.

**AC-HW-C — Shipping-build Performance banner appears under sustained sub-55 fps (R10a §1.3 + §5.3 (a) NEW; classification = Integration/Visual — manual evidence with recorded device run; ADVISORY gate):** Verifies the §5.3 (a) Banner Notification appears when the watchdog breaches under realistic Shipping conditions on a min-spec or sub-min-spec device.

- **Setup (R11a §5 DR-D.2 amended — notched-iOS device required)**: TWO real devices below the min-spec, exercised independently: (i) a **notched-iOS device** (iPhone X / iPhone 12 Pro or newer — must have Dynamic Island or notch with `safeAreaInsets.top ≥ 30 pt`) to verify the R11a-10 safe-area placement on the iOS fleet; (ii) an Android device with visible status bar (e.g., Galaxy S9, Pixel 4a) to verify the Android `WindowInsetsCompat` path. Both devices intentionally below the min-spec to force a watchdog breach. Build with the Hardware Contract + Banner subsystem enabled in Shipping.
- **Procedure**: run SLIPSTORM through 30 seconds of EARLY → MID → PEAK transitions on EACH device. Record each run with screen capture + frame timing capture.
- **Pass criteria**: (a) banner appears within ~1.0–1.5 s of entering PEAK density (watchdog breach detection latency = 60-sample rolling window); (b) banner copy reads **"Performance mode — hardest barrage suppressed."** (R11a-9 locked copy; pre-R11a copy "Performance reduced — survivability adjustments active." is RETIRED — any test apparatus or build artefact still rendering the pre-R11a copy fails this AC) with the documented styling (orange-on-dark, ~24 px tall, anchored below the OS safe-area top inset per (b-safe-area) below); **(b-safe-area, R11a §5 DR-D.2 NEW)** the banner is **fully visible below the OS safe-area top inset** on the notched-iOS device — no clipping behind the notch / Dynamic Island, no visible truncation of the leading or trailing characters; on the Android device, the banner sits below the status-bar inset with no overlap. Verify by frame-by-frame inspection of the screen-capture recording on both devices; (c) M=3 PEAK barrages are suppressed during the breach window (Wave Spawner gate active); (d) banner fades after 3.0 s of clean samples following exit from PEAK to MID. No audio cue, no haptic.
- **Survivability promise during breach (R11a §3 DR-B.3 = (b) qualified)**: with M=3 PEAK suppressed (per §3 Hardware Contract > Cross-system forward contracts > Wave Spawner gate) AND the 3.0 s post-breach grace window honored (per R11a §3 DR-B.3 = (b)), the M=2 surviving triplet set is the only PEAK barrage class the player faces. Test apparatus must additionally verify, from the recorded device run: (i) NO M=3 PEAK barrage is *dispatched* during the 3.0 s window following `OnHardwarePerformanceBreach(true)` (subscribe a Wave Spawner dispatch-log to the recorded run; assert zero M=3 PEAK dispatch events in the window); (ii) any M=3 PEAK barrage already in flight at the moment of breach onset is permitted to complete naturally. Verify the player can survive PEAK density during the breach without facing an inescapable NEW M=3 PEAK barrage on a degraded device — Pillar 5 is preserved.
- **Lead sign-off**: UX-designer + audio-director (silent-banner sign-off) + creative-director (Pillar 5 preservation sign-off) + performance-analyst (device measurement sign-off).

## 9. Open Questions

Platform owns the architecture-seam OQ family (registry → C++ generator pipeline + Shipping guard ADR cross-ref). OQ-1 / OQ-2 / OQ-3 / OQ-4 / OQ-5 / OQ-6 live in `player-movement-mechanics.md` §9.

**OQ-7 — Registry → C++ header generator pipeline (R11a §7 DR-G.1 = R11a-17 NEW — out of GDD scope; belongs in `docs/architecture/` as a seam ADR).**
PM's `MIN_ESCAPE_SLIPS` C++ constant is currently hand-mirrored from `design/registry/entities.yaml` (canonical value 2) at the moment the registry value is bound. Code-review is the only gate that catches registry → local-C++ drift. The closure path is an architecture seam that auto-generates a C++ header from the registry YAML at build time (or commits the generated header to source control with a CI lint that re-runs the generator and asserts no diff).

Required artifacts when the seam is authored:
1. **Generator tool path + invocation command** (e.g., `tools/registry-codegen` invoking a Python or Rust script).
2. **Generated header file path** (e.g., `Source/Slipstorm/Generated/RegistryConstants.h`) + namespace.
3. **CI integration** that re-runs the generator and asserts no uncommitted diff (catches the "registry changed but generator not re-run" failure mode).
4. **PM compile unit migration** from hand-mirrored local constant to `#include` of the generated header.

**Owners (when authored)**: TD (architecture seam design); lead-programmer + tools-programmer (generator + CI integration); PM GDD update at that point retires the §3 Shipping-Safety Enforcement Policy AC-SS-E "known gap" clause and reverts to a pipeline-aware compile-time + CI-time test mechanism.

**Pre-condition**: registry schema first needs a versioning or change-detection convention so that the generator output is deterministic across registry edits — currently `entities.yaml` is an unversioned flat document, which is sufficient for the registry's current use but may need formalization before generator authoring.

Filed 2026-06-16. Not blocking on R11a-APPROVED; PM ships R11a with the local `static_assert` as the only compile-time gate (see §3 Shipping-Safety Enforcement Policy table > MIN_ESCAPE_SLIPS row + §8 AC-SS-E cross-reference).

**Cross-references**:
- §8 AC-SS-A through E (defined in §3 Shipping-Safety Enforcement Policy at the close of that subsection).
- B-SHIP-1 closure path runs through this OQ + the §3 Shipping-Safety policy.
- `player-movement-mechanics.md` §9 OQ-1 (PM object type ADR) is a separate ADR with adjacent ADR-path implications (the Tick Ordering Option (b) choice in §3 Tick Ordering depends on OQ-1's resolution).

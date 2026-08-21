# Epic: Player Movement (Slip)

> **Layer**: Core
> **GDD**: `design/gdd/player-movement.md` (decomposed 2026-06-16 into 3 sub-GDDs: `player-movement-mechanics.md`, `player-movement-presentation.md`, `player-movement-platform.md`)
> **Architecture Module**: `UPlayerLaneMovementComponent` on `ASlipstormPlayerPawn` (ADR-0009 SD1/SD2)
> **Status**: Complete (2026-08-07 — 13/13 stories closed + committed)
> **Stories**: 14 stories (13 numbered + Story 001a test harness) created 2026-07-09; all closed 2026-07-11 through 2026-08-07 (see `## Stories` table below)
> **Runtime Baseline (2026-08-07)**: Unit tests 39/39 GREEN; Integration tests 30/85 GREEN (blocked by test-harness bug, Sprint 1 S1-04 target)

## Overview

Implements SLIPSTORM's Player Movement (Slip) system — the game's single verb (Pillar 1). PM is a `UActorComponent`-hosted lane-snap tween that consumes discrete slip-left/slip-right input events, gates them against the Run State Machine (RUNNING + !paused + !resume_grace), commits collision geometry to the target lane on the SETTLED→SLIPPING transition, and interpolates the visual mesh from source-lane to target-lane over a curve-authored 100–150ms window (SLIP_TWEEN_DURATION_S). The component owns the F-1 through F-6 formula family (lane world position, tween progress, lateral interpolation, edge-check, staggered body/head/arm lean, edge-absorb tail-phase animation), a single-slot input buffer with F-4 edge-check discard rules, a monotonic counter set for telemetry and Death Replay, and the runtime hardware watchdog (60-sample rolling DT window with sustained-sub-55fps + hitch-cluster breach criteria, 3.0s clean hysteresis-release) that publishes `OnHardwarePerformanceBreach(bool)` to Wave Spawner + HUD. Presentation surface covers the commitment-tell (±0.80 leading-face flash with 200ms cadence cap), audio cue dispatch (center-pan slip cue with -6dB ducking under near-miss + HARD-CUT under buffer-drop), haptic dispatch through the ADR-0002 IHapticDispatch bridge (`SlipConfirmed`, `BufferDrop`, `NearMiss`), and the Hardware-Performance Banner ("Performance mode — hardest barrage suppressed."). Curve assets (`SlipCurve`, `LeanCurve`, `EdgeAbsorbCurve`) are hard-referenced `TObjectPtr<UCurveFloat>` UPROPERTY fields validated at `BeginPlay()` with a Shipping-safe fallback path (linear F-3, zero-lean F-5, immediate F-6 return, `bCurveFallbackActive = true`) per AC-SS-D.

## Governing ADRs

| ADR | Decision Summary | Engine Risk |
|-----|------------------|-------------|
| **ADR-0009: Player Movement Component Hosting, Forward-Motion Model, and Tween Implementation** (Accepted 2026-07-09) | `UPlayerLaneMovementComponent : UActorComponent` on new `ASlipstormPlayerPawn : APawn`; stationary-player Option (c) forward-motion model (moving-world convention); `RSM->ForceTickNow()` first-statement tick pattern; `SetActorLocation(bSweep=false)` root commit + `SetRelativeLocation` mesh interpolation dual-write; hard-referenced `TObjectPtr<UCurveFloat>` UPROPERTY curves validated at BeginPlay with Shipping-safe fallback (mirrors ADR-0008 SD2). Adds 2 forbidden patterns to architecture.yaml v8. | MEDIUM |
| **ADR-0007: Run State Machine Hosting and Sleep-Aware Time Source** (Accepted 2026-06-26) | RSM as `UGameInstanceSubsystem` + `FTickableGameObject`; `ForceTickNow()` public pull primitive with `bHasTickedThisFrame` idempotence guard (INT-005-amended); non-dynamic `OnStateChanged` / `OnPausedChanged` multicast delegates PM subscribes to at BeginPlay via `AddUObject`; `FCString::Strtoui64` salt-parse pattern (INT-006-amended). | MEDIUM |
| **ADR-0002: Haptic Platform Bridge** (Proposed; interface INT-002-amended 2026-06-26, HW-verification gate deferred to Polish) | `IHapticDispatch::Fire(EHapticEvent)` + `IsSystemHapticsEnabled()` platform-abstracted haptic surface; PM dispatches `EHapticEvent::{SlipConfirmed, BufferDrop, NearMiss}` (NearMiss enum added by INT-002). Interface consumed by PM is codified and stable; PM implementation compiles regardless of ADR-0002 status label per architecture-review-2026-07-03 §INT-007 pragmatic-promotion rationale. | LOW (interface stable) |

## GDD Requirements

All 35 TR-PM requirements are covered by ADR-0009 (35/35 = 100%). TR-PM-030 is double-covered by ADR-0002 (near-miss haptic enum value INT-002).

| TR-ID | Requirement (one-line) | Sub-GDD | ADR Coverage |
|-------|------------------------|---------|--------------|
| TR-PM-001 | UPlayerLaneMovementComponent on GameThread | mechanics | ADR-0009 ✅ |
| TR-PM-002 | current_lane / target_lane public read-only EPlayerLane | mechanics | ADR-0009 ✅ |
| TR-PM-003 | ERunSlipState SETTLED/SLIPPING ordinals pinned 0/1 | mechanics | ADR-0009 ✅ |
| TR-PM-004 | lane_world_x = (lane_index − 2) × LANE_WIDTH_CM | mechanics | ADR-0009 ✅ |
| TR-PM-005 | F-2 TweenProgress + MAX_SLIP_DT_S cap | mechanics | ADR-0009 ✅ |
| TR-PM-006 | SLIP_TWEEN_DURATION_S clamped [0.10, 0.15]s | mechanics | ADR-0009 ✅ |
| TR-PM-007 | F-3 lateral position via Lerp + SlipCurve | mechanics | ADR-0009 ✅ |
| TR-PM-008 | F-5 rotation: body/head/arm lean via LeanCurve | mechanics | ADR-0009 ✅ |
| TR-PM-009 | F-6 edge-absorb 2-frame fade-out on SETTLED→SLIPPING | mechanics | ADR-0009 ✅ |
| TR-PM-010 | Subscribe RSM OnStateChanged + OnPausedChanged | mechanics | ADR-0009 ✅ |
| TR-PM-011 | Gate slip inputs by RSM RUNNING + !is_paused + !resume_grace | mechanics | ADR-0009 ✅ |
| TR-PM-012 | Single-slot input buffer + F-4 edge-check | mechanics | ADR-0009 ✅ |
| TR-PM-013 | Commitment-tell 80% flash + 2-frame hold + 50ms decay | presentation | ADR-0009 ✅ |
| TR-PM-014 | Commitment-tell cadence cap 1 per 200ms | presentation | ADR-0009 ✅ |
| TR-PM-015 | Monotonic counters for slip/edge-absorb/commit-tell | mechanics | ADR-0009 ✅ |
| TR-PM-016 | Counter reset on COUNTDOWN; preserve on DEAD/COMPLETE/ABORTED | mechanics | ADR-0009 ✅ |
| TR-PM-017 | Freeze on DEAD; preserve for Death Replay | mechanics | ADR-0009 ✅ |
| TR-PM-018 | Snap to target_lane + reset on COMPLETE/ABORTED same frame | mechanics | ADR-0009 ✅ |
| TR-PM-019 | current_lane preserved as source throughout SLIPPING | mechanics | ADR-0009 ✅ |
| TR-PM-020 | 60-sample rolling DT watchdog (≥30 samples >18.18ms) | platform | ADR-0009 ✅ |
| TR-PM-021 | Hysteresis-release: 3.0s clean (all 60 ≤16.67ms) | platform | ADR-0009 ✅ |
| TR-PM-022 | OnHardwarePerformanceBreach(bool) multicast to WS + HUD | platform | ADR-0009 ✅ |
| TR-PM-023 | Breach action: WS suppress M=3 PEAK + 3.0s grace | platform | ADR-0009 ✅ |
| TR-PM-024 | is_hw_performance_degraded read-only to WS/HUD/DPC | platform | ADR-0009 ✅ |
| TR-PM-025 | TickDTRollingBuffer sentinel 0.01667f init at BeginPlay | platform | ADR-0009 ✅ |
| TR-PM-026 | Shipping-safe guards: SLIP_TWEEN, EMovementState, curve nulls | platform | ADR-0009 ✅ |
| TR-PM-027 | static_assert(MIN_ESCAPE_SLIPS==2) registry-drift catch | platform | ADR-0009 ✅ |
| TR-PM-028 | EC15_F6_DECAY_COEFFICIENT 0.7 in F-6 SLIPPING phase 3 | mechanics | ADR-0009 ✅ |
| TR-PM-029 | F-6 lean staggered offsets HEAD_LAG=0.10, ARM_LEAD=0.05 | mechanics | ADR-0009 ✅ |
| TR-PM-030 | Near-miss haptic EHapticEvent::NearMiss | presentation | ADR-0009 ✅ + ADR-0002 (INT-002) ✅ |
| TR-PM-031 | Slip cue duration = audio_cue_ratio × SLIP_TWEEN_DURATION_S | presentation | ADR-0009 ✅ |
| TR-PM-032 | -6dB slip duck on near-miss overlap; HARD-CUT on buffer-drop | presentation | ADR-0009 ✅ |
| TR-PM-033 | Clamp final lean to ±(MAX_LEAN_ANGLE_DEG × 1.2) | mechanics | ADR-0009 ✅ |
| TR-PM-034 | BeginPlay curve verification + warnings | mechanics | ADR-0009 ✅ |
| TR-PM-035 | Tick after RSM via `RSM->ForceTickNow()` first statement (SD4 supersedes AddTickPrerequisiteComponent) | platform | ADR-0009 ✅ |

## Forward Contracts (owed by this epic)

This epic closes forward contracts for other systems. Downstream consumers depend on PM's public surface being live:

- **ADR-0005 (Wave Spawner)** — consumes `is_hw_performance_degraded` (public read-only bool) + `OnHardwarePerformanceBreach(bool)` (multicast delegate) + honors R11a-8 3.0-second post-breach grace window on M=3 PEAK barrages.
- **Pull-Wave (Rule 11)** — direct-read of `PM.current_lane` during SLIPPING for near-miss detection (source-lane semantic preserved per R7-PM-PROPAGATION-REVIEW).
- **HUD (unauthored)** — consumes `is_hw_performance_degraded` for the "Performance mode — hardest barrage suppressed." banner display + safe-area layout binding.
- **Seam 12 `IPlayerMovementProvider`** — `FPlayerMovementProvider_Production(UPlayerLaneMovementComponent*)` constructor becomes callable; test-double `FPlayerMovementTestStub` also lands here.
- **Camera GDD (unauthored)** — inherits SD3 stationary-player / moving-world model as BINDING forward contract.
- **Collision GDD (unauthored)** — inherits SD5 collision-commit contract (`SetActorLocation(bSweep=false)` on pawn root; mesh via `SetRelativeLocation`).

## Definition of Done

This epic is complete when:

- All stories are implemented, reviewed, and closed via `/story-done`
- All acceptance criteria from `design/gdd/player-movement-mechanics.md`, `player-movement-presentation.md`, and `player-movement-platform.md` are verified
- All Logic and Integration stories have passing test files in `tests/unit/player-movement/` and `tests/integration/player-movement/`
- All Visual/Feel and UI stories have evidence docs with sign-off in `production/qa/evidence/`
- ADR-0009 Verification Required items (1)–(6) confirmed on target hardware (UE 5.7 mobile; iPhone + Android mid-tier)
- `static_assert(MIN_ESCAPE_SLIPS == 2)` compile-time gate in PM compile unit (AC-SS-E per ADR-0009 SD6 IG-5 supersedes AC-SS-E in mechanics §7)
- `ERunSlipState ↔ EMovementState` ordinal lockstep `static_assert` at Seam 12 cast site (AC-SS-B)
- Per-tick PM CPU < 0.15 ms p99 on mid-tier mobile (Alpha-gate placeholder; measured via `STAT_PMTick` scope-cycle-counter watchdog)
- ADR-0009's 2 forbidden patterns (`PlayerMovement_SetActorRotation_for_lean`, `PlayerMovement_TickComponent_without_prior_ForceTickNow`) grep-clean in `src/`
- Wave Spawner's dependency on `OnHardwarePerformanceBreach` + `is_hw_performance_degraded` is wired (closes ADR-0005 R11a-8 forward contract on PM side)

## Stories

| # | Story | Type | Status | ADR |
|---|-------|------|--------|-----|
| 001 | [Pawn + Component skeleton + lifecycle + Seam 12 wire](story-001-pawn-component-skeleton.md) | Integration | Complete 2026-07-11 | ADR-0009 + ADR-0007 |
| 001a | [Test harness for Story 001 lifecycle + seam tests](story-001a-test-harness.md) | Integration | Complete 2026-07-11 | ADR-0009 + ADR-0007 |
| 002 | [Lane math + tween prologue (F-1 + F-PROLOGUE + F-2 + SLIP_TWEEN clamp)](story-002-lane-math-tween-prologue.md) | Logic | Complete 2026-07-12 | ADR-0009 |
| 003 | [State machine SETTLED↔SLIPPING + tick body + collision commit + source-lane + F-4 + OnSlipMidpoint](story-003-state-machine-tick-body.md) | Integration | Complete 2026-07-13 | ADR-0009 + ADR-0007 |
| 004 | [F-3 lateral interpolation + mesh SetRelativeLocation + curve fallback](story-004-f3-lateral-interpolation.md) | Logic | Complete 2026-07-13 (91491d4) | ADR-0009 |
| 005 | [Single-slot input buffer + Rule 3 buffer-drop + Rule 11 discard](story-005-input-buffer.md) | Logic | Complete 2026-07-13 (580d39d) | ADR-0009 + ADR-0002 |
| 006 | [F-5 body/head/arm lean + LeanCurve + staggered offsets + SetRelativeRotation clamp](story-006-f5-lean.md) | Logic | Complete 2026-07-13 (580d39d) | ADR-0009 |
| 007 | [F-6 edge-absorb tail + Rule 1 edge no-op + 2-frame fade-out override + EC15 decay + co-write](story-007-f6-edge-absorb.md) | Logic | Complete 2026-07-15 (59e2d3d) | ADR-0009 |
| 008 | [Terminal state handlers (DEAD/COMPLETE/ABORTED/COUNTDOWN) + counter reset + AC-SS-B runtime](story-008-terminal-state-handlers.md) | Integration | Complete 2026-07-16 (7d994ab) | ADR-0009 + ADR-0007 |
| 009 | [HandlePausedChanged + Rule 6 pause/resume/grace freeze + buffer preservation](story-009-pause-grace.md) | Integration | Complete 2026-08-02 (edf6b22) | ADR-0009 + ADR-0007 |
| 010 | [Commitment-tell 80% flash + 2-frame hold + 50ms decay + 200ms cadence cap + counter](story-010-commitment-tell.md) | Visual/Feel | Complete-With-Notes 2026-08-02 (4b6ae6b) — 2 R12a-PENDING items discharge in Sprint 1 S1-06 | ADR-0009 |
| 011 | [Slip audio cue + F-AUDIO-CUE-DURATION + -6dB duck + HARD-CUT triple-overlap](story-011-slip-audio-cue-ducking.md) | Integration | Complete 2026-08-03 (6498c46) | ADR-0009 |
| 012 | [Near-miss beat (Y-dip + audio swell + haptic dispatch) + TriggerNearMissBeat() public API](story-012-near-miss-beat.md) | Integration | Complete 2026-08-03 (b42ce2a) | ADR-0009 + ADR-0002 (Proposed; interface INT-002-stable) |
| 013 | [DT watchdog + breach + 3.0s hysteresis-release + broadcast + is_hw_performance_degraded](story-013-dt-watchdog.md) | Logic | Complete 2026-08-07 (da782af) — 12/12 runtime GREEN | ADR-0009 |

**Sequencing note**: work stories in numerical order — each story's `Depends on:` field lists prerequisites. Foundation stories 001–003 must be DONE before any downstream story starts; Stories 004–013 have partial parallelism within the constraints documented in each file.

**Deferred to downstream epics** (documented in Forward Contracts above):
- Wave Spawner side of R11a-8 grace window (TR-PM-023) → Wave Spawner epic.
- Hardware banner rendering + AC-HW-C on-device manual evidence → HUD epic.
- AC-HW-B min-spec device audit → Polish-phase evidence (not code).

## Next Step

Epic Complete 2026-08-07. Remaining follow-ups tracked in Sprint 1 (`production/sprints/sprint-1.md`):
- **S1-04**: Integration test harness fix (RSMSubsystem null in test PIE world → currently 30/85 Integration pass rate; target ≥ 85% after fix)
- **S1-06**: Story 010 R12a-PENDING PEAT evidence discharge
- **S1-08**: `/architecture-review` PM downstream consumers pass (Wave Spawner R11a-8 grace window, HUD banner subscriber, ADR-0005 forward-contract closure)

Deferred to Polish phase (not blocking epic closure):
- ADR-0009 Verification Required items (1)–(6) — target hardware not yet available (iPhone + Android mid-tier device measurement gates)
- Per-tick PM CPU < 0.15 ms p99 measurement gate via `STAT_PMTick` scope-cycle-counter

Wave Spawner subscriber wiring (`OnHardwarePerformanceBreach`, `is_hw_performance_degraded`) is a Wave Spawner epic scope item — PM's side of the forward contract is READY for consumption.

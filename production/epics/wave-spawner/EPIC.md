# Epic: Wave Spawner Pattern Library

> **Layer**: Feature
> **GDD**: `design/gdd/wave-spawner-pattern-library.md`
> **Architecture Module**: `UWaveSpawnerSubsystem` (SLIPSTORM module — `UGameInstanceSubsystem + FTickableGameObject`)
> **Status**: Ready
> **Stories**: 9 stories (all Ready — run `/dev-story` to begin implementation)

## Overview

The Wave Spawner Pattern Library epic implements `UWaveSpawnerSubsystem`, the Feature-layer
subsystem that feeds `APullWaveSubsystemActor::Construct()` with wave admission decisions.
It pre-allocates a 23-slot `AWave` object pool at `PostLoadMapWithWorld`, subscribes to DPC's
`OnPostTickFrameStatePublished` delegate for tick ordering, and on each `OnDPCFrameReady` event
runs a four-stage admission pipeline: concurrency-cap check → atomic slot pre-commitment →
cadence governor (F-3 barrage weighting) → pattern draw from one of three phase-specific
pools (OPENER / MID / PEAK). A six-state lifecycle (`Cold → Active → Holding ↔ Flushing →
Idle`) governs when ticking is live, when patterns admit, and how RSM pause and terminal
events drain in-flight waves cleanly. A cook-time contract (14 binding Rule 15 checks)
verifies pool authoring invariants before runtime, ensuring the spawner never encounters
an empty pool or an invalid triplet at draw time.

## Governing ADRs

| ADR | Decision Summary | Engine Risk |
|-----|-----------------|-------------|
| ADR-0011: Wave Spawner Pattern Library | D1 Three-Pool Architecture, D2 Four-Stage Admission Pipeline, D3 Six-State Lifecycle, D4 Cook-Time Contract (14 checks) | HIGH |
| ADR-0007: Run State Machine | RSM provider of `RunSeed:uint64` and `OnPausedChanged` delegate — captured at `Cold→Active`; pause-flush and resume-grace behavior | LOW |
| ADR-0008: DPC Frame State | DPC `OnPostTickFrameStatePublished` subscription for tick-ordering pin; `telegraph_window_s` snapshot at admission | LOW |

> **ADR-0005 (Forward Contract / Seam Architecture)** is listed as an upstream dependency of
> ADR-0011 (`Proposed` as of last check). ADR-0011 does not re-decide the hosting primitive,
> so ADR-0005's Proposed status does not block this epic. If ADR-0005 is revised to change
> pool-storage type or subscription lifecycle, ADR-0011 must be revised in lockstep.

## GDD Requirements

| TR-ID | Requirement | ADR Coverage |
|-------|-------------|--------------|
| TR-WS-001 | Three phase-specific pattern pools (OPENER/MID/PEAK) must be pre-authored, static at runtime | ADR-0011 D1 ✅ |
| TR-WS-002 | OPENER pool contains no barrage patterns; all entries must be non-barrage only | ADR-0011 D1 ✅ |
| TR-WS-003 | MID pool contains no barrage patterns; all entries must be non-barrage only (Rule 2) | ADR-0011 D1 ✅ |
| TR-WS-004 | PEAK barrage sub-pool target lanes must be exactly 7 triplets: {0,1,3},{0,1,4},{0,2,3},{0,2,4},{0,3,4},{1,2,4},{1,3,4} | ADR-0011 D1 ✅ |
| TR-WS-005 | Every PEAK barrage pattern must have minimum tier >= 2; tier-1 barrages forbidden | ADR-0011 D1 ✅ |
| TR-WS-006 | Barrage patterns must cluster telegraph onsets within 0.35s window (FLOOR / 2) | ADR-0011 D2 ✅ |
| TR-WS-007 | Non-barrage patterns must stagger consecutive onsets >= 0.70s minimum (TELEGRAPH_WINDOW_FLOOR) | ADR-0011 D2 ✅ |
| TR-WS-008 | Object pool must pre-allocate exactly 23 AWave instances once per session at PostLoadMapWithWorld | ADR-0011 D1 ✅ |
| TR-WS-009 | Pool storage uses UPROPERTY() TArray<TObjectPtr<AWave>> to prevent garbage collection | ADR-0011 D1 ✅ |
| TR-WS-010 | Spawner implements UGameInstanceSubsystem and FTickableGameObject dual inheritance | ADR-0011 ✅ |
| TR-WS-011 | Spawner must not tick by default; conditional tick disabled in Cold/Idle states | ADR-0011 D3 ✅ |
| TR-WS-012 | Spawner reads DPC FDPCFrameState snapshot once per tick at rule 1 top; treated immutable rest of tick | ADR-0011 D2 ✅ |
| TR-WS-013 | Per-run RNG seed captured from RSM RunSeed:uint64 at Cold to Active transition for determinism | ADR-0011 D2/D3 ✅ |
| TR-WS-014 | Death Replay must restore RNG seed before replaying pattern selection for Pillar 5 | ADR-0011 D2 ✅ |
| TR-WS-015 | Cadence gate requires (now - last_spawn_time) >= wave_spawn_interval_s for admission | ADR-0011 D2 ✅ |
| TR-WS-016 | Primer pattern (first draw) admits immediately on Cold to Active; bypass cadence gate check | ADR-0011 D2/D3 ✅ |
| TR-WS-017 | Barrage admission requires 3 free slots atomically; non-barrage requires 1 free slot | ADR-0011 D2 ✅ |
| TR-WS-018 | Dropped barrages set barrage_owed flag; next available 3-slot window reserved for barrage draw | ADR-0011 D2 ✅ |
| TR-WS-019 | Spawner emits 9 named telemetry events with defined payload schemas (C.3 §9) | ADR-0011 D3 ✅ |
| TR-WS-020 | Pool pre-allocation fires exactly once per session; replay re-entry reuses existing pool | ADR-0011 D1 ✅ |
| TR-WS-021 | Atomic pool swap on phase transitions (OPENER to MID, MID to PEAK) using pointer atomicity | ADR-0011 D1/D3 ✅ |
| TR-WS-022 | In-flight waves complete under admission-time telegraph_window_s snapshot; no re-parametrization | ADR-0011 D2 ✅ |
| TR-WS-023 | Phase-boundary drain window (Rule 9): no new pattern started within max_pattern_length_s of transition | ADR-0011 D3 ✅ |
| TR-WS-024 | Spawner subscribes to DPC OnPostTickFrameStatePublished delegate for tick ordering pin | ADR-0011 D2 ✅ |
| TR-WS-025 | Spawner reads RSM OnPausedChanged delegate to trigger pause flush (Rule 13) | ADR-0011 D3 ✅ |
| TR-WS-026 | Rule 12 despawn pipeline ordered: collision unregister -> telegraph unregister -> OnWaveDespawned | ADR-0011 D3 ✅ |
| TR-WS-027 | Pause-flush does NOT update last_spawn_time; pre-pause value remains authoritative on resume | ADR-0011 D3 ✅ |
| TR-WS-028 | RSM resume grace honored via Rule 1 gate; spawner waits RESUME_GRACE_S after OnPausedChanged(false) | ADR-0011 D3 ✅ |
| TR-WS-029 | Cadence governor F-3 drives session barrage average to BARRAGE_EVENTS_PER_PEAK_TARGET_AVG = 2 | ADR-0011 D2 ✅ |
| TR-WS-030 | F-3 force-draw mode activates when zero barrages and t_norm >= T_FORCE (forcing floor satisfaction) | ADR-0011 D2 ✅ |
| TR-WS-031 | base_w (unweighted barrage fraction) must be in [0.15, 0.40] safe range (F-3 cook-time advisory) | ADR-0011 D4 ✅ |
| TR-WS-032 | base_w < W_CEILING cross-knob invariant prevents F-3 proportional branch silent saturation | ADR-0011 D4 ✅ |
| TR-WS-033 | Platform-scoped Death Replay: same architecture (ARM/x86 IEEE-754 divergence constraint) | ADR-0011 D2 ✅ |
| TR-WS-034 | F-3 uses only clamp and linear ops (no transcendentals) for platform determinism | ADR-0011 D2 ✅ |
| TR-WS-035 | Future F-3 changes introducing transcendentals MUST document platform-determinism impact | ADR-0011 D2 ✅ |

**Coverage: 35 / 35 requirements traced to ADR-0011. Untraced requirements: None.**

> **GDD status note**: `design/gdd/wave-spawner-pattern-library.md` is in "In Review"
> (R3a applied 2026-06-22; R4 confirmation pending). ADR-0011 is Accepted (2026-08-16),
> which serves as architectural ratification of the GDD's core decisions. Epic created
> with this advisory noted — stories may reference GDD sections that evolve before R4.

## Definition of Done

This epic is complete when:
- All stories are implemented, reviewed, and closed via `/story-done`
- All acceptance criteria from `design/gdd/wave-spawner-pattern-library.md` (H.1–H.7, 38 ACs) are verified
- All Logic and Integration stories have passing test files in `Source/SLIPSTORM/Tests/`
- Cook-time validator story is implemented or explicitly deferred with a risk note
- All 35 TR-WS requirements have corresponding story coverage
- `APullWaveSubsystemActor::Construct()` is callable end-to-end from the Wave Spawner subsystem

## Stories

| # | Story | Type | Status | ADRs |
|---|-------|------|--------|------|
| 001 | [Subsystem Class, Object Pool, Three-Pool Structure](story-001-subsystem-class-and-object-pool.md) | Logic | **Complete** | ADR-0005, ADR-0011 D1 |
| 002 | [Six-State Lifecycle, Phase Drain, Atomic Pool Swap](story-002-six-state-lifecycle.md) | Logic | **Complete** | ADR-0011 D3 |
| 003 | [Admission Gate + Cadence Gate (F-3b) + Primer Bypass](story-003-admission-gate-and-primer-bypass.md) | Logic | **Complete** | ADR-0011 D2 |
| 004 | [Barrage Atomic Admission + barrage_owed Reservation](story-004-barrage-atomic-admission.md) | Logic | **Complete** | ADR-0011 D2 |
| 005 | [Pattern Draw + Cadence Governor (F-3) + RNG Seeding](story-005-pattern-draw-and-cadence-governor.md) | Logic | Ready | ADR-0011 D2 |
| 006 | [Despawn Pipeline + IWaveSpawnerCallback + Seam 13](story-006-despawn-pipeline-and-seam-13.md) | Integration | Ready | ADR-0011 D3 |
| 007 | [RSM/DPC Integration — Pause Flush, Run Termination, Snapshot Immutability](story-007-rsm-dpc-integration.md) | Integration | Ready | ADR-0011 D3, ADR-0007, ADR-0008 |
| 008 | [Cook-Time Validator (14 Binding Rule 15 Checks)](story-008-cook-time-validator.md) | Logic | Ready | ADR-0011 D4 |
| 009 | [Telemetry + Edge-Case Defense + Performance Baseline](story-009-telemetry-and-edge-case-defense.md) | Integration | Ready | ADR-0011 D3 |

## Next Step

Run `/story-readiness production/epics/wave-spawner/story-001-subsystem-class-and-object-pool.md` then `/dev-story` to begin implementation. Work through stories in order — each story's `Depends on:` field tells you what must be Done first.

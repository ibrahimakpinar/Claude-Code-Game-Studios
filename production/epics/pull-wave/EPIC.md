# Epic: Pull-Wave Behavior

> **Layer**: Feature
> **GDD**: `design/gdd/pull-wave-behavior.md` (R10d-closed 2026-06-17; 1039 lines)
> **Architecture Module**: `APullWaveSubsystemActor` — ordered `TArray`-backed pool + five-state machine (ADR-0010) over ISMC renderer (ADR-0006)
> **Status**: Ready
> **Stories**: Not yet created

## Overview

Implements SLIPSTORM's Pull-Wave Behavior system — the magnetized hazard the player reads, leans away from, and slips. Pull-Wave owns each wave instance's per-instance runtime lifecycle: creation, five-state machine (SPAWNED → LEANING → TRAVERSING → LANDED → DESPAWNING), lateral trajectory evaluation via immutable curve snapshot, hit/near-miss resolution at LANDED, and the six-step despawn pipeline. It does NOT own spawn cadence (Wave Spawner / ADR-0011), telegraph rendering (Telegraph System), or audio responses (Audio Controller).

The subsystem is implemented as a deterministic ordered `TArray<FPullWaveInstanceState>`-backed pool on `APullWaveSubsystemActor` (renderer singleton established by ADR-0006), capped at `MAX_CONCURRENT_WAVES_CAP = 16` active waves with a pool of 23 slots (16 cap + 2 despawn-latency headroom + 5 spawner margin). Each wave holds an immutable `FPullWaveCurveSnapshot` (SAMPLE_COUNT=32 locked) captured at Wave Spawner admission; no `UCurveFloat*` reference is held at runtime. Downstream systems (Collision, Telegraph, Audio, VFX, Camera, Death Replay) consume lifecycle events via non-dynamic multicast delegates; no downstream system reads Pull-Wave's internal state directly except PM's `current_lane` read at LANDED via Seam 12.

ADR-0010 was authored Proposed 2026-08-15 and promoted to Accepted 2026-08-16 via paired-promotion pass with ADR-0011 (per the pragmatic-promotion precedent established for ADR-0009 Accepted despite ADR-0002 Proposed). Greenfield implementation — no prior Pull-Wave runtime code to migrate.

## Governing ADRs

| ADR | Role | Decision Summary | Status | Engine Risk |
|-----|------|------------------|--------|-------------|
| **ADR-0010: Pull-Wave Object Pool, State Machine, and Despawn Pipeline** | **Primary** | `TArray<FPullWaveInstanceState>` pool (Reserve(23)), five-state machine `EPullWaveState`, `FPullWaveCurveSnapshot` value-type snapshot, six-step despawn pipeline (Rule 13), pause-flush queued-to-next-tick, `FPullWaveSpawnParams` handoff contract | Accepted (2026-08-16) | LOW |
| **ADR-0006: Pull-Wave Instanced Renderer** | Parent (renderer) | `APullWaveSubsystemActor` singleton owns `WaveMassISMC` + `TrailCubeISMC`; pool slot storage lives on this actor; `PerInstanceCustomData` slots locked | Accepted (2026-06-24) | MEDIUM |
| **ADR-0011: Wave Spawner Pattern Library** | Sibling (produces handoff) | Wave Spawner admission pipeline produces `FPullWaveSpawnParams`; calls `PullWave->Construct(SpawnParams)` at Stage 4 — handoff contract closed by ADR-0010 D6 | Accepted (2026-08-16) | LOW |
| **ADR-0007: Run State Machine Hosting** | Upstream (RSM) | `GetIsPaused()` + `OnPausedChanged` multicast delegate; Pull-Wave binds via `AddUObject` at BeginPlay | Accepted (2026-06-26) | MEDIUM |
| **ADR-0008: DPC Subsystem Hosting** | Upstream (DPC) | `GetCurrentFrameState().telegraph_window_s` + `is_active` read ONCE at SPAWNED entry | Accepted (2026-06-26) | LOW |
| **ADR-0009: Player Movement Component Hosting** | Upstream (PM) | `IPlayerMovementProvider::GetCurrentLane()` (Seam 12) read at LANDED entry for near-miss detection | Accepted (2026-07-09) | MEDIUM |
| **ADR-0005: Wave Spawner Subsystem Hosting** | Transitive dep | Pool sizing (23 slots) aligned with `AWave` pool sizing; pragmatic-promotion precedent applies — ADR-0005 Proposed does NOT block Pull-Wave stories (ADR-0010 is Accepted; ADR-0010 directly governs all 9 stories) | Proposed | — |

## GDD Requirements

ADR-0010 covers 12 of the 29 TR-PW requirements. TR-PW-011–013, 022–024, 027 are renderer requirements covered by ADR-0006 (separate rendering scope). Remaining TR-IDs are design constants, cross-system forward contracts (Wave Spawner epic), or PM propagation items.

| TR-ID | Requirement (one-line) | ADR Coverage |
|-------|------------------------|--------------|
| TR-PW-001 | Lane count fixed at 5 (indices 0–4); binding contract for PM | PM epic (propagated via `/propagate-design-change`) |
| TR-PW-002 | Five-state lifecycle SPAWNED→LEANING→TRAVERSING→LANDED→DESPAWNING | ADR-0010 ✅ (D2) |
| TR-PW-003 | TELEGRAPH_WINDOW_FLOOR_S = 0.70s; locked per CD ruling R10d | ADR-0010 ✅ (D5) |
| TR-PW-004 | Curve snapshot immutable at SPAWNED; FPullWaveCurveSnapshot SAMPLE_COUNT=32 | ADR-0010 ✅ (D3) |
| TR-PW-005 | Active-wave list must be TArray<FPullWaveInstanceState> with RemoveAt | ADR-0010 ✅ (D1) |
| TR-PW-006 | wave_id monotonically increasing int32; iteration ASCENDING per Rule 14(b) | ADR-0010 ✅ (D1) |
| TR-PW-007 | FPullWaveInstanceState size 188 bytes ≤ 256-byte ceiling | ADR-0010 ✅ (D3) |
| TR-PW-008 | AC-PW-22b pattern 12: FORBID inlined UCurveFloat literals | ADR-0010 ✅ (AC-PW-22b p12) |
| TR-PW-009 | LeanEaseCurve_Canonical pinned RCIM_Cubic + RCTM_Auto (R10b 2026-06-16) | ADR-0006 (cook-time asset spec) |
| TR-PW-010 | Object pool sizing: pool ≥ 23 (16 cap + 2 latency slots + 5 margin) | ADR-0010 ✅ (D1) |
| TR-PW-011 | Wave-mass ISMC + Trail-cube ISMC = 2 batched draw calls at PEAK | ADR-0006 ✅ |
| TR-PW-012 | PerInstanceCustomData[0]=LeanCharge, [1]=NearMissFlash, [2]=VoxelDissolve | ADR-0006 ✅ |
| TR-PW-013 | Trail-cube ISMC PerInstanceCustomData[0]=TrailAlpha | ADR-0006 ✅ |
| TR-PW-014 | Forward velocity frozen at SPAWNED; no mid-flight updates | ADR-0010 ✅ (D2 SPAWNED-const, D6) |
| TR-PW-015 | TraverseElapsedS canonical time source; pause excluded; no resume teleport | ADR-0010 ✅ (D2 TRAVERSING) |
| TR-PW-016 | Despawn pipeline ordered 6-step: Coll→Tel→OnWaveDespawned→hide→clear→pool | ADR-0010 ✅ (D4) |
| TR-PW-017 | Seam 13 OnDespawnedUserCallback slot for test-stub observability (R10c) | ADR-0010 ✅ (D4 + Seam 13 ext.) |
| TR-PW-018 | MIN_ESCAPE_SLIPS=2 (registry constant; decoupled from MAX_PULLS=3) | GDD-owned constant (static_assert site in AC-PW-22b) |
| TR-PW-019 | AC-PW-PEAK-NO-ADJACENT-CLUSTER BLOCKING; 7 surviving M=3 triplets | Wave Spawner epic (forward contract; ADVISORY until WS authored) |
| TR-PW-020 | LEAN_ANGLE_MIN_TIER_GAP_DEG=3.0; static_assert per tier pair (R7 B6) | ADR-0010 ✅ (AC-PW-22b p7) |
| TR-PW-021 | Tick ordering chain: RSM→DPC→PM→Pull-Wave→Telegraph→Collision | ADR-0010 ✅ (D5) |
| TR-PW-022 | Mobile Forward renderer only; no Nanite/Lumen | ADR-0006 ✅ |
| TR-PW-023 | AC-PW-22a per-tick budget ≤0.25ms; MAX_CONCURRENT_WAVES_CAP=16 | ADR-0006 + ADR-0010 ✅ (D2 perf budget) |
| TR-PW-024 | Alpha-tested (cutout) motion trail, NOT translucent | ADR-0006 ✅ |
| TR-PW-025 | NEAR_MISS_FLASH_DURATION_S=0.066s time-based (registry); WAVE_DESPAWN_HOLD_S=0.15s | GDD-owned constant |
| TR-PW-026 | F-BARRAGE-SURVIVABILITY-INVARIANT scoped to tier≥2; tier-1 PEAK barrages forbidden | Wave Spawner epic (forward contract; ADVISORY until WS authored) |
| TR-PW-027 | bTeleport/SetCullDistances not used; trajectory deterministic via position formula | ADR-0006 ✅ |
| TR-PW-028 | PM SLIP_TWEEN_DURATION_S safe range tightened [0.10, 0.15]s | PM epic (forward contract; covered by AC-PW-SLIP-TWEEN-CONSISTENCY) |
| TR-PW-029 | SPAWN_PLANE_Z_OFFSET_M default 15.0m; combined with velocity sets TRAVERSING duration | GDD tuning knob (governs F-TRAVERSE-DURATION formula; no ADR needed) |

## Stories

| # | Story | Type | Status | ADR |
|---|-------|------|--------|-----|
| 001 | [Struct Definitions — FPullWaveInstanceState, FPullWaveCurveSnapshot, FPullWaveSpawnParams](story-001-struct-definitions.md) | Logic | Ready | ADR-0010 |
| 002 | [Pool Storage + WaveId Allocation](story-002-pool-storage-waveid.md) | Logic | Ready | ADR-0010 |
| 003 | [Five-State Machine + TransitionTo() Helper](story-003-state-machine.md) | Logic | Ready | ADR-0010 |
| 004 | [Per-Tick Advance (SPAWNED→TRAVERSING) + Pause-Freeze Gate](story-004-tick-advance-traversing.md) | Integration | Ready | ADR-0010 |
| 005 | [LANDED Entry + CollisionOutcome + Hit/Near-Miss Broadcasts](story-005-landed-collision-outcome.md) | Integration | Ready | ADR-0010 |
| 006 | [Rule 13 Six-Step Despawn Pipeline](story-006-despawn-pipeline.md) | Integration | Ready | ADR-0010 |
| 007 | [Pause-Flush (Queued-to-Next-Tick) + bPauseFlushPending Gate](story-007-pause-flush.md) | Integration | Ready | ADR-0010 |
| 008 | [Run-Termination Drain Semantics](story-008-run-termination-drain.md) | Integration | Ready | ADR-0010 |
| 009 | [Construct() Entry Point + Wave Spawner Integration](story-009-construct-entry-point.md) | Integration | Ready | ADR-0010 + ADR-0011 |

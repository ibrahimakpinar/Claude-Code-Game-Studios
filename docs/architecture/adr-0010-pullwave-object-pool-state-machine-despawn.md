# ADR-0010: Pull-Wave Object Pool, State Machine, and Despawn Pipeline

## Status
Accepted

## Date
2026-08-16 (authored Proposed 2026-08-15; promoted Proposed → Accepted 2026-08-16 via paired-promotion pass with ADR-0011 per scoped `/architecture-review single-gdd design/gdd/pull-wave-behavior.md` verdict — no blocking coverage gaps in Pull-Wave GDD, no cross-ADR conflicts detected, hard dependency ADR-0006 satisfied 2026-06-24; transitive ADR-0005 Proposed dependency resolved via same pragmatic-promotion precedent that landed ADR-0009 Accepted 2026-07-09 despite ADR-0002 Proposed)

## Engine Compatibility

| Field | Value |
|-------|-------|
| **Engine** | Unreal Engine 5.7 |
| **Domain** | Feature (per-instance runtime lifecycle) |
| **Knowledge Risk** | LOW — ADR-0006 (Pull-Wave Instanced Renderer) already validated the rendering surface (ISMC + PerInstanceCustomData + Mobile Forward). ADR-0010 adds a pure C++ struct-based state machine + `TArray` pool + delegate broadcast pattern; no new engine surface. |
| **References Consulted** | `docs/engine-reference/unreal/VERSION.md`; ADR-0006 (renderer + component ownership); ADR-0005 (Wave Spawner subsystem hosting + `AWave` pool size); ADR-0007 (RSM `OnPausedChanged` + `GetIsPaused`); ADR-0008 (DPC `GetCurrentFrameState`); ADR-0009 (PM `current_lane` read seam); ADR-0011 (Wave Spawner admission pipeline — consumes `FPullWaveSpawnParams`); `design/gdd/pull-wave-behavior.md` R14-closed (2026-06-19); `design/gdd/player-movement.md` R11a-11 |
| **Post-Cutoff APIs Used** | None. `TArray<T>` with `RemoveAt` (stable pre-UE-4); `DECLARE_MULTICAST_DELEGATE_*` (stable pre-UE-4); `UPROPERTY() TObjectPtr<T>` (UE 5.0+ modernization already validated by sibling ADRs); `check()`/`ensure()` asserts (stable pre-UE-4). |
| **Verification Required** | ADR-0006 must be Accepted before ADR-0010 → Accepted (renderer component ownership is prerequisite for pool-return semantics). No new engine-surface verification items. |

## ADR Dependencies

| Field | Value |
|-------|-------|
| **Depends On** | ADR-0006 (renderer + `APullWaveSubsystemActor` ownership — pool storage lives on this actor); ADR-0005 (Wave Spawner hosts the admission decision that calls `Construct(FPullWaveSpawnParams)`); ADR-0007 (RSM `OnPausedChanged(bool)` + `GetIsPaused()` — consumed by pause-freeze + pause-flush semantics); ADR-0008 (DPC `GetCurrentFrameState().telegraph_window_s` — read at SPAWNED entry); ADR-0009 (PM `current_lane` read seam per Seam 12 — consumed at LANDED tick for near-miss detection); ADR-0011 (Wave Spawner admission pipeline — produces `FPullWaveSpawnParams` snapshot passed into `Construct()`) |
| **Enables** | `/create-stories pull-wave` — Pull-Wave epic story creation. Also closes the "Future ADR-0010" forward references in ADR-0011 §Consequences/Risks and `docs/architecture/requirements-traceability.md` line 151. |
| **Blocks** | Pull-Wave epic (cannot start story authoring until Accepted); Wave Spawner epic-Done (ADR-0011 explicitly gates on ADR-0010 landing to close the `FPullWaveSpawnParams` snapshot handoff). |
| **Ordering Note** | Sibling of ADR-0011 (Wave Spawner pattern library) — both Feature-layer ADRs that bundle multiple sub-decisions in one doc. ADR-0011 was authored Proposed 2026-08-12; ADR-0010 (this doc) is authored Proposed 2026-08-15. Both may promote to Accepted under the same pragmatic-promotion pass if ADR-0005/0006 remain Proposed (ADR-0006 is Accepted 2026-06-24 per its Status; ADR-0005 is Proposed pending Foundation HW-verification at ADR-0001). |

## Context

### Problem Statement

The Pull-Wave subsystem (renderer + component ownership locked in ADR-0006) is the per-instance runtime authority for every wave in flight during a SLIPSTORM run: it owns the wave's five-state lifecycle (SPAWNED → LEANING → TRAVERSING → LANDED → DESPAWNING), the per-wave immutable snapshot of the source lateral-offset curve, the pool that backs `MAX_CONCURRENT_WAVES_CAP = 16` concurrent waves plus admission headroom, and the fixed-order despawn pipeline that unregisters the wave from Collision + Telegraph + downstream subscribers before returning its slot to the pool.

ADR-0006 documents the renderer (ISMC vs HISM decision + `PerInstanceCustomData` slot count + cull distances + `APullWaveSubsystemActor` singleton ownership). It explicitly defers the pool container type, the state machine, the curve-snapshot format, the despawn-pipeline sequencing, and the cross-system tick-order to a future ADR — this ADR.

Without a documented architecture for these decisions, story authoring at `/create-stories pull-wave` cannot proceed: programmers would derive per-instance lifecycle behavior directly from GDD prose spanning 5 states, 3 forced-drain reasons, and ~15 rules across the Wave Spawner / RSM / DPC / PM / Telegraph / Collision boundaries, with no cross-story stance to check work against. ADR-0011 (Wave Spawner admission pipeline, Proposed 2026-08-12) additionally has a forward reference to this ADR for the `FPullWaveSpawnParams` snapshot contract that Wave Spawner produces at admission and Pull-Wave consumes at `Construct()` — that reference is currently symbolic and needs an ADR to bind.

### Constraints

- **Renderer locked** (ADR-0006): `APullWaveSubsystemActor` singleton owns `WaveMassISMC` (16 instances at PEAK) + `TrailCubeISMC` (48 instances at PEAK). Pool storage lives on this actor. ADR-0010 must not re-decide any renderer topology.
- **Concurrency cap enforced upstream** (Wave Spawner via DPC): `MAX_CONCURRENT_WAVES_CAP = 16` per GDD Rule 12. Pool sizing must exceed this by admission-headroom margin.
- **Pool-size derivation** (Rule 10 + ADR-0005 alignment): pool ≥ 23 slots = 16 concurrent + 2 admission headroom + 5 for spawner-owned pre-allocation. ADR-0005's `AWave` pool at 23 slots is the ordering-authoritative source.
- **Per-tick budget** (AC-PW-22a): total Pull-Wave per-tick CPU ≤ 0.25 ms on mid-tier mobile. ADR-0010's per-tick advance (pause gate + per-wave state update + curve evaluation + cross-system event broadcast) fits within this budget.
- **Cache-line-friendly instance state** (AC-PW-22b): `sizeof(FPullWaveInstanceState) ≤ 256` bytes. GDD line 182-184: measured 188 bytes with `SAMPLE_COUNT = 32` (60 scalar + 128 Samples), 68 bytes headroom. `static_assert` enforces at compile.
- **Iteration order** (Rule 14(b)): active-wave list iteration MUST be `wave_id` ASCENDING per-tick. `wave_id` is monotonically increasing, never reused. `TArray::RemoveAtSwap` breaks this and is forbidden; `RemoveAt` preserves order.
- **Immutable per-wave params** (Rule 14(a)): `FPullWaveCurveSnapshot` is a value-type copy sampled at admission time; no asset reference is held at runtime. Snapshot is captured in `FPullWaveSpawnParams` by Wave Spawner and copied into `FPullWaveInstanceState` at `Construct()`. Never mutated after.
- **Pause-freeze semantic** (RSM Rule + Pull-Wave GDD §States line 108): when `RSM.GetIsPaused() == true`, all accumulators (`LeanProgress`, `TraverseElapsedS`, `LandedHoldElapsedS`) freeze; state enum unchanged; visual + collision stay live but forward position does not advance.
- **Pause-flush is queued-to-next-tick** (R7 B13 binding, GDD line 110): `RSM.OnPausedChanged(false)` from RUNNING routes all in-flight waves to DESPAWNING at the TOP of Pull-Wave's next tick, NOT inline mid-tick. Avoids AC-PW-15 contiguity violation.
- **Single despawn-pipeline sequence** (Rule 13): collision-unregister → telegraph-unregister → OnWaveDespawned broadcast → hide (mesh alpha to 0) → clear (instance data slot) → pool-return. Order is fixed; deviations break Collision/Telegraph observability contracts.
- **Single Telegraph unregister call site** (R6 B5, GDD line 231): `Telegraph->UnregisterWave(wave_id)` fires exactly once per wave, at DESPAWNING entry via Rule 13 step 2. Not at LEANING exit (that's per-tick read cessation, distinct concept).
- **Tick ordering** (GDD line 239): `RSM → DPC → PM → Pull-Wave → Telegraph → Collision`. Bound by the tick-prerequisite mechanism resolved in the DPC-hosting ADR chain (ADR-0007 + ADR-0008).

### Requirements

- **Five-state lifecycle**: `EPullWaveState { SPAWNED, LEANING, TRAVERSING, LANDED, DESPAWNING }` with per-state entry/exit triggers and observable snapshot fields per GDD §States table (line 100-106).
- **Object pool with ordered iteration**: `TArray<FPullWaveInstanceState>` of size ≥ 23; `wave_id` monotonic int32; iteration ASCENDING (Rule 14(b)); `RemoveAt` (not `RemoveAtSwap`); `wave_id` never reused within a session.
- **Immutable curve snapshot**: `FPullWaveCurveSnapshot { float Samples[32]; float EvaluateAt(float) }`; sampled by Wave Spawner at admission; copied by value into instance state; runtime linear interpolation only; no `UCurveFloat*` reference held.
- **Spawn params contract**: `FPullWaveSpawnParams { WaveId, SourceLane, TargetLane, CurveSnapshot, ForwardVelocityMs, SpawnTimeS, LeanDurationS }` — sole entry point for Wave Spawner → Pull-Wave handoff (GDD line 119-128).
- **Pause-freeze**: RSM.GetIsPaused() gated accumulator advance; state enum unchanged during pause; visual + collision live but forward position frozen.
- **Pause-flush (queued-to-next-tick)**: `RSM.OnPausedChanged(false)` from RUNNING sets `bPauseFlushPending = true` synchronously in handler; consumed at TOP of next Pull-Wave tick; all LEANING/TRAVERSING/LANDED waves routed to DESPAWNING with `DespawnReason = PauseFlush` as an atomic batch within that tick (R7 B13).
- **Run-termination drain**: RSM RUNNING → DEAD/COMPLETE/ABORTED — in-flight waves continue advancing through TRAVERSING → LANDED → DESPAWNING with `DespawnReason = RunTermination`; DPC's `is_active = false` halts new spawns; LANDED collision events still fire (Collision GDD post-RUNNING policy).
- **Despawn pipeline (Rule 13)**: fixed sequence at DESPAWNING entry — (1) collision unregister → (2) Telegraph->UnregisterWave(wave_id) → (3) OnWaveDespawned broadcast → (4) mesh hide (ISMC alpha=0 via PerInstanceCustomData) → (5) instance data clear → (6) pool slot return. All 6 steps run within the 1-tick DESPAWNING duration.
- **Cross-system read gates**: Pull-Wave reads `DPC.telegraph_window_s` ONCE at SPAWNED entry (never again); reads `PM.current_lane` per-tick during LANDED via Seam 12 (`IPlayerMovementProvider`); does NOT subscribe to `PM.OnSlipMidpoint` (superseded per Rule 11, R1 RC-A).
- **Broadcast delegates for downstream systems**: `OnWaveHit(WaveId, ...)`, `OnNearMiss(WaveId, ...)`, `OnWaveDespawned(WaveId, DespawnReason)`, `OnLeanProgress(WaveId, LeanProgressNormalized)`, `OnTelegraphUnregistered(WaveId)`. Subscriber set is open (Audio Controller, VFX, Camera, Collision, Telegraph, Death Replay). Pull-Wave does not enumerate subscribers.

## Decision

The Pull-Wave subsystem implements per-instance runtime lifecycle as a **five-state deterministic machine** over an **ordered `TArray`-backed pool** of `FPullWaveInstanceState` structs, driven per-tick by the `APullWaveSubsystemActor`'s tick body (subscribed per ADR-0006 renderer topology). Each wave is instantiated via `Construct(FPullWaveSpawnParams)` from Wave Spawner, holds an immutable `FPullWaveCurveSnapshot` for its lifetime, advances through the five states with pause-freeze and queued-to-next-tick pause-flush semantics, and terminates via the fixed six-step despawn pipeline at DESPAWNING entry. Downstream systems (Collision, Telegraph, Audio, VFX, Camera, Death Replay) consume lifecycle events via multicast delegates; no downstream system reads Pull-Wave's internal state directly except PM's `current_lane` read at LANDED per Seam 12.

### D1 — Ordered Object Pool

Pool storage is a `UPROPERTY() TArray<FPullWaveInstanceState>` on `APullWaveSubsystemActor` (owner locked by ADR-0006). Sizing and iteration semantics:

- **Pool capacity**: `TArray::Reserve(23)` at BeginPlay. 23 = `MAX_CONCURRENT_WAVES_CAP = 16` + 2 admission headroom + 5 Wave-Spawner-owned pre-allocation (per ADR-0005 `AWave` pool alignment). No dynamic resize permitted during a run; `Reserve` locks capacity to avoid mid-tick reallocation.
- **`WaveId` allocation**: monotonically increasing `int32`, assigned at `Construct()` from a subsystem-owned counter. Never reused within a session (Rule 14 ordering). Counter resets at BeginPlay only.
- **Iteration order**: ASCENDING by `WaveId` per-tick, per Rule 14(b). Since new waves are added via `Add()` (append-to-tail) and `WaveId` is monotonic, natural array order equals `WaveId` ASC. Explicit sort not required.
- **Removal semantics**: `TArray::RemoveAt(Index)` — NOT `RemoveAtSwap`. `RemoveAtSwap` breaks `WaveId` ASC iteration by moving the last element to the removed slot. Forbidden. AC-PW-22b pattern 11 greps for `RemoveAtSwap` on the active-wave list source and fails CI.
- **Slot return timing**: `RemoveAt(Index)` fires at the end of DESPAWNING (Rule 13 step 6). Between DESPAWNING entry and RemoveAt, the slot is still present in the array with `State == DESPAWNING` — this is intentional so that same-tick multi-despawn iterations remain observable to AC-PW-15's contiguity assertion.

### D2 — Five-State Lifecycle Machine

The per-wave state machine is `EPullWaveState { SPAWNED, LEANING, TRAVERSING, LANDED, DESPAWNING }`. Transitions are enforced by `check()` at each transition site; forbidden transitions abort in non-Shipping and log `pull_wave_illegal_transition` telemetry + no-op in Shipping.

| State | Entry Trigger | Exit Trigger(s) | Exit → | Advance Behavior (per tick, if `!RSM.GetIsPaused()`) |
|-------|---------------|-----------------|--------|------------------------------------------------------|
| `SPAWNED` | `Construct(FPullWaveSpawnParams)` from Wave Spawner | Next tick (unconditional) | LEANING | Set observable snapshot fields (WaveId, SourceLane, TargetLane); read `DPC.telegraph_window_s` ONCE; assert `DPC.is_active == true`. |
| `LEANING` | SPAWNED exit | `LeanProgress >= 1.0` OR pause-flush OR run-termination | TRAVERSING / DESPAWNING | `LeanProgress += DeltaTime / LeanDurationS`; clamp to `[0, 1]`; fire `OnLeanProgress(WaveId, LeanProgress)`. |
| `TRAVERSING` | LEANING exit; `LeanProgress = 0` cleared | `WorldZ <= player_plane_z` (threshold-cross) OR pause-flush OR run-termination | LANDED / DESPAWNING | `TraverseElapsedS += DeltaTime`; compute `WorldX` via F-TRAJ-LATERAL(EvaluateAt(t_norm)); compute `WorldZ` via F-TRAJ-FORWARD; update ISMC instance transform. |
| `LANDED` | TRAVERSING exit; `TraverseElapsedS` clamped to `TravelDurationS`; hold at `t_norm = 1.0` | End of `WAVE_DESPAWN_HOLD_S` hold OR pause-flush | DESPAWNING | `LandedHoldElapsedS += DeltaTime`; on entry read `PM.current_lane` per Seam 12 to compute `CollisionOutcome ∈ {Hit, NearMiss, CleanMiss}`; fire `OnWaveHit` or `OnNearMiss` as applicable. |
| `DESPAWNING` | Any natural or forced exit; `DespawnReason` set at entry | Cleanup complete (1 tick) | (pool slot returned via `RemoveAt`) | Execute Rule 13 six-step despawn pipeline (D4); no per-tick advance body; pool slot returned at end of tick. |

**Forbidden transitions** (`check()` at `TransitionTo()` entry site — non-Shipping asserts, Shipping logs telemetry + no-ops):

- `SPAWNED → TRAVERSING` / `SPAWNED → LANDED` / `SPAWNED → DESPAWNING`  (must pass through LEANING; DESPAWNING allowed only via pause-flush pathway from LEANING)
- `LEANING → LANDED`  (must pass through TRAVERSING; the only LEANING → DESPAWNING skip is pause-flush/run-term)
- `TRAVERSING → SPAWNED` / `LANDED → LEANING` / `DESPAWNING → *`  (backwards + terminal transitions forbidden)
- Any `* → SPAWNED` after initial `Construct()`  (SPAWNED is an entry-only state)

**Pause-freeze semantic**: `if (RSM.GetIsPaused()) { return; }` guard at the TOP of Pull-Wave's per-wave tick body — before the state-machine dispatch. `LeanProgress`, `TraverseElapsedS`, `LandedHoldElapsedS` all frozen. Visual (ISMC transform) stays at last-tick values; collision registration stays live; no forward advance means no spurious LANDED-entry `OnWaveHit` broadcasts fire during pause.

**Pause-flush (queued-to-next-tick per R7 B13)**: subscription to `RSM.OnPausedChanged(bool bIsPaused)` in `BeginPlay` (via `AddUObject` per ADR-0009 IG-3 rule; PM's outbound-binding rule applies here too — subsystem outbound bindings use `AddUObject`, not lambdas). The handler sets `bPauseFlushPending = true` when `bIsPaused == false` AND RSM was in RUNNING. At the TOP of the next Pull-Wave tick (BEFORE the per-wave iteration loop), if `bPauseFlushPending == true`: every active wave in LEANING / TRAVERSING / LANDED transitions to DESPAWNING with `DespawnReason = PauseFlush` as an atomic batch within that flush tick. `bPauseFlushPending` reset to false. All flushed waves execute Rule 13 pipeline in `WaveId ASC` order within the flush tick; `AC-PW-15` contiguity is preserved.

**Run-termination drain** (RSM RUNNING → DEAD | COMPLETE | ABORTED): in-flight waves continue advancing through TRAVERSING → LANDED → DESPAWNING with `DespawnReason = RunTermination`. DPC's `is_active` becomes false — Wave Spawner halts new spawns. LANDED-tick collision events still fire and are dispatched via `OnWaveHit` / `OnNearMiss` (Collision GDD post-RUNNING policy owns whether they score against the just-ended run). Distinct from PauseFlush: run-termination waves are permitted to reach LANDED naturally; pause-flush waves skip straight to DESPAWNING.

### D3 — Immutable Curve Snapshot (`FPullWaveCurveSnapshot`)

Per-wave lateral trajectory is driven by a value-type curve snapshot copied into `FPullWaveInstanceState` at `Construct()`. Never mutated after; no `UCurveFloat*` runtime reference held.

```cpp
struct FPullWaveCurveSnapshot
{
    static constexpr int32 SAMPLE_COUNT = 32;  // LOCKED — not a tuning knob.
                                               // Step = 1/31 ≈ 0.032 t_norm.
                                               // Linear interp error ≤ 0.001
                                               // (5× under CURVE_ENDPOINT_TOLERANCE = 0.005).

    float Samples[SAMPLE_COUNT];               // Sampled at t_norm = i / (SAMPLE_COUNT - 1).

    // Pure function; no asset reference held.
    // i0 = (int)(TNormClamped * (SAMPLE_COUNT-1)); i1 = min(i0+1, SAMPLE_COUNT-1).
    // OOB prevented by i1 clamp.
    float EvaluateAt(float TNormClamped) const;
};
```

**`SAMPLE_COUNT = 32` is locked** (not a prototype knob):

- Interpolation error ≤ 0.001 at 32 samples (5× under `CURVE_ENDPOINT_TOLERANCE = 0.005`).
- Struct size 128 bytes for the Samples array; total `FPullWaveInstanceState` = 188 bytes (60 scalar + 128 Samples), 68 bytes headroom under AC-PW-22b's 256-byte ceiling.
- `SAMPLE_COUNT = 16` REJECTED — interpolation error ≈ 0.003 (66% of tolerance; no safety margin for authoring variation).
- `SAMPLE_COUNT = 64` REJECTED — violates AC-PW-22b static_assert (316 bytes > 256).

**Sampling site**: Wave Spawner samples the `UCurveFloat*` (validated at admission per ADR-0011 D2 Stage 4) at 32 evenly-spaced `t_norm` values, populates `FPullWaveCurveSnapshot::Samples`, embeds in `FPullWaveSpawnParams`, hands off to Pull-Wave. Pull-Wave never touches the source asset.

**Runtime evaluation**: linear interpolation between samples. Pure function; no allocations per call.

### D4 — Six-Step Despawn Pipeline (Rule 13)

DESPAWNING entry fires the following fixed-order pipeline within a single tick. Each step is observable in the AC-PW-15 unified event log (per Seam 13); contiguity is enforced per `WaveId` across the six steps.

| Step | Action | Rationale |
|------|--------|-----------|
| 1 | `Collision->UnregisterWave(WaveId)` | Collision stops emitting overlap events for this wave. Must fire before OnWaveDespawned so subscribers see clean "no more events for WaveId" ordering. |
| 2 | `Telegraph->UnregisterWave(WaveId)` — the **single** UnregisterWave call site per R6 B5 | Telegraph stops rendering the lean visual. Single call site (not at LEANING exit — that's per-tick read cessation). |
| 3 | Broadcast `OnWaveDespawned(WaveId, DespawnReason)` | Downstream subscribers (Audio, VFX, Camera, Death Replay) trigger cleanup effects. Fires AFTER Collision + Telegraph unregister so subscribers observe "system state is consistent" ordering. |
| 4 | ISMC hide: `SetCustomDataValue(InstanceIndex, DissolveSlot, 1.0f)` — alpha to 0 via PerInstanceCustomData routing (ADR-0006 renderer contract) | Visual disappears. No `RemoveInstance` here — that would re-index all subsequent instances mid-frame. |
| 5 | Clear `FPullWaveInstanceState` fields to defaults (except `WaveId` which remains for the last-tick observation window) | Prepare slot for post-tick pool return; avoid stale-data reads if a subscriber reads state during the DESPAWNING tick. |
| 6 | `ActiveWaves.RemoveAt(Index)` — pool slot returned; ISMC instance is either `RemoveInstance` (last-in-array optimization) or left with alpha=0 for later reuse (Wave Spawner's next admission overwrites) | Slot returned to pool. Iteration order preserved (`RemoveAt`, not `RemoveAtSwap`). |

**Ordering invariant**: steps 1-5 are synchronous; step 6 runs at end-of-tick to preserve AC-PW-15's contiguity assertion across steps 1-5 for the WaveId within the DESPAWNING tick.

**No idempotence required**: Rule 13 fires exactly once per WaveId. `Collision->UnregisterWave` and `Telegraph->UnregisterWave` are not required to be idempotent — the single-call-site contract per R6 B5 is the guarantee.

### D5 — Cross-System Read/Write Contract

Pull-Wave's boundary with upstream + downstream systems is one-way per direction, tick-ordered per GDD line 239.

**Upstream reads** (Pull-Wave consumes):

| Source | Read | When | Cardinality |
|--------|------|------|-------------|
| `RSM.GetIsPaused()` | bool | Every tick, at top of per-wave body | Per-wave-per-tick |
| `RSM.OnPausedChanged(bool)` | delegate | Bound at BeginPlay via `AddUObject` (ADR-0009 IG-3 rule); handler sets `bPauseFlushPending` | Event-driven |
| `RSM.current_state` | ERunState | Read at `bPauseFlushPending` consumption tick to distinguish RUNNING vs ABORTED (only RUNNING triggers flush per Rule 19) | On flush |
| `DPC.GetCurrentFrameState().telegraph_window_s` | float | ONCE per wave at SPAWNED entry — clamped to `[TELEGRAPH_WINDOW_FLOOR_S, ∞)` and captured into `LeanDurationS`; never re-read | Once per wave |
| `DPC.GetCurrentFrameState().is_active` | bool | ONCE per wave at SPAWNED entry — assert-and-skip if false (Wave Spawner should not admit when DPC is inactive; this is a safety check) | Once per wave |
| `PM.current_lane` via Seam 12 `IPlayerMovementProvider::GetCurrentLane()` | int32 | Read at LANDED entry tick for `CollisionOutcome` determination (Hit / NearMiss / CleanMiss) | Once per wave (at LANDED) |
| Wave Spawner `Construct(FPullWaveSpawnParams)` | function call | Sole entry point; called by Wave Spawner admission pipeline (ADR-0011 D2 Stage 4) | Once per wave |

**Forbidden reads** (explicit — story authors MUST NOT wire these):

- `PM.OnSlipMidpoint` delegate — superseded per Rule 11 (R1 RC-A). Pull-Wave near-miss detection reads `PM.current_lane` directly at LANDED tick; the midpoint delegate is not consumed.
- `DPC.current_phase`, `DPC.wave_spawn_interval_s`, `DPC.max_concurrent_waves` — Wave Spawner's domain; Pull-Wave has no reason to read these.
- `RSM.RunSeed` — consumed at Wave Spawner Cold → Active per ADR-0011 D2 Stage 4; Pull-Wave has no RNG surface.

**Downstream broadcasts** (Pull-Wave produces):

| Delegate | Signature | Fired At | Consumers (open subscriber set) |
|----------|-----------|----------|---------------------------------|
| `OnLeanProgress` | `(WaveId, LeanProgressNormalized)` | Each LEANING tick | Telegraph System (optional; poll pattern alternative permitted per GDD line 231) |
| `OnWaveHit` | `(WaveId, ...)` | LANDED entry when Hit | Collision, RSM (drives RSM→DEAD), Audio, VFX, Camera, Death Replay |
| `OnNearMiss` | `(WaveId, ...)` | LANDED entry when NearMiss | PM.TriggerNearMissBeat, Audio, VFX, Camera, Death Replay |
| `OnWaveDespawned` | `(WaveId, DespawnReason)` | Rule 13 step 3 | Audio, VFX, Camera, Death Replay |
| `OnTelegraphUnregistered` | `(WaveId)` | Rule 13 step 2 (fires from inside `Telegraph->UnregisterWave`) — separately observable for AC-PW-15 unified event log per Seam 13 | Test-observable event log |

**Delegate types**: non-dynamic `DECLARE_MULTICAST_DELEGATE_*` (not BP-exposed dynamic multicast). Pattern matches ADR-0007 RSM `OnStateChanged` / `OnPausedChanged` and ADR-0011 Wave Spawner subscriber-contract convention.

### D6 — `FPullWaveSpawnParams` Snapshot Contract

The sole entry point for Wave Spawner → Pull-Wave handoff. Immutable value-type struct; passed by copy at `Construct()`; Pull-Wave copies fields into `FPullWaveInstanceState`.

```cpp
struct FPullWaveSpawnParams
{
    int32                   WaveId;              // Monotonic; assigned by Wave Spawner from
                                                 // subsystem counter. Never reused.
    int32                   SourceLane;          // [0, 4]
    int32                   TargetLane;          // [0, 4]
    FPullWaveCurveSnapshot  CurveSnapshot;       // Immutable per Rule 14(a); sampled by
                                                 // Wave Spawner at admission from validated
                                                 // UCurveFloat asset.
    float                   ForwardVelocityMs;   // ∈ [PULL_WAVE_VELOCITY_MIN_MS,
                                                 //    PULL_WAVE_VELOCITY_MAX_MS]
    float                   SpawnTimeS;          // Absolute game-time at Wave Spawner admission
    float                   LeanDurationS;       // = clamp(DPC.telegraph_window_s,
                                                 //         TELEGRAPH_WINDOW_FLOOR_S, ∞)
                                                 // Captured by Wave Spawner from DPC snapshot
                                                 // at admission (NOT re-read at SPAWNED entry).
};
```

**Producer**: Wave Spawner admission pipeline (ADR-0011 D2 Stage 4). Wave Spawner:
1. Draws pattern via cadence governor.
2. Reads `DPC.GetCurrentFrameState()` for `telegraph_window_s`.
3. Samples validated `UCurveFloat*` into `FPullWaveCurveSnapshot`.
4. Assigns `WaveId` from subsystem counter.
5. Constructs `FPullWaveSpawnParams` by value.
6. Calls `PullWave->Construct(SpawnParams)`.

**Consumer**: Pull-Wave `Construct()`. Pull-Wave:
1. Acquires slot from pool (`ActiveWaves.Add(FPullWaveInstanceState{})`; slot index returned).
2. Copies `FPullWaveSpawnParams` fields into the instance state (per-field copy).
3. Sets `State = SPAWNED`; initializes `LeanProgress = 0.0`, `TraverseElapsedS = 0.0`, `LandedHoldElapsedS = 0.0`.
4. Computes `TravelDurationS` via F-TRAVERSE-DURATION formula.
5. ISMC add instance at initial transform; `PerInstanceCustomData[0] = 0` (lean tier), `[1] = 0` (near-miss flash), `[2] = 0` (dissolve).

**Immutability contract**: after `Construct()` returns, `FPullWaveSpawnParams` is no longer referenced. Any subsequent Wave Spawner mutation of its local copy (if any) does not affect the Pull-Wave instance. All parameter values are const for the wave's lifetime. This closes ADR-0011's forward-reference to `FPullWaveSpawnParams` snapshot immutability.

**Size budget**: `FPullWaveSpawnParams` = 4 (WaveId) + 4 (SourceLane) + 4 (TargetLane) + 128 (CurveSnapshot) + 4 (ForwardVelocityMs) + 4 (SpawnTimeS) + 4 (LeanDurationS) = 152 bytes. Passed by copy on the stack; no heap allocation.

---

## Alternatives Considered

### Alternative 1: `TSparseArray<FPullWaveInstanceState>` pool with stable indices

- **Description**: Use `TSparseArray` instead of `TArray`. Removal is O(1) (marks the slot free without shifting); insertion reuses free slots; slot indices are stable across removals.
- **Pros**: Faster mid-tick removal (`RemoveAt` on `TArray` is O(N) where N is elements after the removed index — for a 23-slot pool with mid-index removal this is ~10 element shifts). Slot index stability could simplify ISMC-index-to-pool-index mapping.
- **Cons**: Iteration is NOT `WaveId` ASC — sparse array iteration order is insertion-order-with-holes, and hole reuse can produce out-of-order slots. Rule 14(b) `WaveId` ASC iteration is a BINDING contract enforced by AC-PW-10 same-tick-multi-admission assertion; violating it produces observable Pillar 5 bugs (waves-in-flight rendered in nondeterministic order, breaking the "iron-order" read the player uses to pattern-match barrages). Explicit sort per-tick would recover the ordering but adds O(N log N) per-tick sort cost that dominates the `TArray` shift cost at N ≤ 23.
- **Rejection Reason**: Rule 14(b) `WaveId` ASC iteration is design-binding and worth the O(N) removal cost at N=23. The removal cost is bounded (max ~22 element shifts per removal; typical `sizeof(FPullWaveInstanceState) = 188` bytes memcpy per shift; total < ~4 KB memcpy per removal — well under the 0.25 ms per-tick budget). Adding a sort to recover ordering is strictly worse than paying the shift cost inline.

### Alternative 2: `UObject`-per-wave instead of `FPullWaveInstanceState` struct pool

- **Description**: Each wave is a `UPullWaveInstance : public UObject` allocated at admission and reclaimed to a `UObject` pool at DESPAWNING. State fields become `UPROPERTY()` members.
- **Pros**: Blueprint-exposable per-wave state (useful if designers wanted to inspect wave state in-editor). Delegate binding via `AddUObject` (already the discipline enforced by ADR-0009 IG-3 rule) becomes natural for per-wave subscribers.
- **Cons**: `UObject` allocation is a heap allocation with `NewObject<T>()` — even against a pooled `UObject` list this incurs GC-tracker registration cost per acquire. `MAX_CONCURRENT_WAVES_CAP = 16` means up to 16 acquires + 16 releases per PEAK second; measured against AC-PW-22a's 0.25 ms budget, this alone consumes a meaningful fraction. `UPROPERTY` overhead on inline scalar members (WaveId, TargetLane, etc.) adds reflection metadata bloat and per-instance GC scan cost. AC-PW-22b's 256-byte cache-line-friendly ceiling assumes plain struct layout; `UObject` layout adds vtable pointers + `UObjectBase` fields (typically ~64 bytes overhead per instance).
- **Rejection Reason**: `UObject` cost is not justified — Pull-Wave has no reflection/Blueprint requirement at the per-instance level (the `APullWaveSubsystemActor` singleton is the Blueprint-exposable surface; per-wave state is programmer-only). Plain struct pool with `TArray` matches ADR-0007 RSM's transition-history approach and ADR-0011's `FPatternPool` storage discipline (sibling pattern consistency).

### Alternative 3: Inline mid-tick pause-flush (`RSM.OnPausedChanged(false)` fires the flush synchronously in the delegate handler)

- **Description**: When `RSM.OnPausedChanged(false)` fires, Pull-Wave's handler immediately walks the active-wave list and routes every LEANING/TRAVERSING/LANDED wave to DESPAWNING inline. No `bPauseFlushPending` flag; no deferral.
- **Pros**: Zero-latency flush; no per-tick check of `bPauseFlushPending`; simpler control flow.
- **Cons**: Contradicts R7 B13 binding. If `OnPausedChanged(false)` fires mid-tick from an external source (game-mode manager, Wave Spawner-side callback, RSM's own tick body), waves 0..N have already been ticked in the frame (their per-wave state advanced under `is_paused=true` guard → skipped) and waves N+1..15 have NOT yet been ticked. An inline flush walks the array — but the walk interleaves DESPAWNING-entry Rule 13 six-step pipeline events with the in-progress tick's mid-iteration state. Rule 13 step 3 (`OnWaveDespawned` broadcast) firing mid-tick against subscribers whose own tick body has not yet run this frame produces non-deterministic event ordering. AC-PW-15 unified event log's per-wave contiguity assertion fails because DESPAWNING-entry events for waves 0..N interleave with LEANING/TRAVERSING events for waves N+1..15 in the same tick.
- **Rejection Reason**: R7 B13 is BINDING and documented in GDD line 110. The inline-flush pattern breaks AC-PW-15 contiguity and produces observable per-frame race conditions between Pull-Wave and its subscribers. The `bPauseFlushPending` + next-tick batch pattern is worth the one-frame latency to preserve batch-atomic contiguity.

### Alternative 4: Held `UCurveFloat*` reference instead of value-copy `FPullWaveCurveSnapshot`

- **Description**: `FPullWaveInstanceState` holds `TObjectPtr<UCurveFloat> LateralOffsetCurve` (GC-safe pointer) and evaluates the curve at runtime by calling `LateralOffsetCurve->GetFloatValue(t_norm)`.
- **Pros**: No sampling step at admission; no 128-byte Samples array in each instance (drops `FPullWaveInstanceState` from 188 to 60 bytes — 3× cache-line-friendlier). Curve authoring changes take effect on the next admission (no cook-time re-sample step).
- **Cons**: Runtime evaluation calls into `FRichCurve::Eval` which walks the key array and interpolates per call — measured at ~200-800 ns per call in UE 5.7 depending on key count. Pull-Wave calls this once per LEANING tick + once per TRAVERSING tick per wave = ~2 calls × 16 waves × 60 fps = 1920 calls/sec ≈ 384 µs to 1.5 ms per second — approaching the 0.25 ms per-tick budget's cumulative annual cost on a hot path. More critical: the curve asset is a shared `UObject`; if any system mutates the curve mid-run (e.g., tuning tool live-editing), all in-flight waves see the mutation mid-flight, breaking Rule 14(a) immutability and Death Replay determinism (AC-WS-13/14).
- **Rejection Reason**: Rule 14(a) immutability + Death Replay determinism (AC-WS-13/14) are BINDING. Value-copy snapshot at admission is the only way to guarantee "the curve the wave was spawned with is the curve the wave lives and dies with." The 128-byte per-instance memory cost (`23 × 128 = 2944 bytes` total pool overhead) is negligible against the 1.5 GB mid-tier mobile memory ceiling. Runtime cost is O(1) array-lookup + linear-interp (~10-20 ns per call) vs `FRichCurve::Eval`'s ~200-800 ns — snapshot is faster at runtime as a bonus.

## Consequences

### Positive

- **Story-authoring unblocked**: `/create-stories pull-wave` can proceed with concrete lifecycle + pool + despawn stances (per-story TR-PW-NNN rows reference D1–D6 sub-decisions).
- **Cross-story consistency guaranteed**: All Pull-Wave stories share one lifecycle spec — no drift risk from stories inventing their own state transitions or despawn ordering.
- **Determinism preserved**: `FPullWaveCurveSnapshot` value-copy at admission (D3 + D6) + `WaveId` ASC iteration (D1) + fixed despawn pipeline order (D4) together preserve Rule 14 determinism for Death Replay (AC-WS-13/14 same-platform constraint per ADR-0011).
- **Single-call-site contracts documented**: `Telegraph->UnregisterWave` fires exactly once per WaveId at DESPAWNING (D4 step 2 — closes R6 B5); pause-flush fires exactly once per pause-boundary at the top of the next tick (D2 R7 B13); despawn pipeline runs exactly once per WaveId in fixed order (D4).
- **ADR-0011 forward-reference closed**: `FPullWaveSpawnParams` snapshot immutability contract is now bound (D6). ADR-0011's Risks table item "FPullWaveSpawnParams struct not yet authored (owned by ADR-0010)" is resolved on this ADR's Acceptance.
- **Cross-system boundary crisp**: D5's explicit forbidden-reads list (PM.OnSlipMidpoint, DPC.current_phase, RSM.RunSeed) removes ambiguity that GDD prose alone leaves open. Story reviewers can grep for these and fail CI on unauthorized use.

### Negative

- **Bounded-pool hard cap**: `TArray::Reserve(23)` at BeginPlay locks capacity; no dynamic resize. If a future story genuinely needs a larger cap, `MAX_CONCURRENT_WAVES_CAP` widens AND pool size widens in lockstep. Both are cross-system contracts requiring `/propagate-design-change`. Not a design defect — the cap is Pillar-2 binding (choice-reaction task at N=3+ overloads read-and-react) — but the ADR ossifies the pool sizing.
- **188-byte instance state not tiny**: at N=23 slots, pool consumes ~4.3 KB. Fits comfortably in L2 cache but not L1 on mid-tier mobile; per-tick iteration will incur some L1 misses. The 68-byte AC-PW-22b headroom is real breathing room but any future field addition (e.g., a per-wave audio-cue token) needs a scope review.
- **Six-step despawn pipeline is complex**: 6 sequential steps × 3 despawn-reason branches (NaturalLanding / RunTermination / PauseFlush) = 18-cell test matrix. Rule 13 test spec must exercise all 18 cells via the AC-PW-15 unified event log. High test surface.
- **Cadence governor tuning surface exposed**: not applicable here (that's ADR-0011). But D6's `FPullWaveSpawnParams` size (152 bytes) is a stack-copy cost at every `Construct()` call. At PEAK cadence ~2 admissions/sec, this is negligible (~300 bytes/sec stack traffic).

### Risks

| Risk | Probability | Impact | Mitigation |
|------|------------|--------|------------|
| Forbidden-transition `check()` fires in Shipping under an untested edge case | MEDIUM | MEDIUM — visible glitch: wave stuck mid-state or vanishes without Rule 13 pipeline events | D2 forbidden-transition table + non-Shipping asserts + Shipping-safe `pull_wave_illegal_transition` telemetry per Shipping-Safety Enforcement Policy (PM platform §R10a-9); QA plan captures the 5-state × 5-state transition matrix (25 cells − 12 legal = 13 forbidden cells) |
| Pause-flush race — `RSM.OnPausedChanged(false)` fires DURING Pull-Wave's tick body (from an external source mid-frame) | LOW | HIGH — waves partially ticked under is_paused=true then partially flushed | R7 B13 queued-next-tick + `bPauseFlushPending` synchronous set in handler; consumption at TOP of next tick BEFORE per-wave iteration begins. AC-PW-MID-TICK-PAUSE-DEFERRAL asserts exactly this scenario |
| `FPullWaveCurveSnapshot` SAMPLE_COUNT=32 too coarse for a future high-curvature `LateralOffsetCurve` | LOW | MEDIUM — trajectory interp error > CURVE_ENDPOINT_TOLERANCE at some interior t_norm | AC-PW-22c prototype gate + Design pre-check math (D3 rationale) show ≤ 0.001 error at 32 samples for expected curve smoothness. If a future curve authoring exceeds this, `SAMPLE_COUNT` bumps to 48 (requires AC-PW-22b re-derivation because 60 + 4 × 48 = 252 still fits under 256). Bumping to 64 breaks AC-PW-22b (316 > 256) and forces a struct-layout redesign |
| Cross-system tick-order violated by a future subsystem's tick registration | LOW | HIGH — Pull-Wave reads DPC snapshot before DPC has published it (frame-stale data) | Tick prerequisite mechanism per ADR-0007/0008 tick-hosting chain; AC-DPC-* + AC-RSM-* tests exercise the tick-order invariant; new subsystem additions REQUIRE a tick-order review at ADR authoring per ADR Dependencies table |
| `RemoveAt` per despawn scales poorly if pool size grows beyond 23 | LOW | LOW — O(N) shift cost with N ≤ ~50 remains sub-microsecond | Pool sizing is design-bound at 23 (see Negative consequence 1); growth requires ADR amendment which can re-evaluate `TSparseArray` at that time |

## GDD Requirements Addressed

| GDD System | Requirement (TR-ID) | How This ADR Addresses It |
|------------|---------------------|--------------------------|
| pull-wave-behavior.md | **TR-PW-002** 5-state lifecycle SPAWNED→LEANING→TRAVERSING→LANDED→DESPAWNING | D2 — full state machine with forbidden-transition table |
| pull-wave-behavior.md | **TR-PW-003** TELEGRAPH_WINDOW_FLOOR_S = 0.70s (R10d locked) | D5 upstream reads table — `LeanDurationS = clamp(DPC.telegraph_window_s, TELEGRAPH_WINDOW_FLOOR_S, ∞)` captured at SPAWNED; TELEGRAPH_WINDOW_FLOOR_S value itself is a tuning knob (GDD-owned, not ADR-decided) |
| pull-wave-behavior.md | **TR-PW-004** FPullWaveCurveSnapshot SAMPLE_COUNT=32 immutable at SPAWNED | D3 — locked SAMPLE_COUNT=32 with rationale + rejection of 16/64 alternatives |
| pull-wave-behavior.md | **TR-PW-005** TArray<FPullWaveInstanceState> with RemoveAt (not RemoveAtSwap) | D1 — container + RemoveAt-not-Swap semantics; Alternative 1 documents the TSparseArray rejection |
| pull-wave-behavior.md | **TR-PW-006** wave_id monotonic int32; iteration ASCENDING per Rule 14(b) | D1 — WaveId allocation + ASC iteration + Add-append-to-tail semantics |
| pull-wave-behavior.md | **TR-PW-007** FPullWaveInstanceState 188 bytes ≤ 256 ceiling | D3 — total 188 bytes with 68-byte headroom; AC-PW-22b static_assert path |
| pull-wave-behavior.md | **TR-PW-010** Pool ≥ 23 (16 + 2 + 5) | D1 — Reserve(23) at BeginPlay; alignment with ADR-0005 AWave pool |
| pull-wave-behavior.md | **TR-PW-014** Velocity frozen at SPAWNED; no mid-flight updates | D6 — `ForwardVelocityMs` captured in FPullWaveSpawnParams; const after `Construct()` copy per D2 SPAWNED-const invariant |
| pull-wave-behavior.md | **TR-PW-015** TraverseElapsedS canonical; pause excluded; no resume teleport | D2 TRAVERSING advance behavior — `TraverseElapsedS += DeltaTime` only when `!RSM.GetIsPaused()`; pause-freeze semantic |
| pull-wave-behavior.md | **TR-PW-016** Despawn pipeline: Coll→Tel→OnWaveDespawned→hide→clear→pool | D4 — full six-step pipeline (adds "hide" + "clear" + "pool" beyond the GDD's 3-step summary; matches GDD §Rule 13 detail) |
| pull-wave-behavior.md | **TR-PW-017** Seam 13 OnDespawnedUserCallback test-stub slot | D5 downstream broadcasts table — `OnWaveDespawned(WaveId, DespawnReason)` at Rule 13 step 3; Seam 13 alignment |
| pull-wave-behavior.md | **TR-PW-021** Tick chain RSM→DPC→PM→PW→Telegraph→Collision | D5 — cross-system read/write contract references the tick-order per GDD line 239; Pull-Wave reads DPC/PM at specified boundaries |
| pull-wave-behavior.md | AC-PW-15 unified event log contiguity | D4 six-step ordering invariant + D2 pause-flush queued-next-tick atomicity together preserve contiguity |
| pull-wave-behavior.md | AC-PW-22b sizeof static_assert | D3 — total 188 bytes; static_assert enforces at compile |
| pull-wave-behavior.md | AC-PW-MID-TICK-PAUSE-DEFERRAL | D2 pause-flush queued-to-next-tick per R7 B13; Alternative 3 documents rejected inline-flush |
| pull-wave-behavior.md | Rule 11 near-miss detection reads PM.current_lane | D5 upstream reads table — read at LANDED entry via Seam 12; forbidden-reads list explicitly excludes PM.OnSlipMidpoint (R1 RC-A) |
| pull-wave-behavior.md | Rule 14(a) FPullWaveCurveSnapshot immutability | D3 — value-type + no asset reference held; Alternative 4 documents rejected held-reference approach |
| pull-wave-behavior.md | Rule 14(b) WaveId ASC iteration | D1 — natural array order + Add-append-to-tail + monotonic WaveId + RemoveAt-not-Swap |
| pull-wave-behavior.md | Rule 19 pause-flush trigger from RUNNING (not ABORTED) | D2 pause-flush section — RSM `current_state` checked at flush consumption to distinguish RUNNING vs ABORTED |
| wave-spawner-pattern-library.md | FPullWaveSpawnParams handoff contract | D6 — FPullWaveSpawnParams format + producer/consumer semantics + immutability. Closes ADR-0011 forward-reference |
| player-movement.md | Seam 12 IPlayerMovementProvider consumption at LANDED | D5 upstream reads table — read at LANDED entry only; not per-tick |

**Out-of-scope for ADR-0010** (deferred to other ADRs, tuning knobs, or downstream stories):
- TR-PW-001, TR-PW-011 through TR-PW-013, TR-PW-022 through TR-PW-024, TR-PW-027 — rendering/visual (owned by ADR-0006)
- TR-PW-008, TR-PW-009 — asset-authoring cook-time checks (owned by Wave Spawner cook-time validator downstream story)
- TR-PW-018, TR-PW-019, TR-PW-020 — MIN_ESCAPE_SLIPS, PEAK triplets, LEAN_ANGLE tier gap (design constants — GDD-owned, referenced by ADR-0011 D4 as cook-time authoring contract)
- TR-PW-025, TR-PW-026 — NEAR_MISS_FLASH_DURATION_S, F-BARRAGE-SURVIVABILITY-INVARIANT scope (design constants — GDD-owned)
- TR-PW-028 — PM SLIP_TWEEN_DURATION_S forward contract (PM-owned; consumed by Pull-Wave via D5 but decided in PM GDD + ADR-0009)
- TR-PW-029 — SPAWN_PLANE_Z_OFFSET_M tuning knob (GDD tuning table, not ADR-decided)

## Performance Implications

- **CPU per-tick** (against AC-PW-22a 0.25 ms budget):
  - D2 state-machine dispatch: O(N) where N = active-wave count (≤ 16). Per-wave body: 1 pause gate, 1 state switch, 1-3 float ops (accumulator advance), 1 curve `EvaluateAt` (D3 O(1)), 1 ISMC `UpdateInstanceTransform` (ADR-0006 batched via `MarkRenderStateDirty` once per frame). Estimated ~5-10 µs per active wave; ≤ 160 µs at N=16 — well under budget.
  - D4 despawn pipeline: fires once per wave-lifetime; 6 steps × ≤ 2-5 µs per step ≈ 20-30 µs per despawn. At PEAK spawn rate ~2 waves/sec, amortized ~50 µs/sec.
  - D1 `RemoveAt`: O(N) shift × 188 bytes memcpy per element; worst-case at N=23, mid-index removal = 11 × 188 = 2 KB memcpy ≈ 1-2 µs on mid-tier mobile.
  - D6 `Construct()`: 152-byte stack copy + 1 `TArray::Add` + field-copy loop. ≤ 5 µs per admission.
  - **Total per-tick cost estimate: < 200 µs at PEAK N=16 — 80% of AC-PW-22a budget with 50 µs headroom**.
- **Memory** (against 1.5 GB mid-tier mobile ceiling per technical-preferences.md):
  - Pool: `23 × 188 bytes = 4324 bytes` for `FPullWaveInstanceState[23]`.
  - Snapshot storage per instance: 128 bytes (embedded in the 188-byte total).
  - Delegate binding overhead: 5 delegates × subscriber-count × ~32 bytes per FDelegateHandle. At 5-10 subscribers total per delegate, ~1.5 KB.
  - `FPullWaveSpawnParams` stack cost: 152 bytes per admission, transient.
  - **Total ADR-0010 memory footprint: < 10 KB — negligible against the 1.5 GB ceiling**.
- **Load Time**: `TArray::Reserve(23)` at `APullWaveSubsystemActor::BeginPlay` — one heap allocation of 4324 bytes. < 100 µs on mid-tier mobile; no impact on load-time budget.
- **Network**: Not applicable — SLIPSTORM is single-player mobile.

## Migration Plan

No existing Pull-Wave lifecycle implementation to migrate — ADR-0010 authors the greenfield per-instance runtime architecture on top of ADR-0006's Accepted renderer topology.

**Story authoring under `/create-stories pull-wave`** produces the implementation from scratch:

1. Story 1 — `FPullWaveInstanceState` + `FPullWaveCurveSnapshot` + `FPullWaveSpawnParams` struct definitions + AC-PW-22b sizeof static_assert (Logic).
2. Story 2 — Pool storage + `WaveId` monotonic allocation + `Reserve(23)` at BeginPlay + `Add`/`RemoveAt` semantics (Logic).
3. Story 3 — Five-state machine `EPullWaveState` + forbidden-transition `check()`s + `TransitionTo()` helper (Logic).
4. Story 4 — Per-tick state advance body (SPAWNED → LEANING → TRAVERSING) with pause-freeze gate (Integration).
5. Story 5 — LANDED entry + `CollisionOutcome` determination via Seam 12 PM read + `OnWaveHit` / `OnNearMiss` broadcasts (Integration).
6. Story 6 — Rule 13 six-step despawn pipeline + `OnWaveDespawned` broadcast + pool slot return (Integration).
7. Story 7 — Pause-flush queued-to-next-tick + `bPauseFlushPending` handler + AC-PW-MID-TICK-PAUSE-DEFERRAL test (Integration).
8. Story 8 — Run-termination drain semantics (RSM RUNNING → DEAD/COMPLETE/ABORTED distinct from PauseFlush) (Integration).
9. Story 9 — `Construct(FPullWaveSpawnParams)` entry point + Wave Spawner integration (Integration; wired against ADR-0011).

**Ordering dependency**: ADR-0010 acceptance is a story-authoring precondition for the Pull-Wave epic. Sprint N (to be planned) authors these stories after ADR-0010 Accepted + ADR-0011 Accepted (both are currently Proposed; can promote together in a paired architecture-review pass).

## Validation Criteria

The following are the ADR-Accepted signals — how we know the architecture is correct:

- **AC-PW-22b compile-time size gate**: `static_assert(sizeof(FPullWaveInstanceState) <= 256)` passes in a fresh build. Value at commit is 188 bytes.
- **AC-PW-22b `RemoveAtSwap` grep gate**: CI grep pattern rejects any `RemoveAtSwap` call on `ActiveWaves` (pattern 11).
- **`WaveId` ASC iteration test** (Logic): admit 20 waves; remove waves at mid-array indices 3, 7, 11; iterate remaining 17 waves; assert `WaveId` strictly ascending across the iteration.
- **`WaveId` never-reused test** (Logic): admit 30 waves across a synthetic 3-minute run; assert `WaveId` values are strictly ascending and no value appears twice.
- **Pause-freeze test** (Integration): drive Pull-Wave to LEANING state; set `RSM.IsPaused = true`; tick 60 frames; assert `LeanProgress` unchanged. Set `RSM.IsPaused = false`; tick 60 more frames; assert `LeanProgress` advanced normally.
- **Pause-flush queued-to-next-tick test** (Integration; AC-PW-MID-TICK-PAUSE-DEFERRAL): fire `RSM.OnPausedChanged(false)` mid-tick from a subscriber's handler; assert no active wave transitions to DESPAWNING within that tick; assert all active waves transition to DESPAWNING atomically at the top of the next tick in `WaveId` ASC order.
- **Rule 13 six-step ordering test** (Integration; AC-PW-15): admit 3 waves (WaveIds N, N+1, N+2); drive all to DESPAWNING in the same tick via pause-flush; capture the unified event log; assert per-`WaveId` contiguity — no interleaving of despawn-pipeline events across WaveIds; steps 1→2→3 in order per WaveId; steps 4→5→6 ordered same-tick.
- **`FPullWaveCurveSnapshot` immutability test** (Logic): construct snapshot from a `UCurveFloat` asset; mutate the source asset; assert snapshot Samples unchanged.
- **`SAMPLE_COUNT=32` interpolation error test** (Logic; AC-PW-22c prototype gate): sample a synthetic high-curvature test curve at 32 samples; assert `max |EvaluateAt(t) − source_curve.GetFloatValue(t)| < CURVE_ENDPOINT_TOLERANCE = 0.005` across 1000 random t values.
- **Forbidden-transition assertion test** (Logic; non-Shipping build): attempt `SPAWNED → TRAVERSING` transition; assert `check()` fires. Repeat for 12 other forbidden cells in the 25-cell transition matrix.
- **Run-termination distinct from PauseFlush test** (Integration): drive Pull-Wave to LEANING; fire RSM RUNNING → DEAD; assert wave continues advancing through TRAVERSING → LANDED → DESPAWNING with `DespawnReason == RunTermination`. Compare to pause-flush scenario where wave transitions directly to DESPAWNING with `DespawnReason == PauseFlush`.
- **`FPullWaveSpawnParams` size gate**: `static_assert(sizeof(FPullWaveSpawnParams) == 152)` passes. Verifies alignment + no unexpected padding.
- **Per-tick budget test** (Performance): run Pull-Wave under N=16 active waves for 60 frames; measure per-tick CPU cost via `STATGROUP_PullWave`; assert < 0.25 ms per AC-PW-22a on mid-tier mobile hardware (iPhone XR / Pixel 5 / Galaxy A52 device list per AC-WS-30).

## Related Decisions

- **ADR-0005** — Wave Spawner Subsystem Hosting (sibling — provides the `AWave` pool 23-slot alignment consumed here)
- **ADR-0006** — Pull-Wave Instanced Renderer (parent — owns `APullWaveSubsystemActor` component topology)
- **ADR-0007** — Run State Machine Hosting (upstream — `OnPausedChanged` + `GetIsPaused` consumers)
- **ADR-0008** — DPC Hosting (upstream — `telegraph_window_s` + `is_active` read at SPAWNED)
- **ADR-0009** — Player Movement Component Hosting (upstream — `current_lane` read via Seam 12 at LANDED)
- **ADR-0011** — Wave Spawner Pattern Library (sibling — produces `FPullWaveSpawnParams` passed into `Construct()`; ADR-0011 has forward-reference closed by this ADR)
- `design/gdd/pull-wave-behavior.md` — governing GDD (R14-closed 2026-06-19; 1039 lines; §States, §Interactions, §Rules 12/13/14 are the primary source for D1–D6)

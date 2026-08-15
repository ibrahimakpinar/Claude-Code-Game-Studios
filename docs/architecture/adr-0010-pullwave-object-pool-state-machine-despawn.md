# ADR-0010: Pull-Wave Object Pool, State Machine, and Despawn Pipeline

## Status
Proposed

## Date
2026-08-15

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

_(deferred to next session — Alternatives, Consequences, Risks, GDD Requirements Addressed, Performance Implications, Migration Plan, Validation Criteria, Related Decisions all authored in a follow-up session per the ADR-0011 staged-authoring precedent.)_

## Consequences

### Positive

_(deferred to next session)_

### Negative

_(deferred to next session)_

### Risks

_(deferred to next session)_

## GDD Requirements Addressed

_(deferred to next session — per-TR mapping for TR-PW-001 through TR-PW-029)_

## Performance Implications

_(deferred to next session)_

## Migration Plan

_(deferred to next session)_

## Validation Criteria

_(deferred to next session)_

## Related Decisions

- **ADR-0005** — Wave Spawner Subsystem Hosting (sibling — provides the `AWave` pool 23-slot alignment consumed here)
- **ADR-0006** — Pull-Wave Instanced Renderer (parent — owns `APullWaveSubsystemActor` component topology)
- **ADR-0007** — Run State Machine Hosting (upstream — `OnPausedChanged` + `GetIsPaused` consumers)
- **ADR-0008** — DPC Hosting (upstream — `telegraph_window_s` + `is_active` read at SPAWNED)
- **ADR-0009** — Player Movement Component Hosting (upstream — `current_lane` read via Seam 12 at LANDED)
- **ADR-0011** — Wave Spawner Pattern Library (sibling — produces `FPullWaveSpawnParams` passed into `Construct()`; ADR-0011 has forward-reference closed by this ADR)
- `design/gdd/pull-wave-behavior.md` — governing GDD (R14-closed 2026-06-19; 1039 lines; §States, §Interactions, §Rules 12/13/14 are the primary source for D1–D6)

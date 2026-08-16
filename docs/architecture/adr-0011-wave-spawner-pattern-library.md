# ADR-0011: Wave Spawner Pattern Library

## Status
Accepted

## Date
2026-08-16 (authored Proposed 2026-08-12; promoted Proposed → Accepted 2026-08-16 via paired-promotion pass with ADR-0010 per scoped `/architecture-review single-gdd design/gdd/pull-wave-behavior.md` verdict — no cross-ADR conflicts detected, ADR-0011's own Ordering Note pragmatic-promotion path invoked; ADR-0005 Depends-On remains Proposed pending Foundation HW-verification at ADR-0001 but interface consumed is stable per same precedent that landed ADR-0009 Accepted 2026-07-09 despite ADR-0002 Proposed)

## Engine Compatibility

| Field | Value |
|-------|-------|
| **Engine** | Unreal Engine 5.7 |
| **Domain** | Core (subsystem admission logic) |
| **Knowledge Risk** | HIGH — UE 5.7 is post-LLM-cutoff (~UE 5.3 training window). ADR-0005/0007/0008 already verified the underlying hosting primitives; ADR-0011 adds no new engine surface. |
| **References Consulted** | `docs/engine-reference/unreal/VERSION.md`; ADR-0005 (subsystem hosting + 23-actor pool); ADR-0007 (RSM RunSeed + OnPausedChanged); ADR-0008 (DPC OnPostTickFrameStatePublished); ADR-0006 (Pull-Wave instanced renderer); `design/gdd/wave-spawner-pattern-library.md`; `design/gdd/pull-wave-behavior.md` |
| **Post-Cutoff APIs Used** | None. All engine primitives (`UGameInstanceSubsystem`, `FTickableGameObject`, `AddUObject` delegate binding, `FCoreUObjectDelegates::PostLoadMapWithWorld`, `UPROPERTY() TArray<TObjectPtr<AWave>>` pool storage) predate UE 5.4 and were already validated by sibling ADRs. |
| **Verification Required** | ADR-0005 hosting stack must be Accepted before ADR-0011 can move Proposed → Accepted. Downstream: cook-time verification tooling (Rule 15's 14 binding checks) is a downstream story, not an engine-verification item. |

## ADR Dependencies

| Field | Value |
|-------|-------|
| **Depends On** | ADR-0005 (Wave Spawner subsystem hosting + 23-actor `AWave` pool + subscribe-in-`Initialize` pattern), ADR-0007 (RSM `RunSeed : uint64` + `OnPausedChanged(bool, double)` forward contracts), ADR-0008 (DPC `OnPostTickFrameStatePublished` multicast + `FDPCFrameState` atomic snapshot) |
| **Enables** | `/create-stories wave-spawner` — Wave Spawner epic story creation. Also unblocks Sprint 1 S1-08 (architecture-review of PM downstream consumers, which requires this ADR present to close the Wave Spawner R11a-8 grace-window binding). |
| **Blocks** | Wave Spawner epic (cannot start story authoring until Accepted). |
| **Ordering Note** | ADR-0005 currently Proposed pending Foundation-layer HW-verification gate at ADR-0001; ADR-0011 may require pragmatic promotion to Accepted under similar reasoning (architecture-review-2026-07-03.md:173) — with the risk that a downstream ADR-0005 revision would cascade into an ADR-0011 amendment. Amendment risk is bounded: ADR-0011 does not re-decide any hosting primitive. |

## Context

### Problem Statement

The Wave Spawner subsystem (host locked in ADR-0005) makes a substantial runtime admission decision every DPC frame: which pattern to spawn (if any), from which of three phase-specific pools, under an atomic barrage slot pre-commitment protocol, biased by a cadence governor toward a per-phase barrage frequency target. Without a documented architecture for these decisions, story authoring at `/create-stories wave-spawner` cannot proceed — programmers would have to derive admission logic directly from GDD prose spanning ~15 rules across 3 pool types and 6 lifecycle states, with no cross-story stance to check work against.

ADR-0005 documents the hosting (subsystem class, pool pre-allocation timing, delegate subscription points, forbidden `SpawnActor`/`Destroy` during gameplay). It explicitly defers pattern selection, cadence governance, slot accounting, phase-swap semantics, and the spawner's own lifecycle state machine to a future ADR — this ADR.

### Constraints

- **Hosting locked** (ADR-0005): `UWaveSpawnerSubsystem` is a `UGameInstanceSubsystem + FTickableGameObject` with a 23-actor `AWave` pool pre-allocated via `FCoreUObjectDelegates::PostLoadMapWithWorld` (Rule 11). ADR-0011 must not re-decide any of these.
- **Forward contracts inscribed** (RSM 0007, DPC 0008): `RunSeed : uint64`, `OnPausedChanged(bool, double)`, `OnPostTickFrameStatePublished`, and the atomic `FDPCFrameState` snapshot are producer-side locked. ADR-0011 documents consumption only.
- **Per-tick admission budget** (ADR-0005 via AC-WS-30): admission logic runs inside the `OnDPCFrameReady` callback under `STATGROUP_WaveSpawner`; ADVISORY-at-story-Done, BLOCKING-at-Alpha on mid-tier mobile (iPhone XR / Pixel 5 / Galaxy A52 per AC-WS-30 device list).
- **Determinism** (Rule 8 via RunSeed): identical seed + identical DPC frame sequence must produce an identical pattern-draw sequence within a session (Death Replay AC-WS-13, AC-WS-14 — same-platform constraint acknowledged; IEEE-754 divergence across iOS/Android is documented, not fixed here).
- **Cook-time enforcement** (Rule 15): 14 binding checks on the pattern pool (`OPENER_NO_BARRAGE`, `PEAK_SURVIVING_TRIPLETS`, `PEAK_BARRAGE_MIN_TIER`, `POOL_NON_EMPTY`, `PEAK_BASE_W_RANGE`, `PEAK_BASE_W_BELOW_CEILING`, `BARRAGE_UNIFORM_TIER`, `BARRAGE_DISTINCT_SOURCE_LANES`, `PRIMER_PATTERN`, `PILLAR_1_VERB_SLIP`, `MIN_BARRAGE_PATTERN_COUNT_PER_TRIPLET`, `PEAK_NO_ADJACENT_CLUSTER` via AC-WS-04, `POOL_SIZE_DERIVATION_MATCH` via AC-WS-09, `NON_BARRAGE_STAGGER` — see GDD line 189-199). ADR-0011 documents these as an authoring contract; validator tooling is out of scope (downstream story).

### Requirements

- **Three-pool selection**: OPENER, MID, PEAK (with PEAK further partitioned into barrage / non-barrage sub-pools) per Rule 2 shape table (GDD line 80-89).
- **Phase-transition pool swap** (Rule 10): pool source switches at OPENER→MID and MID→PEAK phase boundaries; in-flight waves complete under the admission-time pattern snapshot (immutability owed to Pull-Wave via `FPullWaveSpawnParams` — see ADR-0010 for the wave-side lifecycle).
- **Atomic barrage slot pre-commitment** (Rule 7): barrage patterns consume 3 concurrent slots; admission requires all 3 slots available atomically in the `scheduled` + `live` two-layer accounting model.
- **Cadence governor F-3** (Rule 8): per-phase weighted draw distribution biased to hit `BARRAGE_EVENTS_PER_PEAK_TARGET_AVG = 2` with floor `BARRAGE_EVENTS_PER_PEAK_MIN = 1` and ceiling `BARRAGE_EVENTS_PER_PEAK_MAX = 3`.
- **Barrage reservation** (Rule 7): when late-PEAK undershoot risk is detected (`barrages_admitted_this_PEAK < 1` AND `t_norm_PEAK > 0.5`), the cadence governor biases the next draw ≥ 80% barrage weight; if still no admission occurs, `barrage_owed = true` reserves the next admission window with `available ≥ 3` for a fresh barrage draw.
- **Six-state spawner lifecycle**: `Cold → Active → Holding ↔ Flushing → Idle → Cold`, with forbidden transitions per Rule 3 (e.g., no `Active → Cold` bypass).
- **Pause-flush** (Rule 13): on `OnPausedChanged(true)` while RSM `current_state == RUNNING`, hold new spawns and flush in-flight `scheduled` slots on the paired `OnPausedChanged(false)` resume.
- **Run-termination flush** (Rule 14): on RSM `RUNNING → DEAD | COMPLETE | ABORTED`, drain all `live` slots via the `IWaveSpawnerCallback` three-step pipeline (collision unregister → telegraph unregister → callback broadcast) documented in ADR-0005.
- **`FPullWaveSpawnParams` snapshot handoff** (bridge to ADR-0010): admission produces an immutable per-wave parameter snapshot passed to Pull-Wave at spawn time; the wave-side lifecycle owns snapshot consumption (ADR-0010 documents the wave-side contract).
- **Cook-time contract surface**: ADR-0011 enumerates Rule 15's 14 binding checks and cites the GDD line numbers; the validator implementation is a downstream story (asset-spec skill or build-time hook).

## Decision

The Wave Spawner subsystem implements pattern admission as a **four-stage deterministic pipeline** running inside its `OnDPCFrameReady` callback (subscribed per ADR-0005), backed by a **six-state lifecycle machine** that gates whether admission is attempted at all. Pattern content is validated at cook time against 14 binding checks documented here as an authoring contract; the validator tooling is a downstream story.

### D1 — Three-Pool Pattern Architecture

Phase-specific pools are the source of admission draws:

- **OPENER pool** (`is_barrage == false` only, per Rule 3 `OPENER_NO_BARRAGE`): calm-tempo patterns for the run's first ~15% (per DPC phase timing).
- **MID pool** (mixed `is_barrage` values, per Rule 2 shape): overlapping-tempo patterns for the run's mid ~50%.
- **PEAK pool** (partitioned into `PEAK_barrage_sub_pool` + `PEAK_non_barrage_sub_pool`): edge-to-edge tempo for the run's final ~35%.

The PEAK barrage sub-pool is the most-constrained sub-pool:
- Target-lane sets are exactly the 7 surviving triplets per Rule 4: `{0,1,3}, {0,1,4}, {0,2,3}, {0,2,4}, {0,3,4}, {1,2,4}, {1,3,4}`.
- Every barrage pattern has `lean_magnitude_tier ≥ 2` per Rule 5 (tier-1 PEAK barrages BANNED).
- Per-signature authoring variation of ≥ 2 patterns per triplet per Rule 15 `MIN_BARRAGE_PATTERN_COUNT_PER_TRIPLET` (7 triplets × ≥ 2 patterns = ≥ 14 authored PEAK barrage patterns minimum).

**Phase-transition pool swap** (Rule 10): the active pool source switches atomically at OPENER→MID and MID→PEAK boundaries (detected via `FDPCFrameState::current_phase` diff). In-flight waves — those already admitted but not yet despawned — complete under their admission-time `FPullWaveSpawnParams` snapshot. Parameter immutability is Pull-Wave's contract per ADR-0010; ADR-0011 only guarantees the *snapshot* is atomic and immutable at admission (no post-admission mutation from spawner side).

Pool storage is `UPROPERTY() TMap<EPhase, FPatternPool>` on the subsystem, populated at `PostLoadMapWithWorld` alongside the `AWave` pool (ADR-0005 hosting).

### D2 — Admission Gating Pipeline

Every DPC frame, `OnDPCFrameReady(FDPCFrameState)` runs the following ordered pipeline. Each stage either admits, defers, or vetoes; a veto ends the frame's admission attempt (no further stages run):

**Stage 1 — Rule 1 concurrency cap**:
- Read `FDPCFrameState.max_concurrent_waves` (DPC-published, phase-derived).
- If `live_slots.Num() ≥ max_concurrent_waves`: veto (no admission this frame).

**Stage 2 — Rule 7 slot pre-commitment (two-layer)**:
- The admission model has two accounting layers:
  - `scheduled` slots: reserved for patterns whose telegraph is fired but whose `AWave` actor is not yet leased from the pool.
  - `live` slots: reserved for patterns whose `AWave` is leased and actively rendering.
- `available := max_concurrent_waves - (scheduled.Num() + live.Num())`
- **If the incoming pattern is a barrage** (`pattern.is_barrage == true`): atomic 3-slot admission required (`available ≥ 3`). Barrage patterns are indivisible in slot accounting — no partial admission.
- **Non-barrage patterns**: `available ≥ 1` required.
- **`barrage_owed` reservation** (Rule 7 late-PEAK): if `barrage_owed == true`, the next admission window with `available ≥ 3` is RESERVED for a fresh barrage draw from `PEAK_barrage_sub_pool`, taking precedence over any non-barrage draw. On successful barrage admission: `barrage_owed = false`.

**Stage 3 — Rule 8 cadence governor (F-3)**:
- Selects the sub-pool to draw from (relevant only in PEAK; OPENER + MID have single-source sub-pools per Rule 2).
- **PEAK draw weighting**:
  - Let `barrages_admitted_this_PEAK := number of barrage patterns admitted since PEAK entry`.
  - Let `t_norm_PEAK := (current_time - PEAK_entry_time) / PEAK_duration_s`.
  - If `barrages_admitted_this_PEAK < BARRAGE_EVENTS_PER_PEAK_MIN (=1)` AND `t_norm_PEAK > 0.5`: bias next draw ≥ 80% weight toward `PEAK_barrage_sub_pool` (floor-reach bias).
  - Else: weight `PEAK_barrage_sub_pool` proportionally to drive session-sample average toward `BARRAGE_EVENTS_PER_PEAK_TARGET_AVG (=2)` without exceeding `BARRAGE_EVENTS_PER_PEAK_MAX (=3)`.
  - Formula: `barrage_weight = clamp(base_w + λ × (T_avg - projected_barrages_at_PEAK_end), 0, W_CEILING)` where `base_w = |PEAK_barrage_sub_pool| / |PEAK_pool_total|` and `W_CEILING` is a G.1-configured cadence ceiling. Rule 15 `PEAK_BASE_W_BELOW_CEILING` guarantees `base_w < W_CEILING` at cook time to prevent silent saturation.

**Stage 4 — Rule 12 pattern draw**:
- Deterministic sub-pool draw using the RSM `RunSeed` + admission index counter (consumed at `Cold → Active` per ADR-0007).
- Draw returns `FPatternDraw { PatternId, TargetLanes, LeanMagnitudeTier, IsBarrage, ... }`.
- Constructs immutable `FPullWaveSpawnParams` snapshot from the draw + admission-frame state (`current_phase`, `wave_spawn_interval_s`, `telegraph_window_s` — all from `FDPCFrameState`).
- Calls `AWave* Wave = AcquireFromPool()` (ADR-0005 interface) — never `UWorld::SpawnActor` at runtime (Rule 11).
- Calls `Wave->InitializeFromSnapshot(SpawnParams)` — Pull-Wave takes ownership of the snapshot (ADR-0010 contract).
- Adds `Wave->WaveId` to `scheduled_slots`.

Admission returns `EAdmissionResult { Admitted, Deferred_ConcurrencyCap, Deferred_SlotAtomic, Deferred_BarrageOwed, PoolExhausted }`. `PoolExhausted` is only returned in the structurally-impossible Rule 11 case (developer `check()` in non-Shipping; `pool_exhaustion_detected` telemetry + drop admission in Shipping — never `UWorld::SpawnActor` fallback per Rule 11).

### D3 — Six-State Spawner Lifecycle

The subsystem holds an explicit state machine `EWaveSpawnerLifecycleState` distinct from the RSM's run state:

```
       ┌──────┐
       │ Cold │◄────────┐
       └──┬───┘         │
    RSM   │             │  RSM: RUNNING → DEAD | COMPLETE | ABORTED
    IDLE→ │             │  (last live slot despawned → Cold)
COUNTDOWN │             │
    →RUNNING           ┌┴────────┐
       │                │ Flushing│
       ▼                └────▲────┘
   ┌────────┐                │
   │ Active │────────────────┘
   └──┬─────┘  (RSM DEAD/COMPLETE/ABORTED via Rule 14)
      │  ▲
      │  │  RSM OnPausedChanged(true)/false Rule 13 pause-flush
      │  │
      ▼  │
   ┌───────────┐
   │ Holding   │──── Idle (all live drained during long pause;
   └───────────┘     awaits resume with nothing to flush)
```

**States and transitions** (forbidden transitions are enforced by `check()` in `TransitionTo(EWaveSpawnerLifecycleState)`):

| State | Enter Condition | Exit Condition | Admission Behavior |
|-------|-----------------|----------------|--------------------|
| Cold | Subsystem `Initialize()` OR terminal flush complete | RSM `IDLE → COUNTDOWN` fires (via `OnStateChanged` subscription) | Reject admission (no draws) |
| Active | RSM `COUNTDOWN → RUNNING` fires; consumes `RunSeed` snapshot at entry (Rule 8) | RSM `OnPausedChanged(true)` (→ Holding) OR RSM `RUNNING → DEAD \| COMPLETE \| ABORTED` (→ Flushing) | Full four-stage pipeline enabled |
| Holding | RSM `OnPausedChanged(true)` while Active | RSM `OnPausedChanged(false)` (→ Flushing OR → Active OR → Idle per branches below) | Reject admission (Rule 13 pause-flush) |
| Flushing | (a) RSM `RUNNING → DEAD \| COMPLETE \| ABORTED` from Active — drain all live via `IWaveSpawnerCallback` (Rule 14); OR (b) `OnPausedChanged(false)` resume when scheduled slots exist — flush stale scheduled before resuming admission (Rule 13) | (a) All live drained → Cold; (b) All scheduled drained → Active | Reject admission during drain |
| Idle | Long pause: all previously-live waves have naturally despawned during Holding (no drain needed on resume) | RSM `OnPausedChanged(false)` → Active (no flush needed) | Reject admission |

**Forbidden transitions** (`check()` at `TransitionTo` entry — non-Shipping asserts, Shipping logs `illegal_lifecycle_transition` telemetry and no-ops):
- `Cold → Holding` (must transition Cold → Active first)
- `Cold → Flushing` (nothing to flush)
- `Active → Cold` bypass (must transition through Flushing)
- `Holding → Cold` (transitions through Flushing OR Idle → Cold on RSM terminal state)
- `Idle → Flushing` (nothing to flush by definition)

**Rule 13 pause-flush**: on `OnPausedChanged(true)` in Active, transition Active → Holding. Any `scheduled_slots` entries older than the pause boundary become "stale" — on `OnPausedChanged(false)`, if `scheduled_slots.Num() > 0`, transition Holding → Flushing (drain stale scheduled) → Active. If `scheduled_slots.Num() == 0` and `live_slots.Num() == 0`, transition Holding → Idle → Active on resume.

**Rule 14 run-termination flush**: on RSM `RUNNING → DEAD | COMPLETE | ABORTED`, transition Active → Flushing. Drain all `live_slots` via `IWaveSpawnerCallback` three-step pipeline (collision unregister → telegraph unregister → callback broadcast — full sequence documented in ADR-0005). After last drain, transition Flushing → Cold.

### D4 — Cook-Time Verification Contract

The Wave Spawner pattern library requires the following 14 binding checks pass before ship (Rule 15). ADR-0011 documents *what* must be checked; the *how* (validator tooling — asset-spec skill at asset creation OR build-time hook at editor cook) is a downstream story.

| Check | Verification | GDD Line |
|-------|--------------|----------|
| `OPENER_NO_BARRAGE` | `count(p in OPENER_pool where p.is_barrage) == 0` | 189 |
| `PEAK_SURVIVING_TRIPLETS` | `{p.target_lanes for p in PEAK_barrage_pool}` set-equals the 7 surviving triplets | 191 |
| `PEAK_BARRAGE_MIN_TIER` | `min(p.lean_magnitude_tier for p in PEAK_barrage_pool) ≥ 2` | 192 |
| `POOL_NON_EMPTY` | `count(p) ≥ 1` for every pool in `{OPENER, MID, PEAK_non_barrage, PEAK_barrage}` | 197 |
| `PEAK_BASE_W_RANGE` | `0.15 ≤ base_w ≤ 0.40` where `base_w = |PEAK_barrage_pool| / |PEAK_pool_total|` | 198 |
| `PEAK_BASE_W_BELOW_CEILING` | `base_w < W_CEILING` (G.1-configured cadence governor ceiling) | 199 |
| `BARRAGE_UNIFORM_TIER` | All 3 slots of a barrage share the same `lean_magnitude_tier` | Rule 15 R2a-added |
| `BARRAGE_DISTINCT_SOURCE_LANES` | Barrage source-lane values are 3 distinct lanes | Rule 15 R2a-added |
| `PRIMER_PATTERN` | Every pool has at least one primer pattern | Rule 15 R1a-added |
| `PILLAR_1_VERB_SLIP` | Every pattern requires the SLIP verb (no non-SLIP patterns) | Rule 15 R1a-added |
| `MIN_BARRAGE_PATTERN_COUNT_PER_TRIPLET` | Each of the 7 surviving PEAK triplets has ≥ 2 authored patterns | Rule 15 R1a-added |
| `PEAK_NO_ADJACENT_CLUSTER` | AC-WS-04 constraint from Rule 4 (no two adjacent lanes as a PEAK barrage triplet) | AC-WS-04 |
| `POOL_SIZE_DERIVATION_MATCH` | Pool count matches F-2 formula | AC-WS-09 |
| `NON_BARRAGE_STAGGER` | Non-barrage stagger constraint from Rule 6 | AC-WS-07 |

### Architecture Diagram

```
┌─────────────────────────────────────────────────────────────────────────────┐
│  DPC Frame (from ADR-0008 OnPostTickFrameStatePublished)                    │
│  FDPCFrameState { current_phase, wave_spawn_interval_s, telegraph_window_s, │
│                   max_concurrent_waves, t_norm, is_active }                 │
└──────────────────────────┬──────────────────────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│  UWaveSpawnerSubsystem::OnDPCFrameReady(const FDPCFrameState& Frame)        │
│                                                                             │
│  ┌──────────────────────────────────────────────────────────────────────┐  │
│  │  D3 Lifecycle State Machine (EWaveSpawnerLifecycleState)             │  │
│  │  Cold → Active → Holding ↔ Flushing → Idle → Cold                    │  │
│  │  Only Active state proceeds to admission pipeline.                   │  │
│  └──────────────────────┬───────────────────────────────────────────────┘  │
│                         │ (state == Active)                                 │
│                         ▼                                                   │
│  ┌──────────────────────────────────────────────────────────────────────┐  │
│  │  D2 Four-Stage Admission Pipeline                                    │  │
│  │  ┌─────────────────────────────────────────────────────────────────┐ │  │
│  │  │ Stage 1: Rule 1 concurrency cap                                 │ │  │
│  │  │  if live.Num() ≥ Frame.max_concurrent_waves: VETO               │ │  │
│  │  └─────────────────────────────┬───────────────────────────────────┘ │  │
│  │                                ▼                                     │  │
│  │  ┌─────────────────────────────────────────────────────────────────┐ │  │
│  │  │ Stage 2: Rule 7 two-layer slot check (scheduled + live)         │ │  │
│  │  │  barrage: available ≥ 3 (atomic)                                │ │  │
│  │  │  non-barrage: available ≥ 1                                     │ │  │
│  │  │  barrage_owed reservation takes precedence when true            │ │  │
│  │  └─────────────────────────────┬───────────────────────────────────┘ │  │
│  │                                ▼                                     │  │
│  │  ┌─────────────────────────────────────────────────────────────────┐ │  │
│  │  │ Stage 3: Rule 8 cadence governor F-3                            │ │  │
│  │  │  PEAK: weight barrage sub-pool per barrage_weight formula       │ │  │
│  │  │  OPENER/MID: single-source (no weighting)                       │ │  │
│  │  └─────────────────────────────┬───────────────────────────────────┘ │  │
│  │                                ▼                                     │  │
│  │  ┌─────────────────────────────────────────────────────────────────┐ │  │
│  │  │ Stage 4: Rule 12 pattern draw (deterministic via RunSeed)       │ │  │
│  │  │  FPatternDraw draw = SubPool.Draw(seed, admission_index)        │ │  │
│  │  │  FPullWaveSpawnParams snapshot = BuildSnapshot(draw, Frame)     │ │  │
│  │  │  AWave* wave = AcquireFromPool()  ─── ADR-0005 pool             │ │  │
│  │  │  wave->InitializeFromSnapshot(snapshot)  ─── ADR-0010 handoff   │ │  │
│  │  │  scheduled_slots.Add(wave->WaveId)                              │ │  │
│  │  └─────────────────────────────────────────────────────────────────┘ │  │
│  └──────────────────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────────────┘
                           │
                           ▼
     ┌─────────────────────────────────────┐
     │  Pull-Wave (ADR-0010 lifecycle)     │
     │  Owns FPullWaveSpawnParams snapshot │
     └─────────────────────────────────────┘
```

### Key Interfaces

```cpp
// Subsystem entry point (called by DPC per ADR-0008 delegate)
UCLASS()
class UWaveSpawnerSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
    GENERATED_BODY()

public:
    // ADR-0005: bound in Initialize()
    void OnDPCFrameReady(const FDPCFrameState& Frame);
    void OnPausedChanged(bool bIsPaused, double Timestamp);  // ADR-0007 forward contract
    void OnStateChanged(ERunState PrevState, ERunState NewState, ERunOutcome, double);  // ADR-0007

private:
    // D3 lifecycle
    UPROPERTY() EWaveSpawnerLifecycleState LifecycleState = EWaveSpawnerLifecycleState::Cold;
    void TransitionTo(EWaveSpawnerLifecycleState NewState);  // check()s forbidden transitions

    // D2 admission
    EAdmissionResult AttemptAdmission(const FDPCFrameState& Frame);
    bool CheckConcurrencyCap(const FDPCFrameState& Frame) const;         // Stage 1
    bool CheckSlotAvailability(bool bIsBarrage) const;                    // Stage 2
    EPatternSubPool SelectSubPool(EPhase Phase, uint32 AdmissionIndex) const;  // Stage 3
    FPatternDraw DrawPattern(EPatternSubPool SubPool, uint32 AdmissionIndex) const;  // Stage 4

    // D1 pools (populated at PostLoadMapWithWorld per ADR-0005)
    UPROPERTY() TMap<EPhase, FPatternPool> PatternPools;
    UPROPERTY() bool bBarrageOwed = false;
    UPROPERTY() int32 BarragesAdmittedThisPEAK = 0;
    UPROPERTY() TSet<int32> ScheduledSlots;  // WaveId of scheduled AWaves
    UPROPERTY() TSet<int32> LiveSlots;       // WaveId of live AWaves

    // Snapshot handoff to Pull-Wave (ADR-0010 owns FPullWaveSpawnParams struct)
    FPullWaveSpawnParams BuildSpawnSnapshot(const FPatternDraw& Draw, const FDPCFrameState& Frame) const;
};

UENUM()
enum class EWaveSpawnerLifecycleState : uint8
{
    Cold,
    Active,
    Holding,
    Flushing,
    Idle,
};

USTRUCT()
struct FPatternPool
{
    GENERATED_BODY()

    UPROPERTY() TArray<FPatternDefinition> BarragePatterns;
    UPROPERTY() TArray<FPatternDefinition> NonBarragePatterns;
    // Cook-time verifier populates these; runtime never mutates.
};

USTRUCT()
struct FPatternDraw
{
    GENERATED_BODY()

    UPROPERTY() int32 PatternId = INDEX_NONE;
    UPROPERTY() TArray<int32> TargetLanes;     // Rule 4 (PEAK barrages: one of 7 triplets)
    UPROPERTY() int32 LeanMagnitudeTier = 0;   // Rule 5 (PEAK barrages: ≥ 2)
    UPROPERTY() bool bIsBarrage = false;       // Rule 7 (atomic 3-slot when true)
};

UENUM()
enum class EAdmissionResult : uint8
{
    Admitted,
    Deferred_ConcurrencyCap,   // Stage 1 veto
    Deferred_SlotAtomic,       // Stage 2 veto (barrage needs 3 slots)
    Deferred_BarrageOwed,      // Rule 7 reservation blocked non-barrage draw
    PoolExhausted,             // Rule 11 developer-facing assertion path
};
```

## Alternatives Considered

### Alternative 1: Single-pool model with tag-based filtering

- **Description**: Store all patterns in a flat `TArray<FPatternDefinition>` with `EPhase` and `bIsBarrage` as pattern-side tags. Admission queries filter by current phase + barrage weight at draw time.
- **Pros**: Simpler storage (one `TArray` instead of `TMap<EPhase, FPatternPool>`); cook-time authoring can drop patterns into a single pool without pre-partitioning; reduces `UPROPERTY` count on subsystem.
- **Cons**: Every draw walks the full pool for filter matching — O(N) filter per admission vs O(1) sub-pool addressing (small absolute cost but semantically weaker). `PEAK_BASE_W_BELOW_CEILING` cook check (Rule 15 R2a-added, GDD line 199) becomes ambiguous — `base_w = |PEAK_barrage_pool| / |PEAK_pool_total|` requires an explicit partition; tag-based filtering forces the cook check to simulate the runtime filter, weakening the "cook-time verified" invariant. Debuggability suffers — a GDD reader following the OPENER/MID/PEAK narrative would find no direct code-level mapping.
- **Rejection Reason**: Rule 15's cook-time contract (14 binding checks, 3 of which reference specific sub-pool boundaries — `OPENER_NO_BARRAGE`, `PEAK_BASE_W_RANGE`, `PEAK_BASE_W_BELOW_CEILING`) requires explicit pool partitioning at authoring time. Tag-based filtering pushes sub-pool semantics from GDD-authored data structure into a computed runtime property, weakening the invariant.

### Alternative 2: Non-atomic barrage admission (per-slot commitment)

- **Description**: Barrage patterns admit one slot at a time as slots become available, tracking barrage "progress" until all 3 slots are seated. During the multi-tick admission window, the barrage is partially instantiated.
- **Pros**: Reduces admission-vetoed frames in high-density mid-PEAK (fewer barrages deferred by `Deferred_SlotAtomic` when `available == 2`); smoother apparent spawn rate near the concurrency cap.
- **Cons**: Contradicts Rule 7's explicit atomic requirement (barrage is an indivisible 3-slot unit). Introduces a barrage micro-state-machine (`Uncommitted → PartiallyCommitted → FullyCommitted`) that must be flushed on RSM pause/terminal — new Rule 13 / Rule 14 sub-behaviors with no GDD backing. Concurrency race hazard: a partially-committed barrage occupies 1-2 slots that the concurrency cap accounts for but no rendered wave exists — cost accounting drifts from user-visible state. The "barrage triplet is a single visual event" player-experience assumption (Rule 4 rationale — a `{0,1,3}` triplet is *read* as one shape, not three individual waves arriving progressively) is broken.
- **Rejection Reason**: Contradicts Rule 7 atomic requirement and Rule 4 shape-invariant player experience. The performance win (fewer deferred admissions) is real but small (~1-2 deferrals per PEAK per 60-second run in typical density modeling); the cost (new state machine + race hazard + broken player-experience invariant) is high.

### Alternative 3: External-tool cook-time validation (build-system integration)

- **Description**: Instead of documenting the 14 Rule 15 checks as an ADR-0011 contract for downstream tooling, integrate the validator into the UE build system directly (e.g., custom UAT command run during editor cook, or an `IAssetRegistry`-hooked pre-package step).
- **Pros**: Zero-latency detection of pool-authoring errors — fails the cook, not just a runtime `check()`. Removes the human-in-the-loop step (author must remember to run asset-spec skill). Aligns with UE 5.7's evolving cook-verify infrastructure.
- **Cons**: Requires cook infrastructure to exist and be integrated before Wave Spawner can ship — creates a Foundation-layer dependency Sprint 1's Wave Spawner epic does not have time budget for. Rule 15's checks are pattern-content-level; integrating them requires C++ validator code reading `FPatternDefinition` UStruct via reflection — non-trivial, and the checks may need to evolve as pool content changes (adding a check becomes a build-tooling change, not an asset-authoring change). Couples pattern-library evolution to build-system evolution.
- **Rejection Reason**: Out of Sprint 1 scope. ADR-0011 documents the *what* (14 checks); the *how* (validator tooling — asset-spec skill vs build hook vs custom UAT) is deferred to a downstream story that can be prioritized in a later sprint against actual cook-tool availability. Documenting the contract in ADR-0011 preserves the *invariant* without prescribing the *mechanism*.

## Consequences

### Positive

- **Story-authoring unblocked**: `/create-stories wave-spawner` can proceed with concrete admission-pipeline stances to embed in stories (per-story TR-WS-XX rows can reference D1/D2/D3/D4 sub-decisions).
- **Cross-story consistency guaranteed**: All Wave Spawner stories share one admission-pipeline spec — no drift risk from stories inventing their own admission order or slot accounting.
- **Determinism preserved**: RunSeed consumption at `Cold → Active` (Rule 8) + deterministic sub-pool draw index (Stage 4) ensures Death Replay works within a session (AC-WS-13/14 same-platform constraint acknowledged; IEEE-754 divergence across iOS/Android documented, not fixed here).
- **Lifecycle debuggability**: Six explicit states + forbidden-transition `check()`s catch state-machine bugs at test time rather than as latent field bugs (mirrors ADR-0007 RSM approach).
- **Forward contracts closed**: RSM's `RunSeed` + `OnPausedChanged` and DPC's `OnPostTickFrameStatePublished` are now documented as bound-and-consumed rather than "TBD in ADR-0011". Removes documentation stale-reference risk flagged in `architecture-review-2026-07-08.md`.
- **Cook-time contract inscribed**: All 14 Rule 15 checks are enumerated in one place with GDD line citations — future validator tooling has a single spec source to implement against.

### Negative

- **Amendment risk under ADR-0005 revision**: ADR-0005 is Proposed (not Accepted) pending Foundation-layer HW-verification gate at ADR-0001. If a downstream ADR-0005 revision changes pool-storage type or subscription lifecycle, ADR-0011 must be revised in lockstep. Amendment risk is bounded — ADR-0011 does not re-decide any hosting primitive — but not zero.
- **Validator tooling deferred**: ADR-0011 does not include validator implementation. Story authoring in Wave Spawner epic must include a Rule 15 validator story OR flag the risk that runtime `check()` on cold path may fire in the field if cook-time verification is skipped.
- **Pull-Wave contract inscription delayed**: `FPullWaveSpawnParams` snapshot immutability is Pull-Wave's contract (future ADR-0010, not yet authored). ADR-0011 references the contract but cannot cite a specific section — ADR-0010 authoring must inscribe this before Pull-Wave stories start.
- **Six-state complexity vs alternatives**: A simpler three-state (`Cold → Active → Cold`) machine would suffice for the happy path. The six-state model exists to handle Rule 13 (pause-flush) and Rule 14 (terminal-flush) edge cases correctly. This complexity is load-bearing for correctness but adds test surface (12+ transition test cases required per D3 table).
- **Cadence governor tuning surface exposed**: `λ`, `T_avg`, `W_CEILING` are configurable in G.1 — Wave Spawner stories must include tuning-knob defaults and safe-range checks. Poor tuning can silently degrade PEAK barrage frequency without failing any cook-time check (only `PEAK_BASE_W_BELOW_CEILING` guards against silent saturation).

### Risks

| Risk | Probability | Impact | Mitigation |
|------|------------|--------|------------|
| `FPullWaveSpawnParams` struct not yet authored (owned by ADR-0010) | HIGH (ADR-0010 not written) | MEDIUM — ADR-0011 references the struct symbolically | Wave Spawner story authoring gated on ADR-0010 acceptance; document as ADR-0011 forward dependency |
| Rule 15 validator not built before Wave Spawner ships | MEDIUM | HIGH — runtime `check()` in cold path could fire in the field | File Wave Spawner epic story for validator tooling; block epic-Done until validator exists |
| Rule 13 pause-flush edge cases (Holding → Idle vs Holding → Flushing branching) hit an untested transition | MEDIUM | MEDIUM — visual glitch: waves survive resume that should have been flushed | D3 table transitions require per-branch integration tests; QA plan captures the branch matrix |
| `barrage_owed` reservation deadlocks with concurrency cap in high-density PEAK | LOW | MEDIUM — PEAK barrage frequency undershoots `BARRAGE_EVENTS_PER_PEAK_MIN` | Cadence governor's floor-reach bias (`≥ 80%` weight at `t_norm_PEAK > 0.5`) is the primary defense; add `peak_min_barrage_floor_undershoot` telemetry (GDD line 130) to detect field drift |
| Cadence governor F-3 formula's `λ` overshoots ceiling in unusual DPC frame sequences | LOW | LOW — cook-time `PEAK_BASE_W_BELOW_CEILING` guards; runtime `clamp(..., 0, W_CEILING)` is a hard cap | Rule 15 cook check + runtime clamp both in place; no additional mitigation needed |

## GDD Requirements Addressed

| GDD System | Requirement | How This ADR Addresses It |
|------------|-------------|--------------------------|
| wave-spawner-pattern-library.md | Rule 1 concurrency cap (GDD line 79) | D2 Stage 1 — reads `max_concurrent_waves` from `FDPCFrameState`; vetoes admission when `live.Num() >= cap` |
| wave-spawner-pattern-library.md | Rule 2 shape invariants (line 80-89) | D1 — three-pool architecture with PEAK partitioned into barrage/non-barrage sub-pools per shape table |
| wave-spawner-pattern-library.md | Rule 3 OPENER zero barrages (line 95) | D1 — OPENER pool stores `NonBarragePatterns` only; enforced at cook time via Rule 15 `OPENER_NO_BARRAGE` |
| wave-spawner-pattern-library.md | Rule 4 PEAK 7-surviving-triplet enumeration (line 97) | D1 — PEAK_barrage_sub_pool constrained to the 7 triplets; enforced via Rule 15 `PEAK_SURVIVING_TRIPLETS` |
| wave-spawner-pattern-library.md | Rule 5 PEAK barrage tier floor (line 103) | D1 — enforced via Rule 15 `PEAK_BARRAGE_MIN_TIER` (`min tier ≥ 2`) |
| wave-spawner-pattern-library.md | Rule 6 non-barrage stagger | D2 Stage 2 — non-barrage admits with `available ≥ 1`; stagger enforced via cook-time `NON_BARRAGE_STAGGER` |
| wave-spawner-pattern-library.md | Rule 7 atomic barrage slot pre-commitment (line 127-130) | D2 Stage 2 — two-layer `scheduled` + `live` accounting; barrage requires `available ≥ 3`; `barrage_owed` reservation semantics documented |
| wave-spawner-pattern-library.md | Rule 8 cadence governor F-3 (line 143-144) | D2 Stage 3 — PEAK draw weighting formula with `λ`, `T_avg`, `W_CEILING`; floor-reach bias at late PEAK; RunSeed consumed at Cold → Active |
| wave-spawner-pattern-library.md | Rule 9 drain window | Covered indirectly via D3 Rule 14 flush semantics + ADR-0005 `IWaveSpawnerCallback` three-step pipeline |
| wave-spawner-pattern-library.md | Rule 10 phase-transition pool swap | D1 — active pool source switches at OPENER→MID and MID→PEAK boundaries; in-flight waves complete under admission-time snapshot |
| wave-spawner-pattern-library.md | Rule 11 no runtime SpawnActor/Destroy | D2 Stage 4 — `AcquireFromPool()` (ADR-0005 interface), never `UWorld::SpawnActor`; documented as forbidden path with `pool_exhaustion_detected` telemetry |
| wave-spawner-pattern-library.md | Rule 12 pattern draw | D2 Stage 4 — deterministic draw using RunSeed + admission index counter |
| wave-spawner-pattern-library.md | Rule 13 pause-flush | D3 — Active → Holding on `OnPausedChanged(true)`; Holding → Flushing OR Idle → Active on resume per branch table |
| wave-spawner-pattern-library.md | Rule 14 run-termination flush | D3 — Active → Flushing on RSM terminal state; drain via `IWaveSpawnerCallback` three-step pipeline; Flushing → Cold after last drain |
| wave-spawner-pattern-library.md | Rule 15 cook-time verification (line 189-199) | D4 — 14 binding checks enumerated with GDD line citations; validator tooling deferred to downstream story |
| wave-spawner-pattern-library.md | AC-WS-13/14 Death Replay determinism | D2 Stage 4 — deterministic RunSeed + admission-index draw ensures same-platform replay produces identical pattern sequence |
| pull-wave-behavior.md | `FPullWaveSpawnParams` snapshot immutability | D2 Stage 4 — spawner constructs immutable snapshot at admission; Pull-Wave (ADR-0010) owns snapshot lifecycle post-handoff |

## Performance Implications

- **CPU**: Per-tick admission cost (Stages 1-4 executed on every DPC frame in Active state).
  - Stage 1: O(1) — single `Num()` comparison.
  - Stage 2: O(1) — two `Num()` calls + subtraction + integer comparison.
  - Stage 3: O(1) in OPENER/MID (single-source sub-pools); O(1) in PEAK with pre-computed `base_w` (populated at pool-load, not per-tick).
  - Stage 4: O(1) sub-pool addressing + O(1) deterministic draw via `RunSeed XOR admission_index` modulo `sub_pool.Num()`.
  - Total per-tick admission cost: dominated by cache-line access to `PatternPools`, `ScheduledSlots`, `LiveSlots` `UPROPERTY`s. Fits within ADR-0005 AC-WS-30 per-tick budget (measured under `STATGROUP_WaveSpawner`).
- **Memory**: `PatternPools` is a `TMap<EPhase, FPatternPool>` with ≤ 4 entries (OPENER, MID, PEAK_non_barrage, PEAK_barrage). Total pattern definitions ≤ ~30 per pool × 4 pools = ~120 `FPatternDefinition` structs. Each struct ≤ ~64 bytes (target-lane array + ints + booleans). Total ≤ 8 KB. Negligible on mid-tier mobile (1.5 GB memory ceiling per technical-preferences.md).
- **Load Time**: Pool population is a one-shot `PostLoadMapWithWorld` cost (ADR-0005 AC-WS-31 — one frame, ≤ 16.6 ms on mid-tier mobile). ADR-0011 admission pipeline itself has zero load-time cost (subsystem-owned `UPROPERTY`s initialize to defaults).
- **Network**: Not applicable — SLIPSTORM is single-player (mobile endless runner).

## Migration Plan

No existing implementation to migrate — ADR-0011 authors the greenfield admission architecture. Story authoring under `/create-stories wave-spawner` will produce the implementation from scratch.

**Ordering dependency**: ADR-0011 acceptance is a story-authoring precondition. Sprint 1 S1-05 authors this ADR as Proposed; promotion to Accepted requires (a) ADR-0005 Accepted (blocked on Foundation-layer HW-verification gate at ADR-0001), OR (b) pragmatic promotion under `architecture-review-2026-07-03.md:173` reasoning (same-precedent pattern used for ADR-0009 → ADR-0002 dependency).

## Validation Criteria

The following are the ADR-Accepted signals — how we know the architecture is correct:

- **Rule 15 cook-time validator passes**: All 14 binding checks (D4 table) return true on the authored PEAK pattern pool. Validator tooling is a Wave Spawner epic story.
- **Concurrency cap enforced**: Integration test — spawn `max_concurrent_waves + 1` non-barrage admissions in a single DPC frame; assert Stage 1 vetoes the last admission with `EAdmissionResult::Deferred_ConcurrencyCap`.
- **Barrage atomic admission**: Integration test — with `available == 2` slots, attempt barrage admission; assert Stage 2 vetoes with `EAdmissionResult::Deferred_SlotAtomic`.
- **`barrage_owed` reservation**: Integration test — set `bBarrageOwed = true`, attempt non-barrage admission with `available == 3`; assert Stage 2 vetoes with `EAdmissionResult::Deferred_BarrageOwed`.
- **PEAK cadence governor floor**: Simulation test over 100 seeded PEAK runs — assert `barrages_admitted_this_PEAK ≥ BARRAGE_EVENTS_PER_PEAK_MIN (= 1)` in ≥ 99% of runs.
- **PEAK cadence governor target average**: Simulation test over 100 seeded PEAK runs — assert `avg(barrages_admitted_this_PEAK) ∈ [1.8, 2.2]` (converges to `BARRAGE_EVENTS_PER_PEAK_TARGET_AVG = 2`).
- **Lifecycle state machine**: Integration test — drive RSM through IDLE → COUNTDOWN → RUNNING → paused → resumed → DEAD; assert spawner state transitions Cold → Active → Holding → (Flushing OR Active) → Active → Flushing → Cold with no forbidden transitions.
- **Rule 13 pause-flush stale scheduled**: Integration test — admit a barrage in Active (scheduled_slots.Num() > 0), pause RSM before telegraph fires, resume; assert spawner transitions Holding → Flushing → Active and stale scheduled slots are drained.
- **Rule 14 run-termination flush**: Integration test — with live_slots.Num() > 0, transition RSM RUNNING → DEAD; assert spawner transitions Active → Flushing, all live drained via `IWaveSpawnerCallback` three-step pipeline, then Flushing → Cold.
- **Death Replay determinism (same-platform)**: Integration test — capture pattern-draw sequence over one seeded PEAK, replay with identical seed on same platform; assert draw sequences match exactly (AC-WS-13).

## Related Decisions

- **ADR-0005** — Wave Spawner Subsystem Hosting (parent hosting decision; ADR-0011 documents the admission logic that runs on top)
- **ADR-0006** — Pull-Wave Instanced Renderer (Wave Spawner spawns Pull-Waves through the 23-actor `AWave` pool per ADR-0005)
- **ADR-0007** — Run State Machine Hosting (RunSeed + OnPausedChanged consumers)
- **ADR-0008** — Difficulty & Phase Controller Hosting (OnPostTickFrameStatePublished subscriber)
- **Future ADR-0010** — Pull-Wave Object Pool + State Machine + Despawn Pipeline (Wave Spawner passes admission-time snapshot `FPullWaveSpawnParams` to Pull-Wave; parameter immutability is the wave-side responsibility documented in ADR-0010, not here)
- `design/gdd/wave-spawner-pattern-library.md` — governing GDD (all rules, formulas, cook-time checks, ACs)
- `design/gdd/pull-wave-behavior.md` — consumer GDD (Pull-Wave lifecycle owns the `FPullWaveSpawnParams` snapshot after admission)
- `docs/architecture/architecture-review-2026-07-08.md` — flagged ADR-0011 as remaining Core-layer coverage gap

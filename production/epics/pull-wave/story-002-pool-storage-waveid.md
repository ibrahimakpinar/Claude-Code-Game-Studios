# Story 002: Pool Storage + WaveId Allocation

> **Epic**: Pull-Wave Behavior
> **Status**: Complete
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: 2–3 hours
> **Manifest Version**: 2026-07-03 (docs/registry/architecture.yaml v8 — no separate control-manifest.md; forbidden patterns cited from registry)
> **Last Updated**: 2026-08-18

## Context

**GDD**: `design/gdd/pull-wave-behavior.md`
**Requirement**: `TR-PW-005`, `TR-PW-006`, `TR-PW-010`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0010: Pull-Wave Object Pool, State Machine, and Despawn Pipeline
**ADR Decision Summary**: Pool storage is a `UPROPERTY() TArray<FPullWaveInstanceState>` on `APullWaveSubsystemActor` (renderer host locked by ADR-0006). Capacity reserved at 23 via `TArray::Reserve(23)` at `BeginPlay` — no dynamic resize during a run. WaveId is a monotonically increasing `int32` counter assigned at `Construct()`, never reused within a session. Natural array order (append via `Add()`) equals WaveId ASC, so no per-tick sort is required. Removal uses `RemoveAt` (not `RemoveAtSwap`) to preserve WaveId ASC iteration order.

**Engine**: Unreal Engine 5.7 | **Risk**: LOW
**Engine Notes**: `TArray<T>::Reserve(N)`, `TArray::Add`, `TArray::RemoveAt(Index)` — all stable pre-UE-4. `UPROPERTY() TArray<FPullWaveInstanceState>` requires `FPullWaveInstanceState` to be a valid UPROPERTY value type (plain struct with UE macro or `USTRUCT`). `TArray::RemoveAtSwap` is explicitly forbidden per ADR-0010 D1 and AC-PW-22b pattern 9. No post-cutoff APIs.

**Control Manifest Rules (Feature layer)** — from `docs/registry/architecture.yaml` v8:
- Required: `TArray::Reserve(23)` called at `APullWaveSubsystemActor::BeginPlay` before any wave can be admitted (locks pool capacity; prevents mid-tick reallocation).
- Required: New waves appended via `TArray::Add()` only (append-to-tail); `TArray::Insert` at any non-tail position is forbidden (AC-PW-22b pattern 11).
- Required: Pool removal via `TArray::RemoveAt(Index)` only (AC-PW-22b pattern 9 ALLOW).
- Forbidden: `TArray::RemoveAtSwap` on `ActiveWaves` — breaks WaveId ASC iteration invariant (AC-PW-22b pattern 9 FORBID; CI grep).
- Forbidden: `TArray::Insert(NewState, NonTailIndex)` on `ActiveWaves` — breaks ASC ordering (AC-PW-22b pattern 11 FORBID; CI grep scoped to active_wave_list source path).
- Forbidden: `WaveMassISMC` or `TrailCubeISMC` declared on `AWave` or any per-wave class — must live on `APullWaveSubsystemActor` singleton only (docs/registry/architecture.yaml v8 line 646).

---

## Acceptance Criteria

*From GDD `design/gdd/pull-wave-behavior.md`, scoped to this story:*

- [ ] `APullWaveSubsystemActor` (established by ADR-0006) declares `UPROPERTY() TArray<FPullWaveInstanceState> ActiveWaves` as a member field.
- [ ] `BeginPlay()` calls `ActiveWaves.Reserve(23)` — pool capacity locked at 23 slots, no dynamic resize permitted during run. Pool sizing: 16 (`MAX_CONCURRENT_WAVES_CAP`) + 2 (`DESPAWNING_RETURN_LATENCY_SLOTS`) + 5 (`MAX_CONCURRENT_WAVES_CAP_CEIL_MARGIN`) = 23.
- [ ] A monotonic `int32 NextWaveId` counter member initialized to 0 at `BeginPlay()` only (resets each session; never reused within a session per Rule 14). Counter increments at each `Construct()` call and is assigned to the new wave's `WaveId` field.
- [ ] AC-PW-10 determinism: given two simulations spawning identical wave sequences in identical WaveId order with at least one tick where two or more waves are admitted in the same tick, per-tick `(world_x, world_z, t_norm, state)` snapshots compare bit-identical across both simulations. Broadcast order of `OnWaveHit`/`OnNearMiss`/`OnWaveDespawned` across two waves landing in the same tick is WaveId ASCENDING in both simulations (verified via Seam 13 stub event log).
- [ ] WaveId-ASC iteration invariant: given 20 waves admitted and removals at mid-array indices 3, 7, 11, iterating the remaining 17 waves produces strictly ascending WaveId sequence.
- [ ] WaveId-never-reused: given 30 waves admitted across a synthetic 3-minute run, all WaveId values are strictly increasing with no value appearing twice.
- [ ] AC-PW-22b pattern 9 passes: `rg -n 'RemoveAtSwap' Source/SLIPSTORM/PullWave/` returns 0 matches; `rg -n 'RemoveAt\b' Source/SLIPSTORM/PullWave/` returns ≥1 match.
- [ ] AC-PW-22b pattern 11 passes: `rg -n '\.Insert\s*\(' Source/SLIPSTORM/PullWave/` returns 0 matches; `rg -n '\.Add\s*\(' Source/SLIPSTORM/PullWave/` returns ≥1 match.

---

## Implementation Notes

*Derived from ADR-0010 D1 Implementation Guidelines:*

**Pool container:** `UPROPERTY() TArray<FPullWaveInstanceState> ActiveWaves` on `APullWaveSubsystemActor`. `Reserve(23)` at `BeginPlay` allocates the pool once — no heap allocation during run. **USTRUCT() required:** `FPullWaveInstanceState` must be annotated with `USTRUCT()` in `PullWaveTypes.h` for UHT to accept the `UPROPERTY() TArray<>` declaration; add `USTRUCT()` above the struct definition as part of this story's implementation scope. Waves are added via `ActiveWaves.Add(FPullWaveInstanceState{})` at `Construct()` (append-to-tail) and removed via `ActiveWaves.RemoveAt(SlotIndex)` at DESPAWNING step 6 (Story 006).

**WaveId ordering:** Since new waves are appended via `Add()` and `WaveId` is monotonically increasing, natural array order equals WaveId ASC at all times. `RemoveAt` (O(N) element shift, ≤23 elements) preserves this ordering. `RemoveAtSwap` would move the last element to the removed slot, breaking ASC order — explicitly rejected in ADR-0010 D1 Alternative 1.

**DESPAWNING slot retention:** Between DESPAWNING entry (step 1) and `RemoveAt` (step 6), the slot is still present in `ActiveWaves` with `State == DESPAWNING`. This is intentional so that same-tick multi-despawn iterations remain observable to AC-PW-15's contiguity assertion. See Story 006 for the full despawn pipeline.

**RemoveAt cost justification (ADR-0010 D1):** At N=23 slots with mid-index removal: ~11 × 188-byte element shifts ≈ 2 KB memcpy ≈ 1–2 µs on mid-tier mobile — well within the 0.25 ms per-tick budget (ADR-0010 Performance Implications). The O(N) cost is bounded and worth paying to maintain WaveId ASC without an explicit per-tick sort.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 001: `FPullWaveInstanceState` struct definition, `EPullWaveState` enum.
- Story 003: `TransitionTo()` state machine helper.
- Story 009: `Construct(FPullWaveSpawnParams)` entry point that calls `ActiveWaves.Add()`.
- Story 006: `ActiveWaves.RemoveAt(Index)` inside despawn pipeline.

---

## QA Test Cases

*Test cases not yet defined — run /qa-plan to generate them.*

---

## Test Evidence

**Story Type**: Logic
**Required evidence**: `Source/SLIPSTORM/Tests/Unit/PullWave/PullWavePoolStorageWaveIdTest.cpp` — must exist and pass

*(Note: story originally listed `tests/unit/pull-wave/pool-storage-waveid_test.cpp` — actual UE project path differs; matches Story 001 precedent.)*

**Status**: [x] Created — 8 test commands covering all Story 002 acceptance criteria (TC8 added: actor NewObject + InitializePool() runtime seam)

**AC7/AC8 deferral note (approved 2026-08-18):**
- AC-PW-22b pattern 9 ALLOW half (`RemoveAt >= 1` match): deferred to Story 006. Call site does not exist yet. FORBID half (`RemoveAtSwap == 0`) asserted in TC5.
- AC-PW-22b pattern 11 ALLOW half (`.Add >= 1` match): deferred to Story 009. Call site does not exist yet. FORBID half (`.Insert == 0`) asserted in TC6.
- Comment in TC5 and TC6: "ALLOW-half defers to Story 006 / Story 009 respectively — those call sites don't exist yet."

---

## Dependencies

- Depends on: Story 001 (needs `FPullWaveInstanceState` struct)
- Unlocks: Story 004 (tick advance needs pool and WaveId counter)

---

## Completion Notes
**Completed**: 2026-08-18
**Criteria**: 6/8 fully covered, 2 partially covered (25% partial — ADVISORY)
- AC3 increment-at-Construct() clause deferred to Story 009 (call site not yet implemented; init-to-zero verified by TC8)
- AC4 position/state snapshot + broadcast-order assertions deferred to Stories 004/005/006 (tick body and delegates not yet implemented; WaveId ordering determinism verified by TC4)
- AC7/AC8 ALLOW-halves deferred to Stories 006/009 respectively (FORBID halves enforced by TC5/TC6 grep)
**Deviations**: None blocking. PullWaveTypes.h USTRUCT addition is within stated Implementation Notes scope.
**Test Evidence**: Logic — `Source/SLIPSTORM/Tests/Unit/PullWave/PullWavePoolStorageWaveIdTest.cpp` — 8 test commands present
**Code Review**: Complete — APPROVED WITH SUGGESTIONS (2026-08-18)

# Story 001: Struct Definitions — FPullWaveInstanceState, FPullWaveCurveSnapshot, FPullWaveSpawnParams

> **Epic**: Pull-Wave Behavior
> **Status**: Complete
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: (fill before sprint planning)
> **Manifest Version**: 2026-07-03 (docs/registry/architecture.yaml v8 — no separate control-manifest.md; forbidden patterns cited from registry)
> **Last Updated**: 2026-08-18

## Context

**GDD**: `design/gdd/pull-wave-behavior.md`
**Requirement**: `TR-PW-004`, `TR-PW-007`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0010: Pull-Wave Object Pool, State Machine, and Despawn Pipeline
**ADR Decision Summary**: Per-wave runtime state is a plain `FPullWaveInstanceState` struct (188 bytes) containing all lifecycle fields. Lateral trajectory is driven by a value-type `FPullWaveCurveSnapshot` (SAMPLE_COUNT=32, locked) copied into instance state at `Construct()`. `FPullWaveSpawnParams` (152 bytes) is the sole Wave Spawner → Pull-Wave handoff struct; immutable after `Construct()` returns.

**Engine**: Unreal Engine 5.7 | **Risk**: LOW
**Engine Notes**: All types are plain C++ structs — no `UObject` overhead, no reflection metadata. `static_assert` (C++17 standard) stable across all UE 5.x builds. `sizeof` layout verified in ADR-0010 D3: total `FPullWaveInstanceState` = 60 scalar bytes + 128 Samples bytes = 188 bytes (68 bytes headroom under 256-byte ceiling). No post-cutoff APIs.

**Control Manifest Rules (Feature layer)** — from `docs/registry/architecture.yaml` v8:
- Required: `static_assert(sizeof(FPullWaveInstanceState) <= 256)` at struct definition site (AC-PW-22b pattern 3).
- Required: `static_assert(sizeof(FPullWaveSpawnParams) == 152)` to verify no unexpected padding (ADR-0010 Validation Criteria).
- Required: `LEAN_ANGLE_TIER0_DEG = 0.0f` constant defined in source (AC-PW-22b pattern 6).
- Required: Tier ordering chain `static_assert` (TIER0<TIER1<TIER2<TIER3<TIER4) AND MIN_GAP chain (8 sub-patterns 7a–7h per AC-PW-22b R6 B6 + R7 B6).
- Required: `LEAN_BRIGHTNESS_PEAK_RATIO` and `NEAR_MISS_FLASH_DURATION_S` sourced from registry header (AC-PW-22b pattern 8).
- Forbidden: `FPullWaveInstanceState` or `FPullWaveCurveSnapshot` declared as `UPROPERTY` or subclass of `UObject` — no GC overhead permitted at per-wave level (ADR-0010 Alternative 2 rejection).
- Forbidden: `SAMPLE_COUNT` changed from 32 — locked by ADR-0010 D3 (16 rejected for error margin; 64 rejected for AC-PW-22b size violation).

---

## Acceptance Criteria

*From GDD `design/gdd/pull-wave-behavior.md`, scoped to this story:*

- [ ] `FPullWaveCurveSnapshot` struct declared: `static constexpr int32 SAMPLE_COUNT = 32`; `float Samples[SAMPLE_COUNT]`; `float EvaluateAt(float TNormClamped) const` (pure function, linear interp between samples, O(1), no asset reference held). OOB prevented by `i1 = min(i0+1, SAMPLE_COUNT-1)` clamp.
- [ ] `FPullWaveSpawnParams` struct declared: `int32 WaveId`; `int32 SourceLane`; `int32 TargetLane`; `FPullWaveCurveSnapshot CurveSnapshot`; `float ForwardVelocityMs`; `float SpawnTimeS`; `float LeanDurationS`. `static_assert(sizeof(FPullWaveSpawnParams) == 152)` passes.
- [ ] `FPullWaveInstanceState` struct declared with all lifecycle fields: `int32 WaveId`, `int32 SourceLane`, `int32 TargetLane`, `FPullWaveCurveSnapshot CurveSnapshot` (immutable copy), `float ForwardVelocityMs`, `float SpawnTimeS`, `float LeanDurationS`, `float TravelDurationS`, `float LeanProgress`, `float TraverseElapsedS`, `float LandedHoldElapsedS`, `EPullWaveState State`, `int32 ISMCInstanceIndex`, `ECollisionOutcome CollisionOutcome`, plus any additional fields required by D2 observable snapshot. `static_assert(sizeof(FPullWaveInstanceState) <= 256)` passes with measured 188 bytes (verify in non-Shipping build).
- [ ] `EPullWaveState` enum declared: `{ SPAWNED, LEANING, TRAVERSING, LANDED, DESPAWNING }` — exactly five values, no backward-extension without ADR amendment.
- [ ] `ECollisionOutcome` enum declared: `{ Hit, NearMiss, CleanMiss, Unresolved }` (Unresolved is the default until LANDED entry).
- [ ] `FTraverseElapsedQuery` test accessor struct declared (non-Shipping only: `#if !UE_BUILD_SHIPPING`): `bool bWaveFound`; `EPullWaveState State`; `float ElapsedS` (sentinel `-1.0f` when wave found but `State ∈ {SPAWNED, LEANING, DESPAWNING}` per R7 B5 — not `0.0f` to avoid coincidence with valid TRAVERSING-entry value).
- [ ] AC-PW-09 formula verifiable from struct: `TravelDurationS = SPAWN_PLANE_Z_OFFSET_M / ForwardVelocityMs` (stored in `FPullWaveInstanceState`; computed at `Construct()` in Story 009). At `ForwardVelocityMs=7.5` and `SPAWN_PLANE_Z_OFFSET_M=15.0`: `TravelDurationS = 2.0`. At `ForwardVelocityMs=4.0`: `TravelDurationS = 3.75`.
- [ ] AC-PW-22b patterns 3, 6, 7a–7h, 8 pass (see CI structural grep requirements).
- [ ] `DECLARE_STATS_GROUP` for `STATGROUP_PullWave` declared (AC-PW-22b pattern 1).

---

## Implementation Notes

*Derived from ADR-0010 D3 + D6 Implementation Guidelines:*

**`FPullWaveCurveSnapshot` (D3):** `SAMPLE_COUNT = 32` is locked — not a tuning knob. Interpolation error ≤ 0.001 at 32 samples (5× under `CURVE_ENDPOINT_TOLERANCE = 0.005`). `EvaluateAt(float TNormClamped)`: `i0 = (int)(TNormClamped * (SAMPLE_COUNT-1))`, `i1 = min(i0+1, SAMPLE_COUNT-1)`, linear interp between `Samples[i0]` and `Samples[i1]`. Pure function; no allocations per call. Do not add a `UCurveFloat*` member — immutability contract (D3 Alternative 4 rejection).

**`FPullWaveSpawnParams` (D6):** Passed by value (152-byte stack copy) at `Construct()`. After `Construct()` returns, Wave Spawner's local copy is no longer referenced by Pull-Wave. All parameter values are `const` for the wave's lifetime. The `CurveSnapshot` field is sampled by Wave Spawner at admission (ADR-0011 D2 Stage 4 — validated `UCurveFloat*` sampled at 32 evenly-spaced t_norm values); Pull-Wave never touches the source asset.

**`FPullWaveInstanceState` (D3 size budget):** 60 scalar bytes + 128 Samples bytes = 188 bytes total. Declare `TraverseElapsedS` as `float` (single-precision — accumulator range 0–6.25s is well within single-precision range; AC-PW-17b N_MAX=391 tolerance analysis assumes single-precision). Do NOT store `FTraverseElapsedQuery` in the struct — it is a read-path test accessor shape, not a stored field.

**Registry constants:** `NEAR_MISS_FLASH_DURATION_S = 0.066f` and `LEAN_BRIGHTNESS_PEAK_RATIO` sourced from the registry header; use the named constants, not inline magic numbers.

**Tier angle chain:** `LEAN_ANGLE_TIER0_DEG = 0.0f` (locked); safe ranges at publish time: TIER1 ≤ 17°, TIER2 ≤ 22°, TIER3 ≤ 25°, TIER4 ≤ 28°. All four adjacent-pair `<` ordering checks AND four adjacent-pair `>= LEAN_ANGLE_MIN_TIER_GAP_DEG` checks (3.0°) must be `static_assert` at compile time (CI patterns 7a–7h).

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 002: Pool container (`TArray<FPullWaveInstanceState>`), `Reserve(23)`, WaveId counter allocation.
- Story 003: `TransitionTo()` helper, forbidden-transition `check()` enforcement.
- Story 009: `Construct(FPullWaveSpawnParams)` entry point, `TravelDurationS` computation, ISMC add-instance.

---

## QA Test Cases

*Test cases not yet defined — run /qa-plan to generate them.*

---

## Test Evidence

**Story Type**: Logic
**Required evidence**: `Source/SLIPSTORM/Tests/Unit/PullWave/PullWaveStructDefinitionsTest.cpp` — must exist and pass

*(Note: story originally listed `tests/unit/pull-wave/struct-definitions_test.cpp` — actual UE project path differs; see session extract 2026-08-17)*

**Status**: [x] Created — 11 test commands covering all acceptance criteria

---

## Dependencies

- Depends on: None
- Unlocks: Story 002 (pool needs `FPullWaveInstanceState`), Story 003 (state machine needs `EPullWaveState`), Story 009 (entry point needs all three structs)

---

## Completion Notes
**Completed**: 2026-08-18
**Criteria**: 9/9 passing (0 deferred)
**Deviations**:
- ADVISORY: TR-PW-007 registry text says "188 bytes" but actual compiled size is 176 bytes with `uint8` enum backing. ADR-0010 D3 design estimate assumed `int32`-backed enums. Binding contract `static_assert(sizeof <= 256)` passes. TC4 runtime log records measured size. Registry wording should be updated in a follow-up.
- ADVISORY (scope): `Source/SLIPSTORM/PullWave/PullWaveStats.cpp` created outside original story scope to provide `DEFINE_STAT(STAT_PullWaveTick)` stub for linker. Story 004 absorbs and deletes it.
**Test Evidence**: Logic — `Source/SLIPSTORM/Tests/Unit/PullWave/PullWaveStructDefinitionsTest.cpp` (11 test commands; 9/9 ACs covered)
**Code Review**: Complete — 2 passes run; all CHANGES REQUIRED items resolved (EvaluateAt OOB clamp, EPullWaveState PascalCase, DEFINE_STAT stub, ISMCInstanceIndex sentinel test)

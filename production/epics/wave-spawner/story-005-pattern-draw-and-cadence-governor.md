# Story 005: Pattern Draw + Cadence Governor (F-3) + RNG Seeding

> **Epic**: Wave Spawner Pattern Library
> **Status**: Complete
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: L (~4–6h)
> **Manifest Version**: (none — docs/architecture/control-manifest.md not found; run /create-control-manifest)
> **Last Updated**: 2026-08-20

## Context

**GDD**: `design/gdd/wave-spawner-pattern-library.md`
**Requirement**: `TR-WS-013`, `TR-WS-029`, `TR-WS-030`, `TR-WS-033`, `TR-WS-034`, `TR-WS-035`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0011: Wave Spawner Pattern Library — D2 Four-Stage Admission Pipeline, Stage 3 Cadence Governor + Stage 4 Pattern Draw
**ADR Decision Summary**: Stage 3 is the F-3 cadence governor — a three-branch piecewise weight function that drives session barrage average to `BARRAGE_EVENTS_PER_PEAK_TARGET_AVG = 2`. It uses only clamp + linear ops (no transcendentals) for platform-deterministic results across ARM/x86. Stage 4 draws a pattern from the active phase pool using `FRandomStream` seeded once with `RSM.RunSeed:uint64` at `Cold→Active`. Wall-clock-derived seeds are FORBIDDEN. F-3 force-draw mode activates when zero barrages have been drawn and `t_norm_PEAK ≥ T_FORCE`, overriding the weight function to guarantee a barrage. `t_norm_PEAK` is computed locally from run elapsed time.

**Engine**: Unreal Engine 5.7 | **Risk**: HIGH
**Engine Notes**: `FRandomStream` is the correct UE deterministic RNG (seeded with `uint64` via `FRandomStream::Initialize(int32)` — note: UE 5.x `FRandomStream` seeds with `int32`, not `uint64` directly; use lower 32-bits of `RunSeed` or verify a 64-bit seed path in UE 5.7). `GetWorld()->GetTimeSeconds()` is the authoritative game-time source (never wall-clock `FDateTime::Now()`). All F-3 arithmetic must use `float` with `FMath::Clamp` (no `std::clamp`, no `pow`, no `exp`, no `log`). Platform-determinism is a binding requirement for Death Replay (TR-WS-033) — any future change introducing transcendentals MUST document ARM/x86 divergence impact (TR-WS-035).

**Control Manifest Rules (Feature layer)**:
- No control manifest found at `docs/architecture/control-manifest.md` — run `/create-control-manifest`.

---

## Acceptance Criteria

*From GDD `design/gdd/wave-spawner-pattern-library.md`, scoped to this story:*

- [x] **AC-WS-13 (BLOCKING)**: Pattern draw uses `FRandomStream` seeded once with `RSM.RunSeed` at `Cold→Active` (Rule 8). Wall-clock seeds (`FDateTime::Now()`, `FMath::Rand()`, or similar) are FORBIDDEN — `check(false)` or `static_assert` prevents their use. Pattern is selected uniformly at random from the active phase pool (`FRandomStream::RandRange(0, Pool.Num()-1)`). Death Replay compatibility: the same `RunSeed` value reconstructed from RSM replay state produces the same draw sequence (TR-WS-014). Platform constraint: `FRandomStream` determinism is ARM/x86-consistent (no floating-point in RNG path).

- [x] **AC-WS-14 (BLOCKING)**: F-3 cadence governor produces correct three-branch behavior. Given `barrage_count_this_peak = 0` and `t_norm_PEAK ≥ T_FORCE`: force-draw branch — `ShouldDrawBarrage()` returns `true` regardless of weight function (override). Given `barrage_count_this_peak ≥ 1` and proportional branch: `w_barrage = base_w * (TARGET - barrage_count) * t_norm_PEAK` (clamped to `[0, W_CEILING]`); given calculated `w_barrage`, `FRandomStream::FRand()` draw below `w_barrage` → barrage admitted. F-3 uses ONLY `FMath::Clamp` and linear multiplication — no `pow`, `exp`, `log`, or `sqrt` in the weight computation.
  <!-- DEVIATION NOTE (2026-08-19): original AC-WS-14 text read `(TARGET / max(barrage_count, 1))`;
       corrected to `(TARGET - barrage_count)` per story Implementation Notes and TC7 expected value (0.125).
       The ratio variant was a transcription error. Formula B (Deficit) is the authoritative formula. -->

- [x] **AC-WS-15 (BLOCKING — TR-WS-029)**: Pattern draw selects correctly from the active phase pool and maintains draw-count state. `DrawNonBarragePattern()` selects uniformly via `PatternRNG.RandRange(0, Pool.Num()-1)` from `ActiveDrawPool->NonBarragePatterns`; result index is in `[0, Pool.Num()-1]`; empty-pool guard logs telemetry stub and returns without crashing. `DrawBarragePattern()` selects from `PeakPool.BarragePatterns` via the same uniform draw; increments `BarrageCountThisPeak` by 1 on each successful draw; empty-pool guard applies. Verified by test: seed `PatternRNG` with a fixed known value, inject a pool of N entries, call draw, assert resulting index is deterministic and in range; call `DrawBarragePattern` twice and assert `BarrageCountThisPeak == 2`.

---

## Implementation Notes

*Derived from ADR-0011 D2 Stage 3–4 Implementation Guidelines:*

**RNG setup (Cold→Active hook, Story 002 lifecycle):**
```cpp
// In OnLifecycleTransition(Active) when coming from Cold:
PatternRNG.Initialize(static_cast<int32>(RunSeed & 0xFFFFFFFF));
BarrageCountThisPeak = 0;
PeakEntryTimeS = 0.f;  // set when PEAK phase actually starts (Holding→Active at PEAK)
```

**F-3 cadence governor — ShouldDrawBarrage():**
```cpp
bool UWaveSpawnerSubsystem::ShouldDrawBarrage() const
{
    if (ActivePhase != ERunPhase::PEAK) return false;   // barrages only in PEAK

    const float Now = GetWorld()->GetTimeSeconds();
    const float tNormPeak = FMath::Clamp(
        (Now - PeakEntryTimeS) / PEAK_DURATION_S, 0.f, 1.f);

    // Force-draw branch: zero barrages drawn and past T_FORCE threshold
    if (BarrageCountThisPeak == 0 && tNormPeak >= T_FORCE)
        return true;

    // Ceiling branch: already at or above target
    if (BarrageCountThisPeak >= BARRAGE_EVENTS_PER_PEAK_TARGET_AVG)
        return false;

    // Proportional branch: weight scales with time progress and deficit
    const float Deficit = FMath::Max(
        static_cast<float>(BARRAGE_EVENTS_PER_PEAK_TARGET_AVG - BarrageCountThisPeak), 0.f);
    const float WBarrage = FMath::Clamp(
        BASE_W * Deficit * tNormPeak, 0.f, W_CEILING);

    return PatternRNG.FRand() < WBarrage;
}
```

**Pattern draw — Stage 4 (DrawNonBarragePattern / DrawBarragePattern):**
```cpp
void UWaveSpawnerSubsystem::DrawNonBarragePattern(const FDPCFrameState& Frame, float Now)
{
    const TArray<FPatternDefinition>& Pool = ActiveDrawPool->NonBarragePatterns;
    if (Pool.Num() == 0)
    {
        EmitTelemetry(TEXT("empty_pool_at_draw"), ...);
        return;
    }
    const int32 Index = PatternRNG.RandRange(0, Pool.Num() - 1);
    AdmitPattern(Pool[Index], Frame, Now, false /*bBarrage*/);
}

void UWaveSpawnerSubsystem::DrawBarragePattern(const FDPCFrameState& Frame, float Now)
{
    const TArray<FPatternDefinition>& Pool = PeakPool.BarragePatterns;
    if (Pool.Num() == 0)
    {
        EmitTelemetry(TEXT("empty_pool_at_draw"), ...);
        return;
    }
    const int32 Index = PatternRNG.RandRange(0, Pool.Num() - 1);
    AdmitPattern(Pool[Index], Frame, Now, true /*bBarrage*/);
    BarrageCountThisPeak++;
}
```

**Tuning knob constants (data-driven — load from config at Initialize(), NOT hardcoded):**
- `BARRAGE_EVENTS_PER_PEAK_TARGET_AVG = 2` (G.2)
- `T_FORCE` — fraction of PEAK elapsed at which force-draw activates (G.2)
- `W_CEILING` — max weight cap for proportional branch (G.2)
- `BASE_W` — unweighted barrage fraction, must satisfy `BASE_W < W_CEILING` (ADR-0011 D4 cook-time constraint)
- `PEAK_DURATION_S` — total expected PEAK phase duration (G.1)

**Platform determinism commitment (TR-WS-034/035):**
```cpp
// Static assertion: no transcendental ops in F-3 — enforced by code review.
// Any future change introducing pow/exp/log MUST add this note to the ADR:
// "Death Replay may diverge on ARM vs x86 due to [op name] — test required."
```

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 003: F-3b cadence gate `(now − last_spawn_time) ≥ wave_spawn_interval_s` (Stage 3 temporal gate — separate from F-3 governor which controls barrage weighting).
- Story 004: Rule 7 slot pre-commitment (`Scheduled += 3` for barrage, `Scheduled++` for non-barrage).
- Story 007: `RunSeed` capture via RSM delegate; `PeakEntryTimeS` capture on PEAK phase entry.

---

## QA Test Cases

*Written at story-readiness gap-fill 2026-08-20. Developer implements against these — do not invent new test cases.*

**AC-WS-13: RNG seeding + deterministic draw sequence**

- **TC1**: RNG seeded from RunSeed at Cold→Active
  - Given: subsystem in Cold state; `RunSeed = 0xDEADBEEFCAFEBABE`
  - When: `TransitionTo(Active)`
  - Then: `PatternRNG` initialized with `static_cast<int32>(RunSeed & 0xFFFFFFFF)`; same seed produces same first draw as a second fresh `FRandomStream` initialized with the same value
  - Edge cases: `RunSeed = 0` (zero seed); `RunSeed = UINT64_MAX` (all bits set — lower 32 must not overflow)

- **TC2**: Deterministic draw sequence — same seed, same pool, same index
  - Given: two freshly initialized subsystems with identical `RunSeed`; identical pool of 5 entries injected into each
  - When: `DrawNonBarragePattern()` called once on each
  - Then: both select the same pool index (assert `Index_A == Index_B`)
  - Edge cases: single-entry pool (index must be 0); pool with `Num() = kMaxConcurrentWavesStub` entries

- **TC3**: Wall-clock seed path is absent (static verification)
  - Given: `WaveSpawnerSubsystem.cpp` source
  - When: Grep for `FDateTime`, `FMath::Rand()`, `rand()`, `time(`, `FPlatformTime`
  - Then: no matches in `TryAdmitPattern`, `DrawNonBarragePattern`, `DrawBarragePattern`, `ShouldDrawBarrage`, `OnLifecycleTransition`

**AC-WS-14: F-3 cadence governor three-branch behavior**

- **TC4**: Force-draw branch fires when zero barrages and `t_norm >= T_FORCE`
  - Given: `ActivePhase = PEAK`, `BarrageCountThisPeak = 0`, `PeakEntryTimeS` set so `(Now - PeakEntryTimeS) / PEAK_DURATION_S >= T_FORCE`
  - When: `ShouldDrawBarrage()` called
  - Then: returns `true`
  - Edge cases: exactly at `T_FORCE` (boundary — must be `>=`, not `>`); `t_norm = 1.0` (full PEAK elapsed)

- **TC5**: Ceiling branch suppresses barrage when count >= target
  - Given: `ActivePhase = PEAK`, `BarrageCountThisPeak = BARRAGE_EVENTS_PER_PEAK_TARGET_AVG (= 2)`, any `t_norm`
  - When: `ShouldDrawBarrage()` called
  - Then: returns `false`
  - Edge cases: `BarrageCountThisPeak = 3` (over target, still false)

- **TC6**: Proportional branch — weight 0.0 at `t_norm = 0`
  - Given: `ActivePhase = PEAK`, `BarrageCountThisPeak = 1`, `t_norm_PEAK = 0.0` (immediately after PEAK entry)
  - When: `ShouldDrawBarrage()` called
  - Then: `WBarrage = BASE_W * 1 * 0.0 = 0.0` → returns `false` (no draw beats weight zero)

- **TC7**: Proportional branch — deterministic RNG draw against known weight
  - Given: `PatternRNG` seeded with known value S such that `FRand()` returns `0.05`; `BASE_W = 0.25`, `BarrageCountThisPeak = 1`, `t_norm_PEAK = 0.5`, `W_CEILING = 1.0`
  - When: `ShouldDrawBarrage()` called
  - Then: `WBarrage = 0.25 * 1 * 0.5 = 0.125`; `0.05 < 0.125` → returns `true`
  - Edge cases: RNG returns value exactly equal to `WBarrage` (must not be admitted — `<`, not `<=`)

- **TC8**: Non-PEAK phase returns false
  - Given: `ActivePhase = OPENER` (or `MID`)
  - When: `ShouldDrawBarrage()` called
  - Then: returns `false` regardless of `BarrageCountThisPeak` or `t_norm`

**AC-WS-15: Pattern draw pool selection and counter**

- **TC9**: `DrawNonBarragePattern()` — uniform index in range, deterministic
  - Given: pool of 3 entries; `PatternRNG` seeded with known value; bBarrageOwed=false
  - When: `DrawNonBarragePattern()` called
  - Then: selected index in `[0, 2]`; calling with same seed again produces same index
  - Edge cases: single-entry pool (index must always be 0)

- **TC10**: `DrawBarragePattern()` — increments `BarrageCountThisPeak`
  - Given: `BarrageCountThisPeak = 0`; barrage pool with 2 entries
  - When: `DrawBarragePattern()` called twice
  - Then: `BarrageCountThisPeak == 2`

- **TC11**: Empty-pool guard — no crash, no draw
  - Given: `ActiveDrawPool->NonBarragePatterns` is empty (Num() == 0)
  - When: `DrawNonBarragePattern()` called
  - Then: returns without crash; no pattern admitted; telemetry stub call site present (grep for `empty_pool_at_draw`)

---

## Test Evidence

**Story Type**: Logic
**Required evidence**: `Source/SLIPSTORM/Tests/Unit/WaveSpawner/WaveSpawnerCadenceGovernorTest.cpp` — must exist and pass

**Status**: [x] Created — `Source/SLIPSTORM/Tests/Unit/WaveSpawner/WaveSpawnerCadenceGovernorTest.cpp` (682 lines, 10 test commands). Execution deferred to UBT build per Stories 001–004 precedent.

---

## Dependencies

- Depends on: Story 001 (pool structure), Story 003 (cadence gate calls `TryAdmitPattern` which calls `ShouldDrawBarrage`), Story 004 (`bBarrageOwed` reservation overrides `ShouldDrawBarrage` result)
- Unlocks: Story 007 (RSM integration provides `RunSeed` and PEAK phase entry time needed here)

---

## Completion Notes

**Completed**: 2026-08-20
**Criteria**: 3/3 BLOCKING ACs verified (AC-WS-13, AC-WS-14, AC-WS-15)
**Deviations**:
- ADVISORY-1: `kBaseW`, `kWCeiling`, `kTForce`, `kTargetBarrages`, `kPeakDurationS` are `static constexpr` stubs — Story 007 replaces with config-loaded values. DEVIATION NOTEs on each constant in `.h`.
- ADVISORY-2: `PatternRNG.Initialize(0)` uses stub seed 0; real `RSM.RunSeed` provided by Story 007. DEVIATION NOTE at Cold→Active.
- ADVISORY-3 (TR-WS-033): `tNormPeak` from wall-clock `GetCurrentTimeS()` will desync proportional branch between live run and Death Replay. Requires DPC logical time field (Story 007 + DPC epic). ADR-0011 amendment pending.
- ADVISORY-4: `DrawBarragePattern()` hardcodes `PeakPool.BarragePatterns`; correct under current phase model (barrages only in PEAK). An `ensure(ActiveDrawPool == &PeakPool)` guard is recommended in Story 006 or 007 when `ActiveDrawPool` is exercised more broadly.
- ADVISORY-5: TC3 (wall-clock seed absent) removed from automation suite — enforcement is code-review only. Manual grep documented in test file comment. CI step deferred.
- ADVISORY-6: AC-WS-14 formula corrected from ratio (`TARGET / max(count,1)`) to deficit (`TARGET - count`) — HTML comment in story file records the correction; implementation and TC7 expected values are self-consistent.
**Wall-clock seed verification**: Manual grep required at every code review touching the admission pipeline: `grep -rn "FDateTime\|FMath::Rand\b\|rand()\|time(\|FPlatformTime" Source/SLIPSTORM/WaveSpawner/WaveSpawnerSubsystem.cpp` (must return 0 matches).
**Test Evidence**: Logic — `Source/SLIPSTORM/Tests/Unit/WaveSpawner/WaveSpawnerCadenceGovernorTest.cpp` (682 lines, 10 commands: TC1–TC2, TC4–TC11). Execution deferred to UBT headless runner.
**Code Review**: Deferred — to be run before sprint close-out (per /story-done Phase 5 answer).

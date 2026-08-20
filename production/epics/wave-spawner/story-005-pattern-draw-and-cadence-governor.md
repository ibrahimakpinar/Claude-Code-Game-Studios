# Story 005: Pattern Draw + Cadence Governor (F-3) + RNG Seeding

> **Epic**: Wave Spawner Pattern Library
> **Status**: Ready
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: (fill before sprint planning)
> **Manifest Version**: (none — docs/architecture/control-manifest.md not found; run /create-control-manifest)
> **Last Updated**: 2026-08-19

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

- [ ] **AC-WS-13 (BLOCKING)**: Pattern draw uses `FRandomStream` seeded once with `RSM.RunSeed` at `Cold→Active` (Rule 8). Wall-clock seeds (`FDateTime::Now()`, `FMath::Rand()`, or similar) are FORBIDDEN — `check(false)` or `static_assert` prevents their use. Pattern is selected uniformly at random from the active phase pool (`FRandomStream::RandRange(0, Pool.Num()-1)`). Death Replay compatibility: the same `RunSeed` value reconstructed from RSM replay state produces the same draw sequence (TR-WS-014). Platform constraint: `FRandomStream` determinism is ARM/x86-consistent (no floating-point in RNG path).

- [ ] **AC-WS-14 (BLOCKING)**: F-3 cadence governor produces correct three-branch behavior. Given `barrage_count_this_peak = 0` and `t_norm_PEAK ≥ T_FORCE`: force-draw branch — `ShouldDrawBarrage()` returns `true` regardless of weight function (override). Given `barrage_count_this_peak ≥ 1` and proportional branch: `w_barrage = base_w * (TARGET / max(barrage_count, 1)) * t_norm_PEAK` (clamped to `[0, W_CEILING]`); given calculated `w_barrage`, `FRandomStream::FRand()` draw below `w_barrage` → barrage admitted. F-3 uses ONLY `FMath::Clamp` and linear multiplication — no `pow`, `exp`, `log`, or `sqrt` in the weight computation.

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

*Test cases not yet defined — run /qa-plan to generate them.*

---

## Test Evidence

**Story Type**: Logic
**Required evidence**: `Source/SLIPSTORM/Tests/Unit/WaveSpawner/WaveSpawnerCadenceGovernorTest.cpp` — must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: Story 001 (pool structure), Story 003 (cadence gate calls `TryAdmitPattern` which calls `ShouldDrawBarrage`), Story 004 (`bBarrageOwed` reservation overrides `ShouldDrawBarrage` result)
- Unlocks: Story 007 (RSM integration provides `RunSeed` and PEAK phase entry time needed here)

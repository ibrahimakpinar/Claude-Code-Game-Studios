# Story 003: Five-State Machine + TransitionTo() Helper

> **Epic**: Pull-Wave Behavior
> **Status**: Complete
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: 2–3 hours
> **Manifest Version**: 2026-07-03 (docs/registry/architecture.yaml v8 — no separate control-manifest.md; forbidden patterns cited from registry)
> **Last Updated**: 2026-08-18

## Context

**GDD**: `design/gdd/pull-wave-behavior.md`
**Requirement**: `TR-PW-002`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0010: Pull-Wave Object Pool, State Machine, and Despawn Pipeline
**ADR Decision Summary**: Per-wave state transitions are enforced by a `TransitionTo(EPullWaveState)` helper that calls `check()` at every transition site against a 13-cell forbidden-transition table. In non-Shipping builds, `check()` aborts; in Shipping, the parallel guard logs `pull_wave_illegal_transition` telemetry and no-ops. SPAWNED is an entry-only state (only reachable at `Construct()`); DESPAWNING is a terminal state (no exit except pool removal).

**Engine**: Unreal Engine 5.7 | **Risk**: LOW
**Engine Notes**: `check()` / `ensure()` / `UE_LOG()` — all stable pre-UE-4. Build configuration scoping (`!UE_BUILD_SHIPPING`) via preprocessor — stable. `DECLARE_LOG_CATEGORY_EXTERN(LogPullWave, Log, All)` pattern matches sibling subsystems. No post-cutoff APIs.

**Control Manifest Rules (Feature layer)** — from `docs/registry/architecture.yaml` v8:
- Required: `TransitionTo()` MUST be the single transition call site; direct `State = NewState` assignment outside `TransitionTo()` is forbidden.
- Required: Shipping-safe guard alongside every `check()`: `if (!cond) { UE_LOG(LogPullWave, Error, TEXT("pull_wave_illegal_transition: %s→%s WaveId=%d"), ...); return; }` (per ADR-0010 Risks table — Shipping-Safety Enforcement Policy).
- Required: `SCOPE_CYCLE_COUNTER(STAT_PullWaveTick)` inside `Tick()` body of `APullWaveSubsystemActor` (AC-PW-22b pattern 2 — verified in Story 004 but declared here alongside STATGROUP).
- Required: `DECLARE_STATS_GROUP` for `STATGROUP_PullWave` (AC-PW-22b pattern 1 — declared with structs in Story 001; confirm present).

---

## Acceptance Criteria

*From GDD `design/gdd/pull-wave-behavior.md` §State Machine Transitions, scoped to this story:*

- [ ] `EPullWaveState` enum (declared in Story 001) is used by `TransitionTo(FPullWaveInstanceState& Wave, EPullWaveState NewState)` helper on `APullWaveSubsystemActor` (or a free function in the pull-wave module).
- [ ] AC-PW-11 (SPAWNED→LEANING): `TransitionTo()` allows this transition. After `Construct()` sets `State=SPAWNED`, the next tick's state advance calls `TransitionTo(Wave, LEANING)` and succeeds. All mutable params (`target_lane`, `source_lane`, `lean_duration_s`, `forward_velocity_ms`, `CurveSnapshot`) are frozen — late mutation attempts produce no observable change in the wave's behavior fields.
- [ ] AC-PW-12 (LEANING→TRAVERSING): `TransitionTo()` allows this transition. `TraverseElapsedS` is initialized to `0.0f` at TRAVERSING entry. `LeanProgress` is NOT cleared (it is at `1.0f` at the transition boundary). Transition fires when `LeanProgress >= 1.0` per tick body (Story 004).
- [ ] AC-PW-13 (TRAVERSING→LANDED): `TransitionTo()` allows this transition. Entry fires on first tick where `world_z <= player_plane_z` (threshold-cross). `TraverseElapsedS` is clamped to `TravelDurationS` at LANDED entry. `CollisionOutcome` defaults to `Unresolved` until LANDED-entry evaluation (Story 005).
- [ ] AC-PW-14 (LANDED→DESPAWNING): `TransitionTo()` allows this transition. Fires after `WAVE_DESPAWN_HOLD_S` elapsed. No re-fires of `OnWaveHit`/`OnNearMiss` during hold (enforced by Story 005 constraint, not re-checked here).
- [ ] DESPAWNING is a terminal state: `TransitionTo(Wave, AnythingFromDESPAWNING)` always fails the forbidden-transition check.
- [ ] Forbidden-transition assertion test (non-Shipping build): each of the 13 forbidden transitions in the 25-cell matrix triggers `check()` / `UE_LOG(Error)`. Specifically verified: `SPAWNED→TRAVERSING`, `SPAWNED→LANDED`, `LEANING→LANDED`, `TRAVERSING→SPAWNED`, `LANDED→LEANING`, `DESPAWNING→SPAWNED`, `DESPAWNING→LEANING`, plus remaining 6 forbidden cells.
- [ ] Shipping-safe guard: in Test/Shipping build configuration, forbidden-transition call does NOT crash; logs `LogPullWave Error` with `pull_wave_illegal_transition` and returns without modifying state.

---

## Implementation Notes

*Derived from ADR-0010 D2 Implementation Guidelines:*

**Transition table (25 cells — 12 legal, 13 forbidden):**

| From \ To | SPAWNED | LEANING | TRAVERSING | LANDED | DESPAWNING |
|-----------|---------|---------|------------|--------|------------|
| SPAWNED | — | ✅ | ❌ | ❌ | ❌ (only via LEANING pause-flush) |
| LEANING | ❌ | — | ✅ | ❌ | ✅ (pause-flush/run-term) |
| TRAVERSING | ❌ | ❌ | — | ✅ | ✅ (pause-flush/run-term) |
| LANDED | ❌ | ❌ | ❌ | — | ✅ |
| DESPAWNING | ❌ | ❌ | ❌ | ❌ | — |

Note: `SPAWNED→DESPAWNING` is shown ❌ in the primary table but the pause-flush pathway can route from LEANING to DESPAWNING (not SPAWNED). SPAWNED to DESPAWNING direct is forbidden; SPAWNED transitions out only to LEANING.

**`TransitionTo()` pattern:**
```cpp
void APullWaveSubsystemActor::TransitionTo(FPullWaveInstanceState& Wave, EPullWaveState NewState)
{
    const bool bAllowed = IsTransitionAllowed(Wave.State, NewState);
    check(bAllowed); // fires in non-Shipping
    if (!bAllowed)
    {
        UE_LOG(LogPullWave, Error,
               TEXT("pull_wave_illegal_transition: WaveId=%d %s→%s"),
               Wave.WaveId, *LexToString(Wave.State), *LexToString(NewState));
        return; // Shipping-safe no-op
    }
    Wave.State = NewState;
}
```

**`IsTransitionAllowed()` must be a `constexpr` or `static` function** (so it can be evaluated in `static_assert` or test contexts without instantiating the subsystem).

**Log category:** Story 003 must declare the `LogPullWave` log category — it is not in `PullWaveTypes.h`.
Add to `PullWaveSubsystemActor.h`:
```cpp
DECLARE_LOG_CATEGORY_EXTERN(LogPullWave, Log, All);
```
Add to `PullWaveSubsystemActor.cpp`:
```cpp
DEFINE_LOG_CATEGORY(LogPullWave);
```

**`EPullWaveState` string conversion:** `EPullWaveState` is a plain `enum class : uint8`, not `UENUM()`, so `LexToString()` is not available. Use a static helper instead:
```cpp
static const TCHAR* PullWaveStateToString(EPullWaveState S)
{
    switch (S)
    {
        case EPullWaveState::Spawned:     return TEXT("SPAWNED");
        case EPullWaveState::Leaning:     return TEXT("LEANING");
        case EPullWaveState::Traversing:  return TEXT("TRAVERSING");
        case EPullWaveState::Landed:      return TEXT("LANDED");
        case EPullWaveState::Despawning:  return TEXT("DESPAWNING");
        default:                          return TEXT("UNKNOWN");
    }
}
```
Use `PullWaveStateToString(Wave.State)` in the `UE_LOG` call instead of `LexToString(Wave.State)`.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 004: Per-tick state advance body; pause-freeze gate; accumulator updates that trigger transitions.
- Story 006: DESPAWNING-entry pipeline (the six-step sequence that follows the `TransitionTo(DESPAWNING)` call).
- Story 007: Pause-flush batch routing (calls `TransitionTo(DESPAWNING)` for LEANING/TRAVERSING/LANDED waves).

---

## QA Test Cases

*Test cases not yet defined — run /qa-plan to generate them.*

---

## Test Evidence

**Story Type**: Logic
**Required evidence**: `Source/SLIPSTORM/Tests/Unit/PullWave/PullWaveStateMachineTest.cpp` — must exist and pass

**Status**: [x] Created — `Source/SLIPSTORM/Tests/Unit/PullWave/PullWaveStateMachineTest.cpp` (12 test commands)

---

## Dependencies

- Depends on: Story 001 (needs `EPullWaveState` + `FPullWaveInstanceState`)
- Unlocks: Story 004 (tick advance uses `TransitionTo()`), Story 006 (despawn pipeline calls `TransitionTo(DESPAWNING)`)

---

## Completion Notes
**Completed**: 2026-08-18
**Criteria**: 8/8 passing (AC-PW-13 threshold-cross trigger deferred to Story 004; AC-PW-14 no-refires deferred to Story 005 — both within stated scope)
**Deviations**:
- ADVISORY: `check(bAllowed)` gated out of `UE_BUILD_TEST` (`#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST`) to enable TC10 Shipping-safe guard test. Debug/Development abort preserved; Test builds now use the guard path. Accepted as correct trade-off.
- ADVISORY: Story doc text says "13 forbidden" transitions in AC and ADR Decision Summary — corrected to 14 in implementation, tests, and header. Story doc text not updated (minor doc drift).
**Test Evidence**: Logic — `Source/SLIPSTORM/Tests/Unit/PullWave/PullWaveStateMachineTest.cpp` (12 test commands; TC10 exercises `TransitionTo()` on forbidden pair via `AddExpectedError()`)
**Code Review**: Complete — APPROVED WITH SUGGESTIONS (required change: TC10 fix applied)

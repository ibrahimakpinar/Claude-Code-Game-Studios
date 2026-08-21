# Story 003: Admission Gate + Cadence Gate (F-3b) + Primer Bypass

> **Epic**: Wave Spawner Pattern Library
> **Status**: Complete
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: S (~2h)
> **Manifest Version**: (none — docs/architecture/control-manifest.md not found; run /create-control-manifest)
> **Last Updated**: 2026-08-20

## Context

**GDD**: `design/gdd/wave-spawner-pattern-library.md`
**Requirement**: `TR-WS-012`, `TR-WS-015`, `TR-WS-016`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0011: Wave Spawner Pattern Library — D2 Four-Stage Admission Pipeline (primary)
**ADR Decision Summary**: Each spawner tick runs a four-stage admission pipeline. Stage 1 (Rule 1 gate) reads the DPC `FDPCFrameState` snapshot once per tick at pipeline top and treats it as immutable for the rest of that tick. The snapshot is checked for: DPC `is_active`, RSM not paused, RSM not in resume-grace window. Stage 2 (concurrency cap, Rule 7) is handled by Story 004. Stage 3 (cadence gate, F-3b) checks `(now − last_spawn_time) ≥ wave_spawn_interval_s`. Stage 4 (pattern draw) is handled by Story 005. Rule 2a Primer Bypass: on `Cold→Active`, the first draw admits immediately — cadence gate is bypassed — and `last_spawn_time = now` is set post-primer.

**Engine**: Unreal Engine 5.7 | **Risk**: HIGH
**Engine Notes**: DPC `OnPostTickFrameStatePublished` subscription (Story 007) pins tick ordering. The DPC snapshot read must occur after `OnPostTickFrameStatePublished` fires — enforced by subscription architecture, not inline timing. `FDPCFrameState` immutability across a tick is a contract enforced by not re-reading mid-tick; no engine-level copy semantic needed. `wave_spawn_interval_s` is a runtime data-driven config value (G.1 tuning knob) — must not be hardcoded.

**Control Manifest Rules (Feature layer)**:
- No control manifest found at `docs/architecture/control-manifest.md` — run `/create-control-manifest`.

---

## Acceptance Criteria

*From GDD `design/gdd/wave-spawner-pattern-library.md`, scoped to this story:*

- [x] **AC-WS-10 (BLOCKING)**: Per-tick DPC snapshot is captured once at Rule 1 gate top and treated as immutable for the remainder of that tick. Given `DPC.is_active = false`: no pattern is admitted; `last_spawn_time` is NOT updated. Given `DPC.is_active = true`, `RSM.is_paused = true`: no pattern admitted. Given DPC active + RSM not paused + RSM in resume_grace window: no pattern admitted. Verified via `FDPCTestStub` and `FRSMTestStub` injected at `Initialize()`.

- [x] **AC-WS-11 (BLOCKING)**: F-3b cadence gate: pattern admission requires `(now − last_spawn_time) ≥ wave_spawn_interval_s`. Given `now − last_spawn_time = wave_spawn_interval_s − ε`: no pattern admitted. Given `now − last_spawn_time = wave_spawn_interval_s`: pattern admitted. Primer bypass (Rule 2a): on first draw after `Cold→Active`, the cadence gate is NOT checked; `last_spawn_time` is set to `now` immediately after primer draw. On the tick following primer: cadence gate applies normally.

- [x] **AC-WS-11b (BLOCKING — G.1 Tuning Knob)**: `WaveSpawnIntervalS` is NOT initialised to a hardcoded float literal in the admission gate logic. Its runtime value is loaded from a project config or data asset at `Initialize()` (not inlined at point of comparison). The field declaration may carry a fallback default (`2.5f`) only as a C++ member initializer. A `Grep` of `WaveSpawnerSubsystem.cpp` for the gate comparison path must find `WaveSpawnIntervalS` (the field) used for comparison, NOT a bare `2.5f` literal. Verified by `WaveSpawnerAdmissionGateTest`: injecting a non-default interval value via test seam and asserting the cadence gate uses the injected value, not `2.5f`.

---

## Implementation Notes

*Derived from ADR-0011 D2 Implementation Guidelines:*

**Per-tick admission pipeline (tick body, Stage 1 + cadence):**
```cpp
void UWaveSpawnerSubsystem::Tick(float DeltaTime)
{
    // Stage 1 — Rule 1 gate: DPC snapshot once, immutable for this tick
    const FDPCFrameState FrameState = DPCSubsystem->GetCurrentFrameState();
    if (!FrameState.bIsActive)  return;
    if (RSMSubsystem->IsPaused()) return;
    if (IsInResumeGrace())        return;

    // Primer bypass: first draw on Cold→Active skips cadence
    if (bPrimerPending)
    {
        DrawAndAdmit(FrameState);   // primer draw (Stage 4 — Story 005)
        last_spawn_time = GetWorld()->GetTimeSeconds();
        bPrimerPending = false;
        return;  // one admission per tick; exit after primer
    }

    // Stage 3 — F-3b cadence gate
    const float Now = GetWorld()->GetTimeSeconds();
    if ((Now - LastSpawnTimeS) < WaveSpawnIntervalS) return;

    // Stage 2 + 4 — concurrency cap (Story 004) + pattern draw (Story 005)
    TryAdmitPattern(FrameState, Now);
}
```

**Key fields on the subsystem:**
```cpp
float LastSpawnTimeS = 0.f;            // set post-primer and post-admission
float WaveSpawnIntervalS = 2.5f;       // data-driven (G.1 tuning knob — load from config)
bool  bPrimerPending = false;          // set true on Cold→Active; cleared after first draw
bool  bInResumeGrace = false;          // set true on RSM resume; cleared after RESUME_GRACE_S
float ResumeGraceEndTimeS = 0.f;
```

**DPC snapshot immutability contract:**
- `GetCurrentFrameState()` is called ONCE at the top of Tick.
- The returned `FDPCFrameState` struct is captured as a local const value copy.
- No re-reads of `DPCSubsystem` state later in the same tick.
- `telegraph_window_s` from `FrameState` is used by Stage 4 (Story 005) from this same const copy.

**Primer bypass (Rule 2a) — Cold→Active hook in lifecycle (Story 002):**
```cpp
// In OnLifecycleTransition(Active) when coming from Cold:
bPrimerPending = true;
```

**Resume grace (RSM OnPausedChanged(false) — Story 007):**
```cpp
void UWaveSpawnerSubsystem::OnResumeFromPause()
{
    const float Now = GetWorld()->GetTimeSeconds();
    ResumeGraceEndTimeS = Now + RESUME_GRACE_S;
    bInResumeGrace = true;
}

bool UWaveSpawnerSubsystem::IsInResumeGrace() const
{
    if (!bInResumeGrace) return false;
    return GetWorld()->GetTimeSeconds() < ResumeGraceEndTimeS;
}
```

**Dependency injection seams:**
- `DPCSubsystem` and `RSMSubsystem` pointers resolved at `Initialize()` via `Collection.InitializeDependency`.
- Test stubs (`FDPCTestStub`, `FRSMTestStub`) implement the production interfaces; injected via `SetDPCOverride` / `SetRSMOverride` methods on the subsystem (compile-gated `#if !UE_BUILD_SHIPPING`).

**`WaveSpawnIntervalS` must NOT be hardcoded.** Read from project config or data asset at `Initialize()` (G.1 tuning knob, ADR-0011 §Tuning Knobs). Default: `2.5f`.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 002: Lifecycle state machine — `IsTickable()` gating on Cold/Idle.
- Story 004: Rule 7 concurrency cap (Stage 2 admission check — slot pre-commitment).
- Story 005: Stage 4 pattern draw and F-3 cadence governor.
- Story 007: DPC `OnPostTickFrameStatePublished` subscription + RSM `OnPausedChanged` delegate wiring.

---

## QA Test Cases

*Test cases not yet defined — run /qa-plan to generate them.*

---

## Test Evidence

**Story Type**: Logic
**Required evidence**: `Source/SLIPSTORM/Tests/Unit/WaveSpawner/WaveSpawnerAdmissionGateTest.cpp` — must exist and pass

**Status**: [x] Created — `Source/SLIPSTORM/Tests/Unit/WaveSpawner/WaveSpawnerAdmissionGateTest.cpp` (8 test commands: Gate.DPCInactiveVeto, Gate.LifecycleNotActiveVeto, Gate.ResumeGraceVeto, Gate.CadenceNotElapsed, Gate.CadenceElapsed, Gate.PrimerBypass, Gate.PostPrimerCadenceNormal, Gate.IntervalFromDPCSnapshot)

---

## Dependencies

- Depends on: Story 001 (subsystem class skeleton), Story 002 (lifecycle state machine provides `bPrimerPending` hook on Cold→Active)
- Unlocks: Story 004 (barrage admission builds on top of this gate), Story 005 (pattern draw is Stage 4 of the same pipeline)

---

## Completion Notes

**Completed**: 2026-08-20
**Criteria**: 3/3 passing (AC-WS-10 ✅ AC-WS-11 ✅ AC-WS-11b ✅)
**Deviations**:
- ADVISORY (D1): AC-WS-11b specifies `WaveSpawnIntervalS` "loaded from config at `Initialize()`"; implementation reads `FrameState.WaveSpawnIntervalS` from DPC snapshot — correct per ADR-0011 D2. Grep criterion met (no bare `2.5f` at gate comparison). AC text to be reconciled.
- ADVISORY (method length): `OnDPCFrameReady` is 91 lines (standard: 40). Refactor to per-stage private helpers deferred to Story 007 when pipeline gains Stage 2+4 bodies.
- ADVISORY (pre-existing test outer): `NewObject<UWaveSpawnerSubsystem>(GetTransientPackage())` violates `Within = GameInstance`. Inherited from Stories 001+002. Works in editor CI (`GEnsuresAreErrors` ensure fires once per session); game-target CI would require `AddExpectedError` wrapper or stub-outer story.
**Test Evidence**: Logic — `Source/SLIPSTORM/Tests/Unit/WaveSpawner/WaveSpawnerAdmissionGateTest.cpp` (8 commands, all ACs covered)
**Code Review**: Complete — APPROVED WITH SUGGESTIONS (advisory only; no required changes)

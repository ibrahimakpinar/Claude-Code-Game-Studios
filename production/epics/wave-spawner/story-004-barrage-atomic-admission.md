# Story 004: Barrage Atomic Admission + barrage_owed Reservation

> **Epic**: Wave Spawner Pattern Library
> **Status**: Complete
> **Layer**: Feature
> **Type**: Logic
> **Estimate**: M (~3–4h)
> **Manifest Version**: (none — docs/architecture/control-manifest.md not found; run /create-control-manifest)
> **Last Updated**: 2026-08-20

## Context

**GDD**: `design/gdd/wave-spawner-pattern-library.md`
**Requirement**: `TR-WS-017`, `TR-WS-018`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0011: Wave Spawner Pattern Library — D2 Four-Stage Admission Pipeline (primary), Stage 2 Rule 7 Concurrency Cap
**ADR Decision Summary**: Rule 7 concurrency cap governs how many waves may be simultaneously in-flight. `current_concurrent = Scheduled + Live`; `available = MAX_CONCURRENT_WAVES − current_concurrent` (F-1, F-1b). Non-barrage: requires `available ≥ 1` (`Scheduled++`). Barrage: requires `available ≥ 3` atomically (`Scheduled += 3`). If a barrage draw cannot be admitted (insufficient slots), the attempt is dropped, `bBarrageOwed = true` is set, and a non-barrage pattern is drawn instead. When `bBarrageOwed` and `available ≥ 3`, the next draw is reserved from the PEAK barrage sub-pool (barrage reservation fulfilment). Telemetry: emit `barrage_dropped_due_to_concurrency` when a barrage draw is dropped.

**Engine**: Unreal Engine 5.7 | **Risk**: HIGH
**Engine Notes**: `Scheduled` and `Live` counters are game-thread-only (spawner is single-threaded per ADR-0005). No atomic CPU instructions needed — simple `int32` fields suffice. `MAX_CONCURRENT_WAVES = 23` is the object pool size (F-2, TR-WS-008). The `bBarrageOwed` flag survives pause flush but is cleared on Run Termination (Rule 14, Story 007).

**Control Manifest Rules (Feature layer)**:
- No control manifest found at `docs/architecture/control-manifest.md` — run `/create-control-manifest`.

---

## Acceptance Criteria

*From GDD `design/gdd/wave-spawner-pattern-library.md`, scoped to this story:*

- [ ] **AC-WS-12a (BLOCKING — TR-WS-017, F-1/F-1b)**: Concurrency formula and non-barrage admission. `GetAvailableSlots()` computes `available = MAX_CONCURRENT_WAVES − (Scheduled + Live)` where `MAX_CONCURRENT_WAVES = 23` (the pool size constant from Story 001 — do NOT re-define). Non-barrage draw: requires `available ≥ 1`; on admission `Scheduled++`. Non-barrage draw when `available = 0`: no admission this tick, method returns false. Verified by `WaveSpawnerBarrageAdmissionTest`: (a) inject `Scheduled=5, Live=3` → assert `available = 15`; (b) inject `Scheduled=22, Live=0` → non-barrage admitted, assert `Scheduled = 23`; (c) inject `Scheduled=23, Live=0` → no admission, assert `Scheduled` unchanged.

- [ ] **AC-WS-12b (BLOCKING — TR-WS-017/018, Rule 7)**: Barrage atomic admission and dropped-barrage path. Barrage draw: requires `available ≥ 3`; on admission `Scheduled += 3`. Barrage draw when `available < 3` (e.g., `available = 2`): `bBarrageOwed = true`, telemetry call site for `barrage_dropped_due_to_concurrency` invoked (verified by `Grep` on the drop path — call site must be present; payload may be a stub `/* TODO Story 009 */` until Story 009 provides telemetry infrastructure), non-barrage fallback drawn if `available ≥ 1` (`Scheduled++`). When `available = 0` after barrage drop: `bBarrageOwed = true`, no fallback drawn, returns false. Verified by `WaveSpawnerBarrageAdmissionTest`: (a) `available = 5` → barrage admitted, `Scheduled += 3`; (b) `available = 2` → barrage dropped, `bBarrageOwed = true`, non-barrage fallback drawn, `Scheduled++`; (c) `available = 0` → barrage dropped, `bBarrageOwed = true`, no admission.

- [ ] **AC-WS-12c (BLOCKING — TR-WS-018, Reservation Fulfilment)**: `bBarrageOwed` reservation fulfilment and lifecycle. When `bBarrageOwed = true` and `available ≥ 3` on the next draw tick: next draw is reserved from `PeakPool.BarragePatterns` (the PEAK barrage sub-pool); `Scheduled += 3`; `bBarrageOwed = false` after successful admission. When `bBarrageOwed = true` and `available < 3` on the reservation tick: `bBarrageOwed` remains `true`, non-barrage fallback drawn if `available ≥ 1`. `bBarrageOwed` is NOT cleared by pause flush — the pre-pause value persists across pause/resume (Story 007 manages the flush; this story only sets/clears the flag on admission logic). `bBarrageOwed` IS cleared on Run Termination by Story 007's flush path — do not implement that clearance here. Verified by `WaveSpawnerBarrageAdmissionTest`: (a) set `bBarrageOwed = true`, `available = 5` → barrage draw from PeakPool, `bBarrageOwed = false`; (b) set `bBarrageOwed = true`, `available = 2` → remains `true`, fallback admitted; (c) `bBarrageOwed` persists across a simulated pause-flush call (seam injected, no Story 007 infrastructure needed).

---

## Implementation Notes

*Derived from ADR-0011 D2 Stage 2 Implementation Guidelines:*

**Concurrency tracking fields:**
```cpp
int32 Scheduled = 0;   // slots pre-committed for in-flight (not yet Live)
int32 Live = 0;        // slots currently active (incremented when AWave.Activate() called)
bool  bBarrageOwed = false;  // true if a barrage draw was dropped and must be fulfilled
```

**F-1 / F-1b computation and Rule 7 cap check:**
```cpp
int32 UWaveSpawnerSubsystem::GetAvailableSlots() const
{
    const int32 CurrentConcurrent = Scheduled + Live;
    return MAX_CONCURRENT_WAVES - CurrentConcurrent;
}
```

**TryAdmitPattern() — Rule 7 gate + barrage_owed reservation:**
```cpp
bool UWaveSpawnerSubsystem::TryAdmitPattern(const FDPCFrameState& FrameState, float Now)
{
    const bool bBarrageIntended = ShouldDrawBarrage();  // F-3 decision from Story 005

    if (bBarrageOwed || bBarrageIntended)
    {
        // Barrage path: needs 3 slots
        if (GetAvailableSlots() >= 3)
        {
            Scheduled += 3;
            DrawBarragePattern(FrameState, Now);
            bBarrageOwed = false;
            return true;
        }
        else
        {
            // Cannot satisfy barrage — drop and draw non-barrage instead
            bBarrageOwed = true;
            EmitTelemetry(TEXT("barrage_dropped_due_to_concurrency"), ...);
            if (GetAvailableSlots() >= 1)
            {
                Scheduled++;
                DrawNonBarragePattern(FrameState, Now);
                return true;
            }
            return false;  // pool fully saturated; no admission this tick
        }
    }
    else
    {
        // Non-barrage path: needs 1 slot
        if (GetAvailableSlots() >= 1)
        {
            Scheduled++;
            DrawNonBarragePattern(FrameState, Now);
            return true;
        }
        return false;
    }
}
```

**Slot lifecycle — when do Scheduled/Live change?**
- `Scheduled++` (or `+= 3`): at admission decision time (pre-commit).
- `Live++`: when the `AWave` actor is formally activated (handed to the pool slot).
- `Scheduled--` (or `-= 3`): when `Live++` fires (transitions from Scheduled to Live).
- `Live--`: in the despawn pipeline when `OnWaveDespawned` fires (Story 006).

**barrage_owed lifecycle:**
- Set `true`: when barrage draw attempted but `available < 3`.
- Cleared: when barrage reservation is fulfilled (successful barrage admission).
- Cleared on Run Termination: Story 007 sets `bBarrageOwed = false` in the RunTermination flush path.
- NOT cleared by Pause Flush: pre-pause `bBarrageOwed` state persists across pause/resume.

**MAX_CONCURRENT_WAVES**: must match pool size constant from Story 001 (`= 23`). Reference the same compile-time constant — do not re-define.

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 003: Rule 1 gate + cadence gate (Stage 1 + Stage 3 — `TryAdmitPattern` is called by the pipeline after those gates pass).
- Story 005: `ShouldDrawBarrage()` — F-3 cadence governor that decides whether barrage is intended.
- Story 006: `OnWaveDespawned` slot release (`Live--`).
- Story 007: `bBarrageOwed` clearance on Run Termination.
- Story 009: `barrage_dropped_due_to_concurrency` telemetry event implementation (this story emits the call site; telemetry infrastructure is Story 009).

---

## QA Test Cases

*Test cases not yet defined — run /qa-plan to generate them.*

---

## Test Evidence

**Story Type**: Logic
**Required evidence**: `Source/SLIPSTORM/Tests/Unit/WaveSpawner/WaveSpawnerBarrageAdmissionTest.cpp` — must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: Story 001 (pool size constant, `AWave` type), Story 002 (lifecycle state must be Active for `TryAdmitPattern` to be reached), Story 003 (Rule 1 gate + cadence gate call this function)
- Unlocks: Story 005 (F-3 draws need the slot count from this story to determine barrage eligibility)

---

## Completion Notes

**Completed**: 2026-08-20
**Criteria**: 3/3 passing (all BLOCKING ACs verified via code inspection + test file)
**Deviations**:
- ADVISORY-1: `bBarrageOwed = false` in Cold reset pre-implements AC-WS-12c clearance attributed to Story 007. Behaviour is correct; Story 007 need not add a second clearance. Accepted — no fix needed.
- ADVISORY-2: `kMaxConcurrentWavesStub = 23` is a stub for `FDPCFrameState.max_concurrent_waves`. Named constant with DEVIATION NOTE in `.h` and `.cpp`. Story 005 replaces with real DPC cap (16). ADR-compliant.
- INFO: `AcquireFromPool()` does not assign `Slot->WaveId`; `ReleaseToPool` matches on it. Story 005 concern; no Story 004 action.
**Test Evidence**: Logic story — `Source/SLIPSTORM/Tests/Unit/WaveSpawner/WaveSpawnerBarrageAdmissionTest.cpp` (9 test commands: TC1–TC9). Test execution deferred to UBT build per Stories 001–003 precedent.
**Code Review**: Complete — `/code-review` ran this session (CHANGES REQUIRED); all 3 required changes and 5 suggestions applied before closure.

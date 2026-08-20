# Story 006: Despawn Pipeline + IWaveSpawnerCallback + Seam 13

> **Epic**: Wave Spawner Pattern Library
> **Status**: Ready
> **Layer**: Feature
> **Type**: Integration
> **Estimate**: (fill before sprint planning)
> **Manifest Version**: (none — docs/architecture/control-manifest.md not found; run /create-control-manifest)
> **Last Updated**: 2026-08-19

## Context

**GDD**: `design/gdd/wave-spawner-pattern-library.md`
**Requirement**: `TR-WS-026`
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0011: Wave Spawner Pattern Library — D3 Six-State Lifecycle (primary), Seam 13 despawn interface
**ADR Decision Summary**: Rule 12 defines the ordered despawn pipeline: `OnCollisionUnregistered → OnTelegraphUnregistered → OnWaveDespawned`. This ordering is mandatory regardless of despawn reason (`NaturalLanding`, `RunTermination`, `PauseFlush`). The production interface `IWaveSpawnerCallback` exposes all three in the correct order. Seam 13 provides `FWaveSpawnerCallbackTestStub`, a testable implementation with an internal event log and a re-entrant `TFunction` slot (`SetOnDespawnedUserCallback`) that fires AFTER `OnWaveDespawned` completes. Critically, the production `IWaveSpawnerCallback` MUST NOT expose this re-entrant slot — it is test-infrastructure only, guarded by `#if !UE_BUILD_SHIPPING`.

**Engine**: Unreal Engine 5.7 | **Risk**: HIGH
**Engine Notes**: `IWaveSpawnerCallback` is a pure virtual UInterface or plain C++ interface (prefer plain C++ — UInterface inheritance adds overhead not needed here). `FWaveSpawnerCallbackTestStub` is NOT a UObject; it holds a `TArray<FDespawnEvent>` event log (plain struct array). `TFunction<void(int32, EWaveDespawnReason)>` for the re-entrant slot — UE 5.x `TFunction` is stable. `EWaveDespawnReason` must be a `UENUM()` for Blueprint exposability if needed; otherwise a plain enum class. Verify `#if !UE_BUILD_SHIPPING` guard compiles correctly in UE 5.7 editor/shipping configurations.

**Control Manifest Rules (Feature layer)**:
- No control manifest found at `docs/architecture/control-manifest.md` — run `/create-control-manifest`.

---

## Acceptance Criteria

*From GDD `design/gdd/wave-spawner-pattern-library.md`, scoped to this story:*

- [ ] **AC-WS-16 (BLOCKING)**: Rule 12 despawn pipeline fires in exact order: `OnCollisionUnregistered(WaveId)` → `OnTelegraphUnregistered(WaveId)` → `OnWaveDespawned(WaveId, Reason)`. This ordering holds for all three `EWaveDespawnReason` values: `NaturalLanding`, `RunTermination`, `PauseFlush`. Verified via `FWaveSpawnerCallbackTestStub.EventLog()` which records insertion order.

- [ ] **AC-WS-19 (BLOCKING)**: Seam 13 contract. `FWaveSpawnerCallbackTestStub.SetOnDespawnedUserCallback(TFunction)` fires AFTER `OnWaveDespawned` is recorded in the event log (re-entrant: the callback runs within the `OnWaveDespawned` call, after the standard recording step). Production `IWaveSpawnerCallback` does NOT have a `SetOnDespawnedUserCallback` method — `static_assert` or compile-time check prevents accidental exposure. Test stub is only available in non-Shipping builds (`#if !UE_BUILD_SHIPPING`).

---

## Implementation Notes

*Derived from ADR-0011 D3 + Seam 13 Implementation Guidelines:*

**EWaveDespawnReason:**
```cpp
UENUM(BlueprintType)
enum class EWaveDespawnReason : uint8
{
    NaturalLanding,    // Wave completed its normal lifecycle
    RunTermination,    // RSM moved to DEAD/COMPLETE/ABORTED
    PauseFlush         // RSM pause event flushed in-flight waves
};
```

**IWaveSpawnerCallback (production interface — pure C++):**
```cpp
class IWaveSpawnerCallback
{
public:
    virtual ~IWaveSpawnerCallback() = default;
    virtual void OnCollisionUnregistered(int32 WaveId) = 0;
    virtual void OnTelegraphUnregistered(int32 WaveId) = 0;
    virtual void OnWaveDespawned(int32 WaveId, EWaveDespawnReason Reason) = 0;
    // NOTE: SetOnDespawnedUserCallback is intentionally NOT here (Seam 13 contract)
};
```

**Rule 12 despawn pipeline (spawner-side, called per-wave):**
```cpp
void UWaveSpawnerSubsystem::DespawnWave(int32 WaveId, EWaveDespawnReason Reason)
{
    // Ordered pipeline — mandatory sequence regardless of Reason
    if (Callback) Callback->OnCollisionUnregistered(WaveId);
    if (Callback) Callback->OnTelegraphUnregistered(WaveId);

    // Release slot: Live-- (from Story 004)
    Live = FMath::Max(0, Live - GetSlotsForWave(WaveId));

    if (Callback) Callback->OnWaveDespawned(WaveId, Reason);

    // Return AWave to pool (from Story 001 pool acquire/release)
    ReleaseWaveToPool(WaveId);
}
```

**FWaveSpawnerCallbackTestStub (test seam — non-Shipping only):**
```cpp
#if !UE_BUILD_SHIPPING

struct FDespawnEvent
{
    int32 WaveId;
    FName EventName;  // "CollisionUnregistered", "TelegraphUnregistered", "WaveDespawned"
    EWaveDespawnReason Reason;  // only populated for WaveDespawned
};

class FWaveSpawnerCallbackTestStub : public IWaveSpawnerCallback
{
public:
    // IWaveSpawnerCallback implementation
    virtual void OnCollisionUnregistered(int32 WaveId) override
    {
        EventLog.Add({ WaveId, TEXT("CollisionUnregistered"), {} });
    }
    virtual void OnTelegraphUnregistered(int32 WaveId) override
    {
        EventLog.Add({ WaveId, TEXT("TelegraphUnregistered"), {} });
    }
    virtual void OnWaveDespawned(int32 WaveId, EWaveDespawnReason Reason) override
    {
        EventLog.Add({ WaveId, TEXT("WaveDespawned"), Reason });
        // Re-entrant user callback fires AFTER event is recorded
        if (OnDespawnedUserCallback) OnDespawnedUserCallback(WaveId, Reason);
    }

    // Seam 13 re-entrant slot — NOT on IWaveSpawnerCallback
    void SetOnDespawnedUserCallback(TFunction<void(int32, EWaveDespawnReason)> Fn)
    {
        OnDespawnedUserCallback = MoveTemp(Fn);
    }

    const TArray<FDespawnEvent>& GetEventLog() const { return EventLog; }
    void ResetLog() { EventLog.Reset(); }

private:
    TArray<FDespawnEvent> EventLog;
    TFunction<void(int32, EWaveDespawnReason)> OnDespawnedUserCallback;
};

// Static compile-time verification: production interface does NOT have the re-entrant slot
static_assert(!std::is_member_function_pointer_v<
    decltype(&IWaveSpawnerCallback::SetOnDespawnedUserCallback)>,
    "Production interface must NOT expose SetOnDespawnedUserCallback");
// Note: the static_assert above will produce a compile error if the production
// interface ever accidentally gains SetOnDespawnedUserCallback.

#endif  // !UE_BUILD_SHIPPING
```

**File placement:**
- `Source/SLIPSTORM/Seam/` (already created, per git status): `WaveSpawnerCallback.h` (production interface + enum), `WaveSpawnerCallbackTestStub.h` (test seam, `#if !UE_BUILD_SHIPPING`)
- `Source/SLIPSTORM/WaveSpawner/WaveSpawnerSubsystem.h/cpp`: `DespawnWave()` implementation

---

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 004: `Live--` counter release (referenced here for slot accounting but owned by Story 004 which defines the counter).
- Story 007: Pause Flush and Run Termination — they call `DespawnWave()` with the appropriate reason; the pipeline here is the mechanism they use.

---

## QA Test Cases

*Test cases not yet defined — run /qa-plan to generate them.*

---

## Test Evidence

**Story Type**: Integration
**Required evidence**: `Source/SLIPSTORM/Tests/Integration/WaveSpawner/WaveSpawnerDespawnPipelineTest.cpp` — must exist and pass

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: Story 001 (pool release), Story 002 (lifecycle state informs which despawn reason applies), Story 004 (Live counter released during despawn)
- Unlocks: Story 007 (pause flush and run termination call `DespawnWave()` with `PauseFlush` / `RunTermination` reason), Story 009 (telemetry emitted during despawn pipeline)

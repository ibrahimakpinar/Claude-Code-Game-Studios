// Copyright (c) Game Studio. All rights reserved.
// SLIPSTORM — 60-second voxel endless runner, mobile, Unreal Engine 5.7
//
// WaveSpawnerCallbackTestStub.h — Seam 13: test stub for IWaveSpawnerCallback.
//
// NON-SHIPPING ONLY: entire file wrapped in #if !UE_BUILD_SHIPPING.
//
// Provides FWaveSpawnerCallbackTestStub, a plain C++ implementation of
// IWaveSpawnerCallback that records all three Rule 12 pipeline events in an
// ordered TArray EventLog for white-box assertion.
//
// Used by integration tests to verify:
//   AC-WS-16: pipeline fires in exact insertion order for all three reasons
//   AC-WS-19: SetOnDespawnedUserCallback fires AFTER OnWaveDespawned is logged
//   AC-WS-20: Live slot (LiveSlots.Num()) decremented BETWEEN OnTelegraphUnregistered
//             and OnWaveDespawned — confirmed by reading TestOnly_GetLiveCount()
//             inside the re-entrant SetOnDespawnedUserCallback hook.
//
// Seam 13 invariant: SetOnDespawnedUserCallback is NOT on IWaveSpawnerCallback.
// A C++20 requires-expression static_assert at file end enforces this at compile time.
// C++20 enabled via CppStandard = CppStandardVersion.Cpp20 in SLIPSTORM.Build.cs.
//
// Not a UObject — no UCLASS/USTRUCT macros.
//
// ADR: docs/architecture/adr-0011-wave-spawner-pattern-library.md (D3 Seam 13)
// Story: production/epics/wave-spawner/story-006-despawn-pipeline-and-seam-13.md
// TR:    TR-WS-026

#pragma once

#include "CoreMinimal.h"
#include "Seam/WaveSpawnerCallback.h"

#if !UE_BUILD_SHIPPING

/**
 * FDespawnEvent
 *
 * Single recorded entry in FWaveSpawnerCallbackTestStub::EventLog.
 * EventName identifies the pipeline step; Reason is EWaveDespawnReason::None for
 * CollisionUnregistered and TelegraphUnregistered entries (only WaveDespawned carries
 * a meaningful Reason value).
 */
struct FDespawnEvent
{
    int32              WaveId    = 0;
    FName              EventName = NAME_None;
    EWaveDespawnReason Reason    = EWaveDespawnReason::None;
};

/**
 * FWaveSpawnerCallbackTestStub
 *
 * Test implementation of IWaveSpawnerCallback. Records all three pipeline steps
 * in insertion order for white-box assertion by integration tests.
 *
 * Re-entrant hook (AC-WS-19): OnWaveDespawned logs to EventLog FIRST, then calls
 * OnDespawnedUserCallback if set. The re-entrant callback therefore runs after the
 * WaveDespawned event is already in the log — verifiable by reading EventLog.Num()
 * inside the callback.
 *
 * Not a UObject. No UHT macros. Non-Shipping only.
 */
class FWaveSpawnerCallbackTestStub : public IWaveSpawnerCallback
{
public:

    // -------------------------------------------------------------------------
    // IWaveSpawnerCallback implementation
    // -------------------------------------------------------------------------

    virtual void OnCollisionUnregistered(int32 WaveId) override
    {
        EventLog.Add({ WaveId, FName(TEXT("CollisionUnregistered")), EWaveDespawnReason::None });
    }

    virtual void OnTelegraphUnregistered(int32 WaveId) override
    {
        EventLog.Add({ WaveId, FName(TEXT("TelegraphUnregistered")), EWaveDespawnReason::None });
    }

    virtual void OnWaveDespawned(int32 WaveId, EWaveDespawnReason Reason) override
    {
        // Log FIRST, then fire the re-entrant callback (AC-WS-19 ordering contract).
        // When OnDespawnedUserCallback fires, EventLog already contains this entry.
        EventLog.Add({ WaveId, FName(TEXT("WaveDespawned")), Reason });
        if (OnDespawnedUserCallback)
        {
            OnDespawnedUserCallback(WaveId, Reason);
        }
    }

    // -------------------------------------------------------------------------
    // Seam 13 re-entrant slot — intentionally NOT on IWaveSpawnerCallback.
    // -------------------------------------------------------------------------

    /**
     * Installs a re-entrant callback that fires inside OnWaveDespawned, AFTER the
     * WaveDespawned event is appended to EventLog (AC-WS-19).
     *
     * Used by AC-WS-19 to verify the log entry exists at callback time.
     * Used by AC-WS-20 to read TestOnly_GetLiveCount() after slot release.
     *
     * This method MUST NOT be exposed on IWaveSpawnerCallback. See static_assert below.
     */
    void SetOnDespawnedUserCallback(TFunction<void(int32, EWaveDespawnReason)> Fn)
    {
        OnDespawnedUserCallback = MoveTemp(Fn);
    }

    // -------------------------------------------------------------------------
    // Accessors
    // -------------------------------------------------------------------------

    /** Returns all recorded pipeline events in insertion order. */
    const TArray<FDespawnEvent>& GetEventLog() const { return EventLog; }

    /** Clears the event log. Call between test commands to reset state. */
    void ResetLog() { EventLog.Reset(); }

private:
    TArray<FDespawnEvent>                      EventLog;
    TFunction<void(int32, EWaveDespawnReason)> OnDespawnedUserCallback;
};

// ---------------------------------------------------------------------------
// Compile-time Seam 13 invariant guard (C++20 requires-expression).
//
// If SetOnDespawnedUserCallback is accidentally added to the production
// IWaveSpawnerCallback interface, this static_assert fires with a clear
// diagnostic at compile time. The requires-expression evaluates to false
// (not a hard compile error) when the method is absent — exactly the behavior
// we need. The parameter-list form is used (UE-idiomatic: no std::declval).
//
// C++20 enabled via CppStandard = CppStandardVersion.Cpp20 (SLIPSTORM.Build.cs).
// This guard is inside #if !UE_BUILD_SHIPPING alongside the stub; it is never
// evaluated in Shipping builds.
// ---------------------------------------------------------------------------
static_assert(
    !requires(IWaveSpawnerCallback& c, TFunction<void(int32, EWaveDespawnReason)> fn) {
        c.SetOnDespawnedUserCallback(fn);
    },
    "IWaveSpawnerCallback must NOT expose SetOnDespawnedUserCallback "
    "(Seam 13 contract: re-entrant slot is test-infrastructure only)."
);

#endif  // !UE_BUILD_SHIPPING

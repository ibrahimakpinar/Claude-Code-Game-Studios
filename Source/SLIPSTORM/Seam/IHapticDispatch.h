// Copyright (c) Game Studio. All rights reserved.
// SLIPSTORM — 60-second voxel endless runner, mobile, Unreal Engine 5.7
//
// IHapticDispatch.h — Project-owned haptic seam interface + EHapticEvent enum.
//
// This is the MINIMAL declaration required by Story 005 (input buffer + Rule 3
// buffer-drop feedback).  The platform-side implementation (iOS Core Haptics +
// Android VibrationEffect backends) is deferred to ADR-0002's polish-phase story.
//
// Governing ADR: docs/architecture/adr-0002-haptic-platform-bridge.md
//   INT-002-amended (2026-06-26) defines the IHapticDispatch::Fire(EHapticEvent)
//   + IsSystemHapticsEnabled() contract consumed here.
// Story: production/epics/player-movement/story-005-input-buffer.md
//
// Null default implementation:
//   IsSystemHapticsEnabled() returns false — haptics silently disabled on
//   desktop/CI without waiting for the ADR-0002 platform bridge.
//   Fire(EHapticEvent) is a no-op in the null default.
//
// Test-only spy mechanism (WITH_DEV_AUTOMATION_TESTS only):
//   Static function-pointer swapping lets tests install a spy and restore the
//   null default in teardown.  See IHapticDispatch.cpp for the swap API.
//   Call sites (PM code) remain clean — IHapticDispatch::Fire(EHapticEvent::BufferDrop)
//   always calls the currently-installed backend (spy or null).

#pragma once

#include "CoreMinimal.h"
#include "IHapticDispatch.generated.h"

// ---------------------------------------------------------------------------
// EHapticEvent — haptic event vocabulary.
//
// Ordinals are STABLE — do not reorder without updating the ADR-0002 platform
// pattern tables (iOS / Android tier maps).
//
// Values added by Story 005 (BufferDrop only — the enum's first consumer):
//   BufferDrop = 0 — PM-owned: input buffer full on second slip (Rule 3 drop).
//
// Future values (comment placeholders to hold ordinal slots):
//   NearMiss     — Story 012 (near-miss-beat); ordinal 1 reserved.
//   SlipConfirmed — out-of-epic slip-confirmation story; ordinal 2 reserved.
//
// ADR-0002 INT-002-amended also defines IS-owned values (R3Collision,
// DeadBandContact, InputRejected, ContactResting) that land in the IS epic.
// ---------------------------------------------------------------------------

UENUM()
enum class EHapticEvent : uint8
{
    // Explicit ordinal (= 0) makes the "ordinals are STABLE" contract
    // compiler-visible — a future inserted enum value cannot silently shift
    // BufferDrop without triggering an editor/reflection-side diff.
    BufferDrop = 0 UMETA(DisplayName = "Buffer Drop"),
    // NearMiss     — added by Story 012 (near-miss-beat); reserved ordinal 1
    // SlipConfirmed — out-of-epic slip-confirmation story; reserved ordinal 2
};

// ---------------------------------------------------------------------------
// IHapticDispatch — shared haptic dispatch interface (ADR-0002 INT-002-amended).
//
// Static-method interface with a runtime-swappable backend.  This avoids
// passing an IHapticDispatch* through every gameplay call site while still
// allowing tests to swap in a spy.
//
// Thread safety: all calls MUST occur on the game thread (ADR-0002 requirement
// propagated from iOS/Android haptic APIs).
// ---------------------------------------------------------------------------

class IHapticDispatch
{
public:
    /** Returns true when system haptics are enabled and calibrated.
     *  In the null default implementation returns false — haptic dispatch
     *  calls are silent on desktop/CI builds without the platform bridge.
     *  PM callers gate IHapticDispatch::Fire() on this method per AC-25. */
    static bool IsSystemHapticsEnabled();

    /** Fire the haptic event via the currently-installed backend.
     *  No-op in the null default.  Platform backend dispatches the
     *  tier-appropriate haptic pattern per ADR-0002 platform pattern tables.
     *  Must be called on the game thread. */
    static void Fire(EHapticEvent Event);

#if WITH_DEV_AUTOMATION_TESTS
    // -----------------------------------------------------------------------
    // Test-only swap API — function-pointer indirection.
    //
    // Install a spy before a test; call TestOnly_Reset() in teardown (or use
    // ON_SCOPE_EXIT).  The spy records dispatched events + lets the test assert
    // on exact contents + call count without touching production code paths.
    //
    // Preferred teardown idiom:
    //   ON_SCOPE_EXIT { IHapticDispatch::TestOnly_Reset(); };
    // -----------------------------------------------------------------------

    /** Install a custom Fire backend for the duration of a test.
     *  Pass nullptr to restore the null default. */
    static void TestOnly_SetFireFn(void(*InFn)(EHapticEvent));

    /** Install a custom IsSystemHapticsEnabled backend for the duration of a test.
     *  Pass nullptr to restore the null default (returns false). */
    static void TestOnly_SetIsEnabledFn(bool(*)());

    /** Restore both function pointers to the null default. */
    static void TestOnly_Reset();
#endif // WITH_DEV_AUTOMATION_TESTS
};

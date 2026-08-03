// Copyright (c) Game Studio. All rights reserved.
// SLIPSTORM — 60-second voxel endless runner, mobile, Unreal Engine 5.7
//
// IGameSettings.h — Project-owned game-settings seam interface.
//
// Minimal accessor surface for accessibility settings consumed by PM/gameplay
// dispatch guards. The real settings UI + persistence layer lives in a future
// Accessibility Settings GDD (unauthored as of Story 012). This seam allows
// gameplay code to consume settings values today against a stable interface,
// with the persistence layer swapped in at that later story.
//
// Governing forward contract: Accessibility Settings GDD (unauthored).
// Consumer: production/epics/player-movement/story-012-near-miss-beat.md
//   TriggerNearMissBeat AND-gates haptic dispatch on IsNearMissHapticEnabled().
//
// Null default implementation:
//   IsNearMissHapticEnabled() returns false — matches R11a-12 accessibility
//   opt-in default (haptic is OFF unless the player explicitly enables it).
//   Behaves correctly on desktop/CI without waiting for the settings UI layer.
//
// Test-only spy mechanism (WITH_DEV_AUTOMATION_TESTS only):
//   Static function-pointer swapping lets tests install a spy and restore the
//   null default in teardown. Mirrors IHapticDispatch's pattern.
//
// Thread safety: all calls MUST occur on the game thread.

#pragma once

#include "CoreMinimal.h"

// ---------------------------------------------------------------------------
// IGameSettings — shared accessibility/gameplay settings accessor.
//
// Static-method interface with a runtime-swappable backend. Avoids passing an
// IGameSettings* through every gameplay call site while still allowing tests
// to swap in a spy. Mirrors IHapticDispatch.
// ---------------------------------------------------------------------------

class IGameSettings
{
public:
    /** Returns true when the player has opted-in to near-miss haptic feedback.
     *  Accessibility setting per R11a-12 — default OFF. The player toggles this
     *  in the future Accessibility Settings UI (unauthored). PM's near-miss
     *  haptic dispatch AND-gates on this value alongside IsSystemHapticsEnabled().
     *  Story 012 forward contract. */
    static bool IsNearMissHapticEnabled();

#if WITH_DEV_AUTOMATION_TESTS
    // -----------------------------------------------------------------------
    // Test-only swap API — function-pointer indirection.
    //
    // Install a spy before a test; call TestOnly_Reset() in teardown (or use
    // ON_SCOPE_EXIT). The spy lets the test toggle setting values without
    // touching production code paths.
    //
    // Preferred teardown idiom:
    //   ON_SCOPE_EXIT { IGameSettings::TestOnly_Reset(); };
    // -----------------------------------------------------------------------

    /** Install a custom IsNearMissHapticEnabled backend for the duration of a
     *  test. Pass nullptr to restore the null default (returns false). */
    static void TestOnly_SetIsNearMissHapticEnabledFn(bool(*InFn)());

    /** Restore all function pointers to null defaults. */
    static void TestOnly_Reset();
#endif // WITH_DEV_AUTOMATION_TESTS
};

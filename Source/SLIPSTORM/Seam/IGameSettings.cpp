// Copyright (c) Game Studio. All rights reserved.
// SLIPSTORM — 60-second voxel endless runner, mobile, Unreal Engine 5.7
//
// IGameSettings.cpp — Null default implementation + test-swap machinery.
//
// Consumer: production/epics/player-movement/story-012-near-miss-beat.md
//
// Default backend:
//   IsNearMissHapticEnabled() — returns false (R11a-12 accessibility opt-in
//                                default; player toggles ON in future settings UI).
//
// Production backend:
//   The Accessibility Settings GDD (unauthored) will install a real settings
//   backend at Module::StartupModule() by calling
//   TestOnly_SetIsNearMissHapticEnabledFn (or a dedicated production-install
//   API added at that time — the static function-pointer mechanism is the same
//   pattern either way, mirroring IHapticDispatch).

#include "Seam/IGameSettings.h"

// ---------------------------------------------------------------------------
// Null default implementation.
// ---------------------------------------------------------------------------

static bool NullIsNearMissHapticEnabled()
{
    // Returns false — accessibility opt-in default per R11a-12 (haptic OFF
    // unless the player explicitly enables it in Accessibility Settings).
    return false;
}

// ---------------------------------------------------------------------------
// Static function pointer — initialized to null default.
// ---------------------------------------------------------------------------

static bool(*IsNearMissHapticEnabledFn)() = &NullIsNearMissHapticEnabled;

// ---------------------------------------------------------------------------
// IGameSettings public interface — dispatches through the current backend.
// ---------------------------------------------------------------------------

bool IGameSettings::IsNearMissHapticEnabled()
{
    // Game-thread contract — machine-enforced via check() so future callers on
    // a background thread trip an assertion rather than silently race the
    // function-pointer swap. Mirrors IHapticDispatch's IsSystemHapticsEnabled.
    check(IsInGameThread());
    return IsNearMissHapticEnabledFn();
}

// ---------------------------------------------------------------------------
// Test-only swap API — compiled only in dev/automation builds.
// ---------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

void IGameSettings::TestOnly_SetIsNearMissHapticEnabledFn(bool(*InFn)())
{
    IsNearMissHapticEnabledFn = (InFn != nullptr) ? InFn : &NullIsNearMissHapticEnabled;
}

void IGameSettings::TestOnly_Reset()
{
    IsNearMissHapticEnabledFn = &NullIsNearMissHapticEnabled;
}

#endif // WITH_DEV_AUTOMATION_TESTS

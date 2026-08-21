// Copyright (c) Game Studio. All rights reserved.
// SLIPSTORM — 60-second voxel endless runner, mobile, Unreal Engine 5.7
//
// IHapticDispatch.cpp — Null default implementation + test-swap machinery.
//
// Governing ADR: docs/architecture/adr-0002-haptic-platform-bridge.md
// Story: production/epics/player-movement/story-005-input-buffer.md
//
// Default backend:
//   IsSystemHapticsEnabled() — returns false (no platform bridge installed yet).
//   Fire(EHapticEvent)       — no-op (silently discards in desktop/CI builds).
//
// Platform backend:
//   ADR-0002's polish-phase story will install the iOS / Android backend at
//   Module::StartupModule() by calling TestOnly_SetFireFn / TestOnly_SetIsEnabledFn
//   (or a dedicated production-install API added at that time — the static
//   function-pointer mechanism is the same pattern either way).
//
// Test-spy pattern:
//   Static function pointers (FireFn, IsEnabledFn) hold the current backend.
//   Tests swap them in RunTest, then restore in teardown via TestOnly_Reset().
//   This keeps call sites (PM code) clean — they always call IHapticDispatch::Fire()
//   without knowing whether the spy or the null backend is active.

#include "Seam/IHapticDispatch.h"

// ---------------------------------------------------------------------------
// Null default implementations.
// ---------------------------------------------------------------------------

static void NullFire(EHapticEvent /*Event*/)
{
    // Intentional no-op — null default until platform bridge installed.
}

static bool NullIsEnabled()
{
    // Returns false — haptics silently disabled until platform bridge installed.
    return false;
}

// ---------------------------------------------------------------------------
// Static function pointers — initialized to null defaults.
// ---------------------------------------------------------------------------

static void(*FireFn)(EHapticEvent)  = &NullFire;
static bool(*IsEnabledFn)()         = &NullIsEnabled;

// ---------------------------------------------------------------------------
// IHapticDispatch public interface — dispatches through the current backend.
// ---------------------------------------------------------------------------

bool IHapticDispatch::IsSystemHapticsEnabled()
{
    // Game-thread contract per ADR-0002 — machine-enforced via check() so future
    // callers on a background thread trip an assertion rather than silently race
    // the function-pointer swap.
    check(IsInGameThread());
    return IsEnabledFn();
}

void IHapticDispatch::Fire(EHapticEvent Event)
{
    // Game-thread contract per ADR-0002 — see IsSystemHapticsEnabled comment.
    check(IsInGameThread());
    FireFn(Event);
}

// ---------------------------------------------------------------------------
// Test-only swap API — compiled only in dev/automation builds.
// ---------------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS

void IHapticDispatch::TestOnly_SetFireFn(void(*InFn)(EHapticEvent))
{
    FireFn = (InFn != nullptr) ? InFn : &NullFire;
}

void IHapticDispatch::TestOnly_SetIsEnabledFn(bool(*InFn)())
{
    IsEnabledFn = (InFn != nullptr) ? InFn : &NullIsEnabled;
}

void IHapticDispatch::TestOnly_Reset()
{
    FireFn      = &NullFire;
    IsEnabledFn = &NullIsEnabled;
}

#endif // WITH_DEV_AUTOMATION_TESTS

// Copyright (c) Game Studio. All rights reserved.
// SLIPSTORM — 60-second voxel endless runner, mobile, Unreal Engine 5.7
//
// RunStateMachineSubsystem — stub implementation.
// Full implementation deferred to RSM epic (ADR-0007).
// TODO(RSM epic): stub — replace with ADR-0007 implementation.

#include "RunStateMachine/RunStateMachineSubsystem.h"

// ---------------------------------------------------------------------------
// Public API — all stubs return safe defaults.
// TODO(RSM epic): stub — replace with ADR-0007 implementation.
// ---------------------------------------------------------------------------

void URunStateMachineSubsystem::ForceTickNow()
{
    // TODO(RSM epic): stub — replace with ADR-0007 implementation.
    // Full implementation: idempotent within-frame tick via bHasTickedThisFrame guard (ADR-0007 SD2).

#if WITH_DEV_AUTOMATION_TESTS
    // JC-1 + ordering snapshot: capture the current GetCurrentState call count
    // BEFORE incrementing the ForceTickNow counter. `force_tick_now_prologue`
    // test asserts this equals 0 after a TickComponent invocation, proving
    // ForceTickNow ran BEFORE any GetCurrentState property read (ADR-0009 IG-1).
    TestOnly_GetCurrentStateCountAtForceTickNow = TestOnly_GetCurrentStateCallCount;
    ++TestOnly_ForceTickNowCallCount;
#endif // WITH_DEV_AUTOMATION_TESTS
}

ERunState URunStateMachineSubsystem::GetCurrentState() const
{
    // TODO(RSM epic): stub — replace with ADR-0007 implementation.
#if WITH_DEV_AUTOMATION_TESTS
    ++TestOnly_GetCurrentStateCallCount;
    return TestOnly_CurrentState;
#else
    return ERunState::IDLE;
#endif
}

ERunOutcome URunStateMachineSubsystem::GetRunOutcome() const
{
    // TODO(RSM epic): stub — replace with ADR-0007 implementation.
    return ERunOutcome::NONE;
}

bool URunStateMachineSubsystem::IsPaused() const
{
    // TODO(RSM epic): stub — replace with ADR-0007 implementation.
#if WITH_DEV_AUTOMATION_TESTS
    return TestOnly_bPaused;
#else
    return false;
#endif
}

bool URunStateMachineSubsystem::IsResumeGrace() const
{
    // TODO(RSM epic): stub — replace with ADR-0007 implementation.
#if WITH_DEV_AUTOMATION_TESTS
    return TestOnly_bResumeGrace;
#else
    return false;
#endif
}

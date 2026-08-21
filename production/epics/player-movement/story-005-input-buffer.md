# Story 005: Single-slot input buffer + Rule 3 buffer-drop + Rule 11 discard

> **Epic**: Player Movement (Slip)
> **Status**: Complete
> **Layer**: Core
> **Type**: Logic
> **Estimate**: 4–5 hours
> **Manifest Version**: 2026-07-03 (docs/registry/architecture.yaml v8)
> **Last Updated**: 2026-07-13

## Context

**GDD**: `design/gdd/player-movement-mechanics.md` (§3 Rules 3/4/11, §4 F-4 pre-validation, §8 AC-04/05/06/07/19/25).
**Requirement**: `TR-PM-012` (primary owner — buffer + F-4 called from HandleSlipTransition buffered path).
*(F-4 pure utility is IMPLEMENTED in Story 003 for the initial-input path; this story CALLS the existing F-4 for buffered pre-validation.)*
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0009 (SD1 UActorComponent hosts buffer state), secondary ADR-0002 (BufferDrop haptic dispatch via `IHapticDispatch::Fire(EHapticEvent::BufferDrop)`).
**ADR Decision Summary**: Single-slot input buffer state lives on `UPlayerLaneMovementComponent`. Rule 3 drop feedback dispatches `EHapticEvent::BufferDrop` through the ADR-0002 bridge + fires audio sting synchronously in the same event call. Rule 11 buffer discard on any non-RUNNING state entry.

**Engine**: Unreal Engine 5.7 | **Risk**: LOW (ADR-0002 interface INT-002-stable; `BufferDrop` enum value predates INT-002 amendment)
**Engine Notes**: `IHapticDispatch` is a project seam interface (not engine API). No post-cutoff engine surface consumed here.

**Control Manifest Rules (this layer)** — from `docs/registry/architecture.yaml` v8:
- **Required**: Buffer holds AT MOST ONE queued input; a second input while buffer full = Rule 3 drop (fire feedback, do not overwrite).
- **Required**: Rule 11 buffer discard on ANY non-RUNNING RSM state entry (implementation lives partially here + partially in Story 008/009 handlers — this story owns the buffer flag reset).
- **Required**: Rule 3 buffer-drop feedback = haptic dispatch + audio sting SYNCHRONOUS in the same event call (AC-25). Haptic call: `IHapticDispatch::Fire(EHapticEvent::BufferDrop)` gated by `IsSystemHapticsEnabled()`.
- **Forbidden**: Overwriting a queued buffered input on the second-input path.

---

## Acceptance Criteria

*From `design/gdd/player-movement-mechanics.md` §3 Rules 3/4/11 + §8, scoped to this story:*

- [ ] Buffer state on component: `bool has_queued_input`; `ESlipDirection queued_input_direction`. Initialized false / undefined in Story 001 declaration (verify — this story exercises the fields).
- [ ] `HandleSlipTransition(Dir)` buffered-input path (extending Story 003's skeleton):
  - [ ] Rule 5 gate (already handled in Story 003 skeleton) → DISCARD if RSM not RUNNING or paused or grace.
  - [ ] If `movement_state == SLIPPING`:
    - [ ] If `has_queued_input == true` → **Rule 3 drop**: leave the existing buffered slot UNCHANGED; fire `IHapticDispatch::Fire(EHapticEvent::BufferDrop)` (if `IsSystemHapticsEnabled()`); fire audio sting; return.
    - [ ] Else: **F-4 pre-validation** at the CURRENT target lane (not source). If F-4 says invalid (projected off-track) → drop the input (also fires edge-absorb via Story 007 hook `TriggerEdgeAbsorb(target_lane, Dir)` — this story invokes the hook). Else: `has_queued_input = true; queued_input_direction = Dir`.
- [ ] `FlushBufferedInput()` (stubbed in Story 003 — implemented here): called from `CompleteTween()`. Behavior:
  - [ ] If `has_queued_input == true`, IMMEDIATELY (same tick, no idle frame) invoke `HandleSlipTransition(queued_input_direction)`. This will re-enter and (assuming valid) commit a new SLIPPING transition on the same tick. `has_queued_input = false; queued_input_direction = <undefined>` before the re-entry.
- [ ] Rule 11 discard: any `HandleStateChanged` transition to non-RUNNING (COUNTDOWN/DEAD/COMPLETE/ABORTED/IDLE) → `has_queued_input = false`. This story publishes a private helper `DiscardBuffer()`; Story 008 calls it from terminal state handlers.
- [ ] **AC-04 (buffer flush @ completion)**: buffer preserved across a SLIPPING tween; on CompleteTween, buffered slip fires SAME TICK; no idle frame; new tween begins immediately.
- [ ] **AC-05 (second input dropped when full)**: given SLIPPING + `has_queued_input == true`, second `HandleSlipTransition(...)` call leaves the queued value unchanged; feedback fires.
- [ ] **AC-06 (edge-check discard on buffer)**: given SLIPPING Left→FarLeft + buffered `slip-left` — F-4 sees projected FarLeft-going-Left as off-track and DISCARDS; buffer NOT set; edge-absorb hook fires.
- [ ] **AC-07 (counter on flush)**: buffered slip that flushes on CompleteTween → after its own CompleteTween, `slip_complete_count` incremented by 1 additional beat (the flush counts as its own tween).
- [ ] **AC-19 (buffer discard on state leave)**: leaving RUNNING (state transition to COUNTDOWN/DEAD/COMPLETE/ABORTED) → `has_queued_input == false`.
- [ ] **AC-25 (drop feedback synchronous)**: within the same `HandleSlipTransition(Dir)` event call, both the haptic dispatch and audio sting have been invoked before the function returns.

---

## Implementation Notes

*Derived from ADR-0009 SD1 + mechanics §3 Rules 3/4/11 + §4 F-4:*

**Buffered-input path** in HandleSlipTransition (extends Story 003's initial-input path):
```cpp
void UPlayerLaneMovementComponent::HandleSlipTransition(ESlipDirection Dir)
{
    if (!RSMSubsystem
        || RSMSubsystem->GetCurrentState() != ERSMState::RUNNING
        || RSMSubsystem->IsPaused()
        || RSMSubsystem->IsResumeGrace())
    {
        return; // Rule 5 discard
    }

    if (movement_state == ERunSlipState::SLIPPING)
    {
        if (has_queued_input)
        {
            // Rule 3 drop
            if (IHapticDispatch::IsSystemHapticsEnabled())
            {
                IHapticDispatch::Fire(EHapticEvent::BufferDrop);
            }
            PlayBufferDropAudioSting();  // Presentation dispatch
            return;
        }

        // F-4 pre-validation against target_lane (projected end-state)
        EPlayerLane ProjectedTarget;
        if (!IsSlipValidFromLane(target_lane, Dir, ProjectedTarget))
        {
            TriggerEdgeAbsorb(target_lane, Dir); // Story 007 hook
            return;
        }

        has_queued_input = true;
        queued_input_direction = Dir;
        return;
    }

    // SETTLED path — Story 003's implementation
    // (unchanged)
}

void UPlayerLaneMovementComponent::FlushBufferedInput()
{
    if (!has_queued_input) return;
    const ESlipDirection Dir = queued_input_direction;
    has_queued_input = false;
    // Re-enter — since PM is now SETTLED (CompleteTween ran), the SETTLED path fires.
    HandleSlipTransition(Dir);
}

void UPlayerLaneMovementComponent::DiscardBuffer()
{
    has_queued_input = false;
}
```

**Rule 11 discard site**: Story 008's `HandleStateChanged` terminal-state branches call `DiscardBuffer()`; Story 009's `HandlePausedChanged` does NOT discard (pause preserves the buffer per AC-13).

**BufferDrop haptic** (ADR-0002 vocabulary): the `EHapticEvent::BufferDrop` enum value predates INT-002 (which added `NearMiss`). Interface is stable.

**Audio sting**: `PlayBufferDropAudioSting()` is a presentation dispatch stub; the audio system owns the actual cue. This story fires the dispatch; audio routing is downstream.

**Scope-added: `IHapticDispatch` seam interface declaration.**

Grep of `Source/` at Story 005's start shows zero declarations of `IHapticDispatch` or
`EHapticEvent::BufferDrop`. ADR-0002 defines these as a project-owned seam contract
but the C++ header has not yet landed. Story 005 owns the minimal declaration required
to consume the interface — the platform-side implementation (iOS/Android haptic
backends) remains deferred to ADR-0002's polish-phase implementation story.

**Files to create in this story**:
- `Source/SLIPSTORM/Seam/IHapticDispatch.h` — the interface + `EHapticEvent` enum with
  `BufferDrop` value only. Comment placeholders for future `NearMiss` (Story 012) and
  `SlipConfirmed` (out-of-epic) values so the enum ordering stays stable across stories.
- `Source/SLIPSTORM/Seam/IHapticDispatch.cpp` — null default implementation:
  `IsSystemHapticsEnabled()` returns `false`; `Fire(EHapticEvent)` is a no-op.
  Guarantees Story 005's PM code compiles and runs on desktop/CI (haptics silently
  disabled) without waiting for the ADR-0002 platform bridge.

Minimal header sketch (final signature per ADR-0002 INT-002-amended contract):
```cpp
UENUM()
enum class EHapticEvent : uint8
{
    BufferDrop     UMETA(DisplayName = "Buffer Drop"),
    // NearMiss     — added by Story 012 (near-miss-beat)
    // SlipConfirmed — added by out-of-epic slip-confirmation story
};

class SLIPSTORM_API IHapticDispatch
{
public:
    static bool IsSystemHapticsEnabled();
    static void Fire(EHapticEvent Event);
};
```

**Test-only spy**: the unit test file declares `FSpyHapticDispatch` (test-local, NOT in
the seam) that intercepts `Fire` calls via a static swap or thread-local injection
pattern so AC-25 can verify `Fire(BufferDrop)` was invoked exactly once per drop event.
Details are implementation choice — the point is that the test asserts against a spy,
not the seam's null impl.

**Hygiene edit**: update the `// TODO(Story 012): dispatch haptic + audio via ADR-0002
IHapticDispatch bridge.` comment at `Source/SLIPSTORM/Seam/PlayerMovementProvider.cpp:66`
to reference this story — the interface + `BufferDrop` enum land here; Story 012 only
extends the enum with `NearMiss` + wires the near-miss dispatch site.

---

## Out of Scope

- Story 003: F-4 pure utility implementation (this story CALLS it).
- Story 007: `TriggerEdgeAbsorb` implementation + F-6 tail advance + `edge_absorb_trigger_count++` (this story invokes the hook when F-4 discards a buffered off-track input).
- Story 008: `DiscardBuffer()` invocation from `HandleStateChanged` terminal handlers.
- Story 009: pause behavior (buffer PRESERVED on pause per AC-13).
- Story 011: slip audio cue on the buffered flush's own tween — that's the slip cue (Story 011), not the buffer-drop sting.
- ADR-0002 platform bridge implementation (iOS Core Haptics + Android VibrationEffect backends) — Story 005 declares the interface with a null default impl so PM code compiles; the platform-specific implementation is ADR-0002's polish-phase scope (per ADR-0002 lines 302 + 329 device verification gate).

---

## QA Test Cases

*Test file: `tests/unit/player-movement/pm_input_buffer_test.cpp`. Automated unit tests using a stubbed haptic dispatcher + RSM.*

- **AC-04 (buffer flush on completion — no idle frame)**:
  - Given: PM SLIPPING Left→Center at TP=0.5; `HandleSlipTransition(Right)` invoked → buffered. Simulate ticks until TP >= 1.0.
  - When: TickComponent runs the CompleteTween tick.
  - Then: within the same tick, `movement_state == SLIPPING` (new tween Center→Right); `current_lane == Center` (new source); `target_lane == Right`; `has_queued_input == false`; pawn root moved to LaneWorldX(Right). One tick, not two.
  - Edge cases: check `slip_complete_count == 2` after the flush's own CompleteTween (Left→Center + Center→Right).

- **AC-05 (second input dropped when buffer full)**:
  - Given: PM SLIPPING with `has_queued_input == true`, `queued_input_direction = Right`.
  - When: `HandleSlipTransition(Left)` invoked (second attempt).
  - Then: `queued_input_direction` STILL `Right`; `has_queued_input` still true; `IHapticDispatch::Fire(EHapticEvent::BufferDrop)` invoked exactly once; audio sting invoked exactly once.
  - Edge cases: haptic disabled system-wide (`IsSystemHapticsEnabled() == false`) → haptic dispatch skipped; audio still fires.

- **AC-06 (edge-check discard on buffered input)**:
  - Given: PM SLIPPING Left→FarLeft at TP=0.3 (`target_lane == FarLeft`).
  - When: `HandleSlipTransition(Left)` invoked — buffered input would project FarLeft-going-Left = off-track.
  - Then: F-4 returns false; `has_queued_input == false` (not set); `TriggerEdgeAbsorb(FarLeft, Left)` hook called; no haptic BufferDrop fire (this is an edge no-op, not a buffer drop).

- **AC-07 (counter on flush)**:
  - Given: fresh PM at Center, `slip_complete_count == 0`.
  - When: sequence — `HandleSlipTransition(Right)` (initial), then during SLIPPING `HandleSlipTransition(Right)` (buffered → Right→FarRight). Ticks until both tweens complete.
  - Then: after final CompleteTween, `slip_complete_count == 2`; `current_lane == FarRight`.

- **AC-19 (buffer discard on non-RUNNING state entry)**:
  - Given: PM SLIPPING with `has_queued_input == true`.
  - When: `DiscardBuffer()` invoked (simulating Story 008's HandleStateChanged terminal call).
  - Then: `has_queued_input == false`.
  - Edge cases: this story exports `DiscardBuffer()` as a private helper — integration with HandleStateChanged is verified in Story 008.

- **AC-25 (drop feedback synchronous within event call)**:
  - Given: SLIPPING + buffer full.
  - When: `HandleSlipTransition(Left)` invoked.
  - Then: within the same call stack frame (before return), spy on IHapticDispatch confirms `Fire(BufferDrop)` invoked AND audio sting stub invoked. Both invocations observed on the same tick's frame trace.

- **Rule 3 idempotence**:
  - Given: SLIPPING + buffer full.
  - When: `HandleSlipTransition(Left)` invoked THREE times in a row.
  - Then: three BufferDrop dispatches (one per event call); `queued_input_direction` never mutates from original.

---

## Test Evidence

**Story Type**: Logic
**Required evidence**:
- Automated unit test at `tests/unit/player-movement/pm_input_buffer_test.cpp` — must exist and pass.

**Status**: [ ] Not yet created

---

## Dependencies

- **Depends on**: Story 001 (buffer field declarations + IHapticDispatch bridge available); Story 003 (F-4 helper + HandleSlipTransition initial-input path + CompleteTween invokes FlushBufferedInput); Story 007 (TriggerEdgeAbsorb hook target — can be a stub during Story 005's tests if Story 007 not landed yet).
- **Unlocks**: Story 008 (calls `DiscardBuffer()` from terminal-state handlers); Story 009 (pause preserves buffer).

---

## Completion Notes

**Completed**: 2026-07-13
**Criteria**: 10/10 passing — all COVERED by automated tests (traceability table in `/story-done` session log)
**Deviations**:
- **ADVISORY**: `## Test Evidence` section above names `tests/unit/player-movement/pm_input_buffer_test.cpp` (aspirational unit path). Actual test lives at `Source/SLIPSTORM/Tests/Integration/PlayerMovement/PMInputBufferTest.cpp`. Reason: `HandleSlipTransition`'s Rule 5 gate reads `RSMSubsystem->GetCurrentState()`, so tests need a `URunStateMachineSubsystem` — cheapest way to obtain one is via UWorld spawn. Same pattern as Story 004's CC1-CC3 and PMStateMachineTest.cpp. Recurring convention drift from Story 004 → surfaces as a story-template fix rather than per-story correction.
- **ADVISORY**: qa-tester recommended AAA (Arrange/Act/Assert) phase labels in every test branch. Applied to the new `rule5_gate_blocks_buffer_when_not_running` test only (demonstrates the pattern). Existing 7 test branches retain prior comment style. Story 006+ should adopt AAA labels per `test-standards.md`.
- **NOT-A-DEVIATION** (documented for record): The `IHapticDispatch` interface scope-add is per the pre-`/dev-story` readiness plan (Option A during Phase 9 of the readiness pass) and is explicitly documented in the story's Implementation Notes. The interface + `EHapticEvent::BufferDrop` enum land here; Story 012 will extend the enum with `NearMiss` + wire the near-miss dispatch site.
**Test Evidence**: Logic — automated integration tests at `Source/SLIPSTORM/Tests/Integration/PlayerMovement/PMInputBufferTest.cpp` (8 test commands covering AC-04/05/06/07/19/25 + Rule 3 idempotence + Rule 5 gate regression). Evidence gate: **PASS**.
**Build verification**: `Result: Succeeded` (40.61 s; 0 errors, 0 warnings) — Story 005 code compiles and links cleanly in the SLIPSTORM Editor target.
**Code Review**: Complete — `/code-review` in the same session with `unreal-specialist + qa-tester` in parallel → APPROVED WITH SUGGESTIONS. User chose option B — **all 8 suggestions applied inline**:
1. `check(IsInGameThread())` added to `IHapticDispatch::Fire()` + `IsSystemHapticsEnabled()` — machine-enforced game-thread contract
2. `EHapticEvent::BufferDrop = 0` explicit ordinal — stability contract compiler-visible
3. New `rule5_gate_blocks_buffer_when_not_running` test — regression guard against future refactor hoisting the buffer write above the Rule 5 gate; covers PAUSED + RESUME_GRACE + non-RUNNING states
4. Removed dead `mutable` on `BufferDropAudioSting_TestOnlyCallCount`
5. Moved `#include "Seam/IHapticDispatch.h"` from PLMC.h to PLMC.cpp only (transitive include reduction)
6. Removed unused `SLIPSTORM_API` on `IHapticDispatch` (module-private class)
7. Cleaned "actually — no" live-thinking-trace comment in AC-04 test
8. AAA labels applied to new rule5 test (pattern demonstration for Story 006+)
**Files touched**:
- `Source/SLIPSTORM/Seam/IHapticDispatch.h` (new — enum + interface + spy hooks)
- `Source/SLIPSTORM/Seam/IHapticDispatch.cpp` (new — null defaults + function-pointer swap + `check(IsInGameThread())`)
- `Source/SLIPSTORM/Player/PlayerLaneMovementComponent.h` (+54 lines: helper decls, TestOnly counter, friend `FPMInputBufferTest`)
- `Source/SLIPSTORM/Player/PlayerLaneMovementComponent.cpp` (+70 lines: SLIPPING branch of HandleSlipTransition, FlushBufferedInput/DiscardBuffer/PlayBufferDropAudioSting bodies, `Seam/IHapticDispatch.h` include)
- `Source/SLIPSTORM/Tests/Integration/PlayerMovement/PMInputBufferTest.cpp` (new — 8 test commands)
- `Source/SLIPSTORM/Seam/PlayerMovementProvider.cpp` (1-line hygiene: `TODO(Story 012)` now scoped to `NearMiss` extension only, since interface + `BufferDrop` landed in Story 005)


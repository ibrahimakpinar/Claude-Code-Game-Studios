---
story: production/epics/player-movement/story-001a-test-harness.md
tr_ids: [TR-PM-001, TR-PM-002, TR-PM-003, TR-PM-010, TR-PM-025, TR-PM-027, TR-PM-034]
adrs: [docs/architecture/adr-0009-player-movement-hosting.md, docs/architecture/adr-0007-run-state-machine-hosting.md]
cpp_test: Source/SLIPSTORM/Tests/Integration/PlayerMovement/PMLifecycleAndSeamTest.cpp
---

# PM Lifecycle and Seam 12 — Integration Test Spec

## Overview

This spec covers the six integration test cases for Story 001 (Pawn + Component
skeleton + lifecycle + Seam 12 wire), delivered as the Story 001a test harness.
The C++ counterpart is `PMLifecycleAndSeamTest.cpp` (category
`SLIPSTORM.PlayerMovement.LifecycleAndSeam`).

**Runner**: UE Automation Framework, headless (`-nullrhi -nosound -unattended`).
**Flags**: `ClientContext | ProductFilter` (compatible with the CI headless runner).
**Build guard**: entire test file is `#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS`.

Compile-time gates AC-SS-B (ordinal lockstep) and AC-SS-E (`MIN_ESCAPE_SLIPS`)
are implicit — a successful build proves them; no runtime test cases are added.

**World setup pattern**: All cases that invoke BeginPlay use
`FAutomationEditorCommonUtils::CreateNewMap()` to obtain a `UWorld` with a valid
`GameInstance`. Curve injection uses `SpawnActorDeferred` + `FinishSpawning` so
curves are set before BeginPlay fires. Each sub-case spawns and destroys its
own pawn; no state leaks between sub-cases.

**Friend access**: `UPlayerLaneMovementComponent` grants
`friend class FPMLifecycleAndSeamTest;` under `#if WITH_DEV_AUTOMATION_TESTS`
at header line 221. This provides direct read access to:
`TickDTRollingBuffer`, `TickDTRingIndex`, `ContinuousCleanWindowTime`,
`bHardwarePerformanceBreachActive`, `RSMSubsystem`, `StateChangedHandle`,
`PausedChangedHandle`.

---

## Test Cases

### TC1 — AC-SS-D: Curve fallback flag set when curves are null / invalid

**Given** `ASlipstormPlayerPawn` deferred-spawned into a test world.
Curves injected (or left null) on `MovementComponent` before `FinishSpawning`.

**When** `FinishSpawning` fires, triggering `BeginPlay()`.

**Then** (primary — all three curves null)
- `bCurveFallbackActive == true` post-BeginPlay.
- `UE_LOG(LogPlayerMovement, Error, ...)` fires once per null curve (3 entries).
- `BeginPlay()` completes without crashing or orphaning the component.

**Edge cases — each in its own deferred-spawn + teardown scope**

**(a) LeanCurve null only**
- `SlipCurve` and `EdgeAbsorbCurve` are valid (2 keys, range [0,1]).
- `LeanCurve = nullptr`.
- `bCurveFallbackActive == true`; error log contains `"LeanCurve is null"`.

**(b) EdgeAbsorbCurve with exactly 1 key (key-count fail)**
- `SlipCurve` and `LeanCurve` are valid.
- `EdgeAbsorbCurve` has 1 key added via `AddKey(0.0f, 0.5f)`.
- `bCurveFallbackActive == true`; error log contains `"EdgeAbsorbCurve has 1 key"`.

**(c) SlipCurve range fail (first-key > 0, last-key < 1)**
- `LeanCurve` and `EdgeAbsorbCurve` are valid.
- `SlipCurve` has 2 keys at times `[0.1f, 0.9f]` — fails both range checks.
- `bCurveFallbackActive == true`; error log contains `"SlipCurve key range"`.

---

### TC2 — AC-SS-D: Curve fallback flag NOT set when all curves are valid

**Given** `ASlipstormPlayerPawn` deferred-spawned; all three curves assigned
`MakeValidCurve()` (2 keys at `[0.0f, 0.0f]` and `[1.0f, 1.0f]`).

**When** `FinishSpawning` fires, triggering `BeginPlay()`.

**Then**
- `bCurveFallbackActive == false` post-BeginPlay.
- No `Error`-level log entries from `LogPlayerMovement`.

---

### TC3 — Watchdog sentinel init: buffer pre-fill

**Given** `ASlipstormPlayerPawn` deferred-spawned into a test world. Curves left
null (irrelevant to watchdog init). `BeginPlay()` fires via `FinishSpawning`.

**When** `BeginPlay()` completes.

**Then**
- `PM->TickDTRollingBuffer[i] == 0.01667f` for all `i` in `[0, 60)` (friend access).
- `PM->TickDTRingIndex == 0` (friend access).
- `PM->ContinuousCleanWindowTime == 0.0f` (friend access).
- `PM->bHardwarePerformanceBreachActive == false` (friend access).
- `PM->is_hw_performance_degraded == false` (public field corroboration).
- `PM->PrimaryComponentTick.bCanEverTick == true`.

**Regression note**
This is the AC-HW-A Setup G Part 1 regression catch. Future refactors that remove
or skip the sentinel init loop in `BeginPlay` MUST fail this test.

---

### TC4 — Delegate lifecycle: BeginPlay bind + EndPlay unbind + IsValid guard

**Given** `ASlipstormPlayerPawn` deferred-spawned into a test world.
`URunStateMachineSubsystem` is present via `GameInstance` (stub subsystem).
Curves left null.

**When** `FinishSpawning` fires (`BeginPlay` runs), then `EndPlay(Destroyed)`
is called directly on the component (double-EndPlay pattern), then `DestroyActor`
is called on the pawn.

**Then**

*(a)* Post-BeginPlay: `PM->StateChangedHandle.IsValid() == true` (friend access).

*(b)* Post-BeginPlay: `PM->PausedChangedHandle.IsValid() == true` (friend access).

*(c)* Post-BeginPlay: `PM->PrimaryComponentTick.bCanEverTick == true`.

*(d)* Post-BeginPlay: `RSMSubsystem->OnStateChanged.IsBound() == true` —
PM is the sole subscriber in the stub test world.
*(Assumption: if the RSM stub gains additional internal bindings in a future story,
this assertion must be updated.)*

*(e)* Direct `PM->EndPlay(EEndPlayReason::Destroyed)` call: no crash.

*(f)* Post-EndPlay: `PM->PrimaryComponentTick.bCanEverTick == false`.

*(g)* Second direct `PM->EndPlay(EEndPlayReason::Destroyed)` call (double-remove):
no crash. `FDelegateHandle::Remove` on an already-removed handle is a UE no-op.

**Edge case (h) — RSMSubsystem GC'd before EndPlay**
A second pawn is spawned. `PM2->RSMSubsystem` is set to `nullptr` directly via
friend access before calling `PM2->EndPlay(Destroyed)`. Must not crash —
the `IsValid(RSMSubsystem)` guard inside `EndPlay` protects the `Remove` calls.

---

### TC5 — Seam 12 production provider: constructor and proxy semantics

**Given** `UPlayerLaneMovementComponent* PM = NewObject<UPlayerLaneMovementComponent>()`.
No world spawn required — `FPlayerMovementProvider_Production` uses `TWeakObjectPtr`
and returns safe defaults on null.

**When** `FPlayerMovementProvider_Production Provider(PM)` is constructed.

**Then**
- `Provider.GetMovementState() == EMovementState::SETTLED` (proxies `PM->movement_state`).
- `Provider.GetCurrentLane()   == EPlayerLane::Center`    (proxies `PM->current_lane`).
- No compile errors on the ordinal cast in `GetMovementState()` (AC-SS-B
  `static_assert` in `PlayerMovementProvider.cpp` confirms zero drift).

**Proxy mutation check**
After setting `PM->current_lane = EPlayerLane::Right` and
`PM->movement_state = ERunSlipState::SLIPPING`:
- `Provider.GetCurrentLane() == EPlayerLane::Right`.
- `Provider.GetMovementState() == EMovementState::SLIPPING`.

---

### TC6 — Pawn subobject wiring + attachment

**Given** `ASlipstormPlayerPawn` spawned via `TestWorld->SpawnActor<ASlipstormPlayerPawn>()`
(non-deferred — curves irrelevant; BeginPlay expected to emit 3 null-curve errors
which are suppressed with `AddExpectedError`).

**When** `SpawnActor` returns (BeginPlay has run).

**Then**
- `Pawn->MovementComponent != nullptr`.
- `Pawn->RootSceneComponent != nullptr`.
- `Pawn->GetRootComponent() == Pawn->RootSceneComponent` *(Story 001a addition)*.
- `Pawn->MeshComponent != nullptr`.
- `Pawn->MeshComponent->GetAttachParent() == Pawn->RootSceneComponent`.
- `Pawn->MovementComponent->GetOwner() == Pawn` *(Story 001a addition — real spawn
  context, not `NewObject`; `GetOwner()` is reliable at BeginPlay per ADR-0009
  Engine Notes item (6))*.

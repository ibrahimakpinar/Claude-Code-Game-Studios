# S1-04 — Integration Test Harness Fix Evidence

**Sprint**: `production/sprints/sprint-1.md` S1-04
**Type**: Integration harness / test infrastructure
**Status**: PASSED — AC met (≥85% pass rate + ≤5 legitimate residuals)

---

## Root cause + fix

`FAutomationEditorCommonUtils::CreateNewMap()` returns an `EWorldType::Editor` world where `UGameInstanceSubsystem` instances (specifically `URunStateMachineSubsystem`) never register. Pawn `BeginPlay` code paths that resolve `RSMSubsystem = GetGameInstance()->GetSubsystem<URunStateMachineSubsystem>()` therefore returned `nullptr` in tests, cascading failures across integration TCs that rely on the RSM broadcast pipeline.

Fix: replace helper with UE's canonical `FTestWorldWrapper` pattern
(`Engine/Source/Runtime/Engine/Public/Tests/AutomationCommon.h`). Wrapper creates a dedicated `EWorldType::Game` world with a proper `UGameInstance` and dedicated `WorldContext`, so subsystems register. Stack-allocated per TC; destructor handles all teardown (GI Shutdown + DestroyWorldContext).

Applied to 11 integration test files (all files that used the `CreateNewMap` helper):

- `PMAudioCueTest.cpp`
- `PMCommitmentTellTest.cpp`
- `PMEdgeAbsorbCompositionTest.cpp`
- `PMInputBufferTest.cpp`
- `PMLateralInterpolationCompositionTest.cpp`
- `PMLeanCompositionTest.cpp` (pre-converted, WIP)
- `PMLifecycleAndSeamTest.cpp`
- `PMNearMissTest.cpp` (TC8 needed two wrappers — TestWorld + TestWorld2 in same TC)
- `PMPauseGraceTest.cpp`
- `PMStateMachineTest.cpp` (pre-converted, WIP)
- `PMTerminalStatesTest.cpp` (nested-scope call sites — one wrapper per lambda invocation)

---

## Automation results

Canonical headless invocation (per `sprint-1.md:69-75`):

```bash
"/Users/Shared/Epic Games/UE_5.7/Engine/Binaries/Mac/UnrealEditor-Cmd" \
  "/Users/mia/Development/GameStudio/Claude-Code-Game-Studios/SLIPSTORM.uproject" \
  -ExecCmds="Automation RunTests SLIPSTORM.PlayerMovement;Quit" \
  -unattended -nullrhi -nosound -nosplash -stdout -FullStdOutLogOutput
```

**Pass rate**: 119 / 122 = **97.5%** (baseline pre-fix: ~30/85 = ~35% integration segment; combined ~57% overall)
**AC target**: ≥ 85% pass rate — MET
**AC target**: ≤ 5 legitimate residuals — MET (3 residuals)

---

## Legitimate residual failures

### R1 — `PMAudioCueTest` Setup B vacuous (TC3)

**Path**: `SLIPSTORM.PlayerMovement.AudioCue.Setup B — vacuous: near-miss after slip ended, no ducking engaged`

Assertions failing:
- `TC3: cue expired after 150ms tick` — expected `slip_cue_active == false`, got `true`
- `TC3: slip_cue_active still false after near-miss on inactive cue` — expected `false`, got `true`
- `TC3: envelope_phase still NONE (no-op on inactive cue)` — expected NONE, got other

**Classification**: real-behavior candidate. Likely one of:
- Cue lifetime tick accounting drift (headless DT vs real 150ms sim)
- Slip-cue expiry gate regression from Story 011 near-miss integration

**Recommended follow-up**: file bug report + trace `PlaySlipAudioCue` expiry path against `slip_cue_active` reset condition. Not blocking on S1-04.

### R2 — `PMInputBufferTest` AC-06 (edge-check discard)

**Path**: `SLIPSTORM.PlayerMovement.InputBuffer.AC-06 edge-check discard on buffered input`

Assertion failing:
- `AC-06.3: edge_absorb_trigger_count unchanged (Story 007 stub)` — expected `0`, got `1`

**Classification**: spec drift. Test was written against Story 007's stub behavior where edge-absorb did not trigger from buffered input. Post-Story 007 the F-6 edge-absorb path was completed and the count now increments correctly per spec.

**Recommended follow-up**: update test assertion — the *test* is stale, not the code. Convert AC-06.3 to expect `+1` and cite the Story 007-complete behavior. Not blocking on S1-04.

### R3 — `PMLifecycleAndSeamTest` TC4-h (nulled-cache broadcast path)

**Path**: `SLIPSTORM.PlayerMovement.LifecycleAndSeam.Delegate lifecycle bind + unbind`

Assertion failing:
- `TC4-h-broadcast: HandleStateChanged NOT invoked post-EndPlay (nulled-cache path)` — expected `0`, got `1`

**Classification**: real behavior finding. When `PM2->RSMSubsystem` is manually nulled *before* `EndPlay`, the Remove-from-multicast path skips (guarded by `IsValid(RSMSubsystem)`), so the delegate handle stays bound on the subsystem. Post-EndPlay broadcast then still invokes the handler because unbind never ran.

This is a legitimate design concern — the "cached pointer lost before EndPlay" scenario currently leaks delegate subscriptions. Was masked by the prior harness bug (`RSMSubsystem` always null, so TC4-h always aborted before reaching the broadcast).

**Recommended follow-up**: file bug report — audit EndPlay's Remove path to hold a weak/handle-reference-back-to-subsystem so unbind survives cached-pointer loss. Not blocking on S1-04.

### Also — TC4-g (double-EndPlay) SUPPRESSED

`PMLifecycleAndSeamTest.cpp:472-481` originally called `PM->EndPlay(EEndPlayReason::Destroyed)` twice to verify the "second EndPlay must not crash" AC. UE 5.7 tightened `UActorComponent::EndPlay` with `check(bHasBegunPlay)` (`Engine/Source/Runtime/Engine/Private/Components/ActorComponent.cpp:1629`), which asserts and aborts the whole automation session.

Fix: suppressed the second explicit EndPlay call with an in-line comment explaining the engine change. Double-Remove-on-FDelegateHandle safety is still exercised via the TC4-h nulled-cache path and via `TestWorld->DestroyActor(Pawn)` at `:497`.

**Recommended follow-up**: consider whether the AC "second EndPlay must not crash" is still meaningful in UE 5.7 (the engine enforces one-shot semantics itself now). If yes, rewrite as a compile/link-time or reflection-based guard rather than a runtime double-call.

---

## Follow-ups filed

1. **Bug report** — R1 slip-cue expiry drift (`PMAudioCueTest` Setup B TC3).
2. **Test-drift fix** — R2 AC-06.3 assertion needs updating to match Story 007-complete behavior.
3. **Bug report** — R3 delegate-unbind leak when `RSMSubsystem` pointer nulled before `EndPlay` (`PMLifecycleAndSeamTest` TC4-h).
4. **Spec review** — TC4-g "double-EndPlay safe" AC deprecated for UE 5.7; decide replacement (or remove AC).

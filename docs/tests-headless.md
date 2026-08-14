# Headless Test Runbook — SLIPSTORM (UE 5.7)

Canonical invocation, flag-registration rule, common pitfalls, and how to add a
new test category. Companion to `tests/README.md` (quick-reference); this doc is
the "why + how to debug" runbook.

## 1. Canonical `UnrealEditor-Cmd` Invocation

**Local (macOS, Sprint 1 baseline)**:

```bash
"/Users/Shared/Epic Games/UE_5.7/Engine/Binaries/Mac/UnrealEditor-Cmd" \
  "/absolute/path/to/SLIPSTORM.uproject" \
  -ExecCmds="Automation RunTests SLIPSTORM.PlayerMovement;Quit" \
  -unattended -nullrhi -nosound -nosplash -stdout -FullStdOutLogOutput
```

**CI** (`.github/workflows/tests.yml:48-53`):

```bash
"$UE_EDITOR_PATH" "${GITHUB_WORKSPACE}/SLIPSTORM.uproject" \
  -nullrhi -nosound -unattended \
  -ExecCmds="Automation RunTests SLIPSTORM.; Quit" \
  -log -stdout -FullStdOutLogOutput \
  -TestExit="Automation Test Queue Empty" \
  -ReportOutputPath="${GITHUB_WORKSPACE}/Saved/Automation/Logs"
```

**Flag meanings**:

| Flag | Purpose |
|------|---------|
| `-unattended` | Do not prompt for input; required for headless. |
| `-nullrhi` | No RHI (no GPU / no window); required on headless CI runners. |
| `-nosound` | Skip audio device init; avoids driver stalls on CI. |
| `-nosplash` | No splash screen. |
| `-stdout` | Route log to stdout in addition to the log file. |
| `-FullStdOutLogOutput` | Do not truncate log lines; full test output visible. |
| `-ExecCmds="Automation RunTests <FILTER>;Quit"` | Runs matching tests then quits. The `Quit` is essential — otherwise the editor stays open after the run. |
| `-TestExit="Automation Test Queue Empty"` (CI only) | Exits the editor process automatically when the automation queue drains — belt + suspenders alongside `Quit`. |
| `-ReportOutputPath=...` (CI only) | Writes JSON test results to the given path for CI upload. |

**Filter syntax**:

- `SLIPSTORM.` — every test in the SLIPSTORM namespace (used by CI).
- `SLIPSTORM.PlayerMovement` — every PM test.
- `SLIPSTORM.PlayerMovement.Watchdog` — a specific category.
- Individual TC filter: not supported at the `RunTests` level — filter by category, then read logs to identify the failing TC.

## 2. Flag-Registration Rule (S1-04 Root Cause)

**Rule**: every `IMPLEMENT_COMPLEX_AUTOMATION_TEST` (and `IMPLEMENT_SIMPLE_AUTOMATION_TEST`) macro MUST include `EAutomationTestFlags::EditorContext` in its flag mask.

**Why**: the Editor's headless discovery pass runs in EditorContext only. Tests declared with `ClientContext` alone are filtered out of the automation manifest and become undiscoverable via `Automation List` / `RunTests` without an active PIE session. Both flags satisfy the `static_assert`s inside `EAutomationTestFlags_ApplicationContextMask` (UE 5.7 `AutomationTest.h:4110`).

**Canonical mask** (used across `Source/SLIPSTORM/Tests/`):

```cpp
IMPLEMENT_COMPLEX_AUTOMATION_TEST(
    FMyTest,
    "SLIPSTORM.System.Feature",
    EAutomationTestFlags::EditorContext
    | EAutomationTestFlags::ClientContext
    | EAutomationTestFlags::ProductFilter)
```

**Symptom of violation**: `Automation List` returns fewer tests than expected; `Automation RunTests <NAME>` reports "0 tests run" instead of executing.

**Reference**: `Source/SLIPSTORM/Tests/Unit/PlayerMovement/PMWatchdogTest.cpp:101-110`. Historical context in `production/qa/evidence/s1-04-harness-fix-evidence.md`.

## 3. World Setup Rule (S1-04 Harness Fix)

**Rule**: for integration tests requiring `UGameInstanceSubsystem`s (such as `URunStateMachineSubsystem`), use `FTestWorldWrapper` — NOT `FAutomationEditorCommonUtils::CreateNewMap()`.

**Why**: `CreateNewMap()` returns an `EWorldType::Editor` world where `UGameInstanceSubsystem` instances never register. Any pawn `BeginPlay` that resolves the subsystem via `GetGameInstance()->GetSubsystem<T>()` receives `nullptr`, cascading into null-deref failures across the test file.

`FTestWorldWrapper` (from `Engine/Source/Runtime/Engine/Public/Tests/AutomationCommon.h`) creates an `EWorldType::Game` world with a proper `UGameInstance` + dedicated `WorldContext` so subsystems register. Stack-allocated per TC; destructor handles GI Shutdown + DestroyWorldContext teardown.

**Canonical pattern** (see any file in `Source/SLIPSTORM/Tests/Integration/PlayerMovement/`):

```cpp
FTestWorldWrapper TestWorld;
TestWorld.CreateTestWorld(EWorldType::Game);
TestWorld.BeginPlayInTestWorld();

// TC body: spawn actors, advance time, resolve subsystems, etc.
// TestWorld destructor at scope end handles teardown.
```

**Special cases**:

- TCs with two independent worlds (e.g. `PMNearMissTest.cpp` TC8) declare two wrappers: `TestWorld` and `TestWorld2`.
- Nested-scope lambdas (e.g. `PMTerminalStatesTest.cpp`) declare one wrapper per lambda invocation to avoid cross-scope teardown races.

## 4. Common Pitfalls

**a) UE 5.7 double-`EndPlay` aborts the automation session**

`UActorComponent::EndPlay` in UE 5.7 asserts `check(bHasBegunPlay)` (`Engine/Source/Runtime/Engine/Private/Components/ActorComponent.cpp:1629`). Calling `EndPlay` twice on the same component crashes the whole session — not just the TC.

Fix: guard test code that verifies "double-EndPlay is safe" — the engine now enforces one-shot semantics itself. See suppressed AC in `PMLifecycleAndSeamTest.cpp:472-481` (S1-04 evidence R-TC4-g).

**b) Missing `Quit` in `-ExecCmds` leaves editor open forever**

CI hangs indefinitely if the editor doesn't quit. Always end the `ExecCmds` string with `;Quit`. Belt-and-suspenders: pass `-TestExit="Automation Test Queue Empty"` as CI does.

**c) `Automation List` reports fewer tests than expected**

Almost always a missing `EditorContext` flag — see Section 2.

**d) Subsystem-null NPE during `BeginPlay`**

Almost always the `CreateNewMap` vs `FTestWorldWrapper` issue — see Section 3.

**e) `-nullrhi` missing → editor tries to open a window on CI**

Fails on headless runners with GPU init errors. Always include `-nullrhi` for headless invocation.

## 5. How to Add a New Test Category

1. **Choose category name**: `SLIPSTORM.<System>.<Feature>` per `tests/README.md` naming rule.
2. **Choose test type**:
   - **Unit** — pure-math or single-class tests → `Source/SLIPSTORM/Tests/Unit/<System>/`
   - **Integration** — multi-system tests requiring a world → `Source/SLIPSTORM/Tests/Integration/<System>/`
3. **Author the C++ file**:
   - Use `IMPLEMENT_COMPLEX_AUTOMATION_TEST` (for multi-TC tests) or `IMPLEMENT_SIMPLE_AUTOMATION_TEST` (for one-shot tests).
   - **MANDATORY** flag mask: `EAutomationTestFlags::EditorContext | ClientContext | ProductFilter` (Section 2).
   - For integration tests requiring subsystems: use `FTestWorldWrapper` (Section 3).
4. **Author the spec doc** at `tests/<unit|integration>/<system>/<name>-spec.md` listing test cases + expected results per the studio convention.
5. **Verify locally**: run the canonical headless invocation with a category filter scoped to the new category — confirm discovery + pass.
6. **CI wire-up**: no action needed — CI's `SLIPSTORM.` filter picks up everything under the namespace automatically.

## 6. Log Locations

| Path | Contents |
|------|----------|
| `Saved/Logs/` | Full editor log (`SLIPSTORM.log` + rotated `-backup-*.log`) |
| `Saved/Automation/Logs/` | Automation JSON reports (when `-ReportOutputPath` provided) |

## References

- `.github/workflows/tests.yml` — CI invocation
- `tests/README.md` — test-artifact-registry quick-reference
- `production/qa/evidence/s1-04-harness-fix-evidence.md` — S1-04 root-cause + fix history
- `production/sprints/sprint-1.md:69-75` — historical canonical invocation for local runs
- Unreal Engine 5.7: `Engine/Source/Runtime/Engine/Public/Tests/AutomationCommon.h` (`FTestWorldWrapper`)
- Unreal Engine 5.7: `Engine/Source/Runtime/Core/Public/Misc/AutomationTest.h:4110` (`EAutomationTestFlags_ApplicationContextMask`)

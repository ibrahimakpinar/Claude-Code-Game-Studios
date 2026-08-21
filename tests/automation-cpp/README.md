# UE Automation Tests (C++)

UE Automation test classes are compiled into the project module, so the actual
`.h` / `.cpp` source files must live inside `Source/SLIPSTORM/Tests/` (alongside
the rest of the game's C++ code). This directory is a *registry* index for
those tests — it does not contain compiled sources.

## Why two locations?

| Concern | Where it lives |
|---|---|
| Test scenario specs (what to verify, expected values, fixtures) | `tests/unit/` and `tests/integration/` (Markdown) |
| Compiled C++ test classes | `Source/SLIPSTORM/Tests/Unit/` and `Source/SLIPSTORM/Tests/Integration/` |
| Manual test evidence (screenshots, sign-off) | `tests/evidence/` |
| Smoke test list (manual 15-min gate) | `tests/smoke/critical-paths.md` |

The studio convention puts *specs* in `tests/` so they live next to GDDs,
ADRs, and design artifacts — reviewable in PR diffs without compiling the
project. The C++ implementation lives next to game code so it links into the
UE module.

## Class registration

Each C++ test class registers itself via the Unreal Automation macros:

```cpp
// Source/SLIPSTORM/Tests/Unit/Rsm/RunStateMachineTickGuardTest.cpp
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FRunStateMachineTickGuardTest,
    "SLIPSTORM.RSM.TickGuard",
    EAutomationTestFlags::EditorContext
      | EAutomationTestFlags::EngineFilter)

bool FRunStateMachineTickGuardTest::RunTest(const FString& Parameters)
{
    // Spec reference: tests/unit/rsm/tick-guard-spec.md
    // GDD reference: design/gdd/run-state-machine.md §Tick guard
    // ADR reference: docs/architecture/adr-0007-run-state-machine-hosting.md INT-005
    // ...
    return true;
}
```

Each test class **must reference its spec file, GDD section, and (if
applicable) governing ADR** in a comment block at the top of `RunTest()`.
This is the trace path PR reviewers use to verify "this test covers this
acceptance criterion".

## Test category prefix

All categories must start with `SLIPSTORM.` so the Session Frontend filter
and the CI `Automation RunTests SLIPSTORM.` invocation pick them up. Engine
auto-discovered tests under other prefixes (`EditorBuildPromotionTest.`,
`System.Core.`, etc.) are explicitly excluded.

## Running locally

In-editor: `Window → Test Automation` → check `SLIPSTORM.*` → Start Tests.

Headless:
```bash
"$UE_EDITOR_PATH" "$REPO_ROOT/SLIPSTORM.uproject" \
  -nullrhi -nosound -unattended \
  -ExecCmds="Automation RunTests SLIPSTORM.; Quit" \
  -log
```

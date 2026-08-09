# Test Infrastructure

**Engine**: Unreal Engine 5.7
**Test Framework**: UE Automation Framework (built-in)
**CI**: `.github/workflows/tests.yml`
**Setup date**: 2026-06-27

## Directory Layout

```
tests/
  unit/            # Isolated unit tests (formulas, state machines, logic)
  integration/     # Cross-system and save/load tests
  smoke/           # Critical path test list for /smoke-check gate
  evidence/        # Screenshot logs and manual test sign-off records
  automation-cpp/  # UE Automation C++ test class index (actual sources live alongside game code)
```

> **Note on Unreal test class locations**: UE Automation test C++ classes must
> live in the UE project's `Source/<ProjectName>/Tests/` tree (e.g.
> `Source/SLIPSTORM/Tests/`) so they compile into the project module. The
> `tests/` directory above is the studio convention for the *test artifact
> registry*: GDScript-style unit logic specs, integration scenario docs,
> evidence files, and the smoke test list. See `tests/automation-cpp/README.md`
> for how C++ test classes register against this directory.

## Running Tests

**In-editor (interactive)**:
- Open Unreal Editor
- `Window → Test Automation` → Session Frontend
- Filter "SLIPSTORM." → Run Selected

**Headless (CI / local batch)**:

```bash
"$UE_EDITOR_PATH" "/path/to/SLIPSTORM.uproject" \
  -nullrhi -nosound -unattended \
  -ExecCmds="Automation RunTests SLIPSTORM.; Quit" \
  -log
```

Logs are written to `Saved/Logs/`. Test results JSON is written under
`Saved/Automation/Logs/`.

## Test Naming

- **Files** (C++ source): `[System][Feature]Test.cpp` — PascalCase per
  Unreal convention (overrides the studio file-naming default for UE Automation
  test classes only, per Unreal API constraint)
- **Test classes**: `F[SystemName]Test` (e.g. `FRunStateMachineTickGuardTest`)
- **Test categories**: `SLIPSTORM.[System].[Feature]` (e.g.
  `SLIPSTORM.RSM.TickGuard`)
- **Test functions / cases**: `It_Should_<behavior>_When_<condition>` for BDD,
  or `Test_<scenario>_<expected>` for the studio convention

## Story Type → Test Evidence

| Story Type | Required Evidence | Location |
|---|---|---|
| Logic | Automated unit test — must pass | `tests/unit/[system]/` (spec) + `Source/SLIPSTORM/Tests/Unit/` (C++) |
| Integration | Integration test OR playtest doc | `tests/integration/[system]/` (spec) + `Source/SLIPSTORM/Tests/Integration/` (C++) |
| Visual/Feel | Screenshot + lead sign-off | `tests/evidence/` |
| UI | Manual walkthrough OR interaction test | `tests/evidence/` |
| Config/Data | Smoke check pass | `production/qa/smoke-*.md` |

## CI

Tests run automatically on every push to `main` and on every pull request.
A failed test suite blocks merging.

**CI prerequisites** (must be configured before first run):
- Self-hosted runner with Unreal Editor 5.7 installed
- `UE_EDITOR_PATH` environment variable on the runner (full path to
  `UnrealEditor` binary)
- `SLIPSTORM.uproject` generated at repo root (will fail until UE project
  generation is run for the first time)

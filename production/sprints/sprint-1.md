# Sprint 1 — 2026-08-07 to 2026-08-14

## Sprint Goal
Close the Player Movement epic cleanly (commit accumulated work, update epic-close artifacts, land epic-wide runtime-green via Integration test harness fix) and author the next-epic gate ADR-0011 (Wave Spawner pattern library).

**QA Plan**: `production/qa/qa-plan-sprint-1-2026-08-08.md`

## Capacity
- Total days: 5
- Buffer (20%): 1 day reserved for unplanned work
- Available: 4 days
- Solo dev; Claude Code assistant available for all agent tasks.
- Review mode: `lean` (see `production/review-mode.txt`)

## Tasks

### Must Have (Critical Path)
| ID | Task | Agent/Owner | Est. Days | Dependencies | Acceptance Criteria |
|----|------|-------------|-----------|-------------|-------------------|
| S1-01 | Commit Story 013 close (story-013.md + PLMC.h/.cpp + PMWatchdogTest.cpp + active.md) | user + Claude | 0.15 | none | Commit lands on `main`; message follows Conventional Commits + references TR-PM-020/021/022/024/026 + Story: story-013 |
| S1-02 | Commit `chore(tests): headless Automation discovery + PM epic runtime evidence` (15 flag-changed files + evidence) | user + Claude | 0.15 | S1-01 | Separate commit distinct from S1-01; message documents `ClientContext` → `EditorContext` root-cause + 39/39 Unit test pass proof |
| S1-03 | Update PM epic-close artifacts: `EPIC.md` story-status table (mark 003-013 Complete + epic Complete), `production/epics/index.md` (Player Movement row → Done, Core layer 1/1) | user + Claude | 0.2 | S1-02 | EPIC.md status = Complete; all 13 story rows show Complete; index.md shows Player Movement Done + Core layer 1/1 |
| S1-04 | Integration test harness fix — root-cause + fix `CreateTestPlayWorld` helper so RSMSubsystem is available in the test PIE world | unreal-specialist | 1.5 | S1-03 | Re-run `Automation RunTests SLIPSTORM.PlayerMovement` returns ≥ 85% pass rate (up from 57%); ≤ 5 legitimate residual failures documented as tech debt or real bugs |
| S1-05 | Author ADR-0011 (Wave Spawner pattern library) via `/architecture-decision` — resolves the Wave Spawner epic gate | technical-director + Claude | 2 | S1-03 | ADR-0011 authored Accepted or Proposed; Decision + Consequences sections complete; enables `/create-epics wave-spawner` |

### Should Have
| ID | Task | Agent/Owner | Est. Days | Dependencies | Acceptance Criteria |
|----|------|-------------|-----------|-------------|-------------------|
| S1-06 | Story 010 PEAT/Harding evidence **skeleton + design-analytical pre-check** (R12a-PENDING deferred items from Story 010 close; formal Harding FPA / W3C-PEAT gate reconfirmed as Polish-phase per presentation §8 AC-COMMIT-FLASH-CADENCE) | Claude + art-director (Polish) | 0.5 | none | `production/qa/evidence/story-010-peat-evidence.md` exists with design-parameter table, preliminary `flash_rate × contrast` sanity-check per presentation §4 F-COMMIT-CADENCE-CAP working sketch, Polish-phase Harding FPA capture protocol, and empty sign-off rows. Sign-off checkbox marking deferred to Polish per §8. |
| S1-07 | UE-S1 follow-up: ADR-0009 IG-3 outbound-binding carve-out annotation at PLMC.h:121 | Claude | 0.25 | none | One-line comment clarifies IG-3 governs PM's outbound bindings; test-scope lambdas exempt |
| S1-08 | `/architecture-review` PM downstream consumers pass (Wave Spawner R11a-8, HUD banner, ADR-0005 forward contract closure) | technical-director | 0.5 | S1-04, S1-05 | Traceability matrix updated; TR-PM-022/023/024 downstream consumers documented; forward contract table updated |

### Nice to Have
| ID | Task | Agent/Owner | Est. Days | Dependencies | Acceptance Criteria |
|----|------|-------------|-----------|-------------|-------------------|
| S1-09 | QA-S1 follow-up: sequential Setup B → Setup D watchdog integration test (~210-tick real latency) | qa-tester | 0.5 | S1-04 | New TC in PMWatchdogIntegrationTest verifies sequential release-latency; runs headlessly |
| S1-10 | Runtime-verification runbook: `docs/tests-headless.md` documenting canonical `UnrealEditor-Cmd` invocation + how to add new categories | tools-programmer | 0.25 | S1-04 | Runbook exists; includes flag-registration rule, working invocation, common pitfalls |

## Carryover from Previous Sprint
None — this is Sprint 1.

## Risks
| Risk | Probability | Impact | Mitigation |
|------|------------|--------|------------|
| S1-04 harness fix uncovers deeper UE 5.7 PIE-in-headless issue (subsystems require `GameInstance` init that's editor-mode-only) | MEDIUM | HIGH | Scope-boundary at 1.5 day estimate. If root cause exceeds scope, land partial fix (unblock ≥5 tests) + file follow-up story for full fix. Convert Should Have S1-08 into buffer. |
| S1-05 ADR-0011 scope creep — Wave Spawner pattern library needs Rule 11 near-miss detector + PEAK barrage suppression scope decision | MEDIUM | MEDIUM | Consult advisor before writing; time-box authoring session to 3 hrs before revisiting scope. If scope exceeds 2 days, defer to Sprint 2 and use S1-05 slot for architecture-review-only. |
| Uncommitted work from prior session lingers (~75 pre-staged files still hanging around) | LOW | LOW | S1-01 + S1-02 use surgical `git reset HEAD` + targeted `git add` pattern from Stories 011/012 precedent. |
| Integration test harness fix reveals actual gameplay bugs (not just harness bugs) | MEDIUM | HIGH | S1-04 acceptance criteria explicitly allows for ≤5 legitimate failures. New gameplay bugs get filed as bug-report + prioritized in Sprint 2. |

## Dependencies on External Factors
- **UE 5.7 Editor availability** — installed at `/Users/Shared/Epic Games/UE_5.7/Engine/`, verified working headless.
- **No external agent gates in this sprint** (lean mode).

## Definition of Done for this Sprint
- [ ] All Must Have tasks completed
- [ ] All tasks pass acceptance criteria
- [ ] QA plan exists (`production/qa/qa-plan-sprint-1.md`)
- [ ] All Logic/Integration stories have passing unit/integration tests **at ≥ 85% Automation pass rate**
- [ ] Smoke check passed (`/smoke-check sprint`)
- [ ] QA sign-off report: APPROVED or APPROVED WITH CONDITIONS (`/team-qa sprint`)
- [ ] No S1 or S2 bugs in delivered features
- [ ] Design documents updated for any deviations
- [ ] Code reviewed and merged
- [ ] Player Movement epic marked Done in `production/epics/index.md`
- [ ] ADR-0011 authored — Wave Spawner epic ready to enter creation via `/create-epics wave-spawner`

## Runtime-Verification Baseline (established 2026-08-07)
Canonical headless invocation for future stories:
```bash
"/Users/Shared/Epic Games/UE_5.7/Engine/Binaries/Mac/UnrealEditor-Cmd" \
  "/Users/mia/Development/GameStudio/Claude-Code-Game-Studios/SLIPSTORM.uproject" \
  -ExecCmds="Automation RunTests SLIPSTORM.PlayerMovement.<Category>;Quit" \
  -unattended -nullrhi -nosound -nosplash \
  -stdout -FullStdOutLogOutput
```
Test-flag rule: all new automation tests MUST include `EditorContext` in the flag set, otherwise headless discovery filters them out. See S1-10 runbook for full documentation.

Pre-sprint baseline pass rates (as of 2026-08-07 following ClientContext → EditorContext rollout):
- Unit tests: 39/39 GREEN (Watchdog 12, LaneAndTween 5, LateralInterpolation 6, Lean 9, EdgeAbsorb 7)
- Integration tests: ~30/85 GREEN (blocked by RSMSubsystem-null harness bug — this sprint's S1-04 target)

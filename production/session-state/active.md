# Session State

## Handoff — 2026-08-15 (compact)

Full prior handoff archived: `production/session-logs/active-archive-2026-08-15.md` (146 lines, covers 2026-08-09 through 2026-08-15 including the S1-04 harness fix, Sprint 1 closure, ADR-0010 authoring, and prior 2026-08-09 foundation-commit context).

### Current state (as of 2026-08-15 EOD)

- **Branch**: `mymerge` (post `454e1b7` merge from `myorigin/main`; `main` has S1-05 commits only, not S1-07/08/09/10/06 or ADR-0010). Reconciliation is user's call.
- **Working tree**: clean.
- **Sprint 1**: **fully closed** — 7/7 tracked stories done (S1-04, S1-05, S1-06 partial, S1-07, S1-08, S1-09 VERIFIED, S1-10).
- **ADR-0010** (Pull-Wave Object Pool + State Machine + Despawn Pipeline): 387 lines — `0c3180f` (stage 1) + `537e5dc` (stage 2). **Status: Accepted 2026-08-16** (`b94da6e` paired promotion).
- **ADR-0011** (Wave Spawner Pattern Library): 437 lines, `91f4c4c`. **Status: Accepted 2026-08-16** (`b94da6e` paired promotion).

### Paired promotion landed 2026-08-16

Executed via scoped `/architecture-review single-gdd design/gdd/pull-wave-behavior.md` verdict: no blocking coverage gaps (19/29 TR-PW covered; 9 documentary; 0 gaps), no cross-ADR conflicts, hard deps satisfied. Pragmatic-promotion path invoked for ADR-0005 Proposed transitive dependency (same precedent as ADR-0009 Accepted 2026-07-09 with ADR-0002 Proposed).

Also landed in `b94da6e`: TR-PW-027 wording fix + Cross-ADR Forward Contract Closure Log updates (3 rows ⚠️→✅) + new `FPullWaveSpawnParams` row + Feature-layer gaps §7/§8 marked RESOLVED.

**Unblocked**: `/create-stories pull-wave` (9-story epic per ADR-0010 Migration Plan) AND `/create-stories wave-spawner`.

### Next-session recommended (fresh context)

1. **`/create-stories pull-wave`** OR **`/create-stories wave-spawner`** — either epic can enter story authoring. Pull-Wave has 9 planned stories per ADR-0010 Migration Plan; Wave Spawner story count TBD per ADR-0011.
2. **Full `/architecture-review`** — refresh row-level status in the PW table (line 140+) and WS table (line 153+); most rows there are still ❌ from before ADR-0010/0011 authored. Not blocking any new work; hygiene task.
3. ~~**Follow-up #1** — RunUAT/Shipping-target build-verify.~~ ✅ **RESOLVED 2026-08-16** — `SLIPSTORM Mac Shipping` build succeeded in 35 s via `Build.sh SLIPSTORM Mac Shipping`. Produced `Binaries/Mac/SLIPSTORM-Mac-Shipping.app` (222 MB). Fresh clone now confirmed buildable at both Editor + Shipping targets.
4. **S1-04 residuals** — R1 (slip-cue expiry drift), R2 (test staleness vs Story-007-complete), R3 (delegate-unbind leak) — file bug reports; documented at `production/qa/evidence/s1-04-harness-fix-evidence.md`.

## Session Extract — /dev-story 2026-08-17
- Story: production/epics/pull-wave/story-001-struct-definitions.md — Struct Definitions
- Files changed:
  - Source/SLIPSTORM/PullWave/PullWaveTypes.h (CREATED — all enums, structs, constants, static_asserts)
  - Source/SLIPSTORM/Tests/Unit/PullWave/PullWaveStructDefinitionsTest.cpp (CREATED — 8 test commands)
- Test written: Source/SLIPSTORM/Tests/Unit/PullWave/PullWaveStructDefinitionsTest.cpp (8 tests)
- Note: sizeof(FPullWaveInstanceState) = 176 bytes (uint8 enum backing); binding check <= 256 passes. ADR-0010 D3 target of 188 bytes assumed int32 enum backing; see TC4 log output.
- Note: DEFINE_STAT(STAT_PullWaveTick) must be added in Story 004's APullWaveSubsystemActor.cpp
- Note: Test evidence path in story (tests/unit/pull-wave/struct-definitions_test.cpp) differs from actual UE project path (Source/SLIPSTORM/Tests/Unit/PullWave/PullWaveStructDefinitionsTest.cpp) — story updated to reflect actual path
- Note: sizeof(FPullWaveInstanceState) = 176 bytes (uint8 enum backing); binding check <= 256 passes. ADR-0010 D3 target of 188 bytes assumed int32 enum backing; see TC4 log output.
- Note: DEFINE_STAT(STAT_PullWaveTick) must be added in Story 004's APullWaveSubsystemActor.cpp
- Blockers: None
- Post-code-review fix applied 2026-08-17: expanded test from 8 → 11 commands (added registry_constants_pinned TC9, stats_group_declared TC10, despawn_reason_enum TC11); TC1 spike check added for non-uniform Frac isolation
- 2nd code-review fix pass 2026-08-17: (B1) EvaluateAt FMath::Clamp added + FORCEINLINE; (W2) EPullWaveState PascalCase rename (Spawned/Leaning/Traversing/Landed/Despawning); (BLOCK-001) PullWaveStats.cpp stub created with DEFINE_STAT; (GAP-001) ISMCInstanceIndex==-1 sentinel test added to TC7; (W4) ProductFilter→SmokeFilter
- Files changed: Source/SLIPSTORM/PullWave/PullWaveTypes.h, Source/SLIPSTORM/Tests/Unit/PullWave/PullWaveStructDefinitionsTest.cpp, Source/SLIPSTORM/PullWave/PullWaveStats.cpp (CREATED)
- Next: Story 002 — Pool Storage + WaveId (production/epics/pull-wave/story-002-pool-storage-waveid.md)

## Session Extract — /story-done 2026-08-18
- Verdict: COMPLETE WITH NOTES
- Story: production/epics/pull-wave/story-001-struct-definitions.md — Struct Definitions
- Tech debt logged: None (user chose Close without tech-debt log)
- Next recommended: Story 002 — Pool Storage + WaveId — production/epics/pull-wave/story-002-pool-storage-waveid.md

## Session Extract — /dev-story 2026-08-18 (Story 002)
- Story: production/epics/pull-wave/story-002-pool-storage-waveid.md — Pool Storage + WaveId
- Files changed:
  - Source/SLIPSTORM/PullWave/PullWaveTypes.h (EDITED — added USTRUCT()/GENERATED_BODY() to FPullWaveInstanceState; added PullWaveTypes.generated.h include; FPullWaveCurveSnapshot + FPullWaveSpawnParams remain plain C++)
  - Source/SLIPSTORM/PullWave/PullWaveSubsystemActor.h (CREATED — APullWaveSubsystemActor with UPROPERTY() TArray<FPullWaveInstanceState> ActiveWaves and int32 NextWaveId)
  - Source/SLIPSTORM/PullWave/PullWaveSubsystemActor.cpp (CREATED — BeginPlay: Reserve(23), NextWaveId=0)
  - Source/SLIPSTORM/Tests/Unit/PullWave/PullWavePoolStorageWaveIdTest.cpp (CREATED — 7 test commands)
  - production/epics/pull-wave/story-002-pool-storage-waveid.md (EDITED — test evidence updated)
- Decisions applied (2026-08-18):
  - AC7 FORBID-half only: RemoveAtSwap==0 asserted (TC5). ALLOW-half (RemoveAt>=1) deferred to Story 006.
  - AC8 FORBID-half only: Insert==0 asserted (TC6). ALLOW-half (Add>=1) deferred to Story 009.
  - AC4 determinism: two TArray simulations, identical WaveId Add() sequences, verify identical iteration order (TC4). No Tick/Construct.
  - USTRUCT scope: FPullWaveInstanceState only. FPullWaveCurveSnapshot + FPullWaveSpawnParams stay plain C++.
- Post-implementation fix 2026-08-18: Added InitializePool() public method to PullWaveSubsystemActor.cpp; BeginPlay() now delegates to it. Added TC8 (actor_initializepool_seam) to test file — 8 total test commands. TC8 uses NewObject<APullWaveSubsystemActor>(GetTransientPackage()) + InitializePool() to runtime-verify Reserve(23) and NextWaveId==0 without a UWorld.
- Blockers: None
- Next: /code-review then /story-done production/epics/pull-wave/story-002-pool-storage-waveid.md

## Session Extract — /dev-story 2026-08-18 (Story 003)
- Story: production/epics/pull-wave/story-003-state-machine.md — Five-State Machine + TransitionTo() Helper
- Files changed:
  - Source/SLIPSTORM/PullWave/PullWaveSubsystemActor.h (EDITED — added DECLARE_LOG_CATEGORY_EXTERN(LogPullWave); added IsTransitionAllowed(), TransitionTo() declarations)
  - Source/SLIPSTORM/PullWave/PullWaveSubsystemActor.cpp (EDITED — added DEFINE_LOG_CATEGORY(LogPullWave); PullWaveStateToString() helper; IsTransitionAllowed() 25-cell table; TransitionTo() with check()+UE_LOG guard)
  - Source/SLIPSTORM/Tests/Unit/PullWave/PullWaveStateMachineTest.cpp (CREATED — 12 test commands)
- Notable: story doc says "13 forbidden transitions" — actual count is 14 (5×5-5 diagonals-6 legal=14); test corrected, story doc note added
- Blockers: None
- Next: /code-review then /story-done production/epics/pull-wave/story-003-state-machine.md

## Session Extract — /story-done 2026-08-18
- Verdict: COMPLETE WITH NOTES
- Story: production/epics/pull-wave/story-003-state-machine.md — Five-State Machine + TransitionTo() Helper
- Code-review fix pass applied before close: check(bAllowed) gated out of UE_BUILD_TEST; TC10 rewritten to call TransitionTo() on forbidden pair via AddExpectedError(); "13→14" count corrected in 4 display strings + header doc
- Tech debt logged: None (advisory deviations accepted, not logged to register)
- Next recommended: Story 004 — Tick Advance + Traversing — production/epics/pull-wave/story-004-tick-advance-traversing.md

## Session Extract — /story-done 2026-08-18
- Verdict: COMPLETE WITH NOTES
- Story: production/epics/pull-wave/story-002-pool-storage-waveid.md — Pool Storage + WaveId Allocation
- Tech debt logged: None
- Next recommended: Story 003 — Five-State Machine + TransitionTo() Helper — production/epics/pull-wave/story-003-state-machine.md

## Session Extract — /dev-story 2026-08-18 (Story 004)
- Story: production/epics/pull-wave/story-004-tick-advance-traversing.md — Per-Tick Advance (SPAWNED→TRAVERSING) + Pause-Freeze Gate
- Files changed:
  - Source/SLIPSTORM/PullWave/PullWaveTypes.h (EDITED — added PLAYER_PLANE_Z, FLeanTickData, FOnLeanProgressDelegate, FTraverseElapsedQuery)
  - Source/SLIPSTORM/PullWave/PullWaveSubsystemActor.h (REWRITTEN — added IPullWaveRSMProvider, Tick(), AdvanceSpawned/Leaning/Traversing, ComputeTNorm/WorldX/WorldZ static helpers, SetRSMProvider, GetTraverseElapsedForWave, OnLeanProgress delegate, WaveMassISMC)
  - Source/SLIPSTORM/PullWave/PullWaveSubsystemActor.cpp (REWRITTEN — DEFINE_STAT, full tick body with pause-freeze gate, three advance helpers, three static formula helpers, ISMC null-guard, MarkRenderStateDirty batched)
  - Source/SLIPSTORM/PullWave/PullWaveStats.cpp (DELETED — DEFINE_STAT moved to actor .cpp)
  - Source/SLIPSTORM/Tests/Integration/PullWave/PullWaveTickAdvanceTraversingTest.cpp (CREATED — 12 test commands)
- Design decision: AdvanceSpawned() does NOT re-read DPC.telegraph_window_s — ADR-0010 D6 wins; LeanDurationS captured at Construct() (Story 009). Story 004 Implementation Notes incorrectly described this read; D6 is authoritative.
- Code-review fixes applied: BLOCKING-1 (AC-PW-11 field-freeze assertions c-f), WARNING-2 (AddToRoot+ON_SCOPE_EXIT on all 6 actor-based tests), VERIFY-1 (bTeleport=false explicit in UpdateInstanceTransform), ADVISORY-2 (AC-PW-20c tautology replaced with TravelDurationS field assertion)
- Blockers: None

## Session Extract — /story-done 2026-08-18
- Verdict: COMPLETE
- Story: production/epics/pull-wave/story-004-tick-advance-traversing.md — Per-Tick Advance (SPAWNED→TRAVERSING) + Pause-Freeze Gate
- Tech debt logged: None
- Next recommended: Story 005 — LANDED entry body — production/epics/pull-wave/story-005-landed-entry.md

## Session Extract — /dev-story 2026-08-18 (Story 005)
- Story: production/epics/pull-wave/story-005-landed-collision-outcome.md — LANDED Entry + CollisionOutcome + Hit/Near-Miss Broadcasts
- Files changed:
  - Source/SLIPSTORM/PullWave/PullWaveTypes.h (EDITED — added FOnWaveHit + FOnNearMiss multicast delegate declarations)
  - Source/SLIPSTORM/PullWave/PullWaveSubsystemActor.h (EDITED — added #include PlayerMovementProvider.h, SetPMProvider(), OnWaveHit, OnNearMiss delegates, PMProvider* member, ResolveLandedEntry(), AdvanceLanded() declarations)
  - Source/SLIPSTORM/PullWave/PullWaveSubsystemActor.cpp (EDITED — SetPMProvider(), ResolveLandedEntry(), AdvanceLanded() implemented; Tick LANDED case updated; AdvanceTraversing threshold-cross updated to call ResolveLandedEntry inline)
  - Source/SLIPSTORM/Tests/Integration/PullWave/PullWaveLandedCollisionOutcomeTest.cpp (CREATED — 9 test commands)
- Design decisions:
  - ResolveLandedEntry called inline from AdvanceTraversing at threshold-cross (same tick as TransitionTo(LANDED)) — satisfies AC-PW-13
  - TraverseElapsedS clamped to TravelDurationS inside ResolveLandedEntry (not in AdvanceTraversing) — AC-PW-31
  - No PMProvider null-crash: defaults to CleanMiss when no provider wired
  - TriggerNearMissBeat called BEFORE OnNearMiss.Broadcast (control manifest order; verified via TC6 lambda)
  - Pause-freeze for LANDED covered by outer Tick() gate — no second check in AdvanceLanded
- Test evidence: Source/SLIPSTORM/Tests/Integration/PullWave/PullWaveLandedCollisionOutcomeTest.cpp (9 test commands covering AC-PW-13/14/21a/21b/21c/22/28/31 + lean-progress-non-fire)
- Blockers: None
- Next: /code-review then /story-done production/epics/pull-wave/story-005-landed-collision-outcome.md

## Session Extract — /story-done 2026-08-19
- Verdict: COMPLETE WITH NOTES
- Story: production/epics/pull-wave/story-005-landed-collision-outcome.md — LANDED Entry + CollisionOutcome + Hit/Near-Miss Broadcasts
- Tech debt logged: None (advisory test gaps GAP-1/GAP-2/GAP-3 noted in story completion notes, not logged to register)
- Next recommended: Story 006 — Rule 13 Six-Step Despawn Pipeline — production/epics/pull-wave/story-006-despawn-pipeline.md

## Session Extract — /dev-story 2026-08-19 (Story 006)
- Story: production/epics/pull-wave/story-006-despawn-pipeline.md — Rule 13 Six-Step Despawn Pipeline
- Files changed:
  - Source/SLIPSTORM/Seam/CollisionWaveProvider.h (CREATED — ICollisionWaveProvider interface, step 1)
  - Source/SLIPSTORM/Seam/TelegraphWaveProvider.h (CREATED — ITelegraphWaveProvider interface, step 2)
  - Source/SLIPSTORM/PullWave/PullWaveTypes.h (EDITED — FOnWaveDespawned DECLARE_MULTICAST_DELEGATE_TwoParams added)
  - Source/SLIPSTORM/PullWave/PullWaveSubsystemActor.h (EDITED — SetCollisionProvider/SetTelegraphProvider setters + teardown contracts; OnWaveDespawned public delegate; Seam 13 OnDespawnedUserCallback + SetOnDespawnedUserCallback; OnISMCHideOverride + SetOnISMCHideOverride; CollisionProvider/TelegraphProvider private members; AdvanceDespawning declaration)
  - Source/SLIPSTORM/PullWave/PullWaveSubsystemActor.cpp (EDITED — SetCollisionProvider/SetTelegraphProvider impls; AdvanceDespawning 6-step pipeline with pre-capture WaveId/ISMCInstanceIdx; Tick() refactored range-for → index-based with --i in DESPAWNING case; AdvanceLanded updated with explicit PendingDespawnReason=NaturalLanding assignment)
  - Source/SLIPSTORM/Tests/Integration/PullWave/PullWaveDespawnPipelineTest.cpp (CREATED — 11 test commands)
- Design decisions: WaveId/ISMCInstanceIdx captured before step 5 (pre-clear). EDespawnReason passed by value to AdvanceDespawning (safe across step 5 clear). ensure(ActiveWaves.GetSlack()>0) guards step 3 broadcast.
- Blockers: None
- Next: /story-done production/epics/pull-wave/story-006-despawn-pipeline.md

## Session Extract — /code-review 2026-08-19 (Story 006)
- Verdict: CHANGES REQUIRED → all 3 blocking items fixed before /story-done
- Blocking fixes applied:
  - B1: Added ensure(ActiveWaves.GetSlack() > 0) before OnWaveDespawned.Broadcast in AdvanceDespawning (parity with ResolveLandedEntry)
  - B2: Added DespawnFireCount++ and TestEqual("fires exactly once", FireCount, 1) to TC1, TC2, TC3
  - B3: Added OnStateClearOverride seam (non-Shipping TFunction + setter in .h; fired after step 5 clear, before step 6 RemoveAt in .cpp); TC5 rewritten with field-level assertions (WaveId, TargetLane, State, CollisionOutcome, ForwardVelocityMs, TraverseElapsedS all verified zero/default)
- Advisory fixes applied: stale "range-for loop" → "indexed Tick loop" in OnWaveHit/OnNearMiss subscriber constraint doc; cosmetic double-divider before ResolveLandedEntry removed from .cpp
- Next: /story-done production/epics/pull-wave/story-006-despawn-pipeline.md

## Session Extract — /story-done 2026-08-19
- Verdict: COMPLETE WITH NOTES
- Story: production/epics/pull-wave/story-006-despawn-pipeline.md — Rule 13 Six-Step Despawn Pipeline
- Tech debt logged: None (ADVISORY ADR D4 step 5 WaveId deviation accepted, documented in completion notes)
- Next recommended: Story 007 — Pause-Flush — production/epics/pull-wave/story-007-pause-flush.md

### Persistent follow-ups (not blocking, carried from prior handoff)

- **`docs/architecture/control-manifest.md`** — does not exist; referenced by `/story-done` manifest-staleness check (currently skipped).
- **`directory-structure.md` doc drift** — states `production/session-state/active.md` is gitignored but the file is tracked. Either update the doc or `git rm --cached` the file.
- **Branch reconciliation** — `main` vs `mymerge`. See branch note above.

## Session Extract — /dev-story 2026-08-19 (Story 007)
- Story: production/epics/pull-wave/story-007-pause-flush.md — Pause-Flush (Queued-to-Next-Tick) + bPauseFlushPending Gate
- Files changed:
  - Source/SLIPSTORM/PullWave/PullWaveSubsystemActor.h (EDITED — added ERunState.h include; extended IPullWaveRSMProvider with GetCurrentState(); added OnRSMPausedChanged() public seam; added EndPlay() protected override; added bPauseFlushPending + FDelegateHandle PausedChangedHandle private fields)
  - Source/SLIPSTORM/PullWave/PullWaveSubsystemActor.cpp (EDITED — pause-flush top-of-tick batch in Tick() body replacing placeholder comment; OnRSMPausedChanged() implementation; EndPlay() implementation with PausedChangedHandle.Reset())
  - Source/SLIPSTORM/Tests/Integration/PullWave/PullWavePauseFlushTest.cpp (CREATED — 8 test cases: TC1 MidTickDeferral, TC2 RunningStateFlush, TC3 Rule19AbortedSkip, TC4 SpawnedNotFlushed, TC5 DespawningNotReflushed, TC6 WaveIdAscFlushOrder, TC7 HandlerDoublecallIdempotent, TC8 PauseFreezeAndFlushIndependent)
- Design decisions:
  - OnRSMPausedChanged() is public (test-callable seam); tests call directly — no real RSM delegate in test context
  - bPauseFlushPending cleared at flush consumption (first thing in if-block) regardless of RUNNING check
  - Range-for used in flush batch (safe: TransitionTo() changes State only; no RemoveAt fires there)
  - FDelegateHandle.Reset() in EndPlay (production delegate cleanup; no-op in tests)
  - ERunState::RUNNING from RunStateMachine/ERunState.h (already exists with pinned ordinals)
- Blockers: None
- Next: /code-review Source/SLIPSTORM/PullWave/PullWaveSubsystemActor.h Source/SLIPSTORM/PullWave/PullWaveSubsystemActor.cpp Source/SLIPSTORM/Tests/Integration/PullWave/PullWavePauseFlushTest.cpp then /story-done production/epics/pull-wave/story-007-pause-flush.md

## Session Extract — /story-done 2026-08-19
- Verdict: COMPLETE WITH NOTES
- Story: production/epics/pull-wave/story-007-pause-flush.md — Pause-Flush (Queued-to-Next-Tick) + bPauseFlushPending Gate
- Tech debt logged: None (2 advisory deviations documented in story completion notes; not added to tech-debt register)
- Next recommended: Story 008 — Run-Termination Drain — production/epics/pull-wave/story-008-run-termination-drain.md

## Session Extract — /dev-story 2026-08-19 (Story 008)
- Story: production/epics/pull-wave/story-008-run-termination-drain.md — Run-Termination Drain Semantics
- Files changed:
  - Source/SLIPSTORM/PullWave/PullWaveSubsystemActor.h (EDITED — added OnRSMRunStateChanged(ERunState) public seam; bRunTerminated + RunStateChangedHandle private fields; updated story/TR comment header)
  - Source/SLIPSTORM/PullWave/PullWaveSubsystemActor.cpp (EDITED — bRunTerminated=false reset in InitializePool; OnRSMRunStateChanged() implementation; AdvanceLanded PendingDespawnReason now bRunTerminated?RunTermination:NaturalLanding; EndPlay updated to also Reset() RunStateChangedHandle with combined TODO)
  - Source/SLIPSTORM/Tests/Integration/PullWave/PullWaveRunTerminationDrainTest.cpp (CREATED — 8 TCs)
- Design decisions:
  - OnRSMRunStateChanged() is public test-callable seam (consistent with Story 007 pattern)
  - bRunTerminated reset at InitializePool() (not just BeginPlay) so tests using InitializePool() directly get clean state
  - AdvanceLanded is the ONLY callsite that needs bRunTerminated (DESPAWNING via hold-expiry); PauseFlush sets PendingDespawnReason=PauseFlush directly and is mutually exclusive via Rule 19
  - DrainUntilEmpty() helper loops with DeltaTime=0.1f + max 100 iterations for reliable drain completion
  - Wave params: LeanDurationS=0.1f, ForwardVelocityMs=30.0f → TravelDurationS=0.5s; full drain from LEANING in ~8 ticks at 0.1f DeltaTime
- Blockers: None
- Next: /code-review Source/SLIPSTORM/PullWave/PullWaveSubsystemActor.h Source/SLIPSTORM/PullWave/PullWaveSubsystemActor.cpp Source/SLIPSTORM/Tests/Integration/PullWave/PullWaveRunTerminationDrainTest.cpp then /story-done production/epics/pull-wave/story-008-run-termination-drain.md

## Session Extract — /story-done 2026-08-19
- Verdict: COMPLETE WITH NOTES
- Story: production/epics/pull-wave/story-008-run-termination-drain.md — Run-Termination Drain Semantics
- Tech debt logged: None (3 advisory deviations documented in story completion notes; not added to tech-debt register)
- Code review: Complete — /code-review run this session; 1 blocking gap (TC9 NaturalLanding negative control) fixed before close
- Next recommended: Story 009 — Construct Entry Point — production/epics/pull-wave/story-009-construct-entry-point.md

## Session Extract — /dev-story + /code-review + /story-done 2026-08-19 (Story 009)
- Story: production/epics/pull-wave/story-009-construct-entry-point.md — Construct() Entry Point + Wave Spawner Integration
- Files changed:
  - Source/SLIPSTORM/PullWave/PullWaveSubsystemActor.h (EDITED — Construct() + ComputeSpawnTransform() declarations; OnISMCAddInstanceOverride + OnISMCConstructDataOverride seams; TransitionTo() doc updated to carve out Construct() exception; story/TR header updated)
  - Source/SLIPSTORM/PullWave/PullWaveSubsystemActor.cpp (EDITED — Construct() body 7-step implementation; ComputeSpawnTransform() implementation; header updated with Story 009 + ADR-0011 references)
  - Source/SLIPSTORM/Tests/Integration/PullWave/PullWaveConstructTest.cpp (CREATED — 9 TCs: TC1 F-TRAVERSE-DURATION, TC2 ParamsCopy, TC3 StateInit, TC4 ISMCSeam, TC5 PoolFullGuard, TC6 VelocityBinding, TC7 LeanDurationTrusted, TC8 AppendOrdering, TC9 OnlyAddSite)
- Code-review fixes applied before /story-done:
  - BLOCKING-1 (TC5): AddExpectedError("pull_wave_pool_full", Contains, 1) added before Construct(Overflow) — log assertion now wired
  - BLOCKING-2 (TC9): grep pattern corrected from ActiveWaves\.Add\s*( to ActiveWaves\.Add(_GetRef)?\s*( — now matches Add_GetRef call site
- 7 advisory deviations documented in Completion Notes (see story file)
- Blockers: None
- Next recommended: Check production/epics/pull-wave/ for remaining Ready stories in the epic

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

## Session Extract — /code-review + /story-done 2026-08-19 (Wave Spawner Story 001)
- Story: production/epics/wave-spawner/story-001-subsystem-class-and-object-pool.md — Subsystem Class, Object Pool, Three-Pool Structure
- Verdict: COMPLETE WITH NOTES
- Files created during /dev-story (this session):
  - Source/SLIPSTORM/DPC/DPCSubsystem.h (new stub — UDPCSubsystem + FDPCFrameState + FOnPostTickFrameStatePublished)
  - Source/SLIPSTORM/WaveSpawner/Wave.h (new — AWave lightweight state token, no ISMC)
  - Source/SLIPSTORM/WaveSpawner/Wave.cpp (new — companion impl, tick disabled)
  - Source/SLIPSTORM/WaveSpawner/WaveSpawnerTypes.h (new — EWaveSpawnerLifecycleState, FPatternDefinition, FPatternPool, EAdmissionResult, DECLARE_STATS_GROUP; include-ordering bug fixed during review)
  - Source/SLIPSTORM/WaveSpawner/WaveSpawnerSubsystem.h (new — UWaveSpawnerSubsystem full declaration)
  - Source/SLIPSTORM/WaveSpawner/WaveSpawnerSubsystem.cpp (new — full impl with VR-2 null-world guard)
  - Source/SLIPSTORM/Tests/Unit/WaveSpawner/WaveSpawnerPoolTest.cpp (new — 4 COMPLEX test commands)
- Code-review fix: WaveSpawnerTypes.h ordering — DECLARE_STATS_GROUP moved to after #pragma once + includes
- All 4 ACs checked: AC-WS-20a, AC-WS-20b, AC-WS-20c, AC-WS-10x
- Remaining clangd errors: false positives from missing UBT compile_commands.json (CoreMinimal.h not found). Run UBT -mode=GenerateClangDatabase to resolve.
- Next: Wave Spawner Story 002 — production/epics/wave-spawner/story-002-six-state-lifecycle.md

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

## Session Extract — /story-done 2026-08-19 (Wave Spawner Story 002)
- Verdict: COMPLETE WITH NOTES
- Story: production/epics/wave-spawner/story-002-six-state-lifecycle.md — Six-State Lifecycle, Phase Drain, and Atomic Pool Swap
- Files changed: WaveSpawnerTypes.h (ERunPhase added), WaveSpawnerSubsystem.h (GetTickableGameObjectWorld override, TransitionTo/GetActiveDrawPool/GetActivePhase/IsDrainWindowActive public API, TestOnly_SetLifecycleState/TestOnly_GetLifecycleState/TestOnly_IsValidTransition seams, private IsValidTransition/OnLifecycleTransition/CountInFlightWaves helpers), WaveSpawnerSubsystem.cpp (full lifecycle implementation), WaveSpawnerLifecycleTest.cpp (7 COMPLEX test commands)
- Test written: Source/SLIPSTORM/Tests/Unit/WaveSpawner/WaveSpawnerLifecycleTest.cpp (7 commands: ValidTransitions, ForbiddenTransitions, PoolSwapOpenerToMid, PoolSwapMidToPeak, InFlightImmutability, DrainWindowClearance, ColdResetOnTermination)
- Key decisions: ADR-0011 D3 authoritative (9 transitions supersede story ACs); TestOnly_IsValidTransition predicate seam for forbidden-transition testing (check(false) not catchable via AddExpectedError); UEnum::GetValueAsString (VR-9); GetTickableGameObjectWorld override (VR-7)
- Tech debt logged: None
- Next recommended: Wave Spawner Story 003 — Admission Gate + Cadence Gate — production/epics/wave-spawner/story-003-admission-gate-and-primer-bypass.md

## Session Extract — /story-done 2026-08-20
- Verdict: COMPLETE WITH NOTES
- Story: production/epics/wave-spawner/story-003-admission-gate-and-primer-bypass.md — Admission Gate + Cadence Gate (F-3b) + Primer Bypass
- Files changed: WaveSpawnerSubsystem.h (7 new TestOnly_* seams incl. TestOnly_SetResumeGrace, GetCurrentTimeS() declaration, 4 admission gate fields, test backing fields), WaveSpawnerSubsystem.cpp (OnDPCFrameReady full Rule 1 gate + primer bypass + F-3b cadence gate, GetCurrentTimeS(), bPrimerPending hook in OnLifecycleTransition(Active), 4 resets in Cold case), WaveSpawnerAdmissionGateTest.cpp (NEW — 8 TCs covering AC-WS-10/11/11b)
- Test written: Source/SLIPSTORM/Tests/Unit/WaveSpawner/WaveSpawnerAdmissionGateTest.cpp (8 commands: Gate.DPCInactiveVeto, Gate.LifecycleNotActiveVeto, Gate.ResumeGraceVeto, Gate.CadenceNotElapsed, Gate.CadenceElapsed, Gate.PrimerBypass, Gate.PostPrimerCadenceNormal, Gate.IntervalFromDPCSnapshot)
- Key decisions: OnDPCFrameReady (not Tick) per ADR-0011 D2; WaveSpawnIntervalS from FrameState (DPC snapshot) — correct per ADR, AC text reconciled; GetCurrentTimeS() test-injectable seam; TestOnly_SetResumeGrace bridges Story 007 RSM scope; pre-existing NewObject<>/GetTransientPackage() outer advisory
- Tech debt logged: None
- Next recommended: Wave Spawner Story 004 — Barrage Atomic Admission — production/epics/wave-spawner/story-004-barrage-atomic-admission.md

## Session Extract — /story-done 2026-08-20
- Verdict: COMPLETE WITH NOTES
- Story: production/epics/wave-spawner/story-004-barrage-atomic-admission.md — Barrage Atomic Admission + barrage_owed Reservation
- Tech debt logged: None (2 advisories documented in Completion Notes — bBarrageOwed Cold pre-clearance, kMaxConcurrentWavesStub stub)
- Code review: Complete — CHANGES REQUIRED verdict resolved (3 required + 5 suggestions applied)
- Test file: Source/SLIPSTORM/Tests/Unit/WaveSpawner/WaveSpawnerBarrageAdmissionTest.cpp (9 TCs: TC1–TC9 including TC8 rewrite + TC9 boundary test)
- Next recommended: Wave Spawner Story 005 — Pattern Draw + Cadence Governor (F-3) + RNG Seeding — production/epics/wave-spawner/story-005-pattern-draw-and-cadence-governor.md

## Session Extract — /story-done 2026-08-20
- Verdict: COMPLETE WITH NOTES
- Story: production/epics/wave-spawner/story-005-pattern-draw-and-cadence-governor.md — Pattern Draw + Cadence Governor (F-3) + RNG Seeding
- Tech debt logged: None (6 advisory deviations documented in Completion Notes; all by design — Story 007 scope)
- Code review: Deferred to sprint close-out (user selected "No — I'll run /code-review before sprint close-out")
- Wall-clock seed enforcement: manual grep only — `grep -rn "FDateTime|FMath::Rand\b|rand()|time(|FPlatformTime" Source/SLIPSTORM/WaveSpawner/WaveSpawnerSubsystem.cpp` must return 0 matches
- Next recommended: Story 006 — Despawn Pipeline + IWaveSpawnerCallback + Seam 13 (production/epics/wave-spawner/story-006-despawn-pipeline-and-seam-13.md)

## Session Extract — /dev-story 2026-08-20 (Wave Spawner Story 006)
- Story: production/epics/wave-spawner/story-006-despawn-pipeline-and-seam-13.md — Despawn Pipeline + IWaveSpawnerCallback + Seam 13
- Files modified:
  - Source/SLIPSTORM/SLIPSTORM.Build.cs (CppStandard = CppStandardVersion.Cpp20 added)
  - Source/SLIPSTORM/WaveSpawner/WaveSpawnerTypes.h (EWaveDespawnReason enum appended, None=0 sentinel)
  - Source/SLIPSTORM/WaveSpawner/WaveSpawnerSubsystem.h (SetDespawnCallback, DespawnWave public; IWaveSpawnerCallback* Callback private; include added)
  - Source/SLIPSTORM/WaveSpawner/WaveSpawnerSubsystem.cpp (DespawnWave() Rule 12 pipeline appended)
- Files created:
  - Source/SLIPSTORM/Seam/WaveSpawnerCallback.h (IWaveSpawnerCallback pure C++ interface)
  - Source/SLIPSTORM/Seam/WaveSpawnerCallbackTestStub.h (FDespawnEvent + FWaveSpawnerCallbackTestStub + C++20 static_assert, !UE_BUILD_SHIPPING guard)
  - Source/SLIPSTORM/Tests/Integration/WaveSpawner/WaveSpawnerDespawnPipelineTest.cpp (5 TCs: TC1-TC3 AC-WS-16 ×3 reasons, TC4 AC-WS-19, TC5 AC-WS-20)
- TestOnly_SetLiveCount already exists (Story 004); kTestWaveId=-1001 correct
- Blockers: None
- Next: /code-review then /story-done production/epics/wave-spawner/story-006-despawn-pipeline-and-seam-13.md

## Session Extract — /story-done 2026-08-20
- Verdict: COMPLETE WITH NOTES
- Story: production/epics/wave-spawner/story-006-despawn-pipeline-and-seam-13.md — Despawn Pipeline + IWaveSpawnerCallback + Seam 13
- Tech debt logged: None (2 advisory deviations documented in Completion Notes)
- Code review: Complete — /code-review passed before close
- Next recommended: Story 007 — RSM/DPC Integration — production/epics/wave-spawner/story-007-rsm-dpc-integration.md

## Session Extract — /dev-story 2026-08-20
- Story: production/epics/wave-spawner/story-007-rsm-dpc-integration.md — RSM/DPC Integration — Pause Flush, Run Termination, Snapshot Immutability
- Files modified:
  - Source/SLIPSTORM/RunStateMachine/RunStateMachineSubsystem.h (GetRunSeed() declaration added — cross-boundary stub, RSM epic TODO)
  - Source/SLIPSTORM/RunStateMachine/RunStateMachineSubsystem.cpp (GetRunSeed() stub body — returns 0)
  - Source/SLIPSTORM/WaveSpawner/WaveSpawnerTypes.h (FWaveInFlightState plain C++ struct added)
  - Source/SLIPSTORM/WaveSpawner/WaveSpawnerSubsystem.h (RSMSubsystem*, RSMPausedHandle, RSMStateHandle, RunSeed, InFlightWaves, kResumeGraceS; 4 method decls; 4 TestOnly seams)
  - Source/SLIPSTORM/WaveSpawner/WaveSpawnerSubsystem.cpp (RSM wiring Init/Deinit; OnLifecycleTransition RNG fix + InFlightWaves.Reset + PeakEntryTimeS; DespawnWave InFlightWaves.Remove; TryAdmitPattern snapshot capture ×3; HandlePausedChanged, HandleRunStateChanged, OnResumeFromPause, GetInFlightWaveIdsSorted implemented)
- Files created:
  - Source/SLIPSTORM/Tests/Integration/WaveSpawner/WaveSpawnerRSMDPCIntegrationTest.cpp (5 TCs: TC1-TC2 AC-WS-17, TC3 AC-WS-18, TC4 AC-WS-21+29, TC5 AC-WS-28)
- Deviations:
  - DEV-1: AC-WS-18 lifecycle → Cold (not Idle per AC text); Flushing→Idle is forbidden in ADR-0011 D3; story file updated
  - DEV-2: Rule 13 flush is synchronous (not deferred via bPauseFlushPending); may warrant follow-up
  - DEV-3: PeakEntryTimeS uses GetCurrentTimeS() seam (not GetWorld()->GetTimeSeconds())
- Blockers: Test evidence pending compile + headless run (UBT -nullrhi)
- Next: /code-review Source/SLIPSTORM/WaveSpawner/WaveSpawnerSubsystem.h Source/SLIPSTORM/WaveSpawner/WaveSpawnerSubsystem.cpp Source/SLIPSTORM/Tests/Integration/WaveSpawner/WaveSpawnerRSMDPCIntegrationTest.cpp production/epics/wave-spawner/story-007-rsm-dpc-integration.md then /story-done

## Session Extract — /code-review 2026-08-21
- Story: production/epics/wave-spawner/story-007-rsm-dpc-integration.md — RSM/DPC Integration
- Verdict: CHANGES REQUIRED → APPROVED (all fixes applied in same pass)
- BLOCKING fix (B-1): InFlightWaves orphan leak on pause flush — ScheduledSlots.Reset() was
  discarding scheduled wave IDs without removing their InFlightWaves entries; fixed by adding
  PurgeScheduledInFlightEntries() helper called before both ScheduledSlots.Reset() sites
  (pause flush and run termination).
- Files modified by code-review pass:
  - WaveSpawnerSubsystem.h: PurgeScheduledInFlightEntries() declaration; W-1 TransitionTo doc
    comment corrected (Active→Flushing now lists pause-flush trigger; Holding→Flushing now says
    OnPausedChanged(true)); W-2 kResumeGraceS TODO(RSM epic) cross-reference; W-3b ADR filename
    fixed (adr-0007-run-state-machine-hosting.md)
  - WaveSpawnerSubsystem.cpp: PurgeScheduledInFlightEntries() implementation; B-1 fix in both
    HandlePausedChanged + HandleRunStateChanged; W-1 DEVIATION NOTE added; W-2 TODO in
    OnResumeFromPause; W-3b ADR filename fixed
  - WaveSpawnerRSMDPCIntegrationTest.cpp: S1 TC3 ScheduledCount non-trivial arrange; S2 TC6/TC7
    COMPLETE+ABORTED branches; S3 TC8 HandlePausedChanged(false) causal path; S4 TC9 pause flush
    from Holding; W-3b ADR filename fixed; 9 total test commands (was 5)
  - story-007-rsm-dpc-integration.md: W-3a stale signatures fixed (OnStateChanged, 2-param/4-param);
    Test Evidence updated to 9 commands; Last Updated 2026-08-21
- Blockers: Test evidence pending compile + headless run (UBT -nullrhi)
- Next: /story-done production/epics/wave-spawner/story-007-rsm-dpc-integration.md

## Session Extract — /story-done 2026-08-21
- Verdict: COMPLETE WITH NOTES
- Story: production/epics/wave-spawner/story-007-rsm-dpc-integration.md — RSM/DPC Integration — Pause Flush, Run Termination, Snapshot Immutability
- Tech debt logged: 4 items → docs/tech-debt-register.md (created)
  1. AC-WS-18 lifecycle → Cold (ADR-0011 D3 correction)
  2. Rule 13 pause path Active→Flushing direct (no Holding hop)
  3. kResumeGraceS stub + TODO(RSM epic) IsResumeGrace() convergence
  4. GetRunSeed() cross-boundary stub (returns 0 until RSM epic)
- Next recommended: Story 008 — Cook-Time Validator (14 Binding Rule 15 Checks) — production/epics/wave-spawner/story-008-cook-time-validator.md

## Session Extract — /dev-story 2026-08-21 (Wave Spawner Story 008)
- Story: production/epics/wave-spawner/story-008-cook-time-validator.md — Cook-Time Validator (14 Binding Rule 15 Checks)
- Files changed:
  - Source/SLIPSTORM/WaveSpawner/WaveSpawnerTypes.h (EDITED — extended FPatternDefinition with 5 cook-time fields: SourceLanes, OnsetTimes, LeanMagnitudeTier, bIsPrimerEligible, bAllowsSlip)
  - Source/SLIPSTORM/WaveSpawner/WaveSpawnerCookTimeValidator.h (CREATED — UWaveSpawnerCookTimeValidator static class, 14 check declarations + utility helpers)
  - Source/SLIPSTORM/WaveSpawner/WaveSpawnerCookTimeValidator.cpp (CREATED — all 14 Rule 15 check implementations + ValidPeakTriplets static data)
  - Source/SLIPSTORM/Tests/Unit/WaveSpawner/WaveSpawnerCookTimeValidatorTest.cpp (CREATED — 15 test commands: TC1-TC14 fail-case per check + TC15 all-pass)
- Test written: Source/SLIPSTORM/Tests/Unit/WaveSpawner/WaveSpawnerCookTimeValidatorTest.cpp (15 commands)
- Design decisions:
  - FPatternDefinition extended in WaveSpawnerTypes.h (Story 001's stub; these fields were deferred to "Story 003+" per comment; Story 008 is the cook-time consumer so adding here is correct)
  - UWaveSpawnerCookTimeValidator is plain C++ static class (not UCLASS) — U prefix matches story spec; SLIPSTORM_API provides DLL export
  - Validate() runs all 14 checks without short-circuit (full error list always returned)
  - PEAK_SURVIVING_TRIPLETS and MIN_BARRAGE_PATTERN_COUNT_PER_TRIPLET are separate checks with distinct error IDs; both fire when a triplet is absent
  - TC14 (BARRAGE_DISTINCT_SOURCE_LANES) uses 8-pattern pool (7 distinct + 1 duplicate); 7 distinct triplets still present so PEAK_SURVIVING_TRIPLETS does NOT fire — isolates the check
  - kTelegraphWindowFloorS = 0.70f, kBarrageWSpanMaxS = 0.35f defined as private constexpr in validator header
- Blockers: Test evidence pending compile + headless run via UBT -nullrhi
- Next: /code-review Source/SLIPSTORM/WaveSpawner/WaveSpawnerCookTimeValidator.h Source/SLIPSTORM/WaveSpawner/WaveSpawnerCookTimeValidator.cpp Source/SLIPSTORM/Tests/Unit/WaveSpawner/WaveSpawnerCookTimeValidatorTest.cpp then /story-done production/epics/wave-spawner/story-008-cook-time-validator.md

## Session Extract — /story-done 2026-08-21
- Verdict: COMPLETE WITH NOTES
- Story: production/epics/wave-spawner/story-008-cook-time-validator.md — Cook-Time Validator (14 Binding Rule 15 Checks)
- Files changed: Source/SLIPSTORM/WaveSpawner/WaveSpawnerCookTimeValidator.h, Source/SLIPSTORM/WaveSpawner/WaveSpawnerCookTimeValidator.cpp, Source/SLIPSTORM/Tests/Unit/WaveSpawner/WaveSpawnerCookTimeValidatorTest.cpp, Source/SLIPSTORM/WaveSpawner/WaveSpawnerTypes.h (5 new fields), docs/architecture/adr-0011-wave-spawner-pattern-library.md (D4 amended), production/epics/wave-spawner/story-008-cook-time-validator.md (Status: Complete)
- Code review fixes applied: U→F rename (FWaveSpawnerCookTimeValidator), removed Algo/Sort.h include, removed redundant static from anon-namespace helper, fixed TC4 duplicate assertion → BARRAGE_UNIFORM_TIER check
- Advisory tech debt: GAP-1 through GAP-8 (pool-dispatch arm test gaps — NON_BARRAGE_STAGGER MID/PEAK, PILLAR_1_VERB_SLIP OPENER/PEAK, POOL_NON_EMPTY MID/PEAK, extra-triplet path, unsorted SourceLanes) — recommend follow-up in Story 009 scope
- Next: /dev-story production/epics/wave-spawner/story-009-telemetry-and-edge-case-defense.md

## Session Extract — /dev-story 2026-08-21 (Wave Spawner Story 009)
- Story: production/epics/wave-spawner/story-009-telemetry-and-edge-case-defense.md — Telemetry + Edge-Case Defense + Performance Baseline
- Files changed:
  - Source/SLIPSTORM/WaveSpawner/WaveSpawnerTypes.h (EDITED — added DECLARE_CYCLE_STAT_EXTERN for STAT_WaveSpawnerAdmissionTick + STAT_WaveSpawnerPoolAlloc)
  - Source/SLIPSTORM/WaveSpawner/WaveSpawnerSubsystem.h (EDITED +128 lines — Story 009 private fields, 4 helper declarations, extended WITH_DEV_AUTOMATION_TESTS block with FCapturedTelemetryEvent + 7 TestOnly_ methods)
  - Source/SLIPSTORM/WaveSpawner/WaveSpawnerSubsystem.cpp (EDITED +290 lines — DEFINE_STAT x2, Cold→Idle transition, rate-limit resets, SCOPE_CYCLE_COUNTERs, all 9 telemetry call sites, EmitTelemetry/ShouldEmitRateLimited/EmitPatternAdmitted/ValidateAndPrunePoolsAtLoad implementations)
  - docs/architecture/adr-0011-wave-spawner-pattern-library.md (EDITED +5 lines — D3 Cold→Idle edge added)
  - Source/SLIPSTORM/Tests/Integration/WaveSpawner/WaveSpawnerTelemetryTest.cpp (CREATED — 408 lines, 7 integration tests TC1-TC7)
- Deviations accepted:
  - AC-WS-24: no check(false) on pool_exhaustion_detected (Story 004 TC3 would abort; check belongs at AcquireFromPool() in Story 006)
  - AC-WS-23: peak_min_barrage_floor_undershoot fires before TransitionTo(Flushing) (Active→Idle/Flushing→Idle are forbidden per ADR-0011 D3)
  - AC-WS-27b Cold→Idle: Option A chosen — IsValidTransition amended + ADR D3 updated
  - Pattern pruning: limited to accessible fields (tier, SourceLanes via ValidPeakTriplets); span/stagger deferred (private constants)
  - TSet<FName> used for dedup (not FSoftObjectPath — no such field on FPatternDefinition)
- Blockers: Test evidence pending compile + headless run via UBT -nullrhi
- Next: /code-review Source/SLIPSTORM/WaveSpawner/WaveSpawnerSubsystem.h Source/SLIPSTORM/WaveSpawner/WaveSpawnerSubsystem.cpp Source/SLIPSTORM/Tests/Integration/WaveSpawner/WaveSpawnerTelemetryTest.cpp then /story-done production/epics/wave-spawner/story-009-telemetry-and-edge-case-defense.md

## Session Extract — /story-done 2026-08-21
- Verdict: COMPLETE WITH NOTES
- Story: production/epics/wave-spawner/story-009-telemetry-and-edge-case-defense.md — Story 009: Telemetry + Edge-Case Defense + Performance Baseline
- Tech debt logged: None (deviations documented in story completion notes)
- Post-review fixes applied: B-1 pruning fix, W-2 COMPLEX→SIMPLE macro swap, W-3 lifecycle guard, TC3/TC4 comment label corrections
- Next recommended: git commit for Stories 008+009, then branch reconciliation (mymerge vs main)

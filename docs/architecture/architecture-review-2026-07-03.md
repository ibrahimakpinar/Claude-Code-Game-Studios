# Architecture Review — 2026-07-03

**Mode**: `/architecture-review` (full — Phases 1–9; delta against 2026-06-26 baseline)
**Engine**: Unreal Engine 5.7 (pinned 2026-02-13 via `docs/engine-reference/unreal/VERSION.md`)
**GDDs Reviewed**: 11 — `game-concept`, `systems-index`, `input-system`, `run-state-machine`, `difficulty-phase-controller`, `pull-wave-behavior`, `wave-spawner-pattern-library`, `player-movement` (redirect stub — 116 lines post-decomposition Step 6), `player-movement-mechanics`, `player-movement-presentation`, `player-movement-platform` (PM sub-GDDs post decomposition Step 2 PASS 8 CLOSED 2026-06-29)
**ADRs Reviewed**: 9 — ADR-0001 through ADR-0009
**Prior baseline**: `architecture-review-2026-06-26.md` (CONCERNS; 90/207 covered = 43%; INT-004/005/006 open)
**TR Registry**: `docs/architecture/tr-registry.yaml` (unchanged this pass — 35 TR-PM rows routed to sub-GDDs at Step 8b CLOSED 2026-07-02)
**Architecture registry**: `docs/registry/architecture.yaml` (v7→v8 this session — ADR-0009 added 2 forbidden patterns)

---

## Verdict: **CONCERNS**

Major progress since 2026-06-26 baseline. All three surgical amendments open at prior review are now CLOSED (INT-004 ADR-0005 InitializeDependency inscribed; INT-005 ADR-0007 Tick guard; INT-006 ADR-0007 salt-parse `FCString::Strtoui64` switch). ADR-0007 promoted Proposed → Accepted 2026-06-26; ADR-0008 promoted → Accepted 2026-06-27. **ADR-0009 (Player Movement Component Hosting) authored today, Proposed** — closes the 35-TR PM gap flagged in the 2026-06-26 review as the largest remaining Core-layer gap. Coverage moved from 43% to **~60%** (125/207).

**One new item, one carry-over item, both surgical**:

- **🟠 INT-007 (NEW; documentation-quality, not architecturally blocking)** — ADR-0009's Decision Makers + Depends On fields cited ADR-0002 as "Accepted" but ADR-0002 is `Proposed` (interface INT-002-amended and stable, but status gate is Polish-phase Hardware Verification). Amended in-session — both occurrences corrected to accurate `Proposed` framing with the pragmatic-promotion rationale documented in ADR-0009's Depends On note. VERIFIED CLOSED THIS PASS.
- **🟡 INT-003 (carry-over)** — `INPUT_TICK_RATE = 60Hz` naming nuance in ADR-0003. Documentation-quality only. Deferred to next ADR-0003 amendment.

**No cross-ADR conflicts.** **No GDD revision flags.** **No deprecated API references.** **No dependency cycles.** ADR-0009 SD4 correctly mirrors ADR-0008's `RSM->ForceTickNow()` first-statement pattern for DPC; SD6 correctly mirrors ADR-0008 SD2's curve validation gate; SD3 stationary-player is internally consistent with mechanics OQ-2 recommendation-path Option (c).

Pre-Production gate **STILL NOT CLEAR** — the four pre-gate infrastructure items from 2026-06-25/26 are unchanged (no `/test-setup`, no `/ux-design`).

---

## Delta vs 2026-06-26

| Change | Status |
|---|---|
| **ADR-0007 promoted Proposed → Accepted** (2026-06-26 same day; INT-005 + INT-006 amendments landed) | CLOSED |
| **ADR-0008 promoted Proposed → Accepted** (2026-06-27; ADR-0007 dep gate satisfied) | CLOSED |
| **ADR-0005 amended INT-004** (`Collection.InitializeDependency(URunStateMachineSubsystem::StaticClass())` + `Collection.InitializeDependency(UDPCSubsystem::StaticClass())` calls inscribed in `Initialize()` skeleton) | CLOSED |
| **ADR-0009 authored** (Player Movement Component Hosting, Forward-Motion Model, Tween Implementation) — Proposed 2026-07-03 (this session) | NEW |
| **PM decomposition Steps 1–9 CLOSED** (2026-07-02) — 3 sub-GDDs authoritative; monolith → 116-line redirect stub; 4 R12a author briefs authored | CLOSED |
| **Registry v6→v7→v8** — v7 was INT-006 amendment (no new patterns); v8 is this session's ADR-0009 additions (2 new forbidden patterns: `PlayerMovement_SetActorRotation_for_lean` + `PlayerMovement_TickComponent_without_prior_ForceTickNow`) | LANDED |
| **Seam 12 naming reconciliation** (`platform-seam-interfaces.md` lines 1408 + 1431 — `UPlayerMovementComponent*` → `UPlayerLaneMovementComponent*` matching ADR-0009 SD1) | LANDED THIS SESSION |
| **Platform sub-GDD §3 SUPERSEDED marker** (`player-movement-platform.md` §3 Tick Ordering lines 57–64 — pre-ADR-0007 (a)/(b) enumeration marked SUPERSEDED by ADR-0009 SD4 canonical (c) pull-primitive pattern) | LANDED THIS SESSION |
| **INT-007 amendment to ADR-0009** — 2 occurrences of ADR-0002 status corrected from "Accepted" → "Proposed; INT-002-amended interface-stable" with pragmatic-promotion rationale | LANDED THIS SESSION |

ADR-0001, ADR-0003, ADR-0004 unchanged (all Proposed).

---

## Resolved Since 2026-06-26

### ✅ INT-004 — ADR-0005 missing `Collection.InitializeDependency()` (was BLOCKING)

**Resolved by**: ADR-0005 amendment 2026-06-26 (per active.md prior narrative). Both `InitializeDependency(URunStateMachineSubsystem)` and `InitializeDependency(UDPCSubsystem)` calls inscribed in the `Initialize()` skeleton before the `GetSubsystem<>()` bind paths. ADR-0007 IG-3 + ADR-0008 IG-3 language upgraded from "SHOULD be amended" to "MUST be amended — correctness gate". Wave Spawner enter-implementation gate unblocked.

### ✅ INT-005 — ADR-0007 Tick() body code/prose contradiction (was BLOCKING)

**Resolved by**: ADR-0007 amendment 2026-06-26 (verified via `grep -n INT-005 adr-0007-run-state-machine-hosting.md` this pass). Tick() body skeleton (lines 418–425) now inserts `if (bHasTickedThisFrame) { return; }` guard as FIRST STATEMENT before the `bHasTickedThisFrame = true` set — mirroring `ForceTickNow()`'s pattern at lines 431–434. Risk 4 mitigation row + Implementation Guideline 5 + Validation Criteria item + architecture diagram + Key Interfaces flag comment all rephrased "set as FIRST STATEMENT" → "guarded then set". Registry v6 added `RunStateMachine_Tick_set_bHasTickedThisFrame_without_prior_guard` forbidden pattern. Symmetric with `PlayerMovement_TickComponent_without_prior_ForceTickNow` (v8, added this session).

### ✅ INT-006 — ADR-0007 `FParse::HexNumber64` signature (was REQUIRES SOURCE VERIFY)

**Resolved by**: ADR-0007 amendment 2026-06-26 (verified via `grep -n INT-006 adr-0007-run-state-machine-hosting.md` this pass; primary source `/Users/Shared/Epic Games/UE_5.7/Engine/Source/Runtime/Core/Public/Misc/Parse.h` line 361). UE 5.7 signature confirmed `static CORE_API uint64 HexNumber64(FStringView HexString)` — incompatible with the original ADR call site AND incompatible with the LLM-window 5.3 `(const TCHAR*, TCHAR**)` form. Salt-parse switched to `FCString::Strtoui64(*SaltHex, nullptr, 16)` — signature verified stable across UE 5.0–5.7 per `/Users/Shared/Epic Games/UE_5.7/Engine/Source/Runtime/Core/Public/Misc/CString.h:549`. Structural Decision 4 prose at line 114 + registry v7 api: string updated in parallel. ADR-0007 fully unblocked for Proposed → Accepted (which promoted same day).

---

## Open Integration Items

### 🟡 INT-003 (carry-over) — ADR-0003 `INPUT_TICK_RATE = 60Hz` naming nuance

Unchanged from 2026-06-25/26. Documentation-quality only; F-4's `drain_tick_index` integer comparison remains correct regardless of frame rate. Defer to next ADR-0003 amendment (candidate rename: `INPUT_TICK_RATE_TARGET_HZ`).

### 🟠 INT-007 (NEW; RESOLVED THIS PASS) — ADR-0009 factual-status claim about ADR-0002

**ADRs involved**: ADR-0009 (Proposed, 2026-07-03), ADR-0002 (Proposed, INT-002-amended 2026-06-26).

**Claim**: ADR-0009 Decision Makers list line 21 asserted "ADR-0002 (Accepted 2026-06-11)" and Depends On field line 47 asserted "ADR-0002 (Accepted)". Verification against `adr-0002-haptic-platform-bridge.md` line 4 shows the ADR-0002 Status is `Proposed`. ADR-0002 is INT-002-amended and interface-stable (its `EHapticEvent` vocabulary + `IHapticDispatch` surface + iOS/Android platform patterns are all codified), but its Accepted-status gate is the Hardware Verification Gate (per ADR-0002 lines 302 + 329 — physical iPhone 16 + Galaxy S24 NearMiss FULL/DURATION distinctness pass) which is deferred to Polish phase.

**Why it's not architecturally blocking**: ADR-0002's public interface consumed by PM (`IHapticDispatch::Fire(EHapticEvent)` + `IsSystemHapticsEnabled()`) is INT-002-amended and stable. PM's consumption (ADR-0009 SD5-adjacent haptic dispatch at TriggerNearMissBeat + SETTLED→SLIPPING trigger site + buffer-drop trigger site) will compile against the interface regardless of ADR-0002's status label. The precedent from 2026-06-24/26 established that ADR-0005 + ADR-0006 were Accepted before ADR-0007 despite having forward contracts on RSM.

**Why it must still be resolved**: ADR-0009's factual claim was wrong. Future readers may be misled about the actual Depends On chain. Also, the project convention emerging from ADR-0008 (which explicitly stated "must be Accepted before this ADR" for ADR-0007 and delayed its own Acceptance by 1 day) suggests that promoting ADR-0009 while ADR-0002 remains Proposed is a live architectural question, not settled.

**Resolution options** (evaluated pre-resolution):
- **(a) Amend ADR-0009 (Recommended)** — correct 2 occurrences of "ADR-0002 (Accepted)" to "ADR-0002 (Proposed; INT-002-amended interface-stable; HW gate deferred to Polish)" with pragmatic-promotion rationale documented in-place. ADR-0009 can promote → Accepted independently since the interface is stable.
- **(b) Promote ADR-0002 → Accepted first** — requires cascading ADR-0001 promotion (ADR-0002 depends on ADR-0001) which requires the HW verification gate. Not a Pre-Production-phase action.
- **(c) Accept the mismatch until first PM story enters implementation** — kicks the resolution to `/dev-story` where SD-parameter binding will surface the Proposed status.

**Resolution landed this session (option a)**:
- ADR-0009 Decision Makers line 21: "ADR-0002 (Accepted 2026-06-11)" → "ADR-0002 (Proposed 2026-05-14; INT-002-amended 2026-06-26) ... The public interface consumed by PM is INT-002-amended and stable; PM's implementation compiles against the interface regardless of ADR-0002's status label. See ADR-0009 Depends On note below and architecture-review-2026-07-03.md INT-007 for the pragmatic-promotion rationale."
- ADR-0009 Depends On field: "ADR-0002 (Accepted)" → "ADR-0002 (Proposed; interface INT-002-amended 2026-06-26 — status gate is Polish-phase Hardware Verification per ADR-0002 lines 302 + 329, not a Pre-Production blocker) — ... The interface consumed by PM is codified + INT-002-stable; ADR-0009 promotion to Accepted is not gated on ADR-0002's Accepted status per the ADR-0005/0006-Accepted-before-ADR-0007 precedent from 2026-06-24/26 ...".

INT-007 is now VERIFIED CLOSED. ADR-0009 is unblocked for Proposed → Accepted promotion pending a follow-up review pass to gate-check.

---

## Newly Closed Coverage Gaps

| Change | TRs | ADR |
|---|---|---|
| TR-PM-001 through TR-PM-035 | 35 | ✅ COVERED by ADR-0009 (all 35 mapped 1:1 in ADR-0009 GDD Requirements Addressed table; 21 mechanics + 5 presentation + 9 platform per Step 8b tr-registry routing) |

**Net traceability movement:**

| Metric | 2026-06-26 | 2026-07-03 | Δ |
|---|---|---|---|
| Total TRs | 207 | 207 | 0 |
| ✅ Covered | 90 (43%) | **~125 (60%)** | **+35** |
| ⚠️ Partial | ~29 (14%) | ~29 (14%) | 0 |
| ❌ Gap | ~88 (43%) | **~53 (26%)** | **−35** |

| System | TRs | ADRs covering | 2026-06-26 status | 2026-07-03 status |
|---|---|---|---|---|
| Input System | 50 | ADR-0001, 0002, 0003 | ✅ Covered | ✅ Covered (unchanged) |
| Run State Machine | 34 | ADR-0007 | ✅ Covered | ✅ Covered (unchanged) |
| **Player Movement** | **35** | **ADR-0002 (haptic events) + ADR-0009** | ❌ Gap (1 covered via INT-002) | **✅ Covered (35/35)** |
| Difficulty & Phase Controller | 24 | ADR-0008 | ✅ Covered | ✅ Covered (unchanged) |
| Pull-Wave Behavior | 29 | ADR-0006 | ⚠️ Partial | ⚠️ Partial (renderer + topology covered; state machine/pool/despawn still ADR-0010) |
| Wave Spawner & Pattern Library | 35 | ADR-0005 | ⚠️ Partial | ⚠️ Partial (hosting covered; pattern library + cadence governor still ADR-0011) |

No registry TR-ID additions this pass — ADR-0009's 35-row GDD Requirements Addressed table references TR-PM-001..035 which were registered at Step 8b (2026-07-02). Existing TR-IDs unchanged; sub-GDD routing (mechanics 21 / presentation 5 / platform 9) verified consistent with ADR-0009 table.

---

## Cross-ADR Conflicts

**None.** Focused checks for ADR-0009:

### 🔒 Tick-ordering pattern (RSM ↔ DPC ↔ PM triple consumption) — PASS

ADR-0007 SD2 published `URunStateMachineSubsystem::ForceTickNow()` as a public pull primitive with `bHasTickedThisFrame` idempotence guard (INT-005-amended 2026-06-26). ADR-0008 SD1 established the `UDPCController::Tick()` pattern that calls `RSM->ForceTickNow()` as first statement after `SCOPE_CYCLE_COUNTER` + `check(IsInGameThread())`. ADR-0009 SD4 mirrors DPC's pattern for `UPlayerLaneMovementComponent::TickComponent(DeltaTime)` — first statement after `check(IsInGameThread())` is `RSMSubsystem->ForceTickNow()`.

Triple-consumer safety: RSM's `bHasTickedThisFrame` guard handles all six pairwise orderings of RSM regular Tick + DPC Tick + PM Tick; whichever runs first invokes the RSM body once; the other two short-circuit. Registry v8's new `PlayerMovement_TickComponent_without_prior_ForceTickNow` forbidden pattern enforces PM's participation at code-review time (symmetric with v6's `RunStateMachine_Tick_set_bHasTickedThisFrame_without_prior_guard`).

Non-callback invariant preserved: ADR-0009 Implementation Guideline 2 codifies that PM's TickComponent MUST NOT rely on RSM subscriber broadcast firing synchronously inside `ForceTickNow()` — the 1-frame-latency semantic is documented; PM uses direct accessors for zero-latency gating.

### 🔒 Curve validation pattern (DPC + PM) — PASS

ADR-0008 SD2 established `TObjectPtr<UCurveFloat>` UPROPERTY hard reference + `UDPCSubsystem::Initialize()` `ValidateCurveAsset` gate (null check + Keys.Num() >= 2 + [0.0, 1.0] span). ADR-0009 SD6 mirrors the pattern at `UPlayerLaneMovementComponent::BeginPlay()` for 3 curves (SlipCurve / LeanCurve / EdgeAbsorbCurve). Both use log Error + `bCurveFallbackActive`-style flag on failure. Pattern is now 2-consumer-consistent; any future ADR adding curve validation should follow the same shape.

### 🔒 ADR-0009 SD5 vs ADR-0002 IHapticDispatch surface — PASS

ADR-0009 SD5-adjacent dispatch sites (TriggerNearMissBeat, SETTLED→SLIPPING commit-tell, buffer-drop) fire `EHapticEvent::NearMiss` (INT-002-added), `EHapticEvent::SlipConfirmed`, and `EHapticEvent::BufferDrop` — all 3 events codified in ADR-0002. `IHapticDispatch::IsSystemHapticsEnabled()` gate is honored per Implementation Guideline pattern (deferred to first-PM-story implementation; ADR-0009 documents the call surface).

### 🔒 ADR-0009 SD1 (UPlayerLaneMovementComponent) vs Seam 12 (`FPlayerMovementProvider_Production` constructor) — PASS (post-hygiene-edit-this-session)

`platform-seam-interfaces.md` lines 1408 + 1431 previously used `UPlayerMovementComponent*` — mismatched with ADR-0009 SD1's chosen class name `UPlayerLaneMovementComponent`. Hygiene edit landed this session via `replace_all`; both occurrences now `UPlayerLaneMovementComponent`. The Seam 12 production wrapper constructor is compile-callable against ADR-0009's class.

### 🔒 ADR-0009 SD4 vs platform sub-GDD §3 Tick Ordering — PASS (post-hygiene-edit-this-session)

`player-movement-platform.md` §3 Tick Ordering lines 57–64 previously enumerated only pre-ADR-0007 options (a) one-frame-lag + (b) manual PM->TickComponent from RSM — both of which pre-date the `ForceTickNow()` publication. Hygiene edit this session inserted a "⚠ SUPERSEDED by ADR-0009 SD4 (2026-07-03)" marker above the enumeration with full cross-reference to ADR-0009 SD4 + Implementation Guideline 1 + Registry v8 forbidden pattern; the (a)/(b) enumeration is preserved verbatim for revision-history traceability with a `**do not adopt (a) or (b) for a new implementation**` directive. Platform AC-SS-C row post-note added noting the AC becomes a defensive-no-op under the canonical (c) resolution.

### 🔒 EDPCAbortReason ownership + PlayerMovement forbidden patterns — PASS

Registry v8's 2 new forbidden patterns:
- `PlayerMovement_SetActorRotation_for_lean` — parallel to mechanics §3 Cross-Component Interfaces > Rotation Implementation prose; enforces the Rule 2 collision-commitment contract (root rotation breaks the lane-X invariant since collision geometry follows root).
- `PlayerMovement_TickComponent_without_prior_ForceTickNow` — symmetric with RSM v6 pattern; enforces the SD4 canonical (c) pattern at code-review time; cross-references INT-005 idempotence guard for the safety proof.

Neither conflicts with prior 27 patterns. Cross-consistency: v6 RSM Tick guard pattern + v8 PM TickComponent guard pattern operate on the same `bHasTickedThisFrame` idempotence primitive — reader may want to grep both when reading either.

---

## ADR Dependency Order (updated)

| Layer | ADR | Title | Status | Depends On | Notes |
|---|---|---|---|---|---|
| Methodology | ADR-0004 | Dual-Grep Methodology | Proposed | — | Applied through PM decomposition workflow |
| Foundation | ADR-0001 | Palm Rejection R_max + Touch Radius Bridge | Proposed | — | Awaits HW gate (iPhone 16 + Galaxy S24) |
| Foundation | ADR-0002 | Haptic Platform Bridge (INT-002-amended) | Proposed | ADR-0001 | Interface INT-002-stable; awaits HW gate |
| Foundation | ADR-0003 | 60Hz Drain Queue + FInputSystem | Proposed | ADR-0001 | Foundation-chain-blocked |
| Foundation | ADR-0007 | RSM Hosting + Sleep-Aware Time Source | **Accepted** ✅ | — | Promoted 2026-06-26 |
| Foundation | ADR-0005 | Wave Spawner Subsystem Hosting (INT-004-amended) | Accepted | — | Amended 2026-06-26 |
| Foundation | ADR-0006 | Pull-Wave Instanced Renderer (INT-001-amended) | Accepted | — (sibling of 0005) | Unchanged since 2026-06-24 |
| Core | ADR-0008 | DPC Hosting + FDPCFrameState | **Accepted** ✅ | ADR-0007 ✅ | Promoted 2026-06-27 |
| Feature | **ADR-0009** | **Player Movement Component Hosting** | **Proposed** (INT-007 amended) | ADR-0007 ✅ + ADR-0002 (Proposed; interface-stable) | Authored 2026-07-03; ready for Proposed → Accepted gate |

**No cycles. No unresolved dependencies.** ADR-0009 Depends On chain resolves as: ADR-0007 = Accepted ✅; ADR-0002 = Proposed but interface-stable per INT-002 amendment; pragmatic-promotion path documented per INT-007 amendment. **Next promotion candidate: ADR-0009.**

**Forward-ordering observation** (carry-over from 2026-06-26): Foundation-layer Proposed ADRs (0001, 0002, 0003, 0004) remain blocked on the HW verification gate at ADR-0001. This is expected (physical device access is a Polish-phase item), but the emerging pattern of Feature-layer ADRs depending on Proposed Foundation ADRs (ADR-0009 → ADR-0002) will replicate as ADR-0010 (Pull-Wave lifecycle) and ADR-0011 (Wave Spawner pattern library) are authored — each may need to justify pragmatic promotion under similar reasoning.

---

## GDD Revision Flags

**None.** No GDD assumption is contradicted by ADR-0009's verified engine behavior. Two tracked items:

- **Platform §3 Tick Ordering enumeration** (`player-movement-platform.md` lines 57–64) — the (a)/(b) options are pre-ADR-0007 and superseded by ADR-0009 SD4's canonical (c) pattern. Hygiene edit this session landed a SUPERSEDED marker in-place; no GDD revision (in the "Needs Revision" sense) required.
- **PM sub-GDD R12 fresh-context reviews** — remain gated on R12a author revision passes per PM decomposition Step 9 CLOSED entry; independent of this architecture-review's scope. Not GDD revisions in the design-flag sense; scheduled workflow items.

No `systems-index.md` updates required this pass. ADR-0009 status transitions (Proposed → Accepted, when landed) may warrant a small row-3a/3b/3c hosting-ADR annotation, but that's a post-Accepted hygiene item.

---

## Engine Compatibility Audit

### Audit Baseline (cross-ADR sweep)

| Check | Result |
|---|---|
| ADRs with Engine Compatibility section | 8/9 ✅ (ADR-0004 is methodology — N/A) |
| Engine version consistency (UE 5.7) | 9/9 ✅ |
| Post-Cutoff APIs Used field populated | 8/9 ✅ (ADR-0004 N/A) |
| References Consulted field populated | 9/9 ✅ |
| Deprecated API references in ADRs | 0 ✅ |
| Stale engine-version references | 0 ✅ |
| Stale platform-min claims | 0 ✅ |
| `TSharedPtr<T>` mis-used for UObject | 0 ✅ |

### ADR-0009 Specific — Engine Compatibility

10 Post-Cutoff APIs Used listed; all stable pre-cutoff or 5.0+ (`UActorComponent`, `APawn`, `SetActorLocation`, `SetRelativeLocation`, `TObjectPtr<T>`, `UCurveFloat`, `FRichCurve`, non-dynamic multicast delegates, `AddUObject`, `IsValid`). No 5.4/5.5/5.6/5.7-specific APIs invoked — the ADR deliberately avoids `SafeMoveUpdatedComponent` (SD1 rejection prose) which had sweep-resolution behavior variance across 5.4–5.7 per breaking-changes.md.

6 Verification Required items surface implementation-time gates:
1. UActorComponent TickComponent tick group on UE 5.7 mobile forward renderer
2. Hard-referenced UPROPERTY UCurveFloat availability at BeginPlay (mirror of ADR-0008 Verification #1)
3. SetActorLocation(bSweep=false) constant-time on APawn with no physics body
4. SetRelativeLocation composition with parent SetActorLocation
5. TickComponent → ForceTickNow re-entrancy hazard (already gated by INT-005 idempotence)
6. GetOwner() returns pawn reliably at BeginPlay

### Engine Specialist Consultation

Skipped for this delta review — ADR-0009's Structural Decisions are 1:1 mirrors of ADR-0007 SD2 (tick primitive) and ADR-0008 SD2 (curve validation) which received full unreal-specialist validation in the 2026-06-26 pass. Novel choices in ADR-0009 (SD1 UActorComponent choice, SD2 APawn choice, SD5 dual-transform-write pattern) are engine-idiomatic and covered by the 6 Verification Required items rather than by unknown-territory investigation.

If concerns about `SetActorLocation(bSweep=false)` sweep semantics or the SetRelativeLocation + SetActorLocation composition surface during first-PM-story implementation, spawn `unreal-specialist` for those specific Verifications (#3, #4). Not a review-time gate.

---

## Architecture Document Coverage

`docs/architecture/architecture.md` still does not exist. Architecture remains distributed across 9 ADRs + `platform-seam-interfaces.md` + `visual-dispatch-contract.md` + 5 change-impact docs. `/create-architecture` remains the conventional Pre-Production-gate artifact.

**No orphaned architecture.** Every architectural artifact maps to at least one GDD or system in `systems-index.md`.

---

## Required ADR Amendments

### Already-resolved (this pass)

1. **✅ ADR-0009 amendment (INT-007)** — 2 factual-status claims corrected. VERIFIED CLOSED THIS PASS.

### Required ADRs (carry-over from 2026-06-26; unchanged priorities)

2. **🟡 ADR-0010 — Pull-Wave Object Pool, State Machine, and Despawn Pipeline** (Feature) — 5-state lifecycle + pool-size derivation + Seam 13 promotion. Est. TR closure: partial coverage of the ~29 Pull-Wave TRs currently ⚠️ Partial. Engine Risk: MEDIUM.
3. **🟡 ADR-0011 — Wave Spawner Pattern Library Data Model and Cadence Governor** (Feature) — pattern library data ownership + F-3 platform-determinism + RunSeed propagation from RSM. Est. TR closure: remaining ~29 Wave Spawner TRs currently ⚠️ Partial. Engine Risk: LOW.
4. **🟢 ADR-0012 — Telegraph System** (Hazard) — deferred until Telegraph GDD lands.

---

## Pre-Gate Checklist (Phase 9)

Unchanged from 2026-06-25/26. Cannot run `/gate-check pre-production` until all four items resolve:

- ❌ `tests/unit/` + `tests/integration/` MISSING → run `/test-setup`
- ❌ `.github/workflows/` MISSING → run `/test-setup`
- ❌ `design/ux/accessibility-requirements.md` MISSING → run `/ux-design`
- ❌ `design/ux/interaction-patterns.md` MISSING → run `/ux-design`

`design/ux/` currently contains only `input-feedback.md`.

---

## Out of Scope

- **Stories (Phase 3b RTM)** — `production/epics/` does not exist; no stories yet. Phase 3b automatically skipped.
- **Consistency-failure logging** — `docs/consistency-failures.md` does not exist; not created by this skill.
- **Systems #8–17 (Collision through Async Leaderboard)** — no GDDs yet.
- **PM sub-GDD R12 fresh-context reviews** — scheduled workflow item per PM decomposition Step 9 CLOSED entry; gated on R12a author revision passes; not an architecture-review concern.
- **active.md compaction** — advisor-flagged as "accumulated-cost item the next /clear should tackle before /architecture-review" but not blocking of this review's execution; compaction is a session-hygiene concern, not an architecture-review Phase.

---

## Recommended Next Actions

Priority order:

1. **Promote ADR-0009 Proposed → Accepted** — INT-007 resolution landed; the interface-dependency-only rationale is documented in the amended Depends On field. No further pre-Acceptance amendments identified this pass.
2. **Author ADR-0010 (Pull-Wave lifecycle)** — highest-impact remaining Core-layer gap. Est. ~15–20 TR closures.
3. **Author ADR-0011 (Wave Spawner pattern library)** — Feature-layer closure. Est. ~15–20 TR closures.
4. **Run `/test-setup` and `/ux-design`** in parallel — Pre-Production gate infrastructure.
5. **Consider active.md compaction** before the next architecture-review — session-start-hook preview-ordering anomaly has recurred across 5 sessions.
6. **Re-run `/architecture-review`** after each new ADR is written OR after ADR-0009 promotes to Accepted to verify coverage improves + validate the cross-ADR integration surface.

---

## Reflexion Pattern Signal

Same-shape observation as the 2026-06-26 Reflexion note: ADR-0009 surfaced a factual-status defect (INT-007) that a strict "Depends On must be Accepted" rule would treat as blocking, but the pragmatic reading (interface-INT-002-amended + ADR-0005/0006-Accepted-before-ADR-0007 precedent) treats it as a documentation quality fix. The pattern generalizes: as more Feature/Core-layer ADRs land while Foundation-layer ADRs remain Proposed-behind-HW-gate, each new ADR will face the same "cite as Accepted or Proposed" question in its Depends On field. Recommended discipline for future ADR authors: always cite dependency ADRs by their actual Status label (never assume Accepted for readability), and document the pragmatic-promotion path in the Depends On field itself so future readers don't need to re-derive the rationale.

Also: this pass introduced 2 new forbidden patterns (v8) which cross-reference the RSM Tick guard pattern (v6). Cross-pattern grep discipline — when adding a new pattern that shares an invariant primitive with an existing pattern, prepend a "grep both when reading either" cross-reference — is a candidate registry hygiene rule.

---

*This review supersedes the `architecture-review-2026-06-26.md` baseline for purposes of current state. Prior findings INT-004, INT-005, INT-006 confirmed CLOSED; INT-003 remains open (documentation-quality carry-over); INT-007 opened + CLOSED this pass.*

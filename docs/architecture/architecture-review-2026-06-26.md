# Architecture Review — 2026-06-26

**Mode:** `/architecture-review` (full review — Phases 1–9; delta against 2026-06-25 baseline)
**Engine:** Unreal Engine 5.7 (pinned 2026-02-13 via `docs/engine-reference/unreal/VERSION.md`)
**GDDs Reviewed:** 6 — `input-system`, `run-state-machine`, `player-movement`, `difficulty-phase-controller`, `pull-wave-behavior`, `wave-spawner-pattern-library` (file timestamps confirm no GDD changes since 2026-06-24)
**ADRs Reviewed:** 8 — ADR-0001 through ADR-0008
**Prior baseline:** `architecture-review-2026-06-25.md` (CONCERNS verdict; 207 TRs, 31 covered, INT-001/INT-002 BLOCKING, INT-003 documentation)
**TR Registry:** `docs/architecture/tr-registry.yaml` (version 2, 207 entries — unchanged this pass)

---

## Verdict: **CONCERNS**

Major progress since the 2026-06-25 baseline — both BLOCKING integration gaps closed by ADR amendments, and the two highest-priority Foundation/Core ADR gaps closed by ADR-0007 and ADR-0008. Coverage moved from 15% to 43%.

**One new BLOCKING item** in an Accepted ADR (INT-004; ADR-0005 missing `InitializeDependency` calls — same structural pattern as 2026-06-25's INT-001). **Two new defects in the just-authored ADR-0007** (Proposed) — the Tick guard code/prose contradiction (INT-005) and the `FParse::HexNumber64` signature uncertainty (INT-006). All three are surgical amendments; no fundamental architectural rethink is required.

**No cross-ADR conflicts.** **No GDD revision flags.** **No deprecated API references.**

Pre-Production gate **STILL NOT CLEAR** — the four pre-gate infrastructure items from 2026-06-25 are unchanged (no `/test-setup`, no `/ux-design`).

---

## Delta vs 2026-06-25

| Change | Status |
|--------|--------|
| **ADR-0007 authored** (Run State Machine Hosting + Sleep-Aware Time Source) — Proposed | NEW |
| **ADR-0008 authored** (DPC Hosting + FDPCFrameState Atomic Snapshot Publication) — Proposed | NEW |
| **ADR-0005 amended** (INT-001 — IG-7 + Last Verified note for shared-host topology) | AMENDED |
| **ADR-0006 amended** (INT-001 — sub-question (f) shared-host topology binding) | AMENDED |
| **ADR-0002 amended** (INT-002 — `EHapticEvent::NearMiss = 6` + iOS/Android platform patterns) | AMENDED |

ADR-0001, ADR-0003, ADR-0004 unchanged.

---

## Resolved Since 2026-06-25

### ✅ INT-001 — ADR-0005/0006 component-ownership topology (was BLOCKING)

**Resolved by:** ADR-0006 sub-question (f) inscription (lines 194–219) + ADR-0005 Implementation Guideline 7 (lines 292) + ADR-0005 Forbidden Pattern cross-reference. The shared-singleton-host topology (`APullWaveSubsystemActor` owning both ISMCs; pooled `AWave` actors as lightweight state tokens or eliminated) is now load-bearing architecture, not implementation detail. The "2 batched draw calls at PEAK" claim is now structurally provable. Cross-amendment internal consistency verified — both ADRs' Architecture diagrams + Forbidden Pattern entries + GDD Requirements rows align.

### ✅ INT-002 — ADR-0002 EHapticEvent::NearMiss missing (was BLOCKING)

**Resolved by:** ADR-0002 `EHapticEvent::NearMiss = 6` enum value (lines 130–141), iOS FULL pattern (lines 179–185), Android FULL pattern (lines 202–211), Android DURATION pattern (lines 222–225), Architecture diagram update (lines 252–256), 3 new GDD Requirements rows (lines 345–347), and 3 new Hardware Verification Gate items (lines 386–390). PM R11a-12 forward contract closed at the platform-dispatch surface; PM-side opt-in gate (`IGameSettings::IsNearMissHapticEnabled()`) is correctly delegated out of this ADR's scope.

**Forward-contract residue (tracked, not flagged):** ADR-0002's GDD Requirements row for the HUD/Accessibility Settings GDD points at a GDD that does not yet exist. ADR-0002 explicitly marks this out of scope. When the HUD/Accessibility Settings GDD is authored, the `near_miss_haptic_enabled` toggle persistence + UI lands there. No new TR-ID assigned for the forward GDD — it will be registered when that GDD is authored.

---

## Open Integration Items

### 🟡 INT-003 (carry-over) — ADR-0003 `INPUT_TICK_RATE = 60Hz` naming nuance

Unchanged from 2026-06-25 review. Documentation-quality only; F-4's `drain_tick_index` integer comparison remains correct regardless of frame rate. Defer to next ADR-0003 amendment or rename to `INPUT_TICK_RATE_TARGET_HZ`.

### 🔴 INT-004 (NEW) — ADR-0005 missing `Collection.InitializeDependency()` for RSM + DPC (BLOCKING)

**ADRs involved:** ADR-0005 (Accepted), ADR-0007 (Proposed; IG-3), ADR-0008 (Proposed; IG-3).

**Claim:** ADR-0005 `UWaveSpawnerSubsystem::Initialize()` skeleton (lines 239–260) binds to `UDPCSubsystem::OnPostTickFrameStatePublished` via `GetGameInstance()->GetSubsystem<UDPCSubsystem>()` (line 255) without first calling `Collection.InitializeDependency(UDPCSubsystem::StaticClass())`. The same gap exists for RSM: ADR-0005 does not call `Collection.InitializeDependency(URunStateMachineSubsystem::StaticClass())` before any RSM access. ADR-0007 IG-3 (line 448) and ADR-0008 IG-3 (line 405) both explicitly state that ADR-0005 SHOULD be amended with these addenda — but the addenda are not yet inscribed in ADR-0005.

**Why it's blocking (per unreal-specialist independent review):** `FSubsystemCollectionBase` does not guarantee cross-subsystem initialization ordering absent an explicit `InitializeDependency` call. If `UDPCSubsystem` has not initialized when `UWaveSpawnerSubsystem::Initialize()` runs, the `GetSubsystem<>()` call returns `nullptr`, the `if (UDPCSubsystem* DPC = ...)` guard silently swallows the failure, `DPCFrameReadyHandle` is never populated, and `OnPostTickFrameStatePublished` never fires — Wave Spawner never admits waves, silently, in production. No log, no assert, no crash. Same failure mode for the RSM bind (`OnPausedChanged` never fires; pause-flush contract Rule 13 silently fails).

**Pattern signal:** ADR-0005 has now surfaced a BLOCKING integration gap in two consecutive reviews (INT-001 on 2026-06-25; INT-004 on 2026-06-26). ADR-0005 was Accepted on 2026-06-24 before its consumer-side dependencies (ADR-0006 component-ownership, ADR-0007 RSM contracts, ADR-0008 DPC contracts) were authored. Future ADRs promoted to Accepted before their dependency graph is closed may exhibit the same pattern.

**Resolution required:** Amend ADR-0005 Initialize() skeleton with BOTH `Collection.InitializeDependency(URunStateMachineSubsystem::StaticClass())` AND `Collection.InitializeDependency(UDPCSubsystem::StaticClass())` calls before the corresponding `GetSubsystem<>()` bind paths. Upgrade ADR-0007 IG-3 and ADR-0008 IG-3 from "SHOULD be amended" to "MUST be amended — correctness gate." Must land before any Wave Spawner enter-implementation gate.

### 🔴 INT-005 (NEW) — ADR-0007 Tick() body code/prose contradiction (BLOCKING for ADR-0007 Proposed → Accepted)

**ADRs involved:** ADR-0007 (Proposed).

**Claim:** ADR-0007's `URunStateMachineSubsystem::Tick(float DeltaTime)` skeleton (lines 418–425) sets `bHasTickedThisFrame = true` as its first statement but does NOT guard on the flag before executing the tick body. ADR-0007's Risk 4 row (line 543) explicitly promises this defense: "Engine's later regular Tick() call short-circuits because the flag is already true." The two contradict.

ADR-0007 Implementation Guideline 5 (line 452) frames the guard correctly for the DPC-calls-first case ("the engine could tick RSM first, regular Tick() advances remaining_time, then DPC's ForceTickNow() sees flag==false and re-runs RSM tick body") — but the shown `Tick()` body would compile and ship the inverse defect: if `ForceTickNow()` runs first (and runs the tick body via line 435), the engine's later regular `Tick()` call would set the flag (already true → no-op) and then unconditionally re-execute the tick body, double-advancing `remaining_time` and double-firing Rule 17 priority logic. `ForceTickNow()` at lines 428–439 IS correctly guarded; `Tick()` must mirror that guard.

**Resolution required:** Amend ADR-0007 Tick() skeleton to guard on `bHasTickedThisFrame` before executing the body (either inline early-return or extract `TickInternal()` called by both `Tick()` and `ForceTickNow()`). Risk 4 mitigation language should be updated to reference the guard-at-top-of-Tick pattern explicitly. Must land before ADR-0007 promotes Proposed → Accepted.

### 🟠 INT-006 (NEW) — ADR-0007 `FParse::HexNumber64` signature (REQUIRES SOURCE VERIFY; blocks RSM Initialize() implementation)

**ADRs involved:** ADR-0007 (Proposed).

**Claim:** ADR-0007's `Initialize()` skeleton (line 361) uses `FParse::HexNumber64(*SaltHex, Salt)` as a bool-returning function with an out-parameter `uint64& Salt`. Within the LLM training window through UE 5.3, `FParse::HexNumber64` is documented as `static uint64 HexNumber64(const TCHAR* Start, TCHAR** End = nullptr)` — a direct-value-return signature, not bool + out-param. If that is the UE 5.7 signature, the call site does not compile.

The project's `docs/engine-reference/unreal/modules/` directory has no `Misc/Parse.h` snapshot, so this cannot be resolved from local files. The safe cross-version fallback is `FCString::Strtoui64(*SaltHex, nullptr, 16)`.

**Resolution required:** Verify against `Engine/Source/Runtime/Core/Public/Misc/Parse.h` in pinned UE 5.7 source. If signature is value-return, amend the call site (and the `|| Salt == 0` zero-salt rejection branch) to use `FCString::Strtoui64` or to consume the value-return form correctly. Mark in ADR-0007's Verification Required list. Must land before RSM Initialize() implementation.

---

## Newly Closed Coverage Gaps

| Change | TRs | ADR |
|--------|-----|-----|
| TR-RSM-001 through TR-RSM-034 | 34 | ✅ COVERED by ADR-0007 (all 34 mapped 1:1 in ADR-0007 GDD Requirements Addressed table) |
| TR-DPC-001 through TR-DPC-024 | 24 | ✅ COVERED by ADR-0008 (all 24 mapped 1:1 in ADR-0008 GDD Requirements Addressed table) |
| TR-PM-030 (NearMiss haptic) | 1 | ✅ COVERED by ADR-0002 INT-002 amendment (upgraded from ⚠️ Partial — enum was missing) |

**Net traceability movement:**

| Metric | 2026-06-25 | 2026-06-26 | Δ |
|--------|-----------|-----------|---|
| Total TRs | 207 | 207 | 0 |
| ✅ Covered | 31 (15%) | **90 (43%)** | **+59** |
| ⚠️ Partial | ~30 (14%) | ~29 (14%) | −1 |
| ❌ Gap | ~146 (71%) | **~88 (43%)** | **−58** |

| System | TRs | ADRs covering | 2026-06-25 status | 2026-06-26 status |
|--------|-----|---------------|-------------------|-------------------|
| Input System | 50 | ADR-0001, 0002, 0003 | ✅ Covered | ✅ Covered (unchanged) |
| Run State Machine | 34 | **ADR-0007** | ❌ Gap | **✅ Covered (34/34)** |
| Player Movement | 35 | ADR-0002 (haptic events) | ❌ Gap (1 partial) | ❌ Gap (1 covered via INT-002) — ADR-0009 still required |
| Difficulty & Phase Controller | 24 | **ADR-0008** | ❌ Gap | **✅ Covered (24/24)** |
| Pull-Wave Behavior | 29 | ADR-0006 | ⚠️ Partial | ⚠️ Partial (renderer + topology covered; state machine/pool/despawn still ADR-0010) |
| Wave Spawner & Pattern Library | 35 | ADR-0005 | ⚠️ Partial | ⚠️ Partial (hosting covered; pattern library + cadence governor still ADR-0011) |

No registry additions this pass — ADR-0007's 5 "DPC GDD R6 Rule 3 FC-1..FC-5" rows in its GDD Requirements Addressed table are forward-contract designations, not new TR-IDs. The 5 forward contracts are subsumed by TR-DPC-024 plus internal ADR-0007/0008 alignment.

---

## Cross-ADR Conflicts

**None.** No two ADRs make contradictory claims about overlapping decisions.

**Topology cross-check (pull-then-push tick chain) — PASS:** DPC `Tick()` calls `RSM->ForceTickNow()` (pull primitive); DPC then publishes `OnPostTickFrameStatePublished` (push to Wave Spawner). ADR-0007 architecture diagram (lines 171–180), ADR-0008 architecture diagram (lines 220–228), and ADR-0005 architecture diagram (lines 151–153) all describe the same topology. The chain only fails if INT-004 leaves the delegate bind silently no-op.

**EDPCAbortReason ownership inversion — PASS:** Owned by DPC module (ADR-0008 SD-4 + `Public/DPC/DPCAbortReason.h`), forward-declared on RSM side (ADR-0007 line 212). ADR-0008 IG-8 correctly prohibits `UFUNCTION()` on `RSM::RequestAbort(EDPCAbortReason)` while the enum is only forward-declared. Specialist code-review gate addition: any future `UPROPERTY()`/`USTRUCT`/`DECLARE_DYNAMIC_MULTICAST` parameter of type `EDPCAbortReason` on RSM's side would also require full type definition — RSM's Build.cs must NOT add DPC as a public dependency, and code review must catch such additions.

**Shared `Platform/SleepAwareClock.h` — PASS:** ADR-0007 Structural Decision 3 + Migration Plan step 2 specify a shared header at `Source/SLIPSTORM/Public/Platform/SleepAwareClock.h` exposing `GetSleepAwareSeconds()` (RSM) + `GetSleepAwareMilliseconds()` (IS Seam 1). Both backed by `mach_continuous_time()` (iOS) / `clock_gettime(CLOCK_BOOTTIME)` (Android). No two-clock drift risk — the same syscall pair feeds both unit-domains via the shared header.

**ADR-0003 / ADR-0007 `UGameInstanceSubsystem` rejection — PASS (resolved by ADR text, same as 2026-06-25):** ADR-0003 rejects `UGameInstanceSubsystem` for `FInputSystem` because `MakeUnique<>` is incompatible with UObject construction; ADR-0007 adopts `UGameInstanceSubsystem` for RSM (a UObject). The two are not in conflict — the rejection scope is specific to non-UObject classes.

---

## ADR Dependency Order (updated)

| Layer | ADR | Title | Status | Depends On | Hardware/Source Gate |
|-------|-----|-------|--------|------------|----------------------|
| Methodology | ADR-0004 | Dual-Grep Methodology | Proposed | — | One successful application |
| Foundation | ADR-0001 | Palm Rejection R_max + Touch Radius Bridge | Proposed | — | HW gate (iPhone 16 + Galaxy S24) |
| Foundation | ADR-0002 | Haptic Platform Bridge (amended INT-002) | Proposed | ADR-0001 (plugin pattern) | HW gate (NearMiss FULL/DURATION distinctness on both platforms) |
| Foundation | ADR-0003 | 60Hz Drain Queue + FInputSystem | Proposed | ADR-0001 (plugin pattern) | FTSTicker thread verify |
| Foundation | **ADR-0007** | Run State Machine Hosting + Sleep-Aware Time Source | **Proposed** | — | **INT-005 + INT-006 must close before Accepted; 6 source-verify items** |
| Foundation | ADR-0005 | Wave Spawner UGameInstanceSubsystem Hosting (amended INT-001) | Accepted | — | **INT-004 amendment required before Wave Spawner enter-implementation** |
| Foundation | ADR-0006 | Pull-Wave Instanced Renderer (amended INT-001) | Accepted | — (sibling of 0005) | 9 UE 5.7 verifications (unchanged) |
| Core | **ADR-0008** | DPC Hosting + FDPCFrameState Atomic Snapshot Publication | **Proposed** | ADR-0007 (consumes ForceTickNow + RequestAbort + EDPCAbortReason forward-decl) | 5 source-verify items |

**No cycles. No unresolved dependencies.**

**Forward ordering observation:** ADR-0005 (Accepted) declares forward contracts on RSM (`OnPausedChanged`) and DPC (`OnPostTickFrameStatePublished`) that are now codified by ADR-0007 + ADR-0008 (both Proposed). Per `docs/CLAUDE.md`, stories referencing a Proposed ADR are auto-blocked. ADR-0005 itself is Accepted, but Wave Spawner stories that bind to RSM/DPC contracts depend on ADR-0007 + ADR-0008 promoting to Accepted. INT-004 + INT-005 + INT-006 are the operative gate for that promotion sequence.

---

## GDD Revision Flags

**None.** No GDD assumption was contradicted by ADR-0007 or ADR-0008 verified engine behaviour. The two RSM GDD textual surfaces worth tracking:
- RSM GDD Rule 21 (`uint64 UPROPERTY(BlueprintReadOnly)`) — ADR-0007 documents brief deviation (UHT-rejected on UE 5.7; preserved semantic intent via `int64 GetRunSeedBP()` wrapper). Recorded inside ADR-0007 (Key Interfaces, Consequences>Negative, TR-RSM-023 row). No GDD revision required.
- DPC GDD R7 BLOCKING `IsTickable() == true` + `ETickableTickType::Conditional` pair — ADR-0008 Consequences>Negative line 493 flags this as future-GDD-tuning candidate (idiomatic alternative is `ETickableTickType::Always`); not actionable at ADR layer; tracked for future GDD pass.

No `systems-index.md` updates required this pass.

---

## Engine Compatibility Audit

### Audit Baseline (cross-ADR sweep)

| Check | Result |
|-------|--------|
| ADRs with Engine Compatibility section | 7/8 ✅ (ADR-0004 is methodology — N/A) |
| Engine version consistency (UE 5.7) | 8/8 ✅ |
| Post-Cutoff APIs Used field populated | 7/8 ✅ (ADR-0004 N/A) |
| References Consulted field populated | 8/8 ✅ |
| Deprecated API references in ADRs | 0 ✅ |
| Stale engine-version references | 0 ✅ |
| Stale platform-min claims | 0 ✅ |
| `TSharedPtr<T>` mis-used for UObject | 0 ✅ |

### Engine Specialist Findings (unreal-specialist, fresh-context cross-ADR sweep)

Specialist verdict: **CONCERNS** — three items requiring resolution before implementation; remaining items PASS with source-verify gates noted.

| # | Item | Severity | Resolution Path |
|---|------|----------|-----------------|
| 1 | ADR-0007 Tick() body missing `bHasTickedThisFrame` guard — code/prose contradiction with Risk 4 mitigation | BLOCKING for ADR-0007 → Accepted | INT-005 amendment |
| 2 | ADR-0007 `FParse::HexNumber64` signature uncertain (out-param form may not match UE 5.7) | REQUIRES SOURCE VERIFY; blocks RSM Initialize() implementation | INT-006 source verify + amendment |
| 3 | ADR-0005 missing `InitializeDependency(UDPCSubsystem)` (and `InitializeDependency(URunStateMachineSubsystem)`) — silent wave-spawning failure in production if dependency not initialized | BLOCKING for Wave Spawner enter-implementation | INT-004 amendment |
| 4 | `FSubsystemCollectionBase::InitializeDependency()` Initialize-completion semantics on UE 5.7 | PASS at specialist confidence; engine-reference has no `SubsystemCollection.cpp` snapshot — project-level source-verify gate remains open | Pre-ship source-verify gate (already in ADR-0007 Risk 1 + ADR-0008 Verification 3) |
| 5 | FTickableGameObject on UPROPERTY-owned UObject (DPC controller) — one tick per frame | PASS — standard ADR-0005 pattern; ADR-0008 Verification 2 as confirm | — |
| 6 | `mach_continuous_time` / `clock_gettime(CLOCK_BOOTTIME)` availability on iOS 14 / Android API 26 | PASS — both predate the minimums (iOS 10+ / API 17+) | ADR-0007 Verifications 4 + 5 can be closed |
| 7 | GConfig `uint64` hex round-trip — `%016llX` Printf + `FParse::HexNumber64` → `FCString::Strtoui64` fallback | PARTIALLY PASS — contingent on Item 2 resolution | Folded into INT-006 |
| 8 | Same-session-bias check on ADR-0008 (5 Structural Decisions) | PASS — all 5 DPC-specifically grounded; no uncritical mirroring of ADR-0005 | — |
| 9 | EDPCAbortReason UHT constraint (ADR-0008 IG-8) | PASS with code-review gate addition | Future `UPROPERTY()`/`USTRUCT`/`DECLARE_DYNAMIC_MULTICAST` of `EDPCAbortReason` on RSM side would require full type def — code review must catch |
| 10 | Topology cross-check (pull-then-push chain) | PASS — contingent on Item 3 (INT-004) resolving the delegate-bind gap | — |
| 11 | ADR-0005/0006 post-amendment + `friend class UDPCController` declaration | PASS — INT-001 amendments internally consistent; friend is intra-module (DPC); `bDisableCollision` REQUIRES SOURCE VERIFY unchanged from prior review | — |

**Anti-pattern sweep (re-validated; all PASS unchanged from 2026-06-25):**
- `IInputProcessor` for raw touch — confirmed only viable path on UE 5.7 mobile
- `UGameInstanceSubsystem + FTickableGameObject` multi-inheritance — idiomatic; ADR-0007 follows ADR-0005 pattern correctly
- `FTSTicker::AddTicker(0.0f)` per-frame firing — contract unchanged
- `FCoreUObjectDelegates::PostLoadMapWithWorld` for pool pre-allocation — still canonical
- Plain ISMC + `SetNumCustomDataFloats(3)` on Mobile Forward — no Mobile Forward-specific restriction

---

## Architecture Document Coverage

`docs/architecture/architecture.md` still does not exist (no change from 2026-06-25). Architecture remains distributed across 8 ADRs + `platform-seam-interfaces.md` + `visual-dispatch-contract.md` + 6 change-impact docs. A consolidated `architecture.md` via `/create-architecture` is the conventional next artifact for Pre-Production gate.

**No orphaned architecture.** Every architectural artifact maps to at least one GDD or system in `systems-index.md`.

---

## Required ADR Amendments (BLOCKING for implementation stories)

1. **🔴 ADR-0005 amendment** (INT-004) — add `Collection.InitializeDependency(URunStateMachineSubsystem::StaticClass())` AND `Collection.InitializeDependency(UDPCSubsystem::StaticClass())` to `Initialize()` skeleton before the corresponding `GetSubsystem<>()` bind paths. Upgrade ADR-0007 IG-3 and ADR-0008 IG-3 language from "SHOULD be amended" to "MUST be amended — correctness gate." Must land before any Wave Spawner enter-implementation gate.

2. **🔴 ADR-0007 amendment** (INT-005) — fix Tick() body to guard on `bHasTickedThisFrame` BEFORE executing the tick body (mirror `ForceTickNow()`'s guard pattern at lines 428–439). Update Risk 4 mitigation language to reference the guard-at-top-of-Tick pattern explicitly. Must land before ADR-0007 promotes Proposed → Accepted.

3. **🟠 ADR-0007 amendment** (INT-006) — mark `FParse::HexNumber64` as REQUIRES SOURCE VERIFY in Verification Required list; document `FCString::Strtoui64(*SaltHex, nullptr, 16)` as fallback if the signature is value-return on UE 5.7. Must land before RSM `Initialize()` implementation.

## Required ADRs (carry-over from 2026-06-25; not new)

4. **🟡 ADR-0009: Player Movement Component and Hardware Watchdog** (Core) — TR-PM-001..035 minus TR-PM-030 (~34 TRs). Recommended AFTER PM decomposition decision per `design/gdd/reviews/player-movement-decomposition-plan-2026-06-16.md`. Engine Risk: MEDIUM-HIGH.
5. **🟡 ADR-0010: Pull-Wave Object Pool, State Machine, and Despawn Pipeline** (Feature) — 5-state lifecycle + pool-size derivation + Seam 13 promotion.
6. **🟡 ADR-0011: Wave Spawner Pattern Library Data Model and Cadence Governor** (Feature) — pattern library data ownership + F-3 platform-determinism + RunSeed propagation from RSM.
7. **🟢 ADR-0012: Telegraph System** (Hazard) — deferred until Telegraph GDD lands.

---

## Pre-Gate Checklist (Phase 9)

Unchanged from 2026-06-25. Cannot run `/gate-check pre-production` until all four items resolve:

- [ ] **❌ `tests/unit/` + `tests/integration/`** directories MISSING → run `/test-setup`
- [ ] **❌ `.github/workflows/`** MISSING → run `/test-setup`
- [ ] **❌ `design/ux/accessibility-requirements.md`** MISSING → run `/ux-design`
- [ ] **❌ `design/ux/interaction-patterns.md`** MISSING → run `/ux-design`

The `design/ux/` directory currently contains only `input-feedback.md`.

---

## Out of Scope

- **Stories (Phase 3b RTM)** — `production/epics/` does not exist; no stories yet. Phase 3b automatically skipped.
- **Consistency-failure logging** — `docs/consistency-failures.md` does not exist; not created by this skill.
- **Systems #6 (Telegraph), #8–17 (Collision through Async Leaderboard)** — no GDDs yet.

---

## Recommended Next Actions

Priority order:

1. **Amend ADR-0005** with both `InitializeDependency()` addenda (INT-004). Surgical single-document edit; closes the Wave Spawner enter-implementation gate.
2. **Amend ADR-0007** Tick() guard (INT-005) + `FParse::HexNumber64` source verify (INT-006). Two surgical edits in the same document; closes ADR-0007 → Accepted gate.
3. **Run `/test-setup` and `/ux-design`** in parallel — required for Pre-Production gate (unchanged from prior review).
4. **Author ADR-0009 (PM)** — closes the largest remaining Core gap (35 TRs). Recommend AFTER PM decomposition decision lands.
5. **Author ADR-0010 (PW lifecycle)** and **ADR-0011 (WS pattern library)** — Feature-layer closure.
6. **Re-run `/architecture-review`** after each amendment / new ADR to verify coverage and re-validate the cross-ADR integration surface.

---

## Reflexion Pattern Signal

ADR-0005 has surfaced a BLOCKING integration gap in two consecutive reviews (INT-001 2026-06-25; INT-004 2026-06-26). Both gaps were introduced by ADR-0005 being Accepted before its downstream consumer dependencies were authored — INT-001 by ADR-0006 (sibling, authored same day), INT-004 by ADR-0007 + ADR-0008 (Foundation/Core authored later). Pattern observation: **promote Foundation ADRs to Accepted only after their immediate dependency graph is closed**, or accept that each subsequently authored dependency may require an amendment to the earlier-Accepted ADR. The amendment cost is low when caught at architecture-review; the failure cost (silent production no-op as in INT-004) is high. Future Accepted-too-early ADRs in this project may exhibit the same surfacing pattern.

---

*This review supersedes the `architecture-review-2026-06-25.md` baseline for purposes of current state. Prior CONCERNS findings INT-001 + INT-002 are confirmed closed; INT-003 remains open; INT-004, INT-005, INT-006 are new.*

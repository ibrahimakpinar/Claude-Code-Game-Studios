# Architecture Review — 2026-06-25

**Mode:** `/architecture-review` (full review — Phases 1–9)
**Engine:** Unreal Engine 5.7 (pinned 2026-02-13 via `docs/engine-reference/unreal/VERSION.md`)
**GDDs Reviewed:** 6 — `input-system`, `run-state-machine`, `player-movement`, `difficulty-phase-controller`, `pull-wave-behavior`, `wave-spawner-pattern-library`
**ADRs Reviewed:** 6 — ADR-0001 through ADR-0006 (`docs/architecture/`)
**Prior baseline:** `architecture-review-2026-06-16.md` (engine-mode only; 2 CONCERNS on ADR-0002 — both now resolved)
**Total Technical Requirements extracted:** 207

---

## Verdict: **CONCERNS** (not FAIL)

- **No blocking cross-ADR conflicts** in any Accepted ADR.
- **0 GDD revision flags** — Phase 5b residue grep confirmed all engine-API warnings in GDDs are correctly-framed negative examples.
- **3 of 6 GDD-having systems have zero ADR-format records** (Run State Machine, Difficulty & Phase Controller, Player Movement). Design exists and is well-specified across GDDs + `platform-seam-interfaces.md`, but key architectural decisions are not codified as discoverable ADRs.
- **1 implementation-blocking integration ambiguity** between ADR-0005 and ADR-0006 (component ownership for `WaveMassISMC` / `TrailCubeISMC`) — resolvable with a one-paragraph amendment.
- **1 missing forward-contract amendment** to ADR-0002 (`EHapticEvent::NearMiss`) — surfaced by Player Movement R11a-12 but not propagated.
- All engine compatibility issues from the 2026-06-16 prior review are **closed** (Android API 26 + iOS 14 minima both landed in ADR-0002 lines 48–49).

Pre-Production gate **NOT YET CLEAR**. Pre-gate infrastructure items also missing — see Phase 9.

---

## Traceability Summary

| Metric | Count |
|--------|-------|
| Total TRs extracted from 6 GDDs | **207** |
| ✅ Covered (explicit ADR mapping) | **31** (~15%) |
| ⚠️ Partial (covered by `platform-seam-interfaces.md` / `visual-dispatch-contract.md` but not ADR-format) | **~30** |
| ❌ Gap (no ADR coverage of any form) | **~146** |

| System | TRs | ADRs covering | System-level status |
|--------|-----|---------------|---------------------|
| Input System | 50 | ADR-0001, ADR-0002, ADR-0003 | ✅ Covered (3 sub-decisions in ADR format) |
| Run State Machine | 34 | — | ❌ Gap — 0 ADRs |
| Player Movement | 35 | ADR-0002 (haptic events only) | ❌ Gap — 0 own ADRs |
| Difficulty & Phase Controller | 24 | — | ❌ Gap — 0 ADRs |
| Pull-Wave Behavior | 29 | ADR-0006 (renderer only) | ⚠️ Partial — state machine / pool / lifecycle not ADR-covered |
| Wave Spawner & Pattern Library | 35 | ADR-0005 (hosting only) | ⚠️ Partial — pattern library / cadence governor not ADR-covered |

Full per-TR matrix lives in `docs/architecture/requirements-traceability.md` (also written by this review).

---

## Coverage Gaps (no ADR exists)

Foundation/Core layer gaps in priority order — most foundational first.

### ❌ Run State Machine Hosting + Time Source (Foundation)
- **Affected TRs:** TR-RSM-001 through TR-RSM-034 (all 34)
- **What's missing:** Class choice (AGameModeBase? `UGameInstanceSubsystem`? `AActor`?), `FAppTimeSource` sleep-aware iOS+Android clock implementation, `OnStateChanged` + `OnPausedChanged` delegate declarations, RunSeed source (`FPlatformTime::Cycles64()` XOR installation salt), tick-prerequisite ordering pin.
- **Current location:** Diffused across `run-state-machine.md` prose + `platform-seam-interfaces.md` Seam 1 (`IMonotonicClock`). Not in ADR format.
- **Why it matters:** ADR-0005 binds to `OnPostTickFrameStatePublished` from DPC and ADR-0005's lifecycle assumes RSM exists at `Initialize()` time. Both pre-conditions are GDD-level assertions, not ADR-validated decisions.
- **Suggested ADR title:** "Run State Machine Hosting and Sleep-Aware Time Source"
- **Engine Risk:** HIGH — iOS `mach_continuous_time()` + Android `clock_gettime(CLOCK_BOOTTIME)` are sleep-aware paths that diverge from `FPlatformTime::Seconds()`; must be source-verified on UE 5.7.

### ❌ DPC Hosting + Atomic Snapshot Publication (Core)
- **Affected TRs:** TR-DPC-001 through TR-DPC-024 (all 24)
- **What's missing:** Class choice (mirror of ADR-0005 — `UGameInstanceSubsystem`? `UWorldSubsystem`?), `OnPostTickFrameStatePublished` multicast delegate declaration site (ADR-0005 line 252–254 references this delegate as a forward contract from DPC, but DPC has no ADR declaring it), `FDPCFrameState` struct definition + atomic snapshot semantics, `UCurveFloat` asset validation policy, tick host (`FTickableGameObject`?), Path B TelegraphWindowCurve key set codification.
- **Current location:** `difficulty-phase-controller.md` GDD + Seam 7 (`IRSMTimeStateProvider`) + Seam 8 (`IDPCSnapshotConsumer`) + Seam 9 (`IDPCAbortDelegate`) in `platform-seam-interfaces.md`.
- **Why it matters:** ADR-0005 explicitly cites `UDPCSubsystem::OnPostTickFrameStatePublished` as `DECLARE_MULTICAST_DELEGATE_OneParam(FOnPostTickFrameStatePublished, const FDPCFrameState&)` — but `UDPCSubsystem` itself has no ADR. The forward contract is one-sided.
- **Suggested ADR title:** "DPC Hosting and FDPCFrameState Atomic Snapshot Publication"
- **Engine Risk:** MEDIUM — `UCurveFloat` asset-validation patterns + multicast delegate type pinning are well-known UE 5.x patterns; primary risk is consistency with ADR-0005's subscription site.

### ❌ Player Movement Component + Hardware Watchdog (Core)
- **Affected TRs:** TR-PM-001 through TR-PM-035 (minus the 2 covered by ADR-0002 = ~33)
- **What's missing:** Class choice (`UPlayerLaneMovementComponent` per TR-PM-001 vs. `APlayerCharacter`?), 60-sample rolling DT watchdog architecture (TR-PM-020–022), `OnHardwarePerformanceBreach(bool)` multicast delegate emission + Wave Spawner subscription contract (TR-PM-023), Shipping-Safety guards (TR-PM-026–027), tick-after-RSM ordering pin via `AddTickPrerequisiteComponent` (TR-PM-035), curve-asset validation at BeginPlay (TR-PM-034).
- **Current location:** `player-movement.md` GDD (currently in MAJOR REVISION / decomposition planned per session-state — `design/gdd/reviews/player-movement-decomposition-plan-2026-06-16.md` proposes splitting into mechanics / presentation / platform sub-GDDs).
- **Why it matters:** PM is the only player verb; its hardware watchdog imposes a binding forward contract on Wave Spawner (`suppress M=3 PEAK barrage on watchdog breach`) that has no ADR record on either side.
- **Suggested ADR title:** "Player Movement Component and 60-Sample Hardware Watchdog"
- **Engine Risk:** MEDIUM-HIGH — `UCurveFloat` asset paths, `FTickableGameObject` or `UActorComponent` choice, and hardware-watchdog rolling-buffer accumulation are all sensitive to UE 5.7 mobile thermal behavior. Decomposition status complicates ADR authoring — recommend ADR follows decomposition decisions.

### ⚠️ Pull-Wave Object Pool + 5-State Lifecycle (Feature)
- **Affected TRs:** TR-PW-002 (5-state lifecycle), TR-PW-005 (active-wave `TArray` with `RemoveAt` not `RemoveAtSwap`), TR-PW-006 (monotonic int32 wave_id), TR-PW-007 (`FPullWaveInstanceState` ≤256 bytes), TR-PW-010 (pool size 23 = 16 + 2 + 5), TR-PW-015 (TraverseElapsedS + pause exclusion), TR-PW-016 (despawn pipeline ordering), TR-PW-017 (Seam 13 `OnDespawnedUserCallback`).
- **What's missing:** State machine codification (only renderer ADR exists in ADR-0006), pool-size derivation rationale (alignment with ADR-0005's 23-actor pool), Seam 13 promotion to architecture authority (currently informal in `platform-seam-interfaces.md`).
- **Current location:** `pull-wave-behavior.md` GDD + `platform-seam-interfaces.md` Seam 13. ADR-0006 covers ONLY the ISMC choice, explicitly disclaiming actor-pool ownership.
- **Suggested ADR title:** "Pull-Wave Object Pool, State Machine, and Despawn Pipeline"

### ⚠️ Wave Spawner Pattern Library + Cadence Governor (Feature)
- **Affected TRs:** TR-WS-001–007 (3 phase pools; OPENER/MID barrage exclusion; 7-surviving-triplet PEAK barrage; 0.35s simultaneity window; 0.70s non-barrage stagger), TR-WS-013–014 (RunSeed propagation + Death Replay determinism), TR-WS-021 (atomic pool swap on phase transitions), TR-WS-029–035 (F-3 cadence governor + platform-determinism non-transcendentals constraint).
- **What's missing:** Pattern library data ownership (asset vs. registry), F-3 platform-determinism rationale, atomic pointer swap semantics on phase transitions.
- **Current location:** `wave-spawner-pattern-library.md` GDD. ADR-0005 covers ONLY hosting, explicitly out-of-scope for pattern selection.
- **Suggested ADR title:** "Wave Spawner Pattern Library Data Model and Platform-Deterministic Cadence Governor"

### Telegraph System (Hazard) — out of scope
No GDD yet (prototype-gated per `systems-index.md`); ADR also out of scope until GDD lands.

### Systems #8–17 (Collision, Near-Miss, Scoring, Camera, HUD, etc.) — out of scope
No GDDs yet; future work.

---

## Cross-ADR Conflicts

**None** — no two ADRs make contradictory claims about overlapping decisions.

**Surface-level conflict resolved by ADR text:** ADR-0003 explicitly rejects `UGameInstanceSubsystem` for `FInputSystem` (Alternative 2) on the grounds that `MakeUnique<>` is incompatible with UObject construction. ADR-0005 adopts `UGameInstanceSubsystem` for `UWaveSpawnerSubsystem`. This is NOT a conflict: ADR-0005 §Related (line 415) addresses it directly — the ADR-0003 rejection applies to the non-UObject `FInputSystem` design; the Wave Spawner is a UObject and uses the standard subsystem factory. Both decisions are correct in their own scope.

---

## Integration Gaps (architectural ambiguity, not conflicts)

### 🔶 INT-001 — `WaveMassISMC` / `TrailCubeISMC` component ownership unresolved (BLOCKING)

**ADRs involved:** ADR-0005 (Accepted), ADR-0006 (Accepted).

**Claim:** ADR-0005 specifies a 23-actor `AWave` pool. ADR-0006 specifies `APullWaveSubsystemActor` holding shared `WaveMassISMC` (16 instances at PEAK) and `TrailCubeISMC` (48 instances at PEAK). ADR-0006 §GDD Requirements Addressed line 549 leaves component ownership unresolved ("per-AWave-actor vs shared on subsystem actor — that is a per-instance pool implementation detail").

**Why it's a blocker (per unreal-specialist independent review):** `UInstancedStaticMeshComponent` batches only the instances it directly owns. If `WaveMassISMC` lives on each of the 23 `AWave` actors, there are 23 separate ISMCs and 16 separate draw calls from `WaveMassISMC` alone at PEAK — not 1. The "2 batched draw calls at PEAK" claim (ADR-0006 lines 251–252) and the underlying R7 B9 binding require a SINGLE shared `WaveMassISMC` holding all 16 active-wave instances and a SINGLE shared `TrailCubeISMC` holding all 48 trail cubes. The shared-host topology is a load-bearing architectural constraint, not an implementation detail.

**Resolution required:** A one-paragraph amendment to ADR-0006 (or a bridging ADR) confirming that `APullWaveSubsystemActor` is a separate singleton actor — independent of the 23-actor `AWave` pool — that owns the shared ISMCs and the per-wave runtime state (`TArray<FPullWaveRuntimeState>`). The pooled `AWave` actors become lightweight state tokens (or are eliminated and their state absorbed into `APullWaveSubsystemActor`). This must land before the wave-renderer story enters implementation; entering without the resolution will force mid-sprint rework of ADR-0005's pool semantics.

### 🔶 INT-002 — ADR-0002 missing `EHapticEvent::NearMiss` (forward contract not propagated)

**ADRs involved:** ADR-0002 (Proposed).

**Claim:** Player Movement R11a-12 (per session-state) authored an opt-in near-miss haptic with the new enum value `EHapticEvent::NearMiss` and a forward contract on the haptic ADR. TR-PM-030 codifies this: "Fire near-miss haptic `EHapticEvent::NearMiss` when `IGameSettings::IsNearMissHapticEnabled() && TriggerNearMissBeat()`." ADR-0002's `EHapticEvent` enum (lines 112–127) lists only `R3Collision` / `DeadBandContact` / `SlipConfirmed` / `BufferDrop` / `InputRejected` / `ContactResting` — `NearMiss` is absent. `rg "NearMiss\|near.miss" adr-0002-haptic-platform-bridge.md` returns zero hits.

**Resolution required:** Amend ADR-0002 to add `EHapticEvent::NearMiss = 6` plus iOS FULL-tier (`UIImpactFeedbackGenerator(style: .light)` or a tuned `intensity` value) and Android FULL-tier (`VibrationEffect.createOneShot(...)`) patterns. DURATION-tier and lower fall through the existing tier dispatch. Adds 1 new test-stub recording slot. Single-document amendment.

### 🟢 INT-003 — `INPUT_TICK_RATE = 60Hz` naming nuance (DOCUMENTATION)

**ADRs involved:** ADR-0003 (Proposed).

**Claim (per unreal-specialist):** ADR-0003 refers to the drain rate as `INPUT_TICK_RATE = 60Hz`, but the `FTSTicker::AddTicker(0.0f)` pattern fires once per game frame — which is 30Hz on Android min-spec under the AC-PW-22a frame-rate floor, not a hard 60Hz guarantee. F-4's `drain_tick_index` integer comparison remains correct regardless of frame rate; the misnomer is documentation-quality only, not a correctness issue.

**Resolution required:** Optional. Either rename the constant to `INPUT_TICK_RATE_TARGET_HZ` or add an explanatory note. Defer to next ADR-0003 amendment.

---

## ADR Dependency Order

| Layer | ADR | Title | Status | Depends On | Hardware/Source Gate |
|-------|-----|-------|--------|------------|----------------------|
| Methodology | ADR-0004 | Dual-Grep Methodology | Proposed | — | — |
| Foundation | ADR-0001 | Palm Rejection R_max + Touch Radius Bridge | Proposed | — | HW gate (iPhone 16 + Galaxy S24) |
| Foundation | ADR-0002 | Haptic Platform Bridge | Proposed | ADR-0001 (plugin pattern) | HW gate (iOS/Android FULL/DURATION/NONE) |
| Foundation | ADR-0003 | 60Hz Drain Queue + FInputSystem | Proposed | ADR-0001 (plugin pattern) | FTSTicker thread verify |
| Foundation | ADR-0005 | Wave Spawner UGameInstanceSubsystem Hosting | Accepted | — | 4 UE 5.7 verifications |
| Foundation | ADR-0006 | Pull-Wave Instanced Renderer | Accepted | — (sibling of 0005) | 9 UE 5.7 verifications |

**No cycles.** **No unresolved dependencies.**

**Recommended ADR implementation order:**
1. ADR-0004 (methodology — adopt immediately)
2. ADR-0001 (gates ADR-0002 + ADR-0003)
3. ADR-0002 + ADR-0003 (parallelizable after ADR-0001 promotes to Accepted)
4. ADR-0005 + ADR-0006 (sibling pair; can implement in parallel)
5. **GAP** — author RSM hosting + DPC hosting ADRs before any RSM or DPC story enters implementation (these are pre-conditions for ADR-0005's subscription to `OnPostTickFrameStatePublished`)

**Status promotion gates** (not review-verdict blockers, but pre-implementation requirements):
- ADR-0001 → Accepted: requires hardware verification gate (lines 304–313).
- ADR-0002 → Accepted: requires hardware verification gate (lines 343–346).
- ADR-0003 → Accepted: requires `FTSTicker` game-thread verification (line 443).
- ADR-0004 → Accepted: requires one successful application in a future propagation pass (line 287).

Per `docs/CLAUDE.md`, stories referencing a Proposed ADR are auto-blocked. No stories exist yet (pre-production), so this is not currently a blocker — but the gates must close before the first IS / PM / haptic story enters implementation.

---

## GDD Revision Flags

**None.** Phase 5b residue grep against GDDs for stale engine-API claims surfaced only correctly-framed negative examples:
- `design/gdd/run-state-machine.md` lines 64, 283, 284, 538 explicitly warn implementers NOT to use `mach_absolute_time()` / `CLOCK_MONOTONIC` and prescribe `mach_continuous_time()` / `clock_gettime(CLOCK_BOOTTIME, ...)` — consistent with engine reference.
- `design/gdd/input-system.md` line 283 cites `mach_absolute_time()` as a NEGATIVE example (do-not-use).
- No GDD mentions Android API < 26 or iOS < 14 as supported deployment targets.
- ADR-0002 platform-min fixes (Android API 26 / iOS 14) from the 2026-06-16 prior review are confirmed landed (ADR-0002 lines 48–49).

No systems-index updates required.

---

## Engine Compatibility Audit

### Audit Baseline (cross-ADR sweep)

| Check | Result |
|---|---|
| ADRs with Engine Compatibility section | 5/6 ✅ (ADR-0004 is methodology — N/A) |
| Engine version consistency (UE 5.7) | 6/6 ✅ |
| Post-Cutoff APIs Used field populated | 5/6 ✅ (ADR-0004 N/A) |
| References Consulted field populated | 6/6 ✅ |
| Deprecated API references in ADRs | 0 ✅ (`rg` against `deprecated-apis.md` clean) |
| Stale engine-version references | 0 ✅ |
| Stale platform-min claims (prior review residue) | 0 ✅ (both ADR-0002 fixes landed) |
| `TSharedPtr<T>` mis-used for UObject | 0 ✅ (ADR-0005/0006 correctly use `TObjectPtr<T>`; ADR-0003's `TSharedPtr<FInputProcessorProxy>` is for a non-UObject — correct) |

### Engine Specialist Findings (unreal-specialist, fresh-context cross-ADR sweep)

Specialist verdict: **audit baseline confirmed**; one documentation-quality nuance (INT-003 above), one structural finding (INT-001 above), no new conflicts.

**Anti-pattern sweep — all PASS:**
- **(a) `IInputProcessor` for raw touch** — confirmed as only viable path on UE 5.7 mobile. Enhanced Input does not propagate contact radius through `FPointerEvent`. No UE 5.7 breaking change to `FSlateApplication::RegisterInputPreProcessor`. Proxy pattern in ADR-0003 is correct.
- **(b) `UGameInstanceSubsystem + FTickableGameObject` multi-inheritance** — idiomatic in UE 5.x. `ETickableTickType::Conditional` + `IsTickable()` is the documented conditional-overhead pattern. No 5.7-specific gotcha.
- **(c) `FTSTicker::AddTicker(0.0f)` per-frame firing** — contract unchanged per available engine reference. ADR-0003's pre-implementation thread-verification gate (line 443) is the right protection.
- **(d) `FCoreUObjectDelegates::PostLoadMapWithWorld` for pool pre-allocation** — still canonical on UE 5.7. `UGameInstance::OnWorldChanged` is an alternative but `PostLoadMapWithWorld` is the standard one-shot hook.
- **(e) Plain ISMC + `SetNumCustomDataFloats(3)` on Mobile Forward** — no Mobile Forward-specific restriction. ADR-0006 Verification 3 covers material-graph propagation.

**Highest-impact single verification per ADR** (pre-implementation gate, prioritized):

| ADR | Verification |
|-----|--------------|
| ADR-0001 | Hardware finger-tap gate (iPhone 16 + Galaxy S24, lines 304–309). If OEM PPI calibration diverges from formula, EVERY contact is universally accepted or rejected. No other gate recovers from miscalibrated threshold post-ship. |
| ADR-0002 | Confirm `UHapticFeedbackComponent` produces ZERO mobile output on UE 5.7 iOS/Android. Entire bridge architecture rests on this claim; if Epic added a mobile haptic API between 5.3 and 5.7, custom bridge is unnecessary complexity. |
| ADR-0003 | Verify `FTSTicker` callbacks fire on the game thread (not a dedicated ticker thread) on UE 5.7 iOS Metal + Android Vulkan. Off-thread firing breaks all `DrainTick()` → `FSlateApplication` interactions; no in-`DrainTick()` assertion catches this until integration. |
| ADR-0005 | Verify `FCoreUObjectDelegates::PostLoadMapWithWorld` does NOT re-fire on replay-world re-use (Verification 2). The 16.6 ms-per-retry hitch-avoidance — the entire justification for `UGameInstanceSubsystem` over `UWorldSubsystem` — fails silently if the one-shot guard is absent and the delegate fires. |
| ADR-0006 | Verify `SetNumCustomDataFloats(N)` (set in constructor) propagates to `GetPerInstanceCustomData(N)` material nodes under Mobile Forward in UE 5.7 (Verification 3). All per-wave visual variation routes through this path; silent zero-binding = invisible effects. |

---

## Architecture Document Coverage

`docs/architecture/architecture.md` **does not exist**. The architecture is currently distributed across:
- 6 ADRs (`adr-0001`–`adr-0006`)
- `platform-seam-interfaces.md` — 13+ injectable seams (1858 lines)
- `visual-dispatch-contract.md` — visual dispatch architecture (446 lines)
- 5 change-impact docs (audit trail for propagation passes)

For Pre-Production gate, a consolidated `architecture.md` produced by `/create-architecture` is the conventional next artifact. It would reconcile the seams-doc and contract-doc material with the ADRs into a single navigable architecture blueprint.

**No orphaned architecture.** Every architectural artifact maps to at least one GDD or system in `systems-index.md`.

---

## Required ADRs (prioritized; Foundation → Feature)

1. **🔴 ADR-0007: Run State Machine Hosting and Sleep-Aware Time Source** (Foundation) — RSM class choice + `FAppTimeSource` iOS/Android sleep-aware clocks + `OnStateChanged`/`OnPausedChanged` delegate declarations + RunSeed generation. Engine Risk: HIGH. Run `/architecture-decision` next.
2. **🔴 ADR-0008: DPC Hosting and FDPCFrameState Atomic Snapshot Publication** (Core) — `UDPCSubsystem` class choice + `OnPostTickFrameStatePublished` delegate declaration site (ADR-0005 cites this delegate as forward contract; one-sided until DPC ADR lands) + `FDPCFrameState` struct definition + UCurveFloat asset-validation policy. Engine Risk: MEDIUM.
3. **🟠 ADR-0005 + ADR-0006 Component-Ownership Amendment** — single paragraph or bridging ADR resolving `WaveMassISMC`/`TrailCubeISMC` shared-host topology (INT-001 above). Implementation blocker for the wave-renderer story.
4. **🟠 ADR-0002 EHapticEvent::NearMiss Amendment** — add enum value + iOS/Android FULL-tier patterns (INT-002 above). Single-document amendment.
5. **🟡 ADR-0009: Player Movement Component and Hardware Watchdog** (Core) — `UPlayerLaneMovementComponent` class choice + 60-sample rolling DT watchdog + `OnHardwarePerformanceBreach` delegate + tick ordering pin + Shipping-Safety guards. Engine Risk: MEDIUM-HIGH. Recommend authoring AFTER PM decomposition decision (per `player-movement-decomposition-plan-2026-06-16.md`).
6. **🟡 ADR-0010: Pull-Wave Object Pool, State Machine, and Despawn Pipeline** (Feature) — 5-state lifecycle + pool-size derivation aligned with ADR-0005 (16+2+5=23) + Seam 13 promotion.
7. **🟡 ADR-0011: Wave Spawner Pattern Library Data Model and Cadence Governor** (Feature) — pattern library data ownership + F-3 platform-determinism non-transcendentals constraint + atomic pool swap on phase transitions + RunSeed propagation from RSM.
8. **🟢 ADR-0012: Telegraph System** (Hazard) — deferred until Telegraph GDD lands (prototype-gated per `systems-index.md`).

---

## Pre-Gate Checklist (Phase 9)

Cannot run `/gate-check pre-production` until all four items below resolve:

- [ ] **❌ `tests/unit/` + `tests/integration/`** directories MISSING → run `/test-setup`
- [ ] **❌ `.github/workflows/`** MISSING → run `/test-setup`
- [ ] **❌ `design/ux/accessibility-requirements.md`** MISSING → run `/ux-design`
- [ ] **❌ `design/ux/interaction-patterns.md`** MISSING → run `/ux-design`

The `design/ux/` directory currently contains only `input-feedback.md`.

---

## Out of Scope

- **Stories (Phase 3b RTM)** — `production/epics/` does not exist; no stories yet (pre-production). Phase 3b automatically skipped per skill spec.
- **Consistency-failure logging (Phase 8 reflexion)** — `docs/consistency-failures.md` does not exist; skill does not create it.
- **Systems #6 (Telegraph), #8–17 (Collision through Async Leaderboard)** — no GDDs authored yet.

---

## Recommended Next Actions

1. **Author ADR-0007 (RSM Hosting)** — highest-impact Foundation gap. Open a fresh `/clear` session and run `/architecture-decision run-state-machine` (or equivalent).
2. **Author ADR-0008 (DPC Hosting)** — closes the one-sided forward contract from ADR-0005.
3. **Amend ADR-0006 §line 549** with the shared-host topology paragraph (specialist-recommended, ~30 minutes of focused work).
4. **Amend ADR-0002** with `EHapticEvent::NearMiss` enum value + patterns.
5. **Run `/test-setup` and `/ux-design`** in parallel — required for Pre-Production gate.
6. **Re-run `/architecture-review`** after each new ADR is written to verify coverage improves.

---

*This review supersedes the engine-only `architecture-review-2026-06-16.md` baseline. Prior CONCERNS findings (ADR-0002 Android API 26 / iOS 14) are confirmed closed.*

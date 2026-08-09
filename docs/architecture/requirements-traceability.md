# Architecture Traceability Index

**Last Updated:** 2026-06-26
**Engine:** Unreal Engine 5.7
**Source:** `/architecture-review` Phase 8 — companion to `architecture-review-2026-06-26.md`
**TR Registry:** `docs/architecture/tr-registry.yaml` (207 entries, version 2)

---

## Coverage Summary

| Metric | Count |
|--------|-------|
| Total requirements | **207** |
| ✅ Covered (explicit ADR mapping) | **90** (43%) |
| ⚠️ Partial (covered by seam/contract doc, not ADR-format) | **~29** |
| ❌ Gaps (no ADR coverage of any form) | **~88** |

**Per-system rollup:**

| System | TRs | ADRs covering this system | System-level coverage |
|--------|-----|---------------------------|------------------------|
| Input System | 50 | ADR-0001, ADR-0002, ADR-0003 | ✅ Covered (3 ADRs decompose the system into palm rejection, haptic bridge, and drain queue) |
| Run State Machine | 34 | **ADR-0007** | **✅ Covered (34/34 — ADR-0007 maps every TR-RSM-NNN row 1:1)** |
| Player Movement | 35 | **ADR-0009** (+ ADR-0002 for TR-PM-030 haptic event) | **✅ Covered (35/35 — ADR-0009 maps every TR-PM-NNN row 1:1; TR-PM-030 double-covered by ADR-0002 INT-002)** |
| Difficulty & Phase Controller | 24 | **ADR-0008** | **✅ Covered (24/24 — ADR-0008 maps every TR-DPC-NNN row 1:1)** |
| Pull-Wave Behavior | 29 | ADR-0006 (renderer + INT-001 topology) | ⚠️ Partial (renderer + component-ownership covered; state machine, pool, despawn pipeline, Seam 13 still require ADR-0010) |
| Wave Spawner & Pattern Library | 35 | ADR-0005 (hosting + INT-001 topology) | ⚠️ Partial (hosting covered; pattern library, cadence governor, atomic pool swap still require ADR-0011) |

---

## Full Traceability Matrix

Legend: ✅ Covered by ADR · ⚠️ Partial (covered by `platform-seam-interfaces.md` or `visual-dispatch-contract.md`, not ADR) · ❌ Gap (no architectural record)

### Input System (50 TRs) — ADR-0001, ADR-0002, ADR-0003

| TR-ID | Requirement (one-line) | ADR | Status |
|-------|------------------------|-----|--------|
| TR-IS-001 | Bounded per-contact state scoped to touch lifetime | — | ❌ |
| TR-IS-002 | Receives raw touch, dispatches slip-left/right | ADR-0003 | ✅ |
| TR-IS-003 | No game-state awareness except NONE-tier visual | — | ❌ |
| TR-IS-004 | Six platform interfaces injected at construction | platform-seam-interfaces.md (Seams 1–6) | ⚠️ |
| TR-IS-005 | Monotonic ms clock continues across suspension | platform-seam-interfaces.md (Seam 1) | ⚠️ |
| TR-IS-006 | Touch-radius provider for palm rejection w/o engine fork | ADR-0001 | ✅ |
| TR-IS-007 | Haptic dispatch bridge — graceful degradation | ADR-0002 | ✅ |
| TR-IS-008 | Visual dispatch — NONE-tier R-3 + ContactResting | visual-dispatch-contract.md | ⚠️ |
| TR-IS-009 | Boot-flag store — FIRST_RUN_PROMPT_ENABLED persistence | platform-seam-interfaces.md (Seam 6) | ⚠️ |
| TR-IS-010 | Zone classification: L/R halves + 8mm exclusion band | — | ❌ |
| TR-IS-011 | Touch-down → new-touch check before zone classification | ADR-0003 | ✅ |
| TR-IS-012 | Radius ≤ 6.0mm filter via ITouchRadiusProvider | ADR-0001 | ✅ |
| TR-IS-013 | Valid tap: new ID + radius ≤ 6.0mm + outside band | ADR-0001 | ✅ |
| TR-IS-014 | Cancel timer 180ms via IMonotonicClock | platform-seam-interfaces.md (Seam 1) | ⚠️ |
| TR-IS-015 | Cancel timer resets on any touch-up before 180ms | — | ❌ |
| TR-IS-016 | ContactResting haptic on cancel-timer expire | ADR-0002 | ✅ |
| TR-IS-017 | ContactResting suppression in DEAD/RESOLVING/ABORTED | ADR-0002 | ✅ |
| TR-IS-018 | One-tick hold buffer delays dispatch | ADR-0003 | ✅ |
| TR-IS-019 | Same-frame collision: opposite zones same/adjacent tick | ADR-0003 | ✅ |
| TR-IS-020 | Collision window frame-rate-dependent | ADR-0003 | ✅ |
| TR-IS-021 | Min 30 FPS for deterministic collision | ADR-0003 | ✅ |
| TR-IS-022 | DrainTick once per game frame via FTSTicker | ADR-0003 | ✅ |
| TR-IS-023 | DEAD_BAND contacts discarded silently | — | ❌ |
| TR-IS-024 | Resting contacts excluded until lifted+re-pressed | — | ❌ |
| TR-IS-025 | Dispatch occurs regardless of run state except ABORTED | — | ❌ |
| TR-IS-026 | ABORTED state flushes tracking set every DrainTick | — | ❌ |
| TR-IS-027 | R-3 haptic state-gated suppression | ADR-0002 | ✅ |
| TR-IS-028 | NONE-tier R-3 visual via IVisualDispatch | visual-dispatch-contract.md | ⚠️ |
| TR-IS-029 | Exclusion band computed from 8mm × screen_dpi_per_mm | — | ❌ |
| TR-IS-030 | Pixel density verified on min-spec device matrix pre-ship | — | ❌ |
| TR-IS-031 | drain_tick_index assigned at dequeue time | ADR-0003 | ✅ |
| TR-IS-032 | Resting contact stays IS-internal, no PM dispatch | — | ❌ |
| TR-IS-033 | OS_FingerIndex → contact_id mapping during lifetime | — | ❌ |
| TR-IS-034 | Stylus/indirect rejected before radius filter | ADR-0001 | ✅ |
| TR-IS-035 | TOOL_TYPE_UNKNOWN accepted on Android | — | ❌ |
| TR-IS-036 | Cache slot 2s TTL invalidation at enqueue | ADR-0001 | ✅ |
| TR-IS-037 | FirstRunPrompt via IBootFlagStore, read at COUNTDOWN | platform-seam-interfaces.md (Seam 6) | ⚠️ |
| TR-IS-038 | FirstRunPrompt dismissed on first slip in RUNNING | — | ❌ |
| TR-IS-039 | COUNTDOWN→RUNNING clears pre-positioned contacts | — | ❌ |
| TR-IS-040 | No new dispatch after resting flag or b_dispatched=true | — | ❌ |
| TR-IS-041 | Safe_delta unsigned subtraction wrap-guard | — | ❌ |
| TR-IS-042 | Safe_elapsed_ms returns 0 when NowMs precedes start_ms | — | ❌ |
| TR-IS-043 | All timestamps in ms via IMonotonicClock (suspension-safe) | platform-seam-interfaces.md (Seam 1) | ⚠️ |
| TR-IS-044 | Non-finite NowMs treated as clock stall | — | ❌ |
| TR-IS-045 | Three-touch tie-break via ascending contact_id | — | ❌ |
| TR-IS-046 | Pending_removals drain cleans both tracking maps | — | ❌ |
| TR-IS-047 | WCAG 2.3.1 rate-limit owned by FWidgetVisualDispatch | visual-dispatch-contract.md | ⚠️ |
| TR-IS-048 | Dispatched-contact exclusion prevents re-collision | — | ❌ |
| TR-IS-049 | Cancel timer no-reset on opposite-zone slip in same contact | — | ❌ |
| TR-IS-050 | First-run prompt labels at 78% screen height in COUNTDOWN | — | ❌ |

### Run State Machine (34 TRs) — ADR-0007

All TR-RSM-001 through TR-RSM-034 are ✅ **Covered** by **ADR-0007: Run State Machine Hosting and Sleep-Aware Time Source** (**Accepted 2026-06-26**, after Proposed 2026-06-25 + amendments INT-005 (Tick() body guard) + INT-006 (FCString::Strtoui64 salt-parse switch); originally amended 2026-06-26 for ForceTickNow pull primitive). Every TR-RSM-NNN row maps 1:1 in ADR-0007's GDD Requirements Addressed table (ADR-0007 lines 610–651).

**Open amendments before ADR-0007 → Accepted:** ✅ NONE REMAINING. INT-005 RESOLVED 2026-06-26 (Tick() body skeleton gained FIRST-STATEMENT `if (bHasTickedThisFrame) return;` guard mirroring `ForceTickNow()` guard); INT-006 RESOLVED 2026-06-26 (salt-parse switched to `FCString::Strtoui64(*SaltHex, nullptr, 16)` after primary-source verification of UE 5.7 `Misc/Parse.h` confirmed `FParse::HexNumber64` signature evolved 5.3→5.7 and never matched the original bool+out-param usage). See `architecture-review-2026-06-26.md` for original findings; ADR-0007 Last Verified stamps for resolution detail. **ADR-0007 unblocked for Proposed → Accepted gate.**

`platform-seam-interfaces.md` Seam 1 (`IMonotonicClock`, ms-domain, IS-owned) and ADR-0007 `RunTimeSource` (seconds-domain, RSM-owned) share platform primitives via `Source/SLIPSTORM/Public/Platform/SleepAwareClock.h` per ADR-0007 Structural Decision 3 + Migration Plan step 2.

### Player Movement (35 TRs) — ADR-0009

All TR-PM-001 through TR-PM-035 are ✅ **Covered** by **ADR-0009: Player Movement Component Hosting, Forward-Motion Model, and Tween Implementation** (**Accepted 2026-07-09**, authored Proposed 2026-07-03). Every TR-PM-NNN row maps 1:1 in ADR-0009's GDD Requirements Addressed table (21 mechanics + 5 presentation + 9 platform per Step 8b 2026-07-02 tr-registry routing).

**Depends on ADR-0007** (consumes `URunStateMachineSubsystem::ForceTickNow()` + `OnStateChanged` + `OnPausedChanged` non-dynamic multicast delegates + `bHasTickedThisFrame` INT-005 idempotence guard). **ADR-0007 was promoted Proposed → Accepted on 2026-06-26**, satisfying ADR-0009's sole architectural Depends-On gate. **Depends on ADR-0002** (Haptic Platform Bridge) for `IHapticDispatch::Fire(EHapticEvent::{NearMiss, SlipConfirmed, BufferDrop})` + `IsSystemHapticsEnabled()` gate — ADR-0002 remains Proposed pending its Polish-phase Hardware Verification Gate, but the interface consumed by PM is INT-002-amended and stable per architecture-review-2026-07-03 §INT-007 pragmatic-promotion rationale (ADR-0005/0006-Accepted-before-ADR-0007 precedent applied). **ADR-0009 was promoted Proposed → Accepted on 2026-07-09** with no open INT findings remaining (INT-007 factual-status corrections closed in-session 2026-07-03; architecture-review-2026-07-08 delta confirmed unblocked).

**TR-PM-030 double-coverage**: Near-miss `EHapticEvent::NearMiss` haptic is covered by both ADR-0002 (INT-002 amendment 2026-06-26 — enum-value addition) and ADR-0009 (SD5-adjacent dispatch call site + Implementation Guideline haptic-gate pattern). Both ADRs reference the same interface surface (`IHapticDispatch::Fire`); no contract conflict.

`platform-seam-interfaces.md` Seam 12 (`IPlayerMovementProvider` + `FPlayerMovementProvider_Production(UPlayerLaneMovementComponent*)` + `FPlayerMovementTestStub`) provides the test seam for PM-consumer unit tests (Pull-Wave Rule 11 near-miss direct-read; Wave Spawner R11a-8 grace-window forward contract). Class-name reconciliation (`UPlayerMovementComponent*` → `UPlayerLaneMovementComponent*`) landed via 2026-07-03 hygiene edit per architecture-review-2026-07-03 (line 38).

### Difficulty & Phase Controller (24 TRs) — ADR-0008

All TR-DPC-001 through TR-DPC-024 are ✅ **Covered** by **ADR-0008: DPC Hosting and FDPCFrameState Atomic Snapshot Publication** (**Accepted 2026-06-27**, authored 2026-06-26). Every TR-DPC-NNN row maps 1:1 in ADR-0008's GDD Requirements Addressed table (ADR-0008 lines 576–601).

**Depends on ADR-0007** (consumes `RSM->ForceTickNow()` + `RSM->RequestAbort(EDPCAbortReason)` + the forward-declared `EDPCAbortReason` enum on RSM's public header). **ADR-0007 was promoted Proposed → Accepted on 2026-06-26**, satisfying ADR-0008's sole Depends-On gate. **ADR-0008 was promoted Proposed → Accepted on 2026-06-27** following ADR-0007's promotion, with no open INT findings remaining (INT-004 ADR-0008-side IG-3 closed in INT-004 pass 2026-06-26; architecture-review-2026-06-26 surfaced zero ADR-0008-internal blockers).

`platform-seam-interfaces.md` Seams 7 (`IRSMTimeStateProvider`), 8 (`IDPCSnapshotConsumer`), 9 (`IDPCAbortDelegate`) provide the test seams for DPC unit tests per ADR-0008 Migration Plan step 8.

### Pull-Wave Behavior (29 TRs) — ADR-0006 (renderer only)

| TR-ID | Requirement (one-line) | ADR | Status |
|-------|------------------------|-----|--------|
| TR-PW-001 | 5 lanes fixed (indices 0-4) per R1 RC-F | — | ❌ |
| TR-PW-002 | 5-state lifecycle SPAWNED→LEANING→TRAVERSING→LANDED→DESPAWNING | — | ❌ |
| TR-PW-003 | TELEGRAPH_WINDOW_FLOOR_S = 0.70s (R10d locked) | — | ❌ |
| TR-PW-004 | FPullWaveCurveSnapshot SAMPLE_COUNT=32 immutable at SPAWNED | — | ❌ |
| TR-PW-005 | TArray<FPullWaveInstanceState> with RemoveAt (not RemoveAtSwap) | — | ❌ |
| TR-PW-006 | wave_id monotonic int32; iteration ASCENDING per Rule 14(b) | — | ❌ |
| TR-PW-007 | FPullWaveInstanceState 188 bytes ≤ 256 ceiling | — | ❌ |
| TR-PW-008 | AC-PW-22b pat-12: FORBID inlined UCurveFloat literals | — | ❌ |
| TR-PW-009 | LeanEaseCurve_Canonical RCIM_Cubic + RCTM_Auto pinned | — | ❌ |
| TR-PW-010 | Pool ≥ 23 (16 + 2 + 5) | ADR-0005 (alignment) | ⚠️ |
| TR-PW-011 | WaveMassISMC + TrailCubeISMC = 2 draws | ADR-0006 | ✅ |
| TR-PW-012 | PerInstanceCustomData [0]=Lean [1]=NearMiss [2]=Dissolve | ADR-0006 | ✅ |
| TR-PW-013 | TrailCube PerInstanceCustomData[0]=TrailAlpha | ADR-0006 | ✅ |
| TR-PW-014 | Velocity frozen at SPAWNED; no mid-flight updates | — | ❌ |
| TR-PW-015 | TraverseElapsedS canonical; pause excluded; no resume teleport | — | ❌ |
| TR-PW-016 | Despawn pipeline Coll→Tel→OnWaveDespawned→hide→clear→pool | — | ❌ |
| TR-PW-017 | Seam 13 OnDespawnedUserCallback test-stub slot | platform-seam-interfaces.md (Seam 13) | ⚠️ |
| TR-PW-018 | MIN_ESCAPE_SLIPS=2 registry constant | — | ❌ |
| TR-PW-019 | AC-PW-PEAK-NO-ADJACENT-CLUSTER; 7 surviving triplets | — | ❌ |
| TR-PW-020 | LEAN_ANGLE_MIN_TIER_GAP_DEG=3.0 static_assert per pair | — | ❌ |
| TR-PW-021 | Tick chain RSM→DPC→PM→PW→Telegraph→Collision | — | ❌ |
| TR-PW-022 | Mobile Forward only; no Nanite/Lumen | ADR-0006 | ✅ |
| TR-PW-023 | AC-PW-22a ~0.25ms tick; 16 concurrent max | ADR-0006 | ✅ |
| TR-PW-024 | Alpha-tested motion trail; mobile tile-GPU constraint | ADR-0006 (implicit) | ✅ |
| TR-PW-025 | NEAR_MISS_FLASH_DURATION_S=0.066s registry | — | ❌ |
| TR-PW-026 | F-BARRAGE-SURVIVABILITY-INVARIANT scoped tier≥2 | — | ❌ |
| TR-PW-027 | bTeleport=true (Pull-Wave logic) + SetCullDistances | ADR-0006 | ✅ |
| TR-PW-028 | PM SLIP_TWEEN_DURATION_S [0.10, 0.15]s forward contract | — | ❌ |
| TR-PW-029 | SPAWN_PLANE_Z_OFFSET_M default 15.0m | — | ❌ |

Top-priority covering ADR: **ADR-0010 (proposed) — Pull-Wave Object Pool, State Machine, and Despawn Pipeline**.

### Wave Spawner & Pattern Library (35 TRs) — ADR-0005 (hosting only)

| TR-ID | Requirement (one-line) | ADR | Status |
|-------|------------------------|-----|--------|
| TR-WS-001 | 3 phase pools (OPENER/MID/PEAK) pre-authored, static | — | ❌ |
| TR-WS-002 | OPENER pool: no barrages | — | ❌ |
| TR-WS-003 | MID pool: no barrages (Rule 2) | — | ❌ |
| TR-WS-004 | PEAK barrage = 7 triplets exactly | — | ❌ |
| TR-WS-005 | PEAK barrages tier≥2; tier-1 forbidden | — | ❌ |
| TR-WS-006 | Barrage onsets cluster ≤ 0.35s (FLOOR/2) | — | ❌ |
| TR-WS-007 | Non-barrage stagger ≥ 0.70s (FLOOR) | — | ❌ |
| TR-WS-008 | Pool 23 AWave at PostLoadMapWithWorld | ADR-0005 | ✅ |
| TR-WS-009 | UPROPERTY TArray<TObjectPtr<AWave>> GC anchor | ADR-0005 | ✅ |
| TR-WS-010 | UGameInstanceSubsystem + FTickableGameObject | ADR-0005 | ✅ |
| TR-WS-011 | Conditional tick disabled Cold/Idle | ADR-0005 | ✅ |
| TR-WS-012 | Read DPC snapshot once per tick | ADR-0005 | ✅ |
| TR-WS-013 | RunSeed:uint64 from RSM at Cold→Active | — | ❌ |
| TR-WS-014 | Death Replay restores RNG seed pre-replay | — | ❌ |
| TR-WS-015 | Cadence gate: (now − last_spawn_time) ≥ wave_spawn_interval | — | ❌ |
| TR-WS-016 | Primer (first draw) bypasses cadence gate | — | ❌ |
| TR-WS-017 | Barrage admit needs 3 slots atomically | — | ❌ |
| TR-WS-018 | Dropped barrage → barrage_owed reserves next 3-slot window | — | ❌ |
| TR-WS-019 | 9 named telemetry events (C.3 §9 schema) | — | ❌ |
| TR-WS-020 | Pool once per session; replay reuses | ADR-0005 | ✅ |
| TR-WS-021 | Atomic pool swap OPENER→MID→PEAK on phase transitions | — | ❌ |
| TR-WS-022 | In-flight wave completes under admission-time snapshot | — | ❌ |
| TR-WS-023 | Rule 9 phase-boundary drain window | — | ❌ |
| TR-WS-024 | Subscribe DPC OnPostTickFrameStatePublished | ADR-0005 | ✅ |
| TR-WS-025 | Read RSM OnPausedChanged → pause flush (Rule 13) | — | ❌ |
| TR-WS-026 | Rule 12 despawn order: collision-unreg → telegraph-unreg → OnWaveDespawned | — | ❌ |
| TR-WS-027 | Pause-flush does NOT update last_spawn_time | — | ❌ |
| TR-WS-028 | RSM resume grace honored via Rule 1 gate | — | ❌ |
| TR-WS-029 | F-3 governor targets BARRAGE_EVENTS_PER_PEAK_TARGET_AVG=2 | — | ❌ |
| TR-WS-030 | F-3 force-draw when zero barrages and t_norm ≥ T_FORCE | — | ❌ |
| TR-WS-031 | base_w safe [0.15, 0.40] (F-3 cook advisory) | — | ❌ |
| TR-WS-032 | base_w < W_CEILING cross-knob invariant | — | ❌ |
| TR-WS-033 | Death Replay same-architecture (IEEE-754 divergence) | — | ❌ |
| TR-WS-034 | F-3 uses only clamp + linear ops (no transcendentals) | — | ❌ |
| TR-WS-035 | Future F-3 transcendentals MUST document determinism impact | — | ❌ |

Top-priority covering ADR: **ADR-0011 (proposed) — Wave Spawner Pattern Library Data Model and Cadence Governor**.

---

## Known Gaps (Prioritized)

### ADR Amendments (BLOCKING for implementation stories)

1. **🔴 INT-004 (NEW 2026-06-26)** — ADR-0005 missing `Collection.InitializeDependency(URunStateMachineSubsystem)` AND `Collection.InitializeDependency(UDPCSubsystem)` in `Initialize()` skeleton. Silent wave-spawning failure in production if dependencies not initialized. BLOCKING for Wave Spawner enter-implementation gate.
2. **✅ INT-005 RESOLVED 2026-06-26** — ADR-0007 Tick() body skeleton (lines 417–425 post-amendment) gained FIRST-STATEMENT `if (bHasTickedThisFrame) return;` early-return guard before the existing flag set, mirroring `ForceTickNow()` guard at lines 431–434. Risk 4 mitigation code/prose contradiction closed. ADR-0007 Last Verified line stamped 2026-06-26. Registry v5→v6 added `RunStateMachine_Tick_set_bHasTickedThisFrame_without_prior_guard` forbidden pattern.
3. **✅ INT-006 RESOLVED 2026-06-26** — Primary-source verification against `/Users/Shared/Epic Games/UE_5.7/Engine/Source/Runtime/Core/Public/Misc/Parse.h` confirmed UE 5.7 signature is `static CORE_API uint64 HexNumber64(FStringView HexString)` — value-return, FStringView-taking; incompatible with the ADR's original bool+out-param call AND incompatible with the LLM-window 5.3 `(const TCHAR*, TCHAR**)` form (signature evolved 5.3→5.7). ADR-0007 `Initialize()` salt-parse switched to `FCString::Strtoui64(*SaltHex, nullptr, 16)` (UE 5.7 source verified at `Misc/CString.h:549`; C-stdlib-stable across UE 5.0–5.7). Initialize() if-chain restructured from 4-condition single-branch to 2-statement load-then-zero-check pattern. Structural Decision 4 prose at ADR-0007 line 112 + `docs/registry/architecture.yaml` line 480 api: string updated in parallel. Registry v6→v7 (no forbidden pattern added — one-off LLM hallucination about an unstable API, not a generalizable class). ADR-0007 Last Verified line stamped 2026-06-26. **ADR-0007 fully unblocked for Proposed → Accepted gate.**
4. **🟡 INT-003 (carry-over)** — ADR-0003 `INPUT_TICK_RATE = 60Hz` naming nuance. Documentation-quality only.

### Core layer gaps

5. ✅ **RESOLVED 2026-07-09** — ADR-0009 (Player Movement Component Hosting, Forward-Motion Model, Tween Implementation) authored Proposed 2026-07-03 (closes 35-TR PM gap per architecture-review-2026-07-03) and promoted Proposed → Accepted 2026-07-09 (INT-007 closed in-session 2026-07-03; architecture-review-2026-07-08 confirmed unblocked). Hardware watchdog forward contract on Wave Spawner (ADR-0005 R11a-8 grace window) now closed on the PM side.

### Feature layer gaps

6. **TR-PW-002, 005, 006, 010, 015, 016, 017** + others — **ADR-0010** (Pull-Wave Pool + State Machine + Despawn Pipeline).
7. **TR-WS-001…007, 013, 014, 021, 029…035** + others — **ADR-0011** (Wave Spawner Pattern Library + F-3 Cadence Governor).

### Resolved this pass

- ✅ **INT-001 (was BLOCKING)** — ADR-0005/0006 component-ownership ambiguity. Resolved by ADR-0006 sub-question (f) + ADR-0005 IG-7 amendments (2026-06-26).
- ✅ **INT-002 (was BLOCKING)** — ADR-0002 missing `EHapticEvent::NearMiss` enum value. Resolved by ADR-0002 amendment (2026-06-26).

### Future (out-of-scope for this review)

- **Telegraph System (#6)** — GDD prototype-gated; ADR follows GDD.
- **Systems #8–17** (Collision, Near-Miss, Scoring, Camera, HUD, End-Run Screen, Persistence, Audio Controller, Death Replay, Async Leaderboard) — no GDDs yet.
- **HUD/Accessibility Settings GDD** — forward contract from ADR-0002 INT-002 amendment (`near_miss_haptic_enabled` toggle persistence + UI); register associated TRs when GDD authored.

---

## Superseded Requirements

None at v1. The registry begins with 207 active entries; supersession will be recorded here when GDDs evolve.

---

## History

| Date | Total TRs | Full Chain % | Notes |
|------|-----------|--------------|-------|
| 2026-06-25 | 207 | 15% (covered) / 14% (partial) / 71% (gap) | Initial registry first-write; 6 GDDs reviewed, 6 ADRs reviewed. CONCERNS verdict. |
| 2026-06-26 | 207 | **43% (covered) / 14% (partial) / 43% (gap)** | ADR-0007 (RSM) + ADR-0008 (DPC) authored — closes 58 TR gaps. INT-001 + INT-002 closed via ADR-0002/0005/0006 amendments. **3 new INT items** (INT-004 ADR-0005 missing `InitializeDependency`; INT-005 ADR-0007 Tick guard; INT-006 ADR-0007 `FParse::HexNumber64` source verify). CONCERNS verdict. |

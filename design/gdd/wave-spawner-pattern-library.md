# Wave Spawner & Pattern Library

> **Status**: In Review (R3 verdict NEEDS REVISION → R3a in-session revision applied 2026-06-22 — 1 BLOCKING + 1 RECOMMENDED closed; awaits R4 confirmation pass per qa-lead R3 mode recommendation)
> **Author**: ibrahimakpinar + game-designer (lead) + supporting specialists per section
> **Last Updated**: 2026-06-22 (R3a in-session revision per `/design-review` R3 solo qa-lead grep verification — 1 BLOCKING + 1 RECOMMENDED closed via 2 inscription edits at lines 936 + 1054)
> **Implements Pillars**: 2 (Telegraph readability), 3 (60-second escalation arc), 5 (Skill visibility)

> **R3a binding decisions block (2026-06-22)** — applied per qa-lead R3 solo grep-verification synthesis (Pull-Wave R14 precedent). 1 BLOCKING + 1 RECOMMENDED / inscription-class lock-step misses from R2a-4 + R2a-5/R2a-12 closure cycles. Decomposition trigger UNARMED (R3 surface is 1 BLOCKING, well below >8 threshold). Trajectory R1(9) → R2(8) → R3(1) monotonically decreasing. Closure path: in-session single pass (R1a + R2a precedent).
> - **R3a-1** E3 lateral lifecycle inscription fix (qa-lead B-R3-1; oracle-site sweep sub-class (e) per ADR-0004): E3 row at line 936 still read "Pool pre-allocation fires exactly once per session at `Initialize()`" — stale against R2a-4's lifecycle correction. R2a-4 updated 5 sibling sites (Rule 11, Engine integration §, E1, AC-WS-20, AC-WS-31) but missed E3, an adjacent lateral-table row. Cross-system misread risk: any implementer reading the Dependencies § lateral table would conclude pool pre-allocation fires at `UGameInstanceSubsystem::Initialize()` (UE 5.7 lifecycle violation). Replaced with `FCoreUObjectDelegates::PostLoadMapWithWorld` + audit-trail annotation citing R2a-4 sibling fixes.
> - **R3a-2** H.1 narrative count refresh (qa-lead R-R3-1; oracle-site sweep sub-class (b) per ADR-0004): H.1 intro paragraph at line 1054 still read "the 11 binding cook-time checks Rule 15 enumerates" + "AC-WS-01 through AC-WS-09 + AC-WS-32 through AC-WS-35" — stale post-R2a-5 + R2a-12 which added 3 new Rule 15 checks + 3 new ACs (AC-WS-36/37/38). H.8 sibling counts at lines 1140 + 1145 were updated lock-step at R2a closure but H.1 preamble was the missed site. Refreshed to "14 binding cook-time checks" + AC range extended to AC-WS-38.
>
> **Trajectory & forecast calibration**: R3 forecast 0-3 BLOCKING / 3-6 RECOMMENDED (per CD R2 synthesis) — actual R3 = 1 BLOCKING + 1 RECOMMENDED, **first calibrated forecast** for Wave Spawner (Pull-Wave R13 precedent matched). R4 forecast 0 BLOCKING / 0-1 RECOMMENDED HIGH confidence per qa-lead R3 report. R4 mode recommendation: terminal-now (skip R4) or solo qa-lead grep confirmation per Pull-Wave R14 + qa-lead R3 recommendation.

> **R2a binding decisions block (2026-06-22)** — applied per CD R2 lean-panel synthesis (qa-lead + systems-designer + unreal-specialist + creative-director). 8 BLOCKING / inscription-class; decomposition trigger DID NOT FIRE (8 at threshold not >8; no structural distribution). Closure path: in-session single pass.
> - **R2a-1** Tick host inscription (3-spec convergence — qa B-R2-4 + sys B-SD-2 + unreal B-R2-2): `FWaveSpawnerSubsystem` → `UWaveSpawnerSubsystem` (F-prefix wrong for UClass); `UWaveSpawnerSubsystem` implements `FTickableGameObject` alongside `UGameInstanceSubsystem` for per-frame `Tick(float DeltaTime)`; `GetTickableTickType()` returns `ETickableTickType::Conditional` (tick disabled in `Cold`/`Idle` states to avoid idle overhead); `GetStatId()` returns `STATGROUP_WaveSpawner`. AC-WS-30 scope marker renamed.
> - **R2a-2** Tick ordering pin (3-spec convergence — qa B-R2-5 + sys + unreal B-R2-3): the spawner subscribes to DPC's post-tick delegate (`UDPCSubsystem::OnPostTickFrameStatePublished`) rather than ticking independently — makes ordering structural rather than tick-group-dependent. RSM state is read from DPC's published frame state which observes RSM upstream per Pull-Wave tick chain. AC-WS-13 replay determinism dependency now structural.
> - **R2a-3** AC-WS-15 reframe (3-spec convergence — qa B-R2-1 (B-main-1) + sys B-SD-3 + unreal B-R2-4): `FWaveSpawnActorCounter` paper-only seam removed. AC-WS-15 verification mechanism reframed to post-hoc pool inspection (`TArray<TObjectPtr<AWave>>::Num() == 23` at world-ready hook exit; world-actor census across subsequent ticks). No new seam required.
> - **R2a-4** Pool pre-allocation lifecycle fix (unreal-specialist B-R2-1): `UGameInstanceSubsystem::Initialize()` is the WRONG hook — UWorld does not exist at Initialize() in UE 5.7 lifecycle. Deferred to `FCoreUObjectDelegates::PostLoadMapWithWorld` (or `UGameInstance::OnWorldChanged` first-fire) for one-shot pre-allocation on first world load. Replay re-entry reuses the pool unchanged (EC-WS-8 confirms no level reload on replay). Rule 11 + E1 + E3 + AC-WS-20 + AC-WS-31 updated lock-step; E1 line 891 stale "Cold entry" wording retired.
> - **R2a-5** F-3 `base_w < W_CEILING` cross-knob invariant (systems-designer B-SD-1): G.1 safe ranges overlap (β ∈ [0.15, 0.40], W_c ∈ [0.30, 0.60]) so β ≥ W_c is legal at safe-range boundaries → F-3 proportional branch silently saturates at W_c, killing λ-drive feedback. Added binding cross-knob invariant + Rule 15 cook check `PEAK_BASE_W_BELOW_CEILING` + AC-WS-36.
> - **R2a-6** AC-WS-29 fixture reframe (qa-lead B-R2-1): R1a-7's `phase_remaining ≈ 3.05s` fixture was structurally unreachable under Rule 2's MID `max_pattern_length_s = 3.0s` cap (a 3.0s pattern admitted at 3.05s remaining has last onset 50ms BEFORE the swap). Reframed to test the immutability claim AC-WS-29 actually intends: admit MID pattern with onset-2 scheduled at +2.90s (fires 0.15s before swap) and assert onset-2 still uses MID-snapshot `telegraph_window_s = 0.82s` even though the PEAK swap fires before downstream telegraph rendering.
> - **R2a-7** AC-WS-23 schema scope expansion (qa-lead B-R2-2): C.3 §9 listed 5 events; G.3 enumerated 8; total emitted = 9 (4 EC-class runtime defense events absent from schema). Extended C.3 §9 to all 9 events with payload field-sets; added `run_termination_flush_executed` to G.3 rate-limit table; AC-WS-23 scope updated to "all 9 named events."
> - **R2a-8** AC-WS-13 same-platform constraint (qa-lead B-R2-3): AC overstated the invariant relative to EC-WS-15 (Death Replay platform-scoped to same-architecture only — ARM/x86 IEEE-754 divergence). Added same-platform constraint + forward-guard sentence "Any future F-3 change introducing transcendentals MUST update this AC for platform-determinism impact."
> - **R2a-9** [mechanical RECOMMENDED — 3-spec convergence] G.1 UPROPERTY specifier alignment: `EditAnywhere` → `EditDefaultsOnly` (matches G.2 + canonical UDataAsset pattern).
> - **R2a-10** [mechanical RECOMMENDED — 2-spec convergence qa+unreal] `pool_size = 23` literal duplication: annotated `(per F-2)` at the 3 non-oracle sites (Rule 11, AC-WS-09, AC-WS-20). Single oracle remains at F-2.
> - **R2a-11** [mechanical RECOMMENDED — systems-designer R-SD-2] Rule 15 `NON_BARRAGE_STAGGER` empty-set guard: expression updated to `if onset_count ≤ 1: pass; else: min(δᵢ) ≥ TELEGRAPH_WINDOW_FLOOR_S`. Encodes the single-onset vacuous-pass branch the prose described.
> - **R2a-12** [mechanical RECOMMENDED — systems-designer R-SD-5] Rule 15 added 2 cook checks: `BARRAGE_UNIFORM_TIER` (Pull-Wave R7 B11 forward contract) + `BARRAGE_DISTINCT_SOURCE_LANES` (Pull-Wave R7 B12 forward contract). Cook-time enforcement of the within-barrage Pillar 2 + Pillar 5 invariants now lives at Rule 15.
>
> **Specialist disagreement surfaced + ruled by CD**: sys-des B-SD-3 claimed `FWaveSpawnerCallbackTestStub` was paper-only; Phase 2b grep evidence confirmed it IS defined at `platform-seam-interfaces.md` line 1571. Only `FWaveSpawnActorCounter` was paper-only — closed via R2a-3 reframe.
>
> **11 RECOMMENDED + 5 NICE-TO-HAVE deferred to R3 surface** per Pull-Wave R12→R13 precedent: OnPausedChanged delegate type (qa-lead R-R2-1 + unreal R-R2-3 + R1a deferred #5); AC-WS-13 mock-fixture story-sequencing clause (qa R-R2-2); AC-WS-21 LeanEaseCurve_Canonical immutability second fixture (qa R-R2-5 + R1a deferred #2); AC-WS-31 pool-init activation state (qa R-R2-9 / R-main-4); primer pedagogy AC (qa R-R2-6 + R1a deferred #10); AC-WS-14 boundary fixtures enumeration (qa R-R2-8); pause-flush time_to_drain ceiling AC (qa R-R2-4 + R1a deferred #9); AC-WS-22 gate-level downgrade (qa R-R2-10); F-3b drift derivation (sys R-SD-1); Rule 9 direct AC (sys R-SD-4 + R1a deferred #4? actually different); AWave module ownership (unreal R-R2-4). 5 NICE-TO-HAVE: name drift `t_norm`/`t_norm_PEAK`/`τ` (R-main-1); F-2 DESPAWNING rationale (R1a deferred #6 + N-R2-2); F-5 ascending-onset assumption (N-SD-1); F-7 FLOOR/W ratio cross-ref to F-8 (N-SD-2); Rule 7 wording polish.

> **R1a binding decisions block (2026-06-21)** — applied per CD R1 synthesis + 3 user decisions on closure path / Ruling 1 / OQ-WS-3:
> - **R1a-1** Player Fantasy precision (CD Ruling 1): vocabulary-at-lane-class-layer disambiguation + new Rule 15 cook check `MIN_BARRAGE_PATTERN_COUNT_PER_TRIPLET ≥ 2` + AC-WS-35.
> - **R1a-2** OQ-WS-3 CLOSED: locked as `UGameInstanceSubsystem` (perf-driven — UWorldSubsystem rejected per 16.6ms per-replay hitch).
> - **R1a-3** OQ-WS-4 CLOSED (CD Ruling 2): RSM `RunSeed : uint64` forward contract inscribed in Rule 8; AC-WS-13 stays BLOCKING-at-story-Done (Pillar 5 demands hard determinism).
> - **R1a-4** `t_norm_phase` phantom field removed (CD Ruling 3): Rule 8 derives `t_norm_PEAK` locally; DPC's `t_norm` "diagnostic" semantics preserved; Pull-Wave + DPC R14-closed cascade NOT re-opened.
> - **R1a-5** AC-WS-07 wrong constant fixed: 0.35s → 0.70s (4-specialist convergence; Pillar 2 covert sub-telegraph window closed).
> - **R1a-6** Rule 15 ↔ H.1 coverage gap closed: 3 missing ACs added (AC-WS-32 OPENER_NO_BARRAGE, AC-WS-33 PRIMER_PATTERN, AC-WS-34 PILLAR_1_VERB_SLIP); H.1 self-claim corrected.
> - **R1a-7** AC-WS-29 fixture corrected (B4): `phase_remaining ≈ 0.5s` → `≈ 3.05s` (was inside Rule 9 suppression window — inverted the rule).
> - **R1a-8** `UPROPERTY(EditAnywhere, Config)` on UDataAsset fixed (unreal-specialist B1 + perf-analyst R6): `Config` specifier removed; UDataAsset asset-pipeline serialization is the binding pattern (G.1 + G.2).
> - **R1a-9** Rule 11 pool pre-allocation timing locked (perf-analyst B1 + unreal-specialist B3): `UGameInstanceSubsystem::Initialize()` (once per session); replay re-entry reuses.
> - **R1a-10** UWave → AWave global rename + TObjectPtr modernization (unreal-specialist B2): pool entries are AActor (require collision + world placement + SpawnActor); 4 touch sites updated.
> - **R1a-11** F-10 T=0 domain restricted to PEAK non-barrage only (systems-designer B5): prior wording "valid in OPENER + MID + PEAK non-barrage" contradicted Rule 2 shape table; Rule 2 is the authoritative oracle.
> - **R1a-12** Section C.3 labeled anchors added (3-specialist convergence): phantom references at 8 sites now resolve to `### C.3` heading + `C.3 §9` Telemetry sub-table.
> - **R1a-13** AC-WS-12 reservation-fulfilment fixture added (qa-lead R5).
> - **R1a-14** AC-WS-15 verification mechanism: LogActor → seam-injected `FWaveSpawnActorCounter` (unreal-specialist R7).
> - **R1a-15** AC-WS-30/31 device list inheritance: Pull-Wave R10a placeholders (iPhone XR + Pixel 5 / Galaxy A52) (qa-lead R9).
> - **R1a-16** Author note at line 22 stripped (game-designer N10).
> - **R1a-17** OQ-WS resolution sequencing updated: 2 of 7 OQs closed (OQ-WS-3, OQ-WS-4).
>
> 10 RECOMMENDED items deferred to R2 surface per Pull-Wave R12→R13 precedent (pool_size literal duplication, AC-WS-21 LeanEaseCurve_Canonical asset-ref half, AC-WS-22/23 telemetry apparatus, AC-WS-23 schema scope 5-of-9, OnPausedChanged delegate type, F-2 DESPAWNING rationale, pattern_admitted telemetry rate, G.1 BlueprintReadOnly clarification, pause-flush time_to_drain ceiling, primer pedagogical AC).

## Overview

The Wave Spawner is SLIPSTORM's runtime hazard executor: it reads the Difficulty & Phase Controller's per-tick frame state, draws patterns from a phase-specific pool of pre-authored 5-lane shift sequences, and instantiates `Wave` objects through an object pool. As a data layer it owns the pool, enforces the per-phase `max_concurrent_waves` cap, gates barrage admission on atomic slot pre-commitment, and routes despawned waves through `IWaveSpawnerCallback`'s three-step ordered pipeline (collision unregister → telegraph unregister → callback broadcast). As a player-facing effect it is the author of the storm's rhythm — the variety, density, and cadence of pulls that lets a skilled player *read* the run instead of memorize it. Without curated patterns the storm becomes either unreadably random (failing Pillar 5: Skill Is Visible) or memorizable (failing Pillar 2: The World Telegraphs Before It Strikes); the seven-surviving-triplet PEAK pattern library, the barrage minimum-tier ≥ 2 rule, and the per-phase pattern-pool swap on phase boundaries are the curation.

## Player Fantasy

The fantasy is two-layered.

**On the player surface**: the storm has rhythm. Patterns are varied at two distinct layers — the **lane-class vocabulary** (7 surviving PEAK barrage signatures: `{0,1,3}, {0,1,4}, {0,2,3}, {0,2,4}, {0,3,4}, {1,2,4}, {1,3,4}` per Rule 4) and the **per-signature authoring variation** (each signature carries ≥2 authored patterns differing in tier, velocity, source-lane, and onset arrangement; enforced by Rule 15 `MIN_BARRAGE_PATTERN_COUNT_PER_TRIPLET ≥ 2`). A skilled player learns the lane-class vocabulary — they recognize a `{0,1,3}` triplet on its first frame and pre-commit the slip instead of guessing through the telegraph window — while the per-signature variation prevents the same exact pattern instance from cycling at session scale. The 60-second arc feels like a drummer accelerating: three distinct phases (OPENER's spacious 0.94s telegraphs, MID's overlapping 0.82s patterns, PEAK's edge-to-edge 0.70s barrages) that escalate without ever crossing into unreadable randomness. A skilled player should be able to point at any near-miss and say "I read that one" — never "the game just threw that at me."

**Beneath the surface**: the spawner is the silent author. Its job is to *never appear* in the player's mental model of why something happened. If a player dies, the cause is a missed telegraph (Pillar 5: Skill Is Visible) or a too-tight barrage (a tuning failure for designers to fix), never an unfair spawn (a curation failure for the spawner to own). The spawner exists so the storm has both *agency* — Pillar 2's "the world reaches for you" — and *rhythm* — Pillar 3's complete emotional arc inside one run.

**Reference anchors**: Crossy Road's hand-tuned per-zone spawn tables (memorable, not memorizable); Subway Surfers' density-curve-driven 60s session arc; Alto's Odyssey's pattern variety at high speed without unreadable randomness — these are the curation targets.

## Detailed Rules

### Core Rules

**Rule 1 — Per-tick spawn decision gate.** Each tick, the spawner runs a fixed intra-tick event-processing order: (i) RSM events drain first, (ii) DPC `FDPCFrameState` snapshot is then read once and treated as immutable for the remainder of the tick, (iii) admission flow executes. No new spawn-decision evaluation occurs if any gate fails:

- DPC `is_active == false`, OR
- RSM `is_paused == true`, OR
- RSM `resume_grace == true`, OR
- `(now − last_spawn_time) < FDPCFrameState.wave_spawn_interval_s`.

All gates pass ⇒ proceed to Rule 7 (admission) → Rule 8 (selection). **Carve-out**: the OPENER primer (Rule 2a) is exempt from this gate's cadence check; see Rule 2a for the bypass semantics.

**Rule 2 — Phase-specific pattern pools (shape invariants).** Three pools — OPENER, MID, PEAK — are pre-authored and static at runtime. Each pool MUST satisfy the following shape invariants (enforced at cook time per Rule 15):

| Pool | `is_barrage` allowed | `max_concurrent_target` | Allowed tiers | `max_pattern_length_s` |
|---|---|---|---|---|
| OPENER | false only | 1 | 1, 2 | ≤ 2.0s |
| MID | false only (cook-time enforced) | ≤ 2 | 1, 2, 3 | ≤ 3.0s |
| PEAK non-barrage | false | ≤ 3 | any (0–4) | uncapped |
| PEAK barrage | true only | 3 (required) | ≥ 2 (Rule 5) | ≤ `BARRAGE_SIMULTANEITY_WINDOW_S` |

Concrete pattern *content* (lane sequences, onset timing per pattern, art assignment) is authored outside this GDD via `/asset-spec` and lives in `assets/data/wave-patterns/`. This GDD owns the shape contract; the asset spec owns the instances.

**Rule 2a — OPENER primer (first-draw determinism).** The first pattern drawn in any run MUST be the `is_primer=true` pattern: single pull, target lane = 2 (center), `lean_magnitude_tier = 1`, `telegraph_window_s` snapshotted from `FDPCFrameState.telegraph_window_s` at OPENER tick-0 (= 0.94s under Path B). The primer pattern is excluded from the OPENER pool for all subsequent draws in the same run. Rationale: anchors `design/gdd/game-concept.md` §96 ("first 10 seconds teaches the telegraph") — without the primer, a first-time player's introductory wave could be a tier-2 multi-lane pattern, breaking the taught-telegraph moment.

**Primer cadence-gate bypass**: the primer is exempt from Rule 1's cadence-gate condition `(now − last_spawn_time) ≥ wave_spawn_interval_s`. On `Cold → Active`, the spawner admits the primer immediately at the same tick the transition fires (after the RNG seed is captured per Rule 8). Post-primer admission, `last_spawn_time = now` so that the *second* OPENER draw is gated normally (waits `wave_spawn_interval_s_OPENER = 4.0s` from primer admission). Without this bypass a strict reading of Rule 1 would block the primer until `t = 4.0s` and void the "first 10 seconds teaches the telegraph" anchor.

**Rule 3 — OPENER zero barrages.** OPENER pool contains zero `is_barrage=true` patterns. Inherits Pull-Wave AC-PW-OPENER-NO-BARRAGE (R4 RC-R3-5). Tooling: `OPENER_NO_BARRAGE: opener_pool_barrage_count == 0`.

**Rule 4 — PEAK 7-surviving-triplet enumeration.** The PEAK barrage sub-pool's target-lane sets are exactly:

`{0,1,3}, {0,1,4}, {0,2,3}, {0,2,4}, {0,3,4}, {1,2,4}, {1,3,4}`

Adjacent-cluster triplets `{0,1,2}, {1,2,3}, {2,3,4}` are FORBIDDEN at cook time (would force more than `MIN_ESCAPE_SLIPS = 2` consecutive slips, breaking F-BARRAGE-SURVIVABILITY-INVARIANT at REACT ceiling). Inherits Pull-Wave AC-PW-PEAK-NO-ADJACENT-CLUSTER (R7 B8, BLOCKING). Tooling: `PEAK_SURVIVING_TRIPLETS: count == 7 AND list == { … } (set equality)`.

**Rule 5 — PEAK barrage tier floor.** Every PEAK barrage pattern has `lean_magnitude_tier ≥ 2`. Tier-1 PEAK barrages are BANNED. Inherits Pull-Wave AC-PW-PEAK-BARRAGE-MIN-TIER (R8 RC-E). Tooling: `PEAK_BARRAGE_MIN_TIER: min(lean_magnitude_tier) over PEAK barrage pool ≥ 2`.

**Rule 6 — Onset timing: simultaneity window + stagger floor.**

- **Barrage** (`is_barrage == true`): the M = 3 telegraph onsets MUST cluster within `BARRAGE_SIMULTANEITY_WINDOW_S` (currently 0.35s, registered as `FLOOR / 2`). First-to-last-onset span ≤ W.
- **Non-barrage** (`is_barrage == false`): intra-pattern onset stagger MUST be ≥ `TELEGRAPH_WINDOW_FLOOR_S` (currently 0.70s). The 0.35s simultaneity carve-out applies EXCLUSIVELY to barrages.

Tooling: `BARRAGE_W_SPAN: max(onset_last − onset_first) over barrage patterns ≤ 0.35` and `NON_BARRAGE_STAGGER: min(consecutive_onset_delta) over non-barrage patterns ≥ 0.70`.

**Rule 7 — Concurrency cap admission (atomic slot pre-commitment + `barrage_owed` reservation).**

*Slot accounting* (two-layer model):

`current_concurrent = scheduled + live`

where `scheduled` = Wave-Spawner-internal count of waves admitted but not yet advanced past Pull-Wave's `SPAWNED` state (i.e., pre-commitments that have not yet fired their first onset), and `live` = Pull-Wave's runtime cap count: `count(waves where state ∈ {LEANING, TRAVERSING, LANDED})`. Available slots = `FDPCFrameState.max_concurrent_waves − current_concurrent`.

*Admission*:

- A non-barrage candidate requires 1 free slot. If `available ≥ 1`, admit.
- A barrage candidate requires 3 free slots, pre-committed atomically. If `available ≥ 3`, increment `scheduled` by 3 at admission time (all three slots reserved before any onset fires). If `available < 3`, drop the barrage candidate, set `barrage_owed = true`, emit `barrage_dropped_due_to_concurrency` telemetry, and draw a non-barrage replacement from the same pool.

*Atomicity contract*: "atomic" here means a single tick-step statement block on the game thread. The Wave Spawner is hosted as a `UGameInstanceSubsystem` (binding per OQ-WS-3 closure R1a 2026-06-21 — performance-driven decision; UWorldSubsystem rejected because per-replay re-init would fire a 16.6ms pool-alloc hitch per death-retry, materially degrading the core retry loop). All UE subsystems run game-thread-only by default; no concurrent writer to `scheduled` exists. Statement-level torn-write hazards are structurally precluded.

*Reservation* (PEAK only; MID has no barrages by cook-time exclusion per Rule 2):

- While `barrage_owed == true`: the next admission window with `available ≥ 3` is RESERVED for a fresh barrage draw from the PEAK barrage sub-pool, BEFORE any non-barrage admission. On successful barrage admission, clear `barrage_owed`.
- At PEAK exit, if `barrages_admitted_this_PEAK < BARRAGE_EVENTS_PER_PEAK_MIN (= 1)`, emit `peak_min_barrage_floor_undershoot` telemetry. The cadence governor (Rule 8) plus reservation should prevent this in practice; a logged undershoot is a tuning-regression signal.

**Rule 8 — Per-phase pattern selection + cadence governor.**

*Selection* (per admission slot):

- Draw uniformly at random from the active phase's pool, subject to the cadence governor below.
- The per-run RNG is seeded at the `Cold → Active` transition from the RSM-supplied `RunSeed : uint64` field (forward contract on RSM filed via Dependencies § Bidirectional consistency row 1; RSM exposes `RunSeed` on its public interface as run-deterministic given run identity). Death Replay MUST restore this seed before replaying pattern selection — required by Pillar 5 (the player must be able to point at a specific pattern and say "I should have read that one"; without seed determinism, replay shows a different draw order and the death is no longer attributable). Wall-clock-derived seeds are explicitly forbidden (would defeat replay).

*Cadence governor* (PEAK only):

- Track `barrages_admitted_this_PEAK` (int) and `t_norm_PEAK ∈ [0, 1]` (fraction of PEAK elapsed).
- `t_norm_PEAK` is **derived locally** in the spawner as `(now − PEAK_entry_time) / PEAK_DURATION_S`, where `PEAK_entry_time` is captured at the MID→PEAK transition observation tick and `PEAK_DURATION_S` is a registered DPC constant. This GDD does NOT read DPC's `FDPCFrameState.t_norm` field — DPC marks `t_norm` as "diagnostic; not a decision input" (DPC line 172), so spawner uses a locally-tracked equivalent rather than violating DPC's contract.
- If `barrages_admitted_this_PEAK < BARRAGE_EVENTS_PER_PEAK_MIN (= 1)` AND `t_norm_PEAK > 0.5`: bias the next admission's draw probability heavily toward the barrage sub-pool (target ≥ 80% barrage weight) so the floor lands without invoking Rule 7's `barrage_owed` reservation in most runs.
- Otherwise: weight the barrage sub-pool's draw probability proportionally to drive the session-sample average toward `BARRAGE_EVENTS_PER_PEAK_TARGET_AVG (= 2)` without exceeding `BARRAGE_EVENTS_PER_PEAK_MAX (= 3)`.

**Rule 9 — Phase-boundary drain (OPENER→MID and MID→PEAK only).** Within `max_pattern_length_s` of an upcoming OPENER→MID or MID→PEAK transition, no new pattern is started. In-flight patterns complete naturally. This produces a 1–4s rest beat before each transition (Pillar 3: phase identity is felt). Rule 9 does NOT apply to the PEAK→{DEAD, COMPLETE, ABORTED} transition — those are governed unconditionally by Rule 14 (RunTermination flush).

**Rule 10 — Atomic pool swap.** On a phase transition tick:

- The ACTIVE POOL pointer swaps atomically from the old phase's pool to the new phase's pool.
- In-flight Wave INSTANCES are unaffected: each completes its telegraph + traversal + landing under the `telegraph_window_s` value captured into its `FPullWaveSpawnParams` at admission time (inherits Pull-Wave R1 RC-C's `FPullWaveCurveSnapshot` immutable-per-instance precedent).
- "No mid-flight patterns cross the boundary" means no NEW pattern starts are drawn from the old pool after the swap. Existing in-flight waves are not despawned or re-parametrized.
- **Barrage straddling**: a barrage admitted in MID (or any phase) uses the admission-time `telegraph_window_s` for all M onsets — the spawner does NOT split a barrage across phases or re-read the new phase's value mid-barrage.

**Rule 11 — Object pool.**

- Pre-allocate `pool_size = 23` `AWave` actor instances (per F-2 derivation; F-2 is the single oracle site for the 23 value) **exactly once per session on first-world-load**, bound to `FCoreUObjectDelegates::PostLoadMapWithWorld` (or equivalently `UGameInstance::OnWorldChanged` first-fire). This hook fires once on first level load with a valid UWorld; `UGameInstanceSubsystem::Initialize()` is the WRONG hook (UWorld does not yet exist at Initialize() in UE 5.7 — see R2a-4 closure for the lifecycle correction). `UWaveSpawnerSubsystem::Initialize()` subscribes the spawner to this delegate and to DPC's `OnPostTickFrameStatePublished` (R2a-2 tick ordering pin) but performs NO `SpawnActor` calls itself. Replay re-entry (`Flushing → Cold → Active` per EC-WS-8) reuses the existing pool — pool pre-allocation is NEVER re-fired on `Cold` re-entry. `UGameInstanceSubsystem::Deinitialize()` releases the pool on game-instance teardown.
- Pool storage is `UPROPERTY() TArray<TObjectPtr<AWave>>` to prevent garbage collection of pooled instances (UE 5.0+ TObjectPtr modernization of the legacy raw-pointer pattern).
- `Acquire()` returns the first available instance and marks it in-use; `Release(WaveId)` returns an instance to the pool, called from `IWaveSpawnerCallback::OnWaveDespawned` (Pull-Wave Rule 13 step 3).
- No `UWorld::SpawnActor` / `Destroy` calls during active gameplay — both are forbidden once the subsystem has completed first-world-load pool pre-allocation.

**Rule 12 — Despawn pipeline ordering.** When a wave terminates (natural landing, run termination, or pause flush), the spawner invokes `IWaveSpawnerCallback` in strict order:

1. `OnCollisionUnregistered(WaveId)`
2. `OnTelegraphUnregistered(WaveId)`
3. `OnWaveDespawned(WaveId, EWaveDespawnReason, FinalLane)`

`EWaveDespawnReason : uint8 { NaturalLanding, RunTermination, PauseFlush }`. Inherits Pull-Wave AC-PW-15 (ordering invariant). Test stub verifies via `GetEventLogInOrder()` + `WasCollisionUnregisteredBeforeBroadcast()`.

**Rule 13 — Pause flush.**

- On RSM `OnPausedChanged(true)`: the spawner sets `bPauseFlushPending = true` and stops issuing new admission decisions immediately.
- At the next tick top while `is_paused == true`: atomically despawn all in-flight waves in `wave_id` ascending order with `Reason = PauseFlush`. Each despawn invokes the Rule 12 ordered pipeline.
- **Pause flush does NOT update `last_spawn_time`**: the flush is bookkeeping, not an admission. On resume, the cadence gate (F-3b) treats the pre-pause `last_spawn_time` as authoritative, preventing a resume-burst where the gate would erroneously reset and immediately admit the next pattern.
- On RSM `OnPausedChanged(false)`: RSM grants `resume_grace = true` for `RESUME_GRACE_S`. The spawner holds all new admission for the duration of the grace (Rule 1 gate).
- Mid-tick orchestration of pause from inside an `OnWaveDespawned` callback (e.g., AC-PW-MID-TICK-PAUSE-DEFERRAL) is supported via Seam 13's test-stub `SetOnDespawnedUserCallback` — production interface unchanged.

**Rule 14 — Run termination.** On RSM `RUNNING → {DEAD, COMPLETE, ABORTED}`:

- Despawn all in-flight waves with `Reason = RunTermination` via the Rule 12 pipeline.
- In-flight waves' collision callbacks still process per Pull-Wave Rule 13 (terminal hit bookkeeping completes against the just-ended run).
- Flush the pending pattern queue (`barrage_owed` cleared, any deferred candidates dropped).
- No new patterns issued post-transition.

**Rule 15 — Cook-time validation contracts (binding).** The pattern asset pipeline MUST verify the following at cook time and fail the build on violation:

| Check | Assertion |
|---|---|
| `OPENER_NO_BARRAGE` | `count(p in OPENER_pool where p.is_barrage) == 0` |
| `MID_NO_BARRAGE` | `count(p in MID_pool where p.is_barrage) == 0` |
| `PEAK_SURVIVING_TRIPLETS` | `{p.target_lanes for p in PEAK_barrage_pool} == { {0,1,3},{0,1,4},{0,2,3},{0,2,4},{0,3,4},{1,2,4},{1,3,4} }` (set equality) |
| `PEAK_BARRAGE_MIN_TIER` | `min(p.lean_magnitude_tier for p in PEAK_barrage_pool) ≥ 2` |
| `BARRAGE_W_SPAN` | `max(p.last_onset_t − p.first_onset_t for p in {barrage pools}) ≤ BARRAGE_SIMULTANEITY_WINDOW_S` |
| `NON_BARRAGE_STAGGER` | for every non-barrage pattern `p`: `if onset_count(p) ≤ 1: pass (vacuous — no consecutive pairs); else: min(p.consecutive_onset_delta) ≥ TELEGRAPH_WINDOW_FLOOR_S` — **R2a 2026-06-22 expression update per systems-designer R-SD-2**: prior expression `min(...) ≥ FLOOR` was undefined on single-onset patterns (min over empty set is undefined in C++; the prose note "tooling treats this as a no-op pass" was not encoded in the binding expression). Explicit empty-set guard now in the expression. |
| `PRIMER_PATTERN` | OPENER_pool contains exactly one entry with `is_primer == true AND target_lane == 2 AND lean_magnitude_tier == 1` |
| `PILLAR_1_VERB_SLIP` | every pattern's escape solution under `MIN_ESCAPE_SLIPS = 2` + REACT ceiling 0.25s is exclusively `slip` (no verb other than slip required to survive) |
| `POOL_NON_EMPTY` | `count(p) ≥ 1` for every pool in `{OPENER, MID, PEAK_non_barrage, PEAK_barrage}` — an empty pool would produce an empty-draw at runtime |
| `PEAK_BASE_W_RANGE` | `0.15 ≤ |PEAK_barrage_pool| / |PEAK_pool_total| ≤ 0.40` — base barrage fraction must satisfy F-3's cadence governor admissibility band |
| `PEAK_BASE_W_BELOW_CEILING` | `base_w < W_CEILING` (where `base_w = |PEAK_barrage_pool| / |PEAK_pool_total|` and `W_CEILING` is the G.1-configured cadence governor ceiling) — **R2a 2026-06-22 NEW per systems-designer B-SD-1**: prevents F-3 proportional-branch silent saturation. G.1 safe ranges overlap (β ∈ [0.15, 0.40], W_c ∈ [0.30, 0.60]); if β ≥ W_c, F-3's `clamp(β + λ(T_avg − k), 0, W_c)` saturates at W_c and the λ-drive becomes a no-op without any tuner-visible signal. This check fails the build when the configured combination of `base_w` and `W_CEILING` would suppress cadence feedback. |
| `MIN_BARRAGE_PATTERN_COUNT_PER_TRIPLET` | for every PEAK barrage lane-class signature (the 7 surviving triplets per Rule 4), `count(p in PEAK_barrage_pool where p.target_lanes == signature) ≥ 2` — guarantees per-signature authoring variation (tier, velocity, source-lane, onset arrangement) so the lane-class vocabulary does not collapse to 7 identical pattern instances at session scale (CD R1 Ruling 1 2026-06-21; closes game-designer R1 "vocabulary collapse" concern) |
| `BARRAGE_UNIFORM_TIER` | for every barrage pattern `p` (in PEAK_barrage_pool), all M=3 member waves share identical `lean_magnitude_tier` — **R2a 2026-06-22 NEW per systems-designer R-SD-5 (Pull-Wave R7 B11 forward contract enforcement)**: mixed-tier barrages break the Pillar 2 dual-channel angle-magnitude read; cook-time tooling rejects them. Exception: K_class=3 (sole admissible triplet `{0,2,4}` post-R7-B8) is the documented relaxation per Pull-Wave R8 RC-C (preserves K_class=3 variety floor); all other K_classes (4, 5, 6) retain uniform-tier as BLOCKING. |
| `BARRAGE_DISTINCT_SOURCE_LANES` | for every barrage pattern `p` (in PEAK_barrage_pool), the M=3 member waves have distinct `SourceLane` values (no two members share a source lane) — **R2a 2026-06-22 NEW per systems-designer R-SD-5 (Pull-Wave R7 B12 forward contract enforcement)**: shared-source barrages produce multi-wave SPAWNED-frame pile at one source lane, defeating Pillar 5 spatial independence; cook-time tooling rejects them. |

Rule 15 anchors `BARRAGE_SIMULTANEITY_WINDOW_S` and `TELEGRAPH_WINDOW_FLOOR_S` against the registry at cook time. Any PATH (i) FLOOR raise (current ceiling: `TELEGRAPH_WINDOW_FLOOR_MAX_ADMISSIBLE_S = 0.76s`) requires a registry lockstep update to `BARRAGE_SIMULTANEITY_WINDOW_S` (preserving the `FLOOR / 2` design ratio: 0.76s ⇒ W = 0.38s) and a cook-time re-validation of all barrage patterns against the new W.

### States and Transitions

The Wave Spawner has two state surfaces: its own subsystem-level lifecycle state (owned here) and the per-wave-instance state owned by Pull-Wave (read-only from this system).

#### Spawner-level lifecycle states

| State | Meaning | Gate conditions |
|---|---|---|
| `Cold` | Pre-run readiness. Object pool allocated, `scheduled == 0`, `live == 0`, RNG seed not yet captured. Awaiting RSM → RUNNING. | RSM state ∈ {INIT, COUNTDOWN}; DPC `is_active` may be either |
| `Active` | Admitting normally. Rule 1's per-tick spawn decision evaluates every tick. | RSM == RUNNING ∧ `is_paused == false` ∧ `resume_grace == false` ∧ DPC `is_active == true` |
| `Holding` | Single-tick admission skip. No flush, no state mutation beyond `last_spawn_time`. | Any one of `resume_grace == true ∨ cadence_not_elapsed ∨ DPC.is_active == false` |
| `Flushing` | Actively despawning all in-flight waves per Rule 13 (`PauseFlush`) or Rule 14 (`RunTermination`). No new admission. | `bPauseFlushPending == true` ∨ RSM transitioning to {DEAD, COMPLETE, ABORTED} |
| `Idle` | Paused with `live == 0` and `scheduled == 0`. Spawner is dormant. | RSM `is_paused == true` ∧ post-flush settle |

#### Spawner-level transitions

| From | To | Trigger | Side effect |
|---|---|---|---|
| `Cold` | `Active` | RSM enters RUNNING (post-COUNTDOWN); DPC `is_active = true` | Capture per-run RNG seed (Rule 8); draw and admit the primer (Rule 2a) |
| `Active` | `Holding` | RSM grants `resume_grace = true` after `OnPausedChanged(false)`, OR cadence interval not elapsed, OR DPC `is_active = false` | None — Rule 1 gate fails this tick |
| `Holding` | `Active` | All gate conditions pass again | Resume Rule 1 evaluation |
| `Active` | `Flushing` | RSM `OnPausedChanged(true)` ⇒ `bPauseFlushPending = true` | Per Rule 13: at next tick top, despawn all in-flight in `wave_id ASC` with `Reason = PauseFlush` |
| `Flushing` | `Idle` | All in-flight waves processed through Rule 12 pipeline (PauseFlush path) | `scheduled = 0`, `live = 0`, `barrage_owed = false`, deferred queue cleared |
| `Idle` | `Holding` | RSM `OnPausedChanged(false)` ⇒ `resume_grace = true` | Transit through `Holding` for grace window |
| `Active` | `Flushing` | RSM transitions RUNNING → {DEAD, COMPLETE, ABORTED} | Per Rule 14: despawn all in-flight with `Reason = RunTermination` |
| `Flushing` | `Cold` | Post-Rule 14 flush completes; the run has ended | Spawner is pre-run ready; replay reuses the same pool |

**Forbidden transitions** (BLOCKING — cook-time and runtime asserts must verify):

- `Idle` → `Flushing` (there is nothing to flush when `live == 0 ∧ scheduled == 0`)
- `Holding` → `Flushing` directly (RSM must reach `is_paused == true` first; `resume_grace` is the *post-resume* grace, not a pre-pause state)
- `Cold` → `Flushing` (no waves can be in-flight before `Cold → Active`)

#### Per-tick admission flow (within `Active`)

The Active-state per-tick flow is a sequential decision, NOT a state machine — it executes in one tick and resets next tick:

1. **Gate check** (Rule 1): if any gate fails, transition to `Holding` for this tick and return.
2. **Reservation check** (Rule 7 reservation clause): if `barrage_owed == true` and `available ≥ 3`, draw fresh from the PEAK barrage sub-pool and admit atomically (`scheduled += 3`), clear `barrage_owed`, return.
3. **Cadence governor decision** (Rule 8): compute the weighted draw distribution between barrage and non-barrage sub-pools of the active phase.
4. **Draw**: select a pattern from the active pool via deterministic RNG (seeded per Rule 8).
5. **Admission** (Rule 7):
   - If non-barrage: `available ≥ 1 ⇒ admit (scheduled += 1)`.
   - If barrage: `available ≥ 3 ⇒ admit (scheduled += 3)`; else drop, set `barrage_owed = true`, emit `barrage_dropped_due_to_concurrency`, draw a non-barrage replacement.
6. **Onset firing**: for each onset in the admitted pattern, queue an onset event at `t = pattern_start_time + onset_offset` (per the pattern's authored onset schedule). Onsets cause Pull-Wave to transition `SPAWNED → LEANING`, which decrements `scheduled` by 1 and increments `live` by 1.
7. **Cleanup**: update `last_spawn_time = now`.

Next tick: re-evaluate from step 1.

#### Per-wave-instance states (read-only; owned by Pull-Wave)

The wave object's runtime state machine is owned by Pull-Wave: `{SPAWNED, LEANING, TRAVERSING, LANDED, DESPAWNING}`. The Wave Spawner reads this state only for slot accounting under Rule 7's two-layer model:

| Wave state | Contributes to `scheduled` | Contributes to `live` |
|---|---|---|
| `SPAWNED` | ✓ (admitted but not yet leaning) | — |
| `LEANING` | — | ✓ |
| `TRAVERSING` | — | ✓ |
| `LANDED` | — | ✓ |
| `DESPAWNING` | — | — (exiting via Rule 12 pipeline; `Release(WaveId)` returns to pool) |

`current_concurrent = scheduled + live`. Available slots = `FDPCFrameState.max_concurrent_waves − current_concurrent`. This split is essential for Rule 7's atomic barrage pre-commitment to be arithmetically supportable (a barrage's 3 slots are committed at admission, before any onset fires).

#### State invariants (BLOCKING)

- **I-1**: `scheduled` is monotonically non-negative; never decrements below 0.
- **I-2**: `scheduled` decrement is paired 1:1 with a Pull-Wave `SPAWNED → LEANING` transition.
- **I-3**: `live` is paired 1:1 with the count of waves in Pull-Wave states `{LEANING, TRAVERSING, LANDED}` at every tick boundary.
- **I-4**: On `Flushing → Idle` (PauseFlush) or `Flushing → Cold` (RunTermination), both `scheduled` and `live` are zero before the transition completes.
- **I-5**: `barrage_owed = true` ⇒ `current_phase == PEAK` (the flag is cleared on PEAK exit per Rule 14; OPENER and MID have no barrages by Rule 2 / Rule 3).

### C.3 — Interactions with Other Systems

*(Section label `C.3` added R1a 2026-06-21 per CD R1 Ruling: was referenced ~8× across the GDD as canonical anchor for interactions table + telemetry events but had no heading. Labeled anchors elsewhere in this section: `C.3 §9` = the Telemetry sub-table below; cross-references elsewhere in the doc resolve to these labels.)*

Each interaction below names the direction (in/out/bidirectional), the interface, the owner of the schema, and the dependency type (hard = the spawner cannot function without it; soft = the spawner functions but features degrade).

#### Upstream — the Wave Spawner reads from

**1. Difficulty & Phase Controller (DPC)** — hard dependency.

| Direction | Interface | Schema owner |
|---|---|---|
| DPC → Spawner | `FDPCFrameState` snapshot, read once per tick at top of Rule 1 | DPC |

Fields read (per `design/gdd/difficulty-phase-controller.md` lines 129–138):

| Field | Type | Used by | Per-phase defaults (OPENER / MID / PEAK) |
|---|---|---|---|
| `current_phase` | enum `{OPENER, MID, PEAK}` | Rules 2, 9, 10 (active pool select; drain; swap) | — (transition output) |
| `is_active` | bool | Rule 1 (gate); Rule 10 (atomic swap) | — (runtime gate) |
| `telegraph_window_s` | float (seconds) | Rule 2a, Rule 10 (snapshotted into `FPullWaveSpawnParams` at admission) | 0.94 / 0.82 / 0.70 (Path B keys; floor = `TELEGRAPH_WINDOW_FLOOR_S = 0.70s`) |
| `wave_spawn_interval_s` | float (seconds) | Rule 1 (cadence gate) | 4.0 / 2.0 / 1.0 (floor = 0.25s) |
| `max_concurrent_waves` | int32 | Rule 7 (admission cap; clamped to `MAX_CONCURRENT_WAVES_CAP = 16`) | 1 / 2 / 3 (floor = 1) |

*(Note — R1a 2026-06-21 per CD R1 Ruling 3:* the spawner does NOT read DPC's `FDPCFrameState.t_norm` field as a decision input — DPC marks it "diagnostic; not a decision input" (DPC line 172). PEAK cadence governor needs `t_norm_PEAK ∈ [0, 1]`; spawner derives this locally as `(now − PEAK_entry_time) / PEAK_DURATION_S` per Rule 8. `PEAK_entry_time` is captured at the spawner's MID→PEAK transition observation tick; `PEAK_DURATION_S` is a registered DPC constant.*)*

*Binding obligation*: the spawner reads each field ONCE per tick at Rule 1 top and treats the snapshot as immutable for that tick's admission decision. If DPC mutates a field mid-tick (it does not, but the contract is defensive), the spawner does not observe the change until next tick.

*Phase transition contract* (DPC line 395): the spawner observes `current_phase` change at the transition tick and executes Rule 10's atomic pool swap on that tick. DPC owns the transition timing; the spawner owns the swap mechanics.

---

**2. Pull-Wave Behavior** — hard dependency.

| Direction | Interface | Schema owner |
|---|---|---|
| Pull-Wave → Spawner | `Wave` object schema (`FPullWaveSpawnParams` input + per-instance state machine) | Pull-Wave |
| Pull-Wave → Spawner | `LeanEaseCurve_Canonical` asset reference | Pull-Wave (registered constant) |
| Pull-Wave → Spawner | Per-wave state transitions `{SPAWNED, LEANING, TRAVERSING, LANDED, DESPAWNING}` | Pull-Wave |

Fields the spawner writes into `FPullWaveSpawnParams` at admission (per `design/gdd/pull-wave-behavior.md` lines 241ff + Seam 13 schema):

| Field | Type | Source |
|---|---|---|
| `WaveId` | int32 (monotonic) | Spawner-owned counter |
| `SourceLane`, `TargetLane` | int32 ∈ [0, 4] | Pattern definition |
| `lean_magnitude_tier` | int32 ∈ [0, 4] | Derived as `abs(TargetLane − SourceLane)`; verified at admission |
| `is_barrage` | bool | Pattern definition (false outside PEAK barrage sub-pool) |
| `ForwardVelocityMs` | float (m/s) | Pattern definition (clamped to `[PULL_WAVE_VELOCITY_MIN_MS, MAX_MS]`) |
| `LeanDurationS` | float | Pattern definition |
| `TravelDurationS` | float | Pattern definition |
| `TelegraphWindowS` | float | Snapshotted from `FDPCFrameState.telegraph_window_s` at admission (Rule 10) |
| `LeanEaseCurveAsset` | UCurveFloat soft-ptr | `LeanEaseCurve_Canonical` (registry entry) — captured via `FPullWaveCurveSnapshot` per R1 RC-C |

*Binding obligation*: the spawner respects the wave object schema as defined by Pull-Wave and does NOT add fields without an explicit Pull-Wave GDD amendment + ADR review (OQ-PW-3 may add render-component fields; out of this GDD's scope).

*State machine observation*: the spawner subscribes to per-wave state transitions for slot accounting (Section States and Transitions — `scheduled` / `live` model). It does NOT mutate wave state directly; Pull-Wave's per-wave tick logic owns state advancement.

---

**3. Run State Machine (RSM)** — hard dependency.

| Direction | Interface | Schema owner |
|---|---|---|
| RSM → Spawner | Read-only fields: `current_state`, `is_paused`, `resume_grace` (+ duration timer) | RSM |
| RSM → Spawner | `OnPausedChanged(bool bIsPaused)` multicast delegate | RSM (forward contract per `design/gdd/run-state-machine.md` Rule 4 / Rule 19) |
| RSM → Spawner | Transition signals to {DEAD, COMPLETE, ABORTED} | RSM |

*Binding obligation* (R19 grace contract, RSM line 44): on `OnPausedChanged(false)`, RSM sets `resume_grace = true` for `RESUME_GRACE_S`. The spawner MUST honor the grace window — Rule 1 gate fails until grace expires. Without this, in-flight waves whose trajectories began before backgrounding could land before the player has re-acclimated.

*Pause-flush binding* (Rule 13 in this GDD; AC-PW-MID-TICK-PAUSE-DEFERRAL forward contract): the spawner flushes all in-flight on `OnPausedChanged(true)`. Mid-tick orchestration (e.g., pause fired from inside an `OnWaveDespawned` callback) is handled by Seam 13's test-stub `SetOnDespawnedUserCallback` — production path is unaffected.

---

#### Downstream — the Wave Spawner writes to

**4. Pull-Wave Behavior** (callback consumer) — hard.

| Direction | Interface | Schema owner |
|---|---|---|
| Spawner → Pull-Wave | `IWaveSpawnerCallback` production interface (3 methods, strict ordering per Rule 12) | Pull-Wave (Seam 13) |

Production interface (verbatim from `docs/architecture/platform-seam-interfaces.md` Seam 13):

```cpp
enum class EWaveDespawnReason : uint8 { NaturalLanding, RunTermination, PauseFlush };

class IWaveSpawnerCallback {
    virtual void OnCollisionUnregistered(int32 WaveId) = 0;
    virtual void OnTelegraphUnregistered(int32 WaveId) = 0;
    virtual void OnWaveDespawned(int32 WaveId, EWaveDespawnReason Reason, int32 FinalLane) = 0;
};
```

Production binding: `FWaveSpawnerCallback_Production` is constructed with `(UWaveSpawnerSubsystem*, UCollisionSubsystem*, UTelegraphSubsystem*)`. Each callback dispatches to the matching subsystem (`Spawner->ReturnToPool`, `Collision->UnregisterWave`, `Telegraph->UnregisterWave`).

*Ordering invariant* (Pull-Wave AC-PW-15): the 3-step pipeline executes in the order `OnCollisionUnregistered → OnTelegraphUnregistered → OnWaveDespawned` per Rule 12. AC-PW-15 verifies via `WasCollisionUnregisteredBeforeBroadcast()` on the test stub.

---

**5. Collision & Hit Detection** (system #8, undesigned) — hard downstream.

| Direction | Interface | Schema owner |
|---|---|---|
| Spawner → Collision | `UnregisterWave(WaveId)` — via Pull-Wave's `OnCollisionUnregistered` callback | Collision (future GDD; current interface is the seam-defined production wrapper) |

The spawner does not invoke collision directly — Pull-Wave's production `FWaveSpawnerCallback_Production` does it on the spawner's behalf. The spawner publishes the despawn event; the collision system consumes the unregister side-effect.

*Forward contract* (will be inherited by `design/gdd/collision-and-hit-detection.md` when authored): unregister MUST happen before `OnWaveDespawned` fires so that a re-entrant callback observing despawn cannot trigger a stale collision read against an already-returned pool slot.

---

**6. Telegraph System** (system #6, undesigned) — hard downstream.

| Direction | Interface | Schema owner |
|---|---|---|
| Spawner → Telegraph | `RegisterTelegraph(WaveId, TelegraphWindowS, …)` at admission | Telegraph (future GDD) |
| Spawner → Telegraph | `UnregisterWave(WaveId)` — via Pull-Wave's `OnTelegraphUnregistered` callback | Telegraph (future GDD) |

The spawner provides the `telegraph_window_s` value (snapshotted from `FDPCFrameState` at admission) and the `LeanEaseCurve_Canonical` asset reference. The Telegraph System owns visual / haptic rendering of the lean and the readability validation against `TELEGRAPH_WINDOW_FLOOR_S`.

*Forward contract* (will be inherited by Telegraph GDD when authored): Telegraph SHALL NOT mutate `TelegraphWindowS` after admission. If FLOOR changes mid-run (it currently cannot — FLOOR is `provisional` and would require a config-time event), the spawner's already-admitted waves continue under their snapshot values.

---

**7. Scoring Logic** (system #10, undesigned) — soft transitive.

The spawner does not call scoring directly. Scoring observes wave-survived counts (count of `OnWaveDespawned` with `Reason = NaturalLanding`) and near-miss counts (via Pull-Wave's collision-side delegates).

---

**8. HUD / Audio** (presentation layer, undesigned) — soft.

Presentation systems subscribe to `OnWaveDespawned` (audio cue on near-miss, HUD wave counter) and to a future `OnPatternAdmitted` multicast delegate (audio rhythm + HUD pattern visualization). The `OnPatternAdmitted` delegate is a forward contract on this GDD; signature TBD when HUD GDD is authored.

---

**C.3 §9 — Telemetry** — soft (analytics layer, undesigned).

The spawner emits the following 9 events. **R2a 2026-06-22 schema expansion per qa-lead B-R2-2**: prior table listed only 5 events; the 4 EC-class runtime defense events (emitted under EC-WS-5 + EC-WS-15 + EC-WS-17 + EC-WS-17 post-load) were defined in G.3's rate-limit table but had no payload schema here — AC-WS-23's "payload schema" verification was unverifiable for those events. Schema is now complete; lives downstream in a future `design/gdd/telemetry.md` which inherits this table.

| Event | Fields | Rule |
|---|---|---|
| `barrage_dropped_due_to_concurrency` | `(wave_id_intended, t_norm_PEAK, available_slots)` | Rule 7 |
| `peak_min_barrage_floor_undershoot` | `(barrages_admitted_this_PEAK, target_min)` | Rule 7 (PEAK exit) |
| `pattern_admitted` | `(WaveId, pattern_id, current_phase, is_barrage)` | Rule 8 (informational) |
| `pause_flush_executed` | `(waves_flushed, time_to_drain_ms)` | Rule 13 |
| `run_termination_flush_executed` | `(waves_flushed, despawn_reason)` | Rule 14 |
| `pool_exhaustion_detected` | `(wave_id_intended, tick_stamp, available_slots)` | EC-WS-5 (non-Shipping asserts; Shipping skips admission) |
| `empty_pool_at_draw` | `(phase, tick_stamp)` | EC-WS-15 (defense-in-depth after Rule 15 `POOL_NON_EMPTY` cook check) |
| `pattern_asset_invalid_at_load` | `(asset_id, violation_type)` | EC-WS-17 (per-asset deduplicated; one emission per invalid asset per session) |
| `critical_pool_empty_post_load` | `(pool_name)` | EC-WS-17 (terminal — spawner remains in `Idle` for the run) |

---

#### Test-only interaction

**10. Seam 13 `FWaveSpawnerCallbackTestStub`** — test fixture.

Re-entrant `SetOnDespawnedUserCallback(TFunction<void(int32, EWaveDespawnReason, int32)>)` slot fired from inside `OnWaveDespawned` AFTER standard event recording (R10c). Enables AC-PW-MID-TICK-PAUSE-DEFERRAL test scenarios. Production `IWaveSpawnerCallback` MUST NOT expose this surface — adding it to production would violate the test/production seam separation and create production reachable code paths that no production system needs.

---

#### Engine subsystem integration (informational)

The Wave Spawner is implemented as a `UGameInstanceSubsystem` (locked per OQ-WS-3 closure R1a 2026-06-21 — performance-driven decision per perf-analyst R7; `UWorldSubsystem` rejected because per-replay re-init would fire a 16.6ms pool-alloc hitch per death-retry, materially degrading the core retry loop). The class is `UWaveSpawnerSubsystem` (U-prefix per UE convention for UObject-derived subsystems). Pool storage is `UPROPERTY() TArray<TObjectPtr<AWave>>` to prevent GC (UE 5.0+ TObjectPtr modernization of legacy raw-pointer pattern; sibling to seam-doc Seam 13 modernization). The implementation ADR (`docs/architecture/adr-NNNN-wave-spawner-subsystem-hosting.md`, sibling to OQ-PW-3 HISM/ISMC ADR) documents the locked decision and its lifecycle implications; ADR authorship does not re-adjudicate.

**R2a-1 tick host inscription** (2026-06-22 per CD R2 synthesis — closes 3-spec convergence on tick host gap): `UGameInstanceSubsystem` does NOT tick by default in UE 5.7. `UWaveSpawnerSubsystem` MUST additionally inherit from `FTickableGameObject` (multiple inheritance: `class UWaveSpawnerSubsystem : public UGameInstanceSubsystem, public FTickableGameObject`) and implement:

- `virtual void Tick(float DeltaTime) override` — the per-tick admission decision flow (Rule 1).
- `virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UWaveSpawnerSubsystem, STATGROUP_Tickables); }` — Unreal Insights / `stat unit` scope marker.
- `virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Conditional; }` — paired with `virtual bool IsTickable() const override { return CurrentLifecycleState == ECold ? false : (CurrentLifecycleState == EIdle ? false : true); }` to disable tick in `Cold`/`Idle` states and avoid idle CPU cost.

**R2a-2 tick ordering pin** (2026-06-22 per CD R2 synthesis — closes 3-spec convergence on tick-ordering gap): rather than relying on UE tick groups (FTickableGameObject lacks the standard `AddTickPrerequisiteActor`/`AddTickPrerequisiteComponent` primitives that components/actors have), the spawner subscribes to DPC's post-tick delegate. Concretely:

- DPC publishes its `FDPCFrameState` once per tick at the END of its tick callback via a new `OnPostTickFrameStatePublished : DECLARE_MULTICAST_DELEGATE_OneParam(FOnPostTickFrameStatePublished, const FDPCFrameState&)` (forward contract on DPC; filed under Dependencies § Bidirectional consistency row 1).
- `UWaveSpawnerSubsystem::Initialize()` binds to this delegate; the `Tick(DeltaTime)` override is reduced to bookkeeping (rate-limit timer state) — the *admission decision* fires inside the bound delegate callback, NOT inside `Tick`. This makes the spawner's admission read of `FDPCFrameState` structurally ordered after DPC's per-tick computation.
- RSM events similarly arrive via `OnPausedChanged(bool)` multicast (Rule 13); RSM's tick precedes DPC's in the Pull-Wave-pinned tick chain (`RSM → DPC → PM → Pull-Wave → Telegraph → Collision`), so RSM state is already finalized when the DPC delegate fires.
- AC-WS-13 (replay determinism) now has a structural ordering guarantee: identical RSM `RunSeed` + identical DPC frame-state stream → identical admission stream, regardless of tick-group placement.

**R2a-4 pool pre-allocation lifecycle fix** (2026-06-22 per CD R2 synthesis — closes unreal-specialist B-R2-1 on UE 5.7 `Initialize()` lifecycle): R1a-9 anchored pool pre-allocation at `UGameInstanceSubsystem::Initialize()`, but UWorld does NOT exist at that hook in UE 5.7 (`Initialize()` runs before `UGameInstance::LoadComplete()` / first world load → `GetWorld()` returns null → `SpawnActor` asserts). The corrected hook is `FCoreUObjectDelegates::PostLoadMapWithWorld` (or `UGameInstance::OnWorldChanged` first-fire), which fires once on first world load with a valid UWorld. Pool pre-allocation runs exactly once per session at this hook; replay re-entry (`Flushing → Cold → Active` per EC-WS-8) reuses the existing pool — pool pre-allocation is NEVER re-fired on `Cold` re-entry. `UGameInstanceSubsystem::Deinitialize()` releases the pool on game-instance teardown. The one-frame allocation hitch lands during first world-load (concurrent with level streaming and HUD construction), masking it behind the same loading frame the user already accepts.

#### Cross-system interface table (summary)

| # | Counterpart | Direction | Type | Interface |
|---|---|---|---|---|
| 1 | DPC | in | hard | `FDPCFrameState` snapshot |
| 2 | Pull-Wave | in | hard | Wave object schema + `LeanEaseCurve_Canonical` + per-wave state machine |
| 3 | RSM | in | hard | `current_state`, `is_paused`, `resume_grace`, `OnPausedChanged` delegate |
| 4 | Pull-Wave | out | hard | `IWaveSpawnerCallback` (3-step ordered pipeline) |
| 5 | Collision (undesigned) | out (via 4) | hard | `UnregisterWave(WaveId)` via Pull-Wave's production wrapper |
| 6 | Telegraph (undesigned) | out | hard | `RegisterTelegraph` + `UnregisterWave` via Pull-Wave's production wrapper |
| 7 | Scoring (undesigned) | out | soft transitive | Despawn-event consumer |
| 8 | HUD / Audio (undesigned) | out | soft | `OnPatternAdmitted` multicast (future contract) |
| 9 | Telemetry (undesigned) | out | soft | Event emission |
| 10 | Seam 13 test stub | out | test | `SetOnDespawnedUserCallback` re-entrant slot |

## Formulas

Each formula below provides a named expression, a variable table, an output range, behaviour at extremes, and a worked example. The Wave Spawner derives no values from scratch when an upstream system owns the formula — F-8 (`F-BARRAGE-SURVIVABILITY-INVARIANT`) is inherited from Pull-Wave and referenced here without re-derivation; `lateral_distance_m` is intentionally NOT defined here (Pull-Wave owns it as F-TRAJ-LATERAL — see Dependencies).

---

### F-1: Slot accounting (current concurrent)

The slot-accounting formula is defined as:

`current_concurrent = scheduled + live`

**Variables:**

| Variable | Symbol | Type | Range | Description |
|---|---|---|---|---|
| `scheduled` | s | int32 | `[0, max_concurrent_waves]` | Waves admitted but not yet in Pull-Wave `LEANING` state (pre-committed slots) |
| `live` | l | int32 | `[0, max_concurrent_waves]` | Waves in Pull-Wave states `{LEANING, TRAVERSING, LANDED}` |
| `current_concurrent` | c | int32 | `[0, max_concurrent_waves]` | Total occupied slots this tick |
| `max_concurrent_waves` | M | int32 | `[1, MAX_CONCURRENT_WAVES_CAP=16]` | Per-tick cap from `FDPCFrameState` |

**Output Range:** `current_concurrent ∈ [0, max_concurrent_waves]`. The structural ceiling `MAX_CONCURRENT_WAVES_CAP = 16` caps the phase cap itself, not the runtime counter directly. By admission invariant, `scheduled + live ≤ max_concurrent_waves` at every tick boundary.

**Behaviour at extremes:**

- `current_concurrent == max_concurrent_waves`: admission gate closed; new candidates either deferred (barrage with `barrage_owed`) or skipped (non-barrage waiting for cadence interval to elapse).
- `current_concurrent == 0`: spawner is idle or just exited `Flushing`; the next admission has full slots available.

**Example:** PEAK, `max_concurrent_waves = 3`. A barrage admitted at tick T: `scheduled = 3, live = 0 → current_concurrent = 3`. By tick T+5 (assuming 60fps and ~83ms elapsed), all three onsets have fired: `scheduled = 0, live = 3 → current_concurrent = 3` (unchanged — the same three waves, just migrated across the layer boundary).

---

### F-1b: Available slots (named extraction)

Extracted as a named expression for citation by Rule 7 admission checks (`available ≥ 1` for non-barrage; `available ≥ 3` for barrage) and by AC traceability.

`available = max_concurrent_waves − current_concurrent`

**Variables:** all inherited from F-1.

**Output Range:** `available ∈ [0, max_concurrent_waves]`. Drops to zero exactly when the cap is saturated.

---

### F-2: Object pool size derivation

The object-pool-size formula is defined as:

`pool_size = MAX_CONCURRENT_WAVES_CAP + DESPAWNING_RETURN_LATENCY_SLOTS + MAX_CONCURRENT_WAVES_CAP_CEIL_MARGIN`

`pool_size = 16 + 2 + 5 = 23`

**Variables:**

| Variable | Symbol | Type | Range | Description |
|---|---|---|---|---|
| `MAX_CONCURRENT_WAVES_CAP` | C | int32 (const) | 16 | Structural ceiling on `max_concurrent_waves` (DPC; registered) |
| `DESPAWNING_RETURN_LATENCY_SLOTS` | D | int32 (const) | 2 | Slots in Pull-Wave `DESPAWNING` state not yet returned to the pool (covers Rule 12 3-step pipeline one-frame deferral) |
| `MAX_CONCURRENT_WAVES_CAP_CEIL_MARGIN` | μ | int32 (const) | 5 | Headroom above `C + D`; absorbs burst overlap and future cap raises up to `C + μ = 21` |
| `pool_size` | P | int32 (derived) | exactly 23 | Pre-allocated pool count; not a runtime variable |

**Output Range:** exactly 23. This is a derived constant computed once at design time and hard-coded into pool pre-allocation. Cross-knob trigger: if `MAX_CONCURRENT_WAVES_CAP` ever raises above `16 + 5 = 21`, `pool_size` must be recalculated — flagged in Tuning Knobs.

**Example:** PEAK at maximum concurrency. `live = 3` waves in LEANING/TRAVERSING/LANDED. One wave transitions to `DESPAWNING` before its Rule 12 pipeline completes (one frame). Pool inventory: 3 live + 1 DESPAWNING + 19 idle = 23 total. No `UWorld::SpawnActor` call required.

---

### F-3: Cadence governor weight (PEAK barrage probability)

The cadence-governor weight formula is defined as:

```
w_barrage(t_norm, k, base_w) =
  ┌ 0.0                                                       if k ≥ BARRAGE_EVENTS_PER_PEAK_MAX
  ├ BARRAGE_FORCE_FLOOR_W                                     if k < BARRAGE_EVENTS_PER_PEAK_MIN ∧ t_norm ≥ T_FORCE
  └ clamp(base_w + λ · (BARRAGE_EVENTS_PER_PEAK_TARGET_AVG − k), 0.0, W_CEILING)  otherwise
```

**Variables:**

| Variable | Symbol | Type | Range | Description |
|---|---|---|---|---|
| `t_norm` | τ | float | `[0, 1]` | Fraction of PEAK phase elapsed |
| `k` | k | int32 | `[0, BARRAGE_EVENTS_PER_PEAK_MAX = 3]` | `barrages_admitted_this_PEAK` count |
| `base_w` | β | float | `[0.15, 0.40]` (cook-time advisory) | Unweighted barrage fraction: `|PEAK_barrage_pool| / |PEAK_pool_total|` |
| `BARRAGE_EVENTS_PER_PEAK_TARGET_AVG` | T_avg | int32 (const) | 2 | Registry-locked (DPC) |
| `BARRAGE_EVENTS_PER_PEAK_MIN` | T_min | int32 (const) | 1 | Registry-locked floor (DPC) |
| `BARRAGE_EVENTS_PER_PEAK_MAX` | T_max | int32 (const) | 3 | Registry-locked ceiling (DPC) |
| `BARRAGE_FORCE_FLOOR_W` | F | float (tuning) | `[0.7, 0.95]`, default `0.80` | Barrage draw weight when forced; must satisfy `F > W_CEILING` |
| `T_FORCE` | T_f | float (tuning) | `[0.4, 0.7]`, default `0.50` | Normalized PEAK time after which zero-barrage state triggers forced draw |
| `λ` | λ | float (tuning) | `[0.05, 0.30]`, default `0.15` | Proportional drive rate toward `TARGET_AVG` |
| `W_CEILING` | W_c | float (tuning) | `[0.3, 0.6]`, default `0.50` | Max barrage weight in proportional branch; must satisfy `W_c < F` |
| `w_barrage` | w | float | `[0.0, F]` | Output barrage draw weight |

**Output Range:** `w_barrage ∈ [0.0, BARRAGE_FORCE_FLOOR_W]`. Three branches:

- **Ceiling hit** (`k ≥ T_max`): `w = 0` (never draw barrage; enforces `MAX = 3` hard cap).
- **Floor forcing** (`k < T_min ∧ τ ≥ T_FORCE`): `w = F` (heavy bias to satisfy `MIN = 1` without invoking Rule 7 `barrage_owed`).
- **Proportional** (otherwise): `w` drifts up when behind `T_avg`, down when ahead.

**Cross-knob invariants (binding):**

- `W_CEILING < BARRAGE_FORCE_FLOOR_W` — otherwise a proportional-branch draw at ceiling is indistinguishable from a forced draw (force-draw mode loses its semantic).
- `base_w < W_CEILING` — **R2a 2026-06-22 per systems-designer B-SD-1**: G.1 safe ranges overlap (β ∈ [0.15, 0.40], W_c ∈ [0.30, 0.60]) so β ≥ W_c is legal at safe-range boundaries (e.g., β = 0.40, W_c = 0.30). When β ≥ W_c, F-3's proportional branch `clamp(β + λ(T_avg − k), 0, W_c)` silently saturates at `W_c` for all k < T_max — the λ-drive feedback is killed; the tuner sees no signal. This invariant prevents the silent saturation. Enforce via cook-time check `PEAK_BASE_W_BELOW_CEILING: base_w < W_CEILING` (add to Rule 15).

Cook-time advisory: `base_w` MUST ALSO satisfy `0.15 ≤ base_w ≤ 0.40` (i.e., the PEAK barrage sub-pool is between 15% and 40% of the PEAK total pool by entry count). Outside this band, the `λ`-drive cannot suppress the session-sample average toward `T_avg = 2` while honoring `MAX = 3`. Enforce via cook-time check `PEAK_BASE_W_RANGE: 0.15 ≤ |PEAK_barrage_pool| / |PEAK_pool_total| ≤ 0.40` (add to Rule 15).

**Behaviour at extremes:**

- PEAK first tick (`τ = 0, k = 0`): proportional branch evaluates `w = clamp(β + 0.15 × 2, 0, 0.50) = clamp(β + 0.30, 0, 0.50)`. With `β = 0.25`: `w = 0.50`.
- PEAK final tick (`τ ≈ 1`) with `k = 2`: proportional branch `w = clamp(β + 0.15 × 0, 0, 0.50) = β = 0.25`. No forcing, no ceiling.
- PEAK midway (`τ = 0.6`) with `k = 0`: forcing branch fires `w = 0.80`.

**Example:** `τ = 0.6, k = 0, β = 0.25`.

- Branch 1 (`k ≥ 3`)? No.
- Branch 2 (`k < 1 ∧ τ ≥ 0.50`)? **YES.**
- `w_barrage = BARRAGE_FORCE_FLOOR_W = 0.80`.

The next admission has 80% probability of drawing from the PEAK barrage sub-pool, satisfying the `MIN = 1` floor in ≥95% of runs without invoking the `barrage_owed` reservation.

---

### F-3b: Cadence gate (Rule 1 timing)

The cadence-gate formula is defined as:

`admits ⇔ (now − last_spawn_time) ≥ wave_spawn_interval_s`

**Variables:**

| Variable | Symbol | Type | Range | Description |
|---|---|---|---|---|
| `now` | t | float (seconds) | `[0, 60.0]` | Current run time |
| `last_spawn_time` | t₀ | float (seconds) | `[0, now]` | Timestamp of last admitted pattern |
| `wave_spawn_interval_s` | Δ | float (seconds) | `[0.25, 4.0]` | Per-phase cadence interval from `FDPCFrameState` |
| `admits` | A | bool | `{true, false}` | Whether the cadence gate passes this tick |

**Output Range:** Boolean. Gate opens at the tick where elapsed gap first meets or exceeds `Δ`.

**Behaviour at extremes:**

- First admission of the run (`last_spawn_time = 0`): gate passes at the first tick where `now ≥ Δ`.
- PEAK with `Δ = 1.0s`: 1 admission/second steady state.
- OPENER with `Δ = 4.0s`: 1 admission per 4 seconds (cf. Rule 2a primer is the *first* of these).

**Example:** PEAK, `Δ = 1.0s`. Last spawn at `t₀ = 12.0s`. At `t = 12.8s`: `0.8 < 1.0` → gate fails (Holding). At `t = 13.0s`: `1.0 ≥ 1.0` → gate passes, proceed to Rule 7.

---

### F-4: Phase drain window

The phase-drain-window formula is defined as:

`drain_start_t = phase_end_t − max_pattern_length_s`

Admission is suppressed (Rule 9) when: `now ≥ drain_start_t ∧ new_pattern_required`

**Variables:**

| Variable | Symbol | Type | Range | Description |
|---|---|---|---|---|
| `phase_end_t` | t_e | float (seconds) | `[0, 60.0]` | Absolute time at which the phase transition fires |
| `max_pattern_length_s` | L | float (seconds) | `{OPENER: 2.0, MID: 3.0, PEAK: ∞}` | Per-phase max pattern length (Rule 2) |
| `drain_start_t` | t_d | float (seconds) | `[t_e − 3.0, t_e]` | Earliest time at which no new pattern is admitted |
| `now` | t | float (seconds) | `[0, 60.0]` | Current tick time |

**Output Range:** `drain_start_t ∈ [phase_end_t − 3.0, phase_end_t]`. For OPENER (L = 2.0) and MID (L = 3.0) the window is deterministic. For PEAK, `L = ∞` ⇒ no drain window — PEAK → {DEAD, COMPLETE, ABORTED} is governed unconditionally by Rule 14.

**Behaviour at extremes:**

- `now < drain_start_t`: admission proceeds normally.
- `now ≥ drain_start_t ∧ now < phase_end_t`: in-flight patterns complete; no new patterns admitted. Produces the 1–4s rest beat for Pillar 3.
- `now ≥ phase_end_t`: Rule 10 atomic swap fires; new admissions draw from the new phase's pool.

**Example:** MID ends at `phase_end_t = 45.0s`, `L = 3.0s`. `drain_start_t = 42.0s`. At `now = 42.5s`, Rule 9 suppresses new admission. At `now = 43.7s`, PEAK pool swap fires per Rule 10.

---

### F-5: Barrage simultaneity span (cook-time check)

The barrage-simultaneity-span formula is defined as:

`last_onset_t − first_onset_t ≤ BARRAGE_SIMULTANEITY_WINDOW_S`

**Variables:**

| Variable | Symbol | Type | Range | Description |
|---|---|---|---|---|
| `first_onset_t` | o₁ | float (seconds) | `[0, ∞)` | Earliest telegraph onset in the authored barrage pattern (relative to pattern start) |
| `last_onset_t` | o_M | float (seconds) | `[o₁, ∞)` | Latest telegraph onset in the authored barrage pattern |
| `BARRAGE_SIMULTANEITY_WINDOW_S` | W | float (const) | 0.35 (= `TELEGRAPH_WINDOW_FLOOR_S / 2`) | Registry-locked |

**Output Range:** Pass/fail. The span lies in `[0, W]`. Span = 0 is valid (M = 3 onsets fire on the same frame — single-frame barrage). Spans > W fail cook.

**Behaviour at extremes:**

- Single-frame barrage (`o_M = o₁`): cleanest perceptual signature; allowed.
- Spread-to-window barrage (`o_M − o₁ = W = 0.35`): boundary case; cook accepts.
- `o_M − o₁ > W`: cook FAILS; build refused.

At any PATH (i) FLOOR raise: `W = new_FLOOR / 2` (binding per F-7); this check re-runs against the updated W.

---

### F-6: Non-barrage stagger floor (cook-time check)

The non-barrage-stagger floor formula is defined as:

`min(consecutive_onset_delta) ≥ TELEGRAPH_WINDOW_FLOOR_S`

**Variables:**

| Variable | Symbol | Type | Range | Description |
|---|---|---|---|---|
| `consecutive_onset_delta` | δᵢ | float (seconds) | `[0, ∞)` | Time gap between any two consecutive onsets within a non-barrage pattern |
| `TELEGRAPH_WINDOW_FLOOR_S` | F | float (const) | 0.70 | Registry-locked Pillar 2 minimum telegraph window |

**Output Range:** Pass/fail. Any non-barrage pattern with `min(δᵢ) < F` fails cook.

**Behaviour at extremes:**

- Single-onset pattern (most OPENER patterns): `min` over an empty consecutive-pair set is vacuously satisfied — tooling treats this as a no-op pass.
- Two-onset non-barrage with `δ = 0.70`: boundary case; cook accepts.
- `δ < 0.70`: cook FAILS — the pattern is structurally a covert barrage and must be re-authored as `is_barrage = true` (subject to F-5) or its onsets must be re-timed.

---

### F-7: FLOOR/W ratio (registry binding contract)

The FLOOR-to-W ratio formula is defined as:

`BARRAGE_SIMULTANEITY_WINDOW_S = TELEGRAPH_WINDOW_FLOOR_S / 2`

At current values: `0.35 = 0.70 / 2`.
At maximum admissible FLOOR (`TELEGRAPH_WINDOW_FLOOR_MAX_ADMISSIBLE_S = 0.76s`): `W = 0.38s`.

**Variables:**

| Variable | Symbol | Type | Range | Description |
|---|---|---|---|---|
| `TELEGRAPH_WINDOW_FLOOR_S` | F | float (const) | `[0.70, 0.76]` | Current FLOOR, bounded by `TELEGRAPH_WINDOW_FLOOR_MAX_ADMISSIBLE_S` |
| `BARRAGE_SIMULTANEITY_WINDOW_S` | W | float (const) | `[0.35, 0.38]` | Derived: `F / 2`; must be updated atomically with F |

**Output Range:** Not a runtime evaluation — this is a registry-update contract. Whenever `TELEGRAPH_WINDOW_FLOOR_S` changes via CD PATH (i) ruling, `BARRAGE_SIMULTANEITY_WINDOW_S` must be updated in the same commit per ADR-0004 dual-grep methodology. Failure to do so breaks F-5's cook-time check for all barrage patterns simultaneously.

---

### F-8: F-BARRAGE-SURVIVABILITY-INVARIANT (inherited)

This formula is owned by Pull-Wave; this section references it without re-derivation. The Wave Spawner's curation (Rule 4 triplet exclusion + Rule 5 PEAK barrage tier floor + Rule 6 stagger separation + Rule 15 cook-time checks) is the *constructive proof* the invariant holds.

`SLIP_TWEEN × MIN_ESCAPE_SLIPS + REACT ≤ TELEGRAPH_WINDOW_FLOOR_S`

**Variables:**

| Variable | Symbol | Type | Range | Description |
|---|---|---|---|---|
| `SLIP_TWEEN` | s_t | float (const) | 0.15s | Lane-change tween duration (Pull-Wave / Player Movement) |
| `MIN_ESCAPE_SLIPS` | n | int32 (const) | 2 | Worst-case escape-slip count from any player lane across the 7 surviving triplets |
| `REACT` | r | float | `[0.20, 0.25]` | `REACTION_BUDGET` — default 0.20s, ceiling 0.25s |
| `TELEGRAPH_WINDOW_FLOOR_S` | F | float (const) | 0.70s | Pillar 2 contractual minimum |

**Output Range:** Evaluated at both REACT endpoints.

- REACT default 0.20s: `0.15 × 2 + 0.20 = 0.50 ≤ 0.70` ✓ — **200ms** (12 frames @ 60fps) margin.
- REACT ceiling 0.25s: `0.15 × 2 + 0.25 = 0.55 ≤ 0.70` ✓ — **150ms** (9 frames @ 60fps) margin.

At maximum-admissible FLOOR (0.76s): both REACT margins improve by 60ms.

---

### F-10: Lean magnitude tier derivation

The lean-magnitude-tier formula is defined as:

`lean_magnitude_tier = abs(TargetLane − SourceLane)`

**Variables:**

| Variable | Symbol | Type | Range | Description |
|---|---|---|---|---|
| `TargetLane` | L_t | int32 | `[0, NUM_LANES − 1] = [0, 4]` | Destination lane index (0 = leftmost, 4 = rightmost) |
| `SourceLane` | L_s | int32 | `[0, NUM_LANES − 1] = [0, 4]` | Origin lane index |
| `lean_magnitude_tier` | T | int32 | `[0, NUM_LANES − 1] = [0, 4]` | Output tier index; feeds LEAN_ANGLE_TIER{0–4}_DEG and PEAK barrage Rule 5 check |

**Output Range:** `[0, 4]`. Tier 0 = source == target (no lateral move, valid ONLY in PEAK non-barrage per Rule 2 shape invariant — see "Behaviour at extremes" below). Tier 4 = full 5-lane shift (valid in PEAK barrage if authored there; Rule 5 forces all PEAK barrage `T ≥ 2`).

**Behaviour at extremes:**

- `T = 0`: source == target (no lateral move). **Per Rule 2 shape invariant table, T = 0 is valid ONLY in PEAK non-barrage** — OPENER allowed tiers `{1, 2}` and MID allowed tiers `{1, 2, 3}` exclude T = 0; PEAK barrage forbids T = 0 by Rule 5. R1a 2026-06-21 fix per systems-designer B5: prior wording "valid in OPENER + MID + PEAK non-barrage" contradicted Rule 2's tier sets; corrected to PEAK non-barrage only. Rule 2 is the authoritative oracle for per-phase tier admissibility; F-10's domain is the union of all valid lane shifts but per-phase admissibility is bounded by Rule 2.
- `T = 1`: 1-lane shift; FORBIDDEN in PEAK barrage by Rule 5 (tier-1 ban).
- `T = 4`: full sweep; only valid if PEAK barrage triplet set includes a pattern with `|L_t − L_s| = 4` (the 7 surviving triplets each contain at least one such pair, e.g., `{0,1,4}` has `|4 − 0| = 4`).

Cascades on any `NUM_LANES` change: if `NUM_LANES` raises to 6, the valid range becomes `[0, 5]` and `LEAN_ANGLE_TIER5_DEG` must be introduced; Rule 15 `PEAK_BARRAGE_MIN_TIER` and the 7-surviving-triplet enumeration must be re-derived.

**Example:** `SourceLane = 2` (center), `TargetLane = 4` (rightmost). `T = |4 − 2| = 2`. Satisfies Rule 5's `T ≥ 2` PEAK barrage floor; admits a 2-lane-shift barrage onset; maps to `LEAN_ANGLE_TIER2_DEG = 22°`.

---

### Formula → Rule cross-reference

| Formula | Used in |
|---|---|
| F-1, F-1b | Rule 1 (gate), Rule 7 (admission), States and Transitions (invariants I-1 through I-4) |
| F-2 | Rule 11 (object pool) |
| F-3, F-3b | Rule 8 (cadence governor), Rule 1 (cadence gate) |
| F-4 | Rule 9 (drain), Rule 10 (atomic swap) |
| F-5 | Rule 6 (barrage simultaneity), Rule 15 cook-time `BARRAGE_W_SPAN` |
| F-6 | Rule 6 (non-barrage stagger), Rule 15 cook-time `NON_BARRAGE_STAGGER` |
| F-7 | Rule 15 (binding registry contract on FLOOR change) |
| F-8 | Rule 4 (constructive proof via 7-triplet enumeration), Rule 5 (tier floor) |
| F-10 | Rule 5 (PEAK barrage tier floor), Rule 7 (admission) |

## Edge Cases

Each edge case follows the template **Condition → Resolution → Rationale**. Cases EC-WS-1 through EC-WS-18 cover the spawner's reachable behaviour space; cases identified by systems-designer review (Q5 round) are marked as such.

---

**EC-WS-1 — Pause flush during barrage atomic admission.**

- *Condition*: A barrage candidate has just passed Rule 7's `available ≥ 3` check (`scheduled += 3`) but onset 1 has not yet fired. `OnPausedChanged(true)` fires on the same tick.
- *Resolution*: The 3 `SPAWNED`-state waves are flushed via Rule 13's pause-flush per Rule 12 ordered pipeline, each with `Reason = PauseFlush`. `barrage_owed` is NOT set (the barrage was fully admitted, not dropped).
- *Rationale*: `scheduled += 3` is a single statement on the game thread (Rule 7 atomicity contract); pause cannot interrupt mid-statement. The barrage's three `SPAWNED` waves are committed and the flush handles them like any other in-flight wave.

**EC-WS-2 — Run termination during barrage atomic admission.**

- *Condition*: As EC-WS-1 but RSM `RUNNING → {DEAD, COMPLETE, ABORTED}` fires instead.
- *Resolution*: As EC-WS-1 but `Reason = RunTermination`. The 3 `SPAWNED` waves are flushed; in-flight collision callbacks continue to process per Pull-Wave Rule 13 (terminal hit bookkeeping closes against the just-ended run).
- *Rationale*: Rule 14 is unconditional and supersedes Rule 7 mid-admission. The atomic-pre-commitment guarantee remains: either zero or three slots are committed, never partial.

**EC-WS-3 — Multi-onset non-barrage straddles MID → PEAK boundary.** *(corrected from initial draft — MID has no barrages per Rule 2 + Rule 15 `MID_NO_BARRAGE`)*

- *Condition*: A non-barrage multi-onset pattern is admitted at tick T just before `drain_start_t` (Rule 9). Its first onset fires inside MID; its second or third onset fires after the PEAK pool swap (Rule 10) at tick T+N.
- *Resolution*: The in-flight pattern completes under the admission-time `telegraph_window_s` snapshot (0.82s for MID, frozen at admission per Rule 10). The atomic pool swap at the transition tick applies to NEW draws only; in-flight waves are not despawned or re-parametrized.
- *Rationale*: Consistency with Rule 10's immutable-per-instance contract (mirrors Pull-Wave R1 RC-C's `FPullWaveCurveSnapshot` precedent). The pattern uses the phase it was born in.

**EC-WS-4 — `barrage_owed = true` at PEAK exit.**

- *Condition*: A barrage was dropped mid-PEAK (Rule 7) and `barrage_owed` was set. PEAK ends before `available ≥ 3` ever opened.
- *Resolution*: Rule 14 (PEAK → DEAD/COMPLETE/ABORTED) clears `barrage_owed`. If `barrages_admitted_this_PEAK < BARRAGE_EVENTS_PER_PEAK_MIN = 1`, emit `peak_min_barrage_floor_undershoot` telemetry.
- *Rationale*: The floor-undershoot is a tuning-regression signal — the cadence governor (Rule 8) plus reservation should make this rare in practice. A logged undershoot prompts cadence-knob review.

**EC-WS-5 — Pool exhaustion (should be structurally impossible).** *(corrected — fallback to `UWorld::SpawnActor` violates Rule 11)*

- *Condition*: F-2 guarantees `pool_size = 23 ≥ MAX_CONCURRENT_WAVES_CAP + DESPAWNING_RETURN_LATENCY_SLOTS + MARGIN`. A bug causes pool exhaustion in development.
- *Resolution*: In non-Shipping: `check()` assertion fires (developer-facing). In Shipping: the spawner drops this admission candidate (skip the tick), emits `pool_exhaustion_detected` telemetry once per session. **NO** `UWorld::SpawnActor` fallback — that would violate Rule 11.
- *Rationale*: Preserve the pool invariant under all production conditions. A development bug should fail loudly, not silently break the contract.

**EC-WS-6 — Concurrent `OnPausedChanged` events in one tick.**

- *Condition*: RSM somehow fires `OnPausedChanged(true)` AND `OnPausedChanged(false)` in the same tick (e.g., rapid pause-tap by player).
- *Resolution*: The spawner processes events in the order RSM delivers them within step (i) of Rule 1's intra-tick order. The final state at end-of-step-(i) determines `bPauseFlushPending`. If final state is paused, flush at next tick top. If final state is running, no flush — spawner is already in `Holding` for resume_grace.
- *Rationale*: Deterministic event ordering is required for replay determinism. The "final state wins" rule is the natural read of a queue of `bool` mutations.

**EC-WS-7 — First-ever admission at OPENER tick-0 (primer path).**

- *Condition*: RSM transitions COUNTDOWN → RUNNING. Spawner state moves Cold → Active. Rule 2a primer admission fires.
- *Resolution*: The per-run RNG seed is captured at the `Cold → Active` transition tick BEFORE the primer is drawn (so primer admission is deterministic). The primer is then admitted under Rule 2a's cadence-gate bypass (no `wave_spawn_interval_s_OPENER = 4.0s` wait). `last_spawn_time = now` post-primer.
- *Rationale*: Death Replay must restore the seed and replay the primer identically. The bypass is the only way "first 10 seconds teaches the telegraph" can land.

**EC-WS-8 — Replay (second run from same session).**

- *Condition*: Player taps Replay after death. RSM transitions DEAD → COUNTDOWN → RUNNING. Spawner state moves Flushing → Cold → Active.
- *Resolution*: All per-run state resets on `Cold` entry: `barrage_owed = false`, `barrages_admitted_this_PEAK = 0`, primer freshly available. A NEW per-run RNG seed is captured at the new Cold → Active transition.
- *Rationale*: Death Replay applies to the just-ended run only. The replay run is a fresh deterministic stream.

**EC-WS-9 — Cook-time `PEAK_BASE_W_RANGE` violation reached at runtime.** *(collapsed — already handled by Rule 15)*

- *Condition*: A build ships with `base_w < 0.15 OR base_w > 0.40` despite Rule 15's BLOCKING cook-time check.
- *Resolution*: The build refuses at cook time. No runtime handling required. See Rule 15.
- *Rationale*: Cook-time is the primary gate; defense-in-depth at runtime is unnecessary because the cook check is BLOCKING (build cannot ship).

**EC-WS-10 — PATH (i) FLOOR raise mid-session.**

- *Condition*: A config-time event attempts to change `TELEGRAPH_WINDOW_FLOOR_S` mid-run.
- *Resolution*: FLOOR changes are NOT permitted mid-run. `TELEGRAPH_WINDOW_FLOOR_S` is `provisional` and only updates at game-start config load. In-flight waves continue under their admission-time `telegraph_window_s` snapshot per Rule 10.
- *Rationale*: F-7's FLOOR/W binding contract requires a lockstep update of `BARRAGE_SIMULTANEITY_WINDOW_S`; mid-run changes would tear the invariant. Restricting to config-time enforces the contract.

**EC-WS-11 — Concurrent RSM termination + DPC phase transition same tick.** *(systems-designer Q5 round)*

- *Condition*: RSM fires `RUNNING → DEAD` on the same tick that DPC transitions MID → PEAK.
- *Resolution*: Per Rule 1's intra-tick order, RSM events drain first. Rule 14 supersedes Rule 10; the spawner enters `Flushing` via RunTermination. The pool swap is moot because no new admissions occur post-Rule 14. The MID → PEAK transition is observed but unconsumed.
- *Rationale*: Rule 14 is unconditional. The pool-swap mechanism only matters for *future* admissions, of which there are none after termination.

**EC-WS-12 — Primer admission cadence-gate bypass.** *(systems-designer Q5 round — contract gap closed by Option A patch)*

- *Condition*: On `Cold → Active`, `last_spawn_time` is uninitialized or `0.0`. Rule 1's gate `(now − last_spawn_time) ≥ wave_spawn_interval_s` evaluated naively would block the primer until `t = 4.0s`, voiding the "first 10 seconds teaches the telegraph" anchor.
- *Resolution*: Rule 2a's explicit cadence-gate bypass (added 2026-06-20 — see Rule 2a "Primer cadence-gate bypass" clause) handles this. The primer is admitted immediately on `Cold → Active`; `last_spawn_time = now` post-primer.
- *Rationale*: Option A from the systems-designer Q5 reconciliation — semantically explicit, matches the existing `is_primer` special-flag framing.

**EC-WS-13 — DPC `is_active` flip during admission flow.** *(systems-designer Q5 round)*

- *Condition*: DPC sets `is_active = false` mid-run (e.g., internal DPC gate logic triggers a hold). Change occurs between two ticks.
- *Resolution*: The spawner uses the tick-top snapshot exclusively (Rule 1 intra-tick order step ii). The change is observed at the NEXT tick; the spawner transitions Active → Holding that next tick with no flush, no mutation beyond Rule 1 gate fail.
- *Rationale*: DPC's snapshot-per-tick contract makes this structurally impossible mid-tick. Named explicitly so an implementer reading only Rule 1 cannot wonder whether an in-tick DPC callback could interrupt admission.

**EC-WS-14 — Empty sub-pool at draw.** *(systems-designer Q5 round)*

- *Condition*: A content author delivers a valid-but-empty pool (e.g., MID pool with zero patterns) that passes individual-pattern validation. Step 4 of Rule 1's admission flow would produce an empty-draw.
- *Resolution*: Rule 15's BLOCKING cook-time check `POOL_NON_EMPTY` (added 2026-06-20) rejects this at build. Runtime defense-in-depth: non-Shipping asserts; Shipping skips the admission tick and emits `empty_pool_at_draw` telemetry.
- *Rationale*: Cook-time check is structurally cheaper than runtime handling. The runtime arm is purely defense-in-depth.

**EC-WS-15 — Cross-platform Death Replay determinism.** *(systems-designer Q5 round)*

- *Condition*: F-3's piecewise weight uses float arithmetic. IEEE-754 is deterministic per-platform but ARM (iOS/Android) vs x86 (development) can diverge on transcendentals.
- *Resolution*: F-3 currently uses only `clamp` and linear ops — no transcendentals — so it is platform-deterministic today. Death Replay is **platform-scoped**: a run captured on iOS replays only on iOS. Future formula changes that touch this path MUST flag platform-determinism risk.
- *Rationale*: Constraining replay scope is cheaper than fixed-point arithmetic. The constraint is named here and surfaced in Open Questions so a future contributor doesn't break it accidentally.

**EC-WS-16 — `barrage_owed` persists across pause/resume.** *(systems-designer Q5 round)*

- *Condition*: A barrage is dropped immediately before backgrounding. Spawner flushes (Rule 13), enters Idle, then Holding (resume_grace). `barrage_owed` is not cleared by the flush.
- *Resolution*: `barrage_owed` persists through Idle and Holding. It is NOT cleared by Rule 13's pause-flush — only Rule 14's RunTermination clears it. The first post-grace tick in PEAK with `available ≥ 3` executes the reservation admission.
- *Rationale*: Invariant I-5 (`barrage_owed == true ⇒ current_phase == PEAK`) and PEAK identity persists across pause. Clearing on pause would silently drop the `BARRAGE_EVENTS_PER_PEAK_MIN` guarantee for any pause-late-in-PEAK pattern.

**EC-WS-17 — Pattern asset structural validity at load.** *(systems-designer Q5 round)*

- *Condition*: A cook-bypass (direct asset injection in QA, or a hot-patch) ships a pattern with `is_barrage = true ∧ onset_count ≠ 3`, or a non-barrage pattern with `onset_count = 0`.
- *Resolution*: At `Cold` entry (pool pre-allocation), each pooled pattern asset is validated against its structural invariants. Non-Shipping: assert. Shipping: drop the invalid asset, emit `pattern_asset_invalid_at_load` telemetry (once per asset per session). If the resulting valid pool is empty, the spawner emits `critical_pool_empty_post_load` and remains in Idle.
- *Rationale*: Defense-in-depth against cook-pipeline bypass. The cook-time check (Rule 15) is the primary gate; load-time validation is the Shipping long-stop.

**EC-WS-18 — Resume grace expires with zero in-flight waves.** *(systems-designer Q5 round — `last_spawn_time` invariant landed in Rule 13)*

- *Condition*: Player pauses very early in OPENER (only the primer may have been live, possibly already completed). Flush runs, `live = 0, scheduled = 0`. Resume fires; `resume_grace` elapses.
- *Resolution*: Spawner transitions Holding → Active normally. Per the Rule 13 invariant (added 2026-06-20), pause-flush does NOT update `last_spawn_time` — the pre-pause value remains authoritative, preventing a resume-burst where the cadence gate erroneously re-opens immediately.
- *Rationale*: Pause is bookkeeping, not an admission. Treating the flush as a spawn for cadence purposes would shift the spawn rhythm relative to the run timeline, which would be observable to the player.

---

### Dropped candidate edge cases (recorded for traceability)

| EC | Why dropped |
|---|---|
| Float-precision drift in F-3b cadence gate | Worst-case drift over 60s ≈ 2 ms (sub-tick). One-frame-late admission is acceptable; no epsilon needed. Recorded in Tuning Knobs. |
| WaveId int32 overflow | At 60 IDs/run × any plausible session count, int32 max is unreachable in a 60-second mobile runner. Debug-only `check(WaveId > 0)` sentinel; no EC. |
| Statement-level torn write on `scheduled` | Spawner is `UGameInstanceSubsystem` (game-thread-only); no concurrent writer exists. Documented in Rule 7 atomicity contract; no EC. |

## Dependencies

Dependency directions and interface schemas are documented in Detailed Rules § *Interactions with Other Systems* (Section C.3 — 10 counterparts including production interfaces, Seam 13 test stub, and engine integration). This section is the canonical *dependency map* — directionality, hardness classification, and bidirectional-consistency status. It does not duplicate the interface schemas; cross-reference C.3 for those.

### Upstream (the Wave Spawner reads from)

| # | Counterpart | Hardness | Why required | Documented in dependency GDD as "depended on by"? |
|---|---|---|---|---|
| 1 | Difficulty & Phase Controller (`design/gdd/difficulty-phase-controller.md`) | **Hard** | `FDPCFrameState` snapshot drives every admission decision (Rule 1). Without DPC, the spawner has no per-tick parameters and no phase identity. | ✓ DPC line 629 enumerates Wave Spawner bindings (Rule 16 + item 13 atomicity + drain/swap contract) |
| 2 | Pull-Wave Behavior (`design/gdd/pull-wave-behavior.md`) | **Hard** | Defines the `Wave` object schema (`FPullWaveSpawnParams` + per-instance state machine), the `LeanEaseCurve_Canonical` asset, the despawn pipeline ordering invariant (Rule 12 / AC-PW-15), and the 7-surviving-triplet enumeration the spawner enforces at cook time. | ✓ Pull-Wave line 118 explicitly: "Upstream — Wave Spawner (downstream-of-DPC, upstream-of-Pull-Wave). Wave Spawner calls Pull-Wave's `Construct(FPullWaveSpawnParams)` to instantiate each wave." Forward contracts on Wave Spawner enumerated at PW lines 205, 207, 210, 217. |
| 3 | Run State Machine (`design/gdd/run-state-machine.md`) | **Hard** | Provides `current_state`, `is_paused`, `resume_grace` + the `OnPausedChanged(bool)` multicast delegate. Rule 1 gates admission on all three; Rule 13 + Rule 14 are triggered by RSM transitions. The R19 grace contract (RSM line 44) is binding on the spawner. | ⚠ TBD verify in `/consistency-check` — RSM's Dependencies section needs a "depended on by Wave Spawner" entry. Forward contract `OnPausedChanged` multicast is already an RSM forward contract per R2 Cluster F. |

### Downstream (the Wave Spawner writes to)

| # | Counterpart | Hardness | What flows | Bidirectional consistency |
|---|---|---|---|---|
| 4 | Pull-Wave Behavior (callback consumer) | **Hard** | `IWaveSpawnerCallback` 3-method production interface (Rule 12 ordered: collision unregister → telegraph unregister → despawn broadcast). | ✓ Pull-Wave line 1482ff Seam 13 owns the interface schema. |
| 5 | Collision & Hit Detection (`design/gdd/collision-and-hit-detection.md` — system #8, **undesigned**) | **Hard** (transitive via Pull-Wave's production wrapper) | `UnregisterWave(WaveId)` via `OnCollisionUnregistered` callback. | Forward contract pending — Collision GDD will inherit Section C.3's "unregister must precede `OnWaveDespawned` broadcast" rule when authored. |
| 6 | Telegraph System (`design/gdd/telegraph-system.md` — system #6, **undesigned**) | **Hard** | At admission: `RegisterTelegraph(WaveId, TelegraphWindowS, …)`. At despawn: `UnregisterWave(WaveId)` via `OnTelegraphUnregistered` callback. The spawner provides snapshotted `telegraph_window_s` + `LeanEaseCurve_Canonical` asset reference. | Forward contract pending — Telegraph GDD will inherit Rule 10's "Telegraph SHALL NOT mutate `TelegraphWindowS` after admission" + the FLOOR-change immutability invariant. |
| 7 | Scoring Logic (`design/gdd/scoring-logic.md` — system #10, **undesigned**) | **Soft** (transitive) | Despawn-event consumer: count(`OnWaveDespawned` with `Reason = NaturalLanding`) feeds wave-survived score; near-miss counts via Pull-Wave's collision-side delegates. | Scoring observes; the spawner is unaware of Scoring. |
| 8 | HUD / Audio (presentation layer, **undesigned**) | **Soft** | Subscribers to `OnWaveDespawned` + a future `OnPatternAdmitted` multicast (forward contract on this GDD; signature TBD when HUD GDD lands). | Forward contract pending; recorded in Open Questions. |
| 9 | Telemetry (analytics layer, **undesigned**) | **Soft** | 9 named events emitted (see C.3 §9). | Schema lives in future `telemetry.md`. **2026-06-23 /propagate-design-change R3a closure**: prior wording "5 named events" was stale post-R2a-7 (C.3 §9 was expanded from 5 to 9 events including the 4 EC-class runtime defense events). Sibling AC-WS-23 at line 1111 + C.3 §9 schema table both already reflect 9 events; this Dependencies row was the missed lock-step site (sub-class (b) narrative drift per ADR-0004). |
| 10 | Seam 13 `FWaveSpawnerCallbackTestStub` (test fixture) | **Test-only** | `SetOnDespawnedUserCallback` re-entrant slot. | Production `IWaveSpawnerCallback` MUST NOT expose this surface (test/production seam separation). |

### Lateral (engine subsystem dependencies)

| # | Counterpart | Hardness | Note |
|---|---|---|---|
| E1 | `UWorld` (Unreal) | **Hard** | Pool pre-allocation at first-world-load hook (`FCoreUObjectDelegates::PostLoadMapWithWorld`) uses `UWorld`'s actor instantiation. No `SpawnActor`/`Destroy` during gameplay (Rule 11). **R2a 2026-06-22 fix per unreal-specialist B-R2-1**: prior wording was "at `Cold` entry" which was stale from R1a-9's `Initialize()` anchoring; both were wrong (UWorld does not yet exist at `UGameInstanceSubsystem::Initialize()` in UE 5.7). |
| E2 | Unreal GC | **Hard** | Pool storage MUST be `UPROPERTY() TArray<TObjectPtr<AWave>>` to prevent garbage collection (UE 5.0+ TObjectPtr modernization). |
| E3 | `UGameInstanceSubsystem` lifecycle | **Hard** | The spawner is hosted as `UGameInstanceSubsystem` — locked per OQ-WS-3 closure R1a 2026-06-21 (performance-driven; UWorldSubsystem rejected, see Engine subsystem integration above). Pool pre-allocation fires exactly once per session at the first-world-load hook (`FCoreUObjectDelegates::PostLoadMapWithWorld`); replay re-entry reuses the existing pool. **R3 2026-06-22 lock-step fix (qa-lead B-R3-1)**: prior wording was "at `Initialize()`" — stale against R2a-4's lifecycle correction (R2a-4 updated 5 sibling sites — Rule 11, Engine integration §, E1, AC-WS-20, AC-WS-31 — but missed this E3 lateral-table row). UE 5.7 lifecycle: UWorld does NOT exist at `UGameInstanceSubsystem::Initialize()`; pool pre-allocation MUST defer to `PostLoadMapWithWorld`. ADR (`docs/architecture/adr-NNNN-wave-spawner-subsystem-hosting.md`) documents the locked decision; ADR is downstream artifact authorship, not a GDD-revision item. |

### Tuning-knob delegation (avoid duplicate sources of truth)

The following tuning values are **owned upstream and NOT duplicated** in this GDD's Tuning Knobs section. The spawner reads them by reference; the upstream system's GDD is the canonical source:

| Knob | Owner | Used by spawner in |
|---|---|---|
| `telegraph_window_s` (per-phase + Path B keys) | DPC | Rule 2a (primer telegraph window), Rule 10 (frozen at admission) |
| `wave_spawn_interval_s` (per-phase) | DPC | F-3b cadence gate, Rule 1 |
| `max_concurrent_waves` (per-phase) | DPC | F-1 / F-1b slot accounting, Rule 7 |
| `BARRAGE_EVENTS_PER_PEAK_MIN/AVG/MAX` | DPC | F-3 cadence governor weight |
| `TELEGRAPH_WINDOW_FLOOR_S` + `BARRAGE_SIMULTANEITY_WINDOW_S` | DPC / Pull-Wave registry-coupled via F-7 | Rule 6 + Rule 15 cook-time checks |
| `MAX_CONCURRENT_WAVES_CAP` (= 16) | DPC | F-2 pool size derivation |
| `MIN_BARRAGE_LANE_SEPARATION` (= 1) | Pull-Wave | Rule 4 7-triplet derivation (already collapsed into the enumerated set) |
| TIER0–4 angles + `LEAN_ANGLE_MIN_TIER_GAP_DEG` | Pull-Wave | Rule 5 PEAK barrage tier floor, F-10 derivation |
| `LeanEaseCurve_Canonical` asset | Pull-Wave (registered constant) | Section C.3 §2 (snapshotted into `FPullWaveCurveSnapshot` at admission) |
| `RESUME_GRACE_S` | RSM | Rule 1 gate, Rule 13 |

The spawner's **own** tuning surface (cadence governor internals, retry/log thresholds) is enumerated in Tuning Knobs.

### Bidirectional consistency to verify in `/consistency-check`

When `/consistency-check` next runs:

1. **RSM**: confirm `run-state-machine.md` Dependencies section lists Wave Spawner as "depended on by" (gap noted in row 3 above).
2. **DPC**: confirm `difficulty-phase-controller.md` Dependencies section lists Wave Spawner as "depended on by" (likely already in DPC line 629 enumerated bindings, but Dependencies-section formal listing should be checked).
3. **Pull-Wave**: already verified — line 118 + lines 205/207/210/217 enumerate Wave Spawner forward contracts; no gap.
4. **Future GDDs** (Collision #8, Telegraph #6, Scoring #10, HUD, Audio, Telemetry): each must inherit the forward contracts listed in C.3 + this section's rows 5–9 when authored.

## Tuning Knobs

This section enumerates the spawner's **own** tuning surface only — knobs the spawner is authoritative source-of-truth for. Upstream knobs (DPC, Pull-Wave, RSM, registry) are read by reference and listed in Dependencies § *Tuning-knob delegation*; they are NOT duplicated here. Variable definitions and formula bodies live in Formulas; this section adds the gameplay-effect column + cross-knob invariants required by `.claude/rules/design-docs.md`.

### G.1 — Cadence governor knobs (F-3 internals)

These knobs shape the PEAK cadence governor's draw weight between barrage and non-barrage sub-pools. The variable schemas + ranges + defaults are authoritative in F-3 § *Variables* — referenced here for the gameplay-effect axis and cross-knob invariants.

| Knob | Safe range | Default | Gameplay aspect affected |
|---|---|---|---|
| `BARRAGE_FORCE_FLOOR_W` (F) | `[0.7, 0.95]` | `0.80` | Probability of admitting a barrage when the spawner enters force-draw mode (zero barrages with `τ ≥ T_FORCE`). Higher F ⇒ floor-1 satisfied earlier in PEAK; lower F ⇒ more variance, higher risk of invoking Rule 7 `barrage_owed`. |
| `T_FORCE` (T_f) | `[0.4, 0.7]` | `0.50` | Normalized PEAK fraction (τ) at which zero-barrage state escalates to force-draw. Lower T_f ⇒ earlier "panic" toward floor; higher T_f ⇒ rely longer on the proportional branch. |
| `λ` | `[0.05, 0.30]` | `0.15` | Proportional drive strength toward `BARRAGE_EVENTS_PER_PEAK_TARGET_AVG = 2`. Higher λ ⇒ session-average converges to 2 more aggressively; lower λ ⇒ slower regression-to-mean (more variance across runs). |
| `W_CEILING` (W_c) | `[0.3, 0.6]` | `0.50` | Maximum barrage weight in the proportional branch (before force-draw applies). Caps PEAK barrage density when k is still below `T_avg`. |
| `base_w` (β) | `[0.15, 0.40]` (cook-time advisory) | author-set per PEAK pool authoring; computed as `|PEAK_barrage_pool| / |PEAK_pool_total|` | Asymptotic floor of `w_barrage` when forcing is inactive. Sits below `W_CEILING`; binds PEAK author to compose the sub-pool fraction in the band that lets λ-drive converge toward `T_avg` without saturating against ceiling or max. |

**Cross-knob invariants (binding):**

- `W_CEILING < BARRAGE_FORCE_FLOOR_W` — otherwise a proportional-branch draw at ceiling is indistinguishable from a forced draw (force-draw mode loses its semantic). F-3 § *Cross-knob invariant*.
- `base_w ∈ [0.15, 0.40]` — enforced at cook time as Rule 15 `PEAK_BASE_W_RANGE`. Outside this band, λ-drive cannot suppress the session-sample average toward `T_avg = 2` while honoring `MAX = 3`. F-3 § *Cook-time advisory*.
- Force-draw branch fires only when `k < BARRAGE_EVENTS_PER_PEAK_MIN ∧ τ ≥ T_FORCE`. If `BARRAGE_EVENTS_PER_PEAK_MIN` is raised by DPC, `T_FORCE` should be re-tuned downward (force-draw needs more time to satisfy a higher floor) — flag in Open Questions if `T_min` ever changes.
- All four numeric knobs (F, T_f, λ, W_c) are `UPROPERTY(EditDefaultsOnly)` on the spawner's `UWaveSpawnerCadenceConfig : public UDataAsset` config object. **R2a 2026-06-22 specifier alignment per 3-spec convergence (qa R-R2-3 + sys R-SD-3 + unreal R-R2-2)**: prior R1a wording used `UPROPERTY(EditAnywhere)` which is inconsistent with G.2's `EditDefaultsOnly` on the same DataAsset; `EditDefaultsOnly` is the canonical specifier for `UDataAsset` tuning knobs (asset instances ARE CDOs in the content browser; `EditAnywhere` would suggest per-instance overrides that don't serialize back). **R1a 2026-06-21 fix per unreal-specialist B1 + perf-analyst R6 (retained)**: prior wording before R1a used `UPROPERTY(EditAnywhere, Config)` which is structurally invalid in UE — the `Config` specifier serializes to `.ini` and requires `UCLASS(Config=GameName)`, but `UDataAsset` serializes through the asset pipeline (`.uasset`); the `Config` specifier is silently ignored on a UDataAsset subclass. Content-authoring via UDataAsset is the correct pattern for these knobs (designer-editable in the content browser; baked into the asset). Neither `UPROPERTY(BlueprintReadWrite)` nor `UPROPERTY(BlueprintReadOnly)` is permitted — cadence is server-authoritative; the knobs are visible only via the C++ subsystem API. Runtime BP read/write is forbidden.

**Re-tuning trigger conditions:**

- Telemetry signal `peak_min_barrage_floor_undershoot` recurring across sessions (Rule 7 telemetry, Section C.3 §9) ⇒ raise `BARRAGE_FORCE_FLOOR_W` or lower `T_FORCE`.
- Telemetry signal `barrage_dropped_due_to_concurrency` recurring with `available < 3` at peak slot pressure ⇒ inspect upstream `max_concurrent_waves` per-phase (DPC-owned), not this section's knobs.
- Sample-average barrages/PEAK drifting away from `T_avg = 2` ⇒ raise `λ`.

### G.2 — Pool-margin knob (F-2 derivation)

| Knob | Safe range | Default | Gameplay aspect affected |
|---|---|---|---|
| `MAX_CONCURRENT_WAVES_CAP_CEIL_MARGIN` (M) | `{3, 4, 5, 6, 7}` (small ints; see invariant) | `5` | Sizing slack against transient over-draw spikes (e.g., admission tick + DESPAWNING-return latency overlap). Lower M ⇒ tighter memory; higher M ⇒ defensive headroom against pool-exhaustion EC-WS-4 / EC-WS-15. |

**Cross-knob invariants:**

- `pool_size = MAX_CONCURRENT_WAVES_CAP + DESPAWNING_RETURN_LATENCY_SLOTS + MAX_CONCURRENT_WAVES_CAP_CEIL_MARGIN`. The first two terms are DPC-/Pull-Wave-owned (see Dependencies § *Tuning-knob delegation*); only `M` is spawner-internal.
- `MAX_CONCURRENT_WAVES_CAP_CEIL_MARGIN ≥ DESPAWNING_RETURN_LATENCY_SLOTS = 2` — margin must at minimum cover the one-frame Rule 12 return-latency window. Defaulting to 5 gives 3-slot defensive headroom beyond the binding floor.
- **Recompute trigger**: if `MAX_CONCURRENT_WAVES_CAP` ever rises above `16 + 5 = 21`, `pool_size = 23` is no longer correct and the derivation must be re-run before ship (F-2 § *Cross-knob trigger*; Rule 15 cook-time check `POOL_SIZE_DERIVATION_MATCH` would catch the drift). Open Question OQ-WS-1 tracks: should the cap raise be a registry value rather than a constant?
- `M` is `UPROPERTY(EditDefaultsOnly)` on the same `UWaveSpawnerCadenceConfig : UDataAsset` (immutable at runtime; same R1a 2026-06-21 fix as G.1 — `Config` specifier is silently ignored on UDataAsset; UDataAsset asset-pipeline serialization is the correct backing store). Changing M requires re-cooking the asset, not a code patch.

### G.3 — Telemetry rate-limit semantics

Telemetry emission rates are not numeric knobs — they are binding policies the spawner enforces internally. They are listed here so a downstream `design/gdd/telemetry.md` (when authored) inherits the rate-limit contract without re-deriving it.

| Emission | Rate-limit policy | Rationale |
|---|---|---|
| `pool_exhaustion_detected` (EC-WS-4) | **Once per session.** | Pool-exhaustion is a tuning-regression signal; spamming the analytics pipeline with every dropped admission destroys signal-to-noise. The first occurrence is the diagnostic; subsequent occurrences are noise. |
| `critical_pool_empty_post_load` (EC-WS-17) | **Once per session.** | At-most-one fatal-state escalation per run. If the pool is empty post-load, the spawner stays in Idle for the whole run — re-emitting would not add information. |
| `pattern_asset_invalid_at_load` (EC-WS-17) | **Once per asset per session.** | Distinguish *which* asset is invalid; deduplicate repeated load-attempts of the same broken asset within a run. Different invalid assets emit separately. |
| `empty_pool_at_draw` (EC-WS-15) | **Once per session.** | Defense-in-depth signal after Rule 15's `POOL_NON_EMPTY` cook check; recurring should never happen in practice. Once-per-session captures the anomaly without flooding. |
| `pattern_admitted` (Rule 8 informational) | **Per-event, no rate limit.** | Routine admission stream — telemetry ingest is the rate-limiter at the pipeline boundary, not the spawner. |
| `pause_flush_executed` (Rule 13) | **Per-event, no rate limit.** | At-most-one fire per pause-resume cycle; natural rate-limit via the trigger condition itself. |
| `run_termination_flush_executed` (Rule 14) | **Per-event, no rate limit.** | At-most-one fire per run; natural rate-limit via the trigger condition itself (RSM RUNNING → DEAD/COMPLETE/ABORTED transition). R2a 2026-06-22 added per qa-lead B-R2-2 G.3 schema completeness. |
| `peak_min_barrage_floor_undershoot` (Rule 7) | **Per-event (≤ 1 per PEAK).** | Naturally bounded — at most one floor-undershoot per PEAK exit. No additional limiter needed. |
| `barrage_dropped_due_to_concurrency` (Rule 7) | **Per-event, no rate limit.** | Each drop is a distinct tuning-regression data point; aggregation is the analytics pipeline's job. |

**Cross-section invariants:**

- Rate-limit state (per-session flags, per-asset-per-session sets) lives on the spawner instance and is reset on `Cold` entry. Pause/resume (Rule 13) does NOT reset rate-limit state — a single run carries one set of `once-per-session` budgets.
- Future `design/gdd/telemetry.md` MUST inherit this policy as binding; it is not negotiable downstream. Adding a new spawner-emitted event without specifying its rate-limit policy fails cook-time review.

### G.4 — Knob ownership boundary (reference)

The full list of knobs read by the spawner but **owned upstream** is in Dependencies § *Tuning-knob delegation* (10 delegated knobs: per-phase `telegraph_window_s` / `wave_spawn_interval_s` / `max_concurrent_waves` / barrage envelope, registry-coupled `TELEGRAPH_WINDOW_FLOOR_S` + `BARRAGE_SIMULTANEITY_WINDOW_S`, `MAX_CONCURRENT_WAVES_CAP`, `MIN_BARRAGE_LANE_SEPARATION`, TIER0–4 angles + `LEAN_ANGLE_MIN_TIER_GAP_DEG`, `LeanEaseCurve_Canonical`, `RESUME_GRACE_S`). The spawner reads them by reference at admission time (`FDPCFrameState` snapshot; Rule 10 immutability post-admission). Re-tuning those is owned by the upstream system's GDD and not by this section.

Anything not in G.1–G.3 above and not in Dependencies § *Tuning-knob delegation* should be flagged as an undeclared knob and filed against this section's authorship.

## Acceptance Criteria

This section enumerates testable pass/fail conditions for the spawner. Each AC has an ID (`AC-WS-NN`), a binding-surface category, a gate level, and a verification mechanism. Per `.claude/rules/design-docs.md`: every criterion is reproducible by a QA tester (or by an automated test where flagged).

**Gate-level convention** (matches DPC/Pull-Wave R10b two-phase pattern):

- **BLOCKING-at-story-Done** — build cannot exit the Wave Spawner story until verified.
- **ADVISORY-at-story-Done + BLOCKING-at-Alpha** — log/observe at story-Done; must close at the Alpha milestone gate. Used for telemetry + performance ACs that need real session data.

Verification mechanisms:

- **Cook-time** = pattern-asset pipeline check in Rule 15 (build refuses on violation).
- **Unit test** = `tests/unit/wave_spawner/` UE Automation entry; deterministic.
- **Integration test** = `tests/integration/wave_spawner/` against Pull-Wave + DPC + RSM stubs.
- **Manual playtest** = recorded session with observable outcome.
- **Telemetry inspection** = analytics pipeline query post-session.

### H.1 — Cook-time validation ACs (Rule 15 contracts)

All build-blocking; verified at cook time. AC-WS-01 through AC-WS-09 + AC-WS-32 through AC-WS-38 cover the 14 binding cook-time checks Rule 15 enumerates plus 2 derived non-Rule-15 checks (AC-WS-04 `PEAK_NO_ADJACENT_CLUSTER` from Rule 4 — note: subsumed by AC-WS-05's set equality; AC-WS-09 `POOL_SIZE_DERIVATION_MATCH` from F-2 + G.2 recompute trigger). **R3 2026-06-22 narrative-count refresh (qa-lead R-R3-1)**: prior R1a-era text claimed "11 binding cook-time checks Rule 15 enumerates" — stale against R2a-5 + R2a-12 which added 3 new Rule 15 checks (`PEAK_BASE_W_BELOW_CEILING`, `BARRAGE_UNIFORM_TIER`, `BARRAGE_DISTINCT_SOURCE_LANES`) + 3 new ACs (AC-WS-36/37/38). H.8 sibling counts at lines 1140 + 1145 were updated correctly to "Rule 15 × 14" + "H.1 × 16 cook-time"; this preamble was the missed lock-step site. **R1a 2026-06-21 fix per qa-lead B2 / systems-designer B4 / game-designer B5 (retained)**: prior text claimed "9 of 9 Rule 15 checks" — wrong; R1a corrected to enumerate 11 (10 original + R1a's `MIN_BARRAGE_PATTERN_COUNT_PER_TRIPLET`); 3 of the original 10 had no AC (`OPENER_NO_BARRAGE`, `PRIMER_PATTERN`, `PILLAR_1_VERB_SLIP`). Added AC-WS-32/33/34 close that gap; AC-WS-35 covers R1a's new Rule 15 check; AC-WS-36/37/38 cover R2a's three new Rule 15 checks.

| ID | Statement | Verification | Gate |
|---|---|---|---|
| AC-WS-01 | `POOL_NON_EMPTY` — each of the three phase pools (OPENER, MID, PEAK) contains ≥ 1 entry post-cook. A cook with any empty pool refuses the build. | Cook-time | BLOCKING-at-story-Done |
| AC-WS-02 | `MID_NO_BARRAGE` — no MID pool entry has `is_barrage = true`. (Per Rule 2 shape invariant.) | Cook-time | BLOCKING-at-story-Done |
| AC-WS-03 | `PEAK_BARRAGE_MIN_TIER` — every PEAK barrage entry uses lane tiers from `{2, 3, 4}` only (Rule 5 minimum-tier floor; inherits Pull-Wave R8 RC-E PEAK barrage tier ≥ 2). | Cook-time | BLOCKING-at-story-Done |
| AC-WS-04 | `PEAK_NO_ADJACENT_CLUSTER` — no PEAK barrage triplet matches `{0,1,2}`, `{1,2,3}`, or `{2,3,4}` (Rule 4; would force MIN_ESCAPE_SLIPS=2 break). | Cook-time | BLOCKING-at-story-Done |
| AC-WS-05 | `PEAK_SURVIVING_TRIPLETS` — `count == 7` AND `set == { {0,1,3}, {0,1,4}, {0,2,3}, {0,2,4}, {0,3,4}, {1,2,4}, {1,3,4} }` (Rule 4 set equality; inherits Pull-Wave R7 B8). | Cook-time | BLOCKING-at-story-Done |
| AC-WS-06 | `BARRAGE_W_SPAN` — for every barrage pattern, `max(onset_time) − min(onset_time) ≤ BARRAGE_SIMULTANEITY_WINDOW_S = 0.35s` (F-5). Span = 0 valid (single-frame barrage). | Cook-time | BLOCKING-at-story-Done |
| AC-WS-07 | `NON_BARRAGE_STAGGER` — for every non-barrage multi-onset pattern, `min(δᵢ) ≥ TELEGRAPH_WINDOW_FLOOR_S = 0.70s` (F-6 + Rule 6; structurally distinguishes from barrages — a 0.35s–0.69s gap would be a covert sub-telegraph barrage). **R1a 2026-06-21 fix per 4-specialist convergence (game-designer B4, systems-designer B1, qa-lead B1, perf-analyst F9)**: prior text bound this to `BARRAGE_SIMULTANEITY_WINDOW_S = 0.35s` (the barrage simultaneity window, NOT the non-barrage stagger floor) — a 2× error that would have certified covert sub-telegraph non-barrages past cook. | Cook-time | BLOCKING-at-story-Done |
| AC-WS-08 | `PEAK_BASE_W_RANGE` — `0.15 ≤ \|PEAK_barrage_pool\| / \|PEAK_pool_total\| ≤ 0.40` (Rule 15 + F-3 cook-time advisory; outside this band the λ-drive cannot honor `T_avg = 2` against `MAX = 3`). | Cook-time | BLOCKING-at-story-Done |
| AC-WS-09 | `POOL_SIZE_DERIVATION_MATCH` — `pool_size == MAX_CONCURRENT_WAVES_CAP + DESPAWNING_RETURN_LATENCY_SLOTS + MAX_CONCURRENT_WAVES_CAP_CEIL_MARGIN` (per F-2; F-2 is the oracle site for both the derivation and the current `23` literal). Currently `16 + 2 + 5 = 23`. Catches drift if `MAX_CONCURRENT_WAVES_CAP` raises above `21` (G.2 recompute trigger). | Cook-time | BLOCKING-at-story-Done |
| AC-WS-32 | `OPENER_NO_BARRAGE` — `count(p in OPENER_pool where p.is_barrage) == 0` (Rule 3 + Rule 15; inherits Pull-Wave AC-PW-OPENER-NO-BARRAGE R4 RC-R3-5). R1a 2026-06-21 added per qa-lead B2 + game-designer B5 + systems-designer B4 coverage-gap convergence. | Cook-time | BLOCKING-at-story-Done |
| AC-WS-33 | `PRIMER_PATTERN` — `OPENER_pool` contains exactly one entry with `is_primer == true AND target_lane == 2 AND lean_magnitude_tier == 1` (Rule 2a + Rule 15). R1a 2026-06-21 added per coverage gap. | Cook-time | BLOCKING-at-story-Done |
| AC-WS-34 | `PILLAR_1_VERB_SLIP` — for every pattern across all pools, the set of escape solutions under `MIN_ESCAPE_SLIPS = 2` + REACT ceiling 0.25s is exclusively the `slip` verb from every reachable player lane (Rule 15; Pillar 1 cut-criterion enforcement loop). R1a 2026-06-21 added per game-designer B5 — closes Pillar 1 enforcement gap (cook check existed in Rule 15 but had no AC). | Cook-time | BLOCKING-at-story-Done |
| AC-WS-35 | `MIN_BARRAGE_PATTERN_COUNT_PER_TRIPLET` — for every PEAK barrage lane-class signature (the 7 surviving triplets per Rule 4), `count(p in PEAK_barrage_pool where p.target_lanes == signature) ≥ 2`. R1a 2026-06-21 added per CD R1 Ruling 1 — guarantees per-signature authoring variation (tier, velocity, source-lane, onset arrangement) so the lane-class vocabulary does not collapse to 7 identical pattern instances at session scale. | Cook-time | BLOCKING-at-story-Done |
| AC-WS-36 | `PEAK_BASE_W_BELOW_CEILING` — `base_w < W_CEILING` (configured F-3 cross-knob invariant). Pool authoring + G.1 W_CEILING tuning combination MUST satisfy `\|PEAK_barrage_pool\| / \|PEAK_pool_total\| < W_CEILING`. **R2a 2026-06-22 added per systems-designer B-SD-1** — prevents F-3 proportional-branch silent saturation that would kill λ-drive cadence feedback. | Cook-time | BLOCKING-at-story-Done |
| AC-WS-37 | `BARRAGE_UNIFORM_TIER` — for every PEAK barrage pattern, all M=3 member waves share identical `lean_magnitude_tier`. Exception: K_class=3 (sole admissible triplet `{0,2,4}` post-R7-B8) admits mixed-tier per Pull-Wave R8 RC-C relaxation. **R2a 2026-06-22 added per systems-designer R-SD-5** — enforces Pull-Wave R7 B11 forward contract at cook time. | Cook-time | BLOCKING-at-story-Done |
| AC-WS-38 | `BARRAGE_DISTINCT_SOURCE_LANES` — for every PEAK barrage pattern, the M=3 member waves have distinct `SourceLane` values. **R2a 2026-06-22 added per systems-designer R-SD-5** — enforces Pull-Wave R7 B12 forward contract at cook time (no shared-source SPAWNED-frame pile). | Cook-time | BLOCKING-at-story-Done |

### H.2 — Admission pipeline ACs

| ID | Statement | Verification | Gate |
|---|---|---|---|
| AC-WS-10 | **Rule 1 cadence gate** — admission requires `(now − last_spawn_time) ≥ wave_spawn_interval_s` for the active phase. Test: simulate `now = last_spawn_time + Δ − 0.001`; assert no admission. At `now = last_spawn_time + Δ`; assert admission proceeds to Rule 7. | Unit test (`test_cadence_gate_strict_inequality_passes`) | BLOCKING-at-story-Done |
| AC-WS-11 | **Rule 2a primer bypass** — at `Cold → Active` transition, the OPENER primer admits on the same tick the transition fires, regardless of `last_spawn_time = 0`. Post-primer, `last_spawn_time = now` so the *second* OPENER draw waits `wave_spawn_interval_s_OPENER = 4.0s`. | Unit test (`test_primer_admits_on_active_transition_tick`) | BLOCKING-at-story-Done |
| AC-WS-12 | **Rule 7 atomic barrage reservation + fulfilment** — (a) drop-path: for a barrage candidate with `available ≥ 3`, all three slots pre-commit at admission time (`scheduled += 3`) before any onset fires; for `available < 3`, drop, set `barrage_owed = true`, emit `barrage_dropped_due_to_concurrency`, draw a non-barrage replacement from the same pool. (b) fulfilment-path: when `barrage_owed == true` AND the next admission tick has `available ≥ 3` AND a non-barrage candidate is available, the spawner draws from the PEAK barrage sub-pool (NOT the non-barrage candidate), increments `scheduled` by 3, and clears `barrage_owed`. | Unit test (4 scenarios: `available=3`, `available=2`, `available=4`, and `pre-state barrage_owed=true + available=3 + non-barrage candidate available → assert barrage drawn + scheduled+=3 + barrage_owed cleared`). R1a 2026-06-21 added scenario (d) per qa-lead R5 — closes reservation fulfilment-path gap. | BLOCKING-at-story-Done |
| AC-WS-13 | **Rule 8 deterministic RNG seeding (same-platform)** — the per-run RNG seed captured at `Cold → Active` from RSM's `RunSeed : uint64` field (per Rule 8 inscribed forward contract on RSM; OQ-WS-4 closed R1a 2026-06-21 per CD R1 Ruling 2) produces a reproducible admission sequence given identical DPC frame-state stream + identical RSM state. **Given the same platform architecture** (ARM iOS vs ARM Android vs x86 development), two runs with the same `RunSeed` value produce byte-identical `(tick, pattern_id, is_barrage)` traces. Death Replay is platform-scoped per EC-WS-15 (a run captured on iOS replays only on iOS). Wall-clock-derived seeds are explicitly forbidden. **Forward-guard (R2a 2026-06-22 per qa-lead B-R2-3)**: any future F-3 (or other selection-path) change that introduces transcendentals or platform-divergent ops MUST update this AC to document the platform-determinism impact; currently F-3 uses only clamp + linear ops which are platform-deterministic. | Integration test (replay determinism harness on a single platform per test fixture; depends on RSM `RunSeed` field landing per Dependencies § Bidirectional consistency row 1) | BLOCKING-at-story-Done |
| AC-WS-14 | **F-3 cadence governor three-branch correctness** — given fixture `(τ, k, β, F, T_f, λ, W_c, T_avg, T_min, T_max)`, `w_barrage` matches the closed-form spec: branch 1 (k ≥ T_max ⇒ w = 0), branch 2 (k < T_min ∧ τ ≥ T_f ⇒ w = F), branch 3 (otherwise ⇒ proportional clamped). All three example fixtures from F-3 § *Behaviour at extremes* verified. | Unit test (table-driven, ≥ 12 fixtures spanning all 3 branches) | BLOCKING-at-story-Done |

### H.3 — Lifecycle ACs

| ID | Statement | Verification | Gate |
|---|---|---|---|
| AC-WS-15 | **Rule 11 no runtime SpawnActor/Destroy** — across a full 60s run (and through pause/resume cycles), zero net change in `AWave` actor count in the world post-first-world-load pool pre-allocation. **R2a 2026-06-22 verification mechanism update per 3-spec convergence (qa-lead B-R2-1 + sys-des B-SD-3 + unreal-specialist B-R2-4)**: prior R1a-14 text cited "seam-injected `FWaveSpawnActorCounter` test seam" — Phase 2b grep confirmed this struct is NOT authored in `docs/architecture/platform-seam-interfaces.md` (paper-only seam). Reframed to post-hoc pool inspection: assert `WaveSpawnerSubsystem->GetPool().Num() == 23` at first-world-load-hook exit; world-actor census via `UGameplayStatics::GetAllActorsOfClass(GetWorld(), AWave::StaticClass(), OutActors)` asserts `OutActors.Num() == 23` (unchanged) at the end of any subsequent run + replay cycle. No new seam required. | Unit test (pool size + world-actor census; deterministic). | BLOCKING-at-story-Done |
| AC-WS-16 | **Rule 12 ordered despawn pipeline** — for every `AWave` despawn, the callback sequence is exactly: (1) collision unregister → (2) telegraph unregister → (3) `OnWaveDespawned` broadcast. Sequence holds across natural landing, abort, and pause-flush termination paths. | Unit test against `FWaveSpawnerCallbackTestStub` capturing call ordering. | BLOCKING-at-story-Done |
| AC-WS-17 | **Rule 13 pause-flush + grace** — on `OnPausedChanged(true)`, all in-flight waves drain (Rule 12 ordered pipeline) and `pause_flush_executed` emits with `(waves_flushed, time_to_drain_ms)`. On `OnPausedChanged(false)`, the spawner waits `RESUME_GRACE_S` before resuming admission. `last_spawn_time` invariant: NOT reset by pause/resume — survives the cycle. | Integration test (RSM pause/resume harness; assert drain order + grace timer). | BLOCKING-at-story-Done |
| AC-WS-18 | **Rule 14 PEAK exit clears `barrage_owed`** — on `PEAK → {DEAD, COMPLETE, ABORTED}`, `barrage_owed = false`. If `barrages_admitted_this_PEAK < BARRAGE_EVENTS_PER_PEAK_MIN = 1`, emit `peak_min_barrage_floor_undershoot` telemetry exactly once for that PEAK. | Unit test (3 fixtures: PEAK exit with `barrages_admitted = 0`, `= 1`, `= 2`). | BLOCKING-at-story-Done |

### H.4 — Forward-contract inheritance ACs

These verify the 8 forward contracts the spawner inherits from the Pull-Wave + DPC R14 cascade (active.md § *Wave Spawner GDD authoring* — full enumeration). Each contract is now embedded in the spawner's spec; these ACs are the empirical proof.

| ID | Statement | Verification | Gate |
|---|---|---|---|
| AC-WS-19 | **Seam 13 `OnDespawnedUserCallback` test-stub slot** — `FWaveSpawnerCallbackTestStub` exposes a `SetOnDespawnedUserCallback(TFunction<void(...)>)` re-entrant slot. Production `IWaveSpawnerCallback` does NOT expose this surface (test/production seam separation). Compile-time + reflection check. | Unit test + compile-time `static_assert` (production interface lacks the symbol). | BLOCKING-at-story-Done |
| AC-WS-20 | **Pool-sizing contract** — at first-world-load hook (`FCoreUObjectDelegates::PostLoadMapWithWorld`) exit, `AWave` pool count == 23 (per F-2). Pool storage is `UPROPERTY() TArray<TObjectPtr<AWave>>` (Rule 11 GC-anchor; UE 5.0+ TObjectPtr modernization per unreal-specialist B2). **R2a 2026-06-22 lifecycle fix per unreal-specialist B-R2-1**: anchor moved from `UGameInstanceSubsystem::Initialize()` (UWorld unavailable) to first-world-load hook (UWorld valid); pool is allocated once per session and reused across replays. | Unit test (count + UPROPERTY reflection assert). | BLOCKING-at-story-Done |
| AC-WS-21 | **Rule 10 admission-time immutability** — once a `Wave` is admitted, its `telegraph_window_s` snapshot and `LeanEaseCurve_Canonical` asset reference do NOT mutate for the lifetime of that instance, even if DPC's `FDPCFrameState.telegraph_window_s` changes on a later tick. (Path B keys, R10d FLOOR=0.70s, and TELEGRAPH_WINDOW_FLOOR_MAX_ADMISSIBLE_S=0.76s ceiling are all snapshotted, not late-bound.) | Integration test (mutate DPC TW mid-flight; assert in-flight Wave's snapshot unchanged). | BLOCKING-at-story-Done |

### H.5 — Telemetry / observability ACs

Per G.3 rate-limit policy. These need real session data — gate level reflects that.

| ID | Statement | Verification | Gate |
|---|---|---|---|
| AC-WS-22 | **G.3 rate-limit policy enforcement** — across a single session, `pool_exhaustion_detected`, `critical_pool_empty_post_load`, and `empty_pool_at_draw` each fire at most once. `pattern_asset_invalid_at_load` fires at most once *per asset*. Per-event emissions (`pattern_admitted`, `pause_flush_executed`, `peak_min_barrage_floor_undershoot`, `barrage_dropped_due_to_concurrency`) fire on each triggering event. | Telemetry inspection (instrumented soak run + analytics query) | ADVISORY-at-story-Done, BLOCKING-at-Alpha |
| AC-WS-23 | **9 named events payload schema** — every emission's payload matches Section C.3 §9 schema by name + type. Sampling: 1 instance of each event captured in an instrumented session (or synthetic fixture for terminal events like `critical_pool_empty_post_load`); payload field-set deep-compared to schema. **R2a 2026-06-22 scope expansion per qa-lead B-R2-2**: prior text said "5 named events" — but the spawner emits 9 (5 normal-operation + 4 EC-class runtime defense). C.3 §9 schema now lists all 9. | Integration test against schema fixture | ADVISORY-at-story-Done, BLOCKING-at-Alpha |
| AC-WS-24 | **Rate-limit state reset semantics** — once-per-session flag-sets reset on `Cold` entry (run restart). Pause/resume (Rule 13) does NOT reset them — a single run carries one set of budgets. | Unit test (2 scenarios: cold→active→cold; active→paused→active) | ADVISORY-at-story-Done, BLOCKING-at-Alpha |

### H.6 — Edge-case resolution ACs

Verify Edge Cases section EC-WS-1 through EC-WS-18 resolutions. Coverage selection: the 5 highest-severity (data-loss, build-shipping, or invariant-break) edges.

| ID | Statement | Verification | Gate |
|---|---|---|---|
| AC-WS-25 | **EC-WS-4 pool exhaustion no-fallback** — when the pool is exhausted at an admission tick, the spawner skips the tick, emits `pool_exhaustion_detected` (once per session), and does NOT call `UWorld::SpawnActor`. Non-Shipping additionally fires `check()` assertion. | Unit test (force-exhaust pool fixture; assert no SpawnActor + telemetry + non-Shipping assert). | BLOCKING-at-story-Done |
| AC-WS-26 | **EC-WS-15 empty pool at draw** — defense-in-depth after Rule 15's BLOCKING `POOL_NON_EMPTY` cook check fails to fire (e.g., asset hot-patch). Runtime: non-Shipping asserts; Shipping skips admission and emits `empty_pool_at_draw` (once per session). | Unit test (force-empty pool runtime; assert behaviour). | BLOCKING-at-story-Done |
| AC-WS-27 | **EC-WS-17 asset-invalid-at-load** — pool pre-allocation validates each pattern's structural invariants. Non-Shipping: asserts. Shipping: drops the invalid asset, emits `pattern_asset_invalid_at_load` once per asset. If the resulting valid pool is empty, emits `critical_pool_empty_post_load` and the spawner remains in `Idle` for the run. | Integration test (3 fixtures: valid pool, 1 invalid asset, all invalid → critical empty). | BLOCKING-at-story-Done |
| AC-WS-28 | **EC-WS-18 resume-grace with zero in-flight** — if `OnPausedChanged(false)` fires with zero in-flight waves, `RESUME_GRACE_S` still applies (per Rule 13 invariant). `last_spawn_time` was preserved across the pause cycle; cadence gate evaluates against the preserved timestamp post-grace. | Integration test (pause with zero in-flight; assert grace timer + `last_spawn_time` preservation). | BLOCKING-at-story-Done |
| AC-WS-29 | **EC-WS-3 MID→PEAK boundary multi-onset immutability** — a non-barrage multi-onset pattern admitted near (but outside) the MID→PEAK Rule 9 suppression window completes its in-flight onsets under the admission-time snapshotted state, even when downstream telegraph rendering observes the PEAK pool swap. | Integration test: admit a MID 2-onset pattern at `phase_remaining ≈ 3.05s` (just outside Rule 9's `max_pattern_length_s = 3.0s` suppression window) with onset-1 at `t = 0s` and onset-2 at `t = +2.90s` (fires `phase_remaining ≈ 0.15s` — i.e., 150ms BEFORE the swap, while still in MID). The MID→PEAK pool swap (Rule 10) fires at `phase_remaining = 0`, AFTER onset-2 has fired and during onset-2's telegraph + traversal window. Assert: onset-2's wave instance retains MID-snapshot `telegraph_window_s = 0.82s` for its entire lifecycle even though Rule 10's atomic pool swap fires before onset-2's telegraph + traversal complete. **R2a 2026-06-22 fixture reframe per qa-lead B-R2-1**: prior R1a-7 fixture asserted "onsets 2-3 fire after the swap" but Rule 2's MID `max_pattern_length_s = 3.0s` cap structurally prevents a 3.0s pattern admitted at 3.05s remaining from having any onset land after the swap — the AC's claim was unreachable. The reframed AC tests the actual immutability claim (Rule 10's snapshot-at-admission contract surviving the in-flight phase swap) using a fixture that IS reachable under Rule 2's cap. | BLOCKING-at-story-Done |

### H.7 — Performance ACs

Need real session profiling on mid-tier mobile hardware. Bound to Alpha milestone gate.

| ID | Statement | Verification | Gate |
|---|---|---|---|
| AC-WS-30 | **Admission-tick CPU budget** — the spawner's per-tick admission-pipeline cost (Rule 1 → Rule 8 → Rule 7 reservation) is `≤ 0.30 ms p99` on the project's mid-tier mobile target (per CLAUDE.md technical preferences: 16.6 ms frame budget at 60 fps; spawner allocated ≤ 1.8% of frame). **Mid-tier device list** (inherited from Pull-Wave R10a Polish-phase placeholders per qa-lead R9): iPhone XR + Pixel 5 / Galaxy A52. TD + performance-analyst close at Polish-phase device audit. | Profiler capture against soak session; `Unreal Insights` per-tick scope marker on `UWaveSpawnerSubsystem::Tick` (R2a-1 class-prefix fix — was `FWaveSpawnerSubsystem`; F-prefix wrong for UClass). | ADVISORY-at-story-Done, BLOCKING-at-Alpha |
| AC-WS-31 | **Pool pre-allocation cost (session-startup, not per-run)** — pool pre-allocation at first-world-load hook (`FCoreUObjectDelegates::PostLoadMapWithWorld`; fires once per session per Rule 11 lifecycle anchor) completes within a single frame (`≤ 16.6 ms`) on mid-tier mobile. **R2a 2026-06-22 timing-anchor correction per unreal-specialist B-R2-1**: prior R1a-9 text said `UGameInstanceSubsystem::Initialize()` — UE 5.7 lifecycle violation (UWorld unavailable). Pool pre-allocation locked to first-world-load hook (once per session); replay does NOT re-fire allocation. The one-frame hitch at first-world-load is masked behind the loading frame, concurrent with level streaming and HUD construction. **Mid-tier device list** (per AC-WS-30 inheritance): iPhone XR + Pixel 5 / Galaxy A52. | Profiler capture (`stat unit` over `PostLoadMapWithWorld` callback); manual playtest. | ADVISORY-at-story-Done, BLOCKING-at-Alpha |

### H.8 — AC count summary

**R2a 2026-06-22**: 3 ACs added (AC-WS-36 PEAK_BASE_W_BELOW_CEILING + AC-WS-37 BARRAGE_UNIFORM_TIER + AC-WS-38 BARRAGE_DISTINCT_SOURCE_LANES) per R2a-5 + R2a-12 Rule 15 cook check additions. **R1a 2026-06-21**: 4 ACs added (AC-WS-32 OPENER_NO_BARRAGE + AC-WS-33 PRIMER_PATTERN + AC-WS-34 PILLAR_1_VERB_SLIP + AC-WS-35 MIN_BARRAGE_PATTERN_COUNT_PER_TRIPLET) per Rule 15 coverage closure + CD R1 Ruling 1.

- **Total**: 38 ACs (was 35 pre-R2a; was 31 pre-R1a).
- **BLOCKING-at-story-Done**: 33 (H.1 × 16 cook-time [9 original + 4 R1a-added + 3 R2a-added] + H.2 × 5 admission + H.3 × 4 lifecycle + H.4 × 3 forward-contract + H.6 × 5 edge-case).
- **ADVISORY-at-story-Done, BLOCKING-at-Alpha**: 5 (H.5 × 3 telemetry + H.7 × 2 performance).

**Coverage cross-reference:**

- Rule 1–15: covered by H.1 (Rule 15 cook-time × 14 [9 original + 4 R1a-added — AC-WS-32 OPENER_NO_BARRAGE / AC-WS-33 PRIMER_PATTERN / AC-WS-34 PILLAR_1_VERB_SLIP / AC-WS-35 MIN_BARRAGE_PATTERN_COUNT_PER_TRIPLET — + 3 R2a-added — AC-WS-36 PEAK_BASE_W_BELOW_CEILING / AC-WS-37 BARRAGE_UNIFORM_TIER / AC-WS-38 BARRAGE_DISTINCT_SOURCE_LANES — note Rule 15 now enumerates 14 binding checks]; plus 2 non-Rule-15-derived ACs in H.1: AC-WS-04 PEAK_NO_ADJACENT_CLUSTER from Rule 4 and AC-WS-09 POOL_SIZE_DERIVATION_MATCH from F-2 + G.2) + H.2 (Rule 1, 2a, 7, 8) + H.3 (Rule 11, 12, 13, 14) + H.4 (Rule 10). Rules NOT directly AC-covered: Rule 2 (shape invariants — flowed through Rule 15 cook checks); Rule 3 (state machine — implicit in H.3 lifecycle); Rule 4 (triplet enumeration — AC-WS-05 + AC-WS-35 [per-signature variation]); Rule 5 (PEAK tier floor — AC-WS-03); Rule 6 (stagger — AC-WS-07); Rule 9 (drain window — covered by Rule 14 in H.3).
- F-1 through F-7: F-1/F-1b implicit in H.2 + H.3 slot accounting; F-2 in AC-WS-09 + AC-WS-20; F-3 in AC-WS-14; F-3b in AC-WS-10; F-4 in H.3 lifecycle; F-5 in AC-WS-06; F-6 in AC-WS-07; F-7 binding via Rule 15 cook-time + AC-WS-21 immutability.
- EC-WS-1 through EC-WS-18: 5 highest-severity edges in H.6 (EC-WS-3/4/15/17/18). Remaining edges covered transitively through their associated Rule ACs.
- Seam 13 contract: AC-WS-19 + AC-WS-16.
- 8 forward contracts (R14 cascade): all embedded — Path B keys (AC-WS-21 immutability), pool_size + DESPAWNING_RETURN_LATENCY_SLOTS (AC-WS-09, AC-WS-20), Seam 13 slot (AC-WS-19), tier ≥ 2 (AC-WS-03), 7-triplet enum (AC-WS-05), W = 0.35s (AC-WS-06), FLOOR ceiling 0.76s (AC-WS-21 ceiling immutability).

No untraced binding surface identified at authoring time. Items flagged for cross-doc verification land in `/consistency-check` per Dependencies § *Bidirectional consistency to verify*.

## Open Questions

Open Questions surfaced during A–H authoring. Each has a clear forum for resolution (forum-shaped: GDD update, ADR, registry change, or consistency-check pass) so resolution work is not lost in conversation. None block authoring closure of this GDD or initial `/design-review` — they are durable hand-offs.

| ID | Question | Surfaced in | Resolution forum | State |
|---|---|---|---|---|
| OQ-WS-1 | If `MAX_CONCURRENT_WAVES_CAP` ever raises above 21, should it migrate from a hard-coded DPC constant to a registry-locked value? Today the cap is duplicated across DPC + the F-2 derivation; a raise touches both lock-step. A registry entry would make the lock-step explicit (`dual-grep` per ADR-0004), but adds a registry surface for a value that hasn't moved across 14 review rounds. | Tuning Knobs § G.2 (recompute trigger), Formulas § F-2 (cross-knob trigger) | DPC + Pull-Wave joint decision when (if) the cap moves. Filing here so the decision finds the cascade map. | OPEN — not on critical path; revisit at first cap-raise proposal. |
| OQ-WS-2 | `OnPatternAdmitted` multicast delegate signature for HUD subscribers — exact payload (`WaveId`, `pattern_id`, `current_phase`, `is_barrage`, `telegraph_window_s`?) and broadcast timing (admission-tick edge vs post-onset-fire). | Dependencies row 8 (HUD/Audio); forward contract C.3 §9. | HUD GDD (system #9, undesigned). Signature finalized when HUD GDD lands; this GDD inherits the contract via `/propagate-design-change`. | OPEN — pending HUD GDD authoring. |
| OQ-WS-3 | ~~Subsystem hosting class — `UGameInstanceSubsystem` (replay-friendly; recommended in Dependencies § E3) vs `UWorldSubsystem` (per-world; requires re-init on replay).~~ | Dependencies § E3 lateral row. | ADR: `docs/architecture/adr-NNNN-wave-spawner-subsystem-hosting.md`. Assign to `unreal-specialist`. Sibling to OQ-PW-3 (HISM/ISMC ADR). | **CLOSED R1a 2026-06-21** — locked as `UGameInstanceSubsystem` per perf-analyst R7 + CD R1 Q2-locked user decision. Rationale: UWorldSubsystem would fire 16.6ms per-replay pool-alloc hitch, materially degrading the core retry loop. ADR is now downstream artifact authorship (documenting the locked decision) rather than adjudication. Rule 7, Rule 11, Dependencies § E3 + Engine subsystem integration all updated lock-step. |
| OQ-WS-4 | ~~Per-run RNG seed source — Rule 8 captures the seed at `Cold → Active` but the seed *origin* is unspecified.~~ | Detailed Rules § Rule 8; AC-WS-13 verification. | RSM GDD update — RSM Section *Public Interface* should expose a `RunSeed` field that this spawner reads at `Cold → Active`. | **CLOSED R1a 2026-06-21** — per CD R1 Ruling 2: RSM-supplied `RunSeed : uint64` field is the binding source (candidate (a) from prior options). Inscribed in Rule 8. Forward contract on RSM filed via Dependencies § Bidirectional consistency row 1 — RSM GDD update at next `/consistency-check` adds `RunSeed` to its public interface. Wall-clock-derived seeds (candidate (c)) explicitly forbidden. AC-WS-13 verifiable as BLOCKING-at-story-Done once RSM RunSeed lands (no downgrade required). |
| OQ-WS-5 | Pause-during-COUNTDOWN edge — Rule 13 implicitly assumes `OnPausedChanged(true)` fires only in `RUNNING`. If RSM permits pause during `COUNTDOWN`, the spawner has no in-flight waves to flush, no `last_spawn_time` to preserve (pool not yet pre-allocated), and no admission-pipeline state. Should the spawner be a no-op subscriber while in `Idle` / `Cold`, or should RSM gate the pause signal at the state machine level? | Detailed Rules § Rule 13; Edge Cases EC-WS-18 boundary. | RSM `/consistency-check` pass + (if RSM gates) RSM GDD update; (if spawner no-ops) Rule 13 spec amendment for the `Cold` / `Idle` branches. | OPEN — file under RSM consistency-check follow-up #5 (extends Dependencies § Bidirectional consistency list). |
| OQ-WS-6 | `design/gdd/telemetry.md` cross-doc inheritance contract — when the telemetry GDD is authored, it MUST inherit Section C.3 §9 schema + G.3 rate-limit policy as binding (not negotiable downstream). Open: does telemetry.md additionally specify an analytics-pipeline-side rate-limit (e.g., per-IP throttle) that could supersede the spawner-side once-per-session policy, or is the spawner's policy the floor? | Tuning Knobs § G.3 telemetry rate-limit policy; Section C.3 §9 telemetry event list. | Telemetry GDD (system, undesigned). Spawner-side policy is BLOCKING-inherit; the open piece is whether telemetry.md layers additional limits above it. | OPEN — pending telemetry GDD authoring. |
| OQ-WS-7 | `POOL_SIZE_DERIVATION_MATCH` cook-time check (AC-WS-09) tooling — Rule 15 enumerates the check as BLOCKING but the cook-time validation infrastructure (the pattern-asset pipeline) currently runs against pattern *assets*, not against the spawner's pool-allocation code path. Is the check implementable as an asset-pipeline contract, or does it need to become a `tests/unit/wave_spawner/` automated assertion that runs in CI? | Acceptance Criteria § AC-WS-09; Rule 15 cook-time checks. | `tools-programmer` to assess infrastructure fit; if asset-pipeline can't host it, route to `tests/unit/` with a CI gate that mirrors cook-time binding semantics. | OPEN — does not block story-Done if a CI assertion provides equivalent coverage. |

**Resolution sequencing (suggested ordering for downstream work; updated R1a 2026-06-21):**

1. **OQ-WS-4 (RNG seed) — CLOSED R1a** — RSM `RunSeed` forward contract inscribed in Rule 8; RSM GDD update at next `/consistency-check` adds the field.
2. **OQ-WS-3 (subsystem hosting) — CLOSED R1a** — locked as `UGameInstanceSubsystem`; ADR is downstream artifact authorship.
3. **OQ-WS-5** (pause-during-COUNTDOWN) at next RSM `/consistency-check` pass.
4. **OQ-WS-2 + OQ-WS-6** (HUD signature + telemetry inheritance) inherit at the downstream GDD's authoring.
5. **OQ-WS-7** (cook-time tooling fit) routed to `tools-programmer` opportunistically; CI fallback acceptable.
6. **OQ-WS-1** (cap registry migration) revisited only if a cap raise is proposed.

R1a closure tally: 2 of 7 OQs closed (OQ-WS-3, OQ-WS-4). Remaining 5 OQs are forward-looking hand-offs that find their resolution forum cleanly; none block `/design-review` R2.

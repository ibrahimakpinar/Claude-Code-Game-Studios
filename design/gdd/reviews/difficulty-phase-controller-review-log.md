# Difficulty & Phase Controller — Review Log

Reviews of `design/gdd/difficulty-phase-controller.md`. Each entry summarises a single `/design-review` invocation.

---

## Review — 2026-06-04 — Verdict: MAJOR REVISION NEEDED
Scope signal: L
Specialists: game-designer, systems-designer, qa-lead, performance-analyst, audio-director, level-designer, unreal-specialist, creative-director (synthesis)
Blocking items: 12 | Recommended: ~15 | Specialist disagreements: 2 (phase-enum step + audio sting — adjudicated keep; pause fix mechanism — adjudicated `is_active` flag over snapshot freeze)
Prior verdict resolved: First review

Summary: First review. GDD is mature and well-authored but contains a coherent cluster of structural gaps that reinforce each other. Three fault lines: (1) Lifecycle contract incomplete — pause/resume + mid-run subscription + state gating produce real bugs (Pillar 5 mystery audio violations on pause-entry/exit). (2) Pillar 2 unvalidated under PEAK concurrent telegraph load — 0.6s floor validated for single wave only, no AC for 3 simultaneous telegraphs. (3) Wave Spawner cross-system contract too vague — "spawn cycle," "pattern pool," and drain mechanism undefined; downstream GDD blocked. Secondary issues: null-curve fallback incoherent (per-scalar floors point in opposite directions); thermal fallback authoring incomplete; RUN_DURATION_S Tuning Knobs claim false at low end; Unreal type/API errors throughout (`.Eval` should be `.GetFloatValue`, struct should be USTRUCT, pointers should be TObjectPtr); RSM test stub undefined; multiple missing state-gate ACs.

Resolved in-session: All 12 blockers + ~15 recommended items across the single GDD file. Major additions: `is_active: bool` field in FDPCFrameState, Rule 16 (is_active gate, binding on consumers), Rule 17 (mid-run subscription protocol), EC-9/EC-10/EC-11/EC-12 (pause-entry/exit, RUN_DURATION_S=0, NaN guard), Wave Spawner Contract Definitions subsection (user-bound: spawn drain + pre-authored static set), AC-PILLAR-2-CONCURRENT (Alpha-blocking Visual/Feel AC requiring playtest), AC-09a/b/c/d/e (state gates for IDLE/COUNTDOWN/COMPLETE/RESOLVING/DEAD), AC-14a/b/c (boundary + pause/resume), AC-22 split (22a ADVISORY on-device + 22b BLOCKING structural CI). AC count: 22 → 39.

User decisions made during revision:
- Wave Spawner drain mechanism: SPAWN DRAIN (no new patterns within max_pattern_length_s of boundary; in-flight complete)
- Pattern pool semantics: PRE-AUTHORED STATIC SET per phase

CD adjudications:
- Phase enum step + audio sting: KEEP (game-designer's argument that the sting fulfills Pillar 5 by announcing storm escalation prevails over concerns about arbitrariness)
- Pause fix mechanism: `is_active` flag in FDPCFrameState (snapshot freeze rejected — would lie about gameplay state and force consumers to re-derive run-active from other signals)

Specialist disagreements unresolved (escalated to user / future review):
- AC-PILLAR-2-CONCURRENT is playtest-gated — closing this AC requires data from the highest-risk-bet prototype (Telegraph + slip loop) before the floor can be locked.
- OQ-DPC-3 (boundary fractions) similarly requires prototype data.

Convergence trajectory: Author's prior record (Input System 27 reviews, RSM 6, Player Movement 7) and this GDD's structural maturity suggest a 6–9-review path closer to RSM than Input. Estimated 2-3 revision cycles to APPROVE.

Forward-imposed contracts on downstream GDDs (must be respected when authored):
- Wave Spawner: spawn drain + pre-authored static set + Rule 16 is_active gate + introductory OPENER pattern + boundary-closure AC
- Audio Controller: Rule 16 is_active gate + per-transition sting identity + crossfade ≤500ms + tempo character (Pillar 4)
- Pull-Wave Behavior: spawn-time velocity (no mid-flight update) + Rule 16 gate
- Telegraph System: do not impose own floor below DPC's TELEGRAPH_WINDOW_FLOOR_S
- All consumers: Rule 17 mid-run subscription protocol

Test infrastructure created/required:
- IRSMTestStub (defined in platform-seam-interfaces.md)
- FTestLogCapture (FOutputDevice subclass)
- DPCCanonicalCurves_TestFixture (canonical curve set)
- 1e-9 double comparison tolerance

Separate flag for producer (not a GDD revision item):
- `docs/engine-reference/godot/VERSION.md` is Godot 4.6 but project targets UE5 mobile. `.claude/docs/technical-preferences.md` unpopulated. Run `/setup-engine`.

---

## Review — 2026-06-04 — Verdict: MAJOR REVISION NEEDED (re-review 2)
Scope signal: L
Specialists: game-designer, systems-designer, qa-lead, performance-analyst, audio-director, level-designer, unreal-specialist, creative-director (synthesis)
Blocking items: 16 | Recommended: 21 | Editorial defects: 2 (rule + EC numbering out of order)
Prior verdict resolved: Partial — all 12 review-1 blockers landed correctly; 16 new blockers are second-order (boundary-of-boundary cases, downstream contract precision, UE5.7 macro mechanics) — the layer that gets exposed once first-order gaps resolve.

Summary: The structural maturity holds — Rule 16 is_active gate, EC-7/9/10, Wave Spawner Contract Definitions, AC-09a–e state gates, AC-22a/b split all landed correctly. New blockers cluster into 6 fault lines:
 (A) Math/range guards — RUN_DURATION_S=10s with default curves produces ZERO OPENER waves (Pillar 3 break); Rule 14 boundary check() does not guard OPENER_END=0, OPENER_END<0, or PEAK_START>1.0.
 (B) Lifecycle/timing — pause-straddling-boundary swallows escalation sting (Pillar 5 risk); async-load race between Initialize() and FStreamableHandle.OnComplete; AC-NULL-CURVE-INACTIVE needs explicit no-short-circuit policy.
 (C) Downstream contract clarity — PEAK ≥3 patterns undersized for ship (bind ≥5 Alpha); "per session" in intro-pattern undefined; run-termination drain unspecified.
 (D) AC quality — TELEGRAPH_WINDOW_FLOOR_S=0.6s declared locked everywhere + AC-PILLAR-2-CONCURRENT says cannot be locked (contradiction); IRSMTestStub claimed in seam doc but absent (6 ACs blocked); AC-RUN-DURATION-ZERO Fatal capture aborts process; AC-22b "static analysis of compiled binary" incoherent.
 (E) UE 5.7 correctness — USTRUCT(BlueprintType) fields missing UPROPERTY (Blueprint access + Death Replay serialization broken); AC-22b missing DECLARE_CYCLE_STAT; tick-prereq mechanism incoherent for UGameInstanceSubsystem.
 (F) Pillar fidelity — spawn drain creates 1–4s lull misattributed as Pillar 5 (pressure on user's 2026-06-04 decision; CD recommends keep mechanism + bound max_pattern_length_s per phase + correct attribution).

Resolved in-session: NONE. User chose: stop here, revise in fresh session.

CD top 3 unblock priorities (resolve in revision before re-review 3):
 1. Author IRSMTestStub (SetRemainingTime/SetCurrentState/SetIsPaused/SetRunDurationS) in docs/architecture/platform-seam-interfaces.md — unblocks 6 ACs.
 2. Resolve TELEGRAPH_WINDOW_FLOOR_S lock-vs-AC contradiction — pick provisional registry status, or promote AC-PILLAR-2-CONCURRENT to GDD-blocking.
 3. Fix scalar-curve range failure at RUN_DURATION_S<30s + add check(OPENER_END > 0.0) and check(PEAK_START <= 1.0) + AC-OPENER-ZERO/AC-PEAK-BEYOND-ONE.

Specialist disagreements unresolved (escalated to user / future review):
- [game-designer] pressure on spawn drain mechanism (user-bound 2026-06-04). CD adjudication: keep mechanism, adopt per-phase max_pattern_length_s cap (e.g., OPENER ≤2s), re-attribute rationale honestly (operational simplicity, not Pillar 5).
- AC-PILLAR-2-CONCURRENT still playtest-gated (Alpha milestone, not GDD acceptance) — TELEGRAPH_WINDOW_FLOOR_S lock cannot be permanently closed until prototype data lands.

Editorial defects flagged (not blocking but need cleanup):
- Rule numbering out of order: lines 22–119 number 1–11, then 16, 17, then 12, 13, 14, 15. Cross-refs use new numbers correctly so renumbering will require touching all refs.
- Edge Case numbering out of order: EC-1 through 7, then 9, 10, 11, 12, then 8.

Convergence trajectory: ~16 new blockers comparable to review 1's 12 — convergence has not tightened yet, but blockers are second-order (expected post first-order resolution). CD estimate: 3–4 more cycles to APPROVE. Tracking between RSM (6 reviews) and Player Movement (7), not Input System (27). GDD structural maturity holds.

Forward-imposed contracts on downstream GDDs (still binding, plus 4 added/clarified this review):
- All review-1 contracts stand (Wave Spawner spawn drain + static pool + Rule 16 gate; Audio Controller sting identity + crossfade + tempo; Pull-Wave spawn-time velocity; Telegraph floor non-imposition; all consumers Rule 17).
- Wave Spawner: ≥5 PEAK patterns for Alpha (was ≥3); "per session" intro-pattern semantics must be defined; run-termination drain clause symmetric with phase-boundary drain; per-phase max_pattern_length_s cap (recommended OPENER ≤2s per CD).
- Audio Controller: resume mix onset (cold-cut vs fade-in on is_active false→true) distinct from phase-transition sting; OPENER→PEAK skip case (if MID is skipped via frame hitch); sting density may need to scale with RUN_DURATION_S.

Test infrastructure required (re-stated; review-1 IRSMTestStub claim was paper-only):
- IRSMTestStub interface in platform-seam-interfaces.md (NEW — currently absent)
- FConsumerTestStub (mentioned in AC-14b, missing from preconditions block)
- AC for EC-8 (flat curves)
- DPCTestFatalHandler wrapper OR demoted Fatal→Error policy for AC-RUN-DURATION-ZERO
- Source-level CI grep for AC-22b (replace "static analysis of compiled binary")

---

## Review — 2026-06-04 — Verdict: MAJOR REVISION NEEDED (re-review 3)
Scope signal: L
Specialists: game-designer, systems-designer, qa-lead, performance-analyst, audio-director, level-designer, unreal-specialist, creative-director (synthesis)
Blocking items: 21 | Recommended: 23 | Specialist disagreements: 2 (Pillar-5 sting attribution — CD reversed R1 in favor of game-designer; per-phase max_pattern_length_s cap — CD re-bound R2 adjudication)
Prior verdict resolved: Partial — out-of-session revision since R2 added Rule 16/17, EC-9/10/11/12, Wave Spawner Contract Definitions, AC-PILLAR-2-CONCURRENT, AC-09a–e, AC-NULL-CURVE-INACTIVE, AC-NAN-GUARD, AC-RUN-DURATION-ZERO, AC-THRESHOLD-ORDER, AC-MIN-MID-WIDTH, inactive snapshot caching, AC-22a tiered budget. But 9 of 16 R2 blockers AND all 3 R2 CD top-3 priorities carried over unresolved.

Summary: Convergence stall, not second-order layer reveal. R1 demonstrated convergence capacity (12 blockers resolved in-session). R2's out-of-session revision pass missed direct CD top-3 directives. R3 surfaces three failure clusters: (Cluster I — Test infrastructure absent) IRSMTestStub still not authored in platform-seam-interfaces.md (qa-lead verified by file read); FConsumerTestStub missing from infrastructure block; DPCCanonicalCurves_TestFixture asset path unspecified; AC-22b "static analysis or test harness" still ambiguous. Blocks 8 ACs. (Cluster II — UE5 correctness) FDPCFrameState fields still missing UPROPERTY (BlueprintType + Death Replay reflection broken); ERunPhase still missing UENUM(BlueprintType):uint8; UE_LOG Fatal aborts process across AC-RUN-DURATION-ZERO/AC-THRESHOLD-ORDER/AC-MIN-MID-WIDTH (mechanically impossible post-Fatal sequences); AC-22b missing DECLARE_STATS_GROUP + DECLARE_CYCLE_STAT pairing (won't compile); Rule 3 tick-prereq unconditional but EC-4 admits UGameInstanceSubsystem unenforceable; async-load race between Initialize() inactive-snapshot cache and FStreamableHandle.OnComplete; check(OPENER_END>0.0) + check(PEAK_START<1.0) still missing (CD R2 top-3 #3 unresolved). (Cluster III — Binding decisions deferred or under-bound) TELEGRAPH_WINDOW_FLOOR_S lock-vs-AC-PILLAR-2-CONCURRENT contradiction persists across 3 sections (CD R2 top-3 #2 unresolved); PEAK ≥3 patterns (CD R2 said bind ≥5); "per session" intro-pattern still undefined; per-phase max_pattern_length_s cap not adopted despite CD R2 adjudication; run-termination drain unspecified.

New second-order findings (revealed after R2 fixes): systems-designer B-3 (EC-12 NaN guard unreachable post-clamp — AC-NAN-GUARD tests impossible behavior); performance-analyst D-1 (two factual errors in AC-22a budget rationale — struct size 24→32 bytes with alignment; inactive lifetime claim 80%→actual ~40-50%); level-designer B-5 (intra-pattern simultaneous telegraph onset is DPC-contract-compliant but Pillar-2-unsurvivable; need stagger constraint); audio-director B-1 (mix-resume continuity unspecified — Rule 16 silences sting but not buffer-position behavior on backgrounding); audio-director B-3 (sting/crossfade envelope ordering unspecified — masking risk).

CD ADJUDICATION REVERSAL (R1 → R3): Phase-transition sting attribution to Pillar 5 ("Skill Is Visible") is incorrect and is hereby reversed. Pillar 5 is about death attribution (no mystery RNG saves, no mystery deaths); a timer-driven cue at t_norm=0.333 or 0.75 fires regardless of player action and cannot serve a skill-visibility pillar. Re-attribute under Pillar 3 (run-arc legibility) primary, Pillar 4 (tonal escalation through world behavior) secondary. game-designer's D-1 argument prevails. Edit required at GDD lines 166 + 388. CD accepts reputational cost of R1 misadjudication.

CD ADJUDICATION RE-BINDING (R2 → R3): Per-phase max_pattern_length_s cap is re-bound as MANDATORY for R4: OPENER ≤2.0s, MID ≤3.0s, PEAK uncapped (no closing boundary). Author held mechanism without cap in R2 revision; cap stands as CD direction. Resolves game-designer B-4 (4s OPENER pre-MID silence) directly. Wave Spawner GDD inherits binding.

CD top-3 unblock priorities for R4 (three ATOMIC PASSES, not piecemeal):
1. Pass A (Test Infrastructure) — Author IRSMTestStub + FConsumerTestStub interfaces in docs/architecture/platform-seam-interfaces.md (rename file scope from Input-System-specific to multi-system seam catalog); specify DPCCanonicalCurves_TestFixture asset path; replace AC-22b "static analysis or test harness" with concrete CI source-grep pattern (rg invocation against .cpp). Unblocks 8 ACs.
2. Pass B (UE5 Correctness) — Add UPROPERTY(BlueprintReadOnly) to every FDPCFrameState field; promote ERunPhase to UENUM(BlueprintType) enum class : uint8; demote UE_LOG Fatal to Error+ensureAlways across lines 110/354/548/549/550; add DECLARE_STATS_GROUP + DECLARE_CYCLE_STAT to AC-22b requirement; move inactive snapshot caching from Initialize() to post-OnComplete first-tick; move NaN guard pre-division on remaining_time before F-1 clamp; add FMath::IsFinite on curve return values before FloorToInt32; add check(IsInGameThread()) to GetCurrentFrameState(); condition Rule 3 tick-prereq on OQ-7 ADR (mark unenforceable on UGameInstanceSubsystem path).
3. Pass C (Binding Decisions) — TELEGRAPH_WINDOW_FLOOR_S marked PROVISIONAL pending AC-PILLAR-2-CONCURRENT close (consistent across Tuning Knobs + F-3b + Rule 9); PEAK ≥5 patterns at Alpha + OPENER ≥3 + MID ≥4; per-phase max_pattern_length_s cap (OPENER ≤2s, MID ≤3s, PEAK uncapped); run-termination drain clause; "per session" intro-pattern bound to "first OPENER pattern per IDLE→COUNTDOWN→RUNNING transition; not repeated on pause resume"; intra-pattern telegraph stagger constraint (no two onsets within TELEGRAPH_WINDOW_FLOOR_S in same pattern); Pillar 5 sting attribution removed at lines 166+388 and re-attributed Pillar 3+4; check(OPENER_END_NORMALIZED > 0.0) + check(PEAK_START_NORMALIZED < 1.0) + AC-OPENER-ZERO + AC-PEAK-BEYOND-ONE; AC-22a budget rationale corrected (struct size + lifetime %).

Specialist disagreements (CD-resolved this review):
- D-1 (game-designer vs CD-R1): Pillar 5 sting attribution. CD REVERSED R1 in favor of game-designer.
- D-1 (level-designer vs author-R2): per-phase max_pattern_length_s cap. CD RE-BOUND as mandatory.
- (game-designer R-1 vs user-R1): "pre-authored static set" location. CD accepts user's R1 location decision; add joint-ownership annotation.

Specialist disagreements unresolved (escalated to user / R4):
- AC-PILLAR-2-CONCURRENT remains playtest-gated at Alpha — TELEGRAPH_WINDOW_FLOOR_S provisional/locked decision must be CD-bound consistent across GDD in R4 Pass C; final lock cannot close until prototype data lands.

Editorial defects flagged again (non-blocking but persist):
- Rule numbering still 1–11, 16, 17, 12–15 out of order (cross-refs use new numbers; renumber-to-sequential pass requires touching all cross-refs).
- Edge Case numbering still EC-1 through 7, then 9, 10, 11, 12, then 8.
- AC numbering heterogeneous (sequential AC-01–AC-22b + alpha-suffix AC-09a–e/AC-14a–c + named AC-NAN-GUARD/AC-RUN-DURATION-ZERO/AC-THRESHOLD-ORDER/AC-MIN-MID-WIDTH/AC-NULL-CURVE-INACTIVE/AC-PILLAR-2-CONCURRENT) — recommend keep heterogeneous; named ACs carry semantic weight. Document canonical ordering in test-suite mapping.

Convergence trajectory: REVISED. R3 was expected to tighten; carryover failure pattern means convergence stalled. Trajectory FORK: if R4 runs the three atomic passes as a single coherent revision, estimate 2–3 cycles to APPROVE (tracks RSM/Player Movement). If R4 revises piecemeal, estimate 4–6 cycles (drifts toward Input System trajectory). Author's revision strategy choice on R4 determines outcome.

Forward-imposed contracts on downstream GDDs (all R1+R2 contracts stand; R3 adds/clarifies 7):
- All R1+R2 contracts hold (Wave Spawner spawn drain + static pool + Rule 16 gate; Audio Controller sting identity + crossfade + tempo; Pull-Wave spawn-time velocity; Telegraph floor non-imposition; all consumers Rule 17; ≥5 PEAK Alpha; "per session" intro; run-termination drain symmetric).
- Wave Spawner (added/clarified): per-phase max_pattern_length_s cap (OPENER ≤2s, MID ≤3s, PEAK uncapped); intra-pattern telegraph stagger ≥ TELEGRAPH_WINDOW_FLOOR_S between onsets; pattern selection rule must satisfy Pillar 5 constraint (no consecutive same-pattern repeats within phase + cross-run pattern distribution); run-termination drain clause (in-flight PEAK patterns complete; no new patterns post is_active=false; consumers gate via Rule 16).
- Audio Controller (added/clarified): 5th spec item — mix-resume onset behavior on is_active rising edge (Audio Controller GDD specifies buffer-position vs fade-in vs cold-cut); sting/crossfade envelope coordination (crossfade rides under sting decay tail); ≤500ms crossfade rationale tied to drain window; sting density at short RUN_DURATION_S acknowledged.
- Telegraph System: floor labeled provisional pending AC-PILLAR-2-CONCURRENT close.

Test infrastructure (re-stated; R2 IRSMTestStub claim was paper-only AND R3 verified file still does not contain it):
- IRSMTestStub interface in platform-seam-interfaces.md — STILL ABSENT (qa-lead verified by file read; file scopes itself to Input System with 6 Input-System seams).
- FConsumerTestStub (mentioned in AC-14b, STILL missing from preconditions block).
- DPCCanonicalCurves_TestFixture (named in line 481, no asset path bound).
- AC-22b source-grep CI mechanism (still ambiguous "static analysis or test harness").
- Demoted Fatal→Error policy + ensureAlways approach for AC-RUN-DURATION-ZERO / AC-THRESHOLD-ORDER / AC-MIN-MID-WIDTH.

Separate flags for producer (not GDD revision items):
- IRSMTestStub + FConsumerTestStub are SHARED test infrastructure consumed by Wave Spawner, Audio Controller, Telegraph System, Pull-Wave Behavior, Death Replay. Recommend Producer extract as separate "Test Seam Infrastructure" line item attributable to DPC for sequencing but counted as shared work in cost terms.
- docs/engine-reference/godot/VERSION.md still pinned to Godot 4.6; project targets UE5 mobile. `.claude/docs/technical-preferences.md` still unpopulated. `/setup-engine` still not run.

---

## Review — 2026-06-05 — Verdict: MAJOR REVISION NEEDED (re-review 4)
Scope signal: L
Specialists: game-designer, systems-designer, qa-lead, performance-analyst, audio-director, level-designer, unreal-specialist, creative-director (synthesis)
Blocking items: 16 | Recommended: ~25 | Specialist disagreements: 4 (all CD-resolved)
Prior verdict resolved: Yes (mostly) — R3 atomic 3-pass revision verified landed across all 3 passes. Test infrastructure (Seam 7 IRSMTimeStateProvider/FRSMTestStub + Seam 8 IDPCSnapshotConsumer/FConsumerTestStub) authored in platform-seam-interfaces.md. UE5 correctness (UPROPERTY on every FDPCFrameState field, UENUM:uint8 on ERunPhase, Fatal→Error+ensureAlways across 5 sites, DECLARE_STATS_GROUP+DECLARE_CYCLE_STAT+SCOPE_CYCLE_COUNTER bound, async-load caching deferred to FStreamableHandle::CompleteDelegate, input-side NaN guard pre-division, output-side IsFinite on curve returns, check(IsInGameThread()) in Rule 15, Rule 3 enforceability matrix with 3-object-type paths) all verified at specified locations. Binding decisions (TELEGRAPH_WINDOW_FLOOR_S PROVISIONAL consistent across Rule 9/F-3b/Tuning Knobs/registry, check(OPENER_END>0)+check(PEAK_START<1)+ACs, PEAK ≥5/MID ≥4/OPENER ≥3 variety floors, per-phase max_pattern_length_s caps OPENER ≤2s/MID ≤3s/PEAK uncapped, run-termination drain, intro-pattern "first OPENER per RUNNING entry", intra-pattern telegraph stagger, Pillar 5 sting attribution reversed → Pillar 3 primary + Pillar 4 secondary at all 4 sites, AC-NAN-GUARD rewritten reachable, AC-22a struct alignment math + corrected lifetime + cold-cache scenario, 7-item Audio Controller binding, OQ-DPC-1 closed directional NO) all verified.

Summary: Convergence trajectory positive (R1=12 → R2=16 → R3=21 → R4=16 blockers). Critically, R4 blocker COMPOSITION shifted from foundational (R1-R3 "the design isn't specified yet") to second-order (residual seam errors + edge cases created by new bindings + ONE genuine identity question). This is the signature of converging design, not stall. R5 should drop to single-digit blockers if atomic-by-cluster structure is followed.

R4 blockers cluster into 3 tiers:

**Tier 1 — Identity-critical (2 blockers, MUST FIX before APPROVE)**
- B1 [game-designer]: Path B silently deletes PEAK identity. Lines 213/220 define PEAK = "Telegraph at the floor (0.6s). Survival = pattern literacy." If AC-PILLAR-2-CONCURRENT Path B fires and floor rises to 0.9s, PEAK's telegraph window EQUALS OPENER's default — PEAK becomes density-only. Resolution paths don't acknowledge.
- B2 [game-designer + level-designer]: Simultaneous-pulls Player Fantasy vs intra-pattern stagger constraint contradiction. Fantasy (line 14-16): "simultaneous pulls, the storm at its loudest." Intra-pattern stagger (line 494): "no two telegraph onsets within TELEGRAPH_WINDOW_FLOOR_S of each other within any single pattern" — converts simultaneous into rapid-sequential at ≥0.6s spacing. Cross-pattern stagger UNPROTECTED — two near-simultaneous patterns CAN produce 3 telegraph onsets within 0.6s (same Pillar 2 risk the constraint was meant to prevent). Contract forbids simultaneous within a pattern but fails to deliver Pillar 2 across patterns AND breaks the fantasy.

**Tier 2 — Correctness-critical mechanical (9 blockers)**
- [qa-lead] Seam 7 line 700 cold-boot null deref: `RSM.IsValid() ? RSM->GetRemainingTime() : RSM->GetRunDurationS()` — false branch dereferences null TWeakObjectPtr. Fix: `: GetRunDurationS();` (self-call to fallback). CD adjudicated BLOCKING over unreal-specialist's REC.
- [systems-designer] FloorToInt32 UB for large-but-finite curve returns. IsFinite guard catches NaN/Inf only; finite ≥2^31 passes through to implementation-defined `FMath::FloorToInt32`. Fix: MAX_CONCURRENT_WAVES_CAP = 16 non-tunable + Clamp before truncation + AC-08c.
- [systems-designer] Async-load race produces zero-gameplay run. RSM can enter RUNNING before FStreamableHandle::OnComplete fires; player sees 60s blank run (no spawns, no telegraphs, no audio). No policy. Three options: Block (Initialize synchronous) / Gate RSM (RSM gates on DPC.IsInitialized) / Bound-and-Abort (timeout → ABORTED).
- [level-designer] max_concurrent_waves cap enforcement semantic undefined. Line 69 says "as a cap" — but skip-and-drop vs defer-until-slot-opens vs cancel-oldest produce categorically different PEAK gameplay rhythms. Wave Spawner author cannot proceed.
- [audio-director] Old mix layer fade-DOWN timing undefined. Item 4 specifies fade-UP rides under sting decay tail; fade-DOWN unspecified. Either sting plays into silence OR fade-down holds at full level concurrent (masking risk returns — defeats item 4's entire purpose).
- [unreal-specialist] Rule 3 line 28 UWorldSubsystem "hard contract" claim FACTUALLY WRONG. UTickableWorldSubsystem uses FTickableGameObject — does NOT participate in FTickFunction prerequisite graph; TickGroup/AddPrerequisite/TG_PrePhysics don't apply. Only AActor/UActorComponent path is genuinely hard. CD adjudicated GDD-level (NOT defer-to-OQ-7) because downstream contracts depend on ordering guarantee.
- [unreal-specialist] FDPCController F-prefix vs UPROPERTY/TObjectPtr contradiction. Rule 10 line 110 claims TObjectPtr<UCurveFloat> GC tracking via UPROPERTY() — but F-prefix denotes non-UObject and UPROPERTY is silently ignored outside UObjects.
- [unreal-specialist] Ownership model contradiction. Line 462 says "raw observer pointer"; Seam 7 examples show TUniquePtr transfer via MoveTemp.
- [systems-designer] Rule 17 "skip one tick" silences is_active rising edge. New subscriber during pause: Rule 17 skips diff handler one tick; if that tick is un-pause rising edge, Audio Controller item 5 (mix-resume) never invoked. Fix: skip phase-diff behavior only, not is_active rising-edge behavior.

**Tier 3 — Verification-critical (3 blockers)**
- [qa-lead] AC-NAN-GUARD phantom "F-1 evaluated?" boolean hook has no definition anywhere. Snapshot observation is sufficient (NaN-into-clamp → t_norm=1.0; guard → t_norm=0.0, distinguishable). Remove the hook demand.
- [qa-lead] Shipping-branch of AC-THRESHOLD-ORDER, AC-MIN-MID-WIDTH, AC-OPENER-ZERO, AC-PEAK-BEYOND-ONE permanently untestable in standard test config. check() aborts test process in Development; shipping-build parallel guard never runs in tests. Fix: #if DPC_TEST_DISABLE_CHECKS seam OR explicit scoping.
- [audio-director] Item 6 "blind A/B distinguishability" has no QA AC protocol. Compare to AC-PILLAR-2-CONCURRENT rigor (n, threshold, methodology). Add protocol stub.

CD ADJUDICATIONS (4):
1. Seam 7 null deref severity: qa-lead=BLOCKING vs unreal-specialist=RECOMMENDED. **CD adjudicates BLOCKING** (cold-boot crash on real path).
2. UWorldSubsystem tick group: defer-to-ADR vs GDD-level. **CD adjudicates GDD-level BLOCKING** (downstream contracts depend on ordering guarantee).
3. R3 Pillar-5→Pillar-3+4 reversal stress-test: game-designer R-3 challenged Pillar 4 attribution. **CD adjudicates HOLD THE LINE** — the reversal was correct; better attribution rationale is improvement, not another reversal. Reverse-reversing within two cycles = credibility hit.
4. Simultaneous-pulls adjudication (Tier 1 B2): **CD binds Option B (barrage-pattern carve-out) as recommended R5 direction pending user veto**. Reasoning: Option A (revise fantasy to "rapid succession") is honest and shippable but permanently shrinks PEAK's "loudest moment" identity to fit a constraint built for small moments. Option B preserves the fantasy at exactly the climax moment + bounds Pillar 2 risk to designed barrage events the player can be telegraphed for. Cost: +5-7 days for barrage authoring spec. Option C (remove intra-pattern stagger) rejected — reopens R3 wound.

R3 reversal verified stuck: Pillar-5→Pillar-3+4 attribution present at all 4 sites (Rule 7 line 46, EC-9 line 441-442, Audio Controller dependency row, Visual/Audio Requirements line 556). No regression.

Performance-analyst: 0 blocking, 2 recommended (FloorToInt32 cap dup of systems-designer, async timeout watchdog). Clean pass.

R5 path: ATOMIC-BY-CLUSTER, OUT-OF-SESSION (user chose: stop here, revise out-of-session).
- **Cluster A (identity reconciliation — GATES B+C)**: adjudicate Option A/B/C on simultaneous-pulls + specify cross-pattern stagger policy with numeric tolerance + specify PEAK identity statement under chosen path. Updates Player Fantasy section, pillar alignment, PEAK-specific rules. Owner: creative-director + game-designer. 2-3 days if A, 5-7 days if B.
- **Cluster B (mechanical correctness, parallel after A locks)**: Seam 7 null-deref fix + MAX_CONCURRENT_WAVES_CAP + clamp + AC-08c + async-load race policy + max_concurrent_waves enforcement semantic + old mix layer fade-down timing + UWorldSubsystem resolution + FDPCController/UDPCController naming + ownership unification + Rule 17 rising-edge fix. Owner: systems-designer + unreal-specialist. 2-3 days.
- **Cluster C (AC honesty, parallel after A locks)**: remove phantom hook + fix 4 check()-aborting ACs + add blind A/B AC protocol + raise AC-PILLAR-2 to n≥30 + replace `0.6s` literals with TELEGRAPH_WINDOW_FLOOR_S symbol. Owner: qa-lead + audio-director. 1-2 days.

R5 validation criteria: Cluster A produces internally consistent Player Fantasy + PEAK rules on first reviewer pass. Cluster B closes all unreal-specialist + systems-designer BLOCKING. Cluster C makes every AC observable in non-shipping build without phantom infra. Specialist disagreement count: 0-1 (vs 4 in R4).

Convergence trajectory: R3's "atomic = 2-3 cycles; piecemeal = 4-6 cycles" forecast still on track if R5 follows atomic-by-cluster.

Forward-imposed contracts on downstream GDDs (R3 contracts hold; R4 adds 4 binding decisions pending Cluster A):
- All R1-R3 contracts stand.
- IF Option B (barrage carve-out): Wave Spawner GDD must add barrage-pattern type with cross-pattern simultaneous-onset carve-out rules + barrage frequency cap + barrage-specific Pillar 2 affordances.
- IF Option A (revise fantasy): Wave Spawner GDD must add explicit "no cross-pattern stagger constraint" language with numeric tolerance.
- max_concurrent_waves cap semantic must be re-declared in Wave Spawner GDD (hard ceiling / spawn-time gate / skip-vs-defer).
- Audio Controller GDD inherits item 4 fade-down timing clause (Cluster B output).

Editorial defects flagged again (4th consecutive review, non-blocking):
- Rule numbering still 1-11, 16, 17, 12-15 out of order.
- Edge Case numbering still EC-1 through 7, then 9, 10, 11, 12, then 8.
- Recommend renumbering pass during or after Cluster A (touch all cross-refs once before APPROVE).

Separate flags for producer (not GDD revision items):
- Cluster A's barrage-pattern carve-out (if Option B) is a Wave Spawner GDD scope expansion — ~1 week of additional design work attributable to DPC sequencing decision but consumed by Wave Spawner.
- docs/engine-reference/godot/VERSION.md still pinned to Godot 4.6; project targets UE5 mobile. `.claude/docs/technical-preferences.md` still unpopulated. `/setup-engine` still not run.

---

## Revision — 2026-06-05 — R5 Cluster A (identity reconciliation + Wave Spawner mechanical contract)
Type: Author revision (not a review)
Resolves: R4 Cluster A — Tier 1 B1 (PEAK identity collapse risk under floor rise) + B2 (cross-pattern stagger unprotected) + game-designer B-2 (simultaneous-pulls Player Fantasy gap)
Specialists: creative-director (identity layer), game-designer (mechanical layer)
User decision: Option B (barrage-pattern carve-out) ratified, CD recommendation accepted without veto

Identity layer landed (creative-director):
- Player Fantasy rewritten — staggered at floor is PEAK default state; barrage is bounded climax ("1–3 events per PEAK, never the steady state, the storm commits")
- New `## Phase Identity` section — per-phase identity table (OPENER=rhythm, MID=stack, PEAK=commit); PEAK identity = barrage admissibility, structurally independent of TELEGRAPH_WINDOW_FLOOR_S numeric value (closes B1)
- New `## Pillar Alignment Under Option B (Barrage Carve-Out)` section — walks all 5 pillars; Pillar 5 now affected via survivability constraint on barrage authoring (extends R3 attribution map without reversing any R3 decision); pillar attribution at line 6 updated
- Sting attribution (R3 CD-bound Pillar 3+4) HELD per R4 CD adjudication 3 (no reverse-reverse)

Mechanical layer landed (game-designer):
- Numeric envelope locked: M=3 max pulls per barrage, W≤0.3s simultaneity tolerance, N∈[1,3] barrage events per PEAK avg ~2, S two-tier cross-pattern stagger (≥TELEGRAPH_WINDOW_FLOOR_S non-barrage / ≥W barrage-involved, enforced spawn-decision-time)
- Wave Spawner Dependencies row item (8) variety floor split: ≥5 non-barrage + ≥2 barrage = ≥7 PEAK total
- Wave Spawner Dependencies row item (11) intra-pattern stagger carve-out clause added
- Wave Spawner Dependencies row item (12) NEW: cross-pattern stagger policy (closes B2)
- Contract Definitions: Pattern pool gets `is_barrage: bool` field + variety floor split; Pattern selection rule gets PEAK barrage distribution constraint (3) with structural-guarantee requirement (purely probabilistic non-compliant); Intra-pattern stagger constraint gets barrage carve-out clause; new Cross-pattern stagger policy bullet with deferral-not-skip enforcement semantic
- Bidirectional consistency check updated to enumerate the new contract terms
- New AC-PILLAR-2-BARRAGE (Visual/Feel, ADVISORY at story-Done, BLOCKING at Alpha, n≥30, ≥80% slip + zero mystery-death attribution); 3 resolution paths (A pass / B per-pattern fail / C systemic fail → Player Fantasy + Phase Identity re-bind under CD sign-off)
- Entity registry: 5 new constants — MAX_PULLS_PER_BARRAGE (provisional), BARRAGE_SIMULTANEITY_WINDOW_S (provisional), BARRAGE_EVENTS_PER_PEAK_MIN/MAX/TARGET_AVG (locked); last_updated 2026-06-04 → 2026-06-05

Cross-cluster coupling flagged (not escalated, documented in Block 1 + registry):
- Cluster B's max_concurrent_waves enforcement semantic (skip-and-drop vs defer vs hard ceiling) must NOT skip-drop pulls within a single barrage pattern. If Cluster B chooses skip-and-drop, AC-PILLAR-2-BARRAGE will fail and M must be revisited before Alpha.

Scope delta for producer:
- PEAK pool floor +2 patterns (5 → 7 total) attributable to Option B selection, consumed by Wave Spawner GDD.

No R3-locked decision reopened. No CD escalation required (game-designer concurred with all CD recommendations).

Cluster A status: COMPLETE. R5 Cluster B (9 mechanical correctness items: Seam 7 null-deref, MAX_CONCURRENT_WAVES_CAP=16, async-load race policy, max_concurrent_waves enforcement semantic, UWorldSubsystem resolution, F vs U UPROPERTY, ownership unification, Rule 17 rising-edge, old mix fade-DOWN timing) and Cluster C (AC honesty: 4 items + repairs) remain for out-of-session passes before R5 re-review.

---

## Revision — 2026-06-05 — R5 Clusters B + C (mechanical correctness + AC honesty)
Type: Author revision (not a review)
Resolves: R4 Tier 2 mechanical (9 items) + R4 Tier 3 verification (5 items including Cluster A's earlier flagged 0.6s literal repair)
Specialists: systems-designer (B mechanical), unreal-specialist (B UE-API), qa-lead (C AC honesty), audio-director (B item 5 + C item 3, cross-cluster)
User decisions: Accept all defaults (Path C UWorldSubsystem + ForceTickNow forward contract on RSM; defer item 6 phase-mix distinguishability AC; rename FDPCController → UDPCController)

Cluster B mechanical landed:
- **Item 1 (Seam 7 null-deref)**: line 700 in `docs/architecture/platform-seam-interfaces.md` — replaced false-branch `RSM->GetRunDurationS()` (null deref on the same TWeakObjectPtr that just failed IsValid) with self-call `GetRunDurationS()`. New AC-RSM-COLD-BOOT-FALLBACK added to Seam 7 ACs.
- **Item 2 (MAX_CONCURRENT_WAVES_CAP=16)**: registry constant added (status:active, locked). MAX_CONCURRENT_WAVES_FLOOR=1 also added to registry. Tuning Knobs non-tunable invariants table gains CAP row. **Critical fix**: F-3c formula and Rule 8 code block clamp the FLOAT *before* `FMath::FloorToInt32` (not the int after), because the ARM UB occurs inside `FloorToInt32(1e12f)` before the int clamp sees the result. Output Range paragraph + Boundary Checks table updated. New AC-08c with 3 sub-cases (large-finite, large-negative, NaN-via-upstream-guard).
- **Item 3 (async-load race)**: Bound-and-Abort chosen. `DPC_ASYNC_LOAD_WATCHDOG_S = 5.0s` watchdog at Initialize. New `IDPCAbortDelegate::RequestAbort(EDPCAbortReason::AsyncLoadTimeout)` injected interface. OQ-7-agnostic (works on all 4 RSM UE-type paths). New EC-13 + new AC-ASYNC-LOAD-RACE (observable from snapshot + mock delegate, no phantom hooks).
- **Item 4 (max_concurrent_waves enforcement semantic)**: pattern-start gate with atomic slot pre-commitment + deferred-not-skipped retry; bounded deferred queue (1 slot). **Preserves barrage atomicity per Cluster A coupling**: barrage required_slots=M=3 admitted atomically — no partial barrage admission possible. Wave Spawner Dependencies row gains item (13). New Contract Definitions bullet. New AC-CONCURRENT-WAVES-BARRAGE-ATOMICITY (forward AC on Wave Spawner GDD, listed here as DPC obligation record).
- **Item 5 (old mix layer fade-DOWN timing)** — see audio-director cross-cluster below.
- **Item 6 (UWorldSubsystem resolution)**: Path C chosen (ForceTickNow pull semantics). Rule 3 enforceability matrix rewritten. **Forward-imposed contract on RSM GDD**: RSM must expose `ForceTickNow()` primitive, idempotent within a single engine frame. This requires RSM GDD revision before DPC implementation can proceed.
- **Item 7 (F-prefix → UDPCController rename)**: primary declaration site renamed (line 198 Tick() prototype). 3 Seam 7 construction sites in `platform-seam-interfaces.md` converted from `MakeUnique<FDPCController>(MoveTemp(...), ...)` to `NewObject<UDPCController>(GetTransientPackage()) + AddToRoot() + Initialize(stub, curves)`.
- **Item 8 (ownership model)**: raw observer pointer `IRSMTimeStateProvider*` on DPC. TWeakObjectPtr rejected (interface is plain C++, no UObject base). Lifetime contract appended to Dependencies prose. Ownership comment added to Seam 7 test area in platform-seam-interfaces.md.
- **Item 9 (Rule 17 rising-edge fix)**: full rewrite distinguishing `is_active` rising-edge (always fires) from phase-diff handler (skipped on first tick). Two-layer initialization protocol authored. New AC-RISING-EDGE with subscription-during-RUNNING + subscription-during-pause sub-cases. Audio Controller dependency item 5 (mix-resume) extended to cover both pause-exit AND mid-run-subscribe rising-edge triggers.

Cluster C AC honesty landed (qa-lead):
- **Item 1 (AC-NAN-GUARD phantom hook)**: removed. Snapshot-observation-only methodology — `is_active == false` is load-bearing discriminator (especially for -Inf case where t_norm=0.0 matches inactive default).
- **Item 2 (4 check()-aborting ACs)**: Path B Shipping/Test build-configuration scoping chosen (no new compile flag). New build-config scoping convention appended to AC preamble. AC-THRESHOLD-ORDER, AC-MIN-MID-WIDTH, AC-OPENER-ZERO, AC-PEAK-BEYOND-ONE all rewritten to target Shipping/Test path (parallel runtime guard observable) with documented Dev/Debug path (check() aborts before guard).
- **Item 4 (0.6s literal replacement)**: 2 sites in AC-PILLAR-2-CONCURRENT → `TELEGRAPH_WINDOW_FLOOR_S` symbol.
- **Item 5 (sample size raise)**: AC-PILLAR-2-CONCURRENT n≥10 → n≥30 for symmetry with AC-PILLAR-2-BARRAGE; Wilson LCB at n=30 is ~0.62 (acceptable gate) vs ~0.49 at n=10 (unusable).

Audio-director cross-cluster items landed:
- **Cluster B Item 5 (old mix layer fade-DOWN timing)**: Audio Controller dependency item 4 rewritten with detailed timing envelope — old fade-DOWN starts at t=0 (sting onset) and completes ≤200ms; sting decay tail ~350-400ms; new fade-UP unity ≤500ms; ordering invariant old-DOWN-end < sting-decay-tail-end < new-UP-unity ≤500ms; equal-power crossfade shape recommended; mid-run-subscribe cold-entry edge case (no old layer to fade DOWN).
- **Cluster C Item 3 (blind A/B distinguishability AC)**: new AC-AUDIO-STING-DISTINGUISHABILITY authored in new "Pillar 3 + Pillar 4 audio sting distinguishability" subsection. n≥30 testers × ≥10 A/B/X trials = 300 trials. ≥75% threshold (lower than 80% gameplay survivability because pure perceptual task — standard psychoacoustic criterion). Path A pass / B per-sting fail (re-author) / C systemic fail (CD sign-off on architecture change). **Note**: AC-AUDIO-STING-DISTINGUISHABILITY addresses Audio Controller binding contract item 2 (two stings); item 6 (3-phase tempo/mix distinguishability) has a SEPARATE unguarded AC gap NOT closed in this pass per user decision to defer. R5 re-review may surface item 6 as new finding.

Entity registry: 2 new constants (MAX_CONCURRENT_WAVES_CAP + MAX_CONCURRENT_WAVES_FLOOR), both status:active.

Cross-cluster coupling preserved:
- Cluster A barrage atomicity → Cluster B Item 4 enforcement semantic: pattern-start gate + atomic pre-commitment guarantees barrage M=3 slots admitted together.
- Cluster A Rule 17 rising-edge → Cluster B Audio Controller item 5 extension: mid-run subscribe gets rising-edge signal, no longer pause-exit-only.

R5 status: Clusters A + B + C ALL COMPLETE. No R3-locked decision reopened. One forward contract on RSM GDD (ForceTickNow primitive) requires RSM author follow-up before DPC implementation can proceed. One deferred AC (Audio Controller item 6 phase-mix distinguishability) accepted as known gap for R5 re-review surface. Next: `/design-review design/gdd/difficulty-phase-controller.md` (re-review 5) in fresh session.

---

## Review — 2026-06-05 — Verdict: MAJOR REVISION NEEDED (re-review 5)
Scope signal: L
Specialists: game-designer, systems-designer, qa-lead, performance-analyst, audio-director, level-designer, unreal-specialist, creative-director (synthesis)
Blocking items: 11 | Recommended: ~22 | Specialist disagreements: 0 (game-designer + qa-lead independently corroborated AC-PILLAR-2-BARRAGE arithmetic blocker)
Prior verdict resolved: Partial — R5 Clusters A+B+C verified landed mechanically at cited lines (Player Fantasy rewrite, Phase Identity section, Pillar Alignment Under Option B, M=3/W≤0.3s/N∈[1,3] envelope, AC-PILLAR-2-BARRAGE, Seam 7 null-deref fix, MAX_CONCURRENT_WAVES_CAP=16 with float-side clamp, async-load watchdog + IDPCAbortDelegate, max_concurrent_waves pattern-start gate, Path C ForceTickNow, UDPCController rename, raw observer pointer ownership, Rule 17 two-layer protocol, audio fade-DOWN envelope, AC-NAN-GUARD phantom hook removed, 4 ACs build-config scoped, AC-AUDIO-STING-DISTINGUISHABILITY, 0.6s→TELEGRAPH_WINDOW_FLOOR_S symbol, AC-PILLAR-2-CONCURRENT n≥30). But 11 NEW second-order blockers emerged from cluster-boundary coupling errors — convergence trajectory R5=11 vs R4=16 numerically positive but composition shows new defects introduced by R5 rewrite itself (per CD synthesis).

Summary: Convergence trajectory STALLED with regression hotspots. R1=12 → R2=16 → R3=21 → R4=16 → R5=11. Headline number misleading — R5's 11 blockers are NOT 11 lighter, they are 11 DIFFERENT blockers, several created by the R5 rewrite itself. R5 atomic-by-cluster approach partially succeeded (intra-cluster items landed correctly) but failed to catch second-order coupling between clusters. The GDD's invariants live at cluster boundaries; no single specialist or cluster review owned them.

R5 blockers collapse into 4 root causes per CD synthesis:

**Root Cause A — PEAK Phase Identity vs Stagger Floor Geometry (2 blockers, CD-adjudicated)**:
- [game-designer] Phase Identity claim "three concurrent in-flight telegraphs" (line 26) and Player Fantasy "triage of three readable hazards in flight" (line 14) mechanically impossible. At PEAK, telegraph_window_s=0.6s=TELEGRAPH_WINDOW_FLOOR_S → 0s telegraph overlap. MID actually delivers more overlap (0.12s) than PEAK (0.0s). R5 Cluster A rewrite introduced a NEW false identity claim while "resolving" R4 B1's old one.
- [level-designer] First-onset anchor for barrages (line 571) produces 4 onsets in 0.3s window — exceeds M=3 carve-out ceiling. Barrage at t=0/0.15/0.3 + non-barrage admissible at t=0.3 = 4 onsets in [0, 0.3s]. CD-adjudication: anchor on LAST barrage onset vs suppress concurrent admission.

**Root Cause B — Watchdog and Deferred-Queue State Machine Hazards (2 blockers, systems-designer)**:
- [systems-designer] Watchdog races COUNTDOWN. DPC_ASYNC_LOAD_WATCHDOG_S=5.0s=COUNTDOWN_S=5.0s started at Initialize() = zero margin for load spillover. Spurious abort risk.
- [systems-designer] Drop rule (line 574) can zero barrage count: non-barrage deferred → shuffle-bag's reserved barrage arrives → dropped + replaced with non-barrage → PEAK ends N=0 barrages. Breaks Phase Identity + AC-PILLAR-2-BARRAGE precondition.

**Root Cause C — UE5 Tick/GC Contract Underspecification (4 blockers, unreal-specialist + qa-lead)**:
- [unreal-specialist] DPC tick mechanism completely unspecified (line 217). void UDPCController::Tick() has no DeltaTime, no declared base class. Incompatible with all UE5 tick paths. OQ-7 only scopes RSM's type, not DPC's.
- [unreal-specialist] AddToRoot without RemoveFromRoot in Seam 7 test pattern = dangling-pointer crash on teardown.
- [unreal-specialist] ForceTickNow DeltaTime source + non-callback invariant unspecified in Path C forward contract.
- [qa-lead] AC-RISING-EDGE references bIsActiveRisingEdgeObserved field on FConsumerTestStub that does NOT exist in Seam 8 (grep verified). Same paper-only pattern as R4's IRSMTestStub miss.

**Root Cause D — Statistical & Forward-Binding Hygiene (3 blockers, qa-lead + audio-director + game-designer)**:
- [qa-lead] AC-AUDIO-STING-DISTINGUISHABILITY treats 30×10=300 trials as independent binomial; within-tester learning makes effective N≈30. Cited "p<0.01" invalid.
- [game-designer + qa-lead, independently corroborated] AC-PILLAR-2-BARRAGE arithmetic incoherent (line 801). ≥3 first-encounter exposures impossible at ≥2 variety floor (definitional). "≥6 first-encounter events" multiplies first-encounter × patterns.
- [audio-director] Audio Controller item 6 (3-phase tempo/mix distinguishability) deferred at line 810 with non-binding language. Without forward-binding contract, Audio Controller GDD author has no obligation.

CD synthesis: NEEDS REVISION. APPROVED-with-deferred not defensible — Root Cause A is identity-level (PEAK Phase Identity claims mechanical impossibilities); Root Cause B includes Pillar 2 invariant break (drop rule zeroes barrage count). Deferring identity claims and pillar invariants is a contradiction in terms. R6 path: targeted root-cause rewrite (NOT another atomic pass). Two CD-adjudication points: PEAK identity honest rewrite + barrage anchor choice (last vs suppress).

Resolved in-session: ALL 11 BLOCKING + selected recommended items. User chose: revise now in this session via 4 design-decision widget answers:
- A.1: Honest rewrite (CD-recommended) — edge-to-edge sequential cadence + barrage clustered-window exception. Player Fantasy line 14 rewritten; Phase Identity table PEAK row rewritten; "Why PEAK is distinct" section rewritten with two structural differences (cadence-character shift + barrage admissibility); structural independence paragraph updated; Pillar 2 alignment paragraph updated; transition table PEAK row updated.
- A.2: Last-onset anchor — cross-pattern stagger anchor at line 571 changed from first to LAST barrage onset. M=3 envelope preserved structurally. Wave Spawner contract addition acknowledged (small).
- C.1: UObject + FTickableGameObject — Rule 3 enforceability matrix expanded with DPC's own UE type binding (not deferred to OQ-7). Rule 14 Tick() prototype updated to void UDPCController::Tick(float DeltaTime). 5 FTickableGameObject overrides specified (IsTickable/GetStatId/IsTickableInEditor/IsTickableWhenPaused). Production outer flagged for OQ-7 ADR.
- D.2: Variety floor raise — barrage floor raised from ≥2 to ≥3 patterns. PEAK total ≥7→≥8. Wave Spawner Dependencies row item (8) updated. Pool composition addition under Option B updated. Bidirectional consistency check updated. AC-PILLAR-2-BARRAGE preconditions + GIVEN clause rewritten coherently (3 first-encounter events per tester × M=3 onsets = ≥270 aggregate, Wilson LCB ~0.75 acceptable).

Non-decision items resolved by author/CD-recommendation without user input:
- B.1 Watchdog: gated on RUNNING entry (not Initialize). 2s warn tier added (DPC_ASYNC_LOAD_WATCHDOG_WARN_S=2.0s). Detection mechanism via Rule 4 tick state observation.
- B.2 Drop rule: 4-case priority preservation rule (is_barrage=true beats non-barrage at queue contention; both-barrage case re-seeds shuffle-bag; both-non-barrage preserves prior R5 behavior).
- C.2 Seam 7 teardown: both DPC test patterns + AC-14b test pattern now end with explicit DPC->RemoveFromRoot() before StubOwned destructs.
- C.3 ForceTickNow contract refined: signature `void ForceTickNow()`; DeltaTime via FApp::GetDeltaTime() with RSM-internal bHasTickedThisFrame; idempotent within frame; non-callback invariant; game-thread only.
- C.4 Seam 8 FConsumerTestStub: bIsActiveRisingEdgeObserved field + RisingEdgeFireCount counter added with full two-layer protocol implementation distinguishing is_active rising-edge handler (always fires on first-active-tick) from phase-diff handler (skipped on first tick). Reset() + public accessors updated.
- D.1 AC-AUDIO-STING: per-tester pass/fail primary metric (each tester ≥75% on 10 trials → count testers passing → binomial at n=30, k=23 → p ≈ 0.002 against null 50%); aggregate retained as secondary diagnostic. Statistical interpretation rewritten.
- D.3 Item 6 forward-binding: explicit paragraph added requiring Audio Controller GDD to author 3-phase distinguishability AC (3AFC or 3 pairwise A/B/X). BLOCKING at Audio Controller GDD story-Done + Alpha milestone.

Recommended items addressed:
- Pre-init snapshot raw constants (line 303): INACTIVE_SNAPSHOT_WSI_DEFAULT=4.0f, INACTIVE_SNAPSHOT_TW_DEFAULT=0.9f literals (decoupled from floor multipliers).
- Struct layout breakdown text fix (line 841): field offsets walked correctly; double at offset 16 naturally aligned (no pre-pad needed); total 32B unchanged.
- AC-NAN-GUARD RUNNING precondition (line 753): explicit `SetCurrentState(RUNNING)` + `SetIsPaused(false)` added to GIVEN.
- FStreamableHandle BindCompleteDelegate API form noted.

Recommended items NOT addressed in-session (carryover for R6 author discretion or producer):
- Memory budget AC (sizeof static-assert)
- Cap=1 degenerate config startup validator (PEAK curve value < MAX_PULLS_PER_BARRAGE)
- AC-RISING-EDGE exact-tick subscription sub-case
- AC-AUDIO-STING real-hardware (Bluetooth/earbuds) secondary spot-check
- CI build config verification for check()-guard ACs
- AC-22b multi-line macro edge case
- Audio Controller item 4 transient masking note
- 75% A/B/X psychoacoustic citation
- Equal-power crossfade required vs recommended
- OPENER variety floor ≥3 → ≥4
- Fired-onset vs scheduled-onset clarification in cross-pattern stagger
- Pattern structural diversity authoring guideline
- Post-COMPLETE barrage resolution window bound
- Path B floor rise non-barrage pattern viability assessment
- Editorial defects (rule numbering 1-11,16,17,12-15; EC 1-7,9-12,8) — 5th consecutive review

Convergence trajectory: STALLED per CD synthesis. R5 atomic-by-cluster did not catch cluster-boundary coupling. R6 protocol must add cluster-boundary integrity pass. With in-session R5 re-review 5 revisions complete and 4 root causes addressed at the design-decision level, R6 should drop to ≤5 blockers per CD forecast IF the cluster-boundary integrity protocol is added.

Forward-imposed contracts on downstream GDDs (R1-R5 contracts hold; R5 re-review 5 adds/clarifies 6):
- All R1-R5 contracts stand.
- Wave Spawner GDD: PEAK barrage variety floor raised ≥2 → ≥3 (1 additional barrage pattern); PEAK total raised ≥7 → ≥8. Last-onset anchor for barrages in cross-pattern stagger. Drop rule priority preservation (4 cases).
- Audio Controller GDD: per-tester binomial methodology on AC-AUDIO-STING-DISTINGUISHABILITY. Forward-binding REQUIRED for item 6 3-phase distinguishability AC (3AFC or 3 pairwise A/B/X; BLOCKING at Audio Controller GDD story-Done).
- RSM GDD: ForceTickNow contract refined — signature, DeltaTime source (FApp::GetDeltaTime + bHasTickedThisFrame), idempotent within frame, non-callback invariant, game-thread only.

Test infrastructure (verified updated in this session):
- FConsumerTestStub (Seam 8) extended with bIsActiveRisingEdgeObserved + RisingEdgeFireCount + 2 public accessors + Reset() updated. AC-RISING-EDGE row added to "Unblocked" table.
- Seam 7 test patterns (3 sites) updated with explicit DPC->RemoveFromRoot() teardown. Tick() calls updated to Tick(0.0f) per FTickableGameObject signature.

Separate flags for producer (not GDD revision items):
- RSM GDD revision is the NEXT BLOCKING ITEM after DPC R6 approves: must add ForceTickNow() primitive with refined R5 re-review 5 contract before DPC implementation can proceed.
- Wave Spawner GDD scope delta: PEAK pool +3 patterns (5→8 total, +1 over the R5 Cluster A +2 delta) attributable to Option B + R5 re-review 5 barrage floor raise.
- `.claude/docs/technical-preferences.md` still unpopulated; `/setup-engine` still not run.
- Editorial defects 5th consecutive review — recommend renumbering pass before R6 (touch all cross-refs once).

Specialist disagreements: 0 (game-designer + qa-lead independently corroborated AC-PILLAR-2-BARRAGE arithmetic blocker, treated as single root cause).

CD adjudications this review: 4 (2 binding user-decision recommendations + 2 author-default acceptances):
1. Phase Identity rewrite — CD recommended honest rewrite; user ratified.
2. Barrage cross-pattern anchor — CD identified two candidate fixes; user chose last-onset.
3. DPC tick mechanism — CD synthesis flagged GDD-level decision; user chose UObject + FTickableGameObject over UTickableWorldSubsystem or widening OQ-7 ADR.
4. AC-PILLAR-2-BARRAGE fix — CD acknowledged both candidate fixes acceptable; user chose variety floor raise.

---

## Review — 2026-06-05 — Verdict: MAJOR REVISION NEEDED (re-review 6)
Scope signal: L
Specialists: game-designer, systems-designer, qa-lead, performance-analyst, audio-director, level-designer, unreal-specialist, creative-director (synthesis)
Blocking items: 18 | Recommended: ~20 | Specialist disagreements: 0 (7-specialist convergence on 5 root causes)
Prior verdict resolved: Partial — R5 Clusters A+B+C verified landed mechanically. But R5's "in-session 0 blockers" claim was premature; R6 surfaces 18 BLOCKING that R5 atomic-by-cluster did not catch because they sit at cluster-boundary or contract-boundary seams.

Summary: Convergence trajectory STRUCTURALLY BROKEN (CD synthesis), not stalled. R1=12 → R2=16 → R3=21 → R4=16 → R5=11 → R6=18+. The pattern: each round's "resolution" — particularly R5's four user decisions — introduces new contracts the next round is first to verify. R5 D.2 added N≥1 guarantee without N≤3 enforcement. R5 C.1 chose UObject+FTickableGameObject without verifying AddPrerequisite exists on the chosen type (it doesn't — unreal-specialist B-1). R5 A.2 anchored barrages to last-onset without verifying thermal-fallback cap interaction (it deadlocks — convergent BLOCKING from 3 specialists).

R6 blockers cluster into 5 root causes:
- **Root Cause A (Thermal fallback architectural break, CRITICAL)**: cap=2 thermal fallback + MAX_PULLS_PER_BARRAGE=3 = infinite deferred-queue loop, N≥1 guarantee silently breaks, Phase Identity collapses on every thermal-fallback device. Mathematically guaranteed, not playtest-detected. Convergent BLOCKING from game-designer Q3 + systems-designer B1 + performance-analyst CRITICAL.
- **Root Cause B (Barrage architecture overcommit)**: N≤3 ceiling has no enforcement; PEAK Path B structural-independence claim mathematically false (all 3 phases collapse to edge-to-edge sequential at floor rise); shuffle-bag depletion under priority-preservation replacement draws; drop-rule replacement timing unspecified; spatial separation in barrage authoring is phantom binding (no numeric floor); shuffle-bag re-seeding invariants undefined.
- **Root Cause C (RSM forward contract expansion 1→5)**: Rule 3 enforceability matrix references unavailable UE5 mechanism (FTickableGameObject has no FTickFunction); canonical Tick() omits ForceTickNow; RSM bHasTickedThisFrame must be set on regular Tick path AND ForceTickNow path; RSM ABORTED entry from external RequestAbort is new contract; watchdog cancellation logic + pause-semantics unspecified.
- **Root Cause D (AC statistical foundations incoherent)**: AC-AUDIO-STING null hypothesis arithmetically wrong (p_null=0.055 not 0.5); AC-PILLAR-2-BARRAGE per-onset independence violated by CANCEL_TIMER_MS=180ms physical impossibility (effective N=90 not 270, Wilson LCB 0.71 not 0.75); AC-08c sub-case (iii) cites wrong rule + missing curve-output-NaN path; AC-RISING-EDGE contrast sub-case tests non-normative behavior; ITU-R BS.1116-3 citation is wrong family.
- **Root Cause E (Wave Spawner contract still ambiguous)**: drop-rule replacement timing; shuffle-bag re-seed invariants; shadow-list vs bag cursor.

Plus FLAG-1 (CD-adjudicated BLOCKING): post-COMPLETE in-flight hit hazard — PEAK barrage at t=59.5s with last onset t=59.8s lands hits during RESOLVING. RSM has no COMPLETE→DEAD arc.

CD adjudications (3 binding):
1. Root Cause A — Option (b): thermal fallback must lift cap floor to MAX_PULLS_PER_BARRAGE whenever barrages are in the bag.
2. Root Cause C — Rule 3 collapses to ForceTickNow-only on ALL OQ-7 paths (AddPrerequisite path is factually unavailable, not just non-preferred).
3. FLAG-1 — Option B: Collision GDD contract stub — post-COMPLETE hits scored but cannot flip outcome. Option A (cancel patterns at COMPLETE) rejected because it silently changes Phase Identity in last 0.6s of every run.

Resolved in-session: ALL 18 BLOCKING + selected REVISION items. User chose: revise now in this session via 4 design-decision widget answers:
- Q1: A — Full barrage enforcement (preserve R5 Option B fully bound). Added explicit N≤3 counter mechanism + shadow-list replacement + MIN_BARRAGE_LANE_SEPARATION=2 provisional constant + Path B reauthoring instruction.
- Q2: A — Accept CD: thermal fallback lifts cap floor to M. Tuning Knobs entry amended + Rule 14 cap-vs-barrage startup check + new AC-CAP-BARRAGE-COMPATIBILITY + permitted thermal-fallback paths enumerated.
- Q3: A — Explicit TelegraphWindowCurve reauthoring under Path B. Phase Identity "structural independence" claim corrected with explicit reauthoring instruction at Path B floor rise.
- Q4: OPENER ≥3→≥4 AND barrage ≥3→≥4 (PEAK total ≥8→≥9). Variety floors raised in Wave Spawner Contract Definitions + Dependencies row + bidirectional consistency check + AC-PILLAR-2-BARRAGE preconditions.

Non-decision items resolved by author/CD-recommendation without user input:
- Pillar 5 vs Pillar 2 naming contradiction at Pillar Alignment Under Option B: barrage survivability reattributed Pillar 5 → Pillar 2 (R3 reversal logic applies; readability is Pillar 2's domain). Line 6 pillar attribution updated.
- Rule 3 enforceability matrix collapsed to ForceTickNow pull semantics on all OQ-7 paths (R6 unreal-specialist B-1 fix).
- Canonical Tick() code block: ForceTickNow call added as first statement after thread check.
- RSM forward contract expanded 1→5 items: ForceTickNow primitive; bHasTickedThisFrame on regular Tick path; non-callback invariant + state-transition 1-frame latency note; external RequestAbort entry; OnEndFrame handle lifetime.
- Watchdog: specified tick-driven mechanism (Rule 4 pause-freeze + background-freeze + bInitialized cancellation gate). New AC sub-cases R6-a/R6-b/R6-c.
- Drop rule replacement timing: current cycle if available_slots ≥ 1, else deferred-or-dropped per priority preservation. Shadow-list draw, not shuffle-bag cursor.
- AC-AUDIO-STING null hypothesis: corrected to p_null ≈ 0.055; ITU-R BS.1116-3 citation replaced with Burstein 1988 + Boley & Lester AES 127.
- AC-PILLAR-2-BARRAGE: observation unit changed from per-onset to per-barrage-event (N=90, Wilson LCB ≈ 0.71); CANCEL_TIMER_MS=180ms physical impossibility documented.
- AC-08c: sub-case (iii) citation corrected to Rule 14 input-side; new sub-case (iv) for Rule 8 output-side curve-NaN coverage.
- AC-RISING-EDGE: per-tick rising-edge recovery clause added to Rule 16.
- Equal-power crossfade label dropped; per-curve shape requirements bound (smooth perceptual taper, no abrupt level changes).
- Post-COMPLETE hit hazard: Collision GDD forward contract added (CD Option B — hits scored but cannot flip outcome).
- AC-CAP-BARRAGE-COMPATIBILITY: new BLOCKING AC validating Rule 14 cap-vs-barrage check.
- OQ-DPC-6 added: Pull-Wave Behavior GDD derives MIN_BARRAGE_LANE_SEPARATION final value (provisional 2 holds until).
- AC-22a: warm-cache budget tightened (60µs → fail tier, was warning); cold-cache reframed as cold-instruction-cache (R6 performance-analyst HIGH); documented measurement procedure added; static_assert on FDPCFrameState size added.
- RUN_DURATION_S tunable range tightened to [30, 300]s (was [10, 300]s) per game-designer Q5 — forwards binding to RSM GDD revision.

Recommended items NOT addressed in-session (carryover for R7 author discretion):
- Within-PEAK skill invisibility (game-designer Q8 ADVISORY — OQ-DPC-1 noted as reopen-trigger)
- Differentiator 1 hedging language (game-designer Q1 ADVISORY)
- AC-22b SCOPE_CYCLE_COUNTER position not grep-verifiable (qa-lead REC-2 + unreal-specialist R-2)
- AC-PILLAR-2-CONCURRENT hard-zero mystery-death criterion brittleness
- Audio MAJOR items (3AFC threshold ≥67%, 3-pairwise ABX scope, balancing target, 10s gap, headphone mW spec)
- Unreal RECs (inheritance order doc, FStreamableHandle ownership, pre-init publication wording with IsTickable gating)
- Pre-init constant drift validator
- Editorial: rule numbering (6th consecutive review — recommend dedicated renumbering pass post-R7)

Convergence trajectory: CD synthesis flagged STRUCTURALLY BROKEN. R6 in-session revision addressed all 18 BLOCKING at design-decision level; R7 protocol must add cluster-boundary integrity verification + RSM forward contract landing as load-bearing prerequisite. With R6 in-session revisions complete and 5 root causes addressed at the binding level, R7 should drop to ≤5 blockers IF the cluster-boundary integrity protocol catches second-order effects of R6's bindings.

Forward-imposed contracts on downstream GDDs (R1-R5 contracts hold; R6 adds/clarifies 7):
- All R1-R5 contracts stand.
- **RSM GDD revision (BLOCKING for DPC implementation)**: 5-item forward contract — (1) ForceTickNow primitive; (2) bHasTickedThisFrame on regular Tick path; (3) non-callback invariant + 1-frame state-transition latency; (4) external RequestAbort entry to ABORTED; (5) OnEndFrame handle lifetime. Plus RUN_DURATION_S range tightening [10,300] → [30,300].
- Wave Spawner GDD: peak_barrage_count counter for N≤3 enforcement; shadow-list replacement mechanism for drop rule and N≤3 ceiling; MIN_BARRAGE_LANE_SEPARATION cook-time enforcement; PEAK variety floor ≥4 barrage + ≥5 non-barrage = ≥9 total; OPENER ≥4. IWaveSpawnerPoolMetadataProvider::HasBarrageInPeakPool() interface for Rule 14 cap-vs-barrage check.
- Collision GDD: post-COMPLETE in-flight hit policy — hits scored but cannot flip terminal outcome (CD-bound Option B).
- Audio Controller GDD: per-curve shape requirements for crossfade (smooth perceptual taper, replaces invalid equal-power label); ITU citation replaced with Burstein 1988 / Boley & Lester AES 127.
- Pull-Wave Behavior GDD: derive MIN_BARRAGE_LANE_SEPARATION final value from slip distance per TELEGRAPH_WINDOW_FLOOR_S (currently provisional 2 lanes; tracked under OQ-DPC-6).

Test infrastructure (R6 updates):
- New AC-CAP-BARRAGE-COMPATIBILITY requires mock IWaveSpawnerPoolMetadataProvider — light interface (1 method), trivially implementable.
- AC-08c sub-case (iv) requires NaN-returning curve stub — trivial.
- AC-ASYNC-LOAD-RACE gains sub-cases R6-a/b/c (cancellation, pause-freeze, background-freeze) — require ApplicationWillDeactivateDelegate / ApplicationHasReactivatedDelegate simulation.

Separate flags for producer (not GDD revision items):
- RSM GDD revision is the NEXT BLOCKING ITEM after DPC R7 approves: must add 5-item ForceTickNow contract before DPC implementation can proceed.
- Wave Spawner GDD scope delta: PEAK pool +1 barrage (was 8, now 9 total); OPENER pool +1 (was 3, now 4); R6 marginal scope = +2 patterns total.
- Pull-Wave Behavior GDD: OQ-DPC-6 dependency surfaced — slip distance per TELEGRAPH_WINDOW_FLOOR_S must be derived.
- `.claude/docs/technical-preferences.md` still unpopulated; `/setup-engine` still not run.
- Editorial defects 6th consecutive review — recommend dedicated renumbering pass before R7 to clean rule numbering (1-11, 16, 17, 12-15) and EC numbering (1-7, 9-12, 8). Touch all cross-refs once.

Specialist disagreements: 0 (7-specialist convergence on 5 root causes; framing differences only).

CD adjudications this review: 3 binding (Root Cause A thermal lift; Root Cause C Rule 3 ForceTickNow-only on all paths; FLAG-1 Collision GDD post-COMPLETE Option B).

R6 path: fresh session re-review with 7 specialists + CD. Target: ≤5 blockers (CD forecast), no identity-level findings, no Pillar 2 invariant breaks.

---

## Review — 2026-06-06 — Verdict: NEEDS REVISION → Approved (in-session R7 revision applied)
Scope signal: XL
Specialists: game-designer, systems-designer, qa-lead, performance-analyst, audio-director, level-designer, unreal-specialist, creative-director (synthesis)
Blocking items: 10 | Recommended: ~17 | Specialist disagreements: 0 (first time in 7 rounds; cross-cluster overlap between systems-designer B-2 and unreal-specialist B-1 on watchdog region is mutually reinforcing, not a disagreement)
Prior verdict resolved: Yes — all R6 in-session fixes verified landed mechanically. R7 surfaces 10 second-order BLOCKERS that the R6 atomic revision did not catch.

Summary: **The R6 "structurally broken" convergence pattern is itself BROKEN.** R7 evidence: zero identity-level findings, zero R3-reversal challenges, zero specialist disagreements, audio domain fully clean (first time), every BLOCKING is a bounded 1-line or seam-doc authoring fix. Root-cause count = 6 (not 10). The document is in authoring cleanup territory, not design rework territory. Convergence trajectory R1=12 → R2=16 → R3=21 → R4=16 → R5=11 → R6=18 → R7=10. CD synthesis: blocker count is the wrong metric; root-cause count trending toward 0 with bounded fixes.

R7 blockers cluster into 6 root causes per CD synthesis:

**Root Cause A — Paper-only interface infrastructure (4 blockers)**:
- [qa-lead] IDPCAbortDelegate referenced at lines 82, 238, 576, 835, 838, 845, 857, 863, 868 but undefined in platform-seam-interfaces.md (grep → empty).
- [qa-lead] IWaveSpawnerPoolMetadataProvider referenced at lines 238, 856 but undefined anywhere in docs/.
- [qa-lead] AC-08c sub-case (iv) NaN curve mechanism unspecified — no ICurveProvider seam, UE5 FRichCurve NaN behavior not cited.
- [systems-designer B-1] Cap-vs-barrage check has uncontracted async dependency on Wave Spawner pool readiness — symptom of same root cause (the IWaveSpawnerPoolMetadataProvider contract that would specify pool-readiness semantics doesn't exist).

**Pattern note**: this is the same paper-only-infrastructure failure class as R3 (IRSMTestStub paper-only) and R5 (FConsumerTestStub.bIsActiveRisingEdgeObserved field paper-only) — 3 historical recurrences. CD adjudication: **process gap, not content gap. Procedural fix: amend design-review skill with Phase 2b seam-doc grep step.**

**Root Cause B — Path B reauthoring inverts Phase Identity (1 blocker)**:
- [game-designer] Line 40 binding rule mandates MID key bump to 1.02s under Path B floor=0.9s but omits parallel OPENER bump. Authored OPENER at line 680 stays at 0.9s. Under Path B: OPENER=0.9s, MID=1.02s, PEAK=0.9s. MID becomes most generous phase, contradicting Phase Identity line 24 "OPENER = most generous telegraph; player calibrates against known-good window." Fix: add OPENER-key lower-bound clause.

**Root Cause C — IsTickable / pre-init / watchdog contradiction (2 blockers, cross-cluster)**:
- [unreal-specialist] Internal GDD contradiction: line 74 says IsTickable() returns false during async load → lines 349/556 "every DPC tick during load window publishes" is unreachable → EC-13 watchdog_elapsed_s += DeltaTime never advances → Bound-and-Abort watchdog (R5 fix) silently defeated.
- [systems-designer B-2] Double-abort race: cap-vs-barrage failure issues RequestAbort but bInitialized stays false (set only on validation success per line 563) → watchdog cancellation gate never fires → second RequestAbort at t=5s. RSM forward contract doesn't address ABORTED idempotency.

**Root Cause D — Shadow-list scope is local-only (1 blocker)**:
- [systems-designer B-3] Shadow list's no-consecutive-same check at line 624 is local to shadow-list state. Pattern X drawn from main bag; N≤3 ceiling fires; shadow list draws (local last is different/null); shadow can draw X → merged stream reads X,X → violates line 617 constraint (1).

**Root Cause E — Spatial variety collapse (1 blocker)**:
- [level-designer] MIN_BARRAGE_LANE_SEPARATION=2 × M=3 in 5-lane layout: ONLY {0,2,4} satisfies pairwise ≥2 separation. All ≥4 barrage variety-floor patterns use identical {0,2,4} lanes — spatial variety = 1 against K=4 pattern-count variety. At 4-lane layouts NO 3-onset configuration is valid.

**Root Cause F — Static_assert SHOULD not MUST (1 blocker)**:
- [performance-analyst] Line 953: "AC-22b's grep verification SHOULD be extended to verify this static_assert" — SHOULD on a CI enforcement hook. AC-22b's 4 rg patterns don't include a static_assert verification. Silent layout drift will pass CI.

CD ADJUDICATIONS (3 binding):
1. **Root Cause C — Path A**: IsTickable() returns true unconditionally; Tick body gates on bInitialized for active publication but always advances watchdog_elapsed_s and always publishes the pre-init inactive snapshot. Combined with bInitialized=true on validation completion (success OR failure) resolves systems-designer B-2's double-abort.
2. **Root Cause E — Defer to Pull-Wave with MIN_BARRAGE_LANE_SEPARATION=1 provisional**, AND mandate AC-PILLAR-2-BARRAGE-SPATIAL-K on Pull-Wave GDD enforcing cross-run spatial novelty where patterns are actually authored. M=3 is Player Fantasy — revising to M=2 is pillar reopening, rejected.
3. **Root Cause A — Process gap, not content gap**: 3 rounds finding the same paper-only-infrastructure pattern is dispositive evidence the design-review skill is missing a seam-doc grep step. Procedural fix: amend .claude/skills/design-review/SKILL.md with Phase 2b grep step. **Highest-leverage decision in R7** — catches the pattern on every future GDD.

Resolved in-session: ALL 10 BLOCKING + selected RECOMMENDED items. User chose Option A (revise now in-session, mark Approved, move on; CD-recommended). 3 design-decision widget answers:
- Q1=A: OPENER ≥ MID + intra_phase_overlap_s (maintains 0.12s phase delta). At Path B floor=0.9s: OPENER key becomes 1.14s. Line 40 binding rule updated with OPENER-key lower-bound clause and authored example.
- Q2=A: DPC-side bInitialized=true on validation completion (success OR failure). Rule 14 line 238 updated with explicit semantic shift ("bInitialized means validation concluded, NOT succeeded"). Watchdog cancellation gate at line 563 now fires correctly on failure path.
- Q3=A: Amend design-review skill in this session with Phase 2b seam-doc grep step.

Non-decision items resolved per CD synthesis:
- Path A IsTickable resolution applied: Rule 3 line 74 + Tick() canonical block + EC-13 watchdog accumulation moved inside !bInitialized pre-init path.
- Shadow-list no-consecutive-same merged-stream scope: line 624 updated with explicit "compare against most recently admitted pattern from EITHER source" rule.
- MIN_BARRAGE_LANE_SEPARATION: registry updated provisional 2→1; line 626 rewritten with R7 history + Pull-Wave forward contract + AC-PILLAR-2-BARRAGE-SPATIAL-K.
- static_assert SHOULD→MUST at line 953; AC-22b 5th rg pattern added at line 962+.
- 3 new seams authored in platform-seam-interfaces.md: Seam 9 IDPCAbortDelegate + EDPCAbortReason + FDPCAbortTestStub; Seam 10 IWaveSpawnerPoolMetadataProvider + FWaveSpawnerPoolMetadataTestStub; Seam 11 ICurveProvider + FCurveProviderTestStub. ADR Dependencies table + GDD ACs Unblocked table updated.
- AC-CAP-BARRAGE-COMPATIBILITY: mock injection timing specified (post-OnCurvesLoaded, not bare Initialize); test stubs referenced via Seam 9 + 10.
- AC-08c sub-case (iv): ICurveProvider Seam 11 reference added; test infrastructure list updated.
- Collision GDD added to bidirectional consistency check (level-designer REC-3) with post-RUNNING in-flight hit policy inheritance.
- Counter reset clarifier added at line 619 (pause/resume mid-PEAK does not reset; only OPENER/MID→PEAK transition fires reset) per systems-designer R-1.
- Counter admission-vs-perception note added per game-designer REC-1.
- Watchdog DeltaTime clamped via FMath::Min(DeltaTime, MAX_WATCHDOG_DELTA_CLAMP_S=0.1s) per performance-analyst R-3.

Recommended items NOT addressed in-session (carryover for author discretion or implementation-time):
- Audio R-1 "after the first" consumer-scope binding clarification at Rule 16
- Audio R-2 Burstein/Boley citation softening
- Audio R-3 volume calibration topology binding
- Performance R-1 60µs fail-tier measurement window qualifier
- Performance R-2 JIT premise correction (UE5 iOS is AOT)
- Unreal R-1 OQ-7 ADR dependency-declaration binding
- Unreal R-2 test construction pattern unification (TStrongObjectPtr vs AddToRoot)
- Unreal R-4 STAT_DPCTick scoping note (RSM ForceTickNow time inclusion)
- qa-lead REC-1 23/30 population threshold derivation
- qa-lead REC-2 CI Shipping-config mechanism verification
- level-designer R-1 shadow-list authoring guidance
- level-designer R-2 cross-pattern stagger scheduled-vs-fired clarification
- Editorial: rule numbering (1-11,16,17,12-15; EC 1-7,9-12,8) — 7th consecutive review, user has decided cross-ref cost > cleanup cost; deferred indefinitely.

Convergence trajectory: CD synthesis flagged **PATTERN BROKEN** (R6's "structurally broken" verdict invalidated by R7 evidence). The document stopped generating new categories of failure. Every R7 BLOCKING is either (a) a previously identified failure class (paper-only seams — 3rd recurrence) or (b) a clean second-order consequence of an R6 binding resolvable with the original binding intact. **CD strategic recommendation: SKIP R8.** The 7-round trajectory (12→16→21→16→11→18→10) is past the inflection point. R3 was the peak; R4 onward is descent. The next genuine source of design feedback is implementation — specifically the Telegraph prototype with actual OPENER/MID/PEAK timings in player hands. Pull-Wave and Telegraph need DPC as a stable upstream dependency; every additional revision delays them, and Root Cause E will get resolved in Pull-Wave anyway. **Opportunity cost of an R8 is the prototype that would actually validate the design.**

Forward-imposed contracts on downstream GDDs (R1–R6 contracts hold; R7 adds/refines):
- All R1–R6 contracts stand (Wave Spawner spawn drain + variety floors + barrage architecture + shadow-list; Audio Controller 7-item binding + AC-AUDIO-STING; Pull-Wave spawn-time velocity; Telegraph floor non-imposition; Collision post-RUNNING hit policy).
- **Pull-Wave Behavior GDD (R7 NEW)**: must derive MIN_BARRAGE_LANE_SEPARATION final value AND specify production lane count such that ≥4 distinct M=3 spatial configurations remain authorable; AC-PILLAR-2-BARRAGE-SPATIAL-K is BLOCKING at Pull-Wave story-Done.
- **RSM GDD revision**: 5-item ForceTickNow contract (R6 carryover) + RUN_DURATION_S range tightening [10,300]→[30,300] (R6 carryover). R7 OPTIONAL: ABORTED idempotency clause as defense-in-depth (DPC-side bInitialized=true-on-failure already closes the double-abort race at the source per Q2=A).
- **Collision GDD**: post-RUNNING in-flight hit policy added to bidirectional consistency check (level-designer REC-3) — hits scored but cannot flip terminal outcome.

Test infrastructure (R7 — 3 new seams landed in platform-seam-interfaces.md):
- **Seam 9: IDPCAbortDelegate** + EDPCAbortReason enum + FDPCAbortTestStub (GetLastAbortReason, GetAbortCallCount, GetAllAbortReasons, Reset). Unblocks AC-ASYNC-LOAD-RACE + AC-CAP-BARRAGE-COMPATIBILITY.
- **Seam 10: IWaveSpawnerPoolMetadataProvider** + FWaveSpawnerPoolMetadataTestStub (SetHasBarrageInPeakPool, Reset). Unblocks AC-CAP-BARRAGE-COMPATIBILITY positive + negative sub-cases.
- **Seam 11: ICurveProvider** + FCurveProviderTestStub (SetReturnValue, GetCallCount, Reset). Unblocks AC-08c sub-case (iv) — Rule 8 output-side NaN guard validation.
- ADR Dependencies table + GDD ACs Unblocked table updated.

Separate flags for producer (not GDD revision items):
- DPC GDD now Approved. 4 of 13 MVP GDDs approved (Input System, Run State Machine, Player Movement, Difficulty & Phase Controller).
- Pull-Wave Behavior GDD next in design order (#5 of 13 MVP).
- Telegraph System prototype is highest-risk bet per systems-index — recommend prototype in parallel with Pull-Wave GDD authoring.
- RSM GDD revision is the BLOCKING prerequisite for DPC implementation (5-item ForceTickNow forward contract + range tightening).
- Wave Spawner GDD scope unchanged from R6 (≥17 patterns total at Alpha: ≥4 OPENER + ≥4 MID + ≥9 PEAK = 5 non-barrage + 4 barrage).
- `.claude/docs/technical-preferences.md` still unpopulated; `/setup-engine` still not run.
- **NEW**: `.claude/skills/design-review/SKILL.md` Phase 2b seam-doc grep step added — catches paper-only-infrastructure pattern on every future GDD. Validate against next /design-review invocation (likely Pull-Wave).

Specialist disagreements: 0 (first time in 7 rounds; one cross-cluster overlap between systems-designer B-2 and unreal-specialist B-1 on the watchdog/bInitialized region — both specialists independently identified the same root cause from different domains, which is convergent evidence, not disagreement).

CD adjudications this review: 3 binding (Path A IsTickable resolution; Pull-Wave-deferred MIN_BARRAGE_LANE_SEPARATION provisional=1 + AC-PILLAR-2-BARRAGE-SPATIAL-K; process-fix design-review skill amendment).

R7 verdict (post-revision): **Approved**. R8 forecast (if implementation surfaces a surprise): 0–2 blockers per CD assumption set. CD strategic recommendation explicit: do NOT run an R8 on the GDD; the next genuine source of design feedback is the Telegraph prototype. The 7-round investment is complete; the document is implementation-ready.

---

## R-Updated Mini-Review — 2026-06-18 — Verdict: NEEDS REVISION (FLOOR propagation cascade from Pull-Wave R10d ruling)

**Scope signal**: M (5 stale-residue propagation sites + 1 cascade collapse requiring Path B reauthoring + 1 cross-system coordination doc append; no new ADRs)
**Specialists**: main-session analysis only (lean mode — Pull-Wave R11's CD already established mini-review scope; context budget heavy at ~75%)
**Counts**: 6 BLOCKING / 5 RECOMMENDED / 3 NICE-TO-HAVE
**Trigger**: BATCHED with Pull-Wave R11 fresh-context re-review per CD R10d ruling Section 5 sequencing recommendation. Pull-Wave R11 (verdict: MAJOR REVISION NEEDED, 8 BLOCKING) ran first in this session; DPC mini-review is the second half of the batch. Both verdicts feed the combined R11a-arithmetic + R11a-measurement plan for execution in fresh `/clear` sessions.
**Prior verdict resolved**: R7 Approved stands as the baseline; this mini-review is a propagation cascade audit, not a re-litigation of R7 closures.

### Context

Pull-Wave R10d ruling (2026-06-17) raised `TELEGRAPH_WINDOW_FLOOR_S` from 0.65s to 0.70s via creative-director PATH (i) trade-off ruling to close F-BARRAGE-SURVIVABILITY-INVARIANT line 546 self-admission under direction-detection model. R10d was a Pull-Wave-side binding parameter raise; the cross-system propagation to DPC was DEFERRED to `/propagate-design-change` post-R11-APPROVED. The CD R10d Section 5 sequencing recommendation explicitly mandated batching Pull-Wave R11 + DPC mini-review to surface cross-doc inconsistency BLOCKING in the same cycle rather than running Pull-Wave R11 alone.

DPC mini-review audits DPC GDD against the R10d binding parameter raise. Three classes of finding emerge: (A) mechanical FLOOR=0.6→0.70 propagation across ~14 sites; (B) one critical CASCADE FINDING — TelegraphWindowCurve authored keys silently destroy MID's overlap window under FLOOR=0.70s; (C) external doc propagation (`cross-system-survivability-coordination-2026-06-11.md` R10d resolution append).

### Phase 2b Seam-Doc Grep: PASS

All 5 DPC interfaces verified in `docs/architecture/platform-seam-interfaces.md`:
- ICurveProvider (Seam 11 R7 ext.)
- IDPCAbortDelegate (Seam 9 R7 ext.)
- IDPCSnapshotConsumer (Seam 2 base)
- IRSMTimeStateProvider (Seam 7 base)
- IWaveSpawnerPoolMetadataProvider (Seam 10 R7 ext.)

No new seam authoring required by R10d propagation; the FLOOR raise is a pure-parameter change.

### BLOCKING items (6)

#### B1 — TelegraphWindowCurve cascade collapse (HIGHEST-IMPACT FINDING)

**Site**: Line 710 (Tuning Knobs row `TelegraphWindowCurve` authored keys) + lines 38-50 (R6 Q7 binding Path B reauthoring rule).

**Problem**: The authored TelegraphWindowCurve keys `(0.0, 0.9), (0.333, 0.72), (0.75, 0.6), (1.0, 0.6)` were derived under pre-R2 FLOOR=0.6s. Under R10d FLOOR=0.70s, the post-floor-clamp curve becomes:
- OPENER (t_norm=0.0): `max(0.70, 0.9) = 0.9s` (unchanged — 0.20s above floor)
- MID (t_norm=0.333): `max(0.70, 0.72) = 0.72s` (only 0.02s above floor — **collapsed from 0.12s overlap to 0.02s overlap**)
- PEAK (t_norm=0.75): `max(0.70, 0.6) = 0.70s` (clamped to floor)
- t_norm=1.0: `max(0.70, 0.6) = 0.70s` (clamped to floor)

**MID's design-bound 0.12s overlap window is reduced 83% to 0.02s** — silent Phase Identity collapse. This is exactly the failure mode that lines 38-50 R6 Q7 binding rule warns against (citing the 0.9s hypothetical case where MID collapses to PEAK).

**DPC's own binding rules at lines 40-50 require Path B reauthoring**: MID's key at t_norm=0.333 MUST equal `TELEGRAPH_WINDOW_FLOOR_S + intra_phase_overlap_s` where intra_phase_overlap_s = 0.12s; OPENER's key at t_norm=0.0 MUST equal MID's key + intra_phase_overlap_s. Under R10d FLOOR=0.70s:
- MID key MUST equal `0.70 + 0.12 = 0.82s`
- OPENER key MUST equal `0.82 + 0.12 = 0.94s`
- PEAK key (t_norm=0.75) MUST equal `0.70s`

**The R10d raise should have triggered Path B reauthoring but did not.** Pull-Wave R11 GD R1 RECOMMENDED flagging PEAK identity erosion but did not derive the MID-overlap arithmetic. This mini-review surfaces it.

**Fix required**: re-author `TelegraphWindowCurve` to new keys `(0.0, 0.94), (0.333, 0.82), (0.75, 0.70), (1.0, 0.70)` AND verify Differentiator 1 (cadence-character shift MID→PEAK) survives the new curve — at 0.82s MID overlap window is 0.12s preserved; at 0.70s PEAK overlap window is zero (edge-to-edge sequential at the new floor). Wave Spawner pattern authoring must re-baseline against new MID telegraph window 0.82s (not 0.72s) and the Path-B-cascade fact must be propagated to forward-contracted Telegraph System + Wave Spawner pattern library GDDs. Alternative: explicitly bind `intra_phase_overlap_s = 0.02s` as the new design intent — this trades Pillar 3 run-arc readability for parameter stability and requires creative-director adjudication (not mechanical propagation).

#### B2 — Stale FLOOR=0.6s across ~14 sites

**Sites**: Lines 32, 38, 136, 138-142, 252, 479, 507, 622, 661, 666-667, 693, 710 + Tuning Knobs row.

All reference `TELEGRAPH_WINDOW_FLOOR_S = 0.6s` (or carry "PROVISIONAL" marker tied to pre-R10d framing pre-AC-PILLAR-2-CONCURRENT closure). Mechanical FLOOR=0.6→0.70 propagation needed at every site.

#### B3 — Tuning Knobs row line 479 R9 coordination note invalidated by R10d

**Current text** (line 479): "**PM R9 cross-system coordination resolution (2026-06-11)**: this floor is NOT raised by the PM R9 F-BARRAGE-SURVIVABILITY-INVARIANT cliff fix. Option (c) was bound at PM R9 (Wave Spawner cook-time consecutive-triplet exclusion + MIN_ESCAPE_SLIPS reduced 3→2); FLOOR stays at the R2 Cluster E value of 0.65s. No DPC GDD content change required."

**R10d directly inverts this.** Replace with R10d resolution note: "**Pull-Wave R10d cross-system resolution (2026-06-17)**: FLOOR raised 0.65s → 0.70s via creative-director PATH (i) trade-off ruling. Closes Pull-Wave F-BARRAGE-SURVIVABILITY-INVARIANT line 546 self-admission under direction-detection model at zero margin (`0.15 + 0.25 + 0.30 = 0.70 ≤ 0.70s`). R9 Option (c) Wave Spawner cook-time consecutive-triplet exclusion + MIN_ESCAPE_SLIPS=2 REMAINS BINDING (locomotion-only invariant `0.15×2 + 0.25 = 0.55 ≤ 0.70` ✓ 150ms margin). DPC GDD content change REQUIRED (this row + Rule 9 PROVISIONAL framing retirement + 14 FLOOR=0.6→0.70 propagation sites). See `design/gdd/reviews/pull-wave-behavior-review-log.md` R10d entry."

#### B4 — Rule 9 PROVISIONAL framing obsolete

**Sites**: Lines 138-142 (Rule 9) + lines 507 (F-3b Output Range) + line 479 PROVISIONAL marker.

Rule 9 declares FLOOR=0.6s as "PROVISIONAL pending AC-PILLAR-2-CONCURRENT close at prototype milestone." Under R10d, FLOOR was raised to 0.70s by a different mechanism (cross-system survivability ruling, not playtest closure). Resolution path text "If AC fails: raised to smallest value satisfying the 80% target (likely 0.7–0.9s)" is partially fulfilled — FLOOR already raised by external mechanism.

**Restructure required**: FLOOR is now design-time-bound at 0.70s via R10d CD ruling. AC-PILLAR-2-CONCURRENT validates the new value at prototype as confidence check, NOT as closure-gate. Apply the two-phase gate pattern from Pull-Wave R10b: ADVISORY-at-story-Done (static arithmetic verification at FLOOR=0.70s) + BLOCKING-at-Alpha (empirical playtest confirmation). The PROVISIONAL marker should retire across all sites; replace with "R10d-bound at 0.70s pending AC-PILLAR-2-CONCURRENT prototype confidence check (two-phase gate: static at story-Done; empirical at Alpha)."

#### B5 — AC-PILLAR-2-CONCURRENT validation target stale

AC-PILLAR-2-CONCURRENT validates "survivability of `0.6s` under concurrent telegraph load with playtest data." Under R10d, target value is `0.70s`. AC body + WHEN/THEN test inputs must update:
- WHEN: at PEAK with `telegraph_window_s = 0.70s` (not 0.6s) under concurrent telegraph load
- THEN: ≥80% survival on first-encounter at sample size n ≥ 30
- GATE: two-phase — ADVISORY-at-story-Done (arithmetic verification) + BLOCKING-at-Alpha (empirical closure)

If AC-PILLAR-2-CONCURRENT fails at Alpha empirical, the FLOOR raise path under the new design-time-bound framing requires CD ruling per Pull-Wave R10d precedent — not the automatic "raise to 0.7-0.9s" path the original Rule 9 prescribed.

#### B6 — cross-system-survivability-coordination-2026-06-11.md R10d resolution append

The sealed R9 verdict at FLOOR=0.65s is superseded by R10d. Add 2026-06-18 R10d resolution entry to the coordination doc: "PATH (i) FLOOR 0.65 → 0.70s adopted via CD ruling 2026-06-17 closes Pull-Wave F-invariant cliff under direction-detection model; supersedes R9 Option (c) for FLOOR value but Option (c) Wave Spawner cook-time consecutive-triplet exclusion + MIN_ESCAPE_SLIPS=2 REMAINS BINDING (locomotion-only invariant closure mechanism). Both mechanisms together provide structural margin: locomotion-only at 150ms (REACT=0.25); onset-inclusive at zero margin closed under direction-detection model. R10d is the cross-system survivability resolution for the R8 RC-E cliff that R9 deferred."

### RECOMMENDED (5)

1. **DPC GDD status header update** — Line 3 says "Pending Re-Review 7" but DPC is Approved R7 per the R7 review log entry and systems-index. Update to "Approved (R7 2026-06-06); R-Updated DPC mini-review pending 2026-06-18 — 6 BLOCKING FLOOR propagation cascade from Pull-Wave R10d ruling" with new R-Updated propagation prefix.

2. **BARRAGE_SIMULTANEITY_WINDOW_S derivation update** — Currently 0.3s PROVISIONAL. Under R10d FLOOR=0.70s, design intent of `≈ FLOOR/2` produces 0.35s. Cross-pattern stagger policy lines 666-667 cite `BARRAGE_SIMULTANEITY_WINDOW_S` PROVISIONAL 0.3s — update in lockstep with entities.yaml (Pull-Wave R11 already flagged entities.yaml side). Note for DPC mini-review: entities.yaml derivation note at lines 227-243 explicitly derives `W = 0.325s` at FLOOR=0.65s — must update to W = 0.35s at FLOOR=0.70s.

3. **Pillars 1/3 PEAK Phase Identity feel-test forward contract** (per Pull-Wave R11 GD R1) — 7.7% PEAK telegraph slowdown is cosmetic per R10d CD ruling but DPC binds PEAK to "telegraphs at the floor"; document a playtest feel-test forward contract noting that R10d cosmetic slowdown is acknowledged design-time. Bind to `/playtest` skill outcomes at Alpha.

4. **MIN_BARRAGE_LANE_SEPARATION R10d non-invalidation note** — Line 655 R7 PROVISIONAL framing (=1, level-designer R7-B-1 spatial-variety collapse fix) is unaffected by R10d (lane authorability is geometric, not timing) — but adding a "R10d does not invalidate provisional=1" note prevents future misreads.

5. **Audio Controller post-R10d forward contract** — Audio Controller GDD when authored inherits TELEGRAPH_WINDOW_FLOOR_S from DPC. If audio sting timing references the FLOOR, it inherits the R10d raise. Add explicit forward contract.

### NICE-TO-HAVE (3)

1. Editorial rule numbering (1-11, 16, 17, 12-15; EC 1-7, 9-12, 8) — 7th consecutive review deferred per R7 note; R10d doesn't change deferral. R-Updated inherits the deferral.
2. AC-AUDIO-STING null hypothesis re-validation if Audio Controller GDD ever lands at the new FLOOR — current p_null=0.055 derivation was at FLOOR=0.6s.
3. Update DPC GDD header status to reflect both R7 Approved + R10d propagation R-Updated mini-review state in one coherent line.

### Senior verdict (main-session synthesis)

DPC mini-review surfaces one **critical cascade finding** (BLOCKING B1: TelegraphWindowCurve cascade collapse — MID's 0.12s overlap window compresses to 0.02s under R10d without Path B reauthoring) plus **5 mechanical FLOOR propagation BLOCKING** items. The cascade finding is structurally important: DPC's own line 38-50 R6 binding rules explicitly require Path B reauthoring when FLOOR rises, but R10d raised FLOOR without triggering Path B — silent Phase Identity collapse. This is the same class of failure the line 38 paragraph warns about ("the prior version of this paragraph claimed structural independence without acknowledging..."). The mini-review's value is catching this — Pull-Wave R11 GD R1 RECOMMENDED flagging PEAK identity erosion but did not derive the 83% MID-overlap collapse arithmetic.

Otherwise the mini-review is a clean propagation pass — Phase 2b seam grep CLEAN, no new design surface, no specialist disagreements would have emerged from a full panel because the scope is well-bounded by Pull-Wave R11's prior intelligence and the R10d CD's explicit Section 5 sequencing scope. **Verdict: NEEDS REVISION**, not MAJOR REVISION — DPC is structurally sound at R7 Approved + R10d cascade is propagation cleanup + 1 cascade collapse fix. R-Updated re-review forecast post-R11a-arithmetic landing: 0-1 BLOCKING / 1-2 RECOMMENDED.

### R11a routing implications (DPC mini-review surface folds into R11a-arithmetic)

DPC mini-review's 6 BLOCKING items all fold into Pull-Wave R11a-arithmetic's cross-doc batch per CD ruling:
- B1 TelegraphWindowCurve cascade collapse → may need CD adjudication on choice between (a) re-author curve to new keys or (b) bind intra_phase_overlap_s=0.02s as new design intent (Pillar 3 trade) — flag as the only DPC mini-review finding with substantive design surface
- B2 Stale FLOOR=0.6 across 14 sites → mechanical FLOOR=0.6→0.70 propagation
- B3 Tuning Knobs row R9 coordination note → mechanical replacement with R10d resolution note
- B4 Rule 9 PROVISIONAL framing → restructure as design-time-bound under R10d
- B5 AC-PILLAR-2-CONCURRENT validation target → mechanical 0.6→0.70 + apply two-phase gate pattern
- B6 cross-system-survivability-coordination-2026-06-11.md R10d resolution append → mechanical append

### Files NOT modified by DPC mini-review pass (intentional — fresh-context re-review is read-only on target GDD; propagation deferred to R11a-arithmetic)

- `design/gdd/difficulty-phase-controller.md` — target GDD, read-only during mini-review
- `design/registry/entities.yaml` — addressed by Pull-Wave R11a-arithmetic cross-doc batch
- `design/gdd/reviews/cross-system-survivability-coordination-2026-06-11.md` — R10d resolution append queued for R11a-arithmetic
- `design/gdd/pull-wave-behavior.md` — addressed by Pull-Wave R11a-arithmetic (4 stale-residue lines + Pillar 2 language)
- `design/gdd/game-concept.md` — Core Fantasy "half-second" 40% drift addressed by Pull-Wave R11a-arithmetic
- `docs/architecture/platform-seam-interfaces.md` — no seam changes (Phase 2b PASS)

### Files modified by DPC mini-review pass

- `design/gdd/systems-index.md` — DPC row R-Updated mini-review verdict prefix added to combined batched R11 narrative
- `design/gdd/reviews/difficulty-phase-controller-review-log.md` — this R-Updated mini-review entry appended
- `production/session-state/active.md` — combined Pull-Wave R11 + DPC mini-review final checkpoint

### Strategic note

The DPC mini-review's CD R10d Section 5 sequencing recommendation was VALIDATED by this run — running Pull-Wave R11 alone would have surfaced cross-doc BLOCKING (DPC at 0.65s vs Pull-Wave at 0.70s) without catching the TelegraphWindowCurve cascade collapse arithmetic. The batched approach surfaced the cascade finding at design-review time rather than at Telegraph prototype time when the collapse would manifest as "MID feels like PEAK" qualitative regression. The CD's pre-emptive sequencing call paid off structurally.

The R11a-arithmetic workstream (executing in fresh `/clear` session) now has a well-bounded combined surface:
- Pull-Wave: 4 stale-residue lines (408, 596, 640, 953) + Pillar 2 language + Core Fantasy in game-concept.md
- DPC: 14 FLOOR=0.6→0.70 propagation sites + Rule 9 PROVISIONAL framing restructure + Tuning Knobs row R9-note replacement + AC-PILLAR-2-CONCURRENT validation target update + TelegraphWindowCurve Path B reauthoring (CD adjudication may be needed)
- entities.yaml: FLOOR value + BARRAGE_SIMULTANEITY_WINDOW_S derivation
- cross-system-survivability-coordination-2026-06-11.md: R10d resolution append
- Pull-Wave R11a-measurement (parallel): 2-AFC apparatus + LEFT/RIGHT randomization + Alpha PROTO-GATE escalation path
- Methodology ADR: dual-grep sweep technique

The combined R11a + DPC mini-review propagation forecast: ~16-22 BLOCKING items total across all 4 docs, all mechanically prescribed except B1 (TelegraphWindowCurve cascade) which may need CD adjudication. R12 forecast post-R11a landing: Pull-Wave 1-2 BLOCKING / 2-3 RECOMMENDED; DPC R-Updated 0-1 BLOCKING / 1-2 RECOMMENDED.

---

## R11a-arithmetic — 2026-06-18 (Mechanical Propagation Pass for DPC mini-review)

**Trigger**: BATCHED R-Updated DPC mini-review (2026-06-18, 6 BLOCKING + 5 RECOMMENDED + 3 NICE-TO-HAVE — above) + Pull-Wave R11 fresh-context re-review (2026-06-17, 8 BLOCKING + 16 RECOMMENDED + 6 NICE-TO-HAVE — see `design/gdd/reviews/pull-wave-behavior-review-log.md` R11 entry) both adjudicated to require an R11a-arithmetic propagation pass per CD R10d Section 5 sequencing recommendation. Companion to the Pull-Wave R11a-arithmetic entry of same date.

**Mode**: same-author fresh-context mechanical propagation pass with one design-shaped edit (TelegraphWindowCurve Path B reauthoring per DPC's existing R6 Q7 binding rule that R10d failed to trigger). The Path B edit is FLAGGED FOR CD POST-PASS REVIEW.

**Methodology**: dual-grep technique per the new methodology ADR (`docs/architecture/adr-0004-r11a-dual-grep-methodology.md`). The pass validated the methodology by surfacing a 15th FLOOR site (line 717 R8-era 0.68s PROVISIONAL stale residue in the Non-tunable runtime invariants table) beyond the R-Updated mini-review's 14-site enumeration. See methodology ADR Validation Criteria item 1.

### All 6 DPC R-Updated mini-review BLOCKING items closed

- **B1 — TelegraphWindowCurve cascade collapse (HIGHEST-IMPACT, line 710 + R6 Q7 binding rule at lines 38-50)**: Path B reauthoring applied per DPC's own line 38-50 R6 Q7 binding rule. **New curve keys: `(0.0, 0.94), (0.333, 0.82), (0.75, 0.70), (1.0, 0.70)`** (pre-R10d keys were `(0.0, 0.9), (0.333, 0.72), (0.75, 0.6), (1.0, 0.6)`). Restores the 0.12s intra-phase overlap window (MID key = FLOOR + 0.12 = 0.82s; OPENER key = MID + 0.12 = 0.94s; PEAK key = FLOOR = 0.70s) under R10d FLOOR=0.70s; pre-R10d keys under R10d FLOOR would have collapsed MID's overlap from 0.12s to 0.02s (83% reduction = silent Phase Identity collapse). The mechanical derivation follows DPC's own R6 Q7 binding rule that R10d failed to trigger; **the R11a-arithmetic pass applied the rule, did not make a new design judgment**. Path B reauthoring note added to the Tuning Knobs `TelegraphWindowCurve` row with a clear annotation tagging the cascade and the R10d trigger. Lines 38-50 cascade-analysis paragraph updated to replace the R6-era hypothetical case (floor=0.9s walk-through) with the actual R10d case (floor=0.70s walk-through), preserving the general rule and the cascade illustration for future raises. **FLAGGED FOR CD POST-PASS REVIEW**: confirm Path B (re-author for overlap preservation, applied mechanically per the existing binding rule) vs Path C (bind 0.02s overlap as new design intent / Pillar 3 trade). The R11a-arithmetic pass cannot make this design-intent ratification — only CD can. Annotation in the Tuning Knobs row body explicitly flags this for CD post-pass review.

- **B2 — Stale FLOOR=0.6s across 14 sites (+ 1 dual-grep surfaced — 15 total)**: All sites updated to R10d-bound 0.70s with consistent R11a-arithmetic 2026-06-18 annotation tagging:
  - Line 26 (PEAK Phase Identity row in Phase Identity table) — telegraph_window/stagger floor narrative
  - Line 32 (Differentiator 1 cadence-character shift) — MID telegraph_window 0.72s → 0.82s (under Path B reauthoring) + FLOOR 0.6s → 0.70s + PEAK FLOOR 0.6s → 0.70s
  - Line 38 (cascade-analysis paragraph) — replaced R6 hypothetical (floor=0.9s) with actual R10d case (floor=0.70s) for traceability; preserves general rule
  - Lines 42, 46, 48 (Path B reauthoring rules MID/OPENER keys + production/hypothetical examples) — updated to show both R10d (current binding) AND hypothetical Path B floor=0.9s (preserves the further-raise example)
  - Line 136 (Rule 8 three-floors block) — FLOOR 0.6s PROVISIONAL → 0.70s R10d-bound annotation
  - Lines 138-142 (Rule 9 PROVISIONAL framing) — **fully restructured per B4** (see below)
  - Line 252 (null-at-tick-time defensive policy per-scalar-floor incoherence example) — FLOOR=0.6s → 0.70s
  - Line 343 (Phase Identity States and Transitions PEAK row) — per-onset window 0.6s/stagger floor 0.6s → 0.70s/0.70s
  - Line 479 (F-3b Shared variables table TELEGRAPH_WINDOW_FLOOR_S row) — **R9 coordination note inversion per B3** (see below)
  - Lines 507, 509, 515, 517 (F-3b output range, example, boundary checks) — output range `[0.6, ∞)` → `[0.70, ∞)`; example keys updated to Path B production values; boundary checks 0.6s clamped → 0.70s clamped
  - Line 622 (Wave Spawner row item (11) intra-pattern stagger constraint) — PROVISIONAL 0.6s floor → R10d-bound 0.70s floor
  - Line 666 (post-COMPLETE alternative considered) — "last ~0.6s of every run" → "last ~0.70s of every run"
  - Line 668 (Intra-pattern telegraph stagger constraint body) — PROVISIONAL 0.6s → R10d-bound 0.70s; 0.6s onset gap → 0.70s onset gap
  - Line 673 (Cross-pattern stagger policy non-barrage) — PROVISIONAL 0.6s → R10d-bound 0.70s
  - Line 710 (Tuning Knobs TelegraphWindowCurve row) — **Path B reauthoring per B1** (see above)
  - **Line 717 (Non-tunable runtime invariants table TELEGRAPH_WINDOW_FLOOR_S row) — DUAL-GREP SURFACED beyond mini-review's enumeration**: R8-era stale 0.68s PROVISIONAL residue → R10d-bound 0.70s + full history-of-value record (pre-R2 0.6s; R2 Cluster E 0.6→0.65; R8 speculative 0.68 overturned; R9 hold 0.65; R10d 0.65→0.70). This is the methodology ADR's first validating capture — the row was structurally invisible to the mini-review's grep pattern because it was in a different table (Non-tunable runtime invariants, not Tuning Knobs).
  - Line 739 (designer-error class TelegraphWindowCurve example) — `0.9s` to `0.65s` example → `0.94s` to `0.70s` example (R11a-arithmetic illustrative under R10d + Path B keys)
  - Line 744 (designer-error class flat-curve example) — "clamped to 0.6s" → "clamped to 0.70s"
  - Lines 914 + 921 (AC-PILLAR-2-CONCURRENT GIVEN) — `TELEGRAPH_WINDOW_FLOOR_S` window "PROVISIONAL at 0.6s" → "R10d-bound at 0.70s"
  - Line 925 (Path C re-run target) — "staggered ≥0.6s apart" → "staggered ≥0.70s apart"
  - Line 1013 (OQ-DPC-6 derived-constant context) — "TELEGRAPH_WINDOW_FLOOR_S = 0.6s" assumption → "0.70s"

- **B3 — Tuning Knobs row line 479 R9 coordination note invalidated by R10d**: existing "PM R9 cross-system coordination resolution (2026-06-11): this floor is NOT raised by the PM R9 F-BARRAGE-SURVIVABILITY-INVARIANT cliff fix. Option (c) was bound at PM R9 ... FLOOR stays at the R2 Cluster E value of 0.65s. No DPC GDD content change required." text replaced with full R10d resolution note: "Pull-Wave R10d cross-system resolution (2026-06-17 — R11a-arithmetic propagation 2026-06-18; supersedes R9 coordination 2026-06-11 hold-at-0.65s): FLOOR raised 0.65s → 0.70s via creative-director PATH (i) trade-off ruling ..." with full mechanism details + cross-reference to coordination doc R10d resolution append.

- **B4 — Rule 9 PROVISIONAL framing obsolete**: Rule 9 fully restructured. New framing: "Telegraph floor (Pillar 2) — design-time bound at 0.70s under R10d; two-phase gate on AC-PILLAR-2-CONCURRENT." Body restructured to: (a) numeric value 0.70s is design-time bound under R10d CD ruling path (i); (b) two-phase gate explicit (ADVISORY at story-Done static + BLOCKING at Alpha empirical); (c) resolution paths restructured — Path A pass, Path B fail-timing requires CD ruling per R10d precedent (the original automatic "raise to 0.7-0.9s" path retired), Path C fail-perceptual-load unchanged; (d) PROVISIONAL marker scope clarified — applies only to empirical-confirmation phase (Alpha BLOCKING gate); design-time value locked at 0.70s. Per Pull-Wave R10b two-phase gate pattern.

- **B5 — AC-PILLAR-2-CONCURRENT validation target stale**: AC Gate header rewritten with two-phase gate explicit (ADVISORY at DPC story-Done static arithmetic verification across REACT safe range under both perceptibility models + BLOCKING at Alpha empirical playtest closure). AC GIVEN line 921 updated: "PROVISIONAL at 0.6s" → "R10d-bound at 0.70s — see Rule 9". Resolution paths updated: Path A pass locks 0.70s + retires the empirical-phase PROVISIONAL marker; Path B fail-timing requires CD ruling per R10d precedent (pre-R10d automatic "raise to 0.7-0.9s" retired); Path C fail-perceptual-load: stagger target ≥0.70s (was ≥0.6s). Per Pull-Wave R10b two-phase gate pattern.

- **B6 — cross-system-survivability-coordination-2026-06-11.md R10d resolution append**: new "## 2026-06-18 R10d Resolution Entry" section appended near end of doc. Contents: R9 verdict at FLOOR=0.65s superseded by R10d CD PATH (i) raise to 0.70s; R9 Option (c) Wave Spawner cook-time consecutive-triplet exclusion + MIN_ESCAPE_SLIPS=2 REMAINS BINDING (locomotion-only invariant closure mechanism unchanged); mechanism summary (FLOOR=0.70s; MIN_ESCAPE_SLIPS=2; 7 surviving M=3 triplets; BARRAGE_SIMULTANEITY_WINDOW_S derivation target 0.35s; Path B curve reauthoring); propagation pass coverage (DPC 15 sites + Pull-Wave 4 stale-residue + entities.yaml + game-concept.md + methodology ADR); forward contracts still operative (Wave Spawner pool-lifetime +50ms tag informational; DPC TelegraphWindowCurve Path B FLAGGED FOR CD POST-PASS REVIEW; Telegraph + Wave Spawner pattern library GDDs inherit R10d FLOOR + post-Path-B keys when authored).

### DPC RECOMMENDED items addressed

- **R1 — DPC GDD status header update (line 3)**: "Pending Re-Review 7" → "Approved (R7 — 2026-06-06); R-Updated mini-review 2026-06-18 NEEDS REVISION (6 BLOCKING FLOOR propagation cascade from Pull-Wave R10d ruling — see `design/gdd/reviews/difficulty-phase-controller-review-log.md` R-Updated mini-review entry); **R11a-arithmetic propagation pass DONE 2026-06-18** (...) ; awaits R-Updated re-review post-R11a-arithmetic landing (forecast 0-1 BLOCKING / 1-2 RECOMMENDED)." Per the R3 NICE-TO-HAVE editorial direction in the mini-review.
- **R2 — BARRAGE_SIMULTANEITY_WINDOW_S derivation update** — addressed in entities.yaml registry update (target W=0.35s at R10d FLOOR=0.70s); the in-DPC-GDD value update (0.3s → 0.35s) deferred to DPC R-Updated re-review per the existing deferral pattern (current value 0.3 left in place pending DPC author confirmation).
- **R3 — Pillars 1/3 PEAK Phase Identity feel-test forward contract** — not directly authored as a new contract in this pass; the R10d 7.7% PEAK telegraph slowdown is acknowledged in the Tuning Knobs row prose (line 717 + line 479) and the cross-system-survivability-coordination R10d resolution entry; explicit `/playtest` forward contract authoring deferred to DPC R-Updated re-review (out of R11a-arithmetic mechanical-propagation scope).
- **R4 — MIN_BARRAGE_LANE_SEPARATION R10d non-invalidation note** — implicit in the line 1013 OQ-DPC-6 update (now references R10d FLOOR=0.70s as the slip-distance reference; the R7 CD-adjudicated provisional=1 binding is unaffected by R10d — lane authorability is geometric, not timing). Explicit note may be appended at DPC R-Updated re-review if reviewer requests.
- **R5 — Audio Controller post-R10d forward contract** — not authored in this pass (no Audio Controller GDD exists yet); R10d FLOOR raise inherits forward when Audio Controller GDD is authored. Acknowledged in R-Updated mini-review entry above as a future-GDD-inheritance contract.

### DPC NICE-TO-HAVE items

- **N1 — Editorial rule numbering (1-11, 16, 17, 12-15; EC 1-7, 9-12, 8)** — 8th consecutive review deferred per R7 note. R11a-arithmetic scope-disciplined: does not address editorial defects. Annotated in the GDD status header.
- **N2 — AC-AUDIO-STING null hypothesis re-validation** — defer to Audio Controller GDD authoring; out of R11a-arithmetic scope.
- **N3 — DPC GDD header status reflecting both R7 Approved + R10d propagation R-Updated in one coherent line** — addressed by the line-3 status header update under R1.

### Files modified by R11a-arithmetic (DPC scope)

- `design/gdd/difficulty-phase-controller.md` — 15 FLOOR sites updated (B2) + Rule 9 restructure (B4) + Tuning Knobs row line 479 R9-note replacement (B3) + AC-PILLAR-2-CONCURRENT body update (B5) + TelegraphWindowCurve Path B reauthoring (B1) + status header fix (R1) + Last Updated date; ~+60 lines net.
- `design/gdd/reviews/cross-system-survivability-coordination-2026-06-11.md` — R10d resolution entry appended (B6); +~35 lines.
- `design/registry/entities.yaml` — TELEGRAPH_WINDOW_FLOOR_S value + notes restructure (downstream consequence of B2/B3/B4/B5); BARRAGE_SIMULTANEITY_WINDOW_S notes update (R2); header last_updated; ~+30 lines net. (Pull-Wave R11a-arithmetic entry covers this in its own files-modified list — captured here for completeness.)
- `docs/architecture/adr-0004-r11a-dual-grep-methodology.md` — NEW; ~200 lines (Pull-Wave R11a-arithmetic entry covers).
- `design/gdd/systems-index.md` — DPC row updated with R11a-arithmetic prefix; ~+2 lines.
- `production/session-state/active.md` — R11a-arithmetic checkpoint extract (shared with Pull-Wave R11a-arithmetic entry).

### Files NOT modified by R11a-arithmetic (DPC scope, intentional)

- `design/gdd/player-movement.md` — independently in decomposition state; R10d imposes NO PM contracts.
- `docs/architecture/platform-seam-interfaces.md` — Phase 2b seam grep PASS at R-Updated mini-review; no seam changes by R10d propagation.
- Sibling GDDs (input-system, run-state-machine) — not in mini-review scope.

### R-Updated re-review forecast post-R11a-arithmetic

**0-1 BLOCKING / 1-2 RECOMMENDED** per R-Updated mini-review line 674 — UNCHANGED. The R11a-arithmetic pass closed 6/6 R-Updated BLOCKING items + addressed 3/5 RECOMMENDED items + 1/3 NICE-TO-HAVE; residual surface is primarily the CD post-pass review of TelegraphWindowCurve Path B reauthoring (if CD ratifies Path B unchanged, 0 BLOCKING; if CD requests Path C / Pillar 3 trade direction, +1 BLOCKING for the curve key revision pass) plus deferred RECOMMENDED items R3/R5 (Audio Controller forward contract + Pillars 1/3 feel-test contract).

### TelegraphWindowCurve cascade outcome (FLAGGED FOR CD POST-PASS REVIEW)

The Path B reauthoring of `TelegraphWindowCurve` (B1) is the only design-shaped edit in this R11a-arithmetic pass. The new keys `(0.0, 0.94), (0.333, 0.82), (0.75, 0.70), (1.0, 0.70)` were applied mechanically per DPC's own line 38-50 R6 Q7 binding rule that R10d failed to trigger. The pass did not make a new design judgment — it applied the existing binding rule.

**Path B (applied)**: re-author curve keys to preserve the 0.12s intra-phase overlap window per the R6 Q7 binding rule. Telegraph contraction is 0.94s → 0.82s → 0.70s — 0.12s per phase transition, mirroring the pre-R10d intent at the original 0.6s floor (0.9s → 0.72s → 0.6s). Differentiator 1 (cadence-character shift MID→PEAK) is preserved.

**Path C (alternative — NOT applied)**: bind `intra_phase_overlap_s = 0.02s` as new design intent (Pillar 3 trade — accepts the cascade collapse as the post-R10d design state; MID's overlap window shrinks 83% but Path B reauthoring overhead is avoided). This is a Pillar 3 readability trade-off and requires creative-director adjudication; this R11a-arithmetic pass cannot make this call.

**FLAGGED FOR CD POST-PASS REVIEW**: confirm Path B (the current binding under this pass) vs Path C (overlap collapse as new design intent / Pillar 3 trade). The Tuning Knobs row body (line 710 update) explicitly flags this for CD post-pass review with the relevant background.

### Strategic note

The R11a-arithmetic pass is the first real-world application of the dual-grep methodology newly authored as ADR-0004. The DPC mini-review's 14-site enumeration was the input; the dual-grep surfaced a 15th site (line 717) that the mini-review's pattern missed — methodology validating itself in its first deployment. The cascade collapse Path B reauthoring is the only design-shaped edit; the rest is mechanical propagation + annotation cleanup. R-Updated re-review of DPC is the next DPC gate after R11a-arithmetic + R11a-measurement both land.


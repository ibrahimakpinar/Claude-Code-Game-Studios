# Review Log: Run State Machine

---

## Review — 2026-05-04 — Verdict: MAJOR REVISION NEEDED (revised in-session)

**Scope signal:** M
**Specialists:** game-designer, systems-designer, qa-lead, gameplay-programmer, creative-director (senior synthesis)
**Blocking items:** 13 | **Recommended:** (not counted separately — all resolved)
**Prior verdict:** None — first review

**Summary:** The design intent was sound but the first-pass GDD had two compounding structural gaps: the restart path required 2 taps (violating the game-concept "1 tap from next run" contract), and 7 acceptance criteria tested real wall-clock time without an injectable clock abstraction (violating coding-standards.md determinism requirement). Supporting issues included a missing clamp on F-1, a wrong formula example, F-2 missing its `t_pause_start` guard, the DEAD state's cached timer variable unnamed, `is_paused` behavior in RESOLVING unspecified, and the Overview still listing PAUSED as a state after the architectural pivot to `bool is_paused`. All 13 blocking items were resolved in the same session: RESOLVING redesigned to auto-advance after `RESOLVING_SCORE_REVEAL_DURATION_S = 2.0s` (1-tap restart restored); `ITimeSource`/`FakeTimeSource` Clock Injection section added; F-1 clamped; formula examples corrected; F-2 guard added; `cached_remaining_at_death_entry` specified; RESOLVING background behavior specified; "fires exactly once" added to 6 ACs; AC-17 (COMPLETE→RESOLVING) and AC-18 (RESOLVING→IDLE auto-advance) added; Overview rewritten to reflect actual `ERunState` set.

**Pending:** Re-review in a fresh session to verify all 13 fixes and check for any regressions introduced by the RESOLVING redesign.

---

## Review — 2026-05-06 — Verdict: MAJOR REVISION NEEDED (revised in-session)

**Scope signal:** L
**Specialists:** game-designer, systems-designer, qa-lead, unreal-specialist, gameplay-programmer, creative-director (senior synthesis)
**Blocking items:** 12 | **Recommended:** 19
**Prior verdict resolved:** Yes — all 13 prior blockers confirmed addressed

**Summary:** The prior revision pass solved correctness; this review surfaced fidelity problems — the gap between "state machine is correct" and "game delivers its pillars." Four findings dominated: (1) `FApp::GetCurrentTime()` is a frame-cached value that silently zeroes pause accumulation on background/resume — production `FAppTimeSource` must use `FPlatformTime::Seconds()` directly; (2) the COUNTDOWN timer pause had no formula, no cached variable, and no accumulator, meaning a background during the 1.5s countdown would skip the world-assembly entirely on resume — F-4 added; (3) ABORTED state had no emotional choreography and no voluntary abandon path — a "run interrupted" ceremony added to Visual/Audio and UI Requirements; (4) RESOLVING's 2.0s auto-advance risked the player never reading their score — redesigned to tap-to-dismiss with `RESOLVING_SCORE_REVEAL_DURATION_S` as a minimum hold. Additional fixes: `FCoreDelegates::ApplicationWillEnterBackgroundDelegate` named explicitly (resolves Rule 13 delegate ambiguity); `DECLARE_MULTICAST_DELEGATE` and re-entrancy policy added; RSM tick group specified (`TG_PrePhysics`); RUNNING tick-order rule added (timer check precedes death events); EC-1 given explicit design rationale ("player wins ties at the wire"); `ERunOutcome` formally typed; `RunTimeSource` renamed from `ITimeSource` (UInterface naming collision); F-1 upper clamp added; EC-13 init guard for RESOLVING_TIMEOUT_S; AC count expanded from 18 to 26. Creative director verdict: architecture is sound — the next re-review should be the final one before implementation.

**Pending:** Re-review in a fresh session — run /design-review design/gdd/run-state-machine.md after /clear.

---

## Review — 2026-05-06 — Verdict: MAJOR REVISION NEEDED (revised in-session)

**Scope signal:** L
**Specialists:** game-designer, systems-designer, unreal-specialist, gameplay-programmer, ux-designer, creative-director (senior synthesis). qa-lead rate-limited — QA coverage noted as incomplete.
**Blocking items:** 8 | **Recommended:** 15
**Prior verdict resolved:** Yes — all 12 prior blockers confirmed addressed

**Summary:** The prior creative-director's "architecture is sound" verdict was premature in three ways: it didn't pressure-test against platform reality (Android `CLOCK_MONOTONIC` pauses during device sleep — EC-9 claim was factually wrong for mobile), it didn't catch fantasy drift between revisions (RESOLVING oscillated between auto-advance and tap-to-dismiss without fantasy adjudication), and it missed a convergent player-context-transition failure cluster (ABORTED dismiss = run-start, resume without re-acclimation beat, COUNTDOWN animation missing pause-sync) — each caught independently by three separate specialists. All 8 blockers resolved in-session: EC-9 rewritten with CLOCK_BOOTTIME platform verification requirement (OQ-6); hybrid auto-advance RESOLVING with `RESOLVING_AUTO_ADVANCE_S = 3.5s` (fantasy-correct "dream ends on its own schedule"); `ABORTED_CEREMONY_DURATION_S` minimum hold; `RESUME_GRACE_S` knob with `resume_grace` flag suppressing death events post-resume; `OnPausedChanged` delegate specified (closes interface gap for Audio/Movement/Collision); AC-01 / invariants table IDLE row fixed; COUNTDOWN animation sync contract added; AC-31 playtest AC added. AC count expanded from 26 to 31. Three new tuning knobs. Core Rules expanded to 20.

**Pending:** Re-review in a fresh session to confirm all 8 fixes hold. Open Question 6 (Android clock source verification) requires platform research by an engineer — EC-9 cannot fully close without it.

---

## Review — 2026-05-06 — Verdict: NEEDS REVISION (revised in-session)

**Scope signal:** L
**Specialists:** game-designer, systems-designer, qa-lead, unreal-specialist, creative-director (senior synthesis)
**Blocking items:** 8 | **Recommended:** 5
**Prior verdict resolved:** Yes — all 8 prior blockers confirmed addressed

**Summary:** This pass surfaced three categories of remaining work. (1) Two CRITICAL design positions that weren't visible when each was adjudicated individually: the Player Fantasy text contained "I don't know what just happened" — mystery-outcome language in the spec itself, a Pillar 5 self-contradiction; and EC-1's COMPLETE-wins-ties rule (introduced in Review 2 as a Pillar 3 choice) was reversed by the creative-director — simultaneous collision+timer expiry now resolves as DEAD because a mystery survival violates Pillar 5 as much as a mystery death. Rule 17 tick order reversed accordingly. (2) One latent formula correctness bug (F-3/F-4 accumulators had no non-negative clamp; backward clock decrement corrupted F-1) and one spec silence that had accumulated across reviews (RESUME_GRACE_S semantics — what "suppress" means — was never specified; frozen-trajectory-wave contract on resume was a TODO, not a rule). Both resolved: max(0.0,...) clamps added; Rule 19 now specifies collision+spawning suppression and a binding wave-flush contract on resume. (3) Two UE-specific blockers: Rule 16's tick-prerequisite API is architecturally impossible if RSM is a UGameInstanceSubsystem (most common singleton choice) — Rule 16 rewritten with OQ-7 deferral; resume_grace expiry was completely unspecified (FTimerManager vs time_source) — Rule 19 now mandates time_source.GetCurrentTime() per-tick check. Recommended fixes also applied: ensure→check re-entrancy guard, OnPausedChanged re-entrancy prohibition, PAUSED_TIMEOUT_S floor raised, RESOLVING_AUTO_ADVANCE_S init clamp added, ABORTED tonal spec added, Player Fantasy DEAD/COMPLETE language toned down. AC count expanded from 31 to 33. AC-31 threshold raised from 70% to 80%.

**Pending:** Re-review in a fresh session — run /design-review design/gdd/run-state-machine.md after /clear. OQ-7 ADR (RSM object type) remains a blocking implementation prerequisite but does not gate design approval.

---

## Review — 2026-05-06 — Verdict: NEEDS REVISION (revised in-session)

**Scope signal:** L
**Specialists:** game-designer, systems-designer, qa-lead, unreal-specialist, creative-director (senior synthesis)
**Blocking items:** 6 | **Recommended:** 8+
**Prior verdict resolved:** Yes — all 8 prior blockers confirmed addressed

**Summary:** This pass surfaced two categories of issues. (1) Two factual platform errors in the UE-specific implementation spec: the re-entrancy guard (`check(!bBroadcasting)`) was documented as Shipping-safe, but `check()` is also gated by `DO_CHECK = 0` in Shipping builds — both `check()` and `ensure()` are no-ops in production. Fixed with a runtime conditional (`if (bBroadcasting)` + log + early return). More critically, EC-9 claimed iOS `mach_absolute_time` "advances through device sleep" — this is factually incorrect; `mach_absolute_time()` pauses during CPU suspension on iOS, the same bug documented for Android. Fixed by extending EC-9 to cover both platforms, requiring `mach_continuous_time()` on iOS and `CLOCK_BOOTTIME` on Android. (2) Three AC-level defects: AC-26 directly contradicted AC-11 on the `run_outcome` payload value in the ABORTED→IDLE transition (carve-out added to AC-26); AC-18's watchdog test path was unreachable under normal knob values (precondition added); AC-32's "Repeat for DEAD/COMPLETE/RESOLVING/IDLE" precondition was impossible to set up (rewritten to observable property). One missing init guard (EC-17: RESOLVING_TIMEOUT_S must exceed RESOLVING_AUTO_ADVANCE_S) was also added. Recommended fixes applied: AC-25 OnPausedChanged assertion, AC-13/AC-14 FakeTimeSource semantics, AC-17 tap-wins simultaneous tie, Dependencies cross-system invariant notes (Wave Spawner 0.4s post-grace obligation, HUD wire-death Pillar 5 obligation), EC-5 UX warning, OQ-6 updated to cover both platforms, OQ-7 expanded with 3 concrete ADR options and Android thread-safety requirement.

**Pending:** Re-review in a fresh session to confirm all 6 fixes hold. Per creative-director: a 7th review should be tightly scoped to verifying these fixes — not a full re-review. OQ-7 ADR (RSM object type) remains a blocking implementation prerequisite.

---

## Review — 2026-05-06 — Verdict: APPROVED

**Scope signal:** L
**Specialists:** None (lean mode — single-session analysis)
**Blocking items:** 0 | **Recommended:** 4
**Prior verdict resolved:** Yes — all 6 prior blockers confirmed addressed

**Summary:** Tightly scoped re-review as directed by the prior creative-director verdict. All 6 Review 5 fixes confirmed in place: runtime re-entrancy conditional, iOS `mach_continuous_time()` + Android `CLOCK_BOOTTIME` in EC-9, AC-26 ABORTED→IDLE carve-out, AC-18 watchdog precondition, AC-32 observable property rewrite, EC-17 RESOLVING_TIMEOUT_S init guard. No new blocking issues introduced. Four recommended items surfaced and resolved in-session: (1) Clock Injection section now explicitly extends the "not via FTimerManager" policy to all RSM timers (snap durations, RESOLVING thresholds); (2) IDLE added to Rule 20 and EC-14's lifecycle no-op state list, consistent with Rule 4's invariant; (3) EC-18 added — RESOLVING simultaneous tap + auto-advance priority (tap wins, per AC-17); (4) Re-entrancy guard split into `bBroadcastingState` (OnStateChanged) and `bBroadcastingPaused` (OnPausedChanged) — prevents cross-signal suppression. GDD status updated to Approved (revised ×6). OQ-6 (platform clock verification) and OQ-7 (RSM object type ADR) remain open as blocking implementation prerequisites; design approval is not gated on either.

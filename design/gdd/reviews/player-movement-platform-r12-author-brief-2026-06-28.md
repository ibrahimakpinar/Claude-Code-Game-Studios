# PM-Platform R12a Scoped Revision Brief

**Date**: 2026-07-01 (filename dated 2026-06-28 to match sub-GDD `[R12a-PENDING]` pointer references authored during Step 2 PASS 4–8; internal Date field reflects actual authoring session)
**Target document**: `design/gdd/player-movement-platform.md` (post-decomposition; currently at Step 3 forward-contracts COMPLETE 2026-07-01)
**Authoring mode**: scoped author revision (NOT in-session patch — CD-mandated handoff per R7 same-session-bias precedent)
**Authority**: PM decomposition plan §7.3 (`design/gdd/reviews/player-movement-decomposition-plan-2026-06-16.md`) + R11 BLOCKING assignment matrix §5
**Pre-revision gate**: PM decomposition Steps 1–3 COMPLETE (structural split + monolith-lift content + cross-sub-GDD forward contracts). Step 4 R12a briefs authored 2026-07-01 (this document). Step 5 systems-index PM row split queued same session.

---

## 0. Why this brief exists

The PM monolith `design/gdd/player-movement.md` R11 fresh-context re-review (2026-06-16) returned **11 BLOCKING** — exceeding the CD-set decomposition trigger (`>8`). CD recommended decomposition into 3 sub-GDDs by failure-domain seams. Platform sub-GDD inherits **3 direct R11 BLOCKING items** (B-QA-1, B-PERF-1, B-SHIP-1) + **1 shared BLOCKING (API side)** (B-CERT-2 — call-site portion belongs to presentation) per decomposition plan §5. **B-F6-3 was originally scoped as platform-owned (definition-site portion) but was CLOSED at PM decomposition Step 2 PASS 7 (2026-06-29)** via the TickComponent-prologue relocation authored in sub-GDD §4 F-PROLOGUE. B-CERT-1 API side (`IGameSettings::IsCommitmentTellFlashEnabled()`) is also platform-owned, lockstepped with presentation DR-PRES-FLASH.

This brief is the scoped R12a author handoff for the platform sub-GDD. It enumerates the DR-* author decisions the R12a pass must close, cites the extensive `[R12a-PENDING]` context already embedded in `player-movement-platform.md` (authored during Step 2 PASS 4–8 to preserve monolith prose lift + pre-R12a fallback behavior), and sets a calibrated R12 fresh-review forecast.

**Format discipline**: this brief is **light-with-pointers** by design. The sub-GDD already carries the full defect context, option enumeration, pre-R12a fallback behavior, and cross-sub-GDD forward-contract implications for each DR-* — in the EC-30 / EC-31 / EC-32 subsections + §4 F-HW-B-MEASUREMENT + §3 Public Interface (Platform-Side) + Bidirectional Notes. Duplicating that content in this brief would create divergence-risk against the sub-GDD authoritative text. The brief's job is to enumerate what R12a MUST decide + where the decision lands + what verification the decision requires. Rationale confirmed by pre-authoring advisor round (2026-07-01).

**R12 fresh-review forecast**: **1–3 BLOCKING** (single-domain sub-GDD; per plan §7.3 calibrated forecast band). Decomposition trigger per sub-GDD: `>5 BLOCKING`.

**Validation criteria (CD-set)** — R12 succeeds if:
- BLOCKING count within 1–3 forecast band.
- All 3 direct + 1 shared inherited R11 BLOCKINGs have documented closure paths in the sub-GDD (via DR-* decisions authored below).
- Every `[R12a-PENDING]` marker in `player-movement-platform.md` is either resolved (decision authored, marker removed) or explicitly deferred with an updated fallback-behavior stance and traceability trail.
- B-F6-3 CLOSED framing survives fresh-context R12 review (grep gate: sub-GDD §4 F-PROLOGUE + Migration Plan invariant references remain word-consistent with mechanics §4 F-2 + F-6 consumer callsites).
- Cross-sub-GDD lockstep with presentation DR-PRES-FLASH + DR-PRES-HAPTIC-GATE holds — API names match byte-for-byte between platform §3 Public Interface (Platform-Side) declaration and presentation call-site gate-check.
- No new failure-domain surface appears (a 4th BLOCKING cluster within platform scope would trigger further scope tightening).

---

## 1. In-scope R12a author decisions (5 DR-*)

Each decision below carries: the R11 BLOCKING it closes, the sub-GDD landing site(s), the option enumeration (cross-referenced from the sub-GDD — not duplicated), the domain authority, and CD recommendations where applicable.

### 1.1 DR-PLAT-TEST — AC-21 / AC-SS-A tick-601 verifiability + test-strategy preamble refresh (closes B-QA-1)

**Defect**: AC-21 asserts under sustained SLIP_TWEEN safe-range violation that "the first log fires on tick 1 and the second log fires on tick 601 (per the rate-limited log per AC-SS-A)." Standard automated unit-test fixtures cannot advance a `TickComponent` 601 times without either wall-clock delay or a mocked-clock harness. Additionally, the pre-R11a test-strategy preamble does not enumerate the R11a-added private members (4 F-6 fade-out members, watchdog state fields, shipping-safety flags) — automation coverage matrix is stale.

**Author decision required**:
- **Verifiability path**: select one of:
  - **(a) Mocked-clock harness** — inject 601 `TickComponent` calls without wall-clock delay. Preserves the tick-601 semantic + 4-site lockstep contract. **CD recommendation (lower-risk path)**.
  - **(b) Re-author log idiom** to count violations instead of ticks (1 log per 600 violations regardless of tick interleave). Semantically equivalent at sustained-violation steady state; requires 4-site lockstep update if assertion language changes from "tick 601" to "violation 601".
  - **(c) Extend test windows to ≥601 ticks** with wall-clock delay accepted (10 s at 60 fps). Rejected in prior review as unacceptable CI cost.
- **Test-strategy preamble refresh**: enumerate the R11a-added members (F-6 fade-out state + watchdog buffer + shipping-safety flags) in the sub-GDD test-strategy preamble; verify automation coverage matrix per member.

**Sub-GDD landing sites**:
- §3 Tick Ordering + §3 Test Strategy Preamble (`player-movement-platform.md` — preamble refresh site).
- §5 Edge Cases — EC-30 (already authored; R12a resolves the PENDING marker).
- §8 Acceptance Criteria — AC-21 + AC-SS-A test setup narrative + explicit reference to the chosen harness pattern.
- `tests/automation-cpp/` test-fixture code (post-R12a implementation — qa-lead + unreal-specialist authoring session).

**Domain authority**: qa-lead + unreal-specialist (co-sign).

**Post-decision propagation**: qa-lead authors the harness pattern in `tests/automation-cpp/` at Sprint delivery time; AC-21 fixture references it. Update Bidirectional Notes (§6) test infrastructure row.

### 1.2 DR-PLAT-HW-B — AC-HW-B measurement methodology + tolerance (closes B-PERF-1)

**Defect**: AC-HW-B's pass criterion (1) "≥ 99.9 % of frames with `DT ≤ 16.67 ms + 0.5 ms`" budgets 0.5 ms for measurement-apparatus jitter. The 0.5 ms budget has NOT been empirically cross-validated on the named min-spec devices (iPhone XR + Pixel 5). If the actual noise floor on target hardware exceeds 0.5 ms under raw `FApp::GetDeltaTime()` log conditions, AC-HW-B fails with false-positive hitch-detection.

**Author decision required**:
- **Measurement methodology**: select one of (per §4 F-HW-B-MEASUREMENT):
  - **(a) Raw `FApp::GetDeltaTime()` log** written to CSV at run-end. Low-noise, low-context.
  - **(b) Unreal Insights profile** for high-context human investigation. ~0.3–0.5 ms overhead on mobile.
  - **(c) Hybrid (a) + (b)** — raw log for the assertion, Insights for post-hoc human investigation of failed runs. **CD recommendation (per architecture-review pre-flag noted in §4).** Sub-GDD ships §4 with option (a) as pre-R12a baseline until DR-PLAT-HW-B confirms (c).
- **Tolerance calibration**: run a pre-Polish-phase calibration run on the named min-spec devices under raw log conditions. Widen or tighten the 0.5 ms tolerance per empirical noise floor. Document the calibration date + device names + measured floor in AC-HW-B setup notes.

**Sub-GDD landing sites**:
- §4 F-HW-B-MEASUREMENT (`player-movement-platform.md` line ~358 — methodology subsection; sub-GDD ships with option (a) as baseline).
- §5 Edge Cases — EC-31 (already authored; R12a resolves the PENDING marker after Polish-phase calibration).
- §8 Acceptance Criteria — AC-HW-B setup + expected result + calibration reference.
- §7 Tuning Knobs — F-HW-B-MEASUREMENT row (verify safe range + calibration reference).

**Domain authority**: technical-director + performance-analyst (co-sign at Polish gate).

**Post-decision propagation**: Polish-phase device audit executes the calibration run; AC-HW-B tolerance is finalized at Polish gate. R12a authors the framework + records the pre-R12a assumption.

### 1.3 DR-PLAT-SS-C — Tick Ordering Option (b) `bManualTickEnabled` Shipping semantic (closes B-SHIP-1)

**Defect**: §3 Tick Ordering Option (b) documents: if RSM resolves to `UWorldSubsystem` / `UGameInstanceSubsystem`, PM's `TickComponent` is called manually from RSM's `Tick` AND PM's own tick registration MUST be disabled. AC-SS-C asserts if `bManualTickEnabled == true` AND `PrimaryComponentTick.bCanEverTick != false`, TickComponent prologue early-outs with per-frame log at 1 Hz rate limit. **Failure-mode gap**: if the early-out fires, F-PROLOGUE does NOT execute → no `raw_dt` / `effective_dt` defined → watchdog skips the sample → an Option-(b)-broken Shipping build presents as "watchdog never breaches" (silent failure).

**Author decision required**:
- **Early-out watchdog behavior**: select one of:
  - **(a) Early-out still pumps watchdog** — sample `raw_dt = FApp::GetDeltaTime()` independently of F-PROLOGUE inside the early-out path, feed the watchdog. Preserves watchdog signal; the `LogPlayerMovement Error` at 1 Hz still fires so the misconfiguration is visible.
  - **(b) Early-out is "no-watchdog" mode + escalate to `Fatal` in Shipping** — the developer noticing the log catches the misconfiguration before production. Shipping-side escalation is `UE_LOG(LogPlayerMovement, Fatal, ...)` — crashes the build fast rather than silently degrading. Aligns with Shipping-Safety Enforcement Policy's "fail loud, fail fast" principle.
  - **(c) Early-out logs at Error + does not pump watchdog + does not crash** — current pre-R12a fallback behavior. Silent watchdog degradation risk.
- **CD recommendation**: **(a) — pump watchdog independently in the early-out path**. Preserves the watchdog contract while surfacing the misconfiguration via the 1 Hz log. Lower blast-radius than (b) — a Shipping-Fatal in early-out semantics risks player-facing crashes for developer-side misconfigurations.

**Sub-GDD landing sites**:
- §3 Tick Ordering + Shipping-Safety Enforcement Policy (`player-movement-platform.md` — AC-SS-C description site).
- §5 Edge Cases — EC-32 (already authored; R12a resolves the PENDING marker).
- §8 Acceptance Criteria — AC-SS-C setup + expected result + explicit watchdog-still-pumps assertion if (a) chosen.
- §4 F-PROLOGUE — verify the early-out path's `raw_dt` sampling is documented under the chosen option.

**Domain authority**: technical-director + unreal-specialist (co-sign).

**Post-decision propagation**: implementation reflects the chosen early-out behavior. Update Bidirectional Notes (§6) Tick Ordering row.

### 1.4 DR-PLAT-HAPTIC-API — `IHapticDispatch::IsSystemHapticsEnabled()` cross-platform query API (closes B-CERT-2 API side; lockstep with presentation DR-PRES-HAPTIC-GATE)

**Defect**: Presentation-side haptic dispatch gate (DR-PRES-HAPTIC-GATE — see `player-movement-presentation-r12-author-brief-2026-06-28.md` §1.3) requires a platform-owned query API that unifies iOS Core Haptics OS-state check + Android VibrationEffect / DND check into a single cross-platform boolean. Platform sub-GDD §3 Public Interface (Platform-Side) currently lists this API as R12a-pending.

**Author decision required**:
- **API name**: confirm `IHapticDispatch::IsSystemHapticsEnabled()` (working name from sub-GDD Bidirectional Notes) — verify no naming collision with Unreal Engine or third-party plugin API surface at UE 5.7.
- **Return semantics**: `true` iff **both** platform-level system-haptics engine is enabled AND OS-level suppression (iOS Focus modes: Work / Sleep / Driving / Do Not Disturb; Android Do Not Disturb) is inactive. Return `false` on unsupported platforms (implicit no-haptic).
- **Cross-platform impl outline**: iOS uses `UIAccessibility.isReduceMotionEnabled` + Core Haptics engine query + Focus Filter API (iOS 15+). Android uses `Vibrator.hasVibrator()` + `NotificationManager.getCurrentInterruptionFilter()`. Document the mapping in Bidirectional Notes.
- **Query cost budget**: OS-state query MUST be cheap enough to call synchronously on every haptic dispatch site (3 sites per PM tick under worst-case). Target: <10 µs per query on min-spec devices. Cache OS state at 1 Hz if native query cost exceeds budget.

**Sub-GDD landing sites**:
- §3 Public Interface (Platform-Side) — `IHapticDispatch::IsSystemHapticsEnabled()` API row (already authored PENDING; R12a fills in the semantic + cost budget).
- §6 Bidirectional Notes — cross-platform impl mapping + presentation call-site gate-check contract + Input System GDD forward contract.
- §8 Acceptance Criteria — no new AC on platform side (verification happens presentation-side via AC-CERT-2); platform §8 may add an AC-HAPTIC-API-COST assertion if cost budget is load-bearing.

**Domain authority**: unreal-specialist + platform (technical-director sign-off).

**Cross-sub-GDD forward contract**: presentation sub-GDD §3 haptic dispatch sites (SlipConfirmed / BufferDrop / NearMiss) consume this API — see `player-movement-presentation-r12-author-brief-2026-06-28.md` §1.3. Input System GDD §Haptic Vocabulary inherits the query API contract on first revision. Lockstep authoring required.

### 1.5 DR-PLAT-FLASH-API — `IGameSettings::IsCommitmentTellFlashEnabled()` settings-bridge API (closes B-CERT-1 API side; lockstep with presentation DR-PRES-FLASH)

**Defect**: Presentation-side flash gate (DR-PRES-FLASH — see `player-movement-presentation-r12-author-brief-2026-06-28.md` §1.2) requires a platform-owned settings-bridge API analogous to the existing `IGameSettings::IsNearMissHapticEnabled()` pattern. Platform sub-GDD §3 Public Interface (Platform-Side) currently lists this API as R12a-pending.

**Author decision required**:
- **API name**: confirm `IGameSettings::IsCommitmentTellFlashEnabled()` (working name from sub-GDD Bidirectional Notes) — verify parallel to `IsNearMissHapticEnabled()` established at R11a-12.
- **Storage semantics**: setting persists via standard user-settings save/load path (SaveGame or GameUserSettings depending on platform save architecture; defer to HUD/Accessibility Settings GDD authoring). API abstracts storage from the caller.
- **Default resolution**: on first query before HUD/Accessibility Settings GDD authors the setting, return `true` (current design intent — flash dispatches). Post-R12a APPROVED, HUD/Accessibility Settings GDD authors the persistent storage; API return reflects stored value.
- **OS-level reduce-motion integration**: OPTIONAL — if the iOS `UIAccessibility.isReduceMotionEnabled` OR Android `Settings.Global.TRANSITION_ANIMATION_SCALE == 0` is set, should the API return `false` regardless of the in-game setting? CD recommendation: **YES** — OS-level accessibility preferences override in-game defaults when the OS state is more restrictive. Document as an OR-composed override rule.

**Sub-GDD landing sites**:
- §3 Public Interface (Platform-Side) — `IGameSettings::IsCommitmentTellFlashEnabled()` API row (already authored PENDING; R12a fills in the semantic + OS-override rule).
- §6 Bidirectional Notes — HUD/Accessibility Settings GDD forward contract + presentation call-site gate-check contract + OS-level override composition rule.
- §8 Acceptance Criteria — no new AC on platform side (verification happens presentation-side via AC-CERT-1); platform §8 may add an AC-FLASH-API-OS-OVERRIDE assertion if the OR-composed override rule is load-bearing.

**Domain authority**: unreal-specialist + accessibility-specialist (co-sign).

**Cross-sub-GDD forward contract**: presentation sub-GDD §3 `LeadingFaceFlash` dispatch site consumes this API — see `player-movement-presentation-r12-author-brief-2026-06-28.md` §1.2. HUD/Accessibility Settings GDD inherits storage on first authoring. Lockstep authoring required.

---

## 2. Out of scope for R12a (platform)

- **B-F6-3 (F-6 effective_dt SETTLED-path leak, definition-site portion)** — **CLOSED at PM decomposition Step 2 PASS 7 (2026-06-29)** via TickComponent-prologue relocation authored in sub-GDD §4 F-PROLOGUE (single-source `raw_dt = FApp::GetDeltaTime()` and `effective_dt = clamp(raw_dt, 0.0f, MAX_SLIP_DT_S)` before both F-2 SLIPPING-gate and F-6 SETTLED-active-tail consume the value). Caller-side (mechanics F-2 + F-6) references updated to consume the platform-defined `effective_dt`. R12 fresh-context reviewer verifies the closure via `player-movement-mechanics.md` §4 F-2 + F-6 prose + `player-movement-platform.md` §4 F-PROLOGUE + Migration Plan cross-references (no `[R12a-PENDING]` marker on B-F6-3 in either sub-GDD; the marker was removed at PASS 7). See `player-movement-mechanics-r12-author-brief-2026-06-28.md` §2 for the mechanics-side CLOSED framing.
- **Cross-system propagation to Wave Spawner / Input System / HUD GDDs**: forward contracts documented in sub-GDD §6 Bidirectional Notes. Cross-system revisions triggered by DR-* closure land as `/propagate-design-change` runs after R12 APPROVED.
- **Polish-phase items**: canonical UE 5.7 framerate-floor mechanism final selection, named min-spec device list finalization, Polish-phase device audit executing AC-HW-B calibration + AC-HW-A/C verification. R12a produces the framework + records pre-R12a assumptions; Polish executes.
- **Platform implementation code** — Sprint deliverable, not R12a scope. R12a produces the design contract; implementation ADR (if any) comes later.
- **OQ-7 yaml→C++ generator pipeline** (registry→code drift closure for MIN_ESCAPE_SLIPS = 2 static_assert per AC-SS-E) — filed as OQ-7 in `player-movement-platform.md` §9. Independent of R12a decisions; landed at Sprint delivery or later.

---

## 3. Verification approach

R12 fresh-context `/design-review design/gdd/player-movement-platform.md` performed in a `/clear` session AFTER this brief's DR-* decisions are authored into the sub-GDD. Reviewer weights:

- **Every `[R12a-PENDING]` marker resolved** — grep the sub-GDD post-R12a; markers should be absent (or explicitly deferred with a documented traceable rationale).
- **B-F6-3 CLOSED framing survives grep gate**: `player-movement-platform.md` §4 F-PROLOGUE `effective_dt` definition prose + `player-movement-mechanics.md` §4 F-2 prologue consumer prose + F-6 SETTLED-active-tail consumer prose are word-consistent (single-source contract holds).
- **DR-PLAT-TEST harness pattern**: AC-21 + AC-SS-A test-setup narrative references `tests/automation-cpp/` harness pattern by exact name; test-strategy preamble enumerates all R11a-added private members.
- **DR-PLAT-HW-B methodology + tolerance**: §4 F-HW-B-MEASUREMENT names the chosen option (a/b/c); AC-HW-B setup references the pre-Polish calibration date + device names; tolerance value is either 0.5 ms (baseline held) or the empirically-widened value with citation.
- **DR-PLAT-SS-C early-out semantics**: AC-SS-C setup + expected result explicitly document watchdog-pumps-in-early-out (or Fatal-in-Shipping if option (b) chosen); §4 F-PROLOGUE mirror-references the semantic.
- **DR-PLAT-HAPTIC-API cross-sub-GDD lockstep**: `IHapticDispatch::IsSystemHapticsEnabled()` API name matches byte-for-byte between platform §3 declaration and presentation §3 gate-check call-sites (SlipConfirmed / BufferDrop / NearMiss). AC-CERT-2 (presentation §8) enumerates all 3 dispatch sites.
- **DR-PLAT-FLASH-API cross-sub-GDD lockstep**: `IGameSettings::IsCommitmentTellFlashEnabled()` API name matches byte-for-byte between platform §3 declaration and presentation §3 `LeadingFaceFlash` gate-check. AC-CERT-1 (presentation §8) references the platform API by exact name; OS-level override composition rule documented in both sub-GDD Bidirectional Notes.
- **§8 AC count consistency**: post-R12a platform AC count reflects 3 direct DR-* decisions (DR-PLAT-TEST refines AC-21/SS-A; DR-PLAT-HW-B refines AC-HW-B; DR-PLAT-SS-C refines AC-SS-C). API-side DR-*s (DR-PLAT-HAPTIC-API + DR-PLAT-FLASH-API) may not add platform ACs; verification lives presentation-side.
- **Forecast honesty**: if R12 exceeds 3 BLOCKING, invoke plan §10 decomposition-trigger-per-sub-GDD (`>5 BLOCKING`). If R12 returns 0 BLOCKING with the platform sub-GDD demonstrably calibrated (all DR-* closed with traceable rationale), platform reaches APPROVED and ADR-0009 (PM hosting) unblocks for the platform-scope surface.

---

## 4. Reference chain

- **Decomposition plan**: `design/gdd/reviews/player-movement-decomposition-plan-2026-06-16.md` §3.3 (platform scope) + §5 (BLOCKING assignment matrix — 3 direct + 1 shared) + §7.3 (this brief's outline authority).
- **Monolith history**: `design/gdd/player-movement.md` (R11 review at `design/gdd/reviews/player-movement-review-log.md` — R11 entry) + R10 brief `design/gdd/reviews/player-movement-r10-author-brief-2026-06-11.md` + R11 brief `design/gdd/reviews/player-movement-r11-author-brief-2026-06-15.md` (format precedent).
- **Sub-GDD authoritative text**: `design/gdd/player-movement-platform.md` (Step 2 PASS 4–8 authored; PASS 7 F-PROLOGUE B-F6-3 closure) + Step 3 forward-contracts `design/gdd/reviews/player-movement-decomposition-step3-verification-2026-06-30.md` (verification of §6 subsection) + 2026-07-01 Step 3 execution (this session's mechanics §6 line 1042–1064 + presentation §6 line 392–411 + platform §6 line 453–473 authoring).
- **Cross-sub-GDD lockstep briefs** (same-day 2026-07-01):
  - `player-movement-mechanics-r12-author-brief-2026-06-28.md` (mechanics DR-* + B-F6-3 CLOSED framing on caller side)
  - `player-movement-presentation-r12-author-brief-2026-06-28.md` (presentation DR-* + call-site lockstep to DR-PLAT-HAPTIC-API + DR-PLAT-FLASH-API)

---

**Brief authored by**: PM decomposition Step 4 execution session (2026-07-01) per plan §6 Step 4 authority.
**Brief authority**: binding for the R12a author revision pass on `player-movement-platform.md`. Author may override any option enumeration or CD recommendation with documented rationale recorded in the sub-GDD's header decision block.
**Brief execution gate**: R12a authoring occurs in a `/clear` fresh-context session AFTER this brief is durable on disk. R7 same-session-bias precedent applies — do not fold R12a authoring into the same session that produced the brief.

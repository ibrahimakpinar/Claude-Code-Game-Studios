# PM-Presentation R12a Scoped Revision Brief

**Date**: 2026-07-01 (filename dated 2026-06-28 to match sub-GDD `[R12a-PENDING]` pointer references authored during Step 2 PASS 4–8; internal Date field reflects actual authoring session)
**Target document**: `design/gdd/player-movement-presentation.md` (post-decomposition; currently at Step 3 forward-contracts COMPLETE 2026-07-01)
**Authoring mode**: scoped author revision (NOT in-session patch — CD-mandated handoff per R7 same-session-bias precedent)
**Authority**: PM decomposition plan §7.2 (`design/gdd/reviews/player-movement-decomposition-plan-2026-06-16.md`) + R11 BLOCKING assignment matrix §5
**Pre-revision gate**: PM decomposition Steps 1–3 COMPLETE (structural split + monolith-lift content + cross-sub-GDD forward contracts). Step 4 R12a briefs authored 2026-07-01 (this document). Step 5 systems-index PM row split queued same session.

---

## 0. Why this brief exists

The PM monolith `design/gdd/player-movement.md` R11 fresh-context re-review (2026-06-16) returned **11 BLOCKING** — exceeding the CD-set decomposition trigger (`>8`). CD recommended decomposition into 3 sub-GDDs by failure-domain seams. Presentation sub-GDD inherits **5 R11 BLOCKING items** (per decomposition plan §5): B-BANNER-1, B-CERT-1, B-CERT-2 (call-site portion — API side belongs to platform), B-AUDIO-1, B-AUDIO-2.

This brief is the scoped R12a author handoff for the presentation sub-GDD. It enumerates the DR-* author decisions the R12a pass must close, cites the extensive `[R12a-PENDING]` context already embedded in `player-movement-presentation.md` (authored during Step 2 PASS 4–8 to preserve monolith prose lift + pre-R12a fallback behavior), and sets a calibrated R12 fresh-review forecast.

**Format discipline**: this brief is **light-with-pointers** by design. The sub-GDD already carries the full defect context, option enumeration, pre-R12a fallback behavior, and forward-contract implications for each DR-* — in the EC-18 / EC-19 subsections + Tuning Knobs rows + Bidirectional Notes + Cross-System Interface Table. Duplicating that content in this brief would create a divergence-risk against the sub-GDD authoritative text. The brief's job is to enumerate what R12a MUST decide + where the decision lands + what verification the decision requires. Rationale confirmed by pre-authoring advisor round (2026-07-01).

**R12 fresh-review forecast**: **1–3 BLOCKING** (single-domain sub-GDD; per plan §7.2 calibrated forecast band). Decomposition trigger per sub-GDD: `>5 BLOCKING` (tighter than the monolith's `>8` — single-domain doc should converge cleanly).

**Validation criteria (CD-set)** — R12 succeeds if:
- BLOCKING count within 1–3 forecast band.
- All 5 inherited R11 BLOCKINGs have documented closure paths in the sub-GDD (via DR-* decisions authored below).
- Every `[R12a-PENDING]` marker in `player-movement-presentation.md` is either resolved (decision authored, marker removed) or explicitly deferred with an updated fallback-behavior stance and traceability trail.
- No new failure-domain surface appears (a 4th BLOCKING cluster within presentation scope would trigger further scope tightening).

---

## 1. In-scope R12a author decisions (5 DR-*)

Each decision below carries: the R11 BLOCKING it closes, the sub-GDD landing site(s) where the decision must be authored, the option enumeration (already embedded in the sub-GDD — cross-referenced here for locator convenience, not duplicated), the domain authority the decision requires, and CD recommendations where applicable.

### 1.1 DR-PRES-VOCAB — Banner copy vocabulary (closes B-BANNER-1)

**Defect**: Pre-R11a banner copy relied on unauthored HUD GDD vocabulary + "suppressed" dev jargon (Pillar-5 honesty violation). R11a-9 locked copy to "Performance mode — hardest barrage suppressed." — but "barrage" is not yet established as player-facing vocabulary (the HUD GDD is unauthored).

**Author decision required**: select one of the following vocabulary paths:
- **(a) Revert to "waves"** — already established as player-facing in `game-concept.md` + `pull-wave-behavior.md`. Copy becomes "Performance mode — hardest waves disabled." **CD recommendation.**
- **(b) Keep "barrage"** — pushes a BLOCKING forward contract to the HUD GDD to establish "barrage" as player-facing vocabulary on first authoring. Higher coupling risk.
- **(c) Mark DR-D.1 OPEN** pending HUD GDD authoring; ship provisional copy (a) as pre-HUD placeholder.

**Sub-GDD landing sites**:
- §3 Hardware-Performance Banner subsection (`player-movement-presentation.md` lines ~110–140 — banner copy locked-text line).
- Cross-System Interface Table row for HUD GDD (banner copy forward-contract cell).
- AC-BANNER-1 (§8) if a banner-copy-vocabulary AC exists post-R11a (verify against sub-GDD §8 before R12a).

**Domain authority**: UX-designer + narrative-director (co-sign — narrative owns player-facing vocabulary consistency; UX owns player-visible copy).

**Post-decision propagation**: if (a) chosen, the HUD-GDD forward contract downgrades from BLOCKING to informational (banner copy authored PM-side; HUD GDD inherits at first authoring). If (b) chosen, HUD GDD's first authoring session opens with a BLOCKING "author 'barrage' as player-facing vocabulary" item. Update Bidirectional Notes (§6) to reflect the chosen path.

### 1.2 DR-PRES-FLASH — Reduce-motion / flash-disable accessibility setting (closes B-CERT-1)

**Defect**: Pre-R12a commitment-tell flash (`LeadingFaceFlash` — ±0.80 luminance, 2-frame hold, 200 ms cadence cap) dispatches unconditionally. No player-controllable reduce-motion setting exists — App Store + Google Play accessibility cert reviewers can flag uncontrollable flash effects.

**Author decision required**:
- **Setting name**: `commitment_tell_flash_enabled` (working name from EC-18) OR `reduce_motion` (broader semantic — could gate other tells too). Choose scope.
- **Default state**: `true` (current design intent preserved on opt-out) OR `false` (opt-in). **CD recommendation: default = `true`** (preserve design intent for players who don't need the setting; the setting exists for players who would otherwise be excluded — parallel to the `near_miss_haptic_enabled` default-off framing established at R11a-12).
- **User-facing copy**: draft the setting label + one-sentence explanation. Accessibility-specialist co-sign at Polish gate; R12a produces a STARTING DRAFT.
- **Dispatch gate site**: within the cadence-cap gate at `LeadingFaceFlash` dispatch (analogous to `IGameSettings::IsNearMissHapticEnabled()` gate pattern already in the sub-GDD). Do NOT gate the mechanical SETTLED→SLIPPING transition or the `commitment_tell_fire_count` counter increment — only the visual rendering.

**Sub-GDD landing sites**:
- §3 Commitment-Tell (`player-movement-presentation.md` — cadence-cap gate description).
- §5 Edge Cases — EC-18 (already authored; R12a resolves the PENDING marker).
- §7 Tuning Knobs — `commitment_tell_flash_enabled` row (already authored PENDING; fill in the R12a chosen defaults + copy draft).
- §8 Acceptance Criteria — author AC-CERT-1 (currently referenced as "R12a NEW; ADVISORY-pre-Polish, BLOCKING-at-Polish").
- §6 Bidirectional Notes — cross-references to platform API (`IGameSettings::IsCommitmentTellFlashEnabled()`) + HUD/Accessibility Settings GDD forward contract.

**Domain authority**: accessibility-specialist + UX-designer (co-sign).

**Cross-sub-GDD forward contract**: platform sub-GDD `player-movement-platform.md` §3 Public Interface (Platform-Side) inherits `IGameSettings::IsCommitmentTellFlashEnabled()` settings-bridge API — see DR-PLAT-FLASH-API in `player-movement-platform-r12-author-brief-2026-06-28.md`. Lockstep authoring required.

### 1.3 DR-PRES-HAPTIC-GATE — Haptic OS-state gate (closes B-CERT-2 call-site portion)

**Defect**: Pre-R12a PM haptic dispatch sites (`EHapticEvent::SlipConfirmed`, `EHapticEvent::BufferDrop`, `EHapticEvent::NearMiss`) call `IHapticDispatch::Fire()` without querying OS-level haptic-suppression state (iOS Focus modes: Work / Sleep / Driving / Do Not Disturb; Android Do Not Disturb; system-haptics disabled). Apple HIG + Android accessibility guidance require in-app haptic dispatch SHOULD respect the OS-level preference. App Store + Google Play cert risk.

**Author decision required**:
- **Gate composition rule**: platform's `IHapticDispatch::IsSystemHapticsEnabled()` OS-state query is AND-composed with any in-game accessibility toggle (currently only `near_miss_haptic_enabled` per R11a-12; DR-PRES-FLASH may add `commitment_tell_flash_enabled` under a similar pattern). Haptic fires iff **both** conditions evaluate true. Confirm this composition rule.
- **Gate site pattern**: every PM haptic dispatch site (SlipConfirmed / BufferDrop / NearMiss) queries `IHapticDispatch::IsSystemHapticsEnabled()` immediately before the `IHapticDispatch::Fire()` call. Document the gate as a required pattern in the sub-GDD (not per-site inline conditionals — a documented gate rule).
- **Verification against Apple HIG + Android accessibility guidance**: cite the specific HIG section + Android guidance URL in the AC-CERT-2 setup notes.

**Sub-GDD landing sites**:
- §3 Commitment-Tell + §3 Near-Miss Beat + wherever `EHapticEvent::BufferDrop` is dispatched (verify against Rule 3 buffer-full path in the sub-GDD).
- §5 Edge Cases — EC-19 (already authored; R12a resolves the PENDING marker).
- §8 Acceptance Criteria — author AC-CERT-2 (referenced as "R12a NEW; ADVISORY-pre-Polish, BLOCKING-at-Polish").
- §6 Bidirectional Notes — cross-references to platform API + Input System §Haptic Vocabulary co-ownership framing.

**Domain authority**: accessibility-specialist + audio-director + UX-designer (co-sign).

**Cross-sub-GDD forward contract**: platform sub-GDD §3 Public Interface (Platform-Side) inherits `IHapticDispatch::IsSystemHapticsEnabled()` cross-platform query API — see DR-PLAT-HAPTIC-API in `player-movement-platform-r12-author-brief-2026-06-28.md`. Input System GDD §Haptic Vocabulary inherits the query API contract on first revision. Lockstep authoring across presentation + platform required.

### 1.4 DR-PRES-DUCK — Ducking reverse-trigger-order resolution (closes B-AUDIO-1)

**Defect**: AC-AUDIO-CUE-DUCKING trigger-order asymmetry — the R10a §2.9 / §5.4 ducking carve-out specifies "slip ducked under near-miss" only for the forward trigger order (near-miss called during active slip). The reverse case (near-miss swell in mid-play when a fresh slip commits) is undefined at R11a. Pillar-5 (commitment-honesty) violation in worst-case interpretation.

**Author decision required**: select one of:
- **(a) Begin pre-ducked** at `authored − 6 dB` and ramp to authored level after the swell ends.
- **(b) Begin at authored level** and ramp down to `authored − 6 dB` via the 50 ms attack envelope.
- **(c) Fully suppress** for the swell's remaining duration.
- **(d) Play at authored level un-ducked** (near-miss read tolerates the slip overlap because the slip is shorter). **Pre-R12a fallback** — sub-GDD ships EC-17 with this stance until DR-PRES-DUCK closes.

**Sub-GDD landing sites**:
- §3 Audio > Overlapping-cues hierarchy (`player-movement-presentation.md` — ducking carve-out description).
- §5 Edge Cases — EC-17 (already authored; R12a resolves the PENDING marker).
- §8 Acceptance Criteria — AC-AUDIO-CUE-DUCKING (add Setup D covering the chosen reverse-order behavior).

**Domain authority**: audio-director (domain authority).

**Post-decision propagation**: audio implementation MetaSound graph reflects the chosen behavior. Update Bidirectional Notes (§6) Audio System row to reflect the ducking behavior finalized.

### 1.5 DR-PRES-RAMP — HARD-CUT ramp shape (closes B-AUDIO-2)

**Defect**: R11a triple-overlap resolution table specifies "≤ 5 ms ramp" but does not specify the gain curve. Linear ramp on a percussive whoosh cue can produce audible click artefacts on some hardware.

**Author decision required**: select one of:
- **(a) Linear** gain ramp — simplest to implement, click risk on percussive content.
- **(b) Raised-cosine** gain ramp — click-free on percussive content, marginally more expensive. **CD recommendation.** Sub-GDD ships §3 with raised-cosine as pre-R12a fallback per authored note.

**Sub-GDD landing sites**:
- §3 Audio > Triple-overlap resolution table (`player-movement-presentation.md` — ramp shape row).
- §5 Edge Cases — (if a triple-overlap EC exists; verify) — R12a marker.
- §8 Acceptance Criteria — verify no AC currently asserts ramp shape; if not, add AC-AUDIO-RAMP-SHAPE (or fold into AC-AUDIO-CUE-DUCKING as a check) asserting the chosen curve.

**Domain authority**: audio-director (domain authority).

**Post-decision propagation**: MetaSound triple-overlap resolution graph adopts the chosen ramp shape.

---

## 2. Out of scope for R12a (presentation)

- **B-F6-3 (F-6 effective_dt SETTLED-path leak)** — **CLOSED at PM decomposition Step 2 PASS 7 (2026-06-29)** via TickComponent-prologue definition-site relocation to `player-movement-platform.md` §4 F-PROLOGUE. Presentation is not affected — the fix lives entirely in mechanics + platform.
- **Cross-system propagation to HUD GDD / Input System GDD / Wave Spawner GDD**: forward contracts documented in sub-GDD §6 Bidirectional Notes. Cross-system revisions triggered by DR-* closure land as `/propagate-design-change` runs after R12 APPROVED.
- **Polish-phase items**: accessibility-specialist final copy sign-off + audio-director MetaSound implementation + IEC PEAT flash cadence empirical validation at Polish gate. R12a produces STARTING DRAFTS + AC framework; Polish executes.
- **Presentation implementation code** — Sprint deliverable, not R12a scope. R12a produces the design contract; ADR-0009 (PM hosting) + implementation ADR (if any) come later.

---

## 3. Verification approach

R12 fresh-context `/design-review design/gdd/player-movement-presentation.md` performed in a `/clear` session AFTER this brief's DR-* decisions are authored into the sub-GDD. Reviewer weights:

- **Every `[R12a-PENDING]` marker resolved** — grep the sub-GDD post-R12a; markers should be absent (or explicitly deferred with a documented traceable rationale).
- **DR-PRES-VOCAB consistency**: banner copy in §3 matches the copy in the HUD-GDD forward-contract cell in §6 Cross-System Interface Table matches the AC-BANNER-1 test setup expected-string.
- **DR-PRES-FLASH cross-sub-GDD lockstep**: `IGameSettings::IsCommitmentTellFlashEnabled()` API name matches between presentation §3 gate-check + platform `player-movement-platform.md` §3 Public Interface (Platform-Side). AC-CERT-1 setup in presentation §8 references the platform API by exact name.
- **DR-PRES-HAPTIC-GATE cross-sub-GDD lockstep**: `IHapticDispatch::IsSystemHapticsEnabled()` API name matches between all haptic dispatch sites (3 sites) in presentation + platform API declaration. AC-CERT-2 setup enumerates all 3 dispatch sites.
- **DR-PRES-DUCK + DR-PRES-RAMP audio-director sign-off traceability**: AC test setups cite the audio-director decision date + brief version.
- **§8 AC count consistency**: post-R12a AC count matches DR-* decisions authored (5 DR-*s → up to 5 new/updated ACs; some may fold into existing AC setups).
- **Forecast honesty**: if R12 exceeds 3 BLOCKING, invoke plan §10 decomposition-trigger-per-sub-GDD (`>5 BLOCKING`). If R12 returns 0 BLOCKING with the presentation sub-GDD demonstrably calibrated (all DR-* closed with traceable rationale), presentation reaches APPROVED and ADR-0009 (PM hosting) unblocks for the presentation-scope surface.

---

## 4. Reference chain

- **Decomposition plan**: `design/gdd/reviews/player-movement-decomposition-plan-2026-06-16.md` §3.2 (presentation scope) + §5 (BLOCKING assignment matrix — 5 rows to presentation) + §7.2 (this brief's outline authority).
- **Monolith history**: `design/gdd/player-movement.md` (R11 review at `design/gdd/reviews/player-movement-review-log.md` — R11 entry) + R10 brief `design/gdd/reviews/player-movement-r10-author-brief-2026-06-11.md` + R11 brief `design/gdd/reviews/player-movement-r11-author-brief-2026-06-15.md` (format precedent).
- **Sub-GDD authoritative text**: `design/gdd/player-movement-presentation.md` (Step 2 PASS 4–8 authored) + Step 3 forward-contracts `design/gdd/reviews/player-movement-decomposition-step3-verification-2026-06-30.md` (verification of §6 subsection) + 2026-07-01 Step 3 execution (this session's mechanics §6 line 1042–1064 + presentation §6 line 392–411 + platform §6 line 453–473 authoring).
- **Cross-sub-GDD lockstep briefs** (same-day 2026-07-01):
  - `player-movement-mechanics-r12-author-brief-2026-06-28.md` (mechanics DR-* + B-F6-3 CLOSED framing)
  - `player-movement-platform-r12-author-brief-2026-06-28.md` (platform DR-* + B-F6-3 CLOSED framing + API-side lockstep to DR-PRES-FLASH + DR-PRES-HAPTIC-GATE)

---

**Brief authored by**: PM decomposition Step 4 execution session (2026-07-01) per plan §6 Step 4 authority.
**Brief authority**: binding for the R12a author revision pass on `player-movement-presentation.md`. Author may override any option enumeration or CD recommendation with documented rationale recorded in the sub-GDD's header decision block.
**Brief execution gate**: R12a authoring occurs in a `/clear` fresh-context session AFTER this brief is durable on disk. R7 same-session-bias precedent applies — do not fold R12a authoring into the same session that produced the brief.

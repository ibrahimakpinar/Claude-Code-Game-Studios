# PM R11 Scoped Revision Brief

**Date**: 2026-06-15
**Target document**: `design/gdd/player-movement.md` (currently at R10 NEEDS REVISION post-R10a)
**Authoring mode**: scoped author revision (NOT in-session patch — CD-mandated handoff per R7 same-session-bias precedent)
**Authority**: CD synthesis on PM R10 fresh-context re-review (`design/gdd/reviews/player-movement-review-log.md` 2026-06-15 entry)
**Pre-revision gate**: none — Clusters can be addressed independently in any order; §1 and §2 are PRIMARY and should land first

---

## 0. Why this brief exists

PM R10 fresh-context re-review returned **19 BLOCKING in 7 failure clusters** against R10a's forecast of 0-4 BLOCKING (off by ~4-5x). Convergence trajectory: 17→32→28→23→21→3→0→4→26→22+1→**19 (R10)**. R10a executed substantially — 22 R9 BLOCKING + 1 R10-lean BLOCKING closed, 19 new ACs added, F-6 + §1.2 + §1.3 restructured — but the failure surface is genuinely cross-cutting (audio identity, UX safe-areas, watchdog correctness, anti-snap coherence are new classes the R10a brief did not scope), plus 3 triple-specialist-convergence items in Cluster A that the brief targeted but missed (text-survival from pre-R10a F-2 + AC-21).

CD ruling:

> "MAJOR REVISION NEEDED — hand back to author via R11a scoped brief (R10→R11a pattern, NOT in-session revision). R7 in-session-bias risk applies HERE MORE THAN at R10a — the brief execution model itself missed Cluster A text survival. Risk is higher because reviewer + author co-located now with 19 BLOCKING in active context would recreate the R7 failure mode."

This brief is the scoped handoff. **R11a should treat each cluster as a coordinated edit**, not 19 separate point patches. Within each cluster, items share root cause and benefit from being addressed together.

**R11 fresh-review forecast**: **3-8 BLOCKING** (widened from R10a's 0-4 because R10a forecast was off 4-5x and new surface is more cross-cutting UX/audio/cert).

**Decomposition trigger**: if R11 returns >8 BLOCKING, CD recommends **GDD decomposition** (split player-movement.md into sub-GDDs by failure domain) rather than another revision round. The author should treat this as a warning: if R11a discovers a cluster genuinely cannot be resolved at the GDD's current scope, surface this BEFORE shipping R11a rather than discovering it at R11.

**Validation criteria (CD-set)** — we'll know R11 succeeded if:
- BLOCKING count within 3-8 forecast band.
- Cluster A does not recur (the canary — if pre-R11a text survives anywhere it shouldn't, the brief execution model is broken and we need a different revision pattern).
- No new domain surfaces appear (a 4th BLOCKING domain cluster would trigger decomposition recommendation).

---

## 1. §1 — Cluster A: Internal Contradiction (PRIMARY — DO FIRST)

**Defect (triple-specialist convergence: systems-designer + qa-lead + gameplay-programmer)**: Pre-R10a text survived in three locations despite R10a explicitly retiring the semantic.

**Three locations with the contradiction**:
1. **F-2 pseudo-code (lines 618-622)** — still contains:
   ```cpp
   if (SLIP_TWEEN_DURATION_S < 0.001f && !bFloorGuardFired) {
       SLIP_TWEEN_DURATION_S = 0.15f;
       bFloorGuardFired = true;
   }
   ```
   This is the pre-R10a one-shot reset-to-0.15 with `bFloorGuardFired` flag.

2. **AC-21 body (line 1244)** — still reads:
   > "Given `SLIP_TWEEN_DURATION_S = 0.0` at BeginPlay, when a slip begins, F-2 logs an error and resets `SLIP_TWEEN_DURATION_S` to 0.15."

3. **§1.2 enforcement table (line 540) + AC-21 fold-in (line 548) + AC-SS-A (line 552)** — assert the NEW persistent-clamp semantic: "clamp to nearest bound, NOT reset to 0.15"; "`bFloorGuardFired` semantics are RETIRED — no one-shot flag exists."

**Cascade**: AC-SS-A test setup (`SLIP_TWEEN_DURATION_S = 0.20`, ceiling violation) is unimplementable against the current F-2 prologue (only guards `< 0.001f`, never catches ceiling violations).

**Required fix scope (coordinated edit)**:

1. **Rewrite F-2 pseudo-code (lines 618-622)** to implement the persistent clamp:
   ```cpp
   // Persistent clamp: every tick, clamp to nearest safe-range bound.
   // bFloorGuardFired semantics RETIRED per §1.2.
   float effective_slip_tween = SLIP_TWEEN_DURATION_S;
   if (effective_slip_tween < 0.10f) {
       effective_slip_tween = 0.10f;
       bSlipTweenClampActive = true;
       if (++TickClampLogCounter >= 600) {  // rate-limited: 1 log per 10s at 60fps
           UE_LOG(LogPlayerMovement, Error, TEXT("SLIP_TWEEN_DURATION_S=%.4f below safe floor 0.10; persistently clamped"), SLIP_TWEEN_DURATION_S);
           TickClampLogCounter = 0;
       }
   } else if (effective_slip_tween > 0.15f) {
       effective_slip_tween = 0.15f;
       bSlipTweenClampActive = true;
       if (++TickClampLogCounter >= 600) {
           UE_LOG(LogPlayerMovement, Error, TEXT("SLIP_TWEEN_DURATION_S=%.4f above safe ceiling 0.15; persistently clamped"), SLIP_TWEEN_DURATION_S);
           TickClampLogCounter = 0;
       }
   } else {
       bSlipTweenClampActive = false;
       TickClampLogCounter = 0;
   }

   effective_dt   = clamp(DeltaTime, 0.0f, MAX_SLIP_DT_S);
   TweenProgress += effective_dt / effective_slip_tween;
   TweenProgress  = min(TweenProgress, 1.0f);
   ```
   Note: divides by `effective_slip_tween`, not `SLIP_TWEEN_DURATION_S`, so the clamp is load-bearing.

2. **Rewrite AC-21 body (line 1244)** to match the new semantic:
   > "AC-21: Given `SLIP_TWEEN_DURATION_S = 0.09` (below safe-range floor 0.10), when 10 consecutive ticks occur, then on every tick the effective tween duration is clamped to 0.10 (NOT reset to 0.15 — clamp preserves operator intent per §1.2); `bSlipTweenClampActive == true` for all 10 ticks; F-2 logs an error on tick 1 and on tick 601 (per the rate-limited log per AC-SS-A). No crash, no divide-by-zero. The pre-R10a one-shot `bFloorGuardFired` semantic is RETIRED — no one-shot flag exists. See AC-SS-A for the 100-tick ceiling-violation case (`SLIP_TWEEN = 0.20`)."

3. **Verify AC-SS-A (line 552) and AC-SS-A's enforcement-table row (line 540)** describe the same behavior as the rewritten F-2 + AC-21. Cross-reference all three locations explicitly in the GDD so future revisions can spot drift.

4. **Add a new member declaration to PM's public/private contract**: `bSlipTweenClampActive` (bool, public read-only) + `TickClampLogCounter` (int32, private). Document under Public Interface alongside the other diagnostic flags.

**Acceptance for §1**: a fresh-context re-reviewer reading F-2 pseudo-code + AC-21 body + AC-SS-A + §1.2 enforcement table should see the same semantic everywhere. Triple-grep for "bFloorGuardFired" — should return zero matches (it must not appear anywhere except a "RETIRED — see §1.2" marker if at all). Triple-grep for "resets `SLIP_TWEEN_DURATION_S` to 0.15" — zero matches.

---

## 2. §2 — Cluster E: F-6 Implementation Pathway (PRIMARY — DO SECOND)

**Defect (gameplay-programmer)**: Two related items.

**E.1 — `HandleStateChanged` COMPLETE/ABORTED dispatch missing F-6 reset (line 277-285)**:

Current dispatch body for COMPLETE/ABORTED calls `ResetLeanOutputs()` + `SetMeshRelativeRotationToZero()` but does NOT reset `edge_absorb_active` or `edge_absorb_local_timer`. Per the F-6 terminal-state reset semantics table (line 822-828), both COMPLETE and ABORTED must reset these to `false` / `0.0`. If F-6 is mid-tail when COMPLETE/ABORTED fires, the timer and flag persist into the next-run SETTLED state — F-6 will keep contributing to lean outputs after the run ends, violating Rules 8/9 (lean angles = 0 after snap).

**Required fix**:

Add two lines after `ResetLeanOutputs()` in the COMPLETE/ABORTED case body:

```cpp
case ERSMState::COMPLETE:
case ERSMState::ABORTED:
    lateral_world_position = LaneWorldX(target_lane);
    current_lane           = target_lane;
    TweenProgress          = 0.0f;
    ResetLeanOutputs();
    edge_absorb_active      = false;   // NEW per F-6 terminal table
    edge_absorb_local_timer = 0.0f;    // NEW per F-6 terminal table
    movement_state          = ERunSlipState::SETTLED;
    SetMeshRelativeRotationToZero();
    break;
```

Also add the same two lines to the COUNTDOWN case body (lines 247-258) and the IDLE case body (lines 296-308) for parity with the F-6 terminal table. The DEAD case correctly preserves F-6 state per Rule 7 — leave it alone.

Add a new AC: **AC-COUNTER-F6-RESET**: Given F-6 active mid-tail (`edge_absorb_active == true`, `edge_absorb_local_timer == 0.135`), when RSM transitions to COMPLETE, then on the same frame `edge_absorb_active == false` and `edge_absorb_local_timer == 0.0`. Mirror case for ABORTED. Sampled 10 ticks after entry: both remain false / 0.0. Verifies the dispatch body matches the F-6 terminal table.

**E.2 — `HandleSlipTransition` is referenced but never declared (line 811)**:

The §5.1 (a) Override pseudo-code at lines 810-816 places the F-6 kill logic "Inside `HandleSlipTransition` (SETTLED → SLIPPING entry path)." This method appears nowhere else in the GDD: not in Public Interface, not in Cross-Component Interfaces, not in `HandleStateChanged` body, not in any helper-method note. There is no declaration, signature, or call site. The §5.1 (a) Override — the PRIMARY R10a §1.1 decision — is attached to a phantom method.

**Required fix**:

Add a new subsection under Cross-Component Interfaces titled `### Slip Input Dispatch — HandleSlipTransition` (placement: between Delegate Handler Bodies and Death Replay Registration Order). Specify:

1. **Method signature**:
   ```cpp
   void UPlayerLaneMovementComponent::HandleSlipTransition(ESlipDirection InDirection);
   ```

2. **Call site**: invoked from the input-event handler (the binding mechanism resolved by OQ-3 / Input System OQ-1; currently unbound, but PM's input-event handler will route here). Called once per accepted slip event.

3. **Body specification** (authoritative pseudo-code):
   ```cpp
   void UPlayerLaneMovementComponent::HandleSlipTransition(ESlipDirection InDirection)
   {
       // RSM gating (Rule 5)
       if (!RSMComponent->IsRunning() || RSMComponent->IsPaused() || RSMComponent->IsInResumeGrace()) {
           return;  // event discarded; not buffered (per Rule 5)
       }

       // SETTLED path: begin tween or fire edge-absorb
       if (movement_state == ERunSlipState::SETTLED) {
           const EPlayerLane intended_target = ComputeTargetLane(current_lane, InDirection);
           if (intended_target == current_lane) {
               // Rule 1 edge no-op
               FireEdgeAbsorb(InDirection);
               return;
           }
           // §5.1 (a) Override — kill F-6 tail if active before starting new tween
           if (edge_absorb_active) {
               edge_absorb_active      = false;
               edge_absorb_local_timer = 0.0f;
           }
           target_lane    = intended_target;
           TweenProgress  = 0.0f;
           movement_state = ERunSlipState::SLIPPING;
           CommitCollisionToTargetLane();    // Rule 2
           FireCommitmentTell();              // Rule 2 + commitment_tell_fire_count++
           return;
       }

       // SLIPPING path: buffer (Rule 3) or pre-validate edge (F-4)
       if (movement_state == ERunSlipState::SLIPPING) {
           if (has_queued_input) {
               // Buffer full — drop with haptic + audio (Rule 3)
               FireBufferDropHapticAndAudio();
               return;
           }
           const EPlayerLane projected_target = ComputeTargetLane(target_lane, InDirection);
           if (projected_target == target_lane) {
               // F-4 buffer pre-validation: edge no-op
               FireEdgeAbsorb(InDirection);
               return;
           }
           // Accept into buffer
           has_queued_input        = true;
           queued_input_direction  = InDirection;
       }
   }
   ```

4. **Helper-method declarations needed** (declare these as private members):
   - `EPlayerLane ComputeTargetLane(EPlayerLane source, ESlipDirection direction)` — F-1 helper
   - `void FireEdgeAbsorb(ESlipDirection direction)` — Rule 1 / F-4 path; activates F-6 with `edge_absorb_sign` from direction; increments `edge_absorb_trigger_count`
   - `void CommitCollisionToTargetLane()` — Rule 2 collision commit
   - `void FireCommitmentTell()` — Rule 2 commitment-tell + counter increment
   - `void FireBufferDropHapticAndAudio()` — Rule 3 haptic + audio sting

5. **§5.1 (a) Override pseudo-code (lines 810-816)** — replace with a reference: "The §5.1 (a) Override is implemented inline at the SETTLED-path edge-absorb-kill check in `HandleSlipTransition` above (lines [TBD] of revised pseudo-code). The author decision (`§5.1 = (a) Override`) remains binding."

**Acceptance for §2**: AC-F6-B (line 1292) becomes implementable against the dispatch body. AC-F6-A/C/D existing assertions hold. A fresh-context re-reviewer reading the §5.1 (a) decision can trace it to a declared method body.

---

## 3. §3 — Cluster B: Watchdog Specification Defects (3 items)

**B.1 — `TickDTRollingBuffer[60]` zero-init blinds watchdog at startup**

**Defect (performance-analyst BLOCKING + systems-designer + gameplay-programmer RECOMMENDED echoes)**: Zero-initialized buffer slots count as clean samples (`0.0f < 0.01818f` → not breaching). For the first ~60 frames (~1.0s wall-clock at 60fps), the watchdog is suppressed — exactly during the highest-hitch-exposure window (startup shader warm-up, asset streaming hitches, GC pressure from load-to-play transitions). AC-HW-A Setup A-F does not include a startup-init variant, so the watchdog correctness AC passes while the defect is live in production.

**Required fix (author chooses one)**:

(a) **Sentinel pre-fill**: at BeginPlay, fill `TickDTRollingBuffer` with `0.01667f` (nominal 60fps clean sample). Watchdog starts warm but calibrated correctly; first real hitch samples replace optimistic values and degrade the window naturally.

(b) **Sample-count gate**: track `TickDTSampleCount` integer; suppress breach evaluation until `TickDTSampleCount >= 60`. Adjust hysteresis-release `ContinuousCleanWindowTime` accumulator similarly.

Recommend (a) — simpler, single-line BeginPlay change, no per-tick branch. Document the BeginPlay init in the DT watchdog spec.

Add **AC-HW-A Setup G** (new): **startup zero-init coverage**. Inject 30 samples of `DT = 0.020s` (50fps) starting from t=0 of BeginPlay. Verify watchdog enters breach state by the 30th sample (not delayed by the buffer fill). Verifies the BeginPlay init is in place.

**B.2 — AC-HW-B math + units error (line 1387)**

**Defect (performance-analyst BLOCKING + qa-lead RECOMMENDED)**: "≥ 99% of frames have `DT ≤ 17.67ms` (= 60 fps ± 1 ms margin)" contains two distinct problems:
- 17.67ms = 1000/17.67 = 56.6 fps, NOT 60 fps. The label conflates a frame-time tolerance with a framerate tolerance.
- 1% of 3600 frames per 60s run = 36 hitches. Mobile gameplay targeting "sustained 60fps" usually targets <0.1% (3-4 per 60s), not 1%. The 1% figure allows 10× the hitch rate a player would perceive as acceptable.

**Required fix**: rewrite AC-HW-B pass criteria with:
- Unambiguous frame-time ceiling: choose `DT ≤ 16.67ms` for strict 60fps or `DT ≤ 17.17ms` for 60fps + 0.5ms measurement tolerance. State explicitly which.
- Hitch budget grounded in player perception: ≤ 0.1% of frames above ceiling (3-4 hitches per 60s session).
- Separate "steady-state framerate" pass criterion from "peak hitch tolerance" pass criterion.

Example rewrite:
> **Pass criteria (revised)**:
> - **Steady-state**: ≥ 99.9% of frames have `DT ≤ 16.67ms ± 0.5ms` (= 60 fps, exact; measurement tolerance allows for Unreal Insights sample jitter). Equivalent to ≤ 3-4 frames above ceiling per 60s session.
> - **Peak hitch tolerance**: NO single sample with `DT > 33.33ms` (= sub-30 fps tick — the watchdog secondary breach trigger). Any single hitch above 33.33ms automatically fails.
> - **Watchdog non-breach**: throughout the 60s session, `is_hw_performance_degraded == false` at all times (the watchdog itself is the safety valve; if the device cannot stay above the watchdog floor, the device is below min-spec).

**B.3 — In-flight M=3 PEAK during gate engagement window**

**Defect (performance-analyst)**: Wave Spawner gate "does NOT mid-flight-cancel in-progress waves" (line 508). AC-HW-C survivability promise (line 1399) — "Verify the player can survive PEAK density during the breach without facing an inescapable M=3 PEAK" — is unconditional. The spec cannot deliver unconditionally: an M=3 PEAK dispatched at the moment of watchdog breach onset is in-flight and will fire on the player on a degraded device. Pillar-5 honesty failure.

**Author decision required (DR-B.3)**: choose one:
- **(a) Documented caveat**: explicitly state "death is possible during the gate engagement window (one M=3 PEAK barrage may be in-flight at breach onset)." Pillar-5 honesty wins via disclosure. Update AC-HW-C pass criteria to reflect the caveat.
- **(b) Grace window**: Wave Spawner may not dispatch M=3 PEAK for N seconds after `OnHardwarePerformanceBreach(true)` fires. Requires forward-contract amendment on Wave Spawner GDD. N depends on max M=3 PEAK in-flight time at degraded framerate — likely 2.0-3.0s. Eliminates the gap but extends the forward contract on a not-yet-authored GDD.
- **(c) Survivability proof at degraded framerate**: prove that at the degraded framerate that triggers the watchdog (~50 fps), the player is still capable of escaping an M=3 PEAK barrage. Document the proof. If the proof holds, the survivability promise is conditional-but-achievable.

Recommend **(b)** because (a) breaks the Pillar-5-honest framing the banner is supposed to deliver and (c) is hard to prove cleanly at the input-latency boundary. (b) adds clean spec to the forward contract; the Wave Spawner GDD inherits the grace window as a BINDING contract on first authoring.

---

## 4. §4 — Cluster C: Audio Acceptance & Identity (5 items)

**C.1 — Playback-rate scaling breaks slip cue identity at ratio floor (audio-director BLOCKING)**

**Defect**: AC-AUDIO-CUE-PROPORTIONALITY (line 1350) specifies "runtime applies a playback-rate scale to hit the contracted duration." UE Metasounds' rate scaling shifts pitch proportionally. At `audio_cue_ratio = 0.70`, rate = 1/0.70 ≈ 1.43× → ~+6 semitone pitch shift on the whoosh. The 400-1200Hz slip cue at +6 semitones drifts into the 600-1600Hz near-miss band — cue identity destroyed. AC tests duration math, not timbre.

**Author decision required (DR-C.1)**: choose one:
- **(a) Constrain `audio_cue_ratio` safe range** to [0.79, 1.26] (perceptually equivalent to ±2 semitones at default SLIP_TWEEN, the "same cue" threshold). At the current SLIP_TWEEN safe range [0.10, 0.15], this still allows some duration variation.
- **(b) Specify pitch-locked time-stretch implementation**: requires offline batch export of multiple duration variants per cue (~3-5 assets) OR a MetaSounds graph with time-stretch (limited support for sub-200ms percussives in UE 5.6). Costs asset pipeline + memory.
- **(c) Cap variant explosion**: ship 2 master assets (one for SLIP_TWEEN=0.10 band, one for SLIP_TWEEN=0.15 band) and apply rate-scale only within each band (≤±15% rate, ≤±2.5 semitones). Author selects asset by SLIP_TWEEN value.

Recommend **(a)** — single-line spec change, no asset pipeline impact. Document the new safe range in the `audio_cue_ratio` Tuning Knob row. Update AC-AUDIO-CUE-PROPORTIONALITY to test ONLY within the new safe range.

**C.2 — AC-AUDIO-CUE-DUCKING vacuous at defaults**

**Defect (audio-director + qa-lead convergence)**: Slip cue (127.5ms default) ends before near-miss swell (200-300ms) finishes. "Slip cue remains ducked for swell duration" + "100ms release window" assertions are vacuously true on a cue that has already ended.

**Required fix**: rewrite AC-AUDIO-CUE-DUCKING with conditional test setup:
> **AC-AUDIO-CUE-DUCKING (revised)**: Verify that if `TriggerNearMissBeat()` is called within the first (audio_cue_duration_s − 100ms) of the slip cue's playback, the slip cue MUST be ducked by `-6 dB ± 0.5 dB` during the overlap window. Specify the test trigger timing explicitly: call `TriggerNearMissBeat()` at 25ms into a 127.5ms slip cue, verify ducking for the remaining 102.5ms. If `TriggerNearMissBeat()` is called AFTER the slip cue has already ended, this AC is vacuously satisfied — the ducking mechanism is verified only when there is an active slip cue. The 100ms release envelope is measured only if the slip cue is still active when the swell ends.

Also specify ducking mechanism: bus-level gain automation OR per-cue gain envelope. Bus-level is the default in UE Metasounds; document explicitly.

**C.3 — Triple-overlap (buffer-drop + slip + near-miss) undefined**

**Defect (audio-director)**: Priority hierarchy is bilateral. Triple overlap creates contradictory state on slip cue (simultaneously suppressed by buffer-drop AND ducked by near-miss).

**Required fix**: add a triple-overlap resolution table to the Audio subsection:

| Active cues | Behavior |
|---|---|
| buffer-drop only | buffer-drop plays at full |
| slip only | slip plays at full |
| near-miss only | near-miss plays at full |
| buffer-drop + slip | buffer-drop plays at full; slip is HARD-CUT at suppression onset (≤5ms ramp to preserve buffer-drop click clarity) |
| buffer-drop + near-miss | both play at full (near-miss is never ducked under any PM cue) |
| slip + near-miss | slip ducked −6dB / 50ms attack; near-miss plays at full |
| **buffer-drop + slip + near-miss** | **buffer-drop plays at full; slip is HARD-CUT (buffer-drop priority wins over near-miss ducking); near-miss plays at full. Net: buffer-drop + near-miss only.** |

Add a corresponding entry to Edge Cases section.

**C.4 — Center-pan slip variant schedule undefined**

**Defect (audio-director BLOCKING-or-author-resolvable)**: "Audio author's discretion — interleaved or retired, locked at polish." Headphone players (30-40% of mobile) hear two different cues for same event.

**Author decision required (DR-C.4)**: choose one:
- **(a) Center-pan variant replaces panned variant entirely** (simplest; eliminates headphone confusion; pan-as-aesthetic retired). RECOMMENDED.
- **(b) Panned variant primary; center-pan plays only on speaker-mode detection** (requires runtime audio-output detection — UE 5.6 supports via `IAudioMixerPlatformInterface::GetAudioRenderingDeviceType()` or similar). Adds runtime complexity.
- **(c) Document deterministic interleave schedule** (e.g., every 3rd slip uses center-pan) AND acknowledge headphone inconsistency in design.

Recommend **(a)**. Update Audio Locked Decision header to "Pan is removed entirely; all slip cues center-pan. Edge-absorb cue continues mono-front (no change). Pan-as-aesthetic is RETIRED."

**C.5 — AC-AUDIO-PAN-NEUTRALITY statistically invalid (qa-lead BLOCKING)**

**Defect**: N=40, 60% accuracy ceiling — 95% CI is [44%, 74%], indistinguishable from chance OR genuine signal. No significance criterion; "naive player" undefined.

**Required fix**: rewrite AC-AUDIO-PAN-NEUTRALITY:
> **AC-AUDIO-PAN-NEUTRALITY (revised, classification = Visual/Feel — manual evidence with statistical sign-off; ADVISORY gate)**: Verify that pan-absence does NOT carry direction information. Test setup:
> - **N ≥ 5 naive players** (defined: casual mobile gamer; no prior SLIPSTORM exposure; no professional audio background; recruited externally, not internal).
> - Each player completes **40 forced-choice trials**: identify each cue as "slipped" or "edge-absorbed" from audio alone, headphones, randomized order.
> - **Pass criterion**: aggregate accuracy across all players + trials is NOT statistically distinguishable from chance (50%) at p > 0.05 one-tailed binomial test. With N=5 players × 40 trials = 200 trials, the critical pass threshold is ≤ 113 correct (= 56.5%). At N=10 players × 40 trials = 400 trials, threshold is ≤ 220 (= 55%).
> - **Sign-off**: audio-director + UX-designer co-sign the test protocol + results.

If **C.4 = (a)** (retire panned variant), AC-AUDIO-PAN-NEUTRALITY may be redundant — the test exists to validate pan-neutrality of the center-pan variant against the mono edge-absorb cue, but if all slip cues are center-pan then there is no per-cue pan difference to test. Author should decide whether to retain or retire the AC alongside the C.4 decision.

---

## 5. §5 — Cluster D: UX / Accessibility / Platform Certification (4 items)

**D.1 — Banner copy is dev jargon**

**Defect (ux-designer)**: "Performance reduced — survivability adjustments active." Player has no model for "survivability adjustments." Pillar-5 honesty contract violated — banner cannot fulfill its design purpose ("tell the player their device is degraded") if the copy is uninterpretable.

**Author decision required (DR-D.1)**: choose one banner copy:
- **(a)** "Running in performance mode — difficulty adjusted automatically."
- **(b)** "Device running hot — game difficulty lowered."
- **(c)** Author-original copy that meets the standard: player-comprehensible + tells the player what changed (not just that "adjustments" happened) + non-alarming tone.

Recommend **(a)** — communicates the WHAT (difficulty adjusted) without alarming. Document the copy as binding in §5.3 (a). UX-designer co-signs.

**D.2 — Banner top-edge placement vs iOS/Android safe areas**

**Defect (ux-designer)**: 24px banner at top edge (y=0) is silently occluded on iPhone X+ (notch / Dynamic Island, 30-59pt safe-area top inset) and Android with status bar (24-28dp). AC-HW-C testing on a non-notched device passes while shipping silent failure on 60%+ of iOS fleet.

**Required fix**: amend the §5.3 (a) Banner spec:
> **Placement (revised)**: Banner renders below the system safe-area top inset on iOS (`safeAreaInsets.top`) and below the system bar window inset on Android (`WindowInsetsCompat.Type.systemBars()`). Banner is anchored to the top edge of the safe area, not the top edge of the screen.

Amend AC-HW-C: test apparatus MUST include a notched iOS device (e.g., iPhone 12 Pro or newer). Add explicit assertion: "Banner is fully visible below the safe-area top inset on a notched iOS device."

Add a forward contract to the HUD GDD (when authored): HUD inherits this safe-area binding as a BLOCKING layout constraint.

**D.3 — Commitment-tell flash cadence may exceed IEC PEAT photosensitivity threshold**

**Defect (ux-designer)**: At SLIP_TWEEN=0.10s safe-range floor with buffer-flush chains, commitment-tell can fire at 10 flashes/sec — well over IEC 3-per-second threshold. 100% white on dark = maximum luminance contrast. App Store + Google Play certification risk.

**Author decision required (DR-D.3)**: choose one:
- **(a) Cap commitment-tell at 3 fires per second**: gate subsequent fires if last tell ended less than 333ms ago. Add per-tick cooldown to the tell system.
- **(b) Reduce peak luminance** from 100% white to 70% white. Reduces flash luminance contrast below the PEAT criterion at any cadence. Trade-off: less visible commitment confirmation.
- **(c) Combination**: cap to 5 fires/sec AND reduce luminance to 80%. Allows higher cadence while staying under PEAT.

Recommend **(c)** as a middle ground. Verify against IEC 61966-2-2 PEAT criteria with the chosen combination; document the verification in the GDD. Add new AC: **AC-COMMIT-FLASH-CADENCE**: Given SLIP_TWEEN=0.10 and buffer-flush chain, verify commitment-tell fires at most N/sec and at ≤Y% peak luminance; result conforms to IEC PEAT criteria (≤3 flashes/sec at 100% contrast, OR reduced luminance allowing higher cadence).

**D.4 — Near-miss visual beat sub-perceptual + no haptic alternative**

**Defect (ux-designer)**: 2-3% Y-axis dip (4-12px on 6-inch portrait mobile, 33ms onset) is sub-perceptual for deaf-in-speaker-mode players (GDD line 42 acknowledges this player class). No haptic alternative. Pillar-5 confirmation absent.

**Author decision required (DR-D.4)**: choose one of three resolution paths:
- **(a) Enlarge visual beat**: increase Y-dip to 5-7% body displacement; increase onset to 66ms (4 frames); add a brief brightness pulse on avatar's torso voxel (10% above ambient) for visual amplification. Makes the beat perceptible at mobile sizes.
- **(b) Opt-in haptic**: add a runtime accessibility setting "Near-Miss Haptic Feedback" (off by default to preserve current design intent for sighted hearing players; on by player choice). When on, near-miss fires a soft haptic pulse (sub-50ms, low amplitude, distinct from buffer-drop). Players with reduced visual acuity OR deaf-in-speaker-mode opt in.
- **(c) Pillar-5-honest disclosure**: explicitly document in the GDD that near-miss confirmation is inaudible-and-invisible for deaf-in-speaker-mode players; acknowledge this as a known cost; do not ship a fix. Pillar-5 honesty principle requires the disclosure if no fix is chosen.

Recommend **(b)** — preserves design intent (no-haptic for default players) AND closes accessibility gap (opt-in for players who need it). Accessibility-specialist (a separate specialist not consulted on PM R10) should sign off on the setting design.

---

## 6. §6 — Cluster F: Player Fantasy / Coherence (3 items, game-designer vision-class)

**F.1 — §5.1 (a) Override snap discontinuity contradicts anti-snap contract**

**Defect (game-designer)**: LEAN_CURVE_ASSET contract (lines 889, 896, 904-905) enforces `LeanCurve(0.90) ≈ 0.0` specifically to prevent head-snap — a 1-frame discontinuous lean-angle jump at tween completion. The rationale: "if LeanCurve(0.90) is non-zero, the head will visibly snap to neutral on every slip completion." §5.1 (a) Override (lines 810-816) introduces a structurally identical discontinuity: F-6 contributions (up to ~3.5° body/head/arm) are zeroed in a single frame. GDD simultaneously holds anti-snap as load-bearing AND violates it.

**Author decision required (DR-F.1)**: choose one:
- **(a) Apply a 1-2 frame fade-out to F-6 contributions on Override**: instead of zeroing, decay F-6 contributions to 0 over 33ms (2 frames at 60fps). Matches anti-snap envelope logic; preserves §5.1 (a) author decision in spirit (F-5 takes over quickly); eliminates the discontinuity.
- **(b) Document an explicit exception clause**: anti-snap principle applies to natural tween completion but NOT to user-input-driven state transitions (the Override case). Specify a magnitude threshold below which the snap is acceptable (e.g., "if |F-6 contribution| < 2° at Override time, snap is acceptable; otherwise apply (a) fade-out"). Threshold-based hybrid.
- **(c) Add a playtest AC requiring lead sign-off that the Override snap reads as "input responsiveness" not "head twitch"**. Subjective acceptance criterion; defers the question to polish.

Recommend **(a)** — single-formula change, eliminates self-contradiction, preserves §5.1 (a) decision intent. Update F-6 pseudo-code to apply the 2-frame fade-out on Override fire. Update AC-F6-B to assert the fade-out behavior (rather than instantaneous zero).

**F.2 — EC-15 head-clamp artifact at TweenProgress=0.85 + edge_absorb_progress=0.10**

**Defect (game-designer)**: Worked math: head sum = 13.5° → clamps to 12° while body settles to neutral. Reads as "stuck head + settling body" — two-system disconnect, not unified "bend." Player-reachable scenario (player approaching FarLeft mid-tween instinctively re-taps left at 0.10s into the edge-absorb tail).

**Author decision required (DR-F.2)**: choose one:
- **(a) Cap F-6 contribution proportional to F-5 phase**: F-6 contribution decays to 0 proportionally faster when F-5 is in Phase 3 (TweenProgress > 0.80). Eliminates the additive overshoot that causes the head clamp during settle.
- **(b) Widen the clamp ceiling**: raise `±MAX_LEAN_ANGLE_DEG × 1.2` to `±MAX × 1.5` (15° at default). Avoids the clamp firing during normal EC-15 but allows larger lean excursions overall. Visual impact: larger maximum lean angle than designed.
- **(c) Reduce F-6 peak fraction**: lower the 0.35 peak in EdgeAbsorbCurve to 0.20-0.25 during EC-15 path (F-4 mid-tween). Specifically: F-6 contribution multiplied by an additional 0.6-0.7 factor when F-5 is active. Preserves clamp ceiling, preserves SETTLED edge-absorb peak.

Recommend **(a)** — cleanest semantic. Add a formula modifier: when `movement_state == SLIPPING`, F-6 contribution is multiplied by `(1.0 - TweenProgress × 0.5)` — at TweenProgress=0.85 this gives multiplier 0.575, so F-6 head contribution drops from +3.5° to +2.01°, sum = 10° + 2.01° = 12.01° → still at clamp boundary. Author may need to tune the multiplier to avoid clamping entirely. Add AC-F6-E: assert no clamp engagement at TweenProgress=0.85 + edge_absorb_progress=0.10 in EC-15 path.

**F.3 — §5.2 (a) two-class skill experience / content-opaque difficulty reduction**

**Defect (game-designer)**: Banner says "adjustments active" but doesn't tell player WHAT changed. Players on degraded devices never see M=3 PEAK; PB calibrated against M=2-only difficulty class; Pillar-5 invisible skill ceiling.

**Author decision required (DR-F.3)**: choose one:
- **(a) Revise banner copy** (already chosen in DR-D.1 if combined): copy explicitly states "Hardest barrage class disabled" or equivalent so the player knows what they're not seeing. Pillar-5 honest via disclosure.
- **(b) Separate PB tracking by difficulty class**: maintain two PB columns (full-difficulty PB + reduced-difficulty PB). Player sees "PB (Performance Mode): X" vs "PB (Full): Y". Adds UI complexity but preserves competitive integrity.
- **(c) Document the trade-off as a known cost**: PB tracks regardless of difficulty class; player on degraded devices has an easier PB; acknowledge this is a real cost in the GDD. Pillar-5 honest via documentation.

Recommend **(a) + DR-D.1 (a)** combined: the banner copy choice from DR-D.1 should explicitly say "Hardest barrage suppressed" or similar. This closes both DR-D.1 and DR-F.3 in one copy revision.

---

## 7. §7 — Cluster G: Pipeline / Tooling (1 item)

**G.1 — AC-SS-E yaml→C++ header generator pipeline undocumented**

**Defect (qa-lead BLOCKING + gameplay-programmer RECOMMENDED)**: AC-SS-E asserts a code-generation pipeline (`design/registry/entities.yaml` → C++ header) that is undocumented. Without pipeline: unimplementable. With locally-defined constants: meaningless.

**Author decision required (DR-G.1)**: choose one:
- **(a) Document the pipeline**: add an architecture seam for the yaml→C++ header generator. Specify the tool path, invocation command, generated header location, CI integration. This is a TD + lead-programmer decision; coordinate before authoring.
- **(b) Reframe AC-SS-E to test what exists**: rewrite as "Given the C++ constant `MIN_ESCAPE_SLIPS` in `[header path]` is changed from 2 to 3, then PM's compile unit fails to compile with the documented `static_assert`." Removes the yaml dependency. Trade-off: registry-to-code drift is not caught.
- **(c) Open OQ for the pipeline; mark AC-SS-E as STUBBED**: defer the pipeline authoring to a future ADR; mark AC-SS-E as STUBBED pending the pipeline. Document the dependency.

Recommend **(b)** for R11a — quickest fix, no new infrastructure required, the `static_assert` itself is still meaningful (catches local constant drift). Add an open question OQ for the registry→code pipeline as a future architecture improvement.

---

## 8. R11 forecast + decomposition trigger

**R11 fresh-context re-review forecast**: **3-8 BLOCKING**

The band is widened from R10a's 0-4 because:
- R10a forecast was off by 4-5x; calibrated humility is warranted.
- The failure surface is now cross-cutting (UX/audio/accessibility/cert), which historically surfaces more cross-domain findings than pure structural defects.
- Several R11a decisions (DR-B.3, DR-C.1, DR-D.4, DR-F.1, DR-F.2) have multiple acceptable resolutions — the chosen path may surface follow-on findings R11 reviewer catches.

**Decomposition trigger (CD-set)**: if R11 returns **>8 BLOCKING**, CD recommends **GDD decomposition** — split `player-movement.md` into sub-GDDs by failure domain (e.g., `player-movement-mechanics.md` + `player-movement-presentation.md` + `player-movement-platform.md`) rather than another revision round. The author should treat this as a warning: if during R11a the author discovers a cluster genuinely cannot be resolved at the GDD's current scope, surface this BEFORE shipping R11a rather than discovering it at R11.

**Validation criteria for R11 success**:
- BLOCKING count within 3-8 forecast band.
- **Cluster A does not recur** (the canary — if pre-R11a text survives anywhere it shouldn't, the brief execution model is broken).
- No new domain surfaces appear (a 4th BLOCKING domain cluster from a domain not in R10's 7 would trigger decomposition recommendation).

---

## 9. Out of scope for R11a

- **Cross-system propagation to Pull-Wave / DPC / Wave Spawner / HUD**: deferred to `/propagate-design-change` post-R11-APPROVED.
- **`/setup-engine` and Unreal Engine version pinning**: still queued.
- **OQ-1 (PM object type ADR), OQ-2 (Tween implementation ADR), OQ-3 (Input event binding ADR), OQ-4 (lateral_world_position authority during DEAD), OQ-7 (RSM UE object type)**: implementation-prerequisite ADRs, not GDD revisions.
- **Polish-phase stubs**: canonical UE 5.6+ framerate-floor mechanism, named min-spec device list, AC-HW-B device audit — all remain STUBBED for Polish phase per R10a. R11a does NOT close these.
- **Telegraph prototype** (highest-risk bet per systems-index): independent track.
- **Pull-Wave R9 fresh-context re-review**: independent track, queued separately.

---

## 10. Recommended R11a execution order

CD does NOT mandate execution order beyond "§1 and §2 are PRIMARY" because the clusters are independently editable. Recommended order for efficiency:

1. **§1 (Cluster A)** — quickest win; locate-and-replace on three text locations. Builds confidence + closes the highest-conviction finding. ~1-2 hours.
2. **§2 (Cluster E)** — structural; declares `HandleSlipTransition` and fixes COMPLETE/ABORTED dispatch. Cross-references with §6 (F-6 fade-out on Override). ~3-4 hours.
3. **§6 (Cluster F)** — Player Fantasy / Coherence decisions affect §1.1 F-6 spec authored in §2. Resolve DR-F.1, DR-F.2, DR-F.3 here. ~2-3 hours.
4. **§3 (Cluster B)** — Watchdog buffer init + AC-HW-B math + DR-B.3 decision. ~2 hours.
5. **§5 (Cluster D)** — UX / Cert decisions + AC additions. ~2-3 hours.
6. **§4 (Cluster C)** — Audio cluster; DR-C.1 + DR-C.4 decisions then AC rewrites. ~3 hours.
7. **§7 (Cluster G)** — DR-G.1 single decision + AC rewrite. ~30 minutes.

**Total estimated authoring**: 14-18 hours focused work. Budget accordingly.

---

## 11. Author author-decision summary (for AskUserQuestion batching at R11a session open)

The R11a author should batch-resolve the following decisions at session open via `AskUserQuestion` widgets (one widget per cluster of related decisions):

| ID | Decision | Recommended | §-ref |
|---|---|---|---|
| DR-B.3 | M=3 PEAK in-flight policy on breach | (b) Grace window | §3 |
| DR-C.1 | audio_cue_ratio safe range vs pitch shift | (a) Constrain to [0.79, 1.26] | §4 |
| DR-C.4 | Center-pan slip variant schedule | (a) Retire panned variant | §4 |
| DR-D.1 | Banner copy | (a) Performance mode + difficulty adjusted | §5 |
| DR-D.3 | Commitment-tell flash cadence cap | (c) 5 fires/sec + 80% luminance | §5 |
| DR-D.4 | Near-miss accessibility | (b) Opt-in haptic setting | §5 |
| DR-F.1 | Override snap fade-out | (a) 2-frame fade-out | §6 |
| DR-F.2 | EC-15 head clamp | (a) Proportional F-6 cap during F-5 Phase 3 | §6 |
| DR-F.3 | Two-class skill disclosure | (a) Combined with DR-D.1 banner copy | §6 |
| DR-G.1 | AC-SS-E pipeline | (b) Reframe to test local C++ constant | §7 |

10 decisions; author batches in 3 `AskUserQuestion` widgets (Watchdog/UX, Audio/Player-Fantasy, Pipeline). All recommendations are CD-and-specialist-consensus; author overrides any with documented rationale.

---

**Brief authored by**: CD synthesis on PM R10 fresh-context re-review (2026-06-15).
**Brief authority**: binding for R11a author execution. Override only with documented rationale.
**Brief execution gate**: R11a session must open in a `/clear` fresh-context session AFTER this brief is durable on disk. R7 same-session-bias precedent applies.

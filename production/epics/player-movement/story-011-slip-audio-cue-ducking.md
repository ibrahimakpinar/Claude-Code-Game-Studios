# Story 011: Slip audio cue + F-AUDIO-CUE-DURATION + -6dB duck + HARD-CUT triple-overlap

> **Epic**: Player Movement (Slip)
> **Status**: Complete
> **Layer**: Presentation
> **Type**: Integration
> **Estimate**: 6 hours
> **Manifest Version**: 2026-07-03 (docs/registry/architecture.yaml v8)
> **Last Updated**: 2026-08-03

## Context

**GDD**: `design/gdd/player-movement-presentation.md` (§3 Audio-Visual Ownership Split, §4 F-AUDIO-CUE-DURATION + F-AUDIO-CUE-IDENTITY, §4 F-HARDCUT-RAMP, §5 EC-16 Triple-Overlap Audio, §8 AC-AUDIO-CUE-PROPORTIONALITY + AC-AUDIO-CUE-DUCKING, R11a-13/14/15).
**Requirement**: `TR-PM-031` (audio_cue_ratio × SLIP_TWEEN duration), `TR-PM-032` (-6dB duck + HARD-CUT under buffer-drop).
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0009 (dispatch site on SETTLED→SLIPPING trigger; audio system owns mix).
**ADR Decision Summary**: PM dispatches slip cue with computed duration = `audio_cue_ratio × SLIP_TWEEN_DURATION_S` (default 0.93 × 0.15s = 139.5ms) on SETTLED→SLIPPING. Ducking applies when near-miss overlaps active slip: -6dB attack 50ms / release 100ms. Triple-overlap (buffer-drop + slip + near-miss): buffer-drop plays full, slip HARD-CUT with ≤5ms ramp, near-miss plays full (EC-16).

**Engine**: Unreal Engine 5.7 | **Risk**: LOW (audio dispatch surface is project-owned; UE Metasounds bus-level gain automation stable pre-cutoff).
**Engine Notes**: None.

**Control Manifest Rules (this layer)** — from `docs/registry/architecture.yaml` v8:
- **Required**: Slip cue duration = `audio_cue_ratio × SLIP_TWEEN_DURATION_S`; `audio_cue_ratio` safe range [0.89, 1.12]; default 0.93 (per R11a-13).
- **Required**: Pan is CENTER-locked (R11a-14 retired panned variant; single-variant slip cue only).
- **Required**: EC-16 triple-overlap resolution — buffer-drop full, slip HARD-CUT, near-miss full. HARD-CUT ramp ≤5ms per R11a-15 (raised-cosine shape pending R12a DR-PRES-RAMP audio-director decision — implement linear as placeholder if raised-cosine helper unavailable).

---

## Acceptance Criteria

*From `design/gdd/player-movement-presentation.md` §4 + §5 + §8, scoped to this story:*

- [ ] `PlaySlipAudioCue(EPlayerLane From, EPlayerLane To)` dispatched from Story 003's SETTLED→SLIPPING trigger site:
  - [ ] Duration = `FMath::Clamp(audio_cue_ratio, 0.89f, 1.12f) * FMath::Clamp(SLIP_TWEEN_DURATION_S, 0.10f, 0.15f)`.
  - [ ] Center-pan.
  - [ ] Playback rate transposition bounded ±2 semitones (rate ∈ [0.893, 1.124]) — enforced by clamp on `audio_cue_ratio`.
- [ ] `PlayBufferDropAudioSting()` (from Story 005): 300–600 Hz dry percussive, <80ms — pre-authored asset; PM dispatches only.
- [ ] `PlayNearMissAudioSwell()` (from Story 012): 600 Hz–1.6 kHz breath swell, 200–300ms, -6dB relative to slip cue.
- [ ] Ducking logic (AC-AUDIO-CUE-DUCKING Setup A active-overlap):
  - [ ] When near-miss swell overlaps an ACTIVE slip cue, duck the slip bus by -6dB.
  - [ ] Attack envelope: 50ms linear ramp from authored dB to authored − 6 dB.
  - [ ] Sustained duck for the remainder of the overlap window.
  - [ ] Release envelope: 100ms linear ramp back — only exercised if slip cue extends past near-miss (safe-range analysis in Setup C indicates release is typically vacuous — slip always ends before near-miss swell in safe range).
- [ ] HARD-CUT on buffer-drop overlap (EC-16 rule):
  - [ ] When buffer-drop fires WHILE slip cue active → slip cue suppressed via ≤5ms ramp (linear placeholder; raised-cosine pending R12a DR-PRES-RAMP).
  - [ ] Buffer-drop plays at full authored level.
- [ ] Triple-overlap (buffer-drop + slip + near-miss simultaneously):
  - [ ] Buffer-drop full; slip HARD-CUT; near-miss full — per EC-16 resolution.
- [ ] **AC-AUDIO-CUE-PROPORTIONALITY (6 safe-range test pairs)**: verify duration = ratio × SLIP_TWEEN within ±2ms tolerance across:
  - (SLIP_TWEEN=0.15, ratio=0.93) → 139.5ms
  - (0.10, 0.93) → 93.0ms
  - (0.15, 0.89) → 133.5ms
  - (0.15, 1.12) → 168.0ms
  - (0.10, 1.12) → 112.0ms
  - (0.10, 0.89) → 89.0ms
- [ ] **AC-AUDIO-CUE-DUCKING Setup A (active-overlap)**: slip @ t=0ms, near-miss @ t=25ms → slip pre-25ms at authored; attack 25-75ms; sustained duck 75ms-cue-end; near-miss at full.
- [ ] **AC-AUDIO-CUE-DUCKING Setup B (vacuous)**: slip @ t=0ms, near-miss @ t=250ms (after slip ends). No active slip when near-miss triggers → no ducking → AC vacuously satisfied.
- [ ] **AC-AUDIO-CUE-DUCKING Setup C (release envelope unverifiable in safe range)**: joint safe-range analysis — slip max 168ms, near-miss min-onset 200ms → release never exercised in safe-range configurations. Document as unverifiable; leave test as documented no-op with rationale.

**Pending resolution acknowledged (R12a-PENDING DR-PRES-RAMP)**: HARD-CUT ramp shape (linear vs raised-cosine) awaits audio-director decision. Linear placeholder is production-shippable — meets the ≤5ms ramp AC and the "ends before buffer-drop first zero-crossing" constraint. Raised-cosine upgrade is a polish-phase swap, not a blocker.

---

## Implementation Notes

*Derived from presentation §3 + §4 + §5 EC-16 + R11a-13/14/15:*

**Slip cue dispatch site** (extends Story 003):
```cpp
// In SETTLED→SLIPPING branch, after collision commit + TriggerCommitmentTell:
const float duration_s = FMath::Clamp(audio_cue_ratio, 0.89f, 1.12f)
                       * FMath::Clamp(SLIP_TWEEN_DURATION_S, 0.10f, 0.15f);
PlaySlipAudioCue(current_lane, target_lane, duration_s);
```

**Ducking + HARD-CUT state** on component (or on audio mix manager — depends on final audio arch):
- `bool slip_cue_active`; `float slip_cue_remaining_s`; `bool slip_cue_ducked`; `float duck_envelope_s`.
- On near-miss dispatch (Story 012): if `slip_cue_active`, engage duck.
- On buffer-drop dispatch (Story 005): if `slip_cue_active`, engage HARD-CUT.

**HARD-CUT ramp**:
```cpp
// ≤5ms ramp from current level to silence
// Linear placeholder (R12a-PENDING raised-cosine per DR-PRES-RAMP audio-director decision)
const float ramp_s = 0.005f;
// If Metasounds bus gain automation available, use it. Else per-cue gain envelope.
SlipCueBus->SetGainRamp(0.0f, ramp_s);
```

**Center-pan enforcement** (R11a-14): PM never dispatches a panned slip cue variant. If Metasounds pan parameter exists on the cue asset, PM writes 0 (center). Enforcement: single-variant cue asset only.

**Tuning knobs** (presentation §7):
- `float audio_cue_ratio = 0.93f;` safe [0.89, 1.12]

**Performance**: Audio dispatch is fire-and-forget on state transitions. Worst-case concurrency = triple-overlap EC-16 (3 cues + 1 duck envelope tick). No per-tick cost outside active envelope windows (attack 50ms / HARD-CUT 5ms). Budget: negligible against 16.6ms frame budget.

**Rate-transposition invariant** (F-AUDIO-CUE-IDENTITY): playback rate `1.0 / audio_cue_ratio` bounded to ±2 semitones. Verify: `rate_min = 1/1.12 ≈ 0.893` = -1.96 semitones; `rate_max = 1/0.89 ≈ 1.124` = +2.02 semitones. Just inside ±2 semitones. Safe.

---

## Out of Scope

- Story 003: SETTLED→SLIPPING trigger site (this story adds the audio dispatch).
- Story 005: Buffer-drop sting dispatch (this story routes it through the HARD-CUT logic; the sting itself is Story 005).
- Story 012: Near-miss swell dispatch (this story handles the ducking trigger side; near-miss cue authoring is Story 012).
- Audio Bible: cue asset authoring for slip whoosh, buffer-drop click, near-miss swell — assumed pre-authored per audio direction.
- R12a-PENDING DR-PRES-RAMP: raised-cosine HARD-CUT shape decision — implement linear placeholder; audio-director resolves later.

---

## QA Test Cases

*Test file: `tests/integration/player-movement/pm_audio_cue_test.cpp`. Automated integration tests with mocked audio bus.*

- **AC-AUDIO-CUE-PROPORTIONALITY (6 safe-range test pairs)**:
  - Given: knobs set to each pair.
  - When: `PlaySlipAudioCue` invoked.
  - Then: dispatched duration matches expected value within ±2ms.
  - Table: as listed in ACs above.

- **AC-AUDIO-CUE-DUCKING Setup A (active-overlap)**:
  - Given: `PlaySlipAudioCue` at t=0 (duration 139.5ms); `PlayNearMissAudioSwell` at t=25ms.
  - When: audio bus state sampled at t=0, 24, 26, 74, 76, 139.
  - Then: (a) t=0-24: slip at authored level; (b) t=25-75: linear attack from authored to -6dB; (c) t=75-139: sustained -6dB; (d) near-miss at authored throughout.
  - Edge cases: near-miss BEFORE slip → slip fires at authored (near-miss doesn't affect a NOT-YET-active cue).

- **AC-AUDIO-CUE-DUCKING Setup B (vacuous)**:
  - Given: slip @ t=0 (duration 139.5ms); near-miss @ t=250ms.
  - When: near-miss dispatched.
  - Then: no ducking engaged; slip already ended; near-miss at authored level.

- **AC-AUDIO-CUE-DUCKING Setup C (release envelope unverifiable)**:
  - Document-only test: assert that in all safe-range configurations, slip ends before near-miss swell onset, so release envelope is not exercised. If future tuning changes cross this threshold, the test must be revisited.

- **EC-16 triple-overlap (buffer-drop full, slip HARD-CUT, near-miss full)**:
  - Given: slip @ t=0; buffer-drop @ t=20ms; near-miss @ t=30ms.
  - When: bus state sampled at t=25 and t=50.
  - Then: (a) t=20-25: slip ramping to 0 (≤5ms HARD-CUT ramp); (b) t=25+: slip silent; (c) buffer-drop plays full 0-80ms; (d) near-miss plays full from t=30.

- **Center-pan enforcement**:
  - Given: any slip cue dispatch.
  - When: audio parameter read.
  - Then: pan == 0.0 (center); no panned variant dispatched.

- **HARD-CUT ramp duration**:
  - Given: slip active; buffer-drop fires.
  - When: bus state sampled every 1ms from HARD-CUT trigger.
  - Then: slip level reaches 0 within 5ms (bit-accurate given linear ramp).

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Automated integration test at `tests/integration/player-movement/pm_audio_cue_test.cpp` — must pass. Uses a stubbed audio bus that records dispatch + gain envelope events.

**Status**: [x] Created — `Source/SLIPSTORM/Tests/Integration/PlayerMovement/PMAudioCueTest.cpp` (9 test commands; 0 errors, 0 warnings). Actual path uses project convention `Source/SLIPSTORM/Tests/…`, not the aspirational `tests/integration/…` shown above (recurring template drift #2).

---

## Dependencies

- **Depends on**: Story 003 (SETTLED→SLIPPING trigger site); Story 005 (buffer-drop sting dispatch — HARD-CUT logic hooks into it); Audio Bible cue authoring; presentation §7 tuning knob defaults.
- **Unlocks**: Story 012 (near-miss swell duck hook — near-miss dispatch calls into this story's duck-if-slip-active logic).

---

## Completion Notes

**Completed**: 2026-08-03
**Criteria**: 10/10 addressed (9 covered by tests, 1 correctly DEFERRED — AC-3 near-miss dispatch is Story 012 scope per Out of Scope).
**Test Evidence**: Integration test at `Source/SLIPSTORM/Tests/Integration/PlayerMovement/PMAudioCueTest.cpp` (9 test commands: proportionality 6-pair, Setup A ducking + mid-sustain sample, Setup B vacuous, Setup C safe-range math + static_assert, EC-16 triple-overlap with HARD-CUT precedence guard reachable, center-pan invariant, HARD-CUT ramp ≤5ms with float tolerance, rate-transposition clamp with boundary cases, dispatch-site SETTLED→SLIPPING integration). Build clean.
**Code Review**: Complete (2026-08-03). unreal-specialist CLEAN + qa-tester GAPS (2 BLOCKING + 4 non-blocking). All 6 findings addressed inline; rebuild clean first-try.

**Advisory deviations** (documented, non-blocking):
1. Test file at project-convention path `Source/SLIPSTORM/Tests/Integration/PlayerMovement/PMAudioCueTest.cpp` — story spec's aspirational `tests/integration/player-movement/pm_audio_cue_test.cpp` is the known recurring template drift item #2.
2. `static_assert` safe-range guard added on `SLIP_TWEEN_MAX × SLIP_AUDIO_CUE_RATIO_MAX < 0.200f` — advisor-recommended defensive add; enforces Setup C at compile time (not in original spec).
3. HARD_CUT precedence guard inside `EngageDuckIfSlipActive` declines to overwrite in-progress HARD-CUT with a duck — aligns with EC-16 intent ("buffer-drop full, slip HARD-CUT, near-miss full"). Not in spec; covered by TC5 post-fix.
4. `SnapToTargetAndReset` extended with item 9 (Story 011 audio-state clear) on COMPLETE/ABORTED/COUNTDOWN entry — mirrors Story 010 item 8 precedent. Eliminates the latent audio-cue-leak surfaced during code-review S-2 (harmless while dispatch is stub; load-bearing once real audio bus is wired).

**Session note**: First story of the session where both initial implementation and all code-review fixes built clean on first attempt. Inline implementation (per advisor recommendation) replaced specialist spawn from the start — session's 5/5 stall pattern broken.

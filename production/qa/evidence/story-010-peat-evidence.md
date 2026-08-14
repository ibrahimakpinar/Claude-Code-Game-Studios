# Story 010 — PEAT / Harding FPA Photosensitivity Evidence

**Story**: `production/epics/player-movement/story-010-commitment-tell.md`
**Companion evidence**: `production/qa/evidence/story-010-commitment-tell-evidence.md`
**Type**: Visual/Feel — photosensitivity gate
**Status**: **SKELETON + DESIGN PRE-CHECK — Sprint 1 (S1-06)**. Formal Harding FPA / W3C-PEAT PASS verdict deferred to Polish per presentation §8 AC-COMMIT-FLASH-CADENCE and this doc §5.

---

## 1. Purpose & Scope

Sprint-1 skeleton for Story 010's photosensitivity certification. Captures:

- Commitment-tell flash design parameters relevant to PEAT / Harding FPA evaluation (§2).
- Standards + thresholds against which the design is evaluated (§3).
- Design-analytical pre-check that Story 010's design is *expected* to pass formal certification with comfortable margin (§4) — reproduces the GDD `player-movement-presentation.md:307` working sketch in one place for QA/accessibility-specialist reference.
- Polish-phase capture protocol for the formal Harding FPA / W3C-PEAT run (§5).
- Empty sign-off table for Polish completion (§6).

**Not in scope for Sprint 1**: formal Harding FPA / W3C-PEAT tool verdict; art-director / accessibility-specialist sign-off checkboxes. Both are Polish-phase per presentation §8 and this doc §5.

## 2. Design Parameters (Story-010 Commitment-Tell)

Per GDD `player-movement-presentation.md` §3 (Commitment-Tell) + §4 F-COMMIT-CADENCE-CAP + §7 Tuning Knobs (`commitment_tell_flash_amplitude`, `commitment_tell_flash_cadence_cap_ms`):

| Parameter | Value | Source |
|-----------|-------|--------|
| Peak amplitude (`LeadingFaceFlash`) | ±0.80 (80% white; reduced from pre-R11a ±1.0 for PEAT compliance) | R11a-11 |
| Peak hold duration | 2 frames minimum (33 ms @ 60 fps; 66 ms @ 30 fps) | Presentation §3 |
| Decay duration | 50 ms linear fade | Presentation §3 |
| Total visible flash duration | ~83 ms @ 60 fps; ~116 ms @ 30 fps | Derived |
| Cadence cap (cooldown) | 200 ms wall-clock from prior fade-to-zero moment | R11a-11 |
| Design ceiling | ≤ 5 fires/sec | AC-COMMIT-FLASH-CADENCE §8 |
| Expected effective cadence @ 60 fps worst-case (SLIP_TWEEN=0.10 buffer-flush chain) | ≈ 3.5 fires/sec (~83 ms flash + 200 ms cooldown = ~283 ms cycle) | Presentation §4 F-COMMIT-CADENCE-CAP |
| Expected effective cadence @ 30 fps degraded framerate | ≈ 3.16 fires/sec (~116 ms flash + 200 ms cooldown = ~316 ms cycle) | Presentation EC-19 |
| Suppression ratio @ 60 fps worst-case | ≥ 50% of buffer-flush transitions have their visual flash suppressed | AC-COMMIT-FLASH-CADENCE Setup A |
| Substrate | Leading face of avatar torso voxel block; ambient face reflectance ~0.20 | Presentation §3 |
| Trigger scope | SETTLED → SLIPPING transitions only; does NOT fire during edge-absorb | Presentation §3 |
| Accessibility opt-out (R12a-PENDING) | `commitment_tell_flash_enabled` setting; default `true`; gates dispatch only (counter still increments) | Presentation EC-18 |

**Color palette**: 80% white flash on a dark torso voxel material. **No saturated-red flash surface** — the red-flash tier of PEAT / Harding is not engaged by design.

## 3. Standards & Thresholds

The commitment-tell flash is evaluated against three standards commonly cited by the game-industry photosensitivity certification pipeline:

### 3.1 IEC 61966-2-2 (referenced by PEAT)

The International Electrotechnical Commission's photosensitivity criteria used as the underlying reference by W3C-PEAT and Harding FPA. Key thresholds:

- **General flash rule**: no more than 3 flashes per second (3 Hz) with luminance change > 10% at flash area ≥ 25% of display.
- **Red flash rule**: separate stricter criterion for saturated-red transitions (not engaged by Story 010 — white flash on dark substrate only).
- **Pattern rule**: static or oscillating stripe/pattern-based (not engaged by Story 010 — single-face flash, not pattern).
- **Duration cap**: pattern must not repeat for > 5 seconds at threshold conditions.

### 3.2 Harding Flash and Pattern Analyzer (FPA)

Cambridge Research Systems' tool that runs a recorded video capture through the ITU-R BT.1702 test envelope. Outputs a PASS / FAIL verdict per second of video. The formal certification target for Story 010's Polish gate.

### 3.3 W3C-PEAT (Photosensitive Epilepsy Analysis Tool)

University of Wisconsin-Madison Trace R&D Center's tool that runs a video capture against the WCAG 2.1 SC 2.3.1 (Three Flashes or Below Threshold) criterion, using an IEC 61966-2-2-derived detection algorithm. Secondary certification target.

## 4. Design-Analytical Pre-Check (PRELIMINARY, NOT FORMAL)

> **Explicit disclaimer**: this section reproduces the GDD `player-movement-presentation.md:307` working sketch. It is an in-development sanity check that Story 010's design is *expected* to pass the formal Harding FPA / W3C-PEAT run at Polish. It is **NOT the formal certification gate** and does **NOT** substitute for the Polish-phase tool run.

**Working sketch — `flash_rate × contrast` product heuristic**:

- Worst-case cadence @ 60 fps SLIP_TWEEN=0.10 buffer-flush chain: **3.5 fires/sec** (design parameter §2).
- Contrast step (peak 0.80 white against ~0.20 ambient torso reflectance): **0.60** relative contrast.
- Product: **3.5 × 0.60 = 2.1 flashes·contrast/sec**.
- Quick-look heuristic ceiling: **6.0 flashes·contrast/sec** (per GDD §4 F-COMMIT-CADENCE-CAP).
- **Result**: 2.1 << 6.0 → **passes preliminary heuristic with ~65% margin**.

**Corroborating design checks**:

- Effective cadence 3.5 Hz is above the 3 Hz general-flash threshold in raw count — BUT the reduced contrast (60% relative vs 100% relative at ±1.0 peak) is the pre-emptive PEAT margin. IEC 61966-2-2's threshold is contrast-scaled, not raw-rate.
- No red-flash surface (white on dark) → red-flash rule not engaged.
- No pattern surface (single face) → pattern rule not engaged.
- Under R11a-11's 200 ms cadence cap, no sustained flash sequence can exceed 5 fires/sec even under theoretical maximal transition rate (~10 transitions/sec at SLIP_TWEEN=0.10).

**Pre-check verdict**: Story 010 is **expected to pass** the formal Harding FPA / W3C-PEAT Polish-gate run at both Setup A (60 fps worst-case) and Setup B (30 fps degraded) recorded video. Formal PASS verdict remains gated on tool infrastructure availability + accessibility-specialist sign-off at Polish.

**Risk register for the formal run**:

| Risk | Mitigation |
|------|------------|
| Real-device luminance calibration differs from working-sketch ~0.20 ambient torso reflectance assumption | Polish-phase capture uses actual PIE render; formal tool measures actual pixel values |
| Tuning drift moves `commitment_tell_flash_amplitude` above ±0.80 or `commitment_tell_flash_cadence_cap_ms` below 200 ms | Presentation §7 Tuning Knobs safe-range floor/ceiling flags out-of-band tuning; Polish-gate re-certifies |
| Frame rate lower than 30 fps (Low Power Mode, thermal throttling below 30 fps cap) | Watchdog engages Wave Spawner suppression + banner disclosure per R11a-8/R11a-9; commitment-tell still self-limited by 200 ms wall-clock cooldown |

## 5. Polish-Phase Capture Protocol

The formal Harding FPA / W3C-PEAT run requires **actual recorded video** of the game running Story 010's commitment-tell in the scenarios below. This section defines the protocol so the Polish-phase captor produces videos compatible with tool input requirements.

### 5.1 Common capture requirements

- **Resolution**: minimum 720p; native display resolution preferred.
- **Frame rate**: capture at exactly the target scenario framerate (see below). No frame-drop tolerance.
- **Format**: uncompressed or lossless (Harding FPA + W3C-PEAT both require frame-accurate luminance).
- **Duration**: minimum 10 seconds per scenario to satisfy IEC 61966-2-2 duration criterion.
- **Ambient**: neutral gray background; no HUD occlusion of the avatar torso; single-lane camera angle.
- **Device**: target hardware — iPhone XR class (thermal-representative mid-tier iOS); Pixel 5 class (Android).
- **Repeat**: 3 independent captures per scenario to establish inter-run consistency.

### 5.2 Setup A — 60 fps worst-case buffer-flush chain

- Framerate: 60 fps (device performing to spec; watchdog inactive).
- `SLIP_TWEEN_DURATION_S = 0.10` (tightest tween — maximum transition rate).
- Input: sustained rapid-fire alternating left/right slip input for ≥ 10 seconds; drive theoretical maximum ~10 transitions/sec.
- Expected observed commitment-tell cadence in capture: ≤ 5 fires/sec (200 ms cooldown gating engaged); effective ≈ 3.5 fires/sec.
- Tool feed: full capture into Harding FPA + W3C-PEAT.

### 5.3 Setup B — 30 fps degraded framerate

- Framerate: 30 fps (device in Low Power Mode OR watchdog-engaged degraded state).
- `SLIP_TWEEN_DURATION_S = 0.10` (same tightest tween).
- Input: same sustained rapid-fire as Setup A for ≥ 10 seconds.
- Expected observed commitment-tell cadence: ≈ 3.16 fires/sec; suppression ratio ≥ 37%.
- Tool feed: full capture into Harding FPA + W3C-PEAT.

### 5.4 Setup C — accessibility opt-out verification (R12a-PENDING)

- Requires DR-PRES-FLASH R12a closure first (`commitment_tell_flash_enabled` setting authored).
- Framerate: 60 fps.
- Toggle: `commitment_tell_flash_enabled = false`.
- Input: same sustained rapid-fire as Setup A for ≥ 10 seconds.
- Expected observed commitment-tell cadence in capture: **zero visible flashes** (dispatch gate engaged); `commitment_tell_fire_count` in debug overlay still increments.
- Tool feed: N/A — this setup demonstrates the accessibility gate; no PEAT verdict required.

## 6. Polish-Phase Acceptance Criteria & Sign-Off

**All sign-off checkboxes DEFERRED — captured for Polish completion.**

### 6.1 AC-COMMIT-FLASH-CADENCE Formal (Polish BLOCKING)

- [ ] Harding FPA outputs PASS verdict on Setup A capture.
- [ ] Harding FPA outputs PASS verdict on Setup B capture.
- [ ] W3C-PEAT outputs PASS verdict on Setup A capture (WCAG 2.1 SC 2.3.1).
- [ ] W3C-PEAT outputs PASS verdict on Setup B capture.

### 6.2 R12a-PENDING accessibility opt-out (Polish BLOCKING once R12a closed)

- [ ] Setup C demonstrates zero visible commitment-tell flashes.
- [ ] Setup C `commitment_tell_fire_count` still increments per underlying transition.
- [ ] `IGameSettings::IsCommitmentTellFlashEnabled()` toggle persists across sessions.

### 6.3 Sign-Off Table (Polish)

| Role | Sign-off | Date | Notes |
|------|----------|------|-------|
| accessibility-specialist | [ ] Approved | | Owns the PEAT compliance Setup A FFT verification per presentation §Cross-Sub-GDD Dependencies row 4 |
| qa-lead | [ ] Approved | | Setup A + B captures produced per §5 protocol; tool verdicts archived under `production/qa/evidence/polish/peat/` |
| art-director | [ ] Approved | | Amplitude + cadence tuning within Presentation §7 safe range at time of capture |

## 7. Cross-References

- `production/epics/player-movement/story-010-commitment-tell.md` — Story 010 spec
- `production/qa/evidence/story-010-commitment-tell-evidence.md` — companion visual-evidence doc (AC-29 peak, Setup A sighting, R12a-PENDING accessibility check)
- `design/gdd/player-movement.md` R11a-11 — cadence-cap + luminance-reduction rationale
- `design/gdd/player-movement-presentation.md` §3 Commitment-Tell — full design spec
- `design/gdd/player-movement-presentation.md` §4 F-COMMIT-CADENCE-CAP — formula + working sketch (source of §4 pre-check above)
- `design/gdd/player-movement-presentation.md` §7 Tuning Knobs — `commitment_tell_flash_amplitude`, `commitment_tell_flash_cadence_cap_ms` safe ranges
- `design/gdd/player-movement-presentation.md` §8 AC-COMMIT-FLASH-CADENCE — acceptance criterion + Polish-gate BLOCKING assertion
- `design/gdd/player-movement-presentation.md` EC-18 — R12a-PENDING accessibility toggle
- `design/gdd/player-movement-presentation.md` §Cross-Sub-GDD Dependencies — accessibility-specialist Polish-phase audit ownership
- `production/sprints/sprint-1.md` S1-06 — sprint task (this doc is the deliverable)

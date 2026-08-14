# Story 010 — Commitment-Tell Visual Evidence

**Story**: `production/epics/player-movement/story-010-commitment-tell.md`
**Type**: Visual/Feel
**Status**: [ ] Not yet captured — manual pass required

---

## Purpose

Story 010 is Type: Visual/Feel. Automated tests (`SLIPSTORM.PlayerMovement.CommitmentTell`) cover cadence-cap arithmetic, counter parity, lifecycle timing, direction sign, and reset semantics. This document captures the manual visual evidence required for AC-29 peak amplitude verification and AC-COMMIT-FLASH-CADENCE Setup A sighting.

The PEAT/Harding FPA formal gate is deferred to Polish per presentation §8 AC-COMMIT-FLASH-CADENCE. This document's Polish-phase update will capture that gate.

---

## Manual Check 1 — AC-29 Peak Amplitude Visual

**Setup**:
- PIE at 60 fps with default `SLIP_TWEEN_DURATION_S = 0.15`
- `commitment_tell_flash_enabled = true` (default; R12a-PENDING)
- Neutral background allowing clear observation of leading-face flash intensity

**Steps**:
1. Launch PIE.
2. Perform a single slip input (Left or Right).
3. Observe the leading face of the voxel pawn.

**Expected**:
- Leading face flashes to visible white intensity (±0.80 amplitude per R11a-11 PEAT reduction)
- Peak held for ≥2 frames (≈33 ms at 60 fps)
- Linear decay over ~50 ms to zero
- Total visible flash duration ≈83 ms

**Evidence attach**:
- [ ] Screenshot at peak frame
- [ ] Screenshot mid-decay (~25 ms into fade)
- [ ] Screenshot at fade-to-zero completion

**Sign-off**:

| Role | Sign-off | Date | Notes |
|---|---|---|---|
| art-director | [ ] Approved | | Amplitude matches Art Bible LeadingFaceFlash spec |
| creative-director | [ ] Approved | | Peak reads as "commit tell" to the player |

---

## Manual Check 2 — AC-COMMIT-FLASH-CADENCE Setup A Sighting

**Setup**:
- PIE at 60 fps with `SLIP_TWEEN_DURATION_S = 0.10` (tightest tween)
- Sustained rapid-fire buffered slip input for ≥2 seconds

**Steps**:
1. Launch PIE.
2. Rapid-fire alternating left/right slip inputs for 2+ seconds, faster than the 200 ms cadence window.
3. Observe fire cadence visually.
4. Read `commitment_tell_fire_count` from debug overlay (or log at end of test window).

**Expected**:
- Visible flash cadence capped at ≤5 fires/sec peak (cadence cap engaged)
- ≥50% of triggered transitions visually suppressed
- `commitment_tell_fire_count` reflects ALL transitions (counter always increments, cadence cap gates visual only per Setup A)

**Evidence attach**:
- [ ] Video capture (2-second window)
- [ ] Debug overlay screenshot showing `commitment_tell_fire_count` > 10 while visible fire count ≤ 10

**Sign-off**:

| Role | Sign-off | Date | Notes |
|---|---|---|---|
| art-director | [ ] Approved | | Cadence cap reads naturally, not perceived as bug |
| creative-director | [ ] Approved | | Cap is invisible in normal gameplay |

---

## Manual Check 3 — R12a-PENDING Accessibility Opt-Out (placeholder)

**Setup**:
- Toggle `commitment_tell_flash_enabled = false` via console/dev menu (Settings GDD unauthored — dev-only toggle for now)

**Steps**:
1. Launch PIE with toggle disabled.
2. Perform a single slip input.
3. Observe pawn.

**Expected**:
- No visible flash on the leading face
- `commitment_tell_fire_count` still increments (counter is cadence-independent AND accessibility-toggle-independent)

**Evidence attach**:
- [ ] Screenshot showing no flash post-slip
- [ ] Debug overlay showing counter incremented

**Sign-off**:

| Role | Sign-off | Date | Notes |
|---|---|---|---|
| accessibility-specialist | [ ] Approved | | Toggle behavior meets accessibility spec |

---

## Polish-Phase Deferrals

The following gates are deferred to Polish per presentation §8:

- **PEAT/Harding FPA formal gate** — automated Harding FPA scanner run against video capture of Setup A + Setup B scenarios. Requires PEAT test kit (external dependency).
  - Deferred owner: qa-lead + accessibility-specialist
  - Deferred rationale: PEAT tooling integration is a Polish-phase infrastructure task; Story 010's ±0.80 amplitude reduction from ±1.0 is the pre-emptive design compliance measure.
  - **See also**: `production/qa/evidence/story-010-peat-evidence.md` — Sprint 1 skeleton + design-analytical pre-check + Polish capture protocol.

---

## Notes / Deviations Log

- [ ] Any deviations from AC-29 timing (e.g., decay observed as non-linear)
- [ ] Any deviations from Setup A cadence-cap behavior
- [ ] Any surprises during manual verification
</content>

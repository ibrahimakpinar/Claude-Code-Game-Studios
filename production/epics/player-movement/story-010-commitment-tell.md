# Story 010: Commitment-tell 80% flash + 2-frame hold + 50ms decay + 200ms cadence cap + commitment_tell_fire_count

> **Epic**: Player Movement (Slip)
> **Status**: Complete
> **Layer**: Presentation
> **Type**: Visual/Feel
> **Estimate**: 5–7 hours
> **Manifest Version**: 2026-07-03 (docs/registry/architecture.yaml v8)
> **Last Updated**: 2026-08-02

## Context

**GDD**: `design/gdd/player-movement-presentation.md` (§3 Commitment-Tell contract, R11a-11 PEAT compliance, §4 F-COMMIT-CADENCE-CAP, §8 AC-29, AC-COMMIT-FLASH-CADENCE, AC-COMMIT-FLASH-ENABLED).
**Requirement**: `TR-PM-013` (commitment-tell 80% flash + 2-frame hold + 50ms decay), `TR-PM-014` (200ms cadence cap), `TR-PM-015` (final counter: commitment_tell_fire_count).
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0009 (SD5 trigger site is SETTLED→SLIPPING transition; presentation renders).
**ADR Decision Summary**: `TriggerCommitmentTell(target_lane)` stubbed in Story 003 is implemented here. Writes `LeadingFaceFlash` material parameter to ±0.80 (per R11a-11 PEAT reduction from ±1.0). Cadence cap 200ms suppresses VISUAL only; `commitment_tell_fire_count` still increments on EVERY SETTLED→SLIPPING transition per AC-COMMIT-FLASH-CADENCE Setup A. Sign parameter direction is leading torso face (± based on slip direction).

**Engine**: Unreal Engine 5.7 | **Risk**: LOW (material dynamic parameter write stable pre-cutoff).
**Engine Notes**: `UMaterialInstanceDynamic::SetScalarParameterValue` stable pre-cutoff.

**Control Manifest Rules (this layer)** — from `docs/registry/architecture.yaml` v8:
- **Required**: `commitment_tell_fire_count` increments on EVERY SETTLED→SLIPPING transition — CADENCE CAP DOES NOT GATE THE COUNTER (only the visual). This differs from the flash render gate.
- **Required**: Cadence cap 200ms measured from prior fade-to-zero moment to the new transition. If new transition falls within the window, VISUAL suppressed (material param stays at 0). Counter still increments.
- **Required**: Flash peak amplitude `±0.80` per R11a-11 (peak reduced from ±1.0 for PEAT/Harding FPA compliance).
- **Required**: Sign of the ±0.80 param based on leading torso face — left slip = -0.80, right slip = +0.80 (or per material param convention documented in Art Bible).

---

## Acceptance Criteria

*From `design/gdd/player-movement-presentation.md` §3 + §8, scoped to this story:*

- [ ] `TriggerCommitmentTell(EPlayerLane TargetLane)` implementation (called from Story 003's HandleSlipTransition SETTLED→SLIPPING branch):
  - [ ] Always increment `commitment_tell_fire_count += 1` (cadence-independent per Setup A).
  - [ ] Determine sign: `sign = (static_cast<int32>(TargetLane) > static_cast<int32>(current_lane)) ? +1.0f : -1.0f` (rightward slip → +0.80; leftward → -0.80).
  - [ ] Cadence gate: `if (time_since_last_flash_zero_s < COMMIT_FLASH_CADENCE_MS / 1000.0f) { return; }` — suppress visual, counter already incremented.
  - [ ] Write material param `LeadingFaceFlash = sign * 0.80f` on mesh material dynamic instance.
  - [ ] Schedule flash lifecycle: 2-frame hold at ±0.80, then 50ms linear decay to 0. On decay complete, update `time_last_flash_zero_s = current_time`.
- [ ] `time_since_last_flash_zero_s` tracked as a private accumulator (advance via effective_dt each tick).
- [ ] Fallback (accessibility opt-out — R12a-PENDING DR-PRES-FLASH): if `commitment_tell_flash_enabled == false`, the material param write is suppressed but the counter still increments and cadence gate is still updated. Placeholder toggle — Settings GDD unauthored; document assumption inline.
- [ ] **AC-29 (Peak ±0.80, 2-frame hold, 50ms decay, total ≥83ms @ 60fps)**: verified against timing.
- [ ] **AC-COMMIT-FLASH-CADENCE Setup A (cadence cap engaged; counter ≠ visual)**: SLIP_TWEEN=0.10, sustained buffer-flush 2.0s → ≤5 fires/sec peak cadence (visual); ≥50% transitions visually suppressed; `commitment_tell_fire_count` increments on ALL transitions (independent count > 5 in the window).
- [ ] **AC-COMMIT-FLASH-CADENCE Setup B (cadence cap off; PEAT gate deferred to Polish)**: SLIP_TWEEN=0.15, 5 isolated slips ≥500ms apart → all render at ±0.80; cadence ≤2 fires/sec. PEAT Harding FPA formal gate is deferred to Polish per presentation §8 AC (this story's automated test validates cadence + counter parity; PEAT gate = manual evidence in Polish).
- [ ] **AC-COMMIT-FLASH-ENABLED [R12a-PENDING]**: when `commitment_tell_flash_enabled == false`, visual suppressed, counter still increments. Toggle default is a Settings GDD forward contract — assume `true` if setting not present.

**Pending resolution acknowledged (R12a-PENDING DR-PRES-FLASH)**: The `commitment_tell_flash_enabled` toggle authoring lives with the unauthored Settings GDD; the "assume `true` if setting not present" fallback is production-shippable per presentation §8 AC-COMMIT-FLASH-ENABLED. Landing the toggle asset later is a polish-phase upgrade, not a blocker.

---

## Implementation Notes

*Derived from presentation §3 + §4 F-COMMIT-CADENCE-CAP + AC-29 + AC-COMMIT-FLASH-CADENCE:*

**TriggerCommitmentTell body**:
```cpp
void UPlayerLaneMovementComponent::TriggerCommitmentTell(EPlayerLane TargetLane)
{
    // Counter ALWAYS increments — independent of visual suppression (Setup A)
    commitment_tell_fire_count += 1;

    // Cadence gate (visual suppression)
    const float now_s = GetOwner()->GetWorld()->GetTimeSeconds();
    const float window_s = COMMIT_FLASH_CADENCE_MS * 0.001f;
    if (now_s - time_last_flash_zero_s < window_s)
    {
        return; // visual suppressed
    }

    // Accessibility opt-out (R12a-PENDING)
    // if (!IGameSettings::IsCommitmentTellFlashEnabled()) return;

    // Direction sign
    const float sign = (static_cast<int32>(TargetLane) > static_cast<int32>(current_lane)) ? 1.0f : -1.0f;
    const float flash_amp = sign * COMMIT_FLASH_AMPLITUDE;  // ±0.80

    // Fire material dynamic write (delegated to a presentation helper — Art Bible naming)
    if (MeshMaterialDynamic)
    {
        MeshMaterialDynamic->SetScalarParameterValue(TEXT("LeadingFaceFlash"), flash_amp);
    }

    // Schedule 2-frame hold + 50ms decay — implemented via a small timeline component
    // or per-tick decay accumulator. Simplest: track flash_state {hold_ticks_remaining: 2, decay_time_s: 0.050s}
    flash_hold_ticks_remaining = 2;
    flash_decay_active = false;
}
```

**Per-tick lifecycle advance** (added to TickComponent after F-6):
```cpp
if (flash_hold_ticks_remaining > 0)
{
    --flash_hold_ticks_remaining;
    if (flash_hold_ticks_remaining == 0)
    {
        flash_decay_active = true;
        flash_decay_time_s = 0.050f;
    }
}
else if (flash_decay_active)
{
    flash_decay_time_s -= effective_dt;
    const float t = FMath::Clamp(flash_decay_time_s / 0.050f, 0.0f, 1.0f);
    if (MeshMaterialDynamic)
    {
        // Ramp from current sign*0.80 to 0
        MeshMaterialDynamic->SetScalarParameterValue(TEXT("LeadingFaceFlash"), sign_of_current_flash * COMMIT_FLASH_AMPLITUDE * t);
    }
    if (flash_decay_time_s <= 0.0f)
    {
        flash_decay_active = false;
        time_last_flash_zero_s = GetWorld()->GetTimeSeconds();
    }
}
```

**Reset on COUNTDOWN**: Story 008 already resets `commitment_tell_fire_count = 0`. This story additionally resets flash lifecycle state (hold_ticks_remaining=0, decay_active=false, time_last_flash_zero_s=0) — extend Story 008's SnapToTargetAndReset OR add a hook.

**Tuning knobs** (presentation §7):
- `constexpr float COMMIT_FLASH_AMPLITUDE = 0.80f;` safe [0.60, 0.80]
- `constexpr float COMMIT_FLASH_CADENCE_MS = 200.0f;` safe [150, 250]

**PEAT/Harding FPA gate** (AC-COMMIT-FLASH-CADENCE PEAT formal gate): deferred to Polish per presentation §8; manual evidence in `production/qa/evidence/story-010-commitment-tell-evidence.md`.

**IMPORTANT — `MeshMaterialDynamic` resolution pattern**:

The pseudocode above uses `MeshMaterialDynamic->SetScalarParameterValue(...)` without showing how the pointer is resolved. Standard UE pattern for a mesh-material dynamic write:

```cpp
// In PlayerLaneMovementComponent.h — private field:
UPROPERTY()
TObjectPtr<UMaterialInstanceDynamic> MeshMaterialDynamic;

// In PlayerLaneMovementComponent.cpp — BeginPlay, after CachedMeshComponent resolution:
if (IsValid(CachedMeshComponent))
{
    // Slot 0 = the primary material on the voxel mesh. Art Bible authors
    // LeadingFaceFlash as a scalar param on this material.
    MeshMaterialDynamic = CachedMeshComponent->CreateAndSetMaterialInstanceDynamic(0);
    if (!IsValid(MeshMaterialDynamic))
    {
        UE_LOG(LogPlayerMovement, Warning,
            TEXT("Story 010: CreateAndSetMaterialInstanceDynamic returned null. "
                 "Commitment-tell flash will silently no-op; counter still increments."));
    }
}
```

Null-check `MeshMaterialDynamic` at every write site (both the peak-set in `TriggerCommitmentTell` and the per-tick decay ramp). The Story 010 tests can construct a `NewObject<UMaterialInstanceDynamic>()` manually and assign it via friend access to exercise the write path without a real mesh material asset.

**Include** `#include "Materials/MaterialInstanceDynamic.h"` in the .cpp only (forward-declare `class UMaterialInstanceDynamic;` in the .h).

**Slot 0 assumption**: the story assumes the voxel mesh material sits on slot 0. If the Art Bible authors it on a different slot, the CreateAndSetMaterialInstanceDynamic call must use that slot index. Document the assumption inline with a `// Slot 0 = per Art Bible authoring — confirm with art-director if this changes` comment near the resolution.

**Story 011 audio parallel**: Story 011 will fire the slip-audio cue on the same SETTLED→SLIPPING trigger site. Coordinate the tick-order — Story 010 comes first per this story's line 96 F-6 ordering note; Story 011 should insert AFTER Story 010's commitment-tell but BEFORE any tick-end housekeeping.

---

## Out of Scope

- Story 003: `TriggerCommitmentTell` stub (this story implements).
- Story 008: COUNTDOWN reset of `commitment_tell_fire_count`. Extension: also reset flash lifecycle state — add to Story 008's SnapToTargetAndReset or this story augments.
- Story 011: Slip audio cue firing (parallel presentation dispatch on the same SETTLED→SLIPPING trigger site).
- Art Bible: `LeadingFaceFlash` material param authoring — assumed present per Art direction.
- Settings GDD (R12a-PENDING): `commitment_tell_flash_enabled` toggle — assumed default true.

---

## QA Test Cases

*Automated: `Source/SLIPSTORM/Tests/Integration/PlayerMovement/PMCommitmentTellTest.cpp` (cadence cap + counter logic; actual path per UE convention — story-doc's earlier `tests/unit/...` reference was aspirational).*
*Manual evidence: `production/qa/evidence/story-010-commitment-tell-evidence.md` (screenshot + lead sign-off for visual; PEAT gate placeholder for Polish).*

### Automated

- **Counter increments on every transition (Setup A cadence-suppressed)**:
  - Given: PM SETTLED at Center; SLIP_TWEEN_DURATION_S = 0.10; simulate 12 SETTLED→SLIPPING→CompleteTween cycles within 1.0s (buffer-flush chain).
  - When: measure `commitment_tell_fire_count` after 2.0s.
  - Then: counter reflects ALL 12+ transitions (independent of visual suppression); at least 50% of transitions had visual suppressed (cadence cap engaged).
  - Edge cases: measure `time_last_flash_zero_s` update pattern to confirm cap arithmetic.

- **Setup B (cap-off, isolated slips)**:
  - Given: 5 SETTLED→SLIPPING transitions separated by ≥500ms.
  - When: each fires.
  - Then: all 5 visual flashes render (material param written to ±0.80 for each); cadence ≤2 fires/sec.

- **AC-29 amplitude verification**:
  - Given: single transition.
  - When: `TriggerCommitmentTell` invoked.
  - Then: material param `LeadingFaceFlash` == +0.80 or -0.80 (per direction sign); 2-frame hold (at 60fps = 33.3ms); then 50ms decay to 0.
  - Edge cases: (a) Left → Center → sign +0.80; (b) Right → Center → sign -0.80.

- **Cadence cap suppression**:
  - Given: transition fires at t=0; next transition at t=100ms (within 200ms window).
  - When: second transition.
  - Then: counter incremented; visual NOT written; `time_last_flash_zero_s` not updated (still points to first transition's decay-end).

- **Cadence cap release**:
  - Given: transition fires at t=0; decay ends at t=83ms (2-frame hold + 50ms); next transition at t=350ms.
  - When: second transition (350 - 83 = 267 > 200ms window).
  - Then: visual RENDERS; new decay starts.

- **Counter reset on COUNTDOWN** (integration with Story 008):
  - Given: `commitment_tell_fire_count = 8` post-run.
  - When: COUNTDOWN broadcast.
  - Then: counter == 0; flash lifecycle state cleared.

- **R12a-PENDING accessibility opt-out** (placeholder — Settings GDD unauthored):
  - Given: `IsCommitmentTellFlashEnabled() == false` (mock).
  - When: `TriggerCommitmentTell` invoked.
  - Then: counter incremented; material param NOT written.

### Manual

- **Manual check: AC-29 visual verification**:
  - Setup: launch PIE at 60fps with default SLIP_TWEEN; input single slip.
  - Verify: leading face flashes to visible white intensity for ≥2 frames (33ms); decays over ~50ms; total visible ~83ms.
  - Pass condition: screenshot shows flash at peak; sign-off by art-director confirms amplitude matches Art Bible.

- **Manual check: AC-COMMIT-FLASH-CADENCE Setup A visual**:
  - Setup: SLIP_TWEEN=0.10; sustained buffer-flush 2.0s.
  - Verify: fire cadence visibly capped at ≤5 fires/sec; some transitions suppressed.
  - Pass condition: video capture reviewed; PEAT/Harding FPA gate deferred to Polish (this pass is a pre-Polish manual sighting).

---

## Test Evidence

**Story Type**: Visual/Feel
**Required evidence**:
- Automated unit test at `tests/unit/player-movement/pm_commitment_tell_test.cpp` for cadence + counter logic — must pass.
- Manual evidence doc at `production/qa/evidence/story-010-commitment-tell-evidence.md` with screenshot + art-director sign-off.
- PEAT/Harding FPA formal gate deferred to Polish per AC-COMMIT-FLASH-CADENCE.

**Status**: [ ] Not yet created

---

## Dependencies

- **Depends on**: Story 001 (commitment_tell_fire_count field); Story 003 (TriggerCommitmentTell stub + SETTLED→SLIPPING trigger site); Story 008 (COUNTDOWN counter reset — extend to also reset flash lifecycle); Art Bible (material param `LeadingFaceFlash` authored).
- **Unlocks**: HUD banner styling reuses cadence policy (out-of-epic reference).

---

## Completion Notes

**Completed**: 2026-08-02
**Criteria**: 7/7 addressed (5 covered by 8 test commands + 2 explicitly deferred per spec R12a-PENDING). First Visual/Feel story in the epic.

**Deviations**:

- **ADVISORY — R12a-PENDING accessibility toggle deferred per spec**: AC-3 and AC-7 both reference `commitment_tell_flash_enabled`. Story text at line 48 explicitly resolves this as a Settings GDD forward contract with production-shippable "assume `true` if not present" fallback. Toggle field + guard-branch + toggle-test all deferred to the Settings GDD's polish-phase implementation story. **Follow-up needed**: qa-lead should formally log R12a-PENDING as a deferred item so AC-3/AC-7 don't reach release without either implementation or explicit scope removal.

- **ADVISORY — Pawn has no default slot-0 material (BLOCKING bug caught by unreal-specialist)**: `ASlipstormPlayerPawn::MeshComponent` is constructed with no `SetMaterial` call. In headless tests, `CreateAndSetMaterialInstanceDynamic(0)` at BeginPlay returns null; the null-guarded `SetScalarParameterValue` calls silently no-op. TC1/TC5/TC6 write-count assertions would have passed trivially (false positive). **Fixed via test-side MID injection**: `PM->MeshMaterialDynamic = NewObject<UMaterialInstanceDynamic>(PM)` at the top of TC1, TC5, TC6 (inside `RunTest` scope which has friend access; the free helper `SpawnPawnWithCurves_CT` doesn't). New TC8 explicitly tests the null-guard path. In PIE the actual voxel mesh material will be assigned via Blueprint / Art Bible; the null-guard is a headless-test-only concern.

- **ADVISORY — TC7 sentinel drift caught by qa-tester**: TC7 initially asserted `time_last_flash_zero_s == 0.0f` after SnapToTargetAndReset, but the impl at cpp:1285 resets to `-1000.0f` (sentinel so next run's first fire always passes the cadence gate). Fixed to assert `-1000.0f` with explanatory comment.

- **ADVISORY — Self-inflicted XML-tag build error at test file EOF**: I accidentally left `</content></invoke>` XML tool-invocation closing tags at the end of the test file when constructing it with the Write tool. Build 16 caught it; removed in build 17. **Workflow lesson**: large multi-hundred-line Write-tool outputs have nonzero risk of trailing artifacts that only surface at compile time. Reading the tail of any Write-tool-generated file before invoking the build catches this class quickly.

- **ADVISORY — Test path drift (recurring across all 8 stories this session, corrected in-place)**: Story stated `tests/unit/player-movement/pm_commitment_tell_test.cpp`. Actual `Source/SLIPSTORM/Tests/Integration/PlayerMovement/PMCommitmentTellTest.cpp` (UE convention + Integration folder for UWorld-spawn tests). Story text corrected to actual path with note that earlier reference was aspirational. Story template needs project-wide fix to stop the recurrence at story-creation time.

**Test Evidence**:
- **Automated**: `Source/SLIPSTORM/Tests/Integration/PlayerMovement/PMCommitmentTellTest.cpp` — 8 test commands:
  - TC1 `ac29_amplitude_and_lifecycle_timing` — AC-29 peak + 2-frame hold + 50ms decay lifecycle
  - TC2 `direction_sign_left_and_right` — sign direction verification (both directions)
  - TC3 `counter_always_increments_setup_a` — Setup A: 5 fires, 5 counter increments, 0 material writes
  - TC4 `cadence_cap_suppression` — 2nd fire within 200ms window → counter++, visual suppressed
  - TC5 `cadence_cap_release` — 2nd fire outside 200ms window → visual renders
  - TC6 `setup_b_isolated_slips` — 5 fires ≥500ms apart → all 5 render at peak
  - TC7 `snap_reset_clears_flash_state` — SnapToTargetAndReset resets 5 lifecycle fields (post-fix asserts `-1000.0f` sentinel)
  - TC8 `null_mid_null_guard_no_ops_write` — null MID null-guard verification (added via code-review fix)
- **Manual**: `production/qa/evidence/story-010-commitment-tell-evidence.md` — 3 manual checks scaffolded (AC-29 peak, Setup A cadence, R12a-PENDING opt-out) with sign-off tables. Evidence capture pending PIE session.
- **PEAT/Harding FPA formal gate**: deferred to Polish per presentation §8 (documented in evidence doc's Polish-Phase Deferrals section).

**Build verification**: `Result: Succeeded` (9.60 s incremental post-fix; 0 errors, 0 warnings). Build sequence: 16 failed (XML tags), 17 clean (tags removed), 18 failed (helper-scope access to private member), 19 clean (MID injection in test body scope).

**Code Review**: Complete — `/code-review` with `unreal-specialist + qa-tester` in parallel. Initial verdict: **CHANGES REQUIRED** with 2 BLOCKING findings converged across different reviewers:

1. **unreal-specialist BLOCKING**: `SlipstormPlayerPawn` has no default slot-0 material → `CreateAndSetMaterialInstanceDynamic(0)` returns null in headless → all material writes silently no-op → TC1/TC5/TC6 assertions were false-positive.
2. **qa-tester BLOCKING**: TC7 assertion contradicted implementation (`0.0f` vs `-1000.0f` sentinel).

All 4 fixes applied inline:
- TC7 assertion corrected to `-1000.0f`
- MID injection via friend access at TC1/TC5/TC6 (inside `RunTest` scope)
- New TC8 `null_mid_null_guard_no_ops_write` for explicit null-guard coverage
- Story doc test path corrected

Post-fix build: `Result: Succeeded`. No deferred items beyond the spec-deferred R12a-PENDING toggle.

**Notable — parallel-reviewer pattern validation**: unreal-specialist and qa-tester each caught a blocking bug the other missed. Different classes of issue: engine-level (headless MID resolution) vs test-assertion (sentinel drift). This validates spending the review cost on parallel specialist agents rather than a single reviewer.

**Files touched**:
- `Source/SLIPSTORM/Player/PlayerLaneMovementComponent.h`: forward decl `UMaterialInstanceDynamic`, 2 tuning constants (`COMMIT_FLASH_AMPLITUDE`, `COMMIT_FLASH_CADENCE_MS`), 5 flash lifecycle state fields, `MeshMaterialDynamic` UPROPERTY, `CommitmentTellFlashWrite_TestOnlyCallCount` test counter, `friend class FPMCommitmentTellTest`
- `Source/SLIPSTORM/Player/PlayerLaneMovementComponent.cpp`: `Materials/MaterialInstanceDynamic.h` include, BeginPlay MID resolution (cpp:112-127), per-tick flash lifecycle advance inside Rule 5 gate (cpp:352-388), `TriggerCommitmentTell` body (cpp:969-1007), `SnapToTargetAndReset` extension zeros 5 fields + material param (cpp:1281-1289)
- `Source/SLIPSTORM/Tests/Integration/PlayerMovement/PMCommitmentTellTest.cpp` (new, ~17 KB post-fixes): 8 test commands with in-body MID injection pattern
- `production/qa/evidence/story-010-commitment-tell-evidence.md` (new): 3 manual checks scaffold + Polish-phase PEAT deferral


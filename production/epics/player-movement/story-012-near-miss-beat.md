# Story 012: Near-miss beat (Y-dip + audio swell + haptic dispatch) + TriggerNearMissBeat() public API

> **Epic**: Player Movement (Slip)
> **Status**: Complete
> **Layer**: Presentation
> **Type**: Integration
> **Estimate**: 5–7 hours
> **Manifest Version**: 2026-07-03 (docs/registry/architecture.yaml v8)
> **Last Updated**: 2026-08-03

## Context

**GDD**: `design/gdd/player-movement-presentation.md` (§3.2 Near-Miss Beat, §8 AC-NEARMISS-HAPTIC, R11a-12 opt-in near-miss haptic accessibility setting).
**Requirement**: `TR-PM-030` (near-miss haptic `EHapticEvent::NearMiss` when `IGameSettings::IsNearMissHapticEnabled() && TriggerNearMissBeat()`).
*(Requirement text lives in `docs/architecture/tr-registry.yaml` — read fresh at review time)*

**ADR Governing Implementation**: ADR-0009 (primary — TriggerNearMissBeat public API on `UPlayerLaneMovementComponent`), ADR-0002 (Proposed — interface INT-002-amended 2026-06-26 stable; `EHapticEvent::NearMiss` enum value added at INT-002).
**ADR Decision Summary**: PM exposes `TriggerNearMissBeat()` public method called by Pull-Wave's Rule 11 near-miss detector. Behavior: (a) start avatar Y-dip animation 2-3% over ~33ms, return over 80ms (component-local mesh Z offset); (b) dispatch audio swell (600 Hz–1.6 kHz breath, 200-300ms, -6dB relative to slip cue); (c) dispatch `EHapticEvent::NearMiss` via `IHapticDispatch::Fire` gated by AND-composition of `IHapticDispatch::IsSystemHapticsEnabled()` (OS-state cert compliance) AND `IGameSettings::IsNearMissHapticEnabled()` (accessibility opt-in, default OFF).

**Engine**: Unreal Engine 5.7 | **Risk**: LOW at implementation level.
**Engine Notes**: **ADR-0002 is Proposed but its interface (INT-002-amended 2026-06-26) is stable per architecture-review-2026-07-03 §INT-007 pragmatic-promotion rationale + ADR-0009 Depends-On field line 47.** PM compiles against `IHapticDispatch` regardless of ADR-0002's status label. HW-verification gate (physical iPhone 16 + Galaxy S24 NearMiss FULL/DURATION distinctness pass) is deferred to Polish per ADR-0002 lines 302 + 329.

**Control Manifest Rules (this layer)** — from `docs/registry/architecture.yaml` v8:
- **Required**: Haptic dispatch AND-gated by `IsSystemHapticsEnabled()` (OS/cert) AND `IsNearMissHapticEnabled()` (accessibility setting, default OFF per R11a-12).
- **Required**: Haptic parameters: duration ≤50ms, amplitude ≤0.4 (soft-pulse), single-pulse.
- **Required**: Visual Y-dip additive/independent of tween lean (does NOT go through F-5 clamp path).
- **Required**: Audio swell -6dB relative to slip cue.
- **Required**: Haptic dispatch within ±50ms of visual Y-dip onset per AC-NEARMISS-HAPTIC Setup B.

---

## Acceptance Criteria

*From `design/gdd/player-movement-presentation.md` §3.2 + §8 + R11a-12, scoped to this story:*

- [ ] `void TriggerNearMissBeat()` public method on `UPlayerLaneMovementComponent`:
  - [ ] Immediately begin avatar Y-dip animation (mesh component local Z offset dip 2-3% over ~33ms, return over 80ms). Additive to F-3/F-5/F-6 outputs — write to mesh's Z relative location independently of X (lane) and rotation (lean).
  - [ ] Dispatch audio swell: `PlayNearMissAudioSwell()` (presentation → audio bus).
  - [ ] Haptic dispatch guard: `if (IHapticDispatch::IsSystemHapticsEnabled() && IGameSettings::IsNearMissHapticEnabled()) { IHapticDispatch::Fire(EHapticEvent::NearMiss); }`
- [ ] Y-dip lifecycle owned by a small tick-driven state machine on PM:
  - [ ] `float y_dip_time_s = 0.0f`; `bool y_dip_active`; `float y_dip_peak_offset_cm` (2-3% of mesh height — placeholder constant `Y_DIP_PEAK_CM = 3.0f`).
  - [ ] Phase 1 (0-33ms): linear ramp from 0 to peak (down).
  - [ ] Phase 2 (33-113ms): linear return to 0.
  - [ ] Phase 3: `y_dip_active = false`.
- [ ] Public API: `TriggerNearMissBeat()` callable by Pull-Wave (out-of-epic caller); called at least once during a near-miss event.
- [ ] Haptic parameters per R11a-12: `EHapticEvent::NearMiss` bridge interprets as sub-50ms soft pulse; amplitude ≤0.4. (Amplitude / duration authoring lives inside the ADR-0002 platform bridge implementation — PM only dispatches the vocabulary enum value.)
- [ ] **AC-NEARMISS-HAPTIC Setup A (default off)**:
  - Given: `IGameSettings::IsNearMissHapticEnabled() == false`.
  - When: 10 near-miss triggers.
  - Then: ZERO `EHapticEvent::NearMiss` dispatches; visual Y-dip + audio swell still fire.
- [ ] **AC-NEARMISS-HAPTIC Setup B (opt-in on)**:
  - Given: `IGameSettings::IsNearMissHapticEnabled() == true`; `IsSystemHapticsEnabled() == true`.
  - When: 10 near-miss triggers with ≥200ms gaps.
  - Then: exactly 10 haptic dispatches; each within ±50ms of visual Y-dip onset; soft-pulse params (duration ≤50ms, amplitude ≤0.4 — assertion on the bridge stub).
- [ ] **AC-NEARMISS-HAPTIC Setup C (runtime toggle)**:
  - Given: Start with setting off, mid-session toggle to true, trigger 3 near-misses (all dispatch), toggle back to false, trigger 3 more (zero dispatch).
  - Then: dispatch count exactly 3.
- [ ] **OS-state cert compliance (B-CERT-2)**: dispatch AND-gated by `IsSystemHapticsEnabled()` — verify iOS Focus / Android DND suppression path.
- [ ] `IsNearMissHapticEnabled()` toggle default is `false` (accessibility opt-in). Toggle setting label ("Near-Miss Haptic Feedback") + description ("Adds a soft vibration when waves pass close.") documented as a forward contract to the Accessibility Settings GDD (unauthored).

---

## Implementation Notes

*Derived from presentation §3.2 + R11a-12 + ADR-0002 INT-002-amended interface:*

**TriggerNearMissBeat body**:
```cpp
void UPlayerLaneMovementComponent::TriggerNearMissBeat()
{
    // Start Y-dip
    y_dip_active = true;
    y_dip_time_s = 0.0f;

    // Audio dispatch
    PlayNearMissAudioSwell();

    // Haptic dispatch — AND-gated
    if (IHapticDispatch::IsSystemHapticsEnabled()
        && IGameSettings::IsNearMissHapticEnabled())
    {
        IHapticDispatch::Fire(EHapticEvent::NearMiss);
    }
}
```

**Y-dip tick advance** (added to TickComponent after F-6, alongside commitment-tell decay):
```cpp
if (y_dip_active)
{
    y_dip_time_s += effective_dt;
    float offset_cm = 0.0f;
    if (y_dip_time_s <= 0.033f)
    {
        offset_cm = -Y_DIP_PEAK_CM * (y_dip_time_s / 0.033f);
    }
    else if (y_dip_time_s <= 0.113f)
    {
        offset_cm = -Y_DIP_PEAK_CM * (1.0f - (y_dip_time_s - 0.033f) / 0.080f);
    }
    else
    {
        y_dip_active = false;
        offset_cm = 0.0f;
    }
    // Composed with F-3's X write via a separate SetRelativeLocation call, OR
    // composed inside the F-3 write. Simplest: track y_dip_offset_cm and add
    // into the F-3 SetRelativeLocation call at the mesh write site.
    y_dip_offset_cm = offset_cm;
}
else
{
    y_dip_offset_cm = 0.0f;
}
```

**Mesh write composition** (extend Story 004's F-3 SetRelativeLocation):
```cpp
// Instead of MeshComponent->SetRelativeLocation(FVector(rel_x, 0, 0));
MeshComponent->SetRelativeLocation(FVector(rel_x, 0.0f, y_dip_offset_cm));
```

**Accessibility Settings forward contract**: `IsNearMissHapticEnabled()` accessor lives on a future Settings subsystem. Placeholder if unavailable: hardcoded `false` (matches default) with TODO comment referencing Settings GDD.

**HW-verification deferral note** (ADR-0002 Polish gate): This story implements against the INT-002-stable interface. The actual haptic FEEL (soft-pulse distinctness on iPhone 16 / Galaxy S24) is validated at Polish per ADR-0002 lines 302 + 329. `/story-readiness` should not fail on the ADR-0002 Proposed label — the epic's EPIC.md ADR-0002 row + this story's Engine Notes explicitly document the pragmatic-promotion rationale.

**Tuning knobs** (presentation §7):
- `constexpr float Y_DIP_PEAK_CM = 3.0f;` (2-3% of nominal mesh height; adjust in Art Bible sign-off).
- `IsNearMissHapticEnabled()` default `false`; user-facing setting.

**Performance**: Y-dip advance is O(1) per tick during the 113ms active window (linear ramp + phase check). Audio + haptic dispatch is fire-and-forget on trigger. Mesh SetRelativeLocation call frequency unchanged from Story 004 (Y-dip Z-offset composed into the existing F-3 write, not an additional call). Budget: negligible against 16.6ms frame budget.

---

## Out of Scope

- Story 011: slip cue duck when overlapping this near-miss swell (bidirectional hook — Story 011 owns the duck logic; this story fires the swell that TRIGGERS the duck).
- ADR-0002 HW-verification gate: Polish-phase device evidence — not in this story's scope.
- Settings GDD (unauthored): `IsNearMissHapticEnabled()` toggle UI + copy + persistence — forward contract only.
- Pull-Wave Rule 11 near-miss DETECTION (out-of-epic — Pull-Wave calls TriggerNearMissBeat).

---

## QA Test Cases

*Test file: `tests/integration/player-movement/pm_near_miss_test.cpp`. Automated integration tests with mocked `IHapticDispatch` + `IGameSettings`.*

- **AC-NEARMISS-HAPTIC Setup A (default off → zero dispatches)**:
  - Given: `IGameSettings::IsNearMissHapticEnabled() == false`; haptic dispatch spy.
  - When: `TriggerNearMissBeat()` invoked 10 times.
  - Then: 0 `EHapticEvent::NearMiss` dispatches. Y-dip animation fires 10 times (active flag cycles); audio swell dispatched 10 times.

- **AC-NEARMISS-HAPTIC Setup B (opt-in on → dispatch timing)**:
  - Given: setting on; `IsSystemHapticsEnabled() == true`.
  - When: 10 triggers spaced 200ms apart.
  - Then: 10 haptic dispatches; each dispatch timestamp within ±50ms of Y-dip onset (`y_dip_active` set true at the same tick as dispatch call).

- **AC-NEARMISS-HAPTIC Setup C (runtime toggle)**:
  - Given: setting off; trigger 3 (0 dispatches); toggle to true; trigger 3 (3 dispatches); toggle to false; trigger 3 (0 dispatches).
  - When: verify dispatch count.
  - Then: total = 3.

- **OS-state cert compliance (B-CERT-2)**:
  - Given: `IsSystemHapticsEnabled() == false` (simulating iOS Focus / Android DND); setting on.
  - When: 5 triggers.
  - Then: 0 dispatches. AND-gate suppresses regardless of accessibility setting.

- **Y-dip lifecycle**:
  - Given: fresh PM at Center, `TriggerNearMissBeat()` invoked at t=0.
  - When: simulate ticks up to t=113ms + 1 frame.
  - Then: (a) t=0-33ms: `y_dip_offset_cm` linearly increases (in magnitude) toward -3.0cm; (b) t=33-113ms: `y_dip_offset_cm` linearly returns to 0; (c) post-113ms: `y_dip_active == false`; `y_dip_offset_cm == 0`.
  - Edge cases: multiple TriggerNearMissBeat calls in rapid succession — restarts the Y-dip (verify the animation doesn't stack additively beyond peak).

- **Audio swell independence**:
  - Given: `TriggerNearMissBeat()` invoked; audio bus spy records dispatch.
  - When: verify spy.
  - Then: `PlayNearMissAudioSwell` dispatched with -6dB relative to authored slip cue level.

- **Y-dip additive with F-3/F-5/F-6**:
  - Given: PM SLIPPING mid-tween with F-3 relative X = -50; Y-dip peaks at -3cm (t≈33ms).
  - When: read mesh `GetRelativeLocation()`.
  - Then: X == -50.0cm; Z == -3.0cm ± 0.05cm. Y-dip does not affect X.

- **ADR-0002 interface consumption compile check**:
  - Compile-only test: PM includes `IHapticDispatch` header and calls `Fire(EHapticEvent::NearMiss)` — build succeeds. Regression-catches ADR-0002 interface drift.

---

## Test Evidence

**Story Type**: Integration
**Required evidence**:
- Automated integration test at `tests/integration/player-movement/pm_near_miss_test.cpp` — must pass. Uses mocked `IHapticDispatch` + `IGameSettings` for gate assertions.
- ADR-0002 HW-verification device evidence deferred to Polish per epic EPIC.md + ADR-0002 lines 302/329.

**Status**: [x] Created — `Source/SLIPSTORM/Tests/Integration/PlayerMovement/PMNearMissTest.cpp` (11 test commands; 0 errors, 0 warnings). Actual path uses project convention `Source/SLIPSTORM/Tests/…`, not the aspirational `tests/integration/…` shown above (recurring template drift #2).

---

## Dependencies

- **Depends on**: Story 001 (component skeleton + IHapticDispatch bridge available); Story 004 (mesh SetRelativeLocation composition — this story adds Z-offset); Story 011 (slip cue duck hook — near-miss swell triggers duck-if-slip-active).
- **Unlocks**: Pull-Wave Rule 11 near-miss forward contract (out-of-epic — Pull-Wave calls TriggerNearMissBeat and reads current_lane during SLIPPING).

---

## Completion Notes

**Completed**: 2026-08-03
**Criteria**: 9/9 covered (all top-level ACs verified by 11 integration test commands; no DEFERRED items).
**Test Evidence**: Integration test at `Source/SLIPSTORM/Tests/Integration/PlayerMovement/PMNearMissTest.cpp` (11 test commands: Setup A/B/C haptic gates, B-CERT-2 OS-state, Y-dip lifecycle at 5 sample points, Y-dip restart no-stacking, Y-dip additive with F-3, PlayNearMissAudioSwell→EngageDuckIfSlipActive cross-story hook + vacuous case, public API reachability, Y-dip during SETTLED verifies mesh-write hoist, SnapToTargetAndReset item 10 clear coverage). Build clean.
**Code Review**: Complete (2026-08-03). qa-tester GAPS (3 BLOCKING + 3 SUGGESTED + 2 NIT); unreal-specialist agent truncated twice — UE-side review completed manually (CLEAN). All 3 BLOCKING addressed inline; SUGGESTED + NIT deferred per user's BLOCKING-only scope choice. Rebuild clean first-try.

**Advisory deviations** (documented, non-blocking):
1. Test file at project-convention path `Source/SLIPSTORM/Tests/Integration/PlayerMovement/PMNearMissTest.cpp` — story spec's aspirational `tests/integration/player-movement/pm_near_miss_test.cpp` is the known recurring template drift item #2.
2. New `IGameSettings` seam created at `Source/SLIPSTORM/Seam/IGameSettings.h/.cpp` — not in story spec (spec offered "hardcoded false + TODO" fallback). Advisor-recommended because ACs Setup A/B/C literally require toggling the setting. Mirrors IHapticDispatch spy-swap pattern exactly.
3. `EHapticEvent::NearMiss = 1` added to Story 005-owned enum — completing the reserved ordinal per `IHapticDispatch.h` header comment lines 41–42. Not scope creep.
4. F-3 mesh `SetRelativeLocation` hoisted out of the SLIPPING-only branch to a unified state-agnostic call after both SLIPPING/SETTLED branches. Advisor caught the bug in the story spec's original inline sample (Y-dip Z-offset would have been invisible during SETTLED). Mirrors Story 007's rotation-write hoist precedent. Zero-cost consequence: `SetRelativeLocation(0,0,0)` fires once per SETTLED tick when nothing active. Verified no `PMLateralInterpolation*Test.cpp` asserts SetRelativeLocation call-count during SETTLED.
5. `SnapToTargetAndReset` extended with item 10 (Y-dip audio-state clear) on COMPLETE/ABORTED/COUNTDOWN entry — mirrors items 8/9 precedent from Stories 010/011. Eliminates latent bug where mid-animation Y-dip could survive terminal-state entry.
6. Y-dip lifecycle advance placed BEFORE the unified mesh write (current-tick responsiveness). Differs from Story 010 flash lifecycle and Story 011 envelope advance placement (both AFTER mesh writes) — those subsystems don't feed the mesh write. Y-dip does, so it must advance first to avoid one-frame render latency AND Phase-3 residual persistence.

**Session note**: Second consecutive story where inline implementation succeeded first-try (modulo one 32-bit float literal fix in the test file). Advisor's upfront call caught the real design bug in the story spec (F-3 hoist necessity) — same pattern that paid off in Story 011. unreal-specialist agent truncation is a new failure mode not seen before this session — worth watching if it recurs.

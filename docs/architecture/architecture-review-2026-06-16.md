# Architecture Review — 2026-06-16

**Mode:** `/architecture-review engine` (engine compatibility audit only)
**Engine:** Unreal Engine 5.7 (pinned 2026-06-16 via `/setup-engine`)
**Scope:** Validate that the 3 existing ADRs (all Proposed) correctly target UE 5.7 after the engine pin was formally written to `CLAUDE.md` and `docs/engine-reference/unreal/VERSION.md`.

ADRs reviewed:
- ADR-0001: Palm Rejection (R_max) — `docs/architecture/adr-0001-palm-rejection-rmax-calibration.md`
- ADR-0002: Haptic Platform Bridge — `docs/architecture/adr-0002-haptic-platform-bridge.md`
- ADR-0003: Drain Queue Architecture — `docs/architecture/adr-0003-drain-queue-architecture.md`

Engine reference consulted:
- `docs/engine-reference/unreal/VERSION.md`
- `docs/engine-reference/unreal/breaking-changes.md`
- `docs/engine-reference/unreal/deprecated-apis.md`
- `docs/engine-reference/unreal/modules/input.md`

---

## Engine Audit Summary

| Check | Result |
|---|---|
| ADRs with Engine Compatibility section | 3/3 ✅ |
| Engine version consistency (all UE 5.7) | 3/3 ✅ |
| Post-Cutoff APIs Used field populated | 3/3 ✅ |
| References Consulted field populated | 3/3 ✅ |
| Deprecated API references found | 0 ✅ |
| Stale engine-version references | 0 ✅ |

---

## Findings

### 🟡 ADR-0002-F1 — Android min API mismatch (MEDIUM RISK)

**Source:** ADR-0002 §Constraints and §Decision → §HAPTIC_CAPABILITY Detection Logic

- ADR-0002 Constraints claim: *"Android minimum API level: **21 (Android 5.0)** — standard mobile game minimum"*
- ADR-0002 Detection Logic step 2: *"API level < 26 (Android 7.x or earlier) → DURATION (VibrationEffect and hasAmplitudeControl() unavailable before API 26)"*
- ADR-0002 §Android DURATION Tier Patterns: *"API 21–25 or no amplitude control"*

**Engine reality:** `docs/engine-reference/unreal/breaking-changes.md` §Mobile — *"UE 5.7: Minimum Android API level raised to 26 (Android 8.0)"*

**Impact:** The API-21–25 deployment target is unreachable under UE 5.7 — the engine itself will not build/deploy to those devices. The "API level < 26 → DURATION" detection branch is dead code in shipping; only "API ≥ 26, hasAmplitudeControl() == false → DURATION" remains a real branch (for budget devices that ship Android 8+ with ERM actuators).

**Severity:** MEDIUM. Not implementation-blocking — the ADR's logic still works correctly on API 26+ devices. But it carries misleading constraints that could waste design effort if read literally during implementation.

**Recommended fix:**
1. Update Constraints line to: *"Android minimum API level: **26 (Android 8.0)** — UE 5.7 enforced minimum"*
2. Collapse Detection Logic step 2 (the `< 26 → DURATION` branch) into a note that this branch is unreachable under UE 5.7; the DURATION tier is reached only via the `hasAmplitudeControl() == false` branch.
3. Remove the §Android DURATION Tier Patterns subhead's "API 21–25" framing; keep the same vibrate-without-amplitude patterns but reframe as the no-amplitude-control fallback.

### 🟡 ADR-0002-F2 — iOS min OS mismatch (MEDIUM RISK)

**Source:** ADR-0002 §Constraints

- ADR-0002 claims: *"iOS minimum: **iOS 13** — enables CHHapticEngine.capabilitiesForHardware() for hardware detection"*

**Engine reality:** `docs/engine-reference/unreal/breaking-changes.md` §Mobile — *"Minimum iOS deployment target raised to iOS 14"*

**Impact:** iOS 13 deployment is unreachable under UE 5.7. iOS 14 is a superset of iOS 13 for the haptic APIs used here (`CHHapticEngine`, `UIImpactFeedbackGenerator`, `UINotificationFeedbackGenerator` — all iOS 13+), so no logic change required. Cleanup-only.

**Recommended fix:** Update Constraints line to: *"iOS minimum: **iOS 14** — UE 5.7 enforced minimum; CHHapticEngine.capabilitiesForHardware() available since iOS 13 and unchanged"*

### ✅ ADR-0001 — No engine compatibility issues
All Post-Cutoff APIs documented (`UITouch.majorRadius`, `MotionEvent.getTouchMajor()`). No deprecated APIs referenced. Verification Required field correctly flags pre-implementation checks for `FIOSPlatformInputInterface` and `GameActivity` patterns under UE 5.7. The `IInputProcessor` Slate pattern used here is not flagged in `deprecated-apis.md` and remains the correct path for raw touch capture (Enhanced Input does not expose contact radius — bypassing it is intentional and correct).

### ✅ ADR-0003 — No engine compatibility issues
Post-Cutoff APIs documented (`FSlateApplication::Get().GetPlatformApplication()->SetMessageHandler()`, `FTSTicker::GetCoreTicker().AddTicker()`). No deprecated APIs referenced. Verification Required field correctly flags `FTSTicker` game-thread behavior on UE 5.7 mobile as a pre-implementation check. The non-UObject `IInputProcessor` proxy pattern is consistent with the engine reference's input module guidance.

---

## GDD Revision Flags

**None.** `design/gdd/input-system.md` does not assert OS minimum versions. The API-21 and iOS-13 claims are purely ADR-0002 design assumptions inherited from generic "standard mobile game" reasoning, not from the GDD. Only the ADR needs cleanup.

---

## Engine Specialist Consultation

Skipped for this scope. The 2 findings are direct text matches between ADR claims and `breaking-changes.md` and do not require specialist judgment. A follow-up `unreal-specialist` pass is optional if a deeper anti-pattern review of the 3 ADRs is desired (e.g., whether the Slate `IInputProcessor` pre-processor pattern in ADR-0003 is the best UE 5.7 path for mobile touch on iOS Metal / Android Vulkan rendering paths).

---

## Verdict: **CONCERNS** (not blocking)

- All 3 ADRs correctly target UE 5.7 with documented Post-Cutoff APIs and Verification Required gates.
- 2 stale platform-min claims in ADR-0002 — cosmetic cleanup, not implementation-blocking.
- All 3 ADRs remain `Status: Proposed` with mandatory hardware verification gates before promotion to `Accepted`. Those gates already exercise the real API-level realities at deploy time, so the ADR-0002 cleanup is a documentation-quality fix rather than a correctness fix.

---

## Recommended Actions

1. **Author fix to ADR-0002** — single-ADR text edit; see ADR-0002-F1 and ADR-0002-F2 above for exact lines.
2. **No re-review required** after the fix — text cleanup only, no logic changes.
3. **Defer hardware verification gates** to the implementation sprint as already specified in each ADR.

---

## Out of Scope (not run in this review)

- Phase 2 (technical requirements extraction from GDDs) — engine mode only
- Phase 3 (traceability matrix) — engine mode only
- Phase 4 (cross-ADR conflict detection) — engine mode only
- Phase 3b (story/test linkage / RTM) — engine mode only
- Phase 6 (architecture document coverage) — engine mode only

If a full review is wanted before Pre-Production gate, run `/architecture-review` (no args) after the ADR-0002 cleanup lands.

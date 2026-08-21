# Tech Debt Register

Auto-created by `/story-done`. Each entry is a known deviation from design or architecture
that was accepted at close time. Review and address before the next phase gate.

---

- **2026-08-21** (Story 007 — RSM/DPC Integration): AC-WS-18 lifecycle → Cold (AC text said "→ Idle"). ADR-0011 D3 authoritative: Flushing→Idle is a forbidden transition. Story AC text corrected; implementation uses Cold. Reconcile AC-WS-18 text in GDD if/when GDD revision is triggered. — tracked from `production/epics/wave-spawner/story-007-rsm-dpc-integration.md`

- **2026-08-21** (Story 007 — RSM/DPC Integration): Rule 13 pause path goes Active→Flushing directly (not via Holding as `IsValidTransition` doc comment described). Deviation is correct for UX (live waves must not persist during pause screen); DEVIATION NOTE added in HandlePausedChanged; IsValidTransition doc comment corrected. No functional risk. — tracked from `production/epics/wave-spawner/story-007-rsm-dpc-integration.md`

- **2026-08-21** (Story 007 — RSM/DPC Integration): `kResumeGraceS = 1.5f` is a stub constant; should load from `UWaveSpawnerConfig` data asset. Also should eventually converge with `RSMSubsystem->IsResumeGrace()` (ADR-0007) to avoid veto-window desync across systems. TODO(RSM epic) cross-reference added to header and .cpp. — tracked from `production/epics/wave-spawner/story-007-rsm-dpc-integration.md`

- **2026-08-21** (Story 007 — RSM/DPC Integration): `GetRunSeed()` cross-boundary stub on `URunStateMachineSubsystem` returns 0 (coordinator-approved). Will return the real seed when the RSM epic is implemented. `TODO(RSM epic)` comment in stub. Stub ensures Story 007 compiles and tests run headlessly; PatternRNG seeds from 0 (deterministic but not game-meaningful) until RSM epic lands. — tracked from `production/epics/wave-spawner/story-007-rsm-dpc-integration.md`

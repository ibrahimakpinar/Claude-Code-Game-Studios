# Smoke Test: Critical Paths — SLIPSTORM

**Purpose**: Run these checks in under 15 minutes before any QA hand-off.
**Run via**: `/smoke-check` (which reads this file)
**Update**: Add new entries when new core systems are implemented.
**Target hardware**: Mid-tier mobile (per technical-preferences.md — 6-inch screen, sustained 60fps target).

## Core Stability (always run)

1. UE project opens to the SLIPSTORM main menu map without crash (PIE + packaged build on target device)
2. New 60-second run can be started from the main menu via touch input
3. Main menu responds to touch input without freezing or dropped touches

## Core Mechanic — to be expanded per sprint

<!--
Add the primary mechanic check for each sprint here as it is implemented.
Format: short imperative sentence, ≤1 minute to verify, observable from screen.
-->

4. **(Placeholder — first PM sprint)** Player can lean left / right via touch; lean visual matches input direction; SLIP_TWEEN engages and decays without judder.
5. **(Placeholder — first Pull-Wave sprint)** A barrage pulls a wave toward the player at the configured speed; telegraph window appears before pull; player can escape a barrage by leaning the correct direction within the telegraph window.
6. **(Placeholder — first DPC sprint)** Difficulty phase advances on the configured cadence; entering PEAK phase produces a visible barrage-density spike; M=3 PEAK is not dispatched while watchdog hardware-grace window is active.
7. **(Placeholder — first RSM sprint)** Run starts on tap, advances through PRE-RUN → RUN → POST-RUN states, ends at the 60-second mark with the death-or-survive outcome screen.

## Data Integrity

8. Run salt / RNG seed is stable across pause-resume within a single run (once RSM `RunSeed:uint64` is wired)
9. Settings persistence: accessibility toggles (e.g. `near_miss_haptic_enabled`) survive app cold restart (once Accessibility Settings GDD is authored + implemented)

## Performance

10. Sustained 60fps on target mid-tier device through a full 60-second run (no frame drops below 55fps for >100ms)
11. No memory growth over 5 minutes of continuous play (Memreport baseline + delta ≤ 50MB)
12. Thermal: device skin temperature does not exceed thermal-throttle threshold over 3 consecutive 60-second runs

## Accessibility (advisory; required at Polish gate)

13. Performance mode banner ("Performance mode — hardest barrage suppressed.") displays correctly below OS top safe-area on notched-iOS device when watchdog suppresses M=3 PEAK
14. Near-miss haptic toggle (default off) suppresses the near-miss haptic event when disabled

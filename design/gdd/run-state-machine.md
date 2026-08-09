# Run State Machine

> **Status**: Approved (revised ×6)
> **Author**: ibrahimakpinar + agents
> **Last Updated**: 2026-05-06
> **Implements Pillar**: Pillar 3 (Sixty Seconds Is the Whole Game), Pillar 5 (Skill Is Visible)

## Overview

The Run State Machine is the lifecycle authority for a single SLIPSTORM run. It owns the run's current state (`ERunState`: IDLE, COUNTDOWN, RUNNING, DEAD, COMPLETE, RESOLVING, ABORTED) and the canonical run timer — a monotonic countdown from `RUN_DURATION_S` to zero. App-suspension is represented as an orthogonal boolean flag (`bool is_paused`) rather than a discrete state; the flag freezes the timer and suspends gameplay without interrupting the state enum. Every gameplay system that needs to know whether a run is active, or how much time remains, reads from the Run State Machine; nothing writes to it except the discrete events that cause state transitions (run initiated, death collision confirmed, timer expired, player dismisses score screen, app suspended). The Run State Machine does not generate gameplay events — it responds to them and broadcasts its new state via `OnStateChanged`. It has no awareness of game entities, player position, or hazard state. Its outputs are the current state enum, the `is_paused` flag, the `remaining_time` value, and the `run_outcome` field (`ERunOutcome`). RESOLVING is a score reveal phase; the player may tap to skip after a minimum hold (`RESOLVING_SCORE_REVEAL_DURATION_S`), or the state auto-advances at `RESOLVING_AUTO_ADVANCE_S`; a safety watchdog also auto-advances after `RESOLVING_TIMEOUT_S` if neither fires. The dream ends on its own schedule. If a run is interrupted by a stale background pause, ABORTED transitions to IDLE with a brief "run interrupted" acknowledgment ceremony owned by downstream UI and audio. One tap from IDLE starts COUNTDOWN. These are the foundation on which Difficulty & Phase Controller, Collision, Scoring, HUD, and all other downstream systems build their behavior.

## Player Fantasy

Every SLIPSTORM run is a dream the game wakes you from on its own schedule.

The countdown is the dream's first frame — the world assembles in metallic slow-motion and you understand without being told that something is coming. The running state is the dream itself: hypnotic, magnetized, half a second ahead of you no matter what. Death is the dream ending — sudden, cold, the metallic hum cut to silence. Survival is the dream releasing you — the storm running out, the player set down on the other side.

The player never names the state machine. They name the feeling: "I almost made it." "I read that one perfectly." "I can't believe I made it through that." Those sentences are the system's only output. Everything else is plumbing.

When a run is interrupted from outside — backgrounded past its patience — the player's feeling is neither failure nor survival. The dream was deferred, not torn. The ceremony exists to say "it wasn't you." Audio and UI authors: the tone is sympathy, not punishment.

## Detailed Rules

### Core Rules

1. The Run State Machine is the exclusive lifecycle authority for a single run. It owns the current run state and the canonical run timer. No other system may modify either.
2. Exactly one state is active at any time; states are mutually exclusive.
3. The RSM responds to events — it never polls game entities. Transitions are triggered by: player UI input, internal timer expiry, external system events (death confirmed, player tap in RESOLVING), OS app lifecycle callbacks, or `PAUSED_TIMEOUT_S` expiry on app foreground. The COUNTDOWN timer is wall-clock based and pause-aware (see F-4): `t_countdown_start` is sampled via `time_source.GetCurrentTime()` at IDLE → COUNTDOWN entry; COUNTDOWN elapses when `elapsed_countdown_active ≥ COUNTDOWN_DURATION_S`.
4. Run suspension is represented as a separate flag, not a state: `bool is_paused` is independent of `ERunState`. `is_paused` can be true in COUNTDOWN or RUNNING. It is always false in IDLE, DEAD, COMPLETE, ABORTED, or RESOLVING.
5. The run timer is a monotonic countdown computed from the wall clock (`time_source.GetCurrentTime()`), not UE game ticks. All timer arithmetic uses `double` precision throughout. `time_source` is an injected `RunTimeSource` (see Clock Injection). When `is_paused == false`: `remaining = min(RUN_DURATION_S, max(0.0, RUN_DURATION_S − ((time_source.GetCurrentTime() − t_run_start) − total_paused_duration)))`. When `is_paused == true`: return `cached_remaining_at_pause_entry` (frozen; do not evaluate the formula).
6. On app background: set `is_paused = true`, record `t_pause_start = time_source.GetCurrentTime()`, cache `remaining_time` into `cached_remaining_at_pause_entry`. On app foreground: evaluate F-2 first — if `is_stale`, transition to ABORTED (skip F-3). Otherwise: sample `t_resume = time_source.GetCurrentTime()`, accumulate `total_paused_duration += (t_resume − t_pause_start)`, set `is_paused = false`, then allow the first gameplay tick.
7. If `is_paused` remains true for longer than `PAUSED_TIMEOUT_S`, the run is considered stale. On the next app foreground, RSM transitions to ABORTED before resuming any gameplay.
8. If a death-confirmed event and timer expiry arrive in the same tick, **DEAD takes priority** — the player was caught at the wire (see EC-1 for design rationale).
9. If a terminal event (DEAD, COMPLETE) and an app-background event arrive in the same tick, the terminal event takes priority. The run ends; `is_paused` is not set.
10. Once DEAD or COMPLETE is entered, the RSM is idempotent: all subsequent collision events and timer evaluations in the same frame are no-ops.
11. The RSM broadcasts `OnStateChanged(previous_state, new_state, run_outcome, timestamp)` on every state transition. Downstream systems subscribe; they do not poll.
12. The RSM has no knowledge of game entities: it holds no references to the player, waves, or hazard state.
13. **Platform lifecycle delegates**: RSM binds to `FCoreDelegates::ApplicationWillEnterBackgroundDelegate` / `ApplicationHasEnteredForegroundDelegate`. This pair fires only on full app background (home button, App Switcher). Notification banners, Control Center swipes, and incoming-call overlays do not fire these delegates — the run timer continues during such interactions. This is an accepted platform constraint for a 60-second session format. Do NOT use `ApplicationWillDeactivateDelegate` / `ApplicationHasReactivatedDelegate` — that pair fires on transient interruptions including notification banners.
14. **`run_outcome` type**: Run outcome is `ERunOutcome { NONE = 0, DEAD, COMPLETE, ABORTED }`. Initialized to `NONE` at construction. Reset to `NONE` on RESOLVING → IDLE. For ABORTED runs (which never enter RESOLVING), reset to `NONE` on ABORTED → IDLE. Score Persistence reads `run_outcome` during RESOLVING before the RESOLVING → IDLE reset occurs.
15. **`OnStateChanged` implementation**: Broadcast via `DECLARE_MULTICAST_DELEGATE` (not `DECLARE_DYNAMIC_MULTICAST_DELEGATE` — Blueprint exposure and reflection overhead are not required for C++-only subscribers). RSM state transitions must not be triggered from within an `OnStateChanged` subscriber; doing so is a re-entrancy violation and will produce incorrect payload values for remaining subscribers. Re-entrancy is detected via `bool bBroadcastingState`; guard with a runtime conditional — `if (bBroadcastingState) { UE_LOG(LogRSM, Error, TEXT("Re-entrant OnStateChanged — aborting")); return; }` — rather than `check()` or `ensure()`. Both macros are gated by `DO_CHECK`, which is `0` in Shipping builds by default (unless `bUseChecksInShippingBuilds = true` in Target.cs), making them no-ops in production. A runtime conditional is the only Shipping-safe re-entrancy guard.
16. **RSM tick group**: RSM must tick before all systems that read `remaining_time` or `current_state` each tick. The precise mechanism depends on RSM's UE object type (see OQ-7 ADR — required before implementation): if RSM is an `AActor` or `UActorComponent`, it ticks in `TG_PrePhysics` and downstream systems call `AddTickPrerequisiteActor` / `AddTickPrerequisiteComponent`. If RSM is a `UGameInstanceSubsystem` (uses `FTickableGameObject`), tick-group and prerequisite APIs are unavailable — the OQ-7 ADR must specify an alternative ordering mechanism. **OQ-7 is a blocking implementation prerequisite for this rule.**
17. **RUNNING tick order**: At the start of each RUNNING gameplay tick, pending `death_confirmed` events are evaluated before the timer expiry check. If a `death_confirmed` event is present, RSM transitions to DEAD and skips the timer expiry evaluation for that tick. If no `death_confirmed` event is pending, `remaining_time` is then evaluated; if `≤ 0`, RSM transitions to COMPLETE. This ordering ensures that a wave contact at t=60.0s produces DEAD, not COMPLETE — the player's experienced collision takes priority over the simultaneous timer expiry (Pillar 5).
18. **`OnPausedChanged` broadcast**: When `is_paused` changes, RSM broadcasts `OnPausedChanged(bool is_paused, double timestamp)` separately from any `OnStateChanged` broadcast. Downstream systems that react to pause/resume (Audio Controller, Player Movement, Wave Spawner, Collision) subscribe to `OnPausedChanged`. `OnStateChanged` fires only on `ERunState` transitions and does not cover `is_paused` changes. Subscribers to `OnPausedChanged` must not trigger any RSM operation that modifies `is_paused`; re-entrant `OnPausedChanged` broadcasts are forbidden by a separate re-entrancy guard: `bool bBroadcastingPaused`, with the same runtime conditional pattern as Rule 15. Using a separate boolean (not the shared `bBroadcastingState`) ensures that an `OnPausedChanged` subscriber legitimately triggering an `OnStateChanged` broadcast is not silently suppressed by the wrong guard. **Delegate type (2026-06-23 /propagate-design-change Wave Spawner R3a closure — FC-2)**: `OnPausedChanged` is declared as `DECLARE_MULTICAST_DELEGATE_TwoParams(FOnPausedChanged, bool /* is_paused */, double /* timestamp */)` — non-dynamic multicast. Rationale: all current subscribers (Audio Controller, Player Movement, Wave Spawner, Collision) bind in C++ via `AddRaw`/`AddUObject`/`AddLambda`; none require Blueprint exposure. Dynamic multicast (`DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams`) is rejected — it requires `UFUNCTION()`-marked handlers + reflection overhead + cannot bind lambdas (Wave Spawner R2a-2 tick-ordering subscription uses lambda binding inside `UWaveSpawnerSubsystem::Initialize()`). Future Blueprint exposure (if needed for editor tooling or designer-side debugging) would require a parallel dynamic delegate `OnPausedChangedBP` rather than retrofitting this one. **Forward contract on Wave Spawner R3a closure**: Wave Spawner's `Initialize()` binds via `RSM->OnPausedChanged.AddUObject(this, &UWaveSpawnerSubsystem::HandleOnPausedChanged)`.
19. **Resume grace window**: On `is_paused` cleared in RUNNING state, RSM sets `resume_grace = true` and samples `t_grace_start = time_source.GetCurrentTime()`. Grace expires when `time_source.GetCurrentTime() − t_grace_start ≥ RESUME_GRACE_S` is checked each tick — **not via `FTimerManager`** (`FTimerManager` is incompatible with `FakeTimeSource`-based tests; AC-29 requires the clock-injection path). During `resume_grace`: (a) Collision discards `death_confirmed` events; (b) Wave Spawner holds all new spawning; (c) Player Movement freezes movement inputs. Additionally, on every foreground resume during RUNNING (when `is_paused` clears), **Wave Spawner must flush all in-flight waves** — waves whose trajectories began before backgrounding are destroyed on the `OnPausedChanged(is_paused=false)` signal. This is a binding contract this GDD imposes: all in-flight waves are considered past their telegraph window and cannot be read by the returning player (Pillar 2). Wave Spawner GDD must implement this flush. After `RESUME_GRACE_S` elapses, RSM clears `resume_grace = false`. `OnPausedChanged` broadcasts with `is_paused = false` at the start of the grace window (not at the end). `resume_grace` is always `false` in all states other than RUNNING post-resume; foreground events in COUNTDOWN, DEAD, COMPLETE, RESOLVING, or IDLE never set `resume_grace = true`.
20. **Lifecycle callbacks in terminal and inactive states**: `ApplicationWillEnterBackgroundDelegate` and `ApplicationHasEnteredForegroundDelegate` are no-ops when `current_state` is IDLE, DEAD, COMPLETE, ABORTED, or RESOLVING. In IDLE, no run is active; in terminal and post-run states, the run is already over. `is_paused` is not set in any of these states (per Rule 4). See EC-14.

21. **Per-run RNG seed (`RunSeed : uint64`)** *(2026-06-23 /propagate-design-change Wave Spawner R3a closure — FC-1; forward contract from Wave Spawner R1a-3 + CD R1 Ruling 2 closing OQ-WS-4)*: RSM exposes a public read-only `RunSeed : uint64` field captured atomically at the `COUNTDOWN → RUNNING` transition. The seed source is `FPlatformTime::Cycles64()` XOR'd with a per-installation salt (one-time generated at first launch and persisted to user prefs) so the same physical device run produces a fresh seed each time but cross-device determinism is structurally precluded (consistent with EC-WS-15 platform-scoped Death Replay). `RunSeed` is consumed by Wave Spawner at its `Cold → Active` transition (Wave Spawner Rule 8 — "the per-run RNG is seeded at the `Cold → Active` transition from the RSM-supplied `RunSeed : uint64` field"); Death Replay reuses the captured seed to reproduce the run's pattern admission sequence deterministically. The seed is held immutable for the lifetime of the run — replay reads the same value as the initial run. Wall-clock-derived seeds at consumer-side capture are explicitly forbidden (would defeat replay; Wave Spawner AC-WS-13 binding). **Public-interface contract**: `RunSeed` is `UPROPERTY(BlueprintReadOnly, Category="RSM|Run")`; consumers read at the `OnStateChanged(RUNNING)` callback edge or later (reading before COUNTDOWN→RUNNING returns the previous run's seed or zero on first run; consumers must observe the state transition first). **Replay path**: Death Replay restores the captured seed before re-issuing any pattern-admission decision (Wave Spawner's `Cold → Active` re-entry path reuses the same seed value).

### Clock Injection

All wall-clock access in the RSM goes through a single injected interface:

```
RunTimeSource {
    virtual double GetCurrentTime() const = 0   // monotonically non-decreasing wall-clock value (seconds); implementations must never return a value less than a prior return value
    virtual ~RunTimeSource() = default
}
```

This is a non-UObject pure C++ abstract class — **not a UInterface**. Store as `TUniquePtr<RunTimeSource>` in the RSM. The `I` prefix is reserved for UE UInterface types; this class must not use it.

| Implementation | Usage |
|---|---|
| `FAppTimeSource` (production) | Platform-conditional implementation — `FPlatformTime::Seconds()` is **not** sleep-aware on either mobile platform and must not be used directly. On iOS, `FPlatformTime::Seconds()` wraps `mach_absolute_time()`, which pauses when the CPU suspends during device sleep; the correct API is `mach_continuous_time()` (iOS 10+). On Android, `FPlatformTime::Seconds()` wraps `CLOCK_MONOTONIC`, which also pauses during device sleep; the correct API is `clock_gettime(CLOCK_BOOTTIME, ...)`. Both paths require `#if PLATFORM_IOS / #elif PLATFORM_ANDROID` branching in `GetCurrentTime()`. **Do NOT use `FApp::GetCurrentTime()`** — it is a frame-cached value set once per GameThread tick and does not advance during lifecycle callbacks. Using it in F-3 would cause a 5-minute pause to accumulate ~0s, silently corrupting the run timer. |
| `FakeTimeSource` (tests) | Caller-controlled `double` value; advanced manually per test step |

The `RunTimeSource` instance is provided at RSM construction and is not swapped at runtime. All formulas and timer checks reference `time_source.GetCurrentTime()` exclusively — no direct calls to `FPlatformTime::Seconds()` or `FApp::GetCurrentTime()` appear in RSM implementation.

**Timer implementation policy**: All RSM timer evaluations — snap durations (`DEAD_SNAP_DURATION_S`, `COMPLETE_SNAP_DURATION_S`), RESOLVING thresholds (`RESOLVING_SCORE_REVEAL_DURATION_S`, `RESOLVING_AUTO_ADVANCE_S`, `RESOLVING_TIMEOUT_S`), and `resume_grace` expiry — use per-tick `time_source.GetCurrentTime()` comparisons. Do NOT use `FTimerManager` for any RSM timer. `FTimerManager` is incompatible with `FakeTimeSource`-based tests; advancing the fake clock must be sufficient to trigger all timer-dependent transitions (see AC-15, AC-23, AC-27, AC-18, AC-29).

### States and Transitions

| State | Description | Entry Trigger | Valid Exits |
|---|---|---|---|
| **IDLE** | Awaiting run start; shows tap-anywhere prompt | Initial app launch; RESOLVING → IDLE; ABORTED → IDLE | Player taps → COUNTDOWN |
| **COUNTDOWN** | World-assembly beat; pause-aware countdown (F-4) | Player taps Start from IDLE | `elapsed_countdown_active ≥ COUNTDOWN_DURATION_S` → RUNNING; `PAUSED_TIMEOUT_S` exceeded on resume → ABORTED |
| **RUNNING** | Active gameplay; timer counting down | COUNTDOWN elapses | Timer ≤ 0 → COMPLETE; death confirmed → DEAD; `PAUSED_TIMEOUT_S` exceeded on resume → ABORTED |
| **DEAD** | Death snap | Death-confirmed event from Collision | `DEAD_SNAP_DURATION_S` elapses → RESOLVING |
| **COMPLETE** | Run complete snap | Timer ≤ 0 (Rule 17: evaluated first each tick) | `COMPLETE_SNAP_DURATION_S` elapses → RESOLVING |
| **ABORTED** | Stale paused run (timeout exceeded) | `PAUSED_TIMEOUT_S` exceeded on resume from COUNTDOWN or RUNNING | Immediately → IDLE; downstream plays "run interrupted" ceremony; run-start tap not accepted until `ABORTED_CEREMONY_DURATION_S` elapses |
| **RESOLVING** | Post-run score reveal; player may tap to skip after minimum hold, or state auto-advances | DEAD or COMPLETE snap elapses | Player taps after `RESOLVING_SCORE_REVEAL_DURATION_S` → IDLE (tap-to-skip); auto-advance at `RESOLVING_AUTO_ADVANCE_S` → IDLE; `RESOLVING_TIMEOUT_S` watchdog → IDLE |

**`bool is_paused` flag** (orthogonal to `ERunState`):

| Condition | `is_paused` | Effect |
|---|---|---|
| App foregrounded, run active (COUNTDOWN or RUNNING) | `false` | Timer formula evaluates live |
| App backgrounded (COUNTDOWN or RUNNING) | `true` | Timer frozen; returns cached value |
| Any terminal state (DEAD/COMPLETE/ABORTED) | Always `false` | Not applicable after run ends |
| RESOLVING (app backgrounded) | `false` (not set) | Player-tap dismiss input is not processed while backgrounded. On foreground: if player had tapped before backgrounding, dismiss fires on re-entry. F-2 is not evaluated (run is already over). |
| `is_paused == true` for > `PAUSED_TIMEOUT_S` | — | On next foreground: ABORTED → IDLE |

**Per-state invariants:**

| State | `remaining_time` | `run_outcome` |
|---|---|---|
| IDLE | `RUN_DURATION_S` (initialized at construction; F-1 not evaluated) | `NONE` |
| COUNTDOWN | `RUN_DURATION_S` (frozen; countdown uses F-4) | `NONE` |
| RUNNING | `[0, RUN_DURATION_S]`, counting down (or frozen if `is_paused`) | `NONE` |
| DEAD | Frozen at `cached_remaining_at_death_entry` (sampled at RUNNING → DEAD via `time_source.GetCurrentTime()`) | `DEAD` |
| COMPLETE | `0` | `COMPLETE` |
| ABORTED | Frozen (value at abort) | `ABORTED` (reset to `NONE` on → IDLE) |
| RESOLVING | Frozen | `DEAD` or `COMPLETE` (reset to `NONE` on → IDLE) |

### Interactions with Other Systems

**Input System** — RSM sends nothing to Input System. Input System sends nothing to RSM (enforces R-5 from input-system.md). The run-start event and the RESOLVING dismiss tap are separate UI events, not routed through the Input System's gesture pipeline.

**Player Movement** — Reads `current_state`, `is_paused`, and `resume_grace`; processes movement only when state is RUNNING, `is_paused == false`, and `resume_grace == false`. During the `resume_grace` window, movement inputs are frozen even though `is_paused` is false. On DEAD entry, locks movement and begins death-snap pose. On COMPLETE entry, locks movement and begins completion pose. Subscribes to `OnPausedChanged` for immediate pause/resume reaction.

**Collision & Hit Detection** — Sends `death_confirmed` event to RSM. Only sends when RSM state is RUNNING, `is_paused == false`, and `resume_grace == false`; discards events in all other states or during the resume grace window to prevent ghost deaths on return from background.

**Difficulty & Phase Controller** — Reads `remaining_time` each tick to determine the current difficulty phase. Does not receive state-change events — it polls. Must declare RSM as a tick prerequisite (Rule 16) to guarantee it reads a current-frame value.

**Wave Spawner** — Reads `current_state`, `is_paused`, and `resume_grace`; spawns waves only in RUNNING with `is_paused == false` and `resume_grace == false`. Flushes pending wave queue on DEAD or COMPLETE. On `OnPausedChanged(is_paused=false)` during RUNNING: flushes all in-flight waves whose trajectories began before backgrounding (Rule 19 flush contract — all such waves are considered past their 0.4s telegraph window and cannot be read by the returning player). New spawning resumes after grace ends.

**Scoring Logic** — Reads `remaining_time` for time-based score components. Reads `run_outcome` on RESOLVING entry to determine final score presentation. No score is recorded for ABORTED runs.

**In-Run HUD** — Reads `remaining_time` each frame for timer display. Listens to `OnStateChanged` to toggle HUD visibility.

**End-Run Screen** — Reads `run_outcome` on RESOLVING entry to determine which screen to show (DEAD vs. COMPLETE path). On player tap after `RESOLVING_SCORE_REVEAL_DURATION_S` elapses, sends dismiss event to RSM → RESOLVING → IDLE. On `OnStateChanged(previous_state=ABORTED, new_state=IDLE)`, displays a brief "run interrupted" acknowledgment screen dismissed by the next run-start tap. Must not show a score for ABORTED runs.

**Audio Controller** — Subscribes to `OnStateChanged` for music cue transitions and to `OnPausedChanged` for music duck/resume on pause state changes. Music ducking and resuming is triggered by `OnPausedChanged`, not `OnStateChanged`. Death sting audio may bleed into RESOLVING if the sting duration exceeds `DEAD_SNAP_DURATION_S`; RSM does not guarantee sting completion within the snap window. Audio Controller GDD must specify a death sting duration and validate it against `DEAD_SNAP_DURATION_S` to ensure "cut to silence" fantasy fidelity.

**Score Persistence** — Reads `run_outcome` during RESOLVING. Only persists scores for DEAD and COMPLETE outcomes; ABORTED runs are not persisted. Must read before RESOLVING → IDLE fires (which resets `run_outcome` to `NONE`).

**Death Replay** — Listens to `OnStateChanged`; begins capture buffer on RUNNING entry, commits replay data on DEAD entry. "Commit" means finalizing and serializing the capture buffer — this work occurs during the DEAD snap window. If serialization time exceeds `DEAD_SNAP_DURATION_S` on low-end hardware, the commit may not complete before RESOLVING entry. Validate `DEAD_SNAP_DURATION_S` minimum against Death Replay's worst-case processing time before lowering below default. Discards buffer on ABORTED.

**Telegraph System** — Reads `current_state`; renders telegraphs only in RUNNING.

**Near-Miss Detection** — Reads `current_state`; active only in RUNNING.

**Camera System** — Reads `current_state`; adjusts camera behavior per state.

## Formulas

#### F-1: Remaining Run Time

```
remaining_time(t) = min(RUN_DURATION_S, max(0.0, RUN_DURATION_S − ((t − t_run_start) − total_paused_duration)))
```

where `t = time_source.GetCurrentTime()` at evaluation.

**Variables:**

| Variable | Type | Description |
|---|---|---|
| `remaining_time` | `double` (seconds) | Time remaining in the current run; range `[0, RUN_DURATION_S]` |
| `t` | `double` (seconds) | `time_source.GetCurrentTime()` — always use `RunTimeSource`; never `FApp::GetCurrentTime()` or `FPlatformTime::Seconds()` directly |
| `t_run_start` | `double` (seconds) | Wall-clock timestamp sampled synchronously at COUNTDOWN → RUNNING transition, before the first gameplay tick |
| `total_paused_duration` | `double` (seconds) | Cumulative wall-clock time spent paused within this run; initialized to `0.0` at RUNNING entry. **Do not clamp** — values exceeding `RUN_DURATION_S` are legal (a run paused longer than its duration). F-2 handles the stale-run abort. |
| `RUN_DURATION_S` | `double` (seconds) | Total run length; default `60.0`; range `(0, ∞)` |

**Valid only when:** `current_state == RUNNING` and `is_paused == false`.
**When `is_paused == true`:** return `cached_remaining_at_pause_entry` — do not evaluate this formula. `cached_remaining_at_pause_entry` is overwritten on each background event (most-recent-sample-wins; do not accumulate).
**At RUNNING → DEAD transition:** sample `cached_remaining_at_death_entry = remaining_time(time_source.GetCurrentTime())` before entering DEAD. In DEAD state, `remaining_time` returns `cached_remaining_at_death_entry` without evaluating F-1.
**At COMPLETE:** `remaining_time` is `0.0` by the `max(0.0, ...)` clamp — no separate cache required.
**Upper clamp:** `min(RUN_DURATION_S, ...)` ensures `remaining_time` never exceeds `RUN_DURATION_S` even if the wall clock jumps backward.

**Boundary checks:**

| Input condition | Result | Notes |
|---|---|---|
| `t == t_run_start`, `total_paused_duration == 0` | `60.0` | First tick of RUNNING; full run ahead |
| `t − t_run_start − total_paused_duration == 60.0` | `0.0` | Timer expired; triggers RUNNING → COMPLETE |
| `t − t_run_start − total_paused_duration > 60.0` | Clamped to `0.0`; COMPLETE fires | Prevents negative display |
| Clock jumps backward: `t < t_run_start` | Clamped to `RUN_DURATION_S` | Upper bound prevents nonsensical display |
| `is_paused == true`: `t` advances but formula not evaluated | No bleed | `cached_remaining_at_pause_entry` returned |

**Example — run with one pause:**
- `t_run_start = 1000.0`, `RUN_DURATION_S = 60.0`
- Background at `t = 1020.0` (20s elapsed): `cached_remaining_at_pause_entry = 40.0`
- Foreground at `t = 1030.0`: `total_paused_duration += 10.0` → `10.0`
- At `t = 1060.0`: `remaining = 60.0 − ((60.0) − 10.0) = 10.0` ✓

---

#### F-2: ABORTED Detection (evaluated on every app foreground in COUNTDOWN or RUNNING)

```
is_stale = (time_source.GetCurrentTime() − t_pause_start) > PAUSED_TIMEOUT_S
```

**Variables:**

| Variable | Type | Description |
|---|---|---|
| `t_pause_start` | `double` (seconds) | Wall-clock timestamp (`time_source.GetCurrentTime()`) recorded when `is_paused` was set to `true` (both in COUNTDOWN and RUNNING states) |
| `PAUSED_TIMEOUT_S` | `double` (seconds) | Maximum pause duration before the run is declared stale; default `300.0` (5 min) |

**Precondition:** Evaluate only when `is_paused == true` (i.e., current state is COUNTDOWN or RUNNING). Evaluated on every app foreground, before F-3 accumulates `total_paused_duration`.

If `is_stale == true` on foreground: RSM transitions RUNNING/COUNTDOWN → ABORTED → IDLE without resuming gameplay.

---

#### F-3: Pause Accumulation (on every app foreground, RUNNING state only)

```
total_paused_duration += max(0.0, (t_resume − t_pause_start))
```

where `t_resume = time_source.GetCurrentTime()` sampled immediately after F-2 is evaluated.

**Sequencing constraint:** `t_resume` must be sampled and `total_paused_duration` updated before the first gameplay tick executes post-foreground. A tick firing before this update computes `remaining_time` with a stale denominator.

**Clock backward protection:** The `max(0.0, ...)` guard ensures a backward clock movement (NTP correction, DST transition, device clock reset) does not decrement `total_paused_duration`. A backward clock produces a zero increment, not a negative one. Without this guard, F-1 would return an artificially short `remaining_time` — potentially triggering premature COMPLETE.

**Note:** `t_pause_start`, `t_resume`, and all `time_source.GetCurrentTime()` calls in F-2 must use the same `RunTimeSource` instance. Never mix clock sources across these formulas.

---

#### F-4: COUNTDOWN Elapsed Time (pause-aware)

```
elapsed_countdown_active(t) = (t − t_countdown_start) − total_countdown_paused_duration
```

COUNTDOWN elapses when: `elapsed_countdown_active(time_source.GetCurrentTime()) ≥ COUNTDOWN_DURATION_S`

**Variables:**

| Variable | Type | Description |
|---|---|---|
| `t_countdown_start` | `double` (seconds) | Wall-clock timestamp sampled at IDLE → COUNTDOWN entry |
| `total_countdown_paused_duration` | `double` (seconds) | Cumulative paused time during COUNTDOWN; initialized to `0.0` at COUNTDOWN entry. Scoped to the current COUNTDOWN phase — not shared with `total_paused_duration` (which is RUNNING-scoped). |
| `t_countdown_pause_start` | `double` (seconds) | Wall-clock recorded when `is_paused` is set during COUNTDOWN |

**On app background in COUNTDOWN:** `is_paused = true`, `t_countdown_pause_start = time_source.GetCurrentTime()`.
**On app foreground in COUNTDOWN:** evaluate F-2 first — if stale, → ABORTED → IDLE. Otherwise: sample `t_resume = time_source.GetCurrentTime()` once; `total_countdown_paused_duration += max(0.0, (t_resume − t_countdown_pause_start))`, `is_paused = false`. COUNTDOWN resumes from the remaining active countdown time. (Single `t_resume` sample, consistent with F-3. The `max(0.0, ...)` guard mirrors F-3's clock backward protection.)
**At COUNTDOWN entry:** `total_countdown_paused_duration = 0.0`, `t_countdown_start = time_source.GetCurrentTime()`.

**Example — COUNTDOWN with one pause:**
- `COUNTDOWN_DURATION_S = 1.5`, `t_countdown_start = 500.0`
- Background at `t = 500.5` (0.5s active elapsed): `t_countdown_pause_start = 500.5`
- Foreground at `t = 508.0`: `total_countdown_paused_duration += 7.5` → `7.5`
- Elapses when `(t − 500.0) − 7.5 ≥ 1.5` → `t ≥ 509.0` → 1.0s more active time needed ✓

---

**Additional formula examples:**

*Clean 60-second run (no pauses):*
`t_run_start = 500.0` — at `t = 560.0`: `remaining = min(60, max(0, 60.0 − 60.0)) = 0.0` → COMPLETE ✓

*Death with 23 seconds remaining (37 seconds elapsed):*
At `t = t_run_start + 37.0`: `remaining = max(0.0, 60.0 − (37.0 − 0)) = 23.0` — death fires; `run_outcome = DEAD`, `remaining_time` frozen at `23.0` ✓

*Two pauses (8s + 5s = 13s total):*
At `t = t_run_start + 75.0` wall-clock: `remaining = max(0.0, 60.0 − (75.0 − 13.0)) = max(0.0, −2.0)` → clamped to `0.0`, COMPLETE. Player experienced 62 wall-clock seconds, 60 active seconds ✓

## Edge Cases

**EC-1: Death and timer expiry in the same tick**
DEAD takes priority per Rule 17 (collision evaluation precedes timer check). RSM latches to DEAD; the COMPLETE transition is not reached. `run_outcome = DEAD`.

**Design position**: At the wire, if the storm caught you, you died. The collision is the dominant sensory event — the player's eyes were on the wave, their thumb just reacted to it. A wave contact at t=60.0s is a death. Awarding COMPLETE in this case creates a mystery survival: the player experienced the collision as a hit, and the game says they won. That is a Pillar 5 violation ("Skill Is Visible — no mystery outcomes") in the opposite direction from a mystery death, and equally corrosive to the skill loop. Pillar 3 ("Sixty Seconds Is the Whole Game") covers *clean* timer expiry with no simultaneous collision — that is the completed run. A run where the storm caught the player in the final tick is not a completed run; it is a death at the wire.

**EC-2: Multiple death events in the same tick**
Once DEAD is entered, all subsequent `death_confirmed` events in the same frame are silently discarded. `run_outcome` is set once.

**EC-3: App backgrounded on the same tick as a terminal event**
Terminal event takes priority (Core Rule 9). RSM enters DEAD or COMPLETE; `is_paused` is not set. Run ends normally.

**EC-4: App foregrounded after PAUSED_TIMEOUT_S**
RSM enters ABORTED immediately on foreground (from either RUNNING or COUNTDOWN). No end-run score screen is shown. Downstream plays the "run interrupted" acknowledgment ceremony (audio cue + acknowledgment screen dismissed by next run-start tap). `run_outcome = ABORTED`. Transitions directly to IDLE. No score is recorded.

**EC-5: `PAUSED_TIMEOUT_S` out-of-range values**
Any background/resume cycle immediately triggers ABORTED if `PAUSED_TIMEOUT_S` is at or near zero. Two guards apply at initialization: (1) absolute correctness floor: `PAUSED_TIMEOUT_S = max(PAUSED_TIMEOUT_S, 1.0)` — prevents immediate-ABORTED behavior; (2) UX safety warning: log a warning if `PAUSED_TIMEOUT_S < 60.0` — values below the documented safe range minimum risk ABORTing runs during routine mobile interruptions (incoming calls, app-switch). The documented safe range is 60.0–3600.0; values below 120.0s carry increased risk.

**EC-6: Player taps during COUNTDOWN**
No-op. RSM is already in COUNTDOWN and accepts no run-start events. The Input System does not route slip gestures to RSM during COUNTDOWN (R-5 still applies). No input is buffered.

**EC-7: Stale `OnStateChanged` listener fires after RESOLVING → IDLE**
If End-Run Screen or another downstream system responds to `new_state=IDLE` in the same frame as a new COUNTDOWN begins (e.g., player tapped immediately after dismissing score), downstream systems must guard on `previous_state` in the payload to avoid acting on a stale IDLE signal for the previous run.

**EC-8: RESOLVING_TIMEOUT_S watchdog fires**
RSM transitions RESOLVING → IDLE (same path as the player-tap dismiss). Logged as a diagnostic event. `run_outcome` was already set at RESOLVING entry — no data is lost. Downstream systems see a normal RESOLVING → IDLE transition and behave identically to the tap-dismiss path.

**EC-9: Device clock behavior — platform-conditional implementation required (iOS and Android)**
`FPlatformTime::Seconds()` is **not** sleep-aware on either major mobile platform and must not be used directly in `FAppTimeSource`:
- **iOS:** `FPlatformTime::Seconds()` wraps `mach_absolute_time()`, which pauses when the CPU suspends during device sleep. A player who backgrounds with the device asleep has F-2 and F-3 compute too-short elapsed times — stale runs would not abort correctly, and `total_paused_duration` is undercounted (Pillar 3 violation). **Fix:** `FAppTimeSource` must use `mach_continuous_time()` on iOS (available since iOS 10), which advances through device sleep.
- **Android:** `FPlatformTime::Seconds()` wraps `CLOCK_MONOTONIC`, which also pauses during device sleep. **Fix:** `FAppTimeSource` must use `clock_gettime(CLOCK_BOOTTIME, ...)` on Android, which advances through sleep.

Both platforms require `#if PLATFORM_IOS / #elif PLATFORM_ANDROID` branching in `GetCurrentTime()`. **Implementation prerequisite:** verify the correct sleep-aware clock APIs are in use on the target UE5 build for both platforms before shipping. See Open Question 6. NTP steps, timezone changes, and user-adjusted system time do not affect monotonic clocks on either platform.

**EC-10: `RUN_DURATION_S` set to a non-standard value**
RSM operates correctly at any `RUN_DURATION_S > 0`. The 60-second session pillar is enforced by game configuration, not by RSM internals.

**EC-11: `remaining_time` sampled while `is_paused == true`**
Returns `cached_remaining_at_pause_entry`. Downstream systems (HUD, Difficulty Controller) may safely poll `remaining_time` in any state without checking `is_paused` first.

**EC-12: OS kills the app while PAUSED**
Process is terminated. On next launch, RSM starts in IDLE. The interrupted run is implicitly discarded — no persistent run state is recovered. Correct behavior for a 60-second session format.

**EC-13: `RESOLVING_TIMEOUT_S` configured at or below `RESOLVING_SCORE_REVEAL_DURATION_S`**
If `RESOLVING_TIMEOUT_S ≤ RESOLVING_SCORE_REVEAL_DURATION_S`, the watchdog fires before a player tap can ever be accepted (tap is only valid after the minimum hold elapses). Clamp at initialization: `RESOLVING_TIMEOUT_S = max(RESOLVING_TIMEOUT_S, RESOLVING_SCORE_REVEAL_DURATION_S + 5.0)`. Log a warning if clamping was applied.

**EC-14: App lifecycle callbacks in terminal and inactive states**
If `ApplicationWillEnterBackgroundDelegate` fires when `current_state` is IDLE, DEAD, COMPLETE, ABORTED, or RESOLVING, the callback is a no-op — `is_paused` is not set (per Core Rule 20). In IDLE, no run is active; in terminal and post-run states, the run is already over. There is no timer to freeze. On the next foreground, F-2 is not evaluated (the run is not active).

**EC-15: `COMPLETE_SNAP_DURATION_S` must exceed `DEAD_SNAP_DURATION_S`**
Survival deserves a longer beat than death. Clamp at initialization: `COMPLETE_SNAP_DURATION_S = max(COMPLETE_SNAP_DURATION_S, DEAD_SNAP_DURATION_S + 0.05)`. Log a warning if clamping was applied.

**EC-16: Snap duration and run duration initialization minimums**
Clamp the following at initialization; log a warning if any clamping was applied:
- `RUN_DURATION_S = max(RUN_DURATION_S, 1.0)` — prevents zero-length runs
- `COUNTDOWN_DURATION_S = max(COUNTDOWN_DURATION_S, 1.0)` — minimum ceremony length
- `DEAD_SNAP_DURATION_S = max(DEAD_SNAP_DURATION_S, 0.35)` — perceptual floor for death moment
- `COMPLETE_SNAP_DURATION_S` — apply EC-15 after this clamp
- `RESOLVING_AUTO_ADVANCE_S = max(RESOLVING_AUTO_ADVANCE_S, RESOLVING_SCORE_REVEAL_DURATION_S + 0.5)` — ensures the tap-to-skip window is at least 0.5s; without this clamp, setting auto-advance equal to the minimum hold makes tap-to-skip operationally unreachable

**EC-17: `RESOLVING_TIMEOUT_S` must exceed `RESOLVING_AUTO_ADVANCE_S`**
Apply after EC-13: `RESOLVING_TIMEOUT_S = max(RESOLVING_TIMEOUT_S, RESOLVING_AUTO_ADVANCE_S + 1.0)`. Without this guard, a legal misconfiguration (e.g., `RESOLVING_AUTO_ADVANCE_S = 8.0`, `RESOLVING_TIMEOUT_S = 7.0`) silently causes the watchdog to fire before auto-advance, bypassing the intended friendly-advance flow. Log a warning if clamping was applied. The intended ordering: tap window opens at `RESOLVING_SCORE_REVEAL_DURATION_S`, auto-advance fires at `RESOLVING_AUTO_ADVANCE_S`, watchdog fires at `RESOLVING_TIMEOUT_S` as a last resort only.

**EC-18: RESOLVING — simultaneous player tap and auto-advance in the same evaluation cycle**
If a player dismiss tap is received in the same evaluation cycle as `RESOLVING_AUTO_ADVANCE_S` elapses, tap-to-skip takes priority; the auto-advance transition is not evaluated. Both produce RESOLVING → IDLE — the priority rule ensures deterministic behavior (see AC-17).

## Dependencies

**Upstream (systems this one depends on):**
None. The Run State Machine is a Foundation layer system with no upstream dependencies.

**Downstream (systems that depend on this one):**

| System | What it reads from RSM | Interface |
|---|---|---|
| Player Movement | `current_state`, `is_paused`, `resume_grace`, `OnPausedChanged` | Reads each tick; processes input only in RUNNING + not paused + not in grace; subscribes to `OnPausedChanged` |
| Difficulty & Phase Controller | `remaining_time` | Polls each tick (must declare tick prerequisite — Rule 16) |
| Pull-Wave Behavior | `current_state`, `is_paused` | Active only in RUNNING + not paused |
| Telegraph System | `current_state` | Renders telegraphs only in RUNNING |
| Wave Spawner & Pattern Library | `current_state`, `is_paused`, `resume_grace`, `OnPausedChanged` | Spawns only in RUNNING + not paused + not in grace; flushes on DEAD/COMPLETE and on foreground resume (Rule 19 flush contract). **Cross-system invariant (Pillar 2):** Wave Spawner must ensure no wave trajectory contacts the player within 0.4s of resume grace expiry. Wave Spawner GDD must specify enforcement. |
| Collision & Hit Detection | `current_state`, `is_paused`, `resume_grace` | Sends `death_confirmed` only when RUNNING + not paused + not in grace; discards during grace window |
| Near-Miss Detection | `current_state` | Active only in RUNNING |
| Scoring Logic | `remaining_time`, `run_outcome` | Reads time for score; reads outcome at RESOLVING entry |
| Camera System | `current_state` | Adjusts camera behavior per state |
| In-Run HUD | `remaining_time`, `current_state`, `OnStateChanged` | Polls time each frame; toggles visibility on state change. **Cross-system invariant (Pillar 5):** In-Run HUD must provide sufficient timer clarity in the final 1–2 seconds so that wire-deaths (simultaneous collision + timer expiry resolved as DEAD per EC-1) are player-attributable rather than ambiguous. HUD GDD must specify enforcement. |
| End-Run Screen | `run_outcome`, dismiss tap event | Reads on RESOLVING entry; sends tap dismiss; shows ABORTED ceremony on ABORTED→IDLE |
| Score Persistence | `run_outcome` | Reads during RESOLVING; only persists DEAD and COMPLETE outcomes |
| Death Replay | `OnStateChanged` | Begins capture on RUNNING entry; commits (serializes) on DEAD; discards on ABORTED |
| Audio Controller | `OnStateChanged`, `OnPausedChanged` | Music cue transitions per state change; music duck/resume per pause state change |

**Bidirectional note:** The Input System GDD (R-5) explicitly excludes RSM from its dependency set. RSM must not be listed as an Input System dependency.

## Tuning Knobs

| Knob | Default | Safe Range | Gameplay Effect |
|---|---|---|---|
| `RUN_DURATION_S` | `60.0` | `10.0 – 300.0` | Total run length. The 60-second pillar is a design constraint; this knob exists for QA fast-forward, tutorial mode, and potential future challenge modes. |
| `COUNTDOWN_DURATION_S` | `1.5` | `1.0 – 3.0` | World-assembly duration before gameplay begins. Minimum 1.0s — below this, the "metallic slow-motion" beat cannot register as a deliberate moment on a mobile screen. Default 1.5s provides the UX spec sufficient time for the world-assembly animation to establish the dream-frame. Below 1.0s risks reading as a loading flash rather than a ceremony. |
| `DEAD_SNAP_DURATION_S` | `0.4` | `0.35 – 0.8` | How long RSM holds in DEAD before transitioning to RESOLVING. Minimum 0.35s — the experiential floor for death to register as a discrete moment (below ~300ms, the event is processed as simultaneous with what follows). 0.4s is the minimum expressive value. Audio Controller death sting may bleed into RESOLVING; RSM does not guarantee sting completion within the snap window. Validate against Death Replay's worst-case serialization time before tuning below default. |
| `COMPLETE_SNAP_DURATION_S` | `0.6` | `0.35 – 1.5` | How long RSM holds in COMPLETE before transitioning to RESOLVING. Must exceed `DEAD_SNAP_DURATION_S` (enforced at init per EC-15) — survival deserves a longer breath than death. **Cumulative note:** tune together with `RESOLVING_SCORE_REVEAL_DURATION_S` — total minimum time from COMPLETE to player-tap-skip availability is `COMPLETE_SNAP_DURATION_S + RESOLVING_SCORE_REVEAL_DURATION_S`. Combined maximum (1.5 + 4.0 = 5.5s) must be validated in multi-run sessions for rhythm impact. |
| `RESOLVING_SCORE_REVEAL_DURATION_S` | `2.0` | `0.5 – 4.0` | **Minimum hold time** before the player's tap-to-skip is accepted. Score must be visible for this duration before the player can advance to IDLE. After this elapses, any tap advances to IDLE. The "tap to continue" affordance must animate in at this point — do not show a tappable affordance earlier. |
| `RESOLVING_AUTO_ADVANCE_S` | `3.5` | `RESOLVING_SCORE_REVEAL_DURATION_S – 8.0` | **Auto-advance time** — if no player tap arrives, RSM transitions RESOLVING → IDLE automatically at this elapsed time. The dream ends on its own schedule. Must be ≥ `RESOLVING_SCORE_REVEAL_DURATION_S` (enforced at init). Replaces the prior tap-only model to honor the Player Fantasy ("survival is the dream releasing you on its own"). |
| `ABORTED_CEREMONY_DURATION_S` | `1.5` | `0.5 – 3.0` | **Minimum hold before run-start** after the interrupted-run screen appears. Mirrors `RESOLVING_SCORE_REVEAL_DURATION_S` — the player must see the acknowledgment before a tap starts COUNTDOWN. Prevents reflexive app-return taps from launching an unintentional run. |
| `RESUME_GRACE_S` | `0.5` | `0.25 – 2.0` | **Grace window** after `is_paused` clears in RUNNING. During this window: Collision discards `death_confirmed` events; Wave Spawner holds new spawning; Player Movement freezes inputs. All in-flight waves from before backgrounding are flushed immediately on resume (Rule 19 contract); grace gives the player time to re-orient before fresh waves appear. Tuned per platform; validate that the world reads as "ready" before grace ends. |
| `PAUSED_TIMEOUT_S` | `300.0` | `60.0 – 3600.0` | Maximum background duration before the run is declared stale and ABORTED. 300s (5 min) is the recommended default. **Do not set below 120.0s** — values below 120s will ABORT runs during routine mobile interruptions (incoming call, app-switch, 35-second notification triage). Re-evaluate based on measured player interrupt patterns from production telemetry. |
| `RESOLVING_TIMEOUT_S` | `30.0` | `10.0 – 60.0` | Safety watchdog: maximum time RSM may remain in RESOLVING if no player tap arrives. Must exceed `RESOLVING_SCORE_REVEAL_DURATION_S + 5.0s` (enforced at init per EC-13). Under normal operation, the player taps before this fires. |

**Non-tunable constants:**
- `RUN_DURATION_S = 60.0` is the canonical game experience. Deviation requires a deliberate mode-level override, not a production knob adjustment.
- `is_paused` flag behavior is structural and not configurable.

**Knob interactions:**
- `DEAD_SNAP_DURATION_S` affects the death replay capture finalization window. Validate against Death Replay's worst-case processing time before reducing below default.
- `COUNTDOWN_DURATION_S` affects perceived run-to-run rhythm. Test across multi-run sessions (8+ runs), not individual runs.
- `COMPLETE_SNAP_DURATION_S + RESOLVING_SCORE_REVEAL_DURATION_S` defines the minimum time from run completion to the player's next tap opportunity. Test as a combined unit.

## Visual/Audio Requirements

The RSM does not own any visuals or audio directly. All visual and audio output is owned by downstream systems that subscribe to `OnStateChanged` or `OnPausedChanged`.

**COUNTDOWN animation sync contract:** The world-assembly animation for `IDLE → COUNTDOWN` must be driven by `elapsed_countdown_active / COUNTDOWN_DURATION_S` (see F-4), not wall-clock elapsed time. When `is_paused` is set during COUNTDOWN, the animation must pause at its current frame. When `is_paused` is cleared, the animation resumes from the same fractional progress. A background at 0.5s of a 1.5s countdown resumes at 33% — the player sees the world assemble from where it was. Art/VFX owns the implementation; this document defines the binding contract.

**Required downstream reactions per state transition (RSM must guarantee the transition fires):**

| RSM Transition | Audio cue (Audio Controller) | Visual cue (Art/VFX) |
|---|---|---|
| IDLE → COUNTDOWN | Ambient hum begins | World voxels materialize/assemble |
| COUNTDOWN → RUNNING | Run music begins | First wave spawns |
| RUNNING → DEAD | Death sting (cut-to-silence; may bleed into RESOLVING) | Death snap VFX; world freezes |
| RUNNING → COMPLETE | Survival sting | Completion VFX; world releases |
| DEAD/COMPLETE → RESOLVING | Music transitions to end-screen ambient | End-run screen animates in with score |
| RESOLVING → IDLE (tap, auto-advance, or watchdog) | Music fades | End-run screen dismisses |
| ABORTED → IDLE | Brief interruption tone (distinct from death sting and survival sting) | "Run interrupted" acknowledgment screen; dismissed by next run-start tap |
| `is_paused` set (via `OnPausedChanged`) | Music ducks or pauses | Game world freezes; input locked |
| `is_paused` cleared (via `OnPausedChanged`) | Music resumes | `resume_grace` window begins (`RESUME_GRACE_S`); in-flight waves flushed; world remains frozen for grace duration; Player Movement, Wave Spawner, and Collision remain suppressed until grace ends |

**ABORTED tonal direction:** The interruption tone must be ascending or resolving in character — not percussive, not discordant. A soft descending chime or a brief clearing sound works; a harsh cut or any variant of the death sting does not. The acknowledgment screen must use copy that attributes the interruption externally: "Your run was interrupted" rather than "Run failed" or "Run ended." The screen must not share visual language with the DEAD outcome — avoid broken-voxel imagery, avoid red or high-saturation warning colors. Design goal: the player who sees this screen understands "something interrupted the dream; it wasn't me" and immediately feels ready to try again.

## UI Requirements

The RSM does not own any UI. It exposes read-only data and accepts discrete events that UI systems send.

**In-Run HUD:**
- Must display `remaining_time` to the player, updated each frame.
- Must be visible only when `current_state == RUNNING` (or during COUNTDOWN if a countdown display is desired).
- Must respond to `OnStateChanged` to enter/exit visibility without polling state each frame.

**End-Run Screen (RESOLVING):**
- Must read `run_outcome` (DEAD or COMPLETE) on RESOLVING entry to determine which screen to show.
- Score and results are visible from RESOLVING entry.
- Must not accept a player dismiss tap until `RESOLVING_SCORE_REVEAL_DURATION_S` has elapsed (minimum score visibility window).
- The "tap to continue" affordance must **animate in at `RESOLVING_SCORE_REVEAL_DURATION_S` elapsed** — do not show a tappable affordance before input is accepted. A tap received before the affordance appears should be silently discarded; the player must not see a frozen UI.
- After `RESOLVING_SCORE_REVEAL_DURATION_S`, any player tap sends a dismiss event to RSM → RESOLVING → IDLE (tap-to-skip).
- If no tap arrives, RSM auto-advances at `RESOLVING_AUTO_ADVANCE_S` → IDLE. The screen dismisses on the same `OnStateChanged`. The dream ends on its own schedule.
- If `RESOLVING_TIMEOUT_S` watchdog fires (no tap, no auto-advance), RSM transitions RESOLVING → IDLE automatically; screen dismisses on the same `OnStateChanged`.
- Must not be shown for ABORTED runs.

**Interrupted-Run Screen (ABORTED):**
- On `OnStateChanged(previous_state=ABORTED, new_state=IDLE)`, display a distinct "run interrupted" acknowledgment screen. Tone: sympathy, not failure — see Player Fantasy.
- This screen is shown at IDLE — no score, no replay, no timer. Acknowledges the run was interrupted, not lost to a missed telegraph.
- Must not accept a run-start tap until `ABORTED_CEREMONY_DURATION_S` has elapsed (minimum acknowledgment visibility window). Prevents a reflexive tap on app-return from immediately launching COUNTDOWN.
- After `ABORTED_CEREMONY_DURATION_S`, the player's next run-start tap dismisses this screen and triggers IDLE → COUNTDOWN as normal.

**Start-run trigger:**
- The UI layer (not Input System) is responsible for sending the run-start event to RSM on player tap from IDLE.
- The design of the start element (button, tap-anywhere, etc.) is defined by the UX spec, not this GDD.

## Acceptance Criteria

All ACs observe RSM's exposed surface only: `current_state`, `is_paused`, `resume_grace`, `run_outcome`, `remaining_time`, `OnStateChanged` payload, and `OnPausedChanged` payload.

**AC-01: Initial state on construction**
Given RSM is instantiated with `FakeTimeSource` injected. When no events have been received. Then `current_state == IDLE`. `run_outcome == ERunOutcome::NONE`. `is_paused == false`. `remaining_time == RUN_DURATION_S`.

**AC-02: IDLE → COUNTDOWN on run-start event**
Given RSM is in IDLE. When a run-start event is received from the UI layer. Then RSM is in COUNTDOWN. `OnStateChanged` fires exactly once with `previous_state=IDLE, new_state=COUNTDOWN, run_outcome=ERunOutcome::NONE`. `remaining_time == RUN_DURATION_S`.

**AC-03: Run-start event discarded outside IDLE**
Given RSM is in any state except IDLE (e.g., RUNNING, COUNTDOWN, RESOLVING). When a run-start event is received. Then RSM state does not change. `OnStateChanged` does not fire.

**AC-04: COUNTDOWN → RUNNING after settle duration**
Given RSM is in COUNTDOWN with `FakeTimeSource` injected. When clock is advanced by `COUNTDOWN_DURATION_S`. Then RSM is in RUNNING. `OnStateChanged` fires exactly once with `previous_state=COUNTDOWN, new_state=RUNNING, run_outcome=ERunOutcome::NONE`.

**AC-05: COUNTDOWN pause resumes from remaining time**
Given `FakeTimeSource` injected. Given RSM enters COUNTDOWN at clock time T (`COUNTDOWN_DURATION_S = 1.5`). When clock advances 0.5s, then app-background fires (`is_paused = true`). Clock advances 10.0s. App-foreground fires (not stale). Then `is_paused == false`. RSM is still in COUNTDOWN. When clock advances another 1.0s of active time. Then RSM transitions to RUNNING. Total active countdown elapsed = 1.5s; total wall-clock elapsed from T = 11.5s.

**AC-06: RUNNING → DEAD on death-confirmed event**
Given RSM is in RUNNING, `is_paused == false`. When a `death_confirmed` event is received. Then RSM is in DEAD. `run_outcome == ERunOutcome::DEAD`. `OnStateChanged` fires exactly once with `previous_state=RUNNING, new_state=DEAD, run_outcome=DEAD`.

**AC-07: RUNNING → COMPLETE on timer expiry**
Given RSM is in RUNNING with `FakeTimeSource` injected. When clock is advanced until `remaining_time ≤ 0`. Then RSM is in COMPLETE. `run_outcome == ERunOutcome::COMPLETE`. `OnStateChanged` fires exactly once with `previous_state=RUNNING, new_state=COMPLETE, run_outcome=COMPLETE`.

**AC-08: Timer formula accuracy**
Given `FakeTimeSource` is injected. Given RSM enters RUNNING at clock time T with no pauses. When clock is advanced to T + 30.0s. Then `remaining_time` is within ±0.017s of `RUN_DURATION_S − 30.0` (one 60Hz tick tolerance).

**AC-09: Timer freeze and resume across background/foreground**
Given `FakeTimeSource` is injected. Given RSM is in RUNNING with `remaining_time = 40.0`. When app-background fires (`is_paused = true`) and clock is advanced by 10.0s, then app-foreground fires. Then on foreground `remaining_time == 40.0` (unchanged, cached). `is_paused == false`. When clock advances 10 more active seconds. Then `remaining_time ≈ 30.0` (±0.017s).

**AC-10: Timer does not expire while paused**
Given `FakeTimeSource` injected. Given RSM is in RUNNING with `remaining_time == 1.0` and `is_paused == true`. When clock is advanced by 5.0s while paused. Then RSM remains in RUNNING. `run_outcome == ERunOutcome::NONE`. `OnStateChanged` does not fire.

**AC-11: ABORTED on PAUSED_TIMEOUT_S (RUNNING source)**
Given `FakeTimeSource` is injected. Given RSM is in RUNNING with `is_paused == true`. When clock is advanced by more than `PAUSED_TIMEOUT_S`, then app-foreground fires. Then `OnStateChanged` fires exactly twice: first `previous_state=RUNNING, new_state=ABORTED, run_outcome=ABORTED`; then `previous_state=ABORTED, new_state=IDLE, run_outcome=ABORTED`. `current_state == IDLE`. RESOLVING is never entered.

**AC-12: ABORTED on PAUSED_TIMEOUT_S (COUNTDOWN source)**
Given `FakeTimeSource` injected. Given RSM is in COUNTDOWN with `is_paused == true`. When clock is advanced by more than `PAUSED_TIMEOUT_S`, then app-foreground fires. Then `OnStateChanged` fires exactly twice: `previous_state=COUNTDOWN, new_state=ABORTED, run_outcome=ABORTED`; then `previous_state=ABORTED, new_state=IDLE, run_outcome=ABORTED`. RESOLVING is never entered.

**AC-13: DEAD priority over simultaneous COMPLETE**
Given `FakeTimeSource` injected. Given RSM is in RUNNING with clock advanced to exactly `t_run_start + RUN_DURATION_S` (i.e., `remaining_time ≤ 0` when evaluated). When a `death_confirmed` event is processed before the clock is advanced further (both conditions present in the same evaluation cycle). Then `run_outcome == ERunOutcome::DEAD`. RSM enters DEAD, not COMPLETE. `OnStateChanged` fires exactly once with `new_state=DEAD`.

**AC-14: Idempotency — multiple death events after DEAD entry**
Given RSM has entered DEAD. When three additional `death_confirmed` events are processed before the clock is advanced. Then RSM remains in DEAD. `OnStateChanged` fires exactly once for the entire RUNNING → DEAD transition — no additional firings occur after DEAD entry. `run_outcome` is unchanged.

**AC-15: DEAD → RESOLVING after snap duration**
Given RSM is in DEAD with `FakeTimeSource` injected. When clock is advanced by `DEAD_SNAP_DURATION_S`. Then RSM transitions to RESOLVING. `OnStateChanged` fires exactly once with `previous_state=DEAD, new_state=RESOLVING, run_outcome=DEAD`.

**AC-16: RESOLVING — tap rejected before minimum hold**
Given `FakeTimeSource` injected. Given RSM is in RESOLVING. When clock has advanced less than `RESOLVING_SCORE_REVEAL_DURATION_S` and a player dismiss tap is received. Then RSM remains in RESOLVING. `OnStateChanged` does not fire.

**AC-17: RESOLVING → IDLE on player tap-to-skip**
Given `FakeTimeSource` injected. Given RSM is in RESOLVING. When clock is advanced by `RESOLVING_SCORE_REVEAL_DURATION_S` (minimum hold elapsed), then a player dismiss tap is received before `RESOLVING_AUTO_ADVANCE_S`. Then RSM transitions to IDLE. `OnStateChanged` fires exactly once with `previous_state=RESOLVING, new_state=IDLE`. Auto-advance timer does not fire. **Simultaneous tap + auto-advance:** if a tap is received in the same evaluation cycle as `RESOLVING_AUTO_ADVANCE_S` elapses, tap-to-skip takes precedence; auto-advance is not evaluated.

**AC-18: RESOLVING watchdog (safety path — no tap)**
Given `FakeTimeSource` injected. **For this test, configure `RESOLVING_AUTO_ADVANCE_S` to a value greater than `RESOLVING_TIMEOUT_S`** (e.g., `RESOLVING_AUTO_ADVANCE_S = RESOLVING_TIMEOUT_S + 1.0`) so the watchdog fires before auto-advance — without this configuration, the auto-advance path (AC-27) preempts the watchdog and the watchdog path is unreachable. Given RSM is in RESOLVING with no player dismiss tap sent. When clock is advanced by `RESOLVING_TIMEOUT_S`. Then RSM transitions to IDLE. A diagnostic event is logged. `OnStateChanged` fires exactly once with `previous_state=RESOLVING, new_state=IDLE`.

**AC-19: ABORTED does not enter RESOLVING**
Given RSM transitions to ABORTED. When ABORTED → IDLE fires. Then RESOLVING is never entered. `run_outcome == ERunOutcome::ABORTED` until the ABORTED → IDLE transition, after which `run_outcome == ERunOutcome::NONE`.

**AC-20: `death_confirmed` discarded outside RUNNING**
Given RSM is in any state except RUNNING (e.g., COUNTDOWN, DEAD, RESOLVING). When a `death_confirmed` event is received. Then RSM state does not change. `OnStateChanged` does not fire.

**AC-21: `remaining_time` frozen during `is_paused`**
Given `FakeTimeSource` injected. Given RSM is in RUNNING with `remaining_time = 25.3`, then app-background fires (`is_paused = true`). When clock is advanced by 5.0s while paused, then `remaining_time` is queried. Then the returned value is `25.3` (cached).

**AC-22: `death_confirmed` discarded while `is_paused`**
Given RSM is in RUNNING with `is_paused == true`. When a `death_confirmed` event is received. Then RSM state does not change. `is_paused` remains `true`. `OnStateChanged` does not fire.

**AC-23: COMPLETE → RESOLVING after snap duration**
Given RSM is in COMPLETE with `FakeTimeSource` injected. When clock is advanced by `COMPLETE_SNAP_DURATION_S`. Then RSM transitions to RESOLVING. `OnStateChanged` fires exactly once with `previous_state=COMPLETE, new_state=RESOLVING, run_outcome=COMPLETE`.

**AC-24: `run_outcome` reset to NONE on RESOLVING → IDLE**
Given RSM has completed a DEAD run and transitioned through RESOLVING → IDLE (via AC-17 or AC-18). When `run_outcome` is queried from IDLE. Then `run_outcome == ERunOutcome::NONE`.

**AC-25: `is_paused` set during COUNTDOWN on app-background**
Given RSM is in COUNTDOWN. When an app-background event fires. Then `is_paused == true`. `OnPausedChanged` fires exactly once with `is_paused = true`. When an app-foreground event fires within `PAUSED_TIMEOUT_S`. Then `is_paused == false`. `OnPausedChanged` fires exactly once with `is_paused = false`. RSM remains in COUNTDOWN. `OnStateChanged` does not fire.

**AC-26: `OnStateChanged` payload correctness**
Given `FakeTimeSource` injected at clock time T. When any state transition fires `OnStateChanged`. Then: `payload.previous_state` and `payload.new_state` are distinct `ERunState` members; `payload.run_outcome` is `ERunOutcome::DEAD`, `COMPLETE`, or `ABORTED` if `new_state` is respectively DEAD, COMPLETE, or ABORTED; **exception:** when `previous_state == ABORTED` and `new_state == IDLE`, `payload.run_outcome` is `ERunOutcome::ABORTED` (the reset to `NONE` occurs after the broadcast completes, per Rule 14); `ERunOutcome::NONE` for all other transitions; `payload.timestamp` equals `time_source.GetCurrentTime()` at transition time (within ±0.001s). A second transition processed at T+5.0s has `payload.timestamp ≥ T + 5.0`.

**AC-27: RESOLVING → IDLE on auto-advance (no tap)**
Given `FakeTimeSource` injected. Given RSM is in RESOLVING with no player dismiss tap sent. When clock is advanced by `RESOLVING_AUTO_ADVANCE_S`. Then RSM transitions to IDLE. `OnStateChanged` fires exactly once with `previous_state=RESOLVING, new_state=IDLE`. Watchdog (`RESOLVING_TIMEOUT_S`) has not fired.

**AC-28: ABORTED ceremony — run-start tap rejected before minimum hold**
Given `FakeTimeSource` injected. Given RSM has transitioned to IDLE via ABORTED → IDLE. When clock has advanced less than `ABORTED_CEREMONY_DURATION_S` and a run-start event is received. Then RSM remains in IDLE. `OnStateChanged` does not fire. When clock advances by `ABORTED_CEREMONY_DURATION_S` total and a run-start event is received. Then RSM transitions to COUNTDOWN normally.

**AC-29: `resume_grace` suppresses `death_confirmed`**
Given `FakeTimeSource` injected. Given RSM is in RUNNING with `is_paused == true`. When app-foreground fires (not stale). Then `resume_grace == true`. `OnPausedChanged` fires exactly once with `is_paused = false`. When a `death_confirmed` event is received during the grace window. Then RSM state does not change. `run_outcome == ERunOutcome::NONE`. `OnStateChanged` does not fire. When clock advances by `RESUME_GRACE_S`. Then `resume_grace == false`. A subsequent `death_confirmed` event is processed normally (RSM transitions to DEAD).

**AC-30: `OnPausedChanged` fires on `is_paused` change**
Given `FakeTimeSource` injected. Given RSM is in RUNNING. When app-background fires. Then `OnPausedChanged` fires exactly once with `is_paused = true`. `OnStateChanged` does not fire (state remains RUNNING). When app-foreground fires (not stale). Then `OnPausedChanged` fires exactly once with `is_paused = false`. `payload.timestamp ≥` prior `OnPausedChanged` timestamp.

**AC-31: Player language test (playtest — ADVISORY)**
In an observed session of ≥5 runs (mixed DEAD and COMPLETE outcomes), ≥80% of players describe DEAD transitions using timing or near-miss language ("I almost had it," "I was so close," "that was my read") rather than system attribution language ("it glitched," "that was unfair," "I didn't see that"). Lead sign-off required. Evidence stored in `production/qa/evidence/`.

**AC-32: `resume_grace` is false when foreground fires in non-RUNNING states**
Given `FakeTimeSource` injected. Given RSM is in COUNTDOWN with `is_paused == true`. When app-foreground fires (not stale). Then `resume_grace == false`. `OnPausedChanged` fires exactly once with `is_paused = false`. RSM remains in COUNTDOWN.

**Additional observable property for DEAD, COMPLETE, RESOLVING, and IDLE:** `is_paused` can never be `true` in these states (per Rule 4 and Rule 20), so the test setup of "background followed by foreground" is a no-op. Observable property to verify: Given RSM is in DEAD (or COMPLETE, RESOLVING, or IDLE). When an app-background event fires. Then `is_paused` remains `false`. `OnPausedChanged` does not fire. The grace window is exclusively a RUNNING post-resume behavior.

**AC-33: Process death — `PAUSED_TIMEOUT_S` elapsed without foreground (documentation AC)**
Given RSM is in RUNNING with `is_paused == true`. When the OS terminates the app process before foreground ever fires. Then on next app launch, RSM constructs in IDLE with `run_outcome == ERunOutcome::NONE` and `is_paused == false` (AC-01 holds). The interrupted run is implicitly discarded — no ABORTED state is entered, no ceremony fires, no score is recorded. This is the correct behavior for a 60-second session format. Validated by confirming RSM has no persistent run state that survives process death (no save file, no singleton state carrying over to a new session).

## Open Questions

1. **In-flight waves on DEAD or COMPLETE**: When RSM transitions to DEAD or COMPLETE, what happens to pull-waves mid-trajectory? Options: (a) freeze in place, (b) vanish immediately, (c) complete trajectory. Resolution belongs in the Pull-Wave Behavior GDD.

2. **ADR: `ERunState + bool is_paused` architecture**: The decision to represent suspension as a separate flag rather than a state enum node has significant implementation implications. An ADR should be written before implementation begins.

3. **Voluntary run abandon**: ~~Resolved~~ — ABORTED state now provides a "run interrupted" ceremony for stale-timeout abandonment. No active voluntary abandon path (long-press, quit button) in MVP. Revisit post-MVP if production telemetry shows meaningful friction around forced run commitment.

4. **COUNTDOWN visual language**: The 1.5s settle frame is defined behaviorally but not visually. Does it include a visible countdown number, purely a world-assembly animation, or something else? Resolved by UX spec.

5. **`remaining_time` display precision**: Should the HUD display tenths of a second, whole seconds, or tenths only in the final 10 seconds? RSM exposes full `double` precision; the HUD GDD owns the display format decision.

6. **Platform clock source — implementation prerequisite (iOS and Android)**: `FAppTimeSource` must use sleep-aware clocks on both platforms (see EC-9): iOS requires `mach_continuous_time()` (not `mach_absolute_time()` — the latter pauses during device sleep); Android requires `clock_gettime(CLOCK_BOOTTIME, ...)` (not `CLOCK_MONOTONIC` — also pauses during sleep). Both paths must be verified on the target UE5 build before shipping. Must be resolved before implementation begins.

7. **RSM object type and delegate binding ADR (expand from Question 2)**: The existing ADR requirement should also cover: (a) RSM UE object type — determines tick prerequisite API, lifetime, and ordering mechanism. The three viable options for the ADR to evaluate: (i) `AActor`/`UActorComponent` — supports `AddTickPrerequisiteActor`/`AddTickPrerequisiteComponent` and `TG_PrePhysics` ordering; (ii) `UWorldSubsystem` — requires a custom tick-ordering mechanism; (iii) `UGameInstanceSubsystem` (via `FTickableGameObject`) — tick-group and prerequisite APIs unavailable, requires broadcast-on-write caching or equivalent. This choice is a **blocking implementation prerequisite** per Rule 16; (b) delegate binding API (`AddUObject` with stored `FDelegateHandle`, `Remove()` at `EndPlay`/`Deinitialize`); (c) Android delegate thread safety — `ApplicationWillEnterBackgroundDelegate` fires from the Android event thread, not the GameThread; the ADR must specify whether the callback is wrapped in `AsyncTask(ENamedThreads::GameThread, ...)` or documents a verified platform contract ensuring GameThread invocation before any tick reads the written state; (d) re-entrancy guard mechanism for `OnStateChanged` and `OnPausedChanged` broadcasts — runtime conditional (`if (bBroadcasting)` + log + early return), not `check()` or `ensure()` (both are no-ops in Shipping builds by default — see Rule 15).

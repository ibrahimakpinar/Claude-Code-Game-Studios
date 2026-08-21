# ADR-0009: Player Movement Component Hosting, Forward-Motion Model, and Tween Implementation

## Status

Accepted

## Date

2026-07-03 (Proposed) / 2026-07-09 (Accepted)

## Last Verified

2026-07-09 (status promotion Proposed → Accepted — sole architectural Depends-On gate satisfied: ADR-0007 (RSM Hosting) Accepted 2026-06-26. ADR-0002 (Haptic Platform Bridge) Depends-On is Proposed but interface INT-002-amended 2026-06-26 and stable per the pragmatic-promotion rationale documented in this ADR's Depends On field (ADR-0005/0006-Accepted-before-ADR-0007 precedent from 2026-06-24/26 applied — a Feature/Core-layer ADR whose consumed interface is INT-amended-stable may promote independently of the Foundation-layer ADR's HW-verification-gated Accepted-status label). INT-007 resolution landed in-session 2026-07-03 (2 factual-status claims about ADR-0002 corrected in Decision Makers line 21 + Depends On field line 47 with pragmatic-promotion rationale in-place). `architecture-review-2026-07-03.md` line 272 explicitly stated "No further pre-Acceptance amendments identified this pass"; `architecture-review-2026-07-08.md` §Δ vs 2026-07-03 confirmed "ADR-0009 promotion Proposed → Accepted still recommended (unblocked; user elected spike path first — no new blockers surfaced)". No open INT findings against ADR-0009. Feature-layer PM chain now Accepted; unblocks (a) PM Epic implementation + first PM story enter-implementation gate per `docs/CLAUDE.md` dev-story SD-parameter binding rule; (b) Seam 12 `FPlayerMovementProvider_Production(UPlayerLaneMovementComponent*)` production wiring; (c) ADR-0005 forward contract closure on `is_hw_performance_degraded` + `OnHardwarePerformanceBreach` + R11a-8 3.0-second grace window; (d) Pull-Wave Rule 11 near-miss direct-read of `PM.current_lane` — PM's UPlayerLaneMovementComponent surface is now bound; (e) inherited BINDING forward contracts for unauthored Camera GDD (SD3 stationary-player forward-motion model) + unauthored Collision GDD (SD5 collision-commit contract). No ADR body changes required — only status/date/verified fields; INT-007 amendments landed at ADR-0009 authoring pass 2026-07-03.)
2026-07-03

## Decision Makers

- **creative-director (user, 2026-07-03)** — AskUserQuestion sign-off on 3 scope discriminators (OQ-1 + OQ-2 bundled scope; concrete pawn class binding; hard-referenced UPROPERTY curves)
- **ADR-0007 (Accepted 2026-06-26)** — codifies RSM's `ForceTickNow()` pull primitive + non-callback invariant + `OnStateChanged` / `OnPausedChanged` non-dynamic multicast delegates (consumed by PM per mechanics §3 RSM Storage Contract + Delegate Binding Contract)
- **ADR-0008 (Accepted 2026-06-27)** — establishes the `TObjectPtr<UCurveFloat>` UPROPERTY + BeginPlay/Initialize-time `ValidateCurveAsset` fail-fast pattern that PM's SD6 mirrors; also demonstrates the `RSM->ForceTickNow()` first-statement pattern that PM's SD4 mirrors
- **ADR-0005 (Accepted 2026-06-24; amended INT-004 2026-06-26)** — one-sided forward contract on PM's `is_hw_performance_degraded` + `OnHardwarePerformanceBreach` (Wave Spawner R11a-8 grace window); this ADR's public-surface declaration closes the PM half
- **ADR-0002 (Proposed 2026-05-14; INT-002-amended 2026-06-26)** — haptic platform bridge (`IHapticDispatch::Fire(EHapticEvent)` + `IsSystemHapticsEnabled()`) consumed by PM's `SlipConfirmed` / `BufferDrop` / `NearMiss` dispatches per presentation §Near-Miss Beat + §Audio-Visual Ownership Split. ADR-0002 is Proposed pending its Hardware Verification Gate (physical iPhone 16 + Galaxy S24 device NearMiss FULL/DURATION distinctness pass — deferred to Polish phase per ADR-0002 lines 302 + 329). The public interface consumed by PM is INT-002-amended and stable; PM's implementation compiles against the interface regardless of ADR-0002's status label. See ADR-0009 Depends On note below and architecture-review-2026-07-03.md INT-007 for the pragmatic-promotion rationale.
- **PM sub-GDD authors (mechanics + presentation + platform, authored 2026-06-28; Steps 1–9 CLOSED 2026-07-02)** — GDD design locks at mechanics §3 Public Interface + §3 RSM Storage Contract + §3 Forward Motion Dual-Writer (OQ-2 recommended path (c)); platform §3 Tick Ordering + §3 Public Interface; presentation §3 Audio-Visual Ownership Split
- **unreal-specialist** — engine specialist validation (Step 5.5)

## Summary

SLIPSTORM's Player Movement is hosted as `UPlayerLaneMovementComponent : public UActorComponent`, owned by a new `ASlipstormPlayerPawn : public APawn` that is stationary in world space per the Option (c) moving-world forward-motion model (closes mechanics OQ-2 forward-motion resolution; unblocks OQ-1 per mechanics line 500). `TickComponent(DeltaTime)` calls `RSM->ForceTickNow()` as its first statement after the `check(IsInGameThread())` guard — mirrors ADR-0008 SD1 pattern for DPC; engine-scheduler-independent; idempotent within-frame via RSM's `bHasTickedThisFrame` guard (INT-005-amended, ADR-0007 SD2). Collision commitment on the SETTLED→SLIPPING transition uses `GetOwner()->SetActorLocation(FVector(target_x, 0, 0), /*bSweep=*/false)` on the pawn root; mesh interpolation (F-3 `lateral_world_position`) uses `SetRelativeLocation` on the child mesh component — decoupling root-commits-to-target from mesh-visually-mid-arc. Curve assets (`SLIP_CURVE_ASSET`, `LEAN_CURVE_ASSET`, `EDGE_ABSORB_CURVE_ASSET`) are declared as `UPROPERTY() TObjectPtr<UCurveFloat>` hard references and validated at `BeginPlay()` via the ADR-0008 SD2 gate; Shipping-safe fallback (linear F-3, zero-lean F-5, immediate F-6 return) publishes the `bCurveFallbackActive` diagnostic flag (AC-SS-D). Platform sub-GDD §3 Tick Ordering enumeration options (a) one-frame-lag + (b) manual TickComponent-from-RSM — both authored pre-ADR-0007 — are superseded by SD4; a hygiene edit to platform §3 is queued post-ADR per user session-scope selection.

## Engine Compatibility

| Field | Value |
|-------|-------|
| **Engine** | Unreal Engine 5.7 |
| **Domain** | Gameplay |
| **Knowledge Risk** | MEDIUM — UE 5.4–5.7 are post-LLM-cutoff (May 2025). The `UActorComponent` + `APawn` + `SetActorLocation` + `SetRelativeLocation` + `UCurveFloat` + non-dynamic multicast delegate binding surface all pre-dates the cutoff and is API-stable per `docs/engine-reference/unreal/breaking-changes.md`. UMovementComponent alternatives are rejected specifically to avoid `SafeMoveUpdatedComponent` behavior variance across 5.4–5.7 (documented in the engine reference as a family that received sweep-resolution changes). |
| **References Consulted** | `docs/engine-reference/unreal/VERSION.md`, `docs/engine-reference/unreal/breaking-changes.md`, `docs/engine-reference/unreal/deprecated-apis.md`, `docs/engine-reference/unreal/current-best-practices.md`, `docs/architecture/adr-0002-haptic-platform-bridge.md`, `docs/architecture/adr-0005-wave-spawner-subsystem-hosting.md`, `docs/architecture/adr-0007-run-state-machine-hosting.md`, `docs/architecture/adr-0008-dpc-subsystem-hosting.md`, `docs/architecture/platform-seam-interfaces.md` (Seam 12) |
| **Post-Cutoff APIs Used** | `UActorComponent::BeginPlay/EndPlay/TickComponent` (stable pre-cutoff); `PrimaryComponentTick.bCanEverTick` (stable pre-cutoff); `APawn` (stable pre-cutoff); `AActor::SetActorLocation(FVector, bool bSweep, FHitResult*, ETeleportType)` (stable pre-cutoff); `USceneComponent::SetRelativeLocation` / `SetRelativeRotation` (stable pre-cutoff); `UPROPERTY() TObjectPtr<T>` (5.0+ GC-safe pointer modernization); `UCurveFloat::GetFloatValue()` (stable pre-cutoff); `FRichCurve::Keys` inspection (stable pre-cutoff — same access used by ADR-0008 SD2); non-dynamic `DECLARE_MULTICAST_DELEGATE_*` (stable pre-cutoff); `AddUObject` binding (stable pre-cutoff); `IsValid()` runtime guard (stable pre-cutoff); `AsyncTask(ENamedThreads::GameThread, ...)` (NOT used here — PM does not subscribe to lifecycle delegates; RSM absorbs the Android event-thread marshalling per ADR-0007 SD5) |
| **Verification Required** | (1) Confirm `UActorComponent::TickComponent()` runs on GameThread at the default `TG_DuringPhysics` tick group in UE 5.7 mobile — if the mobile forward renderer's tick scheduling differs from desktop, F-3 / F-5 write timing may need adjustment. (2) Confirm hard-referenced `UPROPERTY() TObjectPtr<UCurveFloat>` guarantees curve availability at BeginPlay of the owning component (mirror of ADR-0008 Verification Required #1). (3) Confirm `AActor::SetActorLocation(FVector, /*bSweep=*/false)` on a pawn with no physics body performs a constant-time transform update in UE 5.7 (no Chaos physics resolution on `bSweep=false` — documented for `ETeleportType::TeleportPhysics` overload, but our call uses the default overload; verify no unexpected Chaos entry). (4) Confirm `USceneComponent::SetRelativeLocation` on a child mesh component composes with parent `AActor::SetActorLocation` such that world position of the mesh equals `pawn_root_world + mesh_relative` — needed for F-3's fractional between-lane rendering while root is committed to `target_lane`. (5) Confirm PM's `TickComponent` invocation of `RSM->ForceTickNow()` has no re-entrancy hazard beyond RSM's `bHasTickedThisFrame` idempotence: PM does not subscribe to RSM callbacks from inside `ForceTickNow()`, so the ADR-0007 non-callback invariant is not violated. (6) Confirm `GetOwner()` returns the `ASlipstormPlayerPawn` reliably at BeginPlay of the component (rather than at construction) — expected but re-verify against UE 5.7 component initialization order.

> **Note**: Knowledge Risk is MEDIUM. This ADR must be re-validated if the project
> upgrades engine versions. Flag as "Superseded" and author a new ADR on upgrade.

## ADR Dependencies

| Field | Value |
|-------|-------|
| **Depends On** | ADR-0007 (Accepted 2026-06-26) — this ADR consumes `URunStateMachineSubsystem::ForceTickNow()` + `OnStateChanged` + `OnPausedChanged` + `RequestAbort(EDPCAbortReason)` (only ForceTickNow + the two delegates for PM's direct consumption). ADR-0002 (Proposed; interface INT-002-amended 2026-06-26 — status gate is Polish-phase Hardware Verification per ADR-0002 lines 302 + 329, not a Pre-Production blocker) — PM's haptic dispatches (`SlipConfirmed` / `BufferDrop` / `NearMiss`) consume `IHapticDispatch::Fire(EHapticEvent)` + `IsSystemHapticsEnabled()`. The interface consumed by PM is codified + INT-002-stable; ADR-0009 promotion to Accepted is not gated on ADR-0002's Accepted status per the ADR-0005/0006-Accepted-before-ADR-0007 precedent from 2026-06-24/26 (see architecture-review-2026-07-03.md INT-007 for the pragmatic-promotion rationale + the alternative strict-gate resolution paths). |
| **Enables** | ADR-0005 (closes one-sided forward contract on `is_hw_performance_degraded` + `OnHardwarePerformanceBreach` + R11a-8 3.0-second grace window cited in ADR-0005 lines 252–258 and Wave Spawner R3a FC-3); Seam 12 `IPlayerMovementProvider` production wiring (Pull-Wave concern — `FPlayerMovementProvider_Production(UPlayerMovementComponent*)` constructor becomes callable); PM Epic implementation; Pull-Wave near-miss Rule 11 direct-read of `PM.current_lane` (PM's UPlayerLaneMovementComponent surface is now bound); Camera GDD (unauthored — inherits SD3 stationary-player forward-motion model as BINDING forward contract); Collision GDD (unauthored — inherits SD5 collision-commit contract) |
| **Blocks** | PM Epic implementation; all PM stories; first PM story acceptance (dev-story skill's SD-parameter binding cannot resolve without an Accepted ADR) |
| **Ordering Note** | Feature layer. Depends on Foundation ADR-0002 + Core ADR-0007. Independent of ADR-0001/0003/0004 (Input System chain). Must be Accepted before the first PM story enters implementation; may be Accepted in parallel with ADR-0008 (DPC) since PM does not consume DPC directly. |

## Context

### Problem Statement

Player Movement's three sub-GDDs (`design/gdd/player-movement-mechanics.md`, `player-movement-presentation.md`, `player-movement-platform.md` — authored 2026-06-28 per decomposition Step 2 PASS 8 CLOSED 2026-06-29) codify the mechanical contract, perception surface, and hardware contract in exhaustive detail, but four architectural choices remain GDD-open under mechanics §9 Open Questions:

1. **OQ-1** — PM UE object type ADR (blocking implementation prerequisite): `UActorComponent`, `UMovementComponent`, or `UPawnMovementComponent`. The choice affects tick registration, physics integration, position-update method, and which tick-ordering mechanism (see platform §3 Tick Ordering) is available for RSM→PM ordering.
2. **OQ-2** — Tween implementation ADR: `SafeMoveUpdatedComponent` vs `SetActorLocation`; DeltaTime source; delegate subscription style; forward-motion resolution (Option (a) shared movement struct / Option (b) contractual prohibition / Option (c) stationary-player recommended). Mechanics line 500 binds: "Option (c) must be selected before OQ-1 is finalized."
3. **OQ-3** — Input event binding ADR: SHARED with Input System OQ-1 (UMG overlay vs Slate `IInputProcessor`). Out of scope for this ADR — separate ADR resolves both.
4. **OQ-4** — DEAD-frame position authority: defer to Collision GDD authoring (out of scope for this ADR).

Downstream, the mechanics GDD's RSM Storage Contract (§3 lines 137–154) currently branches on "If OQ-7 ADR selects UActorComponent RSM: `GetOwner()->FindComponentByClass<URunStateMachineComponent>()`; if OQ-7 ADR selects UWorldSubsystem or UGameInstanceSubsystem: `GetWorld()->GetSubsystem<URunStateMachineSubsystem>()`". ADR-0007 SD1 selected `UGameInstanceSubsystem` — the subsystem branch is canonical, but PM's RSM resolution is not codified in an ADR. Similarly, platform §3 Tick Ordering enumerates only pre-ADR-0007 options (a) one-frame-lag and (b) manual `TickComponent` call from RSM's Tick — neither of which is the canonical pattern DPC ADR-0008 SD1 established (`RSM->ForceTickNow()` at controller Tick prologue). ADR-0009 codifies the canonical (c) pattern for PM in symmetry with DPC.

Seam 12 (`platform-seam-interfaces.md` lines 1354–1479, authored 2026-06-06) declares `IPlayerMovementProvider` with `FPlayerMovementProvider_Production(UPlayerMovementComponent* InPM)` — presuming PM is component-hosted. The seam interface is documented as "no ADR required" (line 1818) for the seam itself, but PM's HOSTING (object type, owning actor, tick ordering with the subsystem-hosted RSM, world-reload behavior, initialization) still requires an ADR — that ADR is this one.

The cost of deferring this ADR: (a) ADR-0005's PM-side forward contract (`is_hw_performance_degraded` + `OnHardwarePerformanceBreach` + R11a-8 3.0-second grace window) remains a GDD-level assertion; (b) Seam 12's `FPlayerMovementProvider_Production` constructor references a type (`UPlayerMovementComponent*`) whose ADR is Proposed at best; (c) Pull-Wave Rule 11 near-miss direct-read of `PM.current_lane` cannot be dev-implemented against a Proposed ADR (per `docs/CLAUDE.md`); (d) the first PM story cannot enter implementation.

### Current State

No PM implementation exists. The 3 PM sub-GDDs are the authoritative design documents (mechanics 1353 lines / presentation 547 lines / platform 565 lines; total 2465 lines post-decomposition + Steps 1–9 CLOSED 2026-07-02 per active.md). PM tr-registry entries TR-PM-001 through TR-PM-035 (routing verified Step 8b 2026-07-02) map every mechanics + presentation + platform requirement to a sub-GDD landing site. Seam 12 (`IPlayerMovementProvider` + `FPlayerMovementTestStub`) is declared but not wired to a production implementation.

ADR-0007 (RSM Hosting, Accepted 2026-06-26 with INT-004/005/006 amendments) codifies the RSM surface PM consumes. ADR-0008 (DPC Hosting, Accepted 2026-06-27) demonstrates the `RSM->ForceTickNow()` first-statement pattern + UCurveFloat validation at Initialize + `bCurveFallbackActive` shipping-safe fallback that PM's SD4 + SD6 mirror. ADR-0005 (Wave Spawner, Accepted 2026-06-24 with INT-004 amendment 2026-06-26) cites PM's `is_hw_performance_degraded` and `OnHardwarePerformanceBreach` as forward contracts.

### Constraints

- **Mobile platform, 60 fps, 16.6 ms frame budget** (`.claude/docs/technical-preferences.md`).
- **RSM is `UGameInstanceSubsystem`** (ADR-0007 SD1). PM cannot use `AddTickPrerequisiteComponent(RSMComponent)` — the API does not accept a subsystem outer.
- **`ForceTickNow()` non-callback invariant** (ADR-0007 SD2): PM may call `RSM->ForceTickNow()` but MUST NOT rely on it broadcasting `OnStateChanged` / `OnPausedChanged` synchronously — those subscribers observe transitions on RSM's next regular tick.
- **`RSM->bHasTickedThisFrame` idempotence** (ADR-0007 SD2 + INT-005 guard): PM + DPC both calling `ForceTickNow()` in the same frame is safe — first call runs the RSM tick body, subsequent calls short-circuit.
- **F-3 clamp semantics** (mechanics §4 F-3): interpolation output must land at `lane_world_x(target_lane)` exactly at `TweenProgress = 1.0` — no drift.
- **Rule 2 collision commitment** (mechanics §3): collision geometry commits to `target_lane` on the SETTLED→SLIPPING transition frame BEFORE any tween progress accumulates. Collision and visual positions are decoupled during the tween.
- **Rule 7 DEAD freeze** (mechanics §3): PM must preserve the fractional visual pose at DEAD entry — F-3 output at freeze must remain readable by Death Replay. Root committed to `target_lane`, mesh at fractional between-lane offset.
- **Shipping-Safety Enforcement Policy** (platform §Shipping-Safety): every BLOCKING invariant needs both a dev-build assertion AND a Shipping-safe guard. Applies to F-2 clamp, ordinal lockstep, tick-double-guard, curve fallback, `MIN_ESCAPE_SLIPS` static_assert.
- **Curve asset non-nullability at tick** (mechanics §Authored Asset Contracts): `SLIP_CURVE_ASSET`, `LEAN_CURVE_ASSET`, `EDGE_ABSORB_CURVE_ASSET` must be non-null when `TickComponent` runs, or the Shipping-safe fallback path must engage.
- **Haptic dispatch through ADR-0002 bridge** (presentation §Near-Miss Beat): PM's haptic call sites use `IHapticDispatch::Fire(EHapticEvent)` — not direct engine haptic API — and check `IHapticDispatch::IsSystemHapticsEnabled()` before firing for OS-state cert compliance.

### Requirements

- PM component ticks after RSM on the game frame; `RSM.current_state`, `RSM.is_paused`, `RSM.resume_grace` are current when PM reads them (mechanics §3 RSM Storage Contract).
- PM subscribes to `OnStateChanged` + `OnPausedChanged` in `BeginPlay` and unsubscribes in `EndPlay` with stored handles (mechanics §3 Delegate Binding Contract; ADR-0007 forward contract).
- PM writes lane X to the pawn root via `SetActorLocation(bSweep=false)` on SETTLED→SLIPPING (collision commit); mesh X via `SetRelativeLocation` per tick during SLIPPING (visual interpolation).
- Curve assets validated at BeginPlay; validation failure logs `Error`, sets `bCurveFallbackActive`, publishes fallback flag to telemetry per AC-SS-D.
- Public surface: `current_lane`, `target_lane`, `movement_state`, `lean_angle/head_lean_angle/arm_lean_angle`, `has_queued_input`, `queued_input_direction`, `collision_world_x`, `slip_complete_count`, `edge_absorb_trigger_count`, `commitment_tell_fire_count`, `bSlipTweenClampActive`, `is_hw_performance_degraded`, `OnSlipMidpoint`, `OnHardwarePerformanceBreach`, `TriggerNearMissBeat()`.
- `MIN_ESCAPE_SLIPS` `static_assert` in PM compile unit (AC-SS-E; mechanics §7 F-BARRAGE math consumer; drift caught at compile time).
- `ERunSlipState ↔ EMovementState` (Seam 12) ordinal lockstep `static_assert` at the seam cast site (AC-SS-B).
- Per-tick PM CPU < 0.15 ms p99 mid-tier mobile (Alpha-gate placeholder; measurement gate = watchdog `STAT_PMTick` scope-cycle-counter).

## Decision

`UPlayerLaneMovementComponent` is a `UActorComponent` attached to a new `ASlipstormPlayerPawn : public APawn`. The three design questions surfaced by the `/architecture-decision`-style AskUserQuestion pass on 2026-07-03 (scope, actor class, curve reference style) resolved to bundle OQ-1 + OQ-2, bind `ASlipstormPlayerPawn` here, and use hard-referenced UPROPERTY curves — enumerated across six Structural Decisions below.

### Structural Decision 1 — Class: `UPlayerLaneMovementComponent : public UActorComponent`

PM is hosted as a plain `UActorComponent` attached to the player pawn. Alternatives considered and rejected:

- **`UMovementComponent` subclass**: provides `SafeMoveUpdatedComponent(FVector Delta, FRotator NewRotation, bool bSweep, FHitResult& OutHit)` for swept-collision-safe transform updates. Rejected because (a) PM's collision commitment is a discrete lane snap on the SETTLED→SLIPPING transition (Rule 2), NOT a swept continuous move — the sweep machinery adds cost + complexity for a case that never uses it; (b) `SafeMoveUpdatedComponent` sweep-resolution semantics changed across UE 5.4–5.7 per breaking-changes.md, adding post-cutoff verification burden; (c) `UpdatedComponent` presumes a `USceneComponent` root the movement component owns — PM writes both the pawn root (via `GetOwner()->SetActorLocation`) AND the child mesh (via `SetRelativeLocation`), which the `UpdatedComponent` idiom does not model cleanly.

- **`UPawnMovementComponent` subclass**: presumes `APawn::SetMovementComponent` integration + Character-Movement-Component-adjacent semantics (`ConsumeInputVector`, `AddInputVector`, `Velocity`, `bWantsToJump`, etc.). Rejected because PM does not consume a movement vector — it consumes discrete `slip-left`/`slip-right` events into a lane state machine. The Pawn-movement idiom's input-vector accumulator is dead weight; the Character-adjacent methods have no meaning for lane snapping.

- **Chosen: plain `UActorComponent`**: matches every documented assumption in the three PM sub-GDDs (`TickComponent`, `PrimaryComponentTick.bCanEverTick`, `BeginPlay`, `EndPlay`, `GetOwner()`) and in Seam 12's production constructor (`FPlayerMovementProvider_Production(UPlayerMovementComponent* InPM)`). Simplest fit; no engine machinery for use cases PM never invokes.

### Structural Decision 2 — Owning actor: `ASlipstormPlayerPawn : public APawn`

The player pawn is `ASlipstormPlayerPawn`, an `APawn` subclass. This ADR is the binding site for the class name — no other design or architecture document names a pawn class as of 2026-07-03.

`APawn` (NOT `ACharacter`) was chosen because:

- **`ACharacter` includes a mandatory `UCharacterMovementComponent`** whose lateral movement logic conflicts with PM's ownership of the X axis (mechanics §3 Cross-Component Interfaces — PM writes X exclusively; forward motion is out per SD3).
- **`ACharacter` presumes a capsule collision** — SLIPSTORM's collision authority is Pillar-3 pull-wave targeting against lane indices, not capsule sweep; the capsule is dead weight.
- **`ACharacter` includes gravity + jump + crouch** machinery — none apply.
- **`APawn` is the minimal spawn-and-possess base**: `AController::Possess`, root component, tick, replicated skeleton (though SLIPSTORM is solo — no network relevance). Extension is additive; nothing to strip.

`ASlipstormPlayerPawn` owns `UPlayerLaneMovementComponent` as an `UPROPERTY(VisibleDefaultsOnly, Category="SLIPSTORM|Movement") TObjectPtr<UPlayerLaneMovementComponent> MovementComponent` — subobject-created in the pawn's constructor via `CreateDefaultSubobject<>()`. Game mode spawns the pawn at `IDLE → COUNTDOWN` state transition (details deferred to a future GameMode ADR); pawn's root scene component provides the actor location surface for PM's SetActorLocation X-only writes. Blueprint subclass (`BP_SlipstormPlayerPawn`) authoring is deferred to the first PM story per Risk 4 (no BP asset architecture decision made here).

### Structural Decision 3 — Forward-motion model: Option (c) stationary-player, moving-world (OQ-2 forward-motion closure)

The player actor is stationary in world space. The track and hazards move toward the camera. PM writes ONLY the lateral X coordinate via `SetActorLocation` (Z/Y untouched); no forward-motion component exists to conflict.

This closes mechanics OQ-2 forward-motion resolution (mechanics §3 Cross-Component Interfaces > Forward Motion Dual-Writer) on the recommended Option (c) path, unblocking OQ-1 finalization per mechanics line 500 ("Option (c) must be selected before OQ-1 is finalized").

Alternatives rejected:

- **Option (a) shared movement struct** (PM writes X, forward-motion writes Z, single `SetActorLocation` call integrates both per tick): adds an actor-level state struct + synchronization concern between two components; forward-motion component would need to know when PM last wrote to X to avoid clobbering; not modeled in any existing GDD.
- **Option (b) PM writes X only; forward-motion contractually prohibited from `SetActorLocation`**: forward-motion component would need an alternative transform-write mechanism (child scene component with local translation), which is Option (c) in a different frame of reference — pick Option (c) directly.

**Consequence**: `lateral_world_position` (mechanics §3 Public Interface) is renamed semantically to describe track-space rather than world-space per mechanics §3 line 115's Option-(c) clarification; the property name is retained for downstream interface stability. Camera-system GDD (unauthored) inherits the moving-world contract as a BINDING forward contract — camera + track + hazards are all in the moving-world frame; player + PM + collision are in the stationary-world frame.

### Structural Decision 4 — Tick ordering: `RSM->ForceTickNow()` at TickComponent prologue

`UPlayerLaneMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)` body's **FIRST STATEMENT** after `check(IsInGameThread())` is `RSMSubsystem->ForceTickNow()`. This mirrors ADR-0008 SD1 pattern for `UDPCController::Tick`. The invocation is engine-scheduler-independent; RSM's `bHasTickedThisFrame` guard (ADR-0007 SD2, INT-005-amended 2026-06-26) makes the call idempotent within a single engine frame — if RSM's regular `Tick()` already ran this frame, `ForceTickNow()` short-circuits; if DPC's `Tick()` already called `ForceTickNow()` this frame, the same guard short-circuits.

**Non-callback invariant preserved (ADR-0007 SD2)**: PM subscribes to `OnStateChanged` + `OnPausedChanged` via `AddUObject` in `BeginPlay` and expects those callbacks to fire on RSM's next REGULAR tick, NOT synchronously inside `ForceTickNow()`. PM's `HandleStateChanged` / `HandlePausedChanged` bodies (mechanics §3 Delegate Handler Bodies pseudo-code) tolerate the 1-frame-latency semantic — PM's tick body reads `RSM->GetCurrentState()`, `RSM->IsPaused()`, `RSM->IsResumeGrace()` directly (per-tick property reads) for zero-latency gating decisions, and relies on the callbacks only for one-shot terminal-entry side effects (COUNTDOWN reset, DEAD freeze, COMPLETE/ABORTED snap).

**Platform §3 (a)/(b) enumeration superseded**: player-movement-platform.md §3 Tick Ordering lines 57–64 enumerate only pre-ADR-0007 options (a) one-frame-lag reads + (b) manual `TickComponent` call from RSM's Tick. Both options predate ADR-0007's `ForceTickNow()` publication (2026-06-26). This ADR's SD4 (option (c) — PM pulls RSM at PM tick prologue) is the canonical pattern; a hygiene edit to platform §3 marking (a)/(b) as SUPERSEDED is queued as post-ADR work (per user session-scope selection 2026-07-03; not in this ADR's file-modification scope).

**Platform §Shipping-Safety AC-SS-C row (early-out on `bManualTickEnabled` flag)** becomes a defensive no-op under SD4 — PM never opts into manual-tick — but the AC is retained for forward-compatibility against a future RSM object-type change (per platform §6 Bidirectional Notes item 4).

### Structural Decision 5 — Collision commitment: `SetActorLocation` on pawn root; `SetRelativeLocation` on mesh child (mechanics Rule 2 closure)

On the SETTLED→SLIPPING transition frame (mechanics Rule 2), PM performs two decoupled transform writes:

1. **Root commit**: `GetOwner()->SetActorLocation(FVector(lane_world_x(target_lane), 0.0f, 0.0f), /*bSweep=*/false)`. `bSweep=false` skips Chaos physics resolution — the pawn has no physics body (SD2 rationale); this is a constant-time transform update per Engine Compatibility Verification #3. Y/Z left at 0 in track-space per SD3 stationary-player invariant.
2. **Mesh interpolation** (during SLIPPING): PM's TickComponent writes `MeshComponent->SetRelativeLocation(FVector(F-3(TweenProgress), 0.0f, 0.0f))` each tick where `F-3(TweenProgress)` is mechanics §4 F-3 output — starting at `lane_world_x(source_lane) - lane_world_x(target_lane)` (a negative or positive offset from the committed root) at `TweenProgress = 0.0` and converging to `0.0` at `TweenProgress = 1.0`.

**Invariant**: mesh WORLD X = pawn root world X + mesh relative X. Under SD3 stationary-player, pawn root world X is `lane_world_x(target_lane)` after commit; mesh world X therefore equals `lane_world_x(target_lane) + F-3_relative(TweenProgress)` — which visually reads as interpolating from source_lane toward target_lane, with root already committed. F-3's clamp semantic (mechanics §4 F-3) applies to the RELATIVE offset (not the absolute) — at `TweenProgress = 1.0`, F-3_relative = 0.0 exactly, so mesh world equals target_lane exactly.

**DEAD freeze** (mechanics Rule 7): PM's `HandleStateChanged(DEAD)` sets `movement_state = SETTLED` per R7-PM-PROPAGATION-REVIEW; leaves `TweenProgress` at its fractional value; leaves mesh relative offset at its last-tick value. Root remains at `lane_world_x(target_lane)` (committed at tween start), mesh remains at fractional world X — Death Replay reads the frozen fractional pose.

**Rejected alternative — separate collision volume**: track a `USceneComponent` collision volume independently of the actor root, tween the volume in isolation. Rejected because it doubles the transform-write surface, requires a third scene component in the pawn hierarchy, and provides no gameplay benefit — Pull-Wave's collision reads (per Pull-Wave Rule 10) use PM's `current_lane` / `target_lane` accessor semantics, NOT direct actor-transform reads, so the collision authority is the accessor, not the transform.

### Structural Decision 6 — Curve validation at `BeginPlay`; hard-referenced `UPROPERTY() TObjectPtr<UCurveFloat>` fields

PM's three authored curve assets are declared as hard-referenced UPROPERTY fields:

```cpp
UPROPERTY(EditDefaultsOnly, Category="SLIPSTORM|Movement|Curves")
TObjectPtr<UCurveFloat> SlipCurve;      // F-3 lateral interpolation

UPROPERTY(EditDefaultsOnly, Category="SLIPSTORM|Movement|Curves")
TObjectPtr<UCurveFloat> LeanCurve;      // F-5 body/head/arm lean

UPROPERTY(EditDefaultsOnly, Category="SLIPSTORM|Movement|Curves")
TObjectPtr<UCurveFloat> EdgeAbsorbCurve; // F-6 edge-absorb tail phase
```

Hard reference (not soft reference + `LoadSynchronous`) guarantees curve availability at `BeginPlay()` for the validation pass. Mirrors ADR-0008 SD2 pattern for DPC's curve fields. Editor-assigned via the `EditDefaultsOnly` specifier; Blueprint subclass `BP_SlipstormPlayerPawn` (deferred authoring) will assign concrete asset paths.

`BeginPlay()` runs the validation gate borrowed from ADR-0008 SD2:

```cpp
bool ValidateCurveAsset(const TObjectPtr<UCurveFloat>& Curve, const TCHAR* CurveName)
{
    if (!Curve)
    {
        UE_LOG(LogPlayerMovement, Error, TEXT("Curve asset '%s' is null"), CurveName);
        return false;
    }
    const FRichCurve& FloatCurve = Curve->FloatCurve;
    if (FloatCurve.Keys.Num() < 2)
    {
        UE_LOG(LogPlayerMovement, Error, TEXT("Curve '%s' has fewer than 2 keys"), CurveName);
        return false;
    }
    const float FirstKey = FloatCurve.Keys[0].Time;
    const float LastKey  = FloatCurve.Keys.Last().Time;
    if (FirstKey > 0.0f || LastKey < 1.0f)
    {
        UE_LOG(LogPlayerMovement, Error,
            TEXT("Curve '%s' keys [%f, %f] do not span [0.0, 1.0]"),
            CurveName, FirstKey, LastKey);
        return false;
    }
    return true;
}
```

**Failure path — Shipping-safe fallback (AC-SS-D)**: if any curve fails validation, PM logs `Error` per curve, sets `bCurveFallbackActive = true` (public read-only flag; visible to debug overlay + HUD telemetry path per platform §Shipping-Safety Enforcement Policy row 4), and the fallback path engages:

- F-3 (`lateral_world_position`): linear interpolation from `source_lane` to `target_lane` over `TweenProgress ∈ [0, 1]` — no curve shaping.
- F-5 (`lean_angle` / `head_lean_angle` / `arm_lean_angle`): zero lean output — mesh does not rotate.
- F-6 (`edge_absorb` tail): immediate return — no tail-phase animation; edge-absorb trigger still increments `edge_absorb_trigger_count` for AC parity.

PM continues to function (no crash, no divide-by-zero) but the player perceives a degraded experience. The fallback is a Shipping-safety net, not a design intent.

**Rejected alternatives**:

- **Soft reference + `LoadSynchronous` at BeginPlay**: adds a load-blocking point at BeginPlay for negligible memory savings (3 curves on mobile). If load fails (missing asset), the fallback path engages anyway — same net observable behavior; hard ref is simpler.
- **UDataAsset wrapper (`UPlayerMovementCurveSet` referencing the 3 curves)**: extra indirection only justified if per-difficulty curve variants are anticipated. No such requirement in the GDDs. Ships with a new OQ for the DataAsset schema — increases scope without benefit.

## Architecture

```
UGameInstance
    │
    ├── URunStateMachineSubsystem                 [ADR-0007 SD1 — UGameInstanceSubsystem + FTickableGameObject]
    │       │
    │       ├── ForceTickNow()                     [Public; idempotent within frame via bHasTickedThisFrame]
    │       ├── OnStateChanged                     [MULTICAST_DELEGATE — non-dynamic]
    │       ├── OnPausedChanged                    [MULTICAST_DELEGATE — non-dynamic]
    │       └── GetCurrentState() / IsPaused() / IsResumeGrace() / GetRemainingTime()
    │
    └── UDPCSubsystem                             [ADR-0008 SD1 — wrapper around UDPCController]
            └── (out of PM's direct consumption scope)

UWorld
    │
    └── ASlipstormPlayerPawn                      [ADR-0009 SD2 — APawn subclass; spawned at IDLE→COUNTDOWN]
            │
            ├── RootComponent (USceneComponent)   [Stationary in world space per SD3]
            │       │
            │       └── MeshComponent             [Attached; SetRelativeLocation for F-3 interpolation per SD5]
            │
            └── MovementComponent (UPlayerLaneMovementComponent)
                        │                          [ADR-0009 SD1 — UActorComponent]
                        │
                        ├── BeginPlay()
                        │       ├── Resolve RSMSubsystem = GetWorld()->GetGameInstance()
                        │       │                              ->GetSubsystem<URunStateMachineSubsystem>()
                        │       ├── ValidateCurveAsset(SlipCurve, LeanCurve, EdgeAbsorbCurve)   [SD6]
                        │       │       └── On failure: bCurveFallbackActive = true; fallback path per SD6
                        │       ├── StateChangedHandle  = RSM->OnStateChanged.AddUObject(this, ...)
                        │       ├── PausedChangedHandle = RSM->OnPausedChanged.AddUObject(this, ...)
                        │       ├── Initialize TickDTRollingBuffer[60] = 0.01667f each  [Platform R11a-6 sentinel]
                        │       └── PrimaryComponentTick.bCanEverTick = true
                        │
                        ├── TickComponent(DeltaTime)
                        │       ├── check(IsInGameThread())
                        │       ├── RSM->ForceTickNow()                                  [SD4 — FIRST after thread check]
                        │       ├── §4 F-PROLOGUE: raw_dt = FApp::GetDeltaTime();
                        │       │                   effective_dt = clamp(raw_dt, 0, MAX_SLIP_DT_S)
                        │       ├── Watchdog: push raw_dt into TickDTRollingBuffer; detect breach [platform §3]
                        │       ├── RSM gate: if not (RUNNING && !paused && !resume_grace) skip mechanics update
                        │       ├── F-2: TweenProgress += effective_dt / effective_slip_tween  [mechanics §4]
                        │       ├── F-3: MeshComponent->SetRelativeLocation(F-3(TweenProgress))  [SD5]
                        │       ├── F-5: MeshComponent->SetRelativeRotation(F-5(TweenProgress))
                        │       ├── F-6: if edge_absorb_active, advance edge_absorb_local_timer
                        │       ├── On TweenProgress crossing 0.5: OnSlipMidpoint.Broadcast(...)
                        │       └── On TweenProgress >= 1.0: CompleteTween() [→ SETTLED, buffer flush]
                        │
                        ├── SETTLED→SLIPPING transition (from HandleSlipTransition)
                        │       ├── GetOwner()->SetActorLocation(lane_world_x(target_lane), bSweep=false)  [SD5 root commit]
                        │       ├── LeadingFaceFlash material param = ±0.80    [Presentation §3 trigger — cadence-gated]
                        │       ├── commitment_tell_fire_count += 1
                        │       └── Set movement_state = SLIPPING; F-2 begins advancing
                        │
                        ├── HandleStateChanged(OldState, NewState)             [Bound in BeginPlay per mechanics §3]
                        │       └── (Full body pseudo-code in mechanics §3 Delegate Handler Bodies)
                        │
                        ├── HandlePausedChanged(bNewPaused)                    [Bound in BeginPlay]
                        │       └── (Logging-only body; F-2 handles pause via per-tick reads)
                        │
                        ├── TriggerNearMissBeat()                              [Public — called by Pull-Wave]
                        │       ├── If IHapticDispatch::IsSystemHapticsEnabled() &&
                        │       │      IGameSettings::IsNearMissHapticEnabled()  [Presentation §Near-Miss Beat]
                        │       │       └── IHapticDispatch::Fire(EHapticEvent::NearMiss)   [ADR-0002 bridge]
                        │       └── Start avatar Y-dip animation (component-local MeshComponent Z offset)
                        │
                        ├── EndPlay(EndPlayReason)
                        │       ├── RSM->OnStateChanged.Remove(StateChangedHandle)          [Guarded by IsValid(RSM)]
                        │       ├── RSM->OnPausedChanged.Remove(PausedChangedHandle)
                        │       └── PrimaryComponentTick.bCanEverTick = false
                        │
                        ├── Public read surface (mechanics §3 + platform §3):
                        │       ├── EPlayerLane current_lane, target_lane
                        │       ├── ERunSlipState movement_state
                        │       ├── float lateral_world_position, tween_progress
                        │       ├── float lean_angle, head_lean_angle, arm_lean_angle
                        │       ├── bool has_queued_input; ESlipDirection queued_input_direction
                        │       ├── int32 slip_complete_count, edge_absorb_trigger_count, commitment_tell_fire_count
                        │       ├── bool bSlipTweenClampActive, bCurveFallbackActive
                        │       └── bool is_hw_performance_degraded
                        │
                        └── Public delegates:
                                ├── OnSlipMidpoint                             [MULTICAST_DELEGATE_TwoParams non-dynamic]
                                └── OnHardwarePerformanceBreach                [MULTICAST_DELEGATE_OneParam non-dynamic]

External consumers (bind at their Initialize/BeginPlay):
    ├── Wave Spawner (ADR-0005) — reads is_hw_performance_degraded; binds OnHardwarePerformanceBreach
    ├── HUD (unauthored)         — binds OnHardwarePerformanceBreach for Performance-mode banner
    ├── Pull-Wave (Rule 11)      — direct read of PM.current_lane during SLIPPING; calls TriggerNearMissBeat()
    └── Seam 12 Production wrap  — FPlayerMovementProvider_Production(UPlayerMovementComponent*)
```

## Consequences

### Positive

- **ADR-0005 forward contract closed**: PM's public surface (`is_hw_performance_degraded` + `OnHardwarePerformanceBreach` + R11a-8 3.0 s grace-window semantic) is now ADR-bound. Wave Spawner's `Initialize()` can bind to the delegate at Story-Ready gate.
- **Pull-Wave Rule 11 near-miss unblocked**: `PM.current_lane` direct-read surface is declared on `UPlayerLaneMovementComponent`; source-lane semantic during SLIPPING per mechanics Rules 4 + 7 holds.
- **Seam 12 production path bound**: `FPlayerMovementProvider_Production(UPlayerMovementComponent* InPM)` becomes constructor-callable against a concrete type.
- **Structural symmetry with ADR-0007/0008**: PM ↔ DPC both use `RSM->ForceTickNow()` at Tick prologue; both use hard-ref UPROPERTY UCurveFloat + Initialize-time validation + Shipping-safe fallback. Reviewers can cross-check patterns.
- **New binding site for `ASlipstormPlayerPawn`**: pawn class name is now referenceable in future ADRs, stories, and GDDs without further deliberation.
- **Camera-GDD forward contract established**: stationary-player + moving-world model is ADR-locked; camera authoring will inherit it as a BLOCKING contract.
- **Collision-decoupling invariant simple to verify**: root-commits + mesh-interpolates is a two-transform-per-frame pattern; test evidence is straightforward (assert root at target lane immediately post-commit; assert mesh world = root + relative each tick).

### Negative

- **Camera GDD unauthored**: SD3 stationary-player model creates a forward contract on Camera that has no receiver yet. If Camera authoring elects a different reference frame (world-space camera + moving player), this ADR must be re-litigated. Advisor-flagged Risk 3.
- **`ASlipstormPlayerPawn` class name is a binding without Blueprint subclass architecture**: BP_SlipstormPlayerPawn asset creation + level-blueprint spawn wiring is deferred to the first PM story. If Blueprint layer needs its own architectural decision (e.g., which subsystem-driven GameMode owns spawn timing), a follow-up ADR is required.
- **Curve hard references bloat cook graph**: 3 UCurveFloat hard refs from PM component always cook into the shipping build. Negligible size (curves are trivially small), but consumers of `docs/architecture/current-best-practices.md` may prefer soft refs by default. Documented as accepted trade-off per ADR-0008 precedent.
- **Platform §3 GDD supersession is queued, not landed**: the (a)/(b) tick-ordering enumeration will read as canonical until the hygiene edit lands. A reader may see the pre-ADR-0007 options first and be confused. Mitigated by SD4 prose explicitly citing supersession.
- **`APawn` rejection of `ACharacter` locks out future gravity/jump**: if the design ever adds a jump mechanic, SLIPSTORM would need to add a CharacterMovementComponent-adjacent surface manually or revisit SD2. Design pillars (endless-runner + slip-only movement) make this unlikely, but it's a durable constraint.

### Neutral

- **`UActorComponent` (not `UMovementComponent`) means no `UpdatedComponent`**: PM writes both pawn root and mesh; the two transforms are managed via `GetOwner()->SetActorLocation` and `MeshComponent->SetRelativeLocation` respectively. No engine-provided abstraction for "the thing I'm moving" — PM manages both explicitly. This is aligned with the collision/visual decouple invariant.
- **`FTickableGameObject` NOT used**: PM ticks via `UActorComponent::TickComponent` (default) — no separate tickable interface. Consistent with the component pattern; unlike ADR-0007 (RSM adds FTickableGameObject to a UGameInstanceSubsystem) and ADR-0008 (DPC's UObject controller adds FTickableGameObject).

## Risks

| # | Risk | Likelihood | Mitigation |
|---|---|---|---|
| **1** | Root `SetActorLocation` + child `SetRelativeLocation` decouple invariant fails: F-3 interpolation clamp is applied to the ABSOLUTE mesh world position instead of the RELATIVE offset from the committed root. Result: mesh world position drifts past target_lane at TweenProgress = 1.0. | LOW | SD5 prose explicitly binds F-3's clamp target to the RELATIVE offset. Implementation Guideline 5 codifies the invariant. Test evidence: assert mesh world position equals `lane_world_x(target_lane)` exactly at TweenProgress = 1.0 across the 5-lane × 5-lane transition matrix (mechanics AC-14, AC-19). |
| **2** | Curve hard-ref cook-time inclusion violates a future `current-best-practices.md` addition that forbids hard-ref UPROPERTY UCurveFloat outside DataAsset wrappers. | LOW | ADR-0008 SD2 precedent for DPC's curve fields is identical; this ADR follows the same pattern. If a future best-practices update forbids the pattern, ADR-0008 + ADR-0009 must be reviewed jointly. `docs/engine-reference/unreal/current-best-practices.md` as of 2026-07-03 does not forbid the pattern. |
| **3** | Camera GDD unauthored — the moving-world forward-motion contract in SD3 is a forward contract without a receiver. Camera authoring may elect a world-space camera + moving-player model, invalidating SD3. | MEDIUM | Documented as a Consequence (Negative). Camera GDD is queued for authoring in the Camera epic; producer will run `/propagate-design-change design/gdd/player-movement-mechanics.md` if the Camera receiver ships a different frame. This ADR is the authoritative source; Camera inherits it unless a follow-up ADR rescinds. |
| **4** | `ASlipstormPlayerPawn` binding here does not include BP subclass strategy: BP_SlipstormPlayerPawn asset creation, level-blueprint spawn wiring, and GameMode spawn-timing binding are all deferred. First PM story will surface these as new open questions. | LOW | Documented as a Consequence (Negative). First PM story is Story-Ready-gated on this ADR being Accepted; the BP subclass strategy will be a story-level implementation decision, not an ADR-level architectural decision, unless it grows in complexity. |
| **5** | Cross-ADR re-entrancy hazard: PM + DPC both call `RSM->ForceTickNow()` in the same frame. If UE 5.7's engine tick scheduler orders PM tick BEFORE DPC tick, PM's `ForceTickNow()` runs the RSM body; DPC's `ForceTickNow()` short-circuits (bHasTickedThisFrame=true). DPC's subsequent reads of `RSM->GetCurrentState()` are current. If reversed, symmetric. Non-callback invariant + bHasTickedThisFrame idempotence (INT-005) cover both orderings. | LOW | ADR-0007 SD2 + INT-005 amendment codify the idempotence; this ADR's Engine Compatibility Verification #5 explicitly re-verifies. Registry forbidden pattern `RunStateMachine_Tick_set_bHasTickedThisFrame_without_prior_guard` (added 2026-06-26 v6) prevents regression. |
| **6** | `SetActorLocation(bSweep=false)` on APawn root triggers unexpected Chaos physics query in UE 5.7 mobile forward renderer. | LOW | Engine Compatibility Verification #3 gates this. APawn has no physics body by default (rejected `ACharacter` in SD2 precisely to avoid mandatory physics); `bSweep=false` skips sweep resolution; per UE 5.7 documentation, no Chaos entry is expected. Test evidence: profiler frame trace of PM tick body shows no Chaos scoped events. |
| **7** | Delegate handles from `AddUObject` are not stored, breaking `EndPlay` unsubscribe → RSM broadcasts to a stale WeakObjectPtr → deferred crash on PIE shutdown. | LOW | mechanics §3 Delegate Binding Contract already codifies handle storage as member variables. Implementation Guideline 3 in this ADR re-codifies. Not addressed here is the storage discipline itself — first PM story acceptance criterion asserts handles stored + removed. |

## Migration Plan

Since no PM implementation exists, "migration" describes the sequence for a first-implementation from-zero flow.

**Step 1 — Pre-implementation prerequisites**:
- ADR-0009 status = Accepted (this ADR)
- ADR-0007 status = Accepted ✅ (2026-06-26)
- ADR-0002 status = Accepted ✅ (2026-06-11)
- All 3 PM sub-GDD R12a author revision passes CLOSED (currently pending per Step 9 CLOSED entry — must land before first PM story enters implementation)

**Step 2 — Create `ASlipstormPlayerPawn` skeleton** (first PM story):
- New file: `Source/SLIPSTORM/Public/Player/SlipstormPlayerPawn.h` + `.cpp`
- Class declaration: `class ASlipstormPlayerPawn : public APawn`
- Constructor: `CreateDefaultSubobject<UPlayerLaneMovementComponent>(TEXT("MovementComponent"))`; `RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"))`; `MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"))`; `MeshComponent->SetupAttachment(RootComponent)`
- Blueprint subclass `BP_SlipstormPlayerPawn` created in `Content/Player/`; concrete mesh + material assignments
- Deferred: GameMode spawn wiring — a follow-up ADR or the GameMode story handles this

**Step 3 — Create `UPlayerLaneMovementComponent` skeleton**:
- New file: `Source/SLIPSTORM/Public/Player/PlayerLaneMovementComponent.h` + `.cpp`
- Class declaration: `class UPlayerLaneMovementComponent : public UActorComponent`
- UPROPERTY fields per mechanics §3 Public Interface: `EPlayerLane`, `ERunSlipState`, F-2/F-5/F-6 curves per SD6, counters, `bSlipTweenClampActive`, `bCurveFallbackActive`, `is_hw_performance_degraded`, `TickDTRollingBuffer[60]`, `TickDTRingIndex`, `ContinuousCleanWindowTime`, `bHardwarePerformanceBreachActive`, F-6 fadeout snapshot fields per R11a §6.1
- Delegates: `OnSlipMidpoint`, `OnHardwarePerformanceBreach` — `DECLARE_MULTICAST_DELEGATE_*` non-dynamic (per mechanics §3 R10a-12 + platform §3 R10a-1.3)
- `static_assert(static_cast<uint8>(ERunSlipState::SETTLED) == static_cast<uint8>(EMovementState::SETTLED), "ordinal drift PM ↔ Seam 12 ↔ R7-PM-PROPAGATION-REVIEW")` at cast site (AC-SS-B)
- `static_assert(MIN_ESCAPE_SLIPS == 2, "PM Hardware Contract math assumes MIN_ESCAPE_SLIPS=2; see F-BARRAGE-SURVIVABILITY-INVARIANT-CORNERS")` at local constant declaration (AC-SS-E)
- `BeginPlay()`: resolve RSMSubsystem, validate curves (SD6), bind delegates (mechanics §3 Delegate Binding Contract), initialize watchdog buffer sentinel (platform R11a-6), enable ticking
- `TickComponent(DeltaTime)`: implement per SD4 + mechanics §4 F-2/F-5/F-6 + platform watchdog per §3 DT watchdog pseudo-code
- `EndPlay(EndPlayReason)`: unbind delegates (guarded by `IsValid(RSMSubsystem)`), disable ticking

**Step 4 — Implement Delegate Handler Bodies** (mechanics §3 pseudo-code is the reference):
- `HandleStateChanged(OldState, NewState)` — full body per mechanics §3 lines 191–295
- `HandlePausedChanged(bNewPaused)` — logging-only body per mechanics §3 lines 311–326
- `HandleSlipTransition(ESlipDirection)` — routes valid slip inputs through the state machine + fires SETTLED→SLIPPING transition + LeadingFaceFlash + collision commit per SD5

**Step 5 — Wire Seam 12 production provider**:
- `FPlayerMovementProvider_Production` constructor call site TBD (Pull-Wave concern per Seam 12 doc line 1358 "no ADR required" for the seam itself)
- Pull-Wave story-side implementation binds; PM story documents the call surface only

**Step 6 — Verify against acceptance criteria** (mechanics + presentation + platform §8):
- Mechanics AC-01 through AC-COUNTER-* (lane model, buffer, RSM gating, pause/grace, terminal states, source-lane semantic, counter persistence)
- Presentation AC-COMMIT-* + AC-AUDIO-* + AC-NEARMISS-* + AC-COMMIT-FLASH-CADENCE (commitment-tell, audio ducking, near-miss haptic, IEC 61966-2-2 PEAT cadence)
- Platform AC-HW-A/B/C + AC-SS-A through E + AC-21 (Hardware Contract watchdog, Shipping-Safety enforcement)
- Test evidence per story-type per `.claude/docs/coding-standards.md`: Logic stories (F-1/F-2/F-3/F-4/F-5/F-6 formula tests) — automated unit tests; Integration stories (PM ↔ RSM tick interleave) — automated integration tests or documented playtest; Visual/Feel stories (lean animation, commitment-tell) — screenshots + lead sign-off

**Step 7 — Retire platform §3 (a)/(b) enumeration**:
- Edit `design/gdd/player-movement-platform.md` §3 Tick Ordering lines 57–64 to mark options (a)/(b) as SUPERSEDED by ADR-0009 SD4; canonical pattern is (c) `RSM->ForceTickNow()` pull primitive
- Update platform §Shipping-Safety AC-SS-C row to note that the AC is a forward-compat defensive no-op under SD4 (not the primary defense)
- Post-ADR hygiene; not in this session's file-modification scope per user picker

## Validation Criteria

The following gates must pass before this ADR moves from `Proposed` → `Accepted`:

1. **Cross-ADR consistency**: `/architecture-review` Phase 4 shows zero conflicts between ADR-0009 SD4 and ADR-0007 SD2 / ADR-0008 SD1 (all three use `ForceTickNow()` at Tick prologue).
2. **Traceability**: `docs/architecture/tr-registry.yaml` post-Step-8b routing shows every TR-PM-001 through TR-PM-035 has a `gdd:` field pointing to the correct sub-GDD; ADR-0009 references the routing (Step 8b CLOSED 2026-07-02).
3. **Seam consistency**: `platform-seam-interfaces.md` Seam 12 `FPlayerMovementProvider_Production` constructor signature `(UPlayerMovementComponent*)` matches SD1's chosen class name (`UPlayerLaneMovementComponent`) OR the seam doc gets a small naming reconciliation edit. Verify: `grep -n "UPlayerMovementComponent\|UPlayerLaneMovementComponent" docs/architecture/platform-seam-interfaces.md`.
4. **Registry updates landed**: `docs/registry/architecture.yaml` v7 → v8 with 2 new forbidden_patterns (SetActorRotation for lean; PM Tick without prior ForceTickNow) — see Implementation Guidelines below for exact patterns.
5. **Static-assert coverage**: PM compile unit contains at least 2 `static_assert` statements — one at the ERunSlipState↔EMovementState seam cast site (AC-SS-B), one at the MIN_ESCAPE_SLIPS local constant declaration (AC-SS-E). Grep gate: `grep -rn "static_assert" Source/SLIPSTORM/**/PlayerMovement*.cpp` returns ≥ 2 hits.
6. **First PM story author brief**: `docs/architecture/adr-0009-player-movement-hosting.md` linked from the first PM story's ADR-list at story-authoring time (`/create-stories`).
7. **Non-callback invariant preserved**: PM's `TickComponent` body does not call any RSM subscriber broadcast (`OnStateChanged.Broadcast` / `OnPausedChanged.Broadcast`) — verified via grep gate in PM source.
8. **Fallback flag reachable**: `bCurveFallbackActive` public read-only accessor confirmed present + reads reachable from the debug overlay (per platform §Shipping-Safety row 4).

## GDD Requirements Addressed

This ADR closes the following technical requirements against the 3 PM sub-GDDs. Full routing verified at PM decomposition Step 8b CLOSED 2026-07-02 (`docs/architecture/tr-registry.yaml` v6+).

| Requirement | Source sub-GDD | Structural Decision(s) | Note |
|---|---|---|---|
| **TR-PM-001** — component + property-read interface to RSM | mechanics §3 RSM Storage Contract | SD1 (UActorComponent), SD4 (ForceTickNow prologue) | RSM resolved via GetSubsystem at BeginPlay |
| **TR-PM-002** — `current_lane` / `target_lane` `EPlayerLane` | mechanics §3 Public Interface | SD1 (component surface) | 5-lane enum widened per R7-PM-PROPAGATION |
| **TR-PM-003** — `ERunSlipState` ordinals pinned {SETTLED=0, SLIPPING=1} | mechanics §Movement State Enum | SD1 + AC-SS-B static_assert | Seam 12 lockstep |
| **TR-PM-004** — F-1 `lane_world_x` | mechanics §4 F-1 | SD3 (track-space) | Lane centers at track-X origin under Option (c) |
| **TR-PM-005** — F-2 `TweenProgress` + `MAX_SLIP_DT_S` cap | mechanics §4 F-2 | SD4 (effective_dt from F-PROLOGUE) | RSM gate + clamp per R11a-1 |
| **TR-PM-006** — `SLIP_TWEEN` persistent clamp | mechanics §4 F-2 + platform §Shipping-Safety | SD4 + AC-SS-A | bSlipTweenClampActive flag surface |
| **TR-PM-007** — F-3 `lateral_world_position` + `SlipCurve` | mechanics §4 F-3 | SD5 (SetRelativeLocation on mesh) + SD6 (curve validation) | Relative offset semantics |
| **TR-PM-008** — F-5 body/head/arm lean + `LeanCurve` | mechanics §4 F-5 | SD5 (SetRelativeRotation on mesh) + SD6 | HEAD_LAG_PROGRESS + ARM_LEAD_PROGRESS |
| **TR-PM-009** — F-6 edge-absorb 2-frame fade | mechanics §4 F-6 | SD6 (EdgeAbsorbCurve validation) | R11a-3 fade-out ticks |
| **TR-PM-010** — RSM delegate subscribe | mechanics §3 Delegate Binding Contract | SD1 (BeginPlay bind + EndPlay unbind) | AddUObject + stored handles |
| **TR-PM-011** — Rule 5 input gate | mechanics §3 Rule 5 | SD4 (per-tick RSM property reads) | RUNNING + !paused + !resume_grace |
| **TR-PM-012** — F-4 buffer + edge-check | mechanics §4 F-4 | SD1 (component-owned buffer state) | Single-slot pre-validation |
| **TR-PM-013** — commitment-tell 80% white flash + 2-frame hold + 50ms decay | presentation §Commitment-Tell | SD5 (SETTLED→SLIPPING trigger site) | Cadence gated in presentation |
| **TR-PM-014** — commitment-tell cadence cap | presentation §Commitment-Tell (R11a-11) | SD5 (trigger site owns cadence gate check before render call) | ≤ 5 fires/sec |
| **TR-PM-015** — counters (slip_complete_count, edge_absorb_trigger_count, commitment_tell_fire_count) | mechanics §3 Public Interface + R10a-2.2 | SD1 (UPROPERTY int32 fields) | Counter Persistence AC-COUNTER-* |
| **TR-PM-016** — counter reset semantics | mechanics §3 Rules 10 + Counter Persistence | SD1 (HandleStateChanged COUNTDOWN case) | Reset only on COUNTDOWN |
| **TR-PM-017** — Rule 7 DEAD freeze | mechanics §3 Rule 7 + HandleStateChanged | SD5 (root committed; mesh frozen at fractional) | movement_state = SETTLED per R7-PM-PROPAGATION-REVIEW |
| **TR-PM-018** — Rules 8/9 COMPLETE/ABORTED snap | mechanics §3 Rules 8/9 + HandleStateChanged | SD5 (root + mesh snap to target) | ResetLeanOutputs + SetMeshRelativeRotationToZero |
| **TR-PM-019** — Rule 7 source-lane preservation | mechanics §3 Rule 7 + Source-Lane Semantic | SD5 (current_lane holds source until HandleStateChanged completes) | Pull-Wave R7 Rule 11 forward contract |
| **TR-PM-020** — 60-sample watchdog | platform §3 Runtime DT watchdog | SD4 (TickComponent reads F-PROLOGUE raw_dt; pushes to buffer) | 60-slot ring buffer |
| **TR-PM-021** — hysteresis-release 3.0s | platform §3 Runtime DT watchdog | SD4 (ContinuousCleanWindowTime accumulator) | Prevents flap |
| **TR-PM-022** — OnHardwarePerformanceBreach broadcast | platform §3 Public Interface | SD1 (delegate on component) + SD4 (Tick body broadcast site) | Wave Spawner + HUD subscribe |
| **TR-PM-023** — breach action + Wave Spawner grace | platform §3 Cross-System Interface Table + R11a-8 | SD1 (component surface exposes bool + delegate) | 3.0s grace window is Wave Spawner obligation |
| **TR-PM-024** — is_hw_performance_degraded property | platform §3 Public Interface | SD1 (UPROPERTY bool) | Wave Spawner + telemetry consumers |
| **TR-PM-025** — TickDTRollingBuffer sentinel init | platform §3 R11a-6 BeginPlay | SD1 (BeginPlay initialization) | 0.01667f × 60 pre-fill |
| **TR-PM-026** — Shipping-safe guards (SLIP_TWEEN + EMovementState default + curve nulls) | platform §Shipping-Safety Enforcement Policy | SD6 (curve nulls) + AC-SS-A/B/D | bCurveFallbackActive visible per row 4 |
| **TR-PM-027** — `static_assert(MIN_ESCAPE_SLIPS==2)` | platform §Shipping-Safety row + AC-SS-E | Migration Plan Step 3 (compile-unit local constant) | R11a-17 REFRAMED local test |
| **TR-PM-028** — EC15_F6_DECAY_COEFFICIENT | mechanics §4 F-6 R11a-4 | SD6 (EdgeAbsorbCurve owner) | F-6 proportional cap during F-5 |
| **TR-PM-029** — F-5/F-6 HEAD_LAG / ARM_LEAD staggering | mechanics §4 F-5/F-6 | SD6 (LeanCurve owner) | Progress-offset sampling |
| **TR-PM-030** — near-miss haptic `EHapticEvent::NearMiss` + `IsNearMissHapticEnabled` | presentation §Near-Miss Beat R11a-12 | SD1 (TriggerNearMissBeat() public) | Gated by ADR-0002 bridge + IGameSettings |
| **TR-PM-031** — slip cue duration = audio_cue_ratio × SLIP_TWEEN | presentation §4 F-AUDIO-CUE-DURATION | SD1 (SETTLED→SLIPPING trigger site fires cue) | Presentation renders |
| **TR-PM-032** — slip audio ducking -6dB + triple-overlap HARD-CUT | presentation §5 + R11a-15 | SD1 (dispatch site fires cue) | Audio system owns mix |
| **TR-PM-033** — final `±MAX_LEAN×1.2` clamp (F-5/F-6 co-write) | mechanics §4 F-5 / F-6 R10a-1.1 | SD5 (SetRelativeRotation call site applies clamp) | R10a-4 co-write contract |
| **TR-PM-034** — Authored Asset Contracts BeginPlay verify | mechanics §4 Authored Asset Contracts | SD6 (ValidateCurveAsset gate) | 3 curves; AC-SS-D fallback |
| **TR-PM-035** — Tick Ordering PM-after-RSM | platform §3 Tick Ordering | SD4 (RSM->ForceTickNow() first statement) | Supersedes platform §3 (a)/(b) |

## Implementation Guidelines

1. **First statement of `UPlayerLaneMovementComponent::TickComponent()` (after `check(IsInGameThread())`) MUST be `RSMSubsystem->ForceTickNow()`.** No exceptions. If PM's TickComponent runs without this call, the `RunStateMachine.remaining_time` read further down the body is stale by up to one frame — F-2's `effective_slip_tween` clamp check + F-6 timer advance both silently corrupt.

2. **PM must NOT subscribe to `RSM->OnStateChanged` or `RSM->OnPausedChanged` from inside `ForceTickNow()`'s execution.** ADR-0007 non-callback invariant preserved: PM's subscribers observe transitions on RSM's NEXT REGULAR tick — a documented 1-frame-latency semantic. PM's tick body reads direct accessors (`GetCurrentState()`, `IsPaused()`, `IsResumeGrace()`) for zero-latency gating.

3. **Delegate binding lifetime**: `BeginPlay` calls `AddUObject(this, &Self::HandleStateChanged)` and stores the returned `FDelegateHandle` in a member variable; `EndPlay` guards `if (IsValid(RSMSubsystem))` before calling `Remove(StateChangedHandle)`. NO `AddRaw`, NO lambda bindings. Same discipline for `OnPausedChanged`. Per mechanics §3 Delegate Binding Contract + ADR-0007 Implementation Guideline pattern.

4. **Curve validation runs at BeginPlay, not first tick, not lazy.** Mirrors ADR-0008 SD2 + forbidden pattern `DPC_curve_validation_at_first_tick_or_lazy`. On validation failure, log `Error`, set `bCurveFallbackActive = true`, and let the fallback path in F-3/F-5/F-6 engage — do NOT bail out of BeginPlay (component would be orphaned). AC-SS-D fallback flag reachable from debug overlay.

5. **SD5 collision-commit invariant — F-3 clamp applies to RELATIVE offset**: when `TweenProgress = 1.0`, mesh relative X MUST equal `0.0` exactly, so mesh world X equals `lane_world_x(target_lane)` exactly (pawn root already there). Do NOT clamp mesh world X — the mesh is in relative-space; the root is in absolute-space. F-3's mathematical form uses `source_lane - target_lane` as the interpolation delta (in track-space units); at `TweenProgress = 1.0`, the delta collapses to `0.0` naturally.

6. **`SetActorRotation` on the pawn root for lean is FORBIDDEN** — see registry forbidden pattern (added by this ADR). Rotate the mesh component's local transform via `MeshComponent->SetRelativeRotation(FRotator(lean_angle, 0, 0))`; the pawn root rotation stays at identity. Rotating the root breaks Rule 2 collision commitment: collision geometry follows root rotation, which would rotate the "committed lane X" out of the target lane's world position on the next tick.

7. **Non-dynamic multicast delegates only**: `DECLARE_MULTICAST_DELEGATE_TwoParams(FOnSlipMidpoint, EPlayerLane, EPlayerLane)` and `DECLARE_MULTICAST_DELEGATE_OneParam(FOnHardwarePerformanceBreach, bool)`. NOT `DECLARE_DYNAMIC_MULTICAST_DELEGATE_*` — parallel to ADR-0007 forbidden pattern `RunStateMachine_DECLARE_DYNAMIC_MULTICAST_for_state_or_paused`. Consumers that need Blueprint exposure author a parallel dynamic delegate rather than retrofitting.

8. **Component construction site**: `ASlipstormPlayerPawn` constructor creates PM via `CreateDefaultSubobject<UPlayerLaneMovementComponent>(TEXT("MovementComponent"))` and stores in `UPROPERTY(VisibleDefaultsOnly, Category="SLIPSTORM|Movement") TObjectPtr<UPlayerLaneMovementComponent> MovementComponent`. PM has NO independent constructor-time work — all initialization happens in BeginPlay.

9. **Blueprint subclass authoring** — `BP_SlipstormPlayerPawn` is authored as a Blueprint subclass of `ASlipstormPlayerPawn` and assigned in the Game Mode's `DefaultPawnClass`. The 3 UCurveFloat assets (`CV_SlipCurve`, `CV_LeanCurve`, `CV_EdgeAbsorbCurve` — placeholder names; first PM story confirms) are assigned to the PM component's UPROPERTY fields inside the Blueprint. Curve asset files live in `Content/Player/Curves/`.

10. **Log category**: `DECLARE_LOG_CATEGORY_EXTERN(LogPlayerMovement, Log, All)` in PM header; `DEFINE_LOG_CATEGORY(LogPlayerMovement)` in PM cpp. Used by SD6 curve validation errors, F-2 clamp logs (rate-limited per platform §Shipping-Safety row 5), Rule-5 gate discard debug logs (Verbose only), watchdog breach logs.

## Registry Updates

This ADR adds 2 new forbidden patterns to `docs/registry/architecture.yaml` (v7 → v8):

**Pattern 1** — `PlayerMovement_SetActorRotation_for_lean`:
- **status**: active
- **description**: "UPlayerLaneMovementComponent MUST NOT call `AActor::SetActorRotation` (or any variant that rotates the pawn root) to apply body/head/arm lean. Lean is applied to the MESH component's local transform via `MeshComponent->SetRelativeRotation(...)`. Root remains at identity rotation. See ADR-0009 Structural Decision 5 + Implementation Guideline 6."
- **why**: "Rotating the pawn root rotates the collision geometry with it — the committed lane X (which is a world-space X coordinate set via `SetActorLocation`) would rotate out of the target lane's world position on the next tick, breaking mechanics Rule 2 collision commitment. The collision geometry MUST stay aligned to the track-X axis. Only the visual mesh rotates for lean."
- **adr**: docs/architecture/adr-0009-player-movement-hosting.md
- **added**: 2026-07-03

**Pattern 2** — `PlayerMovement_TickComponent_without_prior_ForceTickNow`:
- **status**: active
- **description**: "UPlayerLaneMovementComponent::TickComponent body MUST call `RSMSubsystem->ForceTickNow()` as its first statement after `check(IsInGameThread())`. Do not delay the call, do not gate it on `movement_state`, do not skip it under any RSM state. Mirrors ADR-0008 pattern for UDPCController::Tick. See ADR-0009 Structural Decision 4 + Implementation Guideline 1."
- **why**: "Without the ForceTickNow prologue, PM reads stale `RSM.current_state` / `RSM.remaining_time` — up to one frame stale under unfavorable engine tick scheduling. F-2's `effective_slip_tween` clamp check and F-6 timer advance both silently corrupt against stale RSM state. RSM's `bHasTickedThisFrame` guard (INT-005) makes ForceTickNow idempotent within-frame — the call is safe even if DPC or engine already ticked RSM this frame."
- **adr**: docs/architecture/adr-0009-player-movement-hosting.md
- **added**: 2026-07-03

Version bump: `version: 7` → `version: 8`; `last_updated:` field prepended with a 2026-07-03 ADR-0009 explanatory comment.

---

*ADR-0009 body ends here. Post-ADR hygiene queued: platform §3 (a)/(b) tick-ordering SUPERSEDED edit; systems-index PM row status update to reference ADR-0009; architecture.yaml v7→v8 registry updates.*

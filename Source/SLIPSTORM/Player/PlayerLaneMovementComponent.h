// Copyright (c) Game Studio. All rights reserved.
// SLIPSTORM — 60-second voxel endless runner, mobile, Unreal Engine 5.7
//
// PlayerLaneMovementComponent.h
//
// UPlayerLaneMovementComponent : UActorComponent — the player movement
// subsystem for lane-slip gameplay.  Hosted on ASlipstormPlayerPawn.
//
// Governing ADRs:
//   ADR-0009 (Player Movement Component Hosting, Tween Implementation)
//   ADR-0007 (RSM Hosting — ForceTickNow, OnStateChanged, OnPausedChanged)
// GDD references:
//   design/gdd/player-movement-mechanics.md §3 Public Interface
//   design/gdd/player-movement-platform.md §3/7/8
//   design/gdd/player-movement-mechanics.md §Movement State Enum
// Story: production/epics/player-movement/story-001-pawn-component-skeleton.md
// Story: production/epics/player-movement/story-003-state-machine-tick-body.md
// Story: production/epics/player-movement/story-005-input-buffer.md

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Curves/CurveFloat.h"
#include "Player/EPlayerLane.h"
#include "Player/ERunSlipState.h"
#include "Player/ESlipDirection.h"
#include "RunStateMachine/ERunState.h"
#include "RunStateMachine/ERunOutcome.h"
#include "PlayerLaneMovementComponent.generated.h"

// Forward declarations
class URunStateMachineSubsystem;
class UStaticMeshComponent;

// ---------------------------------------------------------------------------
// Log category
// ---------------------------------------------------------------------------

DECLARE_LOG_CATEGORY_EXTERN(LogPlayerMovement, Log, All);

// ---------------------------------------------------------------------------
// AC-SS-E — PM Hardware Contract compile-time gate.
// Fires if MIN_ESCAPE_SLIPS is changed without updating the F-BARRAGE math.
// Do not move, comment out, or wrap in #if.
// Reference: ADR-0009 Validation Criteria 5, story-001 §AC-SS-E.
// ---------------------------------------------------------------------------
namespace SLIPSTORM_PM
{
    constexpr int32 MIN_ESCAPE_SLIPS = 2;
    static_assert(MIN_ESCAPE_SLIPS == 2,
        "PM Hardware Contract math assumes MIN_ESCAPE_SLIPS=2; "
        "see F-BARRAGE-SURVIVABILITY-INVARIANT-CORNERS");

    // -----------------------------------------------------------------------
    // Story 002 — lane geometry and tween constants.
    // All distance values are in Unreal metric units (1 UU = 1 cm) unless
    // the symbol name includes the _M suffix (metres for readability).
    // -----------------------------------------------------------------------

    /** Lane width in metres (design reference unit). */
    constexpr float LANE_WIDTH_M  = 1.0f;

    /** Lane width in centimetres (Unreal world units).  LANE_WIDTH_M * 100. */
    constexpr float LANE_WIDTH_CM = LANE_WIDTH_M * 100.0f;

    /** Maximum allowed DeltaTime for a single tween-advance tick (seconds).
     *  Frames longer than this are clamped to prevent tween overshoot.
     *  Equals 3 × target frame time (3 × 0.01667 s ≈ 0.05 s).
     *  Formula F-1; ADR-0009 §Tween Clamping. */
    constexpr float MAX_SLIP_DT_S = 0.05f;
}

// ---------------------------------------------------------------------------
// Non-dynamic multicast delegates (ADR-0009 IG-7)
// Non-dynamic — consumers that need Blueprint exposure author a parallel
// dynamic delegate rather than retrofitting.
// ---------------------------------------------------------------------------

/** Fired at the visual midpoint of a lane tween.
 *  Param 1: FromLane (lane departed); Param 2: ToLane (lane arriving).
 *  Broadcast implementation lands in Story 003. */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnSlipMidpoint, EPlayerLane, EPlayerLane);

/** Fired when the hardware-performance-degraded state transitions.
 *  Param: bIsDegraded (true = breach entered; false = breach cleared).
 *  Broadcast implementation lands in Story 013. */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnHardwarePerformanceBreach, bool);

// ---------------------------------------------------------------------------
// UPlayerLaneMovementComponent
// ---------------------------------------------------------------------------

/**
 * UPlayerLaneMovementComponent
 *
 * Lane-slip movement component for ASlipstormPlayerPawn.
 * Tick is disabled at construction and enabled in BeginPlay after RSM
 * resolution and curve validation.
 *
 * Public fields are declared read-surface only in Story 001; write paths
 * land in Stories 002–013 per the story decomposition plan.
 *
 * Control manifest rules (architecture.yaml v8):
 *   - Tick body FIRST statement (after IsInGameThread check): RSMSubsystem->ForceTickNow()
 *     [Forbidden pattern: PlayerMovement_TickComponent_without_prior_ForceTickNow]
 *     [Implemented in Story 003]
 *   - Lean via SetRelativeRotation on mesh only — SetActorRotation is FORBIDDEN
 *     [Forbidden pattern: PlayerMovement_SetActorRotation_for_lean]
 *   - AddUObject bindings only — no AddRaw, no lambda bindings (ADR-0009 IG-3)
 */
UCLASS(ClassGroup=(SLIPSTORM), meta=(BlueprintSpawnableComponent))
class SLIPSTORM_API UPlayerLaneMovementComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UPlayerLaneMovementComponent();

    // -----------------------------------------------------------------------
    // Public read surface — §3 Public Interface
    // Writes land in later stories (002 lane math, 003 tick body, etc.).
    // -----------------------------------------------------------------------

    /** Lane the player currently occupies (committed root position). */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="SLIPSTORM|Movement")
    EPlayerLane current_lane = EPlayerLane::Center;

    /** Lane the player is tweening toward (same as current_lane when SETTLED). */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="SLIPSTORM|Movement")
    EPlayerLane target_lane = EPlayerLane::Center;

    /** Current movement state: SETTLED or SLIPPING. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="SLIPSTORM|Movement")
    ERunSlipState movement_state = ERunSlipState::SETTLED;

    /** World-space X position of the mesh (interpolated between lane X values during tween).
     *  Formula F-3 writes this; Story 004. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="SLIPSTORM|Movement")
    float lateral_world_position = 0.0f;

    /** Normalized tween progress [0, 1].  Formula F-2; Story 002. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="SLIPSTORM|Movement")
    float tween_progress = 0.0f;

    /** Composite lean angle (degrees) applied to the mesh root.  Formula F-5; Story 006. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="SLIPSTORM|Movement")
    float lean_angle = 0.0f;

    /** Head lean angle (degrees) — subset of F-5 lean; Story 006. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="SLIPSTORM|Movement")
    float head_lean_angle = 0.0f;

    /** Arm lean angle (degrees) — subset of F-5 lean; Story 006. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="SLIPSTORM|Movement")
    float arm_lean_angle = 0.0f;

    /** True when the single-slot input buffer holds an unprocessed direction. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="SLIPSTORM|Movement")
    bool has_queued_input = false;

    /** Direction held in the single-slot input buffer (valid only when has_queued_input). */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="SLIPSTORM|Movement")
    ESlipDirection queued_input_direction = ESlipDirection::Left;

    /** Number of completed slip tweens this run (resets on COUNTDOWN). */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="SLIPSTORM|Movement|Counters")
    int32 slip_complete_count = 0;

    /** Number of edge-absorb events triggered this run. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="SLIPSTORM|Movement|Counters")
    int32 edge_absorb_trigger_count = 0;

    /** Number of commitment-tell material flashes fired this run. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="SLIPSTORM|Movement|Counters")
    int32 commitment_tell_fire_count = 0;

    /** True while the F-2 persistent clamp is active (SLIP_TWEEN overshoot guard). */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="SLIPSTORM|Movement")
    bool bSlipTweenClampActive = false;

    /** Duration of a single lane-slip tween (seconds).  Drives F-2 normalisation.
     *  Design default 0.15s per mechanics §7 Tuning Knobs.
     *  Safe range: [0.10, 0.15] — persistent clamp enforced by GetEffectiveSlipTween.
     *  Values outside the safe range trigger AC-21 / AC-SS-A rate-limited Error log.
     *  Tunable in the Blueprint default or per-Data Asset.  Story 002.
     *  ADR-0009 Tuning Knobs §SLIP_TWEEN_DURATION_S; mechanics §7. */
    UPROPERTY(EditDefaultsOnly, Category="SLIPSTORM|Movement|Tuning")
    float SLIP_TWEEN_DURATION_S = 0.15f;

    /** True when any curve asset failed validation at BeginPlay (AC-SS-D).
     *  Readable by HUD telemetry / debug overlay. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="SLIPSTORM|Movement|Debug")
    bool bCurveFallbackActive = false;

    /** True while the watchdog reports a hardware performance breach (Story 013). */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="SLIPSTORM|Movement|Platform")
    bool is_hw_performance_degraded = false;

    // -----------------------------------------------------------------------
    // Curve assets (hard references, validated at BeginPlay — SD6)
    // -----------------------------------------------------------------------

    /** Lateral slip easing curve.  Time axis [0,1] → value axis [0,1]. */
    UPROPERTY(EditDefaultsOnly, Category="SLIPSTORM|Movement|Curves")
    TObjectPtr<UCurveFloat> SlipCurve;

    /** Lean angle curve.  Time axis [0,1] → value axis lean-degrees. */
    UPROPERTY(EditDefaultsOnly, Category="SLIPSTORM|Movement|Curves")
    TObjectPtr<UCurveFloat> LeanCurve;

    /** Edge-absorb overshoot curve.  Time axis [0,1] → value axis [0,1]. */
    UPROPERTY(EditDefaultsOnly, Category="SLIPSTORM|Movement|Curves")
    TObjectPtr<UCurveFloat> EdgeAbsorbCurve;

    // -----------------------------------------------------------------------
    // Public delegates (non-dynamic, ADR-0009 IG-7)
    // -----------------------------------------------------------------------

    /** Fired at the visual midpoint of a slip tween.  Story 003. */
    FOnSlipMidpoint OnSlipMidpoint;

    /** Fired on hardware-performance-degraded state change.  Story 013. */
    FOnHardwarePerformanceBreach OnHardwarePerformanceBreach;

    // -----------------------------------------------------------------------
    // UActorComponent overrides
    // -----------------------------------------------------------------------

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime,
                               ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;

    // -----------------------------------------------------------------------
    // RSM delegate handlers
    // Signatures match ADR-0007 Key Interfaces verbatim (lines 193–208).
    // Bodies are EMPTY here — Stories 008 and 009 fill them.
    // -----------------------------------------------------------------------

    /** Called by RSM's OnStateChanged.  Story 008 implements the body. */
    void HandleStateChanged(ERunState PreviousState,
                            ERunState NewState,
                            ERunOutcome Outcome,
                            double Timestamp);

    /** Called by RSM's OnPausedChanged.  Story 009 implements the body. */
    void HandlePausedChanged(bool bIsPaused, double Timestamp);

    // -----------------------------------------------------------------------
    // Story 003 — public-facing input entry point
    // -----------------------------------------------------------------------

    /** Primary input dispatch: processes a slip direction from the input system.
     *  In SETTLED state: validates via F-4, then transitions to SLIPPING or
     *  fires edge-absorb hook (Story 007).
     *  In SLIPPING state: buffered-input path (Story 005 implements; this story
     *  stubs with an early return).
     *  Rule 5 gated: discards if RSM state != RUNNING or paused or resume_grace.
     *  ADR-0009 SD5; GDD mechanics §3 Rule 2/4/5. */
    void HandleSlipTransition(ESlipDirection Dir);

    // -----------------------------------------------------------------------
    // Test access — Story 001a
    // -----------------------------------------------------------------------

#if WITH_DEV_AUTOMATION_TESTS
    /** Grants FPMLifecycleAndSeamTest direct read access to private fields.
     *  Required for TC1–TC4: bCurveFallbackActive via friend (public already),
     *  but TickDTRollingBuffer, TickDTRingIndex, ContinuousCleanWindowTime,
     *  bHardwarePerformanceBreachActive, RSMSubsystem, StateChangedHandle,
     *  PausedChangedHandle are all private.
     *  Story 001a Implementation Notes — preferred friend declaration at h:221. */
    friend class FPMLifecycleAndSeamTest;

    /** Grants FPMLaneAndTweenTest direct access to private helpers and fields.
     *  Required for Story 002 unit tests: AdvanceTweenProgress, LaneWorldX,
     *  ComputeTickDT, GetEffectiveSlipTween, SlipTweenClampLogTickCounter.
     *  Story 002 Implementation Notes. */
    friend class FPMLaneAndTweenTest;

    /** Grants FPMStateMachineTest direct access to private helpers and fields.
     *  Required for Story 003 integration tests: IsSlipValidFromLane, LaneWorldX,
     *  CompleteTween, HandleSlipTransition, RSMSubsystem, movement_state,
     *  current_lane, target_lane, tween_progress, slip_complete_count.
     *  Story 003 JC-1 spy counter on RSM (TestOnly_ForceTickNowCallCount). */
    friend class FPMStateMachineTest;

    /** Grants FPMLateralInterpolationTest direct access to F3RelativeOffset, LaneWorldX,
     *  and private state (current_lane, target_lane, SlipCurve, bCurveFallbackActive).
     *  Required for Story 004 unit tests: AC-03 identity/authored curves, TP=1.0
     *  zero-relative invariant, fallback path, lateral_world_position accessor. */
    friend class FPMLateralInterpolationTest;

    /** Grants FPMLateralInterpolationCompositionTest direct access to F3RelativeOffset,
     *  CachedMeshComponent, and the full private state surface needed for
     *  composition invariant and lateral_world_position_settled integration tests.
     *  Story 004 composition tests. */
    friend class FPMLateralInterpolationCompositionTest;

    /** Grants FPMInputBufferTest direct access to private fields and helpers
     *  required for Story 005 unit tests: has_queued_input, queued_input_direction,
     *  movement_state, current_lane, target_lane, slip_complete_count, RSMSubsystem,
     *  CompleteTween, HandleSlipTransition, DiscardBuffer,
     *  BufferDropAudioSting_TestOnlyCallCount.
     *  Story 005 Implementation Notes. */
    friend class FPMInputBufferTest;

    /** Grants FPMLeanTest access to ComputeLean, DirectionSign, and private state
     *  (LeanCurve, bCurveFallbackActive) for AC-26/27/30 + staggered-offset unit tests.
     *  Story 006. */
    friend class FPMLeanTest;
#endif // WITH_DEV_AUTOMATION_TESTS

private:
    // -----------------------------------------------------------------------
    // RSM reference + delegate handles (ADR-0009 IG-3)
    // -----------------------------------------------------------------------

    /** Cached RSM subsystem — resolved once at BeginPlay. */
    UPROPERTY()
    TObjectPtr<URunStateMachineSubsystem> RSMSubsystem;

    /** Cached pointer to the owner pawn's MeshComponent, resolved at BeginPlay.
     *  Written by F-3 via SetRelativeLocation each SLIPPING tick.
     *  Null-safe: F-3 skips the mesh write if the cache did not resolve.
     *  Story 004. */
    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> CachedMeshComponent;

    /** Handle from AddUObject binding — used to unbind in EndPlay. */
    FDelegateHandle StateChangedHandle;

    /** Handle from AddUObject binding — used to unbind in EndPlay. */
    FDelegateHandle PausedChangedHandle;

    // -----------------------------------------------------------------------
    // Watchdog fields (platform R11a-6; TR-PM-025)
    // Buffer advance and breach evaluation land in Story 013.
    // -----------------------------------------------------------------------

    /** Rolling 60-slot DeltaTime ring buffer for watchdog.  Sentinel: 0.01667f. */
    float TickDTRollingBuffer[60];

    /** Write-head for the ring buffer. */
    int32 TickDTRingIndex = 0;

    /** Accumulated clean-window time for the sustained-60fps gate. */
    float ContinuousCleanWindowTime = 0.0f;

    /** True while the watchdog has declared a hardware performance breach. */
    bool bHardwarePerformanceBreachActive = false;

#if WITH_DEV_AUTOMATION_TESTS
    /** Test-only: increments once per HandleStateChanged invocation.
     *  Used by TC4 post-EndPlay broadcast verification. Story 001a. */
    mutable int32 HandleStateChanged_TestOnlyCallCount = 0;

    /** Test-only: increments once per PlayBufferDropAudioSting invocation.
     *  Read via FPMInputBufferTest friend to verify synchronous dispatch (AC-25).
     *  Story 005. Non-const write from PlayBufferDropAudioSting — no mutable needed. */
    int32 BufferDropAudioSting_TestOnlyCallCount = 0;
#endif // WITH_DEV_AUTOMATION_TESTS

    // -----------------------------------------------------------------------
    // Private helpers
    // -----------------------------------------------------------------------

    /** Validates a single curve asset at BeginPlay.
     *  On failure: logs Error and sets bCurveFallbackActive = true.
     *  Checks: non-null, >= 2 keys, key range spans [0, 1]. */
    void ValidateCurveAsset(UCurveFloat* Curve, const TCHAR* CurveName);

    // -----------------------------------------------------------------------
    // Story 002 — lane math + tween helpers
    // -----------------------------------------------------------------------

    /** Returns the world-space X coordinate (cm) for a given lane.
     *  Formula F-1: X = (static_cast<int32>(Lane) - 2) * LANE_WIDTH_CM.
     *  Lane ordinals: FarLeft=0, Left=1, Center=2, Right=3, FarRight=4.
     *  Center lane maps to X = 0.  Pure math — no world state read. */
    static float LaneWorldX(EPlayerLane Lane);

    /** F-PROLOGUE — reads the raw frame DT once and computes the effective DT.
     *  OutRawDT = FApp::GetDeltaTime() (unclamped; watchdog input; time-dilation ignored).
     *  OutEffectiveDT = FMath::Clamp(OutRawDT, 0.0f, MAX_SLIP_DT_S) (mechanics input).
     *  Single-source DT invariant — F-2 (AdvanceTweenProgress) and F-6 (Story 007)
     *  both consume OutEffectiveDT; watchdog (Story 013) consumes OutRawDT.
     *  Do NOT set bSlipTweenClampActive here — that flag reflects the SLIP_TWEEN
     *  knob-range clamp, NOT the DT overrun clamp. */
    void ComputeTickDT(float& OutRawDT, float& OutEffectiveDT) const;

    /** Returns SLIP_TWEEN_DURATION_S persistently clamped to the safe range [0.10, 0.15]
     *  (AC-21 / AC-SS-A shipping-safety knob-range guard).
     *  Sets bSlipTweenClampActive = true when the raw knob is out of range this call;
     *  clears it otherwise (per-call state, not one-shot).
     *  Rate-limited Error log fires when SlipTweenClampLogTickCounter % 600 == 0
     *  on a violating call (i.e., tick 1, tick 601, tick 1201, ... of a sustained violation).
     *  Boundary values 0.10f and 0.15f exactly are NOT violations (strict `<` / `>`). */
    float GetEffectiveSlipTween();

    /** F-2 — advances tween_progress by (EffectiveDT / GetEffectiveSlipTween()).
     *  Pure accumulator — does NOT test for completion, does NOT commit lanes,
     *  does NOT modify movement_state.  Story 003's CompleteTween() owns those
     *  side effects when tween_progress crosses 1.0f. */
    void AdvanceTweenProgress(float EffectiveDT);

    // -----------------------------------------------------------------------
    // Story 002 — rate-limit counter for GetEffectiveSlipTween Error log
    // -----------------------------------------------------------------------

    /** Counts consecutive violating calls to GetEffectiveSlipTween
     *  (SLIP_TWEEN_DURATION_S strictly < 0.10f OR strictly > 0.15f).
     *  Log fires when counter % 600 == 0 on a violating call (tick 1 of a sustained
     *  violation, then tick 601, tick 1201, ...).  Reset to 0 on any non-violating call
     *  so a resumed violation logs immediately.  Default-initialised to 0. */
    int32 SlipTweenClampLogTickCounter = 0;

    // -----------------------------------------------------------------------
    // Story 003 — state machine helpers
    // -----------------------------------------------------------------------

    /** F-4 pure utility: edge-check pre-validation.
     *  Returns true and sets OutTargetLane to the projected destination lane if
     *  the slip from FromLane in Dir is on-track (ordinal stays in [0, 4]).
     *  Returns false and leaves OutTargetLane unchanged for off-track (edge) inputs.
     *  Projection: ordinal = static_cast<int32>(FromLane) + (Dir==Right ? +1 : -1).
     *  Used by HandleSlipTransition (this story) and buffer flush (Story 005).
     *  Pure math — no world state read. const is correct: OutTargetLane is an output
     *  ref, not a mutation of *this. */
    bool IsSlipValidFromLane(EPlayerLane FromLane,
                             ESlipDirection Dir,
                             EPlayerLane& OutTargetLane) const;

    /** Finalises a completed tween: clamps tween_progress to 1.0f, reassigns
     *  current_lane to target_lane (source-lane semantic write), resets movement_state
     *  to SETTLED, increments slip_complete_count, and invokes FlushBufferedInput().
     *  THIS is the ONLY place current_lane = target_lane is written — Rule 4 binding
     *  (R7-PM-PROPAGATION-REVIEW). */
    void CompleteTween();

    /** Story 005 stub — buffer flush invoked at end of CompleteTween (same tick,
     *  no idle frame).  Implementation lands in Story 005.
     *  TODO(Story 005): implement single-slot buffer flush: if has_queued_input,
     *  pop direction, clear has_queued_input, call HandleSlipTransition(dir). */
    void FlushBufferedInput();

    /** Story 007 stub — edge-absorb hook, called from HandleSlipTransition when
     *  F-4 validation returns false.  Implementation (F-6 timer, edge_absorb_trigger_count++,
     *  visual feedback) lands in Story 007.
     *  TODO(Story 007): implement F-6 edge-absorb: advance edge overshoot timer,
     *  increment edge_absorb_trigger_count, trigger visual feedback using FromLane
     *  and Dir for direction-specific mesh deformation (e.g., wall-bounce sign). */
    void TriggerEdgeAbsorb(EPlayerLane FromLane, ESlipDirection Dir);

    /** Rule 11 buffer discard helper — sets has_queued_input = false.
     *  Called by Story 008's HandleStateChanged terminal-state handlers (DEAD /
     *  COMPLETE / ABORTED / COUNTDOWN) when a non-RUNNING state is entered.
     *  Story 009's HandlePausedChanged does NOT call this — pause preserves the
     *  buffer per AC-13.
     *  Published in Story 005; invocation by Story 008. */
    void DiscardBuffer();

    /** Presentation dispatch stub for buffer-drop audio feedback (Rule 3 drop path).
     *  Fires synchronously within the same HandleSlipTransition event call (AC-25).
     *  The audio system owns the actual cue; this method fires the dispatch.
     *  Current stub body: UE_LOG at Verbose.  Audio routing is downstream.
     *  Story 005. */
    void PlayBufferDropAudioSting();

    // -----------------------------------------------------------------------
    // Story 004 — F-3 lateral interpolation
    // -----------------------------------------------------------------------

    /** F-3 relative-offset computation.
     *  Returns the relative-space X offset for the mesh during SLIPPING.
     *  Formula: delta = LaneWorldX(current_lane) - LaneWorldX(target_lane);
     *           curve_t = FMath::Clamp(SlipCurve->GetFloatValue(TweenProgress), 0, 1)
     *                     (or FMath::Clamp(TweenProgress, 0, 1) in fallback path);
     *           return FMath::Lerp(delta, 0.0f, curve_t).
     *  At TweenProgress=1.0: returns 0.0 exactly (ADR-0009 IG-5).
     *  Pure math — reads current_lane, target_lane, SlipCurve, bCurveFallbackActive; writes nothing.
     *  GDD: design/gdd/player-movement-mechanics.md §4 F-3. Story 004. */
    float F3RelativeOffset(float TweenProgress) const;

    /** Story 010 stub — commitment-tell hook, called from HandleSlipTransition on
     *  SETTLED→SLIPPING transition.  Implementation (commitment_tell_fire_count++,
     *  material flash, cadence cap) lands in Story 010.
     *  TODO(Story 010): use TargetLane to select mesh face for the commitment flash:
     *  +X face for ESlipDirection::Right slip (TargetLane > current_lane ordinal),
     *  -X face for ESlipDirection::Left slip (TargetLane < current_lane ordinal).
     *  Increment commitment_tell_fire_count and enforce cadence cap. */
    void TriggerCommitmentTell(EPlayerLane TargetLane);

    // -----------------------------------------------------------------------
    // Story 006 — F-5 body/head/arm lean (LeanCurve + staggered offsets)
    // -----------------------------------------------------------------------

    /** F-5 lean tuning knobs (mechanics §7).
     *  MAX_LEAN_ANGLE_DEG: peak lean magnitude before ±1.2× clamp.  Safe range [8°, 12°].
     *  HEAD_LAG_PROGRESS:  head samples LeanCurve at (TP - lag).  Safe range [0.08, 0.12].
     *  ARM_LEAD_PROGRESS:  arm samples LeanCurve at (TP + lead). Safe range [0.03, 0.08].
     *  ADR-0009 SD5 + SD6; TR-PM-008; GDD design/gdd/player-movement-mechanics.md §7. */
    static constexpr float MAX_LEAN_ANGLE_DEG = 10.0f;
    static constexpr float HEAD_LAG_PROGRESS  = 0.10f;
    static constexpr float ARM_LEAD_PROGRESS  = 0.05f;

    /** F-5 body/head/arm lean computation.
     *  Reads LeanCurve at three staggered progress offsets: TP (body), TP-HEAD_LAG (head),
     *  TP+ARM_LEAD (arm). Each component multiplied by MAX_LEAN_ANGLE_DEG * DirectionSign,
     *  then clamped to ±(MAX_LEAN_ANGLE_DEG * 1.2).
     *  Fallback: LeanCurve null OR bCurveFallbackActive == true → all three outputs = 0.
     *  Pure math — reads LeanCurve, bCurveFallbackActive; writes nothing to *this*.
     *  Outputs via reference parameters. ADR-0009 SD5 + SD6. TR-PM-008, TR-PM-029, TR-PM-033.
     *  GDD: design/gdd/player-movement-mechanics.md §4 F-5. Story 006. */
    void ComputeLean(float TweenProgress,
                     EPlayerLane FromLane,
                     EPlayerLane ToLane,
                     float& OutBodyLean,
                     float& OutHeadLean,
                     float& OutArmLean) const;

    /** Direction sign for lean — +1 if slipping right (ToLane index > FromLane index),
     *  else -1. Pure math on lane ordinals. Story 006. */
    static float DirectionSign(EPlayerLane FromLane, EPlayerLane ToLane);
};

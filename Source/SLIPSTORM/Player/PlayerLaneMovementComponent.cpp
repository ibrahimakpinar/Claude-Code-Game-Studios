// Copyright (c) Game Studio. All rights reserved.
// SLIPSTORM — 60-second voxel endless runner, mobile, Unreal Engine 5.7
//
// PlayerLaneMovementComponent.cpp
//
// Story 001 scope: lifecycle skeleton (constructor, BeginPlay, EndPlay) +
// curve validation + RSM delegate bind/unbind + watchdog sentinel init.
// Story 002 scope: LaneWorldX (F-1), ComputeTickDT (F-PROLOGUE),
//   GetEffectiveSlipTween (AC-21/AC-SS-A), AdvanceTweenProgress (F-2).
// Story 003 scope: TickComponent body (RSM ForceTickNow prologue + Rule 5
//   gating + F-2 advance + AC-24 midpoint broadcast + CompleteTween),
//   HandleSlipTransition (F-4 validation + SETTLED→SLIPPING transition),
//   IsSlipValidFromLane (F-4), CompleteTween, and stubs for
//   FlushBufferedInput (Story 005), TriggerEdgeAbsorb (Story 007),
//   TriggerCommitmentTell (Story 010).
// Story 005 scope: HandleSlipTransition SLIPPING branch (Rule 3 buffer-drop +
//   F-4 pre-validation vs target_lane + edge-absorb hook + buffer set),
//   FlushBufferedInput implementation, DiscardBuffer, PlayBufferDropAudioSting.
//   IHapticDispatch seam interface declaration (ADR-0002 INT-002-amended).
// Story 006 scope: F-5 body/head/arm lean — ComputeLean, DirectionSign, tick-site
//   SetRelativeRotation write, SETTLED lean-zero branch. TR-PM-008/029/033.
// RSM handler bodies by Stories 008–009.
// Watchdog buffer advance by Story 013.

#include "Player/PlayerLaneMovementComponent.h"
#include "Player/SlipstormPlayerPawn.h"       // ASlipstormPlayerPawn::MeshComponent — F-3 cache (Story 004)
#include "RunStateMachine/RunStateMachineSubsystem.h"
#include "Components/StaticMeshComponent.h"   // UStaticMeshComponent::SetRelativeLocation — F-3 (Story 004)
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Misc/App.h"  // FApp::GetDeltaTime() — F-PROLOGUE (Story 002)
#include "Seam/IHapticDispatch.h"             // ADR-0002 INT-002-amended haptic bridge — Story 005

DEFINE_LOG_CATEGORY(LogPlayerMovement);

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

UPlayerLaneMovementComponent::UPlayerLaneMovementComponent()
{
    // Tick is OFF at construction; BeginPlay enables it after RSM resolution
    // and curve validation (ADR-0009 SD4 + SD6).
    PrimaryComponentTick.bCanEverTick = false;
}

// ---------------------------------------------------------------------------
// BeginPlay — ADR-0009 SD6 lifecycle order:
//   1. Resolve RSMSubsystem (fail-safe: log + skip bind if null)
//   2. Validate curves (fail-safe: log Error + set bCurveFallbackActive; never bail)
//   3. Bind RSM delegates via AddUObject; store handles
//   4. Init watchdog sentinel (R11a-6 + TR-PM-025)
//   5. Enable ticking
// ---------------------------------------------------------------------------

void UPlayerLaneMovementComponent::BeginPlay()
{
    Super::BeginPlay();

    // --- 1. Resolve RSM subsystem ---
    // RSMSubsystem is consumed every tick; cache it now rather than resolving
    // per-frame.  Fail-safe: if null (e.g. headless test environment without a
    // game instance), log Error and skip delegate binding.  The component still
    // ticks; RSM gate reads fall back to SETTLED per RunStateMachineSubsystem stubs.
    if (UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
    {
        RSMSubsystem = GI->GetSubsystem<URunStateMachineSubsystem>();
    }

    if (!IsValid(RSMSubsystem))
    {
        UE_LOG(LogPlayerMovement, Error,
            TEXT("PlayerLaneMovementComponent::BeginPlay — RSMSubsystem not found. "
                 "Delegate binding skipped. RSM gate reads will return SETTLED defaults."));
    }

    // --- 1b. Resolve CachedMeshComponent — F-3 tick site (Story 004) ---
    // Resolved once at BeginPlay to avoid per-frame Cast<> overhead.
    // Null-safe: TickComponent F-3 site skips the mesh write when null.
    // Two-stage null-check: outer verifies owner is ASlipstormPlayerPawn;
    // inner verifies MeshComponent was constructed on the pawn.
    if (ASlipstormPlayerPawn* OwnerPawn = Cast<ASlipstormPlayerPawn>(GetOwner()))
    {
        CachedMeshComponent = OwnerPawn->MeshComponent;
        if (!CachedMeshComponent)
        {
            UE_LOG(LogPlayerMovement, Warning,
                TEXT("PlayerLaneMovementComponent::BeginPlay — MeshComponent is null on owning pawn. "
                     "F-3 lateral interpolation (Story 004) will skip mesh writes."));
        }
    }
    else
    {
        UE_LOG(LogPlayerMovement, Warning,
            TEXT("PlayerLaneMovementComponent::BeginPlay — GetOwner() is not ASlipstormPlayerPawn. "
                 "CachedMeshComponent unresolved; F-3 lateral interpolation will skip mesh writes."));
    }

    // --- 2. Validate curve assets ---
    // On any failure: log Error per curve, set bCurveFallbackActive.
    // Do NOT bail out of BeginPlay — the component must continue to a functional
    // (if degraded) state.  Fallback path (linear F-3, zero-lean F-5, immediate
    // F-6 return) exercised in Stories 004 / 006 / 007.
    ValidateCurveAsset(SlipCurve,        TEXT("SlipCurve"));
    ValidateCurveAsset(LeanCurve,        TEXT("LeanCurve"));
    ValidateCurveAsset(EdgeAbsorbCurve,  TEXT("EdgeAbsorbCurve"));

    // --- 3. Bind RSM delegates (ADR-0009 IG-3 / ADR-0007 delegate contract) ---
    // AddUObject — not AddRaw, not lambda.  Handles stored for removal in EndPlay.
    if (IsValid(RSMSubsystem))
    {
        StateChangedHandle  = RSMSubsystem->OnStateChanged.AddUObject(
            this, &UPlayerLaneMovementComponent::HandleStateChanged);

        PausedChangedHandle = RSMSubsystem->OnPausedChanged.AddUObject(
            this, &UPlayerLaneMovementComponent::HandlePausedChanged);
    }

    // --- 4. Watchdog sentinel init (R11a-6 + TR-PM-025) ---
    // Pre-fill all 60 slots with the target 60 fps frame time so the watchdog
    // window starts "clean" and does not spuriously fire on session start.
    // Buffer advance + breach evaluation land in Story 013.
    for (int32 i = 0; i < 60; ++i)
    {
        TickDTRollingBuffer[i] = 0.01667f;
    }
    TickDTRingIndex             = 0;
    ContinuousCleanWindowTime   = 0.0f;
    bHardwarePerformanceBreachActive = false;

    // --- 5. Enable ticking ---
    PrimaryComponentTick.bCanEverTick = true;
}

// ---------------------------------------------------------------------------
// EndPlay — guarded unbind (IsValid guard per ADR-0009 IG-3).
// ---------------------------------------------------------------------------

void UPlayerLaneMovementComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (IsValid(RSMSubsystem))
    {
        RSMSubsystem->OnStateChanged.Remove(StateChangedHandle);
        RSMSubsystem->OnPausedChanged.Remove(PausedChangedHandle);
    }

    PrimaryComponentTick.bCanEverTick = false;

    Super::EndPlay(EndPlayReason);
}

// ---------------------------------------------------------------------------
// TickComponent — Story 003.
//
// Tick ordering (ADR-0009 SD4 + IG-1):
//   1. check(IsInGameThread())
//   2. RSMSubsystem->ForceTickNow()  — MANDATORY FIRST STATEMENT after thread check.
//      [Forbidden pattern: PlayerMovement_TickComponent_without_prior_ForceTickNow]
//   3. ComputeTickDT (F-PROLOGUE)
//   4. Rule 5 gate (RUNNING + !paused + !grace)
//   5. F-2 advance (AdvanceTweenProgress) + AC-24 midpoint broadcast
//   6. CompleteTween if tween_progress >= 1.0f
//
// Non-callback invariant (ADR-0009 IG-2): ForceTickNow may synchronously
// broadcast OnStateChanged / OnPausedChanged. Handlers HandleStateChanged /
// HandlePausedChanged (Stories 008/009) MUST NOT re-enter this tick body.
//
// Source-lane semantic (R7-PM-PROPAGATION-REVIEW BINDING):
// current_lane holds SOURCE lane throughout SLIPPING. Only CompleteTween
// writes current_lane = target_lane.
// ---------------------------------------------------------------------------

void UPlayerLaneMovementComponent::TickComponent(
    float DeltaTime,
    ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    check(IsInGameThread());

    // MANDATORY first statement per ADR-0009 IG-1 + registry v8 forbidden
    // pattern PlayerMovement_TickComponent_without_prior_ForceTickNow.
    if (RSMSubsystem)
    {
        RSMSubsystem->ForceTickNow();
    }

    float raw_dt = 0.0f, effective_dt = 0.0f;
    ComputeTickDT(raw_dt, effective_dt);

    // Watchdog push (Story 013) — consumes raw_dt.
    // WatchdogTick(raw_dt);

    // Rule 5 gate: skip all mechanics if not in active RUNNING state.
    // Inputs received outside the gate are DISCARDED (not buffered).
    if (!RSMSubsystem
        || RSMSubsystem->GetCurrentState() != ERunState::RUNNING
        || RSMSubsystem->IsPaused()
        || RSMSubsystem->IsResumeGrace())
    {
        return; // mechanics skipped; TweenProgress NOT advanced; buffered input NOT flushed
    }

    // F-2 advance — only while SLIPPING.
    if (movement_state == ERunSlipState::SLIPPING)
    {
        const float prev_tp = tween_progress;
        AdvanceTweenProgress(effective_dt);

        // AC-24: midpoint broadcast — fires ONCE per tween when TP crosses 0.5.
        // Condition: previous tick TP < 0.5 AND current tick TP >= 0.5.
        // Does NOT fire on edge-absorb (Story 007), pause/resume, or completed tweens.
        //
        // ORDERING NOTE (Story 004): lateral_world_position reflects the PREVIOUS
        // tick's value during this broadcast — the F-3 write below (~line 218)
        // runs after the broadcast. Subscribers that need the current-tick mesh
        // position must read F3RelativeOffset directly or defer to a post-tick
        // delegate. Current consumers (Camera, Pull-Wave) read lateral_world_position
        // out-of-band per their own tick, not from this callback.
        if (prev_tp < 0.5f && tween_progress >= 0.5f)
        {
            OnSlipMidpoint.Broadcast(/*FromLane=*/current_lane, /*ToLane=*/target_lane);
        }

        // F-3: write mesh relative position + update public lateral_world_position.
        // IG-5: no absolute-position clamp — the relative form lands at 0 naturally at TP=1.0.
        if (CachedMeshComponent)
        {
            const float rel_x = F3RelativeOffset(tween_progress);
            CachedMeshComponent->SetRelativeLocation(FVector(rel_x, 0.0f, 0.0f));
            lateral_world_position = GetOwner()->GetActorLocation().X + rel_x;
        }
        // F-5: body/head/arm lean. Body drives mesh rotation via SetRelativeRotation;
        // head/arm exposed as public properties for skeletal-animation Blueprint consumption.
        // IG-6: SetActorRotation on pawn root is FORBIDDEN — rotate the mesh component only.
        // TR-PM-008, TR-PM-029, TR-PM-033. Story 006.
        if (IsValid(CachedMeshComponent))
        {
            float body_lean_deg = 0.0f;
            float head_lean_deg = 0.0f;
            float arm_lean_deg  = 0.0f;
            ComputeLean(tween_progress, current_lane, target_lane,
                        body_lean_deg, head_lean_deg, arm_lean_deg);

            // Story 007 F-6 co-write will add edge_absorb contribution here before the
            // final SetRelativeRotation call. For Story 006, apply body_lean directly.
            //
            // AXIS: FRotator(Pitch, Yaw, Roll). Lateral lean during a slip is ROLL
            // (rotation around forward X axis, tilting the character left/right).
            // NOT Pitch (nod forward/back). Standard UE convention: forward=+X,
            // right=+Y, up=+Z; SlipstormPlayerPawn applies no mesh-orientation
            // override so this convention is authoritative. Corrected during
            // Story 006 /code-review — spec text (story-006 §Implementation Notes,
            // ADR-0009 IG-6) has `FRotator(lean_angle, 0, 0)` which is Pitch;
            // corrected here to Roll. Spec + ADR amendments tracked in Story 006
            // Completion Notes for follow-up alignment pass.
            CachedMeshComponent->SetRelativeRotation(FRotator(0.0f, 0.0f, body_lean_deg));

            lean_angle      = body_lean_deg;
            head_lean_angle = head_lean_deg;
            arm_lean_angle  = arm_lean_deg;
        }
        // F-6 site (Story 007): if (edge_absorb_active) AdvanceEdgeAbsorb(effective_dt);

        if (tween_progress >= 1.0f)
        {
            CompleteTween();
        }
    }
    else // movement_state == SETTLED
    {
        // AC: lateral_world_position returns LaneWorldX(current_lane) when SETTLED.
        // Keeps the property fresh for Camera and Pull-Wave targeting consumers.
        lateral_world_position = LaneWorldX(current_lane);

        // AC-26: SETTLED (with F-6 inactive) → F-5 outputs zero.
        // Story 007's F-6 tail may leave residual lean during SETTLED entry — that
        // path co-writes lean_angle. This branch only zeroes F-5's contribution.
        // Story 007 will refactor if needed. For now, unconditional zero is correct
        // because F-6 isn't implemented yet.
        lean_angle      = 0.0f;
        head_lean_angle = 0.0f;
        arm_lean_angle  = 0.0f;
    }
}

// ---------------------------------------------------------------------------
// RSM delegate handlers — empty bodies (Stories 008 / 009).
// ---------------------------------------------------------------------------

void UPlayerLaneMovementComponent::HandleStateChanged(
    ERunState PreviousState,
    ERunState NewState,
    ERunOutcome Outcome,
    double Timestamp)
{
#if WITH_DEV_AUTOMATION_TESTS
    ++HandleStateChanged_TestOnlyCallCount;
#endif // WITH_DEV_AUTOMATION_TESTS
    // TODO(Story 008): implement DEAD/COMPLETE/ABORTED/COUNTDOWN switch bodies
    // + AC-SS-B runtime default-branch behavior + counter reset semantics.
}

void UPlayerLaneMovementComponent::HandlePausedChanged(
    bool bIsPaused,
    double Timestamp)
{
    // TODO(Story 009): implement pause freeze / resume-grace integration.
}

// ---------------------------------------------------------------------------
// HandleSlipTransition — Story 003, initial-input path.
//
// SETTLED path (ADR-0009 SD5):
//   1. Rule 5 gate — discard if not active RUNNING.
//   2. If already SLIPPING — buffered-input path (Story 005 stub: early return).
//   3. F-4 pre-validation via IsSlipValidFromLane:
//      - Fail → TriggerEdgeAbsorb (Story 007 stub).
//      - Pass → SETTLED→SLIPPING transition:
//          target_lane = projected lane (from F-4 out-param)
//          SetActorLocation to LaneWorldX(target_lane) — collision commit (SD5)
//          movement_state = SLIPPING
//          tween_progress = 0.0f
//          TriggerCommitmentTell (Story 010 stub)
//
// Source-lane invariant: current_lane is NOT touched here. It stays as the
// source lane. Only CompleteTween reassigns current_lane = target_lane.
// ---------------------------------------------------------------------------

void UPlayerLaneMovementComponent::HandleSlipTransition(ESlipDirection Dir)
{
    // Rule 5 gate — also enforced at input source, but defensive here per spec.
    if (!RSMSubsystem
        || RSMSubsystem->GetCurrentState() != ERunState::RUNNING
        || RSMSubsystem->IsPaused()
        || RSMSubsystem->IsResumeGrace())
    {
        return; // DISCARD (not buffered)
    }

    if (movement_state == ERunSlipState::SLIPPING)
    {
        // Story 005: Buffered-input path (Rule 3 + F-4 pre-validation).
        // GDD: design/gdd/player-movement-mechanics.md §3 Rules 3/4/11, §4 F-4.
        // TR-PM-012: single-slot input buffer with F-4 pre-validation.

        if (has_queued_input)
        {
            // Rule 3 drop: buffer is full — leave the existing queued slot UNCHANGED.
            // Fire haptic dispatch (AC-25: synchronous within this event call).
            if (IHapticDispatch::IsSystemHapticsEnabled())
            {
                IHapticDispatch::Fire(EHapticEvent::BufferDrop);
            }
            // Presentation dispatch: audio sting fires regardless of haptic gate (AC-25).
            PlayBufferDropAudioSting();
            return;
        }

        // Buffer is empty: F-4 pre-validation against target_lane (projected end-state).
        // Using target_lane (not current_lane) — this is the BUFFERED path, where the
        // tween is still in flight and the player's next committed source will be target_lane.
        EPlayerLane ProjectedTarget = target_lane; // initialise to satisfy the compiler
        if (!IsSlipValidFromLane(target_lane, Dir, ProjectedTarget))
        {
            // F-4 rejected the buffered input — off-track from projected target_lane.
            // Fire edge-absorb hook (Story 007 stub) and discard without buffering.
            // No haptic BufferDrop — this is an edge no-op, not a buffer-full drop.
            TriggerEdgeAbsorb(target_lane, Dir);
            return;
        }

        // Valid buffered input — commit to the single-slot buffer.
        has_queued_input        = true;
        queued_input_direction  = Dir;
        return;
    }

    // SETTLED path — F-4 pre-validation.
    EPlayerLane ProjectedTarget = current_lane; // initialise to satisfy the compiler; overwritten on success
    if (!IsSlipValidFromLane(current_lane, Dir, ProjectedTarget))
    {
        // Rule 1 edge no-op path — fire edge-absorb hook (Story 007).
        TriggerEdgeAbsorb(current_lane, Dir);
        return;
    }

    // SETTLED → SLIPPING transition (Rule 2, ADR-0009 SD5).
    // NOTE: current_lane is NOT reassigned here. It retains the source lane value
    // throughout SLIPPING per the source-lane semantic (R7-PM-PROPAGATION-REVIEW).
    // current_lane = target_lane happens ONLY in CompleteTween().
    target_lane = ProjectedTarget;

    // Collision commit: root pawn position snaps to target lane world X BEFORE
    // any tween progress accumulates (ADR-0009 SD5). bSweep=false — constant-time
    // transform update; skips Chaos physics resolution (Engine Compat Verification #3).
    GetOwner()->SetActorLocation(
        FVector(LaneWorldX(target_lane), 0.0f, 0.0f),
        /*bSweep=*/false);

    movement_state = ERunSlipState::SLIPPING;
    tween_progress = 0.0f;

    // Commitment-tell hook — Story 010 stub. TargetLane passed so Story 010 can
    // determine slip direction sign for mesh-face selection.
    TriggerCommitmentTell(target_lane);
}

// ---------------------------------------------------------------------------
// ValidateCurveAsset — ADR-0009 SD6 pseudo-code implementation.
// Checks: non-null, >= 2 keys, key range spans [0, 1].
// On failure: log Error, set bCurveFallbackActive = true.
// ---------------------------------------------------------------------------

void UPlayerLaneMovementComponent::ValidateCurveAsset(
    UCurveFloat* Curve,
    const TCHAR* CurveName)
{
    if (!Curve)
    {
        UE_LOG(LogPlayerMovement, Error,
            TEXT("PlayerLaneMovementComponent::ValidateCurveAsset — %s is null. "
                 "Engaging curve fallback path (AC-SS-D)."), CurveName);
        bCurveFallbackActive = true;
        return;
    }

    const TArray<FRichCurveKey>& Keys = Curve->FloatCurve.Keys;

    if (Keys.Num() < 2)
    {
        UE_LOG(LogPlayerMovement, Error,
            TEXT("PlayerLaneMovementComponent::ValidateCurveAsset — %s has %d key(s); "
                 "requires >= 2. Engaging curve fallback path (AC-SS-D)."),
            CurveName, Keys.Num());
        bCurveFallbackActive = true;
        return;
    }

    // Keys are sorted by time in FRichCurve — first and last key suffice for
    // range validation.
    const float FirstKeyTime = Keys[0].Time;
    const float LastKeyTime  = Keys[Keys.Num() - 1].Time;

    if (FirstKeyTime > 0.0f || LastKeyTime < 1.0f)
    {
        UE_LOG(LogPlayerMovement, Error,
            TEXT("PlayerLaneMovementComponent::ValidateCurveAsset — %s key range "
                 "[%f, %f] does not span [0, 1]. Engaging curve fallback path (AC-SS-D)."),
            CurveName, FirstKeyTime, LastKeyTime);
        bCurveFallbackActive = true;
    }
}

// ---------------------------------------------------------------------------
// Story 002 — lane geometry helpers
// ---------------------------------------------------------------------------

// LaneWorldX — Formula F-1.
// Maps EPlayerLane ordinal to world-space X (cm).
// Ordinals: FarLeft=0, Left=1, Center=2, Right=3, FarRight=4.
// Center (2) → X = 0.  Left (1) → X = -100.  Right (3) → X = +100.

float UPlayerLaneMovementComponent::LaneWorldX(EPlayerLane Lane)
{
    const int32 Ordinal = static_cast<int32>(Lane);
    return static_cast<float>(Ordinal - 2) * SLIPSTORM_PM::LANE_WIDTH_CM;
}

// ---------------------------------------------------------------------------
// ComputeTickDT — F-PROLOGUE (platform §4).
// Single-source DT invariant: OutRawDT = FApp::GetDeltaTime() (unclamped, for
// watchdog); OutEffectiveDT = FMath::Clamp(OutRawDT, 0.0f, MAX_SLIP_DT_S) (for
// mechanics).  Story 013 watchdog consumes raw_dt; Story 007 F-6 + Story 003
// F-2 site consume effective_dt.
//
// Does NOT set bSlipTweenClampActive — that flag reflects the SLIP_TWEEN
// knob-range clamp, not the DT overrun clamp.  FApp::GetDeltaTime() is chosen
// over GetWorld()->GetDeltaSeconds() so the watchdog stays hardware-semantically
// correct if slow-motion / replay-scrubbing gets added later.
// ---------------------------------------------------------------------------

void UPlayerLaneMovementComponent::ComputeTickDT(float& OutRawDT, float& OutEffectiveDT) const
{
    OutRawDT       = FApp::GetDeltaTime();
    OutEffectiveDT = FMath::Clamp(OutRawDT, 0.0f, SLIPSTORM_PM::MAX_SLIP_DT_S);
}

// ---------------------------------------------------------------------------
// GetEffectiveSlipTween — SLIP_TWEEN_DURATION_S persistent clamp
//                        (AC-21 / AC-SS-A shipping-safety knob-range guard).
//
// Returns FMath::Clamp(SLIP_TWEEN_DURATION_S, 0.10f, 0.15f).
// Sets bSlipTweenClampActive = true when the raw knob is out of range this call;
// clears it otherwise (per-call state, not one-shot).
//
// Rate-limited Error log fires when SlipTweenClampLogTickCounter % 600 == 0
// on a violating call (tick 1 of a sustained violation, then tick 601, tick 1201,
// ...).  Reset on any non-violating call so a resumed violation logs immediately.
// Boundary values 0.10f and 0.15f exactly are NOT violations (strict `<` / `>`).
//
// Four-site lockstep policy (platform §3 Shipping-Safety Enforcement Policy):
// if the tick-601 threshold is revised, all four sites must update in lockstep:
//   (1) this method body
//   (2) platform §3 Shipping-Safety table SLIP_TWEEN row AC-21 fold-in prose
//   (3) platform §8 AC-21 body
//   (4) platform §3 AC-SS-A body
// ---------------------------------------------------------------------------

float UPlayerLaneMovementComponent::GetEffectiveSlipTween()
{
    constexpr float FloorS   = 0.10f;
    constexpr float CeilingS = 0.15f;

    const bool bViolation = (SLIP_TWEEN_DURATION_S < FloorS)
                         || (SLIP_TWEEN_DURATION_S > CeilingS);

    if (bViolation)
    {
        bSlipTweenClampActive = true;

        if (SlipTweenClampLogTickCounter % 600 == 0)
        {
            // %.6f explicit — %f default digit count is not cross-platform guaranteed;
            // AddExpectedError substring match in PMLaneAndTweenTest relies on the
            // 6-digit form (e.g. "0.090000"). Do NOT reduce precision here.
            UE_LOG(LogPlayerMovement, Error,
                TEXT("SLIP_TWEEN_DURATION_S=%.6f out of safe range [0.10,0.15]; "
                     "clamping to nearest bound. Counter=%d (logs every 600 consecutive violations)."),
                SLIP_TWEEN_DURATION_S, SlipTweenClampLogTickCounter);
        }
        ++SlipTweenClampLogTickCounter;
    }
    else
    {
        bSlipTweenClampActive = false;
        SlipTweenClampLogTickCounter = 0;
    }

    return FMath::Clamp(SLIP_TWEEN_DURATION_S, FloorS, CeilingS);
}

// ---------------------------------------------------------------------------
// AdvanceTweenProgress — F-2 pure accumulator step.
// Increments tween_progress by (EffectiveDT / GetEffectiveSlipTween()).
// Does NOT test for completion, does NOT commit lanes, does NOT modify
// movement_state.  Story 003's CompleteTween() owns those side effects when
// tween_progress crosses 1.0f.
//
// Called from TickComponent (Story 003) while movement_state == SLIPPING.
// Private — FPMLaneAndTweenTest and FPMStateMachineTest access via friend.
//
// WARNING to TickComponent wiring: GetEffectiveSlipTween() is called ONCE per
// tick from inside this method.  If TickComponent also calls GetEffectiveSlipTween()
// standalone BEFORE this method, the rate-limit counter double-increments and
// the log fires at tick 301 instead of tick 601.  Route TickComponent through
// this method only — don't call GetEffectiveSlipTween() independently in the same tick.
// ---------------------------------------------------------------------------

void UPlayerLaneMovementComponent::AdvanceTweenProgress(float EffectiveDT)
{
    tween_progress += EffectiveDT / GetEffectiveSlipTween();
}

// ---------------------------------------------------------------------------
// Story 003 — IsSlipValidFromLane (F-4 pure utility).
//
// Projects the target lane ordinal from FromLane by ±1 depending on Dir.
// Right → ordinal + 1; Left → ordinal - 1.
// Returns true and sets OutTargetLane if projected ordinal is in [0, 4].
// Returns false and leaves OutTargetLane unchanged for out-of-range (edge) inputs.
//
// Ordinal bounds check uses JC-3 approved form: ordinal ± 1 with explicit
// bounds check (no cast-then-enum-max compare; no static_cast<EPlayerLane>
// of out-of-range value).
// ---------------------------------------------------------------------------

bool UPlayerLaneMovementComponent::IsSlipValidFromLane(
    EPlayerLane FromLane,
    ESlipDirection Dir,
    EPlayerLane& OutTargetLane) const
{
    const int32 FromOrdinal  = static_cast<int32>(FromLane);
    const int32 Delta        = (Dir == ESlipDirection::Right) ? +1 : -1;
    const int32 ToOrdinal    = FromOrdinal + Delta;

    // Valid lane ordinals: [0, 4] (FarLeft=0 … FarRight=4).
    if (ToOrdinal < 0 || ToOrdinal > 4)
    {
        // Edge no-op — OutTargetLane is left unchanged per spec.
        return false;
    }

    OutTargetLane = static_cast<EPlayerLane>(ToOrdinal);
    return true;
}

// ---------------------------------------------------------------------------
// Story 003 — CompleteTween.
//
// Finalises a completed tween. This is the SOLE write site for
// current_lane = target_lane (Rule 4 source-lane semantic binding,
// R7-PM-PROPAGATION-REVIEW). Do NOT write current_lane = target_lane anywhere else.
//
// Steps:
//   1. tween_progress = 1.0f (clamp to exactly 1.0 — downstream F-3 relative-
//      offset invariant per Story 004 requires the settled sentinel value).
//   2. current_lane = target_lane  (SOURCE-LANE COMMIT — only here).
//   3. movement_state = SETTLED.
//   4. slip_complete_count += 1.
//   5. FlushBufferedInput() — Story 005 stub; same tick, no idle frame.
// ---------------------------------------------------------------------------

void UPlayerLaneMovementComponent::CompleteTween()
{
    tween_progress = 1.0f;
    current_lane   = target_lane;   // SOURCE-LANE COMMIT — only legal write site.
    movement_state = ERunSlipState::SETTLED;
    slip_complete_count += 1;
    FlushBufferedInput(); // Story 005 stub — implementation in Story 005.
}

// ---------------------------------------------------------------------------
// Story 003 stubs — bodies intentionally empty; signatures are load-bearing
// for future-story callsites.
// ---------------------------------------------------------------------------

void UPlayerLaneMovementComponent::FlushBufferedInput()
{
    // Story 005 (AC-04, AC-34b): invoked from the tail of CompleteTween. Same tick,
    // no idle frame — the buffered slip commits its own SETTLED→SLIPPING transition
    // synchronously as part of the completing tween's frame.
    if (!has_queued_input)
    {
        return;
    }
    // Snapshot direction, clear the flag BEFORE re-entering HandleSlipTransition.
    // At this point CompleteTween has already run: movement_state == SETTLED, so the
    // re-entry hits the SETTLED path (F-4 validate → collision commit → SLIPPING).
    const ESlipDirection Dir = queued_input_direction;
    has_queued_input          = false;
    HandleSlipTransition(Dir);
}

void UPlayerLaneMovementComponent::DiscardBuffer()
{
    // Story 005 / Rule 11: buffer is discarded on ANY non-RUNNING RSM state entry.
    // Story 008's HandleStateChanged terminal-state branches invoke this helper.
    // Story 009's HandlePausedChanged does NOT call this — pause PRESERVES the
    // buffer per AC-13.
    has_queued_input = false;
}

void UPlayerLaneMovementComponent::PlayBufferDropAudioSting()
{
    // Story 005 presentation dispatch — stub. The audio system owns the actual cue;
    // this method just fires the dispatch. Wiring to the audio subsystem is
    // downstream (audio-designer / audio-programmer scope, out of PM epic).
    UE_LOG(LogPlayerMovement, Verbose, TEXT("PlayBufferDropAudioSting fired"));

#if WITH_DEV_AUTOMATION_TESTS
    // Test hook — AC-25 synchronous-dispatch verification.
    ++BufferDropAudioSting_TestOnlyCallCount;
#endif
}

void UPlayerLaneMovementComponent::TriggerEdgeAbsorb(EPlayerLane FromLane, ESlipDirection Dir)
{
    // TODO(Story 007): implement F-6 edge-absorb: advance edge overshoot timer,
    // increment edge_absorb_trigger_count, trigger visual feedback using FromLane
    // and Dir for direction-specific mesh deformation (e.g., wall-bounce sign).
    // FromLane: source lane that attempted the off-track slip.
    // Dir: the direction that was rejected (Left or Right at the boundary).
}

void UPlayerLaneMovementComponent::TriggerCommitmentTell(EPlayerLane TargetLane)
{
    // TODO(Story 010): use TargetLane to select mesh face for the commitment flash:
    // +X face for a rightward slip (TargetLane ordinal > current_lane ordinal at call time),
    // -X face for a leftward slip (TargetLane ordinal < current_lane ordinal at call time).
    // Increment commitment_tell_fire_count and enforce cadence cap.
}

// ---------------------------------------------------------------------------
// Story 004 — F3RelativeOffset (F-3 lateral interpolation)
//
// Returns the relative-space X offset for CachedMeshComponent during SLIPPING.
//
// Formula:
//   delta   = LaneWorldX(current_lane) - LaneWorldX(target_lane)
//             (offset from target; source is the non-zero end)
//   curve_t = FMath::Clamp(SlipCurve->GetFloatValue(TweenProgress), 0, 1)
//             OR FMath::Clamp(TweenProgress, 0, 1) in the fallback path.
//   result  = FMath::Lerp(delta, 0.0f, curve_t)
//
// At TweenProgress=1.0:
//   curve_t clamps to 1.0 (identity/authored curves and fallback alike).
//   FMath::Lerp(delta, 0.0f, 1.0f) returns exactly 0.0f (ADR-0009 IG-5).
//   No absolute-position clamp is needed or applied — the relative form
//   lands at 0 naturally. See TC f3_tp1_zero_relative_invariant.
//
// Grep gate: F3RelativeOffset must NOT clamp GetComponentLocation().X or any
// absolute-position value. The ONLY FMath::Clamp calls are on curve_t.
// ADR-0009 IG-5; confirmed by code review invariant.
//
// Pure math — reads current_lane, target_lane, SlipCurve, bCurveFallbackActive.
// Writes nothing. O(1) — no allocations on the tick path.
// GDD: design/gdd/player-movement-mechanics.md §4 F-3. Story 004.
// ---------------------------------------------------------------------------

float UPlayerLaneMovementComponent::F3RelativeOffset(float TweenProgress) const
{
    const float source_x = LaneWorldX(current_lane);
    const float target_x = LaneWorldX(target_lane);
    const float delta    = source_x - target_x;

    float curve_t;
    if (SlipCurve && !bCurveFallbackActive)
    {
        curve_t = FMath::Clamp(SlipCurve->GetFloatValue(TweenProgress), 0.0f, 1.0f);
    }
    else
    {
        // Fallback (SD6): linear F-3, no curve shaping. Clamp TweenProgress
        // to [0,1] — accumulator can exceed 1.0 before CompleteTween runs on
        // the same tick (Story 002 TC3 hitch evidence). Preserves the
        // TP>=1.0 zero-relative invariant symmetrically with the SlipCurve path.
        curve_t = FMath::Clamp(TweenProgress, 0.0f, 1.0f);
    }

    return FMath::Lerp(delta, 0.0f, curve_t);
}

// ---------------------------------------------------------------------------
// Story 006 — F-5 lean: DirectionSign + ComputeLean
//
// DirectionSign:
//   Returns +1.0f when ToLane ordinal > FromLane ordinal (rightward slip),
//   returns -1.0f otherwise (leftward slip, same lane [invalid at runtime]).
//   Pure math on lane ordinals.
//   GDD: design/gdd/player-movement-mechanics.md §4 F-5. TR-PM-008.
//
// ComputeLean:
//   Samples LeanCurve at three staggered TweenProgress offsets:
//     body: TweenProgress
//     head: max(0.0, TweenProgress - HEAD_LAG_PROGRESS)  [lags body]
//     arm:  min(1.0, TweenProgress + ARM_LEAD_PROGRESS)  [leads body]
//   Each scaled by (MAX_LEAN_ANGLE_DEG * DirectionSign) and clamped to
//   ±(MAX_LEAN_ANGLE_DEG * 1.2f) per TR-PM-033.
//   Fallback (SD6): LeanCurve null OR bCurveFallbackActive → zero lean.
//   ADR-0009 SD5 + SD6 + IG-6. TR-PM-008, TR-PM-029, TR-PM-033. Story 006.
// ---------------------------------------------------------------------------

float UPlayerLaneMovementComponent::DirectionSign(EPlayerLane FromLane, EPlayerLane ToLane)
{
    const int32 delta = static_cast<int32>(ToLane) - static_cast<int32>(FromLane);
    return (delta > 0) ? 1.0f : -1.0f;
    // Note: delta == 0 (same lane) is not a valid F-5 input — F-5 only runs during
    // SLIPPING, and SLIPPING requires target != current per Story 003 Rule 2.
    // Convention: return -1 for zero-delta (never observed at runtime).
}

void UPlayerLaneMovementComponent::ComputeLean(
    float TweenProgress,
    EPlayerLane FromLane,
    EPlayerLane ToLane,
    float& OutBodyLean,
    float& OutHeadLean,
    float& OutArmLean) const
{
    // Fallback (SD6): LeanCurve null OR runtime fallback flag set → zero lean.
    if (!LeanCurve || bCurveFallbackActive)
    {
        OutBodyLean = 0.0f;
        OutHeadLean = 0.0f;
        OutArmLean  = 0.0f;
        return;
    }

    const float sign      = DirectionSign(FromLane, ToLane);
    const float max_clamp = MAX_LEAN_ANGLE_DEG * 1.2f;

    // Body: sampled at TweenProgress clamped to [0, 1]. Symmetric with the
    // head Max(0, ...) and arm Min(1, ...) clamps below. F-2 accumulator can
    // exceed 1.0 on a hitch (Story 002 TC3 evidence: reaches 1.667 on 5-tick
    // hitch); without this clamp the body curve read would extrapolate, whereas
    // head and arm inputs would still be clamped. Story 004's F-3 solved the
    // same asymmetry the same way.
    const float body_t = LeanCurve->GetFloatValue(FMath::Clamp(TweenProgress, 0.0f, 1.0f));
    // Head: lags body by HEAD_LAG_PROGRESS; clamp input to [0, ∞) via Max.
    const float head_t = LeanCurve->GetFloatValue(FMath::Max(0.0f, TweenProgress - HEAD_LAG_PROGRESS));
    // Arm: leads body by ARM_LEAD_PROGRESS; clamp input to (-∞, 1.0] via Min.
    const float arm_t  = LeanCurve->GetFloatValue(FMath::Min(1.0f, TweenProgress + ARM_LEAD_PROGRESS));

    OutBodyLean = FMath::Clamp(body_t * MAX_LEAN_ANGLE_DEG * sign, -max_clamp, max_clamp);
    OutHeadLean = FMath::Clamp(head_t * MAX_LEAN_ANGLE_DEG * sign, -max_clamp, max_clamp);
    OutArmLean  = FMath::Clamp(arm_t  * MAX_LEAN_ANGLE_DEG * sign, -max_clamp, max_clamp);
}

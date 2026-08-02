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
// Story 007 scope: F-6 edge-absorb tail — TriggerEdgeAbsorb (body),
//   ComputeF6 (EdgeAbsorbCurve + EC15 decay + §5.1(a) fade-out), tick restructure
//   (F-6 advance outside SLIPPING/SETTLED branches, F-5+F-6 co-write sum replacing
//   F-5-only mesh write, §5.1(a) Override hook in HandleSlipTransition).
//   TR-PM-009 (F-6 tail); TR-PM-028 (EC15 decay); TR-PM-033 (final clamp).
// Story 008 scope: HandleStateChanged full switch (DEAD/COMPLETE/ABORTED/COUNTDOWN/IDLE),
//   AC-F6-D DEAD guard on F-6 tick advance (defense-in-depth; Rule 5 gate already stops
//   F-6 advance on DEAD via the RUNNING check at cpp line ~202), SnapToTargetAndReset
//   helper (TR-PM-018), GetMovementStateExternal accessor (AC-SS-B / Seam 12).
//   TR-PM-015 (monotonic counters), TR-PM-016 (reset on COUNTDOWN), TR-PM-017 (DEAD freeze),
//   TR-PM-018 (COMPLETE/ABORTED snap). Non-callback invariant: ADR-0007 SD2.
// Story 009 scope: HandlePausedChanged body. (deferred)
// Watchdog buffer advance by Story 013.

#include "Player/PlayerLaneMovementComponent.h"
#include "Player/SlipstormPlayerPawn.h"       // ASlipstormPlayerPawn::MeshComponent — F-3 cache (Story 004)
#include "RunStateMachine/RunStateMachineSubsystem.h"
#include "Components/StaticMeshComponent.h"   // UStaticMeshComponent::SetRelativeLocation — F-3 (Story 004)
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Misc/App.h"  // FApp::GetDeltaTime() — F-PROLOGUE (Story 002)
#include "Seam/IHapticDispatch.h"             // ADR-0002 INT-002-amended haptic bridge — Story 005
#include "Seam/PlayerMovementProvider.h"      // EMovementState (plain enum class) — Story 008 AC-SS-B
#include "Materials/MaterialInstanceDynamic.h"  // Story 010: commitment-tell flash write

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

    // --- 1c. Resolve MeshMaterialDynamic — Story 010 commitment-tell write target ---
    // Slot 0 = per Art Bible authoring — confirm with art-director if this changes.
    // Nullable: if creation fails (headless test, missing material), commitment-tell
    // counter still increments; only the visual flash write no-ops.
    if (IsValid(CachedMeshComponent))
    {
        MeshMaterialDynamic = CachedMeshComponent->CreateAndSetMaterialInstanceDynamic(0);
        if (!IsValid(MeshMaterialDynamic))
        {
            UE_LOG(LogPlayerMovement, Warning,
                TEXT("Story 010: CreateAndSetMaterialInstanceDynamic returned null. "
                     "Commitment-tell flash writes will no-op; counter still increments."));
        }
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
    }

    // F-6 tick advance — runs OUTSIDE the SLIPPING/SETTLED branches so the
    // edge-absorb tail continues while the PM is SETTLED (Rule 1 edge no-op path).
    // edge_absorb_local_timer_s and edge_absorb_progress are only mutated here and
    // in TriggerEdgeAbsorb; they are O(1) and allocate nothing on the hot path.
    // Story 007; TR-PM-009.
    //
    // AC-F6-D (Story 008 defense-in-depth): guard on DEAD so the F-6 state snapshot
    // is preserved for Death Replay even if this code path is reached. In practice
    // the Rule 5 gate at ~line 202 (RUNNING check) already prevents execution here
    // when RSM is DEAD — this guard is redundant but required by the story spec as
    // explicit defense-in-depth. TR-PM-017.
    const bool bRunStateDead = RSMSubsystem
        && (RSMSubsystem->GetCurrentState() == ERunState::DEAD);
    if (edge_absorb_active && !bRunStateDead)
    {
        edge_absorb_local_timer_s += effective_dt;
        edge_absorb_progress = FMath::Clamp(
            edge_absorb_local_timer_s / EDGE_ABSORB_DURATION_S, 0.0f, 1.0f);

        if (edge_absorb_progress >= 1.0f)
        {
            edge_absorb_active = false;
        }
    }

    // F-5 + F-6 co-write — final summed lean angles written to mesh and public props.
    //
    // Structure:
    //   F-5 (ComputeLean): active only during SLIPPING (returns 0 during SETTLED per
    //     AC-26 / AC-30 — LeanCurve fallback guard already handles null path inside
    //     ComputeLean; we only call it while SLIPPING to avoid the SETTLED zero-lean
    //     invariant relying on ComputeLean's internals).
    //   F-6 (ComputeF6): active during SLIPPING or SETTLED while F-6 tail or fade-out
    //     is running (called unconditionally; returns 0 when neither is active).
    //   Final sum re-clamped to ±(MAX_LEAN×1.2) per TR-PM-033 + Control Manifest.
    //
    // AXIS: FRotator(Pitch, Yaw, Roll). Lateral lean is ROLL (rotation around
    // forward X axis). NOT Pitch. Corrected from Story 006 spec text during
    // /code-review. Story 006 composition test f5_roll_axis_regression_guard gates
    // this. Story 007 MUST NOT regress it. Story 007; TR-PM-008/028/033.
    //
    // IG-6: SetActorRotation on pawn root is FORBIDDEN — rotate the mesh only.
    if (IsValid(CachedMeshComponent))
    {
        // F-5 — non-zero only while SLIPPING (returns zero from ComputeLean fallback
        // guard when curve is null, but calling during SETTLED is also fine because
        // DirectionSign(current_lane, target_lane) = -1 when same-lane; combined with
        // a zero-valued LeanCurve read at TP=0 it would produce 0 anyway). For
        // correctness and clarity we gate on SLIPPING explicitly.
        float f5_body = 0.0f, f5_head = 0.0f, f5_arm = 0.0f;
        if (movement_state == ERunSlipState::SLIPPING)
        {
            ComputeLean(tween_progress, current_lane, target_lane,
                        f5_body, f5_head, f5_arm);
        }

        // F-6 — active during SLIPPING or SETTLED while tail/fade-out is live.
        float f6_body = 0.0f, f6_head = 0.0f, f6_arm = 0.0f;
        ComputeF6(f6_body, f6_head, f6_arm);

        // Final sum + clamp (TR-PM-033; Control Manifest Required).
        const float max_clamp = MAX_LEAN_ANGLE_DEG * 1.2f;
        lean_angle      = FMath::Clamp(f5_body + f6_body, -max_clamp, max_clamp);
        head_lean_angle = FMath::Clamp(f5_head + f6_head, -max_clamp, max_clamp);
        arm_lean_angle  = FMath::Clamp(f5_arm  + f6_arm,  -max_clamp, max_clamp);

        // Write mesh rotation — Roll axis only (IG-6; Story 006 + 007).
        CachedMeshComponent->SetRelativeRotation(FRotator(0.0f, 0.0f, lean_angle));
    }
    else
    {
        // Mesh unavailable — publish zero lean to public props so consumers read
        // consistent values. AC-26 / AC-30 invariant still satisfied.
        lean_angle      = 0.0f;
        head_lean_angle = 0.0f;
        arm_lean_angle  = 0.0f;
    }

    // Story 010: commitment-tell flash lifecycle advance (2-frame hold + 50ms decay).
    // Runs INSIDE the Rule 5 RUNNING gate (above) — flash lifecycle only advances
    // during active gameplay, never during pause/grace/non-RUNNING states.
    if (flash_hold_ticks_remaining > 0)
    {
        --flash_hold_ticks_remaining;
        if (flash_hold_ticks_remaining == 0)
        {
            // Hold phase complete — start decay from ±0.80 to 0.
            flash_decay_active = true;
            flash_decay_time_s = 0.050f; // 50ms decay per TR-PM-013.
        }
    }
    else if (flash_decay_active)
    {
        flash_decay_time_s -= effective_dt;
        // Ramp from ±0.80 → 0 over 50ms. t = remaining / total (1.0 → 0.0).
        const float t = FMath::Clamp(flash_decay_time_s / 0.050f, 0.0f, 1.0f);
        if (IsValid(MeshMaterialDynamic))
        {
            MeshMaterialDynamic->SetScalarParameterValue(
                TEXT("LeadingFaceFlash"),
                sign_of_current_flash * COMMIT_FLASH_AMPLITUDE * t);
#if WITH_DEV_AUTOMATION_TESTS
            ++CommitmentTellFlashWrite_TestOnlyCallCount;
#endif
        }
        if (flash_decay_time_s <= 0.0f)
        {
            // Fade-to-zero moment — update cadence-gate reference and clear decay flag.
            flash_decay_active = false;
            flash_decay_time_s = 0.0f;
            const UWorld* World = GetWorld();
            time_last_flash_zero_s = World ? World->GetTimeSeconds() : 0.0f;
        }
    }
}

// ---------------------------------------------------------------------------
// RSM delegate handlers
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// HandleStateChanged — Story 008.
//
// Implements terminal-state handlers for all RSM state entries:
//   DEAD       — freeze F-6 in place (TR-PM-017 / AC-F6-D); commit source-lane
//                if SLIPPING; discard buffer. Counters PRESERVED (TR-PM-015).
//   COMPLETE   — SnapToTargetAndReset + discard buffer. Counters PRESERVED.
//   ABORTED    — SnapToTargetAndReset + discard buffer. Counters PRESERVED.
//   COUNTDOWN  — SnapToTargetAndReset + discard buffer + RESET counters
//                (TR-PM-016: only COUNTDOWN resets slip_complete_count,
//                edge_absorb_trigger_count, commitment_tell_fire_count).
//   IDLE       — discard buffer; reset F-6 state to zero baseline.
//                Counters PRESERVED (no counter reset on IDLE per spec).
//   All others (RUNNING, RESOLVING) — no-op (PM self-activates via tick).
//
// Non-callback invariant (ADR-0007 SD2):
//   MUST NOT call RSMSubsystem->ForceTickNow().
//   MUST NOT broadcast OnSlipMidpoint.
//   MUST NOT modify RSM state.
//
// PreviousState, Outcome, and Timestamp are unused by this story; (void)-cast.
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

    // Suppress unused-parameter warnings — ADR-0007 SD2 prohibits acting on these.
    (void)PreviousState;
    (void)Outcome;
    (void)Timestamp;

    switch (NewState)
    {
    // -----------------------------------------------------------------------
    // DEAD — freeze everything, preserve F-6 state for Death Replay (AC-F6-D).
    // TR-PM-017; ADR-0009 SD5 (DEAD freeze semantic).
    // -----------------------------------------------------------------------
    case ERunState::DEAD:
    {
        // Rule 7 — DEAD freeze (TR-PM-017). Preserve tween_progress, mesh
        // transforms, F-5 lean, F-6 state for Death Replay. Do NOT snap.
        // Only actions:
        //   1. Commit source-lane semantic (current_lane = target_lane) if SLIPPING.
        //   2. Mark movement_state = SETTLED so no more slip mechanics run.
        //   3. Discard buffered input (no slip fires post-death).
        //
        // R7-PM-PROPAGATION-REVIEW §DEAD handling. This is one of only two
        // legal write sites for current_lane = target_lane; CompleteTween is
        // the other.
        if (movement_state == ERunSlipState::SLIPPING)
        {
            current_lane = target_lane;
        }
        movement_state = ERunSlipState::SETTLED;

        // Discard any pending input — no buffered slip should fire post-death.
        // AC-13 pause exemption does NOT apply here.
        DiscardBuffer();

        // Explicitly NOT touched (preserved for Death Replay per AC-14 / AC-31 / AC-F6-D):
        //   - tween_progress: bit-exact preserved (AC-14 boundary tests TP=0.001, 0.999)
        //   - lean_angle / head_lean_angle / arm_lean_angle: frozen (AC-31)
        //   - mesh relative transforms (SetRelativeLocation / SetRelativeRotation):
        //     no writes → last SLIPPING-tick values persist
        //   - F-6 state (edge_absorb_active, progress, sign, local_timer_s,
        //     f6_override_fadeout_*): preserved (AC-F6-D). F-6 tick advance guard
        //     at cpp:~271 prevents subsequent ticks from mutating progress/timer.
        //   - Counters (slip_complete_count, edge_absorb_trigger_count,
        //     commitment_tell_fire_count): preserved (TR-PM-015, AC-COUNTER-DEAD).
        break;
    }

    // -----------------------------------------------------------------------
    // COMPLETE — snap mesh to destination, reset movement state. TR-PM-018.
    // -----------------------------------------------------------------------
    case ERunState::COMPLETE:
    {
        SnapToTargetAndReset();
        DiscardBuffer();
        // Counters preserved — COMPLETE does not reset monotonic counters (TR-PM-015).
        break;
    }

    // -----------------------------------------------------------------------
    // ABORTED — same snap behavior as COMPLETE. TR-PM-018.
    // -----------------------------------------------------------------------
    case ERunState::ABORTED:
    {
        SnapToTargetAndReset();
        DiscardBuffer();
        // Counters preserved — ABORTED does not reset monotonic counters (TR-PM-015).
        break;
    }

    // -----------------------------------------------------------------------
    // COUNTDOWN — snap + discard + RESET counters. TR-PM-016.
    // This is the ONLY state entry that resets the monotonic counters.
    // -----------------------------------------------------------------------
    case ERunState::COUNTDOWN:
    {
        // Set current_lane and target_lane to Center before SnapToTargetAndReset
        // so the snap writes current_lane = target_lane = Center (new-run start).
        current_lane = EPlayerLane::Center;
        target_lane  = EPlayerLane::Center;
        SnapToTargetAndReset();
        DiscardBuffer();

        // Counter reset — ONLY on COUNTDOWN (TR-PM-016 monotonic counter semantics).
        slip_complete_count         = 0;
        edge_absorb_trigger_count   = 0;
        commitment_tell_fire_count  = 0;
        break;
    }

    // -----------------------------------------------------------------------
    // IDLE — discard buffer; reset F-6 state to zero baseline.
    // Counters are PRESERVED on IDLE (TR-PM-015: only COUNTDOWN resets counters).
    // AC-COUNTER-F6-RESET requires F-6 reset on IDLE (alongside COMPLETE/ABORTED/COUNTDOWN).
    // -----------------------------------------------------------------------
    case ERunState::IDLE:
    {
        DiscardBuffer();

        // Reset F-6 state fully on IDLE (session ended or pre-run state).
        edge_absorb_active                = false;
        edge_absorb_progress              = 0.0f;
        edge_absorb_sign                  = 0.0f;
        edge_absorb_local_timer_s         = 0.0f;
        f6_override_fadeout_snapshot_body = 0.0f;
        f6_override_fadeout_snapshot_head = 0.0f;
        f6_override_fadeout_snapshot_arm  = 0.0f;
        f6_override_fadeout_ticks_remaining = 0;
        break;
    }

    // -----------------------------------------------------------------------
    // RUNNING / RESOLVING / default — no-op on HandleStateChanged.
    // PM self-activates through TickComponent when RSM is RUNNING.
    // -----------------------------------------------------------------------
    default:
        break;
    }
}

void UPlayerLaneMovementComponent::HandlePausedChanged(bool bIsPaused, double Timestamp)
{
    // Story 009: logging-only body. Pause freeze is REALIZED by TickComponent's
    // Rule 5 gate reading RSMSubsystem->IsPaused() / IsResumeGrace() per tick
    // (Story 003 site at cpp:209-215). Buffer is PRESERVED across pause per
    // Rule 6 — deliberately NOT calling DiscardBuffer() here (contrasts with
    // Story 008's terminal-state handlers which DO discard).
    //
    // Story 013 watchdog may consume Timestamp for pause-window telemetry.
    (void)Timestamp;

#if WITH_DEV_AUTOMATION_TESTS
    ++HandlePausedChanged_TestOnlyCallCount;
#endif

    UE_LOG(LogPlayerMovement, Verbose,
           TEXT("PlayerLaneMovementComponent::HandlePausedChanged(bIsPaused=%s)"),
           bIsPaused ? TEXT("true") : TEXT("false"));
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

    // §5.1(a) Override — if F-6 is mid-tail when SETTLED→SLIPPING fires, snapshot
    // the current F-6 lean values, arm a 2-frame fade-out, and clear the active tail.
    // This prevents a discontinuous jump in the co-write sum when the new F-5 tween
    // starts. The snapshot is captured BEFORE target_lane is updated so ComputeF6
    // reads the correct SETTLED state (edge_absorb_sign already set; tween_progress
    // is 0 / irrelevant for the SETTLED branch inside ComputeF6).
    // R11a §6.1 + §6.2; mechanics §5.1(a); story-007 Implementation Notes. Story 007.
    if (edge_absorb_active)
    {
        ComputeF6(f6_override_fadeout_snapshot_body,
                  f6_override_fadeout_snapshot_head,
                  f6_override_fadeout_snapshot_arm);
        f6_override_fadeout_ticks_remaining = 2;
        edge_absorb_active            = false;
        edge_absorb_progress          = 0.0f;
        edge_absorb_local_timer_s     = 0.0f;
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
    // Story 007: F-6 edge-absorb activation. Called from HandleSlipTransition
    // when F-4 pre-validation rejects the input as off-track (Rule 1 edge no-op)
    // OR when a buffered input's F-4 pre-validation vs target_lane fails.
    //
    // FromLane: reserved for Story 010 direction-specific mesh-face selection
    // (wall-bounce visual cue). Not consumed here — the sign is inferred from
    // Dir alone since at the moment of edge no-op, FromLane == current_lane and
    // the wall is in the direction of Dir.
    (void)FromLane;

    edge_absorb_active        = true;
    edge_absorb_progress      = 0.0f;
    edge_absorb_local_timer_s = 0.0f;

    // Sign convention per AC-F6-A: recoil AWAY from the wall.
    //   slip-right at right edge (FarRight)  → leftward  recoil → sign = -1
    //   slip-left  at left  edge (FarLeft)   → rightward recoil → sign = +1
    // Story spec §Implementation Notes pseudocode had this inverted; corrected
    // here to satisfy AC-F6-A "FarRight + slip-right → body/head/arm all negative
    // at mid-tail" (see .h edge_absorb_sign comment for detail).
    edge_absorb_sign = (Dir == ESlipDirection::Right) ? -1.0f : +1.0f;

    // Rule 1 counter — increments regardless of curve fallback state
    // (SD6 fallback note: "edge-absorb trigger still increments the counter
    // for AC parity even in fallback").
    ++edge_absorb_trigger_count;
}

void UPlayerLaneMovementComponent::TriggerCommitmentTell(EPlayerLane TargetLane)
{
    // Story 010: commitment-tell 80% flash + 200ms cadence cap. TR-PM-013/014/015.

    // AC-COMMIT-FLASH-CADENCE Setup A: counter ALWAYS increments — cadence cap
    // gates the visual only, not the counter. Increment BEFORE the cadence gate.
    ++commitment_tell_fire_count;

    // Cadence gate: suppress visual if within 200ms window from prior fade-to-zero.
    const UWorld* World = GetWorld();
    const float now_s = World ? World->GetTimeSeconds() : 0.0f;
    const float window_s = COMMIT_FLASH_CADENCE_MS * 0.001f;
    if (now_s - time_last_flash_zero_s < window_s)
    {
        return; // Visual suppressed — counter already incremented above.
    }

    // Direction sign: rightward slip (TargetLane index > current_lane index) → +1;
    // leftward → -1. Multiplied by COMMIT_FLASH_AMPLITUDE (±0.80) at write time.
    sign_of_current_flash =
        (static_cast<int32>(TargetLane) > static_cast<int32>(current_lane)) ? +1.0f : -1.0f;

    // Peak flash write — Art Bible authors LeadingFaceFlash as a scalar param
    // on the mesh material slot 0. Null-guard covers headless tests + missing MID.
    if (IsValid(MeshMaterialDynamic))
    {
        MeshMaterialDynamic->SetScalarParameterValue(
            TEXT("LeadingFaceFlash"),
            sign_of_current_flash * COMMIT_FLASH_AMPLITUDE);
#if WITH_DEV_AUTOMATION_TESTS
        ++CommitmentTellFlashWrite_TestOnlyCallCount;
#endif
    }

    // Schedule 2-frame hold + 50ms decay lifecycle (advanced per-tick in TickComponent).
    flash_hold_ticks_remaining = 2;
    flash_decay_active = false;
    flash_decay_time_s = 0.0f;
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

// ---------------------------------------------------------------------------
// Story 007 — ComputeF6 (F-6 edge-absorb tail + EC15 decay + §5.1(a) fade-out)
//
// Three-branch composition:
//   1. Fallback (EdgeAbsorbCurve null OR bCurveFallbackActive):
//      All outputs 0; force edge_absorb_active = false so the tail collapses
//      immediately per SD6 fallback semantic ("edge-absorb trigger still
//      increments edge_absorb_trigger_count for AC parity" — but no visual tail).
//   2. Active branch (edge_absorb_active): sample EdgeAbsorbCurve at three
//      staggered progress offsets (body raw, head TP-HEAD_LAG, arm TP+ARM_LEAD).
//      Multiply by MAX_LEAN_ANGLE_DEG * edge_absorb_sign. During SLIPPING with
//      tween_progress > PHASE3_TP_THRESHOLD, multiply each output by
//      EC15_F6_DECAY_COEFFICIENT (R11a §6.2 headroom-preserving decay).
//   3. Inactive branch (no edge-absorb): outputs 0.
//
// Then the §5.1(a) Override snapshot ADDS its fade-out contribution on top
// (multiplier ramp 1.0 → 0.5 → 0). This decrements ticks_remaining as a side
// effect — one call to ComputeF6 consumes exactly one tick's worth of fade-out.
//
// Final component-level clamp to ±(MAX_LEAN_ANGLE_DEG × 1.2) per TR-PM-033.
// Story 007; TR-PM-009 + TR-PM-028.
// ---------------------------------------------------------------------------

void UPlayerLaneMovementComponent::ComputeF6(float& OutBody, float& OutHead, float& OutArm)
{
    // --- Branch 1: fallback → immediate zero + collapse the tail state ---
    if (!EdgeAbsorbCurve || bCurveFallbackActive)
    {
        OutBody = 0.0f;
        OutHead = 0.0f;
        OutArm  = 0.0f;
        // Collapse per SD6 fallback: no tail-phase animation. Counter was
        // already incremented in TriggerEdgeAbsorb; this line ensures we don't
        // spin on a permanently-active tail in a null-curve scenario.
        edge_absorb_active = false;
    }
    // --- Branch 2: active edge-absorb tail ---
    else if (edge_absorb_active)
    {
        // Body: sampled at raw progress (already in [0,1] per tick-advance clamp).
        const float body_t = EdgeAbsorbCurve->GetFloatValue(
            FMath::Clamp(edge_absorb_progress, 0.0f, 1.0f));
        // Head: lags by HEAD_LAG_PROGRESS. Input clamped to [0, ∞) via Max.
        const float head_t = EdgeAbsorbCurve->GetFloatValue(
            FMath::Max(0.0f, edge_absorb_progress - HEAD_LAG_PROGRESS));
        // Arm: leads by ARM_LEAD_PROGRESS. Input clamped to (-∞, 1.0] via Min.
        const float arm_t = EdgeAbsorbCurve->GetFloatValue(
            FMath::Min(1.0f, edge_absorb_progress + ARM_LEAD_PROGRESS));

        OutBody = body_t * MAX_LEAN_ANGLE_DEG * edge_absorb_sign;
        OutHead = head_t * MAX_LEAN_ANGLE_DEG * edge_absorb_sign;
        OutArm  = arm_t  * MAX_LEAN_ANGLE_DEG * edge_absorb_sign;

        // Phase-3 EC15 decay: applies only when SLIPPING AND past the phase-3
        // TP threshold. Purpose: preserve ±(MAX_LEAN × 1.2) headroom against the
        // F-5 + F-6 sum in the tail of a slip. R11a §6.2; TR-PM-028.
        if (movement_state == ERunSlipState::SLIPPING && tween_progress > PHASE3_TP_THRESHOLD)
        {
            OutBody *= EC15_F6_DECAY_COEFFICIENT;
            OutHead *= EC15_F6_DECAY_COEFFICIENT;
            OutArm  *= EC15_F6_DECAY_COEFFICIENT;
        }
    }
    // --- Branch 3: inactive edge-absorb → zero ---
    else
    {
        OutBody = 0.0f;
        OutHead = 0.0f;
        OutArm  = 0.0f;
    }

    // --- §5.1(a) Override fade-out contribution (additive on top of base branch) ---
    // Multiplier ramp per R11a-3 lockstep:
    //   ticks_remaining == 2 → multiplier 1.0 (first post-Override tick)
    //   ticks_remaining == 1 → multiplier 0.5 (second post-Override tick)
    //   ticks_remaining == 0 → no contribution
    // Decrement happens at end of block so one ComputeF6 call consumes one tick.
    if (f6_override_fadeout_ticks_remaining > 0)
    {
        const float multiplier = (f6_override_fadeout_ticks_remaining == 2) ? 1.0f : 0.5f;
        OutBody += f6_override_fadeout_snapshot_body * multiplier;
        OutHead += f6_override_fadeout_snapshot_head * multiplier;
        OutArm  += f6_override_fadeout_snapshot_arm  * multiplier;
        --f6_override_fadeout_ticks_remaining;
    }

    // --- Component-level clamp to ±(MAX_LEAN × 1.2) per TR-PM-033 ---
    const float max_clamp = MAX_LEAN_ANGLE_DEG * 1.2f;
    OutBody = FMath::Clamp(OutBody, -max_clamp, max_clamp);
    OutHead = FMath::Clamp(OutHead, -max_clamp, max_clamp);
    OutArm  = FMath::Clamp(OutArm,  -max_clamp, max_clamp);
}

// ---------------------------------------------------------------------------
// Story 008 — SnapToTargetAndReset (COMPLETE / ABORTED / COUNTDOWN helper).
//
// Snap pawn root to LaneWorldX(target_lane); reset mesh relative transforms to
// zero; reset F-2 tween state, F-5 lean, F-6 state to zero. Does NOT touch
// counters (COMPLETE/ABORTED preserve them; COUNTDOWN zeros them separately
// in HandleStateChanged). Does NOT call DiscardBuffer (caller invokes).
//
// TR-PM-018 (COMPLETE/ABORTED snap-and-reset behavior).
// ADR-0009 SD5 (collision commit via SetActorLocation on pawn root).
// FRotator Roll axis convention preserved (Story 006 correction).
// ---------------------------------------------------------------------------

void UPlayerLaneMovementComponent::SnapToTargetAndReset()
{
    // 1. Logical lane commit — current_lane == target_lane after snap.
    //    (Caller may have already set both to a specific value like Center for
    //    COUNTDOWN; this write is idempotent in that case.)
    current_lane = target_lane;

    // 2. Root at target lane world X. bSweep=false skips Chaos physics resolution
    //    (SD5 collision commit invariant).
    if (AActor* Owner = GetOwner())
    {
        Owner->SetActorLocation(
            FVector(LaneWorldX(target_lane), 0.0f, 0.0f),
            /*bSweep=*/false);
    }

    // 3. Mesh transforms at zero relative (both location AND rotation).
    //    Roll axis convention (Story 006 correction preserved).
    if (IsValid(CachedMeshComponent))
    {
        CachedMeshComponent->SetRelativeLocation(FVector::ZeroVector);
        CachedMeshComponent->SetRelativeRotation(FRotator::ZeroRotator);
    }

    // 4. Public property mirror of mesh world position.
    lateral_world_position = LaneWorldX(target_lane);

    // 5. Reset F-2 tween state.
    tween_progress = 0.0f;
    movement_state = ERunSlipState::SETTLED;

    // 6. Reset F-5 lean angles (AC-16, AC-28, AC-32).
    lean_angle      = 0.0f;
    head_lean_angle = 0.0f;
    arm_lean_angle  = 0.0f;

    // 7. Reset F-6 state fully (AC-COUNTER-F6-RESET applies on COMPLETE/ABORTED/COUNTDOWN).
    edge_absorb_active                  = false;
    edge_absorb_progress                = 0.0f;
    edge_absorb_local_timer_s           = 0.0f;
    edge_absorb_sign                    = 0.0f;
    f6_override_fadeout_ticks_remaining = 0;
    f6_override_fadeout_snapshot_body   = 0.0f;
    f6_override_fadeout_snapshot_head   = 0.0f;
    f6_override_fadeout_snapshot_arm    = 0.0f;

    // 8. Reset Story 010 commitment-tell flash lifecycle state.
    // Story 008 already resets commitment_tell_fire_count on COUNTDOWN separately in
    // HandleStateChanged — this hook resets ONLY the flash lifecycle state (fields owned
    // by Story 010) so the next run's first commitment-tell writes cleanly.
    flash_hold_ticks_remaining = 0;
    flash_decay_active         = false;
    flash_decay_time_s         = 0.0f;
    sign_of_current_flash      = 0.0f;
    time_last_flash_zero_s     = -1000.0f; // Reset to sentinel so next run's first fire always renders.
    if (IsValid(MeshMaterialDynamic))
    {
        // Zero the material param so any lingering flash is cleared on reset.
        MeshMaterialDynamic->SetScalarParameterValue(TEXT("LeadingFaceFlash"), 0.0f);
#if WITH_DEV_AUTOMATION_TESTS
        ++CommitmentTellFlashWrite_TestOnlyCallCount;
#endif
    }
}

// ---------------------------------------------------------------------------
// Story 008 — GetMovementStateExternal (AC-SS-B Seam 12 accessor).
//
// Converts internal ERunSlipState to the ordinal-locked Seam 12 EMovementState
// (defined in Seam/PlayerMovementProvider.h). Ordinals must match per the
// lockstep note in ERunSlipState.h:5.
//
// AC-SS-B: default: branch returns EMovementState::SETTLED as a safe fallback
// for any corrupt ordinal. This is the required grep-verifiable switch site.
// ---------------------------------------------------------------------------

EMovementState UPlayerLaneMovementComponent::GetMovementStateExternal() const
{
    switch (movement_state)
    {
    case ERunSlipState::SETTLED:  return EMovementState::SETTLED;
    case ERunSlipState::SLIPPING: return EMovementState::SLIPPING;
    default:                       return EMovementState::SETTLED; // AC-SS-B safe fallback
    }
}

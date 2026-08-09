# ADR-0006: Pull-Wave Instanced Renderer — Plain ISMC for Wave-Mass + Trail-Cube

## Status

Accepted

## Date

2026-06-24

## Last Verified

2026-07-08 (SPIKE-NOTE evidence folded — iPhone 17 flagship perf spike executed on Xcode simulator; CPU sub-budget claim validated decisively (0.01–0.06 ms measured vs 0.25 ms budget across every reading — Risk 8 Notes); 60fps + thermal claims validated *partially* (60fps observed at ceiling with variance on flagship; thermal cascade did not measurably stress workload due to Xcode simulator limitation); mid-tier + physical thermal follow-up REQUIRED before Sprint 1 gate can clear per Migration Plan §Follow-up. See `prototypes/pull-wave-perf-spike-2026-07-03/SPIKE-NOTE.md` and `architecture-review-2026-07-08.md` INT-008 for the full evidence chain. Prior amendment INT-001 (shared-host topology binding, 2026-06-26) remains authoritative — this stamp bump adds evidence, not architectural change.)

## Decision Makers

- **technical-director** — Decision adjudication and final authority (per Pull-Wave GDD line 1034 and `.claude/docs/coordination-rules.md` §1 vertical delegation)
- **performance-analyst** — R8 B-3 starting input (mobile-tile-GPU cull-tree analysis; 0.05–0.2 ms/tick HISM rebuild estimate against AC-PW-22a's 0.25 ms Pull-Wave per-tick budget)
- **unreal-specialist** — Engine specialist validation (Step 5.5 gate; CONCERNS verdict applied: `bTeleport=true` Decision change; risks 1, 3–7 + verifications 6–9 folded in); INT-001 shared-host topology surfacing (architecture-review 2026-06-25)
- **creative-director** — R8 CD synthesis ratifying perf-analyst B-3 promotion of OQ-PW-3 from open question to REQUIRED ADR (2026-06-11)
- **architecture-review 2026-06-25 (INT-001)** — surfaced shared-host topology as load-bearing architectural constraint (not implementation detail); amendment folded into Decision sub-question (f), Architecture, Validation Criteria, and GDD Requirements row at line 549

## Summary

SLIPSTORM Pull-Wave rendering uses two `UInstancedStaticMeshComponent` (plain ISMC)
components — `WaveMassISMC` (16 instances at PEAK) and `TrailCubeISMC` (48 instances
at PEAK). Plain ISMC is chosen over `UHierarchicalInstancedStaticMeshComponent`
(HISM) on all mobile platform tiers, with no platform-tier conditional logic. HISM's
hierarchical cull-tree rebuild would consume 20–80% of the AC-PW-22a 0.25 ms total
Pull-Wave per-tick budget while producing zero rendering benefit on a fixed-camera
5 m-wide mobile track scene where essentially every active instance is inside the
frustum every frame. `PerInstanceCustomData` slot counts are 3 on wave-mass + 1 on
trail-cube; cull-distance end bound is 35 m on both components.

## Engine Compatibility

| Field | Value |
|-------|-------|
| **Engine** | Unreal Engine 5.7 |
| **Domain** | Rendering (Mobile Forward) |
| **Knowledge Risk** | HIGH — UE 5.4–5.7 are post-LLM-cutoff (May 2025); ISMC API surface has been stable since UE 4.x but mobile-forward + per-instance-custom-data routing on UE 5.7 has not been independently verified against source by this project |
| **References Consulted** | `docs/engine-reference/unreal/VERSION.md`, `docs/engine-reference/unreal/breaking-changes.md`, `docs/engine-reference/unreal/deprecated-apis.md`, `docs/engine-reference/unreal/current-best-practices.md`, `docs/engine-reference/unreal/modules/rendering.md` |
| **Post-Cutoff APIs Used** | `UInstancedStaticMeshComponent::AddInstance` / `RemoveInstance` / `UpdateInstanceTransform` / `SetCustomDataValue` / `SetNumCustomDataFloats` / `SetCullDistances` (long-standing APIs; need UE 5.7 source confirmation that signatures and re-indexing semantics are unchanged from UE 5.3); `TObjectPtr<T>` (GC-safe pointer, UE 5.0+); potentially `BatchUpdateInstancesTransforms` / `RemoveInstances` plural overloads (existence + recommendation on UE 5.7 unknown — see Verifications 6 + 8) |
| **Verification Required** | (1) `UInstancedStaticMeshComponent::AddInstance` / `RemoveInstance` / `RemoveInstances` / `UpdateInstanceTransform` signatures and re-indexing semantics unchanged from UE 5.3 (specifically: `RemoveInstance` still swap-removes by index). (2) `SetCustomDataValue(InstanceIndex, DataIndex, Value, bMarkRenderStateDirty)` exists and `bMarkRenderStateDirty=false` overload semantics on Mobile Forward are unchanged. (3) `SetNumCustomDataFloats(N)` callable post-construction and propagates correctly to the mobile material's `GetPerInstanceCustomData(N)` node under Mobile Forward. (4) `SetCullDistances(Start, End)` honored under Mobile Forward (historically yes — confirm UE 5.7 did not gate it behind a renderer setting). (5) `bDisableCollision` / `NoCollision` on a component with zero physics bodies produces a no-op cost path (no PhysX/Chaos scene allocations per instance). (6) Confirm `BatchUpdateInstancesTransforms` (or equivalent plural-form batch API) existence and recommendation in UE 5.7; if present and preferred, the per-instance `UpdateInstanceTransform` + single `MarkRenderStateDirty()` pattern below may be superseded. (7) Confirm ISMC physics-body update synchronization with render transform on UE 5.7 mobile Chaos under `QueryOnly` collision — visual and overlap-query position must not desync by a frame. (8) Confirm `RemoveInstances(TArray<int32>)` plural overload exists in UE 5.7 and handles multi-index removal atomically with correct re-indexing. (9) Confirm `bTeleport=true` suppresses motion vectors on Mobile Forward and is safe for `QueryOnly` overlap detection at expected wave velocities (4–18 m/s). |

> **Note**: Knowledge Risk is HIGH. ISMC vs HISM is a long-stable choice but the
> mobile-forward + per-instance-custom-data + Chaos-physics-sync surface on UE 5.7
> has not been independently verified. This ADR must be re-validated if the project
> upgrades engine versions; flag as Superseded and author a new ADR on upgrade.

## ADR Dependencies

| Field | Value |
|-------|-------|
| **Depends On** | None — foundational renderer decision; no upstream ADR |
| **Enables** | AC-PW-22a (Pull-Wave per-tick CPU budget 0.25 ms — HISM cull-tree rebuild avoided); R1 RC-G G-2 binding (single batched draw call per ISMC); R7 B9 binding (wave-mass + trail-cube as separate ISMCs = 2 batched draw calls total); OQ-PW-3 closure; Pull-Wave Epic story-Done overall |
| **Blocks** | Pull-Wave Epic implementation — no wave-renderer story may enter implementation until this ADR is Accepted |
| **Ordering Note** | Sibling of ADR-0005 (Wave Spawner subsystem hosting) — class choice is orthogonal to subsystem hosting; component-ownership topology is coupled (Decision sub-question (f) — INT-001 binding): Wave Spawner owns the `AWave` actor pool (state tokens), and the renderer components live on a separate singleton `APullWaveSubsystemActor` per sub-question (f). |

## Context

### Problem Statement

Pull-Wave needs two batched render paths on mobile forward — one for the wave-mass
(an 8–14 voxel irregular cluster, up to 16 simultaneous instances per `MAX_CONCURRENT_WAVES_CAP`)
and one for the trail cubes (axis-aligned 1-unit cubes, 3 per wave = up to 48 trail-cube
instances at PEAK per R7 B9 ISMC separation). Pull-Wave GDD R1 RC-G G-2 binds these
to "Instanced Static Mesh Component (HISM or ISMC)" but does not specify which class.
R8 perf-analyst B-3 promoted the open question (OQ-PW-3) to a REQUIRED ADR after
determining that the wrong choice (HISM on a no-occlusion mobile scene) costs
0.05–0.2 ms per tick — 20–80% of the AC-PW-22a Pull-Wave 0.25 ms per-tick budget —
with no rendering benefit.

The choice is binding and must be made before Pull-Wave wave-renderer implementation
begins; reversing it post-implementation would invalidate the material PerInstanceCustomData
routing, the per-tick update path, the cull-distance configuration, and any tick-budget
measurements taken under the wrong component class.

### Current State

No Pull-Wave wave-renderer implementation exists at this decision point. The Pull-Wave
GDD (`design/gdd/pull-wave-behavior.md`) is R14-closed (terminal cycle 2026-06-19);
the DPC GDD is R14-closed in the same cascade; the Wave Spawner GDD is R3a-closed
(2026-06-22). GDD line 1034 specifies the 5 sub-questions this ADR must address.
ADR-0005 (Wave Spawner subsystem hosting) was accepted 2026-06-24 — sibling decision,
independent.

### Constraints

- **Mobile platform, 60 fps, 16.6 ms frame budget** — per `.claude/docs/technical-preferences.md`:
  target 60 fps sustained on mid-tier mobile; ~100 draw calls per frame budget;
  1.5 GB memory ceiling; Mobile Forward renderer; Lumen + Nanite + Substrate all
  DISABLED on mobile target.
- **AC-PW-22a 0.25 ms Pull-Wave per-tick budget** — every per-tick CPU cost on the
  Pull-Wave path must fit within this budget. HISM cull-tree rebuild at 16 + 48 = 64
  moving instances per tick is estimated at 0.05–0.2 ms — 20–80% of the entire
  budget — by R8 perf-analyst.
- **Fixed camera** — no orbit, no free look. The camera is pointed straight down a
  5 m-wide track; nothing dynamically occludes the wave-mass except the player,
  which is too small to drive a cull-tree.
- **Wave Z-distance band** — waves spawn at `SPAWN_PLANE_Z_OFFSET_M` (default 15.0 m,
  safe range `[10.0, 25.0]` m per `design/registry/entities.yaml`); they translate
  toward the player plane at constant velocity per Rule 6; they despawn shortly
  after the player plane (`WAVE_DESPAWN_HOLD_S = 0.15 s` default). Practical Z-band:
  ~0 m to ~25 m.
- **Concurrency caps** — `MAX_CONCURRENT_WAVES_CAP = 16` covers LEANING + TRAVERSING +
  LANDED states per Rule 12. Trail-cube count is `16 × 3 = 48` per R7 B9.
- **PerInstanceCustomData slot count is locked by GDD** — wave-mass has 3 scalar
  slots (R1 RC-G G-2); trail-cube has 1 scalar slot (R7 B9 / R8 perf-analyst R-1
  drift correction).
- **`MaterialInstanceDynamic` per wave is forbidden** — would explode to ≈128–224
  draw calls on mobile, far exceeding the ~100 mobile draw call budget.

### Requirements

- Two batched render paths producing **2 batched draw calls total** at PEAK density
  (1 per ISMC, per R7 B9).
- Per-instance scalar parameters routed exclusively through `PerInstanceCustomData`
  (3 slots on wave-mass; 1 slot on trail-cube).
- Per-tick update path that fits within AC-PW-22a's 0.25 ms total Pull-Wave per-tick
  CPU budget.
- Cull-distance bound that covers the full wave Z-band with a safety margin.
- Implementation that does not introduce a per-tier branch (single component class
  for all supported tiers) unless the platform-tier benefit is demonstrated.

## Decision

**Adopt plain `UInstancedStaticMeshComponent` (ISMC) for both Pull-Wave render
components, on all mobile platform tiers (no HISM, no platform-tier conditional).**
The performance-analyst R8 B-3 recommendation is accepted in full. The rationale,
sub-question answers, and required UE 5.7 source verifications follow.

**Sub-question (a) — Wave-mass renderer class.** `UInstancedStaticMeshComponent`
(plain ISMC), one component per Pull-Wave subsystem actor, holding up to
`MAX_CONCURRENT_WAVES_CAP = 16` instances spanning LEANING + TRAVERSING + LANDED
states. Component name: `WaveMassISMC`. Instance mesh: the irregular 8–14 voxel
cluster within the 3×3×3 bounding box (single shared `UStaticMesh` asset).

**Sub-question (b) — Trail-cube renderer class.** `UInstancedStaticMeshComponent`
(plain ISMC), one component on the same actor, holding up to 48 instances
(16 active waves × 3 trail-ghost cubes). Component name: `TrailCubeISMC`. Instance
mesh: a unit cube (axis-aligned). Trail-cube slot allocation is wave-bound per
R7 B9 — when a wave releases, its 3 trail-cube slots are returned to the free list
as a contiguous block.

> **GDD-to-ADR naming mapping**: The Pull-Wave GDD prose at lines 658, 756, and 1034
> uses the backticked notation `` `TrailCube_ISMC` `` (with underscore) when describing
> this component. The ADR's C++ UPROPERTY member name is `TrailCubeISMC` (PascalCase
> no underscore) per UE naming convention (matches `WaveMassISMC`, `UWaveSpawnerSubsystem`,
> etc.). Both refer to the same component; the underscore in the GDD prose was
> stylistic notation, not a literal UE identifier requirement. A `/propagate-design-change`
> pass should normalize the GDD prose to `TrailCubeISMC` for consistency with the
> implemented identifier.

**Sub-question (c) — Platform-tier conditional logic.** **None.** Plain ISMC on
every supported tier (iOS Metal tiers, Android Vulkan tier-2 and tier-3, Android
GLES tier-1 fallback). The mobile-forward, fixed-camera, 5 m-wide track scene
removes every justification for HISM regardless of GPU tier: hierarchical frustum
culling has no work to do when essentially every active instance is inside the
camera frustum every frame (waves only exist within the ~0 m to ~25 m Z-band the
camera is pointed at), and no large static scene exists for HISM's octree to
bisect. Branching by tier would double verification surface, force two implementations
of the per-instance update path, and add maintenance debt with no measurable win.
If a future feature (e.g., a "spectator camera" mode for trailers, or a wider
arena variant) ever introduces dynamic occlusion or an order-of-magnitude larger
instance count, the decision can be revisited via a superseding ADR; today's scene
does not motivate it.

**Sub-question (d) — PerInstanceCustomData slot counts.** Affirmed per GDD locks:

- `WaveMassISMC.SetNumCustomDataFloats(3)` — slot 0 `LeanChargeIntensity`
  (R1 RC-G G-2, drives specular ramp 1.7–2.2× base during LEANING), slot 1
  `NearMissEdgeFlash` (66 ms time-decayed chromatic edge flash), slot 2
  `VoxelDissolveFade` (despawn dissolve).
- `TrailCubeISMC.SetNumCustomDataFloats(1)` — slot 0 `TrailAlpha` (cutout threshold
  for ghost-cube fade, uniform during TRAVERSING).

Both components share their material with all instances of the same component;
per-wave variation routes exclusively through `GetPerInstanceCustomData(N)` in the
material graph. `UMaterialInstanceDynamic` per-wave is forbidden — it would
explode draw calls to ≈128–224, an order of magnitude over the ~100 draw call
mobile budget.

**Sub-question (e) — Cull-distance bound.** `SetCullDistances(0, 3500)` on both
components (units = cm; EndCullDistance = 35 m). The 35 m end bound covers the
`SPAWN_PLANE_Z_OFFSET_M` safe range upper of 25.0 m with a 10 m safety margin —
generous against any future spawn-plane tuning within or marginally beyond the
safe range. StartCullDistance = 0 (no near-fade): voxel pop-out within the visible
play field would be more jarring to the player than the negligible cost of
rendering 64 small instances that are about to despawn anyway.
`SetCollisionEnabled(ECollisionEnabled::NoCollision)` on the trail-cube component
(trail cubes are pure visual; collision is owned by `WaveMassISMC`).

**Sub-question (f) — Component ownership topology (INT-001 binding, added by
amendment 2026-06-26).** Both ISMCs live on a single shared singleton actor,
`APullWaveSubsystemActor`, which is independent of the 23-actor `AWave` pool
declared by ADR-0005. There is exactly one `APullWaveSubsystemActor` per
`UWorld`. Per-wave runtime state (`FPullWaveRuntimeState`) is owned in a
`TArray<FPullWaveRuntimeState> ActiveWaves` on the singleton — NOT on individual
`AWave` actors. Each `FPullWaveRuntimeState` carries its `WaveMassInstanceIndex`
into the singleton's `WaveMassISMC` and its three `TrailCubeInstanceIndices` into
the singleton's `TrailCubeISMC`; this index pair is the runtime identity of the
wave for the renderer. The pooled `AWave` actors from ADR-0005 either become
lightweight state tokens carrying only the per-wave fields the gameplay code
references (lane, phase, timestamps) or — if no per-actor seam is required by a
later system — their fields are absorbed into `FPullWaveRuntimeState` and the
`AWave` pool is eliminated. ADR-0005's pool sizing (23) remains correct in either
case (it is a budget for actor-shaped state-token leases, not for renderer
components). **This sub-question is load-bearing**: the alternative topology of
one ISMC pair per pooled `AWave` actor would produce 23 separate
`WaveMassISMC` instances and 23 separate `TrailCubeISMC` instances, each emitting
its own draw call, yielding 32 draw calls at PEAK (16 active × 2 components) —
not the 2 batched draw calls the R1 RC-G G-2 + R7 B9 bindings require. The
"2 batched draw calls at PEAK" claim in this ADR is structurally true only under
the shared-singleton-host topology. The pre-amendment text at the GDD Requirements
Addressed row for wave-spawner-pattern-library line 157 incorrectly labelled
component ownership as a per-instance pool implementation detail; that label was
withdrawn by the architecture-review 2026-06-25 INT-001 finding and is corrected
at that row.

**Adjudication of perf-analyst R8 B-3.** Accepted in full. The cull-tree rebuild
cost estimate of 0.05–0.2 ms per tick on mobile against an AC-PW-22a 0.25 ms total
Pull-Wave tick budget is the load-bearing constraint: HISM would consume 20–80%
of the entire Pull-Wave per-tick budget for a tree that has no occluders to cull
against. HISM's hierarchical frustum culling win materializes when (i) thousands
of static-position instances exist, (ii) most are off-screen most of the time, and
(iii) instances do not move tick-to-tick (so the tree is built once). The Pull-Wave
scene fails all three: 16 wave-mass instances (not thousands), nearly all on-screen
by design (the camera looks straight down the track), and every TRAVERSING instance
moves every tick (forcing tree updates). Plain ISMC's flat per-instance frustum
check is the cheaper algorithm for this shape of problem. The 0.05–0.2 ms estimate
is empirically defensible: HISM tree rebuilds touch every dirty instance plus
log(N) ancestor nodes; for 16 moving instances on a mobile CPU, sub-millisecond
is right, and that cost is non-trivial relative to the budget.

**UE 5.7 source verifications required before implementation.** UE 5.7 is
post-LLM-cutoff (May 2025) and the engine-reference breaking-changes log does
not flag movement on ISMC semantics. The 9 verifications listed in Engine
Compatibility above must be confirmed against the pinned UE 5.7 source under
`Engine/Source/Runtime/Engine/Private/InstancedStaticMesh.cpp` and related headers
before the wave-renderer epic opens. Items 1–5 are TD's; items 6–9 are engine
specialist additions covering batch-update API existence (item 6), physics-body
sync under `QueryOnly` (item 7), plural `RemoveInstances` overload (item 8),
and `bTeleport=true` motion-vector suppression on Mobile Forward (item 9).

### Architecture

```
APullWaveSubsystemActor  [singleton — exactly one per UWorld; INT-001 sub-question (f)]
                                            Mobile Forward, 60 fps, 16.6 ms frame
├── WaveMassISMC : UInstancedStaticMeshComponent
│     • Mesh: SM_WaveMass_VoxelCluster (8–14 voxels, 3×3×3 bbox)
│     • Instances at PEAK: 16  (LEANING + TRAVERSING + LANDED per Rule 12)
│     • CustomDataFloats: 3
│     │     [0] LeanChargeIntensity   → specular ramp 1.7–2.2× base (LEANING)
│     │     [1] NearMissEdgeFlash     → chromatic edge (66 ms decay)
│     │     [2] VoxelDissolveFade     → despawn dissolve
│     • CullDistances: (0, 3500 cm)        — 35 m end (10 m past safe upper 25 m)
│     • Mobility: Movable
│     • CastShadow: false                  — Mobile Forward, no shadow cost
│     • Collision: QueryOnly               — owned overlaps; physics-body sync see Risk 1
│     • Draw calls: 1 (single batched ISMC draw)
│
├── TrailCubeISMC : UInstancedStaticMeshComponent
│     • Mesh: SM_TrailCube_Unit (axis-aligned 1u cube)
│     • Instances at PEAK: 48  (16 waves × 3 ghost cubes, wave-bound slots per R7 B9)
│     • CustomDataFloats: 1
│     │     [0] TrailAlpha            → cutout threshold (uniform during TRAVERSING)
│     • CullDistances: (0, 3500 cm)
│     • Mobility: Movable
│     • CastShadow: false
│     • Collision: NoCollision              — pure visual; zero-cost guarantee per Risk 5
│     • Draw calls: 1 (single batched ISMC draw)
│
└── ActiveWaves : TArray<FPullWaveRuntimeState>
     • Per-wave runtime state lives HERE on the singleton (INT-001 sub-question (f))
     • Each entry carries WaveMassInstanceIndex + TrailCubeInstanceIndices[3]
       — the index pair is the renderer-side identity of the wave
     • NOT owned per-AWave-actor — ADR-0005's pooled AWave actors are state
       tokens (or eliminated; their per-wave fields may be absorbed here)
     • SetCustomDataValue called per dirty wave per tick;
       MarkRenderStateDirty() once at end-of-tick per component to amortize
       render-state flush cost

TOTAL Pull-Wave draw calls at PEAK: 2  (of ~100 mobile budget)
  Structurally requires shared-singleton-host topology (sub-question (f));
  per-AWave-actor ownership would yield 32 draw calls at PEAK (16 × 2).
TOTAL Pull-Wave per-tick CPU budget: 0.25 ms  (AC-PW-22a)
   HISM cull-tree rebuild avoided: 0.05–0.2 ms reclaimed for gameplay logic
```

### Key Interfaces

```cpp
// PullWaveSubsystemActor.h  (excerpt — illustrative, not exhaustive)

/**
 * Singleton actor owning the shared Pull-Wave renderer components and per-wave
 * runtime state. Exactly one instance per UWorld (INT-001 sub-question (f)).
 * Independent of ADR-0005's 23-actor AWave pool — the AWave actors are state
 * tokens (or eliminated); their renderer indices live in ActiveWaves below.
 * The "2 batched draw calls at PEAK" guarantee depends on this topology.
 */
UCLASS()
class SLIPSTORM_API APullWaveSubsystemActor : public AActor
{
    GENERATED_BODY()

public:
    APullWaveSubsystemActor();

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    /** Batched wave-mass renderer. 16 instances at PEAK. 3 custom-data slots.
     *  Owned by THIS singleton — declaring this UPROPERTY on AWave or any
     *  per-wave class is the per_AWave_actor_render_component_ownership
     *  forbidden pattern (registry). */
    UPROPERTY(VisibleAnywhere, Category = "PullWave|Render")
    TObjectPtr<UInstancedStaticMeshComponent> WaveMassISMC;

    /** Batched trail-cube renderer. 48 instances at PEAK. 1 custom-data slot.
     *  Owned by THIS singleton (same constraint as WaveMassISMC).
     *  GDD prose calls this `TrailCube_ISMC`; the UE identifier is `TrailCubeISMC`
     *  per PascalCase convention. */
    UPROPERTY(VisibleAnywhere, Category = "PullWave|Render")
    TObjectPtr<UInstancedStaticMeshComponent> TrailCubeISMC;

    /** Per-wave runtime state — including renderer instance indices. Owned HERE
     *  on the singleton, NOT on pooled AWave actors (INT-001 sub-question (f)). */
    UPROPERTY()
    TArray<FPullWaveRuntimeState> ActiveWaves;

    /** Custom-data slot indices — keep in sync with material graph. */
    static constexpr int32 WaveMass_Slot_LeanCharge    = 0;
    static constexpr int32 WaveMass_Slot_NearMissFlash = 1;
    static constexpr int32 WaveMass_Slot_VoxelDissolve = 2;
    static constexpr int32 TrailCube_Slot_TrailAlpha   = 0;

    /** Cull distance bound (cm). 35 m end > 25 m SPAWN_PLANE_Z_OFFSET_M safe upper. */
    static constexpr float kPullWaveCullEnd_cm = 3500.f;
};

// PullWaveSubsystemActor.cpp

APullWaveSubsystemActor::APullWaveSubsystemActor()
{
    PrimaryActorTick.bCanEverTick = true;

    // SetNumCustomDataFloats(N) MUST be called once in the constructor, before any
    // AddInstance() call. Calling per-instance would incur buffer resize overhead.
    WaveMassISMC = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("WaveMassISMC"));
    WaveMassISMC->SetupAttachment(RootComponent);
    WaveMassISMC->SetMobility(EComponentMobility::Movable);
    WaveMassISMC->SetNumCustomDataFloats(3);                       // (d)
    WaveMassISMC->SetCullDistances(0, kPullWaveCullEnd_cm);        // (e)
    WaveMassISMC->SetCastShadow(false);                            // Mobile Forward — no shadow cost
    WaveMassISMC->SetCollisionEnabled(ECollisionEnabled::QueryOnly); // owned overlaps; see Risk 1 / Verification 7

    TrailCubeISMC = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("TrailCubeISMC"));
    TrailCubeISMC->SetupAttachment(RootComponent);
    TrailCubeISMC->SetMobility(EComponentMobility::Movable);
    TrailCubeISMC->SetNumCustomDataFloats(1);                      // (d)
    TrailCubeISMC->SetCullDistances(0, kPullWaveCullEnd_cm);       // (e)
    TrailCubeISMC->SetCastShadow(false);
    TrailCubeISMC->SetCollisionEnabled(ECollisionEnabled::NoCollision); // pure visual
}

// Per-tick update path (illustrative).
//
// IMPORTANT — bTeleport=true (engine specialist Risk 2): Mobile Forward has TAA
// and motion blur disabled per technical-preferences. Passing bTeleport=false
// generates motion vectors that are never consumed by any post-process effect on
// mobile, wasting per-update CPU. Use bTeleport=true for constant-velocity wave
// updates. See Verification 9 for the safety check under QueryOnly overlap.
//
// PerInstanceCustomData writes pass bMarkRenderStateDirty=false; we
// MarkRenderStateDirty() once at end-of-tick per component to amortize the
// render-state flush cost against the 0.25 ms AC-PW-22a tick budget.
// If UE 5.7 introduces BatchUpdateInstancesTransforms or equivalent (see
// Verification 6), this loop should be migrated to the plural form.

void APullWaveSubsystemActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    bool bWaveMassDirty  = false;
    bool bTrailCubeDirty = false;

    for (FPullWaveRuntimeState& Wave : ActiveWaves)
    {
        // Transform updates for TRAVERSING waves (Rule 6 constant velocity).
        const int32 InstanceIdx = Wave.WaveMassInstanceIndex;
        if (InstanceIdx == INDEX_NONE) { continue; }  // guard against cached sentinel
        WaveMassISMC->UpdateInstanceTransform(
            InstanceIdx,
            Wave.CurrentTransform,
            /*bWorldSpace=*/true,
            /*bMarkRenderStateDirty=*/false,
            /*bTeleport=*/true);                       // mobile forward: motion vectors unused

        // Per-instance custom data (LeanCharge / NearMissFlash / VoxelDissolve).
        WaveMassISMC->SetCustomDataValue(InstanceIdx, WaveMass_Slot_LeanCharge,
            Wave.LeanChargeIntensity, /*bMarkRenderStateDirty=*/false);
        WaveMassISMC->SetCustomDataValue(InstanceIdx, WaveMass_Slot_NearMissFlash,
            Wave.NearMissEdgeFlash, /*bMarkRenderStateDirty=*/false);
        WaveMassISMC->SetCustomDataValue(InstanceIdx, WaveMass_Slot_VoxelDissolve,
            Wave.VoxelDissolveFade, /*bMarkRenderStateDirty=*/false);
        bWaveMassDirty = true;

        // Trail cubes — 3 slots per wave (R7 B9 wave-bound).
        for (int32 t = 0; t < 3; ++t)
        {
            const int32 TrailIdx = Wave.TrailCubeInstanceIndices[t];
            if (TrailIdx == INDEX_NONE) { continue; }
            TrailCubeISMC->UpdateInstanceTransform(
                TrailIdx, Wave.TrailTransforms[t],
                /*bWorldSpace=*/true, /*bMarkRenderStateDirty=*/false, /*bTeleport=*/true);
            TrailCubeISMC->SetCustomDataValue(TrailIdx, TrailCube_Slot_TrailAlpha,
                Wave.TrailAlpha, /*bMarkRenderStateDirty=*/false);
            bTrailCubeDirty = true;
        }
    }

    if (bWaveMassDirty)  { WaveMassISMC->MarkRenderStateDirty(); }
    if (bTrailCubeDirty) { TrailCubeISMC->MarkRenderStateDirty(); }
}

// Spawn / despawn (illustrative — wave-bound slot allocation per R7 B9):
//
//   const int32 NewIdx = WaveMassISMC->AddInstance(SpawnXform, /*bWorldSpace=*/true);
//   Wave.WaveMassInstanceIndex = NewIdx;
//   for (int32 t = 0; t < 3; ++t)
//       Wave.TrailCubeInstanceIndices[t] = TrailCubeISMC->AddInstance(InitialTrailXform[t], true);
//
// On despawn: RemoveInstance() swap-removes; patch the cached InstanceIndex of
// whichever wave got swapped into the freed slot (verify swap-remove semantics
// under UE 5.7 per Verification 1).
//
// Multi-wave same-tick despawn (engine specialist Risk 3): either (a) sort indices
// descending before sequential RemoveInstance to avoid cascading swap re-indexing,
// or (b) use the plural RemoveInstances(TArray<int32>) overload if confirmed
// available in UE 5.7 (Verification 8). Naive sequential RemoveInstance on stale
// cached indices will remove the wrong instance.
//
// Uninitialized sentinel: cache instance indices as int32 initialized to INDEX_NONE
// (-1). Guard all RemoveInstance / UpdateInstanceTransform call sites against
// INDEX_NONE input.
```

## Alternatives Considered

### Alternative 1: Plain ISMC for both components

- **Description**: `UInstancedStaticMeshComponent` for `WaveMassISMC` (16 instances) and `TrailCubeISMC` (48 instances). Flat per-instance frustum culling, no hierarchical cull tree. `SetCullDistances(0, 3500)` on both. PerInstanceCustomData slots as locked in GDD (3 + 1). Single batched draw call per component (2 total Pull-Wave draw calls).
- **Pros**:
  - Zero cull-tree rebuild cost — reclaims 0.05–0.2 ms/tick (20–80% of the AC-PW-22a 0.25 ms Pull-Wave tick budget) versus HISM with no rendering loss in this scene.
  - Simplest implementation: one component class, one update path, no platform-branching code paths to maintain or test.
  - Direct, well-understood `UpdateInstanceTransform` + `SetCustomDataValue` API surface; least UE-5.7-source verification risk because the API hasn't moved since UE 4.x.
  - Naturally fits the 2-draw-call PEAK target (well within the ~100 mobile draw call budget).
  - Lowest CPU memory overhead — no per-component octree allocation.
- **Cons**:
  - Per-instance frustum culling is O(N) per frame on the render thread; at N = 64 (16 + 48) this is negligible but would not scale to thousands. Not a real con at the GDD-locked concurrency caps.
  - If a future feature ever expands the play field to a curved/wider track with dynamic occlusion, this decision will need to be revisited via a superseding ADR. Acceptable cost given today's fixed-camera straight track.
- **Chosen** — see Decision section above for rationale.

### Alternative 2: HISM for both components

- **Description**: `UHierarchicalInstancedStaticMeshComponent` for both `WaveMassISMC` and `TrailCubeISMC`. Engine maintains a per-component octree of instance bounds; hierarchical frustum culling skips entire subtrees off-screen. Same PerInstanceCustomData layout (3 + 1) and same cull-distance bound.
- **Pros**:
  - Hierarchical frustum culling is genuinely cheaper than flat O(N) when N is large and many instances are off-screen — the canonical foliage case (thousands of trees in an open world).
  - Cull tree is preserved as a building block for future scaling if instance counts ever grew by an order of magnitude.
  - Same API surface as ISMC for AddInstance / UpdateInstanceTransform / SetCustomDataValue — drop-in upgrade path from ISMC if a later scene needs it.
- **Cons**:
  - Cull-tree rebuild cost on instance movement: per the R8 perf-analyst mobile-tile-GPU analysis, **0.05–0.2 ms/tick CPU** with 16 moving wave-mass instances. That is **20–80% of the entire 0.25 ms AC-PW-22a Pull-Wave tick budget** with zero rendering benefit.
  - No occluders exist in this scene to drive subtree-cull wins: the camera is fixed, the track is 5 m wide, and essentially every active instance is inside the frustum every frame (waves live in a ~0 m to ~25 m Z-band the camera looks straight down).
  - Trail cubes move every tick during TRAVERSING (Rule 6 constant velocity); a hierarchical tree over 48 instances that all move every frame is precisely the worst case for HISM — the tree rebuild touches almost every node.
  - Higher per-component memory footprint (octree nodes) for no benefit on a mobile 1.5 GB ceiling.
- **Rejection Reason**: HISM's hierarchical-cull benefit requires occluders and/or a large fraction of off-screen instances; the Pull-Wave scene has neither. The 0.05–0.2 ms/tick rebuild cost would consume up to 80% of the AC-PW-22a 0.25 ms tick budget for a tree with no work to do, directly threatening the per-tick budget.

### Alternative 3: Mixed (HISM for one component, ISMC for the other)

- **Description**: Two sub-variants. **3a** — HISM `WaveMassISMC` (16 instances) + plain ISMC `TrailCubeISMC` (48 instances), on the theory that wave-mass voxel clusters are larger/more expensive to render so its cull tree matters more. **3b** — plain ISMC `WaveMassISMC` + HISM `TrailCubeISMC`, on the theory that the larger 48-instance trail-cube count is where hierarchical culling pays off.
- **Pros**:
  - In principle could target HISM exactly where N is largest or per-instance cost is highest.
  - Each sub-variant keeps one component "advanced" while leaving the other simple.
- **Cons**:
  - Both sub-variants inherit Alternative 2's core flaw on the HISM half: no occluders, fixed camera, moving instances. 3a pays 0.05–0.2 ms/tick on 16 moving wave-mass instances; 3b pays a similar or higher cost on 48 moving trail-cube instances (more dirty leaves to rebuild per tick).
  - Doubles the maintenance and verification surface: two distinct component classes, two slightly different update code paths, two sets of UE 5.7 source verifications.
  - No coherent design story — the choice of which component "deserves" HISM is arbitrary given the scene analysis.
  - Provides no measurable benefit on either component in this scene.
- **Rejection Reason**: Mixing component classes adds complexity and verification burden while inheriting HISM's tick-budget cost on whichever component receives it. There is no scene-specific reason either component benefits from hierarchical culling.

### Alternative 4: Platform-tier conditional (HISM on Android Vulkan tier-3 if occlusion shows benefit at prototype, plain ISMC on tier-2 and iOS)

- **Description**: Construct `WaveMassISMC` and `TrailCubeISMC` as either HISM or ISMC at runtime based on `IConsoleManager` device-tier detection (e.g., `sg.MobileQualityLevel` or a custom device-tier probe). Tier-3 Vulkan Android devices would get HISM on the speculation that their higher CPU headroom and better driver could absorb cull-tree rebuild cost and benefit from any future occlusion features.
- **Pros**:
  - Hedges against the possibility that perf-analyst's 0.05–0.2 ms estimate is wrong on high-end Vulkan devices where the rebuild might be cheaper.
  - Preserves the option to upgrade tier-3 devices to "advanced" rendering paths in future without an ADR change.
- **Cons**:
  - Doubles the implementation, test matrix, and UE 5.7 verification surface (two component classes, two update paths, two sets of device-tier QA passes on both iOS and Android).
  - The HISM-benefit-on-tier-3 thesis has no supporting evidence: tier-3 devices share the same scene-level absence of occluders and the same fixed-camera frustum coverage; raising CPU headroom does not create cull-tree work that doesn't exist.
  - Introduces a runtime branch in a per-tick hot path that future-you (or a successor TD) will have to reason about for every Pull-Wave perf regression.
  - Tier-3 devices benefit most from reclaiming the 0.05–0.2 ms for richer effects, not from spending it on a no-op cull tree.
  - Premature hedging against an uncosted future feature ("if occlusion shows benefit at prototype") — that feature, if it ever ships, can motivate its own ADR.
- **Rejection Reason**: Platform-tier conditional logic adds permanent maintenance and verification cost to hedge against a benefit that the scene analysis (fixed camera, no occluders, all instances in frustum, all instances moving) says does not exist on any tier. Simpler to commit to plain ISMC across all tiers and revisit via superseding ADR if a future scene change actually motivates HISM.

## Consequences

### Positive

- **0.05–0.2 ms/tick reclaimed** from avoided HISM cull-tree rebuild — directly available for other Pull-Wave per-tick work within AC-PW-22a's 0.25 ms budget.
- **Simplest possible implementation surface** — one component class, one update path, no per-tier branching. Lowest verification + test surface.
- **Lowest UE 5.7 source-verification risk** — plain ISMC API has been stable since UE 4.x; mobile-forward + PerInstanceCustomData on UE 5.7 is the primary unknown (covered by Verifications 2–5).
- **2 batched draw calls at PEAK** — matches R1 RC-G G-2 + R7 B9 binding exactly; comfortably within the ~100 mobile draw call budget.
- **Lowest CPU memory footprint** — no per-component octree allocation.
- **Cull-distance bound (35 m) is conservative** — generous 10 m safety margin past `SPAWN_PLANE_Z_OFFSET_M` safe upper of 25 m; no pop-out within play field.

### Negative

- **No hierarchical cull-tree as a future-scaling option** — if a future scene grows instance counts by an order of magnitude or introduces dynamic occlusion, this decision must be revisited via a superseding ADR. Acceptable cost given the fixed-camera 5 m-wide-track scene that is the entire game.
- **`bTeleport=true` discards motion-vector information** — mobile forward has TAA + motion blur disabled, so motion vectors are unused; this is free CPU on mobile but would need to revert to `bTeleport=false` if any future post-process effect on mobile starts consuming them.
- **Manual swap-remove index-cache patching required** on `RemoveInstance` (and on the plural `RemoveInstances` overload if it exists per Verification 8) — implementer must handle the cascading-index case for multi-wave same-tick despawn (engine specialist Risk 3).
- **Single-instance API loop** instead of batched plural API (if `BatchUpdateInstancesTransforms` exists in UE 5.7 per Verification 6) — pattern is correct but may be redundant overhead; migrate to the plural form if confirmed available and preferred.

### Risks

**Risk 1 — ISMC overlap-event physics-body sync under `QueryOnly` (HIGH)**.
`WaveMassISMC` uses `SetCollisionEnabled(ECollisionEnabled::QueryOnly)` and expects
`OnComponentBeginOverlap` to fire per-instance with `OtherBodyIndex` carrying the
instance index. Correctness depends on whether `UpdateInstanceTransform` also
updates the underlying Chaos query body for that instance in the same tick. If
instance physics bodies lag the render transform by a frame under certain update
patterns, overlap detection desyncs from visual position. Verification 7 must
confirm this before implementation. If a lag is found, a separate `UpdateBounds()`
or equivalent call may be required after the batch transform update — keep this
as a fallback path in the implementation design.

**Risk 2 — `bTeleport=true` correctness for `QueryOnly` overlaps (LOW post-mitigation)**.
The Decision uses `bTeleport=true` for all wave position updates. If `bTeleport=true`
also affects physics body update semantics (e.g., bypasses Continuous Collision
Detection for that frame), it could miss fast-moving overlap events. At wave
velocities of 4–18 m/s on a 60 fps tick (waves move 0.07–0.30 m/frame), CCD
suppression is unlikely to cause missed overlaps for the player-plane crossing
detection — but Verification 9 must confirm.

**Risk 3 — Same-tick multi-despawn index cascading (MEDIUM)**.
Sequential `RemoveInstance` calls cascade index shifts: each call swap-removes
and may invalidate the next call's cached index. Implementer must either
(a) sort indices descending before sequential `RemoveInstance`, or (b) use the
plural `RemoveInstances(TArray<int32>)` overload if available (Verification 8).
The Key Interfaces code block documents this requirement at the despawn comment
block. Naive sequential `RemoveInstance` on stale cached indices will remove the
wrong instance — this MUST be addressed at implementation time.

**Risk 4 — `QueryOnly` vs `ProbeOnly` enum confusion (LOW)**.
`ECollisionEnabled::QueryOnly` is correct for "overlap events fire, no physics
simulation." `ECollisionEnabled::ProbeOnly` is a distinct newer enum value
(added in UE 5.x) for a lighter-weight probe-event path that does not generate
full overlap events. The Decision's choice is correct; this risk is documented
to prevent a future migration from accidentally moving to `ProbeOnly` without
re-validating overlap delegate semantics.

**Risk 5 — `NoCollision` zero-cost guarantee on `TrailCubeISMC` (LOW)**.
The Decision assumes `SetCollisionEnabled(ECollisionEnabled::NoCollision)` produces
no Chaos scene allocations per instance. Expected behavior across UE 5.x but not
independently confirmed for UE 5.7 ISMC. Verification 5 covers this.

**Risk 6 — Mobile GPU buffer upload pattern under Vulkan vs Metal (LOW)**.
UE 5.4–5.7 may have revised how ISMC instance buffers are uploaded to the GPU on
mobile. At N = 16 / N = 48, per-tick dirty upload is within any reasonable budget;
no design revision required. Flag for profiling during prototype.

**Risk 7 — Mobile Forward shader permutation cost for 3-slot PerInstanceCustomData (LOW)**.
Three `PerInstanceCustomData` slots on the wave-mass material expand per-instance
interpolant count, which can increase shader register pressure on mobile backends.
Three slots is within normal limits, but the wave-mass material's instruction count
must be validated in Shader Complexity view before Alpha. Not a design blocker.

**Risk 8 — iPhone 17 Nominal-thermal 60fps observed at ceiling with variance (MEDIUM; opened 2026-07-08 by SPIKE-NOTE)**.
The 2026-07-08 perf spike measured Frame time at Nominal thermal on iPhone 17 flagship
hardware and observed two readings at the same simulated thermal state: 16.6 ms
(V-sync locked at 60fps) and 18 ms (~55 fps, over budget). GPU cost measured at
7–11 ms range. Comfort headroom on flagship silicon is not confirmed. On the mid-tier
target (iPhone 12/13/SE class per `technical-preferences.md`) — untested — 60fps is
likely to be marginal or fail without further mitigation. The spike did NOT invalidate
this ADR (CPU sub-budget claim validated decisively; ~1 ms of the 7–11 ms GPU cost is
attributable to Pull-Wave, the remainder is scene shell), but it flagged that the
mobile-forward 60fps promise for total-scene rendering is at the edge on flagship.
Xcode `Simulate → Thermal State` was not a valid stressor (Serious GPU < Nominal GPU
in the cascade — physically impossible if the simulator were reducing hardware clocks),
so thermal behavior on real hardware remains untested.

**Mitigation** — physical thermal soak (15+ min warm-room or stress-warm silicon) + mid-tier device follow-up spike are REQUIRED before any Pull-Wave wave-renderer story enters implementation. Both are documented in the Migration Plan §Follow-up-Required-Before-Sprint-1 section. Scaffold assets preserved at `~/Development/Games/PullWaveSpike/` so follow-up spikes reuse the setup; see `prototypes/pull-wave-perf-spike-2026-07-03/SPIKE-NOTE.md` §Follow-up REQUIRED for the 5-item punch list.

**Notes on CPU sub-budget** (positive evidence from same spike): `SCOPE_CYCLE_COUNTER(STAT_PullWavePerTick)` wrapping the ISMC update loop measured 0.01–0.06 ms across every reading and every simulated thermal state — typical value 0.01 ms is **25× under** the AC-PW-22a 0.25 ms budget, widest observed value 0.06 ms is **4× under**. The AC-PW-22a Pull-Wave per-tick CPU claim from this ADR's Validation Criteria is validated by real-hardware measurement. This ADR's ISMC-over-HISM class choice specifically avoided the 0.05–0.2 ms cull-tree rebuild cost, so the ~25× headroom observed is consistent with the Decision's rationale.

**Implementation guidance (folded from engine specialist minor notes)**:

- **`SetNumCustomDataFloats(N)` call order**: call once in the actor constructor,
  before any `AddInstance()` call. Calling per-instance would incur buffer resize
  overhead. The Key Interfaces code block enforces this order.
- **Index cache type and sentinel**: cache instance indices as `int32` initialized
  to `INDEX_NONE` (-1). The Key Interfaces code block guards `RemoveInstance` and
  `UpdateInstanceTransform` call sites against `INDEX_NONE` input.
- **Material instruction count budget**: add Shader Complexity validation to the
  wave-mass material acceptance criteria before Alpha (per Risk 7).

## GDD Requirements Addressed

| GDD System | Requirement | How This ADR Addresses It |
|------------|-------------|--------------------------|
| `design/gdd/pull-wave-behavior.md` line 1034 | OQ-PW-3 — promoted from open question to REQUIRED ADR (R8 perf-analyst B-3, 2026-06-11): ADR must address (a)–(e) for wave-mass + TrailCube_ISMC; story-Done BLOCKING | All 5 sub-questions answered in Decision section. Component classes locked, no platform-tier conditional, slot counts affirmed, 35 m cull distance. |
| `design/gdd/pull-wave-behavior.md` line 658 (R1 RC-G G-2 binding) | `PerInstanceCustomData` routing on the wave-mass ISMC: 3 slots (0=LeanCharge, 1=NearMissFlash, 2=VoxelDissolve); single batched draw call per ISMC; `MaterialInstanceDynamic` per-wave forbidden | Decision sub-question (d) affirms 3-slot count + slot-index assignment; Architecture Diagram + Key Interfaces enforce single batched draw call per component; `MaterialInstanceDynamic` per-wave is restated as forbidden in Decision body. |
| `design/gdd/pull-wave-behavior.md` line 756 (R7 B9 binding) | Trail-cube ISMC separation: `TrailCube_ISMC` is a distinct component (NOT instances of the wave-mass mesh); 48 instances at PEAK; `PerInstanceCustomData[0] = TrailAlpha`; trail cubes are axis-aligned 1-unit cubes | Decision sub-question (b) locks `TrailCubeISMC` as a separate `UInstancedStaticMeshComponent` with 1 slot; Architecture Diagram shows wave-mass ISMC + trail-cube ISMC = 2 batched draw calls total per R7 B9. |
| `design/gdd/pull-wave-behavior.md` AC-PW-22a | Pull-Wave per-tick CPU budget: 0.25 ms total at PEAK density (5 LEANING / 10 TRAVERSING / 1 LANDED / 0–1 DESPAWNING state mix per R7 B10) | Decision rejects HISM specifically on this AC. The 0.05–0.2 ms cull-tree rebuild cost = 20–80% of this budget. Plain ISMC reclaims this for gameplay logic. Validation Criteria below references AC-PW-22a directly. |
| `design/gdd/pull-wave-behavior.md` Rule 6 + Rule 12 | TRAVERSING waves move at constant velocity per tick; `MAX_CONCURRENT_WAVES_CAP = 16` covers LEANING + TRAVERSING + LANDED per Rule 12 | Key Interfaces' per-tick update loop calls `UpdateInstanceTransform` on every TRAVERSING wave with `bTeleport=true` (constant velocity, no motion-vector consumer on mobile forward); instance count cap enforced by Wave Spawner pool size 23 + ADR-0005's Cold/Idle lifecycle. |
| `design/gdd/wave-spawner-pattern-library.md` line 157 | Pool pre-allocation of 23 `AWave` actors at first-world-load; replay re-entry reuses pool unchanged per EC-WS-8 | **INT-001 binding (amended 2026-06-26 — architecture-review 2026-06-25):** Renderer components live on a shared singleton `APullWaveSubsystemActor` (Decision sub-question (f)) — NOT on individual pooled `AWave` actors. ADR-0005's 23-actor pool remains correct as the budget for actor-shaped state-token leases; the renderer-component ownership is independent of pool size. Per-AWave-actor renderer ownership would yield 32 draw calls at PEAK (16 active × 2 components) and break the R1 RC-G G-2 + R7 B9 "2 batched draw calls at PEAK" requirement. The pre-amendment text labelled component ownership as a per-instance pool implementation detail; that label was withdrawn by architecture-review 2026-06-25 INT-001 as load-bearing. |

## Performance Implications

- **CPU (per tick)**: Plain ISMC per-instance flat frustum check at N = 64 (16 + 48)
  is sub-microsecond on a mid-tier mobile CPU — negligible against AC-PW-22a's
  0.25 ms budget. `UpdateInstanceTransform` + 4 `SetCustomDataValue` calls per
  active wave (3 wave-mass slots + 1 trail-cube slot × 3) with `bMarkRenderStateDirty=false`
  is approximately 7 calls × 16 waves = 112 ISMC API calls per tick at PEAK, plus
  2 `MarkRenderStateDirty()` calls — total estimated cost well under 0.10 ms on
  mid-tier mobile, leaving ≥ 0.15 ms of the AC-PW-22a budget for gameplay logic
  (despawn, collision dispatch, telegraph publication). HISM would consume
  0.05–0.2 ms of this same budget for cull-tree rebuild — net reclaim 0.05–0.2 ms.
- **CPU (per spawn / despawn)**: `AddInstance` is O(1) amortized; `RemoveInstance`
  is O(1) swap-remove. Same-tick batch despawn requires either descending-index
  sort (O(N log N) on N despawned) or plural `RemoveInstances` (O(N), pending
  Verification 8). Negligible at the GDD-locked despawn rates.
- **Memory**: 16-instance wave-mass ISMC + 48-instance trail-cube ISMC.
  Per-instance footprint ≈ `FInstancedStaticMeshInstanceData` (~64–80 bytes per
  instance for transform + custom data) + render-thread mirror. Total estimated:
  < 10 KB for both components combined. Negligible against the 1.5 GB mobile
  memory ceiling.
- **GPU**: 2 batched draw calls per frame at PEAK (1 per ISMC). Of the ~100 mobile
  draw call budget, Pull-Wave consumes 2%. Per-instance frustum cull is render-thread
  flat-loop, sub-millisecond at N = 64.
- **Load Time**: Component construction in actor constructor; no asset loading on
  the critical path. Mesh + material asset load is owned by the Wave Spawner pool
  pre-allocation (ADR-0005); renderer components register their materials on first
  `BeginPlay` of the owning actor. Negligible contribution.
- **Network**: Not applicable — SLIPSTORM is single-player.

## Migration Plan

Not applicable — no Pull-Wave wave-renderer implementation exists at this decision
point. This ADR is a greenfield architectural choice that the first wave-renderer
implementation must follow. If a later epic ever introduces a wave-renderer
implementation using HISM (e.g., during prototype before this ADR was accepted),
that implementation must be migrated to plain ISMC by replacing the component
class declaration (`UHierarchicalInstancedStaticMeshComponent` →
`UInstancedStaticMeshComponent`) and re-running the per-tick budget measurement
under AC-PW-22a.

### Follow-up Required Before Sprint 1 (added 2026-07-08 via SPIKE-NOTE)

Any story that depends on this ADR (Pull-Wave wave-renderer implementation, first
`AWave` actor pool population, or any code touching `WaveMassISMC` /
`TrailCubeISMC` per-tick paths) MUST NOT enter implementation until all four
preconditions are satisfied:

1. **iOS toolchain pre-check on the developer machine** — verify Epic Launcher's
   `Options → Target Platforms → iOS` component installs cleanly and RunUAT
   `BuildCookRun -platform=IOS` completes without error. **Evidence from the
   2026-07-08 spike**: a transient `OtherCompilationError` blocker consumed
   2 of the 5-day spike execution window before an Epic Launcher cache-clear
   (`~/Library/Application Support/Epic/EpicGamesLauncher/Data/{ManifestTemp,DownloadManager}`)
   unblocked it. Sprint 1 planners must budget for this on any developer
   machine that has not previously packaged for iOS.
2. **Physical thermal soak on iPhone 17-class or better** — 15+ min sustained
   run in a warm room or after a stress-test app has warmed silicon. Xcode's
   `Simulate → Thermal State` is NOT a valid stressor (evidenced by
   Serious-thermal GPU < Nominal-thermal GPU in the 2026-07-08 cascade). Only
   physical heat reduces hardware clocks; only physical measurement validates
   the thermal claim from this ADR.
3. **Mid-tier device spike** — iPhone 12/13/SE class (or A15-tier Android
   equivalent) with the same protocol as the 2026-07-08 spike. iPhone 17
   flagship data does not clear the mid-tier target from
   `technical-preferences.md`. Reuse preserved
   `~/Development/Games/PullWaveSpike/` scaffold; only deploy + measure remain.
4. **Nominal reading variance investigation** — 3–5 readings 60 sec apart at
   the same simulated thermal state, computing p50/p95/p99 for Frame + GPU +
   PullWavePerTick. The 2026-07-08 spike observed 16.6 ms ↔ 18 ms Nominal
   variance in two adjacent readings — this may be scene-state (wave-position-
   dependent), warmup-related, or a genuine perf boundary. Resolve before
   Sprint 1.

**Evidence chain**: See `prototypes/pull-wave-perf-spike-2026-07-03/SPIKE-NOTE.md`
§Follow-up REQUIRED and `docs/architecture/architecture-review-2026-07-08.md` §Sprint 1
Gate Summary for the punch-list version of these items and the Sprint 1 gate rule.

## Validation Criteria

- **AC-PW-22a Pull-Wave per-tick CPU budget ≤ 0.25 ms at PEAK** (1 LANDING / 10
  TRAVERSING / 5 LEANING / 0–1 DESPAWNING state mix per R7 B10) — measured via
  Unreal Insights `STAT_PullWavePerTick` scope on mid-tier mobile reference device
  (per Wave Spawner GDD AC-WS-30 measurement methodology). This is the load-bearing
  validation for the ISMC-over-HISM decision.
- **2 batched draw calls per frame for Pull-Wave visuals at PEAK** — measured via
  `stat rhi` / `stat gpu` showing exactly one draw per ISMC component. Adding
  more components (e.g., per-wave dynamic materials) regresses the R1 RC-G G-2
  binding.
- **No per-instance MaterialInstanceDynamic usage** — `git grep -rn "CreateMaterialInstanceDynamic\|UMaterialInstanceDynamic" Source/Slipstorm/Pullwave/` should produce zero results in shipped wave-mass + trail-cube renderer code (verifies the forbidden pattern).
- **Wave-mass material instruction count < mobile-forward register-pressure ceiling**
  on the project's reference low-end and mid-tier devices — validated in Shader
  Complexity view before Alpha (covers Risk 7).
- **9 UE 5.7 source verifications completed** with documented results in the
  wave-renderer epic's pre-implementation verification doc before any
  implementation story is set Ready. Required pre-implementation; story-Done
  BLOCKING on completion of all 9.
- **No frame-lag desync between visual and overlap-query positions** under
  `QueryOnly` collision (Risk 1 / Verification 7) — measured via instrumented
  test that fires near-miss events at known wave Z-positions and asserts
  `OnComponentBeginOverlap` index matches expected.
- **Shared-host topology (INT-001 sub-question (f)) is enforced** — at PR review,
  `rg "TObjectPtr<UInstancedStaticMeshComponent>\s+WaveMassISMC|TObjectPtr<UInstancedStaticMeshComponent>\s+TrailCubeISMC" Source/Slipstorm/` returns the
  declaration on exactly one class (`APullWaveSubsystemActor`) and zero other
  classes. At runtime, `UGameplayStatics::GetAllActorsOfClass(World,
  APullWaveSubsystemActor::StaticClass(), Out); check(Out.Num() == 1)` confirms
  singleton-per-world. A declaration on `AWave` or any per-wave class triggers
  the `per_AWave_actor_render_component_ownership` forbidden pattern.

## Related Decisions

- **ADR-0005** — Wave Spawner Subsystem Hosting (`UGameInstanceSubsystem` + `FTickableGameObject`).
  Sibling ADR; independent decision on lifecycle but **coupled by INT-001**:
  ADR-0005 owns the `AWave` actor pool (23-actor sizing); ADR-0006 sub-question (f)
  binds the renderer components to a separate singleton `APullWaveSubsystemActor`
  (NOT the pooled `AWave` actors). The two topologies coexist: the pool is the
  budget for actor-shaped state-token leases; the singleton is the renderer host.
  See ADR-0005 Implementation Guideline cross-referencing this sub-question.
- **architecture-review 2026-06-25** — surfaced INT-001 (`docs/architecture/architecture-review-2026-06-25.md`).
  The amendment at sub-question (f), the Architecture diagram update, the line 549
  GDD Requirements row flip, and the new Validation Criterion are the resolution
  artifacts for that finding.
- **ADR-0004** — R11a Dual-Grep Methodology + Oracle-Site Sweep Corollary. Cited
  for the registry-update + GDD-sync discipline applied to this ADR's authoring
  (the `TrailCube_ISMC` → `TrailCubeISMC` naming inconsistency flagged in the
  Decision is the kind of sub-class (b) narrative drift ADR-0004 catalogs).
- **Pull-Wave GDD** (`design/gdd/pull-wave-behavior.md`) — Authoritative design
  document; R14-closed 2026-06-19. R1 RC-G G-2 (single batched draw call binding),
  R7 B9 (trail-cube ISMC separation), R8 perf-analyst B-3 (OQ-PW-3 promotion to
  ADR), and AC-PW-22a (per-tick budget) are the load-bearing references.
- **Wave Spawner GDD** (`design/gdd/wave-spawner-pattern-library.md`) — R3a-closed
  2026-06-22. Hosts the `AWave` actor pool that owns these renderer components.
- **`.claude/docs/technical-preferences.md`** — Mobile target, 60 fps, ~100 draw
  calls, 1.5 GB memory ceiling, Lumen/Nanite/Substrate disabled on mobile.
- **`docs/architecture/platform-seam-interfaces.md`** — No seam changes from this ADR.

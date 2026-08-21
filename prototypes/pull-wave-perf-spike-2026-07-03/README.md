# Pull-Wave Perf Spike — 2026-07-03

> **PROTOTYPE — NOT FOR PRODUCTION.** Throwaway build to answer one perf question. Do not import into `src/`.

## Question

Can the Pull-Wave PEAK rendering load (16 wave-mass + 48 trail-cube ISMC instances animating at 4–18 m/s) sustain **60fps AND ≤0.25ms per-tick Pull-Wave sub-budget** on iPhone 17 across Xcode-simulated thermal states (Nominal → Critical), using Mobile Forward with no Nanite/Lumen?

## Framing (honest limitation)

iPhone 17 is a **flagship** device, not the mid-tier target platform in `technical-preferences.md`. This spike is a **flagship + thermal-simulator proxy**. A PASS here proves feasibility on Apple silicon; it does **not** clear the mid-tier shipping gate. A follow-up spike on iPhone 12/13/SE class hardware is still required before Sprint 1.

## Scope

- Two ISMCs matching ADR-0006: `WaveMassISMC` (16 instances, 3 PICD floats), `TrailCubeISMC` (48 instances, 1 PICD float), cull distance 35m.
- Per-tick pattern: per-instance `UpdateInstanceTransform(bMarkRenderStateDirty=false, bTeleport=true)` + `SetCustomDataValue(bMarkRenderStateDirty=false)`, then single `MarkRenderStateDirty()` at end of tick.
- Velocities lerp 4–18 m/s across instances.
- Wrapped in `SCOPE_CYCLE_COUNTER(STAT_PullWavePerTick)` for sub-budget isolation.

## Explicitly cut

Palm-rejection, RSM, DPC, player movement, wave spawner, AI, overlap queries, Android, ISMC API verification concerns (V3/V4/V6/V7 in ADR-0006), mid-tier device (deferred).

---

## Setup

### 1. Create a fresh UE 5.7 project

- File → New Project → Games → Blank → **C++** → **Mobile / Tablet** → **Scalable**.
- Location: outside this repo. Name: e.g. `PullWaveSpike`.
- Do NOT drop this project inside `Claude-Code-Game-Studios/`. Prototype code stays isolated per project rules.

### 2. Verify Mobile Forward + no Nanite/Lumen

- Project Settings → Rendering → Mobile → **Mobile Shading = Forward**.
- Project Settings → Rendering → Nanite = **Disabled**.
- Project Settings → Rendering → Global Illumination Method = **None** (Lumen off).
- Project Settings → Rendering → Reflection Method = **None** or **Screen Space** (Lumen reflections off).

### 3. Drop in the actor

- Copy `WavePerfTestActor.h` + `WavePerfTestActor.cpp` into `Source/PullWaveSpike/`.
- Regenerate project files (right-click .uproject → Generate Xcode Project).
- Build for Mac editor (`Development Editor` target) so you can place the actor in a level.

### 4. Level setup (scene shell — not strawman)

Create a new empty level named `PerfSpike`:

- **Floor**: place a `StaticMeshActor` using `/Engine/BasicShapes/Plane` scaled to 20x20 (represents track width and viewing floor).
- **Skybox**: add `BP_Sky_Sphere` or a simple `SkyAtmosphere` actor.
- **Directional light**: one `DirectionalLight`, mobility = `Movable`. Cast shadows on.
- **Tonemapper**: add a `PostProcessVolume`, set `Unbound = true`, ensure default tonemapper is on (no changes).
- **Camera**: place a `CameraActor` looking down the +X axis from Z=200 at Y=0. This is the fixed-camera 5m-wide framing.
- **Spike actor**: drag `AWavePerfTestActor` into the level at origin.
- **Player start**: place a `PlayerStart` next to the camera. In the level Blueprint, on BeginPlay call `SetViewTargetWithBlend` to point at the CameraActor.

Save the level.

### 5. Package for iOS

- File → Package Project → iOS.
- Config: **Development** (Shipping strips stats — you need stats for measurement).
- Open the resulting `.xcodeproj` in Xcode and deploy to the iPhone 17 device.

---

## Measurement Protocol

### On-device console commands

Open the on-device console (bind a hardware keyboard OR use the swipe-3-fingers gesture in dev builds):

```
stat unit          # Frame / Game / Draw / GPU ms
stat scenerendering # draw calls, primitives, sceneMB
stat PullWaveSpike  # our custom stat group — reports STAT_PullWavePerTick
stat gpu            # GPU timing per pass — MAY report zeros on Metal, see fallback
```

### Verify `stat gpu` in first 15 minutes

If `stat gpu` reports all zeros, enable GPU counters in Project Settings → Rendering → Optimizations → **Allow GPU counters** = true, repackage. If still zeros on iPhone 17 Metal, fall back to **Xcode → Debug → Capture Metal Frame** on-device — the FPS HUD + Metal counters give GPU time per pass.

### The soak run (repeat for each thermal state)

**Warmup discard**: samples during seconds 0–10 are contaminated by PSO/shader compilation. Discard them.

For each thermal state — Nominal → Fair → Serious → Critical — do this:

1. **Reset thermal simulator**: Xcode → Debug → **Simulate → Thermal State → [State]**.
2. **Cold launch app** on device.
3. Once in-level, wait 10 seconds (warmup discard window).
4. **Sample for 4 minutes** continuously (seconds 10–250).
5. Record from `stat unit` HUD via video or repeated screenshots every 30 seconds:
   - Frame time (target ≤ 16.6 ms)
   - Game time
   - Draw time
   - GPU time
6. Record `STAT_PullWavePerTick` from `stat PullWaveSpike` HUD (target ≤ 0.25 ms per tick).
7. Record `stat scenerendering` draw call count (should be ~2 for the ISMCs + scene shell overhead).
8. Compute p50 / p95 / p99 across the sampled window.
9. **Cool device to ambient** (5 min minimum, screen off) before next thermal state.

### Verdict thresholds

| Verdict | Condition |
|---|---|
| **PASS** | p95 Frame ≤ 16.6ms AND p95 `STAT_PullWavePerTick` ≤ 0.25ms across **all four** thermal states |
| **CONCERNS** | Nominal + Fair pass; Serious or Critical fails on Frame or PW-per-tick |
| **FAIL** | Nominal or Fair fails on Frame or PW-per-tick |

Every verdict carries the honest disclaimer: **flagship + thermal simulator ≠ mid-tier silicon**. A follow-up mid-tier spike is required before Sprint 1 regardless of this spike's verdict.

---

## What to hand back

When measurements are complete, come back with the 4 thermal states × (p50/p95/p99 × 5 metrics) numbers. I will write `SPIKE-NOTE.md` with the verdict and the follow-up guidance for Sprint 1 gating.

## Time budget

- Setup + package + first deploy: 1.5 h
- 4 thermal soaks × (4 min run + 5 min cooldown): 40 min
- Metal frame capture fallback if needed: 30 min
- SPIKE-NOTE authoring: 30 min (I do this)

Hard cap: 4 hours. If you're past 3 hours and still haven't produced a single valid sample, stop and flag it — the spike is not converging.

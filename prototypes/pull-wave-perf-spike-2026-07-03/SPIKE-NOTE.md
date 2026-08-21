# Pull-Wave Perf Spike — SPIKE-NOTE

> **⚠️ PARTIAL VERDICT.** This spike executed successfully on iPhone 17 physical hardware after resolving an initial iOS toolchain packaging blocker. **CPU sub-budget claim from ADR-0006 validates decisively.** 60fps observation at Nominal is ambiguous (observed range 16.6–18 ms). Thermal cascade did not meaningfully stress the workload — Xcode's `Simulate → Thermal State` sends notifications but does not actually reduce hardware clocks; the evidence is that GPU cost went DOWN, not up, from Nominal (11 ms) through Fair (9.6 ms) to Serious (7 ms), which is impossible if the simulator were actually stressing hardware. **A real physical thermal soak is still required. A mid-tier device spike is still required. Sprint 1 gate remains open.**

## Metadata

| Field | Value |
|---|---|
| **Spike run dates** | 2026-07-03 (scaffold) → 2026-07-08 (executed on iPhone 17) |
| **Device used** | iPhone 17 (flagship — NOT the mid-tier target from `technical-preferences.md`) |
| **iOS version** | 26.x (Xcode 26.1.1 SDK) |
| **UE config** | Development, iOS Mobile Forward, Nanite off, Lumen off, FrameRateLock=PUFRL_60 |
| **Scene shell** | 200m×200m floor + SkyAtmosphere + SkyLight RealTimeCapture + DirectionalLight + PostProcessVolume (tonemapper) + CameraActor (0,-800,200) + WavePerfTestActor at (0,0,100) WrapRangeCm=500 |
| **Duration observed** | ~60 sec per thermal state; no sustained 3–5 min soak per state |
| **Verdict** | **PARTIAL — CPU sub-budget PASS; 60fps ambiguous; thermal robustness NOT VALIDLY TESTED** |

---

## Raw Observations

`stat unit` + `stat gpu` + `stat PullWaveSpike` HUDs observed on-device across four thermal states via Xcode → Debug → Simulate → Thermal State.

| Thermal State | Frame (ms) | GPU (ms) | PullWavePerTick (ms) | Notes |
|---|---|---|---|---|
| **Nominal (reading 1)** | 16.6 | 7.0 | 0.04 | Earlier baseline reading, right after 60fps unlock |
| **Nominal (reading 2)** | **18** | **11** | 0.01 | Cascade baseline reading — inconsistent with reading 1 |
| **Fair** | 16.6 | 9.6 | 0.01 | V-sync locked, no observable stress vs Nominal |
| **Serious** | 16.6 | **7** | 0.01 | **GPU LOWER than Nominal — impossible if simulator were stressing hardware** |
| **Critical** | 16 | **28** (momentary) | 0.06 | GPU spike observed but Frame stayed at 16 → likely momentary UE thermal-notification callback artifact, not sustained perf failure |

---

## Verdicts

### ✅ CPU sub-budget (ADR-0006 ≤0.25 ms per tick)

**PASS decisively.** `PullWavePerTick` measured **0.01–0.06 ms** across every reading and every thermal state. Widest observed value (0.06 ms in Critical) is still **4× under the 0.25 ms budget**. Typical value of 0.01 ms is **25× under budget**. This is not a marginal PASS — the Pull-Wave update loop is a rounding error on iPhone 17.

### ⚠️ 60fps at Nominal (16.67 ms frame budget)

**AMBIGUOUS.** Two Nominal readings taken minutes apart at the same thermal state gave 16.6 ms (V-sync locked at 60fps) and 18 ms (~55 fps, over budget). Both are real measurements. Cherry-picking the favorable one would be dishonest.

- Honest report: **iPhone 17 at Nominal thermal is at or near the 60fps ceiling with observed variance across the ±10% range.** Not a stable PASS from this spike.
- Fair and Serious both showed 16.6 ms (V-sync locked). Critical showed 16 ms with a GPU spike.
- Interpretation: 60fps is **achievable** on iPhone 17 flagship but is not **comfortably headroomed** at Nominal.

### ❌ Thermal robustness — NOT VALIDLY TESTED

The Xcode thermal state simulator sends `NSProcessInfoThermalStateDidChangeNotification` to the app but does **not reduce hardware clock speeds**. Evidence: GPU cost went DOWN (11 → 9.6 → 7 ms) across Nominal → Fair → Serious. If the simulator were meaningfully stressing hardware, this reversal is physically impossible.

Interpretation: **UE has not hooked the thermal notification, or has and does nothing measurable in response.** Either way, this cascade measured essentially the same thing four times with a different label. The variation observed (7–11 ms GPU) is measurement noise + scene-state variance, not thermal response.

The Critical GPU spike to 28 ms is most plausibly a **one-shot UE thermal-callback artifact** (resource reload, quality-tier switch, or similar), not a sustained thermal-throttled measurement. Frame stayed at 16 ms because the spike resolved before affecting the next V-sync interval.

### ❌ Mid-tier device (iPhone 12/13/SE class)

Still required. iPhone 17 is flagship (A19-class silicon). ADR-0006's mobile target is the mid-tier per `technical-preferences.md`. Even a decisive iPhone 17 PASS does not validate mid-tier.

---

## Important Caveat — Scene Shell Cost

The measured GPU cost (7–11 ms Nominal) is **dominated by the scene shell, not Pull-Wave**. Rough breakdown from `stat gpu` observations:

| GPU pass | ~ms | Ownership |
|---|---|---|
| Mobile Scene Render | ~10.5 | Scene shell (floor + sky + shadows + cubes) |
| Postprocessing | ~5.1 | Tonemapper + PP chain |
| Sky/Shadow/Light passes | ~2 | Scene shell |
| **Pull-Wave contribution** | **~1 ms** | The 64 ISMC instances |

**Two implications**:
1. **Pull-Wave's own perf headroom is much larger than the raw Frame/GPU numbers suggest.** The 64 ISMC instances add roughly 1 ms of GPU cost. Most of the observed cost is the scene shell.
2. **SLIPSTORM's real production scene shell will differ.** This spike's shell (200m floor + SkyAtmosphere) is a legitimate representative load but not necessarily the exact production scene composition. Perf verdict is a **floor estimate** for Pull-Wave overhead, not a ceiling for total-scene GPU cost.

---

## What Was Executed

- Blank UE 5.7 Mobile C++ project at `~/Development/Games/PullWaveSpike/` (outside this repo)
- `WavePerfTestActor.h` + `.cpp` compiled + verified in dylib (40 symbols post-full-rebuild)
- `PerfSpike.umap` scene shell built per README §Setup
- Mobile Forward + Nanite/Lumen off + FrameRateLock=PUFRL_60 configured
- `.app` bundle cooked, staged, packaged (`Development` config), signed with Personal Team cert, deployed to iPhone 17 physical
- 4-thermal-state cascade run via Xcode → Debug → Simulate → Thermal State
- `stat unit` + `stat PullWaveSpike` + `stat gpu` HUDs sampled at each state (~60 sec observation each)

## What Was NOT Executed

- **Sustained soak** — 60 sec observation per state, not the 3–5 min soaks originally specified. Enough to establish the readings but not enough to catch slow drift or accumulation.
- **Warmup discard methodology** — captured whatever the HUD showed at observation time; did not formally discard seconds 0–10 (advisor original recommendation).
- **Real physical thermal load** — no sustained heavy workload run before measurement to induce actual heat. The Xcode simulator was used exclusively.
- **Mid-tier device** — iPhone 17 only; iPhone 12/13/SE not tested.
- **Distribution formal p50/p95/p99** — single readings per state, not statistical distributions.

---

## Follow-up REQUIRED before Sprint 1 (Pull-Wave wave-renderer story)

Status of the ADR-0006 Migration Plan gate: **STILL OPEN.** This spike closes some risk but does not clear the gate.

1. **Physical thermal soak on iPhone 17 or better** — run the app for 15+ min in a warm room or after a stress-test app has warmed the silicon. Real hardware throttling requires real heat; Xcode simulator does not deliver this.
2. **Mid-tier device spike** (iPhone 12/13/SE class or A15-tier Android equivalent) — same protocol, same thermal cascade, at least Nominal + Physical-Soak. iPhone 17 flagship data does not clear the mid-tier target from `technical-preferences.md`.
3. **Investigate the Nominal reading variance** (16.6 ↔ 18 ms) — is it warmup-related, scene-state (wave positions at time of capture), or a real perf boundary? Repeat 3–5 Nominal readings 60 sec apart and compute p50/p95/p99.
4. **Investigate the Critical GPU spike (28 ms momentary)** — is it a UE thermal-callback artifact worth hooking, or noise? Enable UE thermal callbacks and log any explicit thermal response code.
5. **Do NOT begin Pull-Wave wave-renderer story implementation** until items 1–4 are answered. ADR-0006 Migration Plan gate unchanged.

## Recommendation for ADR-0006

- **Bump `Last Verified` stamp** to 2026-07-08 with the qualifier: "Verified on iPhone 17 flagship (Xcode simulator only); CPU sub-budget claim validated decisively; 60fps + thermal claims validated *partially*; mid-tier + physical thermal follow-up REQUIRED."
- **Add a Risks-table entry**: "iPhone 17 Nominal-thermal 60fps observed at ceiling with variance; comfort headroom not confirmed. Mitigation: physical thermal soak + mid-tier follow-up before Sprint 1."
- **Add a Migration Plan step**: "Verify iOS toolchain installs cleanly in developer environment before packaging any story that depends on this ADR" (already recommended in prior SPIKE-NOTE draft; blocker was real and consumed 2 days of this spike).

## Scaffold Assets (retained for the follow-up spikes)

- `prototypes/pull-wave-perf-spike-2026-07-03/README.md` — full protocol (still valid for follow-up runs)
- `prototypes/pull-wave-perf-spike-2026-07-03/WavePerfTestActor.h` + `.cpp` — actor code (compiles clean against UE 5.7)
- `~/Development/Games/PullWaveSpike/` — UE 5.7 Mobile C++ project with `PerfSpike.umap` scene + FrameRateLock=PUFRL_60 config
- Follow-up spikes need only redeploy — no rescaffold required.

---

*Spike closed 2026-07-08. Sprint 1 gate remains open pending physical thermal + mid-tier validation.*

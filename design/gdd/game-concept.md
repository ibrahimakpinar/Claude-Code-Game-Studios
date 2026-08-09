# Game Concept: SLIPSTORM

*Created: 2026-05-03*
*Status: Approved (Creative Director — 2026-05-03)*

---

## Elevator Pitch

> It's a 60-second voxel endless runner where magnetized debris actively hunts
> you in surging waves, and survival means reading the pull seven-tenths of a
> second before it lands.

---

## Core Identity

| Aspect | Detail |
|---|---|
| **Genre** | Endless runner / reflex arcade |
| **Platform** | Mobile (iOS / Android) |
| **Target Audience** | Skill-seeking commuters, 18–35 |
| **Player Count** | Single-player |
| **Session Length** | 60-second runs (3–8 sessions per play window) |
| **Monetization** | TBD (cosmetics-only if any) |
| **Estimated Scope** | Medium (6–9 months, solo or 2-person) |
| **Comparable Titles** | Subway Surfers, Crossy Road, Alto's Odyssey |

---

## Core Fantasy

*I read the storm seven-tenths of a second before it hits, and I bend through it.*

The player is the only soft thing in a world of pulling iron. Magnetized debris
doesn't wait to be dodged — it reaches. The fantasy is the near-miss: slipping
free of a wave that already had you, with exactly enough space to survive.

---

## Unique Hook

"It's like Subway Surfers, **and also the hazards reach for you.**"

Most runner hazards are passive — walls, pits, obstacles you sidestep. In
SLIPSTORM, the world has agency. Magnetized debris locks onto the player and
surges in waves. The differentiator is that every hazard telegraphs its pull
one beat before contact — the world *flinches* before it grabs — giving skilled
players a seven-tenths-of-a-second window to slip through. The game rewards
reading, not reflexes alone.

---

## Player Experience Analysis (MDA Framework)

### Target Aesthetics

| Aesthetic | Priority | How We Deliver It |
|---|---|---|
| **Challenge** | 1 — Primary | Read-react-slip loop; escalating pull patterns; transparent skill ceiling |
| **Sensation** | 2 — Secondary | Sub-bass pull pulse, metallic shimmer on slip, near-miss visual juice |
| **Discovery** | 3 — Supporting | New pull-wave patterns at higher difficulty thresholds |
| **Fantasy** | N/A | No character identity; player IS the slipping object |
| **Narrative** | N/A | No story |
| **Fellowship** | N/A | Solo-first; optional leaderboard |
| **Expression** | N/A | No build variety |
| **Submission** | N/A | Explicitly not a calm game |

### Key Dynamics
- Players will learn to read telegraph animations before reacting to the pull
- Players will attempt "one more run" after each near-miss or death
- Skilled players will start anticipating wave patterns rather than reacting

### Core Mechanics
1. **Slip** — lateral movement that is the game's single verb
2. **Pull-wave** — magnetized debris surges toward the player with a visible telegraph
3. **60-second run arc** — escalating wave density from calm opener to peak density

---

## Player Motivation Profile

| Need | How This Game Satisfies It | Strength |
|---|---|---|
| **Competence** | Readable skill ceiling; transparent near-miss feedback | Core |
| **Autonomy** | Player controls timing of slip; no forced path | Supporting |
| **Relatedness** | Optional leaderboard; shareable near-miss clips | Minimal |

**Bartle Profile:**
- [x] **Achievers** — high score, personal bests
- [x] **Killers/Competitors** — leaderboard (optional)
- [ ] Explorers — minimal discovery layer
- [ ] Socializers — solo-first

### Flow State Design
- **Onboarding**: First 10 seconds of run are low-density; one slow pull-wave teaches the telegraph
- **Difficulty scaling**: Wave density and telegraph window duration tighten with run time
- **Feedback clarity**: Near-miss audio + visual confirms skill; death replay shows exact contact frame
- **Failure recovery**: Instant restart; no penalty screens; death is 1 tap from next run

---

## Core Loop

### Moment-to-Moment (30 seconds)
Debris surges toward the player in magnetized waves. Each wave telegraphs with
a visible "lean" animation 0.70s before contact (R11a-arithmetic 2026-06-18 —
matches R10d FLOOR=0.70s; was 0.6s pre-R10d). The player slips laterally to
avoid the pull. Repeat.

### Short-Term (60-second run)
The run escalates in three phases: slow opener (learn the rhythm), mid-escalation
(overlapping waves), peak density (multiple simultaneous pulls). The run ends at
60 seconds or on contact. Score = waves slipped successfully.

### Session-Level (3–8 runs, ~5–10 minutes)
Players chase a personal best or a specific score threshold. "One more run"
retention is driven by the near-miss moment and the feeling that the next run
could be the perfect run.

### Long-Term Progression
Mastery of telegraph reading and slip timing. No permanent stat upgrades. Score
history and leaderboard position are the only persistent state.

### Retention Hooks
- **Mastery**: Visible skill ceiling with a long learning curve
- **Investment**: Personal best score is always visible; players know how close they are
- **Curiosity**: New pull-wave patterns unlock at higher difficulty thresholds

---

## Game Pillars

### Pillar 1: Slip Is the Only Verb
Every feature must answer "how does this make slipping feel better?" If it
can't, it's cut.
*Design test*: If we're debating adding a dash ability vs. a lateral step, this
pillar says choose the step — it's slip, not flight.

### Pillar 2: The World Telegraphs Before It Strikes
Every hazard has a readable pre-pull tell at least 0.70s before contact
(R11a-arithmetic 2026-06-18 — matches R10d FLOOR=0.70s; was 0.6s pre-R10d),
validated on a 6-inch screen at arm's length.
*Design test*: If a new wave type requires a frame-perfect reaction, it fails this
pillar and must be redesigned with a longer or clearer telegraph.

### Pillar 3: Sixty Seconds Is the Whole Game
A complete emotional arc — calm, escalation, peak, resolution — fits inside one
run. No meta-progression required to feel a peak.
*Design test*: If a proposed feature only pays off after multiple runs (XP bars,
unlocks), it violates this pillar.

### Pillar 4: Surreal in Motion, Not in Menus
Strangeness lives in how things move and pull, not in narrative text, loading
screens, or UI chrome.
*Design test*: If the game's surreal identity only shows in the art direction
and not in the wave behavior or run feel, it fails this pillar.

### Pillar 5: Skill Is Visible
Players can point to the exact moment they earned a slip. No invisible RNG
saves, no mystery deaths.
*Design test*: If death could be attributed to latency or unclear collision rather
than a missed telegraph read, it fails this pillar.

### Anti-Pillars

- **NOT a meta-progression grinder**: No XP bars, permanent unlocks, or daily-login
  mechanics. Skill is the only progression.
- **NOT a narrative game**: No characters, dialogue, lore drops, or cutscenes.
  World mood is conveyed through motion and audio only.
- **NOT a physics sandbox**: Magnetism is a gameplay illusion, not a simulation.
  Pulls are scripted curves. Realism never overrides readability.
- **NOT a calm game**: Surreal is the tone, but the heart rate is elevated.
- **NOT a gacha or loot game**: No randomized rewards. Cosmetics are the only
  possible commerce surface (if any).

---

## Inspiration and References

| Reference | What We Take | What We Do Differently | Why It Matters |
|---|---|---|---|
| Subway Surfers | 60-second reflex run, mobile-first, instant retry | Hazards actively reach for you; voxel + surreal | Validates session length and mobile loop |
| Crossy Road | Voxel aesthetic, mobile-optimized, approachable art | Kinetic + surreal tone; not cute | Validates voxel on mobile |
| Alto's Odyssey | Surreal dreamlike tone, flow state | Much faster pace; Challenge primary not Submission | Validates surreal mobile aesthetics |

---

## Target Player Profile

| Attribute | Detail |
|---|---|
| **Age range** | 18–35 |
| **Gaming experience** | Mid-core (plays mobile daily; knows genre conventions) |
| **Time availability** | 60–180 second sessions, 3–8 sessions per commute |
| **Platform preference** | Mobile (primary); has console/PC but plays mobile on the go |
| **Current games they play** | Subway Surfers, Alto's Odyssey, Crossy Road |
| **What they're looking for** | A runner with a readable skill ceiling; something that rewards improvement |
| **What would turn them away** | Pay-to-win; mandatory progression walls; unclear deaths |

---

## Technical Considerations

| Consideration | Assessment |
|---|---|
| **Engine** | Unreal Engine (mobile target) — version TBD, run `/setup-engine` |
| **Key Technical Challenge** | Pre-pull telegraph readability on 6-inch screen at escalating speeds — must be prototyped first |
| **Art Style** | Voxel (Crossy Road style) — chunky cubes, mobile-optimized geometry |
| **Art Pipeline Complexity** | Low-Medium (voxel assets, no rigged characters required) |
| **Audio** | Moderate — reactive audio (pull pulse, slip shimmer, music breathes with wave rhythm) |
| **Networking** | None (solo) — optional async leaderboard |
| **Content Volume** | 1 run arc × N difficulty tiers; pull-wave pattern library |
| **Procedural Systems** | Wave pattern sequencing (procedural or hand-authored pool) — TBD |

---

## Risks and Open Questions

### Design Risks
- Telegraph readability degrades at high wave speeds — may require difficulty cap
- "Surreal in motion" may be hard to distinguish from "generic voxel" without strong audio
- 60-second session feel requires perfect escalation curve — needs early playtesting

### Technical Risks
- Unreal on mobile thermal budget with multiple simultaneous voxel pull animations — validate early
- Touch input latency may conflict with 0.70s telegraph window — measure on target device (R11a-arithmetic 2026-06-18 — was 0.6s pre-R10d)

### Market Risks
- Mobile endless runner category is saturated — hook must be immediately legible in a 6-second store preview
- "Surreal" tone may limit discoverability vs. cute/approachable voxel aesthetics

### Scope Risks
- Reactive audio system (music breathing with wave rhythm) adds scope if procedural — may simplify to pattern-matched stems

### Open Questions
- Should pull-wave patterns be procedural or hand-authored pools? (Prototype needed)
- What is the exact telegraph animation? (First prototype deliverable)
- Leaderboard: global async vs. friend-based vs. none? (Post-MVP decision)

---

## MVP Definition

**Core hypothesis**: Players find the telegraph + slip loop engaging enough to
attempt multiple 60-second runs in a single session.

**Required for MVP:**
1. One pull-wave type with working telegraph animation
2. Slip mechanic (lateral dodge) on touch input
3. 60-second run arc with 3-phase escalation
4. Score counter (waves slipped)
5. Instant restart

**Explicitly NOT in MVP:**
- Multiple wave types
- Voxel art (placeholder geometry is fine)
- Reactive audio
- Leaderboard
- Any monetization

---

## Next Steps

- [x] Creative Director brief approved — 2026-05-03
- [ ] Run `/setup-engine` to configure Unreal Engine version
- [ ] Run `/map-systems` to decompose into individual systems
- [ ] Prototype telegraph + slip loop (highest-risk bet — do this first)
- [ ] Run `/design-system core-loop` after systems index is created

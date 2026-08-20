# Epics Index

Last Updated: 2026-08-19
Engine: Unreal Engine 5.7

| Epic | Layer | System | GDD | Stories | Status |
|------|-------|--------|-----|---------|--------|
| [Player Movement (Slip)](player-movement/EPIC.md) | Core | Player Movement | `design/gdd/player-movement.md` (decomposed into 3 sub-GDDs) | 14 stories (13 numbered + Story 001a test harness); all closed 2026-07-11 through 2026-08-07 | **Done** (2026-08-07) |
| [Pull-Wave Behavior](pull-wave/EPIC.md) | Feature | Pull-Wave | `design/gdd/pull-wave-behavior.md` | 9 stories (all Complete 2026-08-19) | **Done** (2026-08-19) |
| [Wave Spawner Pattern Library](wave-spawner/EPIC.md) | Feature | Wave Spawner | `design/gdd/wave-spawner-pattern-library.md` | 9 stories (all Ready) | **Ready** |

## Layer Progress

| Layer | Systems Total | Epics Created | Epics Ready | Epics Done |
|-------|---------------|---------------|-------------|------------|
| Foundation | (TBD) | 0 | 0 | 0 |
| Core | (TBD) | 1 | 0 | 1 |
| Feature | (TBD) | 2 | 1 | 1 |
| Presentation | (TBD) | 0 | 0 | 0 |

## Next Actions

- **Now**: Begin Wave Spawner implementation — `/story-readiness production/epics/wave-spawner/story-001-subsystem-class-and-object-pool.md` → `/dev-story`
- Commit Pull-Wave stories 001–009 and Wave Spawner stories 001–009 (all untracked in git — `Source/SLIPSTORM/PullWave/`, `Source/SLIPSTORM/Seam/`, `Source/SLIPSTORM/Tests/`, `production/epics/pull-wave/`, `production/epics/wave-spawner/`)
- Work through Wave Spawner stories in order (001 → 009); each `Depends on:` field gates when a story can start

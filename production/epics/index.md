# Epics Index

Last Updated: 2026-08-08
Engine: Unreal Engine 5.7

| Epic | Layer | System | GDD | Stories | Status |
|------|-------|--------|-----|---------|--------|
| [Player Movement (Slip)](player-movement/EPIC.md) | Core | Player Movement | `design/gdd/player-movement.md` (decomposed into 3 sub-GDDs) | 14 stories (13 numbered + Story 001a test harness); all closed 2026-07-11 through 2026-08-07 | **Done** (2026-08-07) |
| [Pull-Wave Behavior](pull-wave/EPIC.md) | Feature | Pull-Wave | `design/gdd/pull-wave-behavior.md` | 9 stories | **Ready** |

## Layer Progress

| Layer | Systems Total | Epics Created | Epics Ready | Epics Done |
|-------|---------------|---------------|-------------|------------|
| Foundation | (TBD) | 0 | 0 | 0 |
| Core | (TBD) | 1 | 0 | 1 |
| Feature | (TBD) | 1 | 1 | 0 |
| Presentation | (TBD) | 0 | 0 | 0 |

## Next Actions

- Complete Sprint 1 close-out tasks (see `production/sprints/sprint-1.md`): S1-04 Integration test harness fix, S1-06 Story 010 PEAT evidence discharge, S1-08 `/architecture-review` PM downstream consumers pass
- Author ADR-0011 Wave Spawner pattern library via `/architecture-decision` (Sprint 1 S1-05) — gates Wave Spawner epic creation
- Author ADR-0010 Pull-Wave lifecycle when Wave Spawner is underway (Pull-Wave is the natural follow-on Feature-layer epic)

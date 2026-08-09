# Technical Preferences

<!-- Populated by /setup-engine. Updated as the user makes decisions throughout development. -->
<!-- All agents reference this file for project-specific standards and conventions. -->

## Engine & Language

- **Engine**: Unreal Engine 5.7
- **Language**: C++ (primary), Blueprint (gameplay prototyping)
- **Rendering**: Mobile Forward renderer (mobile target); Lumen/Nanite disabled on mobile
- **Physics**: Chaos (default)

## Input & Platform

<!-- Written by /setup-engine. Read by /ux-design, /ux-review, /test-setup, /team-ui, and /dev-story -->
<!-- to scope interaction specs, test helpers, and implementation to the correct input methods. -->

- **Target Platforms**: Mobile (iOS, Android)
- **Input Methods**: Touch
- **Primary Input**: Touch
- **Gamepad Support**: None
- **Touch Support**: Full
- **Platform Notes**: 6-inch screen target at arm's length. Thermal budget critical for sustained 60s runs. Voxel art with mobile-optimized geometry (no Nanite/Lumen on mobile). All interactions must be reachable one-handed.

## Naming Conventions

- **Classes**: Prefixed PascalCase (`A` Actor, `U` UObject, `F` struct, `E` enum)
- **Variables**: PascalCase (e.g., `MoveSpeed`); booleans `b` prefix (`bIsAlive`)
- **Signals/Events**: Delegates `On<Event>` (e.g., `OnHealthChanged`); UE Dynamic Multicast for BP-exposed
- **Files**: Match class without prefix (e.g., `PlayerController.h` / `.cpp`)
- **Scenes/Prefabs**: Levels `.umap` (PascalCase); Blueprints `BP_<Name>`; Widgets `WBP_<Name>`
- **Constants**: PascalCase with `k` prefix or UPPER_SNAKE_CASE for static consts

## Performance Budgets

- **Target Framerate**: 60 fps (sustained on mid-tier mobile)
- **Frame Budget**: 16.6 ms
- **Draw Calls**: ~100 per frame (mobile)
- **Memory Ceiling**: ~1.5 GB (mid-tier device target)

## Testing

- **Framework**: UE Automation Framework (built-in)
- **Minimum Coverage**: TBD per system (set in /sprint-plan)
- **Required Tests**: Balance formulas, gameplay systems, networking (if applicable)

## Forbidden Patterns

<!-- Add patterns that should never appear in this project's codebase -->
- [None configured yet — add as architectural decisions are made]

## Allowed Libraries / Addons

<!-- Add approved third-party dependencies here -->
- [None configured yet — add as dependencies are approved]

## Architecture Decisions Log

<!-- Quick reference linking to full ADRs in docs/architecture/ -->
- [No ADRs yet — use /architecture-decision to create one]

## Engine Specialists

<!-- Written by /setup-engine when engine is configured. -->
<!-- Read by /code-review, /architecture-decision, /architecture-review, and team skills -->
<!-- to know which specialist to spawn for engine-specific validation. -->

- **Primary**: unreal-specialist
- **Language/Code Specialist**: ue-blueprint-specialist (Blueprint graphs) or unreal-specialist (C++)
- **Shader Specialist**: unreal-specialist (no dedicated shader specialist — primary covers materials)
- **UI Specialist**: ue-umg-specialist (UMG widgets, CommonUI, input routing, widget styling)
- **Additional Specialists**: ue-gas-specialist (Gameplay Ability System, attributes, gameplay effects), ue-replication-specialist (property replication, RPCs, client prediction, netcode)
- **Routing Notes**: Invoke primary for C++ architecture and broad engine decisions. Invoke Blueprint specialist for Blueprint graph architecture and BP/C++ boundary design. Invoke GAS specialist for all ability and attribute code. Invoke replication specialist for any multiplayer or networked systems (SLIPSTORM is solo — only relevant if leaderboard adds network). Invoke UMG specialist for all UI implementation.

### File Extension Routing

<!-- Skills use this table to select the right specialist per file type. -->
<!-- If a row says [TO BE CONFIGURED], fall back to Primary for that file type. -->

| File Extension / Type | Specialist to Spawn |
|-----------------------|---------------------|
| Game code (.cpp, .h files) | unreal-specialist |
| Shader / material files (.usf, .ush, Material assets) | unreal-specialist |
| UI / screen files (UMG Widget Blueprints, WBP_*) | ue-umg-specialist |
| Scene / prefab / level files (.umap, .uasset) | unreal-specialist |
| Native extension / plugin files (.uplugin, modules) | unreal-specialist |
| Blueprint graphs (.uasset BP classes) | ue-blueprint-specialist |
| General architecture review | unreal-specialist |

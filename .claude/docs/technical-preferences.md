# Technical Preferences

<!-- project.yaml at the repo root is the machine-readable source of truth for
     engine, specialists, naming, performance, platform, and testing.framework.
     This file is the human-readable LEGACY FALLBACK: agents and skills resolve
     each key from project.yaml first and fall back here only when the
     project.yaml key is absent. /setup-engine dual-writes both.
     Forbidden patterns and allowed libraries are NOT migrated — they live only
     in this file. Populated by /setup-engine; updated as decisions are made. -->

<!-- Written 2026-10-06 by /setup-engine (Unreal Engine 5.8.3, C++ primary).
     Dual-written with project.yaml — that file's engine/specialists/naming/
     commands blocks win on any conflict. -->

## Engine & Language

- **Engine**: Unreal Engine 5.8.3 (installed at `E:/UE_5.8` — Build 5.8.3, Changelist 58210709, promoted build)
- **Language**: C++ (primary) — Blueprint/UMG for gameplay prototyping and the rule-editor UI
- **Rendering**: Lumen + Nanite (deferred) — engine default; change via `/settings` if the project moves to a different setup
- **Physics**: Chaos — engine default

## Input & Platform

<!-- Written by /setup-engine. Read by /ux-design, /ux-review, /test-setup, /team-ui, and /dev-story -->
<!-- to scope interaction specs, test helpers, and implementation to the correct input methods. -->

- **Target Platforms**: PC (Steam / Epic) · single-player · **2D**
- **Input Methods**: Keyboard/Mouse
- **Primary Input**: Keyboard/Mouse — the rule editor and the visualization "planting layer" are pointer-dense operations
- **Gamepad Support**: Partial — **UNCONFIRMED**. This is the recommended default for a PC-only target, not a project decision. Revisit before UI work ships: gamepad support constrains every screen (d-pad navigation, no hover-only affordances), so it is not a bolt-on later.
- **Touch Support**: None
- **Platform Notes**: If gamepad support is confirmed, no hover-only interaction may be required without a keyboard/gamepad fallback. Separately: design text and code identifiers are Chinese/English mixed today — decide the shipped-string language before the rule editor's UI text is authored.

## Naming Conventions

<!-- Unreal column. NOTE: `classes` is incomplete by construction — UE requires a
     mandatory type prefix (U/A/F/E/I/T) that the flat enum in project.yaml cannot
     express. The authoritative form for this project is the prefixed one below. -->

- **Classes**: PascalCase **with mandatory UE type prefix** — `U` for UObject, `A` for Actor, `F` for struct, `E` for enum, `I` for interface, `T` for template (e.g. `UReactorSubstance`, `ARuleGrid`, `FCompositionNode`)
- **Variables**: PascalCase (e.g. `MoveSpeed`); booleans carry a `b` prefix (e.g. `bIsSealed`)
- **Signals/Events**: PascalCase `On<Event>` — UE has delegates and events, not signals; handler names are `On`-prefixed
- **Files**: PascalCase matching the class **without** the prefix (e.g. `RuleGrid.h` / `RuleGrid.cpp` for `ARuleGrid`)
- **Scenes/Prefabs**: PascalCase — in UE terms these are **Levels** and **Maps** (e.g. `L_Sandbox`, `L_Puzzle01`)
- **Constants**: SCREAMING_SNAKE — ⚠ **UNVERIFIED**: not sourceable from `docs/engine-reference/unreal/`; confirm before relying on it

## Performance Budgets

<!-- Left unset deliberately: the target simulation scale (grid size × rule count)
     is not decided, so any number here would be a guess. The handoff's own figures
     — 256×256 = 65,536 cells ≈ 3.9M updates/s at 60fps; 512×512 ≈ 15.7M updates/s
     — are capacity notes, not budgets. Set these once the settlement-engine
     prototype establishes a real grid size. -->

- **Target Framerate**: [TO BE CONFIGURED]
- **Frame Budget**: [TO BE CONFIGURED]
- **Draw Calls**: [TO BE CONFIGURED]
- **Memory Ceiling**: [TO BE CONFIGURED]

## Testing

- **Framework**: Automation Spec (Unreal's own C++ test framework)
- **Minimum Coverage**: [TO BE CONFIGURED]
- **Required Tests**: Balance formulas, gameplay systems, networking (if applicable)
- **Test naming**: name tests `Reactor.<System>.<Scenario>` — `commands.test` filters on the `Reactor.` substring, so a test named outside that root silently never runs

## Forbidden Patterns

<!-- Add patterns that should never appear in this project's codebase -->
- [None configured yet — add as architectural decisions are made]

## Allowed Libraries / Addons

<!-- Add approved third-party dependencies here -->
- [None configured yet — add as dependencies are approved]

## Architecture Decisions Log

<!-- Quick reference linking to full ADRs in docs/architecture/ -->
- `docs/architecture/adr-0001-composition-model.md` — Composition model: recursive encapsulation, cycle detection, depth ceiling, encapsulation dual output (Proposed)

## Engine Specialists

<!-- Written by /setup-engine when engine is configured. -->
<!-- Read by /code-review, /architecture-decision, /architecture-review, and team skills -->
<!-- to know which specialist to spawn for engine-specific validation. -->

- **Primary**: unreal-specialist
- **Language/Code Specialist**: unreal-specialist (C++); `ue-blueprint-specialist` for Blueprint graph architecture
- **Shader Specialist**: unreal-specialist (no dedicated shader specialist — primary covers materials)
- **UI Specialist**: ue-umg-specialist (UMG widgets, CommonUI, input routing, widget styling)
- **Additional Specialists**: ue-gas-specialist (Gameplay Ability System, attributes, gameplay effects), ue-blueprint-specialist (Blueprint graph architecture, BP/C++ boundary), ue-replication-specialist (property replication, RPCs, client prediction)
- **Routing Notes**: Invoke primary for C++ architecture and broad engine decisions. Invoke Blueprint specialist for Blueprint graph architecture and BP/C++ boundary design. Invoke GAS specialist for all ability and attribute code. Invoke replication specialist for multiplayer or networked systems — **not currently in scope for this project**. Invoke UMG specialist for all UI implementation; the rule editor is the largest UI surface in this game.

### File Extension Routing

<!-- Skills use this table to select the right specialist per file type. -->
<!-- If a row says [TO BE CONFIGURED], fall back to Primary for that file type. -->

| File Extension / Type | Specialist to Spawn |
|-----------------------|---------------------|
| Game code (.cpp, .h files) | unreal-specialist |
| Shader / material files (.usf, .ush, Material assets) | unreal-specialist |
| UI / screen files (UMG Widget Blueprints) | ue-umg-specialist |
| Scene / level files (.umap, .uasset) | unreal-specialist |
| Native extension / plugin files (Plugin .uplugin, modules) | unreal-specialist |
| Blueprint graphs (.uasset BP classes) | ue-blueprint-specialist |
| General architecture review | unreal-specialist |

# Unreal Engine 5.8 — Breaking Changes

**Last verified:** 2026-10-06

This document tracks breaking API changes and behavioral differences between Unreal Engine 5.3
(likely in model training) and Unreal Engine 5.8 (current version). Organized by risk level.

> **Reading this file.** The **`## 5.7 → 5.8`** section below is the newest span and the
> only one verified on 2026-10-06 (during the 5.7 → 5.8 pin). Everything after it is
> the carried-over 5.3 → 5.7 record from the previous pin, retained in full — a project
> migrating across two versions still needs the earlier span. Do not read the older
> spans' age as staleness; read the per-section verification dates.

---

## 5.7 → 5.8

**Last verified:** 2026-10-06
**Source:** UE 5.8 release notes, "Upgrade Notes" section —
https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-engine-5-8-release-notes?application_version=5.8
(the only source consulted; see `VERSION.md` for why no migration guide exists for this transition)

Every item below is taken from that page's own *Upgrade Notes* / *API Change* /
*Deprecated* entries. Where the release notes gave a version span rather than an
exact one, the span is reproduced rather than guessed at.

### Removal — 5.0–5.6 non-reflected, non-virtual symbols (C++)

**Breaking.** The release notes' own top-line upgrade note:

> Removed 5.0–5.6 non-reflected, non-virtual symbols (past the 2-version grace
> window; non-`UPROPERTY` / non-`UFUNCTION`): `Runtime/Eng…`

**What it means:** the engine's 2-version grace window expired. Symbols that were
non-reflected and non-virtual, introduced-or-lived across 5.0–5.6, are gone. A C++
project calling one of those gets a link/compile error, not a deprecation warning.

**Action for this project:** Reactor is C++-primary, so this is the item that can
actually break the build. No usage exists yet (the settlement engine is unwritten),
so the practical effect is: **when adopting any engine C++ snippet written against
5.0–5.6, do not assume it still links.** Verify against 5.8 headers.

### `UE_LOG` → `UE_LOGF`

**Upgrade note (Core):** these replace `UE_LOG`, which *will eventually be deprecated*;
convert `UE_LOG` to `UE_LOGF`. The release ships **`ConvertUELog.py`** to migrate most
uses safely.

**Action:** run the shipped converter rather than hand-editing; it exists to make this
mechanical. New code should be written `UE_LOGF`.

### `#if PLATFORM_*` → `#if UE_PLATFORM_*`

**Release notes label this "*Optional* migration":**

> Optional migration: Replace `#if PLATFORM_*` with `#if UE_PLATFORM_*`

**Why it matters (from the same note):** the define `PLATFORM_IOS` can be **stomped by
included iOS framework headers**, so code guarded by `#if PLATFORM_IOS` can be
*incorrectly evaluated* — a correctness bug, not just a rename. `UE_PLATFORM_IOS` and
its siblings are not subject to that.

**Action:** optional, but adopt `UE_PLATFORM_*` in new code. This project is PC-only
today, so it is not urgent — but it becomes mandatory if the platform surface grows.

### Core — implicit cast to `bool` removed

**API Change (Core):** the release notes state this **may cause a compile-time error
for existing code**, with the rationale that the implicit cast to `bool` "is most
likely not ever desired." Remedy offered: add an explicit cast, or implement a proper
`LexToString` for the type being converted.

**Action:** if a build fails on a conversion in 5.8, this is a first candidate — add the
explicit cast rather than reworking the type.

### `FSharedEventRef` — `EEventMode::ManualReset` now actually honored

**Behavioral, easy to miss.** Previously `FSharedEventRef` *always* used
`EEventMode::AutoReset`, regardless of what was passed to its constructor. In 5.8 the
constructor argument **works**.

**Why this is the dangerous shape:** it is a silent behavior change at existing call
sites. The old code did not fail to compile — it did something other than what the
call site asked for. Code that passed `ManualReset` and happened to rely on the buggy
`AutoReset` behavior changes meaning without any edit.

**Action:** in C++ code, check every `FSharedEventRef` construction — the release notes
explicitly say "please be sure to check your callsites."

### StateTree — several deprecations

- `FStateTreePropertyRef::GetMutablePtr` — *deprecated and refactored*
- `FindFrame` — *deprecated and refactored*
- `StateTreePropertyRefExternalHandle` — **deprecated**
- Execution context: now uses a **view instead of the property bag**; the property-bag
  version is deprecated. Described in the notes as "a step to use a permanent script
  struct instead of the transient property bag."
- `STATETREE_POD_INSTANCEDATA` removed in favor of
  `UE_STATETREE_CONSTRUCTED_TRIVIALLY_COPIED_NO_DESTRUCTOR_INSTANCEDATA` /
  `UE_STATETREE_ZEROED_TRIVIALLY_COPIED_NO_DESTRUCTOR_INSTANCEDATA`
- `EGenericAICheck` replaced by `UE::StateTree::EComparisonOperator` (an effort to make
  StateTree not depend on AI modules)

**Status in this project:** StateTree is **not** used. Recorded because it is cheap to
record and expensive to rediscover.

### Navigation — 5.5-era deprecated code removed

**Upgrade note (AI Navigation):** "Removed UE 5.5-era deprecated navigation code."

Not used in this project (2D grid simulation, no navmesh). Recorded for completeness.

### `LightWeightInstances` deprecated

**Deprecated (Framework):** "LightWeightInstances code marked as deprecated."

### DDC configuration moved into Engine config sections

**Upgrade note (Core):** Derived Data Cache graphs are now configured in
`[DerivedDataCacheGraphs]` and stores in `[DerivedDataCacheStores]` in the **Engine**
config, with `Base=` inheritance and `Deprecated="…"` support. `Deprecated AsyncPut`,
`KeyLength` and `Verify` graph nodes.

**Why this one is on the radar for this project:** the first headless boot on this
machine *died in the DDC layer* —
`Unable to use cache graph 'Installed' because it has no writable nodes available`
(exit 3). That is a cache-graph configuration problem, which is exactly the surface
this 5.8 change reworks. The mitigation adopted is `-DDC-ForceMemoryCache` in
`commands.smoke` / `commands.test`; if the cache is ever reconfigured properly, this
section is where to start reading.

### Python-defined UFUNCTION return values — packing order fixed

**API Change (Editor → Scripting):** a Python-defined ufunction with multiple return
types (e.g. `ufunction(ret=(int, str))`) must now pack values **in the order declared**,
rather than in reverse.

**Status in this project:** no Python-defined UFUNCTIONs today. The `.uproject` does
enable a large set of editor toolsets/plugins, so this is a plausible future surface
for editor automation — recorded so the reversal is not rediscovered as a bug.

### Editor — `SourceCodeNavigation.EnableAsync` cvar

**Upgrade note (Editor):** the `SourceCodeNavigation.EnableAsync` cvar can be set to
disable the now-async source navigation.

**Practical note for this project:** the build machine has **VS Build Tools only, no VS
IDE** (see `production/handoff-2026-10-05.md`), so IDE source navigation is moot until
an IDE is installed.

---

**Sources (5.7 → 5.8 span only):**
- https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-engine-5-8-release-notes?application_version=5.8

---

# Carried-over record — 5.3 → 5.7

**Last verified:** 2026-02-13 (previous pin; *not* re-verified on 2026-10-06)

## HIGH RISK — Will Break Existing Code

### Substrate Material System (Production-Ready in 5.7)
**Versions:** UE 5.5+ (experimental), 5.7 (production-ready)

Substrate replaces the legacy material system with a modular, physically accurate framework.

```cpp
// ❌ OLD: Legacy material nodes (still work but deprecated)
// Standard material graph with Base Color, Metallic, Roughness, etc.

// ✅ NEW: Substrate material layers
// Use Substrate nodes: Substrate Slab, Substrate Blend, etc.
// Modular material authoring with true physical accuracy
```

**Migration:** Enable Substrate in `Project Settings > Engine > Substrate` and rebuild materials using Substrate nodes.

---

### PCG (Procedural Content Generation) API Overhaul
**Versions:** UE 5.7 (production-ready)

PCG framework reached production-ready status with major API changes.

```cpp
// ❌ OLD: Experimental PCG API (pre-5.7)
// Old node types, unstable API

// ✅ NEW: Production PCG API (5.7+)
// Use FPCGContext, IPCGElement, new node types
// Stable API, production-ready workflow
```

**Migration:** Follow PCG migration guide in 5.7 docs. Expect significant refactoring for experimental PCG code.

---

### Megalights Rendering System
**Versions:** UE 5.5+

New lighting system supports millions of dynamic lights.

```cpp
// ❌ OLD: Limited dynamic lights (clustered forward shading)
// Max ~100-200 dynamic lights before performance degrades

// ✅ NEW: Megalights (5.5+)
// Millions of dynamic lights with minimal performance cost
// Enable: Project Settings > Engine > Rendering > Megalights
```

**Migration:** No code changes needed, but lighting behavior may differ. Test scenes after enabling.

---

## MEDIUM RISK — Behavioral Changes

### Enhanced Input System (Now Default)
**Versions:** UE 5.1+ (recommended), 5.7 (default)

Enhanced Input is now the default input system.

```cpp
// ❌ OLD: Legacy input bindings (deprecated)
InputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);

// ✅ NEW: Enhanced Input
SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) {
    UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent);
    EIC->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
}
```

**Migration:** Replace legacy input bindings with Enhanced Input actions.

---

### Nanite Default Enabled
**Versions:** UE 5.0+ (optional), 5.7 (encouraged)

Nanite virtualized geometry is now the recommended workflow for static meshes.

```cpp
// Enable Nanite on static mesh:
// Static Mesh Editor > Details > Nanite Settings > Enable Nanite Support
```

**Migration:** Convert high-poly meshes to Nanite. Test performance on target platforms.

---

## LOW RISK — Deprecations (Still Functional)

### Legacy Material System
**Status:** Deprecated but supported
**Replacement:** Substrate Material System

Legacy materials still work, but Substrate is recommended for new projects.

---

### Old World Partition (UE4 Style)
**Status:** Deprecated
**Replacement:** World Partition (UE5+)

Use UE5's World Partition system for large worlds.

---

## Platform-Specific Breaking Changes

### Windows
- **UE 5.7**: DirectX 12 is now default (was DX11 in older versions)
- Update shaders for DX12 compatibility

### macOS
- **UE 5.5+**: Metal 3 required (minimum macOS 13)

### Mobile
- **UE 5.7**: Minimum Android API level raised to 26 (Android 8.0)
- Minimum iOS deployment target raised to iOS 14

---

## Migration Checklist

### 5.7 → 5.8 (verified 2026-10-06)

- [ ] Run the shipped `ConvertUELog.py` to convert `UE_LOG` → `UE_LOGF`
- [ ] Audit every `FSharedEventRef` construction — `EEventMode::ManualReset` now works, and code relying on the old always-`AutoReset` behavior changes meaning silently
- [ ] Prefer `#if UE_PLATFORM_*` over `#if PLATFORM_*` in new code (the old form can be stomped by platform framework headers)
- [ ] Expect possible implicit-`bool` compile errors; fix with an explicit cast or a `LexToString`
- [ ] Do not assume a 5.0–5.6 engine C++ snippet still links — non-reflected, non-virtual symbols were removed
- [ ] If headless boot fails in the DDC layer, read the DDC-config change above before debugging anything else

### 5.3 → 5.7 (carried over from the previous pin)

- [ ] Review Substrate materials (convert if ready for new system)
- [ ] Audit PCG usage (update to production API if using experimental)
- [ ] Test Megalights performance (enable and benchmark)
- [ ] Migrate legacy input to Enhanced Input
- [ ] Convert high-poly meshes to Nanite
- [ ] Update shaders for DX12 (Windows) or Metal 3 (macOS)
- [ ] Verify minimum platform versions (Android 8.0, iOS 14)
- [ ] Test Lumen and Nanite performance on target hardware

---

**Sources:**
- **5.7 → 5.8:** https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-engine-5-8-release-notes?application_version=5.8
- 5.3 → 5.7 (carried over): https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-engine-5-7-release-notes
- 5.3 → 5.7 (carried over): https://dev.epicgames.com/documentation/en-us/unreal-engine/upgrading-projects-to-newer-versions-of-unreal-engine

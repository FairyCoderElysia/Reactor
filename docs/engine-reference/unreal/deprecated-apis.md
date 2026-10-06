# Unreal Engine 5.8 — Deprecated APIs

**Last verified:** 2026-10-06

Quick lookup table for deprecated APIs and their replacements.
Format: **Don't use X** → **Use Y instead**

> **Reading this file.** The **`## 5.7 → 5.8`** section immediately below is the newest
> span and the only one verified on 2026-10-06. Everything after the
> `# Carried-over record` divider is the 5.3 → 5.7 table from the previous pin,
> retained in full.

---

## 5.7 → 5.8

**Last verified:** 2026-10-06
**Source:** UE 5.8 release notes, "Upgrade Notes" / "Deprecated" entries —
https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-engine-5-8-release-notes?application_version=5.8

### Logging

| Don't use | Use instead | Notes |
|-----------|-------------|-------|
| `UE_LOG` | `UE_LOGF` | 5.8 release notes: these replace `UE_LOG`, "which will eventually be deprecated." Epic ships **`ConvertUELog.py`** to convert most uses safely — use it rather than hand-editing |

### Platform macros

| Don't use | Use instead | Notes |
|-----------|-------------|-------|
| `#if PLATFORM_IOS` (and `PLATFORM_*` siblings) | `#if UE_PLATFORM_IOS` (and `UE_PLATFORM_*`) | Release notes label this an *optional* migration, but give a correctness reason: `PLATFORM_IOS` **can be stomped by included iOS framework headers**, causing guarded code to be evaluated incorrectly. `UE_PLATFORM_*` cannot |

### StateTree

| Don't use | Use instead | Notes |
|-----------|-------------|-------|
| `FStateTreePropertyRef::GetMutablePtr` | (refactored — see 5.8 API reference) | Deprecated and refactored |
| `FindFrame` | (refactored — see 5.8 API reference) | Deprecated and refactored |
| `StateTreePropertyRefExternalHandle` | (see 5.8 API reference) | Deprecated |
| Property-bag execution context | Execution context **view** | Property-bag version deprecated; notes describe it as "a step to use a permanent script struct instead of the transient property bag" |
| `STATETREE_POD_INSTANCEDATA` | `UE_STATETREE_CONSTRUCTED_TRIVIALLY_COPIED_NO_DESTRUCTOR_INSTANCEDATA` / `UE_STATETREE_ZEROED_TRIVIALLY_COPIED_NO_DESTRUCTOR_INSTANCEDATA` | "POD" has different meaning in C vs C++ (pre-2020); "trivial" is the precise term |
| `EGenericAICheck` (from AIModule) | `UE::StateTree::EComparisonOperator` | Effort to make StateTree independent of AI modules |

> The release notes name the replacement for the last two rows explicitly but only
> say "refactored" for `GetMutablePtr` / `FindFrame`. No replacement signature is
> recorded here because none was stated on the page fetched — see `VERSION.md`'s
> sourcing rule. Read the 5.8 API reference for those two.

### Framework

| Don't use | Use instead | Notes |
|-----------|-------------|-------|
| `LightWeightInstances` | (no replacement named in the notes) | Marked deprecated in 5.8 |

### Removed (not merely deprecated)

| Removed | Notes |
|---------|-------|
| 5.0–5.6 non-reflected, non-virtual symbols | Removed past the 2-version grace window (non-`UPROPERTY` / non-`UFUNCTION`). A C++ project calling one gets a **compile/link error**, not a warning. This is the item most likely to break a C++ build |
| UE 5.5-era deprecated navigation code | Removed in 5.8 |

### Changed behavior — not a deprecation, but the same maintenance risk

| API | Change | Watch out for |
|-----|--------|---------------|
| `FSharedEventRef` constructor's `EEventMode` argument | **Now honored.** Previously always used `EEventMode::AutoReset` regardless of the argument passed | Existing call sites that passed `ManualReset` silently change meaning. The release notes say explicitly to check callsites |
| Implicit cast to `bool` | Removed (Core) | May be a compile error; remedy is an explicit cast or implementing `LexToString` |

### Config location change

| Old location | New location | Notes |
|--------------|--------------|-------|
| DDC graph/store config (previous scheme) | `[DerivedDataCacheGraphs]` and `[DerivedDataCacheStores]` in the **Engine** config | Supports `Base=` inheritance and `Deprecated="…"`. `Deprecated AsyncPut`, `KeyLength`, `Verify` graph nodes |

> **Relevant to this project:** the first headless boot on the dev machine aborted in
> the DDC layer (`Unable to use cache graph 'Installed' because it has no writable
> nodes available`, exit 3). Worked around with `-DDC-ForceMemoryCache`. If the cache
> is ever configured properly, start here.

---

# Carried-over record — 5.3 → 5.7

**Last verified:** 2026-02-13 (previous pin; *not* re-verified on 2026-10-06)

## Input

| Deprecated | Replacement | Notes |
|------------|-------------|-------|
| `InputComponent->BindAction()` | Enhanced Input `BindAction()` | New input system |
| `InputComponent->BindAxis()` | Enhanced Input `BindAxis()` | New input system |
| `PlayerController->GetInputAxisValue()` | Enhanced Input Action Values | New input system |

**Migration:** Install Enhanced Input plugin, create Input Actions and Input Mapping Contexts.

---

## Rendering

| Deprecated | Replacement | Notes |
|------------|-------------|-------|
| Legacy material nodes | Substrate material nodes | Substrate is production-ready in 5.7 |
| Forward shading (default) | Deferred + Lumen | Lumen is default in UE5 |
| Old lighting workflow | Lumen Global Illumination | Real-time GI |

---

## World Building

| Deprecated | Replacement | Notes |
|------------|-------------|-------|
| UE4 World Composition | World Partition (UE5) | Streaming large worlds |
| Level Streaming Volumes | World Partition Data Layers | Better level streaming |

---

## Animation

| Deprecated | Replacement | Notes |
|------------|-------------|-------|
| Old animation retargeting | IK Rig + IK Retargeter | UE5 retargeting system |
| Legacy control rig | Control Rig 2.0 | Production-ready rigging |

---

## Gameplay

| Deprecated | Replacement | Notes |
|------------|-------------|-------|
| `UGameplayStatics::LoadStreamLevel()` | World Partition streaming | Use Data Layers |
| Hardcoded input bindings | Enhanced Input system | Rebindable, modular input |

---

## Niagara (VFX)

| Deprecated | Replacement | Notes |
|------------|-------------|-------|
| Cascade particle system | Niagara | Cascade is fully deprecated |

---

## Audio

| Deprecated | Replacement | Notes |
|------------|-------------|-------|
| Old audio mixer | MetaSounds | Procedural audio system |
| Sound Cue (for complex logic) | MetaSounds | More powerful, node-based |

---

## Networking

| Deprecated | Replacement | Notes |
|------------|-------------|-------|
| `DOREPLIFETIME()` (basic) | `DOREPLIFETIME_CONDITION()` | Conditional replication for optimization |

---

## C++ Scripting

| Deprecated | Replacement | Notes |
|------------|-------------|-------|
| `TSharedPtr<T>` for UObjects | `TObjectPtr<T>` | UE5 type-safe pointers |
| Manual RTTI checks | `Cast<T>()` / `IsA<T>()` | Type-safe casting |

---

## Quick Migration Patterns

### Input Example
```cpp
// ❌ Deprecated
void AMyCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) {
    PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
}

// ✅ Enhanced Input
#include "EnhancedInputComponent.h"

void AMyCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) {
    UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent);
    if (EIC) {
        EIC->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
    }
}
```

### Material Example
```cpp
// ❌ Deprecated: Legacy material
// Use standard material graph (still works but not recommended)

// ✅ Substrate Material
// Enable: Project Settings > Engine > Substrate > Enable Substrate
// Use Substrate nodes in material editor
```

### World Partition Example
```cpp
// ❌ Deprecated: Level streaming volumes
// Load/unload levels manually

// ✅ World Partition
// Enable: World Settings > Enable World Partition
// Use Data Layers for streaming
```

### Particle System Example
```cpp
// ❌ Deprecated: Cascade
UParticleSystemComponent* PSC = CreateDefaultSubobject<UParticleSystemComponent>(TEXT("Particles"));

// ✅ Niagara
UNiagaraComponent* NiagaraComp = CreateDefaultSubobject<UNiagaraComponent>(TEXT("Niagara"));
```

### Audio Example
```cpp
// ❌ Deprecated: Sound Cue for complex logic
// Use Sound Cue editor nodes

// ✅ MetaSounds
// Create MetaSound Source asset, use node-based audio
```

---

## Summary: UE 5.7 Tech Stack

| Feature | Use This (2026) | Avoid This (Legacy) |
|---------|------------------|----------------------|
| **Input** | Enhanced Input | Legacy Input Bindings |
| **Materials** | Substrate | Legacy Material System |
| **Lighting** | Lumen + Megalights | Lightmaps + Limited Lights |
| **Particles** | Niagara | Cascade |
| **Audio** | MetaSounds | Sound Cue (for logic) |
| **World Streaming** | World Partition | World Composition |
| **Animation Retarget** | IK Rig + Retargeter | Old Retargeting |
| **Geometry** | Nanite (high-poly) | Standard Static Mesh LODs |

---

**Sources:**
- https://docs.unrealengine.com/5.7/en-US/deprecated-and-removed-features/
- https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-engine-5-7-release-notes

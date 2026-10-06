# Unreal Engine 5.8 — Current Best Practices

**Last verified:** 2026-10-06

Modern UE5 patterns that may not be in the LLM's training data.
These are production-ready recommendations as of UE 5.8.

> **Reading this file.** The **`## 5.7 → 5.8`** section immediately below is the newest
> span and the only one verified on 2026-10-06. Everything after the
> `# Carried-over record` divider is the 5.3 → 5.7 guidance from the previous pin,
> retained in full.

---

## 5.7 → 5.8

**Last verified:** 2026-10-06
**Sources:** UE 5.8 release notes (Upgrade Notes + feature sections) —
https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-engine-5-8-release-notes?application_version=5.8
; build toolchain items below are **first-hand observations from this machine**, marked as such.

### Build toolchain — what actually blocks a build on this machine

These are observations from running the commands, not documentation claims. Two
distinct failures, in the order they appear:

**1. `UnrealBuildTool.exe` requires .NET 10 in UE 5.8.**

The system-wide dotnet on the dev machine has runtimes 6.0 / 7.0 / 8.0 / 9.0.
Invoking `UnrealBuildTool.exe` directly fails with:

```
You must install or update .NET to run this application.
Framework: 'Microsoft.NETCore.App', version '10.0.0' (x64)
```

`UnrealBuildTool.runtimeconfig.json` requests `net10.0` with
`"rollForward": "LatestMajor"` — but `LatestMajor` cannot roll **forward past** the
installed set, so 9.x does not satisfy a 10.0 request.

The engine ships its own SDK at
`Engine/Binaries/ThirdParty/DotNet/10.0/win-x64`. `Build.bat` and `RunUAT.bat` find it
themselves via `Engine/Build/BatchFiles/GetDotnetPath.bat`, which sets
`DOTNET_ROOT`, prepends to `PATH`, sets `DOTNET_MULTILEVEL_LOOKUP=0` and
`DOTNET_ROLL_FORWARD=LatestMajor`.

**A direct `UnrealBuildTool.exe` invocation does not do any of that.** Set
`DOTNET_ROOT` to the bundled SDK path first:

```bash
export DOTNET_ROOT="E:/UE_5.8/Engine/Binaries/ThirdParty/DotNet/10.0/win-x64"
export PATH="$DOTNET_ROOT:$PATH"
export DOTNET_MULTILEVEL_LOOKUP=0
export DOTNET_ROLL_FORWARD=LatestMajor
```

This is why `project.yaml`'s `commands.build` carries a `# TODO` — it invokes UBT
directly, so the caller carries this burden.

**2. UBT needs write access to `%LOCALAPPDATA%\UnrealBuildTool\`.**

With .NET 10 resolved, UBT proceeds and then calls `BackupLogFile` on
`%LOCALAPPDATA%\UnrealBuildTool\Trace-backup-*.uba`. If that directory is not
writable, UBT dies with an unhandled `UnauthorizedAccessException` **before**
compiling anything:

```
Unhandled exception. System.UnauthorizedAccessException: Access to the path
'...\UnrealBuildTool\Trace-backup-<timestamp>.uba' is denied.
   at EpicGames.Core.Log.BackupLogFile(...)
```

Neither `-Log=` nor `-NoLogBackup` bypasses this — both were tried and the failure
was identical, which suggests the backup path is resolved independently of the log
path argument. Treat a writable `%LOCALAPPDATA%\UnrealBuildTool\` (or a
`%LOCALAPPDATA%` redirect) as a build prerequisite on Windows.

### Headless boot — a DDC failure that looks like a project failure

On this machine, a bare headless invocation **aborts at boot**:

```
LogDerivedDataCache: Display: ZenLocal: Unable to reach ZenServer HTTP service ...
LogDerivedDataCache: Warning: ... Failed to write to ...DerivedDataCache/... WriteError: 3
LogWindows: Error: Unable to use cache graph 'Installed' because it has no writable
nodes available. Add -DDC-ForceMemoryCache to the command line to bypass this ...
```

Observed exit code **3**. The engine's own error text names the remedy.

**Add `-DDC-ForceMemoryCache` to any headless `UnrealEditor-Cmd` invocation** on a
machine in this state, including automation-test runs — the failure happens during
boot, so a test command without it never reaches the tests. With the flag, the same
command exits **0** with a clean boot and shutdown (verified 2026-10-06).

This is a *machine* condition, not a project defect — which is exactly why it is worth
writing down: without this note the symptom reads as "the project does not build," and
the search goes in the wrong place.

### Megalights is production-ready in 5.8

5.8 promotes Megalights out of beta. The release notes cite noise reduction, a
performance target of **60fps**, and new debugging/optimization tooling. Added
feature support includes transmission (subsurface scattering), froxel-based
translucency, high-quality front-layer translucency lighting, IES for volumetrics and
translucency, lighting channels, and cloud shadows. New visualization views: light
finder, ray visualizer (ray iteration count), shadow caster mismatch.

**Project relevance:** low for Reactor — a 2D solid-colour grid does not need millions
of dynamic lights. Recorded because "Megalights is experimental" is now stale advice
and will be repeated from training data.

### Lumen Lite (Beta) — a cheaper GI tier

New medium-quality GI setting using irradiance fields with probe occlusion, described
as **~2× faster than Lumen high quality**. It targets 60fps and is the new default for
current-generation handheld consoles; also supported on PC.

**Project relevance:** this is the interesting one for a 2D game that nonetheless runs
the deferred renderer. If GI cost shows up in a frame-time profile, Lumen Lite is the
first dial to try — not disabling GI outright.

### Substrate Toon Shading (experimental) — NPR

New experimental stylized NPR solution built on the Substrate Blendable GBuffer
(legacy) mode. Supports all light types including local lights, sky lights and Lumen
GI. Exposes ramp-based diffuse/specular control with dithering, self-shadowing
extinction with hatching patterns, anisotropic specular highlights, and GI scale
controls, via a new **Substrate Toon BSDF** and **Toon Profile** asset.

**Project relevance:** potentially high. The brief's art direction is "深色底 + 高饱和
元素色，粒子/流体式可视化" — flat, stylized, high-saturation. Toon shading is the
closest documented fit for that, and it is built on Substrate rather than requiring a
custom shading model.

**Caveat:** it is experimental, and this project's rendering is already served by
solid-colour materials. Do not adopt it speculatively — the ADR-level decision for
"how do reactions get drawn" belongs with the visualization work.

### Substrate on 5.8 — smaller deltas

5.8 continues Substrate iteration rather than redefining it: rough diffuse BSDF moved
to the **EON model** (previously Chan), `SheenQuality` settings removed (Substrate now
always uses Sheen LTC), hair strands use Substrate lighting when enabled, and the
`HairComplexTransmittance` permutation was removed as no longer needed with
classification. Several fixes around adaptive GBuffer, multi-layer materials and
mobile clear coat.

**The 5.7-era guidance to "migrate to Substrate for new projects" still stands
unchanged.** The 5.8 items are refinements, not a second migration.

### Control Rig Dynamics — new lightweight solver

A new plugin providing a particle-based solver inside Control Rig for in-game
character simulation (cosmetic cloth, hair, accessories). The notes claim **5× runtime
performance improvement** over existing physics solutions, with fewer nodes and a
simplified authoring workflow. Control Rig **Physics** separately moved to Beta with
force-based functionality.

**Project relevance:** none directly — Reactor has no characters. Recorded because the
claim is large and easy to misapply.

### How to read version spans in the release notes

The 5.8 release notes' Upgrade Notes section contains items for **many** versions, not
just 5.7 → 5.8. The span is stated with each item (the symbol-removal note says
"5.0–5.6", the EventRef note describes prior behavior). **Read the stated span, not the
page's version** — otherwise years-old changes get attributed to 5.8 and tracked as new
work.

---

# Carried-over record — 5.3 → 5.7

**Last verified:** 2026-02-13 (previous pin; *not* re-verified on 2026-10-06)

## Project Setup

### Use UE 5.7 for New Projects
- Latest features: Megalights, production-ready Substrate and PCG
- Better performance and stability

### Choose the Right Rendering Features
- **Lumen**: Real-time global illumination (RECOMMENDED for most projects)
- **Nanite**: Virtualized geometry for high-poly meshes (RECOMMENDED for detailed environments)
- **Megalights**: Millions of dynamic lights (RECOMMENDED for complex lighting)
- **Substrate**: Modular material system (RECOMMENDED for new projects)

---

## Command Line — Build and Automation Tests (per platform)

Build the project's own editor target before running tests (a fresh checkout has
no compiled game module), then run the automation tests with the editor. Epic
recommends building `<Project>Editor` rather than `UnrealEditor`, so the modules
the `.uproject` disables stay disabled. Give the project as an absolute path.

| Step | Windows | Linux | macOS |
|------|---------|-------|-------|
| Build the editor target | `Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.exe <Project>Editor Win64 Development -Project="<abs>/<Project>.uproject"` | `Engine/Build/BatchFiles/Linux/Build.sh <Project>Editor Linux Development -Project="<abs>/<Project>.uproject"` | `Engine/Build/BatchFiles/Mac/Build.sh <Project>Editor Mac Development -Project="<abs>/<Project>.uproject"` |
| Editor executable for a command-line run | `Engine/Binaries/Win64/UnrealEditor-Cmd.exe` | `Engine/Binaries/Linux/UnrealEditor` | **NOT SOURCEABLE** — Epic documents only the `Engine/Binaries/Mac/UnrealEditor.app` bundle, not a command-line editor inside it |
| Packaged build | `Engine/Binaries/DotNET/AutomationTool/AutomationTool.exe BuildCookRun … -platform=Win64` | `Engine/Build/BatchFiles/RunUAT.sh BuildCookRun … -platform=Linux` | `Engine/Build/BatchFiles/RunUAT.sh BuildCookRun … -platform=Mac` |

The automation arguments are the same on every platform:
`"<abs>/<Project>.uproject" -ExecCmds="Automation RunTests <Project>.; Quit" -unattended -nullrhi -stdout -FullStdOutLogOutput`.
`RunTests` is a partial (substring) match. Each test prints
`Test Completed. Result={<status>}`, and the run ends with
`**** TEST COMPLETE. EXIT CODE: <n> ****` — exit code 0 means no test failed.
Only one `Automation` run request is allowed per `-ExecCmds`, and `-testexit=` is
legacy.

The Windows column was run on UE 5.7. The Linux and macOS columns come from
Epic's documentation and forum answers by Epic staff (sources below); Epic never
prints the full Linux test line, so it is assembled from the executable and the
platform-neutral arguments.

Sources:
- Build.sh on Linux and Mac, platform names — *Create an Installed Build*: https://dev.epicgames.com/documentation/en-us/unreal-engine/create-an-installed-build-of-unreal-engine?application_version=5.7
- `RunUBT.sh <Target> [Linux|Mac] Development` — *Unreal Insights*: https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-insights-in-unreal-engine?application_version=5.7
- `Engine/Binaries/Linux/UnrealEditor`, `Engine/Binaries/Mac/UnrealEditor.app` — *Onboarding Licensees*: https://dev.epicgames.com/documentation/en-us/unreal-engine/onboarding-licensees-in-unreal-engine?application_version=5.7 and *Linux Development Quickstart*: https://dev.epicgames.com/documentation/en-us/unreal-engine/linux-development-quickstart-for-unreal-engine?application_version=5.7
- `-nullrhi`, `-unattended`, `-stdout`, `-ExecCmds` — *Command-Line Arguments Reference*: https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-engine-command-line-arguments-reference?application_version=5.7
- Running tests from the command line — *Run Automation Tests*: https://dev.epicgames.com/documentation/en-us/unreal-engine/run-automation-tests-in-unreal-engine?application_version=5.7 ; result lines and exit code — *Review Test Results*: https://dev.epicgames.com/documentation/en-us/unreal-engine/review-test-results-in-unreal-engine?application_version=5.7
- `RunUAT.sh` from `Engine/Build/BatchFiles` on Mac/Linux — *Unreal Automation Tool Overview*: https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-automation-tool-overview-for-unreal-engine?application_version=5.7

---

## C++ Coding

### Use Modern C++ Features (C++20 in UE5.7)

```cpp
// ✅ Use TObjectPtr<T> (UE5 type-safe pointers)
UPROPERTY()
TObjectPtr<UStaticMeshComponent> MeshComp;

// ✅ Structured bindings
if (auto [bSuccess, Value] = TryGetValue(); bSuccess) {
    // Use Value
}

// ✅ Concepts and constraints (C++20)
template<typename T>
concept Damageable = requires(T t, float damage) {
    { t.TakeDamage(damage) } -> std::same_as<void>;
};
```

### Use UPROPERTY() for Garbage Collection

```cpp
// ✅ UPROPERTY ensures GC doesn't delete this
UPROPERTY()
TObjectPtr<AActor> MyActor;

// ❌ Raw pointers can become dangling
AActor* MyActor; // Dangerous! May be garbage collected
```

### Use UFUNCTION() for Blueprint Exposure

```cpp
// ✅ Callable from Blueprint
UFUNCTION(BlueprintCallable, Category="Combat")
void TakeDamage(float Damage);

// ✅ Implementable in Blueprint
UFUNCTION(BlueprintImplementableEvent, Category="Combat")
void OnDeath();
```

---

## Blueprint Best Practices

### Use Blueprint vs C++

- **C++**: Core gameplay systems, performance-critical code, low-level engine interaction
- **Blueprint**: Rapid prototyping, content creation, data-driven logic, designer workflows

### Blueprint Performance Tips

```cpp
// ✅ Use Event Tick sparingly (expensive)
// Prefer timers or events

// ✅ Use Blueprint Nativization (Blueprints → C++)
// Project Settings > Packaging > Blueprint Nativization

// ✅ Cache frequently accessed components
// Don't call GetComponent every tick
```

---

## Rendering (UE 5.7)

### Use Lumen for Global Illumination

```cpp
// Enable: Project Settings > Engine > Rendering > Dynamic Global Illumination Method = Lumen
// Real-time GI, no lightmap baking needed (RECOMMENDED)
```

### Use Nanite for High-Poly Meshes

```cpp
// Enable on Static Mesh: Details > Nanite Settings > Enable Nanite Support
// Automatically LODs millions of triangles (RECOMMENDED for detailed meshes)
```

### Use Megalights for Complex Lighting (UE 5.5+)

```cpp
// Enable: Project Settings > Engine > Rendering > Megalights = Enabled
// Supports millions of dynamic lights with minimal cost
```

### Use Substrate Materials (Production-Ready in 5.7)

```cpp
// Enable: Project Settings > Engine > Substrate > Enable Substrate
// Modular, physically accurate materials (RECOMMENDED for new projects)
```

---

## Enhanced Input System

### Setup Enhanced Input

```cpp
// 1. Create Input Action (IA_Jump)
// 2. Create Input Mapping Context (IMC_Default)
// 3. Add mapping: IA_Jump → Space Bar

// C++ Setup:
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"

void AMyCharacter::BeginPlay() {
    Super::BeginPlay();

    if (APlayerController* PC = Cast<APlayerController>(GetController())) {
        if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
            ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer())) {
            Subsystem->AddMappingContext(DefaultMappingContext, 0);
        }
    }
}

void AMyCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) {
    UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent);
    EIC->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
    EIC->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AMyCharacter::Move);
}

void AMyCharacter::Move(const FInputActionValue& Value) {
    FVector2D MoveVector = Value.Get<FVector2D>();
    AddMovementInput(GetActorForwardVector(), MoveVector.Y);
    AddMovementInput(GetActorRightVector(), MoveVector.X);
}
```

---

## Gameplay Ability System (GAS)

### Use GAS for Complex Gameplay

```cpp
// ✅ Use GAS for: Abilities, buffs, damage calculation, cooldowns
// Modular, scalable, multiplayer-ready

// Install: Enable "Gameplay Abilities" plugin

// Example Ability:
UCLASS()
class UGA_Fireball : public UGameplayAbility {
    GENERATED_BODY()

public:
    virtual void ActivateAbility(...) override {
        // Ability logic
        SpawnFireball();
        CommitAbility(); // Commit cost/cooldown
    }
};
```

---

## World Partition (Large Worlds)

### Use World Partition for Open Worlds

```cpp
// Enable: World Settings > Enable World Partition
// Automatically streams world cells based on player location

// Data Layers: Organize content (e.g., "Gameplay", "Audio", "Lighting")
// Runtime Data Layers: Load/unload at runtime
```

---

## Niagara (VFX)

### Use Niagara (Not Cascade)

```cpp
// Create: Content Browser > Right Click > FX > Niagara System
// GPU-accelerated, node-based particle system (RECOMMENDED)

// Spawn particles:
UNiagaraComponent* NiagaraComp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
    GetWorld(),
    ExplosionSystem,
    GetActorLocation()
);
```

---

## MetaSounds (Audio)

### Use MetaSounds for Procedural Audio

```cpp
// Create: Content Browser > Right Click > Sounds > MetaSound Source
// Node-based audio, replaces Sound Cue for complex logic (RECOMMENDED)

// Play MetaSound:
UAudioComponent* AudioComp = UGameplayStatics::SpawnSound2D(
    GetWorld(),
    MetaSoundSource
);
```

---

## Replication (Multiplayer)

### Server-Authoritative Pattern

```cpp
// ✅ Client sends input, server validates and replicates
UFUNCTION(Server, Reliable)
void Server_Move(FVector Direction);

void AMyCharacter::Server_Move_Implementation(FVector Direction) {
    // Server validates and applies movement
    AddMovementInput(Direction);
}

// ✅ Replicate important state
UPROPERTY(Replicated)
int32 Health;

void AMyCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const {
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AMyCharacter, Health);
}
```

---

## Performance Optimization

### Use Object Pooling

```cpp
// ✅ Reuse objects instead of Spawn/Destroy
//
// ⚠ INCONSISTENT WITH THIS FILE'S OWN RULE (flagged 2026-08-10).
// The GC section above states that an unmarked raw UObject pointer is
// "Dangerous! May be garbage collected" and requires UPROPERTY() +
// TObjectPtr<T>. This pooling example then holds raw AActor* in a bare TArray,
// which is exactly the shape that section forbids. FOLLOW THE RULE, NOT THIS
// SNIPPET: declare the pool UPROPERTY() TArray<TObjectPtr<AActor>>.
//
// Left visible rather than silently rewritten: the corrected form has not been
// verified against Epic's docs for 5.7, and this directory is the offline
// substitute for them -- editing a code sample here from memory is the failure
// mode the whole reference exists to prevent. Re-verify, then fix both together.
TArray<AActor*> ProjectilePool;

AActor* GetPooledProjectile() {
    for (AActor* Proj : ProjectilePool) {
        if (!Proj->IsActive()) {
            Proj->SetActive(true);
            return Proj;
        }
    }
    // Pool exhausted, spawn new
    return SpawnNewProjectile();
}
```

### Use Instanced Static Meshes

```cpp
// ✅ Hierarchical Instanced Static Mesh Component (HISM)
// Render thousands of identical meshes in one draw call
UHierarchicalInstancedStaticMeshComponent* HISM = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("Trees"));
for (int i = 0; i < 1000; i++) {
    HISM->AddInstance(FTransform(RandomLocation));
}
```

---

## Debugging

### Use Logging

```cpp
// ✅ Structured logging
UE_LOG(LogTemp, Warning, TEXT("Player health: %d"), Health);

// Custom log category
DECLARE_LOG_CATEGORY_EXTERN(LogMyGame, Log, All);
DEFINE_LOG_CATEGORY(LogMyGame);
UE_LOG(LogMyGame, Error, TEXT("Critical error!"));
```

### Use Visual Logger

```cpp
// ✅ Visual debugging
#include "VisualLogger/VisualLogger.h"

UE_VLOG_SEGMENT(this, LogTemp, Log, StartPos, EndPos, FColor::Red, TEXT("Raycast"));
UE_VLOG_LOCATION(this, LogTemp, Log, TargetLocation, 50.f, FColor::Green, TEXT("Target"));
```

---

## Summary: UE 5.7 Recommended Stack

| Feature | Use This (2026) | Notes |
|---------|------------------|-------|
| **Lighting** | Lumen + Megalights | Real-time GI, millions of lights |
| **Geometry** | Nanite | High-poly meshes, automatic LOD |
| **Materials** | Substrate | Modular, physically accurate |
| **Input** | Enhanced Input | Rebindable, modular |
| **VFX** | Niagara | GPU-accelerated |
| **Audio** | MetaSounds | Procedural audio |
| **World Streaming** | World Partition | Large open worlds |
| **Gameplay** | Gameplay Ability System | Complex abilities, buffs |

---

**Sources:**
- https://docs.unrealengine.com/5.7/en-US/
- https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-engine-5-7-release-notes

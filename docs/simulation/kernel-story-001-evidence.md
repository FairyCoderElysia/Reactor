# Story-001 evidence — settlement kernel and grid

**Story**: `production/epics/reactor/story-001-data-driven-kernel-and-grid.md`
**Date**: 2026-10-06
**Engine**: Unreal Engine 5.8.3 (`E:/UE_5.8`)
**Tier**: `minimal` — `qa.level: minimal` waives automated tests, so this document is
the substitute, not a test report. It records what was actually executed.

---

## 1. Compile

```
UnrealBuildTool.exe ReactorEditor Win64 Development -Project=<abs>/Reactor.uproject
```

**Result: Succeeded**, exit 0, 29.38s total (22.08s in UBA).

Three failures were hit and fixed before this passed — recorded because each is a
recurring trap rather than a one-off typo:

| Symptom | Cause | Fix |
|---|---|---|
| `fatal error C1083: cannot open 'Simulation/GridSimulation.h'` | Subdirectory-relative include from a file *inside* that subdirectory resolves to `Simulation/Simulation/…`. The module root was not on the include path. | `PublicIncludePaths.Add(ModuleDirectory)` in `Reactor.Build.cs` |
| `error C4430` / `C2146` on `FAutoConsoleCommandWithArgs` | **That class does not exist in UE 5.8.** The `AutoConsoleCommand` family was folded into `FAutoConsoleCommand`'s overloaded constructors (only the `WithWorld` / `WithOutputDevice` variants survive separately). | Use `FAutoConsoleCommand` with an `FConsoleCommandWithArgsDelegate` |
| 33 × `LNK2019` on `FJsonObject` / `FJsonValue` / `FJsonSerializerPolicy_JsonObject` | Code compiled, then failed to link: the `Json` module is not pulled in by `Core`. | Add `Json` + `JsonUtilities` to `PrivateDependencyModuleNames` |

The second is worth remembering as a pattern: reaching for the pre-5.8 name produced a
**compile error, not a deprecation warning**.

## 2. Two real logic bugs found by running it, not by reading it

### 2a. Double buffering was implemented backwards

`StepPass` cleared the write buffer (`CellsNext.Init(FSubstanceId(), …)`) instead of
seeding it from the read buffer. Every cell nobody wrote to therefore became empty in
the next frame, and **the entire grid emptied in one step**.

Caught by isolation, not by inspection: one cell per substance, far apart, stepped once.

```
isolated: before step,  5 non-empty
isolated: after 1 step, 0 distinct non-empty, changed=5    ← bug present
isolated: after 1 step, 5 distinct non-empty, changed=0    ← after the fix
```

The fix is `CellsNext = CellsPrev` at the top of `StepPass`, with the scan overwriting
only the cells a reaction touched. There is no stale-data hazard in copying there —
the whole buffer is rebuilt from `CellsPrev` every step — and reads still come only from
`CellsPrev`, so the sweep direction still cannot leak into the result.

**This also invalidated a performance measurement.** The first benchmark reported
`0.3375 ms/step` — it was timing a scan over an empty grid. The corrected figures are
~6.7× slower at 256×256. Recorded because the wrong number was plausible and would have
been believed.

### 2b. `TMap<FReactionPair, …>` in a `UPROPERTY` would not reflect

`FReactionPair` is a plain C++ type (deliberately — the ordered pair is the key, and order
matters), and UHT rejects a `TMap` whose key is not a `USTRUCT`. Split into a non-reflected
`FReactorSimulationData`; the editor never authors it, the JSON loader builds it, so no
reflection is lost.

## 3. Measured behaviour

`Reactor.DumpSimulationState 20261006` — grid 256×256, 55% density fill:

| | Water | Stone | Fire | Steam | Lava | total |
|---|---|---|---|---|---|---|
| after fill | 7,087 | 7,308 | 7,232 | 7,242 | 7,184 | 36,053 |
| after step 1 | 4,530 | 4,565 | 2,712 | 9,946 | 10,098 | 31,851 |
| after step 2 | unchanged | | | | | 31,851 |

Step 1: `changed=9820 reacted=9820`. Steps 2+: `changed=0 reacted=0`.

**The equilibrium is expected for this table, and is not a defect.** Every entry in
`DefaultSubstances.json` is a merge rule (`produceB=false`), so one step consumes every
reachable adjacent pair. Sustained, ongoing activity is what the *rule* table is for —
modifier/trigger/nesting, which is story-002 — not what a three-primitive reaction table
can produce. Recorded here so "it goes quiet after one step" is not later mistaken for a
regression.

## 4. Performance

`Reactor.BenchmarkSimulation` — 30 measured steps after 3 warm-up steps, measured with the
grid already at equilibrium (so the scan runs in full but resolves no matches):

| Grid | Cell-updates/step | ms/step | M updates/s | Share of a 16.67 ms frame |
|---|---|---|---|---|
| 256×256 (`workMultiplier=0`) | 65,536 | **2.2531** | 29.1 | 13.5% |
| 512×512 equivalent (`workMultiplier=3`) | 262,144 | **8.0367** | 32.6 | 48% |

The 512×512 figure is reached without allocating a 512×512 buffer by running three extra
whole-grid passes that perform the **same neighbour probes and table lookups** as the real
scan — a cheaper loop would make the extrapolation dishonest. `workMultiplier` is a
measurement knob and must stay `0` in any shipped config.

**Interpretation, kept separate from the measurement:** at the design's stated target
(256×256 · 60fps) the kernel is comfortable at 13.5% of the frame. At 512×512 it takes
48%, which is inside budget but leaves less room for rendering than the headroom number
alone suggests. Neither figure includes rendering or blueprints — this measures the
settlement loop only.

## 5. What is NOT verified

- **Determinism is not mechanically enforced.** The acceptance criterion "same input twice
  → identical result" was reasoned about (no wall-clock use, no shared PRNG stream,
  fixed neighbour probe order, `FRandomStream` seeded per cell) but **no test asserts it**,
  because `qa.level: minimal` waives test files. This is the single most valuable thing a
  later tier should pick up.
- **No rendering.** Nothing was drawn; that is story-004.
- **`commands.run` still opens the editor, not the game** — unchanged by this story.
- **Grid resize was not exercised at runtime.** The dimensions are read from JSON rather
  than compiled in, and `Initialize` uses them, but only 256×256 was actually built.

## 6. Reproducing

```bash
# Build (needs DOTNET_ROOT pointed at the engine's bundled .NET 10)
export DOTNET_ROOT="E:/UE_5.8/Engine/Binaries/ThirdParty/DotNet/10.0/win-x64"
"E:/UE_5.8/Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.exe" \
  ReactorEditor Win64 Development -Project="$(pwd -W)/Reactor.uproject"

# Behaviour
"E:/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "$(pwd -W)/Reactor.uproject" \
  -game -nullrhi -unattended -stdout -DDC-ForceMemoryCache \
  -ExecCmds="Reactor.DumpSimulationState 20261006, Quit"

# Performance
"E:/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "$(pwd -W)/Reactor.uproject" \
  -game -nullrhi -unattended -stdout -DDC-ForceMemoryCache \
  -ExecCmds="Reactor.BenchmarkSimulation 0 30, Quit"
```

`-DDC-ForceMemoryCache` is required on this machine; without it the editor aborts at boot
with `Unable to use cache graph 'Installed'` and exits 3.

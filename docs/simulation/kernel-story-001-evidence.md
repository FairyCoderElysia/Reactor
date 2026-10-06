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

`Reactor.BenchmarkSimulation` — 30 measured steps after 3 warm-up steps, with the grid
already at equilibrium (the scan runs in full but resolves no matches). **Three runs per
configuration**, because a single run proved insufficient: identical invocations of the
256×256 case varied by ~15%.

| Grid | Cell-updates/step | latest run | 3-run range | vs a 60Hz frame | vs baseline grid |
|---|---|---|---|---|---|
| 256×256 (`workMultiplier=0`) | 65,536 | **2.3787 ms** | 2.1252–2.4511 | 7.01× | 1.00× |
| 512×512 equivalent (`workMultiplier=3`) | 262,144 | **7.4621 ms** | 7.0986–7.9899 | 2.23× | 4.00× |

The frame ratio is now reported by the tool itself against
`FSimulationParams::TargetFramerate` (data-driven, from the JSON) rather than a literal
`1000.0 / 60.0` — see the ADVISORY deviation in the story's `## Completion Notes`. The
figures above are the latest run at each size; `project.yaml` carries the same two numbers.

The 512×512 figure is reached without allocating a 512×512 buffer, by running three extra
whole-grid passes that perform the **same neighbour probes and table lookups** as the real
scan — a cheaper loop would make the extrapolation dishonest. `workMultiplier` is a
measurement knob and must stay `0` in any shipped config.

**Interpretation, kept separate from the measurement:** at the design's stated target
(256×256 · 60fps) the kernel takes roughly 1/7 of the frame. At 512×512 it takes about
2.23× the frame budget's *inverse* — i.e. it fits in one 16.67 ms frame with about 2.2×
to spare, but that is no longer generous. Neither figure includes rendering, blueprints or
audio; this measures the settlement loop by itself.

> Two things this table had to be corrected for, both worth keeping. First, the very first
> figure recorded for this story (`0.3375 ms/step`) was **invalid** — it was measured while
> the double-buffer bug had emptied the grid, so it timed a scan over nothing. Second, the
> 2.2531 / 8.0367 pair that replaced it came from a **single run** each; a three-run sweep
> showed ~15% spread at 256×256, so a single sample is not a measurement. The numbers above
> are the latest run, with the range beside them.

> An earlier version of this section recorded `2.2531 ms` / `8.0367 ms`. Those came from a
> single run each and have been superseded by the three-run medians above. The very first
> figure recorded for this story — `0.3375 ms/step` — was **invalid**: it was measured
> while the double-buffer bug had emptied the grid, i.e. it timed a scan over nothing.


## 5. Determinism — enforced by test since 2026-10-06

The acceptance criterion "same input twice → identical result" was originally **only
reasoned about, not verified**, because `qa.level: minimal` waives test files. That gap was
closed the same day, and closing it paid for itself: **the reasoning was wrong in two
places, and only running the check found them.**

### The test

`Source/Reactor/Tests/ReactorSimulationDeterminismTest.cpp` — two Unreal Automation tests,
fed from an in-memory JSON fixture rather than `Content/`, so they cannot start failing
because someone tuned a designer-facing number. Naming follows
`<Project>.<System>.<Scenario>` for the `Reactor.` filter in `commands.test`.

| Test | What it asserts |
|---|---|
| `Reactor.Simulation.Determinism.RepeatedRunsMatch` | Same seed, 24 steps, compared **after every step** — a kernel that diverges and then re-converges would pass a final-only check |
| `Reactor.Simulation.Determinism.DifferentSeedsDiffer` | Two seeds must produce **different** initial grids — without this, a kernel that ignored the seed entirely (or returned a constant grid) would pass the test above |

Command:

```
UnrealEditor-Cmd.exe <abs>/Reactor.uproject -ExecCmds="Automation RunTests Reactor.; Quit" \
  -unattended -nullrhi -stdout -FullStdOutLogOutput -DDC-ForceMemoryCache
```

**Result: 2 tests found, 2 Success, `**** TEST COMPLETE. EXIT CODE: 0 ****`.**
This also exercises `commands.test` end to end for the first time.

### Bug 3 — `GenerateNewSeed()` silently discards a deterministic seed

```cpp
FRandomStream Stream(Seed + Index * 7919);   // seed set...
Stream.GenerateNewSeed();                    // ...then thrown away
```

`GenerateNewSeed()` is implemented as `Initialize(FMath::Rand())`, and `FMath::Rand()`
draws from the **process-global** RNG, seeded from platform entropy. Calling it after
setting a seed replaces that seed with a non-reproducible one.

Reported symptom: `Step 1: first divergence at index 0 ('Lava' vs 'None')`.

Fix: one stream for the whole fill, seeded once, drawn in fixed index order, no
`GenerateNewSeed()`. Single-threaded and order-fixed is reproducible; the per-cell stream
was more elaborate and bought nothing.

### Bug 4 — a floating-point comparison in the seed path

`const bool bOccupied = Stream.FRand() < Density;` is a **float** comparison. `FRand()`'s
low bits are not stable across runs (FMA contraction, optimisation settings), so a cell
sitting on the threshold flips — which changes how many draws the stream consumes
afterwards and re-rolls the substance for every later cell.

Fix: decide occupancy with integer arithmetic (`RandRange(1, 100) <= DensityPercent`).
`Density` stays a `float` in the public API but is quantised to whole percent before it
reaches any decision.

Reported symptom (surfaced after bug 3 was fixed): `Step 1: first divergence at index 1
('Steam' vs 'Lava')`.

> **Both bugs were in the *seeding* path, not the step path**, and both produced a
> plausible non-error: the simulation ran, reached equilibrium, and reported sensible
> occupancy numbers. Neither is visible by reading the code with the intent of finding a
> bug, and neither would have shown up in the performance benchmark — which is why
> revisiting the waived-test decision was worth doing rather than accepting it.

### Limits of what this proves

The test proves determinism **within one process and one build**. It does not prove
cross-platform or cross-compiler reproducibility, and it does not pin the fill to a golden
grid. Both would be needed before a shared puzzle file could be trusted across machines —
noted here rather than claimed.

## 6. Grid resize — verified 2026-10-06

Story 001 requires the grid dimensions to be configuration rather than compiled in. That was
true of the code from the start (`FSimulationParams` carries them, and `Initialize` reads
them), but only 256×256 had ever actually been built. `Reactor.ResizeCheck` closes the gap by
constructing a grid at an arbitrary size from the shipped table:

```
UnrealEditor-Cmd.exe <abs>/Reactor.uproject -game -nullrhi -unattended -stdout   -DDC-ForceMemoryCache -ExecCmds="Reactor.ResizeCheck <width> <height> [steps], Quit"
```

Only `GridWidth` / `GridHeight` are overridden — fixed step and reaction rate stay exactly as
`DefaultSubstances.json` has them, so this is a *resized* run rather than a differently-tuned one.

| Requested | Built | Cells | Occupied after fill | Step 1 | Final |
|---|---|---|---|---|---|
| 128×128 | 128×128 | 16,384 | 9,077 (5 substances) | changed=2,509 · reacted=2,509 | 8,004 |
| **512×512** | **512×512** | **262,144** | 144,080 (5) | changed=39,742 · reacted=39,742 | 127,127 |
| 1024×1024 | 1024×1024 | 1,048,576 | 576,717 (5) | changed=158,782 · reacted=158,782 | 508,841 |

Every size built, filled with all 5 substances, ran a full settlement step with a proportional
reaction count, and settled without error — **with no code change between runs**. The
population falls after step 1 in each case, which is the expected behaviour of this all-merge
reaction table (§3).

**What this does not prove:** that a *large* grid is cheap. 1024×1024 is 16× the baseline cell
count; the measured per-step cost at 512×512-equivalent load is 7.17 ms (§4), so 1024×1024
would be roughly 4× that — well past a 16.67 ms frame. The resize mechanism is sound; the
budget at that size is a separate question, and the answer is "not at 60fps in the current
kernel".

## 7. What is NOT verified

- **No rendering.** Nothing was drawn; that is story-004.
- ~~Grid resize was not exercised at runtime~~ — **closed 2026-10-06.** `Reactor.ResizeCheck`
  built and settled 128×128, 512×512 and 1024×1024 from the shipped table by overriding only
  the dimensions. See §6.
- **`commands.run` still opens the editor, not the game** — unchanged by this story.
- **Cross-platform determinism** — see §5's limits note above.

## 8. Reproducing

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

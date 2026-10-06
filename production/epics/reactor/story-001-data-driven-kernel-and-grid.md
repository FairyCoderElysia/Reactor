# Story 001: 数据驱动的结算内核与网格

> **Epic**: Reactor（Build order 第 1 步）
> **Status**: Complete
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: 3–5 天（`coarse`）
> **Manifest Version**: N/A (minimal — no control manifest)
> **Last Updated**: 2026-10-06

## Context

**GDD**: `design/game-brief.md`
**Requirement**: `Brief MVP feature 1` — 一个 2D 空间 + 3–5 种基础物质（含两两反应表）

**ADR Governing Implementation**: `ADR: N/A — 数据加载与网格数据结构是纯粹的基础设施，没有架构模式争议；本故事的决策点（双缓冲、确定性、数据驱动）已在 `production/handoff-2026-10-05.md` 的架构约定里定下，无需 ADR`
**ADR Decision Summary**: N/A (minimal — no ADRs)
**ADR Version**: N/A (minimal — no ADRs)

**Engine**: Unreal Engine 5.8.3 | **Risk**: HIGH
**Engine Notes**: `docs/engine-reference/unreal/VERSION.md` 给 5.8 行标 HIGH（远超 LLM 训练截止 ~5.3）。
5.8 相关注意：`UE_LOG` → `UE_LOGF`（官方附 `ConvertUELog.py`）；不要假设 5.0–5.6 的引擎 C++ 片段仍能链接（非反射非虚符号已移除）。
**本地构建注意**：UE 5.8 的 `UnrealBuildTool.exe` 是 .NET 10 应用，系统只有 6–9 —— 直调前需把 `DOTNET_ROOT` 指向
`Engine/Binaries/ThirdParty/DotNet/10.0/win-x64`。另：任何 headless 启动都要带 `-DDC-ForceMemoryCache`，否则启动即 EXIT=3。

**Control Manifest Rules (this layer)**:
- Required: N/A (minimal — no control manifest)
- Forbidden: N/A (minimal — no control manifest)
- Guardrail: N/A (minimal — no control manifest)

---

## Acceptance Criteria

*From `design/game-brief.md`（the **Player goal & fail state** field + the MVP feature this story implements），scoped to this story:*

- [x] 三张数据表的字段与加载路径确定，格式写入 `docs/`：物质表（id / 显示名 / 颜色 / `bIsPrimitive`）、反应表（两两相遇产生什么）、**参数块（网格尺寸 / 固定步长 / 反应速率等可调项）**
- [x] 物质表初始含 **3 种**基础物质，反应表覆盖它们的两两组合
- [x] 改一条反应或一个参数，**不重新编译**即生效（实测：改文件 → 跑 → 看到行为差异）

> **规则表（条件 + 效果）不在本故事范围内** —— 它属 MVP 第 2/3 条，归 `story-002`。
> 本故事只做 MVP 第 1 条（空间 + 基础物质 + **两两反应表**）。
> 早先这个故事曾把"规则表 3 条规则"写进 AC，那是**作者把 MVP 编号标错了**，
> 不是实现漏项；已改正，规则表并入 story-002。
- [x] 结算循环以**固定步长**前进，不随帧率漂移
- [x] 相邻两 tick 之间状态可观察地变化（东西确实在动）
- [x] **确定性**：同一初始状态跑两次，得到逐格相同的结果（自动化测试断言）
- [x] `Automation Spec` 测试存在，命名以 `Reactor.` 开头（`commands.test` 按此子串过滤）
- [x] 基准网格 `256×256`（65,536 格）可创建；**扩大或缩小只改配置，不改代码**
  —— 已实跑 128×128 / 512×512 / 1024×1024（见下），代码未改动一行
- [x] **产出第一个真实性能数字**：基准网格下每 tick 耗时实测值，填入 `project.yaml` 的 `performance.*`（此前刻意留空 —— 现在有真数据了）
- [x] `commands.build` 编译通过

## Implementation Notes

本故事不引用 ADR。以下约束来自 `production/handoff-2026-10-05.md` 已定的**架构约定**与 `design/composition-model.md`：

- **数据驱动是硬性约定**：规则表 / 物质反应表 / 参数全放数据文件或蓝图可编辑变量。理由是 UE 编译很贵，而本作核心玩法就是反复调规则 —— 调一条规则要等重编译会直接杀死原型阶段。
- **双缓冲**：读上一帧的状态、写下一帧的状态。单缓冲会让结算顺序污染结果，且破坏确定性。
- **`bIsPrimitive` 现在就要有**：它是 `composition-model.md` 里**递归基例**的落点。即使本故事还用不到递归，这个字段必须现在存在 —— 否则以后加递归要改数据格式与所有已存文件。
- **网格尺寸是配置项，不是常量**：`256×256` 是基准，必须能扩到 `512×512`（26 万格）而不改代码。
- **不要在这里实现闸**：环检测与深度上限属于 `story-003`，且属**封装提交路径**。本故事只需结算循环自身跑得对。

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 002: 规则组合（修饰 / 触发 / 嵌套）与归因记录 —— 本故事只做"规则生效"，不做"规则之间有结构"
- Story 003: 递归的三道闸（基例 / 环检测 / 深度上限）
- Story 004: 可视化与可归因的运行 —— 本故事不产出任何画面

## QA Test Cases

*N/A — no qa-lead specs at this tier; implement against the Acceptance Criteria above*

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived**（advisory, never "must exist and pass"）*

**Story Type**: Logic
**Required evidence**:
- Logic: `Source/Reactor/Tests/ReactorSimulationDeterminismTest.cpp` — must exist and pass
  （`/story-done` 只检查它**存在**；通过与否由 `/gate-check` / `/smoke-check` 判定，两者都在更后面）

> **路径为什么是 Unreal 的形式而不是 `tests/unit/`。** 框架的通用模板给的是
> `tests/unit/[system]/[story-slug]_test.[ext]`，但各引擎的测试根不同 ——
> `/dev-story` SKILL.md 自己写明 Godot 是 `tests/`、Unity 是 `Assets/Tests/`、
> **Unreal 是 `Source/<Module>/Private/Tests/`**。本项目的测试放在
> `Source/Reactor/Tests/`（UBT 会自动发现模块内任意位置的 Automation 测试，无需改 `Build.cs`）。
> 留这段是因为合同把"路径不匹配"列为 Known Fragile Point：`/gate-check` 与
> `/smoke-check` 会照这个字面路径去找，写错就会得到假的 BLOCKING。

**Status**: [x] Evidence recorded 2026-10-06 — see below

**Test file**: `Source/Reactor/Tests/ReactorSimulationDeterminismTest.cpp`
**Result**: 2 tests, **2 Success**, `**** TEST COMPLETE. EXIT CODE: 0 ****`

> **This story originally shipped with no test**, because `qa.level: minimal` waives them
> (dev-story CONTRACT.md L50/L68) and the contract permits a Logic story to close without
> one. The determinism criterion below was therefore only *reasoned about*. That was
> revisited on 2026-10-06 and the reasoning turned out to be **wrong in two places** — both
> bugs were in the seeding path and both produced a plausible non-error. The test found
> them in minutes. See `docs/simulation/kernel-story-001-evidence.md` §5.

**What was run** — the full evidence set, replacing the waived-test gap:

1. **Compile**: `UnrealBuildTool ReactorEditor Win64 Development` → `Result: Succeeded`, exit 0.
   Three compile/link failures were found and fixed on the way (module include path,
   `FAutoConsoleCommandWithArgs` does not exist in 5.8, missing `Json` module dependency).
2. **Automated tests**: `commands.test` (`Automation RunTests Reactor.`) → 2 Success, exit 0.
   Determinism, compared after every step, plus a negative control that two seeds differ.
3. **Behavioural isolation**: `Reactor.DumpSimulationState` — one lone cell per substance,
   stepped twice: all 5 survive. Before the double-buffer fix this reported `0 distinct
   non-empty` after one step, which is how that bug was caught.
4. **Resize**: `Reactor.ResizeCheck` at three sizes — 128×128 (16,384 cells), **512×512
   (262,144)**, 1024×1024 (1,048,576). Each built the requested size, filled to 5 substances,
   ran a full settlement step, and settled without error. Only the dimensions were overridden;
   fixed step and reaction rate stayed as the data file has them.
5. **Settling behaviour**: seeded grid of ~7,200 cells per substance → step 1 changes
   9,820 cells with 9,820 reaction matches; the table then reaches equilibrium. Recorded,
   not hidden: this reaction table is all merge rules, so one step consumes every reachable
   pair. Sustained activity is the rule table's job (story-002), not this table's.
6. **Performance**: `Reactor.BenchmarkSimulation`, 3 runs per configuration → the medians
   now in `project.yaml`'s `performance` block.

**In-repo evidence document**: `docs/simulation/kernel-story-001-evidence.md`

## Dependencies

- Depends on: None
- Unlocks: Story 002（规则组合与归因记录）

---

## Completion Notes

**Completed**: 2026-10-06
**Criteria**: 10/10 passing — 每一条都由实跑验证，无一条来自"读代码推断"
（`commands.build` Result: Succeeded · `commands.test` 2/2 Success · `Reactor.ResizeCheck`
128×128 / 512×512 / 1024×1024 三尺寸实跑 · `Reactor.DumpSimulationState` 观察 · `Reactor.BenchmarkSimulation` 实测）
**Deviations**: 两条 ADVISORY，均已在关闭前修复 ——
  1. `SimulationBenchmark.cpp` 曾硬编码 `1000.0 / 60.0` 计算帧预算，改配置后会静默过期。
     已改为从 `FSimulationParams::TargetFramerate`（数据驱动）读取，benchmark 报告也随之改写为
     `x.xx the configured 60Hz frame`，并新增 `targetFramerate` 到 `DefaultSubstances.json`。
  2. `## Test Evidence` 声明的路径（框架模板的 `tests/unit/…`）与实际位置不符 ——
     已改为真实的 Unreal 路径，并附说明（见该节）。
**Test Evidence**: Logic：`Source/Reactor/Tests/ReactorSimulationDeterminismTest.cpp`
（2 tests：`RepeatedRunsMatch` 逐步比对确定性 · `DifferentSeedsDiffer` 负向对照）
**Code Review**: Skipped — `modes.review_mode` 由 `rigor: minimal` 展开为 `solo`（Phase 5 跳过），
且 `minimal` 档路径不含 `/code-review`。

### 关闭时的诚实附注

- **确定性测试不在合同要求范围内。** `qa.level: minimal` 下 Logic 故事的测试证据门**被跳过**
  （`story-done` SKILL.md L50–62、L208–213），本故事本可**不带任何测试**结案。测试是我主动补的，
  而它**抓出了 2 个真 bug**（`GenerateNewSeed()` 丢弃确定性种子、占用判定用浮点比较）——
  两个都产生"看起来完全正常"的结果，读代码看不出、性能基准也测不到。
  记录在此是因为它是一条关于本档位的经验事实：**升 `standard` 时最值得先把
  `testing.strict.logic: true` 打开。**
- **证据文档 §7 列明了本故事未覆盖的范围**：无渲染（属 story-004）、跨平台/跨编译器确定性未证明、
  `commands.run` 仍指向编辑器而非游戏。

### 后续

下一个故事：`story-002-rule-composition-and-attribution.md`（`Status: Ready`，不被 ADR 阻塞）。

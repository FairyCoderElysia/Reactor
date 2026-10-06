# Story 001: 数据驱动的结算内核与网格

> **Epic**: Reactor（Build order 第 1 步）
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: 3–5 天（`coarse`）
> **Manifest Version**: N/A (minimal — no control manifest)
> **Last Updated**: —

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

- [ ] 三张数据表的字段与加载路径确定，格式写入 `docs/`：物质表（id / 显示名 / 颜色 / `bIsPrimitive`）、反应表（两两相遇产生什么）、规则表（条件 + 效果 + 可调参数）
- [ ] 物质表初始含 **3 种**基础物质，反应表覆盖它们的两两组合
- [ ] 规则表初始含 **3 条**规则
- [ ] 改一条规则或一条反应，**不重新编译**即生效（实测：改文件 → 跑 → 看到行为差异）
- [ ] 结算循环以**固定步长**前进，不随帧率漂移
- [ ] 相邻两 tick 之间状态可观察地变化（东西确实在动）
- [ ] **确定性**：同一初始状态跑两次，得到逐格相同的结果（自动化测试断言）
- [ ] `Automation Spec` 测试存在，命名以 `Reactor.` 开头（`commands.test` 按此子串过滤）
- [ ] 基准网格 `256×256`（65,536 格）可创建；**扩大或缩小只改配置，不改代码**
- [ ] **产出第一个真实性能数字**：基准网格下每 tick 耗时实测值，填入 `project.yaml` 的 `performance.*`（此前刻意留空 —— 现在有真数据了）
- [ ] `commands.build` 编译通过

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
- Logic: `tests/unit/settlement/[story-slug]_test.[ext]` — must exist and pass（`/story-done` 只检查它**存在**；通过与否由 `/gate-check` / `/smoke-check` 判定，两者都在更后面）

**Status**: [x] Evidence recorded 2026-10-06 — see below

**Test evidence: waived** at `qa.level: minimal` — no test was required or written for this story.

**What was actually run instead** (the substitutes that did happen, named so a
minimal-tier summary is not mistaken for a standard-tier one where tests were forgotten):

1. **Compile**: `UnrealBuildTool ReactorEditor Win64 Development` → `Result: Succeeded`, exit 0, 29.4s.
   Three compile/link failures were found and fixed on the way (module include path,
   `FAutoConsoleCommandWithArgs` does not exist in 5.8, missing `Json` module dependency).
2. **Behavioural isolation**: `Reactor.DumpSimulationState` — one lone cell per substance,
   stepped twice: all 5 survive (`changed=0`). Before the double-buffer fix this reported
   `0 distinct non-empty` after one step, which is how the bug was caught.
3. **Settling behaviour**: seeded grid of ~7,200 cells per substance → step 1 changes
   9,820 cells with 9,820 reaction matches; the table then reaches equilibrium. Recorded,
   not hidden: this reaction table is all merge rules, so one step consumes every reachable
   pair. Sustained activity is the rule table's job (story-002), not this table's.
4. **Performance**: `Reactor.BenchmarkSimulation 0 30` and `... 3 30` → the numbers now in
   `project.yaml`'s `performance` block.

**Why there is no unit test file**: `qa.level: minimal` waives tests (dev-story CONTRACT.md
L50/L68). The determinism criterion below is therefore **not yet mechanically enforced** —
that is the honest state, and it is the first thing a later tier should pick up.

**In-repo evidence document**: `docs/simulation/kernel-story-001-evidence.md`

---

## Dependencies

- Depends on: None
- Unlocks: Story 002（规则组合与归因记录）

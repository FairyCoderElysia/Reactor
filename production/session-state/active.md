# Active Session State — Reactor

<!--
  本合同的两个区域不可互换（见 .claude/docs/templates/session-state.md）：

    1. CHECKPOINT 区（STATUS + CHECKPOINT 两个块）——**机器读的区域**。
       session-start.sh 原样注入上下文，因此**构造上有界：保持在 ~25 行以内**。
       每次更新是**整体覆盖**，不是追加。

    2. `<!-- /CHECKPOINT -->` 之后的叙事 —— 给人看的。任何 hook 都不注入它，可以增长。

  注：模板提到的 `bash .claude/scripts/rotate-session-state.sh` 在本 repo **不存在**
  （`.claude/scripts/` 整个目录缺失），所以叙事超过 ~200 行时需要**手工**搬去
  `production/session-logs/`。
-->

<!-- STATUS -->
Epic: reactor
Feature:
Task: story-001 实现完成并已编译验证，待 /story-done
<!-- /STATUS -->

<!-- CHECKPOINT -->
**Updated:** 2026-10-06
**Branch:** `main`
**Current task:** `/dev-story` on `production/epics/reactor/story-001-data-driven-kernel-and-grid.md` — 实现完成、编译通过、行为已实测
**Next step:** `/story-done production/epics/reactor/story-001-data-driven-kernel-and-grid.md`（`minimal` 档无需 `/code-review`）
**Blocked on:** nothing
**Files in progress:** `Source/Reactor/Simulation/{ReactorTypes.h,SubstanceTable.h,SubstanceTable.cpp,GridSimulation.h,GridSimulation.cpp,ReactorSimulation.h,ReactorSimulation.cpp,SimulationBenchmark.h,SimulationBenchmark.cpp}` · `Source/Reactor/Reactor.cpp` · `Source/Reactor/Reactor.Build.cs` · `Content/Data/Reactor/DefaultSubstances.json` · `project.yaml` · `production/epics/reactor/story-001-*.md` · `docs/simulation/kernel-story-001-evidence.md`
**Run result:** `OBSERVED` — headless run, no rendering: `Reactor.DumpSimulationState` prints 5 substances / 6 reactions / 256×256; a seeded 55% grid of ~36,000 cells settles in one step (9,820 changed, 9,820 reacted) to ~31,851 cells and then holds steady. Isolation check: 5 lone cells survive 2 steps unchanged. `Reactor.BenchmarkSimulation` → 2.2531 ms/step @ 256×256, 8.0367 ms/step @ 512×512-equivalent. **`commands.run` still opens the editor, not the game — that is story-004's job, so there is no on-screen evidence for this story and none is expected.**
**Open questions:** 反应表全部是 merge 规则 → 一步即达平衡（**预期的**，持续涌现要靠 story-002 的规则表）· **确定性没有被任何测试断言**（`qa.level: minimal` 豁免测试），这是升档后最该补的一条 · `commands.build` 需要完整文件权限（UBT 的 .NET 文件操作写 `%LOCALAPPDATA%\UnrealBuildTool\`）· 别再用 bash/pwsh 的写测试判断目录可写性（两者权限层不同）
<!-- /CHECKPOINT -->

---

## 笔记

### story-001 的实现事实（2026-10-06）

**编译通过**：`Result: Succeeded`, exit 0。路上修了 3 个失败，每个都是会复发的坑：

1. `Simulation/` 内的文件用 `#include "Simulation/X.h"` → 解析成 `Simulation/Simulation/X.h`。
   `Reactor.Build.cs` 补 `PublicIncludePaths.Add(ModuleDirectory)`。
2. **UE 5.8 没有 `FAutoConsoleCommandWithArgs` 这个类** —— 家族被折叠进 `FAutoConsoleCommand`
   的重载构造函数（只有 `WithWorld`/`WithOutputDevice` 变体还在）。用旧名字得到的是**编译错误，不是弃用警告**。
3. 33 个 `LNK2019` 全指向 `FJsonObject`/`FJsonValue` —— **`Json` 模块不由 `Core` 传递**，得显式加依赖。

**跑出来才发现的 2 个真 bug**（读代码都没看出来）：

- **双缓冲写反了**：`StepPass` 把写缓冲**清空**而不是从读缓冲拷贝 → 没有任何写操作的格子在新帧里变空 →
  **一步清空整张网格**。靠"每物质放一个孤立格子、步进一次"隔离出来。
- **这个 bug 让第一次性能测量无效**：那份 `0.3375 ms/step` 是在**空网格**上测的。修正后是 2.2531 ms（256×256），
  慢了约 6.7 倍。**那个错数字看起来很合理，差点就被信了。**

**性能（实测，已填入 `project.yaml`）**：256×256 → 2.2531 ms/step（占 16.67ms 预算 13.5%）；
512×512 等效 → 8.0367 ms/step（48%）。不含渲染与蓝图。

<!-- /CHECKPOINT -->

---

### ⚠️ 一条重要的环境事实（我为此误判了两次）

**UBT 在启动时会备份自己的 trace 与日志文件**（`EpicGames.Core.Log.BackupLogFile`）。这套操作
（`FileReference.Move` / `Delete`）在**受限沙箱**下被拒，抛出的却是 `UnauthorizedAccessException`
—— **看起来像权限问题，实际是沙箱拦截**。

- 我第一次据此断言"`%LOCALAPPDATA%\UnrealBuildTool\` 在 Windows 上真的不可写"——**错的**。
  在 bash 里创建 / 重命名 / 删除那个目录的文件全部成功，连陈旧 `.uba` 都能自由重命名。
- 真相：**本工具的 bash 不受沙箱限制，pwsh 受限制**。所以 bash 能删、而 UBT（.NET）被拒。
  **不要再用 bash 或 pwsh 的写测试去判断"某个目录是否可写"** —— 它们处在不同的权限层。
- 结论：跑 `commands.build` 需要完整文件权限。`workspace-write` 不够，`danger-full-access` 通过。

### 一句话现状

《反应堆》/ Reactor —— 2D 逐格规则模拟。**CCGS 已搭好、设计已落盘、引擎已配置、代码为零。**
`Source/Reactor/` 只有 UE 模块骨架（`.Build.cs` / `.cpp` / `.h` / 2 个 `Target.cs`）。

### 配置（`project.yaml` 为准）

| 键 | 值 |
|---|---|
| 引擎 | Unreal Engine **5.8.3**（`E:/UE_5.8`，Changelist 58210709） |
| 语言 | **C++** 为主，Blueprint/UMG 做原型与规则编辑器 UI |
| 平台 / 形状 | PC（Steam/Epic）· **2D** · 单人 |
| 输入 | 键盘/鼠标（Gamepad: Partial — **未确认**） |
| 渲染 / 物理 | Lumen + Nanite (deferred) / Chaos |
| `project.stage` | **Concept**（minimal 档不走 `/gate-check`，停下是**预期**） |
| `modes.rigor` | `minimal` · `modes.automation` `guided` · `framework.version` `1.1.2` |
| 代码根 | `Source/Reactor/`（模块不在 `Source/` 下不会被编译） |
| 远端备份 | https://github.com/FairyCoderElysia/Reactor （私有/公开见用户设置） |

### 设计产物（3 份，已定稿，不要重写）

| 文件 | 内容 |
|---|---|
| `design/game-brief.md` | **一页 brief = `minimal` 档的全部设计文档**。MVP 7 项、Build order 8 步、Out of scope |
| `design/composition-model.md` | 组合模型（命门系统）：层级栈、封装双产出、三道闸、授权面分层 |
| `design/modes-and-feedback.md` | 三个模式 = 同一反应堆挂在三种校验器上；**约束就是反馈源** |

**核心玩法**：你从几样基础物质开始定规则，把规则封成部件、把部件封成工厂，一层层往上调用。
**MVP 命门**：第 5 项「部件 / 工厂：封装」—— brief 自述"全游戏成不成立就看这一步"。

### 已切出的 4 个故事（`production/epics/reactor/`）

| 故事 | 阻塞？ | 内容 |
|---|---|---|
| `story-001-数据驱动的结算内核与网格.md` | ✅ 可开工 | 物质/反应/规则三张数据表（改规则不重编译）+ 网格 + 确定性结算循环 |
| `story-002-规则组合与归因记录.md` | ✅ 可开工 | 修饰/触发/嵌套三种组合 + **归因记录**（可视化层反推不出来，必须内核产） |
| `story-003-递归三道闸.md` | ⛔ 被 ADR-0001 阻塞 | 基例 / 环检测 / 深度上限 + 唯一封装提交路径 + 可读拒绝 |
| `story-004-最粗糙可视化与可归因运行.md` | ⛔ 被 ADR-0001 阻塞 | 网格可视化 + 归因覆盖层 + **修掉 `commands.run` 缺陷** |

**为什么只有第 1 步的故事**：brief 自述第 3 步（封装）"全游戏成立与否在这"，
而现在**没有任何可玩的验证**。先用第 1 步拿到"看着它自己反应"的快感证据，
再决定是否升 `standard`（升级 = `/settings` 一条命令，已有文件原地不动）。

**升 `standard` 的触发点是一个可观察事件**：原型跑起来、且封装被验证成立或不成立之后。

### 架构决定：`docs/architecture/adr-0001-composition-model.md`（**Proposed**）

`docs/architecture/` 此前只有 `tr-registry.yaml`，**一个 ADR 都没有**。现在有了 ADR-0001。

三条最容易做错的约束：

1. **封装的"双产出"必须是原子操作 + 共享标识** —— 可见实体与可调用符号一次产出。
   做成"先造实体、以后注册符号"的话，玩家**每一次封装**都会经过不一致的中间态。
2. **归因记录是内核职责，不是渲染层的活。** 渲染层反推不出来（格子状态是叠加结果）。
   同时被 `modes-and-feedback.md` 的生存模式候选③独立指向 —— 两处要求它在引擎层**早期**留位。
3. **三道闸（基例 / 环检测 / 深度上限）属于 MVP，不是打磨项。**
   环检测只在**封装提交路径**上跑全图检查，**不得进入每帧结算热路径**（65k 格下 O(V+E)/帧不可接受）。

还有一条：**"无上界的递归"这个卖点无法靠回滚环检测来保住** —— 环检测是工程必然性
（没有它玩家输入就能挂死结算循环），不是设计选择。可回滚的只有上限的形态与拒绝方式。

### 环境事实（已实测，不要重新猜）

| 项 | 值 |
|---|---|
| 引擎路径 | `E:/UE_5.8`（不在 PATH） |
| 工程 | `Reactor.uproject`，`EngineAssociation: "5.8"`（该字段只存 major.minor，**不是版本不一致**） |
| 已编译产物 | `Binaries/Win64/UnrealEditor-Reactor.dll` ✔（但 `Binaries/` 已被 gitignore） |
| 工具链 | VS Build Tools 2022 @ `E:\vs` 17.14 + MSVC 14.44.35207 + Win11 SDK 26100 + NativeGame + .NET 8.0.8 |
| ⚠️ 缺 IDE | **只有 Build Tools，没有 VS IDE** —— 无调试器 / 断点 / IntelliSense |

**两条会让命令"看起来像工程坏了"的机器条件：**

**① headless 启动必须带 `-DDC-ForceMemoryCache`。** 不带 → 启动即 **EXIT=3**：
`Unable to use cache graph 'Installed' because it has no writable nodes available`。
带上 → **EXIT=0**（已实测）。已写进 `commands.smoke` / `commands.test`。

**② `commands.build` 直调 UBT，需要 .NET 10。** UE 5.8 的 `UnrealBuildTool.exe` 是 .NET 10 应用，
系统只有 6–9。引擎自带 SDK 在 `Engine/Binaries/ThirdParty/DotNet/10.0/win-x64`，
但**只有 `Build.bat` / `RunUAT.bat` 会自动找它**（经 `GetDotnetPath.bat`）。另需
`%LOCALAPPDATA%\UnrealBuildTool\` 可写，否则编译前就抛 `UnauthorizedAccessException`。

**命令验证状态**：`smoke` **已验证 EXIT=0**（端到端，经框架自己的解析器取值后执行）；
`build` / `test` **未验证**（前者受沙箱写权限限制，后者因零测试）。

### CCGS skill 的实际位置（**我为此误判过一次，记下来免得再错**）

**CCGS 的 112 个 skill 在 `C:\Users\14665\.dsh\skills`** —— 每个是标准 `SKILL.md`（部分还有 `CONTRACT.md`）：
`setup-engine` `create-stories` `create-epics` `dev-story` `story-done` `adopt` `brainstorm`
`gate-check` `architecture-decision` `architecture-review` …

- ⚠️ **`skill_load` 工具加载不了它们** —— 那个工具只认本 session 的 skill 目录，而那里是**另一套无关 skill**
  （`autoplan`/`ship`/`ios-*`）。**要读 CCGS 的 skill 就直接 `read` 那两个文件。**
- **不要再查 `~/.claude/skills`** —— 那里不是 CCGS。我第一次就是这样误判"CCGS 没装"的。
- 本 repo 的 `.claude/` **不是** skill 目录，它是 CCGS 的**配置**（agents / rules / hooks / docs）。

### 框架文件的已知缺口（**不要假装它们存在**）

- **`.claude/scripts/` 整个目录不存在** → `project-coherence.sh`、`rotate-session-state.sh` 都没有。
  但注意：CCGS skill 的 `allowed-tools` 里写着 `bash "*/.claude/skills/create-stories/../../hooks/yaml-helper.sh"`
  —— 这个路径**能解析到本 repo**（`.claude/hooks/yaml-helper.sh` 存在），所以配置读取链路是通的。
- `.claude/statusline.sh` 被 `settings.json` 引用，但**文件不在** → statusline 不会生效
- `production/epics/index.md` **不存在** → `/create-stories` Step 6 规定此时打印
  `Epics index not updated: production/epics/index.md absent` 然后继续，**不要建它**（格式无出处）

### 版本控制

- 仓库**已初始化**：`main` ↔ `origin/main`。首次推送 `5f947f0`（260 文件 / 2.5M）。
  之后每次改动都追加以保持备份连续 —— **开工前先 `git status` 确认没有未提交的设计改动**
- **260 文件 / 2.5M**，零二进制。`.gitignore` 排除了 `Intermediate/`(2.7G) / `Binaries/`(60M) /
  `Saved/` / `DerivedDataCache/` / `.vs/` / 全部 `*.sln`
- `.gitattributes` 统一行尾为 LF（224 个 md 是主体），并给 UE 资产类型标了 `binary`
- ⚠️ **未启用 Git LFS**。`Content/` 目前为空；一旦开始放 `.uasset`/`.umap`，
  **需要先评估 LFS** —— UE 资产带独占锁，纯 git 会痛

### 工具：UE 官方 MCP（已配好并验证）

UE 5.8 自带实验插件 `ModelContextProtocol`（**不自动启动**）。打开编辑器后在控制台执行
`ModelContextProtocol.StartServer`（默认端口 8000，路径 `/mcp`）。DSH 侧已配好，
工具以 `mcp__unreal__*` 出现。已验证双向通信。

对本作最有用的：`LiveCodingToolset`（**从 MCP 触发编译并取回诊断** —— 正好缓解
"逐格模拟要反复调参"的迭代成本）、`UMGToolSet`（规则编辑器 UI；**必须先 `list_properties`
再 `set_properties`，属性名不能猜**）、`EditorAppToolset`（`CaptureViewport` / PIE 控制）。

**已知无关问题**：`mcp-codegraph` 的 `--path` 写死为 `E:\毕业论文` —— 它分析的是那个项目，不是 Reactor。

### 相关文件

- `production/handoff-2026-10-05.md` —— 决策记录 + 为什么（引擎选型理由、性能测算、数据驱动约定、UE MCP 细节）
- `production/epics/reactor/story-00{1,2,3,4}-*.md` —— Build order 第 1 步的 4 个故事
- `docs/architecture/adr-0001-composition-model.md` —— 组合模型实现合同
- `docs/engine-reference/unreal/VERSION.md` —— 引擎 pin 与风险等级（**HIGH**，5.8 超出训练截止）
- `project.yaml` —— 所有配置的单一真源

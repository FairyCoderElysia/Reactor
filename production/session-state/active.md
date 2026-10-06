# 会话检查点 — Reactor

<!--
production/session-state/active.md 就是**记忆本体**，不是对话的摘要。
压缩 / 崩溃 / `/clear` 之后**先读这个文件**，再读其他地方。
最后更新：2026-10-06
-->

## 一句话现状

《反应堆》/ Reactor —— 一款 2D 逐格规则模拟游戏。**CCGS 已搭好、设计已落盘、引擎已配置完成。**
**尚无任何游戏代码**（`Source/Reactor/` 只有 UE 模块骨架）。
当前档位 `minimal`，下一步是 `/create-stories`（**建议只切 Build order 第 1 步**）。

## 配置（`project.yaml` 为准）

| 键 | 值 |
|---|---|
| 引擎 | Unreal Engine **5.8.3**（`E:/UE_5.8`，Changelist 58210709） |
| 语言 | **C++** 为主，Blueprint/UMG 做原型与规则编辑器 UI |
| 平台 / 形状 | PC（Steam/Epic）· **2D** · 单人 |
| 输入 | 键盘/鼠标（Gamepad: Partial — **未确认**） |
| 渲染 / 物理 | Lumen + Nanite (deferred) / Chaos |
| `project.stage` | **Concept**（minimal 档不走 `/gate-check`，停下是**预期**） |
| `modes.rigor` | `minimal` |
| `modes.automation` | `guided` |
| `framework.version` | `1.1.2` |
| 代码根 | `Source/Reactor/`（模块不在 `Source/` 下不会被编译） |

## 设计产物（3 份，已定稿，不要重写）

| 文件 | 内容 |
|---|---|
| `design/game-brief.md` | **一页 brief = `minimal` 档的全部设计文档**。MVP 7 项、Build order 8 步、Out of scope |
| `design/composition-model.md` | 组合模型（命门系统）：层级栈、封装双产出、三道闸、授权面分层、与 Noita 的差别 |
| `design/modes-and-feedback.md` | 三个模式 = 同一反应堆挂在三种校验器上；约束就是反馈源 |

**核心玩法一句话**：你从几样基础物质开始定规则，把规则封成部件、把部件封成工厂，一层层往上调用。

**MVP 里的命门**：第 5 项「**部件 / 工厂：封装**」—— brief 自述"全游戏成不成立就看这一步"。

## 架构决定：`docs/architecture/adr-0001-composition-model.md`（**Proposed**）

组合模型已固化成实现合同。**开工实现"封装"或"规则组合"之前必须读它。**
（`Status: Proposed` —— 升 `Accepted` 只能由用户本人或 `technical-director` 在用户明确确认后执行。）

三条最容易被做错的约束：

1. **封装的"双产出"必须是原子操作 + 共享标识** —— 可见实体与可调用符号一次产出。
   若做成"先造实体、以后注册符号"，玩家**每一次封装**都会经过不一致的中间态。
2. **归因记录（哪条规则在哪个格子上干了什么）是内核职责，不是渲染层的活。**
   渲染层反推不出来（格子状态是叠加结果）。这条同时被 `modes-and-feedback.md` 的
   生存模式候选③独立指向 —— 两处都要求它在引擎层**早期**留位，晚了补不上。
3. **三道闸（基例 / 环检测 / 深度上限）属于 MVP，不是打磨项。**
   一个环就足以让结算循环永远跑不完。环检测只在**封装提交路径**上跑全图检查，
   **不得进入每帧结算热路径**（65k 格规模下 O(V+E) 每帧不可接受）。

## 环境事实（已实测，不要重新猜）

| 项 | 值 |
|---|---|
| 引擎路径 | `E:/UE_5.8`（不在 PATH） |
| 工程 | `Reactor.uproject`，`EngineAssociation: "5.8"`（该字段只存 major.minor，**不是版本不一致**） |
| 已编译产物 | `Binaries/Win64/UnrealEditor-Reactor.dll` ✔ |
| 工具链 | VS Build Tools 2022 @ `E:\vs` 17.14 + MSVC 14.44.35207 + Win11 SDK 26100 + NativeGame + .NET 8.0.8 |
| ⚠️ 缺 IDE | **只有 Build Tools，没有 VS IDE** —— 无调试器 / 断点 / IntelliSense。建议补 VS 2022 Community 或 Rider |

### 两条会让命令"看起来像工程坏了"的机器条件

**① headless 启动必须带 `-DDC-ForceMemoryCache`。**
不带 → 启动即 **EXIT=3**：`Unable to use cache graph 'Installed' because it has no writable nodes available`。
带上 → **EXIT=0**（2026-10-06 实测，干净启动退出）。已写进 `commands.smoke` / `commands.test`。

**② `commands.build` 直调 UBT，需要 .NET 10。**
UE 5.8 的 `UnrealBuildTool.exe` 是 .NET 10 应用；系统只有 6/7/8/9。
引擎自带 SDK 在 `Engine/Binaries/ThirdParty/DotNet/10.0/win-x64` —— 但**只有
`Build.bat` / `RunUAT.bat` 会自动找它**（经 `GetDotnetPath.bat`），直调 exe 不会。
另需 `%LOCALAPPDATA%\UnrealBuildTool\` 可写，否则编译前就抛 `UnauthorizedAccessException`。

**命令验证状态**：`commands.smoke` **已验证 EXIT=0**（端到端，经框架自己的解析器取值后执行）；
`commands.build` / `commands.test` **未验证**（前者受沙箱写权限限制，后者因为零测试）。

## 框架文件的已知缺口（**不要假装它们存在**）

- **没有 `.claude/scripts/`** → `/setup-engine` §8.5 的 `project-coherence.sh` **不存在**。
  已手工复现它声明的比对（`[DIFFERS]` 0 条），但完整逻辑无法复现。
- **没有 `.claude/skills/`** —— skill 从全局解析，不在本 repo 内。
- `.claude/statusline.sh` 被 `settings.json` 引用，但**文件不在**。
- **没有 `.gitignore`，且不是 git 仓库** —— `Intermediate/` 已 **2.7G**。

## ⚠️ 最该先做的事（与流程无关，但优先级最高）

**仓库还不是 git 仓库，且没有 `.gitignore`。** 现在所有工作**零版本保护** ——
一次误删就是永久损失。UE 需要忽略：`Binaries/`、`Intermediate/`、`Saved/`、
`DerivedDataCache/`、`.vs/`、`*.sln`。

## 工具：UE 官方 MCP（已配好并验证）

UE 5.8 自带实验插件 `ModelContextProtocol`（**不自动启动**）。
打开编辑器后在控制台执行 `ModelContextProtocol.StartServer`（默认端口 8000，路径 `/mcp`）。
DSH 侧已配好，工具以 `mcp__unreal__*` 出现（`list_toolsets` / `describe_toolset` / `call_tool`）。
已验证双向通信（`SearchCVars` 返回 37 个真实 cvar）。

对本作最有用的：`LiveCodingToolset`（**从 MCP 触发编译并取回诊断** —— 正好缓解
"逐格模拟要反复调参"的迭代成本）、`UMGToolSet`（规则编辑器 UI；**必须先 `list_properties`
再 `set_properties`，属性名不能猜**）、`EditorAppToolset`（`CaptureViewport` / PIE 控制）。

**可优化**：`AllToolsets` 当前挂着约 25 个工具集，大半与本作无关。

**已知无关问题**：`mcp-codegraph` 的 `--path` 写死为 `E:\毕业论文` —— 它分析的是那个项目，不是 Reactor。

## 下一步

```
1. ✅ /setup-engine                    （2026-10-06 完成）
2. ✅ /brainstorm                      （design/game-brief.md 已于 2026-10-05 产出）
3. ⬜ /create-stories                  ← 建议只切 Build order 第 1 步 + ADR-0001 的三道闸
4. ⬜ /dev-story                       ← 第一行游戏代码
```

**为什么建议只切第 1 步**：brief 自述第 3 步（封装）"全游戏成立与否在这"，
而现在**没有任何可玩的验证**。先用第 1 步拿到"看着它自己反应"的快感证据，
再决定是否升 `standard`（升级 = `/settings` 一条命令，已有文件原地不动）。

**升 `standard` 的触发点是一个可观察事件**：原型跑起来、且"封装"被验证成立或不成立之后。
不成立就在这里止损，成立就升档去拆系统。

## 相关文件

- `production/handoff-2026-10-05.md` —— 决策记录 + 为什么（引擎选型理由、性能测算、数据驱动约定、UE MCP 细节）
- `docs/architecture/adr-0001-composition-model.md` —— 组合模型实现合同
- `docs/engine-reference/unreal/VERSION.md` —— 引擎 pin 与风险等级（**HIGH**，5.8 超出训练截止）
- `project.yaml` —— 所有配置的单一真源

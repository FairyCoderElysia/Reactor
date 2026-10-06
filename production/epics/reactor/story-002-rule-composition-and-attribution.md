# Story 002: 规则组合与归因记录

> **Epic**: Reactor（Build order 第 1 步 · 覆盖第 2 步的规则组合部分）
> **Status**: Ready
> **Layer**: Foundation
> **Type**: Integration
> **Estimate**: 3–5 天（`coarse`）
> **Manifest Version**: N/A (minimal — no control manifest)
> **Last Updated**: —

## Context

**GDD**: `design/game-brief.md`
**Requirement**: `Brief MVP feature 2`（规则编辑：条件 + 效果，能加 / 删 / 改 / 调参）+
`Brief MVP feature 3`（规则组合：一条规则能修饰、触发、嵌套另一条规则）
（并交付 `Brief MVP feature 4` 中"看得见是哪条规则干的"所需的**归因记录**）

> **规则表在本故事范围内。** story-001 只做了 MVP 第 1 条（空间 + 基础物质 + **两两反应表**）；
> "条件 + 效果"的**规则表**、以及规则之间的**修饰 / 触发 / 嵌套**都在这里。
> 这是 2026-10-06 从 story-001 转来的 —— 那次修正的原因是 story-001 的作者把 MVP 编号标错了，
> 把第 2/3 条的规则表写进了第 1 条故事的验收标准。

**ADR Governing Implementation**: `ADR: N/A — 本故事做的是"规则之间的组合"，不产生新的封装实体，因此不受 ADR-0001（组合模型）的封装路径约束管辖；封装期规则留到 story-003`
**ADR Decision Summary**: N/A (minimal — no ADRs)
**ADR Version**: N/A (minimal — no ADRs)

**Engine**: Unreal Engine 5.8.3 | **Risk**: HIGH
**Engine Notes**: `docs/engine-reference/unreal/VERSION.md` 标 5.8 为 HIGH。本故事涉及数据 schema 与内核数据结构，注意
5.8 的 `TInstancedStruct` 加了 `*` / `->` 运算符便利 —— 但 ADR-0001 的"备选方案 3"明确**暂不采用**它表示异构节点载荷，
理由是会把校验逻辑与 UE 反射绑在一起。原型期用朴素结构。
**本地构建注意**：headless 启动必须带 `-DDC-ForceMemoryCache`，否则 EXIT=3。

**Control Manifest Rules (this layer)**:
- Required: N/A (minimal — no control manifest)
- Forbidden: N/A (minimal — no control manifest)
- Guardrail: N/A (minimal — no control manifest)

---

## Acceptance Criteria

*From `design/game-brief.md`（the **Player goal & fail state** field + the MVP feature this story implements），scoped to this story:*

- [ ] **规则表存在**：条件 + 效果，初始含 **3 条**规则（从 story-001 转来 —— 那是 MVP 第 2 条）
- [ ] 规则能**加 / 删 / 改 / 调参**，且全部在数据文件里表达
- [ ] 三种组合模式（修饰 / 触发 / 嵌套）各有一个**能跑通的数据示例**
- [ ] 新增或修改一条规则、或一个组合，**不重新编译**即生效（实测）
- [ ] 嵌套至少支持 **3 层**（证明不是"只有一层"的假实现）
- [ ] **归因记录**可查询：给定格子 + 步数，能返回**是哪条规则**造成的
- [ ] 归因记录只记录**真正产生影响**的项（不是每次遍历都记）
- [ ] 归因记录不使基准网格下的 tick 耗时显著劣化（与 story-001 实测的 2.3643 ms 对比）
- [ ] **规则表能产生持续活动**，而不是像纯反应表那样一步达平衡 —— 这是本故事与 story-001 的实质差别
- [ ] **确定性**：同输入两次运行，归因记录**逐条一致**（自动化测试断言）
- [ ] 防御性展开上限存在，超限时**报可读的错、不崩**
- [ ] `Automation Spec` 测试覆盖三种组合模式各至少一例，命名以 `Reactor.` 开头
- [ ] 归因记录的数据结构写入 `docs/`（story-004 的渲染层要按它实现）

## Implementation Notes

本故事不引用 ADR。以下为必须遵守的实质约束：

- **规则表与 reaction 表是两张不同的表，不要合并它们。** 反应表回答"物质 A 遇到物质 B 变成什么"（story-001 已交付，在 `Content/Data/Reactor/DefaultSubstances.json`）；规则表回答"满足什么条件就施加什么效果"，是玩家编辑的对象。合并会让玩家可编辑的东西和数据表纠缠在一起。
- **规则表要让体系持续动起来。** story-001 的反应表全是 merge 规则，一步就消耗掉所有可配对 → 体系立刻静止（已实测，记在 `docs/simulation/kernel-story-001-evidence.md` §3）。规则表的存在意义正是打破这个平衡：修饰改变参数、触发引发连锁、嵌套制造重复。
- **归因记录必须由内核产出，渲染层反推不出来。** 理由（`design/composition-model.md` 第五节）：格子的当前状态是很多条规则**叠加后**的结果，从结果无法还原归因。若内核不记录"这一步是哪条规则干的"，可视化层就永远做不到那件事 —— 而它是核心反馈回路的另一半。这条同时被 `design/modes-and-feedback.md` 的生存模式候选③独立指向，标注为"**必须在引擎层早期留位置，晚了补不上**"。
- **只记真正产生影响项**：基准网格 65,536 格，若每格每步都写一条记录会直接压垮内核。只记"确实发生改变"的格子。
- **防御性展开上限 ≠ story-003 的三道闸。** 本故事只需浅层保护（例如一次结算内的最大展开次数），让"嵌套写错"不至于无法测试。**不要在这里实现封装期的环检测与深度上限** —— 那属 story-003，且属**封装提交路径**，两者混在一起会让两个故事都做不清。
- 三种组合模式必须**全部能在数据文件里表达**，不需要改代码。

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 001: 数据表加载、网格、基础结算循环（本故事依赖它，不重做）
- Story 003: 递归的三道闸（基例 / 环检测 / 深度上限）+ 唯一封装提交路径
- Story 004: 可视化 —— 本故事只产出**归因记录这个数据结构**，不消费它、不画任何东西

## QA Test Cases

*N/A — no qa-lead specs at this tier; implement against the Acceptance Criteria above*

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived**（advisory, never "must exist and pass"）*

**Story Type**: Integration
**Required evidence**:
- Integration: `tests/integration/settlement/[story-slug]_test.[ext]` **OR** playtest doc

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: Story 001（需要网格与结算循环）必须 DONE
- Unlocks: Story 003、Story 004

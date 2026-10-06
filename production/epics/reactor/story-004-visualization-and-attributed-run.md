# Story 004: 最粗糙的可视化与可归因的运行

> **Epic**: Reactor（Build order 第 1 步的收口）
> **Status**: **Blocked**
> **Layer**: Presentation
> **Type**: Visual/Feel
> **Estimate**: 3–5 天（`coarse`）
> **Manifest Version**: N/A (minimal — no control manifest)
> **Last Updated**: —

## Context

**GDD**: `design/game-brief.md`
**Requirement**: `Brief MVP feature 4` — 反应结算循环 + **最小可视化反馈**：看得见反应在发生，也看得见是哪条规则干的

**ADR Governing Implementation**: `ADR-0001: 组合模型 —— 递归封装、环检测与深度上限`
**ADR Decision Summary**: 该 ADR 的决策"五"把可视化定为**核心反馈回路而非打磨项**，并要求结算留下**可归因的痕迹**（哪条规则在哪个格子上做了什么）由内核产出、渲染层只消费。
**ADR Version**: 2026-10-06

> ⛔ **BLOCKED: ADR-0001 is Proposed — accept it with `/architecture-decision accept ADR-0001` once decided**
>
> 本故事消费 ADR-0001 §5 所定义的归因记录，并把"可视化是核心反馈回路"这条约束实现出来，
> 因此受它管辖。ADR-0001 处于 `Proposed` → 被 `dev-story` / `story-readiness` 拦截。
> 解除方式：用户本人将 `Status` 改为 `Accepted`。

**Engine**: Unreal Engine 5.8.3 | **Risk**: HIGH
**Engine Notes**: `docs/engine-reference/unreal/VERSION.md` 标 5.8 为 HIGH。
渲染相关、可能有用但**不要投机采用**：5.8 的 **Substrate Toon Shading（实验性）** 最贴近 brief 的
美术方向（"深色底 + 高饱和元素色"），但它实验性、且本作只需纯色块 —— 是否采用是独立决策，不属本故事。
**本地构建注意**：headless 启动必须带 `-DDC-ForceMemoryCache`（EXIT=3 否则）。

**Control Manifest Rules (this layer)**:
- Required: N/A (minimal — no control manifest)
- Forbidden: N/A (minimal — no control manifest)
- Guardrail: N/A (minimal — no control manifest)

---

## Acceptance Criteria

*From `design/game-brief.md`（the **Player goal & fail state** field + the MVP feature this story implements），scoped to this story:*

- [ ] 网格状态在窗口中可见，不同物质可区分（色块即可）
- [ ] 运行中**肉眼可见变化**（不是静止的一坨）
- [ ] **归因覆盖层可开关**，开启时能看出"这一格是被哪条规则改的"
- [ ] `commands.run` 实测启动**游戏窗口**（1280×720），**不是编辑器** —— 修掉现有缺陷
- [ ] 窗口在基准网格下流畅（用实测帧时间说话，不是感觉）
- [ ] 能连续观察 **60 秒**不掉帧、不崩、不卡死
- [ ] 一次**可归因的失败**可见：构造一个会失败的规则集，能看出失败在哪一步
- [ ] 全程无需美术资产（纯色 / 色块即可）—— 遵守 brief 的 Out of scope
- [ ] 有一份可分享的记录（截图或短录屏），作为 Build order 第 1 步的**验收证据**

## Implementation Notes

*Derived from ADR-0001（本故事受阻，以下为该 ADR 已定的指引）：*

- **本故事不是在做美术，是在做"能否看见"。** 成功标准不是"好看"，而是"你能不能看着它跑起来，并且知道发生了什么"。brief 的 Out of scope 明确排除美术资产（"色块点阵占位"）——**不要滑向美术**，把时间花在"看得见因果"上。
- **归因覆盖层是核心，不是可选装饰。** 它消费 story-002 产的归因记录。若它拖垮帧时间，先降级为"按需开启 / 单步查看"，**而不是砍掉它** —— 它是核心反馈回路的另一半。
- ⚠️ **`commands.run` 的现有缺陷必须在本故事修掉**：目前它指向 `UnrealEditor.exe`（**编辑器**，不是游戏）。框架要求 `commands.run` 启动**游戏本体**、开窗、固定分辨率，`docs/run-and-observe.md` 的观测流程依赖这一点。不修的话，之后每次"跑起来看看"都会变成手动操作。
- 三个模式是同一个反应堆挂在三种校验器上（`design/modes-and-feedback.md`）—— 本故事只做**沙盒**形态（无约束），不要顺手做谜题/生存。

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 001: 数据表、网格、基础结算循环
- Story 002: 三种组合模式与归因记录（本故事**只消费**归因记录，不产出它）
- Story 003: 递归的三道闸
- **美术资产** —— brief 的 Out of scope 明确排除
- 谜题 / 生存两个模式的条件层 —— Build order 第 5/6 步

## QA Test Cases

*N/A — no qa-lead specs at this tier; implement against the Acceptance Criteria above*

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived**（advisory, never "must exist and pass"）— **但 Visual/Feel 故事的留存截图不在豁免范围内***

**Story Type**: Visual/Feel
**Required evidence**:
- Visual/Feel: 在 `production/qa/evidence/` 留存截图 + 在 `production/qa/evidence/[story-slug]-evidence.md` 签核

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: Story 002（需要归因记录作为输入）必须 DONE
- Unlocks: None —— 本故事完成后，Build order 第 1 步结束，回到决策点（见 `production/session-state/active.md`）

> ⚠️ **本故事完成后不要直接开始 Build order 第 3 步（封装，命门）。**
> 先做那个判断：*"看着它自己反应"有快感吗？*
> 有 → 继续第 3 步，并考虑把 `modes.rigor` 升到 `standard`。
> 没有 → 现在止损最便宜，回去改 brief，不要往下堆代码。

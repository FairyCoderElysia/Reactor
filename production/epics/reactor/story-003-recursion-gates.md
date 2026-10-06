# Story 003: 递归的三道闸（基例 / 环检测 / 深度上限）

> **Epic**: Reactor（Build order 第 2 步）
> **Status**: **Blocked**
> **Layer**: Foundation
> **Type**: Logic
> **Estimate**: 3–5 天（`coarse`）
> **Manifest Version**: N/A (minimal — no control manifest)
> **Last Updated**: —

## Context

**GDD**: `design/game-brief.md`
**Requirement**: `Brief MVP feature 3` 的收敛条件 —— "一条规则能修饰、触发、嵌套另一条规则"必须**安全**：
无环、有深度上界、失败可读。（对应 Build order 第 2 步"环检测与深度上限 —— 不然一个环就把模拟卡死"）

**ADR Governing Implementation**: `ADR-0001: 组合模型 —— 递归封装、环检测与深度上限`
**ADR Decision Summary**: 把组合模型固化成四条实现约束 —— 层级递归有下界（基础物质）无上界；封装必须**原子地**同时产出「可见实体 + 可调用符号」；递归必须过三道闸（基例 / 环检测 / 深度上限）；授权面分种植层与整理层。三道闸属 MVP 而非打磨项。
**ADR Version**: 2026-10-06

> ⛔ **BLOCKED: ADR-0001 is Proposed — accept it with `/architecture-decision accept ADR-0001` once decided**
>
> 本故事受 ADR-0001 管辖，而它当前 `Status: Proposed`。`dev-story` / `story-readiness`
> 在**所有档位**都会因"引用了 `Proposed` ADR"而拦截（`docs/effects-map.md` L668）。
> 解除方式：**用户本人**将其 `Status` 改为 `Accepted`（模板规定只有用户，或
> `technical-director` 在用户明确确认后，可执行此操作）。
>
> 这不该被当成形式：升 `Accepted` 意味着确认组合模型的四条约束作为本项目有约束力的技术合同。
> **在它被接受前不应把这三道闸实现进内核** —— 闸的形状直接由那四条约束决定。

**Engine**: Unreal Engine 5.8.3 | **Risk**: HIGH
**Engine Notes**: `docs/engine-reference/unreal/VERSION.md` 标 5.8 为 HIGH。
ADR-0001 的 Engine Compatibility 表把本故事的 Knowledge Risk 单独判为 LOW，理由：三道闸是
**引擎无关的算法**，可以在无引擎依赖的纯 C++ 单元测试里验证 —— 不必等 UE 工程跑起来。
5.8 提示：`UE_LOG` → `UE_LOGF`；不要假设 5.0–5.6 引擎片段仍能链接（非反射非虚符号已移除）。

**Control Manifest Rules (this layer)**:
- Required: N/A (minimal — no control manifest)
- Forbidden: N/A (minimal — no control manifest)
- Guardrail: N/A (minimal — no control manifest)

---

## Acceptance Criteria

*From `design/game-brief.md`（the **Player goal & fail state** field + the MVP feature this story implements），scoped to this story:*

- [ ] **自引用环**（A 调 A）在封装时被拒绝，且错因**指出该规则**
- [ ] **两跳环**（A 调 B、B 调 A）被拒绝，且错因**指出构成环的那条边**
- [ ] **长环**（≥3 跳）同样被拒绝 —— 证明检测不是只看直接边
- [ ] **恰好等于上限**的合法嵌套**通过**（边界不误杀）
- [ ] **超过上限**的嵌套被拒绝，**报可读的错、不崩**
- [ ] 改动深度上限的参数后**不需要重新编译**（实测）
- [ ] 叶子非基础物质时，封装被拒绝（闸① 基例）
- [ ] 封装失败时**不留下半成品**（无孤立实体、无孤立符号）
- [ ] 封装成功时**可见实体与可调用符号共享标识**，二者同时可用
- [ ] 结算循环含**防御性**深度检查，且**未**在热路径做全图遍历（用性能对比证明环检测只在封装路径上跑）
- [ ] `Automation Spec` 测试覆盖上述全部环与深度情形，命名以 `Reactor.` 开头

## Implementation Notes

*Derived from ADR-0001 Implementation Guidelines（本故事受阻，以下为该 ADR 已定的指引）：*

- **闸只在封装提交路径上跑全图检查，不进每帧结算热路径。** 结算循环只做**廉价**的防御性深度检查。否则每帧带一个 O(V+E) 环遍历，在 65k 格规模下不可接受。
- **提交路径必须只有一条。** 将来的种植层与整理层**都**只能经由它封装。两条路径 = 两套校验逻辑，早晚分叉，而分叉出来的那条就是漏洞。
- **拒绝必须可读。** `design/game-brief.md` 要求"失败会留下可读的现场" —— 只有 `false` 的返回值**不满足**这一点，必须能指出是哪条规则/哪条边。
- **阈值参数化。** 深度上限走数据驱动（数据文件 / 蓝图可编辑变量），不写编译期常量。
- **不要在闸里做 GC 敏感操作**（纯数据校验，不持有 UObject 引用）。
- **深度上限的数值不在本故事范围内** —— ADR-0001 明确不预设数字，需由原型手感确定。本故事只负责"它参数化且行为正确"。
- **"无上界"是产品许诺，"有硬上限"是工程事实。** 这个矛盾记在 ADR-0001 的 Negative 后果里。错误信息要**解释原因**，不能只说"超限了"。

## Out of Scope

*Handled by neighbouring stories — do not implement here:*

- Story 001: 数据表、网格、基础结算循环
- Story 002: 三种组合模式与归因记录（本故事依赖"嵌套"作为环的载体，不重做）
- Story 004: 可视化 —— 本故事不产出画面

## QA Test Cases

*N/A — no qa-lead specs at this tier; implement against the Acceptance Criteria above*

---

## Test Evidence

*Governed by `qa.level`: at `qa.level: minimal` tests are **waived**（advisory, never "must exist and pass"）*

**Story Type**: Logic
**Required evidence**:
- Logic: `tests/unit/settlement/[story-slug]_test.[ext]` — must exist and pass

> 附注（来自 ADR-0001）：这些测试**不应依赖引擎**。三道闸是引擎无关的算法，可以也应该脱引擎先验证。

**Status**: [ ] Not yet created

---

## Dependencies

- Depends on: Story 002（需要嵌套作为环的载体）必须 DONE
- Unlocks: Story 004

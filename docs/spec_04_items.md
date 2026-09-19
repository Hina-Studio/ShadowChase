# 4. 道具系统（规格稿 v1）

> 归属：AI 设计位。字段与接口约束遵守 GDD 第 4 节。
> 分层：输入层产意图 → 表现层 → 逻辑层（服务端权威）→ 同步层（ServerRpc）。

## 4.1 字段规范

所有道具条目写入 `data/items.json`，字段固定如下：

| 字段 | 取值 | 说明 |
|------|------|------|
| `id` | snake_case | 唯一 ID |
| `name_zh` / `name_en` | string | 本地化键 |
| `type` | consumable / equipment / quest | 三分类 |
| `use_mode` | holdable / hotbar | **决定交互层实现**：实体手持 或 快捷栏瞬发 |
| `effect` | 结构化对象 | 见各条目 |
| `cooldown_s` / `charges` | number | 冷却或次数 |
| `server_authoritative` | 恒为 true | 所有改变状态的效果必须 ServerRpc |
| `vr_diff` | string | VR/非 VR 表现差异 |
| `collision` | 仅 holdable | `{ "extents": [x,y,z], "grip": [x,y,z] }` |
| `weight` / `stack` | number | 负重与堆叠 |

## 4.2 任务物品（quest）

| ID | 名称 | 使用方式 | 效果 | 备注 |
|----|------|----------|------|------|
| `fuel_can` | 燃料桶 | holdable | 为发电机/载具装填燃料 | 移动 −15%，掉落巨响（暴露） |
| `battery` | 电池组 | holdable | 为设备供电 | 可堆叠 2 |
| `fuse` | 保险丝 | holdable | 修复配电箱 | 三相任务链用 |
| `spare_part` | 零件箱 | holdable | 修复设备/捷径门 | 双人搬运增益 |

> 冗余系数：关键任务物品总生成量 ≥ 需求 × 1.3（与 GDD 6.3 一致）。

## 4.3 武器（equipment）——拖延工具，非解法

| ID | 名称 | 使用方式 | 效果 | 冷却/消耗 | 关键约束 |
|----|------|----------|------|-----------|----------|
| `melee_pipe` | 撬棍 | holdable | 近战硬直 0.6s，击退 1.2m；对怪伤害 ≈ 0 | 无 | 触发伤害怒气 +6 |
| `flash_rod` | 照明棒 | hotbar/投掷 | 制造 3m 光斑 25s（克制 M03） | 1 次 | 不产生怒气 |
| `signal_flare` | 信号弹 | hotbar | 80m 可见光 15s，吸引 M02 到火光处 | 1 次 | 可能引怪，双刃剑 |
| `revolver` | 左轮 | holdable | 单发硬直 1.5s；**伤害极低** | 6 发 | **伤害怒气 ×1.2/点，最高单次 +18**；枪声 80m |

设计意图：开火 = 用长期安全换短期空间（GDD 3.4）。

## 4.4 消耗品/工具（consumable）

| ID | 名称 | 使用方式 | 效果 | 冷却 | VR 差异 |
|----|------|----------|------|------|---------|
| `bandage` | 绷带 | hotbar | 恢复 30 HP，持续 4s 不能移动 | 1 次 | VR：实体缠绷带动画 |
| `painkiller` | 止痛剂 | hotbar | 60s 内受伤 −25%，移速 −5% | 1 次 | 屏幕边缘模糊 |
| `energy_bar` | 能量棒 | hotbar | 体力 +40 | 1 次 | 无 |
| `flashlight` | 手电 | holdable | 锥形光；可致盲 M02 2.5s | 电池 90s | VR：需手部指向 |
| `noise_maker` | 发声器 | holdable | 定时 10s 制造 60m 噪音诱饵 | 1 次 | 可抛掷 |
| `toolkit` | 工具包 | holdable | 修理速度 +40%，双人装配加成 | 无 | 双手占用 |

## 4.5 接口约束（不可违反）

1. 道具逻辑走逻辑层，**服务端权威**；客户端仅预测表现（拾取动画、切换预览）。
2. 实体手持类必须提供 `collision.extents` 与 `collision.grip`。
3. 任何改变状态的效果由 **ServerRpc** 下发：`use_item(id, target_id?, aim?)` → 服务端校验（距离/冷却/持有/体力）→ 广播 `item_used`。
4. 钓鱼防护：拾取与使用均带序号防重放；每 tick 使用次数限 1。

## 4.6 验收口径

- 画面/日志：使用任何道具，HUD/日志可见 `item_used(player,id,tick)`；服务端可回放。
- 防作弊：客户端改冷却/数量 → 服务端拒绝并计数告警。
- 帧耗时：道具层每 tick < 0.2ms（16 人同局）。

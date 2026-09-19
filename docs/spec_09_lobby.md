# 9. 大厅流程（规格稿 v1）

> 归属：AI 设计位。实现 GDD 第 9 节最小接口集，服务端权威。
> 传输：大厅走 TCP/WebSocket（可靠、易调试）；对局走 ENET UDP。

## 9.1 状态机

```
Idle → Creating → Ready(等待玩家) → AllReady? → Starting → InMatch → Ended
                     ↑                |
                     └─ 玩家加入/离开/准备/取消准备 ┘
```

| 状态 | 超时 | 超时行为 |
|------|------|----------|
| Ready | 15min 无操作 | 自动解散，玩家回 Idle |
| Starting | 20s 加载 | 超时踢出未加载玩家，其余进入对局 |
| InMatch | 局内 60min | 载具离开，结算失败 |

## 9.2 接口（最小集，全部请求-应答 + 事件推送）

| RPC | 请求 | 应答 | 说明 |
|-----|------|------|------|
| `create_room` | `{max_players, mode, region, seed?}` | `{room_id, seed, config_hash}` | seed 缺省服务端生成 |
| `join_room` | `{room_id, player_name}` | `{layout_manifest, room_state}` | **只含大厅信息，不含游戏状态** |
| `leave_room` | `{room_id}` | `{ok}` | 房主离开触发重新选举 |
| `set_ready` | `{room_id, ready:bool}` | `{ok}` | 全员 ready 才可开始 |
| `start_match` | `{room_id}` 仅房主 | `{match_id, seed, snapshot_id}` | 服务端校验人数/版本 |
| `reconnect` | `{room_id, player_id, resume_token}` | `{snapshot_id, layout_manifest}` | 重连补发当前快照 |
| `kick` | `{room_id, player_id}` 仅房主 | `{ok}` | 记录审计 |

事件推送（服务端 → 客户端）：
`room_player_joined` / `room_player_left` / `room_ready_changed` / `match_starting` / `match_started` / `match_aborted` / `reconnect_grace_started`。

## 9.3 数据契约

```json
// layout_manifest（大厅用，可缓存；对局开始时仅下发 seed + snapshot_id）
{
  "room_id": "R-8f3a",
  "seed": 20260919,
  "mode": "standard",
  "max_players": 16,
  "region": "cn-east",
  "config_hash": "sha256:...",
  "protocol_version": 3,
  "schema_version": 2
}
```

```json
// room_state
{
  "state": "ready",
  "host_id": "P-001",
  "players": [{ "player_id": "P-001", "name": "A", "ready": true, "latency_ms": 32 }],
  "map_preview": { "blocks": 64, "inner_radius_m": 320 }
}
```

## 9.4 断线重连

| 项 | 规则 |
|----|------|
| 宽限期 | 断线后保留席位 120s（InMatch 内 180s） |
| 凭证 | `resume_token`（HMAC，绑定 room+player+epoch） |
| 补发 | 仅发**最新快照 + 事件增量**，不重放全量历史 |
| 超时 | 宽限结束：普通局移除；对局中转观战（见第 10 节） |

## 9.5 校验与安全

- 所有请求带 `protocol_version` 与 `schema_version`，不匹配拒绝。
- 建房/加入频率限制：10 次/分钟/账号。
- 房间人数上限硬校验（架构 50，首发 16）。
- 服务端为唯一 `seed` 与 `layout_manifest` 来源，客户端不得覆盖。

## 9.6 验收口径

- 画面/日志：ImGui 面板可见房间状态、玩家列表、ready 状态、seed 与 config_hash。
- 服务端证明：`create→join→ready→start` 全链路事件可审计；伪造 host 立即拒绝。
- 帧耗时：大厅逻辑不在 tick 内；RPC 处理 < 1ms/请求。
- 端到端：2 客户端 + 专服互通，双方在 2s 内看到彼此加入（M0 验收项）。

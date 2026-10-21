## 本机只读核对结果

核对日期：2026-10-21；Magpie 版本 0.1.1312，地址 http://127.0.0.1:3425。以下是历史快照，接手后重新读取。未保存邮箱、密钥或请求内容。

GET /v1/magpie/quotas 返回 data[]，每个对象有 provider、name、kind、plan、windows[]、readAt。实际 Codex 窗口：

```json
{"provider":"codex","name":"Codex","windows":[{"name":"7 days","used":94,"remaining":6,"resetsAt":"2026-10-27T15:33:40+08:00"}],"readAt":"2026-10-21T23:48:23.749476+08:00"}
```

当前观测只有 Codex、一个 7 天窗口，没有 Claude 或 5 小时窗口。used/remaining 为百分数。不要把 provider 顶层 resets 对象当窗口重置时间，应使用 windows[].resetsAt；其他字段语义未确认。

GET /v1/magpie/usage 返回默认 30d 汇总和 models[]，包括 calls、input、output、cache_read、reasoning、cost。模型统计不含模型独立额度上限或重置时间。GET /v1/magpie/quotas/history 可读额度历史，但当前 UI 不需要依赖它。

## 上游现状

- companion/codex_display/metrics.py：读取 quotas、usage、requests；Snapshot 已有 limit_window_minutes 和 quota_reset_seconds。
- _most_constrained_window() 混合所有供应商选择最少 remaining，必须改为按供应商处理。
- 缺额度默认 100% 的逻辑不适合新需求，需要明确 unknown。
- companion/codex_display/protocol.py：现有 encode_snapshot 未传 limit_window_minutes 或 quota_reset_seconds，多供应商也未传输。
- docs/BLE_PROTOCOL.md：v4 每条消息最多 250 bytes，默认每 15 秒发快照，设备 60 秒后标记 stale。
- src/ui.cpp：四页固定坐标，Requests 为第二页，现有布局按 480×480 写死。

推荐只输出必要字段的方式核查 API，避免将完整响应中的账户邮箱及请求日志复制到开发文档。

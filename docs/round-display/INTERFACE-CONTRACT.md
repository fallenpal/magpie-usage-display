## 两个 Agent 的共享接口约定

状态：BLE v5 协议和 Companion 编码已实现并通过测试，固件解析尚未实现。逻辑模型、线协议和 fixture 以本文为准。任何键名或语义变更必须先改本文和 fixture，再改两端实现。

## 逻辑模型

快照包含 `generated_at`（UTC Unix 秒）、`utc_offset_minutes` 和 `providers[]`。每个 provider 包含稳定 `id`、显示 `name`、`read_at`（UTC Unix 秒或 null）和 `windows[]`。每个 window 包含稳定 `id`、`name`、`used_percent`、`remaining_percent`、`resets_at`（UTC Unix 秒或 null）、`unlimited` 和 `valid`。

- `used_percent` 和 `remaining_percent` 是 0 到 100 的整数。缺失或越界保持 null，不能默认为零。
- 只有一边存在时，用 `100 - value` 补另一边。两边冲突时记录不含账户信息的警告，并以 `used` 为准重新计算 `remaining`。
- `unlimited=true` 时 `valid=true`，百分比可以为 null。
- 两边百分比都缺失且不是 unlimited 时，`valid=false`。UI 必须把 unlimited、无效和有限额度分开显示。
- 排序：有限且已知 remaining 的窗口按 remaining 升序，其次 unlimited，最后无效窗口。供应商顺序固定为 Codex、Claude、其他，其他保持源顺序。
- 稳定 ID：provider 使用小写 slug 加账户标识的 SHA-256 前 6 位十六进制，账户标识不发送；window 使用名称 slug，重名时追加 `-1`、`-2`。ID 最长 23 字节，不得包含邮箱、凭据或请求正文。
- 上限：最多 8 个供应商，每个供应商最多 4 个窗口，总计最多 16 个窗口。超限按排序截断；总窗口用尽时不再发送没有窗口的供应商。截断设置 `tr=1`，`to` 为省略的供应商与窗口总数。

## BLE v5 线协议

继续使用现有 Status characteristic。每条消息是 UTF-8 JSON，不超过 250 bytes。同一批次共享序列号 `s`，建议从 1 开始递增。每条消息携带 `v=5` 和 `k` 消息类型。

| `k` | 含义 | 每批数量 |
|---|---|---|
| `s` | 汇总与今日统计 | 恰好 1 条 |
| `x` | 累计基础统计 | 恰好 1 条 |
| `p` | 供应商元数据 | 每个供应商 1 条 |
| `w` | 额度窗口 | 每个窗口 1 条 |
| `e` | 批次结束和提交 | 恰好 1 条 |

批次最多 32 条消息。设备在收到新 `s` 时丢弃未完成 staging；收到合法 `e` 且供应商数、窗口数和槽位完整时原子提交。重复索引覆盖同一槽位，乱序、缺条、计数不符或 5 秒超时都不得提交部分数据。旧快照在提交前保持可用。设备忽略 `s` 小于等于最后已提交序列号的批次，连接建立时除外。

### 汇总 `s`

```json
{"v":5,"s":42,"k":"s","t":1800000000,"o":480,"d":162,"f":7,"z":2,"y":714,"l":1250,"m":"gpt-5.6-sol","h":200,"n":9876,"b":0,"c":3,"w":6,"r":5}
```

- `t`：`generated_at`，UTC Unix 秒。
- `o`：`utc_offset_minutes`，用于本地重置时间。
- `d`：今日 token 数；`f`：今日调用数；`z`：今日错误数；`y`：成功率千分比；`l`：平均 TTFT 毫秒。
- `m`：最近模型，最多 23 bytes；`h`：最近状态码；`n`：最近延迟毫秒；`b`：最近是否失败。
- `c`：供应商数量；`w`：窗口总数。
- `r`：所有有限有效窗口中的最低 remaining；没有时省略。
- `tr`：1 表示发生截断，省略表示 0；`to`：被截断的供应商和窗口总数，省略表示 0。

### 累计统计 `x`

```json
{"v":5,"s":42,"k":"x","a":[40,500,123456,23456,987654,34567,6110000,889,"gpt-5.6-sol",450]}
```

`a` 的固定顺序：会话数、总调用数、总输入 token、总输出 token、总缓存读取 token、总推理 token、总费用微元、缓存命中千分比、最高频模型（最多 23 bytes）、最高频模型调用数。

累计统计单独成条，避免基础统计数值很大时汇总消息超过 250 bytes。设备必须在提交前同时收到 `s` 和 `x`。

### 供应商 `p`

```json
{"v":5,"s":42,"k":"p","i":0,"c":3,"id":"codex-fc9469","m":"Codex","ta":1792638605}
```

- `i`：供应商索引，从 0 开始；`c`：供应商总数。
- `id`：稳定 ID，最多 23 bytes；`m`：显示名称，最多 23 bytes。
- `ta`：`read_at`，UTC Unix 秒；未知时省略。

### 窗口 `w`

```json
{"v":5,"s":42,"k":"w","p":0,"i":0,"w":2,"id":"7-days","m":"7 days","ok":1,"u":95,"r":5,"ra":1793086420}
```

- `p`：所属供应商索引；`i`：供应商内窗口索引，0 为主窗口；`w`：该供应商的窗口总数。
- `id`：稳定 ID，最多 15 bytes；`m`：显示名称，最多 23 bytes。
- `ok`：1 表示数据有效，0 表示存在但无法确定额度。
- `u`：used 百分比；`r`：remaining 百分比；`ra`：重置时间 UTC Unix 秒。未知时分别省略。
- `ul`：1 表示无限额度；仅 unlimited 时出现。

示例中的有限窗口可省略 `ul`。unlimited 窗口没有 `u` 和 `r`，无效窗口没有 `u`、`r`、`ra` 和 `ul`，不能补 0 或 100。

### 结束 `e`

```json
{"v":5,"s":42,"k":"e","c":3,"w":6}
```

`c` 和 `w` 必须与设备 staging 计数一致。`tr` 和 `to` 仅在截断时出现，含义与汇总相同。

## 命令和结果

命令和结果继续使用原 Command 和 Result characteristic，版本升到 5。

```json
{"v":5,"sid":319028314,"id":7,"a":"focus_magpie"}
{"v":5,"id":7,"ok":1,"m":"FOCUSED"}
```

动作白名单仍为 `focus_magpie` 和 `refresh`。`sid` 和 `id` 必须为正整数，结果文本最多 48 字符。设备信息返回 `{"v":5,"fw":"0.5.0","screen":"466x466"}`。

## 共享 fixture

位于 `companion/tests/fixtures/`：

- `codex_claude_quotas.json`：Codex、Claude 和 Gemini 合成额度，Claude 明确为测试数据。
- `usage.json`、`requests.json`：基础统计输入。
- `v5_sequence_42.jsonl`：固定 `generated_at=1800000000`、`utc_offset_minutes=480`、`s=42` 的 12 条 golden 消息。第一条为 `s`，第二条为 `x`，最后一条为 `e`。

Companion 测试逐条比较解码后的 golden 消息，并检查每条不超过 250 bytes、批次不超过 32 条。固件解析测试必须复用同一份 fixture。fixture 不包含真实邮箱、API Key、账户标识或请求正文。

## 实施计划

状态：待用户确认。确认前不写业务代码，不提交、不烧录、不安装后台服务。

日期：2026-10-22。

依据：[[REQUIREMENTS]]、[[DATA-EVIDENCE]]、[[INTERFACE-CONTRACT]]、[[AGENT-A-FIRMWARE]]、[[AGENT-B-COMPANION]]、[[BLE_PROTOCOL]]。

## 角色与边界

本轮由我担任主实现人和验收人：

1. 先整理本计划、接口冻结顺序和验收清单，等用户确认。
2. 确认后开始实现 Companion 和固件，按优先级推进。
3. 当 Claude Code 的 5 小时 usage 达到 80%，或以用户提示为准触发时，停止新增范围，形成阶段性交接：分支、commit、改动范围、测试结果、未完成项、风险和 Agent B 任务清单。
4. 交接后由我派工作给 Agent B，并逐项验收。Agent B 的目录边界按 [[AGENT-B-COMPANION]]：`companion/`、`docs/BLE_PROTOCOL.md`、`docs/round-display/INTERFACE-CONTRACT.md`、协议 fixture 和数据测试。
5. 默认不 Fork、不推送、不安装 LaunchAgent、不烧录。需要时单独申请用户授权。

当前 5 小时 usage 百分比无法由我直接读取，以客户端显示或用户提示为准。

## 已确认的产品决策

- 主页面顺序：Overview、Usage、Sessions、Context。Usage 替代原 Requests。
- Usage 多窗口：主窗口加紧凑次要行。主窗口为最受限的有限窗口，次要窗口不混用百分比和重置时间。
- 过期状态：区分 OFFLINE 与 STALE，两者都保留最后一次数据并降亮。
- 供应商顺序：固定 Codex > Claude > 其他。供应商消失时回退到当前顺序的第一个有效项。
- UI 语言：英文短标签，沿用概念图。
- 时间：Companion 传 UTC 绝对时间和 `utc_offset_minutes`，设备本地格式化 `MM/dd HH:mm`，倒计时基于绝对 `resets_at`。
- 警示色默认 used ≥ 80% 黄色，used ≥ 95% 橙红，作为可调参数。
- 默认停在构建和测试，不烧录。

## 接口冻结顺序

依赖关系：1 → 2 → 3 → 4 和 5 可并行 → 6 → 7 → 8。

1. 冻结逻辑模型：Snapshot、Provider、Window 的字段、空值、unlimited、valid、稳定 ID、排序和回退规则。
2. 冻结 BLE v5 线协议：消息类型、键名、批次、序列号、完成标记、原子提交、超时、上限和截断规则。
3. 冻结 fixture：Codex + Claude 合成数据、golden 编码、每条消息不超过 250 bytes、批次总预算和字节预算测试。
4. Companion 按 fixture 实现编码、批次发送和测试。
5. 固件按 fixture 实现解析状态机、原子提交和测试。
6. UI 消费设备侧 AppState，实现四页和 Usage 交互。
7. 集成验证：Companion `--once` 输出交给固件解析测试，跑完整测试和构建。
8. 文档冻结：更新 `docs/BLE_PROTOCOL.md`、[[INTERFACE-CONTRACT]]、`ARCHITECTURE.md` 和验证记录。

4 和 5 可以并行，但都必须以第 3 步冻结的 fixture 为准。任何一方需要改键名或语义，先改 fixture 和契约，再改实现。

## BLE v5 草案

现有 v4 单包 250 bytes 无法容纳多供应商窗口。建议升级到 v5，继续使用现有 Status characteristic，每条消息仍是 JSON 文本，单条不超过 250 bytes。v5 命令和结果继续使用原 UUID，只把版本号升到 5。

### 消息类型

| `k` | 含义 | 数量 |
|---|---|---|
| `s` | 汇总和基础统计 | 每批恰好 1 条 |
| `p` | 供应商元数据 | 每个供应商 1 条 |
| `w` | 额度窗口 | 每个窗口 1 条 |
| `e` | 批次结束和提交 | 每批恰好 1 条 |

所有消息共享同一个快照序列号 `s`。设备收到新 `s` 时重建 staging，收到合法 `e` 且计数匹配时原子提交。旧快照在提交前保持不变。

### 汇总消息

```json
{"v":5,"s":42,"k":"s","t":1792426620,"o":480,"r":5,"d":27233,"f":65,"z":4,"y":938,"l":1200,"m":"gpt-5.6-sol","h":200,"n":9684,"b":0,"x":[22,614,26356,5532,65728,3535,41974,714,"trae/gpt-5.6-sol",571],"c":2,"w":3,"tr":0,"to":0}
```

- `t`：`generated_at`，UTC Unix 秒。
- `o`：`utc_offset_minutes`。
- `r`：所有有限有效窗口中的最低 remaining 百分比，用于 Overview 的 LOWEST QUOTA；无有效窗口时省略。
- `d` 到 `x`：保留 v4 的基础统计字段，继续供 Overview、Sessions、Context 使用。
- `c`：供应商数量。
- `w`：窗口总数。
- `tr`：1 表示发生截断。
- `to`：被截断的供应商和窗口总数。

### 供应商消息

```json
{"v":5,"s":42,"k":"p","i":0,"c":2,"id":"codex","m":"Codex","ta":1792424903}
```

- `i`：供应商索引，从 0 开始。
- `c`：供应商总数。
- `id`：稳定供应商 ID，最多 23 字符，不得包含邮箱或凭据。
- `m`：显示名称，最多 23 字符。
- `ta`：`read_at`，UTC Unix 秒；未知时省略。

### 窗口消息

```json
{"v":5,"s":42,"k":"w","p":0,"i":0,"w":2,"id":"7d","m":"7 days","u":94,"r":6,"ra":1792856020,"ul":0,"ok":1}
```

- `p`：所属供应商索引。
- `i`：窗口在供应商内的索引，主窗口为 0。
- `w`：该供应商的窗口总数。
- `id`：稳定窗口 ID，最多 15 字符。
- `m`：窗口显示名称，最多 23 字符。
- `u`：used 百分比 0 到 100；未知时省略。
- `r`：remaining 百分比 0 到 100；未知时省略。
- `ra`：`resets_at`，UTC Unix 秒；未知时省略。
- `ul`：1 表示无限额度。
- `ok`：1 表示数据有效，0 表示存在但无法确定额度。

缺失字段用省略表示，不使用 0 或 100 代替。unlimited、unknown 和 missing 必须由 UI 分别呈现。

### 结束消息

```json
{"v":5,"s":42,"k":"e","c":2,"w":3,"tr":0,"to":0}
```

设备校验汇总存在、供应商数量、窗口总数和每个槽位完整后提交。计数不符、消息丢失、乱序、重复或超时都不得提交部分数据。

### 批次和失败规则

- 设备在 `s` 变化时丢弃未完成 staging，开始新批次。
- 设备忽略 `s` 小于等于最后已提交序列号的批次，连接建立时除外。
- 设备在收到合法 `e` 时提交，记录 `last_committed_seq`。
- staging 超过 5 秒未收到 `e` 时丢弃，保留旧快照。
- 同一 `i` 重复写入时覆盖同一槽位。
- 单条消息超过 250 bytes 时 Companion 必须拆条或截断，不得发送超长消息。
- 协议上限建议：最多 8 个供应商，每个供应商最多 4 个窗口，总窗口最多 16 个。超过时按供应商顺序和窗口顺序截断，设置 `tr=1` 和 `to`，UI 显示 `TRUNCATED +N`。
- 最坏情况 fixture 必须验证每条消息不超过 250 bytes，批次消息数不超过 32。

### 命令和结果

```json
{"v":5,"sid":319028314,"id":7,"a":"focus_magpie"}
{"v":5,"id":7,"ok":1,"m":"FOCUSED"}
```

动作白名单、session ID、request ID 和去重逻辑保持不变。设备信息特征返回 `{"v":5,"fw":"0.5.0","screen":"466x466"}`。

## Companion 实施

### 数据模型

在 `companion/codex_display/metrics.py` 增加：

- `WindowSnapshot`：`id`、`name`、`used_percent`、`remaining_percent`、`resets_at`、`unlimited`、`valid`。
- `ProviderSnapshot`：`id`、`name`、`read_at`、`windows`。
- `Snapshot` 增加 `providers`，保留基础统计字段。
- 移除全局 `_most_constrained_window()` 作为 Usage 选择逻辑，改为按供应商和窗口解析。
- 保留 `remaining_percent` 作为 Overview 的最低额度汇总，无有效窗口时为 `None`。

### 解析和排序

- 只使用 `windows[].resetsAt`，忽略 provider 顶层 `resets` 对象。
- used 和 remaining 分别解析。只有一侧有效时用 100 减另一侧补齐。两侧冲突时以 used 为环图来源，remaining 由 used 推导，并记录不含敏感信息的诊断。
- 缺失、unknown、unlimited 分别保留，不默认 100 或 0。
- 稳定 ID：优先使用源字段；源无 ID 时按 provider 和窗口名称生成短 slug。多账户时使用不可逆短哈希区分，绝不传输明文邮箱。
- 供应商排序：Codex 优先，其次 Claude，其他按 API 顺序。
- 窗口排序：有限且 remaining 已知的窗口按 remaining 升序，主窗口为最低 remaining；未知窗口和无限窗口排在后面。主窗口索引为 0。
- 供应商消失时 UI 按稳定 ID 回退，Companion 不创建空供应商。

### 编码和发送

- `protocol.py` 增加 v5 汇总、供应商、窗口、结束消息编码。
- `BleCompanion` 每次状态更新发送完整批次：汇总、供应商、窗口、结束。
- 每条消息独立检查 250 bytes 上限。
- 连接建立或重连后先刷新数据，再发送完整批次。
- 保留 `--once` 模式，输出整批 JSON 消息或可读摘要，便于 fixture 对接。

### 测试

- `test_metrics.py`：多供应商、多窗口、缺字段、无限额度、冲突、多账户、排序、回退、忽略 provider 顶层 resets。
- `test_protocol.py`：每条消息字节预算、批次计数、截断标志、v5 命令和结果、长名称边界。
- `test_ble.py`：完整批次顺序、重连后重发、状态变化发送、命令去重。
- 新增 `companion/tests/fixtures/`：Codex + Claude 合成数据，Claude 明确标注测试数据，不包含真实邮箱、密钥或请求正文。
- 新增 golden fixture 测试：编码结果和字节长度固定，固件解析测试共用同一份 fixture。

## 固件实施

### 板级 bring-up

官方 1.75C 资料已核对：

- 屏幕 466×466，CO5300，QSPI。
- 触控 CST9217，I2C。
- PMU AXP2101。
- LCD SDIO0 到 SDIO3 为 GPIO4 到 GPIO7，SCLK GPIO38，CS GPIO12。
- LCD reset GPIO1，触控 reset GPIO2。
- I2C SDA GPIO15，SCL GPIO14，触控中断 GPIO11。
- 原理图网络名包含 GPIO0、GPIO18 和 PWR，按键映射需在真机确认。

计划修改：

- `board_config.h`：屏幕尺寸改为 466，LCD reset 改为 1，触控 reset 改为 2，保留其余引脚。
- `main.cpp`：`Arduino_CO5300` 使用官方 1.75C 构造函数参数，列偏移 6；移除 2.16 专用的 `writeC8D8(0x36, 0xA0)`，除非真机验证需要。
- `initTouch()`：按官方示例设置 466×466，先手动复位 TP_RST，再 `touch.begin`，使用 `setMirrorXY(true, true)`，不启用 `setSwapXY`。真机确认坐标方向。
- `platformio.ini`：新增 `esp32-s3-touch-amoled-175c` 环境，使用 32 MB Flash、8 MB PSRAM 的板级配置和分区表。旧 2.16 环境保留，但旧构建成功不算 1.75C 验证。
- 按键映射：GPIO0 短按熄屏/亮屏，长按 3 秒清除绑定；GPIO18 短按翻页，长按 1 秒快捷操作；PWR 长按 6 秒关机。真机确认。

### 数据模型和解析

- `AppState` 增加固定上限的供应商和窗口数组。建议最多 8 个供应商，每个最多 4 个窗口，总窗口最多 16 个。
- 窗口字段：`id`、`name`、`used_percent`、`remaining_percent`、`resets_at`、`unlimited`、`valid`。百分比用 `int16_t`，未知用 -1。
- 供应商字段：`id`、`name`、`read_at`、`windows`、`window_count`。
- `AppState` 增加 `batch_truncated`、`batch_truncated_count`、`lowest_remaining_percent`。
- 解析器按消息类型写入 staging，收到结束消息后校验计数并一次性入队。
- 批次超时、丢包、乱序、重复和新批次覆盖规则按 v5 草案实现。
- 设备侧不再从全局最低额度选择 Usage 供应商，UI 只按供应商列表渲染。

### UI 圆屏布局

屏幕 466×466，圆心 (233,233)，半径 233。主内容放在 320×320 居中安全框内，四角不放置文字或卡片。顶部品牌行和底部页面圆点使用圆内较宽的中央区域。

全局元素：

- 顶部居中 `MAGPIE` 文字和状态点，连接状态显示 ONLINE、STALE、OFFLINE、SYNCING。
- 底部居中四个页面圆点，当前页绿色，其他灰色。
- Usage 页右侧竖排供应商圆点，仅供应商数量大于 1 时显示。最多 8 个，超出时显示截断提示。
- 电池或充电标签放在底部右侧圆内，无效时隐藏。
- 当前时间是否保留为全局元素待实现时按空间决定，不阻塞验收。Usage 页必须显示重置的本地日期时间。

Usage 页：

- 顶部 `USAGE` 小标题，下面显示供应商名称和主窗口名称，例如 `CODEX · 7 DAYS`。
- 主环直径约 190 到 210 px，270° 开口弧，底部留口。
- 环中心显示 used 百分比，使用 `font_montserrat_100` 或数字 100 px 加 `%` 32 px。下方显示 `USED` 和 `remaining% REMAINING`。
- 环下方显示 `RESETS IN 5d 15h` 和本地重置时间 `10/27 15:33`。
- 主窗口下方显示最多 2 到 3 条紧凑次要窗口行，每条包含窗口名、百分比和重置倒计时。超出时显示 `+N MORE` 或 `TRUNCATED`，不静默丢弃。
- 无限额度显示 `UNLIMITED`，不显示假百分比。未知显示 `UNKNOWN` 或 `--`。过期时保留数值并降亮，同时显示 STALE 或 OFFLINE。
- 供应商上下滑使用整页吸附动画，切换后保留稳定 ID。供应商消失时回退到列表第一项。

其他三页：

- Overview：成功环、CALLS、ERRORS、LOWEST QUOTA，全部在安全框内。
- Sessions：30 天会话数、总 Token、估算费用、TOP MODEL。
- Context：缓存命中环、Token 构成条、Input、Output、Cache、Reasoning 数值。
- 四页都只用圆内安全区，不把 480×480 坐标直接缩放。

### 手势和状态

- 自定义手势识别：按下记录起点，移动超过阈值后按主要方向锁定为水平或垂直，锁定后忽略另一方向。
- 水平滑动切换四个主页面，垂直滑动仅在 Usage 页切换供应商。
- 只有一个供应商时隐藏竖排圆点，不创建空页面。
- 快捷操作浮层、配对浮层和 toast 在 466×466 下重新排版，按钮不被圆边裁切。
- 保留 15 秒刷新、60 秒无操作熄屏、触摸或按键唤醒、首次触摸只唤醒不操作。
- 保留 60 秒无新快照标记 STALE。BLE 连接但无有效批次超过 60 秒显示 STALE；BLE 断开显示 OFFLINE。检查现有 45 秒 BLE watchdog，必要时延长到 70 秒以上，避免 OFFLINE 抢在 STALE 之前。
- 到零显示 WAITING 或等待更新，不假定额度恢复。
- 倒计时长时间显示天/小时，临近重置显示分钟/秒。
- 保留 BLE Secure Connections、六位配对码、bonding、自动重连和动作白名单。

## 实施阶段和优先级

### P0 接口和基础

1. 建立共同准备提交，纳入现有交接文档和本计划。
2. 创建独立 worktree：`/Users/bruce/dev/magpie-usage-display-round-firmware` 和 `/Users/bruce/dev/magpie-usage-display-round-companion`。
3. 安装或确认 Python 3.11、PlatformIO 和项目 `.pio-venv`。当前机器只有 Python 3.14 和 3.15，`pio` 不在 PATH，需要先补齐。
4. 写 v5 契约和 fixture，跑字节预算测试。

### P1 Companion

1. 多供应商解析、排序、稳定 ID、used/remaining 补齐和冲突处理。
2. v5 编码和批次发送。
3. 单元测试和 `--once` 验证。

### P2 固件核心

1. 1.75C 引脚、显示、触控、PMU 和构建环境。
2. v5 解析状态机、原子提交、超时和截断。
3. AppState 和 UI 数据接口。

### P3 UI 和交互

1. 四页圆屏重排。
2. Usage 主窗口、次要窗口、倒计时和状态。
3. 水平/垂直手势、供应商选择保留和回退。
4. 快捷操作、配对浮层、熄屏和唤醒。

### P4 集成和交付

1. 全量测试和 1.75C 构建。
2. 更新协议、契约、架构和验证记录。
3. 生成阶段性交接包。

5 小时 usage 达到 80% 时，按当前完成度交接。P0、P1 和 P2 的核心优先于 P3 的视觉打磨；P4 的文档和验证记录必须随交接一起给出。

## 验收清单

| 编号 | 验收项 | 方法 |
|---|---|---|
| A1 | 交接文档和计划形成共同准备提交，两个 worktree 从同一 commit 启动 | git log、git status |
| A2 | 仓库、fixture、日志和截图中没有 API Key、邮箱、请求正文 | 搜索和人工检查 |
| A3 | 未授权不 Fork、不推送、不安装 LaunchAgent、不烧录 | 过程检查 |
| B1 | 当前 Codex 数据正确显示 95% used、5% remaining、2026-10-27 15:33:40 +08:00 | `--once` 和 fixture |
| B2 | Codex + Claude 合成 fixture 各自显示百分比和倒计时，Claude 标注测试数据 | 测试和 fixture |
| B3 | 供应商顺序 Codex > Claude > 其他，消失时回退 | 单元测试和 UI 测试 |
| B4 | 主窗口为最受限有限窗口，所有窗口保留，不混用百分比和重置时间 | 单元测试和 fixture |
| B5 | 缺字段、unknown、unlimited 分别呈现，不默认 100 或 0 | 单元测试和 UI 检查 |
| B6 | used 和 remaining 冲突时记录来源并明确采用值 | 单元测试和日志检查 |
| B7 | 稳定 ID 能区分多账户且不泄露邮箱 | 单元测试 |
| B8 | 忽略 provider 顶层 resets，只使用窗口 resetsAt | 单元测试 |
| C1 | 每条 v5 消息不超过 250 bytes，最坏 fixture 通过 | 字节预算测试 |
| C2 | 汇总、供应商、窗口、结束消息计数匹配后原子提交 | 协议测试和设备解析测试 |
| C3 | 丢包、乱序、重复、新批次和超时都不提交部分数据 | 协议测试和状态机测试 |
| C4 | 截断时设置 `tr` 和 `to`，UI 显示提示，不静默丢弃 | 协议测试和 UI 检查 |
| C5 | v5 命令、结果、白名单和去重行为不变 | 单元测试 |
| C6 | 设备信息报告 v5 和 466×466 | BLE 读取或固件检查 |
| D1 | 1.75C 目标板环境构建通过 | `pio run -e esp32-s3-touch-amoled-175c` |
| D2 | 引脚与官方 1.75C 一致，LCD reset 1，触控 reset 2，466×466 | 代码审查和真机 |
| D3 | 显示初始化和列偏移按官方示例，触控方向正确 | 真机目视和触摸测试 |
| D4 | AXP2101、GPIO0、GPIO18、PWR 行为符合目标板 | 真机按键和电源测试 |
| D5 | 60 秒无操作熄屏，触摸或按键唤醒，首次触摸只唤醒 | 真机测试 |
| D6 | 60 秒无新快照显示 STALE，断连显示 OFFLINE，保留数据并降亮 | 真机或状态注入测试 |
| D7 | BLE 配对、bonding、自动重连和安全特征保持不变 | 配对和重连测试 |
| E1 | 四页顺序正确，水平滑动和页面圆点正常 | 真机或模拟器 |
| E2 | Usage 垂直滑动切换供应商，单供应商隐藏圆点且无空页 | 真机或模拟器 |
| E3 | Usage 显示供应商、窗口名、used、remaining、倒计时和本地重置时间 | 真机或模拟器 |
| E4 | 主窗口加紧凑次要行，超出时显示 `+N MORE` 或 `TRUNCATED` | 真机或模拟器 |
| E5 | 默认 Codex；Claude 只来自真实数据或明确合成 fixture | fixture 和数据检查 |
| E6 | 缺字段、unknown、unlimited 的视觉区分正确 | 真机或模拟器 |
| E7 | 警示色阈值可调，到零显示 WAITING | 代码审查和状态注入 |
| E8 | 四个页面内容都在 466 圆内，无圆边裁切 | 真机或 466×466 预览 |
| E9 | 倒计时长时间显示天/小时，临近显示分钟/秒，时区正确 | 单元测试和状态注入 |
| E10 | 快捷操作、配对浮层、toast 和电池显示在圆内正常 | 真机或模拟器 |
| F1 | `python3 -m unittest discover -s companion/tests -v` 全部通过 | 本地运行 |
| F2 | `pio run -e esp32-s3-touch-amoled-175c` 通过 | 本地运行 |
| F3 | Companion `--once` 输出可被固件解析 fixture 消费 | 集成测试 |
| F4 | `docs/BLE_PROTOCOL.md`、[[INTERFACE-CONTRACT]]、`ARCHITECTURE.md` 和验证记录已更新 | 文档审查 |
| F5 | 阶段性交接包含分支、commit、改动范围、测试结果、未完成项、风险和 Agent B 任务 | 交接文档 |

真机不可用或未授权烧录时，D3、D4、D5、D6、D7、E1 到 E10 中的真机项明确标记为待验证，不能声称已通过。

## 风险和待确认

- 当前机器没有 `pio`，只有 Python 3.14 和 3.15，项目要求 Python 3.11。需要安装工具链或由用户提供环境。
- 1.75C 的 GPIO0、GPIO18 和 PWR 按键行为需真机确认，原理图网络名只能作为线索。
- 触控坐标方向、镜像和显示列偏移需真机确认，不能只按芯片型号迁移。
- 32 MB Flash 的 PlatformIO 板级定义和分区表需要新配置，现有 `n16r8` 不一定匹配。
- Magpie 当前只返回 Codex，Claude 和 5 小时窗口没有真实数据，验收只能用合成 fixture。
- 实际 BLE MTU 和写入吞吐需在真机测量，批次消息数上限可能需要调整。
- 没有 LVGL 模拟器时，466×466 圆边裁切只能靠坐标审查和真机验证。
- 5 小时 usage 达到 80% 的时机可能落在 P3 或 P4，交接时按实际完成度调整 Agent B 任务。

## 待用户确认

1. 是否确认本实施计划、优先级和验收清单。
2. 是否同意 v5 草案作为冻结基础，或需要调整消息类型、字段或上限。
3. 是否同意先把现有交接文档和本计划提交为共同准备提交，并从该提交创建两个 worktree。
4. 是否可以安装 Python 3.11、PlatformIO 和项目虚拟环境。
5. 1.75C 真机是否可用，是否允许在构建和测试完成后进行烧录验证。默认不烧录。
6. UI 是否保留当前时间和 Magpie logo。默认按概念图使用居中 MAGPIE 文字和状态点，不显示 logo。

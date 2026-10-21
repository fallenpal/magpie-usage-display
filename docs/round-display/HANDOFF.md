## 阶段性交接记录

日期：2026-10-22。阶段：Companion 与固件实现完成，Agent B 已完成串行集成、自动化测试与目标板构建，Agent A 已复核并接收交接。真机烧录未进行。

### 分支与提交

| Worktree | 分支 | 提交 | 状态 |
| --- | --- | --- | --- |
| `/Users/bruce/dev/magpie-usage-display-round-firmware` | `codex/round-display-ui` | `5b2635c` | 工作区干净 |
| `/Users/bruce/dev/magpie-usage-display-round-companion` | `codex/provider-usage` | `4e0b97c` | 工作区干净，已合入 main |
| `/Users/bruce/dev/magpie-usage-display-round` | `main` | `edcd062` | 交接已接收，两条分支均已合入 |

两条分支都从 `ae7ede4` 出发。没有推送，没有个人 origin，没有创建 GitHub Fork。

### 固件改动范围

新增：

- `boards/waveshare-esp32-s3-touch-amoled-175c.json`：32 MB Flash、8 MB PSRAM，分区表 `default_32MB.csv`。
- `src/app_state_types.h`：供应商、窗口和配额类型，固定上限 8、4、16。
- `src/status_parser.h`、`src/status_parser.cpp`：BLE v5 批次原子解析。
- `test/fixtures/v5_sequence_42.jsonl`：与 Companion 共用的 golden fixture，SHA-256 为 `9efd774f0482cca959c48d4e7c5149428cabd9b56b2786857e6180e8623c99c6`。
- `test/test_status_parser/test_main.cpp`：9 个 Unity 解析器测试，含乱序和计数溢出用例。
- [[VERIFICATION]]：构建、测试和待验证项记录。

修改：

- `platformio.ini`：新增 1.75C 环境和 `native` 测试环境，默认环境切到 1.75C，保留 2.16 回归环境。
- `src/app_state.cpp`、`src/app_state.h`：绝对重置时间、本地时间格式化、百分比格式化。
- `src/ble_bridge.cpp`：协议升到 v5，接入解析器，单条 250 bytes 上限，watchdog 延长到 75 秒。
- `src/board_config.h`：466×466，LCD 复位 GPIO1，触摸复位 GPIO2，列偏移 6，触摸镜像 XY。
- `src/main.cpp`：方向锁定手势、60 秒熄屏、触摸复位时序。
- `src/ui.cpp`、`src/ui.h`：466×466 四页布局、Usage 供应商切换、STALE 遮罩和警示色。
- 根目录和 `docs/round-display/` 下的状态文档。

### 验证结果

| 项目 | 结果 |
| --- | --- |
| `pio run -e esp32-s3-touch-amoled-175c` | 通过，36.904 秒，Flash 1,243,179 bytes，9.5%，RAM 44.3% |
| `pio run -e esp32-s3-touch-amoled-216` | 历史通过，23.68 秒，本轮未重跑 |
| `pio test -e native` | 9 个测试通过，0.414 秒 |
| Companion `unittest discover` | 43 个测试通过，0.132 秒 |
| Companion `--once` | 历史已验证，本轮未重跑 |

复现命令及扫描候选位置见 [[VERIFICATION]]。两份 fixture SHA-256 一致。

合并历史：`87c3b18` 合入 `codex/provider-usage` 的 `4e0b97c`，随后 `49fa7ea` 合入 `codex/round-display-ui` 的 `5b2635c`。最终 merge commit 为 `49fa7ea13e64c044cf1e1f3896211618c3404daa`，后续文档提交不改变实现基线。

秘密扫描检查 70 个文本文件，未发现常见密钥格式命中；6 处测试或 fixture 候选的来源未全部独立证实，不能宣称无真实账户标识。完整范围和限制见 [[VERIFICATION]]。

### 未完成项

- CO5300 初始化、列偏移、显示方向和亮度需要真机确认。
- CST9217 触摸坐标、镜像、复位时序和手势方向需要真机确认。
- GPIO0、GPIO18、PWR 按键映射、AXP2101 电量与长按关机需要真机确认。
- BLE 配对、bonding、75 秒 watchdog 和真实 MTU 下的批次写入需要真机确认。
- 圆边裁切、长名称和 4 窗口排版需要真机确认。
- Companion 到固件的端到端 BLE 发送尚未在真机执行。
- 真机烧录、后台服务安装、推送和 Fork 均未授权。

### 风险

- IRAM 静态占用显示 100%，链接成功，但 BLE 与显示并发稳定性只能在真机观察。
- LVGL 内置字体只覆盖 ASCII 和少量符号。界面已移除 U+00B7，真机需检查所有可见文本。
- 5 秒 staging 超时和 15 秒发送间隔在真实 BLE 吞吐下可能偏紧，批次消息上限可能需要调整。
- 当前只有 Codex 真实数据，Claude 和 Gemini 排版只由合成 fixture 覆盖。
- 固件按接收顺序显示供应商，排序正确性由 Companion 保证；固件验收需确认稳定 ID 选择在供应商消失时回退第一项。

### Agent B 任务

1. 在 `/Users/bruce/dev/magpie-usage-display-round` 的 `main` 上串行合入 `codex/provider-usage` 和 `codex/round-display-ui`，保留两位 Agent 的提交记录。
2. 合并后运行 Companion 测试、固件 native 测试和 1.75C 构建，确认 fixture 两份副本一致。
3. 检查 [[INTERFACE-CONTRACT]]、[[../BLE_PROTOCOL]]、[[VERIFICATION]] 与代码一致，更新集成结果。
4. 检查仓库没有 API Key、账户邮箱、请求正文和真实账户标识。
5. 不烧录、不安装后台服务、不推送、不创建 Fork。真机验证项保持待验证。
6. 交付最终 commit、测试输出、文档差异和遗留风险，交给 Agent A 验收。

### 验收清单

- [x] BLE v5 协议冻结，Companion 编码和固件解析使用同一 fixture。
- [x] 批次原子提交、32 条上限、250 bytes 上限、5 秒超时、序列忽略规则有测试。
- [x] 1.75C 目标板构建通过（本轮重跑）；2.16 回归构建为历史结果，本轮未重跑。
- [x] 466×466 四页布局和 Usage 供应商切换已实现。
- [x] OFFLINE、STALE、SYNCING、ONLINE 分开显示，过期数据保留并降亮。
- [x] 两条分支合入 main 并通过自动化测试和 1.75C 构建。
- [x] Agent A 复核测试、构建、合并历史和秘密扫描候选，交接已接收。
- [ ] 真机硬件和端到端 BLE 验证。

### 2026-10-22 真机更新

用户已授权并完成首次烧录。发现并修复 BLEWriteCB 栈溢出，实际固件提交为 `6c29b33`；认证成功并连续接收真实 Codex、Claude 数据。详细备份、恢复、日志和待验收项见 [[FLASHING-2026-10-22]]。前文“未烧录”描述为历史状态。

## 验证记录

记录日期：2026-10-22。范围：Companion v5 编码与固件 1.75C 适配的本地构建、自动化测试和静态检查。没有烧录设备，没有安装后台服务，没有连接真实 BLE 设备，没有创建 GitHub Fork。

### 环境

- macOS，项目虚拟环境 `/Users/bruce/dev/magpie-usage-display-round/.pio-venv`。
- Python 3.11，PlatformIO Core 6.2.0。
- pioarduino platform-espressif32 55.03.35，ArduinoJson 7.4.2，LVGL 8.4.0。
- 固件目标环境：`esp32-s3-touch-amoled-175c`，自定义板级 JSON 为 32 MB Flash、8 MB PSRAM，分区表 `default_32MB.csv`。
- 旧环境 `esp32-s3-touch-amoled-216` 保留，仅用于回归构建，不代表 1.75C 验证。

### 构建结果

| 项目 | 结果 |
| --- | --- |
| `pio run -e esp32-s3-touch-amoled-175c` | 通过，29.28 秒 |
| 目标板 Flash 使用 | 1,242,499 / 13,107,200 bytes，9.5% |
| 目标板 RAM 使用 | 145,028 / 327,680 bytes，44.3% |
| 镜像大小 | 1,242,755 bytes |
| esptool 参数 | `--flash-size 32MB` |
| `pio run -e esp32-s3-touch-amoled-216` | 通过，23.68 秒，旧方屏环境回归构建 |

构建产物位于 `.pio/build/esp32-s3-touch-amoled-175c/firmware.bin` 和 `firmware.elf`。IRAM 静态占用显示为 100%，链接成功，但真机 bring-up 时仍需观察 BLE 和显示并发下的稳定性。

### 自动化测试

| 套件 | 命令 | 结果 |
| --- | --- | --- |
| Companion | `python3 -m unittest discover -s companion/tests -v` | 43 个测试通过 |
| 固件解析器 | `pio test -e native` | 7 个测试通过 |

固件解析器测试覆盖：

- 共享 golden fixture `v5_sequence_42.jsonl` 的 12 条消息，逐条 staging，最后一条原子提交。
- 提交后的重复序列被忽略，重连后允许同一序列重新提交。
- 5 秒 staging 超时丢弃未完成批次，保留旧快照。
- 缺少窗口消息时批次不提交，输出状态保持原值。
- 只提供 used 或 remaining 一侧时派生另一侧，不伪造 0 或 100。
- 单批次 32 条消息上限。
- 更高序列的新批次覆盖未完成 staging。

fixture 由 Companion 和固件共用。固件 worktree 的副本位于 `test/fixtures/v5_sequence_42.jsonl`，内容与 `companion/tests/fixtures/v5_sequence_42.jsonl` 相同，全部为合成数据，不含真实邮箱、API Key 或请求正文。

### 静态检查与协议验收

- BLE 协议版本为 v5，设备信息上报 `{"v":5,"fw":"0.5.0","screen":"466x466"}`。
- 固件解析 `s` 汇总、`x` 累计统计、`p` 供应商、`w` 窗口和 `e` 结束消息。
- 只有汇总、统计和全部供应商、窗口计数完整时才提交，提交前不覆盖旧快照。
- 单条消息 250 bytes 上限、单批次 32 条上限、5 秒超时和已提交序列忽略规则已实现。
- 解析器拒绝缺失或超限的供应商、窗口计数，拒绝布尔或越界百分比。
- AppState 最多 8 个供应商、每个最多 4 个窗口、总计最多 16 个窗口。
- Usage 页使用 270° 开口弧，主窗口显示 used、remaining、倒计时和本地重置时间，最多 3 条次要窗口。
- 供应商顺序在 Companion 固定为 Codex、Claude、其他；固件按接收顺序显示，按稳定 ID 保留选择，供应商消失时回退第一项。
- OFFLINE、STALE、SYNCING、ONLINE 四种状态分开显示；非在线状态保留最后数据，通过半透明遮罩降亮。
- BLE 状态 watchdog 延长到 75 秒，晚于 60 秒 STALE 阈值，避免先断开再显示过期。
- 警示色阈值为 used ≥ 80% 黄色、used ≥ 95% 橙红色，定义在 `src/ui.cpp` 顶部，默认不闪烁。
- 水平滑动切换四个主页面，垂直滑动只在 Usage 页切换供应商，方向锁定后忽略另一方向。
- 60 秒无操作熄屏，触摸或按键唤醒；配对码显示期间不自动熄屏。
- 16 MB 和 32 MB 两个 PlatformIO 环境都可构建。

### 尚未验证

以下项目需要真机和显式烧录授权：

- CO5300 初始化、列偏移 6、466×466 实际显示方向和亮度。
- CST9217 触摸坐标、镜像方向、GPIO1 和 GPIO2 复位时序。
- GPIO0、GPIO18 和 PWR 按键的实际映射和长短按行为。
- AXP2101 电量、充电状态和长按关机。
- BLE Secure Connections、六位配对码、bonding、自动重连和 75 秒 watchdog 的实际时序。
- 圆形安全区在真实字体渲染下的裁切、长供应商名称和 4 个窗口的排版。
- STALE 60 秒、OFFLINE 断连、5 秒 staging 超时和屏幕熄屏的实机时序。
- Companion 通过真实 BLE 发送完整批次的端到端流程。当前只验证了 Companion 的 `--once` 本机输出和固件 fixture 解析。

### 安全边界

没有执行 `pio run -t upload`、`esptool write_flash`、擦除 Flash 或修改分区。没有创建 LaunchAgent 或其他后台服务。没有 fork、push 或创建 GitHub Release。真机烧录前需要单独确认恢复方案和串口设备路径。

### 复现命令

```sh
cd /Users/bruce/dev/magpie-usage-display-round-firmware
/Users/bruce/dev/magpie-usage-display-round/.pio-venv/bin/pio test -e native
/Users/bruce/dev/magpie-usage-display-round/.pio-venv/bin/pio run -e esp32-s3-touch-amoled-175c

cd /Users/bruce/dev/magpie-usage-display-round-companion
/Users/bruce/dev/magpie-usage-display-round/.pio-venv/bin/python -m unittest discover -s companion/tests -v
```

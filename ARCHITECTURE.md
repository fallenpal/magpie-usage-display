## 当前架构与目标

Magpie 本地 HTTP API → macOS Python Companion → 加密 BLE → ESP32-S3 LVGL 圆屏。

companion/ 负责接口轮询、统计、编码及 BLE；src/ 负责板级启动、状态、BLE 接收和 UI；platformio.ini 为固件构建配置。现有上游面向 480×480 的 2.16 开发板，本项目目标为 466×466 圆屏 1.75C。

设计决策：额度按供应商账户及窗口展示；Usage 横向属于四个主页面之一，纵向切换供应商；重置时间以绝对时间传输，设备本地倒计时；完整快照原子更新，过期和未知显式呈现。多供应商不能沿用全局最小额度选择逻辑。

BLE v5 多供应商批次协议已冻结，Companion 编码和测试已完成，细节见 docs/round-display/INTERFACE-CONTRACT.md。Agent A 负责固件，Agent B 负责 Companion 并集成。硬件引脚和触控驱动必须查官方 1.75C 文档，不能仅按芯片型号迁移。

当前状态：Companion 多供应商适配已在 codex/provider-usage 分支实现，43 个测试通过，并已对本机 Magpie 做只读 --once 验证。固件 1.75C 适配已在 codex/round-display-ui 分支实现，7 个解析器测试通过，1.75C 目标板构建通过，详见 docs/round-display/VERIFICATION.md。尚未烧录，未创建 GitHub Fork。

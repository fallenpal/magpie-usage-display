## 可直接发给 Agent A 的任务

请接手 /Users/bruce/dev/magpie-usage-display-round 的圆屏固件和界面适配。先读根目录 AGENTS.md、ARCHITECTURE.md，以及 docs/round-display 下 REQUIREMENTS.md、DATA-EVIDENCE.md、INTERFACE-CONTRACT.md 和最新版图片 assets/concept-v2-usage-providers.png。

在独立 worktree /Users/bruce/dev/magpie-usage-display-round-firmware，使用短期分支 codex/round-display-ui。开始前核对当前基线和交接文件实际状态，若交接包尚未提交，先让集成人建立共同准备提交，再从该提交创建 worktree，不遗漏图片与文档。

负责 src/、esp32/、src/fonts/、platformio.ini、lv_conf.h 以及需要的目标板构建配置。核对微雪 1.75C 引脚及 CO5300/CST9217 驱动，适配 466×466 圆屏。实现四页界面、Usage 上下滑供应商、倒计时、状态处理和 BLE 接收模型。

不修改 companion/ 或 docs/BLE_PROTOCOL.md，协议以 [[INTERFACE-CONTRACT]] 和 companion/tests/fixtures/v5_sequence_42.jsonl 为准。先用合成 fixture 开发，不依赖 Claude 真实账户。

解析器必须接收 `k:"s"` 汇总和 `k:"x"` 累计统计两种消息，二者都在才允许提交；供应商和窗口按 `c`/`w` 计数校验。批次上限 32 条，超过 5 秒未收到 `e` 时丢弃 staging，保留旧快照。

验证目标板编译、圆形边界排版、手势方向锁定和数据边界；记录尚未完成的真机验证。不要未授权烧录、擦除当前固件或安装后台服务。

交付：分支、commit、改动范围、构建结果、预览和硬件待验证清单，交给 Agent B 串行集成。不要创建按 Agent 命名的长期分支。

## 可直接发给 Agent B 的任务

请接手 /Users/bruce/dev/magpie-usage-display-round 的 Mac Companion 多供应商额度适配，并担任集成人。先读根目录 AGENTS.md、ARCHITECTURE.md 和 docs/round-display 的全部需求与接口文档。

先检查 git status，仅将这次交接资料形成共同准备提交（不收走无关改动），记录 commit。两位 Agent 都从同一准备提交启动。自己的独立 worktree 路径 /Users/bruce/dev/magpie-usage-display-round-companion，短期分支 codex/provider-usage。

负责 companion/、docs/BLE_PROTOCOL.md、docs/round-display/INTERFACE-CONTRACT.md、协议 fixture 和数据测试。Agent A 负责固件及 platformio.ini 等板级配置，不能互相覆盖。

只读核对本机 http://127.0.0.1:3425 的 API；按供应商和窗口保留额度及绝对重置时间，未知值不伪造。不要记录邮箱、API Key 或请求正文。Claude 当前未核实，只用明确的合成 fixture。

优先冻结多供应商 BLE 线协议，给出样例、版本兼容策略、250-byte 预算、拆包、快照完整性和失败处理，供 Agent A 对接。具体字段参照 INTERFACE-CONTRACT.md，不要让 UI 将账户额度标成 GPT-5 独立额度。

运行有意义的数据及协议测试，覆盖多供应商、多窗口、缺字段、无限额度、超长消息和时间边界。保留原有安全机制及快捷操作白名单。

作为集成人，检查并串行合入两条分支到本地主干 main，执行最终契约和构建验证。远程 Fork、origin、推送以及设备刷写根据后续用户授权处理，不假称已完成。给出最终 commit、验证结果和可恢复的刷写方案，实际刷写前取得用户确认。

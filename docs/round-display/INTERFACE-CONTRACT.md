## 两个 Agent 的共享接口约定

状态：逻辑数据模型已确定，线协议编码由 Agent B 提交设计后冻结。本文不能冒充已实现协议。Agent A 先依赖 fixture 和逻辑模型开发，不自行设计另一套消息键。

逻辑快照包含 generated_at（UTC Unix 秒）、providers[]。每个 provider 包含稳定 id、显示 name、read_at、windows[]；每个 window 包含稳定 id（如果源无 ID，按 provider 与窗口名称确定）、name、used_percent、remaining_percent、resets_at（UTC Unix 秒或 null）、unlimited 及明确有效性。缺失百分比保持 null，不能默认为零。多账户时 ID 必须能区分账户但不可向设备传明文邮箱。

只在数据有效且为百分数时用 100-remaining 补 used；两者冲突时记录无敏感信息的诊断并明确采用的来源，不悄悄混合。

设备侧数据模型由 Agent A 维护，Mac 编码及 docs/BLE_PROTOCOL.md 由 Agent B 维护。双方对照同一 fixtures 验证。

## BLE 约束

不能将任意长度 providers 数组直接塞入原有 250-byte v4 消息。Agent B 负责决定版本升级及拆包方案；建议保留基础统计消息，额外发送按供应商/窗口拆分的额度消息，并携带批次或序列号及完成标记。设备只在一批数据完整后原子更新列表，定义丢包、乱序、超时和上限。实际方案必须有字节预算测试，提供固件可解析的编码 fixture。

不要先验强制某种 JSON 键或硬编码最大供应商数量，依据实测 MTU、内存和协议预算确定并记录。如限制条目数，必须定义截断及 UI 提示，不静默丢弃。

## 依赖顺序

1. Agent B 核对 API，提交 wire contract、样例和边界规则。
2. Agent A 先做板级 bring-up 与 UI fixture，再实现冻结协议的解析。
3. Agent B 为集成人，按顺序合并两个短期分支，进行 Companion 与设备端契约检查。
4. 集成构建和只读 API 验证后，单独准备用户确认的硬件刷写及恢复步骤。

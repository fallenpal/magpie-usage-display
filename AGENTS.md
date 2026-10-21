## 项目协作入口

本地 main 从 upstream 固定基线开始；目前没有个人 origin，也未创建 GitHub Fork。当前任务是 1.75C 圆屏适配，需求入口 docs/round-display/README.md。

开发先读 ARCHITECTURE.md 和 docs/round-display/REQUIREMENTS.md、INTERFACE-CONTRACT.md。使用两个独立 worktree，禁止两个 Agent 在同一 checkout 切分支、暂存或覆盖文件。Agent A 负责固件，Agent B 负责 Companion、协议文档并串行集成，具体交接见各自文档。

验证：Companion 使用 python3 -m unittest discover -s companion/tests -v；固件使用 `.pio-venv/bin/pio test -e native` 和 `.pio-venv/bin/pio run -e esp32-s3-touch-amoled-175c`。不得将旧 2.16 方屏构建成功当成 1.75C 验证成功，当前结果见 docs/round-display/VERIFICATION.md。

部署目标：Mac Companion + 用户微雪 ESP32-S3-Touch-AMOLED-1.75C。固件烧录或后台服务安装需要明确用户授权和恢复方案。默认本轮只准备交接，不刷写。不得保存 API Key、账户邮箱或请求内容到仓库。

Markdown 不使用一级标题；文档引用采用 wikilink；不使用破折号。实现完更新架构、协议和验证记录，不声称未实际运行的测试已通过。

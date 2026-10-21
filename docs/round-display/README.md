## 圆屏适配开发包

目标：将 peterroe/magpie-usage-display 适配微雪 ESP32-S3-Touch-AMOLED-1.75C，第二页改为多供应商 Usage，支持重置倒计时。

资料整理日期：2026-10-22。当前只有源码基线、需求及效果图，尚未实现、烧录或创建 GitHub Fork。

- 上游：https://github.com/peterroe/magpie-usage-display
- 固定基线：5940d479afeb89affc04d13c01f3f644b3f28cc7
- 本地仓库：/Users/bruce/dev/magpie-usage-display-round
- Git remote：upstream 指向原作者；个人 origin 尚未设置。
- 最新设计：[[assets/concept-v2-usage-providers.png]]
- 历史设计：[[assets/concept-v1-requests.png]]，第二页 Requests 已被替代。
- 阅读顺序：[[REQUIREMENTS]] → [[DATA-EVIDENCE]] → [[INTERFACE-CONTRACT]] → 对应 Agent 交接说明。
- Agent A：[[AGENT-A-FIRMWARE]]；Agent B：[[AGENT-B-COMPANION]]。

图片由内置 imagegen 生成，是排版概念，不是运行截图。圆弧比例、色条比例、精确字号以实现时真实数据和 466×466 像素验证为准。图中其他页面、Claude 和倒计时均为示例；Codex 94% 是历史读取值，不能当实时状态。

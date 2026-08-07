# 全量一致性核查（Verification）

> **定位**：按需执行的全量核查清单——验证「文档 ↔ 代码 ↔ 产物 ↔ 实际行为」四者一致。
>
> **不绑定使用时机**：开发期按各归属文档做增量验证（改什么查什么）；仅当**用户明确要求全量核查**时，按本文档逐项执行。递增版本号、构建、发布均**不**强制触发本清单。
>
> 权威文档引用：环境与命令 → `docs/environment.md` §3；API 规格与数据模型 → `docs/api-spec.md`；操作端点 → `docs/player-operations.md`；符号/VMA → `docs/data-sources.md`；真机工作流 → `docs/deployment/phone-dev-workflow.md`。

**执行前置**：项目根目录 `/home/chenxi-zqs/Code/opencode-workspace/projects/android-game-api-export`；Python 一律 `uv run`（禁止裸 python / 系统 pip）；核查产物只落项目内 `output/`、`.tmp/`。所有命令在项目根目录执行，构建例外（见 A）。

---

## 0. 核查模型（四维一致性）

```
文档 docs ──(1)──→ 产物 artifacts（APK / static-data / libgame.so）
   ↕(2)                    ↕(3,4)
代码 code ──运行──→ 实际行为（真机 runtime）
```

| 维度 | 验证内容 | 对应章节 |
|---|---|---|
| 1 文档↔产物 | 文档声称的版本元数据 / VMA 偏移 / 数据数量 与 实际产物一致 | A / B / C / D |
| 2 文档↔代码 | 文档声称的端点清单 / 数据模型 / 能力声明 与 代码实现一致 | E |
| 3 代码↔行为 | 端点行为在真机上真的成立 | F |
| 4 运行健康 | 进程无崩溃、日志无 ERROR、JNI 无异常 | F |

---

## A. 构建产物验收（维度 1）

构建命令见 `docs/environment.md` §3（此处不重复）。构建完成后核对版本元数据：

**版本元数据核对**（与 `build.gradle.kts` 一致）：

| 项 | 当前值（v0.3.1） |
|---|---|
| versionCode | 36 |
| versionName | 0.3.1 |
| compileSdk / targetSdk / minSdk | 34 / 34 / 30 |
| abiFilters | arm64-v8a + armeabi-v7a |
| NDK（`ndkVersion`） | 26.3.11579264（= r26d，source.properties 确认） |

**APK 体积参考**：模块 APK ≈ 5.7MB（含内嵌静态数据子集）；LSPatch 集成版 ≈ 53MB。

---

## B. 符号校验（维度 1，换游戏版本后必核）

命令见 `docs/environment.md` §3（`check_symbols.py`，比对 39 个符号）。脚本比对 `libgame.so` 符号表与 `module/app/src/main/cpp/game_symbols.h` 中 `*_VMA` 常量。

**通过标准**：所有符号输出 `✅ 一致`，无 `⚠️ 需更新`。

**失败处理**：`⚠️` 行出现 → 用输出中的新地址更新 `game_symbols.h` 对应 `_VMA` 常量 → 重新构建 → 重跑本项。

**符号总量参考**（README/静态分析结论）：`libgame-symbols.txt` 8,297 行；FUNC 6,626（GLOBAL 6,544）、OBJECT 1,664 → 文档中的「6,626 个导出函数」以此为准。

---

## C. 静态数据完整性（维度 1，全量版）

完整数据位于 `static-data/json/`（tables + text）。核查命令：

```bash
uv run python -c "
import json, pathlib
root = pathlib.Path('static-data/json')
s = json.load(open(root/'tables/_summary.json'))
assert len(s) == 100, f'tables={len(s)}'
assert sum(e['record_count'] for e in s) == 14396, 'records != 14396'
langs = sorted(p.stem for p in (root/'text').glob('*.json') if p.stem not in ('_summary','formula-e'))
assert langs == ['de','en','fr','ja','zh-Hans','zh-Hant'], langs
for l in langs:
    d = json.load(open(root/'text'/f'{l}.json'))
    assert len(d) == (35812 if l=='en' else 35811), f'{l}: {len(d)}'
assert len(json.load(open(root/'text/formula-e.json'))) == 1991
print('OK: 100 tables / 14396 records / 6 langs / formula-e 1991')
"
```

**通过标准**（断言全过）：
- `tables/_summary.json` 为 100 项 list，`record_count` 总和 = 14,396
- 6 种语言：zh-Hans / zh-Hant / ja / en / de / fr（en 35,812 条，其余各 35,811 条）
- `formula-e.json` = 1,991 条

> 数据**生成**流程（game_res 解析 → JSON）见 `docs/reference/static-data.md` §5 工具链；本项只核查生成**产物**与文档声明一致。

---

## D. 模块内嵌子集（维度 1，打包进 APK 的部分）

子集位于 `module/app/src/main/assets/static-data/`（约 13MB），由 `scripts/parse/package_assets.py` 生成（生成流程见 `docs/reference/static-data.md` §5）。核查命令：

```bash
uv run python -c "
import json, pathlib
ad = pathlib.Path('module/app/src/main/assets/static-data')
assert len(list((ad/'tables').glob('*.json'))) == 28
assert sorted(p.stem for p in (ad/'text').glob('*.json')) == ['en','zh-Hans']
m = json.load(open(ad/'manifest.json'))
assert m['tables'] and m['text_langs'] == ['zh-Hans','en']
print('OK: 28 tables / 2 langs / manifest 有效')
"
```

**通过标准**：28 张内嵌表（清单见 `package_assets.py` 的 `INCLUDE_TABLES`）+ 2 种语言（zh-Hans、en）+ `manifest.json` 存在且 `text_langs` 字段正确。

> 全量 100 表只在 `static-data/json/`，模块内仅 28 表子集——`/api/data/tables/{name}` 只对 28 表有效，其余返回 404（见 `docs/api-spec.md` §4 静态端点表）。

---

## E. 端点与数据模型（维度 2，静态比对）

**端点总数 = 32**（/api/info 11 GET + /api/action 10 POST + /api/data 11 GET）。完整清单：

**GET /api/info/**（`InfoController.kt`，11 个）

| 端点 | 说明 |
|---|---|
| `/api/info/player` | 主角色状态（money/mapId/hp/mp 等） |
| `/api/info/player/party` | 队伍成员列表 |
| `/api/info/inventory` | 背包 bags+slots |
| `/api/info/map` | 地图信息 |
| `/api/info/quest` | 返回 `{"activeQuest": n}` |
| `/api/info/units` | 场景单位 |
| `/api/info/ui` | 界面状态 |
| `/api/info/player/skills` | 已学技能 |
| `/api/info/player/mercenaries` | 佣兵列表 |
| `/api/info/path?tx=&ty=` | 寻路路径（目标坐标） |
| `/api/info/events` | 事件流（首次调用建立基线返回空，之后返回差异事件） |

**POST /api/action/**（`PlayerController.kt`，10 个）——成功响应后 attach 最新 state

| 端点 | 说明 |
|---|---|
| `/api/action/player/move` | 移动 |
| `/api/action/player/use-item` | 使用道具 |
| `/api/action/player/{role}/equip` | 穿装备（支持 `bag+slot` 或 `category` 自动找槽） |
| `/api/action/player/{role}/unequip` | 卸装备 |
| `/api/action/player/{role}/auto-attack` | 自动攻击开关 |
| `/api/action/player/{role}/skill` | 施放技能 |
| `/api/action/player/switch` | 切换主控角色 |
| `/api/action/inventory/discard` | 丢弃物品 |
| `/api/action/party/include` | 佣兵入队 |
| `/api/action/party/exclude` | 佣兵离队 |

**GET /api/data/**（`DataController.kt`，11 个）——静态数据

| 端点 | 数据源（内嵌子集） |
|---|---|
| `/api/data/roles` | CHARCLASSBASE 单表 |
| `/api/data/items` | ITEMDATABASE |
| `/api/data/skills` | SKILLDESCBASE |
| `/api/data/mercenaries` | MERCENARYINFOBASE |
| `/api/data/maps` | MAPINFOBASE |
| `/api/data/monsters` | MONDATABASE |
| `/api/data/quests` | QUESTINFOBASE |
| `/api/data/npcs` | NPCINFOBASE |
| `/api/data/events` | reverse/events.json |
| `/api/data/text?lang=` | text/<lang>（仅 zh-Hans/en 可用） |
| `/api/data/tables/{name}` | 仅内嵌 28 表 |

**比对方法**：

```bash
# 代码侧实际端点
grep -rE '@(Get|Post)Mapping' module/app/src/main/java/com/inotia4/export/controller/
# 文档侧
grep -E '/api/(info|action|data)' docs/api-spec.md
```

**通过标准**：代码侧 32 条注解与 `docs/api-spec.md` 端点表逐条一致（路径、方法、数量），且全部端点均已实现（无 TODO/stub 端点）。注意 `/api/info/events` 与 `/api/data/events` 重名不同前缀，均为合法端点。

**字段级比对（可选深度核查）**：`docs/api-spec.md` §3 数据模型（Player/Role/Inventory/Skills 等）声明的字段 ↔ controller 实际响应 JSON 字段。抽查 2-3 个模型即可（完整比对成本高，按需执行）。

> **不暴露的 native 能力**：`NativeBridge.kt` 含 OP 类 JNI（SetMoney/Teleport/SellItem 等），仅保留 native 函数、**无 HTTP 端点**——/api/op/* 为未来规划（见 `docs/control-capability.md` §4）。核查时确认 controller 中**不存在** `/api/op/` 的 `@GetMapping/@PostMapping` 注解（`PlayerController.kt` 类注释提及 /api/op/ 属正常，仅注释不算端点）。

---

## F. 真机行为与运行健康（维度 3 + 4）

环境：oneplus-13（`docs/deployment/phone-dev-workflow.md`），手机 adb 连接 + Tailscale/局域网互通，模块经 LSPatch 注入。部署与连接步骤见 `docs/deployment/phone-dev-workflow.md` §3-4（此处不重复）。

**F1. API 连通与采样**（命令见 `docs/deployment/phone-dev-workflow.md` §4 与 `docs/environment.md` §3）：

```bash
# 连续轮询 /api/info/player + party + inventory，检测字段变化
uv run python scripts/analyze/api_poll.py <手机IP> [间隔秒] [次数]

# 全自动联调会话（等 API 就绪→等世界就绪→连续采样→输出报告，log/live-test/）
uv run python scripts/analyze/live_session.py [手机IP] [时长上限分钟]
```

**通过标准**：采样无连接失败；`money`/`mapId` 等字段随游戏内操作变化。

**F2. 操作端点**：进游戏世界后逐项实测（v0.3.2-0.3.6 修复后行为）：

| 操作 | 验证动作 | 预期 |
|---|---|---|
| `player/move` | 移动角色到可达点 | 响应 `{"ok":true,...}`，随后 player 状态坐标变化（v0.3.2 修复：SearchPath+循环 MoveAsPath，目标须可达否则 `no path`） |
| `player/use-item` | 使用消耗品（药水） | 背包物品数减少；**不可消耗品返回 `item not usable`**（v0.3.2 修复：ITEMDATABASE IsUse 校验，装备/合成材料拒绝） |
| `player/{role}/equip` | 穿装备 | 角色 attrs/equip 变化；**目标槽被占用时自动替换**（v0.3.3 修复：先卸后穿） |
| `player/{role}/unequip` | 卸装备 | 装备槽清空（背包出现物品） |
| `player/{role}/skill` | 施放技能 | 目标受击（UI/units 可见） |
| `player/switch` | 切换主控 | party 主控标记变化（v0.3.2 修复路由注册） |
| `inventory/discard` | 丢弃物品 | 背包数量减少且返回 `ok:true`（v0.3.2 修复：按槽位清空判定，而非函数返回值） |
| `party/include` | 佣兵入队 | 队伍列表变化；**已在队返回 `already in party`、满员返回 `party full`**（v0.3.6 校验顺序） |
| `party/exclude` | 佣兵离队 | 队伍列表变化；**主控返回 `cannot exclude leader`、任务 NPC 返回 `cannot exclude quest npc`**（v0.3.5 修复：前置校验拦截非法操作，避免游戏内乱码弹窗） |

**F3. 事件流**：进世界后调用 `/api/info/events` 两次——首次返回空基线，之后执行任意操作（移动/战斗/拾取）再拉取，应出现对应事件（money/hp/mp/exp/level_up/move/inventory）。

**F4. 寻路**：`GET /api/info/path?tx=<x>&ty=<y>` 返回路径点数组；路径合法（相邻点可达、终点接近目标）。

**F5. 运行健康审查**：

```bash
# 模块日志：应无 ERROR / 异常堆栈 / JNI 错误
adb logcat -d | grep -iE 'inotia4|AndroidRuntime' | grep -iE 'error|exception|fatal' || echo '无错误日志'

# 进程稳定性：采样期间无进程重启/崩溃（live_session 报告应无断连）
# 事件流：F3 采样期间无重复/乱序事件（可选，与 F3 合并执行）
```

**通过标准**：日志无 ERROR/崩溃堆栈；JNI 无 `UnsatisfiedLinkError`/`JNI DETECTED ERROR`；采样全程进程稳定。

---

## 核查通过汇总表

| # | 核查项 | 命令/位置 | 通过标准 |
|---|---|---|---|
| A | 构建产物验收 | `docs/environment.md` §3 构建命令 | 版本元数据与 build.gradle.kts 一致 |
| B | 符号校验 | `uv run python scripts/analyze/check_symbols.py` | 全部 `✅ 一致` |
| C | 静态数据全量 | C 节 uv run 断言 | 100 表 / 14,396 条 / 6 语言 / formula-e 1,991 |
| D | 模块内嵌子集 | D 节 uv run 断言 | 28 表 + 2 语言 + manifest 有效 |
| E | 端点与数据模型 | grep 比对 controller vs api-spec | 32 端点一致，无 /api/op/ |
| F | 真机行为 + 运行健康 | api_poll / live_session + F2-F5 | 采样稳定、操作生效、事件流/寻路正常、日志无 ERROR |

---

## 关联文档

- 环境与命令：`docs/environment.md` §3（构建/符号/联调命令）
- API 规格与数据模型：`docs/api-spec.md`
- 操作分级与实现状态：`docs/player-operations.md`
- 符号与 VMA：`docs/data-sources.md`
- 静态数据生成流程：`docs/reference/static-data.md` §5
- 真机工作流：`docs/deployment/phone-dev-workflow.md` §3-4
- 目录规范与提交纪律：`README.md`「目录规范与环境隔离」

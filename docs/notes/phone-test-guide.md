# 真机测试指南（安装 + 日志采集）

> **⚠️ 已归档（2026-08-05）**：本文基于 v0.2.1 首版（dlopen 时代、units 未逆向），已被
> `docs/notes/phone-dev-workflow.md`（日常开发工作流：构建/部署/重启/自动进世界/curl 验证）完全替代。
> 保留仅作历史留档，不再使用其中的命令与限制描述。

> 日期：2026-08-05 ｜ 适用：`output/inotia4-export-module-v0.2.1.apk`（M4 首版含日志）
> 目标：无 adb 环境下运行游戏，采集模块运行日志并提交，用于验证 dlopen 时机 / 符号解析 / 结构体偏移。

## 1. 前置条件

- 已 root + Zygisk-LSPosed（Android 11+）
- 已安装游戏：`com.com2us.inotia4.normal.freefull.google.global.android.common`
- 模块 APK：`output/inotia4-export-module-v0.2.1.apk`（debug 签名，直接可装）

## 2. 安装步骤

1. **安装模块 APK**（普通安装即可，无需签名校验）
2. **LSPosed 启用**：打开 LSPosed Manager → 模块 → 勾选「Inotia4 Export」→ 重启或按提示激活
3. **配置作用域**：进入模块详情 → 作用域 → 勾选游戏包（`com.com2us.inotia4...`）→ 保存
4. **完全重启** LSPosed（确保模块生效）

## 3. 运行与采集日志

1. 启动游戏，进入主界面后等待 10 秒（让模块完成初始化）
2. 无 adb 时用**文件管理器**打开日志文件：
   ```
   /sdcard/Android/data/com.com2us.inotia4.normal.freefull.google.global.android.common/files/inotia4-export.log
   ```
   > 若文件管理器看不到 Android/data（Android 11+ 限制），可尝试：
   > - 用自带「文件」App 的「内部存储」直接导航（部分机型可访问）
   > - 或通过 LSPosed 模块通知 / MT 管理器 / 或临时接 adb 拉取
3. 若日志不存在：确认步骤 2 是否完成（模块未注入进程时不会产生日志）

## 4. 提交内容

把 `inotia4-export.log` 内容提交即可。日志将包含：
- 模块注入进程确认
- `System.loadLibrary(game)` 是否被拦截（dlopen 时机）
- 每个 dlsym 符号解析结果（JSON 报告：`dlopen`/`INVEN_nMoney`/`PARTY_GetMember`/... 的 true/false）
- dlopen 失败原因（dlerror）
- ApiServer 启动结果

## 5. 已知限制（首版 v0.2.1）

- 技能列表：`CHARSYSTEM_GetSkillList` 为 stub，v1 未实现运行时技能（API 返回职业静态可学技能）
- `/api/units`：单位坐标字段偏移未逆向，v1 返回空数组
- 日志文件会持续追加，多次运行后可能较大；提交前可清空重跑一次

## 6. 预期成功标志（供对照）

日志末尾出现：
```
[NativeBridge init OK, symbols: {"dlopen":true,"INVEN_nMoney":true,...,"UTIL_GetBitValue":true}]
[ApiServer start requested]
```
则模块完整工作；局域网内即可访问 `http://<手机IP>:8088/api/player`。

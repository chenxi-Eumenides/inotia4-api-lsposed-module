#!/usr/bin/env python3
"""UI 面板数据采集脚本：每 0.1 秒轮询 /api/debug/ui，记录原始数据到 JSONL 文件。

用于采集不同游戏界面（背包/任务/技能/商店/合成等）下的 UI 原始数据，
辅助逆向分析面板检测数据结构。

用法：
  uv run python scripts/analyze/monitor_ui.py [手机IP] [输出文件]

采集过程中：
  - 脚本持续轮询 /api/debug/ui，写入 JSONL 文件
  - 用户在手机上依次打开各个 UI 面板
  - 每次打开面板前等待 1-2 秒让脚本采集该状态的稳定数据
  - Ctrl+C 停止采集
"""

from __future__ import annotations

import json
import sys
import time
import urllib.request
from pathlib import Path

DEFAULT_IP = "100.110.139.83"
DEFAULT_OUT = Path(__file__).resolve().parents[2] / ".tmp" / "ui_monitor.jsonl"
INTERVAL = 0.1  # 轮询间隔（秒）


def poll(ip: str) -> dict | None:
    url = f"http://{ip}:8088/api/debug/ui"
    try:
        req = urllib.request.Request(url)
        req.add_header("Connection", "close")
        with urllib.request.urlopen(req, timeout=3) as resp:
            return json.loads(resp.read())
    except Exception:
        return None


def main() -> None:
    ip = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_IP
    out_path = Path(sys.argv[2]) if len(sys.argv) > 2 else DEFAULT_OUT
    out_path.parent.mkdir(parents=True, exist_ok=True)

    print(f"监控 {ip}:8088/api/debug/ui → {out_path}")
    print(f"轮询间隔 {INTERVAL}s，Ctrl+C 停止\n")
    print("请在手机上依次操作：")
    print("  1. 游戏世界（无面板打开）→ 等待稳定")
    print("  2. 打开背包/装备界面 → 等待")
    print("  3. 打开任务界面 → 等待")
    print("  4. 打开技能界面 → 等待")
    print("  5. 打开商店界面 → 等待")
    print("  6. 打开合成界面 → 等待")
    print("  7. 打开佣兵界面 → 等待")
    print("  8. 打开设置界面 → 等待")
    print("  9. NPC 对话 → 等待")
    print(" 10. 弹窗/确认框 → 等待")
    print()

    count = 0
    last_data = None

    try:
        with open(out_path, "a") as f:
            while True:
                data = poll(ip)
                if data is None:
                    time.sleep(INTERVAL)
                    continue

                ts = time.time()
                line = json.dumps({"ts": ts, **data}, ensure_ascii=False)

                # 只在数据变化时打印
                if data != last_data:
                    screen = data.get("state", "?")
                    popup_on = data.get("popupOn", "?")
                    stack_hex = data.get("popupStackHex", "")[:16]
                    print(f"[{count:>6d}] state={screen} popup={popup_on} stack={stack_hex}...")
                    last_data = dict(data)

                f.write(line + "\n")
                f.flush()
                count += 1
                time.sleep(INTERVAL)
    except KeyboardInterrupt:
        print(f"\n\n停止。共采集 {count} 条记录 → {out_path}")


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""frida spawn 运行器：以 spawn 模式重启游戏加载 JS 探针，send 消息实时写入输出文件。

用法：
  uv run python scripts/analyze/frida_probe_spawn.py <js脚本> <输出文件> [超时秒]

说明：
  - spawn 模式（重启游戏进程）注入，Interceptor hook 可用（attach 模式 hook 不触发，见 sysmenu-exploration.md §3）
  - 游戏启动后需自行操作进入对应界面；探针 send 消息实时追加到输出文件
"""

from __future__ import annotations

import sys
import time
from pathlib import Path

import frida

PROJECT_ROOT = Path(__file__).resolve().parents[2]
PKG = "com.com2us.inotia4.normal.freefull.google.global.android.common"


def main() -> None:
    if len(sys.argv) < 3:
        print("用法: uv run python scripts/analyze/frida_probe_spawn.py <js脚本> <输出文件> [超时秒]")
        sys.exit(1)

    js_path = Path(sys.argv[1])
    if not js_path.is_absolute():
        js_path = PROJECT_ROOT / js_path
    out_path = Path(sys.argv[2])
    if not out_path.is_absolute():
        out_path = PROJECT_ROOT / out_path
    timeout = float(sys.argv[3]) if len(sys.argv) > 3 else 300.0

    device = frida.get_usb_device(timeout=10)
    print(f"spawn: {PKG}", flush=True)
    pid = device.spawn([PKG])
    session = device.attach(pid)
    device.resume(pid)
    print("游戏已启动，等待 libgame.so 加载（15s）", flush=True)
    time.sleep(15)

    out_path.parent.mkdir(parents=True, exist_ok=True)
    start = time.time()

    def on_message(message: dict, data: bytes | None) -> None:
        if message.get("type") == "send":
            line = str(message.get("payload", ""))
            print(f"[{time.time() - start:6.1f}s] {line}", flush=True)
            with open(out_path, "a") as f:
                f.write(f"[{time.time() - start:6.1f}s] {line}\n")
        elif message.get("type") == "error":
            line = f"ERROR: {message.get('description')}"
            print(line, flush=True)
            with open(out_path, "a") as f:
                f.write(f"[{time.time() - start:6.1f}s] {line}\n")

    script = session.create_script(js_path.read_text())
    script.on("message", on_message)
    script.load()
    print("探针已加载（Ctrl+C 退出）", flush=True)

    try:
        while time.time() - start < timeout:
            time.sleep(0.5)
    except KeyboardInterrupt:
        pass
    finally:
        script.unload()
        session.detach()
        print(f"\n已退出，输出文件: {out_path}", flush=True)


if __name__ == "__main__":
    main()

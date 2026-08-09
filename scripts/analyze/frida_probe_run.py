#!/usr/bin/env python3
"""frida 探针运行器：attach 游戏进程加载 JS 探针，将 send 消息实时写入输出文件。

用法：
  uv run python scripts/analyze/frida_probe_run.py <js脚本> <输出文件> [超时秒]

说明：
  - 以 attach 模式加载探针（纯内存读，不注入 hook，避免 WebView 崩溃）
  - 探针的 send() 消息实时追加到输出文件并打印到 stdout
  - 超时后自动退出；Ctrl+C 提前退出
"""

from __future__ import annotations

import sys
import time
from pathlib import Path

import frida

PROJECT_ROOT = Path(__file__).resolve().parents[2]


def main() -> None:
    if len(sys.argv) < 3:
        print("用法: uv run python scripts/analyze/frida_probe_run.py <js脚本> <输出文件> [超时秒]")
        sys.exit(1)

    js_path = Path(sys.argv[1])
    if not js_path.is_absolute():
        js_path = PROJECT_ROOT / js_path
    out_path = Path(sys.argv[2])
    if not out_path.is_absolute():
        out_path = PROJECT_ROOT / out_path
    timeout = float(sys.argv[3]) if len(sys.argv) > 3 else 300.0

    # 按显示名查找游戏进程（pid 每次重启变化）
    device = frida.get_usb_device(timeout=10)
    target = None
    for proc in device.enumerate_processes():
        if "inotia4" in proc.name.lower() or "inotia" in proc.name.lower():
            target = proc
            break
    if target is None:
        print("ERROR: 未找到游戏进程（inotia4）")
        sys.exit(1)

    print(f"attach 进程: {target.name} (pid={target.pid})", flush=True)
    session = device.attach(target.pid)

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
    print("探针已加载，开始记录（Ctrl+C 退出）", flush=True)

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

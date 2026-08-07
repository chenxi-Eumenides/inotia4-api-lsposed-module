#!/usr/bin/env python3
"""frida 宿主：保持连接运行 /tmp/opencode/thread_probe.js，打印 console.log。

用法: uv run python scripts/analyze/run_probe.py [pid] [时长秒] [输出文件]
"""

from __future__ import annotations

import sys
import time
from pathlib import Path

import frida

PID = int(sys.argv[1]) if len(sys.argv) > 1 else 17137
DURATION = int(sys.argv[2]) if len(sys.argv) > 2 else 420
OUT = Path(sys.argv[3]) if len(sys.argv) > 3 else Path(".tmp/ui_probe.log")
JS = Path(__file__).resolve().parent / "/tmp/opencode/thread_probe.js"

OUT.parent.mkdir(parents=True, exist_ok=True)
out = open(OUT, "w")

dev = frida.get_usb_device(timeout=10)
session = dev.attach(PID)
script = session.create_script(JS.read_text())

def on_msg(msg, data):
    t = msg.get("type")
    if t == "send":
        out.write(msg["payload"] + "\n")
    elif t == "log":
        out.write(msg.get("payload", "") + "\n")
    elif t == "error":
        out.write("JS-ERROR: " + msg.get("stack", str(msg)) + "\n")
    out.flush()

script.on("message", on_msg)
script.load()
out.write(f"attached pid={PID} base ok, duration={DURATION}s\n")
out.flush()
print(f"attached pid={PID}, logging to {OUT}, {DURATION}s...")

try:
    time.sleep(DURATION)
except KeyboardInterrupt:
    pass
finally:
    script.unload()
    session.detach()
    out.close()
    print("done")

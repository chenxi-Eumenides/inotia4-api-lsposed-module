#!/usr/bin/env python3
"""剧情对话全流程采集 v2：JS setInterval 推送状态，Python 接收 + HTTP API 推进。

用法：uv run python scripts/analyze/evt_story_walk.py <PID> [推进间隔秒]
输出：每次状态变化打印一行。
推进方式：POST /api/action/dialog/select {"action":"skip"}（真机2 完全 API 操控，不再用触摸坐标）。
"""
from __future__ import annotations

import json
import sys
import time
import urllib.request

import frida

BASE = "http://192.168.3.54:8088"

JS = r"""
var base = null;
Process.enumerateModules().forEach(function (m) {
    if (m.path.indexOf("libgame.so") >= 0 && base === null) base = m.base;
});
if (base === null) { send({ error: "libgame not found" }); }

function u32(p) { try { return p.readU32(); } catch (e) { return -1; } }
function u16(p) { try { return p.readU16(); } catch (e) { return -1; } }
function u8(p) { try { return p.readU8(); } catch (e) { return -1; } }
function ptr(p) { try { return p.readPointer(); } catch (e) { return null; } }

function snap() {
    var o = {
        st: u16(base.add(0x307492)),
        gst: u32(base.add(0x72b068)),
        popup: u8(base.add(0x3070e8)),
        evtNState: u32(base.add(0x713034)),
        evtNIndex: u32(base.add(0x713018)),
        pText: String(ptr(base.add(0x3075d0)))
    };
    var pt = ptr(base.add(0x3075d0));
    if (pt !== null && !pt.isNull()) {
        try { o.text = pt.readUtf8String(160); } catch (e) { o.text = "<bad>"; }
    }
    return JSON.stringify(o);
}

setInterval(function () { send(snap()); }, 1000);
send({ ready: true });
"""


def main():
    pid = int(sys.argv[1])
    interval = float(sys.argv[2]) if len(sys.argv) > 2 else 3.0
    session = frida.get_usb_device(timeout=10).attach(pid)
    script = session.create_script(JS)
    script.load()

    last = None
    seq = []
    start = time.time()
    tap_count = 0
    last_tap = 0.0

    def on_message(msg, data):
        nonlocal last, seq, tap_count, last_tap
        if msg.get("type") != "send":
            return
        payload = msg.get("payload")
        if isinstance(payload, dict) and payload.get("ready"):
            print("探针就绪", flush=True)
            return
        try:
            cur = json.loads(payload)
        except Exception:
            return
        key = (cur.get("gst"), cur.get("evtNState"), cur.get("pText"))
        if key != last:
            print(
                f"[{time.time() - start:.0f}s] st={cur.get('st')} gst={cur.get('gst')} popup={cur.get('popup')}"
                f" evtNState={cur.get('evtNState')} evtNIndex={cur.get('evtNIndex')}"
                f" pText={cur.get('pText')} text={cur.get('text')!r}",
                flush=True,
            )
            last = key
            seq.append(cur)

    script.on("message", on_message)
    while time.time() - start < 600:
        if time.time() - last_tap >= interval:
            try:
                req = urllib.request.Request(
                    BASE + "/api/action/dialog/select",
                    data=b'{"action":"skip"}',
                    headers={"Content-Type": "application/json"},
                    method="POST",
                )
                urllib.request.urlopen(req, timeout=5).read()
            except Exception:
                pass
            tap_count += 1
            last_tap = time.time()
        if last and last[0] == 0 and last[1] == 0:
            time.sleep(2)
            break
        time.sleep(0.5)

    session.detach()
    print(f"=== 点击 {tap_count} 次，共采集 {len(seq)} 个状态 ===", flush=True)


if __name__ == "__main__":
    main()

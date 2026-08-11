#!/usr/bin/env python3
"""探针 v13：hook CHAR_Move(0xe9808) 记录 direction 参数序列，验证 face-target 转向。
用法：uv run python .tmp/hook_facedir.py <PID>
"""
from __future__ import annotations
import sys, time
import frida

PID = sys.argv[1] if len(sys.argv) > 1 else "30424"

JS = r"""
var base = null;
Process.enumerateModules().forEach(function (m) {
    if (m.path.indexOf("libgame.so") >= 0 && base === null) base = m.base;
});
if (base === null) { send("ERROR: libgame.so not found"); }

var CHAR_MOVE = 0xe9808;
var last = [];
Interceptor.attach(base.add(CHAR_MOVE), {
    onEnter: function (args) {
        var dir = args[1].toInt32();
        var px = -1, py = -1;
        try { px = args[0].add(0x2).readS16(); py = args[0].add(0x4).readS16(); } catch (e) {}
        last.push("dir=" + dir + " @(" + px + "," + py + ")");
        if (last.length > 20) last.shift();
        send("[CHAR_Move] dir=" + dir + " pos=(" + px + "," + py + ")");
    }
});
send("hooked CHAR_Move");
"""

s = frida.get_device_manager().add_remote_device("127.0.0.1:27042")
session = s.attach(int(PID))
script = session.create_script(JS)
script.on("message", lambda msg, data: print(msg.get("payload") if msg.get("type") == "send" else msg))
script.load()
print("hooked, waiting for move...")
try:
    time.sleep(120)
except KeyboardInterrupt:
    pass

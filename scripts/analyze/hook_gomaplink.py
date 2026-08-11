#!/usr/bin/env python3
# ⚠️ DEPRECATED: 本脚本使用旧 VMA 0x713878（v0.4.28 前误用的地图 ID 源，实为瓦片矩阵起点）。
# 真实地图 ID 已改为 GOT 双层解引用 *(*(base+0x2f4000+0xe80))（= MAPINFOBASE 记录下标）。
# 本脚本保留仅作历史参考。
"""探针：hook GoMapLinkByChar(0x9cdc0) + 读玩家 tile bit7，验证 2056 出口为何不切图。
用法：uv run python .tmp/hook_gomaplink.py
"""
from __future__ import annotations
import sys, time
import frida

PID = sys.argv[1] if len(sys.argv) > 1 else "26683"

JS = r"""
var base = null;
Process.enumerateModules().forEach(function (m) {
    if (m.path.indexOf("libgame.so") >= 0 && base === null) base = m.base;
});
if (base === null) { send("ERROR: libgame.so not found"); }

var GO_MAP_LINK = 0x9cdc0;
var C_POS_X = 0x2;  // char +0x02 x, +0x04 y (int16)
var C_POS_Y = 0x4;

Interceptor.attach(base.add(GO_MAP_LINK), {
    onEnter: function (args) {
        this.ch = args[0];
        this.tx = args[1].toInt32();
        this.ty = args[2].toInt32();
        var ch = this.ch;
        var px = 0, py = 0;
        try { px = ch.add(C_POS_X).readS16(); py = ch.add(C_POS_Y).readS16(); } catch (e) {}
        send("[GoMapLink] enter ch=" + ch + " tile=(" + this.tx + "," + this.ty + ") player=(" + px + "," + py + ")");
    },
    onLeave: function (retval) {
        send("[GoMapLink] leave ret=" + retval.toInt32());
    }
});

setInterval(function () {
    // 玩家当前 tile 与坐标
    try {
        // 玩家 = CHARSYSTEM 池第一个角色？用最简单的：hook 输出已经足够
        send("[tick] alive");
    } catch (e) { send("[tick] err " + e); }
}, 5000);
"""

s = frida.get_device_manager().add_remote_device("127.0.0.1:27042")
session = s.attach(int(PID))
script = session.create_script(JS)
script.on("message", lambda msg, data: print(msg.get("payload") if msg.get("type") == "send" else msg))
script.load()
print("hooked, waiting for moves... (ctrl-c to stop)")
try:
    time.sleep(300)
except KeyboardInterrupt:
    pass

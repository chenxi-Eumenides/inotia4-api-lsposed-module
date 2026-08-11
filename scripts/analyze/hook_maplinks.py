#!/usr/bin/env python3
"""探针 v12：枚举 MAPLINK 对象（*(0x2f3000+0x3b8)，步长 0x430，type@+0x311==2）。
输出：当前位置 tile + 目标地图 ID(bit0-9) + 落地坐标(+0x2/+0x3)。
用法：uv run python .tmp/hook_maplinks.py <PID>
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

var MAP_LINK_GOT = 0x2f3000 + 0x3b8;
var p = base.add(MAP_LINK_GOT).readPointer();
var arr = p.readPointer();
send("MAPLINK array: " + arr);
if (arr.isNull()) { throw 0; }
var end = arr.add(0x19000 + 0xe90);
var start = arr.sub(0x430);
var links = [];
for (var o = end; o.compare(start) > 0; o = o.sub(0x430)) {
    try {
        var valid = o.readU8();
        if (!valid) continue;
        var type = o.add(0x311).readU8();
        if (type !== 2) continue;
        var x = o.add(0x2).readU8();
        var y = o.add(0x3).readU8();
        var id_raw = o.add(0x4).readU16();
        var target = id_raw & 0x3ff;
        var dir = (id_raw >> 10) & 0x7;
        links.push("tile=(" + (o.add(0x2).readS16() >> 4) + "," + (o.add(0x4).readS16() >> 4) + ") -> map " + target + " land=(" + x + "," + y + ") dir=" + dir);
    } catch (e) {
        send("read err: " + e.message);
        break;
    }
}
send("total map links: " + links.length);
links.forEach(function (l) { send(l); });
"""

s = frida.get_device_manager().add_remote_device("127.0.0.1:27042")
session = s.attach(int(PID))
script = session.create_script(JS)
script.on("message", lambda msg, data: print(msg.get("payload") if msg.get("type") == "send" else msg))
script.load()
time.sleep(4)

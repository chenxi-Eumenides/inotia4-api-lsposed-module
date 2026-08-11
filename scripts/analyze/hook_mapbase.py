#!/usr/bin/env python3
# ⚠️ DEPRECATED: 本脚本使用旧 VMA 0x713878（v0.4.28 前误用的地图 ID 源，实为瓦片矩阵起点）。
# 真实地图 ID 已改为 GOT 双层解引用 *(*(base+0x2f4000+0xe80))（= MAPINFOBASE 记录下标）。
# 本脚本保留仅作历史参考。
"""探针 v13：dump MAP_nBaseInfo（4096B）找 mapId→text_id 映射。
用法：uv run python .tmp/hook_mapbase.py <PID>
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

var MAP_BASE = 0x713878;
var mb = base.add(MAP_BASE);
send("MAP_nBaseInfo: " + mb);

// 前 64 字节 u16 dump
var u16s = [];
for (var i = 0; i < 64; i += 2) {
    u16s.push(mb.add(i).readU16());
}
send("u16[0..31]: " + u16s.join(","));

// 常见偏移尝试：地图名 text_id 可能在 u16[2]/[4]/[6] 等
send("u16[0]=mapId: " + u16s[0]);
send("u16[1]: " + u16s[1]);
send("u16[2]: " + u16s[2]);
send("u16[3]: " + u16s[3]);

// 尺寸：宽度/高度（常见 u16 或 u32）
send("u16[4]: " + u16s[4] + " u16[5]: " + u16s[5]);
send("u16[6]: " + u16s[6] + " u16[7]: " + u16s[7]);

// 玩家坐标
var GET_MEMBER = new NativeFunction(base.add(0x11f384), 'pointer', ['int']);
var ch = GET_MEMBER(0);
var px = ch.isNull() ? -1 : ch.add(0x2).readS16();
var py = ch.isNull() ? -1 : ch.add(0x4).readS16();
send("player: (" + px + "," + py + ")");
"""

s = frida.get_device_manager().add_remote_device("127.0.0.1:27042")
session = s.attach(int(PID))
script = session.create_script(JS)
script.on("message", lambda msg, data: print(msg.get("payload") if msg.get("type") == "send" else msg))
script.load()
time.sleep(4)

# ⚠️ DEPRECATED: 本脚本使用旧 VMA 0x713878（v0.4.28 前误用的地图 ID 源，实为瓦片矩阵起点）。
# 真实地图 ID 已改为 GOT 双层解引用 *(*(base+0x2f4000+0xe80))（= MAPINFOBASE 记录下标）。
# 本脚本保留仅作历史参考。
import sys, time, frida
PID = sys.argv[1]
JS = r"""
var base = null;
Process.enumerateModules().forEach(function (m) {
    if (m.path.indexOf("libgame.so") >= 0 && base === null) base = m.base;
});
var addr = base.add(0x713878);
send("direct u16: " + addr.readU16());
send("direct u32: " + addr.readU32());
var ptr = addr.readPointer();
send("as pointer: " + ptr + " -> u16 " + (ptr.isNull() ? "null" : ptr.readU16()));
// 尝试 MAP_nBaseInfo 指针指向的结构：常见 GOT 模式 *(base+VMA)
// 读 u32 窗口（0x713878 前后）
var out = [];
for (var i = 0; i < 8; i++) out.push(base.add(0x713878 + i).readU8());
send("bytes at 0x713878: " + out.join(","));
"""
s = frida.get_device_manager().add_remote_device("127.0.0.1:27042")
session = s.attach(int(PID))
script = session.create_script(JS)
script.on("message", lambda m, d: print(m.get("payload") if m.get("type") == "send" else m))
script.load()
time.sleep(3)

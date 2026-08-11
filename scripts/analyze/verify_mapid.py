# ⚠️ DEPRECATED: 本脚本使用旧 VMA 0x713878（v0.4.28 前误用的地图 ID 源，实为瓦片矩阵起点）。
# 真实地图 ID 已改为 GOT 双层解引用 *(*(base+0x2f4000+0xe80))（= MAPINFOBASE 记录下标）。
# 本脚本保留仅作历史参考。
# 注：本脚本已改用新 GOT 地址验证，保留作校验参考。
import sys, time, frida
PID = sys.argv[1]
JS = r"""
var base = null;
Process.enumerateModules().forEach(function (m) {
    if (m.path.indexOf("libgame.so") >= 0 && base === null) base = m.base;
});
// *(0x2f4000+0xe80) 双层解引用 u32
var p1 = base.add(0x2f4000 + 0xe80).readPointer();
send("*(0x2f4000+0xe80) = " + p1 + " mapId=" + p1.readU32());
// *(0x2f4000+0xe60) 尺寸1
var p2 = base.add(0x2f4000 + 0xe60).readPointer();
send("*(0x2f4000+0xe60) = " + p2 + " val=" + p2.readU32());
// *(0x2f6000+0xd0) 尺寸2
var p3 = base.add(0x2f6000 + 0xd0).readPointer();
send("*(0x2f6000+0xd0) = " + p3 + " val=" + p3.readU32());
// SAVE_nMapID
send("SAVE_nMapID: " + base.add(0x729824).readU16());
// 瓦片矩阵假 mapId
send("tiles[0..1] (假mapId): " + base.add(0x713878).readU16());
"""
s = frida.get_device_manager().add_remote_device("127.0.0.1:27042")
session = s.attach(int(PID))
script = session.create_script(JS)
script.on("message", lambda m, d: print(m.get("payload") if m.get("type") == "send" else m))
script.load()
time.sleep(3)

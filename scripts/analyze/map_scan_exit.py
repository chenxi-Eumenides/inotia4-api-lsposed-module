#!/usr/bin/env python3
"""扫描地图瓦片网格找出口（bit7=1 的 tile）"""
import frida, sys, time

JS = r"""
var g_base = Process.findModuleByName("libgame.so").base;
var grid = g_base.add(0x2f3000 + 0xf48).readPointer();
send("grid base=" + grid);
// 地图大小：从 MAP_nBaseInfo 或通行矩阵找。CheckMapLink 用 y*64+x（sbfiz x2,#6 → ×64）
// 扫描足够大范围：64×64 tile
var exits = [];
var maxY = 64, maxX = 64;
// 先探测地图实际尺寸：找边界。走 128x128 保守
var limit = 64;
for (var y = 0; y < limit; y++) {
    for (var x = 0; x < limit; x++) {
        var idx = y * 64 + x;
        var b = grid.add(idx).readU8();
        if ((b & 0x80) != 0) {
            exits.push({x: x, y: y, v: b & 0x7f});
        }
    }
}
send("exit tiles: " + JSON.stringify(exits));
// 当前角色 tile
var ch = g_base.add(0x2f6000 + 0xa50).readPointer().readPointer();
send("ch=" + ch + " chX=" + ch.add(0x2).readS16() + " chY=" + ch.add(0x4).readS16());
"""

pid = int(sys.argv[1])
dev = frida.get_device_manager().add_remote_device("127.0.0.1:27042")
sess = dev.attach(pid)
script = sess.create_script(JS)
script.on("message", lambda m, d: print(m.get("payload") if m.get("type")=="send" else m))
script.load()
time.sleep(2)
sess.detach()

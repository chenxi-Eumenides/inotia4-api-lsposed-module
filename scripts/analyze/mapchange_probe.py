#!/usr/bin/env python3
"""测试 GAMEPLAY_GoMapLinkByChar(0x9cdc0) 官方切图入口：
1. 读当前角色坐标/方向
2. 调 GoMapLinkByChar(ch, tile_x, tile_y)
3. 观察是否触发 MAPCHANGE_Set（读 [0x2f5000+0xa60] 槽）
"""
import frida, sys, time, json, urllib.request

BASE = "http://192.168.3.54:8088"
JS = r"""
var g_base = Process.findModuleByName("libgame.so").base;
send("base=" + g_base);

function readSlot(addr) {
    try {
        var p = addr.readPointer();
        return p.isNull() ? null : p;
    } catch(e) { return null; }
}

// 当前角色 = [0x2f6000+0xa50] 解引用
var chSlot = readSlot(g_base.add(0x2f6000 + 0xa50));
send("ch=" + chSlot);
if (chSlot) {
    send("ch[+0x2](x)=" + chSlot.add(0x2).readS16() + " ch[+0x4](y)=" + chSlot.add(0x4).readS16() + " ch[+0x6](dir)=" + chSlot.add(0x6).readS8());
}

// MAPCHANGE_Set 目标槽 [0x2f5000+0xa60]
var mcSlot = readSlot(g_base.add(0x2f5000 + 0xa60));
send("mapchange slot=" + mcSlot);
if (mcSlot) {
    send("  mapId=" + mcSlot.add(0x0).readU16() + " x=" + mcSlot.add(0x2).readU16() + " y=" + mcSlot.add(0x4).readU16() + " dir=" + mcSlot.add(0x6).readU16());
}

// hook MAPCHANGE_Set
Interceptor.attach(g_base.add(0x9c740), {
    onEnter: function(args) {
        send("MAPCHANGE_Set(mapId=" + args[0].toInt32() + ", x=" + args[1].toInt32() + ", y=" + args[2].toInt32() + ", dir=" + args[3].toInt32() + ")");
    }
});

// hook GAMEPLAY_GoMapLink
Interceptor.attach(g_base.add(0x9cc74), {
    onEnter: function(args) {
        send("GoMapLink(" + args[0] + ")");
    }
});
"""

def api_get(path):
    try:
        return json.loads(urllib.request.urlopen(BASE + path, timeout=5).read())
    except Exception as e:
        return {"err": str(e)}

def main():
    pid = int(sys.argv[1])
    dev = frida.get_device_manager().add_remote_device("127.0.0.1:27042")
    sess = dev.attach(pid)
    script = sess.create_script(JS)
    script.on("message", lambda m, d: print(m.get("payload") if m.get("type")=="send" else m))
    script.load()
    time.sleep(1)
    # 用 API walk 移动一段触发
    print("=== API walk dir=0 ===")
    req = urllib.request.Request(BASE + "/api/action/movement/walk",
        data=b'{"direction":0}', headers={'Content-Type':'application/json'})
    print(urllib.request.urlopen(req, timeout=5).read()[:100])
    time.sleep(2)
    sess.detach()

main()

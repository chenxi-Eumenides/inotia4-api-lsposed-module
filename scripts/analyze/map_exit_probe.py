#!/usr/bin/env python3
"""探测 map 3080 四方向出口：walk 每个方向观察 MAPCHANGE_Set"""
import frida, sys, time, json, urllib.request

BASE = "http://192.168.3.54:8088"
JS = r"""
var g_base = Process.findModuleByName("libgame.so").base;
Interceptor.attach(g_base.add(0x9c740), {
    onEnter: function(args) {
        send("MAPCHANGE_Set(mapId=" + args[0].toInt32() + ", x=" + args[1].toInt32() + ", y=" + args[2].toInt32() + ", dir=" + args[3].toInt32() + ")");
    }
});
Interceptor.attach(g_base.add(0x9c7ec), {
    onEnter: function(args) {
        send("ProcessMapChange() 开始");
    }
});
Interceptor.attach(g_base.add(0x151590), {
    onEnter: function(args) {
        send("GAMESTATE_SetState(" + args[0].toInt32() + ")");
    }
});
"""

def api_post(path, data=None):
    req = urllib.request.Request(BASE + path,
        data=json.dumps(data).encode() if data else b'',
        headers={'Content-Type': 'application/json'})
    try:
        return json.loads(urllib.request.urlopen(req, timeout=5).read())
    except Exception as e:
        return {"err": str(e)}

def api_get(path):
    try:
        return json.loads(urllib.request.urlopen(BASE + path, timeout=5).read())
    except Exception as e:
        return {"err": str(e)}

pid = int(sys.argv[1])
dev = frida.get_device_manager().add_remote_device("127.0.0.1:27042")
sess = dev.attach(pid)
script = sess.create_script(JS)
script.on("message", lambda m, d: print("[frida]", m.get("payload") if m.get("type")=="send" else m))
script.load()
time.sleep(1)

pos = api_get("/api/info/game").get("snapshot", {})
print("=== 起始位置:", pos.get("x"), pos.get("y"), "map", pos.get("mapId"))
for d in [1, 2, 3, 0]:
    api_post("/api/action/movement/walk", {"direction": d})
    time.sleep(1.5)
    pos = api_get("/api/info/game").get("snapshot", {})
    print("walk dir=%d -> x=%s y=%s map=%s" % (d, pos.get("x"), pos.get("y"), pos.get("mapId")))
    if pos.get("mapId") != 3080:
        print("*** 切图成功! map ->", pos.get("mapId"))
        break
sess.detach()

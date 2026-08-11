#!/usr/bin/env python3
"""验证 walk 修复：hook MAP_SetFocus 观察摄像机是否跟随角色移动"""
import frida, sys, time, json, urllib.request

BASE = "http://192.168.3.54:8088"
JS = r"""
var g_base = Process.findModuleByName("libgame.so").base;
send("base=" + g_base);

// hook MAP_SetFocus(0x11336c)
Interceptor.attach(g_base.add(0x11336c), {
    onEnter: function(args) {
        send("MAP_SetFocus(x=" + args[0].toInt32() + ", y=" + args[1].toInt32() + ")");
    }
});

// hook CHAR_Move(0xe9808) 看 flag
Interceptor.attach(g_base.add(0xe9808), {
    onEnter: function(args) {
        send("CHAR_Move(dir=" + args[1].toInt32() + ", delta=" + args[2].toInt32() + ", flag=" + args[3].toInt32() + ")");
    }
});

// hook GAMEPLAY_GoMapLinkByChar
Interceptor.attach(g_base.add(0x9cdc0), {
    onEnter: function(args) {
        send("GoMapLinkByChar(ch=" + args[0] + ", tileX=" + args[1].toInt32() + ", tileY=" + args[2].toInt32() + ")");
    },
    onLeave: function(retval) {
        send("  -> " + retval.toInt32());
    }
});

// hook MAPCHANGE_Set(0x9c740) 看是否触发切图
Interceptor.attach(g_base.add(0x9c740), {
    onEnter: function(args) {
        send("MAPCHANGE_Set(mapId=" + args[0].toInt32() + ", x=" + args[1].toInt32() + ", y=" + args[2].toInt32() + ", dir=" + args[3].toInt32() + ")");
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

print("=== walk dir=0 (向上) 3s ===")
api_post("/api/action/movement/walk", {"direction": 0})
time.sleep(1)
print("=== 当前位置:", api_get("/api/info/game").get("snapshot", {}).get("x"))
time.sleep(2)
sess.detach()

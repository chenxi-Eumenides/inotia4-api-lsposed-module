#!/usr/bin/env python3
"""walk 镜头跟随回归验证：hook MAP_SetFocus + CHAR_Move，调 API walk"""
import frida, sys, time, json, urllib.request

BASE = "http://192.168.3.54:8088"
JS = r"""
var g_base = Process.findModuleByName("libgame.so").base;
var focus_calls = 0;
var move_calls = 0;
Interceptor.attach(g_base.add(0xe9808), {
    onEnter: function(args) {
        move_calls++;
        this.dir = args[1].toInt32();
        this.flag = args[3].toInt32();
    },
    onLeave: function(retval) {
        if (move_calls <= 8) send("CHAR_Move(dir=" + this.dir + " flag=" + this.flag + ") ret=" + retval.toInt32());
    }
});
Interceptor.attach(g_base.add(0x11336c), {
    onEnter: function(args) {
        focus_calls++;
        if (focus_calls <= 8) send("MAP_SetFocus(" + args[0].toInt32() + "," + args[1].toInt32() + ")");
    }
});
send("hooks ready");
"""

def api_post(path, data=None):
    req = urllib.request.Request(BASE + path,
        data=json.dumps(data).encode() if data else b'',
        headers={'Content-Type': 'application/json'})
    try:
        return json.loads(urllib.request.urlopen(req, timeout=5).read())
    except Exception as e:
        return {"err": str(e)}

pid = int(sys.argv[1])
dev = frida.get_device_manager().add_remote_device("127.0.0.1:27042")
sess = dev.attach(pid)
script = sess.create_script(JS)
script.on("message", lambda m, d: print("[frida]", m.get("payload") if m.get("type")=="send" else m))
script.load()
time.sleep(1)
print("=== API walk dir=0 (下) ===")
api_post("/api/action/movement/walk", {"direction": 0})
time.sleep(2)
sess.detach()

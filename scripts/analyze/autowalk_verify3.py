#!/usr/bin/env python3
"""验证 3：改 [ch+0x312] displayType=1，CHAR_Process 是否驱动主控玩家"""
import frida, sys, time, json, urllib.request

PKG = "com.com2us.inotia4.normal.freefull.google.global.android.common"
BASE = "http://192.168.3.54:8088"

JS = r"""
var g_base = Process.findModuleByName("libgame.so").base;
send("base=" + g_base);
var getMember = new NativeFunction(g_base.add(0x11f384), 'pointer', ['int']);
var searchPath = new NativeFunction(g_base.add(0xdb094), 'int', ['pointer', 'int', 'int', 'int']);

Interceptor.attach(g_base.add(0xe9db8), {
    onEnter: function(args) { send(">>> MoveAsPath ch=" + args[0]); }
});
Interceptor.attach(g_base.add(0xf210c), {
    onEnter: function(args) {
        var ch = args[0];
        send("CHAR_Process ch=" + ch + " 0x312=" + ch.add(0x312).readS8() + " pathList=" + ch.add(0x2f0).readPointer());
    }
});
var ch0 = null;
rpc.exports = {
    init: function() {
        ch0 = getMember(0);
        return "ch0=" + ch0 + " 0x312=" + ch0.add(0x312).readS8() + " 0x2e2=" + ch0.add(0x2e2).readU8();
    },
    fillPath: function() {
        var r = searchPath(ch0, 300, 400, 1);
        return "searchPath=" + r + " pathList=" + ch0.add(0x2f0).readPointer();
    },
    setDisp: function(v) {
        ch0.add(0x312).writeS8(v);
        return "0x312=" + ch0.add(0x312).readS8();
    },
    pos: function() { return ch0.add(0x2).readS16() + "," + ch0.add(0x4).readS16(); },
    state: function() { return "pos=" + ch0.add(0x2).readS16() + "," + ch0.add(0x4).readS16() + " 0x312=" + ch0.add(0x312).readS8() + " pathList=" + ch0.add(0x2f0).readPointer(); }
};
"""

def api_post(path, data=None):
    req = urllib.request.Request(BASE + path,
        data=json.dumps(data).encode() if data else b'',
        headers={'Content-Type': 'application/json'})
    try:
        return json.loads(urllib.request.urlopen(req, timeout=8).read())
    except Exception as e:
        return {"err": str(e)}

def api_get(path):
    try:
        return json.loads(urllib.request.urlopen(BASE + path, timeout=5).read())
    except Exception as e:
        return {"err": str(e)}

def main():
    dev = frida.get_device_manager().add_remote_device("127.0.0.1:27042")
    pid = dev.spawn([PKG])
    dev.resume(pid)
    time.sleep(6)
    session = dev.attach(pid)
    script = session.create_script(JS)
    script.on("message", lambda m, d: print("[f]", m.get("payload") if m.get("type")=="send" else m))
    script.load()
    time.sleep(2)
    for i in range(30):
        s = api_get("/api/info/ui/screen")
        if s.get("screen") and s["screen"] != "loading":
            print("[api] screen=" + json.dumps(s)); break
        time.sleep(2)
    time.sleep(1)
    print("[api] enter-slot:", json.dumps(api_post("/api/action/save/enter-slot", {"slot": 0}))[:80])
    time.sleep(4)
    print("init:", script.exports_sync.init())
    print("fillPath:", script.exports_sync.fill_path())
    time.sleep(1)
    print("=== 改 displayType=1 观察 4s:")
    print("setDisp:", script.exports_sync.set_disp(1))
    for i in range(4):
        time.sleep(1)
        print("[t+%ds] %s" % (i+1, script.exports_sync.state()))
    print("=== 还原 0x312=0:")
    print("setDisp:", script.exports_sync.set_disp(0))
    session.detach()

main()

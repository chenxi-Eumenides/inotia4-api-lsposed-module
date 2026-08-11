#!/usr/bin/env python3
"""精确 hook CHAR_Process 驱动链 f20e8-f2118，观测凯恩每个判断"""
import frida, sys, time, json, urllib.request

PKG = "com.com2us.inotia4.normal.freefull.google.global.android.common"
BASE = "http://192.168.3.54:8088"

JS = r"""
var g_base = Process.findModuleByName("libgame.so").base;
send("base=" + g_base);
var getMember = new NativeFunction(g_base.add(0x11f384), 'pointer', ['int']);
var searchPath = new NativeFunction(g_base.add(0xdb094), 'int', ['pointer', 'int', 'int', 'int']);

// hook f20e8 入口（驱动检查起点）
Interceptor.attach(g_base.add(0xf20e8), {
    onEnter: function(args) {
        var ch = args[0];
        var action = ch.add(0x280).readPointer();
        send("f20e8 enter: ch=" + ch + " 0x2e0=" + ch.add(0x2e0).readU8() + " action=" + action + (action.isNull()?"":" id="+action.readU16()) + " 0x2fa=" + ch.add(0x2fa).readU8());
    }
});
// hook CHAR_IsActionWalk 返回值
Interceptor.attach(g_base.add(0xe757c), {
    onEnter: function(args) { this.ch = args[0]; this.ap = args[1]; },
    onLeave: function(ret) {
        if (this.ap && !this.ap.isNull())
            send("IsActionWalk(" + this.ch + " actionId=" + this.ap.readU16() + ") = " + ret.toInt32());
    }
});
// hook f210c（PATHLIST 检查）
Interceptor.attach(g_base.add(0xf210c), {
    onEnter: function(args) {
        send("f210c: pathList=" + args[0].add(0x2f0).readPointer());
    }
});

var ch0 = null;
rpc.exports = {
    init: function() { ch0 = getMember(0); return "ch0=" + ch0; },
    fillPath: function() {
        var r = searchPath(ch0, 300, 400, 1);
        return "searchPath=" + r + " pathList=" + ch0.add(0x2f0).readPointer() + " actionId=" + ch0.add(0x280).readPointer().readU16();
    },
    pos: function() { return ch0.add(0x2).readS16() + "," + ch0.add(0x4).readS16(); }
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
    time.sleep(3)
    print("pos:", script.exports_sync.pos())
    session.detach()

main()

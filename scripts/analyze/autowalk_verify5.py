#!/usr/bin/env python3
"""最终验证：CHAR_SetActionID(ch,2,target) 正规设置行走动作后，玩家被 CHAR_Process 自动驱动？"""
import frida, sys, time, json, urllib.request

PKG = "com.com2us.inotia4.normal.freefull.google.global.android.common"
BASE = "http://192.168.3.54:8088"

JS = r"""
var g_base = Process.findModuleByName("libgame.so").base;
send("base=" + g_base);
var getMember = new NativeFunction(g_base.add(0x11f384), 'pointer', ['int']);
var searchPath = new NativeFunction(g_base.add(0xdb094), 'int', ['pointer', 'int', 'int', 'int']);
var setActionID = new NativeFunction(g_base.add(0xe79ec), 'void', ['pointer', 'int', 'pointer']);
var findAction = new NativeFunction(g_base.add(0xdd3ac), 'pointer', ['pointer', 'int']);

Interceptor.attach(g_base.add(0xe9db8), {
    onEnter: function(args) { send(">>> MoveAsPath ch=" + args[0]); }
});
Interceptor.attach(g_base.add(0xe757c), {
    onEnter: function(args) { this.ap = args[1]; },
    onLeave: function(ret) {
        if (this.ap && !this.ap.isNull()) send("IsActionWalk(actionId=" + this.ap.readU16() + ") = " + ret.toInt32());
    }
});

var ch0 = null;
rpc.exports = {
    init: function() {
        ch0 = getMember(0);
        return "ch0=" + ch0 + " 0x280=" + ch0.add(0x280).readPointer();
    },
    fillPath: function() {
        var r = searchPath(ch0, 300, 400, 1);
        return "searchPath=" + r + " pathList=" + ch0.add(0x2f0).readPointer();
    },
    setWalk: function() {
        var action = findAction(ch0, 2);
        send("findAction(2)=" + action + " (id=" + (action.isNull()? -1 : action.readU16()) + ")");
        setActionID(ch0, 2, ch0);
        var cur = ch0.add(0x280).readPointer();
        return "0x280=" + cur + " id=" + (cur.isNull()? -1 : cur.readU16());
    },
    setIdle: function() {
        var action = findAction(ch0, 0);
        send("findAction(0)=" + action);
        setActionID(ch0, 0, ch0);
        return "0x280=" + ch0.add(0x280).readPointer();
    },
    pos: function() { return ch0.add(0x2).readS16() + "," + ch0.add(0x4).readS16(); },
    state: function() {
        var a = ch0.add(0x280).readPointer();
        return "pos=" + ch0.add(0x2).readS16() + "," + ch0.add(0x4).readS16() + " actionId=" + (a.isNull()? -1 : a.readU16()) + " pathList=" + ch0.add(0x2f0).readPointer();
    }
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
    print("=== setWalk (SetActionID 2):")
    print("setWalk:", script.exports_sync.set_walk())
    time.sleep(1)
    print("state:", script.exports_sync.state())
    print("=== 观察 4s:")
    for i in range(4):
        time.sleep(1)
        print("[t+%ds] %s" % (i+1, script.exports_sync.state()))
    print("=== setIdle 还原:")
    print("setIdle:", script.exports_sync.set_idle())
    time.sleep(1)
    print("state:", script.exports_sync.state())
    session.detach()

main()

#!/usr/bin/env python3
"""验证：SearchPath 填 PATHLIST + 动作=行走 → 游戏主循环自动逐帧驱动玩家"""
import frida, sys, time, json, urllib.request

PKG = "com.com2us.inotia4.normal.freefull.google.global.android.common"
BASE = "http://192.168.3.54:8088"

JS = r"""
var g_base = Process.findModuleByName("libgame.so").base;
send("base=" + g_base);

// CHAR_GetDisplayType 0xdcfd0 / CHAR_IsActionWalk 0xe757c / CHAR_MoveAsPath 0xe9db8 / CHAR_Process 0xf1c04
var isActionWalk = new NativeFunction(g_base.add(0xe757c), 'int', ['pointer', 'pointer']);
var moveAsPath = new NativeFunction(g_base.add(0xe9db8), 'int', ['pointer']);
var getMember = new NativeFunction(g_base.add(0x11f384), 'pointer', ['int']);
var searchPath = new NativeFunction(g_base.add(0xdb094), 'int', ['pointer', 'int', 'int', 'int']);

// hook MoveAsPath 调用者：backtrace 看是否由 CHAR_Process(f2118) 驱动
Interceptor.attach(g_base.add(0xe9db8), {
    onEnter: function(args) {
        send("MoveAsPath ch=" + args[0]);
    }
});
// hook CHAR_Process 内 f210c-f2118 区间观察条件
Interceptor.attach(g_base.add(0xf210c), {
    onEnter: function(args) {
        var ch = args[0];
        var pathList = ch.add(0x2f0).readPointer();
        var action = ch.add(0x280).readPointer();
        var walk = 0;
        if (!action.isNull()) walk = action.readU16();
        send("CHAR_Process: pathList=" + pathList + " actionId=" + walk + " (path null? " + pathList.isNull() + ")");
    }
});

var ch0 = null;
rpc.exports = {
    init: function() {
        ch0 = getMember(0);
        send("ch0=" + ch0 + " 0x2e2=" + ch0.add(0x2e2).readU8());
        send("before: 0x280=" + ch0.add(0x280).readPointer() + " 0x2f0=" + ch0.add(0x2f0).readPointer());
        return "ok";
    },
    // SearchPath 填 PATHLIST（不消费），目标 (300,400) tile
    fillPath: function() {
        var r = searchPath(ch0, 300, 400, 1);
        send("searchPath(" + 300 + "," + 400 + ")=" + r + " pathList=" + ch0.add(0x2f0).readPointer());
        return r;
    },
    // 设置动作=行走：找 action 对象（复用 0x280 现有动作指针改 ID，或直接构造）
    setWalkAction: function() {
        // 简单方式：直接改 [ch+0x280] 指向的动作对象 ID==2（若已有动作对象）
        var action = ch0.add(0x280).readPointer();
        if (!action.isNull()) {
            action.writeU16(2);  // 动作ID=2 行走
            send("set actionId=2 at " + action + " (was " + action.readU16() + ")");
        } else {
            send("action is null!");
        }
        return "ok";
    },
    resetAction: function() {
        var action = ch0.add(0x280).readPointer();
        if (!action.isNull()) { action.writeU16(0); send("reset actionId=0"); }
        return "ok";
    },
    pos: function() {
        return ch0.add(0x2).readS16() + "," + ch0.add(0x4).readS16();
    },
    isWalk: function() {
        var action = ch0.add(0x280).readPointer();
        if (action.isNull()) return "no-action";
        return "" + isActionWalk(ch0, action);
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
    time.sleep(1)
    # 1. 填 PATHLIST（不消费）
    print("fillPath:", script.exports_sync.fill_path())
    time.sleep(1)
    print("pos after fill:", script.exports_sync.pos(), " isWalk:", script.exports_sync.is_walk())
    # 2. 设动作=行走
    print("setWalk:", script.exports_sync.set_walk_action())
    time.sleep(1)
    print("pos after setWalk:", script.exports_sync.pos(), " isWalk:", script.exports_sync.is_walk())
    # 3. 观察 3 秒：游戏主循环是否自动走（每帧 MoveAsPath）
    for i in range(3):
        time.sleep(1)
        print("[t+%ds] pos=%s" % (i+1, script.exports_sync.pos()))
    # 4. 还原动作
    print("reset:", script.exports_sync.reset_action())
    time.sleep(2)
    print("[final] pos=%s" % script.exports_sync.pos())
    session.detach()

main()

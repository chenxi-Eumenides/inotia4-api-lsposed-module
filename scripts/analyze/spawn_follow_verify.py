#!/usr/bin/env python3
"""spawn 精确验证 CHAR_Move 跟随条件（隔离 GoMapLinkByChar 副作用）"""
import frida, sys, time, json, urllib.request

PKG = "com.com2us.inotia4.normal.freefull.google.global.android.common"
BASE = "http://192.168.3.54:8088"

JS = r"""
var g_base = Process.findModuleByName("libgame.so").base;
send("base=" + g_base);

// 当前玩家指针槽
var pslot = g_base.add(0x2f6000 + 0xa50);
var mslot = g_base.add(0x2f3000 + 0x5b0);

// hook CHAR_Move：记录 ch、flag、0x2fa、player、mchar、x22==player?
Interceptor.attach(g_base.add(0xe9808), {
    onEnter: function(args) {
        var ch = args[0];
        var flag = args[3].toInt32();
        var fa = ch.add(0x2fa).readU8();
        var player = pslot.readPointer();
        var mchar = mslot.readPointer();
        send("CHAR_Move ch=" + ch + " dir=" + args[1].toInt32() + " flag=" + flag +
             " [0x2fa]=" + fa + " player=" + player + " chIsPlayer=" + (ch.equals(player)) +
             " mchar=" + mchar + " chIsMchar=" + (ch.equals(mchar)));
    }
});

// hook MAP_SetFocus
Interceptor.attach(g_base.add(0x11336c), {
    onEnter: function(args) {
        send(">>> MAP_SetFocus(" + args[0].toInt32() + "," + args[1].toInt32() + ")");
    }
});

// hook GoMapLinkByChar（模块每帧调用）
Interceptor.attach(g_base.add(0x9cdc0), {
    onEnter: function(args) {
        send("GoMapLinkByChar(ch=" + args[0] + " tx=" + args[1].toInt32() + " ty=" + args[2].toInt32() + ")");
    }
});

// RPC：直接调 CHAR_Move（纯调用，无 GoMapLink）
var charMove = new NativeFunction(g_base.add(0xe9808), 'int', ['pointer', 'int', 'int', 'int']);
// RPC：取 member(0)
var getMember = new NativeFunction(g_base.add(0x11f384), 'pointer', ['int']);
var goMapLinkByChar = new NativeFunction(g_base.add(0x9cdc0), 'int', ['pointer', 'int', 'int']);

var ch0 = null;
rpc.exports = {
    getCh: function() { ch0 = getMember(0); send("member(0)=" + ch0); return ch0.toString(); },
    movePure: function(dir) { send("--- pure CHAR_Move dir=" + dir); return charMove(ch0, dir, 8, 0); },
    moveWithLink: function(dir) {
        send("--- CHAR_Move+GoMapLink dir=" + dir);
        var r = charMove(ch0, dir, 8, 0);
        var x = ch0.add(0x2).readS16();
        var y = ch0.add(0x4).readS16();
        goMapLinkByChar(ch0, x >> 4, y >> 4);
        return r;
    },
    getPos: function() { return ch0.add(0x2).readS16() + "," + ch0.add(0x4).readS16(); },
    getFa: function() { return ch0.add(0x2fa).readU8(); }
};
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
    # 等 API ready + 进存档
    for i in range(30):
        s = api_get("/api/info/ui/screen")
        if s.get("screen") and s["screen"] != "loading":
            print("[api] screen=" + json.dumps(s)); break
        time.sleep(2)
    time.sleep(1)
    print("[api] enter-slot:", api_post("/api/action/save/enter-slot", {"slot": 0}))
    time.sleep(4)
    # 取 ch
    ch = script.exports_sync.get_ch()
    # 纯 CHAR_Move 3 次
    for i in range(3):
        script.exports_sync.move_pure(0)
        time.sleep(0.5)
        print("[api] pos=" + script.exports_sync.get_pos())
    # CHAR_Move+GoMapLink 3 次（模块 walk 路径）
    for i in range(3):
        script.exports_sync.move_with_link(0)
        time.sleep(0.5)
        print("[api] pos=" + str(script.exports_sync.get_pos()) + " 0x2fa=" + str(script.exports_sync.get_fa()))
    time.sleep(2)
    session.detach()

main()

#!/usr/bin/env python3
"""验证 hook 状态：GAMESTATE_Draw 入口 + frame_tick 调用计数"""
import frida, sys, time, json, urllib.request

BASE = "http://192.168.3.54:8088"
JS = r"""
var g_base = Process.findModuleByName("libgame.so").base;
var gb = Process.findModuleByName("libgamebridge.so").base;
send("libgame base=" + g_base);
send("libgamebridge base=" + gb);

// GAMESTATE_Draw 入口（0x1512b8）前 4 指令
var draw = g_base.add(0x1512b8);
send("GAMESTATE_Draw[0]=" + draw.readU32().toString(16) + " [1]=" + draw.add(4).readU32().toString(16) + " [2]=" + draw.add(8).readU32().toString(16) + " [3]=" + draw.add(12).readU32().toString(16));

// frame_tick 在 gamebridge 的偏移（nm 确认 0x51f28）
var ft = gb.add(0x51f28);
send("frame_tick addr=" + ft);
var cnt = 0;
Interceptor.attach(ft, { onEnter: function(args) { cnt++; } });

// GAMESTATE_Draw 入口 hook
var drawCnt = 0;
Interceptor.attach(draw, { onEnter: function(args) { drawCnt++; } });

// 每 2 秒报告计数
var report = setInterval(function() {
    send("frame_tick=" + cnt + " draw_entry=" + drawCnt);
}, 2000);
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
script.on("message", lambda m, d: print("[f]", m.get("payload") if m.get("type")=="send" else m))
script.load()
time.sleep(2)
print("=== walk dir=0:")
api_post("/api/action/movement/walk", {"direction": 0})
time.sleep(6)
print("=== walk_stop:")
api_post("/api/action/movement/walk/stop")
time.sleep(2)
sess.detach()

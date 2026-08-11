#!/usr/bin/env python3
"""探针 v7：调 MAPSYSTEM_ChangeMap(0x114fc4) 切图回 3080。
用法：uv run python .tmp/hook_changemap.py <PID> <mapId> <x> <y>
"""
from __future__ import annotations
import sys, time
import frida

PID = sys.argv[1] if len(sys.argv) > 1 else "29265"
MAP_ID = int(sys.argv[2]) if len(sys.argv) > 2 else 3080
X = int(sys.argv[3]) if len(sys.argv) > 3 else 120
Y = int(sys.argv[4]) if len(sys.argv) > 4 else 312

JS = r"""
var base = null;
Process.enumerateModules().forEach(function (m) {
    if (m.path.indexOf("libgame.so") >= 0 && base === null) base = m.base;
});
if (base === null) { send("ERROR: libgame.so not found"); }

var CHANGE_MAP = new NativeFunction(base.add(0x114fc4), 'void', ['int', 'int', 'int', 'int']);
var GET_MEMBER = new NativeFunction(base.add(0x11f384), 'pointer', ['int']);
var GAMESTATE_nState = 0x72b068;

send("calling ChangeMap(mapId=__MAP__, x=__X__, y=__Y__, dir=0)...");
var gs_before = base.add(GAMESTATE_nState).readU32();
send("GAMESTATE_nState before: " + gs_before);
CHANGE_MAP(__MAP__, __X__, __Y__, 0);
send("ChangeMap called");
""".replace("__MAP__", str(MAP_ID)).replace("__X__", str(X)).replace("__Y__", str(Y))

s = frida.get_device_manager().add_remote_device("127.0.0.1:27042")
session = s.attach(int(PID))
script = session.create_script(JS)
script.on("message", lambda msg, data: print(msg.get("payload") if msg.get("type") == "send" else msg))
script.load()
time.sleep(4)
print("---API check---")
import urllib.request
try:
    r = urllib.request.urlopen("http://192.168.3.54:8088/api/info/current-map", timeout=5).read().decode()
    print(r[:200])
except Exception as e:
    print("api err:", e)

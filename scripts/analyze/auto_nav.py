#!/usr/bin/env python3
"""自动导航：frida 读瓦片矩阵 → BFS 找路径 → API 分段 move 到目标出口。
用法：uv run python .tmp/auto_nav.py <PID> <target_tx> <target_ty>
"""
from __future__ import annotations
import sys, time, json, urllib.request
import frida
from collections import deque

PID = sys.argv[1] if len(sys.argv) > 1 else "29265"
TARGET_TX = int(sys.argv[2]) if len(sys.argv) > 2 else 39
TARGET_TY = int(sys.argv[3]) if len(sys.argv) > 3 else 42
API = "http://192.168.3.54:8088"

def api_get(path):
    try:
        return json.loads(urllib.request.urlopen(API + path, timeout=5).read().decode())
    except Exception as e:
        return {"error": str(e)}

def api_post(path, body):
    try:
        req = urllib.request.Request(API + path, data=json.dumps(body).encode(),
                                     headers={"Content-Type": "application/json"})
        return json.loads(urllib.request.urlopen(req, timeout=12).read().decode())
    except Exception as e:
        return {"error": str(e)}

# 1. frida 读瓦片矩阵
JS = r"""
var base = null;
Process.enumerateModules().forEach(function (m) {
    if (m.path.indexOf("libgame.so") >= 0 && base === null) base = m.base;
});
var got = base.add(0x2f3f48);
var tiles = got.readPointer();
var rows = [];
for (var y = 0; y < 64; y++) {
    var row = "";
    for (var x = 0; x < 64; x++) {
        var b = tiles.add(y * 64 + x).readU8();
        row += (b & 0x08) ? "1" : "0";  // 1=阻挡
    }
    rows.push(row);
}
var GET_MEMBER = new NativeFunction(base.add(0x11f384), 'pointer', ['int']);
var ch = GET_MEMBER(0);
var px = ch.isNull() ? -1 : ch.add(0x2).readS16();
var py = ch.isNull() ? -1 : ch.add(0x4).readS16();
send(JSON.stringify({rows: rows, px: px, py: py}));
"""

s = frida.get_device_manager().add_remote_device("127.0.0.1:27042")
session = s.attach(int(PID))
script = session.create_script(JS)
result = {}
script.on("message", lambda msg, data: result.update(json.loads(msg["payload"])) if msg.get("type") == "send" else None)
script.load()
time.sleep(3)

rows = result["rows"]
px, py = result["px"], result["py"]
sx, sy = px // 16, py // 16
print(f"player tile: ({sx},{sy})  target: ({TARGET_TX},{TARGET_TY})")

# 2. BFS
grid = [[c == "1" for c in row] for row in rows]
def bfs():
    q = deque([(sx, sy)])
    prev = {(sx, sy): None}
    while q:
        x, y = q.popleft()
        if (x, y) == (TARGET_TX, TARGET_TY):
            path = []
            cur = (x, y)
            while cur:
                path.append(cur)
                cur = prev[cur]
            return path[::-1]
        for dx, dy in [(1,0),(-1,0),(0,1),(0,-1)]:
            nx, ny = x+dx, y+dy
            if 0 <= nx < 64 and 0 <= ny < 64 and not grid[ny][nx] and (nx, ny) not in prev:
                prev[(nx, ny)] = (x, y)
                q.append((nx, ny))
    return None

path = bfs()
if path is None:
    print("NO PATH!")
    sys.exit(1)
print(f"path found: {len(path)} tiles")
# 每 4 tile 一个 waypoint，转像素
waypoints = [(x*16+8, y*16+8) for x, y in path[::4]]
if waypoints[-1] != (TARGET_TX*16+8, TARGET_TY*16+8):
    waypoints.append((TARGET_TX*16+8, TARGET_TY*16+8))
print("waypoints:", waypoints)

# 3. API 分段 move
for i, (wx, wy) in enumerate(waypoints):
    r = api_post("/api/action/movement/move", {"x": wx, "y": wy})
    ok = r.get("ok", False)
    cur = api_get("/api/info/current-map")
    print(f"  move[{i}] ({wx},{wy}): ok={ok} → map={cur.get('mapId')} pos=({cur.get('x')},{cur.get('y')})")
    if cur.get("mapId") != api_get("/api/info/current-map").get("mapId") and i > 0:
        pass
    time.sleep(1.2)
print("DONE")

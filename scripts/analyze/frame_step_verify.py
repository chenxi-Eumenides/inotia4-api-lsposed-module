#!/usr/bin/env python3
"""验证 move/walk 逐帧移动：移动过程中每 100ms 采样 frame+位置，确认每步帧计数递增"""
import json, time, urllib.request

BASE = "http://192.168.3.54:8088"

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

def sample():
    f = api_get("/api/info/game/frame").get("frame", -1)
    s = api_get("/api/info/game").get("snapshot", {})
    return f, s.get("x"), s.get("y")

# 进 world
api_post("/api/action/save/enter-slot", {"slot": 0})
time.sleep(4)

print("=== 基线（3 次采样）:")
for i in range(3):
    f, x, y = sample()
    print(f"  t={i}: frame={f} pos=({x},{y})")
    time.sleep(0.5)

# walk 测试（方向 1=左，走 60 帧 ≈ 3.5s）
print("\n=== walk dir=1（左）移动过程采样（每 100ms）:")
f0, x0, y0 = sample()
api_post("/api/action/movement/walk", {"direction": 1})
for i in range(40):
    f, x, y = sample()
    print(f"  t={i*100}ms: frame={f} pos=({x},{y})")
    if (x, y) != (x0, y0) and i > 0 and f == last_f:
        pass
    last_f = f
    time.sleep(0.1)
f1, x1, y1 = sample()
print(f"walk 结束: frame {f0}->{f1} (+{f1-f0}), pos ({x0},{y0})->({x1},{y1})")

time.sleep(1)

# move 测试（寻路到右侧目标）
print("\n=== move 到 (x0+160, y0) 移动过程采样（每 100ms）:")
f0, x0, y0 = sample()
r = api_post("/api/action/movement/move", {"x": x0 + 160, "y": y0})
print(f"move 返回: {r.get('ok')} {r.get('error', '')}")
for i in range(40):
    f, x, y = sample()
    print(f"  t={i*100}ms: frame={f} pos=({x},{y})")
    time.sleep(0.1)
f1, x1, y1 = sample()
print(f"move 结束: frame {f0}->{f1} (+{f1-f0}), pos ({x0},{y0})->({x1},{y1})")

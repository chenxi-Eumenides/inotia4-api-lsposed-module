#!/usr/bin/env python3
"""从读存档一路走到影子丛林2 路障前（自旋等待，无固定 sleep 控时序）。

流程（剧情按用户权威描述 m1018 + 实测确认）：
  1. enter-slot slot:0 → 营地 map0 (120,312)
  2. move (392,320) → 剧情1（evtId=1 凯恩×卓拉德）→ skip → 任务180 简报 popup → ok
  3. 等切图到 map30（影子丛林1）→ move (560,240) 触发剧情2（evtId=11 打士兵）
  4. skip 剧情2 → 若触发药水教学（tutorial_pause）→ use-item 药水完成
  5. move (600,660) → 剧情3（evtId=2 切图剧情）→ skip → 等切图 map31（影子丛林2）
  6. move (312,152) → 到路障正左方 (280,152) 停下

等待机制：所有阶段用自旋轮询 API（sleep 仅在轮询间隔用），不用固定 sleep 控制两阶段时序。
用法：uv run python scripts/flow/run_to_barricade.py [IP]
"""
from __future__ import annotations

import json
import sys
import time
import urllib.request
import urllib.error

BASE_PORT = 8088
POLL_INTERVAL = 0.5
MAX_WAIT = 120.0


def fetch_json(url: str) -> dict:
    time.sleep(0.05)
    with urllib.request.urlopen(url, timeout=5) as r:
        return json.loads(r.read().decode("utf-8"))


def post_json(url: str, body: dict | None = None) -> dict:
    time.sleep(0.05)
    data = json.dumps(body).encode() if body is not None else b""
    req = urllib.request.Request(url, data=data, headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=5) as r:
        return json.loads(r.read().decode("utf-8"))


# name → API 路径映射（wait_for 用）
API_PATHS = {
    "current-map": "/api/info/current-map",
    "ui/screen": "/api/info/ui/screen",
    "dialog/content": "/api/info/dialog/content",
}


def wait_for(name: str, pred, timeout: float = MAX_WAIT) -> dict:
    """自旋等待：轮询 API 直到 pred(result) 为真，超时抛异常。"""
    start = time.time()
    last: dict = {}
    path = API_PATHS.get(name, name)
    while time.time() - start < timeout:
        try:
            last = fetch_json(f"{BASE}{path}")
        except Exception:
            last = {}
        if pred(last):
            return last
        time.sleep(POLL_INTERVAL)
    raise TimeoutError(f"wait {name} timed out, last={last}")

def wait_until_stop(timeout: float = MAX_WAIT) -> dict:
    start = time.time()
    path = API_PATHS.get("current-map")
    last = fetch_json(f"{BASE}{path}")
    while time.time() - start < timeout:
        now = fetch_json(f"{BASE}{path}")
        if last.get("x") == now.get("x") and last.get("y") == now.get("y"):
            break
        last = now
        time.sleep(POLL_INTERVAL)
    raise TimeoutError(f"move not stop")

def current_map() -> dict:
    return fetch_json(f"{BASE}/api/info/current-map")


def screen() -> dict:
    return fetch_json(f"{BASE}/api/info/ui/screen")


def dialog_content() -> dict:
    return fetch_json(f"{BASE}/api/info/dialog/content")


def quest_list() -> dict:
    return fetch_json(f"{BASE}/api/info/quest/list")


def log(msg: str) -> None:
    print(f"[{time.strftime('%H:%M:%S')}] {msg}", flush=True)


IP = sys.argv[1] if len(sys.argv) > 1 else "192.168.3.54"
BASE = f"http://{IP}:{BASE_PORT}"


def main() -> None:
    # --- 0. 强制回主菜单（确保干净状态起点）---
    log("回主菜单")
    s0 = screen()
    if s0.get("screen") in ("story", "dialog", "wipeout"):
        # 剧情/弹窗/死亡态先回到 world：循环 skip / ok / game_over
        for _ in range(10):
            c = dialog_content()
            t = c.get("type")
            if t == "story":
                post_json(f"{BASE}/api/action/dialog/select", {"action": "skip"})
            elif t == "popup":
                post_json(f"{BASE}/api/action/dialog/select", {"action": "ok"})
            elif t == "wipeout":
                post_json(f"{BASE}/api/action/dialog/select", {"action": "game_over"})
            elif t == "npc":
                post_json(f"{BASE}/api/action/dialog/select", {"action": "next"})
            else:
                break
            time.sleep(0.5)
    post_json(f"{BASE}/api/action/ui/main-menu", {})
    wait_for("ui/screen", lambda d: d.get("screen") == "main_menu")
    log("已在主菜单")

    # --- 1. 读存档 → 营地 ---
    log("enter-slot slot:0")
    r = post_json(f"{BASE}/api/action/save/enter-slot", {"slot": 0})
    log(f"enter-slot: {r}")
    # already in game（世界态仍残留）时回主菜单再试一次
    if not r.get("ok", False) and "already in game" in str(r):
        post_json(f"{BASE}/api/action/ui/main-menu", {})
        wait_for("ui/screen", lambda d: d.get("screen") == "main_menu")
        r = post_json(f"{BASE}/api/action/save/enter-slot", {"slot": 0})
        log(f"重进: {r}")
    wait_for("ui/screen", lambda d: d.get("screen") == "world")
#    m = wait_for("current-map", lambda d: d.get("mapId") == 0 and d.get("x") and d.get("x") > 0)
#    log(f"在营地 map0 ({m.get('x')},{m.get('y')})")

    # --- 2. 剧情1（凯恩×卓拉德）--- 走到触发区 (392,320)
    log("move → (392,320) 触发剧情1")
    post_json(f"{BASE}/api/action/movement/move", {"x": 392, "y": 320})
    wait_for("ui/screen", lambda d: d.get("screen") == "story")
    log("剧情1 触发（evtId=1）")
    c = dialog_content()
    log(f"剧情1: index={c.get('index')}/{c.get('count')} speaker={c.get('speaker')}")

    log("skip 剧情1")
    # skip 可能只跳一段（index 推进），循环 skip 直到 popup 或 world
    for _ in range(20):
        c2 = dialog_content()
        t2 = c2.get("type")
        if t2 == "popup":
            break
        if t2 == "story":
            post_json(f"{BASE}/api/action/dialog/select", {"action": "skip"})
            time.sleep(0.5)
        else:
            break
    wait_for("dialog/content", lambda d: d.get("type") == "popup")
    log("任务180 简报 popup")
    post_json(f"{BASE}/api/action/dialog/select", {"action": "ok"})
    wait_for("ui/screen", lambda d: d.get("screen") == "world")
    # 等切图到 map30
#    wait_for("ui/screen", lambda d: d.get("screen") == "story")
#    log("切图到影子丛林1 (map30)")
    q = quest_list()
    log(f"quest: {q.get('quests')}")

    # --- 3. 剧情2（事件11 打士兵）---
    log("move → (560,240) 触发剧情2")
    post_json(f"{BASE}/api/action/movement/move", {"x": 560, "y": 240})
    wait_for("ui/screen", lambda d: d.get("screen") == "story")
    log("剧情2 触发（evtId=11）")
    c = dialog_content()
    log(f"剧情2: index={c.get('index')}/{c.get('count')}")

    log("skip 剧情2")
    post_json(f"{BASE}/api/action/dialog/select", {"action": "skip"})
    wait_for("ui/screen", lambda d: d.get("screen") == "world")
    log("剧情2 结束，士兵敌对化")
    post_json(f"{BASE}/api/action/movement/move", {"x": 480, "y": 460})
    wait_unitl_stop()
    post_json(f"{BASE}/api/action/movement/move", {"x": 600, "y": 660})
    log("move → (600,660) 触发剧情3")

    # --- 4. 若触发药水教学 → use-item 完成 ---
    s = screen()
    if s.get("screen") == "tutorial_pause":
        log("药水教学激活（tutorial_pause）→ use-item 完成")
        post_json(f"{BASE}/api/action/inventory/use-item", {"bag": 0, "slot": 0})
        wait_for("ui/screen", lambda d: d.get("screen") == "world")
        log("教学完成，回 world")
        # 用药水后可能弹出 character_info 面板，关闭
        s2 = screen()
        if s2.get("screen") not in ("world",):
            log(f"关闭面板 {s2.get('screen')}")
            post_json(f"{BASE}/api/action/ui/panel/close", {})
            wait_for("ui/screen", lambda d: d.get("screen") == "world")

    # --- 5. 剧情3（事件2 切图剧情）---
    try:
        wait_for("ui/screen", lambda d: d.get("screen") == "story", timeout=90)
        log("剧情3 触发（evtId=2）")
        c = dialog_content()
        log(f"剧情3: index={c.get('index')}/{c.get('count')} speaker={c.get('speaker')}")
        log("skip 剧情3")
        post_json(f"{BASE}/api/action/dialog/select", {"action": "skip"})
        wait_for("ui/screen", lambda d: d.get("screen") == "world", timeout=90)
    except TimeoutError:
        log("剧情3 未触发（可能已在 map31），继续")
    # 等切图到 map31
    wait_for("ui/screen", lambda d: d.get("screen") == "world")
#    wait_for("current-map", lambda d: d.get("mapId") == 31)
    log("切图到影子丛林2 (map31)")
    q = quest_list()
    log(f"quest: {q.get('quests')}")

    # --- 6. 走到路障 (312,152) ---
    log("move → (312,152) 路障")
    post_json(f"{BASE}/api/action/movement/move", {"x": 312, "y": 152})
    wait_for("current-map", lambda d: d.get("mapId") == 31 and 250 <= (d.get("x") or 0) <= 300 and 120 <= (d.get("y") or 0) <= 180)
    m = current_map()
    log(f"到达路障前 ({m.get('x')},{m.get('y')})")
    q = quest_list()
    log(f"最终 quest: {q.get('quests')}")
    log("流程完成。")


if __name__ == "__main__":
    main()

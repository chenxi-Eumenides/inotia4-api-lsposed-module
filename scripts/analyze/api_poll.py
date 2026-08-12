#!/usr/bin/env python3
"""连续轮询手机 API，检测字段变化（Tailscale 联调用）。

用法：uv run python scripts/analyze/api_poll.py <手机tailscale-IP> [间隔秒] [次数]
输出：每次采样的 /api/info/player 字段 + 变化标记
"""
from __future__ import annotations

import json
import sys
import time
import urllib.request

BASE_PORT = 8088


def fetch_json(url: str) -> dict:
    with urllib.request.urlopen(url, timeout=5) as r:
        return json.loads(r.read().decode("utf-8"))


def main() -> None:
    ip = sys.argv[1] if len(sys.argv) > 1 else "192.168.3.54"
    interval = float(sys.argv[2]) if len(sys.argv) > 2 else 2.0
    count = int(sys.argv[3]) if len(sys.argv) > 3 else 30
    base = f"http://{ip}:{BASE_PORT}"
    print(f"轮询 {base}/api/info/player，间隔 {interval}s，共 {count} 次（Ctrl+C 停止）")
    prev: dict = {}
    for i in range(count):
        try:
            p = fetch_json(f"{base}/api/info/player")
            party = fetch_json(f"{base}/api/info/player/party")
            inv = fetch_json(f"{base}/api/info/inventory")
        except Exception as e:  # noqa: BLE001
            print(f"[{i:02d}] 连接失败: {e}")
            time.sleep(interval)
            continue
        changed = [k for k in p if prev.get(k) != p[k]]
        prev = dict(p)
        lead = party[0] if isinstance(party, list) and party and party[0] else {}
        bag_total = sum(
            sum(1 for s in b.get("slots", []) if s) for b in inv.get("bags", [])
        )
        print(
            f"[{i:02d}] money={p.get('money')} map={p.get('mapId')} "
            f"pos=({p.get('x')},{p.get('y')}) party={p.get('partyCount')} "
            f"lead_hp={lead.get('hp')}/{lead.get('maxHp')} lv={lead.get('level')} "
            f"bag_items={bag_total}{' ⚠️变:' + str(changed) if changed else ''}"
        )
        time.sleep(interval)


if __name__ == "__main__":
    main()

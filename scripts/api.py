#!/usr/bin/env python3
"""通用 API 请求脚本：向游戏模块 HTTP 服务发送 GET/POST 请求并打印返回 JSON。

使用方法：直接编辑底部 __main__ 里的调用（改路径/body），然后运行：
  uv run python scripts/api.py
"""
from __future__ import annotations

import json
import urllib.request

IP = "192.168.3.54"
BASE_PORT = 8088

def main():
    r = info("game/info")
    log(r)
    r = info("game/snapshot")
    log(r)
    r = info("ui/screen")
    log(r)


def info(path: str) -> dict:
    return get(f"info/{path}")


def data(path: str) -> dict:
    return get(f"data/{path}")


def action(path: str, body: dict) -> dict:
    return post(f"action/{path}", body)


def op(path: str, body: dict) -> dict:
    return post(f"op/{path}", body)


def get(path: str) -> dict:
    url = f"http://{IP}:{BASE_PORT}/api/{path}"
    print(f"→ GET  /api/{path}", flush=True)
    with urllib.request.urlopen(url, timeout=5) as r:
        return json.loads(r.read().decode("utf-8"))


def post(path: str, body: dict) -> dict:
    url = f"http://{IP}:{BASE_PORT}/api/{path}"
    print(f"→ POST /api/{path} {json.dumps(body, ensure_ascii=False)}", flush=True)
    req = urllib.request.Request(url, data=json.dumps(body).encode("utf-8"),
        headers={"Content-Type": "application/json"}, method="POST")
    with urllib.request.urlopen(req, timeout=5) as r:
        return json.loads(r.read().decode("utf-8"))


def log(data):
    print(json.dumps(data, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()


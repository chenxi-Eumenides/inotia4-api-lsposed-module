#!/usr/bin/env python3
"""frida 全量 dump 416 图通行矩阵。

流程：
1. frida attach 游戏进程
2. 对每个 mapId，调 MAPSYSTEM_ChangeMap(0x114fc4) 强制切图
3. 切图后短延时（等 MAP_Load 完成 + 矩阵填充）
4. 读 *(*(base+0x2f3f48)) 4096 字节 = 矩阵
5. base64 编码后存为 JSON
6. 全部完成后卸载 frida

输出：apk/static-data/json/maps/tiles_frida.json（与 tiles.json 格式一致）
耗时：~5-10 分钟（每图 0.5-1s 切图 + 0.5s 抓取）

⚠️ 需要真机联调，且游戏需处于主菜单或可切图状态。
部分剧情锁定的 map 可能无法切图（脚本会跳过，记录到 errors.json）。

运行：uv run python scripts/analyze/dump_all_map_tiles.py <PID>
"""
from __future__ import annotations
import base64
import json
import sys
import time
import frida


PID = int(sys.argv[1]) if len(sys.argv) > 1 else None
if PID is None:
    print("Usage: uv run python scripts/analyze/dump_all_map_tiles.py <PID>", file=sys.stderr)
    sys.exit(1)


JS = r"""
'use strict';

var g_base = Process.findModuleByName("libgame.so").base;
var MATRIX_VMA = 0x2f3f48;
var MAPSYSTEM_CHANGEMAP_VMA = 0x114fc4;

var matrixPtr = g_base.add(MATRIX_VMA).readPointer();
send("matrix base: " + matrixPtr);

rpc.exports = {
    changemap: function (mapId) {
        var fn = new NativeFunction(g_base.add(MAPSYSTEM_CHANGEMAP_VMA), 'void', ['int', 'int', 'int', 'int']);
        fn(mapId, 0, 0, 0);
    },
    dumpMatrix: function () {
        var bytes = Memory.readByteArray(matrixPtr, 64 * 64);
        return Array.from(new Uint8Array(bytes));
    },
};
"""


def main() -> None:
    dev = frida.get_device_manager().add_remote_device("127.0.0.1:27042")
    sess = dev.attach(PID)
    script = sess.create_script(JS)
    script.load()
    time.sleep(1)

    result: dict[str, dict] = {}
    errors: list[int] = []

    for map_id in range(416):
        try:
            script.exports.changemap(map_id)
            time.sleep(0.6)
            raw = script.exports.dump_matrix()
            tiles_b64 = base64.b64encode(bytes(raw)).decode("ascii")
            blocking = sum(1 for b in raw if b & 0x08)
            result[f"m{map_id}"] = {
                "mapId": map_id,
                "tiles": tiles_b64,
                "blockingCount": blocking,
            }
            if map_id % 20 == 0:
                print(f"  m{map_id} OK (blocking={blocking})", flush=True)
        except Exception as e:
            errors.append(map_id)
            print(f"  m{map_id} FAILED: {e}", flush=True)

    out_path = "apk/static-data/json/maps/tiles_frida.json"
    with open(out_path, "w") as f:
        json.dump(result, f, separators=(",", ":"))
    err_path = "apk/static-data/json/maps/tiles_frida_errors.json"
    with open(err_path, "w") as f:
        json.dump(errors, f)

    print(f"OK: {len(result)} maps dumped, {len(errors)} errors")
    sess.detach()


if __name__ == "__main__":
    main()

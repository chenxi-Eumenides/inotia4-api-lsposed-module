#!/usr/bin/env python3
"""批量 dump 多张图，对比 frida vs offline。

运行：uv run python scripts/analyze/dump_multi_maps.py <PID>
"""
from __future__ import annotations
import base64
import json
import sys
import time
import frida


PID = int(sys.argv[1]) if len(sys.argv) > 1 else None
if PID is None:
    print("Usage: uv run python scripts/analyze/dump_multi_maps.py <PID>", file=sys.stderr)
    sys.exit(1)


JS = r"""
'use strict';
var g_base = Process.findModuleByName("libgame.so").base;
var MATRIX_VMA = 0x2f3f48;
var WIDTH_VMA = 0x2f4e60;
var HEIGHT_VMA = 0x2f60d0;
var CURMAP_VMA = 0x2f4e80;
var CHANGEMAP_VMA = 0x114fc4;

var matrixPtr = g_base.add(MATRIX_VMA).readPointer();
var widthPtr = g_base.add(WIDTH_VMA).readPointer();
var heightPtr = g_base.add(HEIGHT_VMA).readPointer();
var curMapPtr = g_base.add(CURMAP_VMA).readPointer();

rpc.exports = {
    changemapAndRead: function (mapId) {
        var fn = new NativeFunction(g_base.add(CHANGEMAP_VMA), 'void', ['int', 'int', 'int', 'int']);
        fn(mapId, 0, 0, 0);
        Thread.sleep(0.8);
        var m = g_base.add(MATRIX_VMA).readPointer();
        return {
            width: widthPtr.readU32(),
            height: heightPtr.readU32(),
            curMap: curMapPtr.readU32(),
            matrix: Array.from(new Uint8Array(m.readByteArray(64 * 64))),
        };
    },
};
"""


def main() -> None:
    dev = frida.get_device_manager().add_remote_device("127.0.0.1:27042")
    sess = dev.attach(PID)
    script = sess.create_script(JS)
    script.load()
    time.sleep(0.5)

    with open("static-data/json/maps/tiles.json") as f:
        offline = json.load(f)

    test_maps = [0, 1, 31, 100, 200, 300, 415]
    results = {}
    for map_id in test_maps:
        try:
            r = script.exports.changemap_and_read(map_id)
            b = bytes(r["matrix"])
            fri = base64.b64encode(b).decode("ascii")
            results[f"m{map_id}"] = {
                "mapId": map_id,
                "width": r["width"],
                "height": r["height"],
                "tiles": fri,
            }
            print(f"m{map_id} OK (w={r['width']} h={r['height']})")
        except Exception as e:
            print(f"m{map_id} FAILED: {e}")

    with open("static-data/json/maps/tiles_frida_samples.json", "w") as f:
        json.dump(results, f, separators=(",", ":"))

    print()
    for map_id in test_maps:
        key = f"m{map_id}"
        if key not in results:
            continue
        fri_bytes = bytearray(base64.b64decode(results[key]["tiles"]))
        off_bytes = bytearray(base64.b64decode(offline[key]["tiles"]))
        diffs = sum(1 for a, b in zip(fri_bytes, off_bytes) if a != b)
        exit_only = sum(1 for a, b in zip(fri_bytes, off_bytes) if (a & 0x80) and not (b & 0x80))
        print(f"m{map_id:3d}: w={results[key]['width']:3d} h={results[key]['height']:3d} "
              f"diffs={diffs:4d} (exit-only={exit_only:3d}) "
              f"offline_w={offline[key]['width']:3d} h={offline[key]['height']:3d} blocking={offline[key]['blockingCount']}")

    sess.detach()


if __name__ == "__main__":
    main()

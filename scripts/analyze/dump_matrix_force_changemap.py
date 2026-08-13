#!/usr/bin/env python3
"""frida: dump 当前 matrix + 强制切图触发 MAP_Load 记录参数 + 重新 dump。

运行：uv run python scripts/analyze/dump_matrix_force_changemap.py <PID> [map_id]
"""
from __future__ import annotations
import base64
import json
import sys
import time
import frida


PID = int(sys.argv[1]) if len(sys.argv) > 1 else None
TARGET_MAP = int(sys.argv[2]) if len(sys.argv) > 2 else 31
if PID is None:
    print("Usage: uv run python scripts/analyze/dump_matrix_force_changemap.py <PID> [map_id]", file=sys.stderr)
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

send("matrix=" + matrixPtr + " widthPtr=" + widthPtr + " heightPtr=" + heightPtr + " curMapPtr=" + curMapPtr);
send("init width=" + widthPtr.readU32() + " height=" + heightPtr.readU32() + " curMap=" + curMapPtr.readU32());

rpc.exports = {
    dump: function () {
        var m = matrixPtr.readByteArray(64 * 64);
        return {
            width: widthPtr.readU32(),
            height: heightPtr.readU32(),
            curMap: curMapPtr.readU32(),
            matrix: Array.from(new Uint8Array(m)),
        };
    },
    changemapAndRead: function (mapId) {
        var before = {
            width: widthPtr.readU32(),
            height: heightPtr.readU32(),
            curMap: curMapPtr.readU32(),
            matrix: Array.from(new Uint8Array(matrixPtr.readByteArray(64 * 64))),
        };
        var fn = new NativeFunction(g_base.add(CHANGEMAP_VMA), 'void', ['int', 'int', 'int', 'int']);
        fn(mapId, 0, 0, 0);
        Thread.sleep(0.8);
        var after = {
            width: widthPtr.readU32(),
            height: heightPtr.readU32(),
            curMap: curMapPtr.readU32(),
            matrix: Array.from(new Uint8Array(matrixPtr.readByteArray(64 * 64))),
        };
        return { before: before, after: after };
    },
};
"""


def main() -> None:
    dev = frida.get_device_manager().add_remote_device("127.0.0.1:27042")
    sess = dev.attach(PID)
    script = sess.create_script(JS)
    script.load()
    time.sleep(0.5)

    result = script.exports.changemap_and_read(TARGET_MAP)
    print(f"=== Target mapId = {TARGET_MAP} ===")
    print(f"BEFORE: curMap={result['before']['curMap']} w={result['before']['width']} h={result['before']['height']}")
    print(f"AFTER:  curMap={result['after']['curMap']}  w={result['after']['width']} h={result['after']['height']}")
    print()

    m_before = bytes(result['before']['matrix'])
    m_after = bytes(result['after']['matrix'])
    print(f"Matrix diffs: {sum(1 for a, b in zip(m_before, m_after) if a != b)} / 4096")

    # Bit analysis of after matrix
    bit3 = sum(1 for b in m_after if b & 0x08)
    bit6 = sum(1 for b in m_after if b & 0x40)
    bit7 = sum(1 for b in m_after if b & 0x80)
    nonzero = sum(1 for b in m_after if b != 0)
    print(f"AFTER matrix: bit3={bit3} bit6={bit6} bit7={bit7} nonzero={nonzero}")

    # Save the after matrix as base64 (compatible with our JSON format)
    b64 = base64.b64encode(m_after).decode("ascii")
    out = {
        "mapId": TARGET_MAP,
        "width": result['after']['width'],
        "height": result['after']['height'],
        "tiles": b64,
        "blockingCount": bit3,
    }
    out_path = f"apk/static-data/json/maps/tiles_frida_m{TARGET_MAP}.json"
    with open(out_path, "w") as f:
        json.dump(out, f, indent=2)
    print(f"Saved: {out_path}")
    print()

    # Now also fetch via HTTP for comparison
    import urllib.request
    try:
        with urllib.request.urlopen("http://192.168.3.54:8088/api/info/current-map/tiles", timeout=5) as r:
            http_data = json.loads(r.read())
        http_bytes = bytearray(base64.b64decode(http_data["tiles"]))
        http_match = http_bytes == m_after
        print(f"HTTP /tiles matches frida: {http_match}")
    except Exception as e:
        print(f"HTTP fetch failed: {e}")

    sess.detach()


if __name__ == "__main__":
    main()

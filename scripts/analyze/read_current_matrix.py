#!/usr/bin/env python3
"""直接读 runtime matrix（不切图），与 offline 对比。"""
from __future__ import annotations
import base64
import json
import sys
import time
import frida


PID = int(sys.argv[1]) if len(sys.argv) > 1 else None
if PID is None:
    print("Usage: uv run python scripts/analyze/read_current_matrix.py <PID>", file=sys.stderr)
    sys.exit(1)


JS = r"""
'use strict';
var g_base = Process.findModuleByName('libgame.so').base;
var MATRIX_VMA = 0x2f3f48;
var WIDTH_VMA = 0x2f4e60;
var HEIGHT_VMA = 0x2f60d0;
var CURMAP_VMA = 0x2f4e80;
var widthPtr = g_base.add(WIDTH_VMA).readPointer();
var heightPtr = g_base.add(HEIGHT_VMA).readPointer();
var curMapPtr = g_base.add(CURMAP_VMA).readPointer();
rpc.exports = {
    read: function () {
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

    r = script.exports.read()
    b = bytes(r["matrix"])
    print(f"runtime: curMap={r['curMap']} w={r['width']} h={r['height']} "
          f"bit3={sum(1 for x in b if x & 0x08)} "
          f"bit6={sum(1 for x in b if x & 0x40)} "
          f"bit7={sum(1 for x in b if x & 0x80)} "
          f"nonzero={sum(1 for x in b if x != 0)}")

    with open("static-data/json/maps/tiles.json") as f:
        off = json.load(f)
    key = f"m{r['curMap']}"
    if key not in off:
        print(f"no offline data for {key}")
        return
    o = bytearray(base64.b64decode(off[key]["tiles"]))
    print(f"offline: w={off[key]['width']} h={off[key]['height']} "
          f"blocking={off[key]['blockingCount']} bit3={sum(1 for x in o if x & 0x08)} "
          f"nonzero={sum(1 for x in o if x != 0)}")

    diffs = [(i, a, bb) for i, (a, bb) in enumerate(zip(o, b)) if a != bb]
    print(f"diffs: {len(diffs)}/4096")
    if diffs:
        print("First 15 diffs (idx, off, run, diff_bits):")
        for i, a, bb in diffs[:15]:
            print(f"  [{i:4d}] (y={i//64:2d}, x={i%64:2d}) 0x{a:02x} 0x{bb:02x} {a^bb:08b}")

    sess.detach()


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""frida hook MAP_LoadBase，记录每次写入 matrix 的字节。

观察：matrix 字节的来源、width/height 实际值、tile ID 编码。

运行：uv run python scripts/analyze/hook_map_loadbase.py <PID>
"""
from __future__ import annotations
import sys
import frida


PID = int(sys.argv[1]) if len(sys.argv) > 1 else None
if PID is None:
    print("Usage: uv run python scripts/analyze/hook_map_loadbase.py <PID>", file=sys.stderr)
    sys.exit(1)


JS = r"""
'use strict';

var g_base = Process.findModuleByName("libgame.so").base;
var MATRIX_VMA = 0x2f3f48;
var WIDTH_VMA = 0x2f4e60;
var HEIGHT_VMA = 0x2f60d0;

var matrixPtr = g_base.add(MATRIX_VMA).readPointer();
var widthPtr = g_base.add(WIDTH_VMA);
var heightPtr = g_base.add(HEIGHT_VMA);

send("Matrix ptr: " + matrixPtr);
send("Current width: " + widthPtr.readU8() + ", height: " + heightPtr.readU8());

// Hook MAP_LoadBase (0x112060) - record parameters and matrix writes
var mapLoadBase = g_base.add(0x112060);

Interceptor.attach(mapLoadBase, {
    onEnter: function (args) {
        this.xStart = args[0].toInt32();
        this.yStart = args[1].toInt32();
        this.xEnd = args[2].toInt32();
        this.yEnd = args[3].toInt32();
        this.ptr = args[4];
        send("MAP_LoadBase called: xStart=" + this.xStart + " yStart=" + this.yStart +
             " xEnd=" + this.xEnd + " yEnd=" + this.yEnd +
             " expectedCells=" + (this.xEnd - this.xStart) * (this.yEnd - this.yStart));
    },
    onLeave: function (retval) {
        send("MAP_LoadBase returned: " + retval);
        // Read first 32 matrix bytes after the call
        var m = matrixPtr.readByteArray(64);
        var bytes = Array.from(new Uint8Array(m));
        var hex = bytes.map(function (b) { return ('0' + b.toString(16)).slice(-2); }).join(' ');
        send("Matrix row 0 (64 bytes): " + hex);
    }
});

// Hook the strb instruction inside the inner loop (0x112138) by hooking any write to matrixPtr
// Actually, hook the strb by attaching to MAP_LoadBase+offset
// Simpler: log matrix at intervals during the call

send("Hooks installed. Waiting for next map load...");
"""


def main() -> None:
    dev = frida.get_device_manager().add_remote_device("127.0.0.1:27042")
    sess = dev.attach(PID)
    script = sess.create_script(JS)
    script.on("message", lambda m, d: print(m.get("payload") if m.get("type") == "send" else m))
    script.load()

    import time
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        sess.detach()


if __name__ == "__main__":
    main()

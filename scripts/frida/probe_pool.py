import frida
import sys

PKG = "com.com2us.inotia4.normal.freefull.google.global.android.common"

js = r"""
function bytesToHex(arr) {
    var s = '';
    for (var i = 0; i < arr.length; i++) {
        var b = arr[i] & 0xff;
        s += (b < 16 ? '0' : '') + b.toString(16) + ' ';
    }
    return s;
}

var base = null;
Process.enumerateModules().forEach(function (m) {
    if (m.name === 'libgame.so') base = m.base;
});
console.log('base=' + base);
if (!base) { console.log('LIBGAME_NOT_FOUND'); }
else {
    var count = base.add(0x307528).readU32();
    var poolPtr = base.add(0x307530).readPointer();
    console.log('count=' + count + ' poolPtr=' + poolPtr);

    var n = Math.min(count, 12);
    for (var i = 0; i < n; i++) {
        var slot = poolPtr.add(i * 10);
        var bytes = slot.readByteArray(10);
        var u0 = slot.add(0).readU8();
        var u2 = slot.add(2).readU16();
        var u4 = slot.add(4).readU16();
        var b6 = slot.add(6).readU8();
        var b7 = slot.add(7).readU8();
        var b8 = slot.add(8).readU8();
        console.log('slot' + i + ' raw=[' + bytesToHex(bytes) + '] u0=' + u0 +
            ' u2=' + u2 + ' u4=' + u4 + ' b6=' + b6 + ' b7=' + b7 + ' b8=' + b8);
    }

    for (var m = 0; m < 3; m++) {
        var chPtr = base.add(0x728ec0 + m * 8).readPointer();
        if (!chPtr.isNull()) {
            var x = chPtr.add(0x02).readS16();
            var y = chPtr.add(0x04).readS16();
            var lvl = chPtr.add(0x0e).readS8();
            console.log('member' + m + ' ptr=' + chPtr + ' x=' + x + ' y=' + y + ' lvl=' + lvl);
        } else {
            console.log('member' + m + ' null');
        }
    }
}
"""


def main():
    dev = frida.get_usb_device(timeout=10)
    pid = int(sys.argv[1]) if len(sys.argv) > 1 else None
    session = dev.attach(pid if pid else PKG)
    script = session.create_script(js)
    script.on("message", lambda msg, data: print(msg["payload"] if msg["type"] == "send" else msg))
    script.load()
    sys.stdin.read()


if __name__ == "__main__":
    main()

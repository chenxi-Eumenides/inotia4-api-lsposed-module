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
if (!base) { console.log('LIBGAME_NOT_FOUND'); }
else {
    // CHARSYSTEM_pPool @0x307538（角色对象池）
    var p = base.add(0x307538).readPointer();
    console.log('CHARSYSTEM_pPool ptr=' + p);
    if (!p.isNull()) {
        var raw = p.readByteArray(64);
        console.log('raw64=' + bytesToHex(raw));
        // 直接按角色结构读 p 本身（可能是对象数组起点）
        try {
            var x = p.add(0x02).readS16();
            var y = p.add(0x04).readS16();
            var t = p.add(0x09).readU8();
            console.log('as-char: x=' + x + ' y=' + y + ' type=' + t);
        } catch (e) { console.log('as-char failed: ' + e); }
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

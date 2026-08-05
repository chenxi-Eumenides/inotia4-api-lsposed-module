import frida
import sys

PKG = "com.com2us.inotia4.normal.freefull.google.global.android.common"

js = r"""
function hex(arr) {
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
    // 两个候选：GOT 0x2f6010 与符号 MERCENARYSYSTEM_pSlotList 0x307750
    var p1 = base.add(0x2f6010).readPointer();
    var p2 = base.add(0x307750).readPointer();
    console.log('*(0x2f6010)=' + p1);
    console.log('*(0x307750)=' + p2);
    [p1, p2].forEach(function (p, idx) {
        if (p.isNull()) return;
        console.log('=== candidate' + idx + ' ' + p + ' raw 0x40 ===');
        console.log(hex(new Uint8Array(p.readByteArray(0x40))));
        // 槽 19（佣兵关联）
        var s19 = p.add(19 * 0x14);
        console.log('slot19 raw: ' + hex(new Uint8Array(s19.readByteArray(0x14))));
    });
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

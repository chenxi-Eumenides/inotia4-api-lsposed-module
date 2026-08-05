import frida
import sys

PKG = "com.com2us.inotia4.normal.freefull.google.global.android.common"

js = r"""
var base = null;
Process.enumerateModules().forEach(function (m) {
    if (m.name === 'libgame.so') base = m.base;
});
if (!base) { console.log('LIBGAME_NOT_FOUND'); }
else {
    // INVEN_pItem 槽数组，dump 前几个物品的原始结构
    var inven = base.add(0x7131c0);
    var seen = {};
    var n = 0;
    for (var b = 0; b < 6 && n < 8; b++) {
        var bag = inven.add(b * 0x80);
        for (var j = 0; j < 16 && n < 8; j++) {
            var item = bag.add(j * 8).readPointer();
            if (item.isNull()) continue;
            var flags = item.add(0x08).readU16();
            if (seen[flags]) continue;
            seen[flags] = 1;
            n++;
            var bytes = item.readByteArray(0x30);
            var hex = '';
            var u8arr = new Uint8Array(bytes);
            for (var k = 0; k < u8arr.length; k++) {
                hex += (u8arr[k] < 16 ? '0' : '') + u8arr[k].toString(16) + ' ';
            }
            console.log('item flags=' + flags + ' raw=[' + hex + ']');
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

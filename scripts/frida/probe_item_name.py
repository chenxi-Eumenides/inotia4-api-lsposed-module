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
    var getBit = new NativeFunction(base.add(0x140528), 'int', ['int', 'int', 'int']);
    var getName = new NativeFunction(base.add(0x106bb4), 'int', ['pointer', 'pointer']);
    var inven = base.add(0x7131c0);
    var seen = {};
    var n = 0;
    for (var b = 0; b < 6 && n < 10; b++) {
        var bag = inven.add(b * 0x80);
        for (var j = 0; j < 16 && n < 10; j++) {
            var item = bag.add(j * 8).readPointer();
            if (item.isNull()) continue;
            var flags = item.add(0x08).readU16();
            if (seen[flags]) continue;
            seen[flags] = 1;
            n++;
            var category = getBit(flags, 15, 6);
            var nameId = item.add(0x0c).readU16();
            // 调用 ITEM_GetName
            var buf = Memory.alloc(256);
            try {
                getName(item, buf);
                var realName = buf.readUtf8String();
            } catch (e) {
                var realName = 'ERR:' + e;
            }
            console.log('flags=' + flags + ' category=' + category +
                ' nameId(+0x0c)=' + nameId + ' GAME_NAME=' + JSON.stringify(realName));
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

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
    var getAttr = new NativeFunction(base.add(0xdfd18), 'int', ['pointer', 'int']);
    var hero = base.add(0x728ec0).readPointer();
    console.log('hero=' + hero);
    for (var id = 0; id < 32; id++) {
        var v = getAttr(hero, id);
        var direct = hero.add(0x24 + id * 4).readS32();
        console.log('attr id=' + id + ' GetAttr=' + v + ' direct+0x24=' + direct);
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

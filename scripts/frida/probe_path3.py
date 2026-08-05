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
    var searchPath = new NativeFunction(base.add(0xdb094), 'int', ['pointer', 'int', 'int', 'int']);
    var hero = base.add(0x728ec0).readPointer();
    var hx = hero.add(2).readS16();
    var hy = hero.add(4).readS16();
    var tx = hx - 120, ty = hy - 120;
    var ret = searchPath(hero, tx, ty, 1);
    console.log('search(' + tx + ',' + ty + ')=' + ret);
    var node = hero.add(0x2f0).readPointer();
    var cnt = 0;
    while (!node.isNull() && cnt < 60) {
        var nx = node.add(0x0c).readU8();
        var ny = node.add(0x0d).readU8();
        console.log('  (' + nx + ',' + ny + ')');
        node = node.add(0x18).readPointer();
        cnt++;
    }
    console.log('total nodes: ' + cnt);
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

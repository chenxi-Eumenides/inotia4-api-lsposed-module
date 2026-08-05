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
    var isBlocking = new NativeFunction(base.add(0x113bcc), 'int', ['int', 'int']);
    var hero = base.add(0x728ec0).readPointer();
    var hx = hero.add(2).readS16();
    var hy = hero.add(4).readS16();
    console.log('hero at ' + hx + ',' + hy);
    var targets = [];
    for (var dx = -120; dx <= 120; dx += 40) {
        for (var dy = -120; dy <= 120; dy += 40) {
            if (dx === 0 && dy === 0) continue;
            targets.push([hx + dx, hy + dy]);
        }
    }
    var found = 0;
    for (var t = 0; t < targets.length && found < 3; t++) {
        var tx = targets[t][0], ty = targets[t][1];
        var blocking = isBlocking(tx, ty);
        var ret = searchPath(hero, tx, ty, 1);
        var head = hero.add(0x2f0).readPointer();
        if (ret === 1 && !head.isNull()) {
            console.log('SUCCESS target=' + tx + ',' + ty + ' block=' + blocking + ' pathHead=' + head);
            found++;
        }
    }
    if (found === 0) {
        console.log('no success; sample blocking at few points:');
        for (var i = 0; i < 5; i++) {
            var tx = hx + (i - 2) * 40, ty = hy + 40;
            console.log('  (' + tx + ',' + ty + ') blocking=' + isBlocking(tx, ty) + ' search=' + searchPath(hero, tx, ty, 1));
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

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
    var hero = base.add(0x728ec0).readPointer();
    var getStat = new NativeFunction(base.add(0xdf8d0), 'int', ['pointer', 'int']);
    var getStatMain = new NativeFunction(base.add(0xdb9f0), 'int', ['pointer', 'int']);
    var getStatBase = new NativeFunction(base.add(0xdb9e4), 'int', ['pointer', 'int']);
    var getStatSub = new NativeFunction(base.add(0xdf888), 'int', ['pointer', 'int']);
    var getStatusPoint = new NativeFunction(base.add(0xd9c44), 'int', ['pointer']);
    console.log('hero=' + hero);
    console.log('GetStatusPoint=' + getStatusPoint(hero));
    console.log('--- CHAR_GetStat ---');
    for (var id = 0; id < 8; id++) {
        console.log('GetStat(id=' + id + ')=' + getStat(hero, id));
    }
    console.log('--- GetStatMain/Base/Sub (0..4) ---');
    for (var i = 0; i < 5; i++) {
        console.log('i=' + i + ' Main=' + getStatMain(hero, i) + ' Base=' + getStatBase(hero, i) + ' Sub=' + getStatSub(hero, i));
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

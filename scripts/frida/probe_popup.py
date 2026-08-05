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
    var getCount = new NativeFunction(base.add(0x9db00), 'int', ['pointer']);
    var getData = new NativeFunction(base.add(0x9dabc), 'pointer', ['pointer', 'int']);
    var arr = base.add(0x3024c8);
    var n = getCount(arr);
    console.log('popup array count=' + n);
    for (var i = 0; i < n && i < 10; i++) {
        var el = getData(arr, i);
        if (el.isNull()) continue;
        var popup = el.readU64();
        var param = el.add(8).readU64();
        console.log('popup[' + i + '] id=' + popup + ' param=' + param);
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

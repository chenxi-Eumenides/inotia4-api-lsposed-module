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
    var getName = new NativeFunction(base.add(0xd9c54), 'pointer', ['pointer']);
    var hero = base.add(0x728ec0).readPointer();
    var namePtr = getName(hero);
    var name = '';
    try { name = namePtr.readUtf8String(); } catch (e) { name = 'ERR:' + e; }
    console.log('hero name=' + JSON.stringify(name));
    // 佣兵（角色池找 +0x352>=0）
    var pool = base.add(0x307538).readPointer();
    for (var j = 0; j < 128; j++) {
        var obj = pool.add(j * 0x430);
        try {
            var merc = obj.add(0x352).readS8();
            var status = obj.add(0x311).readU8();
            if (merc >= 0 && status <= 2) {
                var nm = '';
                try { nm = getName(obj).readUtf8String(); } catch (e) { nm = 'ERR'; }
                console.log('char obj' + j + ' slot=' + merc + ' status=' + status + ' name=' + JSON.stringify(nm));
            }
        } catch (e) { break; }
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

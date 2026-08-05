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
    var p = base.add(0x307538).readPointer();
    console.log('hero=' + p);
    var STEP = 0x430;
    // 从 hero 向后扫描 128 个对象，校验坐标合理性
    var found = 0;
    for (var i = 0; i < 128; i++) {
        var obj = p.add(i * STEP);
        try {
            var x = obj.add(0x02).readS16();
            var y = obj.add(0x04).readS16();
            var t = obj.add(0x09).readU8();
            var st = obj.add(0x311).readU8();
            // 坐标合理范围过滤（0-2000），type/st 合理
            if (x >= 0 && x < 2000 && y >= 0 && y < 2000 && st < 8) {
                console.log('+' + (i * STEP) + ' obj=' + obj + ' x=' + x + ' y=' + y + ' type=' + t + ' status=' + st);
                found++;
            }
        } catch (e) { break; }
    }
    console.log('found=' + found);
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

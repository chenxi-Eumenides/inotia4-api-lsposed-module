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
    var addr = base.add(0xdfd18);
    Interceptor.attach(addr, {
        onEnter: function (args) {
            this.attrId = args[1].toInt32();
        },
        onLeave: function (retval) {
            console.log('CHAR_GetAttr(id=' + this.attrId + ') = ' + retval.toInt32());
        }
    });
    console.log('hooked CHAR_GetAttr @' + addr + ', 请重新打开属性面板');
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

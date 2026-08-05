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
    // hook ASTAR_GeneratePath 捕获 CHAR_SearchPath 的真实调用参数
    Interceptor.attach(base.add(0xd93e4), {
        onEnter: function (args) {
            var astar = args[0];
            this.astar = astar;
            console.log('GeneratePath called: x0=' + astar +
                ' w1(sx)=' + args[1].toInt32() +
                ' w2(sy)=' + args[2].toInt32() +
                ' w3(ex)=' + args[3].toInt32() +
                ' w4(ey)=' + args[4].toInt32() +
                ' w5=' + args[5].toInt32() +
                ' x6=' + args[6]);
            console.log('  astar+0x28=' + astar.add(0x28).readPointer() +
                ' +0x30=' + astar.add(0x30).readPointer() +
                ' +0x38=' + astar.add(0x38).readU8() +
                ' +0x3c=' + astar.add(0x3c).readS32() +
                ' +0x40=' + astar.add(0x40).readS32());
        },
        onLeave: function (retval) {
            console.log('  GeneratePath ret=' + retval.toInt32());
        }
    });
    // 触发一次 CHAR_SearchPath
    var searchPath = new NativeFunction(base.add(0xdb094), 'int', ['pointer', 'int', 'int', 'int']);
    var hero = base.add(0x728ec0).readPointer();
    var hx = hero.add(2).readS16();
    var hy = hero.add(4).readS16();
    console.log('calling CHAR_SearchPath(' + (hx - 120) + ',' + (hy - 120) + ',1)');
    var r = searchPath(hero, hx - 120, hy - 120, 1);
    console.log('CHAR_SearchPath ret=' + r);
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

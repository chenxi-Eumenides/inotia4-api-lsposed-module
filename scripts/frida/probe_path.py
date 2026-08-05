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
    console.log('hero=' + hero + ' x=' + hero.add(2).readS16() + ' y=' + hero.add(4).readS16());
    // 目标点（像素）：角色附近可走点
    var tx = hero.add(2).readS16() + 80;
    var ty = hero.add(4).readS16() + 80;
    console.log('search path to ' + tx + ',' + ty);
    var ret = searchPath(hero, tx, ty, 1);
    console.log('CHAR_SearchPath ret=' + ret);
    // 读角色 +0x2F0 路径
    var pathList = hero.add(0x2f0).readPointer();
    console.log('pathList head=' + pathList);
    var node = pathList;
    var cnt = 0;
    while (!node.isNull() && cnt < 40) {
        var nx = node.add(0x0c).readU8();
        var ny = node.add(0x0d).readU8();
        var next = node.add(0x18).readPointer();
        console.log('  node ' + cnt + ' (' + nx + ',' + ny + ') next=' + next);
        node = next;
        cnt++;
    }
    if (cnt === 0) console.log('  (empty path)');
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

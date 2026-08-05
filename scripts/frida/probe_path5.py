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
    var genPath = new NativeFunction(base.add(0xd93e4), 'int', ['pointer', 'int', 'int', 'int', 'int', 'int', 'int']);
    var hero = base.add(0x728ec0).readPointer();
    var hx = hero.add(2).readS16();
    var hy = hero.add(4).readS16();
    // 地图指针与宽度（复刻 CHAR_SearchPath db148/db168）
    var map = base.add(0x2f4e60).readPointer();
    var width = map.isNull() ? 0 : map.readS32();
    var cb = base.add(0x2f5450).readPointer();   // +0x28 碰撞回调
    var cb2 = base.add(0x2f3c80).readPointer();  // +0x30
    console.log('cb=' + cb + ' cb2=' + cb2);
    // 构造 ASTAR 对象（0x60 字节）
    var astar = Memory.alloc(0x60);
    Memory.protect(astar, 0x60, 'rwx');
    astar.writeByteArray(new Array(0x60).fill(0));
    astar.add(0x28).writePointer(cb);
    astar.add(0x30).writePointer(cb2);
    astar.add(0x38).writeU8((width * 2 + 1) & 0xff);
    astar.add(0x3c).writeS32(0);
    astar.add(0x40).writeS32(0);
    var tx = hx - 120, ty = hy - 120;
    var ret = genPath(astar, hx >> 3, hy >> 3, tx >> 3, ty >> 3, 1, 1);
    console.log('GeneratePath ret=' + ret + ' width=' + width);
    var node = astar.add(0x18).readPointer();
    var cnt = 0;
    while (!node.isNull() && cnt < 80) {
        var nx = node.add(0x0c).readU8();
        var ny = node.add(0x0d).readU8();
        console.log('  (' + nx + ',' + ny + ')');
        node = node.add(0x18).readPointer();
        cnt++;
    }
    console.log('total: ' + cnt);
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

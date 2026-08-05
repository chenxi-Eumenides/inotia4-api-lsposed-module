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
    for (var m = 0; m < 3; m++) {
        var chPtr = base.add(0x728ec0 + m * 8).readPointer();
        if (chPtr.isNull()) { console.log('member' + m + ' null'); continue; }
        // 技能链表头 +0x2A0
        var head = chPtr.add(0x2A0).readPointer();
        var bmp = chPtr.add(0x2B0).readU16();
        var skillPoints = chPtr.add(0x328).readS8();
        var activePtr = chPtr.add(0x280).readPointer();
        var activeId = activePtr.isNull() ? -1 : activePtr.add(0).readU16();
        console.log('member' + m + ' head=' + head + ' bmp=0x' + bmp.toString(16) +
            ' pts=' + skillPoints + ' active=' + activeId);
        // 遍历技能链表
        var node = head;
        var cnt = 0;
        while (!node.isNull() && cnt < 32) {
            var id = node.add(0).readU16();
            var lvl = node.add(2).readU8();
            var next = node.add(0x18).readPointer();
            console.log('  skill actionId=' + id + ' level=' + lvl + ' next=' + next);
            node = next;
            cnt++;
            if (cnt > 30) console.log('  ...loop guard');
        }
        if (cnt === 0) console.log('  (empty list)');
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

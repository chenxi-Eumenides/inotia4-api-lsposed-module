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
    var slots = base.add(0x2f6010).readPointer().readPointer();
    var maxSlots = base.add(0x2f3978).readS8();
    console.log('slots=' + slots + ' maxSlots=' + maxSlots);
    // 角色池
    var hero = base.add(0x307538).readPointer();
    var chars = [];
    for (var j = 0; j < 128; j++) {
        var obj = hero.add(j * 0x430);
        try {
            var merc = obj.add(0x352).readS8();
            if (merc >= 0) {
                chars.push({slot: merc, nameId: obj.add(0x0a).readU16(), lvl: obj.add(0x0e).readS8()});
            }
        } catch (e) { break; }
    }
    console.log('chars:', JSON.stringify(chars));
    for (var i = 0; i < maxSlots; i++) {
        var slot = slots.add(i * 0x14);
        var type = slot.add(0).readU8();
        var u1 = slot.add(1).readU8();
        var u2 = slot.add(2).readU16();
        var flags = slot.add(0x0b).readU8();
        var c0 = slot.add(0x0c).readS16();
        var c2 = slot.add(0x0e).readS16();
        if (flags & 1) {
            var ch = chars.find(function (x) { return x.slot === i; });
            console.log('slot' + i + ' type=' + type + ' u1=' + u1 + ' u2=' + u2 +
                ' flags=0x' + flags.toString(16) + ' char0=' + c0 + ' char2=' + c2 +
                ' -> nameId=' + (ch ? ch.nameId : '?') + ' lvl=' + (ch ? ch.lvl : '?'));
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

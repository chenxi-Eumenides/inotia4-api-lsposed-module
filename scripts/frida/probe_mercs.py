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
    // 未上场佣兵槽：*(0x2f6010) 指针数组（20B/槽），上限 *(0x2f3978) s8
    var slotArr = base.add(0x2f6010).readPointer();
    var maxSlots = base.add(0x2f3978).readS8();
    console.log('slotArr=' + slotArr + ' maxSlots=' + maxSlots);
    if (!slotArr.isNull()) {
        for (var i = 0; i < 32; i++) {
            var slot = slotArr.add(i * 0x14);
            var type = slot.add(0).readU8();
            var memberIdx = slot.add(1).readU8();
            var nameId = slot.add(2).readU16();
            var flags = slot.add(0x0b).readU8();
            var extra0c = slot.add(0x0c).readS16();
            var extra0e = slot.add(0x0e).readS16();
            console.log('slot' + i + ' type=' + type + ' idx=' + memberIdx +
                ' nameId=' + nameId + ' flags=0x' + flags.toString(16) +
                ' extra=' + extra0c + ',' + extra0e);
        }
    }
    // 遍历角色池找 +0x352 佣兵槽关联
    var hero = base.add(0x307538).readPointer();
    for (var j = 0; j < 128; j++) {
        var obj = hero.add(j * 0x430);
        try {
            var merc = obj.add(0x352).readS8();
            var status = obj.add(0x311).readU8();
            if (merc >= 0 || status <= 2) {
                var x = obj.add(0x02).readS16();
                var y = obj.add(0x04).readS16();
                console.log('char obj' + j + ' mercSlot=' + merc + ' status=' + status + ' x=' + x + ' y=' + y);
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

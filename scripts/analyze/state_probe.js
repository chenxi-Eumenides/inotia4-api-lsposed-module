// state_probe.js - 快速读状态
var base = null;
Process.enumerateModules().forEach(function(m){ if (m.name === "libgame.so") base = m.base; });
if (!base) { console.log("NO_LIBGAME"); } else {
    var state = base.add(0x307492).readU16();          // G_STATE_VMA
    var popupOn = base.add(0x2f5000 + 0xf8).readPointer(); // STATE GOT
    console.log("base=" + base + " state=" + state + " popupStackAddr=" + base.add(0x728fd8));
    try {
        var stk = base.add(0x728fd8).readPointer();
        console.log("popupStack=" + stk + " count=" + stk.add(8).readU32());
        var cnt = stk.add(8).readU32();
        if (cnt > 0 && cnt < 27) {
            var top = stk.add(0x18).readPointer().add((cnt-1)*0x40);
            console.log("topEnter=" + top.add(0x10).readPointer().sub(base));
        }
    } catch(e) { console.log("stk err " + e); }
    // 主菜单/选角相关
    var sel = base.add(0x308080 + 0x8).readU32();
    console.log("selClass=" + sel);
    var resume = base.add(0x2f6000 + 0x8).readPointer();
    console.log("resumeFlag=" + (resume.isNull() ? "null" : resume.readU8()));
    var curSlot = base.add(0x2f4000 + 0xd20).readPointer();
    console.log("curSlot=" + (curSlot.isNull() ? "null" : curSlot.readU8()));
    var prodClass = base.add(0x2f5000 + 0xa00).readPointer();
    console.log("prodClass=" + (prodClass.isNull() ? "null" : prodClass.readU8()));
}

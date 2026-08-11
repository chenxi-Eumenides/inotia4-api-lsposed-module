// frida 探针：npc_quest 面板 state（三层解引用）与任务文本
var base = null;
Process.enumerateModules().forEach(function (m) {
    if (m.path.indexOf("libgame.so") >= 0 && base === null) base = m.base;
});
send("base=" + base);

function rdp(off) { try { return base.add(off).readPointer(); } catch (e) { return ptr(0); } }
function rd8(off) { try { return base.add(off).readU8(); } catch (e) { return -1; } }

setInterval(function () {
    var out = "=== npcquest ===";
    var idxp = rdp(0x2f3240);
    var idx = -1;
    try { if (!idxp.isNull()) idx = idxp.readS16(); } catch (e) {}
    out += " questIdx=" + idx;
    var stbl = rdp(0x2f6b40);
    var stbl2 = stbl.isNull() ? ptr(0) : stbl.readPointer();
    out += " | stateTbl2=" + stbl2;
    if (idx >= 0 && !stbl2.isNull()) { try { out += " state[" + idx + "]=" + stbl2.add(idx).readU8(); } catch (e) {} }
    var slots = rdp(0x2f42f0);
    var slots2 = slots.isNull() ? ptr(0) : slots.readPointer();
    out += " | slots2=" + slots2;
    if (!slots2.isNull() && idx >= 0) {
        try { out += " qslot0=" + slots2.readU16(); } catch (e) {}
    }
    send(out);
}, 3000);

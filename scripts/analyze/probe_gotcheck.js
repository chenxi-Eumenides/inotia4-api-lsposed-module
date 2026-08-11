// frida 探针：检查 ChangeQuestState 内部依赖的 GOT 指针是否有效
var base = null;
Process.enumerateModules().forEach(function (m) {
    if (m.path.indexOf("libgame.so") >= 0 && base === null) base = m.base;
});
send("base=" + base);

function rdp(off) { try { return base.add(off).readPointer(); } catch (e) { return ptr(0); } }
function rd16(off) { try { return base.add(off).readU16(); } catch (e) { return -1; } }
function rd8(off) { try { return base.add(off).readU8(); } catch (e) { return -1; } }

setInterval(function () {
    var out = "=== got check ===";
    var qcnt_got = rdp(0x2f6000 + 0xe08);
    out += " questCntGOT@0x2f6e08=" + qcnt_got;
    if (!qcnt_got.isNull()) out += " val=" + qcnt_got.readU16();
    var stbl_got = rdp(0x2f6000 + 0xb40);
    out += " | stateTblGOT@0x2f6b40=" + stbl_got;
    if (!stbl_got.isNull()) {
        var stbl = stbl_got.readPointer();
        out += " tbl=" + stbl;
        if (!stbl.isNull()) { try { out += " st[0]=" + stbl.readU8() + " st[381]=" + stbl.add(381).readU8(); } catch (e) {} }
    }
    var slots_got = rdp(0x2f4000 + 0x2f0);
    out += " | slotsGOT@0x2f42f0=" + slots_got;
    if (!slots_got.isNull()) out += " base=" + slots_got.readPointer();
    var qc_got = rdp(0x2f5000 + 0x400);
    out += " | qcGOT@0x2f5400=" + qc_got;
    if (!qc_got.isNull()) out += " val=" + qc_got.readU8();
    send(out);
}, 3000);

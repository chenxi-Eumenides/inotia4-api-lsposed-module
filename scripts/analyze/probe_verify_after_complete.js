// frida 探针：验证任务 381 完成后 stateTbl/槽数组状态
var base = null;
Process.enumerateModules().forEach(function (m) {
    if (m.path.indexOf("libgame.so") >= 0 && base === null) base = m.base;
});
send("base=" + base);

function rdp(off) { try { return base.add(off).readPointer(); } catch (e) { return ptr(0); } }
function rd8(off) { try { return base.add(off).readU8(); } catch (e) { return -1; } }

setInterval(function () {
    var out = "=== quest state ===";
    var stbl = rdp(0x2f6b40);
    var stbl2 = stbl.isNull() ? ptr(0) : stbl.readPointer();
    if (!stbl2.isNull()) {
        out += " st381=" + stbl2.add(381).readU8();
        out += " st21=" + stbl2.add(21).readU8();
        out += " st180=" + stbl2.add(180).readU8();
    }
    var qs = rdp(0x2f43d0);
    var qs2 = qs.isNull() ? ptr(0) : qs.readPointer();
    var qc = rdp(0x2f6270);
    var count = qc.isNull() ? -1 : qc.readU8();
    out += " | count=" + count;
    if (!qs2.isNull()) {
        for (var i = 0; i < count && i < 8; ++i) { try { out += " [" + i + "]=" + qs2.add(i * 12).readU16(); } catch (e) {} }
    }
    send(out);
}, 3000);

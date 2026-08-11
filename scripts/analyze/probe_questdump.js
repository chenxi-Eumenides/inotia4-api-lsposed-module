// frida 探针：dump QUEST 槽数组（双层解引用，12B/槽）+0 questId
var base = null;
Process.enumerateModules().forEach(function (m) {
    if (m.path.indexOf("libgame.so") >= 0 && base === null) base = m.base;
});
send("base=" + base);

function rdp(off) { try { return base.add(off).readPointer(); } catch (e) { return ptr(0); } }

setInterval(function () {
    var out = "=== quest dump ===";
    var qc = rdp(0x2f6270);
    var qs_got = rdp(0x2f43d0);
    var qs = qs_got.isNull() ? ptr(0) : qs_got.readPointer();
    if (!qs.isNull()) {
        var count = qc.isNull() ? -1 : qc.readU8();
        out += " count=" + count;
        for (var i = 0; i < count && i < 16; ++i) {
            var qid = 0;
            try { qid = qs.add(i * 12).readU16(); } catch (e) {}
            out += " [" + i + "]=" + qid;
        }
    } else {
        out += " slots=null";
    }
    send(out);
}, 3000);

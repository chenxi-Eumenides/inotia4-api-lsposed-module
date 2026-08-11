// frida 探针：attach 后调用一次 UINpcQuest_ButtonOKExe(0xc3414)，然后轮询 quest 状态
var base = null;
Process.enumerateModules().forEach(function (m) {
    if (m.path.indexOf("libgame.so") >= 0 && base === null) base = m.base;
});
send("base=" + base);

function rdp(off) { try { return base.add(off).readPointer(); } catch (e) { return ptr(0); } }
function rd8(off) { try { return base.add(off).readU8(); } catch (e) { return -1; } }
function rd16(off) { try { return base.add(off).readU16(); } catch (e) { return -1; } }

var called = false;
setInterval(function () {
    if (!called) {
        called = true;
        send("CALLING UINpcQuest_ButtonOKExe @0xc3414");
        try {
            var fn = new NativeFunction(base.add(0xc3414), "int", []);
            var r = fn();
            send("return=" + r);
        } catch (e) {
            send("call error: " + e);
        }
    }
    var out = "=== post ===";
    var aq = rd16(0x728ff8);
    out += " activeQuest=" + aq;
    var qc = rdp(0x2f6270);
    var count = qc.isNull() ? -1 : qc.readU8();
    out += " questCount=" + count;
    var qs = rdp(0x2f43d0);
    var qs2 = qs.isNull() ? ptr(0) : qs.readPointer();
    if (!qs2.isNull()) {
        for (var i = 0; i < count; ++i) { try { out += " [" + i + "]=" + qs2.add(i * 12).readU16(); } catch (e) {} }
    }
    var stbl = rdp(0x2f6b40);
    var stbl2 = stbl.isNull() ? ptr(0) : stbl.readPointer();
    if (!stbl2.isNull()) { try { out += " state381=" + stbl2.add(381).readU8(); } catch (e) {} }
    send(out);
}, 2500);

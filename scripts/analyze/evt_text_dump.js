// frida：dump 剧情对话文本缓冲区原始字节 + hook TEXTCTRL2_Draw / EVTSYSTEM_DrawDialog 抓文本
// 用法：timeout 300 uv run frida -U -p <PID> -q -l scripts/analyze/evt_text_dump.js
var base = null;
Process.enumerateModules().forEach(function (m) {
    if (m.path.indexOf("libgame.so") >= 0 && base === null) base = m.base;
});
if (base === null) { send("ERROR: libgame.so not found"); }

function hexDump(p, n) {
    if (p === null || p.isNull()) return "NULL";
    try {
        var bytes = p.readByteArray(n);
        var arr = new Uint8Array(bytes);
        var out = [];
        for (var i = 0; i < arr.length; i++) {
            out.push(("0" + arr[i].toString(16)).slice(-2));
            if ((i + 1) % 16 === 0) out.push("|");
        }
        return out.join(" ");
    } catch (e) { return "<err:" + e + ">"; }
}

function probe() {
    var pText = base.add(0x3075d0).readPointer();
    var pTeller = base.add(0x713028).readPointer();
    var tc = base.add(0x713050);
    var out = [];
    out.push("pText=" + pText + " hex: " + hexDump(pText, 96));
    out.push("pTeller=" + pTeller + " hex: " + hexDump(pTeller, 64));
    for (var i = 0; i < 4; i++) {
        var tp = tc.add(i * 8).readPointer();
        out.push("TC[" + i + "]=" + tp + " hex: " + hexDump(tp, 64));
    }
    // 尝试 UTF-16 读取
    try { if (pText !== null && !pText.isNull()) out.push("pText utf16le: " + pText.readUtf16String(80)); } catch (e) {}
    try { if (pTeller !== null && !pTeller.isNull()) out.push("pTeller utf16le: " + pTeller.readUtf16String(40)); } catch (e) {}
    send(out.join("\n"));
}

send("base=" + base);
probe();
setInterval(probe, 800);

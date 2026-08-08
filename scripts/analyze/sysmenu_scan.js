// P0-c: SYSTEMMENU 结构探索（纯内存轮询，无 Interceptor hook，避免 WebView 崩溃）
// 用法: uv run frida -U -p <PID> -l scripts/analyze/sysmenu_scan.js
// 附加后打开选项面板(面板按钮→选项)，观察输出

var base = null;
Process.enumerateModules().forEach(function (m) {
    if (m.path.indexOf("libgame.so") >= 0 && base === null) base = m.base;
});

var REGIONS = [
    { start: 0x307000, len: 0x3000 },
    { start: 0x711c00, len: 0x1200 },
    { start: 0x728c00, len: 0x1000 },
    { start: 0x2f9f00, len: 0x800 },
    { start: 0x712500, len: 0x400 },
];

function dumpRegion(start, len) {
    var out = [];
    try {
        var addr = base.add(start);
        var bytes = addr.readByteArray(len);
        var arr = new Uint8Array(bytes);
        for (var i = 0; i < len; i++) {
            if (arr[i] !== 0) out.push((start + i).toString(16) + ":" + arr[i].toString(16));
        }
    } catch (e) {}
    return out;
}

var snapshot = null;
var snapshotTime = 0;

setInterval(function () {
    var cur = {};
    REGIONS.forEach(function (r) {
        cur[r.start] = dumpRegion(r.start, r.len);
    });
    if (snapshot !== null) {
        var changed = false;
        REGIONS.forEach(function (r) {
            var a = snapshot[r.start], b = cur[r.start];
            if (a.length !== b.length) { changed = true; return; }
            for (var i = 0; i < a.length; i++) {
                if (a[i] !== b[i]) { changed = true; break; }
            }
        });
        if (changed) {
            var t = Date.now();
            console.log("[变化 @" + (t - snapshotTime) + "ms]");
            REGIONS.forEach(function (r) {
                var a = snapshot[r.start], b = cur[r.start];
                var set = {}, all = {};
                b.forEach(function (x) { set[x] = 1; all[x] = 1; });
                a.forEach(function (x) { if (!all[x]) set[x] = 0; });
                Object.keys(set).forEach(function (k) {
                    var st = set[k] ? "新增" : "消失";
                    console.log("  " + r.start.toString(16) + " " + st + " 0x" + k.split(":")[0] + "=" + k.split(":")[1]);
                });
            });
            snapshot = cur;
            snapshotTime = t;
        }
    } else {
        snapshot = cur;
        snapshotTime = Date.now();
        console.log("基线已建立, base=" + base);
    }
}, 1000);

// frida：非 hook 扫描 UI 内存窗口，检测面板切换时地址变化
// 用法：timeout 300 uv run frida -U -p <PID> -q -l scripts/analyze/ui_mem_scan.js
// 不调用 Interceptor，只用 setInterval 轮询读内存，避免崩溃

var base = null;
Process.enumerateModules().forEach(function (m) {
    if (m.path.indexOf("libgame.so") >= 0 && base === null) base = m.base;
});

// 扫描区间：覆盖 .data/.bss 中 UI 状态变量集中区域
var REGIONS = [
    { start: 0x307000, len: 0x3000 },   // STATE/POPUP 区
    { start: 0x2f9f00, len: 0x800 },    // g_sPopupStateList 区
    { start: 0x711c00, len: 0x1000 },   // UIPopupMsg/UIQuestMenu/UIStore 区
    { start: 0x728c00, len: 0x1000 },   // PARTY/QUESTSYSTEM 区
];

function readRegion(start, len) {
    var out = {};
    try {
        var addr = base.add(start);
        var bytes = addr.readByteArray(len);
        var arr = new Uint8Array(bytes);
        for (var i = 0; i < len; i++) {
            if (arr[i] !== 0) out[(start + i).toString(16)] = arr[i];
        }
    } catch (e) {}
    return out;
}

// 对每个区域保存非零字节快照，比较差异
var snapshots = {};
REGIONS.forEach(function (r) {
    snapshots[r.start] = readRegion(r.start, r.len);
});

console.log("base=" + base);
console.log("内存窗口已就绪，请依次打开各面板，观察差异输出...\n");

// 每 500ms 采样一次
setInterval(function () {
    REGIONS.forEach(function (r) {
        var cur = readRegion(r.start, r.len);
        var prev = snapshots[r.start];
        var diffs = [];
        Object.keys(cur).forEach(function (k) {
            if (prev[k] === undefined || prev[k] !== cur[k]) {
                diffs.push("+" + k + "=" + cur[k]);
            }
        });
        Object.keys(prev).forEach(function (k) {
            if (cur[k] === undefined) diffs.push("-" + k);
        });
        if (diffs.length > 0) {
            console.log("[" + Date.now() + "] region 0x" + r.start.toString(16) + ": " + diffs.join(" "));
            snapshots[r.start] = cur;
        }
    });
}, 500);

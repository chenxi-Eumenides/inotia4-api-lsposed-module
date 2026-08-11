// frida 探针：分段调用任务完成链，定位崩溃点
// 链：UI_SetPopupProcessInfo(3,0) → QUESTSYSTEM_ChangeQuestState(questId,3) → EVTSYSTEM_DoCheckAllEvent(questId)
var base = null;
Process.enumerateModules().forEach(function (m) {
    if (m.path.indexOf("libgame.so") >= 0 && base === null) base = m.base;
});
send("base=" + base);

function rdp(off) { try { return base.add(off).readPointer(); } catch (e) { return ptr(0); } }
function rd16(off) { try { return base.add(off).readU16(); } catch (e) { return -1; } }

var step = 0;
setInterval(function () {
    if (step >= 3) return;
    var idxp = rdp(0x2f3240);
    var questId = -1;
    try { if (!idxp.isNull()) questId = idxp.readS16(); } catch (e) {}
    send("questId=" + questId + " step=" + step);
    if (step == 0) {
        try {
            var ui = new NativeFunction(base.add(0xaecc8), "int", ["int", "int"]);
            var r = ui(3, 0);
            send("UI_SetPopupProcessInfo(3,0) ret=" + r);
        } catch (e) { send("UI_SetPopupProcessInfo ERR: " + e); }
    } else if (step == 1) {
        try {
            var cqs = new NativeFunction(base.add(0x123bb4), "int", ["int", "int"]);
            var r = cqs(questId, 3);
            send("ChangeQuestState(" + questId + ",3) ret=" + r);
        } catch (e) { send("ChangeQuestState ERR: " + e); }
    } else if (step == 2) {
        try {
            var evt = new NativeFunction(base.add(0xfb2a8), "int", ["int"]);
            var r = evt(questId);
            send("DoCheckAllEvent(" + questId + ") ret=" + r);
        } catch (e) { send("DoCheckAllEvent ERR: " + e); }
    }
    step++;
    var aq = rd16(0x728ff8);
    send("activeQuest=" + aq);
}, 3000);

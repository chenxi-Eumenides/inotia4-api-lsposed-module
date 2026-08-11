// frida 探针：npc_quest 面板全局变量轮询（send 消息，配合 frida_probe_run.py 使用）
var base = null;
Process.enumerateModules().forEach(function (m) {
    if (m.path.indexOf("libgame.so") >= 0 && base === null) base = m.base;
});
send("base=" + base);

function rd8(off) { try { return base.add(off).readU8(); } catch (e) { return -1; } }
function rd32(off) { try { return base.add(off).readU32(); } catch (e) { return -1; } }
function rdp(off) { try { return base.add(off).readPointer(); } catch (e) { return ptr(0); } }
function cstr(p) { try { var s = p.readCString(); if (s && s.length > 0 && s.length < 200) return "'" + s + "'"; } catch (e) {} return null; }

setInterval(function () {
    var out = "=== probe ===";
    out += " | UIQuestState=" + rd8(0x7125c8);
    out += " | QMenuListSize=" + rd32(0x7125c0);
    out += " | QMenuSubList=" + rdp(0x712520);
    out += " | NpcTaskIdx=" + rd8(0x307820) + " Cnt=" + rd8(0x307821);
    out += " | NpcDesc=" + cstr(rdp(0x307810));
    out += " | ChoiceCnt=" + rd8(0x302d70) + " Focus=" + rd8(0x302d80);
    var qc = rdp(0x2f6270);
    out += " | QuestCount=" + (qc.isNull() ? "null" : qc.readU8());
    var qs = rdp(0x2f43d0);
    out += " | QuestSlots=" + qs;
    var p1 = rdp(0x712428), p2 = rdp(0x7124c8), p3 = rdp(0x712528);
    out += " | T1=" + cstr(p1) + " | T2=" + cstr(p2) + " | T3=" + cstr(p3);
    send(out);
}, 3000);

// frida：修正版剧情对话探针——读 GAMESTATE 状态函数指针（0x309980 区）+ hook EVTSYSTEM_SetState
// 依据：GAMESTATE_SetState(0x151590) 中 GOT 槽 [0x2f4890]/[0x2f3938]/[0x2f4930]/[0x2f5580]/[0x2f6248]
//   指向状态函数指针变量（位于 0x309980 区：enter@0x309988 proc@0x3099a0 draw@0x309990 pk@0x309998 exit@0x309980）
// 用法：timeout 600 uv run frida -U -p <PID> -q -l scripts/analyze/evt_state_probe2.js
var base = null;
Process.enumerateModules().forEach(function (m) {
    if (m.path.indexOf("libgame.so") >= 0 && base === null) base = m.base;
});
if (base === null) { send("ERROR: libgame.so not found"); }

var STATE_FN = {
    0x9c4dc: "EVT_Enter", 0x9c5c4: "EVT_Exit", 0x9c618: "EVT_Process", 0x9c640: "EVT_Draw", 0x9c73c: "EVT_PressKey",
    0x9c75c: "MAPCHG_Enter", 0x9c7ec: "MAPCHG_Process", 0x9c9ac: "MAPCHG_PressKey", 0x9c9b0: "MAPCHG_Draw", 0x9ca24: "MAPCHG_Exit",
    0x9ca70: "PLAY_Enter", 0x9cae0: "PLAY_Exit", 0x9cae4: "PLAY_Process", 0x9cfc0: "PLAY_PressKey", 0x9d6cc: "PLAY_Draw"
};

function sym(ptr) {
    if (ptr === null || ptr.isNull()) return "NULL";
    var vma = ptr.sub(base).toInt32() >>> 0;
    return (vma in STATE_FN) ? STATE_FN[vma] : ("0x" + vma.toString(16));
}

function readPtr(p) { try { return p.readPointer(); } catch (e) { return null; } }
function readU32(p) { try { return p.readU32(); } catch (e) { return null; } }
function readU16(p) { try { return p.readU16(); } catch (e) { return null; } }
function readU8(p) { try { return p.readU8(); } catch (e) { return null; } }

var lastLine = "";

function probe() {
    var lines = [];
    var st = readU16(base.add(0x307492));
    var gst = readU32(base.add(0x72b068));
    var popup = readU8(base.add(0x3070e8));
    lines.push("STATE_nState=" + st + " GAMESTATE_nState=" + gst + " popupOn=" + popup);

    var enter = readPtr(base.add(0x309988));
    var proc = readPtr(base.add(0x3099a0));
    var draw = readPtr(base.add(0x309990));
    var pk = readPtr(base.add(0x309998));
    var exit = readPtr(base.add(0x309980));
    lines.push("GAMESTATE: enter=" + sym(enter) + " proc=" + sym(proc) + " draw=" + sym(draw)
        + " pk=" + sym(pk) + " exit=" + sym(exit));

    var enState = readU32(base.add(0x713034));
    var enInfo = readU8(base.add(0x713048));
    var enIndex = readU32(base.add(0x713018));
    var enId = readU32(base.add(0x71300c));
    var enDataCount = readU32(base.add(0x713010));
    lines.push("EVT: nState=" + enState + " nInfo=" + enInfo + " nIndex=" + enIndex + " nID=" + enId
        + " nDataCount=" + enDataCount);

    var pObj = readPtr(base.add(0x712ef0));
    var pTeller = readPtr(base.add(0x713028));
    var pText = readPtr(base.add(0x3075d0));
    lines.push("EVT: pObject=" + pObj + " pTeller=" + pTeller + " pText=" + pText);
    if (pText !== null && !pText.isNull()) {
        try { lines.push("  text: " + pText.readUtf8String(80)); } catch (e) {}
    }
    if (pTeller !== null && !pTeller.isNull()) {
        try { lines.push("  teller type=" + readU8(pTeller) + " x=" + readU16(pTeller.add(2)) + " y=" + readU16(pTeller.add(4))); } catch (e) {}
    }

    var tc = base.add(0x713050);
    lines.push("EVT: TextCtrl flag(2e)=" + readU8(tc.add(0x2e)) + " page(5a)=" + readU16(tc.add(0x5a))
        + " totalPages(58)=" + readU16(tc.add(0x58)));

    var line = lines.join("\n");
    if (line !== lastLine) {
        send("[" + new Date().toISOString().substr(11, 12) + "]");
        send(line);
        lastLine = line;
    }
}

// hook EVTSYSTEM_SetState 记录 nState 迁移
var setState = base.add(0xfab38);
Interceptor.attach(setState, {
    onEnter: function (args) {
        send(">> EVTSYSTEM_SetState(" + args[0].toInt32() + ")");
    }
});

send("base=" + base);
send("探针就绪：当前剧情对话状态已采集");
probe();
setInterval(probe, 500);

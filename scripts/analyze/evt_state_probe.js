// frida：剧情对话（EVTSYSTEM）状态探针
// 用法：timeout 900 uv run frida -U -p <PID> -q -l scripts/analyze/evt_state_probe.js
// 目标：剧情对话激活判定（GAMESTATE Event 状态 + EVTSYSTEM 全局），说话人/文本读取
// 依据静态逆向：
//   GAMESTATE_nState @0x72b068 (u32)；函数指针槽：Enter=[0x2f4000+0x890] Process=[0x2f3000+0x938]
//     Draw=[0x2f4000+0x930] PressKey=[0x2f5000+0x580] Exit=[0x2f6000+0x248]
//   GAMESTATE 状态函数：Play(Enter9ca70/Proc9cae4/Draw9d6cc/PK9cfc0/Exit9cae0)
//     Event(Enter9c4dc/Proc9c618/Draw9c640/PK9c73c/Exit9c5c4) MapChange(Enter9c75c/Proc9c7ec/Draw9c9b0/PK9c9ac/Exit9ca24)
//   EVTSYSTEM：nState@0x713034 u32、nInfo@0x713048 u8、nIndex@0x713018 u32、nID@0x71300c u32、
//     nDataCount@0x713010 u32、pObject@0x712ef0、pFocusChar@0x712ef8、pTeller@0x713028、
//     pText@0x3075d0、TextCtrl@0x713050(128B)、nObjectType@0x7130d4 u8
//   对照：STATE_nState@0x307492 u16、UIPopupMsg_bOn@0x3070e8 u8、g_arrPopupStack@0x728fd8

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
function readCStr(p, max) {
    try {
        if (p === null || p.isNull()) return null;
        return p.readUtf8String(max || 128);
    } catch (e) { return "<bad-ptr>"; }
}
function readCStrAt(vma, max) {
    var p = readPtr(base.add(vma));
    if (p === null || p.isNull()) return "NULL";
    return readCStr(p, max);
}

var lastLine = "";

function probe() {
    var lines = [];

    // 游戏层状态
    var st = readU16(base.add(0x307492));
    var gst = readU32(base.add(0x72b068));
    var popup = readU8(base.add(0x3070e8));
    lines.push("STATE_nState=" + st + " GAMESTATE_nState=" + gst + " popupOn=" + popup);

    // popup 栈
    var stk = base.add(0x728fd8);
    var cnt = readU32(stk.add(8));
    lines.push("popupStack count=" + cnt);

    // GAMESTATE 函数指针槽
    var enter = readPtr(base.add(0x2f4890));
    var proc = readPtr(base.add(0x2f3938));
    var draw = readPtr(base.add(0x2f4930));
    var pk = readPtr(base.add(0x2f5580));
    var exit = readPtr(base.add(0x2f6248));
    lines.push("GAMESTATE: enter=" + sym(enter) + " proc=" + sym(proc) + " draw=" + sym(draw)
        + " pk=" + sym(pk) + " exit=" + sym(exit));

    // EVTSYSTEM 全局
    var enState = readU32(base.add(0x713034));
    var enInfo = readU8(base.add(0x713048));
    var enIndex = readU32(base.add(0x713018));
    var enId = readU32(base.add(0x71300c));
    var enDataCount = readU32(base.add(0x713010));
    var enObjType = readU8(base.add(0x7130d4));
    var enAlpha = readU8(base.add(0x713008));
    lines.push("EVT: nState=" + enState + " nInfo=" + enInfo + " nIndex=" + enIndex + " nID=" + enId
        + " nDataCount=" + enDataCount + " nObjectType=" + enObjType + " nDisplayAlpha=" + enAlpha);

    // 指针
    var pObj = readPtr(base.add(0x712ef0));
    var pFocus = readPtr(base.add(0x712ef8));
    var pTeller = readPtr(base.add(0x713028));
    lines.push("EVT: pObject=" + pObj + " pFocusChar=" + pFocus + " pTeller=" + pTeller);
    if (pObj !== null && !pObj.isNull()) {
        // CHAR 结构：+0 type +0x2 x +0x4 y（参照 probe_charsystem），名字尝试 CHARSYSTEM 名字
        var t = readU8(pObj);
        var x = readU16(pObj.add(2));
        var y = readU16(pObj.add(4));
        lines.push("  pObject CHAR: type=" + t + " x=" + x + " y=" + y);
    }
    // pTeller 可能是 CHAR* 或文本
    if (pTeller !== null && !pTeller.isNull()) {
        lines.push("  pTeller str: " + readCStr(pTeller, 64));
    }

    // 文本
    var pText = readPtr(base.add(0x3075d0));
    lines.push("EVT: pText=" + pText + " str=" + readCStr(pText, 200));

    // TextCtrl 128B 关键字段 dump（+0x2e 标志、+0x58/+0x5a 页码）
    var tc = base.add(0x713050);
    var tcFlag = readU8(tc.add(0x2e));
    var tcPage = readU16(tc.add(0x5a));
    var tcPages = readU16(tc.add(0x58));
    lines.push("EVT: TextCtrl flag(2e)=" + tcFlag + " page(5a)=" + tcPage + " totalPages(58)=" + tcPages);
    // TextCtrl 前 4 个 8B 指针尝试解引用
    for (var i = 0; i < 4; i++) {
        var tp = readPtr(tc.add(i * 8));
        lines.push("  TC[" + i + "]=" + tp + " -> " + readCStr(tp, 80));
    }

    var line = lines.join("\n");
    if (line !== lastLine) {
        send("[" + new Date().toISOString().substr(11, 12) + "]");
        send(line);
        lastLine = line;
    }
}

send("base=" + base);
send("探针就绪：当前 world 基线已打印；请切图进入剧情对话观察变化");
probe();
setInterval(probe, 500);

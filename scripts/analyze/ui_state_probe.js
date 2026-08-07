// frida：结构化解析 popup 栈 + 描述符表，验证面板识别结构
// 用法：timeout 600 uv run frida -U -p <PID> -q -l scripts/analyze/ui_state_probe.js
// 依据静态逆向：
//   g_arrPopupStack (base+0x728fd8, 32B Array):
//     +0x00 u64 type(=2)  +0x08 u32 count  +0x14 u32 elem_size(=0x40)  +0x18 u64 data
//   栈元素 0x40B: id@+0x00, flags@+0x08/0x09, enter@+0x10, process@+0x18, f3@+0x28, f4@+0x30, event@+0x38
//   g_sPopupStateList (base+0x2f9f58, 27条×64B): id@+0x00, enter@+0x10, process@+0x18, event@+0x38

var base = null;
Process.enumerateModules().forEach(function (m) {
    if (m.path.indexOf("libgame.so") >= 0 && base === null) base = m.base;
});
if (base === null) { send("ERROR: libgame.so not found"); }

var KNOWN = {
    0xb7ac4: "UIEquip_Enter", 0xd048c: "UISkill_Enter", 0xd2884: "UIStore_Enter",
    0xc0fc0: "UIMix_Enter", 0xcc4fc: "UIQuestMenu_Enter", 0xc4da4: "UIOption_Enter",
    0xba978: "UIHelp_Enter", 0xbdfec: "UIMercenary_Enter", 0xc29a8: "UINpc_Enter",
    0x155878: "UIInApp_Enter",
    0x1223e4: "POPUPSTATE_Create", 0x1224f4: "POPUPSTATE_SetEvent",
    0x122600: "POPUPSTATE_Pop", 0x122608: "POPUPSTATE_Process",
    0x122634: "POPUPSTATE_Event", 0x122698: "POPUPSTATE_Clear", 0x1223f8: "POPUPSTATE_Exist",
    0x148950: "SC_CHARACTER_INFO", 0x14a664: "SC_CHOICE", 0x14a8b0: "SC_EQUIP",
    0x14ad98: "SC_INPUT_ITEMCOUNT", 0x14af14: "SC_MERCENARY_MANAGER", 0x14b330: "SC_MIX",
    0x14b5dc: "SC_NPC", 0x14b858: "SC_NPC_QUEST", 0x14ba98: "SC_NPC_REST",
    0x14bb48: "SC_NPC_REVIVE", 0x14be20: "SC_OPTION_MMENU", 0x14c218: "SC_QUESTMENU",
    0x14c720: "SC_SAVESLOT", 0x14d670: "SC_SELECT_CHARACTER", 0x14df04: "SC_SHORTCUT_MENU",
    0x14f194: "SC_SKILL", 0x14f4b8: "SC_STORE", 0x14fb38: "SC_SYSTEMMENU",
    0x1506d8: "SC_WIPEOUT", 0x150f48: "SC_WORLDMAP", 0x15e054: "SC_INAPP_ARMOR",
    0x15e3dc: "SC_INAPP_GEMSHOP", 0x15e740: "SC_INAPP_GOODS", 0x15eac8: "SC_INAPP_HOT",
    0x15ee70: "SC_INAPP_PACKAGE", 0x15f1f8: "SC_INAPP_WEAPON", 0x16f050: "SC_DAILY_REWARD"
};

function symOf(ptr) {
    if (ptr === null) return "NULL";
    if (ptr.isNull()) return "NULL";
    var vma = ptr.sub(base).toInt32() >>> 0;
    return (vma in KNOWN) ? KNOWN[vma] : ("0x" + vma.toString(16));
}

function readU32(p) { try { return p.readU32(); } catch (e) { return null; } }
function readU64(p) { try { return p.readU64(); } catch (e) { return null; } }
function readPtr(p) { try { return p.readPointer(); } catch (e) { return null; } }

var POPUP_STACK = 0x728fd8;   // g_arrPopupStack
var STATE_LIST = 0x2f9f58;    // g_sPopupStateList
var ELEM = 0x40;

var lastLine = "";

function probe() {
    var stackPtr = base.add(POPUP_STACK);
    var type = readU64(stackPtr);
    var count = readU32(stackPtr.add(8));
    var elemSize = readU32(stackPtr.add(0x14));
    var data = readPtr(stackPtr.add(0x18));

    var lines = [];
    lines.push("st=" + type + " cnt=" + count + " esz=" + elemSize + " data=" + data);

    if (data !== null && count > 0 && count <= 27) {
        for (var i = 0; i < count; i++) {
            var el = data.add(i * ELEM);
            var id = readU32(el);
            var enter = readPtr(el.add(0x10));
            var process = readPtr(el.add(0x18));
            var event = readPtr(el.add(0x38));
            var f3 = readPtr(el.add(0x28));
            var f4 = readPtr(el.add(0x30));
            lines.push("  el[" + i + "] id=" + id
                + " enter=" + symOf(enter)
                + " proc=" + symOf(process)
                + " evt=" + symOf(event));
        }
    } else {
        lines.push("  (no elements, data=" + data + " cnt=" + count + ")");
    }

    // 描述符表前 3 条：看运行时是否填充了函数指针
    for (var i = 0; i < 3; i++) {
        var entry = base.add(STATE_LIST).add(i * 64);
        var eid = readU32(entry);
        var eenter = readPtr(entry.add(0x10));
        var eproc = readPtr(entry.add(0x18));
        var eevt = readPtr(entry.add(0x38));
        lines.push("  sList[" + i + "] id=" + eid
            + " enter=" + symOf(eenter)
            + " proc=" + symOf(eproc)
            + " evt=" + symOf(eevt));
    }

    var line = lines.join("\n");
    if (line !== lastLine) {
        send("[" + new Date().toISOString().substr(11, 12) + "]");
        send(line);
        lastLine = line;
    }
}

send("base=" + base + " (VMA0x728fd8=popupStack 0x2f9f58=stateList)");
send("探针就绪：请在手机上依次打开各 UI 面板，观察栈元素 id/函数符号变化");

setInterval(probe, 300);

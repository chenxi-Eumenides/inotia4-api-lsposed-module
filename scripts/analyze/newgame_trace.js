// 新建角色存档全流程监听（2026-08-12 探索）
// 目标链：SaveSlot_GoToNewGame(0x14cc5c) → GAME_ExitSaveSlotSelectCharacter(0x10013c) → 职业选择 →
//   STATE_EnterGame(0x1511a0) 检测新建标志 → GAME_StartNewGame(0x10017c) → 剧情 → 初始营地
// 用法: uv run python scripts/analyze/frida_probe_run.py scripts/analyze/newgame_trace.js .tmp/newgame/trace.log 600
// 注意: VMA 存数字数组不用 0x 字面量键；内存监视解引用指针（[0x2f4000+0xd20] 等为指针槽）

var base = null;
var MAX_WAIT = 60;

function findBase() {
    Process.enumerateModules().forEach(function (m) {
        if (m.path.indexOf("libgame.so") >= 0 && base === null) base = m.base;
    });
}

findBase();
if (base === null) {
    var waited = 0;
    while (base === null && waited < MAX_WAIT) {
        Thread.sleep(0.5);
        findBase();
        waited += 0.5;
    }
}
if (base === null) { send("ERROR: libgame.so not found"); }

function cstr(p) {
    try {
        if (p.isNull()) return "(null)";
        var s = p.readCString();
        return s === null ? "(unreadable)" : JSON.stringify(s);
    } catch (e) { return "(err)"; }
}

var HOOKS = [
    { vma: 0x14cc5c, name: "SaveSlot_GoToNewGame" },
    { vma: 0x14cd08, name: "SaveSlot_SlotButtonExe" },
    { vma: 0x14c720, name: "Scene_Init_POPUP_SC_SAVESLOT" },
    { vma: 0x14d670, name: "F_PANEL_CHAR_SELECT_ENTER" },
    { vma: 0x10013c, name: "GAME_ExitSaveSlotSelectCharacter" },
    { vma: 0x10015c, name: "GAME_ExitSelectCharacter" },
    { vma: 0x10017c, name: "GAME_StartNewGame" },
    { vma: 0x1002e8, name: "GAME_StartResumeGame" },
    { vma: 0x1511a0, name: "STATE_EnterGame" },
    { vma: 0x10f968, name: "MAINMENU_CreateSelectCharList" },
    { vma: 0x10fae8, name: "MAINMENU_ReleaseSelectCharList" },
    { vma: 0xf3880, name: "CHARSYSTEM_Produce" },
    { vma: 0xfb1e4, name: "EVTSYSTEM_SetReady" },
    { vma: 0xaecc8, name: "UI_SetPopupProcessInfo" },
    { vma: 0x100f2c, name: "GAMEINFO_Create" },
    { vma: 0x1149d4, name: "MAP_Load" },
    { vma: 0x151590, name: "GAMESTATE_SetState" },
    { vma: 0x129b38, name: "SAVE_CreateSaveSlot" },
    { vma: 0xdb76c, name: "CHAR_CreateCharState" },
    { vma: 0xe68c8, name: "CHAR_InitializeStatus" },
    { vma: 0xe67c8, name: "CHAR_InitializeSkill" },
];

HOOKS.forEach(function (h) {
    try {
        Interceptor.attach(base.add(h.vma), {
            onEnter: function (args) {
                var p = "";
                try {
                    if (h.name === "GAME_StartNewGame") {
                        p = " slot=" + args[0].toInt32() +
                            " charName=" + cstr(args[1]) +
                            " data=" + args[2].toInt32();
                    } else if (h.name === "UI_SetPopupProcessInfo") {
                        p = " id=" + args[0].toInt32() + " data=" + args[1].toInt32();
                    } else if (h.name === "GAMESTATE_SetState") {
                        p = " state=" + args[0].toInt32();
                    } else if (h.name === "MAP_Load") {
                        p = " mapId=" + args[0].toInt32();
                    } else if (h.name === "GAME_StartResumeGame") {
                        p = " slot=" + args[0].toInt32();
                    } else if (h.name === "SaveSlot_GoToNewGame" || h.name === "SaveSlot_SlotButtonExe") {
                        p = " x0=" + args[0];
                    } else if (h.name === "CHARSYSTEM_Produce") {
                        p = " x0=" + args[0] + " charInfo=" + args[1] +
                            " name=" + cstr(args[1].add(0));
                    } else {
                        p = " x0=" + args[0] + " x1=" + args[1];
                    }
                } catch (e) {}
                send("[HIT] " + h.name + " (0x" + h.vma.toString(16) + ")" + p);
            },
        });
    } catch (e) {
        send("hook失败 0x" + h.vma.toString(16) + " " + h.name + ": " + e);
    }
});

function ptrU8(off) {
    try {
        var p = base.add(off).readPointer();
        if (p.isNull()) return -1;
        return p.readU8();
    } catch (e) { return -2; }
}

// 内存监视（变化才输出；轮询 500ms）
var WATCH = [
    { off: 0x7135a9, name: "uiSelClass", kind: "u8" },
    { off: 0x7135aa, name: "uiSaveSlotType", kind: "u8" },
    { off: 0x3099a8, name: "bNewGame", kind: "u8" },
    { off: 0x2f50f8, name: "gameState", kind: "ptrU8" },
    { off: 0x729824, name: "saveMapId", kind: "u16" },
    { off: 0x2f4d20, name: "curSlot", kind: "ptrU8" },
    { off: 0x2f6008, name: "newGameFlag", kind: "ptrU8" },
    { off: 0x2f4e10, name: "selClassIdx", kind: "ptrU8" },
    { off: 0x2f5a00, name: "produceClassIdx", kind: "ptrU8" },
];
var last = {};
setInterval(function () {
    if (base === null) return;
    WATCH.forEach(function (w) {
        try {
            var v;
            if (w.kind === "u8") v = base.add(w.off).readU8();
            else if (w.kind === "u16") v = base.add(w.off).readU16();
            else v = ptrU8(w.off);
            if (last[w.name] === undefined || last[w.name] !== v) {
                last[w.name] = v;
                send("[MEM] " + w.name + " = " + v + " (0x" + w.off.toString(16) + ")");
            }
        } catch (e) {}
    });
}, 500);

send("新建链 hook 就绪, base=" + base);
send("请手动操作：主菜单 → 点空白存档槽 → 选职业 → 确认 → 看剧情 → 到初始营地");

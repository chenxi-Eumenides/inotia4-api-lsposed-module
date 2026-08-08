// P0-c: SYSTEMMENU 选项面板结构追踪
// hook 选项面板进入 + SAVE 调用链 + 状态变量监测
// 用法: timeout 300 uv run frida -U -p <PID> -q -l scripts/analyze/sysmenu_trace.js

var base = null;
Process.enumerateModules().forEach(function (m) {
    if (m.path.indexOf("libgame.so") >= 0 && base === null) base = m.base;
});

var PANEL_ENTERS = {
    0xc4da4: "UIOption_Enter(设置/选项)",
    0xc0fc0: "UIMix_Enter(合成)",
    0xb7ac4: "UIEquip_Enter(人物/背包)",
    0xd048c: "UISkill_Enter(技能)",
    0xcc4fc: "UIQuestMenu_Enter(任务)",
    0xc29a8: "UINpc_Enter(NPC)",
};

var SAVE_FNS = {
    0x129600: "SAVE_Save",
    0x1290c0: "SAVE_SaveData",
    0x129830: "SAVE_ProcessSave",
};

var WATCH = [
    0x307492, 0x307490, 0x712518, 0x712510, 0x3070e8,
    0x72a0f8, 0x728fd8, 0x728ed8, 0x7125c8, 0x712628,
    0x302d80, 0x7135a9, 0x7135aa, 0x7125f8,
];

function dumpWatch() {
    var out = [];
    for (var i = 0; i < WATCH.length; i++) {
        try {
            var v = base.add(WATCH[i]).readU32();
            out.push("0x" + WATCH[i].toString(16) + "=" + v);
        } catch (e) {
            out.push("0x" + WATCH[i].toString(16) + "=ERR");
        }
    }
    return out.join(" ");
}

Object.keys(PANEL_ENTERS).forEach(function (vma) {
    try {
        Interceptor.attach(base.add(parseInt(vma, 16)), {
            onEnter: function () {
                console.log("[进入] " + PANEL_ENTERS[vma] + " | " + dumpWatch());
            },
        });
    } catch (e) {
        console.log("hook失败 " + PANEL_ENTERS[vma] + ": " + e);
    }
});

Object.keys(SAVE_FNS).forEach(function (vma) {
    try {
        Interceptor.attach(base.add(parseInt(vma, 16)), {
            onEnter: function (args) {
                console.log("[SAVE] " + SAVE_FNS[vma] + " 被调用 | x0=" + args[0] + " x1=" + args[1] + " x2=" + args[2]);
            },
        });
    } catch (e) {
        console.log("SAVE hook失败 " + vma + ": " + e);
    }
});

console.log("SYSTEMMENU 追踪已就绪, base=" + base);
console.log("请打开选项面板(选项按钮)与保存/回到主菜单");

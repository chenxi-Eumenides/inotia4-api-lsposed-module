// P0-1: SYSTEMMENU 保存链动态验证
// hook SystemMenu_ButtonSaveExe + SAVE 链，验证保存按钮真实调用路径
// 用法: uv run python scripts/analyze/frida_probe_run.py scripts/analyze/sysmenu_save_hook.js .tmp/sysmenu/save_hook.log 300
// 注意: 对象键不能用 0x 字面量（会被转十进制字符串导致 parseInt(,16) 地址错位），VMA 一律存数字数组

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

var HOOKS = [
    { vma: 0x14f7c4, name: "SystemMenu_ButtonSaveExe" },
    { vma: 0x14fec0, name: "SystemMenu_ButtonHelpExe" },
    { vma: 0x14fd18, name: "SystemMenu_ButtonBackExe" },
    { vma: 0x14f7e8, name: "SystemMenu_ButtonExitExe" },
    { vma: 0x129830, name: "SAVE_ProcessSave" },
    { vma: 0x129600, name: "SAVE_Save" },
    { vma: 0x1290c0, name: "SAVE_SaveData" },
    { vma: 0x1274f0, name: "SAVE_SaveItem" },
    { vma: 0x129480, name: "SAVE_SaveCharacterAll" },
];

HOOKS.forEach(function (h) {
    try {
        Interceptor.attach(base.add(h.vma), {
            onEnter: function (args) {
                var ret = "";
                try { ret = " x0=" + args[0] + " x1=" + args[1]; } catch (e) {}
                send("[HIT] " + h.name + " (0x" + h.vma.toString(16) + ")" + ret);
            },
        });
    } catch (e) {
        send("hook失败 0x" + h.vma.toString(16) + " " + h.name + ": " + e);
    }
});

send("SAVE 链 hook 就绪, base=" + base);
send("请点击 SYSTEMMENU 面板的保存按钮 (1663,457)");

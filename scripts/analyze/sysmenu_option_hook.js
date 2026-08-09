// P0-1: SC_OPTION_MMENU 设置项按钮动态验证
// hook UIOption_ButtonListExe + APPINFO 设置函数，验证设置面板按钮映射
// 用法: uv run python scripts/analyze/frida_probe_spawn.py scripts/analyze/sysmenu_option_hook.js .tmp/sysmenu/option_hook.log 300
// 注意: 对象键不能用 0x 字面量（会被转十进制字符串导致 parseInt(,16) 地址错位），VMA 一律存数字数组

var base = null;
Process.enumerateModules().forEach(function (m) {
    if (m.path.indexOf("libgame.so") >= 0 && base === null) base = m.base;
});

var HOOKS = [
    { vma: 0xc44d8, name: "UIOption_ButtonListExe" },
    { vma: 0xd84e8, name: "APPINFO_SetVolume" },
    { vma: 0xd8590, name: "APPINFO_SetGraphicValue" },
    { vma: 0x129830, name: "SAVE_ProcessSave" },
    { vma: 0x12a1a0, name: "SAVE_MergeDatInit" },
    { vma: 0x12a210, name: "SAVE_Merge" },
    { vma: 0x92a90, name: "C2SHubSaveDataCheckExistFromServer" },
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

send("OPTION 设置项 hook 就绪, base=" + base);
send("请点击主菜单[环境设置]→ 依次点 Sound/Light/保存/载入/语言按钮");

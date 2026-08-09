// 快速验证：hook 高频函数（弹窗创建），判断 Interceptor 是否生效
// 用法: uv run python scripts/analyze/frida_probe_spawn.py scripts/analyze/hook_probe_test.js .tmp/sysmenu/hook_test.log 300
// 注意: 对象键不能用 0x 字面量（会被转十进制字符串导致 parseInt(,16) 地址错位），VMA 一律存数字数组

var base = null;
Process.enumerateModules().forEach(function (m) {
    if (m.path.indexOf("libgame.so") >= 0 && base === null) base = m.base;
});

var TARGETS = [
    { vma: 0xca778, name: "UIPopupMsg_CreateOKFromTextData" },
    { vma: 0x1377f0, name: "SOUNDSYSTEM_Play" },
    { vma: 0x129830, name: "SAVE_ProcessSave" },
    { vma: 0x14f7c4, name: "SystemMenu_ButtonSaveExe" },
];

TARGETS.forEach(function (t) {
    try {
        Interceptor.attach(base.add(t.vma), {
            onEnter: function (args) {
                send("[HIT] " + t.name);
            },
        });
        send("[OK] hook " + t.name + " @ " + base.add(t.vma));
    } catch (e) {
        send("[FAIL] " + t.name + ": " + e);
    }
});
send("验证探针就绪 base=" + base);

// frida：直接用 enumerateSymbols 拿地址 hook 面板 Enter，避免 base+offset 误差
// 用法：timeout 300 uv run frida -U -p <PID> -q -l scripts/analyze/ui_panel_hook.js
var SYMS = ["UIEquip_Enter", "UISkill_Enter", "UIStore_Enter", "UIMix_Enter",
    "UIQuestMenu_Enter", "UIOption_Enter", "UIMercenary_Enter", "UINpc_Enter"];

var WATCH = [0x307492, 0x307490, 0x72b068, 0x72b06d, 0x3070e8,
    0x72a0f8, 0x728fd8, 0x712518, 0x712510, 0x728ed8,
    0x7125c8, 0x712628, 0x302d80, 0x7135a9, 0x7135aa];

var base = null;
Process.enumerateModules().forEach(function (m) {
    if (m.path.indexOf("libgame.so") >= 0 && base === null) base = m.base;
});
if (base === null) {
    console.log("libgame.so 未找到");
} else {
    console.log("base=" + base);
    SYMS.forEach(function (name) {
        var found = false;
        Process.enumerateModules().forEach(function (m) {
            if (m.path.indexOf("libgame.so") < 0) return;
            m.enumerateSymbols().forEach(function (s) {
                if (s.name === name && !found) {
                    found = true;
                    try {
                        Interceptor.attach(s.address, {
                            onEnter: function () {
                                var parts = ["[进入] " + name];
                                WATCH.forEach(function (vma) {
                                    try {
                                        parts.push("0x" + vma.toString(16) + "=" + base.add(vma).readU32());
                                    } catch (e) {
                                        parts.push("0x" + vma.toString(16) + "=ERR");
                                    }
                                });
                                console.log(parts.join(" "));
                            }
                        });
                        console.log("已hook " + name + " @ " + s.address);
                    } catch (e) {
                        console.log("hook失败 " + name + " @ " + s.address + ": " + e);
                    }
                }
            });
        });
    });
    console.log("就绪");
}

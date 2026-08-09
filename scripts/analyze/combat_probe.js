// P0-2 combat 探索：UIPlay_bPressedAction(0x307080) 写入者探测
// hook 攻击按钮标志的读写，配合真机点击攻击按钮观察触发链
// 用法: uv run python scripts/analyze/frida_probe_spawn.py scripts/analyze/combat_probe.js .tmp/combat/combat_probe.log 300
// 注意: 对象键不能用 0x 字面量（会被转十进制字符串导致 parseInt(,16) 地址错位），VMA 一律存数字数组

var base = null;
Process.enumerateModules().forEach(function (m) {
    if (m.path.indexOf("libgame.so") >= 0 && base === null) base = m.base;
});
if (base === null) { send("ERROR: libgame.so not found"); }

var addrPressedAction = base.add(0x307080);
var addrPressedDirection = base.add(0x307081);

send("bPressedAction=" + addrPressedAction + " bPressedDirection=" + addrPressedDirection);

// 内存读写监控（frida Memory.patchCode 不适合，用 Interceptor 找不到写入者时轮询）
// 直接 hook 可能的写入函数：UIPlay_ButtonDirection / UIPlay_ButtonSKill
var HOOKS = [
    { vma: 0xc6078, name: "UIPlay_ButtonSKill" },
    { vma: 0xc6274, name: "UIPlay_ButtonSwap" },
    { vma: 0xc66d4, name: "UIPlay_ButtonDirection" },
    { vma: 0xc6478, name: "UIPlay_ButtonOK" },
];

HOOKS.forEach(function (h) {
    try {
        Interceptor.attach(base.add(h.vma), {
            onEnter: function (args) {
                var s = " [HIT] " + h.name + " x0=" + args[0] + " x1=" + args[1] + " x2=" + args[2];
                s += " bPressedAction=" + addrPressedAction.readU8() + " bPressedDir=" + addrPressedDirection.readU8();
                send(s);
            },
        });
    } catch (e) {
        send("hook失败 " + h.name + ": " + e);
    }
});

// 每 2 秒采样标志位，找写入者规律
setInterval(function () {
    var a = addrPressedAction.readU8();
    var d = addrPressedDirection.readU8();
    if (a !== 0 || d !== 0) {
        send("[SAMPLE] bPressedAction=" + a + " bPressedDirection=" + d);
    }
}, 2000);

send("combat 探针就绪");

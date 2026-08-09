// 验证 INVEN_MoveItem 真实行为：hook 入参 + 返回值，空槽源场景
// 用法: uv run python scripts/analyze/frida_probe_run.py scripts/analyze/inven_move_probe.js .tmp/combat/move_probe.log 120

var base = null;
Process.enumerateModules().forEach(function (m) {
    if (m.path.indexOf("libgame.so") >= 0 && base === null) base = m.base;
});

var F_INVEN_MOVE_ITEM = 0x104934;
Interceptor.attach(base.add(F_INVEN_MOVE_ITEM), {
    onEnter: function (args) {
        this.item = args[0];
        send("[ENTER] INVEN_MoveItem item=" + args[0] + " count=" + args[1] + " toBag=" + args[2] + " toSlot=" + args[3]);
    },
    onLeave: function (retval) {
        send("[LEAVE] retval=" + retval + " (w0) item=" + this.item);
    }
});
send("INVEN_MoveItem hook 就绪 base=" + base);

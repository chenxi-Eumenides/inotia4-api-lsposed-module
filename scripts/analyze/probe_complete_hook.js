// frida 探针：hook UINpcQuest_ButtonOKExe 内部链，观察调用序列与崩溃点
var base = null;
Process.enumerateModules().forEach(function (m) {
    if (m.path.indexOf("libgame.so") >= 0 && base === null) base = m.base;
});
send("base=" + base);

function hook(name, addr) {
    try {
        Interceptor.attach(base.add(addr), {
            onEnter: function (args) {
                send(name + " ENTER args=" + args[0] + " " + args[1] + " " + args[2]);
            },
            onLeave: function (ret) {
                send(name + " LEAVE ret=" + ret);
            }
        });
    } catch (e) { send(name + " hook fail: " + e); }
}

hook("UI_SetPopupProcessInfo", 0xaecc8);
hook("QUESTSYSTEM_ChangeQuestState", 0x123bb4);
hook("EVTSYSTEM_DoCheckAllEvent", 0xfb2a8);
hook("MAPSYSTEM_RemoveQuestLinkAsQuest", 0x11403c);
hook("CHARSYSTEM_ResetInfoState", 0xf4bb8);
hook("QUESTSYSTEM_Add", 0x122a48);

var called = false;
setInterval(function () {
    if (!called) {
        called = true;
        send("CALLING UINpcQuest_ButtonOKExe");
        try {
            var fn = new NativeFunction(base.add(0xc3414), "int", []);
            send("ret=" + fn());
        } catch (e) { send("call error: " + e); }
    }
}, 2500);

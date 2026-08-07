var base = null;
Process.enumerateModules().forEach(function (m) {
    if (m.path.indexOf("libgame.so") >= 0 && base === null) base = m.base;
});

var KNOWN = {
    0xca880: "CreateOK", 0xca8dc: "CreateYesNo", 0xca950: "CreateNone",
    0xca54c: "Create", 0xc9f04: "CreateButtonControl", 0xb181c: "SetTextControl",
    0xcaafc: "Process", 0xcab4c: "Event", 0xca318: "SetLayout",
    0xca9d8: "ButtonOKExe", 0xcaa78: "ButtonCancelExe", 0xca4e8: "Free"
};

function dumpText(ptr, label) {
    var hex = "", u16 = "", u8 = "";
    try {
        var b = ptr.readByteArray(96);
        var a = new Uint8Array(b);
        for (var i = 0; i < a.length; i++) hex += ("0" + a[i].toString(16)).slice(-2);
    } catch (e) { hex = "ERR"; }
    try { u16 = ptr.readUtf16String(96); } catch (e) {}
    try { u8 = ptr.readUtf8String(96); } catch (e) {}
    send("TXT " + label + " ptr=" + ptr + " hex=[" + hex + "] u16=[" + u16 + "] u8=[" + u8 + "]");
}

function hook(name, vma, nargs, dumpArg) {
    try {
        Interceptor.attach(base.add(vma), {
            onEnter: function (args) {
                var s = "HOOK " + name + ":";
                for (var i = 0; i < nargs; i++) s += " a" + i + "=" + args[i];
                send(s);
                if (dumpArg >= 0) dumpText(args[dumpArg], name + " arg" + dumpArg);
            }
        });
        send("hooked " + name + " @0x" + vma.toString(16));
    } catch (e) {
        send("hook FAIL " + name + ": " + e);
    }
}

hook("UIPopupMsg_Create", 0xca54c, 4, 0);
hook("UIPopupMsg_CreateOK", 0xca880, 5, -1);
hook("UIPopupMsg_CreateYesNo", 0xca8dc, 7, -1);
hook("UIPopupMsg_CreateNone", 0xca950, 4, -1);
hook("UIPopupMsg_CreateButtonControl", 0xc9f04, 3, -1);
hook("X_TEXTCTRL_SetTextControl", 0xb181c, 5, 1);
hook("UIPopupMsg_Process", 0xcaafc, 1, -1);
hook("UIPopupMsg_Free", 0xca4e8, 1, -1);

var B_ON = 0x3070e8, FP_OK = 0x3070e0, FP_CANCEL = 0x3070d8;
var YNT = 0x711ce8, PARAM = 0x711cf0, SIZE = 0x711cf8, FLAG = 0x711d00;

function symOf(ptr) {
    if (ptr === null || ptr.isNull()) return "NULL";
    var vma = ptr.sub(base).toInt32() >>> 0;
    return (vma in KNOWN) ? KNOWN[vma] : ("0x" + vma.toString(16));
}

var lastState = "";
setInterval(function () {
    var s = "";
    try { s += "bOn=" + base.add(B_ON).readU32(); } catch (e) { s += "bOn=ERR"; }
    try { s += " OK=" + symOf(base.add(FP_OK).readPointer()); } catch (e) { s += " OK=ERR"; }
    try { s += " CANCEL=" + symOf(base.add(FP_CANCEL).readPointer()); } catch (e) { s += " CANCEL=ERR"; }
    try { s += " ynt=" + base.add(YNT).readU32(); } catch (e) {}
    try { s += " param=" + base.add(PARAM).readU32(); } catch (e) {}
    try { s += " size=" + base.add(SIZE).readU32(); } catch (e) {}
    try { s += " flag=" + base.add(FLAG).readU32(); } catch (e) {}
    if (s !== lastState) {
        send("STATE " + s);
        lastState = s;
    }
}, 300);

send("ready: 弹窗探针增强版，请连续打开不同类型的弹窗");

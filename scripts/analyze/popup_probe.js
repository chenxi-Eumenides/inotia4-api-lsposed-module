var base = null;
Process.enumerateModules().forEach(function (m) {
    if (m.path.indexOf("libgame.so") >= 0 && base === null) base = m.base;
});

var B_ON = 0x3070e8;        // UIPopupMsg_bOn
var P_TEXT = 0x70f650;      // 当前弹窗文本指针（UTIL_CopyText 输出，Create 写入）
var FP_OK = 0x3070e0;       // UIPopupMsg_fpOK
var FP_CANCEL = 0x3070d8;   // UIPopupMsg_fpCancel
var YESNO_TYPE = 0x711ce8;  // UIPopupMsg_i32YesNoType
var PARAM = 0x711cf0;       // UIPopupMsg_i32Param
var SIZE_TYPE = 0x711cf8;   // UIPopupMsg_i32SizeType
var FLAG_711d00 = 0x711d00; // Create 写入的 bOn 副本

var KNOWN = {
    0xb7ac4: "UIEquip_Enter", 0xd048c: "UISkill_Enter", 0xd2884: "UIStore_Enter",
    0xc0fc0: "UIMix_Enter", 0xcc4fc: "UIQuestMenu_Enter", 0xc4da4: "UIOption_Enter",
    0xba978: "UIHelp_Enter", 0xbdfec: "UIMercenary_Enter", 0xc29a8: "UINpc_Enter",
    0x155878: "UIInApp_Enter", 0xca880: "UIPopupMsg_CreateOK", 0xca8dc: "UIPopupMsg_CreateYesNo",
    0xca950: "UIPopupMsg_CreateNone", 0xca9d8: "ButtonOKExe", 0xcaa78: "ButtonCancelExe"
};

function symOf(ptr) {
    if (ptr === null || ptr.isNull()) return "NULL";
    var vma = ptr.sub(base).toInt32() >>> 0;
    return (vma in KNOWN) ? KNOWN[vma] : ("0x" + vma.toString(16));
}

function readU32(p) { try { return p.readU32(); } catch (e) { return null; } }
function readPtr(p) { try { return p.readPointer(); } catch (e) { return null; } }

function readText(ptr) {
    if (ptr === null || ptr.isNull()) return "NULL";
    var hex = "";
    try {
        var b = ptr.readByteArray(48);
        var a = new Uint8Array(b);
        for (var i = 0; i < a.length; i++) hex += ("0" + a[i].toString(16)).slice(-2);
    } catch (e) { hex = "ERR"; }
    var u16 = "", u8 = "";
    try { u16 = ptr.readUtf16String(64); } catch (e) {}
    try { u8 = ptr.readUtf8String(64); } catch (e) {}
    return "hex=[" + hex + "] u16=[" + u16 + "] u8=[" + u8 + "]";
}

var last = "";
function probe() {
    var bOn = readU32(base.add(B_ON));
    var flagD = readU32(base.add(FLAG_711d00));
    var txtPtr = readPtr(base.add(P_TEXT));
    var fpOK = readPtr(base.add(FP_OK));
    var fpCancel = readPtr(base.add(FP_CANCEL));
    var ynt = readU32(base.add(YESNO_TYPE));
    var param = readU32(base.add(PARAM));
    var szt = readU32(base.add(SIZE_TYPE));

    var text = readText(txtPtr);
    var ptextSym = "";
    try {
        var pv = base.add(0x3070b8).readU64();
        var ptr2 = ptr(pv);
        ptextSym = " pTextSym0x3070b8=[" + pv.toString(16) + "]" + (ptr2.isNull() ? "" : readText(ptr2));
    } catch (e) { ptextSym = " pTextErr"; }

    var ctrlHex = "";
    try {
        var c = base.add(0x711d10).readByteArray(48);
        var ca = new Uint8Array(c);
        for (var i = 0; i < ca.length; i++) ctrlHex += ("0" + ca[i].toString(16)).slice(-2);
    } catch (e) { ctrlHex = "ERR"; }

    var line = "bOn=" + bOn + " flag=" + flagD
        + " P_TEXT(0x70f650)=" + (txtPtr === null ? "NULL" : "0x" + txtPtr.toString(16))
        + " text=" + text
        + " OK=" + symOf(fpOK) + " CANCEL=" + symOf(fpCancel)
        + " ynt=" + ynt + " param=" + param + " sz=" + szt
        + " ctrl711d10=[" + ctrlHex + "]" + ptextSym;
    if (line !== last) {
        send("[" + new Date().toISOString().substr(11, 12) + "] " + line);
        last = line;
    }
}

send("popup probe ready, base=" + base);
setInterval(probe, 300);

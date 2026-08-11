'use strict';

let base = null;
Process.enumerateModules().forEach(function (m) {
    if (m.path.indexOf("libgame.so") >= 0 && base === null) base = m.base;
});
if (!base) { send("ERROR: no libgame"); throw new Error("no libgame"); }

function fmt(ptr) {
    if (ptr === null || ptr.isNull()) return "NULL";
    return "0x" + (ptr.sub(base).toInt32() >>> 0).toString(16);
}

function dump(label) {
    let s = label;
    try {
        const mp = base.add(0x2f4000 + 0x890).readPointer().readPointer();
        s += " mainProcess=" + fmt(mp);
    } catch (e) { s += " mainProcess=ERR"; }
    try {
        const ec = base.add(0x2f6000 + 0x248).readPointer().readPointer();
        s += " exitCb=" + fmt(ec);
    } catch (e) { s += " exitCb=ERR"; }
    try {
        const cs = base.add(0x2f6000 + 0xaa0).readPointer();
        s += " curState=" + cs.readU32();
    } catch (e) { s += " curState=ERR"; }
    try {
        s += " mapChangeFlag=" + base.add(0x2f4000 + 0x468).readPointer().readU8();
    } catch (e) { s += " mapFlag=ERR"; }
    try {
        const mcd = base.add(0x2f5000 + 0xa60).readPointer();
        s += " mapData=" + mcd.readU16() + "," + mcd.add(2).readU16() + "," + mcd.add(4).readU16() + "," + mcd.add(6).readU16();
    } catch (e) { s += " mapData=ERR"; }
    try {
        const stk = base.add(0x2f3000 + 0x590).readPointer();
        s += " popupCnt=" + stk.add(8).readU32();
    } catch (e) { s += " popup=ERR"; }
    try {
        s += " fe0=" + base.add(0x2f6000 + 0xfe0).readPointer().readU8();
    } catch (e) {}
    send(s);
}

dump("STATE0");
setInterval(function () { dump("T"); }, 1500);
send("ready");

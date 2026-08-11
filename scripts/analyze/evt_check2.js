const base = Module.findBaseAddress("libgame.so");
send("base:" + base);
const r32 = (vma) => base.add(vma).readU32();
const r8 = (vma) => base.add(vma).readU8();
const rp = (vma) => base.add(vma).readPointer();
const rp32 = (vma) => base.add(vma).readPointer().add(0).readU32();

send("GAMESTATE_nState:" + r32(0x72b068));
send("EVT_nState:" + r32(0x713034) + " nIndex:" + r32(0x713018) + " nID:" + r32(0x71300c) + " nDataCount:" + r32(0x713010));
const pText = rp(0x3075d0);
send("pText:" + pText);
if (!pText.isNull()) {
    try { send("text:" + pText.readUtf8String(300)); } catch (e) { send("text read fail"); }
}
const pTeller = rp(0x713028);
send("pTeller:" + pTeller);
if (!pTeller.isNull()) {
    send("teller type:" + pTeller.add(0).readU8() + " x:" + pTeller.add(2).readU16() + " y:" + pTeller.add(4).readU16());
    const getName = new NativeFunction(base.add(0xd9c54), 'pointer', ['pointer']);
    try { const nm = getName(pTeller); send("teller name:" + nm.readUtf8String()); } catch (e) { send("name fail"); }
}
const ctrl = base.add(0x713050);
send("TextCtrl flag(2e):" + ctrl.add(0x2e).readU8() + " totalPages(58):" + ctrl.add(0x58).readU16() + " curPage(5a):" + ctrl.add(0x5a).readU16());
const idxPtr = rp(0x2f4000 + 0xa50);
const stateArr = rp(0x2f6000 + 0xf98);
send("scene idxPtr:" + idxPtr + " stateArr:" + stateArr);
if (!idxPtr.isNull()) {
    const idx = idxPtr.readS8();
    send("scene index:" + idx);
    if (!stateArr.isNull()) send("scene state[" + idx + "]:" + stateArr.add(idx * 4).readU32());
}
send("popup_on:" + r8(0x3070e8));

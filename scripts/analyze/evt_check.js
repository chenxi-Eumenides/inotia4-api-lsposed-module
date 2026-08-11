// 检查剧情对话内存状态：EVTSYSTEM 各字段 + 场景状态数组 + TextCtrl
const base = Module.findBaseAddress("libgame.so");
send("base:", base);
const r32 = (vma) => base.add(vma).readU32();
const r8 = (vma) => base.add(vma).readU8();
const rp = (vma) => base.add(vma).readPointer();

const gst = r32(0x72b068);
send("GAMESTATE_nState:", gst);
send("EVT_nState:", r32(0x713034), "nIndex:", r32(0x713018), "nID:", r32(0x71300c), "nDataCount:", r32(0x713010));
const pText = rp(0x3075d0);
send("pText:", pText);
if (!pText.isNull()) {
    try { send("text:", pText.readUtf8String(200)); } catch (e) { send("text read fail", e); }
}
const pTeller = rp(0x713028);
send("pTeller:", pTeller);
if (!pTeller.isNull()) {
    send("teller type:", pTeller.add(0).readU8(), "x:", pTeller.add(2).readU16(), "y:", pTeller.add(4).readU16());
    // CHAR_GetName 0xd9c54
    const getName = new NativeFunction(base.add(0xd9c54), 'pointer', ['pointer']);
    try { const nm = getName(pTeller); send("teller name:", nm.readUtf8String()); } catch (e) { send("name fail", e); }
}
// TextCtrl
const ctrl = base.add(0x713050);
send("TextCtrl flag(0x2e):", ctrl.add(0x2e).readU8(), "totalPages(0x58):", ctrl.add(0x58).readU16(), "curPage(0x5a):", ctrl.add(0x5a).readU16());
// 场景状态
const idxPtr = rp(0x2f4000 + 0xa50);
const stateArr = rp(0x2f6000 + 0xf98);
send("scene idx ptr:", idxPtr, "stateArr ptr:", stateArr);
if (!idxPtr.isNull()) {
    const idx = idxPtr.readS8();
    send("scene index:", idx);
    if (!stateArr.isNull()) send("scene state[", idx, "]:", stateArr.add(idx * 4).readU32());
}
// popup
send("popup_on:", r8(0x3070e8));

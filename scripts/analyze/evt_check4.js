const base = Process.getModuleByName("libgame.so").base;
const r32 = (vma) => base.add(vma).readU32();
const rp = (vma) => base.add(vma).readPointer();
send("GAMESTATE:" + r32(0x72b068));
send("EVT_nState:" + r32(0x713034) + " nIndex:" + r32(0x713018) + " nID:" + r32(0x71300c) + " nDataCount:" + r32(0x713010));
send("pText:" + rp(0x3075d0));
// EVTSYSTEM_pEventState 0x3075c8
const pES = rp(0x3075c8);
send("pEventState ptr:" + pES);
if (!pES.isNull()) {
    // dump 前 16 个 u32
    let s = "eventState[0..15]:";
    for (let i = 0; i < 16; i++) s += " " + pES.add(i*4).readU32();
    send(s);
}
// scene 状态数组
const idxPtr = rp(0x2f4000 + 0xa50);
const stateArr = rp(0x2f6000 + 0xf98);
send("scene idx:" + (idxPtr.isNull()? "null" : idxPtr.readS8()) + " stateArr:" + stateArr);
if (!idxPtr.isNull() && !stateArr.isNull()) {
    const idx = idxPtr.readS8();
    let s = "sceneStates[0..15]:";
    for (let i = 0; i < 16; i++) s += " " + stateArr.add(i*4).readU32();
    send(s);
    if (idx >= 0) send("cur scene state:" + stateArr.add(idx*4).readU32());
}

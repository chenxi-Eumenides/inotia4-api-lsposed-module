const base = Process.getModuleByName("libgame.so").base;
const r32 = (vma) => base.add(vma).readU32();
const rp = (vma) => base.add(vma).readPointer();
// EVTSYSTEM_pEventState 0x3075c8 完整 dump
const pES = rp(0x3075c8);
send("pEventState:" + pES);
if (!pES.isNull()) {
    let s = "";
    for (let i = 0; i < 64; i++) { s += pES.add(i*4).readU32() + " "; }
    send("eventState[0..63]: " + s);
}
// 任务槽区：槽数 [0x2f6000+0x270] 头+0 (u8)
const qcnt = base.add(0x2f6000 + 0x270).readPointer().readU8();
send("quest count:" + qcnt);
const qarr = base.add(0x2f4000 + 0x3d0).readPointer().readPointer();
let q = "quests:";
for (let i = 0; i < qcnt && i < 8; i++) q += " " + qarr.add(i*12).readU16();
send(q);

const base = Process.getModuleByName("libgame.so").base;
const pES = base.add(0x3075c8).readPointer();
send("before eventState[0]:" + pES.add(0).readU32());
pES.add(0).writeU32(0);
send("after eventState[0]:" + pES.add(0).readU32());

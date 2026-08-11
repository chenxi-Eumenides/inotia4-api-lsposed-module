#!/usr/bin/env python3
import sys, time
import frida

PID = sys.argv[1] if len(sys.argv) > 1 else "28421"

JS = r"""
var base = null;
Process.enumerateModules().forEach(function (m) {
    if (m.path.indexOf("libgame.so") >= 0 && base === null) base = m.base;
});
send("module base: " + base);
if (base === null) { send("ERROR: libgame.so not found"); throw 0; }

function u32(addr) {
    try { return base.add(addr).readU32(); }
    catch (e) { return "ERR:" + e.message; }
}

send("GAMESTATE_nState(0x72b068): " + u32(0x72b068));
send("EVT_nState(0x713034): " + u32(0x713034));
send("EVT_nIndex(0x713018): " + u32(0x713018));
send("EVT_nID(0x71300c): " + u32(0x71300c));
send("EVT_nDataCount(0x713010): " + u32(0x713010));
send("EVT_pText(0x3075d0): " + u32(0x3075d0));
var ptext = base.add(0x3075d0).readPointer();
send("pText: " + ptext);
if (!ptext.isNull()) { try { send("text: " + ptext.readUtf8String(200)); } catch (e) { send("read err " + e.message); } }
var pteller = base.add(0x713028).readPointer();
send("pTeller: " + pteller);
"""

s = frida.get_device_manager().add_remote_device("127.0.0.1:27042")
session = s.attach(int(PID))
script = session.create_script(JS)
script.on("message", lambda msg, data: print(msg.get("payload") if msg.get("type") == "send" else msg))
script.load()
time.sleep(3)

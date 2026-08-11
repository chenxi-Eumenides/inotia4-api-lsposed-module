#!/usr/bin/env python3
"""探针 v5：检查 EVTSYSTEM + GAMESTATE 当前状态。
用法：uv run python .tmp/hook_evt5.py <PID>
"""
from __future__ import annotations
import sys, time
import frida

PID = sys.argv[1] if len(sys.argv) > 1 else "28421"

JS = r"""
var base = null;
Process.enumerateModules().forEach(function (m) {
    if (m.path.indexOf("libgame.so") >= 0 && base === null) base = m.base;
});
if (base === null) { send("ERROR: libgame.so not found"); }

function u32(addr) { try { return Memory.readU32(base.add(addr)); } catch (e) { return -1; } }
function u8(addr) { try { return Memory.readU8(base.add(addr)); } catch (e) { return -1; } }
function rdptr(addr) { try { return Memory.readPointer(base.add(addr)); } catch (e) { return rdptr("0x0"); } }

var GAMESTATE_nState = 0x72b068;
var EVT_nState = 0x713034;
var EVT_nIndex = 0x713018;
var EVT_nID = 0x71300c;
var EVT_nDataCount = 0x713010;
var EVT_pText = 0x3075d0;
var EVT_pTeller = 0x713028;
var POPUP_ON = 0x728fd0; // ? 需要确认，G_POPUP_ON_VMA
var STATE_nState = 0x728fd8; // ? 需要确认

send("GAMESTATE_nState: " + u32(GAMESTATE_nState));
send("EVT_nState: " + u32(EVT_nState));
send("EVT_nIndex: " + u32(EVT_nIndex));
send("EVT_nID: " + u32(EVT_nID));
send("EVT_nDataCount: " + u32(EVT_nDataCount));
var ptext = rdptr(EVT_pText);
send("EVT_pText: " + ptext);
if (!ptext.isNull()) {
    try { send("EVT text: " + ptext.readUtf8String(200)); } catch (e) { send("read err " + e); }
}
var pteller = rdptr(EVT_pTeller);
send("EVT_pTeller: " + pteller);
if (!pteller.isNull()) {
    try { send("teller nameId: " + pteller.add(0x2).readU16()); } catch (e) {}
}
"""

s = frida.get_device_manager().add_remote_device("127.0.0.1:27042")
session = s.attach(int(PID))
script = session.create_script(JS)
script.on("message", lambda msg, data: print(msg.get("payload") if msg.get("type") == "send" else msg))
script.load()
time.sleep(3)

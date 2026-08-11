import sys, time, frida
PID = sys.argv[1]
JS = r"""
var base = null;
Process.enumerateModules().forEach(function (m) {
    if (m.path.indexOf("libgame.so") >= 0 && base === null) base = m.base;
});
send("SAVE_nMapID(0x729824): " + base.add(0x729824).readU16());
// MAPCHANGE 目标结构 *(0x2f5000+0xa60)
var mc = base.add(0x2f5000 + 0xa60).readPointer();
send("MAPCHANGE struct: " + mc + " mapId=" + (mc.isNull() ? "null" : mc.readU16()) + " x=" + (mc.isNull() ? "-" : mc.add(2).readU16()) + " y=" + (mc.isNull() ? "-" : mc.add(4).readU16()));

// hook MAP_Load(0x1149d4) 观察 mapId 参数
Interceptor.attach(base.add(0x1149d4), {
    onEnter: function (args) {
        send("[MAP_Load] w0(mapId)=" + args[0].toInt32() + " w1=" + args[1].toInt32() + " w2=" + args[2].toInt32());
    }
});
send("MAP_Load hooked");
"""
s = frida.get_device_manager().add_remote_device("127.0.0.1:27042")
session = s.attach(int(PID))
script = session.create_script(JS)
script.on("message", lambda m, d: print(m.get("payload") if m.get("type") == "send" else m))
script.load()
time.sleep(20)

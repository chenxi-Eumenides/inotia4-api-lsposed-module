import frida, time, sys
dev = frida.get_device_manager().add_remote_device('127.0.0.1:27042')
pid = [p.pid for p in dev.enumerate_processes() if 'inotia4' in p.name.lower()][0]
s = dev.attach(pid)
code = """
var base = Process.getModuleByName("libgame.so").base;
var evtSetState = new NativeFunction(base.add(0xfab38), 'void', ['int32']);
var gstSetState = new NativeFunction(base.add(0x151590), 'void', ['int32']);
Interceptor.attach(base.add(0xfab38), {
  onEnter: function(args) { send("EVTSYSTEM_SetState(" + args[0].toInt32() + ")"); }
});
Interceptor.attach(base.add(0x151590), {
  onEnter: function(args) { send("GAMESTATE_SetState(" + args[0].toInt32() + ")"); }
});
var r32 = function(v) { return base.add(v).readU32(); };
send("hook ready, gst=" + r32(0x72b068) + " evt=" + r32(0x713034));
"""
script = s.create_script(code)
script.on('message', lambda m, d: print("[%s] %s" % (time.strftime('%H:%M:%S'), m.get('payload', m)), flush=True))
script.load()
time.sleep(60)

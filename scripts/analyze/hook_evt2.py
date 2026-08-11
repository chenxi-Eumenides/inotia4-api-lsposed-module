import frida, time
dev = frida.get_device_manager().add_remote_device('127.0.0.1:27042')
pid = [p.pid for p in dev.enumerate_processes() if 'inotia4' in p.name.lower()][0]
s = dev.attach(pid)
code = """
var base = Process.getModuleByName("libgame.so").base;
var names = {0x9c4dc:'EVT_Enter', 0x9c618:'EVT_Process', 0x9c640:'EVT_Draw', 0x9c73c:'EVT_PressKey', 0x9c5c4:'EVT_Exit', 0x9ca70:'Play_Enter', 0x9cae0:'Play_Exit', 0x9cae4:'Play_Process', 0x9cfc0:'Play_PressKey', 0x9d6cc:'Play_Draw', 0x9c75c:'MC_Enter', 0x9c7ec:'MC_Process', 0x9c9ac:'MC_PressKey', 0x9c9b0:'MC_Draw', 0x9ca24:'MC_Exit'};
Interceptor.attach(base.add(0xfab38), {
  onEnter: function(args) {
    var gst = base.add(0x72b068).readU32();
    var bt = Thread.backtrace(this.context, Backtracer.ACCURATE).slice(0,4).map(function(p){
      var off = p.sub(base).toInt32() >>> 0;
      return names[off] || ('0x'+off.toString(16));
    });
    send("EVT_SetState(" + args[0].toInt32() + ") gst=" + gst + " from=" + bt.join(','));
  }
});
send("ready");
"""
script = s.create_script(code)
script.on('message', lambda m, d: print("[%s] %s" % (time.strftime('%H:%M:%S'), m.get('payload', m)), flush=True))
script.load()
time.sleep(75)

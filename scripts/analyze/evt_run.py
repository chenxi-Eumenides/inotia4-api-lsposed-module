import frida, sys, time
dev = frida.get_device_manager().add_remote_device('192.168.3.54:5555')
target = None
for proc in dev.enumerate_processes():
    if 'inotia4' in proc.name.lower():
        target = proc; break
print('target:', target.name if target else None, target.pid if target else '')
s = dev.attach(target.pid)
code = open('.tmp/evt_check.js').read()
s.on('message', lambda m, d: print('[MSG]', m.get('payload', m)))
s.load(code)
time.sleep(1)
s.detach()

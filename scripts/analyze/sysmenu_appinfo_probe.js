// P0-1: APPINFO 结构体字段采样验证
// 读取 APPINFOBASE_pData 指向结构体的各字段当前值（音量/震动/声音/自动保存/画质）+ 语言变量
// 用法: uv run python scripts/analyze/frida_probe_run.py scripts/analyze/sysmenu_appinfo_probe.js .tmp/sysmenu/appinfo.log 200
// 依据静态逆向: APPINFOBASE_pData = *(0x2f5000+0xb18); 字段: +0x00音量 +0x01震动 +0x02声音 +0x03自动保存 +0x04画质掩码
// 语言: UI索引 = *(0x2f9000+0xf34) u32; SGL语言ID = *(0x2f5000+0x28) 指针指向 u32

var base = null;
Process.enumerateModules().forEach(function (m) {
    if (m.path.indexOf("libgame.so") >= 0 && base === null) base = m.base;
});
if (base === null) { send("ERROR: libgame.so not found"); }

function dumpAppInfo() {
    var p = base.add(0x2f5000 + 0xb18).readPointer();
    if (p.isNull()) { send("APPINFOBASE_pData = NULL"); return; }
    var langPtr = base.add(0x2f5000 + 0x28).readPointer();
    var langSgl = "N/A";
    try { langSgl = langPtr.readU32(); } catch (e) {}
    var langUi = base.add(0x2f9000 + 0xf34).readU32();
    send("APPINFO@ " + p
        + " 音量=" + p.add(0x00).readU8()
        + " 震动=" + p.add(0x01).readU8()
        + " 声音=" + p.add(0x02).readU8()
        + " 自动保存=" + p.add(0x03).readU8()
        + " 画质掩码=" + p.add(0x04).readU8()
        + " | 语言UI=" + langUi
        + " SGL语言ID=" + langSgl);
}

dumpAppInfo();
send("请在设置面板点击 Sound 开/关、Light Effect 开/关，观察字段变化");
setInterval(dumpAppInfo, 500);

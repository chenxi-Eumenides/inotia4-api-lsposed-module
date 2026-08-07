// frida：枚举 libgame.so 模块与 UIEnter 符号地址
var mods = Process.enumerateModules().filter(function (m) {
    return m.path.indexOf("libgame.so") >= 0;
});
console.log("libgame.so 副本数: " + mods.length);
mods.forEach(function (m) {
    console.log("MOD base=" + m.base + " size=" + m.size + " path=" + m.path);
    ["UIEquip_Enter", "UISkill_Enter", "UIStore_Enter", "UIMix_Enter",
     "UIQuestMenu_Enter", "UIOption_Enter", "UIMercenary_Enter", "UINpc_Enter"].forEach(function (n) {
        var s = m.enumerateSymbols().filter(function (x) { return x.name === n; });
        if (s.length > 0) {
            console.log("  " + n + " addr=" + s[0].address + " offset=" + s[0].address.sub(m.base));
        }
    });
});

// frida 脚本：追踪游戏内 UI 面板切换时的内存变化（简化版）
// 用法：timeout 300 uv run frida -U -p <游戏PID> -q -l scripts/analyze/ui_panel_trace.js
// 游戏中依次打开各面板，观察输出

const PANEL_ENTERS = {
    0xb7ac4: "UIEquip_Enter(人物/背包)",
    0xd048c: "UISkill_Enter(技能)",
    0xd2884: "UIStore_Enter(商店)",
    0xc0fc0: "UIMix_Enter(合成)",
    0xcc4fc: "UIQuestMenu_Enter(任务)",
    0xc4da4: "UIOption_Enter(设置)",
    0xbdfec: "UIMercenary_Enter(佣兵)",
    0xc29a8: "UINpc_Enter(NPC)",
};

const WATCH = [0x307492, 0x307490, 0x72b068, 0x72b06d, 0x3070e8,
    0x72a0f8, 0x728fd8, 0x712518, 0x712510, 0x728ed8,
    0x7125c8, 0x712628, 0x302d80, 0x7135a9, 0x7135aa];

function findBase() {
    const m = Process.enumerateModules().find(x => x.path.includes("libgame.so"));
    return m ? m.base : null;
}

function dumpWatch(base) {
    const out = [];
    for (const vma of WATCH) {
        try {
            const addr = base.add(vma);
            const u32 = addr.readU32();
            out.push("0x" + vma.toString(16) + "=" + u32);
        } catch (e) {
            out.push("0x" + vma.toString(16) + "=ERR");
        }
    }
    return out.join(" ");
}

function hookPanels(base) {
    for (const [vma, name] of Object.entries(PANEL_ENTERS)) {
        try {
            const addr = base.add(parseInt(vma, 16));
            Interceptor.attach(addr, {
                onEnter() {
                    console.log("[进入] " + name + " | " + dumpWatch(base));
                },
            });
            console.log("已hook: " + name);
        } catch (e) {
            console.log("hook失败 " + name + ": " + e);
        }
    }
}

const base = findBase();
if (!base) {
    console.log("libgame.so 未找到");
} else {
    console.log("base=" + base);
    hookPanels(base);
    console.log("就绪，请依次打开面板...");
}


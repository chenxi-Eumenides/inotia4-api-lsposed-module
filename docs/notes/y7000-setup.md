# y7000 Windows 模拟器方案（动态分析环境）

> 日期：2026-08-05 ｜ 用途：运行 ARM-only 游戏（艾诺迪亚4，arm64-v8a + armeabi-v7a）供 frida 动态分析与 API 联调
> 原则：y7000 上所有项目文件放 `D:\Apps\Qemu\android-game-api-export\`（环境隔离）
>
> **⚠️ 方案状态（2026-08-05 更新）：本文方案已实测否定，仅留档。**
> - 方案②（LineageOS-qemu ARM VM）：TCG 实测 boot 25+ 分钟未完成，不可用
> - 方案①（官方 Emulator x86_64）：翻译层下 frida 无法 hook ARM 库，且 LSPatch 模块 native 层跨架构高风险
> - 最终结论：**x86_64 主机无可行模拟器方案，frida 分析与 LSPatch native 验证唯一可靠路径 = ARM 真机（root 手机）**
> - 详见 `emulator-research.md` §6-7（实测记录与最终结论）

## 方案选型（2026-08-05 研究结论）

| 方案 | 可行性 | 说明 |
|---|---|---|
| **① 官方 Android Emulator + API 30 x86_64 镜像**（推荐） | ★★★★★ | x86_64 镜像**内置 libndk ARM 翻译层**（API 28-30 官方支持 ARMv7/ARM64 应用），WHPX 硬件加速接近原生速度；targetSdk 29 游戏完美匹配 |
| ② LineageOS-for-QEMU arm64only（qemu-system-aarch64） | ★★★★ | 现成 ARM VM（1.05GB），真 ARM 环境，但 TCG 慢（启动 5-20 分钟） |
| ③ 官方模拟器 arm64 镜像 | ❌ | arm64 镜像不能在 x86_64 host 运行（已确认） |
| ④ Redroid（WSL2 容器） | ★★★ | x86_64 镜像内置 libndk，headless；需自编译 WSL2 内核（binder/ashmem），门槛高 |

## 推荐方案 ① 步骤

```powershell
# 1. 装 SDK 组件（sdkmanager 在 Android Studio 或 platform-tools）
sdkmanager "platform-tools" "emulator" "system-images;android-30;google_apis;x86_64"

# 2. 建 AVD（targetSdk 29 游戏与 API 30 匹配）
avdmanager create avd -n inotia4 -k "system-images;android-30;google_apis;x86_64" -d pixel_4

# 3. 启动（需 Windows 开 Hyper-V/WHPX；-accel-check 验证）
emulator -avd inotia4 -gpu auto -no-snapshot

# 4. 装游戏（x86_64 镜像自带 ARM 翻译，ARM 库透明运行）
adb install game.apk
adb shell am start -n <package>/.MainActivity

# 5. frida-server 部署（容器/模拟器内）
adb push frida-server /data/local/tmp/ && adb shell "chmod 755 /data/local/tmp/frida-server && /data/local/tmp/frida-server &"
adb forward tcp:27042 tcp:27042
```

若游戏在 Android 11 上兼容性异常，退回 `system-images;android-29;google_apis;x86_64`。

## 备选方案 ②（QEMU 11 已装，scoop）

镜像：`UTM-VM-lineage-23.2-...-virtio_arm64only.zip`（~1.05GB）
下载：`https://github.com/jqssun/android-lineage-qemu/releases/latest`

```bat
qemu-system-aarch64 -machine virt -cpu max,pauth-impdef=on -accel tcg,tb-size=1024,thread=multi -m 4096 ^
  -drive if=pflash,unit=0,file=...\edk2-aarch64-code.fd,format=raw,readonly=on ^
  -drive if=pflash,unit=1,file=...\efi_vars.fd ^
  -drive file=...\vda.qcow2,if=none,id=vda ^
  -device virtio-blk-pci,drive=vda,bootindex=0 ^
  -drive file=...\vdb.qcow2,if=none,id=vdb ^
  -device virtio-blk-pci,drive=vdb,bootindex=1 ^
  -device virtio-net-pci,netdev=net0 ^
  -netdev user,id=net0,hostfwd=tcp:0.0.0.0:5555-:5555 ^
  -device virtio-gpu-pci -display gtk,gl=off ^
  -device qemu-xhci -device usb-tablet,bus=usb-bus.0 -device usb-kbd,bus=usb-bus.0
adb connect 127.0.0.1:5555
```

注意：不要用 `virtio_arm64only_16k` 子目标（16KB 页内核拒载旧 NDK 编译的 .so）。

## 待办

- [ ] 确定方案（①/②），在 `D:\Apps\Qemu\android-game-api-export\` 下准备镜像与脚本
- [ ] 游戏 APK 传输到 y7000（项目文件夹内）
- [ ] LSPatch 0.6 与 libxposed 101 兼容性实测（必要时降级 API 93）

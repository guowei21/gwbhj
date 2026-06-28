# GWBHJ — 过强标黑机 KSU越狱版

基于 [COPG v5.4.0](https://github.com/AlirezaParsi/COPG) 的 Zygisk 设备伪装模块，支持 Magisk / KernelSU / APatch。

## 功能

| 功能 | 说明 |
|------|------|
| 设备信息伪装 | 伪装 `Build.MODEL`、`BRAND`、`DEVICE`、`MANUFACTURER`、`FINGERPRINT`、`PRODUCT`、`BOARD`、`HARDWARE`、`ID`、`DISPLAY`、`SERIAL` 等 Java 层字段 |
| 系统属性伪造 (COW) | **默认开启** — 通过 COW 方式修改 `ro.product.*`、`ro.hardware`、`ro.soc.*`、`ro.build.*` 等 30+ 属性，per-process 生效，无需额外标签 |
| CPU 信息伪造 | **默认开启** — companion 进程 bind mount 伪造 `/proc/cpuinfo`（Kirin 9020 8 核），`:blocked` 标签可关闭 |
| ANDROID_ID 伪造 | **默认开启** — 伪造 `Settings.Secure.android_id`（Cache + GenerationTracker 双路径）；**每个白名单 App 派生独立 ID**（FNV-1a 64 + splitmix64，种子+包名），`android_id.txt` 作为种子 |
| 序列号控制 | 随机生成序列号，写入 `serial.txt`，支持运行时热更新 |
| 白名单 + `:blocked` | 只对白名单包名生效，`:blocked` 标签让敏感 App（如银行）关闭 CPU 伪装 |
| KSU 越狱脚本 | 音量键交互菜单：修改序列号 / 修改ANDROID_ID / 越狱隐藏环境 / 冻结管理 / 退出 |
| 安装后软重启 | 刷入模块后自动调用 KSU soft reboot，无需手动重启 |

## 当前伪装配置

> Huawei PLU-AL10 / HWPLU（build.prop）+ Kirin 9020（real cpuinfo）

| 项 | 值 |
|----|----|
| MODEL | PLU-AL10 |
| BRAND | HUAWEI |
| DEVICE | HWPLU |
| MANUFACTURER | HUAWEI |
| PRODUCT (name) | PLU-AL10 |
| BOARD | PLU |
| FINGERPRINT | HUAWEI/HWPLU/PLU-AL10:12/HUAWEIPLU-AL10/104.3.0.198C00:user/release-keys |
| BUILD_ID | HUAWEIPLU-AL10 |
| BUILD_TYPE | user |
| BUILD_TAGS | release-keys |
| INCREMENTAL | 104.3.0.198C00 |
| DESCRIPTION | PLU-AL10-user 104.3.0 HUAWEIPLU-AL10 198-CHN-LGRP1 release-keys |
| SECURITY_PATCH | 2021-10-05 |
| RELEASE / SDK | 12 / 31 |
| VNDK | 31 |
| fingerprintName | HUAWEI-Z198 |
| hardwareversion | HL1XHDM |
| hardware.sku | PLU-AL10 |
| ab_ota_partitions | system |
| characteristics | default |
| HARDWARE | kirin9020 |
| PLATFORM (BOARD) | HiSilicon |
| CPU | HiSilicon ARMv8 (ARM64), Kirin 9020（2×0xd4f big + 6×0xd46 little = 8 核, BogoMIPS 3.84） |
| ABI | arm64-v8a |
| GPU | Maleoon 920 |
| OpenGL | ES 3.2（196610） |
| ANDROID_ID | 每白名单 App 独立派生（种子 + 包名，FNV-1a + splitmix64，16 位 hex） |

## 项目结构

```
gwbhj/
├── .github/workflows/build.yml   # GitHub Actions CI（4 ABI 交叉编译 + 打包）
├── .gitignore
├── build.sh                      # 本地构建脚本（Termux / Linux）
├── module/                       # Magisk 模块静态文件
│   ├── action.sh                 # 音量键交互菜单（5 选项）
│   ├── customize.sh              # 安装时自定义逻辑（root 检测、ABI 清理、权限设置、生成 serial.txt + android_id.txt）
│   ├── service.sh                # 开机后服务（serial.txt + android_id.txt 权限 + SELinux 维护）
│   ├── uninstall.sh              # 卸载清理
│   ├── whitelist.txt             # 白名单（每行一个包名，支持 :blocked 标签）
│   └── META-INF/com/google/android/
│       ├── update-binary         # 安装入口
│       └── updater-script        # #!/sbin/sh; exit 0
└── src/                          # C++ 源码
    ├── CMakeLists.txt            # CMake 构建配置
    ├── include/zygisk.hpp        # Zygisk API 头文件 (v5.4.0)
    ├── spoof_module.cpp          # 核心模块：COW属性伪造、Build字段修改、CPU mount、ANDROID_ID伪造、白名单+blocked
    └── atexit.cpp                # 自定义 __cxa_atexit/__cxa_finalize（防止 dlclose 异常）
```

## 工作原理

### Zygisk 模块加载流程

```
Zygisk 启动 App 进程
  └─ onLoad()              → 保存 api/env 引用
  └─ preAppSpecialize()    → 读取进程包名
      ├─ 不在白名单        → DLCLOSE_MODULE_LIBRARY，跳过
      └─ 在白名单          → 执行伪装：
          ├─ spoofDevice()         修改 Build.MODEL/BRAND/DEVICE/SERIAL 等 Java 静态字段
          ├─ forgeProp() × 30+     COW 方式修改系统属性（per-process，默认开启）
          ├─ executeCompanion()    通过 socket 通知 companion 进程
          │   ├─ 无 :blocked       → mount_spoof (bind mount cpuinfo)
          │   └─ 有 :blocked       → unmount_spoof (关闭 CPU 伪装)
  └─ postAppSpecialize()   → ANDROID_ID 伪造（forgeAndroidId）
      ├─ android_id.txt 非空 → 派生 per-app ID (seed+包名, FNV-1a+splitmix64)
      │                       → 伪造 Settings.Secure.android_id (Cache + GenerationTracker)
      └─ android_id.txt 空   → DLCLOSE_MODULE_LIBRARY，跳过
```

### ANDROID_ID 伪造机制

采用 COPG v5.4.0 的 `forgeAndroidId()` 实现，在 `postAppSpecialize` 中执行（需要 App UID / SELinux 上下文访问 MemoryIntArray ashmem）：

1. **Per-app 派生**：以 `android_id.txt` 为种子，结合 App 的 base_package（去掉 `:proc` 后缀），通过 FNV-1a 64 + splitmix64 派生 16 位 hex ID。每个白名单 App 得到不同 ID，同 App 的主进程与子进程共享同一 ID，重启后稳定不变
2. **Cache 路径**（Android 16+）：替换 `Settings$Secure` 内部 `Cache` 类的 `mValues` HashMap 条目
3. **GenerationTracker 路径**（旧版本）：替换 `GenerationTracker` 内部 `mMap` ConcurrentHashMap 条目
4. 两路径均使用 JNI 反射直接修改内部数据结构，无需调用 `ContentResolver`

### Companion 进程

- 以 root 身份运行（Zygisk companion 机制）
- `mount_spoof`：从二进制内写入 cpuinfo 到临时文件 → bind mount 到 `/proc/cpuinfo` → unlink 临时文件
- `unmount_spoof`：umount + 清理（供 `:blocked` 标签使用）

### 隐藏性

- Release 版本关闭所有日志输出（`LOGI/LOGW/LOGE` 为空宏）
- 类名使用通用名称 `ZModule`
- cpuinfo 临时文件 mount 后立即 unlink，磁盘无残留
- 属性修改为 per-process COW，不修改全局属性区
- ANDROID_ID 修改发生在 app 进程内部，仅影响自身 `Settings.Secure` 缓存

## 使用方法

### 安装

1. 确保已安装 Magisk + ReZygisk、KernelSU + Zygisk Next、或 APatch + Zygisk 实现
2. 在模块管理器中刷入 ZIP
3. 安装脚本会自动：检测 root 方案、检测 Zygisk 实现、清理多余 ABI、设置权限、生成 `serial.txt` + `android_id.txt`
4. 安装完成后自动执行 KSU soft reboot

### 音量键交互菜单

手动运行 `action.sh` 进入交互菜单：

```sh
sh /data/adb/modules/gwbhj_jailbreak/action.sh
```

菜单选项（循环切换）：

| 选项 | 功能 |
|------|------|
| 1. 修改序列号 | 随机生成新的 16 位序列号，写入 serial.txt |
| 2. 修改ANDROID_ID | 随机生成新的 16 位 hex 种子，写入 android_id.txt（所有白名单 App 派生 ID 同步刷新） |
| 3. 越狱隐藏环境 | 放宽内核安全限制 + PID 回收 + soft reboot |
| 4. 冻结管理 | 冻结/解冻系统软件 |
| 5. 退出 | 退出菜单 |

操作方式：
- **音量下**：切换到下一个选项（第 5 项按下循环回第 1 项）
- **音量上**：确认当前选项

### 白名单配置

编辑 `/data/adb/modules/gwbhj_jailbreak/whitelist.txt`：

```
com.tencent.tmgp.dfm
com.coolapk.market
com.liuzh.deviceinfo

com.icbc                  # :blocked — 银行类 App 关闭 CPU 伪装
com.cmbchina              # :blocked
```

- `#` 开头的行为注释
- 包名后加 `:blocked` 标签可关闭该 App 的 CPU 伪装（COW 属性伪装仍生效）
- 保存后无需重启，模块自动检测文件变化并热更新
- 子进程匹配：`com.xxx.app:remote` 按冒号前 `com.xxx.app` 匹配白名单

### ANDROID_ID

- 首次安装自动生成 16 位随机 hex 种子，写入 `android_id.txt`
- **每个白名单 App 拥有独立的 ANDROID_ID**：以 `android_id.txt` 内容为种子，结合 App 包名通过 FNV-1a 64 + splitmix64 派生稳定的 16 位 hex ID（同一 App 的主进程与 `:remote`/`:proc` 子进程共享同一 ID，符合真实 Android 8+ 行为；不同 App 得到不同 ID，避免跨 App 关联检测）
- 派生是确定性的（无 RNG）：重启后各 App 的 ANDROID_ID 保持不变；修改种子后所有 App 的 ID 同步刷新
- 手动重置：运行 `action.sh` 选择「修改ANDROID_ID」（重置种子，所有 App 派生 ID 同步变化）
- 手动修改：`echo "YOUR_SEED" > /data/adb/modules/gwbhj_jailbreak/android_id.txt`
- 留空 `android_id.txt` 可禁用 ANDROID_ID 伪造（仅保留 COW + CPU 伪装）

### 序列号

- 首次安装自动生成 16 位随机序列号，写入 `serial.txt`
- 手动重置：运行 `action.sh` 选择「修改序列号」
- 手动修改：`echo "YOUR_SERIAL" > /data/adb/modules/gwbhj_jailbreak/serial.txt`

### 越狱隐藏环境

运行 `action.sh` 选择「越狱隐藏环境」：
1. 放宽内核安全参数（kptr_restrict、dmesg_restrict 等）
2. restorecon 属性区
3. PID 回收（绕过进程检测）
4. 自动 soft reboot

## 打包构建

### 方法一：GitHub Actions（推荐，全 ABI）

推送到 `main` 分支或手动触发 workflow，自动编译 4 个 ABI 并打包为完整模块 ZIP。

### 方法二：本地 Linux / Termux

前置依赖：`cmake`、`clang++` 或 `g++`、`zip`、Android NDK（可选）

```sh
# 单 ABI 构建（当前设备架构）
./build.sh

# 指定版本号
./build.sh 1.1.0 116

# 产物：output/GWBHJ-KSU-Jailbreak-v1.1.0-<ABI>.zip
```

### 方法三：Windows + NDK

需要已安装 Android NDK（如 `D:\android-ndk-r27d`）。

```powershell
$NDK = "D:\android-ndk-r27d"
$TOOLCHAIN = "$NDK\toolchains\llvm\prebuilt\windows-x86_64\bin"
$SRC = "源码目录\src"
$INCLUDE = "$SRC\include"

# 编译 arm64-v8a
& "$TOOLCHAIN\aarch64-linux-android21-clang++.cmd" `
    -std=c++17 -Os -flto -ffunction-sections -fdata-sections `
    -fvisibility=hidden -fvisibility-inlines-hidden -fno-rtti `
    -g0 -DNDEBUG -fPIC -shared `
    -o arm64-v8a.so `
    -I"$INCLUDE" "$SRC\spoof_module.cpp" "$SRC\atexit.cpp" `
    -Wl,--gc-sections -Wl,--exclude-libs,ALL `
    -static-libstdc++ -llog

# strip
& "$TOOLCHAIN\llvm-strip.exe" --strip-unneeded arm64-v8a.so

# 其他 ABI 同理，替换编译器前缀
```

打包 ZIP 时**必须使用正斜杠 `/` 作为路径分隔符**：

```powershell
$stageDir = "staging"
New-Item -ItemType Directory -Path "$stageDir\zygisk" -Force
New-Item -ItemType Directory -Path "$stageDir\META-INF\com\google\android" -Force

Copy-Item arm64-v8a.so "$stageDir\zygisk\arm64-v8a.so"
Copy-Item module\* "$stageDir\" -Recurse -Force

$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[System.IO.File]::WriteAllText("$stageDir\module.prop", @"
id=gwbhj_jailbreak
name=过强标黑机
version=1.1.0
versionCode=116
author=蜡笔不小心
description=基于COPG v5.4.0的设备伪装模块,COW属性+CPU伪装默认开启,支持ANDROID_ID伪造与序列号控制,白名单:blocked标签可关闭CPU伪装,自带越狱免解过环境
minMagisk=20.4
"@, $utf8NoBom)

# 打包 ZIP
$zip = [System.IO.Compression.ZipFile]::Open("GWBHJ-v1.1.0.zip", "Create")
foreach ($file in [System.IO.Directory]::GetFiles($stageDir, "*", "AllDirectories")) {
    $relative = $file.Substring($stageDir.Length).TrimStart('\','/') -replace '\\','/'
    $entry = $zip.CreateEntry($relative, "Optimal")
    $es = $entry.Open()
    $fs = [System.IO.File]::OpenRead($file)
    $fs.CopyTo($es); $fs.Dispose(); $es.Dispose()
}
$zip.Dispose()
```

### DEBUG 构建

编译时添加 `-DDEBUG` 宏启用日志输出：

```sh
cmake -DDEBUG=ON ...
```

DEBUG 模式下 `logcat | grep GWBHJModule` 可查看模块运行日志。

## 自定义设备信息

修改 `src/spoof_module.cpp` 中的 `g_dev_*` 编译时常量（所有值均经 `OBF()` 编译期 XOR 混淆）：

```cpp
static const std::string g_dev_brand         = OBF("HUAWEI");
static const std::string g_dev_device        = OBF("HWPLU");
static const std::string g_dev_manufacturer  = g_dev_brand;             // HUAWEI
static const std::string g_dev_model         = OBF("PLU-AL10");
static const std::string g_dev_product       = g_dev_model;             // PLU-AL10
static const std::string g_dev_board         = OBF("PLU");
static const std::string g_dev_fingerprint   = OBF("HUAWEI/HWPLU/PLU-AL10:12/HUAWEIPLU-AL10/104.3.0.198C00:user/release-keys");
static const std::string g_dev_hardware      = OBF("kirin9020");
static const std::string g_dev_platform      = OBF("HiSilicon");
static const std::string g_dev_chipname      = OBF("Kirin 9020");
```

同时修改 `CPU_CORES[]` 数组（核心数/part 号）与 `CPU_FEATURES[]`（指令集特征）、`buildCpuinfo()` 中的 BogoMIPS/Hardware 等。修改后需重新编译所有 ABI 并打包。

## 技术细节

| 技术 | 实现 |
|------|------|
| Java 层伪装 | JNI `SetStaticObjectField` 修改 `android.os.Build` 静态字段（含 ID、DISPLAY、SERIAL） |
| 系统属性伪造 (COW) | mmap `__system_property_find` 返回的 `prop_info`，per-process 修改，**默认对所有白名单 App 开启**；自动检测并跳过 long 属性（bionic `kLongFlag`，Android 12+），避免 OnePlus/Android 16 等设备内联编辑崩溃 |
| cpuinfo 伪造 | Companion 进程 `mount --bind`，临时文件 mount 后立即 `unlink`；`:blocked` 标签 App 不挂载 |
| ANDROID_ID 伪造 | `postAppSpecialize` 中反射修改 `Settings$Secure` 内部 Cache / GenerationTracker，双 ctor 兼容 Android 16+；**per-app 派生**：`android_id.txt` 种子 + base_package 经 FNV-1a 64 + splitmix64 派生 16 位 hex，每 App 独立、主进程与子进程共享、重启稳定 |
| 序列号 | `ro.boot.serialno` + `ro.serialno` 属性伪造 + `Build.SERIAL` Java 字段 |
| 白名单 | 文件 mtime 监测 + mutex 保护，支持热更新，`:blocked` 标签支持 |
| 子进程匹配 | `包名:子进程名` 自动截取冒号前部分匹配白名单 |
| 自定义 atexit | 替代 `__cxa_atexit/__cxa_finalize`，防止 dlclose 时析构崩溃 |

## 兼容性

| 项目 | 要求 |
|------|------|
| Android | 9.0+ (API 26+) |
| Root | Magisk 20.4+ / KernelSU / APatch |
| Zygisk | Zygisk Next 或 ReZygisk |
| ABI | arm64-v8a / armeabi-v7a / x86_64 / x86 |

## 致谢

- 基于 [COPG v5.4.0](https://github.com/AlirezaParsi/COPG) by Alireza Parsi
- Zygisk API by topjohnwu

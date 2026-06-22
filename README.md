# GWBHJ — 过强标黑机 v2.0 授权版

基于 Zygisk 的 Android 设备伪装模块，支持 Magisk / KernelSU / APatch。v2.0 新增授权验证系统。

## 功能

| 功能 | 说明 |
|------|------|
| 设备信息伪装 | 伪装 `Build.MODEL`、`BRAND`、`DEVICE`、`MANUFACTURER`、`FINGERPRINT`、`PRODUCT`、`BOARD`、`HARDWARE` 等 Java 层字段 |
| 系统属性伪造 | 通过 COW (Copy-On-Write) 方式修改 `ro.product.*`、`ro.hardware`、`ro.soc.*`、`ro.build.*` 等 30+ 属性，per-process 生效 |
| CPU 信息伪造 | 通过 companion 进程 bind mount 伪造 `/proc/cpuinfo`，伪装为 Kirin 9030 Pro 14 核 |
| 序列号控制 | 随机生成序列号，写入 `serial.txt`，支持运行时热更新 |
| 白名单机制 | 只对白名单中的包名生效，支持子进程名匹配（如 `com.xxx:remote`） |
| KSU 越狱脚本 | 音量键交互菜单 + 命令行参数模式 |
| 安装后软重启 | 刷入模块后自动调用 KSU soft reboot，无需手动重启 |
| **授权验证** | **v2.0 新增：Ed25519 签名验签、设备绑定、互抵锁死、grace period 兜底** |

## 当前伪装配置

| 项 | 值 |
|----|----|
| MODEL | SGT-AL10 |
| 营销名 | HUAWEI Mate 80 Pro Max |
| BRAND | HUAWEI |
| DEVICE | SGT |
| MANUFACTURER | HUAWEI |
| PRODUCT | SGT-AL10 |
| FINGERPRINT | HUAWEI/SGT-AL10/HWSGT:12/HUAWEISGT-AL10/6.0.0.150C00:user/release-keys |
| HARDWARE | kirin9030pro |
| CPU | HUAWEI Kirin 9030 Pro (14 核) |
| HarmonyOS | 6.0.0 |

## 项目结构

```
gwbhj/
├── .github/workflows/build.yml   # GitHub Actions CI（4 ABI 交叉编译 + 打包）
├── .gitignore
├── build.sh                      # 本地构建脚本（Termux / Linux）
├── module/                       # Magisk 模块静态文件
│   ├── action.sh                 # 音量键交互菜单 + 命令行参数模式
│   ├── customize.sh              # 安装时自定义逻辑（root 检测、ABI 清理、权限设置）
│   ├── service.sh                # 开机后服务（权限维护 + license 权限）
│   ├── uninstall.sh              # 卸载清理（含 license.json/clash_info.json）
│   ├── whitelist.txt             # 白名单（每行一个包名，# 开头为注释）
│   └── META-INF/com/google/android/
│       ├── update-binary         # 安装入口
│       └── updater-script        # #!/sbin/sh; exit 0
├── server/                       # 服务端 EdgeOne Edge Functions
│   ├── edge-functions/index.js   # API: bind/verify/clash-info/admin
│   ├── package.json
│   ├── wrangler.toml
│   └── README.md                 # 部署指南
└── src/                          # C++ 源码
    ├── CMakeLists.txt            # CMake 构建配置
    ├── include/
    │   ├── zygisk.hpp            # Zygisk API 头文件 (v4)
    │   ├── sha256.h              # SHA-256 实现（纯 C++，无依赖）
    │   ├── ed25519.h             # Ed25519 验签（纯 C++，无依赖）
    │   └── mini_json.h           # 迷你 JSON 解析器（无依赖）
    ├── spoof_module.cpp          # 核心模块：属性伪造、Build 修改、cpuinfo mount、白名单、授权验证
    ├── sha256.cpp                # SHA-256 实现
    ├── ed25519.cpp               # Ed25519 签名验证实现
    ├── mini_json.cpp             # JSON 解析实现
    └── atexit.cpp                # 自定义 __cxa_atexit/__cxa_finalize
```

## 工作原理

### Zygisk 模块加载流程

```
Zygisk 启动 App 进程
  └─ onLoad()              → 保存 api/env 引用
  └─ preAppSpecialize()    → 读取进程包名
      ├─ 不在白名单        → DLCLOSE_MODULE_LIBRARY，跳过
      ├─ 在白名单但授权失败 → DLCLOSE_MODULE_LIBRARY，不执行伪装
      └─ 在白名单且授权通过 → 执行伪装：
          ├─ spoofDevice()       修改 Build.MODEL/BRAND/DEVICE 等 Java 静态字段
          ├─ forgeProp() × 30+   COW 方式修改系统属性（per-process，不影响全局）
          └─ executeCompanion()  通过 socket 通知 companion 进程 mount cpuinfo
  └─ postAppSpecialize()   → 日志记录
```

### Companion 进程

- 以 root 身份运行（Zygisk companion 机制）
- 接收 `mount_spoof` 命令：从二进制内写入 cpuinfo 内容到临时文件 → bind mount 到 `/proc/cpuinfo` → 删除临时文件
- 接收 `unmount_spoof` 命令：umount + 清理

### 隐藏性

- Release 版本关闭所有日志输出（`LOGI/LOGW/LOGE` 为空宏），`logcat` 无暴露
- 类名使用通用名称 `ZModule`，二进制中无模块特征字符串
- `cpuinfo` 临时文件 mount 成功后立即删除，磁盘无残留
- 属性修改为 per-process COW，不修改全局属性区

## 使用方法

### 安装

1. 确保已安装 Magisk + ReZygisk（或 Zygisk Next）、KernelSU + Zygisk Next、或 APatch + Zygisk 实现
2. 在模块管理器中刷入 ZIP
3. 安装脚本会自动：检测 root 方案、检测 Zygisk 实现、清理多余 ABI、设置权限、生成随机序列号
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
| 2. 越狱隐藏环境 | 放宽内核安全限制 + PID 回收 + soft reboot |
| 3. 冻结多开系统软件 | 冻结多开用户的系统软件 |
| 4. 查看状态 | 查看模块和授权状态 |
| 5. 退出 | 退出菜单 |

操作方式：
- **音量下**：切换到下一个选项
- **音量上**：确认当前选项

### 命令行模式

```sh
sh /data/adb/modules/gwbhj_jailbreak/action.sh reset-serial   # 修改序列号
sh /data/adb/modules/gwbhj_jailbreak/action.sh jailbreak      # 越狱隐藏环境
sh /data/adb/modules/gwbhj_jailbreak/action.sh freeze         # 冻结多开系统软件
sh /data/adb/modules/gwbhj_jailbreak/action.sh status         # 查看状态
```

## 授权验证系统 (v2.0)

### 工作流程

1. 用户在管理端 APK 输入卡密 → APK 调用服务端 `/api/bind` → 服务端返回签名 `license.json` → APK 写入模块目录
2. `.so` 在 `preAppSpecialize` 中调用 `authOk()` 验签 → 验证通过才执行伪装
3. APK 定期调用 `/api/verify` 更新 `license.json`，`.so` 只需读本地文件

### authOk() 验证链

```
读取 license.json → Ed25519 验签 → device_hash 匹配本机 → revoked==false
→ lock_until 未过期 → active==true → grace period 有效 → 全部通过
```

认证失败时：不修改 Build/属性/cpuinfo，DLCLOSE 卸载模块，不影响 Zygisk。

### 设备 ID

从 `/proc/cmdline` 顺序读取：`androidboot.cpuid` → `chipid` → `emmcid` → `oplusboot.serialno` → `serialno`，取第一个非空值，生成 `SHA256(hardware_id + SALT)`。

### 互抵机制

- 同一卡密同一时间只允许一台设备
- 被互抵后伪装功能关闭
- 连续互抵次数递增锁定：1-3次锁1小时 → 4-5次锁4小时 → ... → 10次永久锁死

### Grace Period

网络断开时允许离线使用 24 小时（`grace_seconds` 可配置）。超期自动关闭伪装。

### 安全设计

| 层级 | 保护 |
|------|------|
| license.json 篡改 | Ed25519 验签失败 |
| 伪造服务器响应 | 无私钥无法签名 |
| 多人共用卡密 | 互抵 + 渐进锁死 |
| 抓包重放 | nonce + 时间戳 |

### 白名单配置

编辑 `/data/adb/modules/gwbhj_jailbreak/whitelist.txt`，每行一个包名：

```
com.tencent.tmgp.dfm
com.coolapk.market
com.liuzh.deviceinfo
```

- `#` 开头的行为注释
- 保存后无需重启，模块会自动检测文件变化并热更新
- 支持子进程匹配：`com.xxx.app:remote` 会按 `com.xxx.app` 匹配白名单

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
./build.sh 1.0.10 110

# 产物：output/GWBHJ-KSU-Jailbreak-v1.0.10-<ABI>.zip
```

### 方法三：Windows + NDK

需要已安装 Android NDK（如 `D:\android-ndk-r27d`）。

```powershell
# 设置变量
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
    -I"$INCLUDE" "$SRC\spoof_module.cpp" "$SRC\atexit.cpp" "$SRC\sha256.cpp" "$SRC\ed25519.cpp" "$SRC\mini_json.cpp" `
    -Wl,--gc-sections -Wl,--exclude-libs,ALL `
    -static-libstdc++ -llog

# strip
& "$TOOLCHAIN\llvm-strip.exe" --strip-unneeded arm64-v8a.so

# 其他 ABI 同理，替换编译器前缀：
# armeabi-v7a → armv7a-linux-androideabi21-clang++.cmd
# x86_64      → x86_64-linux-android21-clang++.cmd
# x86         → i686-linux-android21-clang++.cmd
```

打包 ZIP 时**必须使用正斜杠 `/` 作为路径分隔符**（Android unzip 不识别 `\`）：

```powershell
# 手动打包（PowerShell）
$stageDir = "staging"
New-Item -ItemType Directory -Path "$stageDir\zygisk" -Force
New-Item -ItemType Directory -Path "$stageDir\META-INF\com\google\android" -Force

# 复制文件
Copy-Item arm64-v8a.so "$stageDir\zygisk\arm64-v8a.so"
Copy-Item module\* "$stageDir\" -Recurse -Force

# 创建 module.prop（UTF-8 无 BOM）
$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[System.IO.File]::WriteAllText("$stageDir\module.prop", @"
id=gwbhj_jailbreak
name=过强标黑机 KSU越狱版
version=1.0.10
versionCode=110
author=GWBHJ
description=伪装 SGT-AL10、HUAWEI Mate 80 Pro Max、Kirin 9030 Pro、CPU及序列号，白名单控制，KSU越狱版。
minMagisk=20.4
"@, $utf8NoBom)

# 打包 ZIP（强制使用 / 路径）
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip = [System.IO.Compression.ZipFile]::Open("GWBHJ-KSU-Jailbreak-v1.0.10.zip", "Create")
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

如需调试日志输出，编译时添加 `-DDEBUG` 宏：

```sh
# Linux / Termux
cmake -DDEBUG=ON ...

# Windows NDK
-std=c++17 ... -DDEBUG ...
```

DEBUG 模式下 `logcat | grep GWBHJModule` 可查看模块运行日志。

## 自定义设备信息

修改 `src/spoof_module.cpp` 中的编译时常量：

```cpp
static constexpr const char* DEVICE_BRAND = "HUAWEI";
static constexpr const char* DEVICE_DEVICE = "SGT";
static constexpr const char* DEVICE_MANUFACTURER = "HUAWEI";
static constexpr const char* DEVICE_MODEL = "SGT-AL10";
static constexpr const char* DEVICE_MARKET_NAME = "HUAWEI Mate 80 Pro Max";
static constexpr const char* DEVICE_FINGERPRINT = "HUAWEI/SGT-AL10/HWSGT:12/...";
static constexpr const char* DEVICE_PRODUCT = "SGT-AL10";
static constexpr const char* DEVICE_HARDWARE = "kirin9030pro";
static constexpr const char* DEVICE_PLATFORM = "kirin9030";
```

同时修改 `CPUINFO_SPOOF[]` 数组中的 `/proc/cpuinfo` 内容。

修改后需重新编译所有 ABI 并打包。

## 技术细节

| 技术 | 实现 |
|------|------|
| Java 层伪装 | JNI `SetStaticObjectField` 修改 `android.os.Build` 静态字段 |
| 系统属性伪造 | COW mmap `__system_property_find` 返回的 `prop_info`，per-process 修改 |
| cpuinfo 伪造 | Companion 进程 `mount --bind`，临时文件 mount 后立即 `unlink` |
| 序列号 | `ro.boot.serialno` + `ro.serialno` 属性伪造 + `Build.SERIAL` Java 字段 |
| 白名单 | 文件 mtime 监测 + mutex 保护，支持热更新 |
| 子进程匹配 | `包名:子进程名` 自动截取冒号前部分匹配白名单 |
| 自定义 atexit | 替代 `__cxa_atexit/__cxa_finalize`，防止 dlclose 时析构崩溃 |
| 授权验证 | 纯 C++ 实现 SHA256 + Ed25519 + JSON 解析，无外部依赖 |
| 设备 ID | 从 `/proc/cmdline` 读取硬件 ID，生成 `SHA256(hwId + SALT)` |
| 验签 | 内置服务端公钥，Ed25519 验证 license.json 签名 |

## 兼容性

| 项目 | 要求 |
|------|------|
| Android | 9.0+ (API 26+) |
| Root | Magisk 20.4+ / KernelSU / APatch |
| Zygisk | Zygisk Next 或 ReZygisk（Magisk 内置 Zygisk 需关闭） |
| ABI | arm64-v8a / armeabi-v7a / x86_64 / x86 |

## 致谢

- 基于 [COPG v5.1.1](https://github.com/AlirezaParsi/Change-My-Device) by Alireza Parsi
- Zygisk API by topjohnwu

# GWBHJ 管理端开发规划

2026-06-22 | v1.0

---

## 一、项目概述

- **项目名称**: GWBHJ Manager（基于 PrismSpace 集成）
- **包名**: `com.hw.security.service`（伪装为系统服务，避免被目标App扫描发现）
- **定位**: 基于 PrismSpace 框架集成 GWBHJ 授权伪装管理模块，替代原 KernelSU WebUI 方案
- **兼容**: Magisk / KernelSU / APatch 三种 root 方案
- **核心原则**: 授权验证逻辑在 .so 中执行，管理端只是前端；利用 PrismSpace 的 root 管理能力 + WebUI 体系
- **通信方式**: root 权限读写模块目录文件 + 执行 shell 命令
- **优势**: PrismSpace 已有 root Shell 封装、模块管理 UI、KSU WebUI 支持，无需从零开发基础框架

---

## 二、系统架构

### 2.1 整体架构

服务端 (EdgeOne EF) <--HTTP--> APK 端 (GWBHJ Mgr 管理前端) <--文件--> .so 端 (ZModule 核心逻辑)

### 2.2 信任边界

- APK 写入 license.json / whitelist.txt，.so 读取验签
- .so 不信任 APK，只信任服务端签名数据
- APK 不信任 .so，只通过文件间接通信
- 服务端私钥签名，.so / APK 用公钥验签

### 2.3 数据流

1. 用户输入卡密 -> APK 调用 /api/bind -> 服务端返回 license.json(签名) -> APK 写入文件 -> .so 读取验签 -> authOk()? 是:伪装开启 / 否:DLCLOSE
2. APK 定期调用 /api/verify -> 服务端返回最新 license.json -> APK 写入文件 -> .so 定期检查
3. APK 调用 /api/clash-info -> 服务端返回 clash_info(签名) -> APK 展示互抵详情

---

## 三、技术栈

| 类别 | 选型 | 说明 |
|------|------|------|
| 语言 | Kotlin | 现代 Android 开发首选 |
| UI | Jetpack Compose + Material 3 | 原生 UI，现代风格 |
| 网络 | OkHttp3 + Retrofit | HTTP 请求，简单轻量 |
| JSON | Gson | 序列化/反序列化 |
| Ed25519 | Bouncy Castle | 纯 Java 实现，无需 JNI |
| Root | libsu (topjohnwu) | 成熟的 Android root shell 库 |
| 构建 | Gradle 8+ Kotlin DSL | 标准构建系统 |
| 最低 SDK | API 26 (Android 8.0) | 与模块要求一致 |
| 目标 SDK | API 34 (Android 14) | 编译目标 |
| 混淆 | R8 + ProGuard | 代码优化和混淆 |
| CI | GitHub Actions | 自动构建 APK + 模块 ZIP |
| 定时任务 | WorkManager | 低功耗周期性验证，受 Doze 约束 |

---

## 四、APK 功能模块

### 4.1 项目结构

```
app/src/main/java/com/hw/security/service/
├── MainActivity.kt               # 入口：根据授权状态路由
├── auth/
│   ├── AuthViewModel.kt         # 授权验证逻辑
│   ├── AuthScreen.kt            # 卡密输入/绑定 UI
│   ├── DeviceIdHelper.kt        # 设备 ID 生成（读取 /proc/cmdline）
│   └── LicenseManager.kt        # license.json 管理 + Ed25519 验签
├── status/
│   ├── StatusViewModel.kt       # 状态面板 ViewModel
│   ├── StatusScreen.kt          # 主状态面板 UI
│   └── ClashInfoScreen.kt       # 互抵/锁死详情 UI
├── management/
│   ├── WhitelistViewModel.kt    # 白名单管理逻辑
│   ├── WhitelistScreen.kt       # 白名单管理 UI
│   ├── FreezeListViewModel.kt   # 冻结列表管理逻辑
│   ├── FreezeListScreen.kt      # 冻结列表管理 UI
│   └── SerialManager.kt         # 序列号管理逻辑
├── actions/
│   ├── ActionExecutor.kt        # action.sh 命令执行器
│   ├── JailbreakScreen.kt       # 越狱操作 UI
│   └── FreezeScreen.kt          # 冻结操作 UI
├── worker/
│   ├── VerifyWorker.kt          # WorkManager 定期验证 Worker
│   └── WorkerScheduler.kt       # WorkManager 调度配置
├── network/
│   ├── ApiClient.kt             # Retrofit API 客户端
│   ├── ApiService.kt            # API 接口定义
│   └── ApiResponse.kt           # API 响应模型
├── root/
│   ├── RootHelper.kt            # root 权限检测 + Shell 执行封装
│   ├── ModuleFileIO.kt          # 模块目录文件读写
│   └── ShellExecutor.kt         # su -c 命令执行
├── crypto/
│   ├── Ed25519Verifier.kt       # Ed25519 验签
│   └── HashGenerator.kt         # SHA256 device_hash 生成
├── data/
│   ├── License.kt               # license.json 数据模型
│   ├── ClashInfo.kt             # clash_info.json 数据模型
│   └── DeviceInfo.kt            # 设备信息数据模型
├── theme/
│   ├── Theme.kt                 # 主题定义
│   └── Colors.kt                # 颜色定义
└── util/
    ├── Constants.kt             # 常量定义
    └── Logger.kt                # 日志工具
```

### 4.2 页面导航

```
App 启动
  |
  |-- 检测 root 权限 ------ 无 root -> 退出提示
  |-- 检测模块是否安装 ---- 未安装 -> 安装引导
  |-- 读取 license.json
  |
  |-- 有有效授权 -> 主面板 (StatusScreen)
  |                   |-- 授权状态卡片
  |                   |-- 设备信息卡片
  |                   |-- 序列号卡片
  |                   |-- 白名单入口
  |                   |-- 冻结列表入口
  |                   |-- 越狱操作入口
  |                   |-- 互抵详情入口
  |
  |-- 无授权/授权过期 -> 绑定页面 (AuthScreen)
  |                       |-- 输入卡密
  |                       |-- 绑定按钮
  |                       |-- 绑定结果
  |
  |-- 互抵/锁死 -> 互抵详情 (ClashInfoScreen)
                    |-- 互抵历史
                    |-- 锁定状态
                    |-- 剩余时间
```

---

## 五、核心功能详细设计

### 5.1 授权验证流程

#### 卡密绑定

1. 用户输入卡密 GWBHJ-XXXX-XXXX-XXXX
2. APK 通过 root 权限读取 /proc/cmdline
3. 顺序尝试: androidboot.cpuid / chipid / emmcid / oplusboot.serialno / serialno，取第一个非空值
4. APK 生成 device_hash = SHA256(hardware_id + SALT)，SALT 与 .so 中一致
5. APK 发送 POST /api/bind，body: { code, device_hash, device_info }
6. 服务端返回 license.json（Ed25519 签名）
7. APK 验签 license.json：公钥验签 -> device_hash 匹配本机 -> revoked==false -> lock_until 未过期
8. 验签通过 -> 写入 /data/adb/modules/gwbhj_jailbreak/license.json
9. 验签失败 -> 显示错误信息，不写入文件

#### 定期验证（WorkManager）

**核心原则：APK 不需要常驻后台，.so 不需要网络**

WorkManager 调度方式：
- 每 15-30 分钟触发一次 verify（WorkManager 最小间隔为 15 分钟）
- 受 Android Doze 模式约束，系统合并网络请求批次执行
- 单次请求 < 1KB，功耗可忽略
- 不需要前台 Service，不需要常驻进程
- 用户杀掉 APK 进程不影响下次调度

```kotlin
class WorkerScheduler {
    companion object {
        fun schedulePeriodicVerify() {
            val request = PeriodicWorkRequestBuilder<VerifyWorker>(15, TimeUnit.MINUTES)
                .setConstraints(Constraints.Builder()
                    .setRequiredNetworkType(NetworkType.CONNECTED)
                    .build())
                .build()
            WorkManager.getInstance(context)
                .enqueueUniquePeriodicWork(
                    "gwbhj_verify",
                    ExistingPeriodicWorkPolicy.KEEP,
                    request
                )
        }
    }
}
```

```kotlin
class VerifyWorker(context: Context, params: WorkerParameters) :
    CoroutineWorker(context, params) {

    override suspend fun doWork(): Result {
        val license = ModuleFileIO.readLicense()
        if (license == null) return Result.failure()

        val deviceHash = DeviceIdHelper.generateDeviceHash()
        val nonce = UUID.randomUUID().toString()
        val ts = System.currentTimeMillis() / 1000

        val response = ApiClient.service.verify(
            code = license.codeId,
            deviceHash = deviceHash,
            nonce = nonce,
            ts = ts
        )

        if (response.isSuccessful) {
            val newLicense = response.body()!!
            if (Ed25519Verifier.verify(newLicense)) {
                ModuleFileIO.writeLicense(newLicense)
                return Result.success()
            }
            ModuleFileIO.deleteLicense()
            return Result.failure()
        }

        // 互抵 (409) -> 删除 license + 通知
        if (response.code() == 409) {
            ModuleFileIO.deleteLicense()
            showClashNotification()
            return Result.failure()
        }

        // 网络失败 -> 走 grace period，不修改本地文件
        return Result.retry()
    }
}
```

验证触发时机：

| 场景 | 触发方式 | 功耗 | 响应速度 |
|------|---------|------|---------|
| WorkManager 定时 | 系统自动唤醒 | 极低 | ~15-30分钟 |
| 用户打开 APK | 立即 verify | 零（用户主动） | 秒级 |
| 白名单 App 启动 | .so 本地 authOk() | 无网络 | 秒级（纯本地） |

### 5.2 Grace Period 机制

**Grace period 是网络断开时的安全兜底，也是防止断网绕过验证的关键**

```
网络正常时:
  APK 定期 verify -> 更新 license.json
  .so 读本地 license.json -> authOk() -> 伪装开启
  last_server_ok 不断被服务端更新为最新时间戳

网络临时断开时 (grace period 内):
  APK verify 失败 -> 不修改本地文件
  .so 读本地 license.json:
    last_server_ok + grace_seconds(默认86400=24h) > 当前时间 -> 通过
  说明: 最后一次成功验证后，允许 24 小时离线使用

网络长期断开时 (超出 grace period):
  last_server_ok + grace_seconds < 当前时间
  -> authOk() 失败 -> DLCLOSE -> 伪装关闭
  这是刻意设计: 防止故意断网绕过验证
```

时间线示例：

```
10:00  APK verify 成功 -> last_server_ok 更新
10:15  APK verify 成功 -> last_server_ok 更新
10:30  APK verify 成功 -> last_server_ok 更新
22:00  网络断开
       .so authOk(): 10:30 + 24h = 次日10:30 > 22:00 -> 通过
次日10:30 .so authOk(): 10:30 + 24h < 当前时间 -> 失败 -> 伪装关闭
```

### 5.3 互抵即时生效机制

```
被互抵后的处理链:

1. WorkManager 定时 verify -> 服务端返回 409(被互抵)
   -> APK 删除 license.json
   -> .so 下次 authOk() 读到空文件 -> DLCLOSE -> 伪装关闭

2. 用户打开 APK -> 立即 verify -> 同上

3. 最坏情况: ~15-30 分钟生效
   最好情况: 用户打开APK或白名单App启动时秒级生效
```

APK 检测到互抵后的操作：

```kotlin
fun onClashDetected() {
    // 1. 删除 license.json（.so 下次 authOk() 必然失败）
    ModuleFileIO.deleteLicense()

    // 2. 查询互抵详情（供 UI 展示）
    val clashInfo = ApiClient.service.clashInfo(...)

    // 3. 发送通知提醒用户
    showClashNotification(clashInfo)

    // 4. 停止定时 verify（已无有效授权）
    WorkManager.getInstance(context)
        .cancelUniqueWork("gwbhj_verify")
}
```

### 5.4 文件篡改检测

APK 监控 license.json 是否被外部篡改：

```kotlin
class TamperDetector {
    companion object {
        private var lastWrittenHash: String = ""

        fun recordHash(json: String) {
            lastWrittenHash = SHA256(json)
        }

        fun detectTamper(): Boolean {
            val currentContent = ModuleFileIO.readLicenseRaw()
            if (currentContent.isEmpty()) return false
            return SHA256(currentContent) != lastWrittenHash
        }
    }
}
```

| 场景 | APK 检测 | 处理 |
|------|---------|------|
| license.json 被外部修改 | hash 不匹配 | 删除文件 + 上报服务端标记可疑 |
| license.json 被删除重建 | hash 不匹配 | 同上 |
| APK 正常更新 | hash 匹配 | 正常 |

### 5.5 完整保障链

```
APK 网络正常 -> 定期更新 license.json -> .so 随时验签通过 -> 伪装开启
APK 网络临时断 -> grace period 兜底(24h) -> 期内正常使用
APK 网络长期断 -> 超出grace -> 伪装关闭（防绕过）
目标App禁用APK网络 -> APK包名伪装，难以定向封锁
目标App杀APK后台 -> WorkManager不依赖后台进程，系统自动调度
用户手动杀APK -> 同上，无影响
license.json被篡改 -> APK检测hash变化 -> 删除+上报
被互抵 -> APK删除license.json -> .so authOk()失败 -> 伪装关闭
.so端 -> 纯本地验签，永远不需要网络
```

```kotlin
class DeviceIdHelper {
    companion object {
        private const SALT = "GWBHJ_COMPILE_TIME_SALT"  // 与 .so 一致

        fun generateDeviceHash(): String {
            val hwId = readHardwareId()
            if (hwId.isEmpty()) return ""
            return SHA256(hwId + SALT)
        }

        private fun readHardwareId(): String {
            val cmdline = RootHelper.readFile("/proc/cmdline")
            val fields = listOf(
                "androidboot.cpuid",
                "androidboot.chipid",
                "androidboot.emmcid",
                "oplusboot.serialno",
                "androidboot.serialno"
            )
            for (field in fields) {
                val value = extractField(cmdline, field)
                if (value.isNotEmpty()) return value
            }
            return ""
        }
    }
}
```

### 5.6 license.json 数据模型

```json
{
    "code_id": "GWBHJ-XXXX-XXXX-XXXX",
    "device_hash": "sha256...",
    "active": true,
    "revoked": false,
    "last_server_ok": 1719012345,
    "check_interval": 600,
    "grace_seconds": 86400,
    "lock_until": null,
    "clash_count": 0,
    "signature": "ed25519_signature_base64"
}
```

Kotlin 数据类：

```kotlin
data class License(
    val codeId: String,
    val deviceHash: String,
    val active: Boolean,
    val revoked: Boolean,
    val lastServerOk: Long,
    val checkInterval: Int,       // 秒
    val graceSeconds: Int,        // 秒
    val lockUntil: Long?,         // timestamp, null 表示未锁定
    val clashCount: Int,
    val signature: String         // Ed25519 签名 base64
)
```

### 5.7 clash_info.json 数据模型

```json
{
    "code": "GWBHJ-XXXX-XXXX-XXXX",
    "current_device_hash": "sha256...",
    "is_kicked": false,
    "kick_details": [
        {
            "time": "2026-06-22T10:00:00Z",
            "device_brand": "OPPO",
            "device_model": "Find X7",
            "device_serial": "OP9876543210",
            "device_hash_prefix": "sha256:abc...",
            "action": "踢掉了你的设备"
        }
    ],
    "lock_status": "none",
    "lock_remaining_seconds": 0,
    "next_lock_level": "1h",
    "signature": "ed25519_signature_base64"
}
```

### 5.8 Root 操作封装

```kotlin
class RootHelper {
    companion object {
        fun hasRoot(): Boolean = ShellExecutor.execute("id").isSuccess

        fun detectRootSolution(): String {
            if (ShellExecutor.execute("which apd").isSuccess) return "APatch"
            if (ShellExecutor.execute("which ksud").isSuccess) return "KernelSU"
            if (ShellExecutor.execute("which magisk").isSuccess) return "Magisk"
            return "Unknown"
        }

        fun isModuleInstalled(): Boolean =
            ShellExecutor.execute("test -d /data/adb/modules/gwbhj_jailbreak").isSuccess
    }
}
```

```kotlin
class ModuleFileIO {
    companion object {
        private const MODULE_DIR = "/data/adb/modules/gwbhj_jailbreak"

        fun readLicense(): License? {
            val json = ShellExecutor.readFile("$MODULE_DIR/license.json")
            if (json.isEmpty()) return null
            return Gson().fromJson(json, License::class.java)
        }

        fun writeLicense(license: License) {
            ShellExecutor.writeFile("$MODULE_DIR/license.json", Gson().toJson(license))
        }

        fun readWhitelist(): List<String> {
            val content = ShellExecutor.readFile("$MODULE_DIR/whitelist.txt")
            return content.lines().map { it.trim() }
                .filter { it.isNotEmpty() && !it.startsWith("#") }
        }

        fun writeWhitelist(packages: List<String>) {
            ShellExecutor.writeFile("$MODULE_DIR/whitelist.txt", packages.joinToString("\n"))
        }

        fun readSerial(): String = ShellExecutor.readFile("$MODULE_DIR/serial.txt").trim()

        fun writeSerial(serial: String) {
            ShellExecutor.writeFile("$MODULE_DIR/serial.txt", serial)
        }
    }
}
```

### 5.9 action.sh 命令行模式

action.sh 需新增命令行参数模式（无参数时保留音量键交互菜单）：

```sh
sh action.sh reset-serial      # 修改序列号
sh action.sh jailbreak         # 越狱隐藏环境
sh action.sh freeze            # 冻结多开系统软件
sh action.sh status            # 查看状态
```

APK 通过 RootHelper 执行：

```kotlin
class ActionExecutor {
    companion object {
        private const ACTION_SH = "sh /data/adb/modules/gwbhj_jailbreak/action.sh"

        fun resetSerial() = ShellExecutor.execute("$ACTION_SH reset-serial")
        fun jailbreak() = ShellExecutor.execute("$ACTION_SH jailbreak")
        fun freeze() = ShellExecutor.execute("$ACTION_SH freeze")
    }
}
```

---

## 六、服务端 API 设计

### 6.1 用户端接口

| 接口 | 方法 | 请求体 | 成功响应 | 错误码 |
|------|------|--------|----------|--------|
| /api/bind | POST | `{ code, device_hash, device_info }` | 200: license.json (Ed25519签名) | 400(无效卡密) / 409(已锁定) / 403(不匹配) / 429(永久锁定) |
| /api/verify | POST | `{ code, device_hash, nonce, ts }` | 200: license.json (Ed25519签名) | 403(不匹配) / 410(已过期) / 409(被互抵) |
| /api/clash-info | POST | `{ code, device_hash }` | 200: clash_info.json (Ed25519签名) | 403(不匹配) / 404(无记录) |

### 6.2 管理端接口

| 接口 | 方法 | 请求体 | 响应 |
|------|------|--------|------|
| /admin/generate | POST | `{ count, prefix, admin_key }` | `{ codes: [...] }` |
| /admin/delete | POST | `{ codes: [...], admin_key }` | `{ deleted: [...] }` |
| /admin/list | GET | `?admin_key=xxx` | `{ codes: [...] }` |

### 6.3 请求安全

- 每次请求包含 nonce + ts（防重放攻击）
- 服务端校验 ts 在 300s 内
- nonce 为 UUID，服务端缓存 5 分钟已使用 nonce
- 限速：同一 device_hash 每分钟最多 5 次请求
- 管理端接口需要 admin_key 认证

---

## 七、互抵和锁死机制

### 7.1 互抵规则

- 设备 A 使用卡密 -> 验证通过 -> A 成为活跃设备
- 设备 B 使用同一卡密 -> 验证通过 -> B 成为活跃设备，A 被互抵
- 设备 A 下次验证 -> 返回异地登录信息 -> 关闭所有伪装功能

### 7.2 锁死规则

| 连续互抵次数 | 锁定时长 |
|-------------|---------|
| 1-3 次 | 1 小时 |
| 4-5 次 | 4 小时 |
| 6-8 次 | 1 天 |
| 9 次 | 1 周 |
| 10 次 | 永久锁定 |

锁死期间：不允许绑定、不允许验证通过、显示剩余时间。

### 7.3 APK 互抵提示

```
[!] 卡密异地登录，请妥善管理卡密
[!] 连续互抵将导致卡密锁死
连续互抵次数: 2/3 (锁死: 1小时)
[查看互抵详情]
```

---

## 八、APK 隐藏性设计

### 8.1 包名伪装

- 包名: `com.hw.security.service`
- App 名称: "系统服务" 或其他无关名称
- 图标: 伪装为普通工具图标或不显示图标

### 8.2 隐藏模式

- 默认无 launcher icon（通过 intent 启动）
- 快捷入口：通知点击 / 特定 dialer code / 快捷方式
- 运行时可切换显示/隐藏 launcher icon

### 8.3 最少权限

- 仅声明 `INTERNET`（用于网络验证）
- 不声明任何可疑权限

---

## 九、.so 端改动

### 9.1 需要新增的功能

```cpp
// 设备 ID 读取（从 /proc/cmdline）
static std::string readHardwareId();

// device_hash 生成
static std::string generateDeviceHash();

// license.json 验签
static bool verifyLicenseSignature(const std::string& json, const std::string& sig);

// 授权检查
static bool authOk() {
    // 1. 读取 license.json
    // 2. Ed25519 验签
    // 3. 校验 device_hash 是否匹配本机
    // 4. 校验 revoked == false
    // 5. 校验 lock_until 是否过期
    // 6. 校验 last_server_ok + grace_seconds 是否有效
    // 全部通过才返回 true
}

// preAppSpecialize 中的新逻辑
void preAppSpecialize(...) {
    if (!authOk()) {
        api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
        return;
    }
    // 授权通过后才执行白名单、设备伪装、CPU 伪装
    // ... 原有逻辑 ...
}
```

### 9.2 编译时内置

- 服务端公钥（Ed25519）
- SALT 常量（与 APK 中一致）
- device_hash 生成逻辑
- license 验签逻辑

### 9.3 license.json 读取路径

`/data/adb/modules/gwbhj_jailbreak/license.json`

---

## 十、action.sh 改动

### 10.1 新增命令行参数模式

```sh
#!/system/bin/sh
# GWBHJ action.sh - 支持命令行参数和音量键交互

MODULE_DIR="/data/adb/modules/gwbhj_jailbreak"
SERIAL_FILE="$MODULE_DIR/serial.txt"
FREEZE_LIST="$MODULE_DIR/freeze_list.txt"

# 命令行参数模式
if [ $# -gt 0 ]; then
    case "$1" in
        reset-serial) reset_serial ;;
        jailbreak)    jailbreak ;;
        freeze)       freeze_apps ;;
        status)       show_status ;;
        *)            echo "Unknown command: $1"; exit 1 ;;
    esac
    exit 0
fi

# 无参数时进入音量键交互菜单（保留原有逻辑）
# ... 原有交互菜单代码 ...
```

### 10.2 新增 status 命令

```sh
show_status() {
    echo "Module: GWBHJ v1.0.15"
    echo "Root: $(detect_root)"
    echo "Serial: $(cat $SERIAL_FILE 2>/dev/null || echo 'not set')"
    echo "Whitelist: $(wc -l < $MODULE_DIR/whitelist.txt 2>/dev/null) packages"
    if [ -f "$MODULE_DIR/license.json" ]; then
        echo "License: active"
    else
        echo "License: not bound"
    fi
}
```

---

## 十一、模块端文件规划

```
/data/adb/modules/gwbhj_jailbreak/
├── zygisk/
│   └── arm64-v8a.so           # Zygisk companion (.so)
├── action.sh                  # 交互菜单 + 命令行模式
├── service.sh                  # 开机服务
├── uninstall.sh                # 卸载清理
├── whitelist.txt               # 白名单
├── freeze_list.txt             # 冻结列表
├── serial.txt                  # 序列号
├── license.json                # 授权文件（APK 写入，.so 读取验签）
├── clash_info.json             # 互抵详情（APK 写入，服务端签名）
├── module.prop                 # 模块属性
└── META-INF/com/google/android/
    ├── update-binary            # 安装入口
    └── updater-script           # #!/sbin/sh; exit 0
```

---

## 十二、开发阶段规划

### Phase 1: 基础框架（1-2 周）

- [ ] 创建 Android 项目（Kotlin + Compose + Gradle）
- [ ] 实现 root 权限检测和 Shell 执行（libsu）
- [ ] 实现模块目录文件读写（ModuleFileIO）
- [ ] 实现主页面框架（导航、主题）
- [ ] 实现 root/模块检测页面

### Phase 2: 授权系统（2-3 周）

- [ ] 实现设备 ID 生成（DeviceIdHelper）
- [ ] 实现 SHA256 device_hash（HashGenerator）
- [ ] 实现 Ed25519 验签（Ed25519Verifier）
- [ ] 实现卡密绑定 UI（AuthScreen）
- [ ] 实现定期验证逻辑（LicenseManager）
- [ ] 实现 WorkManager VerifyWorker + WorkerScheduler
- [ ] 实现 grace period 处理
- [ ] 实现文件篡改检测（TamperDetector）
- [ ] 实现互抵即时生效逻辑（删除 license + 通知）

### Phase 3: 服务端 API（2-3 周）

- [ ] 实现 EdgeOne Edge Functions
- [ ] 实现 /api/bind
- [ ] 实现 /api/verify
- [ ] 实现 /api/clash-info
- [ ] 实现 Ed25519 签名（服务端私钥）
- [ ] 实现 nonce/限速/日志
- [ ] 实现 KV 存储

### Phase 4: 管理后台（1-2 周）

- [ ] 实现 /admin/generate
- [ ] 实现 /admin/delete
- [ ] 实现 /admin/list
- [ ] 实现管理后台 UI（可选）

### Phase 5: 状态面板（1-2 周）

- [ ] 实现授权状态卡片
- [ ] 实现设备信息卡片
- [ ] 实现序列号管理 UI
- [ ] 实现互抵/锁死详情 UI（ClashInfoScreen）

### Phase 6: 功能管理（1-2 周）

- [ ] 实现白名单管理 UI（WhitelistScreen）
- [ ] 实现冻结列表管理 UI（FreezeListScreen）
- [ ] 实现 action.sh 命令行参数模式
- [ ] 实现越狱/冻结操作 UI

### Phase 7: .so 端改动（2-3 周）

- [ ] 实现设备 ID 读取函数（C++）
- [ ] 实现 license 验签逻辑（C++）
- [ ] 实现 authOk() 并接入 Zygisk 拦截
- [ ] 内置服务端公钥和 SALT
- [ ] 编译测试所有 ABI

### Phase 8: 集成测试（1-2 周）

- [ ] APK + .so 联调测试
- [ ] 互抵场景测试（被踢后 15-30 分钟内生效）
- [ ] grace period 测试（24h 离线兜底 + 超期自动关闭）
- [ ] 网络异常测试（断网、APK被杀、WorkManager调度）
- [ ] 文件篡改检测测试
- [ ] 隐藏性测试（目标 App 扫描）
- [ ] 功耗测试（WorkManager 定时验证功耗监测）

### Phase 9: 发布（1 周）

- [ ] GitHub Actions CI 配置
- [ ] APK 签名 + 混淆
- [ ] 模块 ZIP 打包
- [ ] 文档更新
- [ ] 发布

**预计总工期: 12-16 周**

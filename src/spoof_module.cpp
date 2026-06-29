/* ============================================================================
 *  过强标黑机 (GWBHJ) — Hardcoded device & CPU spoof module for Android
 *  Based on COPG v5.4.0 by Alireza Parsi
 *  Simplified: hardcoded config, whitelist-based, separate SERIAL/ANDROID_ID control
 *
 *  Default: COW prop spoof ON, CPU mount ON, ANDROID_ID ON (for whitelisted apps)
 *  Tag (colon-suffix in whitelist.txt):
 *    :blocked   → force unmount cpuinfo (force real CPU for sensitive apps)
 * ========================================================================== */
#include <jni.h>
#include <string>
#include <zygisk.hpp>
#include <fstream>
#include <unordered_set>
#include <dlfcn.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <unistd.h>
#include <android/log.h>
#include <mutex>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <vector>
#include <fcntl.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cerrno>
#include <sys/system_properties.h>

#ifdef DEBUG
#define LOG_TAG "GWBHJModule"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#else
#define LOGI(...) ((void)0)
#define LOGW(...) ((void)0)
#define LOGE(...) ((void)0)
#endif

// ─────────────────────────────────────────
// String obfuscation (anti-strings scan)
// Compile-time XOR → .rodata holds ciphertext only; decoded to stack at runtime
// ─────────────────────────────────────────
namespace obf {
    constexpr uint8_t K = 0x5A;
    template<size_t N>
    struct Str {
        char d[N];
        constexpr Str(const char (&s)[N]) : d{} {
            for (size_t i = 0; i < N; i++) d[i] = s[i] ^ K;
        }
    };
    template<size_t N>
    static inline std::string dec(const Str<N>& s) {
        char buf[N];
        for (size_t i = 0; i < N; i++) buf[i] = s.d[i] ^ K;
        return std::string(buf, N - 1);
    }
}
#define OBF(s) ([]() -> std::string { \
    constexpr auto _e = obf::Str<sizeof(s)>(s); \
    return obf::dec(_e); \
}())

// ─────────────────────────────────────────
// Paths (obfuscated — decoded once at static init, no plaintext in .rodata)
// ─────────────────────────────────────────
static const std::string g_module_dir      = OBF("/data/adb/modules/gwbhj_jailbreak");
static const std::string g_whitelist_path  = OBF("/data/adb/modules/gwbhj_jailbreak/whitelist.txt");
static const std::string g_serial_path     = OBF("/data/adb/modules/gwbhj_jailbreak/serial.txt");
static const std::string g_android_id_path = OBF("/data/adb/modules/gwbhj_jailbreak/android_id.txt");
static const std::string g_cpuinfo_path    = OBF("/data/adb/modules/gwbhj_jailbreak/.c");

// ─────────────────────────────────────────
// Hardcoded device profile (obfuscated values — decoded at static init)
// Huawei PLU-AL10 / HWPLU (build.prop) + Kirin 9020 (real cpuinfo)
// Anti-leak: duplicate values reference a single OBF() literal
// ─────────────────────────────────────────
static const std::string g_dev_brand         = OBF("HUAWEI");
static const std::string g_dev_manufacturer  = g_dev_brand;             // HUAWEI
static const std::string g_dev_device        = OBF("HWPLU");
static const std::string g_dev_model         = OBF("PLU-AL10");
static const std::string g_dev_product       = g_dev_model;             // PLU-AL10
static const std::string g_dev_board         = OBF("PLU");
static const std::string g_dev_fingerprint   = OBF("HUAWEI/HWPLU/PLU-AL10:12/HUAWEIPLU-AL10/104.3.0.198C00:user/release-keys");
static const std::string g_dev_hardware      = OBF("kirin9020");
static const std::string g_dev_platform      = OBF("HiSilicon");
static const std::string g_dev_chipname      = OBF("Kirin 9020");
static const std::string g_dev_build_id      = OBF("HUAWEIPLU-AL10");
static const std::string g_dev_display_id    = g_dev_build_id;          // HUAWEIPLU-AL10
static const std::string g_dev_build_tags    = OBF("release-keys");
static const std::string g_dev_build_type    = OBF("user");
static const std::string g_dev_release       = OBF("12");
static const std::string g_dev_sdk           = OBF("31");
static const std::string g_dev_vndk_version  = g_dev_sdk;               // 31
static const std::string g_dev_security_patch = OBF("2021-10-05");
static const std::string g_dev_incremental   = OBF("104.3.0.198C00");
static const std::string g_dev_description   = OBF("PLU-AL10-user 104.3.0 HUAWEIPLU-AL10 198-CHN-LGRP1 release-keys");
static const std::string g_dev_gpu           = OBF("Maleoon 920");
static const std::string g_dev_cpu_abi       = OBF("arm64-v8a");
static const std::string g_dev_fingerprint_name = OBF("HUAWEI-Z198");
static const std::string g_dev_hw_version    = OBF("HL1") + OBF("XHDM");
static const std::string g_dev_hw_sku        = g_dev_model;             // PLU-AL10
static const std::string g_dev_ab_ota        = OBF("system");
static const std::string g_dev_characteristics = OBF("default");

struct PropOverride {
    const char* name;
    std::string value;
};

// 普通属性 — 对所有白名单应用伪装(不含版本号/VNDK/ABI 列表)
static const std::vector<PropOverride> PROP_OVERRIDES = {
    {"ro.product.model",             g_dev_model},
    {"ro.product.brand",             g_dev_brand},
    {"ro.product.manufacturer",      g_dev_manufacturer},
    {"ro.product.device",            g_dev_device},
    {"ro.product.name",              g_dev_product},
    {"ro.product.board",             g_dev_board},
    {"ro.build.fingerprint",         g_dev_fingerprint},
    {"ro.build.id",                  g_dev_build_id},
    {"ro.build.display.id",          g_dev_display_id},
    {"ro.build.tags",                g_dev_build_tags},
    {"ro.build.type",                g_dev_build_type},
    {"ro.build.description",         g_dev_description},
    {"ro.build.characteristics",     g_dev_characteristics},
    {"ro.build.version.incremental", g_dev_incremental},
    {"ro.board.platform",            g_dev_platform},
    {"ro.hardware",                  g_dev_hardware},
    {"ro.hardware.chipname",         g_dev_chipname},
    {"ro.soc.manufacturer",          g_dev_brand},
    {"ro.soc.model",                 g_dev_chipname},
    {"ro.product.fingerprintName",   g_dev_fingerprint_name},
    {"ro.product.hardwareversion",   g_dev_hw_version},
    {"ro.boot.product.hardware.sku", g_dev_hw_sku},
    {"ro.product.ab_ota_partitions", g_dev_ab_ota},
    {"ro.product.system.model",      g_dev_model},
    {"ro.product.system.brand",      g_dev_brand},
    {"ro.product.system.device",     g_dev_device},
    {"ro.product.system.name",       g_dev_product},
    {"ro.product.system.manufacturer", g_dev_manufacturer},
    {"ro.product.vendor.model",      g_dev_model},
    {"ro.product.vendor.brand",      g_dev_brand},
    {"ro.product.vendor.device",     g_dev_device},
    {"ro.product.vendor.name",       g_dev_product},
    {"ro.product.vendor.manufacturer", g_dev_manufacturer},
    {"ro.product.cpu.abi",           g_dev_cpu_abi},
    {"ro.hardware.gpu",              g_dev_gpu},
    {"ro.gpu.vendor",                g_dev_brand},
    {"ro.gpu.model",                 g_dev_gpu},
    {"ro.opengles.version",          "196610"},
};
static const size_t PROP_OVERRIDE_COUNT = PROP_OVERRIDES.size();

// 版本号相关属性 — 仅对腾讯游戏类应用(三角洲等)动态伪装。
// 其它应用(酷安/设备信息类)读到伪造 SDK 后调用不存在的 API → NoSuchMethodError 闪退,
// 因此这类属性不进默认 PROP_OVERRIDES,由 isTencentGamePackage() 判断后单独应用。
static const std::vector<PropOverride> PROP_OVERRIDES_VERSION = {
    {"ro.build.version.release",        g_dev_release},
    {"ro.build.version.sdk",            g_dev_sdk},
    {"ro.build.version.security_patch", g_dev_security_patch},
    {"ro.product.vndk.version",         g_dev_vndk_version},
    {"ro.product.cpu.abilist",          g_dev_cpu_abi},
    {"ro.product.cpu.abilist64",        g_dev_cpu_abi},
};
static const size_t PROP_OVERRIDE_VERSION_COUNT = PROP_OVERRIDES_VERSION.size();

// ─────────────────────────────────────────
// /proc/cpuinfo — Kirin 9020: 2 big (0xd4f) + 6 little (0xd46) = 8 cores
// ─────────────────────────────────────────
static const char CPU_FEATURES[] =
    "fp asimd evtstrm aes pmull sha1 sha2 crc32 atomics fphp asimdhp cpuid "
    "asimdrdm jscvt fcma lrcpc dcpop sha3 sm3 sm4 asimddp sha512 sve asimdfhm "
    "dit uscat ilrcpc flagm ssbs sb paca pacg dcpodp sve2 sveaes svepmull "
    "svebitperm svesha3 svesm4 flagm2 frint svei8mm svebf16 i8mm bf16 dgh";

struct CpuCore { uint16_t part; uint8_t revision; };

static const CpuCore CPU_CORES[] = {
    {0xd4f, 0}, {0xd4f, 0},                                         // 2 big
    {0xd46, 0}, {0xd46, 0}, {0xd46, 0}, {0xd46, 0}, {0xd46, 0}, {0xd46, 0}, // 6 little
};
static constexpr size_t CPU_CORE_COUNT = sizeof(CPU_CORES) / sizeof(CPU_CORES[0]);

static std::string buildCpuinfo() {
    std::string out;
    out.reserve(2048);
    for (size_t i = 0; i < CPU_CORE_COUNT; i++) {
        char buf[400];
        snprintf(buf, sizeof(buf),
            "processor\t: %zu\n"
            "BogoMIPS\t: 3.84\n"
            "Features\t: %s\n"
            "CPU implementer\t: 0x48\n"
            "CPU architecture: 8\n"
            "CPU variant\t: 0x1\n"
            "CPU part\t: 0x%x\n"
            "CPU revision\t: %u\n\n",
            i, CPU_FEATURES, CPU_CORES[i].part, CPU_CORES[i].revision);
        out += buf;
    }
    out += OBF("Hardware\t: Kirin 9020\n");
    return out;
}

// ─────────────────────────────────────────
// Whitelist cache: package_name -> is_blocked
// ─────────────────────────────────────────
static std::unordered_set<std::string> g_whitelist;
static std::unordered_set<std::string> g_blocked_packages;
static std::string g_serial;
static std::string g_android_id;
static std::mutex g_data_mutex;
static time_t g_whitelist_mtime = 0;
static time_t g_serial_mtime = 0;
static time_t g_android_id_mtime = 0;
static bool g_whitelist_loaded = false;

static std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

static std::string parsePackageName(const std::string& line) {
    std::string entry = trim(line);
    size_t colon = entry.find(':');
    if (colon != std::string::npos) return entry.substr(0, colon);
    return entry;
}

static bool hasBlockedTag(const std::string& line) {
    std::string entry = trim(line);
    size_t first_colon = entry.find(':');
    if (first_colon == std::string::npos) return false;

    std::string tags_part = entry.substr(first_colon + 1);
    size_t start = 0;
    while (start < tags_part.length()) {
        size_t end = tags_part.find(':', start);
        std::string tag;
        if (end == std::string::npos) {
            tag = tags_part.substr(start);
            start = tags_part.length();
        } else {
            tag = tags_part.substr(start, end - start);
            start = end + 1;
        }
        if (trim(tag) == "blocked") return true;
    }
    return false;
}

static void loadHardcodedWhitelist() {
    std::lock_guard<std::mutex> lock(g_data_mutex);
    g_whitelist.insert(OBF("com.tencent.tmgp.dfm"));
    g_whitelist.insert(OBF("com.liuzh.deviceinfo"));
    g_whitelist.insert(OBF("com.coolapk.market"));
    g_whitelist_loaded = true;
    LOGI("Hardcoded whitelist loaded: %zu packages", g_whitelist.size());
}

static std::string readLineFromFile(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return "";
    std::string line;
    std::getline(f, line);
    return trim(line);
}

static void reloadFileIfNeeded(const std::string& path, time_t& mtime, std::string& dest) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) return;
    if (st.st_mtime == mtime && !dest.empty()) return;
    std::string val = readLineFromFile(path);
    if (val.empty()) return;
    std::lock_guard<std::mutex> lock(g_data_mutex);
    dest = std::move(val);
    mtime = st.st_mtime;
    LOGI("Reloaded %s", path.c_str());
}

static void reloadWhitelistIfNeeded() {
    struct stat st;
    if (stat(g_whitelist_path.c_str(), &st) != 0) {
        if (!g_whitelist_loaded) loadHardcodedWhitelist();
        return;
    }
    if (st.st_mtime == g_whitelist_mtime && g_whitelist_loaded) return;

    std::ifstream f(g_whitelist_path);
    if (!f.is_open()) {
        if (!g_whitelist_loaded) loadHardcodedWhitelist();
        return;
    }

    std::unordered_set<std::string> new_list;
    std::unordered_set<std::string> new_blocked;
    std::string line;
    while (std::getline(f, line)) {
        std::string trimmed = trim(line);
        if (trimmed.empty() || trimmed[0] == '#') continue;
        std::string pkg = parsePackageName(trimmed);
        if (pkg.empty()) continue;
        new_list.insert(pkg);
        if (hasBlockedTag(trimmed)) {
            new_blocked.insert(pkg);
        }
    }

    if (new_list.empty()) {
        if (!g_whitelist_loaded) loadHardcodedWhitelist();
        return;
    }

    std::lock_guard<std::mutex> lock(g_data_mutex);
    g_whitelist = std::move(new_list);
    g_blocked_packages = std::move(new_blocked);
    g_whitelist_mtime = st.st_mtime;
    g_whitelist_loaded = true;
    LOGI("Whitelist reloaded: %zu packages, %zu blocked",
         g_whitelist.size(), g_blocked_packages.size());
}

// ─────────────────────────────────────────
// Companion (root) — syscalls, no fork+exec
// ─────────────────────────────────────────
static void writeCpuinfoSpoof() {
    std::string content = buildCpuinfo();
    int fd = open(g_cpuinfo_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) { LOGE("cpuinfo write fail: %s", strerror(errno)); return; }
    write(fd, content.c_str(), content.size());
    close(fd);
    chmod(g_cpuinfo_path.c_str(), 0644);
    LOGI("cpuinfo spoof written (%zu bytes)", content.size());
}

// Companion protocol: single byte — 0x00 = unmount, 0x01 = mount. No string cmds.
static void companion(int fd) {
    uint8_t cmd = 0xFF;
    ssize_t bytes = read(fd, &cmd, 1);
    if (bytes <= 0) { close(fd); return; }

    int result = -1;
    if (cmd == 0x00) {                                    // unmount
        umount("/proc/cpuinfo");
        unlink(g_cpuinfo_path.c_str());
        result = 0;
        LOGI("[COMPANION] CPU unmount");
    } else if (cmd == 0x01) {                             // mount
        writeCpuinfoSpoof();
        umount("/proc/cpuinfo");
        result = mount(g_cpuinfo_path.c_str(), "/proc/cpuinfo", "none", MS_BIND, nullptr);
        if (result == 0) unlink(g_cpuinfo_path.c_str());
        LOGI("[COMPANION] CPU mount (result=%d)", result);
    }
    write(fd, &result, sizeof(result));
    close(fd);
}

static bool isWhitelistedProcess(const std::string& process_name) {
    if (g_whitelist.count(process_name)) return true;
    size_t colon = process_name.find(':');
    if (colon == std::string::npos) return false;
    return g_whitelist.count(process_name.substr(0, colon));
}

static bool isBlockedPackage(const std::string& process_name) {
    if (g_blocked_packages.count(process_name)) return true;
    size_t colon = process_name.find(':');
    if (colon == std::string::npos) return false;
    return g_blocked_packages.count(process_name.substr(0, colon));
}

// ─────────────────────────────────────────
// 腾讯游戏类应用识别 — 仅这类应用需要伪装安卓版本
// 三角洲行动 (com.tencent.tmgp.dfm) 等腾讯系游戏的包名均以 com.tencent.tmgp 开头,
// 对这类应用伪装 SDK / release / security_patch / VNDK / abilist 可绕过机型检测;
// 其它应用(酷安/设备信息/银行类)读到伪造版本号会触发 NoSuchMethodError 或 dlopen
// 失败 → 闪退,因此版本号伪装只对这类应用启用(动态伪装策略)。
// ─────────────────────────────────────────
static bool isTencentGamePackage(const std::string& process_name) {
    std::string base = process_name;
    size_t colon = base.find(':');
    if (colon != std::string::npos) base = base.substr(0, colon);
    // com.tencent.tmgp.* — 腾讯手游通系列(三角洲、和平精英、王者等)
    if (base.rfind(OBF("com.tencent.tmgp"), 0) == 0) return true;
    return false;
}

// ─────────────────────────────────────────
// COW Prop Spoof (stealth — per-process, zero-residency)
// Default ON for all whitelisted apps
// ─────────────────────────────────────────
static std::vector<std::pair<uintptr_t, uintptr_t>> g_priv_prop_ranges;

static bool ensurePropAreaPrivate(const void* addr) {
    uintptr_t t = (uintptr_t)addr;
    for (auto& r : g_priv_prop_ranges)
        if (t >= r.first && t < r.second) return true;

    FILE* f = fopen("/proc/self/maps", "r");
    if (!f) return false;
    char line[512];
    bool ok = false;
    while (fgets(line, sizeof(line), f)) {
        uintptr_t s, e; unsigned long long off; char perms[8]; char path[256];
        path[0] = 0;
        if (sscanf(line, "%lx-%lx %7s %llx %*x:%*x %*u %255[^\n]",
                   &s, &e, perms, &off, path) < 4) continue;
        if (t < s || t >= e) continue;
        char* p = path; while (*p == ' ') p++;
        if (strncmp(p, "/dev/__properties__", 19) != 0) break;
        int pfd = open(p, O_RDONLY);
        if (pfd >= 0) {
            void* r = mmap((void*)s, (size_t)(e - s), PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_FIXED, pfd, (off_t)off);
            close(pfd);
            if (r != MAP_FAILED) { g_priv_prop_ranges.push_back({s, e}); ok = true; }
        }
        break;
    }
    fclose(f);
    return ok;
}

static void forgeProp(const char* name, const char* val) {
    const prop_info* cpi = __system_property_find(name);
    if (!cpi) { LOGW("[PROP] %s: not found, skip", name); return; }
    size_t len = strlen(val);
    if (len >= PROP_VALUE_MAX) { LOGW("[PROP] %s: value len %zu >= %d (long prop), skip", name, len, PROP_VALUE_MAX); return; }
    if (!ensurePropAreaPrivate(cpi)) { LOGE("[PROP] %s: COW remap failed", name); return; }
    volatile uint32_t* serial = (volatile uint32_t*)cpi;
    char* value = (char*)cpi + sizeof(uint32_t);
    uint32_t old = *serial;
    // LONG property (bionic kLongFlag = 1<<16, Android 12+): the value is NOT inline —
    // prop_info.value[] holds a uint32_t offset to the real value elsewhere in the prop
    // area. Overwriting it inline (below) clobbers that offset, and since we keep the
    // low 24 bits (incl. kLongFlag) bionic still treats it as long → reads area+offset →
    // strlen() walks a garbage pointer → SIGSEGV in SystemProperties::Read. Can't edit a
    // long prop in place; skip it (fail-safe: real value stays, Build.* JNI field still
    // spoofs the Java side). The earlier len>=PROP_VALUE_MAX guard only checks the NEW
    // value — this guards the EXISTING prop being long.
    if (old & 0x00010000u) { LOGW("[PROP] %s: long property, skip (inline edit unsafe)", name); return; }
    *serial = old | 1;                                   // mark dirty (readers retry)
    __sync_synchronize();
    memcpy(value, val, len); value[len] = '\0';
    __sync_synchronize();
    *serial = ((uint32_t)len << 24) | (((old & 0x00FFFFFFu) + 2) & 0x00FFFFFFu);  // len + bumped gen, bit0=0
    __sync_synchronize();
    char rb[PROP_VALUE_MAX] = {0};
    __system_property_get(name, rb);
    LOGI("[PROP] %s -> '%s' (read-back '%s')", name, val, rb);
}

// ─────────────────────────────────────────
// Per-app ANDROID_ID derivation (from COPG v5.4.0)
// ─────────────────────────────────────────
// ANDROID_ID on Android 8+ is scoped per app-signing-key: two different apps
// on the SAME real device read DIFFERENT ids. A single shared fake across every
// whitelisted app is therefore detectable by any backend that correlates ids
// across apps (shared anti-cheat / analytics SDK). So instead of applying the
// seed verbatim we mix seed+":"+base_package into a stable 64-bit value
// (FNV-1a 64 + splitmix64 avalanche) and emit 16 lowercase hex — distinct per
// app, identical across reboots (no RNG), and shared by an app's main +
// :proc children (both pass the same base_package). Empty seed -> caller skips.
static std::string deriveAndroidId(const std::string& seed, const std::string& package_name) {
    std::string in = seed;
    in.push_back(':');
    in += package_name;
    unsigned long long h = 14695981039346656037ULL;     // FNV-1a 64 offset basis
    for (unsigned char c : in) { h ^= c; h *= 1099511628211ULL; }
    // splitmix64 finalizer — avalanche so seed+pkg pairs scatter across the range
    h += 0x9E3779B97F4A7C15ULL;
    h = (h ^ (h >> 30)) * 0xBF58476D1CE4E5B9ULL;
    h = (h ^ (h >> 27)) * 0x94D049BB133111EBULL;
    h =  h ^ (h >> 31);
    char buf[17];
    snprintf(buf, sizeof(buf), "%016llx", h);
    return std::string(buf, 16);
}

// ─────────────────────────────────────────
// ANDROID_ID Spoof (from COPG v5.4.0)
// Synchronous forge via Settings$Secure cache.
// Executed in postAppSpecialize where the app uid is available
// for MemoryIntArray ashmem access in the app's SELinux domain.
// ─────────────────────────────────────────
static void forgeAndroidId(JNIEnv* env, const char* fakeId) {
    if (!fakeId || !*fakeId) return;
    auto clr = [&]{ if (env->ExceptionCheck()) env->ExceptionClear(); };

    jclass secCls = env->FindClass("android/provider/Settings$Secure");           clr();
    jclass nvcCls = env->FindClass("android/provider/Settings$NameValueCache");   clr();
    jclass gtCls  = env->FindClass("android/provider/Settings$GenerationTracker");clr();
    jclass miaCls = env->FindClass("android/util/MemoryIntArray");                clr();
    if (!secCls || !nvcCls || !gtCls || !miaCls) {
        LOGE("[AID] class resolve fail (sec=%p nvc=%p gt=%p mia=%p)", secCls, nvcCls, gtCls, miaCls);
        return;
    }

    jfieldID cacheFld  = env->GetStaticFieldID(secCls, "sNameValueCache", "Landroid/provider/Settings$NameValueCache;"); clr();
    jfieldID valuesFld = env->GetFieldID(nvcCls, "mValues", "Landroid/util/ArrayMap;");                                  clr();
    jfieldID tracksFld = env->GetFieldID(nvcCls, "mGenerationTrackers", "Landroid/util/ArrayMap;");                      clr();
    if (!cacheFld || !valuesFld || !tracksFld) {
        LOGE("[AID] field resolve fail (cache=%p values=%p tracks=%p)", cacheFld, valuesFld, tracksFld);
        return;
    }

    jobject cache = env->GetStaticObjectField(secCls, cacheFld); clr();
    if (!cache) { LOGE("[AID] sNameValueCache null"); return; }
    jobject values = env->GetObjectField(cache, valuesFld); clr();
    jobject tracks = env->GetObjectField(cache, tracksFld); clr();
    if (!values || !tracks) { LOGE("[AID] maps null (values=%p tracks=%p)", values, tracks); return; }

    jmethodID miaCtor = env->GetMethodID(miaCls, "<init>", "(I)V"); clr();
    jmethodID miaSet  = env->GetMethodID(miaCls, "set", "(II)V");   clr();
    jobject mia = miaCtor ? env->NewObject(miaCls, miaCtor, (jint)1) : nullptr; clr();
    if (!mia) { LOGE("[AID] MemoryIntArray create fail"); return; }
    if (miaSet) { env->CallVoidMethod(mia, miaSet, (jint)0, (jint)0); clr(); }

    jstring nameStr = env->NewStringUTF(OBF("android_id").c_str());
    jstring fakeStr = env->NewStringUTF(fakeId);

    jclass amCls = env->GetObjectClass(tracks);
    jmethodID putMid = env->GetMethodID(amCls, "put", "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;"); clr();
    env->DeleteLocalRef(amCls);
    if (!putMid) { LOGE("[AID] ArrayMap.put not found"); return; }

    jobject tracker = nullptr;
    jobject trackerKey = nullptr;

    jclass keyCls = env->FindClass("android/provider/Settings$GenerationTracker$Key"); clr();
    if (keyCls) {
        jmethodID keyCtor = env->GetMethodID(keyCls, "<init>", "(Ljava/lang/String;I)V"); clr();
        jmethodID gtCtorK = env->GetMethodID(gtCls, "<init>",
            "(Landroid/provider/Settings$GenerationTracker$Key;Landroid/util/MemoryIntArray;IILjava/util/function/Consumer;)V"); clr();
        if (keyCtor && gtCtorK) {
            jobject kobj = env->NewObject(keyCls, keyCtor, nameStr, (jint)0); clr();
            if (kobj) {
                tracker = env->NewObject(gtCls, gtCtorK, kobj, mia, (jint)0, (jint)0, (jobject)nullptr); clr();
                if (tracker) trackerKey = kobj; else env->DeleteLocalRef(kobj);
            }
        }
    }
    if (!tracker) {
        jmethodID gtCtorS = env->GetMethodID(gtCls, "<init>",
            "(Ljava/lang/String;Landroid/util/MemoryIntArray;IILjava/util/function/Consumer;)V"); clr();
        if (gtCtorS) { tracker = env->NewObject(gtCls, gtCtorS, nameStr, mia, (jint)0, (jint)0, (jobject)nullptr); clr(); }
        trackerKey = nameStr;
    }
    if (!tracker) { LOGE("[AID] GenerationTracker create fail (both ABIs)"); return; }

    { jobject p = env->CallObjectMethod(values, putMid, nameStr, fakeStr); clr(); if (p) env->DeleteLocalRef(p); }
    if (keyCls && trackerKey != nameStr) {
        jobject pv = env->CallObjectMethod(values, putMid, trackerKey, fakeStr); clr(); if (pv) env->DeleteLocalRef(pv);
    }
    { jobject pt = env->CallObjectMethod(tracks, putMid, trackerKey, tracker); clr(); if (pt) env->DeleteLocalRef(pt); }

    LOGI("[AID] forged android_id -> %s", fakeId);
}

// ─────────────────────────────────────────
// Main Module
// ─────────────────────────────────────────
class _Z : public zygisk::ModuleBase {
public:
    void onLoad(zygisk::Api* api, JNIEnv* env) override {
        this->api = api;
        this->env = env;
        LOGI("GWBHJ loaded (COPG v5.4.0 based)");
    }

    void preAppSpecialize(zygisk::AppSpecializeArgs* args) override {
        if (!args || !args->nice_name) {
            api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        const char* package_name = env->GetStringUTFChars(args->nice_name, nullptr);
        if (!package_name) {
            api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        std::string pkg(package_name);
        env->ReleaseStringUTFChars(args->nice_name, package_name);

        // base_package = pkg without the ":proc" suffix. ANDROID_ID on Android 8+
        // is scoped per app-signing-key, so an app's main process and its
        // :remote/:proc children must share the SAME id. deriveAndroidId() mixes
        // the seed with base_package, so feeding the base (not the full nice_name)
        // makes every process of one app converge on one id while keeping it
        // distinct across apps.
        std::string base_package = pkg;
        size_t colon = base_package.find(':');
        if (colon != std::string::npos) base_package = base_package.substr(0, colon);

        LOGI("Processing: %s", pkg.c_str());

        reloadWhitelistIfNeeded();
        reloadFileIfNeeded(g_serial_path, g_serial_mtime, g_serial);
        reloadFileIfNeeded(g_android_id_path, g_android_id_mtime, g_android_id);

        bool in_whitelist;
        bool blocked;
        std::string current_serial;
        std::string current_android_id;
        {
            std::lock_guard<std::mutex> lock(g_data_mutex);
            in_whitelist = isWhitelistedProcess(pkg);
            blocked = isBlockedPackage(pkg);
            current_serial = g_serial;
            current_android_id = g_android_id;
        }

        if (!in_whitelist) {
            LOGI("%s: not in whitelist, skip", pkg.c_str());
            api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        LOGI("%s: in whitelist [blocked=%d], applying spoof", pkg.c_str(), blocked);

        // 动态伪装:仅对腾讯游戏类应用(三角洲等)伪装安卓版本号相关属性。
        // 其它应用(酷安/设备信息类)读伪造 SDK 会 NoSuchMethodError 闪退,故跳过。
        bool spoof_version = isTencentGamePackage(pkg);
        LOGI("%s: version spoof = %d", pkg.c_str(), spoof_version);

        buildClass = nullptr;
        versionClass = nullptr;
        modelField = brandField = deviceField = manufacturerField = nullptr;
        fingerprintField = productField = boardField = hardwareField = nullptr;
        idField = displayField = serialField = nullptr;

        ensureBuildClass();
        spoofDevice(current_serial);

        for (size_t i = 0; i < PROP_OVERRIDE_COUNT; i++) {
            forgeProp(PROP_OVERRIDES[i].name, PROP_OVERRIDES[i].value.c_str());
        }
        if (spoof_version) {
            for (size_t i = 0; i < PROP_OVERRIDE_VERSION_COUNT; i++) {
                forgeProp(PROP_OVERRIDES_VERSION[i].name,
                           PROP_OVERRIDES_VERSION[i].value.c_str());
            }
        }
        if (!current_serial.empty()) {
            forgeProp("ro.boot.serialno", current_serial.c_str());
            forgeProp("ro.serialno", current_serial.c_str());
        }
        LOGI("[PROP] COW spoof done (%zu props%s + serial)",
             PROP_OVERRIDE_COUNT + (spoof_version ? PROP_OVERRIDE_VERSION_COUNT : 0),
             spoof_version ? " + version" : "");

        if (blocked) {
            executeCompanionCommand(0x00);              // unmount
        } else {
            executeCompanionCommand(0x01);              // mount
        }

        do_android_id = !current_android_id.empty();
        // Derive a PER-APP ANDROID_ID from the seed (android_id.txt) + this app's
        // base package. Each whitelisted app now reads a distinct id (matching how
        // real Android 8+ behaves), while an app's own main + :proc children share
        // one id. Deterministic (FNV-1a + splitmix64, no RNG) → stable across
        // reboots. Empty seed (android_id.txt empty/missing) → skip spoof entirely.
        if (do_android_id) {
            m_android_id = deriveAndroidId(current_android_id, base_package);
            LOGI("[AID] derived per-app ANDROID_ID: %s", m_android_id.c_str());
        }

        if (!do_android_id) {
            api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
        }
    }

    void postAppSpecialize([[maybe_unused]] const zygisk::AppSpecializeArgs* args) override {
        if (do_android_id) {
            forgeAndroidId(env, m_android_id.c_str());
            LOGI("[AID] done, unloading module");
            api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
        }
    }

private:
    zygisk::Api* api;
    JNIEnv* env;
    bool do_android_id = false;
    std::string m_android_id;
    jclass buildClass = nullptr;
    jclass versionClass = nullptr;
    jfieldID modelField = nullptr;
    jfieldID brandField = nullptr;
    jfieldID deviceField = nullptr;
    jfieldID manufacturerField = nullptr;
    jfieldID fingerprintField = nullptr;
    jfieldID productField = nullptr;
    jfieldID boardField = nullptr;
    jfieldID hardwareField = nullptr;
    jfieldID idField = nullptr;
    jfieldID displayField = nullptr;
    jfieldID serialField = nullptr;

    bool executeCompanionCommand(uint8_t cmd) {
        auto fd = api->connectCompanion();
        if (fd < 0) { LOGE("Companion connect failed"); return false; }
        write(fd, &cmd, 1);
        int result = -1;
        read(fd, &result, sizeof(result));
        close(fd);
        LOGI("Companion cmd=0x%02x result=%d", cmd, result);
        return result == 0;
    }

    void ensureBuildClass() {
        if (buildClass) return;

        jclass localBuild = env->FindClass("android/os/Build");
        if (!localBuild) { env->ExceptionClear(); LOGE("FindClass Build failed"); return; }
        buildClass = static_cast<jclass>(env->NewGlobalRef(localBuild));
        env->DeleteLocalRef(localBuild);
        if (!buildClass) { LOGE("NewGlobalRef Build failed"); return; }

        modelField = env->GetStaticFieldID(buildClass, "MODEL", "Ljava/lang/String;");
        brandField = env->GetStaticFieldID(buildClass, "BRAND", "Ljava/lang/String;");
        deviceField = env->GetStaticFieldID(buildClass, "DEVICE", "Ljava/lang/String;");
        manufacturerField = env->GetStaticFieldID(buildClass, "MANUFACTURER", "Ljava/lang/String;");
        fingerprintField = env->GetStaticFieldID(buildClass, "FINGERPRINT", "Ljava/lang/String;");
        productField = env->GetStaticFieldID(buildClass, "PRODUCT", "Ljava/lang/String;");
        boardField = env->GetStaticFieldID(buildClass, "BOARD", "Ljava/lang/String;");
        hardwareField = env->GetStaticFieldID(buildClass, "HARDWARE", "Ljava/lang/String;");
        idField = env->GetStaticFieldID(buildClass, "ID", "Ljava/lang/String;");
        displayField = env->GetStaticFieldID(buildClass, "DISPLAY", "Ljava/lang/String;");
        serialField = env->GetStaticFieldID(buildClass, "SERIAL", "Ljava/lang/String;");
        if (env->ExceptionCheck()) { env->ExceptionClear(); serialField = nullptr; }

        jclass localVersion = env->FindClass("android/os/Build$VERSION");
        if (localVersion) {
            versionClass = static_cast<jclass>(env->NewGlobalRef(localVersion));
            env->DeleteLocalRef(localVersion);
        }

        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            if (buildClass) env->DeleteGlobalRef(buildClass);
            if (versionClass) env->DeleteGlobalRef(versionClass);
            buildClass = versionClass = nullptr;
        }
    }

    void spoofDevice(const std::string& serial) {
        if (!buildClass) { LOGE("spoofDevice: buildClass null"); return; }

        auto setStr = [&](jclass cls, jfieldID field, const std::string& value) {
            if (!cls || !field) return;
            jstring js = env->NewStringUTF(value.c_str());
            if (!js || env->ExceptionCheck()) { env->ExceptionClear(); return; }
            env->SetStaticObjectField(cls, field, js);
            env->DeleteLocalRef(js);
            if (env->ExceptionCheck()) env->ExceptionClear();
        };

        setStr(buildClass, modelField, g_dev_model);
        setStr(buildClass, brandField, g_dev_brand);
        setStr(buildClass, deviceField, g_dev_device);
        setStr(buildClass, manufacturerField, g_dev_manufacturer);
        setStr(buildClass, fingerprintField, g_dev_fingerprint);
        setStr(buildClass, productField, g_dev_product);
        setStr(buildClass, boardField, g_dev_platform);
        setStr(buildClass, hardwareField, g_dev_hardware);
        setStr(buildClass, idField, g_dev_build_id);
        setStr(buildClass, displayField, g_dev_display_id);

        if (!serial.empty() && serialField) {
            setStr(buildClass, serialField, serial);
        }

        LOGI("Device spoofed: %s (%s)%s", g_dev_model.c_str(), g_dev_brand.c_str(),
             serial.empty() ? "" : " +serial");
    }
};

REGISTER_ZYGISK_MODULE(_Z)
REGISTER_ZYGISK_COMPANION(companion)

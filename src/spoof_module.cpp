/* ============================================================================
 *  过强标黑机 (GWBHJ) — Hardcoded device & CPU spoof module for Android
 *  Based on COPG v5.1.1 by Alireza Parsi
 *  Simplified: hardcoded config, whitelist-based, separate SERIAL control
 *  + Authorization: Ed25519 license verification, device_hash binding
 * ========================================================================== */
#include <jni.h>
#include <string>
#include <zygisk.hpp>
#include <fstream>
#include <unordered_map>
#include <unordered_set>
#include <dlfcn.h>
#include <sys/mman.h>
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
#include <ctime>
#include "sha256.h"
#include "ed25519.h"
#include "mini_json.h"

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

static const char* MODULE_DIR = "/data/adb/modules/gwbhj_jailbreak";
static const char* WHITELIST_PATH = "/data/adb/modules/gwbhj_jailbreak/whitelist.txt";
static const char* SERIAL_PATH = "/data/adb/modules/gwbhj_jailbreak/serial.txt";
static const char* CPUINFO_SPOOF_PATH = "/data/adb/modules/gwbhj_jailbreak/.c";

// ─────────────────────────────────────────
// Hardcoded device profile (compile-time constants)
// ─────────────────────────────────────────
static constexpr const char* DEVICE_BRAND = "HUAWEI";
static constexpr const char* DEVICE_DEVICE = "HWSGT";
static constexpr const char* DEVICE_MANUFACTURER = "HUAWEI";
static constexpr const char* DEVICE_MODEL = "SGT-AL10";
static constexpr const char* DEVICE_MARKET_NAME = "HUAWEI Mate 80 Pro Max";
static constexpr const char* DEVICE_FINGERPRINT = "HUAWEI/SGT-AL10/HWSGT:12/HUAWEISGT-AL10/6.0.0.150C00:user/release-keys";
static constexpr const char* DEVICE_PRODUCT = "SGT-AL10";
static constexpr const char* DEVICE_HARDWARE = "kirin9030";
static constexpr const char* DEVICE_PLATFORM = "SGT";

struct PropOverride {
    const char* name;
    const char* value;
};

static const PropOverride PROP_OVERRIDES[] = {
    {"ro.product.model",           DEVICE_MODEL},
    {"ro.product.brand",           DEVICE_BRAND},
    {"ro.product.manufacturer",    DEVICE_MANUFACTURER},
    {"ro.product.device",          DEVICE_DEVICE},
    {"ro.build.fingerprint",       DEVICE_FINGERPRINT},
    {"ro.product.name",            DEVICE_PRODUCT},
    {"ro.product.marketname",      DEVICE_MARKET_NAME},
    {"ro.config.marketing_name",   DEVICE_MARKET_NAME},
    {"ro.product.board",           DEVICE_PLATFORM},
    {"ro.hardware",                DEVICE_HARDWARE},
    {"ro.hardware.chipname",       "Kirin 9030 Pro"},
    {"ro.soc.manufacturer",        "HUAWEI"},
    {"ro.soc.model",               "Kirin 9030 Pro"},
    {"ro.product.system.model",    DEVICE_MODEL},
    {"ro.product.system.brand",    DEVICE_BRAND},
    {"ro.product.system.device",   DEVICE_DEVICE},
    {"ro.product.system.name",     DEVICE_PRODUCT},
    {"ro.product.vendor.model",    DEVICE_MODEL},
    {"ro.product.vendor.brand",    DEVICE_BRAND},
    {"ro.product.vendor.device",   DEVICE_DEVICE},
    {"ro.product.vendor.name",     DEVICE_PRODUCT},
    {"ro.product.odm.model",       DEVICE_MODEL},
    {"ro.product.odm.brand",       DEVICE_BRAND},
    {"ro.product.odm.device",      DEVICE_DEVICE},
    {"ro.product.odm.name",        DEVICE_PRODUCT},
    {"ro.build.display.id",        "SGT-AL10 6.0.0.150(C00E150R6P1)"},
    {"ro.build.version.incremental", "6.0.0.150C00"},
    {"ro.build.version.emui",      "EmotionUI_15.0.0"},
    {"ro.build.version.harmony",   "HarmonyOS 6.0.0"},
    {"hw_sc.build.platform.version", "6.0.0"},
    {"ro.config.hw_systemversion",  "SGT-AL10 6.0.0.150(C00E150R6P1)"},
};
static constexpr size_t PROP_OVERRIDE_COUNT = sizeof(PROP_OVERRIDES) / sizeof(PROP_OVERRIDES[0]);

// ─────────────────────────────────────────
// Hardcoded /proc/cpuinfo spoof content
// ─────────────────────────────────────────
static const char CPUINFO_SPOOF[] =
    "Processor\t: AArch64 Processor rev 0 (aarch64)\n"
    "\n"
    "processor\t: 0\n"
    "BogoMIPS\t: 2000.00\n"
    "Features\t: fp asimd evtstrm aes pmull sha1 sha2 crc32 atomics fphp asimdhp cpuid asimdrdm jscvt fcma lrcpc dcpop sha3 sm3 sm4 asimddp sha512 sve asimdfhm dit uscat ilrcpc flagm ssbs sb paca pacg dcpodp sve2 sveaes svepmull svebitperm i8mm bf16 dgh bti\n"
    "CPU implementer\t: 0x48\n"
    "CPU architecture: 8\n"
    "CPU variant\t: 0x2\n"
    "CPU part\t: 0xd46\n"
    "CPU revision\t: 0\n"
    "\n"
    "processor\t: 1\n"
    "BogoMIPS\t: 2000.00\n"
    "Features\t: fp asimd evtstrm aes pmull sha1 sha2 crc32 atomics fphp asimdhp cpuid asimdrdm jscvt fcma lrcpc dcpop sha3 sm3 sm4 asimddp sha512 sve asimdfhm dit uscat ilrcpc flagm ssbs sb paca pacg dcpodp sve2 sveaes svepmull svebitperm i8mm bf16 dgh bti\n"
    "CPU implementer\t: 0x48\n"
    "CPU architecture: 8\n"
    "CPU variant\t: 0x2\n"
    "CPU part\t: 0xd46\n"
    "CPU revision\t: 1\n"
    "\n"
    "processor\t: 2\n"
    "BogoMIPS\t: 2000.00\n"
    "Features\t: fp asimd evtstrm aes pmull sha1 sha2 crc32 atomics fphp asimdhp cpuid asimdrdm jscvt fcma lrcpc dcpop sha3 sm3 sm4 asimddp sha512 sve asimdfhm dit uscat ilrcpc flagm ssbs sb paca pacg dcpodp sve2 sveaes svepmull svebitperm i8mm bf16 dgh bti\n"
    "CPU implementer\t: 0x48\n"
    "CPU architecture: 8\n"
    "CPU variant\t: 0x2\n"
    "CPU part\t: 0xd47\n"
    "CPU revision\t: 2\n"
    "\n"
    "processor\t: 3\n"
    "BogoMIPS\t: 2000.00\n"
    "Features\t: fp asimd evtstrm aes pmull sha1 sha2 crc32 atomics fphp asimdhp cpuid asimdrdm jscvt fcma lrcpc dcpop sha3 sm3 sm4 asimddp sha512 sve asimdfhm dit uscat ilrcpc flagm ssbs sb paca pacg dcpodp sve2 sveaes svepmull svebitperm i8mm bf16 dgh bti\n"
    "CPU implementer\t: 0x48\n"
    "CPU architecture: 8\n"
    "CPU variant\t: 0x2\n"
    "CPU part\t: 0xd47\n"
    "CPU revision\t: 3\n"
    "\n"
    "processor\t: 4\n"
    "BogoMIPS\t: 2000.00\n"
    "Features\t: fp asimd evtstrm aes pmull sha1 sha2 crc32 atomics fphp asimdhp cpuid asimdrdm jscvt fcma lrcpc dcpop sha3 sm3 sm4 asimddp sha512 sve asimdfhm dit uscat ilrcpc flagm ssbs sb paca pacg dcpodp sve2 sveaes svepmull svebitperm i8mm bf16 dgh bti\n"
    "CPU implementer\t: 0x48\n"
    "CPU architecture: 8\n"
    "CPU variant\t: 0x2\n"
    "CPU part\t: 0xd47\n"
    "CPU revision\t: 4\n"
    "\n"
    "processor\t: 5\n"
    "BogoMIPS\t: 2000.00\n"
    "Features\t: fp asimd evtstrm aes pmull sha1 sha2 crc32 atomics fphp asimdhp cpuid asimdrdm jscvt fcma lrcpc dcpop sha3 sm3 sm4 asimddp sha512 sve asimdfhm dit uscat ilrcpc flagm ssbs sb paca pacg dcpodp sve2 sveaes svepmull svebitperm i8mm bf16 dgh bti\n"
    "CPU implementer\t: 0x48\n"
    "CPU architecture: 8\n"
    "CPU variant\t: 0x2\n"
    "CPU part\t: 0xd47\n"
    "CPU revision\t: 5\n"
    "\n"
    "processor\t: 6\n"
    "BogoMIPS\t: 2000.00\n"
    "Features\t: fp asimd evtstrm aes pmull sha1 sha2 crc32 atomics fphp asimdhp cpuid asimdrdm jscvt fcma lrcpc dcpop sha3 sm3 sm4 asimddp sha512 sve asimdfhm dit uscat ilrcpc flagm ssbs sb paca pacg dcpodp sve2 sveaes svepmull svebitperm i8mm bf16 dgh bti\n"
    "CPU implementer\t: 0x48\n"
    "CPU architecture: 8\n"
    "CPU variant\t: 0x2\n"
    "CPU part\t: 0xd47\n"
    "CPU revision\t: 6\n"
    "\n"
    "processor\t: 7\n"
    "BogoMIPS\t: 2000.00\n"
    "Features\t: fp asimd evtstrm aes pmull sha1 sha2 crc32 atomics fphp asimdhp cpuid asimdrdm jscvt fcma lrcpc dcpop sha3 sm3 sm4 asimddp sha512 sve asimdfhm dit uscat ilrcpc flagm ssbs sb paca pacg dcpodp sve2 sveaes svepmull svebitperm i8mm bf16 dgh bti\n"
    "CPU implementer\t: 0x48\n"
    "CPU architecture: 8\n"
    "CPU variant\t: 0x2\n"
    "CPU part\t: 0xd47\n"
    "CPU revision\t: 7\n"
    "\n"
    "processor\t: 8\n"
    "BogoMIPS\t: 2000.00\n"
    "Features\t: fp asimd evtstrm aes pmull sha1 sha2 crc32 atomics fphp asimdhp cpuid asimdrdm jscvt fcma lrcpc dcpop sha3 sm3 sm4 asimddp sha512 sve asimdfhm dit uscat ilrcpc flagm ssbs sb paca pacg dcpodp sve2 sveaes svepmull svebitperm i8mm bf16 dgh bti\n"
    "CPU implementer\t: 0x48\n"
    "CPU architecture: 8\n"
    "CPU variant\t: 0x2\n"
    "CPU part\t: 0xd47\n"
    "CPU revision\t: 8\n"
    "\n"
    "processor\t: 9\n"
    "BogoMIPS\t: 2000.00\n"
    "Features\t: fp asimd evtstrm aes pmull sha1 sha2 crc32 atomics fphp asimdhp cpuid asimdrdm jscvt fcma lrcpc dcpop sha3 sm3 sm4 asimddp sha512 sve asimdfhm dit uscat ilrcpc flagm ssbs sb paca pacg dcpodp sve2 sveaes svepmull svebitperm i8mm bf16 dgh bti\n"
    "CPU implementer\t: 0x48\n"
    "CPU architecture: 8\n"
    "CPU variant\t: 0x2\n"
    "CPU part\t: 0xd47\n"
    "CPU revision\t: 9\n"
    "\n"
    "processor\t: 10\n"
    "BogoMIPS\t: 2000.00\n"
    "Features\t: fp asimd evtstrm aes pmull sha1 sha2 crc32 atomics fphp asimdhp cpuid asimdrdm jscvt fcma lrcpc dcpop sha3 sm3 sm4 asimddp sha512 sve asimdfhm dit uscat ilrcpc flagm ssbs sb paca pacg dcpodp sve2 sveaes svepmull svebitperm i8mm bf16 dgh bti\n"
    "CPU implementer\t: 0x48\n"
    "CPU architecture: 8\n"
    "CPU variant\t: 0x2\n"
    "CPU part\t: 0xd47\n"
    "CPU revision\t: 10\n"
    "\n"
    "processor\t: 11\n"
    "BogoMIPS\t: 2000.00\n"
    "Features\t: fp asimd evtstrm aes pmull sha1 sha2 crc32 atomics fphp asimdhp cpuid asimdrdm jscvt fcma lrcpc dcpop sha3 sm3 sm4 asimddp sha512 sve asimdfhm dit uscat ilrcpc flagm ssbs sb paca pacg dcpodp sve2 sveaes svepmull svebitperm i8mm bf16 dgh bti\n"
    "CPU implementer\t: 0x48\n"
    "CPU architecture: 8\n"
    "CPU variant\t: 0x2\n"
    "CPU part\t: 0xd47\n"
    "CPU revision\t: 11\n"
    "\n"
    "processor\t: 12\n"
    "BogoMIPS\t: 2000.00\n"
    "Features\t: fp asimd evtstrm aes pmull sha1 sha2 crc32 atomics fphp asimdhp cpuid asimdrdm jscvt fcma lrcpc dcpop sha3 sm3 sm4 asimddp sha512 sve asimdfhm dit uscat ilrcpc flagm ssbs sb paca pacg dcpodp sve2 sveaes svepmull svebitperm i8mm bf16 dgh bti\n"
    "CPU implementer\t: 0x48\n"
    "CPU architecture: 8\n"
    "CPU variant\t: 0x2\n"
    "CPU part\t: 0xd47\n"
    "CPU revision\t: 12\n"
    "\n"
    "processor\t: 13\n"
    "BogoMIPS\t: 2000.00\n"
    "Features\t: fp asimd evtstrm aes pmull sha1 sha2 crc32 atomics fphp asimdhp cpuid asimdrdm jscvt fcma lrcpc dcpop sha3 sm3 sm4 asimddp sha512 sve asimdfhm dit uscat ilrcpc flagm ssbs sb paca pacg dcpodp sve2 sveaes svepmull svebitperm i8mm bf16 dgh bti\n"
    "CPU implementer\t: 0x48\n"
    "CPU architecture: 8\n"
    "CPU variant\t: 0x2\n"
    "CPU part\t: 0xd47\n"
    "CPU revision\t: 13\n"
    "\n"
    "Hardware\t: HUAWEI Kirin 9030 Pro\n";

static const char* LICENSE_PATH = "/data/adb/modules/gwbhj_jailbreak/license.json";

static const char* COMPILE_TIME_SALT = "GWBHJ_2026_SALT_v2";

static const char* SERVER_PUBLIC_KEY_B64 =
    "REPLACE_WITH_SERVER_PUBLIC_KEY_BASE64";

// ─────────────────────────────────────────
// Hardcoded whitelist (fallback if file missing)
// ─────────────────────────────────────────
static const char* HARDCODED_WHITELIST[] = {
    "com.tencent.tmgp.dfm",
};
static constexpr size_t HARDCODED_WHITELIST_COUNT = sizeof(HARDCODED_WHITELIST) / sizeof(HARDCODED_WHITELIST[0]);

// ─────────────────────────────────────────
// Whitelist & Serial cache
// ─────────────────────────────────────────
static std::unordered_set<std::string> g_whitelist;
static std::string g_serial;
static std::mutex g_data_mutex;
static time_t g_whitelist_mtime = 0;
static time_t g_serial_mtime = 0;
static bool g_whitelist_loaded = false;

static std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

static void loadHardcodedWhitelist() {
    std::lock_guard<std::mutex> lock(g_data_mutex);
    for (size_t i = 0; i < HARDCODED_WHITELIST_COUNT; i++) {
        g_whitelist.insert(HARDCODED_WHITELIST[i]);
    }
    g_whitelist_loaded = true;
    LOGI("Hardcoded whitelist loaded: %zu packages", g_whitelist.size());
}

static std::string readSerialFromFile() {
    std::ifstream f(SERIAL_PATH);
    if (!f.is_open()) return "";
    std::string line;
    std::getline(f, line);
    return trim(line);
}

static void reloadWhitelistIfNeeded() {
    struct stat st;
    if (stat(WHITELIST_PATH, &st) != 0) {
        if (!g_whitelist_loaded) {
            loadHardcodedWhitelist();
        }
        return;
    }
    if (st.st_mtime == g_whitelist_mtime && g_whitelist_loaded) return;

    std::ifstream f(WHITELIST_PATH);
    if (!f.is_open()) {
        if (!g_whitelist_loaded) loadHardcodedWhitelist();
        return;
    }

    std::unordered_set<std::string> new_list;
    std::string line;
    while (std::getline(f, line)) {
        std::string pkg = trim(line);
        if (!pkg.empty() && pkg[0] != '#') {
            new_list.insert(pkg);
        }
    }

    if (new_list.empty()) {
        if (!g_whitelist_loaded) loadHardcodedWhitelist();
        return;
    }

    std::lock_guard<std::mutex> lock(g_data_mutex);
    g_whitelist = std::move(new_list);
    g_whitelist_mtime = st.st_mtime;
    g_whitelist_loaded = true;
    LOGI("Whitelist reloaded from file: %zu packages", g_whitelist.size());
}

static void reloadSerialIfNeeded() {
    struct stat st;
    if (stat(SERIAL_PATH, &st) != 0) return;
    if (st.st_mtime == g_serial_mtime && !g_serial.empty()) return;

    std::string val = readSerialFromFile();
    if (val.empty()) return;

    std::lock_guard<std::mutex> lock(g_data_mutex);
    g_serial = std::move(val);
    g_serial_mtime = st.st_mtime;
    LOGI("Serial reloaded: %s", g_serial.c_str());
}

// ─────────────────────────────────────────
// Device ID & Authorization
// ─────────────────────────────────────────
static std::string g_cached_device_hash;
static std::mutex g_auth_mutex;

static std::string readProcCmdline() {
    std::ifstream f("/proc/cmdline");
    if (!f.is_open()) return "";
    std::string content((std::istreambuf_iterator<char>(f)),
                         std::istreambuf_iterator<char>());
    return content;
}

static std::string extractCmdlineField(const std::string& cmdline, const std::string& key) {
    size_t pos = cmdline.find(key + "=");
    if (pos == std::string::npos) return "";

    size_t valStart = pos + key.size() + 1;
    size_t valEnd = cmdline.find_first_of(" \t\n\r", valStart);
    if (valEnd == std::string::npos) valEnd = cmdline.size();

    std::string value = cmdline.substr(valStart, valEnd - valStart);
    return trim(value);
}

static std::string readHardwareId() {
    std::string cmdline = readProcCmdline();
    if (cmdline.empty()) {
        LOGW("[AUTH] Failed to read /proc/cmdline");
        return "";
    }

    static const char* fields[] = {
        "androidboot.cpuid",
        "androidboot.chipid",
        "androidboot.emmcid",
        "oplusboot.serialno",
        "androidboot.serialno"
    };

    for (const char* field : fields) {
        std::string value = extractCmdlineField(cmdline, field);
        if (!value.empty()) {
            LOGI("[AUTH] Hardware ID from %s", field);
            return value;
        }
    }

    LOGW("[AUTH] No hardware ID found in cmdline");
    return "";
}

static std::string generateDeviceHash() {
    std::lock_guard<std::mutex> lock(g_auth_mutex);
    if (!g_cached_device_hash.empty()) return g_cached_device_hash;

    std::string hwId = readHardwareId();
    if (hwId.empty()) {
        LOGE("[AUTH] Cannot generate device_hash: no hardware ID");
        return "";
    }

    std::string input = hwId + COMPILE_TIME_SALT;
    g_cached_device_hash = SHA256::hash(input);
    LOGI("[AUTH] device_hash generated: %s...", g_cached_device_hash.substr(0, 16).c_str());
    return g_cached_device_hash;
}

static std::string readFileContent(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return "";
    std::string content((std::istreambuf_iterator<char>(f)),
                         std::istreambuf_iterator<char>());
    return content;
}

static bool verifyLicenseSignature(const std::string& jsonContent, const std::string& signature) {
    if (signature.empty() || SERVER_PUBLIC_KEY_B64[0] == 'R') {
        LOGW("[AUTH] Server public key not configured, signature verification skipped");
        return false;
    }
    bool result = Ed25519::verify(jsonContent, signature, SERVER_PUBLIC_KEY_B64);
    if (!result) {
        LOGE("[AUTH] Ed25519 signature verification FAILED");
    }
    return result;
}

static bool authOk() {
    std::string licenseContent = readFileContent(LICENSE_PATH);
    if (licenseContent.empty()) {
        LOGW("[AUTH] license.json not found or empty");
        return false;
    }

    MiniJson license = MiniJson::parse(licenseContent);
    if (!license.isObject()) {
        LOGE("[AUTH] license.json parse failed");
        return false;
    }

    std::string signature;
    if (license.has("signature") && license["signature"].isString()) {
        signature = license["signature"].asString();
    }

    std::string jsonForVerify = licenseContent;
    size_t sigPos = jsonForVerify.rfind("\"signature\"");
    if (sigPos != std::string::npos) {
        size_t objStart = jsonForVerify.rfind(',', sigPos);
        if (objStart != std::string::npos) {
            jsonForVerify = jsonForVerify.substr(0, objStart) + jsonForVerify.substr(sigPos);
        }
    }

    {
        size_t sigKeyPos = licenseContent.rfind("\"signature\"");
        if (sigKeyPos != std::string::npos) {
            size_t commaPos = licenseContent.rfind(',', sigKeyPos);
            if (commaPos != std::string::npos) {
                jsonForVerify = licenseContent.substr(0, commaPos);
                jsonForVerify += "\n}";
            }
        }
    }

    if (!verifyLicenseSignature(jsonForVerify, signature)) {
        LOGE("[AUTH] License signature verification failed");
        return false;
    }

    if (license.has("device_hash") && license["device_hash"].isString()) {
        std::string licenseDeviceHash = license["device_hash"].asString();
        std::string localDeviceHash = generateDeviceHash();
        if (localDeviceHash.empty() || licenseDeviceHash != localDeviceHash) {
            LOGE("[AUTH] device_hash mismatch: license=%s... local=%s...",
                 licenseDeviceHash.substr(0, 16).c_str(),
                 localDeviceHash.empty() ? "EMPTY" : localDeviceHash.substr(0, 16).c_str());
            return false;
        }
    } else {
        LOGE("[AUTH] license.json missing device_hash");
        return false;
    }

    if (license.has("revoked") && license["revoked"].isBool()) {
        if (license["revoked"].asBool()) {
            LOGE("[AUTH] License revoked");
            return false;
        }
    }

    if (license.has("lock_until") && license["lock_until"].isNumber()) {
        long lockUntil = license["lock_until"].asLong();
        if (lockUntil > 0) {
            time_t now = time(nullptr);
            if (now < lockUntil) {
                LOGE("[AUTH] License locked until %ld (now=%ld)", lockUntil, (long)now);
                return false;
            }
        }
    }

    if (license.has("active") && license["active"].isBool()) {
        if (!license["active"].asBool()) {
            LOGE("[AUTH] License not active");
            return false;
        }
    }

    if (license.has("last_server_ok") && license["last_server_ok"].isNumber()) {
        long lastServerOk = license["last_server_ok"].asLong();
        int graceSeconds = 86400;
        if (license.has("grace_seconds") && license["grace_seconds"].isNumber()) {
            graceSeconds = (int)license["grace_seconds"].asLong();
        }
        time_t now = time(nullptr);
        if (lastServerOk + graceSeconds < (long)now) {
            LOGE("[AUTH] Grace period expired: last_server_ok=%ld + grace=%d < now=%ld",
                 lastServerOk, graceSeconds, (long)now);
            return false;
        }
    } else {
        LOGE("[AUTH] license.json missing last_server_ok");
        return false;
    }

    LOGI("[AUTH] Authorization OK");
    return true;
}

// ─────────────────────────────────────────
// Companion (runs as root)
// cpuinfo_spoof content lives ONLY in the binary.
// The on-disk file is written fresh before every mount and
// removed immediately after unmount — nothing persistent to tamper with.
// ─────────────────────────────────────────
static void writeCpuinfoFromBinary() {
    int fd = open(CPUINFO_SPOOF_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        LOGE("Failed to write cpuinfo spoof: %s", strerror(errno));
        return;
    }
    size_t len = sizeof(CPUINFO_SPOOF) - 1;
    write(fd, CPUINFO_SPOOF, len);
    close(fd);
    chmod(CPUINFO_SPOOF_PATH, 0644);
    LOGI("cpuinfo spoof written from binary (%zu bytes)", len);
}

static void removeCpuinfoFile() {
    unlink(CPUINFO_SPOOF_PATH);
}

static void companion(int fd) {
    char buffer[256];
    ssize_t bytes = read(fd, buffer, sizeof(buffer) - 1);
    if (bytes <= 0) {
        close(fd);
        return;
    }
    buffer[bytes] = '\0';
    std::string command(buffer);

    if (command == "unmount_spoof") {
        system("/system/bin/umount /proc/cpuinfo 2>/dev/null");
        removeCpuinfoFile();
        int result = 0;
        write(fd, &result, sizeof(result));
        LOGI("[COMPANION] CPU unmount + file removed");
    } else if (command == "mount_spoof") {
        writeCpuinfoFromBinary();
        system("/system/bin/umount /proc/cpuinfo 2>/dev/null");
        char mount_cmd[512];
        snprintf(mount_cmd, sizeof(mount_cmd),
                 "/system/bin/mount --bind %s /proc/cpuinfo",
                 CPUINFO_SPOOF_PATH);
            int result = system(mount_cmd);
            if (result == 0) {
                removeCpuinfoFile();
            }
            write(fd, &result, sizeof(result));
            LOGI("[COMPANION] CPU mount (result=%d)", result);
        }
    close(fd);
}

static bool isWhitelistedProcess(const std::string& process_name) {
    if (g_whitelist.find(process_name) != g_whitelist.end()) return true;

    size_t colon = process_name.find(':');
    if (colon == std::string::npos) return false;

    std::string package_name = process_name.substr(0, colon);
    return g_whitelist.find(package_name) != g_whitelist.end();
}

// ─────────────────────────────────────────
// COW Prop Spoof (stealth — per-process, zero-residency)
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
    if (len >= PROP_VALUE_MAX) { LOGW("[PROP] %s: value too long, skip", name); return; }
    if (!ensurePropAreaPrivate(cpi)) { LOGE("[PROP] %s: COW remap failed", name); return; }
    volatile uint32_t* serial = (volatile uint32_t*)cpi;
    char* value = (char*)cpi + sizeof(uint32_t);
    uint32_t old = *serial;
    *serial = old | 1;
    __sync_synchronize();
    memcpy(value, val, len); value[len] = '\0';
    __sync_synchronize();
    *serial = ((uint32_t)len << 24) | (((old & 0x00FFFFFFu) + 2) & 0x00FFFFFFu);
    __sync_synchronize();
    char rb[PROP_VALUE_MAX] = {0};
    __system_property_get(name, rb);
    LOGI("[PROP] %s -> '%s' (readback='%s')", name, val, rb);
}

// ─────────────────────────────────────────
// Main Module
// ─────────────────────────────────────────
class ZModule : public zygisk::ModuleBase {
public:
    void onLoad(zygisk::Api* api, JNIEnv* env) override {
        this->api = api;
        this->env = env;
        LOGI("GWBHJ loaded (hardcoded config)");
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

        LOGI("Processing: %s", pkg.c_str());

        reloadWhitelistIfNeeded();
        reloadSerialIfNeeded();

        bool in_whitelist;
        std::string current_serial;
        {
            std::lock_guard<std::mutex> lock(g_data_mutex);
            in_whitelist = isWhitelistedProcess(pkg);
            current_serial = g_serial;
        }

        if (!in_whitelist) {
            LOGI("%s: not in whitelist, skip", pkg.c_str());
            api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        if (!authOk()) {
            LOGW("%s: authorization failed, spoof disabled", pkg.c_str());
            api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        LOGI("%s: in whitelist + authorized, applying spoof", pkg.c_str());

        ensureBuildClass();
        spoofDevice(current_serial);

        for (size_t i = 0; i < PROP_OVERRIDE_COUNT; i++) {
            forgeProp(PROP_OVERRIDES[i].name, PROP_OVERRIDES[i].value);
        }
        if (!current_serial.empty()) {
            forgeProp("ro.boot.serialno", current_serial.c_str());
            forgeProp("ro.serialno", current_serial.c_str());
        }

        executeCompanionCommand("mount_spoof");

        api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
    }

    void postAppSpecialize([[maybe_unused]] const zygisk::AppSpecializeArgs* args) override {
        LOGI("postAppSpecialize: app specialized, spoof active");
    }

private:
    zygisk::Api* api;
    JNIEnv* env;
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

    bool executeCompanionCommand(const std::string& command) {
        auto fd = api->connectCompanion();
        if (fd < 0) { LOGE("Companion connect failed"); return false; }
        write(fd, command.c_str(), command.size());
        int result = -1;
        read(fd, &result, sizeof(result));
        close(fd);
        LOGI("Companion '%s' result=%d", command.c_str(), result);
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

        setStr(buildClass, modelField, DEVICE_MODEL);
        setStr(buildClass, brandField, DEVICE_BRAND);
        setStr(buildClass, deviceField, DEVICE_DEVICE);
        setStr(buildClass, manufacturerField, DEVICE_MANUFACTURER);
        setStr(buildClass, fingerprintField, DEVICE_FINGERPRINT);
        setStr(buildClass, productField, DEVICE_PRODUCT);
        setStr(buildClass, boardField, DEVICE_PLATFORM);
        setStr(buildClass, hardwareField, DEVICE_HARDWARE);
        setStr(buildClass, idField, "HUAWEISGT-AL10");
        setStr(buildClass, displayField, "SGT-AL10 6.0.0.150(C00E150R6P1)");

        if (!serial.empty() && serialField) {
            setStr(buildClass, serialField, serial);
        }

        LOGI("Device spoofed: %s (%s)%s", DEVICE_MODEL, DEVICE_BRAND,
             serial.empty() ? "" : " +serial");
    }
};

REGISTER_ZYGISK_MODULE(ZModule)
REGISTER_ZYGISK_COMPANION(companion)

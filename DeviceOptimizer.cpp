#include <jni.h>
#include <android/native_window.h>
#include <android/log.h>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <chrono>
#include <thread>
#include <ctime>

#define LOG_TAG "DeviceOptimizer"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// ============================================================
//  CẤU HÌNH - Tuỳ chỉnh tại đây
// ============================================================
static const float ANIMATION_SCALE = 0.0f;   // 0 = tắt hoàn toàn | 0.5 = giảm nửa
static const int   TARGET_HZ       = 90;      // Refresh rate
static const int   TARGET_FPS      = 90;      // FPS mục tiêu

// ============================================================
//  DEVICE INFO STRUCT
// ============================================================
struct DeviceInfo {
    long totalRAM;
    long availableRAM;
    long usedRAM;
    int  cpuCores;
    int  refreshRate;
    float animationScale;
    float batteryLevel;
    float temperature;
    float cpuUsage;
    std::string deviceModel;
    std::string androidVersion;
};

// ============================================================
//  JNI HELPER - Gọi Android Settings API
// ============================================================
static void putGlobalFloat(JNIEnv* env, jobject context,
                           const char* key, float value)
{
    jclass settingsGlobal = env->FindClass("android/provider/Settings$Global");
    jmethodID putFloat = env->GetStaticMethodID(
        settingsGlobal, "putFloat",
        "(Landroid/content/ContentResolver;Ljava/lang/String;F)Z");

    // Lấy ContentResolver
    jclass ctxClass = env->GetObjectClass(context);
    jmethodID getResolver = env->GetMethodID(
        ctxClass, "getContentResolver",
        "()Landroid/content/ContentResolver;");
    jobject resolver = env->CallObjectMethod(context, getResolver);

    jstring jKey = env->NewStringUTF(key);
    env->CallStaticBooleanMethod(settingsGlobal, putFloat, resolver, jKey, value);

    env->DeleteLocalRef(jKey);
    env->DeleteLocalRef(resolver);
    env->DeleteLocalRef(settingsGlobal);
    env->DeleteLocalRef(ctxClass);
}

static void putSystemInt(JNIEnv* env, jobject context,
                         const char* key, int value)
{
    jclass settingsSystem = env->FindClass("android/provider/Settings$System");
    jmethodID putInt = env->GetStaticMethodID(
        settingsSystem, "putInt",
        "(Landroid/content/ContentResolver;Ljava/lang/String;I)Z");

    jclass ctxClass = env->GetObjectClass(context);
    jmethodID getResolver = env->GetMethodID(
        ctxClass, "getContentResolver",
        "()Landroid/content/ContentResolver;");
    jobject resolver = env->CallObjectMethod(context, getResolver);

    jstring jKey = env->NewStringUTF(key);
    env->CallStaticBooleanMethod(settingsSystem, putInt, resolver, jKey, value);

    env->DeleteLocalRef(jKey);
    env->DeleteLocalRef(resolver);
    env->DeleteLocalRef(settingsSystem);
    env->DeleteLocalRef(ctxClass);
}

// ============================================================
//  SMOOTH DEVICE
// ============================================================

// ✅ Tắt animation (0 hoặc 0.5)
void optimizePerformance(JNIEnv* env, jobject context)
{
    putGlobalFloat(env, context, "window_animation_scale",    ANIMATION_SCALE);
    putGlobalFloat(env, context, "transition_animation_scale", ANIMATION_SCALE);
    putGlobalFloat(env, context, "animator_duration_scale",   ANIMATION_SCALE);

    LOGI("Animation scale set to %.1f", ANIMATION_SCALE);
}

// ✅ Set 90Hz Refresh Rate
void setRefreshRate(JNIEnv* env, jobject context)
{
    putSystemInt(env, context, "peak_refresh_rate",      TARGET_HZ);
    putSystemInt(env, context, "min_refresh_rate",       TARGET_HZ);
    putSystemInt(env, context, "display_refresh_rate",   TARGET_HZ);

    LOGI("Refresh rate set to %dHz", TARGET_HZ);
}

// ✅ Set FPS (ANativeWindow - dùng trong game loop)
void setFrameRate(ANativeWindow* window)
{
    if (window != nullptr) {
        // ANATIVEWINDOW_FRAME_RATE_COMPATIBILITY_DEFAULT = 0
        ANativeWindow_setFrameRate(window, (float)TARGET_FPS, 0);
        LOGI("Frame rate set to %d FPS", TARGET_FPS);
    }
}

// ✅ Bật Game Mode
void enableGameMode(JNIEnv* env, jobject context, ANativeWindow* window)
{
    LOGI("=== Game Mode ON ===");
    optimizePerformance(env, context);   // Tắt animation
    setRefreshRate(env, context);        // 90Hz
    setFrameRate(window);               // 90 FPS
}

// ✅ Tắt Game Mode - trả về bình thường
void disableGameMode(JNIEnv* env, jobject context)
{
    LOGI("=== Game Mode OFF ===");
    putGlobalFloat(env, context, "window_animation_scale",     1.0f);
    putGlobalFloat(env, context, "transition_animation_scale", 1.0f);
    putGlobalFloat(env, context, "animator_duration_scale",    1.0f);

    putSystemInt(env, context, "peak_refresh_rate", 60);
    putSystemInt(env, context, "min_refresh_rate",  60);
}

// ============================================================
//  PERFORMANCE MONITOR
// ============================================================

static long lastCpuTotal = 0;
static long lastCpuIdle  = 0;

// ✅ Lấy CPU usage (%)
float getCPUUsage()
{
    std::ifstream file("/proc/stat");
    if (!file.is_open()) return 0.0f;

    std::string line;
    std::getline(file, line);
    file.close();

    std::istringstream iss(line);
    std::string cpu;
    long user, nice, system, idle, iowait, irq, softirq;
    iss >> cpu >> user >> nice >> system >> idle >> iowait >> irq >> softirq;

    long totalIdle  = idle + iowait;
    long total      = user + nice + system + idle + iowait + irq + softirq;

    long deltaTotal = total - lastCpuTotal;
    long deltaIdle  = totalIdle - lastCpuIdle;

    lastCpuTotal = total;
    lastCpuIdle  = totalIdle;

    if (deltaTotal > 0)
        return ((deltaTotal - deltaIdle) * 100.0f) / deltaTotal;

    return 0.0f;
}

// ✅ Lấy RAM khả dụng (MB)
long getAvailableRAM(JNIEnv* env, jobject context)
{
    jclass activityManagerClass = env->FindClass("android/app/ActivityManager");
    jclass ctxClass = env->GetObjectClass(context);

    jmethodID getService = env->GetMethodID(
        ctxClass, "getSystemService",
        "(Ljava/lang/String;)Ljava/lang/Object;");

    jstring amStr = env->NewStringUTF("activity");
    jobject am = env->CallObjectMethod(context, getService, amStr);

    jclass memInfoClass = env->FindClass("android/app/ActivityManager$MemoryInfo");
    jmethodID memInfoInit = env->GetMethodID(memInfoClass, "<init>", "()V");
    jobject memInfo = env->NewObject(memInfoClass, memInfoInit);

    jmethodID getMemInfo = env->GetMethodID(
        activityManagerClass, "getMemoryInfo",
        "(Landroid/app/ActivityManager$MemoryInfo;)V");
    env->CallVoidMethod(am, getMemInfo, memInfo);

    jfieldID availMemField = env->GetFieldID(memInfoClass, "availMem", "J");
    jlong availMem = env->GetLongField(memInfo, availMemField);

    env->DeleteLocalRef(amStr);
    env->DeleteLocalRef(am);
    env->DeleteLocalRef(memInfo);
    env->DeleteLocalRef(memInfoClass);
    env->DeleteLocalRef(activityManagerClass);
    env->DeleteLocalRef(ctxClass);

    return (long)(availMem / (1024 * 1024));
}

// ✅ Lấy mức pin (%)
float getBatteryLevel(JNIEnv* env, jobject context)
{
    jclass ctxClass = env->GetObjectClass(context);
    jmethodID registerReceiver = env->GetMethodID(
        ctxClass, "registerReceiver",
        "(Landroid/content/BroadcastReceiver;Landroid/content/IntentFilter;)Landroid/content/Intent;");

    jclass intentFilterClass = env->FindClass("android/content/IntentFilter");
    jmethodID filterInit = env->GetMethodID(intentFilterClass, "<init>", "(Ljava/lang/String;)V");
    jstring action = env->NewStringUTF("android.intent.action.BATTERY_CHANGED");
    jobject filter = env->NewObject(intentFilterClass, filterInit, action);
    jobject batteryIntent = env->CallObjectMethod(context, registerReceiver, nullptr, filter);

    if (batteryIntent == nullptr) return 0.0f;

    jclass intentClass = env->GetObjectClass(batteryIntent);
    jmethodID getIntExtra = env->GetMethodID(intentClass, "getIntExtra",
        "(Ljava/lang/String;I)I");

    jstring levelKey = env->NewStringUTF("level");
    jstring scaleKey = env->NewStringUTF("scale");

    int level = env->CallIntMethod(batteryIntent, getIntExtra, levelKey, -1);
    int scale = env->CallIntMethod(batteryIntent, getIntExtra, scaleKey, -1);

    env->DeleteLocalRef(action);
    env->DeleteLocalRef(filter);
    env->DeleteLocalRef(batteryIntent);
    env->DeleteLocalRef(levelKey);
    env->DeleteLocalRef(scaleKey);
    env->DeleteLocalRef(intentFilterClass);
    env->DeleteLocalRef(intentClass);
    env->DeleteLocalRef(ctxClass);

    if (scale > 0)
        return (level / (float)scale) * 100.0f;
    return 0.0f;
}

// ✅ Lấy nhiệt độ máy (°C)
float getTemperature(JNIEnv* env, jobject context)
{
    jclass ctxClass = env->GetObjectClass(context);
    jmethodID registerReceiver = env->GetMethodID(
        ctxClass, "registerReceiver",
        "(Landroid/content/BroadcastReceiver;Landroid/content/IntentFilter;)Landroid/content/Intent;");

    jclass intentFilterClass = env->FindClass("android/content/IntentFilter");
    jmethodID filterInit = env->GetMethodID(intentFilterClass, "<init>", "(Ljava/lang/String;)V");
    jstring action = env->NewStringUTF("android.intent.action.BATTERY_CHANGED");
    jobject filter = env->NewObject(intentFilterClass, filterInit, action);
    jobject batteryIntent = env->CallObjectMethod(context, registerReceiver, nullptr, filter);

    if (batteryIntent == nullptr) return 0.0f;

    jclass intentClass = env->GetObjectClass(batteryIntent);
    jmethodID getIntExtra = env->GetMethodID(intentClass, "getIntExtra",
        "(Ljava/lang/String;I)I");

    jstring tempKey = env->NewStringUTF("temperature");
    int temp = env->CallIntMethod(batteryIntent, getIntExtra, tempKey, -1);

    env->DeleteLocalRef(action);
    env->DeleteLocalRef(filter);
    env->DeleteLocalRef(batteryIntent);
    env->DeleteLocalRef(tempKey);
    env->DeleteLocalRef(intentFilterClass);
    env->DeleteLocalRef(intentClass);
    env->DeleteLocalRef(ctxClass);

    return temp / 10.0f;
}

// ============================================================
//  STORAGE MANAGER
// ============================================================

struct StorageInfo {
    long totalStorage;      // MB
    long availableStorage;  // MB
    float usagePercent;
};

StorageInfo getStorageInfo(JNIEnv* env)
{
    StorageInfo info = {0, 0, 0.0f};

    jclass statFsClass = env->FindClass("android/os/StatFs");
    jclass environmentClass = env->FindClass("android/os/Environment");

    jmethodID getExternal = env->GetStaticMethodID(
        environmentClass, "getExternalStorageDirectory",
        "()Ljava/io/File;");
    jobject extDir = env->CallStaticObjectMethod(environmentClass, getExternal);

    jclass fileClass = env->FindClass("java/io/File");
    jmethodID getPath = env->GetMethodID(fileClass, "getPath", "()Ljava/lang/String;");
    jstring path = (jstring)env->CallObjectMethod(extDir, getPath);

    jmethodID statFsInit = env->GetMethodID(statFsClass, "<init>", "(Ljava/lang/String;)V");
    jobject statFs = env->NewObject(statFsClass, statFsInit, path);

    jmethodID getTotalBytes = env->GetMethodID(statFsClass, "getTotalBytes", "()J");
    jmethodID getAvailBytes = env->GetMethodID(statFsClass, "getAvailableBytes", "()J");

    jlong total     = env->CallLongMethod(statFs, getTotalBytes);
    jlong available = env->CallLongMethod(statFs, getAvailBytes);

    info.totalStorage     = (long)(total     / (1024 * 1024));
    info.availableStorage = (long)(available / (1024 * 1024));
    info.usagePercent     = ((total - available) / (float)total) * 100.0f;

    env->DeleteLocalRef(statFs);
    env->DeleteLocalRef(path);
    env->DeleteLocalRef(extDir);
    env->DeleteLocalRef(fileClass);
    env->DeleteLocalRef(statFsClass);
    env->DeleteLocalRef(environmentClass);

    return info;
}

// ============================================================
//  NETWORK OPTIMIZER
// ============================================================

bool isWifiConnected(JNIEnv* env, jobject context)
{
    jclass ctxClass = env->GetObjectClass(context);
    jmethodID getService = env->GetMethodID(
        ctxClass, "getSystemService",
        "(Ljava/lang/String;)Ljava/lang/Object;");

    jstring cmStr = env->NewStringUTF("connectivity");
    jobject cm = env->CallObjectMethod(context, getService, cmStr);

    jclass cmClass = env->GetObjectClass(cm);
    jmethodID getActiveNetInfo = env->GetMethodID(
        cmClass, "getActiveNetworkInfo",
        "()Landroid/net/NetworkInfo;");
    jobject netInfo = env->CallObjectMethod(cm, getActiveNetInfo);

    if (netInfo == nullptr) return false;

    jclass netInfoClass = env->GetObjectClass(netInfo);
    jmethodID getType = env->GetMethodID(netInfoClass, "getType", "()I");
    jmethodID isConn  = env->GetMethodID(netInfoClass, "isConnected", "()Z");

    int type        = env->CallIntMethod(netInfo, getType);
    bool connected  = env->CallBooleanMethod(netInfo, isConn);

    env->DeleteLocalRef(cmStr);
    env->DeleteLocalRef(cm);
    env->DeleteLocalRef(cmClass);
    env->DeleteLocalRef(netInfo);
    env->DeleteLocalRef(netInfoClass);
    env->DeleteLocalRef(ctxClass);

    return (type == 1 && connected); // 1 = WIFI
}

// ============================================================
//  FRAME LIMITER - Dùng trong game loop
// ============================================================
class FrameLimiter {
public:
    explicit FrameLimiter(int fps)
        : frameTime(std::chrono::duration<double>(1.0 / fps)),
          lastFrame(std::chrono::high_resolution_clock::now())
    {}

    void wait()
    {
        auto now     = std::chrono::high_resolution_clock::now();
        auto elapsed = now - lastFrame;

        if (elapsed < frameTime)
            std::this_thread::sleep_for(frameTime - elapsed);

        lastFrame = std::chrono::high_resolution_clock::now();
    }

private:
    std::chrono::duration<double> frameTime;
    std::chrono::high_resolution_clock::time_point lastFrame;
};

// ============================================================
//  JNI ENTRY POINTS - Gọi từ Java/Kotlin
// ============================================================
extern "C" {

// Bật Game Mode
JNIEXPORT void JNICALL
Java_com_yourapp_DeviceOptimizer_enableGameMode(
    JNIEnv* env, jobject /* this */, jobject context, jobject nativeWindow)
{
    ANativeWindow* window = ANativeWindow_fromSurface(env, nativeWindow);
    enableGameMode(env, context, window);
    if (window) ANativeWindow_release(window);
}

// Tắt Game Mode
JNIEXPORT void JNICALL
Java_com_yourapp_DeviceOptimizer_disableGameMode(
    JNIEnv* env, jobject /* this */, jobject context)
{
    disableGameMode(env, context);
}

// Lấy CPU usage
JNIEXPORT jfloat JNICALL
Java_com_yourapp_DeviceOptimizer_getCPUUsage(
    JNIEnv* /* env */, jobject /* this */)
{
    return getCPUUsage();
}

// Lấy RAM khả dụng
JNIEXPORT jlong JNICALL
Java_com_yourapp_DeviceOptimizer_getAvailableRAM(
    JNIEnv* env, jobject /* this */, jobject context)
{
    return getAvailableRAM(env, context);
}

// Lấy mức pin
JNIEXPORT jfloat JNICALL
Java_com_yourapp_DeviceOptimizer_getBatteryLevel(
    JNIEnv* env, jobject /* this */, jobject context)
{
    return getBatteryLevel(env, context);
}

// Lấy nhiệt độ
JNIEXPORT jfloat JNICALL
Java_com_yourapp_DeviceOptimizer_getTemperature(
    JNIEnv* env, jobject /* this */, jobject context)
{
    return getTemperature(env, context);
}

// Kiểm tra Wifi
JNIEXPORT jboolean JNICALL
Java_com_yourapp_DeviceOptimizer_isWifiConnected(
    JNIEnv* env, jobject /* this */, jobject context)
{
    return isWifiConnected(env, context);
}

} // extern "C"

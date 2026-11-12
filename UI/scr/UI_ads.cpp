#include "UI_ads.h"
#include "UI_lang.h"
#include <iostream>

#if defined(__ANDROID__)
#include <jni.h>
#include <android/log.h>

#define LOG_TAG "EnergyCrisisAds"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

static JavaVM* g_vm = nullptr;

extern "C" {

JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* /*reserved*/) {
    g_vm = vm;
    LOGI("JNI_OnLoad initialized successfully");
    return JNI_VERSION_1_6;
}

JNIEXPORT void JNICALL Java_bg_energycrisis_game_MainActivity_nativeOnRewardEarned(JNIEnv* /*env*/, jclass /*clazz*/) {
    LOGI("JNI: nativeOnRewardEarned received");
    AdManager::get().onRewardEarned();
}

JNIEXPORT void JNICALL Java_bg_energycrisis_game_MainActivity_nativeOnAdClosed(JNIEnv* /*env*/, jclass /*clazz*/) {
    LOGI("JNI: nativeOnAdClosed received");
    AdManager::get().onAdClosed();
}

JNIEXPORT void JNICALL Java_bg_energycrisis_game_MainActivity_nativeOnAdFailed(JNIEnv* /*env*/, jclass /*clazz*/) {
    LOGE("JNI: nativeOnAdFailed received");
    AdManager::get().onAdFailed();
}

JNIEXPORT void JNICALL Java_bg_energycrisis_game_MainActivity_nativeOnAdNotReady(JNIEnv* /*env*/, jclass /*clazz*/) {
    LOGI("JNI: nativeOnAdNotReady received");
    AdManager::get().onAdNotReady();
}

} // extern "C"
#endif

AdManager& AdManager::get() {
    static AdManager instance;
    return instance;
}

AdManager::AdManager() {
#if !defined(__ANDROID__)
    // On desktop / arcade releases, default to unlocked so local testing is friction-free
    m_30mUnlocked = true;
#else
    m_30mUnlocked = false;
#endif
}

bool AdManager::is30mUnlocked() const {
    return m_30mUnlocked;
}

void AdManager::set30mUnlocked(bool unlocked) {
    m_30mUnlocked = unlocked;
}

bool AdManager::pollRewardEarned() {
    if (m_rewardEarnedFlag) {
        m_rewardEarnedFlag = false;
        return true;
    }
    return false;
}

AdStatus AdManager::getStatus() const {
    return m_status;
}

const std::string& AdManager::getStatusMessage() const {
    return m_statusMessage;
}

void AdManager::clearStatusMessage() {
    m_statusMessage.clear();
}

bool AdManager::isAdReady() const {
#if defined(__ANDROID__)
    if (!g_vm) return false;
    JNIEnv* env = nullptr;
    bool attached = false;
    jint getEnvRes = g_vm->GetEnv((void**)&env, JNI_VERSION_1_6);
    if (getEnvRes == JNI_EDETACHED) {
        if (g_vm->AttachCurrentThread(&env, nullptr) == JNI_OK) {
            attached = true;
        } else {
            return false;
        }
    } else if (getEnvRes != JNI_OK) {
        return false;
    }

    bool ready = false;
    jclass activityClass = env->FindClass("bg/energycrisis/game/MainActivity");
    if (activityClass) {
        jmethodID isReadyMethod = env->GetStaticMethodID(activityClass, "isAdReadyFromNative", "()Z");
        if (isReadyMethod) {
            ready = env->CallStaticBooleanMethod(activityClass, isReadyMethod);
        }
        env->DeleteLocalRef(activityClass);
    }

    if (attached) {
        g_vm->DetachCurrentThread();
    }
    return ready;
#else
    return true;
#endif
}

void AdManager::showRewardedAd() {
    m_status = AdStatus::SHOWING;
    m_statusMessage = Lang::tr("menu.ad_status_loading");

#if defined(__ANDROID__)
    if (!g_vm) {
        m_status = AdStatus::FAILED;
        m_statusMessage = Lang::tr("menu.ad_status_failed");
        return;
    }

    JNIEnv* env = nullptr;
    bool attached = false;
    jint getEnvRes = g_vm->GetEnv((void**)&env, JNI_VERSION_1_6);
    if (getEnvRes == JNI_EDETACHED) {
        if (g_vm->AttachCurrentThread(&env, nullptr) == JNI_OK) {
            attached = true;
        } else {
            m_status = AdStatus::FAILED;
            m_statusMessage = Lang::tr("menu.ad_status_failed");
            return;
        }
    } else if (getEnvRes != JNI_OK) {
        m_status = AdStatus::FAILED;
        m_statusMessage = Lang::tr("menu.ad_status_failed");
        return;
    }

    jclass activityClass = env->FindClass("bg/energycrisis/game/MainActivity");
    if (activityClass) {
        jmethodID showAdMethod = env->GetStaticMethodID(activityClass, "showAdFromNative", "()V");
        if (showAdMethod) {
            env->CallStaticVoidMethod(activityClass, showAdMethod);
        } else {
            m_status = AdStatus::FAILED;
            m_statusMessage = Lang::tr("menu.ad_status_failed");
        }
        env->DeleteLocalRef(activityClass);
    } else {
        m_status = AdStatus::FAILED;
        m_statusMessage = Lang::tr("menu.ad_status_failed");
    }

    if (attached) {
        g_vm->DetachCurrentThread();
    }
#else
    // Desktop simulation: directly reward
    onRewardEarned();
#endif
}

void AdManager::onRewardEarned() {
    m_30mUnlocked = true;
    m_rewardEarnedFlag = true;
    m_status = AdStatus::REWARD_EARNED;
    m_statusMessage = Lang::tr("menu.ad_reward_success");
}

void AdManager::onAdClosed() {
    m_status = AdStatus::CLOSED;
}

void AdManager::onAdFailed() {
    m_status = AdStatus::FAILED;
    m_statusMessage = Lang::tr("menu.ad_status_failed");
}

void AdManager::onAdNotReady() {
    m_status = AdStatus::NOT_READY;
    m_statusMessage = Lang::tr("menu.ad_status_not_ready");
}


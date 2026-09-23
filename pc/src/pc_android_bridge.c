#include "pc_android_bridge.h"
#include "pc_settings.h"

static volatile int s_android_actions = 0;

void pc_android_request_action(int action_flags) {
#ifdef TARGET_ANDROID
    __sync_fetch_and_or(&s_android_actions, action_flags);
#else
    (void)action_flags;
#endif
}

int pc_android_take_actions(void) {
#ifdef TARGET_ANDROID
    return __sync_lock_test_and_set(&s_android_actions, 0);
#else
    return 0;
#endif
}

#ifdef TARGET_ANDROID
#include <jni.h>

JNIEXPORT jint JNICALL
Java_com_acgc_port_GameActivity_nativeTakePendingActions(JNIEnv* env, jclass clazz) {
    (void)env;
    (void)clazz;
    return (jint)pc_android_take_actions();
}

JNIEXPORT jint JNICALL
Java_com_acgc_port_GameActivity_nativeGetAndroidSetting(JNIEnv* env, jclass clazz, jint key) {
    (void)env;
    (void)clazz;

    switch ((int)key) {
        case 1: return (jint)g_pc_settings.touch_controls_visible;
        case 2: return (jint)g_pc_settings.touch_opacity;
        case 3: return (jint)g_pc_settings.touch_scale;
        case 4: return (jint)g_pc_settings.touch_hide_with_gamepad;
        case 5: return (jint)g_pc_settings.stick_deadzone;
        case 6: return (jint)g_pc_settings.stick_sensitivity;
        case 7: return (jint)g_pc_settings.cstick_deadzone;
        case 8: return (jint)g_pc_settings.cstick_sensitivity;
        default: return 0;
    }
}
#endif

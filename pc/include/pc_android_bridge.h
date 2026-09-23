#ifndef PC_ANDROID_BRIDGE_H
#define PC_ANDROID_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Bit flags consumed by GameActivity on the Android UI thread. */
enum {
    PC_ANDROID_ACTION_APPLY_TOUCH     = 1 << 0,
    PC_ANDROID_ACTION_EDIT_CONTROLS   = 1 << 1,
    PC_ANDROID_ACTION_RESET_CONTROLS  = 1 << 2,
    PC_ANDROID_ACTION_SYNC_DATA       = 1 << 3,
    PC_ANDROID_ACTION_RELOAD_CONTENT  = 1 << 4,
    PC_ANDROID_ACTION_CHANGE_FOLDER   = 1 << 5,
    PC_ANDROID_ACTION_RESTART_GAME    = 1 << 6,
    PC_ANDROID_ACTION_CLOSE_APP       = 1 << 7,
};

void pc_android_request_action(int action_flags);
int pc_android_take_actions(void);

#ifdef __cplusplus
}
#endif

#endif /* PC_ANDROID_BRIDGE_H */

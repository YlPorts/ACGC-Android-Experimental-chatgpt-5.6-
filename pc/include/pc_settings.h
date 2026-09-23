#ifndef PC_SETTINGS_H
#define PC_SETTINGS_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int window_width;
    int window_height;
    int fullscreen;       /* 0=windowed, 1=fullscreen, 2=borderless */
    int vsync;            /* 0=off, 1=on */
    int max_fps;          /* 0=uncapped, otherwise frame limiter target */
    int msaa;             /* 0=off, 2/4/8=samples */
    int aspect_ratio;     /* 0=4:3, 1=16:9, 2=21:9, 3=automatic window aspect */
    int preload_textures; /* 0=off (load on demand), 1=on (load all at startup), 2=on + cache file */
    int texture_pack_enabled; /* 0=original textures, 1=allow replacements */
    int disable_resetti;  /* 0=normal (Resetti appears on reset), 1=disable reset penalty */
    int disable_shop_visitor_req; /* 0=normal (Nookington's needs a foreign-town shopper), 1=skip requirement */
    int borderless_acres; /* 0=original acre transitions (faster, draws less), 1=continuous camera/movement */
    int nes_aspect;       /* NES emulator aspect: 0=fullscreen stretch, 1=4:3 pillarbox (default) */
    int master_volume;    /* Applied at the PC audio output, 0-100 (default 100) */
    int stick_deadzone;      /* Main-stick deadzone, percent 0-40 (default 12) */
    int stick_sensitivity;   /* Main-stick response, percent 50-150 (default 100) */
    int cstick_deadzone;     /* C-stick deadzone, percent 0-40 (default 12) */
    int cstick_sensitivity;  /* C-stick response, percent 50-150 (default 100) */
    int touch_controls_visible;   /* Android touch overlay: 0=hidden, 1=visible */
    int touch_opacity;            /* Android touch overlay opacity, percent 25-100 */
    int touch_scale;              /* Android touch overlay scale, percent 75-150 */
    int touch_hide_with_gamepad;  /* Hide touch controls while a physical pad is connected */
    char language[32];            /* en=ROM original; other codes use languages/<code>/ */
} PCSettings;

extern PCSettings g_pc_settings;

void pc_settings_load(void);
void pc_settings_save(void);
void pc_settings_apply(void);
void pc_settings_cycle_resolution(int* width, int* height, int dir);

#ifdef __cplusplus
}
#endif

#endif /* PC_SETTINGS_H */

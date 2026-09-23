/* pc_settings.c - runtime settings loaded from settings.ini */
#include "pc_settings.h"
#include "pc_platform.h"
#include "m_player_lib.h"
#include "ac_birth_control.h"

#include <ctype.h>

#ifdef TARGET_ANDROID
#define PC_DEFAULT_MSAA 0
#define PC_DEFAULT_ASPECT 1
#define PC_DEFAULT_MSAA_TEXT "0"
#define PC_DEFAULT_ASPECT_TEXT "1"
#else
#define PC_DEFAULT_MSAA 4
#define PC_DEFAULT_ASPECT 3
#define PC_DEFAULT_MSAA_TEXT "4"
#define PC_DEFAULT_ASPECT_TEXT "3"
#endif

PCSettings g_pc_settings = {
    .window_width  = PC_SCREEN_WIDTH,
    .window_height = PC_SCREEN_HEIGHT,
    .fullscreen    = 0,
    .vsync         = 0,
    .max_fps       = 60,
    .msaa          = PC_DEFAULT_MSAA,
    .aspect_ratio  = PC_DEFAULT_ASPECT,
    .preload_textures = 0,
    .texture_pack_enabled = 1,
    .disable_resetti = 0,
    .disable_shop_visitor_req = 0,
    .borderless_acres = 1,
    .nes_aspect = 1,
    .master_volume = 100,
    .stick_deadzone = 12,
    .stick_sensitivity = 100,
    .cstick_deadzone = 12,
    .cstick_sensitivity = 100,
    .touch_controls_visible = 1,
    .touch_opacity = 75,
    .touch_scale = 100,
    .touch_hide_with_gamepad = 0,
    .language = "en",
};

static const char* SETTINGS_FILE = "settings.ini";

static const char* DEFAULT_SETTINGS =
    "[Graphics]\n"
#ifndef TARGET_ANDROID
    "# Window size (ignored in fullscreen)\n"
    "window_width = 640\n"
    "window_height = 480\n"
    "\n"
    "# 0 = windowed, 1 = fullscreen, 2 = borderless fullscreen\n"
    "fullscreen = 0\n"
    "\n"
#endif
    "# Vertical sync: 0 = off, 1 = on\n"
    "vsync = 0\n"
    "\n"
#ifdef TARGET_ANDROID
    "# Max FPS: 60, 90, or 120\n"
#else
    "# Max FPS: 60, 120, 240, or 0 for uncapped\n"
#endif
    "max_fps = 60\n"
    "\n"
#ifdef TARGET_ANDROID
    "# Image quality / anti-aliasing: 0 = normal, 2 = high\n"
#else
    "# Anti-aliasing samples: 0 = off, 2, 4, or 8\n"
#endif
    "msaa = " PC_DEFAULT_MSAA_TEXT "\n"
    "\n"
#ifdef TARGET_ANDROID
    "# Game image aspect: 0 = 4:3, 1 = 16:9, 2 = 21:9\n"
#else
    "# Game image aspect: 0 = 4:3, 1 = 16:9, 2 = 21:9, 3 = automatic\n"
#endif
    "aspect_ratio = " PC_DEFAULT_ASPECT_TEXT "\n"
    "\n"
    "# Custom texture replacements: 0 = original, 1 = enabled\n"
    "texture_pack_enabled = 1\n"
    "\n"
#ifndef TARGET_ANDROID
    "[Enhancements]\n"
    "# Preload HD textures at startup: 0 = off (load on demand), 1 = preload, 2 = preload + cache file (fastest)\n"
    "preload_textures = 0\n"
    "\n"
#endif
    "[Gameplay]\n"
    "# Disable Mr. Resetti: 0 = normal, 1 = disable\n"
    "disable_resetti = 0\n"
    "\n"
    "# Shop upgrade visitor requirement (Nookington's needs a shopper from another town): 0 = required, 1 = not required\n"
    "disable_shop_visitor_req = 0\n"
    "\n"
#ifndef TARGET_ANDROID
    "# Borderless acres: 0 = original acre transitions (faster, draws less), 1 = continuous movement/camera\n"
    "borderless_acres = 1\n"
    "\n"
#endif
    "# NES emulator aspect ratio: 0 = stretch to fullscreen, 1 = 4:3 pillarbox\n"
    "nes_aspect = 1\n"
    "\n"
    "[Audio]\n"
    "# Master output volume as a percentage (0-100)\n"
    "master_volume = 100\n"
    "\n"
    "[Input]\n"
    "# Stick deadzones (0-40) and sensitivity (50-150 percent)\n"
    "stick_deadzone = 12\n"
    "stick_sensitivity = 100\n"
    "cstick_deadzone = 12\n"
    "cstick_sensitivity = 100\n"
#ifdef TARGET_ANDROID
    "\n"
    "[Touch]\n"
    "touch_controls_visible = 1\n"
    "touch_opacity = 75\n"
    "touch_scale = 100\n"
    "touch_hide_with_gamepad = 0\n"
#endif
    "\n"
    "[Language]\n"
    "# en = original ROM text; other codes load languages/<code>/aram\n"
    "language = en\n"
    ;

static const char* skip_ws(const char* s) {
    while (*s == ' ' || *s == '\t') s++;
    return s;
}

static void trim_end(char* s) {
    int len = (int)strlen(s);
    while (len > 0 && (s[len-1] == ' ' || s[len-1] == '\t' ||
                       s[len-1] == '\r' || s[len-1] == '\n')) {
        s[--len] = '\0';
    }
}

static void apply_setting(const char* key, const char* value) {
    int val = atoi(value);

    if (strcmp(key, "window_width") == 0) {
        if (val >= 640) g_pc_settings.window_width = val;
    } else if (strcmp(key, "window_height") == 0) {
        if (val >= 480) g_pc_settings.window_height = val;
    } else if (strcmp(key, "fullscreen") == 0) {
        if (val >= 0 && val <= 2) g_pc_settings.fullscreen = val;
    } else if (strcmp(key, "vsync") == 0) {
        if (val == 0 || val == 1) g_pc_settings.vsync = val;
    } else if (strcmp(key, "max_fps") == 0) {
        if (val >= 0) {
            g_pc_settings.max_fps = val;
        }
    } else if (strcmp(key, "msaa") == 0) {
        if (val == 0 || val == 2 || val == 4 || val == 8)
            g_pc_settings.msaa = val;
    } else if (strcmp(key, "aspect_ratio") == 0) {
        if (val >= 0 && val <= 3) g_pc_settings.aspect_ratio = val;
    } else if (strcmp(key, "preload_textures") == 0) {
        if (val >= 0 && val <= 2) g_pc_settings.preload_textures = val;
    } else if (strcmp(key, "texture_pack_enabled") == 0) {
        if (val == 0 || val == 1) g_pc_settings.texture_pack_enabled = val;
    } else if (strcmp(key, "disable_resetti") == 0) {
        if (val == 0 || val == 1) g_pc_settings.disable_resetti = val;
    } else if (strcmp(key, "disable_shop_visitor_req") == 0) {
        if (val == 0 || val == 1) g_pc_settings.disable_shop_visitor_req = val;
    } else if (strcmp(key, "borderless_acres") == 0) {
        if (val == 0 || val == 1) g_pc_settings.borderless_acres = val;
    } else if (strcmp(key, "nes_aspect") == 0) {
        if (val == 0 || val == 1) g_pc_settings.nes_aspect = val;
    } else if (strcmp(key, "master_volume") == 0) {
        if (val >= 0 && val <= 100) g_pc_settings.master_volume = val;
    } else if (strcmp(key, "stick_deadzone") == 0) {
        if (val >= 0 && val <= 40) g_pc_settings.stick_deadzone = val;
    } else if (strcmp(key, "stick_sensitivity") == 0) {
        if (val >= 50 && val <= 150) g_pc_settings.stick_sensitivity = val;
    } else if (strcmp(key, "cstick_deadzone") == 0) {
        if (val >= 0 && val <= 40) g_pc_settings.cstick_deadzone = val;
    } else if (strcmp(key, "cstick_sensitivity") == 0) {
        if (val >= 50 && val <= 150) g_pc_settings.cstick_sensitivity = val;
    } else if (strcmp(key, "touch_controls_visible") == 0) {
        if (val == 0 || val == 1) g_pc_settings.touch_controls_visible = val;
    } else if (strcmp(key, "touch_opacity") == 0) {
        if (val >= 25 && val <= 100) g_pc_settings.touch_opacity = val;
    } else if (strcmp(key, "touch_scale") == 0) {
        if (val >= 75 && val <= 150) g_pc_settings.touch_scale = val;
    } else if (strcmp(key, "touch_hide_with_gamepad") == 0) {
        if (val == 0 || val == 1) g_pc_settings.touch_hide_with_gamepad = val;
    } else if (strcmp(key, "language") == 0) {
        size_t i;
        size_t len = strlen(value);
        int valid = len > 0 && len < sizeof(g_pc_settings.language);
        for (i = 0; valid && i < len; i++) {
            unsigned char c = (unsigned char)value[i];
            if (!(isalnum(c) || c == '-' || c == '_')) valid = 0;
        }
        if (valid) {
            strncpy(g_pc_settings.language, value, sizeof(g_pc_settings.language) - 1);
            g_pc_settings.language[sizeof(g_pc_settings.language) - 1] = '\0';
        }
    }
}

static void apply_frame_limit_setting(void) {
    int max_fps;

    if (g_pc_frame_limit_override >= 0) {
        g_pc_settings.max_fps = g_pc_frame_limit_override;
        g_pc_frame_limit_override = -1;
    }

    max_fps = g_pc_settings.max_fps;

    if (max_fps <= 0) {
        max_fps = 0;
    }

    g_frame_limiter = (u32)max_fps;
}

static void apply_borderless_acres_setting(void) {
    int enabled = g_pc_settings.borderless_acres != 0;

    if (enabled && !g_mPlib_wade_disabled) {
        aBC_RequestNearbyRefresh();
    }
    g_mPlib_wade_disabled = enabled;
}

static void sanitize_platform_settings(void) {
#ifdef TARGET_ANDROID
    /* These desktop values may exist in a settings.ini copied from PC, but
     * Android owns the surface size and always runs borderless landscape. */
    g_pc_settings.fullscreen = 0;
    g_pc_settings.window_width = PC_SCREEN_WIDTH;
    g_pc_settings.window_height = PC_SCREEN_HEIGHT;
    if (g_pc_settings.aspect_ratio < 0 || g_pc_settings.aspect_ratio > 2)
        g_pc_settings.aspect_ratio = 1;
    if (g_pc_settings.msaa != 0 && g_pc_settings.msaa != 2) g_pc_settings.msaa = 0;
    /* The port uses delta time for simulation. Restrict mobile choices to
     * tested targets; VSync may still cap these to the panel refresh rate. */
    if (g_pc_settings.max_fps != 60 && g_pc_settings.max_fps != 90 &&
        g_pc_settings.max_fps != 120) {
        g_pc_settings.max_fps = 60;
    }
    /* On-demand replacement is safer in a 32-bit address space. */
    g_pc_settings.preload_textures = 0;
    if (g_pc_settings.texture_pack_enabled != 0) g_pc_settings.texture_pack_enabled = 1;
    /* Preserve the original acre-transition behavior on mobile. */
    g_pc_settings.borderless_acres = 0;
    if (g_pc_settings.stick_deadzone < 0 || g_pc_settings.stick_deadzone > 40)
        g_pc_settings.stick_deadzone = 12;
    if (g_pc_settings.cstick_deadzone < 0 || g_pc_settings.cstick_deadzone > 40)
        g_pc_settings.cstick_deadzone = 12;
    if (g_pc_settings.stick_sensitivity < 50 || g_pc_settings.stick_sensitivity > 150)
        g_pc_settings.stick_sensitivity = 100;
    if (g_pc_settings.cstick_sensitivity < 50 || g_pc_settings.cstick_sensitivity > 150)
        g_pc_settings.cstick_sensitivity = 100;
    if (g_pc_settings.touch_controls_visible != 0) g_pc_settings.touch_controls_visible = 1;
    if (g_pc_settings.touch_opacity < 25 || g_pc_settings.touch_opacity > 100)
        g_pc_settings.touch_opacity = 75;
    if (g_pc_settings.touch_scale < 75 || g_pc_settings.touch_scale > 150)
        g_pc_settings.touch_scale = 100;
    if (g_pc_settings.touch_hide_with_gamepad != 0) g_pc_settings.touch_hide_with_gamepad = 1;
#endif
}

static void write_defaults(const char* path) {
    FILE* f = fopen(path, "w");
    if (f) {
        fputs(DEFAULT_SETTINGS, f);
        fclose(f);
    }
}

void pc_settings_save(void) {
    FILE* f = fopen(SETTINGS_FILE, "w");
    if (!f) {
        printf("[Settings] Failed to write %s\n", SETTINGS_FILE);
        return;
    }
    fprintf(f, "[Graphics]\n");
#ifndef TARGET_ANDROID
    fprintf(f, "# Window size (ignored in fullscreen)\n");
    fprintf(f, "window_width = %d\n", g_pc_settings.window_width);
    fprintf(f, "window_height = %d\n", g_pc_settings.window_height);
    fprintf(f, "\n");
    fprintf(f, "# 0 = windowed, 1 = fullscreen, 2 = borderless fullscreen\n");
    fprintf(f, "fullscreen = %d\n", g_pc_settings.fullscreen);
    fprintf(f, "\n");
#endif
    fprintf(f, "# Vertical sync: 0 = off, 1 = on\n");
    fprintf(f, "vsync = %d\n", g_pc_settings.vsync);
    fprintf(f, "\n");
#ifdef TARGET_ANDROID
    fprintf(f, "# Max FPS: 60, 90, or 120\n");
#else
    fprintf(f, "# Max FPS: 60, 120, 240, or 0 for uncapped\n");
#endif
    fprintf(f, "max_fps = %d\n", g_pc_settings.max_fps);
    fprintf(f, "\n");
#ifdef TARGET_ANDROID
    fprintf(f, "# Image quality / anti-aliasing: 0 = normal, 2 = high\n");
#else
    fprintf(f, "# Anti-aliasing samples: 0 = off, 2, 4, or 8\n");
#endif
    fprintf(f, "msaa = %d\n", g_pc_settings.msaa);
    fprintf(f, "\n");
#ifdef TARGET_ANDROID
    fprintf(f, "# Game image aspect: 0 = 4:3, 1 = 16:9, 2 = 21:9\n");
#else
    fprintf(f, "# Game image aspect: 0 = 4:3, 1 = 16:9, 2 = 21:9, 3 = automatic\n");
#endif
    fprintf(f, "aspect_ratio = %d\n", g_pc_settings.aspect_ratio);
    fprintf(f, "\n");
    fprintf(f, "# Custom texture replacements: 0 = original, 1 = enabled\n");
    fprintf(f, "texture_pack_enabled = %d\n", g_pc_settings.texture_pack_enabled);
    fprintf(f, "\n");
#ifndef TARGET_ANDROID
    fprintf(f, "[Enhancements]\n");
    fprintf(f, "# Preload HD textures at startup: 0 = off (load on demand), 1 = preload, 2 = preload + cache file (fastest)\n");
    fprintf(f, "preload_textures = %d\n", g_pc_settings.preload_textures);
    fprintf(f, "\n");
#endif
    fprintf(f, "[Gameplay]\n");
    fprintf(f, "# Disable Mr. Resetti: 0 = normal, 1 = disable\n");
    fprintf(f, "disable_resetti = %d\n", g_pc_settings.disable_resetti);
    fprintf(f, "\n");
    fprintf(f, "# Shop upgrade visitor requirement (Nookington's needs a shopper from another town): 0 = required, 1 = not required\n");
    fprintf(f, "disable_shop_visitor_req = %d\n", g_pc_settings.disable_shop_visitor_req);
    fprintf(f, "\n");
#ifndef TARGET_ANDROID
    fprintf(f, "# Borderless acres: 0 = original acre transitions (faster, draws less), 1 = continuous movement/camera\n");
    fprintf(f, "borderless_acres = %d\n", g_pc_settings.borderless_acres);
    fprintf(f, "\n");
#endif
    fprintf(f, "# NES emulator aspect ratio: 0 = stretch to fullscreen, 1 = 4:3 pillarbox\n");
    fprintf(f, "nes_aspect = %d\n", g_pc_settings.nes_aspect);
    fprintf(f, "\n");
    fprintf(f, "[Audio]\n");
    fprintf(f, "# Master output volume as a percentage (0-100)\n");
    fprintf(f, "master_volume = %d\n", g_pc_settings.master_volume);
    fprintf(f, "\n");
    fprintf(f, "[Input]\n");
    fprintf(f, "# Stick deadzones (0-40) and sensitivity (50-150 percent)\n");
    fprintf(f, "stick_deadzone = %d\n", g_pc_settings.stick_deadzone);
    fprintf(f, "stick_sensitivity = %d\n", g_pc_settings.stick_sensitivity);
    fprintf(f, "cstick_deadzone = %d\n", g_pc_settings.cstick_deadzone);
    fprintf(f, "cstick_sensitivity = %d\n", g_pc_settings.cstick_sensitivity);
#ifdef TARGET_ANDROID
    fprintf(f, "\n[Touch]\n");
    fprintf(f, "touch_controls_visible = %d\n", g_pc_settings.touch_controls_visible);
    fprintf(f, "touch_opacity = %d\n", g_pc_settings.touch_opacity);
    fprintf(f, "touch_scale = %d\n", g_pc_settings.touch_scale);
    fprintf(f, "touch_hide_with_gamepad = %d\n", g_pc_settings.touch_hide_with_gamepad);
#endif
    fprintf(f, "\n[Language]\n");
    fprintf(f, "# en = original ROM text; other codes load languages/<code>/aram\n");
    fprintf(f, "language = %s\n", g_pc_settings.language);
    fclose(f);
    printf("[Settings] Saved %s\n", SETTINGS_FILE);
}

/* Accessor for TUs that can't include pc_settings.h (pc_nes_fixnes.c). */
int pc_settings_get_nes_aspect(void) {
    return g_pc_settings.nes_aspect;
}

/* --- Resolution preset table (shared) ---
 * Ordered by width then height. The desktop's native size is injected at
 * first use (de-duplicated against the static list) so the user can snap
 * to whatever their monitor is running. Multiple presets share widths now
 * (e.g. 1280x720 vs 1280x960), so cycling is index-based rather than the
 * old width-comparison. */
#define RES_MAX 32
static int res_w_tbl[RES_MAX];
static int res_h_tbl[RES_MAX];
static int res_count = 0;

static void add_preset(int w, int h) {
    if (res_count >= RES_MAX) return;
    for (int i = 0; i < res_count; i++) {
        if (res_w_tbl[i] == w && res_h_tbl[i] == h) return; /* de-dupe */
    }
    int at = res_count;
    for (int i = 0; i < res_count; i++) {
        if (res_w_tbl[i] > w || (res_w_tbl[i] == w && res_h_tbl[i] > h)) {
            at = i;
            break;
        }
    }
    for (int i = res_count; i > at; i--) {
        res_w_tbl[i] = res_w_tbl[i - 1];
        res_h_tbl[i] = res_h_tbl[i - 1];
    }
    res_w_tbl[at] = w;
    res_h_tbl[at] = h;
    res_count++;
}

static void ensure_presets(void) {
    if (res_count > 0) return;
    /* 4:3 */
    add_preset(640,  480);
    add_preset(800,  600);
    add_preset(960,  720);
    add_preset(1024, 768);
    add_preset(1152, 864);
    add_preset(1280, 960);
    add_preset(1400, 1050);
    add_preset(1600, 1200);
    /* 16:9 */
    add_preset(1280, 720);
    add_preset(1366, 768);
    add_preset(1600, 900);
    add_preset(1920, 1080);
    add_preset(2560, 1440);
    add_preset(3840, 2160);
    /* Desktop native (inserted sorted, de-duped against the list above). */
    SDL_DisplayMode mode;
    if (SDL_GetDesktopDisplayMode(0, &mode) == 0) {
        add_preset(mode.w, mode.h);
    }
}

/* Find the closest preset by total pixel count. Used when the caller's
 * current (w, h) isn't in the list (e.g. custom settings.ini value). */
static int nearest_index(int w, int h) {
    long long want = (long long)w * h;
    int best = 0;
    long long best_diff = (long long)1 << 62;
    for (int i = 0; i < res_count; i++) {
        long long diff = (long long)res_w_tbl[i] * res_h_tbl[i] - want;
        if (diff < 0) diff = -diff;
        if (diff < best_diff) { best_diff = diff; best = i; }
    }
    return best;
}

void pc_settings_cycle_resolution(int* width, int* height, int dir) {
    ensure_presets();
    int cur = -1;
    for (int i = 0; i < res_count; i++) {
        if (res_w_tbl[i] == *width && res_h_tbl[i] == *height) { cur = i; break; }
    }
    if (cur < 0) cur = nearest_index(*width, *height);

    if (dir > 0 && cur < res_count - 1) cur++;
    else if (dir < 0 && cur > 0) cur--;

    *width  = res_w_tbl[cur];
    *height = res_h_tbl[cur];
}

void pc_settings_apply(void) {
    apply_frame_limit_setting();
    apply_borderless_acres_setting();

    if (!g_pc_window) return;

#ifndef TARGET_ANDROID
    int w = g_pc_settings.window_width;
    int h = g_pc_settings.window_height;

    switch (g_pc_settings.fullscreen) {
        case 1: {
            /* Exclusive fullscreen at the user's chosen resolution. SDL
             * needs the display mode set before transitioning to
             * SDL_WINDOW_FULLSCREEN or it'll just use the desktop mode. */
            SDL_DisplayMode target = { 0 };
            target.w = w;
            target.h = h;
            int display_idx = SDL_GetWindowDisplayIndex(g_pc_window);
            if (display_idx < 0) display_idx = 0;
            SDL_DisplayMode closest;
            if (SDL_GetClosestDisplayMode(display_idx, &target, &closest)) {
                SDL_SetWindowDisplayMode(g_pc_window, &closest);
            }
            SDL_SetWindowFullscreen(g_pc_window, SDL_WINDOW_FULLSCREEN);
            break;
        }
        case 2: {
            /* Borderless window at the user's chosen size, centred. Exit
             * any fullscreen mode first (including FULLSCREEN_DESKTOP)
             * so the resize sticks. */
            SDL_SetWindowFullscreen(g_pc_window, 0);
            SDL_SetWindowBordered(g_pc_window, SDL_FALSE);
            SDL_SetWindowSize(g_pc_window, w, h);
            SDL_SetWindowPosition(g_pc_window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
            break;
        }
        case 0:
        default: {
            SDL_SetWindowFullscreen(g_pc_window, 0);
            SDL_SetWindowBordered(g_pc_window, SDL_TRUE);
            SDL_SetWindowSize(g_pc_window, w, h);
            SDL_SetWindowPosition(g_pc_window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
            break;
        }
    }
#endif

    SDL_GL_SetSwapInterval(g_pc_settings.vsync);
    pc_platform_update_window_size();

    printf("[Settings] Applied: %dx%d fullscreen=%d aspect=%d vsync=%d max_fps=%d msaa=%d\n",
           g_pc_settings.window_width, g_pc_settings.window_height,
           g_pc_settings.fullscreen, g_pc_settings.aspect_ratio, g_pc_settings.vsync,
           g_pc_settings.max_fps, g_pc_settings.msaa);
}

void pc_settings_load(void) {
    FILE* f = fopen(SETTINGS_FILE, "r");
    if (!f) {
        write_defaults(SETTINGS_FILE);
        sanitize_platform_settings();
        apply_frame_limit_setting();
        apply_borderless_acres_setting();
        printf("[Settings] Created default %s\n", SETTINGS_FILE);
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        const char* p = skip_ws(line);

        if (*p == '#' || *p == ';' || *p == '\0' || *p == '\n' || *p == '[')
            continue;

        char* eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        char* key = (char*)skip_ws(line);
        trim_end(key);
        char* value = (char*)skip_ws(eq + 1);
        trim_end(value);

        if (*key && *value) {
            apply_setting(key, value);
        }
    }
    fclose(f);
    sanitize_platform_settings();
    apply_frame_limit_setting();
    apply_borderless_acres_setting();

    printf("[Settings] Loaded %s: %dx%d fullscreen=%d aspect=%d vsync=%d max_fps=%d msaa=%d preload_textures=%d borderless_acres=%d\n",
           SETTINGS_FILE, g_pc_settings.window_width, g_pc_settings.window_height,
           g_pc_settings.fullscreen, g_pc_settings.aspect_ratio, g_pc_settings.vsync,
           g_pc_settings.max_fps, g_pc_settings.msaa,
           g_pc_settings.preload_textures, g_pc_settings.borderless_acres);
}

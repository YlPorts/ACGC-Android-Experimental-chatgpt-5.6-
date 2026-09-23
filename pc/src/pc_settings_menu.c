#include "pc_settings_menu.h"
#include "pc_settings.h"
#include "pc_android_bridge.h"
#include "pc_keybindings.h"
#include "pc_menu_util.h"
#include "pc_text_draw.h"
#include "pc_language.h"

#include "graph.h"
#include "m_font.h"
#include "m_rcp.h"
#include "main.h" /* SCREEN_WIDTH_F */

#include <stddef.h>
#include <stdio.h>
#include <string.h>

/* --- Item IDs (stable across tabs - used by the dispatch switches) --- */
enum {
    ITEM_DISPLAY,
    ITEM_VSYNC,
    ITEM_MAX_FPS,
    ITEM_MSAA,
    ITEM_ASPECT,
    ITEM_RES,
    ITEM_TEXTURES,
    ITEM_RESETTI,
    ITEM_SHOP_VISITOR,
    ITEM_BORDERLESS_ACRES,
    ITEM_NES_ASPECT,
    ITEM_LANGUAGE,
    ITEM_MASTER_VOLUME,
    ITEM_STICK_DEADZONE,
    ITEM_CSTICK_DEADZONE,
    ITEM_BINDINGS,
    ITEM_TEXTURE_PACK,
    ITEM_STICK_SENSITIVITY,
    ITEM_CSTICK_SENSITIVITY,
    ITEM_TOUCH_VISIBLE,
    ITEM_TOUCH_OPACITY,
    ITEM_TOUCH_SCALE,
    ITEM_TOUCH_HIDE_GAMEPAD,
    ITEM_TOUCH_EDIT,
    ITEM_TOUCH_RESET,
    ITEM_ANDROID_SYNC,
    ITEM_ANDROID_RELOAD,
    ITEM_ANDROID_CHANGE_FOLDER,
    ITEM_ANDROID_RESTART,
    ITEM_ANDROID_EXIT,
};

/* Per-item static metadata. restart=1 appends " *" and folds into the
 * "requires restart" hint at the bottom of the page. */
typedef struct {
    const char* label;
    int         id;
    int         restart;
} Item;

#ifdef TARGET_ANDROID
static const Item tab_video_items[] = {
    { "Formato",      ITEM_ASPECT,       0 },
    { "VSync",        ITEM_VSYNC,        0 },
    { "FPS maximos",  ITEM_MAX_FPS,      0 },
    { "Calidad",      ITEM_MSAA,         1 },
    { "Texturas",     ITEM_TEXTURE_PACK, 1 },
};

static const Item tab_audio_items[] = {
    { "Volumen", ITEM_MASTER_VOLUME, 0 },
};

static const Item tab_stick_items[] = {
    { "Sensibilidad",   ITEM_STICK_SENSITIVITY,  0 },
    { "Zona muerta",    ITEM_STICK_DEADZONE,     0 },
    { "Sensib. C-stick",ITEM_CSTICK_SENSITIVITY, 0 },
    { "Zona muerta C",  ITEM_CSTICK_DEADZONE,    0 },
};

static const Item tab_touch_items[] = {
    { "Controles tactiles", ITEM_TOUCH_VISIBLE,       0 },
    { "Opacidad",           ITEM_TOUCH_OPACITY,       0 },
    { "Tamano",             ITEM_TOUCH_SCALE,         0 },
    { "Ocultar con mando",  ITEM_TOUCH_HIDE_GAMEPAD,  0 },
    { "Editar posiciones",  ITEM_TOUCH_EDIT,          0 },
    { "Restablecer",        ITEM_TOUCH_RESET,         0 },
};

static const Item tab_gameplay_items[] = {
    { "Idioma",        ITEM_LANGUAGE,     1 },
    { "Resetti",       ITEM_RESETTI,      0 },
    { "Mejora tienda", ITEM_SHOP_VISITOR, 0 },
    { "Pantalla NES",  ITEM_NES_ASPECT,   0 },
};

static const Item tab_system_items[] = {
    { "Sincronizar datos", ITEM_ANDROID_SYNC,          0 },
    { "Recargar contenido",ITEM_ANDROID_RELOAD,        0 },
    { "Cambiar carpeta",   ITEM_ANDROID_CHANGE_FOLDER, 0 },
    { "Reiniciar juego",   ITEM_ANDROID_RESTART,       0 },
    { "Cerrar app",        ITEM_ANDROID_EXIT,          0 },
};
#else
static const Item tab_video_items[] = {
    { "Display",    ITEM_DISPLAY,  0 },
    { "VSync",      ITEM_VSYNC,    0 },
    { "Max FPS",    ITEM_MAX_FPS,  0 },
    { "MSAA",       ITEM_MSAA,     1 },
    { "Resolution", ITEM_RES,      0 },
    { "Textures",   ITEM_TEXTURES, 1 },
};

static const Item tab_gameplay_items[] = {
    { "Language",         ITEM_LANGUAGE,         1 },
    { "Resetti",          ITEM_RESETTI,          0 },
    { "Shop upgrade",     ITEM_SHOP_VISITOR,     0 },
    { "Borderless Acres", ITEM_BORDERLESS_ACRES, 0 },
    { "NES aspect",       ITEM_NES_ASPECT,       0 },
};

static const Item tab_audio_items[] = {
    { "Master volume", ITEM_MASTER_VOLUME, 0 },
};

static const Item tab_controls_items[] = {
    { "Stick sensitivity",   ITEM_STICK_SENSITIVITY,  0 },
    { "Stick deadzone",      ITEM_STICK_DEADZONE,     0 },
    { "C-stick sensitivity", ITEM_CSTICK_SENSITIVITY, 0 },
    { "C-stick deadzone",    ITEM_CSTICK_DEADZONE,    0 },
    { "Keybindings",         ITEM_BINDINGS,           0 },
};
#endif

typedef struct {
    const char* name;
    const Item* items;
    int         count;
} Tab;

#define TAB_ITEMS(a) (a), (int)(sizeof(a) / sizeof((a)[0]))
static const Tab s_tabs[] = {
#ifdef TARGET_ANDROID
    { "Video",   TAB_ITEMS(tab_video_items) },
    { "Audio",   TAB_ITEMS(tab_audio_items) },
    { "Mando",   TAB_ITEMS(tab_stick_items) },
    { "Tactil",  TAB_ITEMS(tab_touch_items) },
    { "Juego",   TAB_ITEMS(tab_gameplay_items) },
    { "Sistema", TAB_ITEMS(tab_system_items) },
#else
    { "Video",    TAB_ITEMS(tab_video_items) },
    { "Audio",    TAB_ITEMS(tab_audio_items) },
    { "Controls", TAB_ITEMS(tab_controls_items) },
    { "Gameplay", TAB_ITEMS(tab_gameplay_items) },
#endif
};
#define TAB_COUNT ((int)(sizeof(s_tabs) / sizeof(s_tabs[0])))

#ifdef TARGET_ANDROID
static const char* localized_item_label(int id, const char* fallback) {
    switch (id) {
        case ITEM_ASPECT: return pc_language_ui_lookup("settings.aspect", "Aspect");
        case ITEM_VSYNC: return "VSync";
        case ITEM_MAX_FPS: return pc_language_ui_lookup("settings.max_fps", "Max FPS");
        case ITEM_MSAA: return pc_language_ui_lookup("settings.quality", "Quality");
        case ITEM_TEXTURE_PACK: return pc_language_ui_lookup("settings.textures", "Textures");
        case ITEM_MASTER_VOLUME: return pc_language_ui_lookup("settings.volume", "Volume");
        case ITEM_STICK_SENSITIVITY: return pc_language_ui_lookup("settings.stick_sensitivity", "Sensitivity");
        case ITEM_STICK_DEADZONE: return pc_language_ui_lookup("settings.stick_deadzone", "Deadzone");
        case ITEM_CSTICK_SENSITIVITY: return pc_language_ui_lookup("settings.cstick_sensitivity", "C-stick sensitivity");
        case ITEM_CSTICK_DEADZONE: return pc_language_ui_lookup("settings.cstick_deadzone", "C-stick deadzone");
        case ITEM_TOUCH_VISIBLE: return pc_language_ui_lookup("settings.touch_controls", "Touch controls");
        case ITEM_TOUCH_OPACITY: return pc_language_ui_lookup("settings.opacity", "Opacity");
        case ITEM_TOUCH_SCALE: return pc_language_ui_lookup("settings.size", "Size");
        case ITEM_TOUCH_HIDE_GAMEPAD: return pc_language_ui_lookup("settings.hide_with_gamepad", "Hide with gamepad");
        case ITEM_TOUCH_EDIT: return pc_language_ui_lookup("settings.edit_positions", "Edit positions");
        case ITEM_TOUCH_RESET: return pc_language_ui_lookup("settings.reset", "Reset");
        case ITEM_LANGUAGE: return pc_language_ui_lookup("settings.language", "Language");
        case ITEM_RESETTI: return "Resetti";
        case ITEM_SHOP_VISITOR: return pc_language_ui_lookup("settings.shop_upgrade", "Shop upgrade");
        case ITEM_NES_ASPECT: return pc_language_ui_lookup("settings.nes_display", "NES display");
        case ITEM_ANDROID_SYNC: return pc_language_ui_lookup("settings.sync_data", "Sync data");
        case ITEM_ANDROID_RELOAD: return pc_language_ui_lookup("settings.reload_content", "Reload content");
        case ITEM_ANDROID_CHANGE_FOLDER: return pc_language_ui_lookup("settings.change_folder", "Change folder");
        case ITEM_ANDROID_RESTART: return pc_language_ui_lookup("settings.restart_game", "Restart game");
        case ITEM_ANDROID_EXIT: return pc_language_ui_lookup("settings.close_app", "Close app");
        default: return fallback;
    }
}

static const char* localized_tab_name(int tab, const char* fallback) {
    switch (tab) {
        case 0: return "Video";
        case 1: return "Audio";
        case 2: return pc_language_ui_lookup("settings.tab_controller", "Controller");
        case 3: return pc_language_ui_lookup("settings.tab_touch", "Touch");
        case 4: return pc_language_ui_lookup("settings.tab_game", "Game");
        case 5: return pc_language_ui_lookup("settings.tab_system", "System");
        default: return fallback;
    }
}
#endif

/* --- Sub-pages --- */
typedef enum {
    SUB_SETTINGS = 0,
    SUB_CONFIRM_RES = 1,
    SUB_CONFIRM_BACK = 2,
    SUB_BINDINGS = 3,
} SubPage;

static int     s_active = 0;
static SubPage s_sub    = SUB_SETTINGS;
static int     s_tab    = 0;
static int     s_sel    = -1; /* start on the tab row */

/* Pending edits - only committed to g_pc_settings on Apply. */
static PCSettings s_pending;
static int        s_pending_dirty = 0;

/* Resolution-change confirmation state. */
#define RES_CONFIRM_MS 15000u
static int    s_res_old_w = 0;
static int    s_res_old_h = 0;
static Uint32 s_res_deadline = 0;
static int    s_res_sel = 1;

/* Back-confirm state (Discard vs Keep editing when Back hit while dirty). */
static int    s_back_sel = 0; /* 0 = Keep editing (safe default), 1 = Discard */

/* --- Bindings editor (SUB_BINDINGS) state --- */
typedef struct {
    const char* label;
    int kb_off;            /* offset into PCKeybindings */
    int pad_off;           /* offset into PCPadBindings, -1 = not pad-bindable */
    const char* pad_fixed; /* pad column text when pad_off < 0 */
} BindRow;

#define BROW(lbl, kf, pf)      { lbl, offsetof(PCKeybindings, kf), offsetof(PCPadBindings, pf), NULL }
#define BROW_K(lbl, kf, fixed) { lbl, offsetof(PCKeybindings, kf), -1, fixed }

static const BindRow s_bind_rows[] = {
    BROW("A",     a,     a),
    BROW("B",     b,     b),
    BROW("X",     x,     x),
    BROW("Y",     y,     y),
    BROW("Start", start, start),
    BROW("Z",     z,     z),
    BROW("L",     l,     l),
    BROW("R",     r,     r),
    BROW_K("Stick Up",    stick_up,    "L-Stick"),
    BROW_K("Stick Down",  stick_down,  "L-Stick"),
    BROW_K("Stick Left",  stick_left,  "L-Stick"),
    BROW_K("Stick Right", stick_right, "L-Stick"),
    BROW_K("C-Stick Up",    cstick_up,    "R-Stick"),
    BROW_K("C-Stick Down",  cstick_down,  "R-Stick"),
    BROW_K("C-Stick Left",  cstick_left,  "R-Stick"),
    BROW_K("C-Stick Right", cstick_right, "R-Stick"),
    BROW("D-Pad Up",    dpad_up,    dpad_up),
    BROW("D-Pad Down",  dpad_down,  dpad_down),
    BROW("D-Pad Left",  dpad_left,  dpad_left),
    BROW("D-Pad Right", dpad_right, dpad_right),
};
#define BIND_ROW_COUNT    ((int)(sizeof(s_bind_rows) / sizeof(s_bind_rows[0])))
#define BIND_VISIBLE      9
#define BIND_IDX_DEFAULTS BIND_ROW_COUNT
#define BIND_IDX_BACK     (BIND_ROW_COUNT + 1)

static const char* localized_bind_label(const char* label) {
    if (label == NULL) return "";
    if (strcmp(label, "Stick Up") == 0) return pc_language_ui_lookup("bindings.stick_up", label);
    if (strcmp(label, "Stick Down") == 0) return pc_language_ui_lookup("bindings.stick_down", label);
    if (strcmp(label, "Stick Left") == 0) return pc_language_ui_lookup("bindings.stick_left", label);
    if (strcmp(label, "Stick Right") == 0) return pc_language_ui_lookup("bindings.stick_right", label);
    if (strcmp(label, "C-Stick Up") == 0) return pc_language_ui_lookup("bindings.cstick_up", label);
    if (strcmp(label, "C-Stick Down") == 0) return pc_language_ui_lookup("bindings.cstick_down", label);
    if (strcmp(label, "C-Stick Left") == 0) return pc_language_ui_lookup("bindings.cstick_left", label);
    if (strcmp(label, "C-Stick Right") == 0) return pc_language_ui_lookup("bindings.cstick_right", label);
    if (strcmp(label, "D-Pad Up") == 0) return pc_language_ui_lookup("bindings.dpad_up", label);
    if (strcmp(label, "D-Pad Down") == 0) return pc_language_ui_lookup("bindings.dpad_down", label);
    if (strcmp(label, "D-Pad Left") == 0) return pc_language_ui_lookup("bindings.dpad_left", label);
    if (strcmp(label, "D-Pad Right") == 0) return pc_language_ui_lookup("bindings.dpad_right", label);
    return label;
}

static int s_bind_sel = 0;
static int s_bind_col = 0;      /* 0 = keyboard, 1 = gamepad */
static int s_bind_scroll = 0;
static int s_capture = 0;       /* waiting for the next key/button press */
static int s_capture_grace = 0; /* frames pad-driven hosts stay blocked after capture */

static PCInputCode* bind_kb_slot(int row) {
    return (PCInputCode*)((char*)&g_pc_keybindings + s_bind_rows[row].kb_off);
}

static PCPadCode* bind_pad_slot(int row) {
    if (s_bind_rows[row].pad_off < 0) return NULL;
    return (PCPadCode*)((char*)&g_pc_padbindings + s_bind_rows[row].pad_off);
}

/* Rebinding swaps with whichever action already used the code, so no
 * action ends up silently double-bound. */
static void bind_assign_kb(int row, PCInputCode code) {
    PCInputCode* dst = bind_kb_slot(row);
    for (int i = 0; i < BIND_ROW_COUNT; i++) {
        if (i == row) continue;
        PCInputCode* other = bind_kb_slot(i);
        if (*other == code) { *other = *dst; break; }
    }
    *dst = code;
}

static void bind_assign_pad(int row, PCPadCode code) {
    PCPadCode* dst = bind_pad_slot(row);
    if (!dst) return;
    if (code >= 0) {
        for (int i = 0; i < BIND_ROW_COUNT; i++) {
            if (i == row) continue;
            PCPadCode* other = bind_pad_slot(i);
            if (other && *other == code) { *other = *dst; break; }
        }
    }
    *dst = code;
}

static void bind_enter_page(void) {
    s_sub = SUB_BINDINGS;
    s_bind_sel = 0;
    s_bind_col = 0;
    s_bind_scroll = 0;
    s_capture = 0;
}

static void bind_fix_scroll_and_col(void) {
    if (s_bind_sel < BIND_ROW_COUNT) {
        if (s_bind_sel < s_bind_scroll) s_bind_scroll = s_bind_sel;
        if (s_bind_sel >= s_bind_scroll + BIND_VISIBLE)
            s_bind_scroll = s_bind_sel - BIND_VISIBLE + 1;
        if (s_bind_col == 1 && s_bind_rows[s_bind_sel].pad_off < 0) s_bind_col = 0;
    }
}

static void capture_finish(int changed) {
    s_capture = 0;
    s_capture_grace = 15;
    if (changed) pc_keybindings_save();
}

/* The game font atlas only covers a subset of ASCII (see CHAR_* in
 * m_font.h): '*' is a tilde, '+' a heart, '/' a music note, ';' a droplet,
 * '\\' an annoyed face, '[' ']' '^' accented letters, etc. Replace
 * unsupported bytes so cells stay readable. */
static int glyph_ok(unsigned char c) {
    if (c >= '0' && c <= '9') return 1;
    if (c >= '@' && c <= 'Z') return 1;
    if (c >= 'a' && c <= 'z') return 1;
    switch (c) {
        case ' ': case '!': case '"': case '%': case '&': case '\'':
        case '(': case ')': case ',': case '-': case '.': case ':':
        case '<': case '=': case '>': case '?': case '_':
            return 1;
    }
    return 0;
}

static void sanitize_glyphs(char* s) {
    for (; *s; s++) {
        if (!glyph_ok((unsigned char)*s)) *s = '?';
    }
}

/* Startup snapshot + restart-required flag. Some settings (MSAA, texture
 * pack preload mode) only take effect on process restart. More might appear.*/
static PCSettings s_startup;
static int        s_startup_captured = 0;
static int        s_pending_restart = 0;

/* --- Dirty + helpers --- */

static void recompute_dirty(void) {
    s_pending_dirty =
        (s_pending.fullscreen       != g_pc_settings.fullscreen) ||
        (s_pending.vsync            != g_pc_settings.vsync) ||
        (s_pending.max_fps          != g_pc_settings.max_fps) ||
        (s_pending.msaa             != g_pc_settings.msaa) ||
        (s_pending.aspect_ratio     != g_pc_settings.aspect_ratio) ||
        (s_pending.window_width     != g_pc_settings.window_width) ||
        (s_pending.window_height    != g_pc_settings.window_height) ||
        (s_pending.preload_textures != g_pc_settings.preload_textures) ||
        (s_pending.texture_pack_enabled != g_pc_settings.texture_pack_enabled) ||
        (s_pending.disable_resetti  != g_pc_settings.disable_resetti) ||
        (s_pending.disable_shop_visitor_req != g_pc_settings.disable_shop_visitor_req) ||
        (s_pending.borderless_acres != g_pc_settings.borderless_acres) ||
        (s_pending.nes_aspect       != g_pc_settings.nes_aspect) ||
        (strcmp(s_pending.language, g_pc_settings.language) != 0) ||
        (s_pending.master_volume    != g_pc_settings.master_volume) ||
        (s_pending.stick_deadzone   != g_pc_settings.stick_deadzone) ||
        (s_pending.stick_sensitivity != g_pc_settings.stick_sensitivity) ||
        (s_pending.cstick_deadzone  != g_pc_settings.cstick_deadzone) ||
        (s_pending.cstick_sensitivity != g_pc_settings.cstick_sensitivity) ||
        (s_pending.touch_controls_visible != g_pc_settings.touch_controls_visible) ||
        (s_pending.touch_opacity != g_pc_settings.touch_opacity) ||
        (s_pending.touch_scale != g_pc_settings.touch_scale) ||
        (s_pending.touch_hide_with_gamepad != g_pc_settings.touch_hide_with_gamepad);
}

static void snapshot(void) {
    s_pending = g_pc_settings;
    s_pending_dirty = 0;
}

/* Per-item dispatch: cycle, format, changed. Add cases here when a
 * new setting row is added to any tab. */

static void item_cycle(int id, int dir) {
    switch (id) {
        case ITEM_DISPLAY: {
            int v = s_pending.fullscreen + (dir > 0 ? 1 : 2);
            s_pending.fullscreen = v % 3;
        } break;
        case ITEM_VSYNC:
            s_pending.vsync = !s_pending.vsync;
            break;
        case ITEM_MAX_FPS: {
#ifdef TARGET_ANDROID
            static const int steps[] = { 60, 90, 120 };
#else
            static const int steps[] = { 0, 60, 120, 180, 240, 300, 360 };
#endif
            int idx = 0;
            int count = sizeof(steps) / sizeof(steps[0]);
            for (int i = 0; i < count; i++) if (s_pending.max_fps == steps[i]) { idx = i; break; }
            idx = (idx + (dir > 0 ? 1 : count - 1)) % count;
            s_pending.max_fps = steps[idx];
        } break;
        case ITEM_MSAA: {
#ifdef TARGET_ANDROID
            static const int steps[] = { 0, 2 };
#else
            static const int steps[] = { 0, 2, 4, 8 };
#endif
            int idx = 0;
            int count = sizeof(steps) / sizeof(steps[0]);
            for (int i = 0; i < count; i++) if (s_pending.msaa == steps[i]) { idx = i; break; }
            idx = (idx + (dir > 0 ? 1 : count - 1)) % count;
            s_pending.msaa = steps[idx];
        } break;
        case ITEM_ASPECT: {
            int v = s_pending.aspect_ratio + (dir > 0 ? 1 : 2);
            s_pending.aspect_ratio = v % 3;
        } break;
        case ITEM_RES:
            pc_settings_cycle_resolution(&s_pending.window_width, &s_pending.window_height, dir);
            break;
        case ITEM_TEXTURES: {
            int v = s_pending.preload_textures + (dir > 0 ? 1 : -1);
            if (v < 0) v = 2;
            if (v > 2) v = 0;
            s_pending.preload_textures = v;
        } break;
        case ITEM_RESETTI:
            s_pending.disable_resetti = !s_pending.disable_resetti;
            break;
        case ITEM_SHOP_VISITOR:
            s_pending.disable_shop_visitor_req = !s_pending.disable_shop_visitor_req;
            break;
        case ITEM_BORDERLESS_ACRES:
            s_pending.borderless_acres = !s_pending.borderless_acres;
            break;
        case ITEM_NES_ASPECT:
            s_pending.nes_aspect = !s_pending.nes_aspect;
            break;
        case ITEM_LANGUAGE: {
            static const char* codes[] = { "en", "es", "fr", "de", "it" };
            int current = 0;
            int count = (int)(sizeof(codes) / sizeof(codes[0]));
            for (int i = 0; i < count; i++) {
                if (strcmp(s_pending.language, codes[i]) == 0) { current = i; break; }
            }
            current = (current + (dir > 0 ? 1 : count - 1)) % count;
            strcpy(s_pending.language, codes[current]);
        } break;
        case ITEM_MASTER_VOLUME: {
            int v = s_pending.master_volume + (dir > 0 ? 10 : -10);
            if (v < 0)   v = 0;
            if (v > 100) v = 100;
            s_pending.master_volume = v;
        } break;
        case ITEM_STICK_DEADZONE: {
            int v = s_pending.stick_deadzone + (dir > 0 ? 2 : -2);
            if (v < 0)  v = 0;
            if (v > 40) v = 40;
            s_pending.stick_deadzone = v;
        } break;
        case ITEM_CSTICK_DEADZONE: {
            int v = s_pending.cstick_deadzone + (dir > 0 ? 2 : -2);
            if (v < 0)  v = 0;
            if (v > 40) v = 40;
            s_pending.cstick_deadzone = v;
        } break;
        case ITEM_STICK_SENSITIVITY: {
            int v = s_pending.stick_sensitivity + (dir > 0 ? 10 : -10);
            if (v < 50) v = 50;
            if (v > 150) v = 150;
            s_pending.stick_sensitivity = v;
        } break;
        case ITEM_CSTICK_SENSITIVITY: {
            int v = s_pending.cstick_sensitivity + (dir > 0 ? 10 : -10);
            if (v < 50) v = 50;
            if (v > 150) v = 150;
            s_pending.cstick_sensitivity = v;
        } break;
        case ITEM_TEXTURE_PACK:
            s_pending.texture_pack_enabled = !s_pending.texture_pack_enabled;
            break;
        case ITEM_TOUCH_VISIBLE:
            s_pending.touch_controls_visible = !s_pending.touch_controls_visible;
            break;
        case ITEM_TOUCH_OPACITY: {
            static const int steps[] = { 25, 50, 75, 100 };
            int idx = 0;
            int count = (int)(sizeof(steps) / sizeof(steps[0]));
            for (int i = 0; i < count; i++) if (s_pending.touch_opacity == steps[i]) { idx = i; break; }
            idx = (idx + (dir > 0 ? 1 : count - 1)) % count;
            s_pending.touch_opacity = steps[idx];
        } break;
        case ITEM_TOUCH_SCALE: {
            static const int steps[] = { 75, 100, 125, 150 };
            int idx = 0;
            int count = (int)(sizeof(steps) / sizeof(steps[0]));
            for (int i = 0; i < count; i++) if (s_pending.touch_scale == steps[i]) { idx = i; break; }
            idx = (idx + (dir > 0 ? 1 : count - 1)) % count;
            s_pending.touch_scale = steps[idx];
        } break;
        case ITEM_TOUCH_HIDE_GAMEPAD:
            s_pending.touch_hide_with_gamepad = !s_pending.touch_hide_with_gamepad;
            break;
        case ITEM_TOUCH_EDIT:
        case ITEM_TOUCH_RESET:
        case ITEM_ANDROID_SYNC:
        case ITEM_ANDROID_RELOAD:
        case ITEM_ANDROID_CHANGE_FOLDER:
        case ITEM_ANDROID_RESTART:
        case ITEM_ANDROID_EXIT:
            break;
        case ITEM_BINDINGS:
            /* opened via confirm, left/right does nothing */
            break;
    }
    recompute_dirty();
}

static void item_format(int id, char* buf, size_t n) {
    switch (id) {
        case ITEM_DISPLAY:
            snprintf(buf, n, "%s",
                s_pending.fullscreen == 0 ? pc_language_ui_lookup("settings.value.windowed", "< Windowed >") :
                s_pending.fullscreen == 1 ? pc_language_ui_lookup("settings.value.fullscreen", "< Fullscreen >") :
                                            pc_language_ui_lookup("settings.value.borderless", "< Borderless >"));
            break;
        case ITEM_VSYNC:
            snprintf(buf, n, "%s", s_pending.vsync ? pc_language_ui_lookup("settings.value.yes", "< Yes >") : pc_language_ui_lookup("settings.value.no", "< No >"));
            break;
        case ITEM_MAX_FPS:
            if (s_pending.max_fps > 0) snprintf(buf, n, "< %d >", s_pending.max_fps);
            else                       snprintf(buf, n, "%s", pc_language_ui_lookup("settings.value.uncapped", "< Uncapped >"));
            break;
        case ITEM_MSAA:
#ifdef TARGET_ANDROID
            snprintf(buf, n, "%s",
                s_pending.msaa == 0 ? pc_language_ui_lookup("settings.value.normal", "< Normal >") : pc_language_ui_lookup("settings.value.high2x", "< High 2x >"));
#else
            if (s_pending.msaa > 0) snprintf(buf, n, "< %dx >", s_pending.msaa);
            else                    snprintf(buf, n, "%s", pc_language_ui_lookup("settings.value.off", "< Off >"));
#endif
            break;
        case ITEM_ASPECT:
            snprintf(buf, n, "%s",
                s_pending.aspect_ratio == 0 ? "< 4:3 >" :
                s_pending.aspect_ratio == 1 ? "< 16:9 >" :
                                              "< 21:9 >");
            break;
        case ITEM_RES:
            snprintf(buf, n, "< %dx%d >", s_pending.window_width, s_pending.window_height);
            break;
        case ITEM_TEXTURES:
            snprintf(buf, n, "%s",
                s_pending.preload_textures == 0 ? pc_language_ui_lookup("settings.value.on_demand", "< On Demand >") :
                s_pending.preload_textures == 1 ? pc_language_ui_lookup("settings.value.preload", "< Preload >")   :
                                                  pc_language_ui_lookup("settings.value.preload_cache", "< Preload&Cache >"));
            break;
        case ITEM_RESETTI:
            /* disable_resetti flips the polarity - show the user-facing side. */
            snprintf(buf, n, "%s", s_pending.disable_resetti ? pc_language_ui_lookup("settings.value.no", "< No >") : pc_language_ui_lookup("settings.value.yes", "< Yes >"));
            break;
        case ITEM_SHOP_VISITOR:
            /* disable_shop_visitor_req flips the polarity - show the user-facing side. */
            snprintf(buf, n, "%s", s_pending.disable_shop_visitor_req ? pc_language_ui_lookup("settings.value.no_visit", "< No visit >") : pc_language_ui_lookup("settings.value.original", "< Original >"));
            break;
        case ITEM_BORDERLESS_ACRES:
            snprintf(buf, n, "%s", s_pending.borderless_acres ? pc_language_ui_lookup("settings.value.on", "< On >") : pc_language_ui_lookup("settings.value.off", "< Off >"));
            break;
        case ITEM_NES_ASPECT:
            snprintf(buf, n, "%s", s_pending.nes_aspect ? "< 4:3 >" : pc_language_ui_lookup("settings.value.stretched", "< Stretched >"));
            break;
        case ITEM_LANGUAGE:
            if (strcmp(s_pending.language, "en") == 0) snprintf(buf, n, "< English >");
            else if (strcmp(s_pending.language, "es") == 0) snprintf(buf, n, "< Espanol >");
            else if (strcmp(s_pending.language, "fr") == 0) snprintf(buf, n, "< Francais >");
            else if (strcmp(s_pending.language, "de") == 0) snprintf(buf, n, "< Deutsch >");
            else if (strcmp(s_pending.language, "it") == 0) snprintf(buf, n, "< Italiano >");
            else snprintf(buf, n, "< %s >", s_pending.language);
            break;
        case ITEM_MASTER_VOLUME:
            snprintf(buf, n, "< %d%% >", s_pending.master_volume);
            break;
        case ITEM_STICK_DEADZONE:
            snprintf(buf, n, "< %d%% >", s_pending.stick_deadzone);
            break;
        case ITEM_CSTICK_DEADZONE:
            snprintf(buf, n, "< %d%% >", s_pending.cstick_deadzone);
            break;
        case ITEM_STICK_SENSITIVITY:
            snprintf(buf, n, "< %d%% >", s_pending.stick_sensitivity);
            break;
        case ITEM_CSTICK_SENSITIVITY:
            snprintf(buf, n, "< %d%% >", s_pending.cstick_sensitivity);
            break;
        case ITEM_TEXTURE_PACK:
            snprintf(buf, n, "%s", s_pending.texture_pack_enabled ? pc_language_ui_lookup("settings.value.yes", "< Yes >") : pc_language_ui_lookup("settings.value.no", "< No >"));
            break;
        case ITEM_TOUCH_VISIBLE:
            snprintf(buf, n, "%s", s_pending.touch_controls_visible ? pc_language_ui_lookup("settings.value.show", "< Show >") : pc_language_ui_lookup("settings.value.hide", "< Hide >"));
            break;
        case ITEM_TOUCH_OPACITY:
            snprintf(buf, n, "< %d%% >", s_pending.touch_opacity);
            break;
        case ITEM_TOUCH_SCALE:
            snprintf(buf, n, "< %d%% >", s_pending.touch_scale);
            break;
        case ITEM_TOUCH_HIDE_GAMEPAD:
            snprintf(buf, n, "%s", s_pending.touch_hide_with_gamepad ? pc_language_ui_lookup("settings.value.yes", "< Yes >") : pc_language_ui_lookup("settings.value.no", "< No >"));
            break;
        case ITEM_TOUCH_EDIT:
        case ITEM_TOUCH_RESET:
        case ITEM_ANDROID_SYNC:
        case ITEM_ANDROID_RELOAD:
        case ITEM_ANDROID_CHANGE_FOLDER:
        case ITEM_ANDROID_RESTART:
        case ITEM_ANDROID_EXIT:
            snprintf(buf, n, "Ejecutar");
            break;
        case ITEM_BINDINGS:
            snprintf(buf, n, "Edit...");
            break;
        default:
            buf[0] = '\0';
            break;
    }
}

static int item_changed(int id) {
    switch (id) {
        case ITEM_DISPLAY:    return s_pending.fullscreen       != g_pc_settings.fullscreen;
        case ITEM_VSYNC:      return s_pending.vsync            != g_pc_settings.vsync;
        case ITEM_MAX_FPS:    return s_pending.max_fps          != g_pc_settings.max_fps;
        case ITEM_MSAA:       return s_pending.msaa             != g_pc_settings.msaa;
        case ITEM_ASPECT:     return s_pending.aspect_ratio     != g_pc_settings.aspect_ratio;
        case ITEM_RES:        return (s_pending.window_width  != g_pc_settings.window_width) ||
                                     (s_pending.window_height != g_pc_settings.window_height);
        case ITEM_TEXTURES:   return s_pending.preload_textures != g_pc_settings.preload_textures;
        case ITEM_RESETTI:    return s_pending.disable_resetti  != g_pc_settings.disable_resetti;
        case ITEM_SHOP_VISITOR: return s_pending.disable_shop_visitor_req != g_pc_settings.disable_shop_visitor_req;
        case ITEM_BORDERLESS_ACRES: return s_pending.borderless_acres != g_pc_settings.borderless_acres;
        case ITEM_NES_ASPECT:    return s_pending.nes_aspect    != g_pc_settings.nes_aspect;
        case ITEM_LANGUAGE:      return strcmp(s_pending.language, g_pc_settings.language) != 0;
        case ITEM_MASTER_VOLUME: return s_pending.master_volume != g_pc_settings.master_volume;
        case ITEM_STICK_DEADZONE:  return s_pending.stick_deadzone  != g_pc_settings.stick_deadzone;
        case ITEM_STICK_SENSITIVITY: return s_pending.stick_sensitivity != g_pc_settings.stick_sensitivity;
        case ITEM_CSTICK_DEADZONE: return s_pending.cstick_deadzone != g_pc_settings.cstick_deadzone;
        case ITEM_CSTICK_SENSITIVITY: return s_pending.cstick_sensitivity != g_pc_settings.cstick_sensitivity;
        case ITEM_TEXTURE_PACK: return s_pending.texture_pack_enabled != g_pc_settings.texture_pack_enabled;
        case ITEM_TOUCH_VISIBLE: return s_pending.touch_controls_visible != g_pc_settings.touch_controls_visible;
        case ITEM_TOUCH_OPACITY: return s_pending.touch_opacity != g_pc_settings.touch_opacity;
        case ITEM_TOUCH_SCALE: return s_pending.touch_scale != g_pc_settings.touch_scale;
        case ITEM_TOUCH_HIDE_GAMEPAD: return s_pending.touch_hide_with_gamepad != g_pc_settings.touch_hide_with_gamepad;
        case ITEM_TOUCH_EDIT:
        case ITEM_TOUCH_RESET:
        case ITEM_ANDROID_SYNC:
        case ITEM_ANDROID_RELOAD:
        case ITEM_ANDROID_CHANGE_FOLDER:
        case ITEM_ANDROID_RESTART:
        case ITEM_ANDROID_EXIT:
        case ITEM_BINDINGS:      return 0; /* actions/bindings save themselves */
    }
    return 0;
}

/* For items flagged restart=1. Does the live g_pc_settings value differ
 * from what the process booted with? Used after Apply to decide whether
 * the "restart required" banner should show. */
static int item_differs_from_startup(int id) {
    switch (id) {
        case ITEM_MSAA:         return g_pc_settings.msaa != s_startup.msaa;
        case ITEM_TEXTURES:     return g_pc_settings.preload_textures != s_startup.preload_textures;
        case ITEM_TEXTURE_PACK: return g_pc_settings.texture_pack_enabled != s_startup.texture_pack_enabled;
        case ITEM_LANGUAGE:     return strcmp(g_pc_settings.language, s_startup.language) != 0;
    }
    return 0;
}

static void recompute_restart_needed(void) {
    s_pending_restart = 0;
    for (int t = 0; t < TAB_COUNT; t++) {
        const Tab* tab = &s_tabs[t];
        for (int i = 0; i < tab->count; i++) {
            if (tab->items[i].restart && item_differs_from_startup(tab->items[i].id)) {
                s_pending_restart = 1;
                return;
            }
        }
    }
}

/* --- Nav helpers --- */

static int cur_item_count(void) { return s_tabs[s_tab].count; }
static int idx_apply(void)      { return cur_item_count(); }
static int idx_back(void)       { return cur_item_count() + 1; }

/* --- Apply / res-confirm finalisers --- */

static void res_confirm_keep(void) {
    pc_settings_save();
    printf("[SETTINGS] resolution kept: %dx%d\n",
           g_pc_settings.window_width, g_pc_settings.window_height);
}

static void res_confirm_revert(void) {
    g_pc_settings.window_width  = s_res_old_w;
    g_pc_settings.window_height = s_res_old_h;
    s_pending.window_width      = s_res_old_w;
    s_pending.window_height     = s_res_old_h;
    pc_settings_apply();
    pc_settings_save();
    recompute_dirty();
    printf("[SETTINGS] resolution reverted to %dx%d\n", s_res_old_w, s_res_old_h);
}

static void res_confirm_finish(void) {
    if (s_res_sel == 0) res_confirm_keep();
    else                res_confirm_revert();
    s_res_deadline = 0;
    s_sub = SUB_SETTINGS;
}

static void apply_pending(void) {
    if (!s_pending_dirty) return;

    int res_changed = (s_pending.window_width  != g_pc_settings.window_width) ||
                      (s_pending.window_height != g_pc_settings.window_height);
    if (res_changed) {
        s_res_old_w = g_pc_settings.window_width;
        s_res_old_h = g_pc_settings.window_height;
    }

    int touch_changed =
        (s_pending.touch_controls_visible != g_pc_settings.touch_controls_visible) ||
        (s_pending.touch_opacity != g_pc_settings.touch_opacity) ||
        (s_pending.touch_scale != g_pc_settings.touch_scale) ||
        (s_pending.touch_hide_with_gamepad != g_pc_settings.touch_hide_with_gamepad) ||
        (s_pending.stick_deadzone != g_pc_settings.stick_deadzone) ||
        (s_pending.stick_sensitivity != g_pc_settings.stick_sensitivity) ||
        (s_pending.cstick_deadzone != g_pc_settings.cstick_deadzone) ||
        (s_pending.cstick_sensitivity != g_pc_settings.cstick_sensitivity);

    g_pc_settings = s_pending;
    pc_settings_apply();
    if (touch_changed) pc_android_request_action(PC_ANDROID_ACTION_APPLY_TOUCH);
    s_pending_dirty = 0;
    recompute_restart_needed();

    if (res_changed) {
        s_sub = SUB_CONFIRM_RES;
        s_res_sel = 1;
        s_res_deadline = SDL_GetTicks() + RES_CONFIRM_MS;
        printf("[SETTINGS] resolution changed, awaiting confirmation (15s)\n");
    } else {
        pc_settings_save();
        printf("[SETTINGS] applied\n");
    }
}

#ifdef TARGET_ANDROID
static int item_is_android_action(int id) {
    return id == ITEM_TOUCH_EDIT || id == ITEM_TOUCH_RESET ||
           id == ITEM_ANDROID_SYNC || id == ITEM_ANDROID_RELOAD ||
           id == ITEM_ANDROID_CHANGE_FOLDER || id == ITEM_ANDROID_RESTART ||
           id == ITEM_ANDROID_EXIT;
}

static void activate_android_action(int id) {
    /* Leaving or editing from this page should never discard pending values. */
    if (s_pending_dirty) apply_pending();

    switch (id) {
        case ITEM_TOUCH_EDIT:
            pc_android_request_action(PC_ANDROID_ACTION_EDIT_CONTROLS);
            break;
        case ITEM_TOUCH_RESET:
            pc_android_request_action(PC_ANDROID_ACTION_RESET_CONTROLS);
            break;
        case ITEM_ANDROID_SYNC:
            pc_android_request_action(PC_ANDROID_ACTION_SYNC_DATA);
            break;
        case ITEM_ANDROID_RELOAD:
            pc_android_request_action(PC_ANDROID_ACTION_RELOAD_CONTENT);
            break;
        case ITEM_ANDROID_CHANGE_FOLDER:
            pc_android_request_action(PC_ANDROID_ACTION_CHANGE_FOLDER);
            break;
        case ITEM_ANDROID_RESTART:
            pc_android_request_action(PC_ANDROID_ACTION_RESTART_GAME);
            break;
        case ITEM_ANDROID_EXIT:
            pc_android_request_action(PC_ANDROID_ACTION_CLOSE_APP);
            break;
    }
}
#else
static int item_is_android_action(int id) { (void)id; return 0; }
static void activate_android_action(int id) { (void)id; }
#endif

/* =========================================================================
 * Public API
 * ========================================================================= */

void pc_settings_menu_enter(void) {
    /* First-time entry captures what the process actually booted with.
     * Anything that changes from here and can't live-apply gets flagged
     * as "restart required". */
    if (!s_startup_captured) {
        s_startup = g_pc_settings;
        s_startup_captured = 1;
    }
    snapshot();
    s_tab = 0;
    s_sel = -1;           /* start cursor on the tab row */
    s_sub = SUB_SETTINGS;
    s_res_deadline = 0;
    s_capture = 0;
    s_capture_grace = 0;
    s_active = 1;
}

int pc_settings_menu_active(void) {
    return s_active;
}

/* --- Navigation --- */

int pc_settings_menu_nav_up(void) {
    if (!s_active) return 0;
    if (s_sub == SUB_BINDINGS) {
        if (s_capture) return 1;
        if (s_bind_sel > 0) s_bind_sel--;
        bind_fix_scroll_and_col();
        return 1;
    }
    if (s_sub != SUB_SETTINGS) return 1;
    /* Clamp at the top - no wrap. */
    if (s_sel == 0)      s_sel = -1;            /* first item -> tab row */
    else if (s_sel > 0)  s_sel--;
    return 1;
}

int pc_settings_menu_nav_down(void) {
    if (!s_active) return 0;
    if (s_sub == SUB_BINDINGS) {
        if (s_capture) return 1;
        if (s_bind_sel < BIND_IDX_BACK) s_bind_sel++;
        bind_fix_scroll_and_col();
        return 1;
    }
    if (s_sub != SUB_SETTINGS) return 1;
    /* Clamp at the bottom - no wrap. */
    if (s_sel == -1)             s_sel = 0;     /* tab row -> first item */
    else if (s_sel < idx_back()) s_sel++;
    return 1;
}

/* dir: -1 = left, +1 = right. Drives confirm-page choice toggles, tab
 * switching on the tab row, and value cycling on a selected item. */
static int nav_horizontal(int dir) {
    if (!s_active) return 0;
    if (s_sub == SUB_BINDINGS) {
        if (s_capture) return 1;
        if (s_bind_sel < BIND_ROW_COUNT) {
            if (dir > 0 && s_bind_rows[s_bind_sel].pad_off >= 0) s_bind_col = 1;
            else if (dir < 0)                                    s_bind_col = 0;
        }
        return 1;
    }
    if (s_sub == SUB_CONFIRM_RES) {
        s_res_sel = (s_res_sel + 1) % 2;
        return 1;
    }
    if (s_sub == SUB_CONFIRM_BACK) {
        s_back_sel = (s_back_sel + 1) % 2;
        return 1;
    }
    if (s_sel == -1) {
        /* Tab row: clamp at the ends, no wrap. */
        if (dir < 0) { if (s_tab > 0) s_tab--; }
        else         { if (s_tab < TAB_COUNT - 1) s_tab++; }
    } else if (s_sel < cur_item_count()) {
        int id = s_tabs[s_tab].items[s_sel].id;
        if (!item_is_android_action(id)) item_cycle(id, dir);
    }
    return 1;
}

int pc_settings_menu_nav_left(void)  { return nav_horizontal(-1); }
int pc_settings_menu_nav_right(void) { return nav_horizontal(+1); }

int pc_settings_menu_confirm(void) {
    if (!s_active) return 0;
    if (s_sub == SUB_BINDINGS) {
        if (s_capture) return 1;
        if (s_bind_sel < BIND_ROW_COUNT) {
            if (s_bind_col == 0 || bind_pad_slot(s_bind_sel)) s_capture = 1;
        } else if (s_bind_sel == BIND_IDX_DEFAULTS) {
            pc_keybindings_reset_defaults();
            pc_keybindings_save();
        } else { /* Back */
            s_sub = SUB_SETTINGS;
        }
        return 1;
    }
    if (s_sub == SUB_CONFIRM_RES) {
        res_confirm_finish();
        return 1;
    }
    if (s_sub == SUB_CONFIRM_BACK) {
        if (s_back_sel == 1) {
            /* Discard - close the menu; pending edits drop on the floor. */
            s_active = 0;
            return 0;
        }
        s_sub = SUB_SETTINGS;
        return 1;
    }
    if (s_sel == -1) {
        /* Confirm on the tab row - just hop into the first item. */
        s_sel = 0;
    } else if (s_sel < cur_item_count()) {
        int id = s_tabs[s_tab].items[s_sel].id;
        if (id == ITEM_BINDINGS) bind_enter_page();
        else if (item_is_android_action(id)) activate_android_action(id);
        else item_cycle(id, +1);
    } else if (s_sel == idx_apply()) {
        apply_pending();
    } else if (s_sel == idx_back()) {
        if (s_pending_dirty) {
            s_sub = SUB_CONFIRM_BACK;
            s_back_sel = 0;
            return 1;
        }
        s_active = 0;
        return 0;
    }
    return 1;
}

int pc_settings_menu_cancel(void) {
    if (!s_active) return 0;
    if (s_sub == SUB_BINDINGS) {
        if (s_capture) { capture_finish(0); return 1; }
        s_sub = SUB_SETTINGS;
        return 1;
    }
    if (s_sub == SUB_CONFIRM_RES) {
        s_res_sel = 1; /* safer default on Esc/B */
        res_confirm_finish();
        return 1;
    }
    if (s_sub == SUB_CONFIRM_BACK) {
        /* Esc = cancel the confirm - snap back to the settings page. */
        s_sub = SUB_SETTINGS;
        return 1;
    }
    if (s_pending_dirty) {
        s_sub = SUB_CONFIRM_BACK;
        s_back_sel = 0;
        return 1;
    }
    s_active = 0;
    return 0;
}

void pc_settings_menu_tick(void) {
    if (!s_active) return;
    if (s_capture_grace > 0) s_capture_grace--;
    if (s_sub == SUB_CONFIRM_RES && s_res_deadline != 0) {
        if (SDL_GetTicks() >= s_res_deadline) {
            s_res_sel = 1;
            res_confirm_finish();
        }
    }
}

/* --- Keybinding capture --- */

int pc_settings_menu_capture_active(void) {
    return s_active && s_capture;
}

int pc_settings_menu_capture_blocking(void) {
    return s_active && (s_capture || s_capture_grace > 0);
}

int pc_settings_menu_handle_capture_event(const SDL_Event* e) {
    if (!pc_settings_menu_capture_active()) return 0;
    int row = s_bind_sel;
    if (row >= BIND_ROW_COUNT) { s_capture = 0; return 1; }

    switch (e->type) {
        case SDL_KEYDOWN: {
            if (e->key.repeat) return 1;
            SDL_Scancode sc = e->key.keysym.scancode;
            if (sc == SDL_SCANCODE_ESCAPE) { capture_finish(0); return 1; }
            if (s_bind_col == 1) {
                /* Delete/Backspace clears a pad binding; other keys ignored. */
                if (sc == SDL_SCANCODE_DELETE || sc == SDL_SCANCODE_BACKSPACE) {
                    bind_assign_pad(row, PC_PAD_NONE);
                    capture_finish(1);
                }
                return 1;
            }
            bind_assign_kb(row, (PCInputCode)sc);
            capture_finish(1);
            return 1;
        }
        case SDL_MOUSEBUTTONDOWN: {
            int b = e->button.button;
            if (s_bind_col == 0 &&
                (b == SDL_BUTTON_LEFT || b == SDL_BUTTON_RIGHT || b == SDL_BUTTON_MIDDLE)) {
                bind_assign_kb(row, PC_INPUT_MOUSE_BIT | b);
                capture_finish(1);
            }
            return 1;
        }
        case SDL_CONTROLLERBUTTONDOWN: {
            int b = e->cbutton.button;
            /* Back/Select is reserved for the pause menu - treat as cancel. */
            if (b == SDL_CONTROLLER_BUTTON_BACK) { capture_finish(0); return 1; }
            if (s_bind_col == 1) {
                bind_assign_pad(row, (PCPadCode)b);
                capture_finish(1);
            }
            return 1;
        }
        case SDL_CONTROLLERAXISMOTION: {
            int ax = e->caxis.axis;
            if (s_bind_col == 1 && e->caxis.value > 16000 &&
                (ax == SDL_CONTROLLER_AXIS_TRIGGERLEFT ||
                 ax == SDL_CONTROLLER_AXIS_TRIGGERRIGHT)) {
                bind_assign_pad(row, PC_PAD_AXIS_BIT | ax);
                capture_finish(1);
            }
            return 1;
        }
    }
    return 0;
}

/* =========================================================================
 * Drawing
 * ========================================================================= */

/* Changed-but-unapplied values render amber; otherwise the normal row color. */
static void value_colors(int selected, int changed, int* r, int* g, int* b, int* a) {
    if (changed) {
        *r = 255; *g = 215; *b = 90;
        *a = selected ? 255 : 200;
    } else {
        pc_menu_row_colors(selected, r, g, b, a);
    }
}

/* Tab row rendering: tabs are evenly spaced around the screen centre.
 * Active tab is highlighted; when the nav cursor is on the tab row, the
 * active tab also gets the bracketed "> name <" decoration. */
static void draw_tab_row(struct game_s* game, f32 y) {
    int on_tab_row = (s_sel == -1);

    /* Pre-measure widths and total horizontal extent so we can centre. */
    int widths[TAB_COUNT];
    int total = 0;
#ifdef TARGET_ANDROID
    const int gap_px = 7;
#else
    const int gap_px = 18;
#endif
    for (int t = 0; t < TAB_COUNT; t++) {
        #ifdef TARGET_ANDROID
        widths[t] = pc_text_width(localized_tab_name(t, s_tabs[t].name));
#else
        widths[t] = pc_text_width(s_tabs[t].name);
#endif
        total += widths[t];
    }
    total += gap_px * (TAB_COUNT - 1);

    f32 x = (SCREEN_WIDTH_F - (f32)total) * 0.5f;
    for (int t = 0; t < TAB_COUNT; t++) {
        #ifdef TARGET_ANDROID
        const char* name = localized_tab_name(t, s_tabs[t].name);
#else
        const char* name = s_tabs[t].name;
#endif
        int active = (t == s_tab);
        int r, g, b, a;
        if (active && on_tab_row) { r = 255; g = 235; b = 120; a = 255; }
        else if (active)          { r = 255; g = 255; b = 255; a = 230; }
        else                      { r = 160; g = 160; b = 160; a = 180; }

        /* Active tab + cursor-on-tab-row gets the full bump; active but
         * cursor-off-tab-row stays normal size (the white color already
         * marks it). Inactive tabs render normal too. */
        f32 s = (active && on_tab_row) ? PC_MENU_SCALE_SELECTED : 1.0f;
        pc_text_draw(game, name, x, y, r, g, b, a, s);
        x += (f32)widths[t] + (f32)gap_px;
    }
}

static void draw_settings_page(struct game_s* game) {
    int r, g, b, a;
    f32 lx = 70.0f;
    f32 vx = 200.0f;
    f32 y_tab = 50.0f;
    f32 y0    = 78.0f;
    f32 line_h = 16.0f;

    pc_menu_draw_centered(game, pc_language_ui_lookup("settings.title", "- Settings -"),
                          30.0f, 255, 255, 255, 255, 1.0f);
    draw_tab_row(game, y_tab);

    const Tab* tab = &s_tabs[s_tab];
    for (int i = 0; i < tab->count; i++) {
        const Item* it = &tab->items[i];
        int selected = (s_sel == i);

        char value_buf[64];
        item_format(it->id, value_buf, sizeof(value_buf));

        int changed = item_changed(it->id);
        f32 y = y0 + i * line_h;

        f32 s = selected ? PC_MENU_SCALE_SELECTED : 1.0f;
        pc_menu_row_colors(selected, &r, &g, &b, &a);
        pc_menu_draw_left(game, localized_item_label(it->id, it->label), lx, y, r, g, b, a, s);
        value_colors(selected, changed, &r, &g, &b, &a);
        pc_menu_draw_left(game, value_buf, vx, y, r, g, b, a, s);
    }

    /* Anchor Apply/Back to the tallest tab's footprint so they don't
     * shift up when the user moves to a tab with fewer items. */
    int max_items = 0;
    for (int t = 0; t < TAB_COUNT; t++) {
        if (s_tabs[t].count > max_items) max_items = s_tabs[t].count;
    }

    /* Apply: green when there's something to apply. */
    f32 apy = y0 + max_items * line_h + 10.0f;
    int sel_apply = (s_sel == idx_apply());
    if (sel_apply && s_pending_dirty) {
        r = 120; g = 255; b = 140; a = 255;
    } else if (sel_apply) {
        r = 160; g = 160; b = 160; a = 220;
    } else if (s_pending_dirty) {
        r = 120; g = 220; b = 140; a = 200;
    } else {
        r = 120; g = 120; b = 120; a = 160;
    }
    pc_menu_draw_centered(game, pc_language_ui_lookup("settings.apply", "Apply"), apy, r, g, b, a,
                          sel_apply ? PC_MENU_SCALE_SELECTED : 1.0f);

    /* Back */
    f32 bky = apy + line_h;
    int sel_back = (s_sel == idx_back());
    pc_menu_row_colors(sel_back, &r, &g, &b, &a);
    pc_menu_draw_centered(game, pc_language_ui_lookup("settings.back", "Back"), bky, r, g, b, a,
                          sel_back ? PC_MENU_SCALE_SELECTED : 1.0f);

    /* Restart banner stays up until the process actually restarts (survives
     * reopening the menu). Sits below Back so it never competes with the cursor. */
    if (s_pending_restart) {
        pc_menu_draw_centered(game, pc_language_ui_lookup("settings.restart_to_apply", "Restart to apply"),
                              bky + line_h + 6.0f, 255, 195, 85, 230, 1.0f);
    }
}

static void draw_res_confirm_page(struct game_s* game) {
    char buf[48];

    pc_menu_draw_centered(game, pc_language_ui_lookup("settings.keep_resolution", "- Keep this resolution? -"), 70.0f, 255, 255, 255, 255, 1.0f);

    snprintf(buf, sizeof(buf), "%dx%d",
             g_pc_settings.window_width, g_pc_settings.window_height);
    pc_menu_draw_centered(game, buf, 95.0f, 230, 230, 230, 255, 1.0f);

    Uint32 now = SDL_GetTicks();
    Uint32 ms_left = (now < s_res_deadline) ? s_res_deadline - now : 0;
    int secs = (int)((ms_left + 999) / 1000);
    snprintf(buf, sizeof(buf), pc_language_ui_lookup("settings.reverting_in_fmt", "Reverting in %d..."), secs);
    pc_menu_draw_centered(game, buf, 125.0f, 230, 200, 110, 255, 1.0f);

    pc_menu_draw_two_choice(game, pc_language_ui_lookup("settings.keep", "Keep"), pc_language_ui_lookup("settings.revert", "Revert"), s_res_sel, 160.0f);
}

/* Discard-changes confirmation shown when the user tries to Back out
 * while s_pending_dirty. No auto-timer (the user must pick). */
static void draw_back_confirm_page(struct game_s* game) {
    pc_menu_draw_centered(game, pc_language_ui_lookup("settings.discard_changes", "- Discard changes? -"), 80.0f, 255, 255, 255, 255, 1.0f);
    pc_menu_draw_centered(game, pc_language_ui_lookup("settings.unapplied_changes", "You have unapplied changes."),
                          115.0f, 230, 230, 230, 255, 1.0f);
    pc_menu_draw_two_choice(game, pc_language_ui_lookup("settings.keep_editing", "Keep editing"), pc_language_ui_lookup("settings.discard", "Discard"), s_back_sel, 160.0f);
}

/* Keyboard/gamepad columns for every remappable action, with scrolling.
 * Cells are selected with left/right; confirm arms capture for the cell. */
static void draw_bindings_page(struct game_s* game) {
    int r, g, b, a;
    f32 lx = 36.0f, kx = 128.0f, px = 232.0f;
    f32 y0 = 62.0f, line_h = 13.0f;

    pc_menu_draw_centered(game, pc_language_ui_lookup("bindings.title", "- Keybindings -"), 28.0f, 255, 255, 255, 255, 1.0f);

    pc_menu_draw_left(game, pc_language_ui_lookup("bindings.action", "Action"), lx, 46.0f, 150, 150, 150, 200, 1.0f);
    pc_menu_draw_left(game, pc_language_ui_lookup("bindings.keyboard", "Keyboard"), kx, 46.0f, 150, 150, 150, 200, 1.0f);
    pc_menu_draw_left(game, pc_language_ui_lookup("bindings.gamepad", "Gamepad"), px, 46.0f, 150, 150, 150, 200, 1.0f);

    for (int i = 0; i < BIND_VISIBLE; i++) {
        int row = s_bind_scroll + i;
        if (row >= BIND_ROW_COUNT) break;
        const BindRow* br = &s_bind_rows[row];
        f32 y = y0 + i * line_h;
        int row_sel = (s_bind_sel == row);
        char buf[48];

        pc_menu_row_colors(row_sel, &r, &g, &b, &a);
        pc_menu_draw_left(game, localized_bind_label(br->label), lx, y, r, g, b, a, 1.0f);

        /* Only game-charset ASCII renders (see glyph_ok), so the selected
         * cell is marked with <...> instead of brackets. */

        /* keyboard cell */
        {
            int cell_sel = row_sel && (s_bind_col == 0);
            if (cell_sel && s_capture) {
                snprintf(buf, sizeof(buf), "<press key>");
                r = 255; g = 140; b = 90; a = 255;
            } else {
                snprintf(buf, sizeof(buf), cell_sel ? "<%s>" : "%s",
                         pc_input_code_name(*bind_kb_slot(row)));
                sanitize_glyphs(buf);
                pc_menu_row_colors(cell_sel, &r, &g, &b, &a);
            }
            pc_menu_draw_left(game, buf, kx, y, r, g, b, a, 1.0f);
        }

        /* gamepad cell */
        {
            int cell_sel = row_sel && (s_bind_col == 1);
            if (br->pad_off < 0) {
                snprintf(buf, sizeof(buf), "%s", br->pad_fixed);
                r = 110; g = 110; b = 110; a = 160;
            } else if (cell_sel && s_capture) {
                snprintf(buf, sizeof(buf), "<press btn>");
                r = 255; g = 140; b = 90; a = 255;
            } else {
                snprintf(buf, sizeof(buf), cell_sel ? "<%s>" : "%s",
                         pc_pad_code_name(*bind_pad_slot(row)));
                pc_menu_row_colors(cell_sel, &r, &g, &b, &a);
            }
            pc_menu_draw_left(game, buf, px, y, r, g, b, a, 1.0f);
        }
    }

    /* scroll indicators (position implies direction; no arrow glyphs
     * exist in the game font) */
    if (s_bind_scroll > 0)
        pc_menu_draw_left(game, "...", 14.0f, y0, 180, 180, 180, 200, 1.0f);
    if (s_bind_scroll + BIND_VISIBLE < BIND_ROW_COUNT)
        pc_menu_draw_left(game, "...", 14.0f, y0 + (BIND_VISIBLE - 1) * line_h, 180, 180, 180, 200, 1.0f);

    /* footer rows */
    {
        int sel_def = (s_bind_sel == BIND_IDX_DEFAULTS);
        pc_menu_row_colors(sel_def, &r, &g, &b, &a);
        pc_menu_draw_centered(game, pc_language_ui_lookup("bindings.restore_defaults", "Restore Defaults"), 186.0f, r, g, b, a,
                              sel_def ? PC_MENU_SCALE_SELECTED : 1.0f);
    }
    {
        int sel_back = (s_bind_sel == BIND_IDX_BACK);
        pc_menu_row_colors(sel_back, &r, &g, &b, &a);
        pc_menu_draw_centered(game, pc_language_ui_lookup("settings.back", "Back"), 200.0f, r, g, b, a,
                              sel_back ? PC_MENU_SCALE_SELECTED : 1.0f);
    }

    /* hint line */
    if (s_capture) {
        pc_menu_draw_centered(game,
            s_bind_col == 0 ? pc_language_ui_lookup("bindings.press_key", "Press a key or mouse button (Esc cancels)")
                            : pc_language_ui_lookup("bindings.press_controller", "Press a controller button (Del clears, Esc cancels)"),
            218.0f, 255, 195, 85, 230, 1.0f);
    } else if (s_bind_sel < BIND_ROW_COUNT) {
        pc_menu_draw_centered(game, pc_language_ui_lookup("bindings.confirm_rebind", "Confirm to rebind"), 218.0f, 150, 150, 150, 180, 1.0f);
    }
}

void pc_settings_menu_draw(struct game_s* game, int with_dim_backdrop) {
    if (!s_active || !game || !game->graph) return;
    if (with_dim_backdrop) pc_menu_dim_rect(game->graph, 180);
    if (s_sub == SUB_SETTINGS)          draw_settings_page(game);
    else if (s_sub == SUB_CONFIRM_RES)  draw_res_confirm_page(game);
    else if (s_sub == SUB_BINDINGS)     draw_bindings_page(game);
    else                                draw_back_confirm_page(game);
}

/* Real Android GLES shader/GX/Emu64 paths, with a surfaceless EGL context.
 * The only adapters replace window management and on-screen debug reports.
 * No game image or copyrighted external assets are needed. */
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include "pc_gx_internal.h"
#include "dolphin/gx.h"
#include "libforest/gbi_extensions.h"
#include "libforest/emu64/emu64_wrapper.h"
#include "libultra/gu.h"
#include <assert.h>
#include <stdarg.h>

extern int g_pc_uber_shader_only;
int g_pc_window_w = 64, g_pc_window_h = 64;
int g_pc_widescreen_stretch, g_pc_model_viewer_no_cull, g_pc_verbose;
SDL_Window* g_pc_window;
uintptr_t pc_image_base, pc_image_end;
void pc_platform_update_window_size(void) {}
u32 convert_partial_address(u32 address) { return address; }
void JW_JUTReport(int x, int y, int lines, const char* format, ...) {
    (void)x; (void)y; (void)lines;
    va_list args; va_start(args, format); vfprintf(stderr, format, args); va_end(args);
}

static void expect_pixel(const char* name, int r, int g, int b) {
    unsigned char pixel[4];
    pc_gx_draw_pending();
    glReadPixels(32, 32, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    GLenum error = glGetError();
    printf("%s: RGBA=%u,%u,%u,%u GL=0x%x\n", name, pixel[0], pixel[1], pixel[2], pixel[3], error);
    assert(error == GL_NO_ERROR);
    assert(abs((int)pixel[0] - r) <= 2 && abs((int)pixel[1] - g) <= 2 && abs((int)pixel[2] - b) <= 2);
}

static void clear(void) {
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

static void draw_quad(void) {
    clear();
    GXBegin(GX_QUADS, GX_VTXFMT0, 4);
    for (int i = 0; i < 4; ++i) {
        GXPosition3f32(i == 1 || i == 2 ? .75f : -.75f, i >= 2 ? .75f : -.75f, 0);
        GXNormal3f32(0, 0, 1);
        GXColor4u8(255, 255, 255, 255);
        GXTexCoord2f32(.25f, .25f);
    }
    GXEnd();
}

static void run_list(Gfx* list, const char* name, int r, int g, int b) {
    clear();
    emu64_taskstart(list);
    expect_pixel(name, r, g, b);
}

int main(int argc, char** argv) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    (void)argv;
    EGLDisplay display = eglGetPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, NULL);
    assert(display != EGL_NO_DISPLAY && eglInitialize(display, NULL, NULL));
    const EGLint attributes[] = {EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_NONE};
    EGLConfig config; EGLint count;
    assert(eglChooseConfig(display, attributes, &config, 1, &count) && count == 1);
    const EGLint surface_attributes[] = {EGL_WIDTH, 64, EGL_HEIGHT, 64, EGL_NONE};
    const EGLint context_attributes[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    EGLSurface surface = eglCreatePbufferSurface(display, config, surface_attributes);
    EGLContext context = eglCreateContext(display, config, EGL_NO_CONTEXT, context_attributes);
    assert(surface != EGL_NO_SURFACE && context != EGL_NO_CONTEXT);
    assert(eglMakeCurrent(display, surface, surface, context));
    printf("GPU=%s GLES=%s pointers=%zu\n", glGetString(GL_RENDERER), glGetString(GL_VERSION), sizeof(void*));
    g_pc_uber_shader_only = argc > 1;
    pc_gx_init();
    glViewport(0, 0, 64, 64);
    GXSetZMode(GX_FALSE, GX_ALWAYS, GX_FALSE);
    GXSetAlphaCompare(GX_ALWAYS, 0, GX_AOP_AND, GX_ALWAYS, 0);
    GXSetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_COPY);
    GXSetNumTevStages(1);
    GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
    GXSetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
    GXSetNumChans(1);
    GXSetChanCtrl(GX_COLOR0A0, GX_FALSE, GX_SRC_REG, GX_SRC_REG, GX_LIGHT_NULL, GX_DF_NONE, GX_AF_NONE);
    GXSetChanMatColor(GX_COLOR0A0, (GXColor){180, 120, 60, 255});
    draw_quad(); expect_pixel("material", 180, 120, 60);
    GXSetTevColorS10(GX_TEVREG0, (GXColorS10){90, 170, 230, 255});
    GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_C0);
    draw_quad(); expect_pixel("GXColorS10 caller ABI", 90, 170, 230);
    GXSetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
    GXSetChanCtrl(GX_COLOR0, GX_TRUE, GX_SRC_REG, GX_SRC_REG, GX_LIGHT0, GX_DF_CLAMP, GX_AF_NONE);
    GXSetChanCtrl(GX_ALPHA0, GX_FALSE, GX_SRC_REG, GX_SRC_REG, GX_LIGHT_NULL, GX_DF_NONE, GX_AF_NONE);
    GXSetChanAmbColor(GX_COLOR0A0, (GXColor){32, 32, 32, 255});
    GXLightObj light;
    GXInitLightColor(&light, (GXColor){128, 128, 128, 255});
    GXInitLightPos(&light, 0, 0, 10);
    GXLoadLightObjImm(&light, GX_LIGHT0);
    draw_quad(); expect_pixel("lit material", 113, 75, 38);
    unsigned char image[32] = {0}, palette[32];
    for (unsigned i = 0; i < sizeof(palette); i += 2) { palette[i] = 0x80; palette[i + 1] = 0x1f; }
    GXTlutObj lut; GXTexObj texture;
    GXInitTlutObj(&lut, palette, GX_TL_RGB5A3, 16); GXLoadTlut(&lut, 0);
    GXInitTexObjCI(&texture, image, 8, 8, GX_TF_C4, GX_CLAMP, GX_CLAMP, GX_FALSE, 0);
    GXLoadTexObj(&texture, GX_TEXMAP0);
    GXSetTevOp(GX_TEVSTAGE0, GX_REPLACE);
    draw_quad(); expect_pixel("indexed texture and palette", 0, 0, 255);
    GXSetTevOp(GX_TEVSTAGE0, GX_MODULATE);
    draw_quad(); expect_pixel("lit indexed texture", 0, 0, 38);

    emu64_init();
    glViewport(0, 0, 64, 64); glDisable(GL_SCISSOR_TEST);
    Gfx list[64] = {0}, *p = list;
    gDPPipeSync(p++); gDPSetCycleType(p++, G_CYC_1CYCLE);
    gDPSetRenderMode(p++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
    gDPSetCombineMode(p++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    gDPSetPrimColor(p++, 0, 0, 180, 120, 60, 255);
    gSPClearGeometryMode(p++, 0xffffffff); gSPTexture(p++, 0, 0, 0, 0, G_OFF);
    gDPFillRectangle(p++, 100, 100, 540, 380); gSPEndDisplayList(p++);
    run_list(list, "Emu64 primitive color (RC4 black regression)", 180, 120, 60);
    p = list;
    gDPSetEnvColor(p++, 40, 150, 210, 255);
    gDPSetCombineLERP(p++, 0, 0, 0, ENVIRONMENT, 0, 0, 0, ENVIRONMENT,
                          0, 0, 0, ENVIRONMENT, 0, 0, 0, ENVIRONMENT);
    gDPFillRectangle(p++, 100, 100, 540, 380); gSPEndDisplayList(p++);
    run_list(list, "Emu64 environment color", 40, 150, 210);
    unsigned char font_image[32]; memset(font_image, 255, sizeof(font_image));
    p = list;
    gDPSetPrimColor(p++, 0, 0, 230, 190, 80, 255);
    gDPSetCombineMode(p++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gDPSetTextureImage_Dolphin(p++, G_IM_FMT_I, G_IM_SIZ_8b, 4, 8, font_image);
    gDPSetTile_Dolphin(p++, GX_TF_I8, 0, 0, GX_CLAMP, GX_CLAMP, 0, 0);
    gSPTextureRectangle(p++, 100 * 4, 100 * 4, 540 * 4, 380 * 4, 0, 0, 0, 32, 32);
    gSPEndDisplayList(p++);
    run_list(list, "Emu64 menu texture rectangle and primitive tint", 230, 190, 80);
    /* A model path uses fixed-point projection, native static vertices,
     * segmented texture coordinates and the same primitive tint. */
    static Vtx vertices[] = {
        {{{-75, -75, -5}, 0, {0, 0}, {255, 255, 255, 255}}},
        {{{75, -75, -5}, 0, {128, 0}, {255, 255, 255, 255}}},
        {{{75, 75, -5}, 0, {128, 128}, {255, 255, 255, 255}}},
        {{{-75, 75, -5}, 0, {0, 128}, {255, 255, 255, 255}}},
    };
    Mtx projection, modelview;
    guOrtho(&projection, -100, 100, -100, 100, 1, 10, 1);
    guMtxIdent(&modelview);
    p = list;
    gSPMatrix(p++, &projection, G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
    gSPMatrix(p++, &modelview, G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
    gSPTexture(p++, 0xffff, 0xffff, 0, 0, G_ON);
    gSPVertex(p++, vertices, 4, 0);
    gSP2Triangles(p++, 0, 1, 2, 0, 0, 2, 3, 0);
    gSPEndDisplayList(p++);
    run_list(list, "Emu64 textured model and fixed-point matrices", 230, 190, 80);
    pc_gx_shutdown();
    assert(eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT));
    eglDestroyContext(display, context); eglDestroySurface(display, surface); eglTerminate(display);
    puts("PASS production GLES/GX/Emu64 rendering; physical Mali/gameplay not exercised");
    return 0;
}

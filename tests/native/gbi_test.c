#include "libforest/gbi_extensions.h"
#include "pc_address.h"
#include "pc_gbi.h"
#include <assert.h>
#include <stddef.h>

static Vtx vertices[4];
static u8 texture[32];
static Gfx child[] = { gsSPEndDisplayList() };
static Gfx list[] = {
    gsSPVertex(vertices, 4, 0),
    gsDPSetTextureImage(G_IM_FMT_RGBA, G_IM_SIZ_16b, 1, texture),
    gsSPDisplayList(child),
    gsSPVertex(SEGMENT_ADDR(8, 32), 4, 0),
    gsSPEndDisplayList(),
};

int main(void) {
    union { Gfx native[2]; unsigned char bytes[2 * sizeof(Gfx)]; } raw = {0};
    const unsigned char wire[16] = {
        0xde,0,0,0, 0x08,0,0,0x20,
        0xdf,0,0,0, 0,0,0,0,
    };
    memcpy(raw.bytes, wire, sizeof(wire));
    pc_gbi_decode_wire_list(&raw, &raw, 2);
    assert(raw.native[0].words.w0 == 0xde000000 && raw.native[0].words.w1 == 0x08000020);
    assert(raw.native[1].words.w0 == 0xdf000000 && raw.native[1].words.w1 == 0);
    Gfx wide_command;
#ifdef PC_NATIVE_64
    /* Static Linux test executables can load below 4 GB. Exercise a full-width
     * integer address explicitly; no memory at this synthetic value is read. */
    uintptr_t wide_address = (uintptr_t)UINT32_MAX + 0x12345;
    gSPDisplayList(&wide_command, wide_address);
    assert(pc_address_unpack(wide_command.words.w1) == wide_address);
#else
    gSPDisplayList(&wide_command, (uintptr_t)child);
#endif
    assert(sizeof(u32) == 4 && sizeof(s32) == 4);
    assert(sizeof(Vtx) == 16 && sizeof(Mtx) == 64);
    assert(offsetof(Gwords, w1) == 4);
#ifdef PC_NATIVE_64
    assert(sizeof(Gfx) == 16);
    assert(sizeof(Gtexrect2) == 3 * sizeof(Gfx));
    assert(list[0].dma.addr == PC_GBI_STATIC_ADDRESS);
    assert(list[0].words.native_address == (uintptr_t)vertices);
    assert(list[1].words.native_address == (uintptr_t)texture);
    assert(list[2].words.native_address == (uintptr_t)child);
    assert(list[3].words.native_address == (uintptr_t)SEGMENT_ADDR(8, 32));
#else
    assert(sizeof(Gfx) == 8);
#endif
    Gfx dynamic[3] = {0};
    gSPVertex(&dynamic[0], vertices, 4, 0);
    gDPSetTextureImage(&dynamic[1], G_IM_FMT_RGBA, G_IM_SIZ_16b, 1, texture);
    gSPVertex(&dynamic[2], SEGMENT_ADDR(8, 32), 4, 0);
#ifdef PC_NATIVE_64
    assert(pc_address_unpack(dynamic[0].words.w1) == (uintptr_t)vertices);
    assert(pc_address_unpack(dynamic[1].words.w1) == (uintptr_t)texture);
#endif
    assert(dynamic[2].words.w1 == (uintptr_t)SEGMENT_ADDR(8, 32));
    return 0;
}

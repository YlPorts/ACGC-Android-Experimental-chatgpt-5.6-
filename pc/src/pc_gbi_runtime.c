#include <stdint.h>
#include "pc_address.h"
#include <stdio.h>
#include "types.h"
#include "PR/gbi.h"
#include "pc_gbi.h"

void pc_gbi_decode_wire_list(void* destination, const void* source, unsigned count) {
    Gfx* output = destination;
    const unsigned char* input = source;
    /* Backwards order also permits expansion within the same allocation. */
    while (count) {
        unsigned i = --count;
        const unsigned char* p = input + i * 8;
        uint32_t w0 = ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];
        uint32_t w1 = ((uint32_t)p[4]<<24)|((uint32_t)p[5]<<16)|((uint32_t)p[6]<<8)|p[7];
        output[i].words.w0 = w0;
        output[i].words.w1 = w1;
#ifdef PC_NATIVE_64
        output[i].words.native_address = 0;
#endif
    }
}

#define PC_GBI_ODD_PTR_TOKEN_BASE 0x02F00000u
#define PC_GBI_ODD_PTR_TOKEN_COUNT 8192u

static uintptr_t s_odd_ptr_tokens[PC_GBI_ODD_PTR_TOKEN_COUNT];
static unsigned int s_odd_ptr_token_next = 0;
static int s_warned_odd_ptr = 0;

unsigned int pc_gbi_pack_runtime_ptr(uintptr_t addr, int is_ptr, const char* expr, const char* file, int line) {
#if UINTPTR_MAX > UINT32_MAX
    (void)expr; (void)file; (void)line;
    /* Values wider than a console address are native even when a legacy
     * caller explicitly passed uintptr_t instead of a pointer expression. */
    return (is_ptr || addr > UINT32_MAX) ? pc_address_pack(addr) : (unsigned int)addr;
#else
    unsigned int slot;

    if (!is_ptr) {
        return (unsigned int)addr;
    }

    if ((addr & 1u) == 0) {
        return (unsigned int)(addr | 1u);
    }

    slot = s_odd_ptr_token_next++ & (PC_GBI_ODD_PTR_TOKEN_COUNT - 1u);
    s_odd_ptr_tokens[slot] = addr;

    if (!s_warned_odd_ptr) {
        fprintf(stderr,
                "[GBI] odd pointer alignment: %s at %s:%d = 0x%08x; using fallback token table\n",
                expr, file, line, (unsigned int)addr);
        s_warned_odd_ptr = 1;
    }

    return PC_GBI_ODD_PTR_TOKEN_BASE + slot * 2u;
#endif
}

uintptr_t pc_gbi_unpack_runtime_ptr(unsigned int packed) {
#if UINTPTR_MAX > UINT32_MAX
    uintptr_t address;
    return pc_address_try_unpack(packed, &address) ? address : 0;
#else
    unsigned int token = packed - PC_GBI_ODD_PTR_TOKEN_BASE;

    if (token < PC_GBI_ODD_PTR_TOKEN_COUNT * 2u && (token & 1u) == 0) {
        return s_odd_ptr_tokens[token / 2u];
    }

    return 0;
#endif
}

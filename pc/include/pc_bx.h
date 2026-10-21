#ifndef PC_BX_H
#define PC_BX_H
#include "jaudio_NES/bx.h"
#include <stddef.h>
typedef struct PcBx PcBx;
/* IBNK/WSYS bytes contain BE u32 offsets, not native pointer fields. */
PcBx* pc_bx_decode(const u8* bytes, size_t size, int wave_system);
Bank_* pc_bx_bank(PcBx* bx);
Wsys_* pc_bx_wsys(PcBx* bx);
void pc_bx_free(PcBx* bx);
#endif

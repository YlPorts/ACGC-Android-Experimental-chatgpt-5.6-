/* Opaque 32-bit references for native addresses in fixed-width runtime objects.
 * These references are process-local. Never write them to ROM/GCI/pack files. */
#ifndef PC_ADDRESS_H
#define PC_ADDRESS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint32_t pc_address_pack(uintptr_t address);
uintptr_t pc_address_unpack(uint32_t reference);
int pc_address_try_unpack(uint32_t reference, uintptr_t* address);

#ifdef __cplusplus
}
#endif
#endif

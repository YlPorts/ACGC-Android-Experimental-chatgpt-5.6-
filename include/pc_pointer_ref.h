/* Runtime identity/reference conversion. Not a file offset or a console
 * virtual address. Keep fixed-width game fields while preserving full native
 * addresses in the process-local registry. */
#ifndef PC_POINTER_REF_H
#define PC_POINTER_REF_H
#ifdef TARGET_PC
#include "pc_address.h"
#define PC_PTR_REF(pointer) pc_address_pack((uintptr_t)(pointer))
#define PC_REF_PTR(type, reference) ((type)pc_address_unpack((uint32_t)(reference)))
#else
#define PC_PTR_REF(pointer) ((u32)(pointer))
#define PC_REF_PTR(type, reference) ((type)(reference))
#endif
#endif

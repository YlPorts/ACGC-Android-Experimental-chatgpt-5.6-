#ifndef PC_GUEST_MEMORY_H
#define PC_GUEST_MEMORY_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Original GameCube addresses identify resources, never process pointers.
 * Access bytes through their relocated, endian-converted native asset. */
void pc_guest_map(uint32_t address, void* data, size_t size, int swap_type);
void pc_guest_map_dol_asset(const uint8_t header[256], uint32_t file_offset,
                          void* data, size_t size, int swap_type);
int pc_guest_read(uint32_t address, void* data, size_t size);
int pc_guest_write(uint32_t address, const void* data, size_t size);
#ifdef __cplusplus
}
#endif
#endif

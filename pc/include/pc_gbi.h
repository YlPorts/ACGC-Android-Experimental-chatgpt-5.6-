#ifndef PC_GBI_H
#define PC_GBI_H
/* Expand 8-byte big-endian ROM commands to the native compiled Gfx layout.
 * Destination must have count * sizeof(Gfx) bytes. In-place expansion works. */
#ifdef __cplusplus
extern "C" {
#endif
void pc_gbi_decode_wire_list(void* destination, const void* source, unsigned count);
#ifdef __cplusplus
}
#endif
#endif

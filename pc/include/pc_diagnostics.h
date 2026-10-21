#ifndef PC_DIAGNOSTICS_H
#define PC_DIAGNOSTICS_H
#ifdef __cplusplus
extern "C" {
#endif
/* Local, bounded session report. Never enables experimental regional audio. */
void pc_diagnostics_begin(void);
void pc_diagnostics_end(void);
void pc_diagnostics_log(const char* format, ...) __attribute__((format(printf, 1, 2)));
#ifdef __cplusplus
}
#endif
#endif

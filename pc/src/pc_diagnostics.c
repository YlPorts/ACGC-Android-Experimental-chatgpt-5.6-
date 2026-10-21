#include "pc_diagnostics.h"
#include <stdarg.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

#define REPORT_LIMIT 65536u
static atomic_flag lock = ATOMIC_FLAG_INIT;
static FILE* report;
static size_t written;

static void acquire(void) {
    while (atomic_flag_test_and_set_explicit(&lock, memory_order_acquire)) {}
}
static void release(void) { atomic_flag_clear_explicit(&lock, memory_order_release); }

void pc_diagnostics_begin(void) {
    acquire();
    if (report) fclose(report);
    report = fopen("diagnostics.txt", "wb");
    written = 0;
    release();
    pc_diagnostics_log("Native process: pointers=%zu long=%zu; PC base 0.9.3", sizeof(void*), sizeof(long));
}

void pc_diagnostics_end(void) {
    acquire();
    if (report) fclose(report);
    report = NULL;
    release();
}

void pc_diagnostics_log(const char* format, ...) {
    char line[640];
    va_list args; va_start(args, format);
    int length = vsnprintf(line, sizeof(line) - 1, format, args);
    va_end(args);
    if (length < 0) return;
    size_t size = (size_t)length;
    if (size >= sizeof(line) - 1) size = sizeof(line) - 2;
    line[size++] = '\n';
    acquire();
    if (report) {
        if (written + size <= REPORT_LIMIT - 80) {
            written += fwrite(line, 1, size, report);
            fflush(report);
        } else {
            fputs("Report limit reached; collection stopped for this session.\n", report);
            fclose(report);
            report = NULL;
        }
    }
    release();
}

#include "pc_diagnostics.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void* writer(void* data) {
    for (unsigned i = 0; i < 2000; ++i) pc_diagnostics_log("writer=%u message=%u", *(unsigned*)data, i);
    return NULL;
}
int main(void) {
    char directory[] = "/tmp/acgc-diagnostics-XXXXXX";
    assert(mkdtemp(directory) && !chdir(directory));
    pc_diagnostics_begin();
    unsigned ids[] = {1, 2}; pthread_t threads[2];
    for (unsigned i = 0; i < 2; ++i) assert(!pthread_create(&threads[i], NULL, writer, &ids[i]));
    for (unsigned i = 0; i < 2; ++i) assert(!pthread_join(threads[i], NULL));
    pc_diagnostics_end();
    FILE* file = fopen("diagnostics.txt", "rb"); assert(file);
    assert(!fseek(file, 0, SEEK_END)); long size = ftell(file); assert(size > 64000 && size <= 65536);
    rewind(file); char line[640]; unsigned lines = 0;
    while (fgets(line, sizeof(line), file)) {
        assert(strchr(line, '\n'));
        assert(!strncmp(line, "Native process:", 15) || !strncmp(line, "writer=", 7) || !strncmp(line, "Report limit", 12));
        ++lines;
    }
    assert(lines > 1000); fclose(file);
    pc_diagnostics_begin(); pc_diagnostics_log("fresh session"); pc_diagnostics_end();
    file = fopen("diagnostics.txt", "rb"); assert(file);
    assert(!fseek(file, 0, SEEK_END) && ftell(file) < 640); fclose(file);
    assert(!unlink("diagnostics.txt") && !chdir("/") && !rmdir(directory));
    return 0;
}

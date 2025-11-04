#include "monitor.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

int get_io_metrics(pid_t pid, IoMetrics *metrics) {
    if (!metrics) {
        errno = EINVAL;
        return -1;
    }

    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/io", pid);

    FILE *fp = fopen(path, "r");
    if (!fp) {
        return -1;
    }

    metrics->pid = pid;
    metrics->read_bytes = 0;
    metrics->write_bytes = 0;
    metrics->read_syscalls = 0;
    metrics->write_syscalls = 0;

    char line[256];
    int found_read_bytes = 0;
    int found_write_bytes = 0;
    int found_syscr = 0;
    int found_syscw = 0;

    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "read_bytes:", 11) == 0) {
            if (sscanf(line + 11, "%llu", &metrics->read_bytes) == 1) found_read_bytes = 1;
        } else if (strncmp(line, "write_bytes:", 12) == 0) {
            if (sscanf(line + 12, "%llu", &metrics->write_bytes) == 1) found_write_bytes = 1;
        } else if (strncmp(line, "syscr:", 6) == 0) {
            if (sscanf(line + 6, "%llu", &metrics->read_syscalls) == 1) found_syscr = 1;
        } else if (strncmp(line, "syscw:", 6) == 0) {
            if (sscanf(line + 6, "%llu", &metrics->write_syscalls) == 1) found_syscw = 1;
        }

        if (found_read_bytes && found_write_bytes && found_syscr && found_syscw) {
            break;
        }
    }

    fclose(fp);

    // Apenas read/write_bytes são considerados essenciais, pois syscr/syscw podem não estar presentes
    // em todos os kernels ou configurações.
    if (!found_read_bytes || !found_write_bytes) {
        errno = ENODATA;
        return -1;
    }

    return 0;
}

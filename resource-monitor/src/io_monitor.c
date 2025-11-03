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

    char line[256];
    int found_read = 0;
    int found_write = 0;

    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "read_bytes:", 11) == 0) {
            if (sscanf(line + 11, "%llu", &metrics->read_bytes) != 1) {
                fclose(fp);
                errno = EPROTO;
                return -1;
            }
            found_read = 1;
        } else if (strncmp(line, "write_bytes:", 12) == 0) {
            if (sscanf(line + 12, "%llu", &metrics->write_bytes) != 1) {
                fclose(fp);
                errno = EPROTO;
                return -1;
            }
            found_write = 1;
        }

        if (found_read && found_write) {
            break;
        }
    }

    fclose(fp);

    if (!found_read || !found_write) {
        errno = ENODATA;
        return -1;
    }

    return 0;
}

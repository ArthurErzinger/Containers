#include "monitor.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

int get_memory_metrics(pid_t pid, MemoryMetrics *metrics) {
    if (!metrics) {
        errno = EINVAL;
        return -1;
    }

    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/status", pid);

    FILE *fp = fopen(path, "r");
    if (!fp) {
        return -1;
    }

    metrics->pid = pid;
    metrics->vm_size_kb = -1;
    metrics->vm_rss_kb = -1;

    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "VmSize:", 7) == 0) {
            if (sscanf(line + 7, "%ld", &metrics->vm_size_kb) != 1) {
                fclose(fp);
                errno = EPROTO;
                return -1;
            }
        } else if (strncmp(line, "VmRSS:", 6) == 0) {
            if (sscanf(line + 6, "%ld", &metrics->vm_rss_kb) != 1) {
                fclose(fp);
                errno = EPROTO;
                return -1;
            }
        }

        if (metrics->vm_size_kb >= 0 && metrics->vm_rss_kb >= 0) {
            break;
        }
    }

    fclose(fp);

    if (metrics->vm_size_kb < 0 || metrics->vm_rss_kb < 0) {
        errno = ENODATA;
        return -1;
    }

    return 0;
}

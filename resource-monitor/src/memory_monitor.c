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
    metrics->vm_swap_kb = -1;

    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "VmSize:", 7) == 0) {
            sscanf(line + 7, "%ld", &metrics->vm_size_kb);
        } else if (strncmp(line, "VmRSS:", 6) == 0) {
            sscanf(line + 6, "%ld", &metrics->vm_rss_kb);
        } else if (strncmp(line, "VmSwap:", 7) == 0) {
            sscanf(line + 7, "%ld", &metrics->vm_swap_kb);
        }

        if (metrics->vm_size_kb >= 0 && metrics->vm_rss_kb >= 0 && metrics->vm_swap_kb >= 0) {
            break;
        }
    }

    fclose(fp);

    // VmSwap pode ser 0 ou não existir, então não é um requisito para o sucesso
    if (metrics->vm_size_kb < 0 || metrics->vm_rss_kb < 0) {
        errno = ENODATA;
        return -1;
    }

    return 0;
}

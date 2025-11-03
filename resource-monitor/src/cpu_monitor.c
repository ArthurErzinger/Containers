#include "monitor.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

int get_global_cpu_time(unsigned long *total_time) {
    FILE *fp = fopen("/proc/stat", "r");
    if (!fp) {
        return -1;
    }

    char line[256];
    if (fgets(line, sizeof(line), fp) == NULL) {
        fclose(fp);
        return -1;
    }

    fclose(fp);

    if (strncmp(line, "cpu ", 4) != 0) {
        return -1; // Unexpected format
    }

    unsigned long user, nice, system, idle, iowait, irq, softirq, steal;
    int scanned = sscanf(line + 5, "%lu %lu %lu %lu %lu %lu %lu %lu",
                          &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal);

    if (scanned < 4) {
        return -1; // Could not parse all required fields
    }

    *total_time = user + nice + system + idle + iowait + irq + softirq + steal;

    return 0;
}

int get_cpu_metrics(pid_t pid, CpuMetrics *metrics) {
    if (!metrics) {
        errno = EINVAL;
        return -1;
    }

    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);

    FILE *fp = fopen(path, "r");
    if (!fp) {
        return -1;
    }

    metrics->pid = pid;

    int scanned = fscanf(fp,
                         "%*d "  // pid
                         "%*s "  // comm
                         "%*c "  // state
                         "%*d %*d %*d %*d %*d "
                         "%*u %*u %*u %*u %*u "
                         "%lu %lu %lu %lu "  // utime, stime, cutime, cstime
                         "%*d %*d %*d %*d "
                         "%ld",      // num_threads
                         &metrics->utime,
                         &metrics->stime,
                         &metrics->cutime,
                         &metrics->cstime,
                         &metrics->num_threads);

    fclose(fp);

    if (scanned != 5) {
        errno = EPROTO;
        return -1;
    }

    return 0;
}

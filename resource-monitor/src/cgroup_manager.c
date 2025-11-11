#include "cgroup.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/stat.h>
#include <errno.h>
#include <dirent.h>

// --- Utility Functions ---

static int is_cgroup_v2() {
    return access("/sys/fs/cgroup/cgroup.controllers", F_OK) == 0;
}

static int read_cgroup_uint64(const char* cgroup_path, const char* file, uint64_t* value) {
    char file_path[1024];
    snprintf(file_path, sizeof(file_path), "%s/%s", cgroup_path, file);
    FILE* fp = fopen(file_path, "r");
    if (!fp) return 0;
    if (fscanf(fp, "%%" SCNu64, value) != 1) {
        fclose(fp);
        return 0;
    }
    fclose(fp);
    return 1;
}

static int read_cgroup_int64(const char* cgroup_path, const char* file, int64_t* value) {
    char file_path[1024];
    snprintf(file_path, sizeof(file_path), "%s/%s", cgroup_path, file);
    FILE* fp = fopen(file_path, "r");
    if (!fp) {
        *value = -1; // Indicate not present or error
        return 0;
    }
    if (fscanf(fp, "%%" SCNd64, value) != 1) {
        *value = -1;
        fclose(fp);
        return 0;
    }
    fclose(fp);
    return 1;
}

static void read_stat_file(const char* cgroup_path, const char* file, const char* keys[], uint64_t* values[], int count) {
    char file_path[1024];
    snprintf(file_path, sizeof(file_path), "%s/%s", cgroup_path, file);
    FILE* fp = fopen(file_path, "r");
    if (!fp) return;
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        char key[128];
        uint64_t value;
        if (sscanf(line, "%%s %%" SCNu64, key, &value) == 2) {
            for (int i = 0; i < count; i++) {
                if (strcmp(key, keys[i]) == 0) {
                    *values[i] = value;
                    break;
                }
            }
        }
    }
    fclose(fp);
}

// --- Individual Metric Readers ---

static void read_single_cpu_metrics(const char* path, CgroupCpuMetrics* metrics) {
    memset(metrics, 0, sizeof(CgroupCpuMetrics));
    if (is_cgroup_v2()) {
        const char* keys[] = {"usage_usec", "nr_periods", "nr_throttled", "throttled_usec"};
        uint64_t* values[] = {&metrics->usage_ns, &metrics->nr_periods, &metrics->nr_throttled, &metrics->throttled_ns};
        read_stat_file(path, "cpu.stat", keys, values, 4);
        metrics->usage_ns *= 1000;
        metrics->throttled_ns *= 1000;
    } else {
        read_cgroup_uint64(path, "cpuacct.usage", &metrics->usage_ns);
        const char* keys[] = {"nr_periods", "nr_throttled", "throttled_time"};
        uint64_t* values[] = {&metrics->nr_periods, &metrics->nr_throttled, &metrics->throttled_ns};
        read_stat_file(path, "cpu.stat", keys, values, 3);
    }
}

static void read_single_memory_metrics(const char* path, CgroupMemoryMetrics* metrics) {
    memset(metrics, 0, sizeof(CgroupMemoryMetrics));
    if (is_cgroup_v2()) {
        read_cgroup_uint64(path, "memory.current", &metrics->current);
        const char* keys[] = {"anon", "file", "kernel_stack", "slab", "sock", "pgfault", "pgmajfault"};
        uint64_t* values[] = {&metrics->anon, &metrics->file, &metrics->kernel_stack, &metrics->slab, &metrics->sock, &metrics->pgfault, &metrics->pgmajfault};
        read_stat_file(path, "memory.stat", keys, values, 7);
    } else {
        read_cgroup_uint64(path, "memory.usage_in_bytes", &metrics->current);
        const char* keys[] = {"total_inactive_anon", "total_active_file", "total_kernel_stack", "total_slab", "total_sock", "total_pgfault", "total_pgmajfault"};
        uint64_t* values[] = {&metrics->anon, &metrics->file, &metrics->kernel_stack, &metrics->slab, &metrics->sock, &metrics->pgfault, &metrics->pgmajfault};
        read_stat_file(path, "memory.stat", keys, values, 7);
    }
}

static void read_single_io_metrics(const char* path, CgroupIoMetrics* metrics) {
    memset(metrics, 0, sizeof(CgroupIoMetrics));
    if (is_cgroup_v2()) {
        const char* keys[] = {"rbytes", "wbytes", "rios", "wios"};
        uint64_t* values[] = {&metrics->rbytes, &metrics->wbytes, &metrics->rios, &metrics->wios};
        read_stat_file(path, "io.stat", keys, values, 4);
    } else {
        // v1 I/O reading is complex and not implemented for aggregation.
    }
}

// --- Recursive Aggregation ---

static void aggregate_metrics_recursive(const char* base_path, CgroupCpuMetrics* total_cpu, CgroupMemoryMetrics* total_mem, CgroupIoMetrics* total_io) {
    CgroupCpuMetrics current_cpu;
    CgroupMemoryMetrics current_mem;
    CgroupIoMetrics current_io;

    read_single_cpu_metrics(base_path, &current_cpu);
    read_single_memory_metrics(base_path, &current_mem);
    read_single_io_metrics(base_path, &current_io);

    total_cpu->usage_ns += current_cpu.usage_ns;
    total_cpu->nr_periods += current_cpu.nr_periods;
    total_cpu->nr_throttled += current_cpu.nr_throttled;
    total_cpu->throttled_ns += current_cpu.throttled_ns;

    total_mem->current += current_mem.current;
    total_mem->anon += current_mem.anon;
    total_mem->file += current_mem.file;
    total_mem->kernel_stack += current_mem.kernel_stack;
    total_mem->slab += current_mem.slab;
    total_mem->sock += current_mem.sock;
    total_mem->pgfault += current_mem.pgfault;
    total_mem->pgmajfault += current_mem.pgmajfault;

    total_io->rbytes += current_io.rbytes;
    total_io->wbytes += current_io.wbytes;
    total_io->rios += current_io.rios;
    total_io->wios += current_io.wios;

    DIR* dir = opendir(base_path);
    if (!dir) return;

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR && strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            char sub_path[2048];
            snprintf(sub_path, sizeof(sub_path), "%s/%s", base_path, entry->d_name);
            aggregate_metrics_recursive(sub_path, total_cpu, total_mem, total_io);
        }
    }
    closedir(dir);
}

// --- Metric Printers (CORRECTED) ---

static void print_cpu_metrics(const CgroupCpuMetrics* metrics) {
    printf("--- CPU Metrics (Aggregated) ---\
");
    printf("  Usage: %.2f ms\n", (double)metrics->usage_ns / 1e6);
    if (metrics->cpu_quota_us > 0 && metrics->cpu_period_us > 0) {
        double percentage = 100.0 * metrics->cpu_quota_us / metrics->cpu_period_us;
        printf("  Limit: %.2f%% (on top-level cgroup)\
", percentage);
    } else {
        printf("  Limit: Not set\n");
    }
    printf("  Throttling:\
");
    printf("    - Periods: %%%" PRIu64 "\n", metrics->nr_periods);
    printf("    - Throttled Periods: %%%" PRIu64 "\n", metrics->nr_throttled);
    printf("    - Throttled Time: %.2f ms\n", (double)metrics->throttled_ns / 1e6);
}

static void print_memory_metrics(const CgroupMemoryMetrics* metrics) {
    printf("--- Memory Metrics (Aggregated) ---\
");
    printf("  Current Usage: %%%" PRIu64 " MB\n", metrics->current / (1024 * 1024));
    if (metrics->memory_limit_bytes > 0 && metrics->memory_limit_bytes != -1) {
        printf("  Limit: %lld MB (on top-level cgroup)\
", metrics->memory_limit_bytes / (1024 * 1024));
    } else {
        printf("  Limit: Not set\n");
    }
    printf("  Memory Stats (in MB):\
");
    printf("    - Anonymous: %%%" PRIu64 "\n", metrics->anon / (1024 * 1024));
    printf("    - File Cache: %%%" PRIu64 "\n", metrics->file / (1024 * 1024));
    printf("  Page Faults:\
");
    printf("    - Total: %%%" PRIu64 "\n", metrics->pgfault);
    printf("    - Major: %%%" PRIu64 "\n", metrics->pgmajfault);
}

static void print_io_metrics(const CgroupIoMetrics* metrics) {
    printf("--- I/O Metrics (Aggregated) ---\
");
    printf("  Bytes Read: %%%" PRIu64 " MB\n", metrics->rbytes / (1024 * 1024));
    printf("  Bytes Written: %%%" PRIu64 " MB\n", metrics->wbytes / (1024 * 1024));
    printf("  Read IOs: %%%" PRIu64 "\n", metrics->rios);
    printf("  Write IOs: %%%" PRIu64 "\n", metrics->wios);
}

// --- Main Display Function (CORRECTED) ---

void display_cgroup_metrics(const char* cgroup_path) {
    if (cgroup_path == NULL || strlen(cgroup_path) == 0) {
        fprintf(stderr, "Error: Cgroup path provided is empty.\n");
        return;
    }

    if (!is_cgroup_v2()) {
        fprintf(stderr, "Error: Metric aggregation is only supported for cgroup v2.\n");
        fprintf(stderr, "Your system appears to be using cgroup v1.\n");
        return;
    }

    char full_path[2048];
    if (cgroup_path[0] != '/') {
        snprintf(full_path, sizeof(full_path), "/sys/fs/cgroup/%%s", cgroup_path);
    } else {
        strncpy(full_path, cgroup_path, sizeof(full_path) - 1);
        full_path[sizeof(full_path) - 1] = '\0';
    }

    struct stat st;
    if (stat(full_path, &st) != 0 || !S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Error: The constructed cgroup path is invalid or not a directory.\n");
        fprintf(stderr, "Path: '%s'\n", full_path);
        perror("Reason");
        return;
    }

    printf("\nDisplaying aggregated metrics for cgroup hierarchy: %s\n\n", full_path);

    CgroupCpuMetrics total_cpu = {0};
    CgroupMemoryMetrics total_mem = {0};
    CgroupIoMetrics total_io = {0};

    // Get limits from the top-level cgroup only
    char temp_path[2560];
    snprintf(temp_path, sizeof(temp_path), "%s/cpu.max", full_path);
    FILE* fp = fopen(temp_path, "r");
    if (fp) {
        if (fscanf(fp, "%%lld %%lld", &total_cpu.cpu_quota_us, &total_cpu.cpu_period_us) != 2) {
             total_cpu.cpu_quota_us = -1;
        }
        fclose(fp);
    } else {
        total_cpu.cpu_quota_us = -1;
    }
    read_cgroup_int64(full_path, "memory.max", &total_mem.memory_limit_bytes);

    // Start the recursive aggregation
    aggregate_metrics_recursive(full_path, &total_cpu, &total_mem, &total_io);

    print_cpu_metrics(&total_cpu);
    printf("\n");
    print_memory_metrics(&total_mem);
    printf("\n");
    print_io_metrics(&total_io);
}

// --- Management Functions (Create, Move, Limit) ---
// These functions remain largely unchanged as they operate on specific cgroups

int create_cgroup(const char* cgroup_name) {
    if (cgroup_name == NULL || strlen(cgroup_name) == 0 || strchr(cgroup_name, '/') != NULL) {
        fprintf(stderr, "Error: Invalid cgroup name. It cannot be empty or contain a '/'.\n");
        errno = EINVAL;
        return -1;
    }
    if (is_cgroup_v2()) {
        char path[2048];
        snprintf(path, sizeof(path), "/sys/fs/cgroup/%%s", cgroup_name);
        if (mkdir(path, 0755) != 0 && errno != EEXIST) return -1;
    } else {
        const char* controllers[] = {"cpu", "memory", "blkio"};
        for (int i = 0; i < 3; i++) {
            char path[2048];
            snprintf(path, sizeof(path), "/sys/fs/cgroup/%%s/%%s", controllers[i], cgroup_name);
            char controller_path[1024];
            snprintf(controller_path, sizeof(controller_path), "/sys/fs/cgroup/%%s", controllers[i]);
            struct stat st;
            if (stat(controller_path, &st) == 0 && S_ISDIR(st.st_mode)) {
                if (mkdir(path, 0755) != 0 && errno != EEXIST) return -1;
            }
        }
    }
    return 0;
}

static int write_to_cgroup_file(const char* cgroup_path, const char* file, const char* value) {
    char file_path[2048];
    snprintf(file_path, sizeof(file_path), "%s/%s", cgroup_path, file);
    FILE* fp = fopen(file_path, "w");
    if (!fp) return -1;
    if (fprintf(fp, "%s", value) < 0) {
        fclose(fp);
        return -1;
    }
    fclose(fp);
    return 0;
}

int move_process_to_cgroup(pid_t pid, const char* cgroup_name) {
    char cgroup_fs_path[1024];
    char pid_str[32];
    snprintf(pid_str, sizeof(pid_str), "%d", pid);

    if (is_cgroup_v2()) {
        snprintf(cgroup_fs_path, sizeof(cgroup_fs_path), "/sys/fs/cgroup/%%s", cgroup_name);
        return write_to_cgroup_file(cgroup_fs_path, "cgroup.procs", pid_str);
    } else {
        const char* controllers[] = {"cpu", "memory", "blkio"};
        int result = 0;
        for (int i = 0; i < 3; i++) {
            snprintf(cgroup_fs_path, sizeof(cgroup_fs_path), "/sys/fs/cgroup/%%s/%%s", controllers[i], cgroup_name);
            struct stat st;
            if (stat(cgroup_fs_path, &st) == 0 && S_ISDIR(st.st_mode)) {
                if (write_to_cgroup_file(cgroup_fs_path, "cgroup.procs", pid_str) != 0) {
                    result = -1;
                }
            }
        }
        return result;
    }
}

int apply_cpu_limit(const char* cgroup_name, int percentage) {
    char cgroup_fs_path[1024];
    if (is_cgroup_v2()) {
        snprintf(cgroup_fs_path, sizeof(cgroup_fs_path), "/sys/fs/cgroup/%%s", cgroup_name);
        long long quota = 100000 * percentage / 100;
        char limit_str[64];
        snprintf(limit_str, sizeof(limit_str), "%%lld 100000", quota);
        return write_to_cgroup_file(cgroup_fs_path, "cpu.max", limit_str);
    } else {
        snprintf(cgroup_fs_path, sizeof(cgroup_fs_path), "/sys/fs/cgroup/cpu/%%s", cgroup_name);
        long long quota = 100000 * percentage / 100;
        char quota_str[32];
        snprintf(quota_str, sizeof(quota_str), "%%lld", quota);
        if (write_to_cgroup_file(cgroup_fs_path, "cpu.cfs_period_us", "100000") != 0) return -1;
        return write_to_cgroup_file(cgroup_fs_path, "cpu.cfs_quota_us", quota_str);
    }
}

int apply_memory_limit(const char* cgroup_name, long long bytes, long long swap_bytes) {
    char cgroup_fs_path[1024];
    char limit_str[32];
    int result = 0;

    snprintf(limit_str, sizeof(limit_str), "%%lld", bytes);
    if (is_cgroup_v2()) {
        snprintf(cgroup_fs_path, sizeof(cgroup_fs_path), "/sys/fs/cgroup/%%s", cgroup_name);
        if (write_to_cgroup_file(cgroup_fs_path, "memory.max", limit_str) != 0) result = -1;
    } else {
        snprintf(cgroup_fs_path, sizeof(cgroup_fs_path), "/sys/fs/cgroup/memory/%%s", cgroup_name);
        if (write_to_cgroup_file(cgroup_fs_path, "memory.limit_in_bytes", limit_str) != 0) result = -1;
    }

    if (swap_bytes >= 0) {
        char swap_limit_str[32];
        snprintf(swap_limit_str, sizeof(swap_limit_str), "%%lld", swap_bytes);
        if (is_cgroup_v2()) {
            if (write_to_cgroup_file(cgroup_fs_path, "memory.swap.max", swap_limit_str) != 0) result = -1;
        } else {
            // For v1, the file is memory.memsw.limit_in_bytes
            snprintf(cgroup_fs_path, sizeof(cgroup_fs_path), "/sys/fs/cgroup/memory/%%s", cgroup_name);
            if (write_to_cgroup_file(cgroup_fs_path, "memory.memsw.limit_in_bytes", swap_limit_str) != 0) result = -1;
        }
    }
    return result;
}

int apply_io_limit(const char* cgroup_name, const char* device, long long read_bps, long long write_bps) {
    char cgroup_fs_path[1024];
    if (is_cgroup_v2()) {
        snprintf(cgroup_fs_path, sizeof(cgroup_fs_path), "/sys/fs/cgroup/%%s", cgroup_name);
        char limit_str[128];
        snprintf(limit_str, sizeof(limit_str), "%s rbps=%%lld wbps=%%lld", device, read_bps, write_bps);
        return write_to_cgroup_file(cgroup_fs_path, "io.max", limit_str);
    } else {
        snprintf(cgroup_fs_path, sizeof(cgroup_fs_path), "/sys/fs/cgroup/blkio/%%s", cgroup_name);
        char read_limit_str[128];
        snprintf(read_limit_str, sizeof(read_limit_str), "%%s %%lld", device, read_bps);
        if (write_to_cgroup_file(cgroup_fs_path, "blkio.throttle.read_bps_device", read_limit_str) != 0) return -1;
        
        char write_limit_str[128];
        snprintf(write_limit_str, sizeof(write_limit_str), "%%s %%lld", device, write_bps);
        return write_to_cgroup_file(cgroup_fs_path, "blkio.throttle.write_bps_device", write_limit_str);
    }
}
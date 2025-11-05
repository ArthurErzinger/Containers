#include "cgroup.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/stat.h>
#include <errno.h>

// --- Utility Functions ---

/**
 * @brief Checks if the system is using cgroup v2 unified hierarchy.
 */
static int is_cgroup_v2() {
    return access("/sys/fs/cgroup/cgroup.controllers", F_OK) == 0;
}

/**
 * @brief Reads a uint64 value from a file inside a cgroup directory.
 * Returns 1 on success, 0 on failure.
 */
static int read_cgroup_uint64(const char* cgroup_path, const char* file, uint64_t* value) {
    char file_path[1024];
    snprintf(file_path, sizeof(file_path), "%s/%s", cgroup_path, file);

    FILE* fp = fopen(file_path, "r");
    if (!fp) {
        return 0; // File might not exist, which is not always an error
    }

    if (fscanf(fp, "%" SCNu64, value) != 1) {
        fclose(fp);
        return 0; // Failed to parse
    }

    fclose(fp);
    return 1;
}

/**
 * @brief Reads key-value pairs from a stat file (e.g., cpu.stat, memory.stat).
 */
static void read_stat_file(const char* cgroup_path, const char* file, const char* keys[], uint64_t* values[], int count) {
    char file_path[1024];
    snprintf(file_path, sizeof(file_path), "%s/%s", cgroup_path, file);

    FILE* fp = fopen(file_path, "r");
    if (!fp) {
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        char key[128];
        uint64_t value;
        if (sscanf(line, "%s %" SCNu64, key, &value) == 2) {
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

// --- Metric Readers ---

static void read_cpu_metrics(const char* path, CgroupCpuMetrics* metrics) {
    if (is_cgroup_v2()) {
        const char* keys[] = {"usage_usec", "nr_periods", "nr_throttled", "throttled_usec"};
        uint64_t* values[] = {&metrics->usage_ns, &metrics->nr_periods, &metrics->nr_throttled, &metrics->throttled_ns};
        read_stat_file(path, "cpu.stat", keys, values, 4);
        metrics->usage_ns *= 1000; // convert usec to nsec
        metrics->throttled_ns *= 1000; // convert usec to nsec
    } else {
        // cgroup v1 paths
        char cpuacct_path[1024];
        snprintf(cpuacct_path, sizeof(cpuacct_path), "%s/cpuacct.usage", path);
        read_cgroup_uint64(path, "cpuacct.usage", &metrics->usage_ns);

        const char* keys[] = {"nr_periods", "nr_throttled", "throttled_time"};
        uint64_t* values[] = {&metrics->nr_periods, &metrics->nr_throttled, &metrics->throttled_ns};
        read_stat_file(path, "cpu.stat", keys, values, 3);
    }
}

static void read_memory_metrics(const char* path, CgroupMemoryMetrics* metrics) {
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

/**
 * @brief Parses a blkio file (v1) to sum read and write operations.
 * These files have lines like: <device> <operation> <value>
 */
static void parse_blkio_file(const char* cgroup_path, const char* file, uint64_t* total_read, uint64_t* total_write) {
    char file_path[2048];
    snprintf(file_path, sizeof(file_path), "%s/%s", cgroup_path, file);

    FILE* fp = fopen(file_path, "r");
    if (!fp) return;

    *total_read = 0;
    *total_write = 0;
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        char operation[32];
        uint64_t value;
        // Format is typically: <device> <operation> <value>
        if (sscanf(line, "%*s %31s %" SCNu64, operation, &value) == 2) {
            if (strcmp(operation, "Read") == 0) {
                *total_read += value;
            } else if (strcmp(operation, "Write") == 0) {
                *total_write += value;
            }
        }
    }
    fclose(fp);
}

static void read_io_metrics(const char* path, CgroupIoMetrics* metrics) {
    if (is_cgroup_v2()) {
        const char* keys[] = {"rbytes", "wbytes", "rios", "wios"};
        uint64_t* values[] = {&metrics->rbytes, &metrics->wbytes, &metrics->rios, &metrics->wios};
        read_stat_file(path, "io.stat", keys, values, 4);
    } else {
        // For v1, parse the blkio files
        uint64_t rbytes = 0, wbytes = 0, rios = 0, wios = 0;
        
        // Try reading from throttle files first
        parse_blkio_file(path, "blkio.throttle.io_service_bytes", &rbytes, &wbytes);
        parse_blkio_file(path, "blkio.throttle.io_serviced", &rios, &wios);
        
        // If they were empty (or didn't exist), try the non-throttled versions
        if (rbytes == 0 && wbytes == 0) {
            parse_blkio_file(path, "blkio.io_service_bytes", &rbytes, &wbytes);
        }
        if (rios == 0 && wios == 0) {
            parse_blkio_file(path, "blkio.io_serviced", &rios, &wios);
        }

        metrics->rbytes = rbytes;
        metrics->wbytes = wbytes;
        metrics->rios = rios;
        metrics->wios = wios;
    }
}

// --- Metric Printers ---

static void print_cpu_metrics(const CgroupCpuMetrics* metrics) {
    printf("--- CPU Metrics ---\n");
    printf("  Usage: %.2f ms\n", (double)metrics->usage_ns / 1e6);
    printf("  Throttling:\n");
    printf("    - Periods: %" PRIu64 "\n", metrics->nr_periods);
    printf("    - Throttled Periods: %" PRIu64 "\n", metrics->nr_throttled);
    printf("    - Throttled Time: %.2f ms\n", (double)metrics->throttled_ns / 1e6);
}

static void print_memory_metrics(const CgroupMemoryMetrics* metrics) {
    printf("--- Memory Metrics ---\n");
    printf("  Current Usage: %" PRIu64 " MB\n", metrics->current / (1024 * 1024));
    printf("  Memory Stats (in MB):\n");
    printf("    - Anonymous: %" PRIu64 "\n", metrics->anon / (1024 * 1024));
    printf("    - File Cache: %" PRIu64 "\n", metrics->file / (1024 * 1024));
    printf("    - Kernel Stack: %" PRIu64 "\n", metrics->kernel_stack / (1024 * 1024));
    printf("    - Slab: %" PRIu64 "\n", metrics->slab / (1024 * 1024));
    printf("  Page Faults:\n");
    printf("    - Total: %" PRIu64 "\n", metrics->pgfault);
    printf("    - Major: %" PRIu64 "\n", metrics->pgmajfault);
}

static void print_io_metrics(const CgroupIoMetrics* metrics) {
    printf("--- I/O Metrics ---\n");
    printf("  Bytes Read: %" PRIu64 " MB\n", metrics->rbytes / (1024 * 1024));
    printf("  Bytes Written: %" PRIu64 " MB\n", metrics->wbytes / (1024 * 1024));
    printf("  Read IOs: %" PRIu64 "\n", metrics->rios);
    printf("  Write IOs: %" PRIu64 "\n", metrics->wios);
}

// --- Main Display Function ---

void display_cgroup_metrics(const char* cgroup_path) {
    char full_path[1024];
    // If the path is relative (doesn't start with /), prepend the default cgroup root.
    if (cgroup_path[0] != '/') {
        snprintf(full_path, sizeof(full_path), "/sys/fs/cgroup/%s", cgroup_path);
    } else {
        strncpy(full_path, cgroup_path, sizeof(full_path) - 1);
        full_path[sizeof(full_path) - 1] = '\0';
    }

    struct stat st;
    if (stat(full_path, &st) != 0 || !S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Error: Invalid cgroup path or path is not a directory: %s\n", full_path);
        return;
    }

    printf("\nDisplaying metrics for cgroup: %s\n\n", full_path);

    CgroupCpuMetrics cpu_metrics = {0};
    CgroupMemoryMetrics mem_metrics = {0};
    CgroupIoMetrics io_metrics = {0};

    if (is_cgroup_v2()) {
        read_cpu_metrics(full_path, &cpu_metrics);
        read_memory_metrics(full_path, &mem_metrics);
        read_io_metrics(full_path, &io_metrics);
    } else {
        // --- V1 Path Handling ---
        const char* controllers[] = {"/cpu/", "/memory/", "/blkio/", "/cpuacct/", "/systemd/"};
        const char* cgroup_name_part = NULL;
        int controller_count = sizeof(controllers) / sizeof(controllers[0]);

        for (int i = 0; i < controller_count; i++) {
            const char* found = strstr(full_path, controllers[i]);
            if (found) {
                cgroup_name_part = found + strlen(controllers[i]);
                break;
            }
        }

        if (!cgroup_name_part) {
            // If no specific controller is in the path, assume the path is the name itself
            // relative to the controller root (e.g. user provides "user.slice")
            cgroup_name_part = full_path;
        }

        char cpu_path[2048], mem_path[2048], io_path[2048];
        snprintf(cpu_path, sizeof(cpu_path), "/sys/fs/cgroup/cpu/%s", cgroup_name_part);
        snprintf(mem_path, sizeof(mem_path), "/sys/fs/cgroup/memory/%s", cgroup_name_part);
        snprintf(io_path, sizeof(io_path), "/sys/fs/cgroup/blkio/%s", cgroup_name_part);

        read_cpu_metrics(cpu_path, &cpu_metrics);
        read_memory_metrics(mem_path, &mem_metrics);
        read_io_metrics(io_path, &io_metrics);
    }

    print_cpu_metrics(&cpu_metrics);
    printf("\n");
    print_memory_metrics(&mem_metrics);
    printf("\n");
    print_io_metrics(&io_metrics);
}

int create_cgroup(const char* cgroup_name) {
    // Basic validation for the cgroup name
    if (cgroup_name == NULL || strlen(cgroup_name) == 0 || strchr(cgroup_name, '/') != NULL) {
        fprintf(stderr, "Error: Invalid cgroup name. It cannot be empty or contain a '/'.\n");
        errno = EINVAL;
        return -1;
    }

    if (is_cgroup_v2()) {
        char path[2048];
        snprintf(path, sizeof(path), "/sys/fs/cgroup/%s", cgroup_name);
        if (mkdir(path, 0755) != 0) {
            return -1; // errno is set by mkdir
        }
    } else {
        // For v1, create the directory under each main controller
        const char* controllers[] = {"cpu", "memory", "blkio"};
        int num_controllers = sizeof(controllers) / sizeof(controllers[0]);
        for (int i = 0; i < num_controllers; i++) {
            char path[2048];
            snprintf(path, sizeof(path), "/sys/fs/cgroup/%s/%s", controllers[i], cgroup_name);
            // Check if controller exists before trying to create the directory
            char controller_path[1024];
            snprintf(controller_path, sizeof(controller_path), "/sys/fs/cgroup/%s", controllers[i]);
            struct stat st;
            if (stat(controller_path, &st) == 0 && S_ISDIR(st.st_mode)) {
                if (mkdir(path, 0755) != 0) {
                    // If one fails, we should ideally try to clean up the ones we already created.
                    // For now, we'll just return the error.
                    return -1; // errno is set by mkdir
                }
            }
        }
    }
    return 0; // Success
}

/**
 * @brief Helper to write a string value to a file within a cgroup.
 */
static int write_to_cgroup_file(const char* cgroup_path, const char* file, const char* value) {
    char file_path[2048];
    snprintf(file_path, sizeof(file_path), "%s/%s", cgroup_path, file);

    FILE* fp = fopen(file_path, "w");
    if (!fp) {
        return -1; // errno is set by fopen
    }

    if (fprintf(fp, "%s", value) < 0) {
        fclose(fp);
        return -1; // errno is set by fprintf
    }

    fclose(fp);
    return 0;
}

int move_process_to_cgroup(pid_t pid, const char* cgroup_name) {
    char full_path[1024];
    if (cgroup_name[0] != '/') {
        snprintf(full_path, sizeof(full_path), "/sys/fs/cgroup/%s", cgroup_name);
    } else {
        strncpy(full_path, cgroup_name, sizeof(full_path) - 1);
        full_path[sizeof(full_path) - 1] = '\0';
    }

    char pid_str[32];
    snprintf(pid_str, sizeof(pid_str), "%d", pid);

    if (is_cgroup_v2()) {
        return write_to_cgroup_file(full_path, "cgroup.procs", pid_str);
    } else {
        // For v1, write to each controller's cgroup.procs file
        const char* controllers[] = {"/cpu/", "/memory/", "/blkio/", "/cpuacct/", "/systemd/"};
        const char* cgroup_name_part = NULL;
        int controller_count = sizeof(controllers) / sizeof(controllers[0]);

        for (int i = 0; i < controller_count; i++) {
            const char* found = strstr(full_path, controllers[i]);
            if (found) {
                cgroup_name_part = found + strlen(controllers[i]);
                break;
            }
        }

        if (!cgroup_name_part) {
            cgroup_name_part = full_path;
        }

        // Write to all controllers that exist
        int result = 0;
        for (int i = 0; i < 3; i++) { // Only cpu, memory, blkio matter for moving process
            char controller_path[2048];
            char final_cgroup_path[4096];
            snprintf(controller_path, sizeof(controller_path), "/sys/fs/cgroup/%s", controllers[i]);
            snprintf(final_cgroup_path, sizeof(final_cgroup_path), "%s%s", controller_path, cgroup_name_part);

            struct stat st;
            if (stat(final_cgroup_path, &st) == 0 && S_ISDIR(st.st_mode)) {
                if (write_to_cgroup_file(final_cgroup_path, "cgroup.procs", pid_str) != 0) {
                    result = -1; // Continue trying others, but report failure
                }
            }
        }
        return result;
    }
}

int apply_cpu_limit(const char* cgroup_name, int percentage) {
    char full_path[1024];
    if (cgroup_name[0] != '/') {
        snprintf(full_path, sizeof(full_path), "/sys/fs/cgroup/%s", cgroup_name);
    } else {
        strncpy(full_path, cgroup_name, sizeof(full_path) - 1);
        full_path[sizeof(full_path) - 1] = '\0';
    }

    if (is_cgroup_v2()) {
        long long quota = 100000 * percentage / 100;
        char limit_str[64];
        snprintf(limit_str, sizeof(limit_str), "%lld 100000", quota);
        return write_to_cgroup_file(full_path, "cpu.max", limit_str);
    } else {
        long long quota = 100000 * percentage / 100;
        char quota_str[32];
        snprintf(quota_str, sizeof(quota_str), "%lld", quota);
        
        char period_str[32];
        snprintf(period_str, sizeof(period_str), "100000");

        char cpu_path[2048];
        snprintf(cpu_path, sizeof(cpu_path), "/sys/fs/cgroup/cpu/%s", cgroup_name);

        if (write_to_cgroup_file(cpu_path, "cpu.cfs_period_us", period_str) != 0) {
            return -1;
        }
        return write_to_cgroup_file(cpu_path, "cpu.cfs_quota_us", quota_str);
    }
}

int apply_memory_limit(const char* cgroup_name, long long bytes, long long swap_bytes) {
    char full_path[1024];
    if (cgroup_name[0] != '/') {
        snprintf(full_path, sizeof(full_path), "/sys/fs/cgroup/%s", cgroup_name);
    } else {
        strncpy(full_path, cgroup_name, sizeof(full_path) - 1);
        full_path[sizeof(full_path) - 1] = '\0';
    }

    char limit_str[32];
    int result = 0; // Assume success initially

    // Apply main memory limit
    snprintf(limit_str, sizeof(limit_str), "%lld", bytes);
    if (is_cgroup_v2()) {
        if (write_to_cgroup_file(full_path, "memory.max", limit_str) != 0) {
            result = -1;
        }
    } else {
        char mem_path[2048];
        snprintf(mem_path, sizeof(mem_path), "/sys/fs/cgroup/memory/%s", cgroup_name);
        if (write_to_cgroup_file(mem_path, "memory.limit_in_bytes", limit_str) != 0) {
            result = -1;
        }
    }

    // Apply swap memory limit, if provided (swap_bytes >= 0 indicates a limit is desired)
    if (swap_bytes >= 0) {
        snprintf(limit_str, sizeof(limit_str), "%lld", swap_bytes);
        if (is_cgroup_v2()) {
            if (write_to_cgroup_file(full_path, "memory.swap.max", limit_str) != 0) {
                result = -1; // Report failure for swap, even if main memory worked
            }
        } else {
            char memsw_path[2048];
            snprintf(memsw_path, sizeof(memsw_path), "/sys/fs/cgroup/memory/%s", cgroup_name); // memsw is under memory controller
            if (write_to_cgroup_file(memsw_path, "memory.memsw.limit_in_bytes", limit_str) != 0) {
                result = -1;
            }
        }
    }

    return result;
}

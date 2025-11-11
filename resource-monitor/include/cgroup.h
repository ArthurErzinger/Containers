#ifndef CGROUP_H
#define CGROUP_H

#include <stdint.h>
#include <sys/types.h>

// Struct to hold CPU metrics from a cgroup
typedef struct {
    // From cpuacct.usage
    uint64_t usage_ns; // Total CPU time consumed by this cgroup (in nanoseconds)

    // From cpu.stat
    uint64_t nr_periods;     // Number of enforcement intervals that have occurred
    uint64_t nr_throttled;   // Number of times the cgroup has been throttled
    uint64_t throttled_ns;   // Total time the cgroup has been throttled (in nanoseconds)

    // Limits
    int64_t cpu_quota_us;
    int64_t cpu_period_us;
} CgroupCpuMetrics;

// Struct to hold Memory metrics from a cgroup
typedef struct {
    // From memory.current
    uint64_t current; // Current memory usage in bytes

    // From memory.stat
    uint64_t anon;           // Amount of memory used in anonymous mappings
    uint64_t file;           // Amount of memory used to cache filesystem data
    uint64_t kernel_stack;   // Amount of memory used by kernel stacks
    uint64_t slab;           // Amount of memory used by the slab allocator
    uint64_t sock;           // Amount of memory used by TCP/IP sockets
    uint64_t pgfault;        // Total number of page faults
    uint64_t pgmajfault;     // Number of major page faults

    // Limit
    int64_t memory_limit_bytes;
} CgroupMemoryMetrics;

// Struct to hold Block I/O metrics from a cgroup
typedef struct {
    // From io.stat
    uint64_t rbytes; // Total bytes read
    uint64_t wbytes; // Total bytes written
    uint64_t rios;   // Total read I/O operations
    uint64_t wios;   // Total write I/O operations
} CgroupIoMetrics;


/**
 * @brief Reads all relevant metrics for a given cgroup path and displays them.
 *
 * @param cgroup_path The path to the cgroup provided by the user.
 *                    This function will handle path resolution for both
 *                    cgroup v1 and v2 hierarchies.
 */
void display_cgroup_metrics(const char* cgroup_path);

/**
 * @brief Creates a new cgroup.
 *
 * @param cgroup_name The name of the cgroup to create (e.g., "my-group").
 * @return 0 on success, -1 on failure (and sets errno).
 */
int create_cgroup(const char* cgroup_name);

/**
 * @brief Moves a process to a cgroup.
 *
 * @param pid The ID of the process to move.
 * @param cgroup_name The name/path of the target cgroup.
 * @return 0 on success, -1 on failure (and sets errno).
 */
int move_process_to_cgroup(pid_t pid, const char* cgroup_name);

/**
 * @brief Applies a CPU limit to a cgroup.
 *
 * @param cgroup_name The name/path of the target cgroup.
 * @param percentage The CPU limit in percentage (e.g., 50 for 50%).
 * @return 0 on success, -1 on failure.
 */
int apply_cpu_limit(const char* cgroup_name, int percentage);

/**
 * @brief Applies a memory limit to a cgroup.
 *
 * @param cgroup_name The name/path of the target cgroup.
 * @param bytes The memory limit in bytes.
 * @return 0 on success, -1 on failure.
 */
int apply_memory_limit(const char* cgroup_name, long long bytes, long long swap_bytes);

/**
 * @brief Applies an I/O limit to a cgroup.
 *
 * @param cgroup_name The name/path of the target cgroup.
 * @param device The device to apply the limit to (e.g., "8:0").
 * @param read_bps The read limit in bytes per second.
 * @param write_bps The write limit in bytes per second.
 * @return 0 on success, -1 on failure.
 */
int apply_io_limit(const char* cgroup_name, const char* device, long long read_bps, long long write_bps);


#endif //CGROUP_H

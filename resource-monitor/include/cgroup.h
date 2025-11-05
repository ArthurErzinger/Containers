#ifndef CGROUP_H
#define CGROUP_H

#include <stdint.h>

// Struct to hold CPU metrics from a cgroup
typedef struct {
    // From cpuacct.usage
    uint64_t usage_ns; // Total CPU time consumed by this cgroup (in nanoseconds)

    // From cpu.stat
    uint64_t nr_periods;     // Number of enforcement intervals that have occurred
    uint64_t nr_throttled;   // Number of times the cgroup has been throttled
    uint64_t throttled_ns;   // Total time the cgroup has been throttled (in nanoseconds)
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


#endif //CGROUP_H

#ifndef CGROUP_H
#define CGROUP_H

#include <sys/types.h> // For pid_t

typedef enum {
    CGROUP_UNKNOWN = 0,
    CGROUP_V1,
    CGROUP_V2
} CgroupVersion;

// Structs for report generation
typedef struct {
    unsigned long long usage_usec;
    unsigned long long user_usec;
    unsigned long long system_usec;
} CgroupCPU;

typedef struct {
    unsigned long long current_bytes;
    unsigned long long max_bytes;
    unsigned long long swap_bytes;
} CgroupMemory;

typedef struct {
    unsigned long long read_bytes;
    unsigned long long write_bytes;
} CgroupIO;

typedef struct {
    CgroupCPU cpu;
    CgroupMemory memory;
    CgroupIO io;
} CgroupResourceUsage;

typedef struct {
    char* cgroup_id;
    pid_t* pids;
    int pid_count;
    CgroupResourceUsage resource_usage;
} CgroupReport;

CgroupVersion get_cgroup_version();
void list_available_controllers(CgroupVersion version);
char* select_cgroup(); // Returns the selected relative path, or NULL
void display_cgroup_metrics(CgroupVersion version, const char* relative_path);
void generate_cgroup_report(CgroupVersion version, const char* relative_path, const char* output_file);
const char* format_bytes(unsigned long long bytes); // Utility function for human-readable byte sizes
const char* format_microseconds(unsigned long long usec); // Utility function for human-readable microsecond timings
int create_cgroup(CgroupVersion version, const char* parent_cgroup_path, const char* cgroup_name);
int apply_resource_limits(CgroupVersion version, const char* relative_cgroup_path);
int move_process_to_cgroup(CgroupVersion version, pid_t pid, const char* relative_cgroup_path);

#endif //CGROUP_H

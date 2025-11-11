#ifndef CGROUP_H
#define CGROUP_H

#include <sys/types.h> // For pid_t

typedef enum {
    CGROUP_UNKNOWN = 0,
    CGROUP_V1,
    CGROUP_V2
} CgroupVersion;

CgroupVersion get_cgroup_version();
void list_available_controllers(CgroupVersion version);
char* select_cgroup(); // Returns the selected relative path, or NULL
void display_cgroup_metrics(CgroupVersion version, const char* relative_path);
const char* format_bytes(unsigned long long bytes); // Utility function for human-readable byte sizes
const char* format_microseconds(unsigned long long usec); // Utility function for human-readable microsecond timings
int create_cgroup(CgroupVersion version, const char* parent_cgroup_path, const char* cgroup_name);
int apply_resource_limits(CgroupVersion version, const char* relative_cgroup_path);
int move_process_to_cgroup(CgroupVersion version, pid_t pid, const char* relative_cgroup_path);

#endif //CGROUP_H

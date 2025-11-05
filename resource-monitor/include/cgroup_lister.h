#ifndef CGROUP_LISTER_H
#define CGROUP_LISTER_H

/**
 * @brief Lists all cgroups found for the main controllers in a readable format.
 *
 * This function scans the cgroup filesystem (typically /sys/fs/cgroup) for
 * the cpu, memory, and blkio controllers. It then recursively lists all
 * cgroup directories found under each controller, printing them to standard
 * output in a structured and human-readable tree format.
 */
void list_all_cgroups();

#endif //CGROUP_LISTER_H

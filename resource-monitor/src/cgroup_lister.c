#include "cgroup_lister.h"
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

// Forward declaration
static void print_cgroups_recursive(const char *base_path, int level);

/**
 * @brief Checks if the system is using cgroup v2 unified hierarchy.
 * @return 1 if cgroup v2 is detected, 0 otherwise.
 */
static int is_cgroup_v2() {
    // cgroup v2 has a single unified hierarchy, and its root contains 'cgroup.controllers'.
    return access("/sys/fs/cgroup/cgroup.controllers", F_OK) == 0;
}

/**
 * @brief Lists cgroups for a cgroup v1 legacy hierarchy.
 */
static void list_cgroups_v1() {
    const char *controllers[] = {"cpu", "memory", "blkio", "systemd"};
    int num_controllers = sizeof(controllers) / sizeof(controllers[0]);

    printf("Detected Cgroup v1 legacy hierarchy.\n\n");

    for (int i = 0; i < num_controllers; i++) {
        char path[512];
        snprintf(path, sizeof(path), "/sys/fs/cgroup/%s", controllers[i]);
        
        // Check if the controller directory exists before trying to list it
        struct stat st;
        if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
            printf("--- %s Cgroups ---\n", controllers[i]);
            print_cgroups_recursive(path, 0);
            printf("\n");
        }
    }
}

/**
 * @brief Lists cgroups for a cgroup v2 unified hierarchy.
 */
static void list_cgroups_v2() {
    printf("Detected Cgroup v2 unified hierarchy.\n\n");
    printf("--- Unified Hierarchy ---\n");
    print_cgroups_recursive("/sys/fs/cgroup", 0);
    printf("\n");
}


void list_all_cgroups() {
    printf("Listing available cgroups:\n");
    if (is_cgroup_v2()) {
        list_cgroups_v2();
    } else {
        list_cgroups_v1();
    }
}

/**
 * @brief Helper function to recursively scan and print cgroup directories.
 */
static void print_cgroups_recursive(const char *base_path, int level) {
    DIR *dir = opendir(base_path);
    if (!dir) {
        // This can happen due to permissions, so we don't treat it as a fatal error.
        // If it's the top-level call, we might want to print a warning.
        if (level == 0) {
            fprintf(stderr, "Warning: Could not open directory %s. Permissions might be insufficient.\n", base_path);
        }
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        // Skip '.' and '..' directories
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // On v2, cgroup.controllers and other files are not cgroups themselves.
        // We are only interested in directories.
        if (entry->d_type != DT_DIR) {
            continue;
        }

        // Indent based on the level in the hierarchy
        for (int i = 0; i < level; i++) {
            printf("  ");
        }
        printf("|- %s\n", entry->d_name);

        // Recurse into the subdirectory
        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", base_path, entry->d_name);
        print_cgroups_recursive(path, level + 1);
    }

    closedir(dir);
}
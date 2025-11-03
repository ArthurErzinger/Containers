#define _DEFAULT_SOURCE
#include "process_tree.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>

PidList find_child_pids(pid_t parent_pid) {
    PidList list = { .pids = NULL, .count = 0 };
    DIR *proc_dir = opendir("/proc");
    if (!proc_dir) {
        perror("Erro ao abrir /proc");
        return list;
    }

    struct dirent *entry;
    pid_t *children = NULL;
    int capacity = 10;
    children = malloc(capacity * sizeof(pid_t));
    if (!children) {
        closedir(proc_dir);
        return list;
    }

    while ((entry = readdir(proc_dir)) != NULL) {
        // Verifica se a entrada é um diretório numérico (um PID)
        if (entry->d_type == DT_DIR) {
            char *endptr;
            long pid_long = strtol(entry->d_name, &endptr, 10);
            if (*endptr == '\0') { // É um número
                pid_t current_pid = (pid_t)pid_long;
                char stat_path[256];
                snprintf(stat_path, sizeof(stat_path), "/proc/%s/stat", entry->d_name);

                FILE *fp = fopen(stat_path, "r");
                if (fp) {
                    pid_t ppid = 0;
                    // O PPID é o 4º campo no arquivo stat
                    if (fscanf(fp, "%*d %*s %*c %d", &ppid) == 1) {
                        if (ppid == parent_pid) {
                            if (list.count >= capacity) {
                                capacity *= 2;
                                pid_t *new_children = realloc(children, capacity * sizeof(pid_t));
                                if (!new_children) {
                                    free(children);
                                    fclose(fp);
                                    closedir(proc_dir);
                                    list.count = 0;
                                    list.pids = NULL;
                                    return list;
                                }
                                children = new_children;
                            }
                            children[list.count++] = current_pid;
                        }
                    }
                    fclose(fp);
                }
            }
        }
    }

    closedir(proc_dir);
    list.pids = children;
    return list;
}

void free_pid_list(PidList *pid_list) {
    if (pid_list && pid_list->pids) {
        free(pid_list->pids);
        pid_list->pids = NULL;
        pid_list->count = 0;
    }
}

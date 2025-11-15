#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>      // Para isdigit()
#include <sys/stat.h>   // Para stat()

#include "namespace.h"

// Helper function to initialize NamespaceInfo
static void init_namespace_info(NamespaceInfo *ns_info, const char *type, ino_t inode) {
    strncpy(ns_info->type, type, sizeof(ns_info->type) - 1);
    ns_info->type[sizeof(ns_info->type) - 1] = '\0';
    ns_info->inode = inode;
    snprintf(ns_info->path, sizeof(ns_info->path), "%s:[%lu]", type, (unsigned long)inode);
    ns_info->pids = NULL;
    ns_info->pid_count = 0;
    ns_info->pid_capacity = 0;
}

// Helper function to add a PID to NamespaceInfo
static void add_pid_to_namespace_info(NamespaceInfo *ns_info, pid_t pid) {
    if (ns_info->pid_count == ns_info->pid_capacity) {
        ns_info->pid_capacity = (ns_info->pid_capacity == 0) ? 4 : ns_info->pid_capacity * 2;
        pid_t *new_pids = realloc(ns_info->pids, ns_info->pid_capacity * sizeof(pid_t));
        if (new_pids == NULL) {
            perror("Failed to reallocate pids");
            exit(EXIT_FAILURE);
        }
        ns_info->pids = new_pids;
    }
    ns_info->pids[ns_info->pid_count++] = pid;
}

// Helper function to free NamespaceInfo resources
static void free_namespace_info(NamespaceInfo *ns_info) {
    free(ns_info->pids);
    ns_info->pids = NULL;
    ns_info->pid_count = 0;
    ns_info->pid_capacity = 0;
}

void list_process_namespaces(pid_t pid) {
    char ns_path[256];
    snprintf(ns_path, sizeof(ns_path), "/proc/%d/ns", pid);

    DIR *dir = opendir(ns_path);
    if (dir == NULL) {
        fprintf(stderr, "Erro ao abrir %s: ", ns_path);
        perror(NULL);
        return;
    }

    printf("┌──────────────────────┬────────────────────────────────┐\n");
    printf("│ Namespaces para o processo PID %-8d               │\n", pid);
    printf("├──────────────────────┼────────────────────────────────┤\n");
    printf("│ TIPO                 │ ID (INODE)                     │\n");
    printf("├──────────────────────┼────────────────────────────────┤\n");

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char link_path[512];
        char link_target[256];
        snprintf(link_path, sizeof(link_path), "%s/%s", ns_path, entry->d_name);

        ssize_t len = readlink(link_path, link_target, sizeof(link_target) - 1);
        if (len != -1) {
            link_target[len] = '\0';
            printf("│ %-20s │ %-30s │\n", entry->d_name, link_target);
        }
    }
    printf("└──────────────────────┴────────────────────────────────┘\n");

    closedir(dir);
}

// Função auxiliar para verificar se um nome de diretório é um PID (contém apenas dígitos)
static int is_pid_dir(const char *name) {
    for (size_t i = 0; name[i] != '\0'; i++) {
        if (!isdigit(name[i])) {
            return 0; // Não é um PID
        }
    }
    return 1; // É um PID
}

void find_processes_in_namespace(void) {
    char ns_type[32];
    long target_inode;
    int choice;

    printf("\nSelecione o tipo de namespace para procurar:\n");
    printf("  [1] mnt\n");
    printf("  [2] uts\n");
    printf("  [3] ipc\n");
    printf("  [4] pid\n");
    printf("  [5] net\n");
    printf("  [6] user\n");
    printf("  [7] cgroup\n");
    printf("Opção: ");

    if (scanf("%d", &choice) != 1) {
        while (getchar() != '\n');
        fprintf(stderr, "Entrada inválida.\n");
        return;
    }
    while (getchar() != '\n');

    switch (choice) {
        case 1: strcpy(ns_type, "mnt"); break;
        case 2: strcpy(ns_type, "uts"); break;
        case 3: strcpy(ns_type, "ipc"); break;
        case 4: strcpy(ns_type, "pid"); break;
        case 5: strcpy(ns_type, "net"); break;
        case 6: strcpy(ns_type, "user"); break;
        case 7: strcpy(ns_type, "cgroup"); break;
        default:
            fprintf(stderr, "Opção de namespace inválida.\n");
            return;
    }

    printf("Digite o inode do namespace '%s' que deseja encontrar: ", ns_type);
    if (scanf("%ld", &target_inode) != 1) {
        while (getchar() != '\n');
        fprintf(stderr, "Entrada de inode inválida.\n");
        return;
    }
    while (getchar() != '\n');

    printf("\nProcurando por processos no namespace '%s' com inode %ld...\n", ns_type, target_inode);

    DIR *proc_dir = opendir("/proc");
    if (proc_dir == NULL) {
        perror("Erro ao abrir /proc");
        return;
    }

    printf("PIDs encontrados:\n");
    int found_count = 0;
    struct dirent *entry;
    while ((entry = readdir(proc_dir)) != NULL) {
        // Verifica se a entrada é um diretório e se o nome é um número (PID)
        if (is_pid_dir(entry->d_name)) {
            char path[512];
            snprintf(path, sizeof(path), "/proc/%s/ns/%s", entry->d_name, ns_type);

            struct stat sb;
            // Tenta obter o stat do link do namespace
            if (stat(path, &sb) == 0) {
                // Compara o inode com o alvo
                if (sb.st_ino == (ino_t)target_inode) {
                    printf("- %s\n", entry->d_name);
                    found_count++;
                }
            }
            // Ignora erros de stat (processo pode ter terminado, permissão negada, etc.)
        }
    }

    closedir(proc_dir);

    if (found_count == 0) {
        printf("Nenhum processo encontrado para este namespace e inode.\n");
    } else {
        printf("\nTotal de %d processo(s) encontrado(s).\n", found_count);
    }
}

void compare_process_namespaces(pid_t pid1, pid_t pid2) {
    const char *ns_types[] = {"mnt", "uts", "ipc", "pid", "net", "user", "cgroup", NULL};
    char path1[512], path2[512];
    struct stat stat1, stat2;

    printf("\nComparando Namespaces entre PID %d e PID %d\n", pid1, pid2);
    printf("┌──────────┬──────────────┬──────────────┬────────────────┐\n");
    printf("│ TIPO     │ PID %-8d │ PID %-8d │ COMPARTILHADO? │\n", pid1, pid2);
    printf("├──────────┼──────────────┼──────────────┼────────────────┤\n");

    for (int i = 0; ns_types[i] != NULL; i++) {
        snprintf(path1, sizeof(path1), "/proc/%d/ns/%s", pid1, ns_types[i]);
        snprintf(path2, sizeof(path2), "/proc/%d/ns/%s", pid2, ns_types[i]);

        int stat1_ok = (stat(path1, &stat1) == 0);
        int stat2_ok = (stat(path2, &stat2) == 0);

        printf("│ %-8s │", ns_types[i]);

        if (stat1_ok) {
            printf(" %-12lu │", (unsigned long)stat1.st_ino);
        } else {
            printf(" %-12s │", "N/A");
        }

        if (stat2_ok) {
            printf(" %-12lu │", (unsigned long)stat2.st_ino);
        } else {
            printf(" %-12s │", "N/A");
        }

        if (stat1_ok && stat2_ok) {
            if (stat1.st_ino == stat2.st_ino) {
                printf(" Sim            │\n");
            } else {
                printf(" Não            │\n");
            }
        } else {
            printf(" N/A            │\n");
        }
    }

    printf("└──────────┴──────────────┴──────────────┴────────────────┘\n");
}

void generate_json_namespace_report(void) {
    const char *filename = "report.json";
    FILE *file = fopen(filename, "w");
    if (file == NULL) {
        perror("Erro ao criar o arquivo de relatório");
        return;
    }

    const char *ns_types[] = {"mnt", "uts", "ipc", "pid", "net", "user", "cgroup", NULL};
    NamespaceInfo *unique_namespaces = NULL;
    int unique_ns_count = 0;
    int unique_ns_capacity = 0;

    DIR *proc_dir = opendir("/proc");
    if (proc_dir == NULL) {
        perror("Erro ao abrir /proc");
        fclose(file);
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(proc_dir)) != NULL) {
        if (entry->d_type == DT_DIR && is_pid_dir(entry->d_name)) {
            pid_t current_pid = atoi(entry->d_name);
            char ns_path_dir[512];
            snprintf(ns_path_dir, sizeof(ns_path_dir), "/proc/%d/ns", current_pid);

            for (int i = 0; ns_types[i] != NULL; i++) {
                char ns_file_path[1024];
                snprintf(ns_file_path, sizeof(ns_file_path), "%s/%s", ns_path_dir, ns_types[i]);

                struct stat sb;
                if (stat(ns_file_path, &sb) == 0) {
                    int found = 0;
                    for (int j = 0; j < unique_ns_count; j++) {
                        if (strcmp(unique_namespaces[j].type, ns_types[i]) == 0 && unique_namespaces[j].inode == sb.st_ino) {
                            add_pid_to_namespace_info(&unique_namespaces[j], current_pid);
                            found = 1;
                            break;
                        }
                    }

                    if (!found) {
                        if (unique_ns_count == unique_ns_capacity) {
                            unique_ns_capacity = (unique_ns_capacity == 0) ? 10 : unique_ns_capacity * 2;
                            NamespaceInfo *new_namespaces = realloc(unique_namespaces, unique_ns_capacity * sizeof(NamespaceInfo));
                            if (new_namespaces == NULL) {
                                perror("Failed to reallocate unique_namespaces");
                                closedir(proc_dir);
                                fclose(file);
                                for (int j = 0; j < unique_ns_count; j++) {
                                    free_namespace_info(&unique_namespaces[j]);
                                }
                                free(unique_namespaces);
                                return;
                            }
                            unique_namespaces = new_namespaces;
                        }
                        init_namespace_info(&unique_namespaces[unique_ns_count], ns_types[i], sb.st_ino);
                        add_pid_to_namespace_info(&unique_namespaces[unique_ns_count], current_pid);
                        unique_ns_count++;
                    }
                }
            }
        }
    }
    closedir(proc_dir);

    // Manually generate JSON into the file
    fprintf(file, "[\n");
    for (int i = 0; i < unique_ns_count; i++) {
        fprintf(file, "  {\n");
        fprintf(file, "    \"type\": \"%s\",\n", unique_namespaces[i].type);
        fprintf(file, "    \"inode\": %lu,\n", (unsigned long)unique_namespaces[i].inode);
        fprintf(file, "    \"path\": \"%s\",\n", unique_namespaces[i].path);
        fprintf(file, "    \"pids\": [");
        for (int j = 0; j < unique_namespaces[i].pid_count; j++) {
            fprintf(file, "%d", unique_namespaces[i].pids[j]);
            if (j < unique_namespaces[i].pid_count - 1) {
                fprintf(file, ", ");
            }
        }
        fprintf(file, "]\n");
        fprintf(file, "  }");
        if (i < unique_ns_count - 1) {
            fprintf(file, ",\n");
        } else {
            fprintf(file, "\n");
        }
    }
    fprintf(file, "]\n");
    fclose(file);

    // Free all allocated NamespaceInfo resources
    for (int i = 0; i < unique_ns_count; i++) {
        free_namespace_info(&unique_namespaces[i]);
    }
    free(unique_namespaces);

    printf("Relatório 'report.json' gerado com sucesso.\n");

    // Ask user if they want to see the report
    printf("Deseja exibir o conteúdo do relatório gerado? (s/n): ");
    int user_choice = getchar();
    // Consume the rest of the line, especially the newline character
    while (getchar() != '\n' && getchar() != EOF);

    if (user_choice == 's' || user_choice == 'S') {
        FILE *read_file_ptr = fopen(filename, "r");
        if (read_file_ptr == NULL) {
            perror("Erro ao abrir o arquivo de relatório para leitura");
            return;
        }

        printf("\n--- Conteúdo de %s ---\n", filename);
        char buffer[1024];
        while (fgets(buffer, sizeof(buffer), read_file_ptr) != NULL) {
            printf("%s", buffer);
        }
        printf("\n--- Fim do Relatório ---\n");
        fclose(read_file_ptr);
    }
}

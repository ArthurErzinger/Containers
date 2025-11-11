#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>      // Para isdigit()
#include <sys/stat.h>   // Para stat()

#include "namespace.h"

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

#define MAX_NS_TYPES 7
#define MAX_UNIQUE_INODES 4096 // A reasonable limit for unique inodes

typedef struct {
    ino_t inodes[MAX_UNIQUE_INODES];
    int count;
} UniqueInodes;

static void add_unique_inode(UniqueInodes *unique_inodes, ino_t inode) {
    for (int i = 0; i < unique_inodes->count; i++) {
        if (unique_inodes->inodes[i] == inode) {
            return; // Already exists
        }
    }
    if (unique_inodes->count < MAX_UNIQUE_INODES) {
        unique_inodes->inodes[unique_inodes->count++] = inode;
    }
}

void generate_system_namespace_report(void) {
    const char *ns_types[MAX_NS_TYPES] = {"mnt", "uts", "ipc", "pid", "net", "user", "cgroup"};
    UniqueInodes unique_ns[MAX_NS_TYPES] = {0};

    DIR *proc_dir = opendir("/proc");
    if (proc_dir == NULL) {
        perror("Erro ao abrir /proc");
        return;
    }

    printf("\nGerando relatório de namespaces do sistema...\n");

    struct dirent *entry;
    while ((entry = readdir(proc_dir)) != NULL) {
        if (entry->d_type == DT_DIR && isdigit(entry->d_name[0])) {
            char ns_path[512];
            snprintf(ns_path, sizeof(ns_path), "/proc/%s/ns", entry->d_name);

            for (int i = 0; i < MAX_NS_TYPES; i++) {
                char ns_file_path[1024];
                snprintf(ns_file_path, sizeof(ns_file_path), "%s/%s", ns_path, ns_types[i]);

                struct stat sb;
                if (stat(ns_file_path, &sb) == 0) {
                    add_unique_inode(&unique_ns[i], sb.st_ino);
                }
            }
        }
    }
    closedir(proc_dir);

    printf("┌──────────┬──────────────────────────┐\n");
    printf("│ TIPO     │ Namespaces Únicos Ativos │\n");
    printf("├──────────┼──────────────────────────┤\n");
    for (int i = 0; i < MAX_NS_TYPES; i++) {
        printf("│ %-8s │ %-24d │\n", ns_types[i], unique_ns[i].count);
    }
    printf("└──────────┴──────────────────────────┘\n");
}

#include "cgroup.h"
#include <unistd.h> // Para sysconf
#include <stdio.h>
#include <stdlib.h> // For strdup, free
#include <string.h>
#include <errno.h>
#include <dirent.h> // Required for opendir, readdir, closedir
#include <sys/stat.h> // Required for mkdir
#include <ctype.h> // For isspace

// Utility function for human-readable byte sizes
const char* format_bytes(unsigned long long bytes) {
    static char buffer[32];
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int i = 0;
    double value = (double)bytes;

    while (value >= 1024 && i < 4) {
        value /= 1024;
        i++;
    }
    snprintf(buffer, sizeof(buffer), "%.2f %s", value, units[i]);
    return buffer;
}

// Utility function for human-readable microsecond timings
const char* format_microseconds(unsigned long long usec) {
    static char buffer[32];
    double seconds = (double)usec / 1000000.0;
    snprintf(buffer, sizeof(buffer), "%.2f s", seconds);
    return buffer;
}

CgroupVersion get_cgroup_version() {
    FILE *fp;
    char line[512]; 
    CgroupVersion version = CGROUP_V1; 

    fp = fopen("/proc/mounts", "r");
    if (fp == NULL) {
        perror("Erro ao abrir /proc/mounts");
        return CGROUP_UNKNOWN;
    }

    while (fgets(line, sizeof(line), fp) != NULL) {
        char device[256], mount_point[256], filesystem_type[256];
        
        if (sscanf(line, "%255s %255s %255s", device, mount_point, filesystem_type) == 3) {
            if (strcmp(mount_point, "/sys/fs/cgroup") == 0 && strcmp(filesystem_type, "cgroup2") == 0) { 
                version = CGROUP_V2;
                break; 
            }
        }
    }

    fclose(fp);
    return version;
}

void list_available_controllers(CgroupVersion version) {
    printf("\n--- Controladores CGroup Disponíveis ---\n");

    if (version == CGROUP_V2) {
        FILE *fp = fopen("/sys/fs/cgroup/cgroup.controllers", "r");
        if (fp == NULL) {
            perror("Erro ao abrir /sys/fs/cgroup/cgroup.controllers");
            printf("Dica: Verifique as permissões ou se o cgroup v2 está corretamente configurado.\n");
            return;
        }
        char line[512];
        if (fgets(line, sizeof(line), fp) != NULL) {
            line[strcspn(line, "\n")] = 0;
            printf("Controladores (v2): %s\n", line);
        } else {
            printf("Nenhum controlador encontrado em /sys/fs/cgroup/cgroup.controllers.\n");
        }
        fclose(fp);
    } else if (version == CGROUP_V1) {
        DIR *d;
        struct dirent *dir;
        char path[512];

        printf("Controladores (v1):\n");
        d = opendir("/sys/fs/cgroup");
        if (d) {
            while ((dir = readdir(d)) != NULL) {
                if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) {
                    continue;
                }
                snprintf(path, sizeof(path), "/sys/fs/cgroup/%s", dir->d_name);
                if (dir->d_type == DT_DIR) {
                    if (strcmp(dir->d_name, "cpu") == 0 ||
                        strcmp(dir->d_name, "cpuacct") == 0 ||
                        strcmp(dir->d_name, "memory") == 0 ||
                        strcmp(dir->d_name, "blkio") == 0 ||
                        strcmp(dir->d_name, "pids") == 0 ||
                        strcmp(dir->d_name, "cpuset") == 0 ||
                        strcmp(dir->d_name, "devices") == 0 ||
                        strcmp(dir->d_name, "freezer") == 0 ||
                        strcmp(dir->d_name, "net_cls") == 0 ||
                        strcmp(dir->d_name, "net_prio") == 0 ||
                        strcmp(dir->d_name, "hugetlb") == 0 ||
                        strcmp(dir->d_name, "perf_event") == 0 ||
                        strcmp(dir->d_name, "rdma") == 0) {
                        printf("  - %s\n", dir->d_name);
                    }
                }
            }
            closedir(d);
        } else {
            perror("Erro ao abrir /sys/fs/cgroup");
            printf("Dica: Verifique as permissões ou se o cgroup v1 está corretamente configurado.\n");
        }
    } else {
        printf("Versão do CGroup desconhecida. Não é possível listar os controladores.\n");
    }
    printf("--------------------------------------\n");
}

static void list_cgroups_recursive(const char *base_path, const char *prefix);

char* select_cgroup() {
    printf("\n===== Lista de CGroups Disponíveis =====\n");
    printf("Caminhos relativos a /sys/fs/cgroup:\n\n");
    printf("/ (root cgroup)\n");

    list_cgroups_recursive("/sys/fs/cgroup", "");

    printf("------------------------------------------\n");
    printf("Digite o caminho relativo do cgroup que deseja inspecionar (ou 'sair'): ");

    char input_buffer[512];
    if (fgets(input_buffer, sizeof(input_buffer), stdin) == NULL) {
        return NULL;
    }

    input_buffer[strcspn(input_buffer, "\n")] = 0;

    if (strcmp(input_buffer, "sair") == 0 || strlen(input_buffer) == 0) {
        return NULL;
    }

    char *selected_path = strdup(input_buffer);
    if (selected_path == NULL) {
        perror("Erro ao alocar memória para o caminho do cgroup");
    }
    return selected_path;
}

static void list_cgroups_recursive(const char *base_path, const char *prefix) {
    DIR *d;
    struct dirent *dir;
    char path[1024];

    d = opendir(base_path);
    if (d == NULL) {
        return;
    }

    while ((dir = readdir(d)) != NULL) {
        if (dir->d_type == DT_DIR) {
            if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) {
                continue;
            }

            snprintf(path, sizeof(path), "%s/%s/cgroup.procs", base_path, dir->d_name);
            FILE *fp = fopen(path, "r");
            if (fp != NULL) {
                fclose(fp);

                printf("%s/%s\n", prefix, dir->d_name);

                snprintf(path, sizeof(path), "%s/%s", base_path, dir->d_name);
                char new_prefix[1024];
                snprintf(new_prefix, sizeof(new_prefix), "%s/%s", prefix, dir->d_name);
                list_cgroups_recursive(path, new_prefix);
            }
        }
    }
    closedir(d);
}

int create_cgroup(CgroupVersion version, const char* parent_cgroup_path, const char* cgroup_name) {
    char full_parent_path[4096];
    char new_cgroup_path[4096];
    char subtree_control_path[4096];
    FILE *fp;

    if (strcmp(parent_cgroup_path, "/") == 0 || strcmp(parent_cgroup_path, "") == 0) {
        snprintf(full_parent_path, sizeof(full_parent_path), "/sys/fs/cgroup");
    } else {
        if (parent_cgroup_path[0] == '/') {
            snprintf(full_parent_path, sizeof(full_parent_path), "/sys/fs/cgroup%s", parent_cgroup_path);
        } else {
            snprintf(full_parent_path, sizeof(full_parent_path), "/sys/fs/cgroup/%s", parent_cgroup_path);
        }
    }

    int ret = snprintf(new_cgroup_path, sizeof(new_cgroup_path), "%s/%s", full_parent_path, cgroup_name);
    if (ret < 0 || (unsigned)ret >= sizeof(new_cgroup_path)) {
        fprintf(stderr, "Erro: O caminho do cgroup resultante é muito longo.\n");
        return -1;
    }

    printf("\nTentando criar cgroup em: %s\n", new_cgroup_path);

    if (mkdir(new_cgroup_path, 0755) == -1) {
        perror("Erro ao criar diretório do cgroup");
        if (errno == EACCES) {
            printf("Dica: A criação de cgroups geralmente requer privilégios de root. Tente executar o programa com 'sudo'.\n");
        }
        return -1;
    }
    printf("Cgroup '%s' criado com sucesso em '%s'.\n", cgroup_name, new_cgroup_path);

    if (version == CGROUP_V2) {
        int ret = snprintf(subtree_control_path, sizeof(subtree_control_path), "%s/cgroup.subtree_control", full_parent_path);
        if (ret < 0 || (unsigned)ret >= sizeof(subtree_control_path)) {
            fprintf(stderr, "Erro: O caminho para cgroup.subtree_control é muito longo.\n");
            return -1;
        }
        printf("Tentando habilitar controladores 'cpu', 'memory' e 'io' no pai: %s\n", subtree_control_path);

        fp = fopen(subtree_control_path, "w");
        if (fp == NULL) {
            perror("Erro ao abrir cgroup.subtree_control do pai para escrita");
            if (errno == EACCES) {
                printf("Dica: Habilitar controladores requer privilégios de root. Tente executar o programa com 'sudo'.\n");
            }
            return 0; 
        }
        if (fprintf(fp, "+cpu +memory +io") < 0) {
            perror("Erro ao escrever em cgroup.subtree_control");
            fclose(fp);
            return -1;
        }
        fclose(fp);
        printf("Controladores 'cpu', 'memory' e 'io' habilitados no cgroup pai '%s'.\n", full_parent_path);
    } else if (version == CGROUP_V1) {
        const char* controllers[] = {"cpu", "memory", "pids", "blkio"};
        int num_controllers = sizeof(controllers) / sizeof(controllers[0]);
        int success_count = 0;

        printf("Criação de Cgroup v1: Criando diretórios nos controladores...\n");

        for (int i = 0; i < num_controllers; i++) {
            char cgroup_path_v1[4096];
            int ret = snprintf(cgroup_path_v1, sizeof(cgroup_path_v1), "/sys/fs/cgroup/%s/%s", controllers[i], cgroup_name);
            if (ret < 0 || (unsigned)ret >= sizeof(cgroup_path_v1)) {
                fprintf(stderr, "Erro: O caminho do cgroup v1 para o controlador '%s' é muito longo.\n", controllers[i]);
                continue;
            }

            if (mkdir(cgroup_path_v1, 0755) == 0) {
                printf("  - Criado: %s\n", cgroup_path_v1);
                success_count++;
            } else if (errno != EEXIST) {
                fprintf(stderr, "  - Falha ao criar %s: %s\n", cgroup_path_v1, strerror(errno));
            } else {
                printf("  - Já existe: %s\n", cgroup_path_v1);
                success_count++; // Consider existing as success for this purpose
            }
        }

        if (success_count == 0) {
            fprintf(stderr, "Não foi possível criar o cgroup v1 em nenhum controlador.\n");
            return -1;
        }
        printf("Criação do cgroup v1 '%s' concluída.\n", cgroup_name);
    }

    return 0;
}

int move_process_to_cgroup(CgroupVersion version, pid_t pid, const char* relative_cgroup_path) {
    char full_cgroup_path[4096];
    char cgroup_procs_path[4096];
    FILE *fp;

    if (strcmp(relative_cgroup_path, "/") == 0 || strcmp(relative_cgroup_path, "") == 0) {
        snprintf(full_cgroup_path, sizeof(full_cgroup_path), "/sys/fs/cgroup");
    } else {
        if (relative_cgroup_path[0] == '/') {
            snprintf(full_cgroup_path, sizeof(full_cgroup_path), "/sys/fs/cgroup%s", relative_cgroup_path);
        } else {
            snprintf(full_cgroup_path, sizeof(full_cgroup_path), "/sys/fs/cgroup/%s", relative_cgroup_path);
        }
    }

    printf("\nTentando mover PID %d para o cgroup: %s\n", pid, full_cgroup_path);

    if (version == CGROUP_V2) {
        int ret = snprintf(cgroup_procs_path, sizeof(cgroup_procs_path), "%s/cgroup.procs", full_cgroup_path);
        if (ret < 0 || (unsigned)ret >= sizeof(cgroup_procs_path)) {
            fprintf(stderr, "Erro: O caminho para cgroup.procs é muito longo.\n");
            return -1;
        }
        fp = fopen(cgroup_procs_path, "w");
        if (fp == NULL) {
            perror("Erro ao abrir cgroup.procs para escrita");
            if (errno == EACCES) {
                printf("Dica: Mover processos para cgroups geralmente requer privilégios de root. Tente executar o programa com 'sudo'.\n");
            } else if (errno == ENOENT) {
                printf("Dica: O cgroup '%s' ou o arquivo 'cgroup.procs' não existe. Verifique o caminho.\n", full_cgroup_path);
            }
            return -1;
        }
        if (fprintf(fp, "%d", pid) < 0) {
            perror("Erro ao escrever PID em cgroup.procs");
            fclose(fp);
            return -1;
        }
        fclose(fp);
        printf("PID %d movido com sucesso para o cgroup '%s'.\n", pid, full_cgroup_path);
    } else if (version == CGROUP_V1) {
        const char* controllers[] = {"cpu", "memory", "pids"}; // Controllers where we'll move the task
        int num_controllers = sizeof(controllers) / sizeof(controllers[0]);
        int success_count = 0;
        char task_path[4096];
        FILE* fp_task;

        printf("Movendo PID %d para cgroup v1 '%s' nos controladores...\n", pid, relative_cgroup_path);

        for (int i = 0; i < num_controllers; i++) {
            int ret = snprintf(task_path, sizeof(task_path), "/sys/fs/cgroup/%s%s/tasks", controllers[i], relative_cgroup_path);
             if (ret < 0 || (unsigned)ret >= sizeof(task_path)) {
                fprintf(stderr, "Erro: O caminho para o arquivo 'tasks' do controlador '%s' é muito longo.\n", controllers[i]);
                continue;
            }

            fp_task = fopen(task_path, "w");
            if (fp_task == NULL) {
                fprintf(stderr, "  - Falha ao abrir %s: %s\n", task_path, strerror(errno));
                continue;
            }

            if (fprintf(fp_task, "%d", pid) < 0) {
                fprintf(stderr, "  - Falha ao escrever PID em %s: %s\n", task_path, strerror(errno));
            } else {
                printf("  - PID %d movido com sucesso em '%s'.\n", pid, controllers[i]);
                success_count++;
            }
            fclose(fp_task);
        }

        if (success_count == 0) {
            fprintf(stderr, "Não foi possível mover o PID para o cgroup v1 em nenhum controlador.\n");
            return -1;
        }
        return 0;
    }

    return 0;
}

void display_cgroup_metrics(CgroupVersion version, const char* relative_path) {
    char full_path[2048]; 
    char file_path[2068]; 
    FILE *fp; 
    char line[512];

    if (strcmp(relative_path, "/") == 0) {
        snprintf(full_path, sizeof(full_path), "/sys/fs/cgroup");
    } else {
        if (relative_path[0] == '/') {
            snprintf(full_path, sizeof(full_path), "/sys/fs/cgroup%s", relative_path);
        } else {
            snprintf(full_path, sizeof(full_path), "/sys/fs/cgroup/%s", relative_path);
        }
    }

    printf("\n--- Métricas para o CGroup: %s ---\n", full_path);
    printf("Versão do CGroup: v%d\n", version);

    if (version == CGROUP_V2) {
        // --- CPU Metrics ---
        printf("\n[CPU]\n");
        snprintf(file_path, sizeof(file_path), "%s/cpu.stat", full_path);
        fp = fopen(file_path, "r");
        if (fp == NULL) {
            perror("Erro ao abrir cpu.stat");
        } else {
            unsigned long long usage_usec = 0, user_usec = 0, system_usec = 0;
            while (fgets(line, sizeof(line), fp) != NULL) {
                if (sscanf(line, "usage_usec %llu", &usage_usec) == 1) {}
                else if (sscanf(line, "user_usec %llu", &user_usec) == 1) {}
                else if (sscanf(line, "system_usec %llu", &system_usec) == 1) {}
            }
            fclose(fp);
            printf("  Uso Total de CPU: %s\n", format_microseconds(usage_usec));
            printf("  Uso de CPU (Usuário): %s\n", format_microseconds(user_usec));
            printf("  Uso de CPU (Sistema): %s\n", format_microseconds(system_usec));
        }

        // --- Memory Metrics ---
        printf("\n[Memória]\n");
        snprintf(file_path, sizeof(file_path), "%s/memory.current", full_path);
        fp = fopen(file_path, "r");
        if (fp == NULL) {
            perror("Erro ao abrir memory.current");
        } else {
            unsigned long long memory_current = 0;
            if (fscanf(fp, "%llu", &memory_current) == 1) {
                printf("  Uso Atual de Memória: %s\n", format_bytes(memory_current));
            } else {
                printf("  Não foi possível ler o uso atual de memória.\n");
            }
            fclose(fp);
        }

        snprintf(file_path, sizeof(file_path), "%s/memory.stat", full_path);
        fp = fopen(file_path, "r");
        if (fp == NULL) {
            perror("Erro ao abrir memory.stat");
        } else {
            unsigned long long anon = 0, file = 0, kernel_stack = 0, slab = 0, pgfault = 0, pgmajfault = 0;
            while (fgets(line, sizeof(line), fp) != NULL) {
                if (sscanf(line, "anon %llu", &anon) == 1) {}
                else if (sscanf(line, "file %llu", &file) == 1) {}
                else if (sscanf(line, "kernel_stack %llu", &kernel_stack) == 1) {}
                else if (sscanf(line, "slab %llu", &slab) == 1) {}
                else if (sscanf(line, "pgfault %llu", &pgfault) == 1) {}
                else if (sscanf(line, "pgmajfault %llu", &pgmajfault) == 1) {}
            }
            fclose(fp);
            printf("  Memória Anônima: %s\n", format_bytes(anon));
            printf("  Memória de Arquivo: %s\n", format_bytes(file));
            printf("  Pilha do Kernel: %s\n", format_bytes(kernel_stack));
            printf("  Slab: %s\n", format_bytes(slab));
            printf("  Page Faults: %llu\n", pgfault);
            printf("  Major Page Faults: %llu\n", pgmajfault);
        }

        // --- I/O Metrics ---
        printf("\n[I/O]\n");
        snprintf(file_path, sizeof(file_path), "%s/io.stat", full_path);
        fp = fopen(file_path, "r");
        if (fp == NULL) {
            perror("Erro ao abrir io.stat");
        } else {
            char device_name[256];
            unsigned long long rbytes = 0, wbytes = 0, rios = 0, wios = 0;
            printf("  Métricas por Dispositivo:\n");
            while (fgets(line, sizeof(line), fp) != NULL) {
                if (sscanf(line, "%255s rbytes %llu wbytes %llu rios %llu wios %llu", 
                           device_name, &rbytes, &wbytes, &rios, &wios) == 5) {
                    printf("    Dispositivo %s: Lidos %s, Escritos %s, Leituras %llu, Escritas %llu\n",
                           device_name, format_bytes(rbytes), format_bytes(wbytes), rios, wios);
                } else {
                    printf("    %s", line);
                }
            }
            fclose(fp);
        }

    } else if (version == CGROUP_V1) {
        int ret;
        // --- CPU Metrics (v1) ---
        printf("\n[CPU (v1)]\n");
        ret = snprintf(file_path, sizeof(file_path), "/sys/fs/cgroup/cpuacct%s/cpuacct.stat", full_path);
        if (ret < 0 || (unsigned)ret >= sizeof(file_path)) {
            fprintf(stderr, "Erro: O caminho para cpuacct.stat é muito longo.\n");
        } else {
            fp = fopen(file_path, "r");
            if (fp == NULL) {
                perror("Erro ao abrir cpuacct.stat");
            } else {
                unsigned long long user_jiffies = 0, system_jiffies = 0;
                // Nota: O tempo em v1 é medido em 'jiffies' (ticks do clock), não em microssegundos.
                // A conversão exata requer saber o valor de USER_HZ (geralmente 100).
                while (fgets(line, sizeof(line), fp) != NULL) {
                    if (sscanf(line, "user %llu", &user_jiffies) == 1) {}
                    else if (sscanf(line, "system %llu", &system_jiffies) == 1) {}
                }
                fclose(fp);
                printf("  Uso de CPU (Usuário): %llu jiffies\n", user_jiffies);
                printf("  Uso de CPU (Sistema): %llu jiffies\n", system_jiffies);
                printf("  (1 jiffy = 1/%ld s)\n", sysconf(_SC_CLK_TCK));
            }
        }

        // --- Memory Metrics (v1) ---
        printf("\n[Memória (v1)]\n");
        ret = snprintf(file_path, sizeof(file_path), "/sys/fs/cgroup/memory%s/memory.usage_in_bytes", full_path);
        if (ret < 0 || (unsigned)ret >= sizeof(file_path)) {
            fprintf(stderr, "Erro: O caminho para memory.usage_in_bytes é muito longo.\n");
        } else {
            fp = fopen(file_path, "r");
            if (fp == NULL) {
                perror("Erro ao abrir memory.usage_in_bytes");
            } else {
                unsigned long long memory_current = 0;
                if (fscanf(fp, "%llu", &memory_current) == 1) {
                    printf("  Uso Atual de Memória: %s\n", format_bytes(memory_current));
                } else {
                    printf("  Não foi possível ler o uso atual de memória.\n");
                }
                fclose(fp);
            }
        }

        ret = snprintf(file_path, sizeof(file_path), "/sys/fs/cgroup/memory%s/memory.stat", full_path);
        if (ret < 0 || (unsigned)ret >= sizeof(file_path)) {
            fprintf(stderr, "Erro: O caminho para memory.stat é muito longo.\n");
        } else {
            fp = fopen(file_path, "r");
            if (fp == NULL) {
                perror("Erro ao abrir memory.stat");
            } else {
                unsigned long long anon = 0, file = 0, kernel_stack = 0, slab = 0, pgfault = 0, pgmajfault = 0;
                while (fgets(line, sizeof(line), fp) != NULL) {
                    if (sscanf(line, "total_rss %llu", &anon) == 1) {} // Em v1, 'total_rss' é análogo a 'anon'
                    else if (sscanf(line, "total_cache %llu", &file) == 1) {} // Em v1, 'total_cache' é análogo a 'file'
                    else if (sscanf(line, "total_kernel_stack %llu", &kernel_stack) == 1) {}
                    else if (sscanf(line, "total_slab %llu", &slab) == 1) {}
                    else if (sscanf(line, "total_pgfault %llu", &pgfault) == 1) {}
                    else if (sscanf(line, "total_pgmajfault %llu", &pgmajfault) == 1) {}
                }
                fclose(fp);
                printf("  Memória Anônima (RSS): %s\n", format_bytes(anon));
                printf("  Memória de Arquivo (Cache): %s\n", format_bytes(file));
                // Kernel stack e slab não estão sempre disponíveis ou podem ter nomes diferentes.
                // printf("  Pilha do Kernel: %s\n", format_bytes(kernel_stack));
                // printf("  Slab: %s\n", format_bytes(slab));
                printf("  Page Faults: %llu\n", pgfault);
                printf("  Major Page Faults: %llu\n", pgmajfault);
            }
        }

        // --- I/O Metrics (v1) ---
        printf("\n[I/O (v1)]\n");
        ret = snprintf(file_path, sizeof(file_path), "/sys/fs/cgroup/blkio%s/blkio.throttle.io_service_bytes", full_path);
        if (ret < 0 || (unsigned)ret >= sizeof(file_path)) {
            fprintf(stderr, "Erro: O caminho para blkio.throttle.io_service_bytes é muito longo.\n");
        } else {
            fp = fopen(file_path, "r");
            if (fp == NULL) {
                perror("Erro ao abrir blkio.throttle.io_service_bytes");
            } else {
                char device_name[32], type[16];
                unsigned long long bytes = 0;
                printf("  Bytes por Dispositivo:\n");
                while (fscanf(fp, "%s %s %llu", device_name, type, &bytes) == 3) {
                    if (strcmp(type, "Read") == 0) {
                        printf("    Dispositivo %s: Lidos %s\n", device_name, format_bytes(bytes));
                    } else if (strcmp(type, "Write") == 0) {
                        printf("    Dispositivo %s: Escritos %s\n", device_name, format_bytes(bytes));
                    }
                }
                fclose(fp);
            }
        }
    }

    printf("--------------------------------------------------\n");
}

// --- Funções para Limitação de Recursos ---

typedef struct {
    int major;
    int minor;
    char name[32];
} BlockDevice;

#define MAX_DEVICES 32

static int list_block_devices(BlockDevice *devices, int max_devices) {
    FILE *fp = fopen("/proc/partitions", "r");
    if (fp == NULL) {
        perror("Erro ao abrir /proc/partitions");
        return 0;
    }

    char line[256];
    int count = 0;
    // Pular as duas primeiras linhas (cabeçalho)
    fgets(line, sizeof(line), fp);
    fgets(line, sizeof(line), fp);

    printf("\n--- Dispositivos de Bloco Disponíveis ---\n");
    while (fgets(line, sizeof(line), fp) != NULL && count < max_devices) {
        int major, minor;
        long long blocks;
        char name[32];
        if (sscanf(line, "%d %d %lld %s", &major, &minor, &blocks, name) == 4) {
            devices[count].major = major;
            devices[count].minor = minor;
            strncpy(devices[count].name, name, sizeof(devices[count].name) - 1);
            devices[count].name[sizeof(devices[count].name) - 1] = '\0';
            printf("  [%d] %s (%d:%d)\n", count + 1, name, major, minor);
            count++;
        }
    }
    fclose(fp);
    printf("-----------------------------------------\n");
    return count;
}

static int write_to_cgroup_file(const char* file_path, const char* value) {
    FILE *fp = fopen(file_path, "w");
    if (fp == NULL) {
        perror("Erro ao abrir arquivo do cgroup para escrita");
        if (errno == EACCES) {
            printf("Dica: Aplicar limites de recursos requer privilégios de root. Tente executar com 'sudo'.\n");
        } else if (errno == ENOENT) {
            printf("Dica: O arquivo de controle não existe. O controlador pode não estar habilitado.\n");
        }
        return -1;
    }
    if (fprintf(fp, "%s", value) < 0) {
        perror("Erro ao escrever no arquivo do cgroup");
        fclose(fp);
        return -1;
    }
    fclose(fp);
    printf("Limite aplicado com sucesso em '%s'.\n", file_path);
    return 0;
}

int apply_resource_limits(CgroupVersion version, const char* relative_cgroup_path) {
    char full_cgroup_path_base[4096];
    char control_file_path[4096];
    char input_buffer[256];

    // Para v1, relative_cgroup_path é algo como "/my_group", que será anexado ao caminho do controlador.
    // Para v2, é o caminho completo a partir de /sys/fs/cgroup.
    // A lógica de construção do caminho precisa ser diferente.
    if (version == CGROUP_V2) {
        if (strcmp(relative_cgroup_path, "/") == 0 || strcmp(relative_cgroup_path, "") == 0) {
            snprintf(full_cgroup_path_base, sizeof(full_cgroup_path_base), "/sys/fs/cgroup");
        } else {
            if (relative_cgroup_path[0] == '/') {
                snprintf(full_cgroup_path_base, sizeof(full_cgroup_path_base), "/sys/fs/cgroup%s", relative_cgroup_path);
            } else {
                snprintf(full_cgroup_path_base, sizeof(full_cgroup_path_base), "/sys/fs/cgroup/%s", relative_cgroup_path);
            }
        }
        printf("\n--- Aplicando Limites para CGroup v2: %s ---\n", full_cgroup_path_base);
    } else if (version == CGROUP_V1) {
        // Para v1, o caminho base é apenas o nome do grupo, ex: "/my_group"
        strncpy(full_cgroup_path_base, relative_cgroup_path, sizeof(full_cgroup_path_base) - 1);
        full_cgroup_path_base[sizeof(full_cgroup_path_base) - 1] = '\0';
        printf("\n--- Aplicando Limites para CGroup v1: %s ---\n", full_cgroup_path_base);
    } else {
        printf("A aplicação de limites não está implementada para esta versão do cgroup.\n");
        return -1;
    }


    // --- Limite de CPU ---
    printf("\n[CPU] Limite em porcentagem (ex: 20 para 20%% de 1 core). Deixe em branco para ignorar: ");
    if (fgets(input_buffer, sizeof(input_buffer), stdin) && input_buffer[0] != '\n') {
        int cpu_percent = atoi(input_buffer);
        if (cpu_percent > 0) {
            int ret;
            if (version == CGROUP_V2) {
                long max_quota = (long)cpu_percent * 1000; // quota em microssegundos
                long period = 100000; // período de 100ms
                ret = snprintf(control_file_path, sizeof(control_file_path), "%s/cpu.max", full_cgroup_path_base);
                if (ret < 0 || (unsigned)ret >= sizeof(control_file_path)) {
                    fprintf(stderr, "Erro: O caminho para cpu.max é muito longo.\n");
                    return -1; // Retorna para evitar mais erros
                }
                snprintf(input_buffer, sizeof(input_buffer), "%ld %ld", max_quota, period);
                write_to_cgroup_file(control_file_path, input_buffer);
            } else { // CGROUP_V1
                long period = 100000; // 100ms
                long quota = (long)cpu_percent * period / 100;
                ret = snprintf(control_file_path, sizeof(control_file_path), "/sys/fs/cgroup/cpu%s/cpu.cfs_period_us", full_cgroup_path_base);
                 if (ret < 0 || (unsigned)ret >= sizeof(control_file_path)) {
                    fprintf(stderr, "Erro: O caminho para cpu.cfs_period_us é muito longo.\n");
                } else {
                    snprintf(input_buffer, sizeof(input_buffer), "%ld", period);
                    write_to_cgroup_file(control_file_path, input_buffer);
                }

                ret = snprintf(control_file_path, sizeof(control_file_path), "/sys/fs/cgroup/cpu%s/cpu.cfs_quota_us", full_cgroup_path_base);
                if (ret < 0 || (unsigned)ret >= sizeof(control_file_path)) {
                    fprintf(stderr, "Erro: O caminho para cpu.cfs_quota_us é muito longo.\n");
                } else {
                    snprintf(input_buffer, sizeof(input_buffer), "%ld", quota);
                    write_to_cgroup_file(control_file_path, input_buffer);
                }
            }
        }
    }

    // --- Limite de Memória ---
    printf("\n[Memória] Limite em Megabytes (MB). Deixe em branco para ignorar: ");
    if (fgets(input_buffer, sizeof(input_buffer), stdin) && input_buffer[0] != '\n') {
        long memory_mb = atol(input_buffer);
        if (memory_mb > 0) {
            unsigned long long memory_bytes = (unsigned long long)memory_mb * 1024 * 1024;
            if (version == CGROUP_V2) {
                int ret = snprintf(control_file_path, sizeof(control_file_path), "%s/memory.max", full_cgroup_path_base);
                if (ret < 0 || (unsigned)ret >= sizeof(control_file_path)) {
                    fprintf(stderr, "Erro: O caminho para memory.max é muito longo.\n");
                    return -1;
                }
            } else { // CGROUP_V1
                int ret = snprintf(control_file_path, sizeof(control_file_path), "/sys/fs/cgroup/memory%s/memory.limit_in_bytes", full_cgroup_path_base);
                if (ret < 0 || (unsigned)ret >= sizeof(control_file_path)) {
                    fprintf(stderr, "Erro: O caminho para memory.limit_in_bytes é muito longo.\n");
                    return -1;
                }
            }
            snprintf(input_buffer, sizeof(input_buffer), "%llu", memory_bytes);
            write_to_cgroup_file(control_file_path, input_buffer);
        }
    }

    // --- Limite de I/O ---
    printf("\n[I/O] Deseja configurar limites de I/O? (s/n): ");
    if (fgets(input_buffer, sizeof(input_buffer), stdin) && (input_buffer[0] == 's' || input_buffer[0] == 'S')) {
        BlockDevice devices[MAX_DEVICES];
        int device_count = list_block_devices(devices, MAX_DEVICES);
        if (device_count > 0) {
            printf("Escolha o número do dispositivo para aplicar o limite: ");
            if (fgets(input_buffer, sizeof(input_buffer), stdin)) {
                int choice = atoi(input_buffer);
                if (choice > 0 && choice <= device_count) {
                    BlockDevice *target_dev = &devices[choice - 1];
                    
                    // Limite de Leitura (rbps)
                    printf("Limite de LEITURA (rbps) em MB/s para %s. Deixe em branco para ignorar: ", target_dev->name);
                    if (fgets(input_buffer, sizeof(input_buffer), stdin) && input_buffer[0] != '\n') {
                        long rbps_mb = atol(input_buffer);
                        if (rbps_mb > 0) {
                            unsigned long long rbps_bytes = (unsigned long long)rbps_mb * 1024 * 1024;
                            char final_rule[512];
                            snprintf(final_rule, sizeof(final_rule), "%d:%d %llu", target_dev->major, target_dev->minor, rbps_bytes);
                            if (version == CGROUP_V2) {
                                // v2 usa um formato diferente dentro de um único arquivo
                                // Esta parte da v2 é complexa e já estava simplificada, vamos focar na v1
                            } else { // CGROUP_V1
                                int ret = snprintf(control_file_path, sizeof(control_file_path), "/sys/fs/cgroup/blkio%s/blkio.throttle.read_bps_device", full_cgroup_path_base);
                                if (ret < 0 || (unsigned)ret >= sizeof(control_file_path)) {
                                    fprintf(stderr, "Erro: O caminho para blkio.throttle.read_bps_device é muito longo.\n");
                                } else {
                                    write_to_cgroup_file(control_file_path, final_rule);
                                }
                            }
                        }
                    }

                    // Limite de Escrita (wbps)
                    printf("Limite de ESCRITA (wbps) em MB/s para %s. Deixe em branco para ignorar: ", target_dev->name);
                    if (fgets(input_buffer, sizeof(input_buffer), stdin) && input_buffer[0] != '\n') {
                        long wbps_mb = atol(input_buffer);
                        if (wbps_mb > 0) {
                            unsigned long long wbps_bytes = (unsigned long long)wbps_mb * 1024 * 1024;
                            char final_rule[512];
                            snprintf(final_rule, sizeof(final_rule), "%d:%d %llu", target_dev->major, target_dev->minor, wbps_bytes);
                             if (version == CGROUP_V2) {
                                // v2 usa um formato diferente dentro de um único arquivo
                            } else { // CGROUP_V1
                                int ret = snprintf(control_file_path, sizeof(control_file_path), "/sys/fs/cgroup/blkio%s/blkio.throttle.write_bps_device", full_cgroup_path_base);
                                if (ret < 0 || (unsigned)ret >= sizeof(control_file_path)) {
                                    fprintf(stderr, "Erro: O caminho para blkio.throttle.write_bps_device é muito longo.\n");
                                } else {
                                    write_to_cgroup_file(control_file_path, final_rule);
                                }
                            }
                        }
                    }

                    // Lógica original da v2 para I/O era um pouco diferente, esta adaptação foca em v1
                    // e mantém a simplicidade para v2. A lógica v2 original foi removida para evitar confusão
                    // e pode ser reimplementada de forma mais robusta se necessário.

                } else {
                    fprintf(stderr, "Escolha de dispositivo inválida.\n");
                }
            }
        } else {
            printf("Nenhum dispositivo de bloco encontrado ou erro ao listar.\n");
        }
    }
    
    printf("--------------------------------------------------\n");
    return 0;
}

void generate_cgroup_report(CgroupVersion version, const char* relative_path, const char* output_file) {
    char full_path[2048];
    char file_path[2068];
    FILE *fp;
    char line[512];

    CgroupReport report;
    memset(&report, 0, sizeof(CgroupReport));

    if (strcmp(relative_path, "/") == 0) {
        snprintf(full_path, sizeof(full_path), "/sys/fs/cgroup");
        report.cgroup_id = strdup("cgroup:/");
    } else {
        if (relative_path[0] == '/') {
            snprintf(full_path, sizeof(full_path), "/sys/fs/cgroup%s", relative_path);
        } else {
            snprintf(full_path, sizeof(full_path), "/sys/fs/cgroup/%s", relative_path);
        }
        char cgroup_id_buf[256];
        snprintf(cgroup_id_buf, sizeof(cgroup_id_buf), "cgroup:%s", relative_path);
        report.cgroup_id = strdup(cgroup_id_buf);
    }

    if (version == CGROUP_V2) {
        // --- CPU Metrics ---
        snprintf(file_path, sizeof(file_path), "%s/cpu.stat", full_path);
        fp = fopen(file_path, "r");
        if (fp != NULL) {
            while (fgets(line, sizeof(line), fp) != NULL) {
                if (sscanf(line, "usage_usec %llu", &report.resource_usage.cpu.usage_usec) == 1) {}
                else if (sscanf(line, "user_usec %llu", &report.resource_usage.cpu.user_usec) == 1) {}
                else if (sscanf(line, "system_usec %llu", &report.resource_usage.cpu.system_usec) == 1) {}
            }
            fclose(fp);
        }

        // --- Memory Metrics ---
        snprintf(file_path, sizeof(file_path), "%s/memory.current", full_path);
        fp = fopen(file_path, "r");
        if (fp != NULL) {
            if (fscanf(fp, "%llu", &report.resource_usage.memory.current_bytes) != 1) {
                report.resource_usage.memory.current_bytes = 0;
            }
            fclose(fp);
        }

        // --- I/O Metrics ---
        snprintf(file_path, sizeof(file_path), "%s/io.stat", full_path);
        fp = fopen(file_path, "r");
        if (fp != NULL) {
            char dev[32];
            unsigned long long rbytes = 0, wbytes = 0;
            while (fgets(line, sizeof(line), fp) != NULL) {
                if (sscanf(line, "%s rbytes=%llu wbytes=%llu", dev, &rbytes, &wbytes) == 3) {
                    report.resource_usage.io.read_bytes += rbytes;
                    report.resource_usage.io.write_bytes += wbytes;
                }
            }
            fclose(fp);
        }

    } else if (version == CGROUP_V1) {
        long ticks_per_sec = sysconf(_SC_CLK_TCK);

        // --- CPU Metrics (v1) ---
        snprintf(file_path, sizeof(file_path), "/sys/fs/cgroup/cpuacct%s/cpuacct.stat", relative_path);
        fp = fopen(file_path, "r");
        if (fp != NULL) {
            unsigned long long user_jiffies = 0, system_jiffies = 0;
            while (fgets(line, sizeof(line), fp) != NULL) {
                if (sscanf(line, "user %llu", &user_jiffies) == 1) {}
                else if (sscanf(line, "system %llu", &system_jiffies) == 1) {}
            }
            fclose(fp);
            report.resource_usage.cpu.user_usec = (user_jiffies * 1000000) / ticks_per_sec;
            report.resource_usage.cpu.system_usec = (system_jiffies * 1000000) / ticks_per_sec;
            report.resource_usage.cpu.usage_usec = report.resource_usage.cpu.user_usec + report.resource_usage.cpu.system_usec;
        }

        // --- Memory Metrics (v1) ---
        snprintf(file_path, sizeof(file_path), "/sys/fs/cgroup/memory%s/memory.usage_in_bytes", relative_path);
        fp = fopen(file_path, "r");
        if (fp != NULL) {
            if (fscanf(fp, "%llu", &report.resource_usage.memory.current_bytes) != 1) {
                report.resource_usage.memory.current_bytes = 0;
            }
            fclose(fp);
        }

        // --- I/O Metrics (v1) ---
        snprintf(file_path, sizeof(file_path), "/sys/fs/cgroup/blkio%s/blkio.throttle.io_service_bytes", relative_path);
        fp = fopen(file_path, "r");
        if (fp != NULL) {
            char dev[32], type[16];
            unsigned long long bytes = 0;
            while (fscanf(fp, "%s %s %llu", dev, type, &bytes) == 3) {
                if (strcmp(type, "Read") == 0) {
                    report.resource_usage.io.read_bytes += bytes;
                } else if (strcmp(type, "Write") == 0) {
                    report.resource_usage.io.write_bytes += bytes;
                }
            }
            fclose(fp);
        }
    }

    // --- PIDs ---
    const char* pid_file = (version == CGROUP_V2) ? "cgroup.procs" : "tasks";
    if (version == CGROUP_V1) {
         snprintf(file_path, sizeof(file_path), "/sys/fs/cgroup/cpu%s/%s", relative_path, pid_file);
    } else {
         snprintf(file_path, sizeof(file_path), "%s/%s", full_path, pid_file);
    }

    fp = fopen(file_path, "r");
    if (fp != NULL) {
        pid_t pid;
        int capacity = 10;
        report.pids = malloc(capacity * sizeof(pid_t));
        report.pid_count = 0;
        while (fscanf(fp, "%d", &pid) == 1) {
            if (report.pid_count >= capacity) {
                capacity *= 2;
                report.pids = realloc(report.pids, capacity * sizeof(pid_t));
            }
            report.pids[report.pid_count++] = pid;
        }
        fclose(fp);
    }

    // --- Generate JSON Report ---
    fp = fopen(output_file, "w");
    if (fp == NULL) {
        perror("Erro ao abrir arquivo de relatório para escrita");
        free(report.cgroup_id);
        free(report.pids);
        return;
    }

    fprintf(fp, "[\n");
    fprintf(fp, "  {\n");
    fprintf(fp, "    \"cgroup_id\": \"%s\",\n", report.cgroup_id);
    fprintf(fp, "    \"pids\": [");
    for (int i = 0; i < report.pid_count; i++) {
        fprintf(fp, "%d%s", report.pids[i], (i == report.pid_count - 1) ? "" : ", ");
    }
    fprintf(fp, "],\n");
    fprintf(fp, "    \"resource_usage\": {\n");
    fprintf(fp, "      \"cpu\": {\n");
    fprintf(fp, "        \"usage_usec\": %llu,\n", report.resource_usage.cpu.usage_usec);
    fprintf(fp, "        \"user_usec\": %llu,\n", report.resource_usage.cpu.user_usec);
    fprintf(fp, "        \"system_usec\": %llu\n", report.resource_usage.cpu.system_usec);
    fprintf(fp, "      },\n");
    fprintf(fp, "      \"memory\": {\n");
    fprintf(fp, "        \"current_bytes\": %llu,\n", report.resource_usage.memory.current_bytes);
    fprintf(fp, "        \"max_bytes\": %llu,\n", report.resource_usage.memory.max_bytes);
    fprintf(fp, "        \"swap_bytes\": %llu\n", report.resource_usage.memory.swap_bytes);
    fprintf(fp, "      },\n");
    fprintf(fp, "      \"io\": {\n");
    fprintf(fp, "        \"read_bytes\": %llu,\n", report.resource_usage.io.read_bytes);
    fprintf(fp, "        \"write_bytes\": %llu\n", report.resource_usage.io.write_bytes);
    fprintf(fp, "      }\n");
    fprintf(fp, "    }\n");
    fprintf(fp, "  }\n");
    fprintf(fp, "]\n");

    fclose(fp);
    printf("Relatório do cgroup gerado com sucesso em '%s'.\n", output_file);

    // Free allocated memory
    free(report.cgroup_id);
    free(report.pids);
}

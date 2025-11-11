#include "cgroup.h"
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

    snprintf(new_cgroup_path, sizeof(new_cgroup_path), "%s/%s", full_parent_path, cgroup_name);

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
        snprintf(subtree_control_path, sizeof(subtree_control_path), "%s/cgroup.subtree_control", full_parent_path);
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
        printf("Criação de cgroup v1 não implementada ainda.\n");
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
        snprintf(cgroup_procs_path, sizeof(cgroup_procs_path), "%s/cgroup.procs", full_cgroup_path);
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
        printf("Mover processo para cgroup v1 não implementado ainda.\n");
        return -1;
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
        printf("\n[Implementação futura: Ler e exibir métricas de CPU, Memória e I/O para cgroup v1]\n");
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
    if (version != CGROUP_V2) {
        printf("A aplicação de limites só está implementada para cgroup v2.\n");
        return -1;
    }

    char full_cgroup_path[4096];
    char control_file_path[4096];
    char input_buffer[256];

    if (strcmp(relative_cgroup_path, "/") == 0 || strcmp(relative_cgroup_path, "") == 0) {
        snprintf(full_cgroup_path, sizeof(full_cgroup_path), "/sys/fs/cgroup");
    } else {
        snprintf(full_cgroup_path, sizeof(full_cgroup_path), "/sys/fs/cgroup%s", 
                 relative_cgroup_path[0] == '/' ? relative_cgroup_path : strcat(strcpy(input_buffer, "/"), relative_cgroup_path));
    }

    printf("\n--- Aplicando Limites para CGroup: %s ---\n", full_cgroup_path);

    // --- Limite de CPU ---
    printf("\n[CPU] Limite em porcentagem (ex: 20 para 20%% de 1 core). Deixe em branco para ignorar: ");
    if (fgets(input_buffer, sizeof(input_buffer), stdin) && input_buffer[0] != '\n') {
        int cpu_percent = atoi(input_buffer);
        if (cpu_percent > 0) {
            long max_quota = (long)cpu_percent * 1000; // quota em microssegundos
            long period = 100000; // período de 100ms
            snprintf(control_file_path, sizeof(control_file_path), "%s/cpu.max", full_cgroup_path);
            snprintf(input_buffer, sizeof(input_buffer), "%ld %ld", max_quota, period);
            write_to_cgroup_file(control_file_path, input_buffer);
        }
    }

    // --- Limite de Memória ---
    printf("\n[Memória] Limite em Megabytes (MB). Deixe em branco para ignorar: ");
    if (fgets(input_buffer, sizeof(input_buffer), stdin) && input_buffer[0] != '\n') {
        long memory_mb = atol(input_buffer);
        if (memory_mb > 0) {
            unsigned long long memory_bytes = (unsigned long long)memory_mb * 1024 * 1024;
            snprintf(control_file_path, sizeof(control_file_path), "%s/memory.max", full_cgroup_path);
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
                    char io_limit_str[256] = "";
                    char temp_str[128];

                    printf("Limite de LEITURA (rbps) em MB/s para %s. Deixe em branco para ignorar: ", target_dev->name);
                    if (fgets(input_buffer, sizeof(input_buffer), stdin) && input_buffer[0] != '\n') {
                        long rbps_mb = atol(input_buffer);
                        if (rbps_mb > 0) {
                            unsigned long long rbps_bytes = (unsigned long long)rbps_mb * 1024 * 1024;
                            snprintf(temp_str, sizeof(temp_str), "rbps=%llu", rbps_bytes);
                            strcat(io_limit_str, temp_str);
                        }
                    }

                    printf("Limite de ESCRITA (wbps) em MB/s para %s. Deixe em branco para ignorar: ", target_dev->name);
                    if (fgets(input_buffer, sizeof(input_buffer), stdin) && input_buffer[0] != '\n') {
                        long wbps_mb = atol(input_buffer);
                        if (wbps_mb > 0) {
                            if (strlen(io_limit_str) > 0) strcat(io_limit_str, " ");
                            unsigned long long wbps_bytes = (unsigned long long)wbps_mb * 1024 * 1024;
                            snprintf(temp_str, sizeof(temp_str), "wbps=%llu", wbps_bytes);
                            strcat(io_limit_str, temp_str);
                        }
                    }

                    if (strlen(io_limit_str) > 0) {
                        char final_rule[512];
                        snprintf(final_rule, sizeof(final_rule), "%d:%d %s", target_dev->major, target_dev->minor, io_limit_str);
                        snprintf(control_file_path, sizeof(control_file_path), "%s/io.max", full_cgroup_path);
                        write_to_cgroup_file(control_file_path, final_rule);
                    } else {
                        printf("Nenhum limite de I/O especificado.\n");
                    }
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

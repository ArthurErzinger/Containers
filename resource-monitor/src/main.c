#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <string.h>
#include <time.h>
#include <dirent.h>
#include <ctype.h>

#include "monitor.h"
#include "namespace.h"
#include "cgroup.h"

// --- Funções Utilitárias de UI ---

static void flush_stdin_line(void) {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

// --- Funções de Seleção de Processo (anteriormente em process_selector.c) ---

static int select_process() {
    DIR *dir;
    struct dirent *entry;
    char path[512];
    char comm[256];
    FILE *fp;

    printf("===== Lista de Processos em Execução =====\n");
    printf("%-10s %s\n", "PID", "Comando");
    printf("------------------------------------------\n");

    if ((dir = opendir("/proc")) == NULL) {
        perror("Erro ao abrir /proc");
        return -1;
    }

    while ((entry = readdir(dir)) != NULL) {
        // Verifica se o nome da entrada é um número (PID)
        int is_pid = 1;
        for (char *p = entry->d_name; *p; p++) {
            if (!isdigit(*p)) {
                is_pid = 0;
                break;
            }
        }

        if (is_pid) {
            // Constrói o caminho para o arquivo comm
            snprintf(path, sizeof(path), "/proc/%s/comm", entry->d_name);
            fp = fopen(path, "r");
            if (fp) {
                if (fgets(comm, sizeof(comm), fp) != NULL) {
                    // Remove a nova linha do final do nome do comando
                    comm[strcspn(comm, "\n")] = 0;
                    printf("%-10s %s\n", entry->d_name, comm);
                }
                fclose(fp);
            }
        }
    }
    closedir(dir);

    printf("------------------------------------------\n");
    printf("Digite o PID do processo que deseja monitorar: ");

    int selected_pid = -1;
    if (scanf("%d", &selected_pid) != 1) {
        // Limpa o buffer de entrada em caso de erro
        flush_stdin_line();
        fprintf(stderr, "Entrada inválida. Por favor, insira um número.\n");
        return -1;
    }

    // Limpa o restante da linha (ex: o caractere '\n') que o scanf deixou no buffer
    flush_stdin_line();

    return selected_pid;
}


// --- Funções Utilitárias de UI ---

static void wait_for_enter(void) {
    printf("\nPressione Enter para continuar...");
    fflush(stdout);
    flush_stdin_line();
}

static int kbhit(void) {
    struct termios oldt, newt;
    int ch;
    int oldf;

    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

    ch = getchar();

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf);

    if (ch != EOF) {
        ungetc(ch, stdin);
        return 1;
    }
    return 0;
}

// --- Funções do Profiler (Refatorado) ---

static void write_csv_header(FILE *fp) {
    fprintf(fp, "timestamp,pid,cpu_percentage,read_rate_bps,write_rate_bps,utime_jiffies,stime_jiffies,num_threads,vm_size_kb,vm_rss_kb,vm_swap_kb,min_page_faults,maj_page_faults,voluntary_context_switches,nonvoluntary_context_switches,total_read_bytes,total_write_bytes,read_syscalls,write_syscalls,net_rx_bytes,net_tx_bytes,net_rx_packets,net_tx_packets\n");
}

static void write_csv_data(FILE *fp, pid_t pid, double cpu_percentage, double read_rate_bps, double write_rate_bps, const CpuMetrics *cpu, const MemoryMetrics *mem, const IoMetrics *io, const NetworkMetrics *net) {
    time_t rawtime;
    struct tm *info;
    char timestamp_str[80];

    time(&rawtime);
    info = localtime(&rawtime);
    strftime(timestamp_str, 80, "%Y-%m-%d %H:%M:%S", info);

    fprintf(fp, "%s,%d,%.2f,%.2f,%.2f,%lu,%lu,%ld,%ld,%ld,%ld,%lu,%lu,%ld,%ld,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu\n",
            timestamp_str,
            pid,
            cpu_percentage,
            read_rate_bps,
            write_rate_bps,
            cpu->utime,
            cpu->stime,
            cpu->num_threads,
            mem->vm_size_kb,
            mem->vm_rss_kb,
            mem->vm_swap_kb,
            cpu->minflt,
            cpu->majflt,
            cpu->voluntary_ctxt_switches,
            cpu->nonvoluntary_ctxt_switches,
            io->read_bytes,
            io->write_bytes,
            io->read_syscalls,
            io->write_syscalls,
            net->rx_bytes,
            net->tx_bytes,
            net->rx_packets,
            net->tx_packets);
}

static void run_continuous_monitoring(pid_t pid, int interval, FILE *csv_output) {
    CpuMetrics cpu1, cpu2;
    MemoryMetrics mem;
    IoMetrics io1, io2;
    NetworkMetrics net;
    unsigned long global_cpu1, global_cpu2;
    long ticks_per_sec = sysconf(_SC_CLK_TCK);

    printf("\nIniciando monitoramento... Pressione Enter para parar.\n");
    sleep(1);

    if (csv_output != NULL) {
        write_csv_header(csv_output);
    }

    // Coleta inicial
    if (get_cpu_metrics(pid, &cpu1) != 0 || get_io_metrics(pid, &io1) != 0 || get_global_cpu_time(&global_cpu1) != 0) {
        fprintf(stderr, "Erro ao coletar métricas iniciais para o PID %d. O processo existe e você tem permissão?\n", pid);
        return;
    }

    while (!kbhit()) {
        sleep(interval);

        // Coleta subsequente
        if (get_cpu_metrics(pid, &cpu2) != 0 || get_memory_metrics(pid, &mem) != 0 || get_io_metrics(pid, &io2) != 0 || get_network_metrics(pid, &net) != 0 || get_global_cpu_time(&global_cpu2) != 0) {
            fprintf(stderr, "\nErro ao coletar métricas para o PID %d. O processo pode não existir mais.\n", pid);
            break;
        }

        // --- Cálculos ---
        unsigned long process_ticks_delta = (cpu2.utime + cpu2.stime) - (cpu1.utime + cpu1.stime);
        unsigned long total_cpu_delta = global_cpu2 - global_cpu1;

        double cpu_percentage = 0.0;
        if (total_cpu_delta > 0) {
            cpu_percentage = 100.0 * (double)process_ticks_delta / (double)total_cpu_delta;
        }

        double read_rate_bps = (double)(io2.read_bytes - io1.read_bytes) / interval;
        double write_rate_bps = (double)(io2.write_bytes - io1.write_bytes) / interval;

        // --- Exibição ---
        system("clear");
        printf("Monitorando PID: %d (Intervalo: %ds) | Pressione Enter para parar\n", pid, interval);
        printf("====================================================================\n");
        printf("Uso de CPU: %.2f%% \n", cpu_percentage);
        printf("Taxa de Leitura I/O: %.2f bytes/s\n", read_rate_bps);
        printf("Taxa de Escrita I/O: %.2f bytes/s\n", write_rate_bps);
        printf("--------------------------------------------------------------------\n");

        printf("CPU:\n");
        printf("  - Tempo de Usuário:   %.2f segundos\n", (double)cpu2.utime / ticks_per_sec);
        printf("  - Tempo de Sistema:   %.2f segundos\n", (double)cpu2.stime / ticks_per_sec);
        printf("  - Número de Threads:  %ld\n", cpu2.num_threads);
        printf("  - Trocas de Contexto: %ld (voluntárias), %ld (involuntárias)\n", cpu2.voluntary_ctxt_switches, cpu2.nonvoluntary_ctxt_switches);

        printf("Memória:\n");
        printf("  - Memória Virtual (VmSize): %ld KB\n", mem.vm_size_kb);
        printf("  - Memória Residente (VmRSS): %ld KB\n", mem.vm_rss_kb);
        printf("  - Memória em Swap (VmSwap):  %ld KB\n", mem.vm_swap_kb);
        printf("  - Page Faults:              %lu (minor), %lu (major)\n", cpu2.minflt, cpu2.majflt);

        printf("I/O:\n");
        printf("  - Total Bytes Lidos:    %llu\n", io2.read_bytes);
        printf("  - Total Bytes Escritos:   %llu\n", io2.write_bytes);
        printf("  - Syscalls de Leitura:  %llu\n", io2.read_syscalls);
        printf("  - Syscalls de Escrita:  %llu\n", io2.write_syscalls);
        
        printf("Rede:\n");
        printf("  - Total Bytes Recebidos:    %llu\n", net.rx_bytes);
        printf("  - Total Bytes Transmitidos: %llu\n", net.tx_bytes);
        printf("  - Total Pacotes Recebidos:  %llu\n", net.rx_packets);
        printf("  - Total Pacotes Transmitidos: %llu\n", net.tx_packets);
        printf("====================================================================\n");

        if (csv_output != NULL) {
            write_csv_data(csv_output, pid, cpu_percentage, read_rate_bps, write_rate_bps, &cpu2, &mem, &io2, &net);
        }

        // Prepara para a próxima iteração
        cpu1 = cpu2;
        io1 = io2;
        global_cpu1 = global_cpu2;
    }
    flush_stdin_line(); // Limpa o Enter que parou o loop

    if (csv_output != NULL) {
        fclose(csv_output);
        printf("\nDados exportados para CSV com sucesso!\n");
    }
}


// --- Funções de Menu ---

void print_main_menu() {
    printf("\n===========================\n");
    printf(" RESOURCE MONITOR SYSTEM\n");
    printf("===========================\n\n");
    printf("[1] Resource Profiler\n");
    printf("[2] Namespace Analyzer\n");
    printf("[3] Control Group Manager\n");
    printf("[4] Sair\n\n");
}

void print_profiler_submenu() {
    printf("\n  --- [1] Resource Profiler ---\n");
    printf("    ├─ [1] Monitorar Processo (Uso de CPU, I/O, etc.)\n");
    printf("    └─ [2] Voltar ao menu principal\n\n");
}

void print_namespace_submenu() {
    printf("\n  --- [2] Namespace Analyzer ---\n");
    printf("    ├─ [1] Listar namespaces de um processo\n");
    printf("    └─ [2] Voltar ao menu principal\n\n");
}

void print_cgroup_submenu() {
    printf("\n  --- [3] Control Group Manager ---\n");
    printf("    ├─ [1] Listar métricas de um cgroup\n");
    printf("    └─ [2] Voltar ao menu principal\n\n");
}

// --- Handlers de Lógica dos Menus ---

void handle_profiler_menu() {
    int choice = 0;
    while (choice != 2) {
        print_profiler_submenu();
        printf("Opção do Profiler: ");
        if (scanf("%d", &choice) != 1) {
            flush_stdin_line();
            choice = 0;
            continue;
        }
        flush_stdin_line();

        if (choice == 1) {
            int selected_pid = select_process();
            if (selected_pid != -1) {
                int interval = 3;
                printf("Digite o intervalo de monitoramento em segundos (padrão: 3): ");
                char interval_str[10];
                if (fgets(interval_str, sizeof(interval_str), stdin) != NULL) {
                    int user_interval = atoi(interval_str);
                    if (user_interval > 0) {
                        interval = user_interval;
                    }
                }

                char export_choice;
                char filename[256] = {0};
                FILE *csv_file = NULL;

                printf("Deseja exportar os dados para um arquivo CSV? (s/n): ");
                if (scanf(" %c", &export_choice) == 1) {
                    flush_stdin_line();
                    if (export_choice == 's' || export_choice == 'S') {
                        printf("Digite o nome do arquivo CSV (ex: metricas.csv): ");
                        if (fgets(filename, sizeof(filename), stdin) != NULL) {
                            filename[strcspn(filename, "\n")] = 0; // Remove newline
                            if(strlen(filename) > 0) {
                                csv_file = fopen(filename, "w");
                                if (csv_file == NULL) {
                                    perror("Erro ao abrir o arquivo CSV para escrita");
                                }
                            }
                        }
                    }
                } else {
                    flush_stdin_line();
                }

                run_continuous_monitoring(selected_pid, interval, csv_file);
            }
        } else if (choice != 2) {
            printf("Opção inválida.\n");
            sleep(1);
        }
    }
}

void handle_namespace_menu() {
    int choice = 0;
    while (choice != 2) {
        print_namespace_submenu();
        printf("Opção do Namespace Analyzer: ");
        if (scanf("%d", &choice) != 1) {
            flush_stdin_line();
            choice = 0;
            continue;
        }
        flush_stdin_line();

        if (choice == 1) {
             int pid = select_process();
             if (pid != -1) {
                // list_namespaces_for_pid(pid);
                printf("\n[AVISO] Funcionalidade ainda não implementada.\n");
                wait_for_enter();
             }
        } else if (choice != 2) {
            printf("Opção inválida.\n");
            sleep(1);
        }
    }
}

void handle_cgroup_menu() {
    int choice = 0;
    while (choice != 2) {
        print_cgroup_submenu();
        printf("Opção do CGroup Manager: ");
        if (scanf("%d", &choice) != 1) {
            flush_stdin_line();
            choice = 0;
            continue;
        }
        flush_stdin_line();

        if (choice == 1) {
            char cgroup_path[512];
            printf("Digite o caminho do cgroup (ex: /sys/fs/cgroup/memory/my-group): ");
            if (fgets(cgroup_path, sizeof(cgroup_path), stdin) != NULL) {
                cgroup_path[strcspn(cgroup_path, "\n")] = 0; // Remove newline
                // display_cgroup_metrics(cgroup_path);
                printf("\n[AVISO] Funcionalidade ainda não implementada.\n");
                wait_for_enter();
            }
        } else if (choice != 2) {
            printf("Opção inválida.\n");
            sleep(1);
        }
    }
}

// --- Função Principal ---

int main() {
    int choice = 0;
    while (choice != 4) {
        print_main_menu();
        printf("Opção principal: ");
        if (scanf("%d", &choice) != 1) {
            flush_stdin_line();
            choice = -1;
        } else {
            flush_stdin_line();
        }

        switch (choice) {
            case 1:
                handle_profiler_menu();
                break;
            case 2:
                handle_namespace_menu();
                break;
            case 3:
                handle_cgroup_menu();
                break;
            case 4:
                printf("Saindo...\n");
                break;
            default:
                printf("Opção inválida. Tente novamente.\n");
                sleep(1);
                break;
        }
    }
    return 0;
}
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <string.h>
#include <time.h>

#include "process_selector.h"
#include "process_monitor.h"
#include "namespace.h"
#include "cgroup.h"
#include "monitor.h" // Para get_global_cpu_time

// --- Funções Utilitárias de UI ---

static void flush_stdin_line(void) {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

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

// --- Funções do Profiler ---

static void write_csv_header(FILE *fp) {
    fprintf(fp, "timestamp,pid,comm,utime,stime,minflt,majflt,num_threads,voluntary_ctxt_switches,nonvoluntary_ctxt_switches,vm_size_kb,vm_rss_kb,vm_swap_kb,rchar,wchar,syscr,syscw,net_rx_bytes,net_tx_bytes,net_rx_packets,net_tx_packets,net_connections,app_uptime_seconds,cpu_percentage,read_rate_bps,write_rate_bps\n");
}

static void write_csv_data(FILE *fp, const ProcessMetrics *metrics, double cpu_percentage, double read_rate_bps, double write_rate_bps) {
    time_t rawtime;
    struct tm *info;
    char timestamp_str[80];

    time(&rawtime);
    info = localtime(&rawtime);
    strftime(timestamp_str, 80, "%Y-%m-%d %H:%M:%S", info);

    fprintf(fp, "%s,%d,%s,%lu,%lu,%lu,%lu,%ld,%ld,%ld,%ld,%ld,%ld,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%ld,%.2f,%.2f,%.2f,%.2f\n",
            timestamp_str, metrics->pid, metrics->comm, metrics->utime, metrics->stime, metrics->minflt, metrics->majflt,
            metrics->num_threads, metrics->voluntary_ctxt_switches, metrics->nonvoluntary_ctxt_switches,
            metrics->vm_size, metrics->vm_rss, metrics->vm_swap,
            metrics->rchar, metrics->wchar, metrics->syscr, metrics->syscw,
            metrics->net_rx_bytes, metrics->net_tx_bytes, metrics->net_rx_packets, metrics->net_tx_packets,
            metrics->net_connections, metrics->app_uptime_seconds,
            cpu_percentage, read_rate_bps, write_rate_bps);
}

static void run_continuous_monitoring(pid_t pid, int interval, FILE *csv_output) {
    ProcessMetrics metrics1, metrics2;
    unsigned long global_cpu1, global_cpu2;
    long ticks_per_sec = sysconf(_SC_CLK_TCK);

    printf("\nIniciando monitoramento... Pressione Enter para parar.\n");
    sleep(1);

    if (csv_output != NULL) {
        write_csv_header(csv_output);
    }

    // Coleta inicial
    if (get_process_metrics(pid, &metrics1) != 0 || get_global_cpu_time(&global_cpu1) != 0) {
        fprintf(stderr, "Erro ao coletar métricas iniciais para o PID %d.\n", pid);
        return;
    }

    while (!kbhit()) {
        sleep(interval);

        // Coleta subsequente
        if (get_process_metrics(pid, &metrics2) != 0 || get_global_cpu_time(&global_cpu2) != 0) {
            fprintf(stderr, "\nErro ao coletar métricas para o PID %d. O processo pode não existir mais.\n", pid);
            break;
        }

        // --- Cálculos ---
        unsigned long process_ticks_delta = (metrics2.utime + metrics2.stime) - (metrics1.utime + metrics1.stime);
        unsigned long total_cpu_delta = global_cpu2 - global_cpu1;

        double cpu_percentage = 0.0;
        if (total_cpu_delta > 0) {
            cpu_percentage = 100.0 * (double)process_ticks_delta / (double)total_cpu_delta;
        }

        double read_rate_bps = (double)(metrics2.rchar - metrics1.rchar) / interval;
        double write_rate_bps = (double)(metrics2.wchar - metrics1.wchar) / interval;

        // --- Exibição ---
        system("clear");
        printf("Monitorando PID: %d (Intervalo: %ds) | Pressione Enter para parar\n", pid, interval);
        printf("====================================================================\n");
        printf("Uso de CPU: %.2f%% \n", cpu_percentage);
        printf("Taxa de Leitura I/O: %.2f bytes/s\n", read_rate_bps);
        printf("Taxa de Escrita I/O: %.2f bytes/s\n", write_rate_bps);
        printf("--------------------------------------------------------------------\n");

        printf("CPU:\n");
        printf("  - Tempo de Usuário:   %.2f segundos\n", (double)metrics2.utime / ticks_per_sec);
        printf("  - Tempo de Sistema:   %.2f segundos\n", (double)metrics2.stime / ticks_per_sec);
        printf("  - Número de Threads:  %ld\n", metrics2.num_threads);
        printf("  - Trocas de Contexto: %ld (voluntárias), %ld (involuntárias)\n", metrics2.voluntary_ctxt_switches, metrics2.nonvoluntary_ctxt_switches);

        printf("Memória:\n");
        printf("  - Memória Virtual (VmSize): %ld KB\n", metrics2.vm_size);
        printf("  - Memória Residente (VmRSS): %ld KB\n", metrics2.vm_rss);
        printf("  - Memória em Swap (VmSwap): %ld KB\n", metrics2.vm_swap);
        printf("  - Page Faults:        %lu (minor), %lu (major)\n", metrics2.minflt, metrics2.majflt);

        printf("I/O:\n");
        printf("  - Total Bytes Lidos:    %llu\n", metrics2.rchar);
        printf("  - Total Bytes Escritos:   %llu\n", metrics2.wchar);
        printf("  - Syscalls Leitura: %llu\n", metrics2.syscr);
        printf("  - Syscalls Escrita: %llu\n", metrics2.syscw);

        printf("Rede:\n");
        printf("  - Bytes Recebidos:    %llu\n", metrics2.net_rx_bytes);
        printf("  - Pacotes Recebidos:  %llu\n", metrics2.net_rx_packets);
        printf("  - Bytes Enviados:     %llu\n", metrics2.net_tx_bytes);
        printf("  - Pacotes Enviados:   %llu\n", metrics2.net_tx_packets);
        printf("  - Conexões Ativas:    %ld\n", metrics2.net_connections);

        printf("Geral:\n");
        printf("  - Tempo de Atividade: %.2f segundos\n", metrics2.app_uptime_seconds);
        printf("====================================================================\n");

        if (csv_output != NULL) {
            write_csv_data(csv_output, &metrics2, cpu_percentage, read_rate_bps, write_rate_bps);
        }

        // Prepara para a próxima iteração
        metrics1 = metrics2;
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
                int user_interval = 0;
                if (scanf("%d", &user_interval) == 1) {
                    if (user_interval > 0) {
                        interval = user_interval;
                    }
                }
                flush_stdin_line();

                char export_choice;
                char filename[256] = {0};
                FILE *csv_file = NULL;

                printf("Deseja exportar os dados para um arquivo CSV durante o monitoramento? (s/n): ");
                if (scanf(" %c", &export_choice) == 1) { // Espaço antes de %c para consumir newline
                    flush_stdin_line();
                    if (export_choice == 's' || export_choice == 'S') {
                        printf("Digite o nome do arquivo CSV (ex: metricas.csv): ");
                        if (scanf("%255s", filename) == 1) {
                            flush_stdin_line();
                            csv_file = fopen(filename, "w");
                            if (csv_file == NULL) {
                                perror("Erro ao abrir o arquivo CSV para escrita");
                            }
                        } else {
                            flush_stdin_line();
                        }
                    }
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
            if (scanf("%511s", cgroup_path) == 1) {
                flush_stdin_line();
                // display_cgroup_metrics(cgroup_path);
                printf("\n[AVISO] Funcionalidade ainda não implementada.\n");
                wait_for_enter();
            } else {
                flush_stdin_line();
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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <fcntl.h>
#include <termios.h>
#include "process_selector.h"
#include "process_monitor.h"
#include "monitor.h"
#include "namespace.h"
#include "cgroup.h"
#include <string.h>
#include <time.h>
#include "process_tree.h"

static void flush_stdin_line(void) {
    int c;
    while ((c = getchar()) != '\n' && c != EOF) {
        // descarta caracteres remanescentes
    }
}

static void wait_for_enter(void) {
    printf("\nPressione Enter para continuar...");
    fflush(stdout);
    int c;
    while ((c = getchar()) != '\n' && c != EOF) {
        // aguarda Enter do usuário
    }
}

// Função para verificar se uma tecla foi pressionada sem bloquear
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

static void display_metrics_for_pid(pid_t pid) {
    CpuMetrics cpu;
    MemoryMetrics memory;
    IoMetrics io;

    if (get_cpu_metrics(pid, &cpu) != 0) {
        perror("Erro ao coletar métricas de CPU");
        return;
    }
    if (get_memory_metrics(pid, &memory) != 0) {
        perror("Erro ao coletar métricas de memória");
        return;
    }
    if (get_io_metrics(pid, &io) != 0) {
        perror("Erro ao coletar métricas de I/O");
        io.pid = pid;
        io.read_bytes = 0;
        io.write_bytes = 0;
    }

    long ticks_per_sec = sysconf(_SC_CLK_TCK);

    printf("\n--- Métricas para o Processo PID: %d ---\n", pid);
    printf("CPU:\n");
    printf("  - Tempo de Usuário:   %.2f segundos\n", (double)cpu.utime / ticks_per_sec);
    printf("  - Tempo de Sistema:   %.2f segundos\n", (double)cpu.stime / ticks_per_sec);
    printf("  - Número de Threads:  %ld\n", cpu.num_threads);

    printf("Memória:\n");
    printf("  - Memória Virtual (VmSize): %ld KB\n", memory.vm_size_kb);
    printf("  - Memória Residente (VmRSS): %ld KB\n", memory.vm_rss_kb);

    printf("I/O:\n");
    printf("  - Bytes Lidos:   %llu\n", io.read_bytes);
    printf("  - Bytes Escritos:  %llu\n", io.write_bytes);
    printf("----------------------------------------\n");
}

static void calculate_and_display_percentages(pid_t pid) {
    unsigned long total_cpu1, total_cpu2;
    unsigned long long total_process_ticks1 = 0, total_process_ticks2 = 0;
    unsigned long long total_read_bytes1 = 0, total_read_bytes2 = 0;
    unsigned long long total_write_bytes1 = 0, total_write_bytes2 = 0;

    // --- Primeira Medição --- //
    if (get_global_cpu_time(&total_cpu1) != 0) {
        perror("Erro ao ler o tempo total de CPU (1)");
        return;
    }

    PidList children1 = find_child_pids(pid);
    pid_t *all_pids1 = malloc((children1.count + 1) * sizeof(pid_t));
    if (!all_pids1) { free_pid_list(&children1); return; }
    all_pids1[0] = pid;
    memcpy(all_pids1 + 1, children1.pids, children1.count * sizeof(pid_t));

    for (int i = 0; i < children1.count + 1; ++i) {
        CpuMetrics cpu;
        IoMetrics io;
        if (get_cpu_metrics(all_pids1[i], &cpu) == 0) {
            total_process_ticks1 += cpu.utime + cpu.stime;
        }
        if (get_io_metrics(all_pids1[i], &io) == 0) {
            total_read_bytes1 += io.read_bytes;
            total_write_bytes1 += io.write_bytes;
        }
    }

    free(all_pids1);
    free_pid_list(&children1);

    printf("Coletando métricas por 1 segundo...\n");
    sleep(1);

    // --- Segunda Medição --- //
    if (get_global_cpu_time(&total_cpu2) != 0) {
        perror("Erro ao ler o tempo total de CPU (2)");
        return;
    }

    PidList children2 = find_child_pids(pid);
    pid_t *all_pids2 = malloc((children2.count + 1) * sizeof(pid_t));
    if (!all_pids2) { free_pid_list(&children2); return; }
    all_pids2[0] = pid;
    memcpy(all_pids2 + 1, children2.pids, children2.count * sizeof(pid_t));

    for (int i = 0; i < children2.count + 1; ++i) {
        CpuMetrics cpu;
        IoMetrics io;
        if (get_cpu_metrics(all_pids2[i], &cpu) == 0) {
            total_process_ticks2 += cpu.utime + cpu.stime;
        }
        if (get_io_metrics(all_pids2[i], &io) == 0) {
            total_read_bytes2 += io.read_bytes;
            total_write_bytes2 += io.write_bytes;
        }
    }

    free(all_pids2);
    free_pid_list(&children2);

    // --- Cálculos --- //
    unsigned long process_time_delta = total_process_ticks2 - total_process_ticks1;
    unsigned long total_time_delta = total_cpu2 - total_cpu1;

    double cpu_percentage = 0.0;
    if (total_time_delta > 0) {
        cpu_percentage = 100.0 * (double)process_time_delta / (double)total_time_delta;
    }

    double read_rate_bps = (double)(total_read_bytes2 - total_read_bytes1);
    double write_rate_bps = (double)(total_write_bytes2 - total_write_bytes1);

    printf("\n--- Percentuais e Taxas para a Árvore de Processos do PID: %d ---\n", pid);
    printf("Uso de CPU: %.2f%%\n", cpu_percentage);
    printf("Taxa de Leitura I/O: %.2f bytes/s\n", read_rate_bps);
    printf("Taxa de Escrita I/O: %.2f bytes/s\n", write_rate_bps);
    printf("--------------------------------------------------------------------\n");
}

static void export_metrics_to_csv(pid_t pid) {
    CpuMetrics cpu;
    MemoryMetrics memory;
    IoMetrics io;

    if (get_cpu_metrics(pid, &cpu) != 0) {
        perror("Erro ao coletar métricas de CPU para exportação");
        return;
    }
    if (get_memory_metrics(pid, &memory) != 0) {
        perror("Erro ao coletar métricas de memória para exportação");
        return;
    }
    if (get_io_metrics(pid, &io) != 0) {
        perror("Aviso: Não foi possível coletar métricas de I/O para exportação");
        io.read_bytes = 0;
        io.write_bytes = 0;
    }

    char filename[256];
    printf("Digite o nome do arquivo CSV para exportar (ex: metricas.csv): ");
    scanf("%255s", filename);
    flush_stdin_line();

    FILE *fp = fopen(filename, "w");
    if (!fp) {
        perror("Erro ao abrir o arquivo para escrita");
        return;
    }

    // Header
    fprintf(fp, "pid,utime,stime,num_threads,vm_size_kb,vm_rss_kb,read_bytes,write_bytes\n");
    // Data
    fprintf(fp, "%d,%lu,%lu,%ld,%ld,%ld,%llu,%llu\n",
            pid, cpu.utime, cpu.stime, cpu.num_threads,
            memory.vm_size_kb, memory.vm_rss_kb,
            io.read_bytes, io.write_bytes);

    fclose(fp);
    printf("Métricas exportadas com sucesso para '%s'\n", filename);
}

// --- Funções para exibir os menus ---

void print_main_menu() {
    printf("\n===========================\n");
    printf(" RESOURCE MONITOR SYSTEM\n");
    printf("===========================\n\n");
    printf("[1] Resource Profiler\n");
    printf("[2] Namespace Analyzer\n");
    printf("[3] Control Group Manager\n");
    printf("[4] Experimentos\n");
    printf("[5] Sair\n\n");
}

void print_profiler_submenu() {
    printf("\n  --- [1] Resource Profiler ---\n");
    printf("    ├─ [1] Monitorar processo continuamente (Pressione Enter para parar)\n");
    printf("    ├─ [2] Exibir métricas de um processo (snapshot)\n");
    printf("    ├─ [3] Calcular percentuais de uso\n");
    printf("    ├─ [4] Exportar métricas em CSV\n");
    printf("    └─ [5] Voltar ao menu principal\n\n");
}

void print_namespace_submenu() {
    printf("\n  --- [2] Namespace Analyzer ---\n");
    printf("    ├─ [1] Listar namespaces de um processo\n");
    printf("    ├─ [2] Listar processos em um namespace\n");
    printf("    ├─ [3] Comparar namespaces entre dois processos\n");
    printf("    ├─ [4] Gerar relatório geral de namespaces\n");
    printf("    └─ [5] Voltar ao menu principal\n\n");
}

void print_cgroup_submenu() {
    printf("\n  --- [3] Control Group Manager ---\n");
    printf("    ├─ [1] Listar métricas de CPU, Memória e BlkIO\n");
    printf("    ├─ [2] Criar cgroup experimental\n");
    printf("    ├─ [3] Mover processo para cgroup\n");
    printf("    ├─ [4] Aplicar limites de CPU e Memória\n");
    printf("    ├─ [5] Gerar relatório de utilização\n");
    printf("    └─ [6] Voltar ao menu principal\n\n");
}

void print_experiments_submenu() {
     printf("\n  --- [4] Experimentos ---\n");
     printf("    ├─ [1] Overhead de monitoramento\n");
     printf("    ├─ [2] Isolamento via namespaces\n");
     printf("    ├─ [3] Throttling de CPU\n");
     printf("    ├─ [4] Limitação de memória\n");
     printf("    └─ [5] Limitação de I/O\n\n");
}

// --- Funções para lidar com a lógica dos submenus ---

void handle_profiler_menu() {
    int choice = 0;
    while (choice != 5) {
        print_profiler_submenu();
        printf("Opção do Profiler: ");
        if (scanf("%d", &choice) != 1) {
            flush_stdin_line(); // Limpa a entrada inválida
            choice = 0; // Reseta a escolha para evitar loop infinito
            continue;
        }
        flush_stdin_line(); // Limpa o newline após o scanf

        if (choice == 1) {
            int selected_pid = select_process();
            if (selected_pid != -1) {
                int interval = 3; // Intervalo padrão de 3 segundos
                printf("Digite o intervalo de monitoramento em segundos (padrão: 3): ");
                int user_interval = 0;
                if (scanf("%d", &user_interval) == 1) {
                    if (user_interval > 0) {
                        interval = user_interval;
                    }
                }
                flush_stdin_line(); // Limpa o buffer após ler o intervalo


                printf("\nIniciando monitoramento... Pressione Enter para parar.\n");
                sleep(2); // Dá tempo para o usuário ler a mensagem

                time_t start_time = time(NULL); // Registra o tempo de início

                while (!kbhit()) {
                    system("clear");
                    time_t current_time = time(NULL);
                    double elapsed_seconds = difftime(current_time, start_time);
                    printf("Monitorando PID: %d (Intervalo: %ds) | Pressione Enter para parar | Tempo decorrido: %.0fs\n\n", selected_pid, interval, elapsed_seconds);
                    ProcessMetrics metrics;
                    if (get_process_metrics(selected_pid, &metrics) == 0) {
                        print_process_metrics(&metrics);
                    } else {
                        fprintf(stderr, "\nErro ao coletar métricas para o PID %d. O processo pode não existir mais.\n", selected_pid);
                        break; // Sai do loop se o processo sumir
                    }
                    sleep(interval);
                }
                flush_stdin_line(); // Limpa o Enter que parou o loop
            }
        } else if (choice == 2) {
            int selected_pid = select_process();
            if (selected_pid != -1) {
                ProcessMetrics metrics;
                if (get_process_metrics(selected_pid, &metrics) == 0) {
                    print_process_metrics(&metrics);
                } else {
                    fprintf(stderr, "\nErro ao coletar métricas para o PID %d. O processo pode não existir mais.\n", selected_pid);
                }
                wait_for_enter();
            }
        } else if (choice == 3) {
            int pid = select_process();
            if (pid != -1) {
                calculate_and_display_percentages(pid);
                wait_for_enter();
            }
        } else if (choice == 4) {
            int pid = select_process();
            if (pid != -1) {
                export_metrics_to_csv(pid);
                wait_for_enter();
            }
        } else if (choice != 5) {
            printf("Opção inválida.\n");
            sleep(1);
        }
    }
}

void handle_namespace_menu() {
    int choice = 0;
    while (choice != 5) {
        print_namespace_submenu();
        printf("Opção do Namespace Analyzer: ");
        scanf("%d", &choice);
        if (choice >= 1 && choice <= 4) {
            printf("\n[AVISO] Opção 2.%d não implementada.\n", choice);
            sleep(2);
        } else if (choice != 5) {
            printf("Opção inválida.\n");
            sleep(1);
        }
    }
}

void handle_cgroup_menu() {
    int choice = 0;
    while (choice != 6) {
        print_cgroup_submenu();
        printf("Opção do CGroup Manager: ");
        scanf("%d", &choice);
        if (choice >= 1 && choice <= 5) {
            printf("\n[AVISO] Opção 3.%d não implementada.\n", choice);
            sleep(2);
        } else if (choice != 6) {
            printf("Opção inválida.\n");
            sleep(1);
        }
    }
}

void handle_experiments_menu() {
     printf("\n[AVISO] Menu de Experimentos não implementado.\n");
     sleep(2);
}


// --- Função Principal ---

int main() {
    int choice = 0;
    while (choice != 5) {
        print_main_menu();
        printf("Opção principal: ");
        if (scanf("%d", &choice) != 1) {
            while (getchar() != '\n'); // Limpa buffer
            choice = -1;
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
                handle_experiments_menu();
                break;
            case 5:
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

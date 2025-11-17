#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <string.h>
#include <time.h>
#include <dirent.h>
#include <ctype.h>
#include <errno.h>

#include "monitor.h"
#include "namespace.h"
#include "cgroup.h"

// Constantes para configuração padrão
#define DEFAULT_MONITORING_INTERVAL 3
#define DEFAULT_BATCH_INTERVAL 1
#define DEFAULT_BATCH_SAMPLES 10
#define MAX_FILENAME_LENGTH 256
#define MAX_PATH_LENGTH 512
#define MAX_BUFFER_LENGTH 256
#define MAX_INTERVAL_INPUT_LENGTH 10

static void print_usage(const char *program_name) {
    printf("Uso: %s [opções]\n", program_name);
    printf("Opções disponíveis:\n");
    printf("  -n, --namespace-report           Gera um relatório JSON dos namespaces ativos\n");
    printf("  -c, --cgroup-report              Gera um relatório JSON do cgroup raiz (report.json)\n");
    printf("  -m, --monitor <PID>              Executa o profiler em modo batch para o PID informado\n");
    printf("      --interval <segundos>        Intervalo entre amostras no modo batch (padrão: 1s)\n");
    printf("      --samples <quantidade>       Número de amostras a coletar no modo batch (padrão: 10)\n");
    printf("  -o, --output <arquivo.csv>       Caminho de saída para salvar os dados do modo batch em CSV\n");
    printf("\nSem opções, o programa abre o menu interativo completo.\n");
}



// --- Funções Utilitárias de UI ---

// Limpa o buffer de entrada até encontrar newline ou EOF
static void flush_stdin_line(void) {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

// --- Funções de Seleção de Processo (anteriormente em process_selector.c) ---

static int select_process() {
    DIR *dir;
    struct dirent *entry;
    char path[MAX_PATH_LENGTH];
    char comm[MAX_BUFFER_LENGTH];
    FILE *fp;

    printf("===== Lista de Processos em Execução =====\n");
    printf("%-10s %s\n", "PID", "Comando");
    printf("------------------------------------------\n");

    if ((dir = opendir("/proc")) == NULL) {
        perror("Erro ao abrir /proc");
        return -1;
    }

    while ((entry = readdir(dir)) != NULL) {
        int is_pid = 1;
        for (char *p = entry->d_name; *p; p++) {
            if (!isdigit(*p)) {
                is_pid = 0;
                break;
            }
        }
        if (is_pid) {
            snprintf(path, sizeof(path), "/proc/%s/comm", entry->d_name);
            fp = fopen(path, "r");
            if (fp) {
                if (fgets(comm, sizeof(comm), fp) != NULL) {
                    comm[strcspn(comm, "\n")] = 0;
                    printf("%-10s %s\n", entry->d_name, comm);
                }
                fclose(fp);
            }
        }
    }
    closedir(dir);
    printf("------------------------------------------\n");

    int selected_pid = -1;
    while (1) {
        printf("Digite o PID do processo que deseja monitorar (ou 0 para voltar): ");
        if (scanf("%d", &selected_pid) != 1) {
            flush_stdin_line();
            fprintf(stderr, "Entrada inválida. Por favor, insira um número.\n");
            continue;
        }
        flush_stdin_line();

        if (selected_pid == 0) {
            return -1; // Opção para o usuário voltar
        }
        
        // Verifica a existência e permissão do processo
        snprintf(path, sizeof(path), "/proc/%d", selected_pid);
        if (access(path, R_OK) == 0) {
            // O diretório do processo existe e temos permissão de leitura
            break; 
        } else {
            fprintf(stderr, "Erro: Processo com PID %d não encontrado ou acesso negado.\n", selected_pid);
            fprintf(stderr, "Verifique se o PID existe e se você tem permissão para acessá-lo.\n\n");
            selected_pid = -1;
        }
    }
    return selected_pid;
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
        fprintf(stderr, "Erro ao coletar métricas iniciais para o PID %d. O processo pode ter terminado inesperadamente.\n", pid);
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
        long num_cpu_cores = sysconf(_SC_NPROCESSORS_ONLN);
        if (num_cpu_cores < 1) {
            num_cpu_cores = 1; // Fallback in case sysconf fails
        }

        unsigned long process_ticks_delta = (cpu2.utime + cpu2.stime) - (cpu1.utime + cpu1.stime);
        unsigned long total_cpu_delta = global_cpu2 - global_cpu1;

        double cpu_percentage = 0.0;
        if (total_cpu_delta > 0) {
            cpu_percentage = 100.0 * (double)process_ticks_delta / (double)total_cpu_delta * num_cpu_cores;
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

static int run_batch_monitoring(pid_t pid, int interval, int samples, const char *csv_path) {
    if (pid <= 0 || interval <= 0 || samples <= 0) {
        fprintf(stderr, "Parâmetros inválidos para o modo batch.\n");
        return -1;
    }

    FILE *csv_fp = NULL;
    if (csv_path != NULL && strlen(csv_path) > 0) {
        csv_fp = fopen(csv_path, "w");
        if (csv_fp == NULL) {
            perror("Erro ao abrir arquivo CSV para modo batch");
            return -1;
        }
        write_csv_header(csv_fp);
    }

    CpuMetrics prev_cpu, curr_cpu;
    IoMetrics prev_io, curr_io;
    MemoryMetrics mem;
    NetworkMetrics net;
    unsigned long prev_global = 0, curr_global = 0;
    int rc = 0;

    if (get_cpu_metrics(pid, &prev_cpu) != 0 ||
        get_io_metrics(pid, &prev_io) != 0 ||
        get_global_cpu_time(&prev_global) != 0) {
        fprintf(stderr, "Não foi possível coletar as métricas iniciais para o PID %d.\n", pid);
        if (csv_fp) {
            fclose(csv_fp);
        }
        return -1;
    }

    printf("Modo batch iniciado para PID %d (%d amostras, intervalo %ds).\n", pid, samples, interval);

    for (int sample = 1; sample <= samples; sample++) {
        sleep(interval);

        if (get_cpu_metrics(pid, &curr_cpu) != 0 ||
            get_memory_metrics(pid, &mem) != 0 ||
            get_io_metrics(pid, &curr_io) != 0 ||
            get_network_metrics(pid, &net) != 0 ||
            get_global_cpu_time(&curr_global) != 0) {
            fprintf(stderr, "Falha ao coletar métricas para o PID %d (amostra %d).\n", pid, sample);
            rc = -1;
            break;
        }

        long num_cpu_cores = sysconf(_SC_NPROCESSORS_ONLN);
        if (num_cpu_cores < 1) {
            num_cpu_cores = 1;
        }

        unsigned long process_ticks_delta = (curr_cpu.utime + curr_cpu.stime) - (prev_cpu.utime + prev_cpu.stime);
        unsigned long total_cpu_delta = curr_global - prev_global;
        double cpu_percentage = 0.0;
        if (total_cpu_delta > 0) {
            cpu_percentage = 100.0 * (double)process_ticks_delta / (double)total_cpu_delta * num_cpu_cores;
        }

        double read_rate_bps = (double)(curr_io.read_bytes - prev_io.read_bytes) / interval;
        double write_rate_bps = (double)(curr_io.write_bytes - prev_io.write_bytes) / interval;

        printf("Amostra %d/%d | CPU: %.2f%% | RSS: %ld KB | Leitura: %.2f B/s | Escrita: %.2f B/s\n",
               sample, samples, cpu_percentage, mem.vm_rss_kb, read_rate_bps, write_rate_bps);

        if (csv_fp) {
            write_csv_data(csv_fp, pid, cpu_percentage, read_rate_bps, write_rate_bps,
                           &curr_cpu, &mem, &curr_io, &net);
        }

        prev_cpu = curr_cpu;
        prev_io = curr_io;
        prev_global = curr_global;
    }

    if (csv_fp) {
        fclose(csv_fp);
        printf("CSV salvo em '%s'.\n", csv_path);
    }

    return rc;
}


// --- Funções de Menu ---

void print_main_menu() {
    printf("\n===========================\n");
    printf(" RESOURCE MONITOR SYSTEM\n");
    printf("===========================\n\n");
    printf("[1] Resource Profiler\n");
    printf("[2] Namespace Analyzer\n");
    printf("[3] Control Group Manager\n");
    printf("[4] Executar Experimentos\n");
    printf("[5] Sair\n\n");
}



void print_profiler_submenu() {
    printf("\n  --- [1] Resource Profiler ---\n");
    printf("    ├─ [1] Monitorar Processo (Uso de CPU, I/O, etc.)\n");
    printf("    └─ [2] Voltar ao menu principal\n\n");
}



void print_namespace_submenu() {
    printf("\n  --- [2] Namespace Analyzer ---\n");
    printf("    ├─ [1] Listar namespaces de um processo\n");
    printf("    ├─ [2] Encontrar processos em um namespace\n");
    printf("    ├─ [3] Comparar namespaces entre dois processos\n");
    printf("    ├─ [4] Gerar relatório de namespaces do sistema\n");
    printf("    └─ [5] Voltar ao menu principal\n\n");
}



void print_cgroup_submenu() {
    printf("\n  --- [3] Control Group Manager ---\n");
    printf("    ├─ [1] Listar métricas de cgroups (CPU, Memory, BlkIO)\n");
    printf("    ├─ [2] Criar cgroup experimental\n");
    printf("    ├─ [3] Mover processo para cgroup\n");
    printf("    ├─ [4] Aplicar limites de CPU, Memória e I/O\n");
    printf("    ├─ [5] Gerar relatório de utilização\n");
    printf("    └─ [6] Voltar ao menu principal\n\n");
}

void print_experiments_submenu() {
    printf("\n  --- [4] Executar Experimentos ---\n");
    printf("    ├─ [1] Executar TODOS os experimentos\n");
    printf("    ├─ [2] Experimento 1: Overhead de Monitoramento\n");
    printf("    ├─ [3] Experimento 2: Isolamento via Namespaces\n");
    printf("    ├─ [4] Experimento 3: Throttling de CPU (sudo)\n");
    printf("    ├─ [5] Experimento 4: Limitação de Memória (sudo)\n");
    printf("    ├─ [6] Experimento 5: Limitação de I/O (sudo)\n");
    printf("    └─ [7] Voltar ao menu principal\n\n");
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
                int interval = DEFAULT_MONITORING_INTERVAL;
                printf("Digite o intervalo de monitoramento em segundos (padrão: %d): ", DEFAULT_MONITORING_INTERVAL);
                char interval_str[MAX_INTERVAL_INPUT_LENGTH];
                if (fgets(interval_str, sizeof(interval_str), stdin) != NULL) {
                    int user_interval = atoi(interval_str);
                    if (user_interval > 0) {
                        interval = user_interval;
                    }
                }

                char export_choice;
                char filename[MAX_FILENAME_LENGTH] = {0};
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
                // Nota: csv_file já é fechado dentro de run_continuous_monitoring()
                // se for diferente de NULL
            }
        } else if (choice != 2) {
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
        if (scanf("%d", &choice) != 1) {
            flush_stdin_line();
            choice = 0;
            continue;
        }
        flush_stdin_line();

        switch (choice) {
            case 1: {
                int pid = select_process();
                if (pid != -1) {
                    list_process_namespaces(pid);
                    wait_for_enter();
                }
                break;
            }
            case 2: {
                find_processes_in_namespace();
                wait_for_enter();
                break;
            }
            case 3: {
                printf("\n--- Selecione o PRIMEIRO processo ---\n");
                int pid1 = select_process();
                if (pid1 != -1) {
                    printf("\n--- Selecione o SEGUNDO processo ---\n");
                    int pid2 = select_process();
                    if (pid2 != -1) {
                        compare_process_namespaces(pid1, pid2);
                        wait_for_enter();
                    }
                }
                break;
            }
            case 4: {
                generate_json_namespace_report();
                wait_for_enter();
                break;
            }
            case 5:
                // Voltar ao menu principal
                break;
            default:
                printf("Opção inválida.\n");
                sleep(1);
                break;
        }
    }
}

void handle_experiments_menu() {
    int choice = 0;
    int result = 0;

    while (choice != 7) {
        print_experiments_submenu();
        printf("Opção de Experimento: ");
        if (scanf("%d", &choice) != 1) {
            flush_stdin_line();
            choice = 0;
            continue;
        }
        flush_stdin_line();

        switch (choice) {
            case 1:
                printf("\n>>> Executando TODOS os experimentos...\n");
                result = system("bash scripts/run_experiments.sh");
                if (result == 0) {
                    printf("\n✓ Experimentos concluídos com sucesso!\n");
                } else {
                    printf("\n✗ Alguns experimentos falharam. Verifique os logs.\n");
                }
                wait_for_enter();
                break;
            case 2:
                printf("\n>>> Experimento 1: Overhead de Monitoramento\n");
                result = system("bash scripts/experimento1_overhead.sh");
                wait_for_enter();
                break;
            case 3:
                printf("\n>>> Experimento 2: Isolamento via Namespaces\n");
                result = system("bash scripts/experimento2_namespaces.sh");
                wait_for_enter();
                break;
            case 4:
                printf("\n>>> Experimento 3: Throttling de CPU (requer sudo)\n");
                if (getuid() != 0) {
                    printf("⚠  ATENÇÃO: Este experimento requer privilégios de root.\n");
                    printf("   Execute o programa com sudo ou rode: sudo bash scripts/experimento3_cpu_throttle.sh\n");
                } else {
                    result = system("bash scripts/experimento3_cpu_throttle.sh");
                }
                wait_for_enter();
                break;
            case 5:
                printf("\n>>> Experimento 4: Limitação de Memória (requer sudo)\n");
                if (getuid() != 0) {
                    printf("⚠  ATENÇÃO: Este experimento requer privilégios de root.\n");
                    printf("   Execute o programa com sudo ou rode: sudo bash scripts/experimento4_mem_limit.sh\n");
                } else {
                    result = system("bash scripts/experimento4_mem_limit.sh");
                }
                wait_for_enter();
                break;
            case 6:
                printf("\n>>> Experimento 5: Limitação de I/O (requer sudo)\n");
                if (getuid() != 0) {
                    printf("⚠  ATENÇÃO: Este experimento requer privilégios de root.\n");
                    printf("   Execute o programa com sudo ou rode: sudo bash scripts/experimento5_io_limit.sh\n");
                } else {
                    result = system("bash scripts/experimento5_io_limit.sh");
                }
                wait_for_enter();
                break;
            case 7:
                // Voltar ao menu principal
                break;
            default:
                printf("Opção inválida.\n");
                sleep(1);
                break;
        }
    }
}

void handle_cgroup_manager_menu() {



    int choice = 0;



    CgroupVersion cgroup_version = get_cgroup_version();







    if (cgroup_version == CGROUP_UNKNOWN) {



        printf("Erro: Não foi possível determinar a versão do cgroup. Verifique as permissões ou o sistema.\n");



        wait_for_enter();



        return;



    }







    printf("\nVersão do CGroup detectada: %s\n", 



           cgroup_version == CGROUP_V1 ? "v1" : "v2");



    wait_for_enter(); // Pausa para o usuário ver a versão







    while (choice != 6) {
        print_cgroup_submenu();
        printf("Opção do CGroup Manager: ");
        if (scanf("%d", &choice) != 1) {
            flush_stdin_line();
            choice = 0;
            continue;
        }
        flush_stdin_line();







        switch (choice) {
            case 1: {
                char *cgroup_path = select_cgroup();
                if (cgroup_path != NULL) {
                    display_cgroup_metrics(cgroup_version, cgroup_path);
                    free(cgroup_path); // Free the dynamically allocated path
                    wait_for_enter();
                }
                break;
            }



            case 2: {
                char cgroup_name[MAX_BUFFER_LENGTH];
                char parent_path[MAX_BUFFER_LENGTH];

                printf("Digite o nome do novo cgroup (ex: 'meu_teste'): ");
                if (fgets(cgroup_name, sizeof(cgroup_name), stdin) == NULL) {
                    printf("Erro ao ler o nome do cgroup.\n");
                    break;
                }
                cgroup_name[strcspn(cgroup_name, "\n")] = 0; // Remove newline

                if (strlen(cgroup_name) == 0) {
                    printf("Nome do cgroup não pode ser vazio.\n");
                    break;
                }

                printf("Digite o caminho relativo do cgroup pai (ex: '/' para raiz, 'user.slice'). Pressione Enter para usar a raiz: ");
                if (fgets(parent_path, sizeof(parent_path), stdin) == NULL) {
                    printf("Erro ao ler o caminho do cgroup pai.\n");
                    break;
                }
                parent_path[strcspn(parent_path, "\n")] = 0; // Remove newline

                if (create_cgroup(cgroup_version, parent_path, cgroup_name) == 0) {
                    printf("Cgroup criado com sucesso (verifique as mensagens acima).\n");
                } else {
                    printf("Falha ao criar cgroup.\n");
                }
                wait_for_enter();
                break;
            }



            case 3: {
                int pid = select_process();
                if (pid != -1) {
                    char *cgroup_path = select_cgroup();
                    if (cgroup_path != NULL) {
                        if (move_process_to_cgroup(cgroup_version, pid, cgroup_path) == 0) {
                            printf("Processo movido com sucesso (verifique as mensagens acima).\n");
                        } else {
                            printf("Falha ao mover processo para cgroup.\n");
                        }
                        free(cgroup_path); // Free the dynamically allocated path
                        wait_for_enter();
                    }
                }
                break;
            }



            case 4: {
                char *cgroup_path = select_cgroup();
                if (cgroup_path != NULL) {
                    apply_resource_limits(cgroup_version, cgroup_path);
                    free(cgroup_path);
                    wait_for_enter();
                }
                break;
            }



            case 5: {
                char *cgroup_path = select_cgroup();
                if (cgroup_path != NULL) {
                    generate_cgroup_report(cgroup_version, cgroup_path, "report.json");
                    free(cgroup_path);
                    wait_for_enter();
                }
                break;
            }



            case 6:
                // Voltar ao menu principal
                break;
            default:
                printf("Opção inválida.\n");
                sleep(1);
                break;
        }
    }
}





// --- Função Principal ---



int main(int argc, char *argv[]) {
    pid_t batch_pid = -1;
    int batch_interval = DEFAULT_BATCH_INTERVAL;
    int batch_samples = DEFAULT_BATCH_SAMPLES;
    const char *batch_output = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-n") == 0 || strcmp(argv[i], "--namespace-report") == 0) {
            generate_json_namespace_report();
            return 0;
        } else if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--cgroup-report") == 0) {
            CgroupVersion cgroup_version = get_cgroup_version();
            if (cgroup_version != CGROUP_UNKNOWN) {
                generate_cgroup_report(cgroup_version, "/", "report.json");
            } else {
                fprintf(stderr, "Erro: Não foi possível determinar a versão do cgroup.\n");
                return 1;
            }
            return 0;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-m") == 0 || strcmp(argv[i], "--monitor") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Erro: --monitor requer um PID.\n");
                return 1;
            }
            batch_pid = (pid_t)atoi(argv[++i]);
            if (batch_pid <= 0) {
                fprintf(stderr, "Erro: PID inválido informado para --monitor.\n");
                return 1;
            }
        } else if (strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--interval") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Erro: --interval requer um valor em segundos.\n");
                return 1;
            }
            batch_interval = atoi(argv[++i]);
            if (batch_interval <= 0) {
                fprintf(stderr, "Erro: --interval deve ser um inteiro positivo.\n");
                return 1;
            }
        } else if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--samples") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Erro: --samples requer um valor inteiro.\n");
                return 1;
            }
            batch_samples = atoi(argv[++i]);
            if (batch_samples <= 0) {
                fprintf(stderr, "Erro: --samples deve ser um inteiro positivo.\n");
                return 1;
            }
        } else if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--output") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Erro: --output requer um caminho para o arquivo CSV.\n");
                return 1;
            }
            batch_output = argv[++i];
        } else {
            fprintf(stderr, "Opção desconhecida: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    if (batch_pid > 0) {
        if (run_batch_monitoring(batch_pid, batch_interval, batch_samples, batch_output) != 0) {
            return 1;
        }
        return 0;
    }

    int choice = 0;
    while (choice != 5) {
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
                handle_cgroup_manager_menu();
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

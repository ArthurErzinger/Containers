#include "process_monitor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> // para sysconf, _SC_CLK_TCK

// Função auxiliar para ler um valor de um arquivo no formato "Chave: Valor"
static int parse_key_value_file(const char *path, const char *key, long *result) {
    FILE *fp = fopen(path, "r");
    if (!fp) return -1;

    char line[256];
    int found = 0;
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, key, strlen(key)) == 0) {
            sscanf(line + strlen(key), "%ld", result);
            found = 1;
            break;
        }
    }

    fclose(fp);
    return found ? 0 : -1;
}

// Função auxiliar para ler valores do /proc/[pid]/io
static int parse_io_file(const char *path, unsigned long long *rchar, unsigned long long *wchar, unsigned long long *syscr, unsigned long long *syscw) {
    FILE *fp = fopen(path, "r");
    if (!fp) return -1;

    char line[256];
    int found = 0;
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "rchar:", 6) == 0) {
            sscanf(line + 6, "%llu", rchar);
            found++;
        } else if (strncmp(line, "wchar:", 6) == 0) {
            sscanf(line + 6, "%llu", wchar);
            found++;
        } else if (strncmp(line, "syscr:", 6) == 0) {
            sscanf(line + 6, "%llu", syscr);
            found++;
        } else if (strncmp(line, "syscw:", 6) == 0) {
            sscanf(line + 6, "%llu", syscw);
            found++;
        }
        if (found == 4) break;
    }

    fclose(fp);
    return (found >= 2) ? 0 : -1; // Pelo menos rchar e wchar devem existir
}

// Função auxiliar para ler valores do /proc/[pid]/net/dev
static int parse_net_dev_file(const char *path, unsigned long long *rx_bytes, unsigned long long *tx_bytes, unsigned long long *rx_packets, unsigned long long *tx_packets) {
    FILE *fp = fopen(path, "r");
    if (!fp) return -1;

    *rx_bytes = 0;
    *tx_bytes = 0;
    *rx_packets = 0;
    *tx_packets = 0;

    char line[512];
    // Pular as duas primeiras linhas (cabeçalho)
    if (!fgets(line, sizeof(line), fp) || !fgets(line, sizeof(line), fp)) {
        fclose(fp);
        return -1;
    }

    unsigned long long current_rx_bytes, current_tx_bytes, current_rx_packets, current_tx_packets;
    char iface_name[64];

    while (fgets(line, sizeof(line), fp)) {
        // Formato: iface: rx_bytes rx_packets ... tx_bytes tx_packets ...
        int items = sscanf(line, "%s %llu %llu %*u %*u %*u %*u %*u %*u %llu %llu",
                           iface_name, &current_rx_bytes, &current_rx_packets, &current_tx_bytes, &current_tx_packets);
        
        // Ignora a interface de loopback
        if (items == 5 && strncmp(iface_name, "lo:", 3) != 0) {
            *rx_bytes += current_rx_bytes;
            *tx_bytes += current_tx_bytes;
            *rx_packets += current_rx_packets;
            *tx_packets += current_tx_packets;
        }
    }

    fclose(fp);
    return 0;
}

// Função auxiliar para contar linhas em um arquivo, pulando o cabeçalho
static long count_file_lines(const char *path, int skip_header) {
    FILE *fp = fopen(path, "r");
    if (!fp) return 0;

    long count = 0;
    char line[256];

    // Pular linhas do cabeçalho
    for (int i = 0; i < skip_header; ++i) {
        if (!fgets(line, sizeof(line), fp)) {
            fclose(fp);
            return 0;
        }
    }

    // Contar as linhas restantes
    while (fgets(line, sizeof(line), fp)) {
        count++;
    }

    fclose(fp);
    return count;
}

// Função auxiliar para obter o uptime do sistema em segundos
static double get_system_uptime() {
    FILE *fp = fopen("/proc/uptime", "r");
    if (!fp) return 0.0;

    double uptime_seconds;
    if (fscanf(fp, "%lf", &uptime_seconds) != 1) {
        fclose(fp);
        return 0.0;
    }
    fclose(fp);
    return uptime_seconds;
}


int get_process_metrics(int pid, ProcessMetrics *metrics) {
    char path[256];
    FILE *fp;

    metrics->pid = pid;

    // 1. Ler /proc/[pid]/stat
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    fp = fopen(path, "r");
    if (!fp) {
        perror("Erro ao abrir /proc/[pid]/stat");
        return -1;
    }
    // (1) pid (2) comm ... (10) minflt (12) majflt (14) utime (15) stime ... (20) num_threads (22) starttime
    int result = fscanf(fp, "%*d (%255[^)]) %*c %*d %*d %*d %*d %*d %*u %lu %*u %lu %*u %lu %lu %*d %*d %*d %*d %ld %*d %lu",
                        metrics->comm, &metrics->minflt, &metrics->majflt, &metrics->utime, &metrics->stime, &metrics->num_threads, &metrics->starttime_jiffies);
    fclose(fp);
    if (result < 6) return -1;

    // 2. Ler /proc/[pid]/status
    snprintf(path, sizeof(path), "/proc/%d/status", pid);
    parse_key_value_file(path, "VmSize:", &metrics->vm_size);
    parse_key_value_file(path, "VmRSS:", &metrics->vm_rss);
    parse_key_value_file(path, "VmSwap:", &metrics->vm_swap);
    parse_key_value_file(path, "voluntary_ctxt_switches:", &metrics->voluntary_ctxt_switches);
    parse_key_value_file(path, "nonvoluntary_ctxt_switches:", &metrics->nonvoluntary_ctxt_switches);


    // 3. Ler /proc/[pid]/io
    snprintf(path, sizeof(path), "/proc/%d/io", pid);
    if (parse_io_file(path, &metrics->rchar, &metrics->wchar, &metrics->syscr, &metrics->syscw) != 0) {
        metrics->rchar = 0;
        metrics->wchar = 0;
        metrics->syscr = 0;
        metrics->syscw = 0;
    }

    // 4. Ler /proc/[pid]/net/dev
    snprintf(path, sizeof(path), "/proc/%d/net/dev", pid);
    if (parse_net_dev_file(path, &metrics->net_rx_bytes, &metrics->net_tx_bytes, &metrics->net_rx_packets, &metrics->net_tx_packets) != 0) {
        metrics->net_rx_bytes = 0;
        metrics->net_tx_bytes = 0;
        metrics->net_rx_packets = 0;
        metrics->net_tx_packets = 0;
    }

    // 5. Contar conexões de rede
    long tcp_connections, udp_connections;
    snprintf(path, sizeof(path), "/proc/%d/net/tcp", pid);
    tcp_connections = count_file_lines(path, 1);
    snprintf(path, sizeof(path), "/proc/%d/net/udp", pid);
    udp_connections = count_file_lines(path, 1);
    metrics->net_connections = tcp_connections + udp_connections;

    // 6. Calcular tempo de vida do aplicativo
    long ticks_per_sec = sysconf(_SC_CLK_TCK);
    double system_uptime = get_system_uptime();
    double process_starttime_seconds = (double)metrics->starttime_jiffies / ticks_per_sec;
    metrics->app_uptime_seconds = system_uptime - process_starttime_seconds;


    return 0;
}

void print_process_metrics(const ProcessMetrics *metrics) {
    if (!metrics) return;

    long ticks_per_sec = sysconf(_SC_CLK_TCK);

    printf("\n--- Métricas para o Processo PID: %d ---\n", metrics->pid);
    printf("CPU:\n");
    printf("  - Tempo de Usuário:   %.2f segundos\n", (double)metrics->utime / ticks_per_sec);
    printf("  - Tempo de Sistema:   %.2f segundos\n", (double)metrics->stime / ticks_per_sec);
    printf("  - Tempo Total Ativo:  %.2f segundos\n", (double)(metrics->utime + metrics->stime) / ticks_per_sec);
    printf("  - Número de Threads:  %ld\n", metrics->num_threads);
    printf("  - Page Faults:        %lu (minor), %lu (major)\n", metrics->minflt, metrics->majflt);
    printf("  - Trocas de Contexto: %ld (voluntárias), %ld (involuntárias)\n", metrics->voluntary_ctxt_switches, metrics->nonvoluntary_ctxt_switches);

    printf("Memória:\n");
    printf("  - Memória Virtual (VmSize): %ld KB\n", metrics->vm_size);
    printf("  - Memória Residente (VmRSS): %ld KB\n", metrics->vm_rss);
    printf("  - Memória em Swap (VmSwap): %ld KB\n", metrics->vm_swap);

    printf("I/O:\n");
    printf("  - Bytes Lidos:    %llu\n", metrics->rchar);
    printf("  - Bytes Escritos:   %llu\n", metrics->wchar);
    printf("  - Syscalls Leitura: %llu\n", metrics->syscr);
    printf("  - Syscalls Escrita: %llu\n", metrics->syscw);

    printf("Rede:\n");
    printf("  - Bytes Recebidos:    %llu\n", metrics->net_rx_bytes);
    printf("  - Pacotes Recebidos:  %llu\n", metrics->net_rx_packets);
    printf("  - Bytes Enviados:     %llu\n", metrics->net_tx_bytes);
    printf("  - Pacotes Enviados:   %llu\n", metrics->net_tx_packets);
    printf("  - Conexões Ativas:    %ld\n", metrics->net_connections);

    printf("Tempo de Vida do Aplicativo:\n");
    printf("  - Uptime:             %.2f segundos\n", metrics->app_uptime_seconds);

    printf("----------------------------------------\n");
}
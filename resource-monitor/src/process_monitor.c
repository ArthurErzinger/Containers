#include "process_monitor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> // para sysconf, _SC_PAGESIZE

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
static int parse_io_file(const char *path, unsigned long long *rchar, unsigned long long *wchar) {
    FILE *fp = fopen(path, "r");
    if (!fp) return -1;

    char line[256];
    int found_r = 0, found_w = 0;
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "rchar:", 6) == 0) {
            sscanf(line + 6, "%llu", rchar);
            found_r = 1;
        } else if (strncmp(line, "wchar:", 6) == 0) {
            sscanf(line + 6, "%llu", wchar);
            found_w = 1;
        }
        if (found_r && found_w) break;
    }

    fclose(fp);
    return (found_r && found_w) ? 0 : -1;
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
    // O formato do /stat é complexo, então lemos os campos com cuidado
    // (1) pid (2) comm ... (14) utime (15) stime ... (20) num_threads
    int result = fscanf(fp, "%*d %*s %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu %lu %*d %*d %*d %*d %ld",
                        &metrics->utime, &metrics->stime, &metrics->num_threads);
    fclose(fp);
    if (result < 3) return -1; // Não conseguiu ler os 3 campos

    // 2. Ler /proc/[pid]/status para VmSize e VmRSS
    snprintf(path, sizeof(path), "/proc/%d/status", pid);
    if (parse_key_value_file(path, "VmSize:", &metrics->vm_size) != 0) return -1;
    if (parse_key_value_file(path, "VmRSS:", &metrics->vm_rss) != 0) return -1;

    // 3. Ler /proc/[pid]/io para rchar e wchar
    snprintf(path, sizeof(path), "/proc/%d/io", pid);
    if (parse_io_file(path, &metrics->rchar, &metrics->wchar) != 0) {
        // O arquivo /proc/[pid]/io pode não ser legível para todos os processos
        // ou pode estar desabilitado no kernel. Tratamos isso como não fatal.
        metrics->rchar = 0;
        metrics->wchar = 0;
    }

    return 0;
}

void print_process_metrics(const ProcessMetrics *metrics) {
    if (!metrics) return;

    long ticks_per_sec = sysconf(_SC_CLK_TCK);

    printf("\n--- Métricas para o Processo PID: %d ---\n", metrics->pid);
    printf("CPU:\n");
    printf("  - Tempo de Usuário:   %.2f segundos\n", (double)metrics->utime / ticks_per_sec);
    printf("  - Tempo de Sistema:   %.2f segundos\n", (double)metrics->stime / ticks_per_sec);
    printf("  - Número de Threads:  %ld\n", metrics->num_threads);

    printf("Memória:\n");
    printf("  - Memória Virtual (VmSize): %ld KB\n", metrics->vm_size);
    printf("  - Memória Residente (VmRSS): %ld KB\n", metrics->vm_rss);

    printf("I/O:\n");
    printf("  - Bytes Lidos:   %llu\n", metrics->rchar);
    printf("  - Bytes Escritos:  %llu\n", metrics->wchar);
    printf("----------------------------------------\n");
}

#include "monitor.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

// Função auxiliar para ler um valor de um arquivo no formato "Chave: Valor"
static int parse_status_file(FILE *fp, const char *key, long *result) {
    char line[256];
    int found = 0;
    // Retorna ao início do arquivo para procurar do começo
    fseek(fp, 0, SEEK_SET);

    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, key, strlen(key)) == 0) {
            if (sscanf(line + strlen(key), "%ld", result) == 1) {
                found = 1;
            }
            break;
        }
    }
    return found;
}

int get_global_cpu_time(unsigned long *total_time) {
    FILE *fp = fopen("/proc/stat", "r");
    if (!fp) {
        return -1;
    }

    char line[256];
    if (fgets(line, sizeof(line), fp) == NULL) {
        fclose(fp);
        return -1;
    }

    fclose(fp);

    if (strncmp(line, "cpu ", 4) != 0) {
        return -1; // Unexpected format
    }

    unsigned long user, nice, system, idle, iowait, irq, softirq, steal;
    int scanned = sscanf(line + 5, "%lu %lu %lu %lu %lu %lu %lu %lu",
                          &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal);

    if (scanned < 4) {
        return -1; // Could not parse all required fields
    }

    *total_time = user + nice + system + idle + iowait + irq + softirq + steal;

    return 0;
}

int get_cpu_metrics(pid_t pid, CpuMetrics *metrics) {
    if (!metrics) {
        errno = EINVAL;
        return -1;
    }

    char stat_path[64];
    snprintf(stat_path, sizeof(stat_path), "/proc/%d/stat", pid);

    FILE *stat_fp = fopen(stat_path, "r");
    if (!stat_fp) {
        return -1;
    }

    metrics->pid = pid;

    // Lê dados do /proc/[pid]/stat
    // A contagem de campos é 1-based. Formato: %d (%s) %c %d ...
    int scanned = fscanf(stat_fp,
                         "%*d %*s %*c %*d %*d %*d %*d %*d %*u " // pula os primeiros 9 campos
                         "%lu "      // (10) minflt
                         "%*u "      // (11) cminflt
                         "%lu "      // (12) majflt
                         "%*u "      // (13) cmajflt
                         "%lu "      // (14) utime
                         "%lu "      // (15) stime
                         "%lu "      // (16) cutime
                         "%lu "      // (17) cstime
                         "%*d "      // (18) priority
                         "%*d "      // (19) nice
                         "%ld",       // (20) num_threads
                         &metrics->minflt,
                         &metrics->majflt,
                         &metrics->utime,
                         &metrics->stime,
                         &metrics->cutime,
                         &metrics->cstime,
                         &metrics->num_threads);

    fclose(stat_fp);

    if (scanned != 7) {
        errno = EPROTO;
        return -1;
    }

    // Lê dados do /proc/[pid]/status para context switches
    char status_path[64];
    snprintf(status_path, sizeof(status_path), "/proc/%d/status", pid);
    FILE *status_fp = fopen(status_path, "r");
    if (!status_fp) {
        // Não é um erro fatal se não conseguir ler, mas os campos ficarão como 0
        metrics->voluntary_ctxt_switches = 0;
        metrics->nonvoluntary_ctxt_switches = 0;
        return 0;
    }

    int found_voluntary = parse_status_file(status_fp, "voluntary_ctxt_switches:", &metrics->voluntary_ctxt_switches);
    int found_nonvoluntary = parse_status_file(status_fp, "nonvoluntary_ctxt_switches:", &metrics->nonvoluntary_ctxt_switches);

    fclose(status_fp);

    // Se não encontrou, zera os valores
    if (!found_voluntary) metrics->voluntary_ctxt_switches = 0;
    if (!found_nonvoluntary) metrics->nonvoluntary_ctxt_switches = 0;

    return 0;
}
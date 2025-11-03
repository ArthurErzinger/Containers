#ifndef PROCESS_MONITOR_H
#define PROCESS_MONITOR_H

#include <stdio.h> // Para size_t

// Estrutura para armazenar as métricas coletadas de um processo
typedef struct {
    int pid;
    char comm[256];

    // Métricas de CPU (lidas do /proc/[pid]/stat e /proc/[pid]/status)
    unsigned long utime;       // Tempo de usuário (em jiffies)
    unsigned long stime;       // Tempo de sistema (em jiffies)
    unsigned long starttime_jiffies; // Tempo de início do processo (em jiffies desde o boot do sistema)
    unsigned long minflt;      // Minor page faults
    unsigned long majflt;      // Major page faults
    long num_threads;          // Número de threads
    long voluntary_ctxt_switches;
    long nonvoluntary_ctxt_switches;
    double app_uptime_seconds; // Tempo de vida do aplicativo em segundos

    // Métricas de Memória (lidas do /proc/[pid]/status)
    long vm_size;              // Memória virtual (VmSize) em KB
    long vm_rss;               // Memória residente (VmRSS) em KB
    long vm_swap;              // Memória em swap (VmSwap) em KB

    // Métricas de I/O (lidas do /proc/[pid]/io)
    unsigned long long rchar;  // Bytes lidos
    unsigned long long wchar;  // Bytes escritos
    unsigned long long syscr;  // Read syscalls
    unsigned long long syscw;  // Write syscalls

    // Métricas de Rede (lidas do /proc/[pid]/net/dev)
    unsigned long long net_rx_bytes;
    unsigned long long net_tx_bytes;
    unsigned long long net_rx_packets;
    unsigned long long net_tx_packets;
    long net_connections;

} ProcessMetrics;

/**
 * @brief Coleta as métricas de CPU, memória e I/O para um dado PID.
 *
 * @param pid O ID do processo a ser monitorado.
 * @param metrics Um ponteiro para a estrutura ProcessMetrics onde os dados serão armazenados.
 * @return 0 em caso de sucesso, -1 em caso de falha (ex: processo não encontrado).
 */
int get_process_metrics(int pid, ProcessMetrics *metrics);

/**
 * @brief Exibe as métricas de um processo que foram coletadas.
 *
 * @param metrics Um ponteiro para a estrutura ProcessMetrics preenchida.
 */
void print_process_metrics(const ProcessMetrics *metrics);


#endif // PROCESS_MONITOR_H

#ifndef MONITOR_H
#define MONITOR_H

#include <sys/types.h>  // pid_t

/**
 * Representa métricas de CPU obtidas via /proc/[pid]/stat e /proc/[pid]/status.
 */
typedef struct {
    pid_t pid;
    unsigned long utime;   // tempo em modo usuário (jiffies)
    unsigned long stime;   // tempo em modo kernel (jiffies)
    unsigned long cutime;  // tempo dos filhos em modo usuário (jiffies)
    unsigned long cstime;  // tempo dos filhos em modo kernel (jiffies)
    long num_threads;      // número de threads
    unsigned long minflt;  // minor page faults
    unsigned long majflt;  // major page faults
    long voluntary_ctxt_switches; // voluntary context switches
    long nonvoluntary_ctxt_switches; // non-voluntary context switches
} CpuMetrics;

/**
 * Representa métricas de memória obtidas via /proc/[pid]/status.
 */
typedef struct {
    pid_t pid;
    long vm_size_kb; // VmSize em KB
    long vm_rss_kb;  // VmRSS em KB
    long vm_swap_kb; // VmSwap em KB
} MemoryMetrics;

/**
 * Representa métricas de I/O obtidas via /proc/[pid]/io.
 */
typedef struct {
    pid_t pid;
    unsigned long long read_bytes;   // read_bytes do processo
    unsigned long long write_bytes;  // write_bytes do processo
    unsigned long long read_syscalls; // syscr (read syscalls)
    unsigned long long write_syscalls; // syscw (write syscalls)
} IoMetrics;

/**
 * Representa métricas de Rede agregadas de /proc/net/dev.
 */
typedef struct {
    pid_t pid;
    // Métricas de tráfego
    unsigned long long rx_bytes;   // Total de bytes recebidos
    unsigned long long tx_bytes;   // Total de bytes transmitidos
    unsigned long long rx_packets; // Total de pacotes recebidos
    unsigned long long tx_packets; // Total de pacotes transmitidos
} NetworkMetrics;

int get_cpu_metrics(pid_t pid, CpuMetrics *metrics);
int get_memory_metrics(pid_t pid, MemoryMetrics *metrics);
int get_io_metrics(pid_t pid, IoMetrics *metrics);
int get_network_metrics(pid_t pid, NetworkMetrics *metrics);
int get_global_cpu_time(unsigned long *total_time);

#endif // MONITOR_H

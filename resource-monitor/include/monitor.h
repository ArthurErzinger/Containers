#ifndef MONITOR_H
#define MONITOR_H

#include <sys/types.h>  // pid_t

/**
 * Representa métricas de CPU obtidas via /proc/[pid]/stat.
 */
typedef struct {
    pid_t pid;
    unsigned long utime;   // tempo em modo usuário (jiffies)
    unsigned long stime;   // tempo em modo kernel (jiffies)
    unsigned long cutime;  // tempo dos filhos em modo usuário (jiffies)
    unsigned long cstime;  // tempo dos filhos em modo kernel (jiffies)
    long num_threads;      // número de threads
} CpuMetrics;

/**
 * Representa métricas de memória obtidas via /proc/[pid]/status.
 */
typedef struct {
    pid_t pid;
    long vm_size_kb; // VmSize em KB
    long vm_rss_kb;  // VmRSS em KB
} MemoryMetrics;

/**
 * Representa métricas de I/O obtidas via /proc/[pid]/io.
 */
typedef struct {
    pid_t pid;
    unsigned long long read_bytes;   // read_bytes do processo
    unsigned long long write_bytes;  // write_bytes do processo
} IoMetrics;

int get_cpu_metrics(pid_t pid, CpuMetrics *metrics);
int get_memory_metrics(pid_t pid, MemoryMetrics *metrics);
int get_io_metrics(pid_t pid, IoMetrics *metrics);
int get_global_cpu_time(unsigned long *total_time);

#endif // MONITOR_H

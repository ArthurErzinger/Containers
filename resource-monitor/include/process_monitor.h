#ifndef PROCESS_MONITOR_H
#define PROCESS_MONITOR_H

#include <stdio.h> // Para size_t

// Estrutura para armazenar as métricas coletadas de um processo
typedef struct {
    int pid;

    // Métricas de CPU (lidas do /proc/[pid]/stat)
    unsigned long utime;       // Tempo de usuário (em jiffies)
    unsigned long stime;       // Tempo de sistema (em jiffies)
    long num_threads;          // Número de threads

    // Métricas de Memória (lidas do /proc/[pid]/status)
    long vm_size;              // Memória virtual (VmSize) em KB
    long vm_rss;               // Memória residente (VmRSS) em KB

    // Métricas de I/O (lidas do /proc/[pid]/io)
    unsigned long long rchar;  // Bytes lidos
    unsigned long long wchar;  // Bytes escritos

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

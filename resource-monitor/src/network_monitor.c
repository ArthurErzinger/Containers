#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <linux/limits.h>

#include "monitor.h"

/**
 * Coleta métricas de tráfego de rede (bytes e pacotes) do /proc/[pid]/net/dev.
 * Soma os valores de todas as interfaces, exceto 'lo' (loopback).
 */
int get_network_metrics(pid_t pid, NetworkMetrics *metrics) {
    if (metrics == NULL) {
        return -1;
    }

    // Inicializa a estrutura de métricas
    memset(metrics, 0, sizeof(NetworkMetrics));
    metrics->pid = pid;

    char path[PATH_MAX];
    snprintf(path, sizeof(path), "/proc/%d/net/dev", pid);

    FILE *file = fopen(path, "r");
    if (file == NULL) {
        // Não é necessariamente um erro fatal se o processo não tiver um namespace de rede (ex: threads de kernel)
        // perror("Não foi possível abrir /proc/[pid]/net/dev");
        return -1; // Retorna erro, mas o chamador pode decidir como lidar com isso
    }

    char line[256];
    // Pula as duas primeiras linhas (cabeçalho)
    if (fgets(line, sizeof(line), file) == NULL || fgets(line, sizeof(line), file) == NULL) {
        fclose(file);
        return -1;
    }

    unsigned long long rx_bytes, rx_packets, tx_bytes, tx_packets;
    char iface_name[32];

    while (fgets(line, sizeof(line), file) != NULL) {
        // Formato: interface: rx_bytes rx_packets ... tx_bytes tx_packets ...
        sscanf(line, " %31[^:]:%*s %llu %llu %*s %*s %*s %*s %*s %*s %llu %llu",
               iface_name, &rx_bytes, &rx_packets, &tx_bytes, &tx_packets);

        // Ignora a interface de loopback
        if (strcmp(iface_name, "lo") != 0) {
            metrics->rx_bytes += rx_bytes;
            metrics->rx_packets += rx_packets;
            metrics->tx_bytes += tx_bytes;
            metrics->tx_packets += tx_packets;
        }
    }

    fclose(file);
    return 0;
}

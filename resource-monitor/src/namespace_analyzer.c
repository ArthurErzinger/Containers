#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "namespace.h"

void list_process_namespaces(pid_t pid) {
    char ns_path[256];
    // Constrói o caminho para o diretório de namespaces do processo
    snprintf(ns_path, sizeof(ns_path), "/proc/%d/ns", pid);

    // Abre o diretório
    DIR *dir = opendir(ns_path);
    if (dir == NULL) {
        fprintf(stderr, "Erro ao abrir %s: ", ns_path);
        perror(NULL); // perror imprime a mensagem de erro do sistema (ex: No such file or directory)
        return;
    }

    printf("┌──────────────────────┬────────────────────────────────┐\n");
    printf("│ Namespaces para o processo PID %-8d               │\n", pid);
    printf("├──────────────────────┼────────────────────────────────┤\n");
    printf("│ TIPO                 │ ID (INODE)                     │\n");
    printf("├──────────────────────┼────────────────────────────────┤\n");

    struct dirent *entry;
    // Lê cada entrada no diretório
    while ((entry = readdir(dir)) != NULL) {
        // Ignora as entradas '.' e '..'
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char link_path[512];
        char link_target[256];

        // Constrói o caminho completo para o link simbólico
        snprintf(link_path, sizeof(link_path), "%s/%s", ns_path, entry->d_name);

        // Lê o destino do link simbólico (que contém o ID do namespace)
        ssize_t len = readlink(link_path, link_target, sizeof(link_target) - 1);
        if (len != -1) {
            link_target[len] = '\0'; // Adiciona o terminador nulo
            printf("│ %-20s │ %-30s │\n", entry->d_name, link_target);
        }
    }
    printf("└──────────────────────┴────────────────────────────────┘\n");

    closedir(dir);
}

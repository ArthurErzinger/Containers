#include "process_selector.h"
#include <stdio.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

int select_process() {
    DIR *dir;
    struct dirent *entry;
    char path[256];
    char comm[256];
    FILE *fp;

    printf("===== Lista de Processos em Execução =====\n");
    printf("% -10s %s\n", "PID", "Comando");
    printf("------------------------------------------\n");

    if ((dir = opendir("/proc")) == NULL) {
        perror("Erro ao abrir /proc");
        return -1;
    }

    while ((entry = readdir(dir)) != NULL) {
        // Verifica se o nome da entrada é um número (PID)
        int is_pid = 1;
        for (char *p = entry->d_name; *p; p++) {
            if (!isdigit(*p)) {
                is_pid = 0;
                break;
            }
        }

        if (is_pid) {
            // Constrói o caminho para o arquivo comm
            snprintf(path, sizeof(path), "/proc/%s/comm", entry->d_name);
            fp = fopen(path, "r");
            if (fp) {
                if (fgets(comm, sizeof(comm), fp) != NULL) {
                    // Remove a nova linha do final do nome do comando
                    comm[strcspn(comm, "\n")] = 0;
                    printf("% -10s %s\n", entry->d_name, comm);
                }
                fclose(fp);
            }
        }
    }
    closedir(dir);

    printf("------------------------------------------\n");
    printf("Digite o PID do processo que deseja monitorar: ");

    int selected_pid = -1;
    if (scanf("%d", &selected_pid) != 1) {
        fprintf(stderr, "Entrada inválida. Por favor, insira um número.\n");
        return -1;
    }

    return selected_pid;
}

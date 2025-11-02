#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "process_selector.h"
#include "process_monitor.h"

// --- Funções para exibir os menus ---

void print_main_menu() {
    printf("\n===========================\n");
    printf(" RESOURCE MONITOR SYSTEM\n");
    printf("===========================\n\n");
    printf("[1] Resource Profiler\n");
    printf("[2] Namespace Analyzer\n");
    printf("[3] Control Group Manager\n");
    printf("[4] Experimentos\n");
    printf("[5] Sair\n\n");
}

void print_profiler_submenu() {
    printf("\n  --- [1] Resource Profiler ---\n");
    printf("    ├─ [1] Monitorar processo por PID\n");
    printf("    ├─ [2] Exibir métricas de CPU, Memória e I/O\n");
    printf("    ├─ [3] Calcular percentuais de uso\n");
    printf("    ├─ [4] Exportar métricas em CSV\n");
    printf("    └─ [5] Voltar ao menu principal\n\n");
}

void print_namespace_submenu() {
    printf("\n  --- [2] Namespace Analyzer ---\n");
    printf("    ├─ [1] Listar namespaces de um processo\n");
    printf("    ├─ [2] Listar processos em um namespace\n");
    printf("    ├─ [3] Comparar namespaces entre dois processos\n");
    printf("    ├─ [4] Gerar relatório geral de namespaces\n");
    printf("    └─ [5] Voltar ao menu principal\n\n");
}

void print_cgroup_submenu() {
    printf("\n  --- [3] Control Group Manager ---\n");
    printf("    ├─ [1] Listar métricas de CPU, Memória e BlkIO\n");
    printf("    ├─ [2] Criar cgroup experimental\n");
    printf("    ├─ [3] Mover processo para cgroup\n");
    printf("    ├─ [4] Aplicar limites de CPU e Memória\n");
    printf("    ├─ [5] Gerar relatório de utilização\n");
    printf("    └─ [6] Voltar ao menu principal\n\n");
}

void print_experiments_submenu() {
     printf("\n  --- [4] Experimentos ---\n");
     printf("    ├─ [1] Overhead de monitoramento\n");
     printf("    ├─ [2] Isolamento via namespaces\n");
     printf("    ├─ [3] Throttling de CPU\n");
     printf("    ├─ [4] Limitação de memória\n");
     printf("    └─ [5] Limitação de I/O\n\n");
}

// --- Funções para lidar com a lógica dos submenus ---

void handle_profiler_menu() {
    int choice = 0;
    while (choice != 5) {
        print_profiler_submenu();
        printf("Opção do Profiler: ");
        scanf("%d", &choice);

        if (choice == 1) {
            int selected_pid = select_process();
            if (selected_pid != -1) {
                ProcessMetrics metrics;
                if (get_process_metrics(selected_pid, &metrics) == 0) {
                    print_process_metrics(&metrics);
                } else {
                    fprintf(stderr, "\nErro ao coletar métricas para o PID %d. O processo pode não existir mais.\n", selected_pid);
                }
                printf("\nPressione Enter para continuar...");
                while(getchar() != '\n'); // Limpa o buffer de entrada antigo
                getchar(); // Espera pelo Enter
            }
        } else if (choice > 1 && choice <= 4) {
            printf("\n[AVISO] Opção 1.%d não implementada.\n", choice);
            sleep(2);
        } else if (choice != 5) {
            printf("Opção inválida.\n");
            sleep(1);
        }
    }
}

void handle_namespace_menu() {
    int choice = 0;
    while (choice != 5) {
        print_namespace_submenu();
        printf("Opção do Namespace Analyzer: ");
        scanf("%d", &choice);
        if (choice >= 1 && choice <= 4) {
            printf("\n[AVISO] Opção 2.%d não implementada.\n", choice);
            sleep(2);
        } else if (choice != 5) {
            printf("Opção inválida.\n");
            sleep(1);
        }
    }
}

void handle_cgroup_menu() {
    int choice = 0;
    while (choice != 6) {
        print_cgroup_submenu();
        printf("Opção do CGroup Manager: ");
        scanf("%d", &choice);
        if (choice >= 1 && choice <= 5) {
            printf("\n[AVISO] Opção 3.%d não implementada.\n", choice);
            sleep(2);
        } else if (choice != 6) {
            printf("Opção inválida.\n");
            sleep(1);
        }
    }
}

void handle_experiments_menu() {
     printf("\n[AVISO] Menu de Experimentos não implementado.\n");
     sleep(2);
}


// --- Função Principal ---

int main() {
    int choice = 0;
    while (choice != 5) {
        print_main_menu();
        printf("Opção principal: ");
        if (scanf("%d", &choice) != 1) {
            while (getchar() != '\n'); // Limpa buffer
            choice = -1;
        }

        switch (choice) {
            case 1:
                handle_profiler_menu();
                break;
            case 2:
                handle_namespace_menu();
                break;
            case 3:
                handle_cgroup_menu();
                break;
            case 4:
                handle_experiments_menu();
                break;
            case 5:
                printf("Saindo...\n");
                break;
            default:
                printf("Opção inválida. Tente novamente.\n");
                sleep(1);
                break;
        }
    }
    return 0;
}
#ifndef PROCESS_TREE_H
#define PROCESS_TREE_H

#include <sys/types.h>

// Estrutura para armazenar uma lista de PIDs
typedef struct {
    pid_t *pids;
    int count;
} PidList;

/**
 * Encontra todos os PIDs filhos (descendentes) de um dado PID pai.
 * A função aloca memória para a lista de PIDs, que deve ser liberada pelo chamador.
 *
 * @param parent_pid O PID do processo pai.
 * @return Uma estrutura PidList contendo a lista de PIDs filhos e sua contagem.
 *         Retorna uma lista com count = 0 se não houver filhos ou em caso de erro.
 */
PidList find_child_pids(pid_t parent_pid);

/**
 * Libera a memória alocada para uma PidList.
 *
 * @param pid_list A lista de PIDs a ser liberada.
 */
void free_pid_list(PidList *pid_list);

#endif // PROCESS_TREE_H

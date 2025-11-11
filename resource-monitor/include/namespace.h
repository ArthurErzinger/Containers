#ifndef NAMESPACE_H
#define NAMESPACE_H

#include <sys/types.h>

/**
 * @brief Lista todos os namespaces associados a um determinado processo.
 * 
 * Esta função lê o diretório /proc/[pid]/ns e imprime o tipo e o inode
 * de cada namespace ao qual o processo pertence.
 * 
 * @param pid O ID do processo a ser analisado.
 */
void list_process_namespaces(pid_t pid);

/**
 * @brief Encontra e lista todos os processos em um namespace específico.
 * 
 * Pede ao usuário para selecionar um tipo de namespace e fornecer um número de inode.
 * Em seguida, varre o sistema de arquivos /proc para encontrar todos os processos
 * que correspondem a esse namespace e imprime seus PIDs.
 */
void find_processes_in_namespace(void);

/**
 * @brief Compara os namespaces entre dois processos.
 * 
 * Para cada tipo de namespace padrão, esta função obtém o inode do namespace
 * para cada um dos dois PIDs e imprime uma tabela mostrando se eles são
 * compartilhados ou não.
 * 
 * @param pid1 O ID do primeiro processo.
 * @param pid2 O ID do segundo processo.
 */
void compare_process_namespaces(pid_t pid1, pid_t pid2);

/**
 * @brief Gera um relatório de todos os namespaces ativos no sistema.
 * 
 * Varre o sistema de arquivos /proc para encontrar todos os namespaces
 * únicos de cada tipo e imprime um resumo.
 */
void generate_system_namespace_report(void);



#endif //NAMESPACE_H

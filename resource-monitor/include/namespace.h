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

#endif //NAMESPACE_H

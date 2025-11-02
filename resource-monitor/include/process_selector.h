#ifndef PROCESS_SELECTOR_H
#define PROCESS_SELECTOR_H

/**
 * @brief Lista todos os processos em execução, solicita que o usuário escolha um e retorna o PID selecionado.
 *
 * Esta função lê o diretório /proc para encontrar todos os processos em execução,
 * exibe uma lista de PIDs e nomes de processos para o usuário, e então
 * solicita que o usuário insira o PID do processo que deseja monitorar.
 *
 * @return O PID (Process ID) do processo selecionado pelo usuário. Retorna -1 em caso de erro.
 */
int select_process();

#endif // PROCESS_SELECTOR_H


# Os 7 Namespaces Padrão do Linux

Aqui está a lista dos 7 namespaces padrão do Linux, com uma descrição do que cada um isola e um exemplo prático.

---

### 1. Mount (mnt)
*   **Descrição:** Isola os pontos de montagem do sistema de arquivos. Cada namespace de montagem tem seu próprio conjunto de "montagens", o que significa que processos em namespaces de montagem diferentes podem ter visões completamente distintas da estrutura de diretórios e dos dispositivos montados.
*   **Exemplo Prático:** É a base do sistema de arquivos de um container. Um container pode ter seu próprio diretório raiz (`/`) e montar sistemas de arquivos específicos (ex: `/app`, `/data`) que não são visíveis para o sistema hospedeiro ou para outros containers.

### 2. UTS (UNIX Time-sharing System)
*   **Descrição:** Isola o nome do host (`hostname`) e o nome de domínio NIS (Network Information Service). Processos em namespaces UTS diferentes podem ter nomes de máquina diferentes.
*   **Exemplo Prático:** Um container pode ter o hostname `meu-banco-de-dados` enquanto a máquina hospedeira se chama `servidor-producao-01`.

### 3. IPC (Inter-Process Communication)
*   **Descrição:** Isola recursos de comunicação entre processos, como semáforos, filas de mensagens e segmentos de memória compartilhada (System V IPC).
*   **Exemplo Prático:** Duas aplicações diferentes rodando no mesmo host podem usar os mesmos identificadores de memória compartilhada sem entrar em conflito, pois cada uma opera em seu próprio namespace IPC isolado.

### 4. PID (Process ID)
*   **Descrição:** Isola o espaço de identificação de processos (PIDs). Um processo em um novo namespace PID pode ter o PID 1, tornando-se o "processo init" daquele namespace. Ele só enxerga os processos que estão no mesmo namespace PID ou em namespaces filhos.
*   **Exemplo Prático:** Dentro de um container, o primeiro processo executado geralmente tem o PID 1. Se você executar o comando `ps aux` dentro do container, verá apenas os processos do próprio container, e não todos os processos da máquina hospedeira.

### 5. Network (net)
*   **Descrição:** Isola os recursos de rede. Cada namespace de rede tem seu próprio conjunto de interfaces de rede (ex: `lo`, `eth0`), sua própria tabela de roteamento, regras de firewall (iptables) e seu próprio espaço de portas.
*   **Exemplo Prático:** É por isso que dois containers na mesma máquina podem, ambos, escutar na porta 80. Cada um tem seu próprio "endereço IP" e seu próprio conjunto de portas, completamente isolado do outro.

### 6. User
*   **Descrição:** Isola os identificadores de usuário e grupo (UIDs e GIDs). Isso permite que um processo tenha privilégios de root (UID 0) dentro do seu namespace, enquanto é mapeado para um usuário sem privilégios no sistema hospedeiro.
*   **Exemplo Prático:** É um recurso de segurança fundamental para "rootless containers". Um processo dentro de um container pode rodar como `root` para instalar pacotes ou vincular-se a portas baixas, mas para o sistema hospedeiro, ele é apenas um processo de um usuário comum, sem permissão para danificar o sistema real.

### 7. Cgroup (Control Group)
*   **Descrição:** Isola a visão da hierarquia de cgroups. Um processo dentro de um namespace de cgroup terá uma visão da sua hierarquia de cgroups como se estivesse no diretório raiz (`/`), em vez de ver o caminho completo no sistema hospedeiro.
*   **Exemplo Prático:** Útil em cenários de virtualização aninhada (container dentro de container). O container interno pode gerenciar seus próprios limites de recursos sem precisar saber sobre a estrutura de cgroups do container externo ou do host.

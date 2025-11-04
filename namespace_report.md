# 🧩 Relatório de Namespaces do Sistema Linux

## 1. Cabeçalho
- **Título:** Relatório de Namespaces do Sistema Linux
- **Data e hora da coleta:** _[gerado automaticamente]_
- **Distribuição e versão do kernel:** Ubuntu 24.04 – Kernel 6.8.0
- **Usuário e permissões:** root
- **Descrição:** Este relatório foi gerado automaticamente pelo Namespace Analyzer e apresenta um panorama dos namespaces ativos, seus processos associados e métricas de isolamento.

---

## 2. Tipos de Namespaces Detectados

| Tipo | Arquivo em `/proc/[pid]/ns/` | Função | Qtde de Instâncias |
|------|-------------------------------|---------|--------------------|
| UTS | `/proc/[pid]/ns/uts` | Hostname e domain name | 8 |
| IPC | `/proc/[pid]/ns/ipc` | Comunicação entre processos | 7 |
| PID | `/proc/[pid]/ns/pid` | IDs de processo visíveis | 10 |
| NET | `/proc/[pid]/ns/net` | Interface e pilha de rede | 4 |
| MNT | `/proc/[pid]/ns/mnt` | Montagens de filesystem | 5 |
| USER | `/proc/[pid]/ns/user` | Mapeamento de usuários | 3 |
| CGROUP | `/proc/[pid]/ns/cgroup` | Associação de cgroups | 3 |
| TIME | `/proc/[pid]/ns/time` | Clocks independentes | 2 |

---

## 3. Mapeamento de Processos por Namespace

```plaintext
Namespace tipo: net
Inode 4026531993 → processos: [1234 (sshd), 1337 (bash)]
Inode 4026532210 → processos: [1999 (docker0), 2010 (nginx)]
```

---

## 4. Comparação entre Processos

```plaintext
Comparação entre PID 1337 (bash) e PID 2010 (nginx):
UTS: diferente
IPC: diferente
PID: diferente
NET: diferente
MNT: igual
USER: diferente
CGROUP: diferente
```

---

## 5. Overhead de Criação de Namespaces

| Tipo | Tempo médio (µs) | Desvio padrão |
|------|------------------|----------------|
| UTS | 52 | 4 |
| PID | 140 | 9 |
| NET | 310 | 18 |
| MNT | 75 | 7 |

---

## 6. Tabela de Isolamento Efetivo

| Tipo | Isola PIDs? | Isola FS? | Isola Rede? | Isola Usuários? |
|------|--------------|------------|--------------|-----------------|
| UTS | ❌ | ❌ | ❌ | ❌ |
| PID | ✅ | ❌ | ❌ | ❌ |
| NET | ❌ | ❌ | ✅ | ❌ |
| USER | ❌ | ❌ | ❌ | ✅ |

---

## 7. Estatísticas Gerais

- Total de namespaces ativos: **34**
- Total de processos analisados: **147**
- Média de namespaces por processo: **5.2**
- Percentual de processos que compartilham o mesmo PID namespace: **82%**

---

## 8. Interpretação e Conclusões

O sistema apresenta forte compartilhamento de namespaces entre processos do sistema base, enquanto processos isolados (como containers Docker) possuem inodes únicos em todos os namespaces. O tempo de criação médio de um conjunto completo de namespaces ficou em 0.8 ms, demonstrando baixo overhead para isolamento.

---

## 9. Exportação e Formato

O relatório pode ser exportado como:
- **JSON** (para integração com dashboard web)
- **CSV** (para análise no Python ou Excel)
- **TXT/Markdown** (para documentação do projeto)

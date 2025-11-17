# Experimentos Obrigatórios - Resource Monitor

Este diretório contém os resultados dos **5 Experimentos Obrigatórios** conforme especificado no projeto.

## 📂 Estrutura

```
experiments/
├── README.md (este arquivo)
├── exp1_overhead/         - Experimento 1: Overhead de Monitoramento
├── exp2_namespaces/       - Experimento 2: Isolamento via Namespaces
├── exp3_cpu_throttling/   - Experimento 3: Throttling de CPU
├── exp4_memory_limit/     - Experimento 4: Limitação de Memória
├── exp5_io_limit/         - Experimento 5: Limitação de I/O
```

## 🚀 Como Executar os Experimentos

### Opção 1: Via Menu Interativo

```bash
./resource-monitor
# Escolha opção [4] Executar Experimentos
```

### Opção 2: Via Linha de Comando

```bash
# Executar TODOS os experimentos
make experimentos

# Ou executar individualmente:
make exp1  # Experimento 1: Overhead
make exp2  # Experimento 2: Namespaces
sudo make exp3  # Experimento 3: CPU Throttling (requer sudo)
sudo make exp4  # Experimento 4: Memory Limit (requer sudo)
sudo make exp5  # Experimento 5: I/O Limit (requer sudo)
```

### Opção 3: Executar Scripts Diretamente

```bash
bash scripts/experimento1_overhead.sh
bash scripts/experimento2_namespaces.sh
sudo bash scripts/experimento3_cpu_throttle.sh
sudo bash scripts/experimento4_mem_limit.sh
sudo bash scripts/experimento5_io_limit.sh
```

## 📊 Descrição dos Experimentos

### Experimento 1: Overhead de Monitoramento

**Objetivo:** Medir o impacto do próprio profiler no sistema

**Procedimento:**
1. Executar workload sem monitoramento (baseline)
2. Executar workload com monitoramento em diferentes intervalos (1s, 3s, 5s)
3. Comparar tempos de execução e calcular overhead

**Métricas Reportadas:**
- Tempo de execução com e sem profiler
- CPU overhead (%)
- Latência de sampling

**Arquivos Gerados:**
- `exp1_overhead/relatorio.txt` - Relatório completo
- `exp1_overhead/metrics_*.csv` - Dados de métricas coletadas

---

### Experimento 2: Isolamento via Namespaces

**Objetivo:** Validar efetividade do isolamento via namespaces

**Procedimento:**
1. Criar processos com diferentes tipos de namespaces
2. Verificar visibilidade de recursos (PIDs, rede, filesystems)
3. Medir tempo de criação de cada tipo de namespace

**Métricas Reportadas:**
- Tabela de isolamento efetivo por tipo de namespace
- Overhead de criação (µs)
- Número de processos por namespace no sistema

**Arquivos Gerados:**
- `exp2_namespaces/relatorio.txt` - Relatório completo
- `exp2_namespaces/namespaces_sistema.json` - Snapshot do sistema

---

### Experimento 3: Throttling de CPU

**Objetivo:** Avaliar precisão de limitação de CPU via cgroups

**Procedimento:**
1. Executar processo CPU-intensive sem limite (baseline)
2. Aplicar limites de 25%, 50%, 100% e 200% de CPU
3. Medir CPU usage real e throughput em cada configuração

**Métricas Reportadas:**
- CPU% medido vs limite configurado
- Desvio percentual
- Throughput (iterações/segundo) em cada configuração

**Arquivos Gerados:**
- `exp3_cpu_throttling/relatorio.txt` - Relatório completo com tabela de resultados

**⚠️ Requer sudo**

---

### Experimento 4: Limitação de Memória

**Objetivo:** Testar comportamento ao atingir limite de memória

**Procedimento:**
1. Criar cgroup com limite de 100MB
2. Tentar alocar 200MB incrementalmente
3. Observar comportamento (OOM killer, falhas de alocação)

**Métricas Reportadas:**
- Quantidade máxima alocada
- Número de falhas (memory.failcnt)
- Comportamento do sistema ao atingir limite

**Arquivos Gerados:**
- `exp4_memory_limit/relatorio.txt` - Relatório completo
- `exp4_memory_limit/mem_output.txt` - Output do workload

**⚠️ Requer sudo**

---

### Experimento 5: Limitação de I/O

**Objetivo:** Avaliar precisão de limitação de I/O via cgroups

**Procedimento:**
1. Executar workload I/O-intensive sem limite (baseline)
2. Aplicar limites de 10MB/s, 5MB/s e 1MB/s
3. Medir throughput real em cada configuração

**Métricas Reportadas:**
- Throughput medido vs limite configurado
- Latência de I/O
- Impacto no tempo total de execução

**Arquivos Gerados:**
- `exp5_io_limit/relatorio.txt` - Relatório completo
- `exp5_io_limit/io_limit_*.txt` - Output de cada teste

**⚠️ Requer sudo**

---

## 📝 Notas Importantes

1. **Permissões:** Experimentos 3, 4 e 5 requerem privilégios de root (sudo)
2. **Duração:** Cada experimento leva entre 1-3 minutos para completar
3. **Cgroup Version:** Os scripts detectam automaticamente se o sistema usa cgroup v1 ou v2
4. **Limpeza:** Todos os scripts limpam recursos (cgroups, processos) automaticamente ao final
5. **Relatórios:** Relatórios são salvos em formato texto com tabelas formatadas

## 🔧 Troubleshooting

### "Permission denied" ao criar cgroup
- **Solução:** Execute com sudo (experimentos 3, 4, 5)

### "Cgroup version unknown"
- **Solução:** Verifique se o sistema tem cgroups habilitados: `ls /sys/fs/cgroup`

### Workloads não compilados
- **Solução:** Execute `make workloads` antes dos experimentos

### Script não encontrado
- **Solução:** Certifique-se de estar no diretório `resource-monitor/`

---

**Data de Criação:** $(date '+%Y-%m-%d')
**Versão:** 1.0

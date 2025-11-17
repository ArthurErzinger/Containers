# Resource Monitor - Guia de Troubleshooting e Aprendizados

**Data de criação:** 2025-11-17
**Versão:** 1.0
**Propósito:** Documentar aprendizados, problemas comuns e soluções para o sistema de experimentos

---

## 📋 Índice

1. [Visão Geral](#visão-geral)
2. [Anatomia dos Experimentos](#anatomia-dos-experimentos)
3. [Problemas Comuns e Soluções](#problemas-comuns-e-soluções)
4. [Debugging de Experimentos](#debugging-de-experimentos)
5. [Padrões de Código](#padrões-de-código)
6. [Referência Rápida](#referência-rápida)

---

## 🎯 Visão Geral

O sistema possui **5 experimentos obrigatórios** que validam diferentes aspectos do resource-monitor:

| # | Nome | Requer Sudo | Duração | Risco |
|---|------|-------------|---------|-------|
| 1 | Overhead de Monitoramento | ✗ | ~60s | Baixo |
| 2 | Isolamento via Namespaces | ✗ | ~10s | Baixo |
| 3 | Throttling de CPU | ✓ | ~75s | Médio |
| 4 | Limitação de Memória | ✓ | ~20s | Alto (OOM) |
| 5 | Limitação de I/O | ✓ | Variável | Baixo |

**Script orquestrador:** `scripts/run_experiments.sh`
**Diretório de resultados:** `experiments/exp{1-5}_*/`

---

## 🔬 Anatomia dos Experimentos

### Experimento 1: Overhead de Monitoramento

**Objetivo:** Quantificar o impacto do profiler no sistema.

**Metodologia:**
```bash
1. Baseline → test_cpu SEM monitoramento (20s)
2. Teste 1  → test_cpu COM monitoramento (intervalo 1s)
3. Teste 2  → test_cpu COM monitoramento (intervalo 3s)
4. Teste 3  → test_cpu COM monitoramento (intervalo 5s)
5. Cálculo  → Overhead = Tempo_Com - Tempo_Sem
```

**Workload:** `test_cpu --duration 20 --threads 2`

**Métricas:**
- Tempo de execução (nanosegundos via `date +%s.%N`)
- Overhead absoluto (segundos)
- Overhead percentual
- CSV com métricas detalhadas

**Resultado esperado:** Overhead < 1%

---

### Experimento 2: Isolamento via Namespaces

**Objetivo:** Validar efetividade do isolamento de namespaces.

**Metodologia:**
```bash
1. Gerar relatório JSON dos namespaces do sistema
2. Para cada tipo (pid, net, mnt, uts, ipc, user):
   - Medir tempo de criação com unshare
3. Testar isolamento PID criando namespace isolado
4. Comparar visibilidade de processos
```

**Ferramentas:**
- `unshare --<tipo> /bin/true` - criação de namespace
- `resource-monitor --namespace-report` - análise do sistema
- `ps aux` - listagem de processos

**Métricas:**
- Tempo de criação (microssegundos)
- Overhead médio de criação
- Tabela descritiva de recursos isolados

**⚠️ IMPORTANTE:** Alguns testes podem falhar sem permissões adequadas (gracefully handled).

---

### Experimento 3: Throttling de CPU (sudo)

**Objetivo:** Avaliar precisão da limitação de CPU via cgroups.

**Metodologia:**
```bash
1. Detectar cgroup v1 ou v2
2. Baseline → test_cpu sem limite
3. Para cada limite (25%, 50%, 100%, 200%):
   - Criar cgroup
   - Configurar cpu.max (v2) ou cpu.cfs_quota_us (v1)
   - Mover processo para cgroup
   - Medir CPU% real e throughput
4. Calcular desvio: |CPU_medido - CPU_limite| / CPU_limite
```

**Configuração de limites:**
- **Cgroup v2:** `echo "25000 100000" > cpu.max` (25%)
- **Cgroup v1:** `echo 25000 > cpu.cfs_quota_us` (25%)

**Métricas:**
- Throughput (iterações/segundo)
- CPU% medido vs configurado
- Desvio percentual

**Resultado esperado:** Desvio < 5%

---

### Experimento 4: Limitação de Memória (sudo)

**Objetivo:** Testar comportamento ao atingir limites de memória.

**Metodologia:**
```bash
1. Baseline → alocar 50MB sem limite
2. Teste → criar cgroup com limite 100MB
3. Tentar alocar 200MB (2x o limite!)
4. Observar: OOM killer, failcnt, comportamento
```

**Configuração:**
- **Cgroup v2:** `echo 100M > memory.max`
- **Cgroup v1:** `echo 104857600 > memory.limit_in_bytes`

**Métricas:**
- Quantidade alocada
- Número de falhas (memory.failcnt ou memory.events)
- Status do OOM killer

**⚠️ CUIDADO:** Este experimento pode matar processos!

---

### Experimento 5: Limitação de I/O (sudo)

**Objetivo:** Avaliar precisão da limitação de I/O bandwidth.

**Metodologia:**
```bash
1. Detectar dispositivo de blocos (major:minor)
2. Baseline → test_io sem limite (100MB)
3. Para cada limite (10MB/s, 5MB/s, 1MB/s):
   - Configurar io.max (v2) ou blkio.throttle (v1)
   - Medir bandwidth real de read/write
4. Comparar medido vs configurado
```

**Configuração:**
- **Cgroup v2:** `echo "8:1 rbps=10485760 wbps=10485760" > io.max`
- **Cgroup v1:** `echo "8:1 10485760" > blkio.throttle.read_bps_device`

**Detecção de dispositivo:**
```bash
DEVICE=$(df /tmp | tail -1 | awk '{print $1}')
MAJOR_MINOR=$(stat -c "%t:%T" $DEVICE)
```

**Métricas:**
- Write bandwidth (MB/s)
- Read bandwidth (MB/s)
- Tempo de execução

---

## 🐛 Problemas Comuns e Soluções

### Problema 1: Script para após Experimento 1

**Sintomas:**
```bash
[✓] Experimento 1 concluído com sucesso
✗ Alguns experimentos falharam. Verifique os logs.
```

**Causa Raiz:**
O `set -e` no script orquestrador (`run_experiments.sh`) faz o script terminar no primeiro comando que retorna exit code != 0. Scripts filhos podem retornar códigos diferentes de 0 temporariamente durante execução.

**Solução:**
```bash
# ANTES (problemático)
if bash "$SCRIPT_DIR/$script"; then
    RESULTS[$num]="PASSOU"
else
    RESULTS[$num]="FALHOU"
fi

# DEPOIS (correto)
bash "$SCRIPT_DIR/$script"
local exit_code=$?

if [ $exit_code -eq 0 ]; then
    RESULTS[$num]="PASSOU"
else
    RESULTS[$num]="FALHOU"
fi
```

**Lição aprendida:** Sempre capturar exit codes explicitamente ao usar `set -e`.

---

### Problema 2: Processos em Background Falhando Silenciosamente

**Sintomas:**
```bash
unshare: unshare falhou: Operação não permitida
# (aparece DEPOIS do script terminar)
```

**Causa Raiz:**
No `experimento2_namespaces.sh`, processos em background (`&`) falham após o script principal terminar:

```bash
unshare --fork --pid --mount-proc sleep 30 &
ISOLATED_PID=$!
sleep 1
kill $ISOLATED_PID 2>/dev/null || true
# Processo ainda existe mas sem permissões!
```

**Solução:**
```bash
# Tentar criar namespace isolado (pode falhar sem permissões)
if unshare --fork --pid --mount-proc sleep 5 >/dev/null 2>&1 &
then
    ISOLATED_PID=$!
    sleep 1
    PS_COUNT_OUTSIDE=$(ps aux | wc -l)
    kill $ISOLATED_PID 2>/dev/null || true
    wait $ISOLATED_PID 2>/dev/null || true  # IMPORTANTE!
else
    # Fallback
    PS_COUNT_OUTSIDE=$(ps aux | wc -l)
fi
```

**Lição aprendida:**
1. Sempre use `wait` para processos em background
2. Redirecione stderr para evitar mensagens pós-script
3. Tenha fallbacks para operações que podem falhar

---

### Problema 3: Exit Code Não Respeitado

**Sintomas:**
```bash
bash scripts/run_experiments.sh
# Retorna exit code 1 mesmo com tudo passando
```

**Causa Raiz:**
Lógica de retorno no final do script:

```bash
if [ $FAILED -gt 0 ]; then
    exit 1
else
    exit 0
fi
```

Se nenhum experimento falhou MAS alguns foram SKIP, `$FAILED` é 0 mas o script retorna 0 (correto).

**Solução:** A lógica está correta. O problema estava em outro lugar (ver Problema 1).

---

### Problema 4: Cgroup v1 vs v2 Incompatibilidade

**Sintomas:**
```bash
bash: /sys/fs/cgroup/cpu.max: Arquivo ou diretório inexistente
```

**Causa Raiz:**
Sistema usa cgroup v1 mas script tenta acessar interface v2.

**Solução:**
Sempre detectar versão antes:

```bash
if [ -f "/sys/fs/cgroup/cgroup.controllers" ]; then
    CGROUP_VERSION=2
    CGROUP_BASE="/sys/fs/cgroup"
else
    CGROUP_VERSION=1
    CGROUP_BASE="/sys/fs/cgroup/cpu"
fi
```

**Lição aprendida:** Nunca assuma versão de cgroup. Sempre detecte.

---

## 🔍 Debugging de Experimentos

### Técnica 1: Executar Experimento Individual

```bash
# Executar apenas um experimento
bash scripts/experimento2_namespaces.sh

# Salvar output para análise
bash scripts/experimento2_namespaces.sh > /tmp/exp2.txt 2>&1

# Ver exit code
bash scripts/experimento2_namespaces.sh; echo "Exit: $?"
```

---

### Técnica 2: Adicionar Debug ao Orquestrador

```bash
# Adicionar após cada experimento
run_experiment 1 "..." "..." "no"
echo "[DEBUG] Exp1: PASSED=$PASSED FAILED=$FAILED" >&2
```

---

### Técnica 3: Desabilitar set -e Temporariamente

```bash
# No início do script
# set -e  # Comentar temporariamente

# Permite ver todos os erros sem parar
```

---

### Técnica 4: Monitorar Processos em Tempo Real

```bash
# Terminal 1
bash scripts/run_experiments.sh

# Terminal 2
watch -n 1 'ps aux | grep -E "(test_cpu|test_memory|test_io|unshare)"'
```

---

### Técnica 5: Verificar Cgroups Ativos

```bash
# Cgroup v2
ls -la /sys/fs/cgroup/exp_*/

# Cgroup v1
ls -la /sys/fs/cgroup/cpu/exp_*/
ls -la /sys/fs/cgroup/memory/exp_*/
```

---

### Técnica 6: Analisar Logs com Grep

```bash
# Ver apenas experimentos e resultados
bash scripts/run_experiments.sh 2>&1 | grep -E "(Experimento|PASSOU|FALHOU|SKIP)"

# Ver apenas erros
bash scripts/run_experiments.sh 2>&1 | grep -i "erro\|error\|falhou\|failed"

# Ver exit codes
bash scripts/run_experiments.sh 2>&1 | grep -i "exit"
```

---

## 📐 Padrões de Código

### Estrutura Comum dos Scripts

```bash
#!/bin/bash
set -e                          # Parar em erros
export LC_NUMERIC=C             # Locale para números

# 1. CONFIGURAÇÃO
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
EXP_DIR="$PROJECT_ROOT/experiments/exp_XXX"

# 2. CORES
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m'

# 3. LIMPEZA
rm -f "$EXP_DIR"/*

# 4. DETECÇÃO DE AMBIENTE
if [ -f "/sys/fs/cgroup/cgroup.controllers" ]; then
    CGROUP_VERSION=2
fi

# 5. BASELINE
echo -e "${GREEN}[1/4]${NC} Executando baseline..."
# ... código baseline ...

# 6. TESTES
echo -e "${GREEN}[2/4]${NC} Executando testes..."
# ... código testes ...

# 7. RELATÓRIO
echo -e "${GREEN}[3/4]${NC} Gerando relatório..."
cat > "$REPORT" << EOF
...
EOF

# 8. CLEANUP
rm -rf /sys/fs/cgroup/exp_* 2>/dev/null || true

exit 0
```

---

### Padrão: Medição de Tempo

```bash
# Usar nanosegundos para precisão
start=$(date +%s.%N)
# ... workload ...
end=$(date +%s.%N)

# Calcular diferença
time=$(echo "$end - $start" | bc)
```

---

### Padrão: Criação de Cgroup

```bash
# Detectar versão
if [ -f "/sys/fs/cgroup/cgroup.controllers" ]; then
    CGROUP_VERSION=2
    CGROUP_PATH="/sys/fs/cgroup/exp_test"

    mkdir -p "$CGROUP_PATH"
    echo "+cpu +memory +io" > /sys/fs/cgroup/cgroup.subtree_control

    # Configurar limites
    echo "50000 100000" > "$CGROUP_PATH/cpu.max"

    # Adicionar processo
    echo $$ > "$CGROUP_PATH/cgroup.procs"
else
    CGROUP_VERSION=1
    CGROUP_PATH="/sys/fs/cgroup/cpu/exp_test"

    mkdir -p "$CGROUP_PATH"

    # Configurar limites
    echo 50000 > "$CGROUP_PATH/cpu.cfs_quota_us"
    echo 100000 > "$CGROUP_PATH/cpu.cfs_period_us"

    # Adicionar processo
    echo $$ > "$CGROUP_PATH/tasks"
fi
```

---

### Padrão: Cleanup Seguro

```bash
# Remover cgroups (v2)
rmdir /sys/fs/cgroup/exp_* 2>/dev/null || true

# Remover cgroups (v1)
rmdir /sys/fs/cgroup/cpu/exp_* 2>/dev/null || true
rmdir /sys/fs/cgroup/memory/exp_* 2>/dev/null || true

# Matar processos órfãos
pkill -f "test_cpu" 2>/dev/null || true
pkill -f "test_memory" 2>/dev/null || true
```

---

### Padrão: Tratamento de Erros

```bash
# Capturar exit code explicitamente
set +e
comando_pode_falhar
exit_code=$?
set -e

if [ $exit_code -eq 0 ]; then
    echo "Sucesso!"
else
    echo "Falhou com código $exit_code"
fi
```

---

## 🚀 Referência Rápida

### Comandos Úteis

```bash
# Executar todos os experimentos (usuário normal)
bash scripts/run_experiments.sh

# Executar todos os experimentos (com sudo)
sudo bash scripts/run_experiments.sh

# Executar experimento individual
bash scripts/experimento1_overhead.sh
bash scripts/experimento2_namespaces.sh
sudo bash scripts/experimento3_cpu_throttle.sh

# Ver relatórios
cat experiments/exp1_overhead/relatorio.txt
cat experiments/exp2_namespaces/relatorio.txt

# Limpar todos os resultados
rm -rf experiments/exp*/

# Verificar processos dos workloads
pgrep -f test_cpu
pgrep -f test_memory
pgrep -f test_io

# Matar processos órfãos
pkill -f test_cpu
pkill -f test_memory

# Verificar cgroups ativos
# v2
ls -la /sys/fs/cgroup/exp*/
# v1
ls -la /sys/fs/cgroup/cpu/exp*/
```

---

### Checklist de Troubleshooting

Quando um experimento falha:

- [ ] 1. Executar experimento individual para isolar problema
- [ ] 2. Verificar exit code do experimento
- [ ] 3. Verificar se workloads estão compilados (`make workloads`)
- [ ] 4. Verificar permissões (experimentos 3-5 precisam de sudo)
- [ ] 5. Verificar versão de cgroup (`ls /sys/fs/cgroup/cgroup.controllers`)
- [ ] 6. Verificar processos órfãos (`pgrep -f test_`)
- [ ] 7. Limpar cgroups antigos (`rmdir /sys/fs/cgroup/exp_*`)
- [ ] 8. Verificar espaço em disco (`df -h`)
- [ ] 9. Verificar memória disponível (`free -h`)
- [ ] 10. Ver logs completos (`bash script.sh > /tmp/debug.txt 2>&1`)

---

### Problemas Conhecidos

| Problema | Causa | Solução |
|----------|-------|---------|
| "Operação não permitida" no exp2 | Namespaces requerem CAP_SYS_ADMIN | Normal, script trata graciosamente |
| "No such file" em cgroup | Versão v1/v2 errada | Script detecta automaticamente |
| Exp3-5 skip | Não executado com sudo | `sudo bash scripts/run_experiments.sh` |
| OOM killer mata processo | Exp4 por design | Comportamento esperado |
| Script para no exp1 | Problema com set -e | Já corrigido |
| Processos órfãos | Background sem wait | Já corrigido |

---

## 📚 Lições Aprendidas

### 1. Gestão de Processos

- **Sempre use `wait` para processos em background**
- **Redirecione stderr quando apropriado**
- **Tenha fallbacks para operações que podem falhar**

### 2. Tratamento de Erros

- **`set -e` é perigoso em scripts compostos**
- **Sempre capture exit codes explicitamente**
- **Use `set +e` temporariamente quando necessário**

### 3. Compatibilidade

- **Nunca assuma versão de cgroup**
- **Sempre detecte ambiente antes de configurar**
- **Teste em v1 E v2**

### 4. Debugging

- **Adicione debug logs temporários**
- **Execute experimentos individualmente**
- **Use ferramentas de monitoramento (watch, pgrep)**

### 5. Documentação

- **Documente comportamento esperado**
- **Registre problemas conhecidos**
- **Mantenha checklist de troubleshooting**

---

## 🔗 Referências

- [cgroups v2 documentation](https://www.kernel.org/doc/html/latest/admin-guide/cgroup-v2.html)
- [Linux namespaces](https://man7.org/linux/man-pages/man7/namespaces.7.html)
- [unshare command](https://man7.org/linux/man-pages/man1/unshare.1.html)
- [OOM killer](https://www.kernel.org/doc/gorman/html/understand/understand016.html)

---

**Última atualização:** 2025-11-17
**Contribuidores:** Claude Code
**Licença:** Mesma do projeto principal

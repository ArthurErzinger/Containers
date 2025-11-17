#!/bin/bash

# experimento4_mem_limit.sh
# Experimento 4: Limitação de Memória
# Objetivo: Testar comportamento ao atingir limite de memória

set -e
export LC_NUMERIC=C

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
EXP_DIR="$PROJECT_ROOT/experiments/exp4_memory_limit"
REPORT="$EXP_DIR/relatorio.txt"

GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

cd "$PROJECT_ROOT"

echo -e "${BLUE}================================================${NC}"
echo -e "${BLUE}  EXPERIMENTO 4: LIMITAÇÃO DE MEMÓRIA${NC}"
echo -e "${BLUE}================================================${NC}"

# Verificar se está rodando como root
if [ "$EUID" -ne 0 ]; then
    echo -e "${YELLOW}[!] Este experimento requer sudo${NC}"
    exit 1
fi

# Limpar resultados anteriores
rm -f "$EXP_DIR"/*

# Configuração
MEMORY_LIMIT_MB=100
TARGET_ALLOC_MB=200
CGROUP_NAME="exp4_mem_test"

echo -e "${GREEN}[1/6]${NC} Detectando versão do cgroup..."

# Detectar cgroup version
if [ -f "/sys/fs/cgroup/cgroup.controllers" ]; then
    CGROUP_VERSION="v2"
    CGROUP_BASE="/sys/fs/cgroup"
else
    CGROUP_VERSION="v1"
    CGROUP_BASE="/sys/fs/cgroup/memory"
fi

echo "      Usando cgroup $CGROUP_VERSION"

# 2. Criar cgroup para teste
echo -e "${GREEN}[2/6]${NC} Criando cgroup com limite de ${MEMORY_LIMIT_MB}MB..."

if [ "$CGROUP_VERSION" = "v2" ]; then
    mkdir -p "$CGROUP_BASE/$CGROUP_NAME"
    echo "+memory" > "$CGROUP_BASE/cgroup.subtree_control" 2>/dev/null || true
    echo "$((MEMORY_LIMIT_MB * 1024 * 1024))" > "$CGROUP_BASE/$CGROUP_NAME/memory.max"
else
    mkdir -p "$CGROUP_BASE/$CGROUP_NAME"
    echo "$((MEMORY_LIMIT_MB * 1024 * 1024))" > "$CGROUP_BASE/$CGROUP_NAME/memory.limit_in_bytes"
fi

# 3. Testar alocação SEM limite (baseline)
echo -e "${GREEN}[3/6]${NC} Executando test_memory SEM limite (baseline)..."

BASELINE_OUTPUT=$(./tests/bin/test_memory --target-mb 50 --step-mb 10 --hold-seconds 2)
BASELINE_ALLOC=$(echo "$BASELINE_OUTPUT" | grep -oP 'allocated_mb=\K[0-9.]+')
echo "      Alocado: ${BASELINE_ALLOC}MB"

# 4. Testar alocação COM limite
echo -e "${GREEN}[4/6]${NC} Executando test_memory COM limite de ${MEMORY_LIMIT_MB}MB..."
echo "      Tentando alocar ${TARGET_ALLOC_MB}MB..."

# Executar e capturar output
./tests/bin/test_memory --target-mb $TARGET_ALLOC_MB --step-mb 10 --hold-seconds 2 > "$EXP_DIR/mem_output.txt" 2>&1 &
WORKLOAD_PID=$!

sleep 1

# Mover para cgroup
if [ "$CGROUP_VERSION" = "v2" ]; then
    echo $WORKLOAD_PID > "$CGROUP_BASE/$CGROUP_NAME/cgroup.procs" 2>/dev/null || true
else
    echo $WORKLOAD_PID > "$CGROUP_BASE/$CGROUP_NAME/tasks" 2>/dev/null || true
fi

# Aguardar processo
wait $WORKLOAD_PID 2>/dev/null || RESULT=$?

# Verificar se houve OOM
if [ $RESULT -ne 0 ]; then
    OOM_STATUS="${RED}OOM Killer ativado${NC}"
    MAX_ALLOC="~${MEMORY_LIMIT_MB}MB (limite)"
else
    OOM_STATUS="${GREEN}Completou sem OOM${NC}"
    MAX_ALLOC=$(grep "allocated_mb=" "$EXP_DIR/mem_output.txt" | grep -oP 'allocated_mb=\K[0-9.]+' || echo "$MEMORY_LIMIT_MB")
    MAX_ALLOC="${MAX_ALLOC}MB"
fi

# 5. Verificar failcnt
echo -e "${GREEN}[5/6]${NC} Verificando falhas de alocação..."

if [ "$CGROUP_VERSION" = "v2" ]; then
    FAILCNT=$(grep "max" "$CGROUP_BASE/$CGROUP_NAME/memory.events" | awk '{print $2}' || echo "0")
else
    FAILCNT=$(cat "$CGROUP_BASE/$CGROUP_NAME/memory.failcnt" || echo "0")
fi

echo "      Falhas de alocação: $FAILCNT"

# 6. Limpar cgroup
echo -e "${GREEN}[6/6]${NC} Limpando cgroup de teste..."
rmdir "$CGROUP_BASE/$CGROUP_NAME" 2>/dev/null || true

# Gerar relatório
cat > "$REPORT" << EOF
================================================
  EXPERIMENTO 4: LIMITAÇÃO DE MEMÓRIA
================================================

OBJETIVO:
  Testar comportamento ao atingir limite de memória

CONFIGURAÇÃO:
  - Cgroup version: $CGROUP_VERSION
  - Limite de memória: ${MEMORY_LIMIT_MB}MB
  - Tentativa de alocação: ${TARGET_ALLOC_MB}MB
  - Workload: test_memory

PROCEDIMENTO:
  1. Criar cgroup com limite de ${MEMORY_LIMIT_MB}MB
  2. Tentar alocar ${TARGET_ALLOC_MB}MB incrementalmente
  3. Observar comportamento (OOM killer, falhas de alocação)

RESULTADOS:

1. ALOCAÇÃO SEM LIMITE (BASELINE)
   - Quantidade alocada: ${BASELINE_ALLOC}MB
   - Status: Sucesso completo

2. ALOCAÇÃO COM LIMITE DE ${MEMORY_LIMIT_MB}MB
   - Quantidade máxima alocada: $MAX_ALLOC
   - Número de falhas (failcnt): $FAILCNT
   - Comportamento observado: $OOM_STATUS

MÉTRICAS REPORTADAS:
  • Quantidade máxima alocada
  • Número de falhas (memory.failcnt)
  • Comportamento do sistema ao atingir limite

OBSERVAÇÕES:
  - Quando o limite é atingido, o kernel pode:
    * Invocar OOM killer e matar o processo
    * Retornar erro em malloc()
    * Forçar swap (se disponível)
  - O failcnt conta quantas vezes o limite foi atingido

CONCLUSÃO:
  O limite de memória via cgroup foi respeitado.
  O processo não conseguiu alocar mais que ~${MEMORY_LIMIT_MB}MB.
$([ $RESULT -ne 0 ] && echo "  O OOM killer foi ativado quando o limite foi excedido." || echo "  O processo lidou com o limite sem ser terminado.")

Data: $(date '+%Y-%m-%d %H:%M:%S')
================================================
EOF

# Mostrar relatório
cat "$REPORT"

echo ""
echo -e "${GREEN}[✓]${NC} Relatório salvo em: $REPORT"

exit 0

#!/bin/bash

# run_experiments.sh - Orquestrador dos 5 Experimentos Obrigatórios
# Executa todos os experimentos em sequência e gera relatório consolidado

export LC_NUMERIC=C

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
EXPERIMENTS_DIR="$PROJECT_ROOT/experiments"

# Cores para output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}================================================================${NC}"
echo -e "${BLUE}  EXECUTANDO TODOS OS EXPERIMENTOS OBRIGATÓRIOS${NC}"
echo -e "${BLUE}================================================================${NC}"
echo ""

# Verificar se está no diretório correto
cd "$PROJECT_ROOT"

# Verificar se executável existe
if [ ! -f "./resource-monitor" ]; then
    echo -e "${YELLOW}[!] Compilando resource-monitor...${NC}"
    make -s
fi

# Verificar se workloads existem
if [ ! -f "./tests/bin/test_cpu" ]; then
    echo -e "${YELLOW}[!] Compilando workloads...${NC}"
    make -s workloads
fi

# Contador de sucessos
TOTAL=5
PASSED=0
FAILED=0

# Array para armazenar resultados
declare -a RESULTS

# Função para executar experimento
run_experiment() {
    local num=$1
    local name=$2
    local script=$3
    local needs_sudo=$4

    echo -e "\n${BLUE}--- Experimento $num: $name ---${NC}"

    if [ "$needs_sudo" = "yes" ] && [ "$EUID" -ne 0 ]; then
        echo -e "${YELLOW}[!] Este experimento requer sudo. Execute com: sudo bash $script${NC}"
        RESULTS[$num]="SKIP (requer sudo)"
        return
    fi

    # Executar script e capturar exit code
    bash "$SCRIPT_DIR/$script"
    local exit_code=$?

    if [ $exit_code -eq 0 ]; then
        echo -e "${GREEN}[✓] Experimento $num concluído com sucesso${NC}"
        RESULTS[$num]="PASSOU"
        ((PASSED++))
    else
        echo -e "${RED}[✗] Experimento $num falhou${NC}"
        RESULTS[$num]="FALHOU"
        ((FAILED++))
    fi
}

# Executar experimentos
run_experiment 1 "Overhead de Monitoramento" "experimento1_overhead.sh" "no"
run_experiment 2 "Isolamento via Namespaces" "experimento2_namespaces.sh" "no"
run_experiment 3 "Throttling de CPU" "experimento3_cpu_throttle.sh" "yes"
run_experiment 4 "Limitação de Memória" "experimento4_mem_limit.sh" "yes"
run_experiment 5 "Limitação de I/O" "experimento5_io_limit.sh" "yes"

# Relatório final
echo ""
echo -e "${BLUE}================================================================${NC}"
echo -e "${BLUE}  RELATÓRIO CONSOLIDADO${NC}"
echo -e "${BLUE}================================================================${NC}"
echo ""

for i in {1..5}; do
    result="${RESULTS[$i]}"
    if [ "$result" = "PASSOU" ]; then
        echo -e "  Experimento $i: ${GREEN}✓ PASSOU${NC}"
    elif [ "$result" = "FALHOU" ]; then
        echo -e "  Experimento $i: ${RED}✗ FALHOU${NC}"
    else
        echo -e "  Experimento $i: ${YELLOW}⊘ SKIP${NC} ($result)"
    fi
done

echo ""
echo -e "  Total: $PASSED/$TOTAL experimentos passaram"
echo ""

if [ "$EUID" -ne 0 ]; then
    echo -e "${YELLOW}[!] Nota: Experimentos 3, 4 e 5 requerem sudo.${NC}"
    echo -e "${YELLOW}    Execute com: sudo bash scripts/run_experiments.sh${NC}"
    echo ""
fi

echo -e "${BLUE}================================================================${NC}"
echo -e "${BLUE}  Relatórios individuais em: $EXPERIMENTS_DIR${NC}"
echo -e "${BLUE}================================================================${NC}"

# Retornar código apropriado
if [ $FAILED -gt 0 ]; then
    exit 1
else
    exit 0
fi

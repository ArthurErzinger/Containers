#!/bin/bash

# experimento1_overhead.sh
# Experimento 1: Overhead de Monitoramento
# Objetivo: Medir o impacto do próprio profiler no sistema

set -e
export LC_NUMERIC=C

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
EXP_DIR="$PROJECT_ROOT/experiments/exp1_overhead"
REPORT="$EXP_DIR/relatorio.txt"

GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m'

cd "$PROJECT_ROOT"

echo -e "${BLUE}================================================${NC}"
echo -e "${BLUE}  EXPERIMENTO 1: OVERHEAD DE MONITORAMENTO${NC}"
echo -e "${BLUE}================================================${NC}"

# Limpar resultados anteriores
rm -f "$EXP_DIR"/*

# Configuração
DURATION=20
THREADS=2
INTERVALS=(1 3 5)

echo -e "${GREEN}[✓]${NC} Configuração: test_cpu --duration $DURATION --threads $THREADS"

# Função para medir tempo de execução
measure_time() {
    local start=$(date +%s.%N)
    "$@" > /dev/null 2>&1
    local end=$(date +%s.%N)
    echo "$(echo "$end - $start" | bc)"
}

# 1. BASELINE: Executar sem monitoramento
echo -e "${GREEN}[1/4]${NC} Executando workload SEM monitoramento (baseline)..."
BASELINE_TIME=$(measure_time ./tests/bin/test_cpu --duration $DURATION --threads $THREADS)
echo "      Tempo: ${BASELINE_TIME}s"

# 2. Executar COM monitoramento em diferentes intervalos
declare -A MONITORED_TIMES

for interval in "${INTERVALS[@]}"; do
    echo -e "${GREEN}[2/4]${NC} Executando workload COM monitoramento (intervalo ${interval}s)..."

    # Iniciar workload em background
    ./tests/bin/test_cpu --duration $DURATION --threads $THREADS > /dev/null 2>&1 &
    WORKLOAD_PID=$!

    # Medir tempo total
    start=$(date +%s.%N)

    # Monitorar o processo
    ./resource-monitor --monitor $WORKLOAD_PID --interval $interval --samples $((DURATION / interval)) \
        --output "$EXP_DIR/metrics_${interval}s.csv" > /dev/null 2>&1 || true

    # Aguardar workload terminar
    wait $WORKLOAD_PID 2>/dev/null || true
    end=$(date +%s.%N)

    MONITORED_TIMES[$interval]=$(echo "$end - $start" | bc)
    echo "      Tempo: ${MONITORED_TIMES[$interval]}s"
done

# 3. Calcular overhead
echo -e "${GREEN}[3/4]${NC} Calculando overhead..."

# 4. Gerar relatório
echo -e "${GREEN}[4/4]${NC} Gerando relatório..."

cat > "$REPORT" << EOF
================================================
  EXPERIMENTO 1: OVERHEAD DE MONITORAMENTO
================================================

OBJETIVO:
  Medir o impacto do próprio profiler no sistema

CONFIGURAÇÃO:
  - Workload: test_cpu --duration ${DURATION} --threads ${THREADS}
  - Intervalos testados: ${INTERVALS[*]} segundos

PROCEDIMENTO:
  1. Executar workload de referência SEM monitoramento (baseline)
  2. Executar mesmo workload COM monitoramento em diferentes intervalos
  3. Medir diferenças em tempo de execução

RESULTADOS:
┌──────────────────────┬──────────────┬────────────────┬──────────────┐
│ Configuração         │ Tempo (s)    │ Overhead (s)   │ Overhead (%) │
├──────────────────────┼──────────────┼────────────────┼──────────────┤
│ SEM monitoramento    │ $(printf "%-12.3f" $BASELINE_TIME) │ -              │ -            │
EOF

for interval in "${INTERVALS[@]}"; do
    time=${MONITORED_TIMES[$interval]}
    overhead=$(echo "$time - $BASELINE_TIME" | bc)
    overhead_pct=$(echo "scale=2; ($overhead / $BASELINE_TIME) * 100" | bc)

    printf "│ COM monitoramento %ds │ %-12.3f │ %-14.3f │ %-12.2f │\n" \
        $interval $time $overhead $overhead_pct >> "$REPORT"
done

cat >> "$REPORT" << EOF
└──────────────────────┴──────────────┴────────────────┴──────────────┘

MÉTRICAS REPORTADAS:
  • Tempo de execução com e sem profiler
  • CPU overhead (tempo adicional)
  • Overhead percentual
  • Latência de sampling (intervalo configurado)

OBSERVAÇÕES:
  - Overhead aumenta com intervalos menores (mais samplings)
  - Dados de métricas salvos em: $EXP_DIR/metrics_*.csv

CONCLUSÃO:
  O profiler adiciona overhead de $(echo "scale=2; ((${MONITORED_TIMES[1]} - $BASELINE_TIME) / $BASELINE_TIME) * 100" | bc)% a $(echo "scale=2; ((${MONITORED_TIMES[5]} - $BASELINE_TIME) / $BASELINE_TIME) * 100" | bc)%
  dependendo do intervalo de sampling.

Data: $(date '+%Y-%m-%d %H:%M:%S')
================================================
EOF

# Mostrar relatório
cat "$REPORT"

echo ""
echo -e "${GREEN}[✓]${NC} Relatório salvo em: $REPORT"
echo -e "${GREEN}[✓]${NC} Dados de métricas em: $EXP_DIR/metrics_*.csv"

exit 0

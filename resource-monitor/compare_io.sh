#!/bin/bash

# Força a localidade numérica para usar pontos (.) como separador decimal
export LC_NUMERIC="C"

# Script para comparar a saída de I/O do resource-monitor com o iotop.
# USO: ./compare_io.sh <PID>

# --- Configuração ---
PID=$1
MONITOR_PATH="./resource-monitor"
INTERVAL=3 # Segundos
CSV_OUTPUT="monitor_output.tmp.csv"
IOTOP_OUTPUT="iotop_output.tmp.txt"
INPUT_FILE="commands.tmp.txt"

# --- Validação ---

# Verifica se o usuário é root ou tem sudo
if [[ $EUID -ne 0 ]]; then
    if ! command -v sudo &> /dev/null; then
        echo "ERRO: Este script precisa de 'sudo' para executar o iotop. Por favor, instale o sudo ou execute como root."
        exit 1
    fi
    SUDO="sudo"
fi

# Verifica se um PID foi fornecido
if [ -z "$PID" ]; then
    echo "ERRO: Nenhum PID fornecido."
    echo "USO: $0 <PID>"
    exit 1
fi

# Verifica se o processo existe
if [ ! -d "/proc/$PID" ]; then
    echo "ERRO: Processo com PID $PID não encontrado."
    exit 1
fi

# Verifica se o resource-monitor existe e é executável
if [ ! -x "$MONITOR_PATH" ]; then
    echo "ERRO: O executável '$MONITOR_PATH' não foi encontrado ou não tem permissão de execução."
    echo "Por favor, compile o projeto (make) e execute este script no mesmo diretório."
    exit 1
fi

echo "--- Comparador de I/O ---"
echo "PID Alvo: $PID"
echo "Intervalo: $INTERVAL segundos"
echo "Aguarde enquanto coletamos os dados..."

# --- Coleta de Dados ---

# 1. Executa o iotop em batch mode
echo "Executando iotop..."
$SUDO iotop -b -P -p "$PID" -d "$INTERVAL" -n 2 > "$IOTOP_OUTPUT" 2>/dev/null

# 2. Executa o resource-monitor de forma não-interativa
echo "Executando resource-monitor..."
# Cria um arquivo com a sequência de comandos
printf "1\n%d\n%d\ns\n%s\n" "$PID" "$INTERVAL" "$CSV_OUTPUT" > "$INPUT_FILE"

# Executa o monitor em background, com a entrada redirecionada do arquivo
"$MONITOR_PATH" < "$INPUT_FILE" > /dev/null 2>&1 &
MONITOR_PROC_PID=$!

# Espera tempo suficiente para o monitor escrever pelo menos uma linha de dados
sleep $(($INTERVAL + 2))

# Para o processo do monitor
kill "$MONITOR_PROC_PID" > /dev/null 2>&1
wait "$MONITOR_PROC_PID" 2>/dev/null

echo "Coleta de dados finalizada. Analisando resultados..."
echo ""

# --- Análise e Exibição ---

# 3. Processa a saída do iotop
IOTOP_LINE=$(grep " $PID " "$IOTOP_OUTPUT" | tail -n 1)
if [ -n "$IOTOP_LINE" ]; then
    IOTOP_READ_BPS=$(echo "$IOTOP_LINE" | awk '{print $5}')
    IOTOP_WRITE_BPS=$(echo "$IOTOP_LINE" | awk '{print $7}')
else
    IOTOP_READ_BPS=0
    IOTOP_WRITE_BPS=0
fi

# 4. Processa a saída do resource-monitor (CSV)
if [ -f "$CSV_OUTPUT" ]; then
    MONITOR_LINE=$(tail -n 1 "$CSV_OUTPUT")
    MONITOR_READ_BPS=$(echo "$MONITOR_LINE" | cut -d',' -f4)
    MONITOR_WRITE_BPS=$(echo "$MONITOR_LINE" | cut -d',' -f5)
else
    MONITOR_READ_BPS=0
    MONITOR_WRITE_BPS=0
    echo "AVISO: O arquivo de saída do monitor '$CSV_OUTPUT' não foi criado."
fi

# Define um valor padrão de 0 se as variáveis estiverem vazias
IOTOP_READ_BPS=${IOTOP_READ_BPS:-0}
IOTOP_WRITE_BPS=${IOTOP_WRITE_BPS:-0}
MONITOR_READ_BPS=${MONITOR_READ_BPS:-0}
MONITOR_WRITE_BPS=${MONITOR_WRITE_BPS:-0}

# 5. Exibe a tabela de comparação
echo "--- Resultados da Comparação (valores em Bytes/s) ---"
printf "% -20s | % -20s | % -20s\n" "Métrica" "iotop" "resource-monitor"
printf -- '------------------------------------------------------------------\n'
printf "% -20s | % -20.2f | % -20.2f\n" "Taxa de Leitura" "$IOTOP_READ_BPS" "$MONITOR_READ_BPS"
printf "% -20s | % -20.2f | % -20.2f\n" "Taxa de Escrita" "$IOTOP_WRITE_BPS" "$MONITOR_WRITE_BPS"
printf -- '------------------------------------------------------------------\n'
echo "Nota: 'iotop' foi executado com a flag '-P' para agregar por processo."
echo "Pequenas diferenças são esperadas devido a intervalos de amostragem."

# --- Limpeza ---
rm -f "$CSV_OUTPUT" "$IOTOP_OUTPUT" "$INPUT_FILE"
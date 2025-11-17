#!/bin/bash

# run.sh - Compila e executa o Resource Monitor
# Uso simples: ./run.sh

set -e

echo "Compilando..."
make

echo ""
echo "Executando resource-monitor..."
echo ""
sudo ./resource-monitor

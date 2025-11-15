#!/bin/bash

# Limpa builds anteriores para garantir que não haja arquivos de objeto antigos.
echo "Cleaning previous build..."
make clean

# Compila o projeto. Não é necessário sudo para isso.
echo "Building project..."
make

# Executa o monitor de recursos com privilégios de superusuário,
# que são necessários para acessar informações de todos os processos no /proc.
echo "Running resource monitor with sudo..."
sudo ./resource-monitor
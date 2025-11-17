# Resource Monitor System

Sistema completo de monitoramento e análise de recursos para processos e containers no Linux, explorando as primitivas do kernel: namespaces, control groups (cgroups) e interfaces /proc e /sys.

## 📋 Descrição do Projeto

Este projeto implementa três componentes principais de monitoramento:

### 1. Resource Profiler
Monitora e coleta métricas detalhadas de processos:
- **CPU:** tempo de usuário/sistema, threads, context switches
- **Memória:** RSS, VSZ, swap, page faults
- **I/O:** bytes lidos/escritos, syscalls de leitura/escrita
- **Rede:** bytes e pacotes transmitidos/recebidos

### 2. Namespace Analyzer
Analisa isolamento via namespaces do Linux:
- Lista namespaces de processos específicos
- Encontra processos em um namespace específico
- Compara namespaces entre processos
- Gera relatórios JSON do sistema

### 3. Control Group Manager
Gerencia e analisa cgroups (v1 e v2):
- Lê métricas de CPU, Memory e BlkIO
- Cria cgroups experimentais
- Move processos para cgroups
- Aplica limites de recursos
- Gera relatórios de utilização

## 🔧 Requisitos e Dependências

### Requisitos de Sistema
- **Sistema Operacional:** Linux (testado em Ubuntu 24.04+)
- **Kernel:** Linux 4.5+ (suporte a cgroup v2 recomendado)
- **Permissões:** Algumas operações requerem privilégios de root (sudo)

### Ferramentas de Desenvolvimento
- **Compilador:** GCC compatível com C2x/C23
- **Make:** GNU Make para compilação
- **Bibliotecas:** Apenas libc e bibliotecas padrão do sistema (sem dependências externas)

## 🚀 Compilação

Para compilar o projeto:

```bash
cd resource-monitor
make
```

Para compilar os programas de teste (workloads):

```bash
make workloads
```

Para limpar arquivos compilados:

```bash
make clean
```

## 📖 Uso

O `resource-monitor` pode ser executado em três modos:

### 1. Modo Interativo (Menu)

Execute sem argumentos para acessar o menu interativo completo:

```bash
./resource-monitor
```

**Opções do menu:**
- **[1] Resource Profiler** - Monitoramento de processos
- **[2] Namespace Analyzer** - Análise de namespaces
- **[3] Control Group Manager** - Gerenciamento de cgroups
- **[4] Sair**

### 2. Modo Batch (Linha de Comando)

Monitora um processo específico em modo não-interativo:

```bash
./resource-monitor --monitor <PID> [opções]
```

**Opções disponíveis:**
- `--interval <segundos>` - Intervalo entre amostras (padrão: 1s)
- `--samples <quantidade>` - Número de amostras (padrão: 10)
- `-o, --output <arquivo.csv>` - Salva dados em CSV

**Exemplo:**
```bash
# Monitorar processo 1234 por 30 segundos (30 amostras de 1s)
./resource-monitor --monitor 1234 --samples 30 --output metricas.csv
```

### 3. Geração de Relatórios

#### Relatório de Namespaces
```bash
./resource-monitor -n
# ou
./resource-monitor --namespace-report
```

#### Relatório de Cgroups
```bash
sudo ./resource-monitor -c
# ou
sudo ./resource-monitor --cgroup-report
```

Ambos geram arquivos `report.json` no diretório atual.

## 🧪 Programas de Teste

O projeto inclui programas de teste para validar o monitoramento:

### test_cpu - Carga de CPU
```bash
./tests/bin/test_cpu --duration 30 --threads 4
```

### test_memory - Alocação de Memória
```bash
./tests/bin/test_memory --target-mb 512 --step-mb 16
```

### test_io - I/O Intensivo
```bash
./tests/bin/test_io --file teste.dat --size-mb 256 --block-kb 1024
```

## 📊 Formato dos Dados

### CSV Export
O modo batch e o profiler interativo podem exportar dados em CSV com as seguintes colunas:
- timestamp, pid, cpu_percentage
- read_rate_bps, write_rate_bps
- utime_jiffies, stime_jiffies, num_threads
- vm_size_kb, vm_rss_kb, vm_swap_kb
- min_page_faults, maj_page_faults
- voluntary_context_switches, nonvoluntary_context_switches
- total_read_bytes, total_write_bytes
- read_syscalls, write_syscalls
- net_rx_bytes, net_tx_bytes, net_rx_packets, net_tx_packets

### JSON Reports
Relatórios de namespaces e cgroups são exportados em formato JSON estruturado.

## ⚠️ Notas Importantes

1. **Permissões:** Operações com cgroups geralmente requerem `sudo`
2. **Cgroup Version:** O sistema detecta automaticamente se está usando cgroup v1 ou v2
3. **Processos Privilegiados:** Alguns processos do sistema podem não ser acessíveis sem root
4. **Leitura de /proc/[pid]/io:** Requer permissão de leitura no processo alvo

## 👥 Autores e Contribuições

**Equipe de Desenvolvimento:**
- Arthur - Resource Profiler e Integração
- [Contribuidor 2] - Namespace Analyzer
- [Contribuidor 3] - Control Group Manager
- [Contribuidor 4] - Testes e Validação

## 📝 Licença

Este projeto foi desenvolvido como atividade acadêmica para a disciplina de Sistemas Operacionais.

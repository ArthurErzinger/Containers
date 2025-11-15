# Resource Monitor System

Este projeto implementa um sistema de monitoramento e análise de recursos para processos e containers no Linux, explorando namespaces e cgroups.

## Requisitos e Dependências

*   **Compilador C:** GCC (versão compatível com C23)
*   **Bibliotecas:** Apenas `libc` e bibliotecas padrão do sistema.
*   **Sistema Operacional:** Linux (testado em Ubuntu 24.04+)

## Construindo o Projeto

Para compilar o projeto, navegue até o diretório `resource-monitor` e execute `make`:

```bash
cd resource-monitor
make
```

Isso criará o executável `resource-monitor` no diretório raiz do projeto.

## Uso

O `resource-monitor` pode ser executado de duas formas:

### Modo Interativo (Menu)

Execute o programa sem argumentos para acessar o menu interativo:

```bash
./resource-monitor
```

No menu, você pode escolher entre as seguintes opções:
1.  **Resource Profiler:** Monitora o uso de CPU, I/O, memória e rede de um processo.
2.  **Namespace Analyzer:** Analisa e reporta informações sobre namespaces.
3.  **Control Group Manager:** Gerencia e analisa cgroups.
4.  **Sair:** Encerra o programa.

### Gerar Relatório de Namespaces (JSON)

Para gerar diretamente um relatório de todos os namespaces ativos no sistema em formato JSON, use o argumento `-n` ou `--namespace-report`:

```bash
./resource-monitor -n
```

A saída será impressa no `stdout`. Você pode redirecioná-la para um arquivo:

```bash
./resource-monitor -n > namespace_report.json
```

## Autores

*   [Seu Nome/Nomes dos Autores]

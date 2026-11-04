# Auditoria da baseline v0.2.0

Data: 2026-11-03. A auditoria precedeu a implementação. GCC 15.2.0, CMake 4.2.3, Qt 6.10.2 no Ubuntu.

Antes de modificar código foram executados configure Debug, build com `-j"$(nproc)"` e CTest: **9/9 aprovados**. Os nove testes e seus arquivos foram preservados integralmente: tests/tests.cpp, tests/disassembly_tests.cpp e tests/gui_tests.cpp não foram alterados nesta versão.

Git: agora existe repositório com branch main, **sem commits**; os 135 arquivos da v0.2.0 estão adicionados ao índice. `git log` informa ausência de commits. Não foi feito commit, reset, alteração do índice ou limpeza do trabalho anterior. O relatório v0.2.0, que descreve `.git` vazio, representa a sessão anterior.

| Área | Baseline observada | Tratamento |
|---|---|---|
| Execução | Parsing/decoding em openFile e navegação na GUI | Worker dedicado com dono, ID de pedido, mailbox e join |
| Cancelamento | Inexistente | Token atômico e checkpoints em parser/engine; não publicar cancelados |
| Progresso | Somente fases 0/30/100, sem event loop durante análise | Barra na toolbar, fase/contadores e consulta de mailbox limitada |
| Instruções | Teto local 20.000 reiniciado na navegação | Cota atômica compartilhada por projeto e limite de trabalho |
| Memória | Limites de registros/nomes sem orçamento conjunto | Reservas conservadoras cobradas antes das alocações retidas/scratch |
| Tempo | Sem deadline | steady_clock desde o início do pedido, checkpoints cooperativos |
| Loops | visited, mas destinos poderiam ser enfileirados repetidamente | scheduled adicional, work budget e testes de back edges |
| ELF | Campos/endian explícitos, ranges por divisão, truncamentos e referências validados | Preservar validações; adicionar controle de recursos e fuzzing |
| I/O | Stream aberto antes de verificar tipo | Recusar tipos não regulares antes de abrir para evitar FIFO bloqueante |
| Modelos | Assembly em QAbstractTableModel; tabelas periféricas em QTableWidget | Preservar e restringir acesso à GUI; publicação continua síncrona |
| Compatibilidade | APIs e nove testes existentes | Nenhuma remoção de API; overloads/entrada assíncrona adicionais |

Limitações assumidas: orçamento não limita RSS; syscalls não são interrompidas; sem snapshot/isolamento; modelos volumosos ainda podem pausar publicação. Qt instalado não é instrumentado por TSan: validação concorrente do próprio controlador é separada da validação de widgets.

# Execução assíncrona e limites — 0.2.1

## Propriedade e concorrência

`AnalysisController` pertence à GUI. Seus métodos start/cancel são chamados nessa thread. Cada pedido recebe um ID monotônico, limites e uma cota compartilhada de instruções. O controlador possui **um std::jthread ativo e no máximo um pedido pendente**. Start com trabalho ativo cancela o anterior e substitui o pendente; não cria uma fila ilimitada de threads. Controladores independentes podem trabalhar em paralelo sem compartilhar BinaryFile ou Image.

O worker abre um BinaryFile próprio, chama ElfLoader e DisassemblyEngine com o mesmo AnalysisControl e produz AnalysisOutcome. Nunca acessa modelos/widgets. O progresso é uma mailbox de **um** sample sob std::mutex, sem emitir eventos Qt de dentro do worker. O núcleo limita sua taxa a aproximadamente 40 ms por fase e o controlador consulta a mailbox a cada 15 ms. Resultado final é publicado por store release/load acquire e só transferido depois de join.

O controlador emite sinais Qt de progresso/conclusão **na thread proprietária**. Window filtra o ID e ignora pedidos antigos ou resultados após fechamento. Cancelamento tardio vence mesmo se o decoding já tiver terminado. Fechar/destruir cancela e faz join, sem terminate, detach ou destruição de objetos ainda usados pelo worker. A publicação em widgets ainda ocorre na GUI; listas enormes podem pausar essa fase.

A cota `InstructionBudget` é atômica (CAS sem overshoot) e pode ser compartilhada entre controladores. Cada AnalysisControl é usado por um só worker, exceto cancel e getters atômicos. Não é permitido chamar reserveMemory/work simultaneamente no mesmo controle.

## Cancelamento e interrupções

AnalysisControl mantém token atômico e deadline de steady_clock. Checkpoints existem por registro ELF, bloco/char de strings, semente, item de fila, instrução e após o decoder retornar. Interrupções usam AnalysisStopped com StopReason tipado:

- Cancelled;
- TimeLimit;
- MemoryLimit;
- InstructionLimit;
- WorkLimit.

No parser, interrupção não produz Image parcial publicável. No engine, interrupções podem devolver instruções já validadas, com diagnóstico/limited e stopReason. Window descarta **todo** resultado cancelado. Para limites com ELF válido, pode publicar análise parcial; bytes restantes continuam não analisados. Pedido de navegação sem novas instruções por esgotamento conserva o resultado anterior.

Não há interrupção assíncrona de código/stream: uma syscall lenta ou arquivo em filesystem remoto ainda pode atrasar cancelamento/join. BinaryFile recusa tipos não regulares antes de open para evitar FIFOs/dispositivos, mas não existe snapshot nem proteção completa contra substituição concorrente de path entre stat/open. Essas garantias exigiriam I/O específico de SO ou isolamento de processo.

## Orçamentos

Padrões de AnalysisLimits:

| Limite | Padrão | Escopo |
|---|---|---|
| maxMemoryBytes | 128 MiB | Reservas conservadoras do núcleo por pedido, parser + engine |
| maxTime | 10 segundos | Tempo desde o início efetivo do worker, sem incluir espera pendente |
| maxInstructions | 20.000 | Cota global do projeto; limite adicional local do pedido |
| maxWorkItems | 1.000.000 | Sementes, filas, registros e blocos de strings por pedido |

Memory: reservas são cobradas **antes** de alocar tabelas/nós/strings retidos e scratch limitado. Tabelas usam reserve após validar contagem/faixa; crescimento de nomes, metadados, conjuntos de visitados/agendados, instruções/referências e vetor final são contabilizados conservadoramente. A contabilização é monotônica no pedido e não devolve créditos de temporários, podendo interromper antes do necessário.

**Não é um allocator exato nem um limite de RSS.** Não inclui Qt, caches de bibliotecas, stacks/allocator, arquivo/modelo do projeto já publicado ou outros controladores. Não há garantia de que o RSS do processo fique abaixo do valor configurado; para isso será necessário allocator dedicado ou processo isolado com limites do SO. Exaustão real de memória continua dependente do sistema, embora exceções do worker sejam capturadas.

Instructions: cada tentativa de decoder consome uma unidade, mesmo que cancelada, repetida por navegação ou recusada posteriormente por conflito. A cota é compartilhada com navegações e não pode ser reiniciada criando outro pedido. Abrir/reabrir um arquivo cria nova cota de projeto. O mesmo conjunto de instruções pode ser reanalisado; não existe banco incremental. Cotas do projeto são fixadas ao abrir; alterar configurações afeta memória/tempo dos próximos pedidos e a cota global dos próximos projetos.

Visited + scheduled impedem ciclos e repetição de destinos diretos. maxWorkItems também limita sementes duplicadas, endereços inválidos e trabalho que não gera instruções. Os limites históricos de 200.000 registros/símbolos, 4096 bytes por nome e 256 program headers para disassembly permanecem.

## APIs preservadas

- `Loader::load(BinaryFile&)` e `ElfLoader::load(BinaryFile&)` continuam disponíveis. Um overload `load(file, control)` permite controle do pedido.
- `DisassemblyEngine::analyze(file, image, requested, maxInstructions)` continua disponível com os limites padrão e teto legado de 20.000. O overload `analyze(file, image, control, requested)` permite outras cotas.
- `Window::openFile(path, interactive)` mantém retorno bool e espera pelo resultado, mas delega o trabalho ao worker e usa um event loop local sem entrada de usuário. É API de compatibilidade para os testes/programas existentes.
- Menu File e inicialização normal usam `openFileAsync`. Novas solicitações de análise por navegação também são assíncronas; a seleção final só é aplicada se o pedido ainda for o mais recente.
- `cancelAnalysis`, `analysisBusy`, `setAnalysisLimits` e `analysisLimits` foram adicionados sem remover interfaces.

A consulta `analysisBusy` cobre worker ativo e pedido pendente. Não use esperas bloqueantes de GUI para consumir a API assíncrona; os testes usam event loop e timeout.

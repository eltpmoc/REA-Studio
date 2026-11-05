# Changelog

## Em desenvolvimento — v0.3.2-alpha

- Análise de dominadores, pós-dominadores acíclicos e laços naturais no núcleo, integrada à IR detalhada, com testes de limites e cancelamento. Recuperação estruturada e publicação ainda pendentes.
- Validação desta etapa: GCC/Clang/ASan+UBSan 26/26 e TSan núcleo 9/9. Ver docs/validation-v0.3.2.md.


## 0.3.1-alpha — Semantic IR local

- Definições imutáveis de bitvectors, cadeias def-use, aliases GPR e memória local little endian. Recuperação conservadora de retorno inteiro System V e primeira expressão de somar comprovada no ELF real.
- C-like com confiança e limites explícitos; MachineState preservado. SSA entre blocos, memória geral e tipos assinados continuam pendentes. Veja docs/decompiler-v0.3.1.md.


## 0.3.0-alpha — navegação por funções, 2026-11-04

- Navigate → Go to Function, Ctrl+Shift+F, filtro incremental e catálogo integrado FunctionModel/SymbolModel, com nome original/demangled, VA, tamanho, origem e confiança.
- Análise sob demanda reutiliza instruções validadas, compartilha cota global, permite cancelamento e preserva a publicação anterior em falhas. Seleção sincroniza Assembly, CFG, referências, IR e pseudocódigo.
- Navegação real Qt offscreen comprovada nas quatro funções do ELF obrigatório; GCC Debug/Release, Clang e ASan/UBSan 24/24; TSan core 7/7. Windows/desktop nativo continuam pendentes. Veja docs/function-navigation.md.

## 0.3.0-alpha — escrita de memória e primeira IR, 2026-11-04

- Modo explícito Read/Write, edição hexadecimal por diálogo, comparação dos bytes originais, read-back, erros parciais e desfazer do último patch verificado. Sem alteração de proteção de páginas ou elevação de privilégios; escrita não atômica.
- Primeiro X86IRLifter, operandos tipados/metadados/flags/proveniência e lowering limitado de GPR 32/64. Novas abas IR/Pseudocode experimentais, com opacos e dispatcher de PC explícitos. Não equivale a um decompilador completo.
- Prioridade confirmada de evolução: decompilação/IR; matriz inicial de capacidades e gates, sem alegação de paridade integral com Ghidra/IDA.
- 21/21 GCC Debug/Release, Clang e ASan/UBSan; 6/6 TSan core. Windows e TSan widgets continuam pendentes. Corrigido join duplicado de fechamento reproduzido pelo novo teste GUI de escrita.


## 0.3.0-alpha — memória de processos, 2026-11-04

- Nova aba Process Memory e File → Open process memory, com lista/filtro de processos, abertura por duplo clique, regiões/permissões e hex/ASCII em endereços de runtime.
- Backend Linux /proc (memória somente leitura com descritores ancorados) e backend Windows Toolhelp/OpenProcess/VirtualQueryEx/ReadProcessMemory, sem elevação automática ou modificação do alvo.
- Leitura paginada de até 4 KiB, atualização manual/automática de 1 s, cópia do hex, erros/leituras parciais, cancelamento e worker std::jthread com publicação na GUI após join.
- Testes de memória real própria/processo-filho, mudanças em runtime, identidade, término, limites, concorrência, navegação GUI e lifetime. Preservados os 17 anteriores; 19/19 GCC/Clang/ASan+UBSan e 5/5 TSan core.
- Implementação Windows preparada, sem execução local Windows. Memória virtual viva, não dump físico ou snapshot atômico.


## 0.3.0-alpha — preparação pública local, 2026-11-03

- Presets Linux Release e Windows MSVC x64, identificação alpha e metadata do build instalada. MSVC com UTF-8 em todos os alvos, NOMINMAX e CRT DLL compatível com Qt.
- Auditoria local/remota somente leitura: 107 arquivos MIT íntegros; main remoto contém LICENSE MIT, enquanto a árvore local é GPL. Divergência documentada em COPYING.md, sem alteração remota ou reescrita de histórico.
- .gitignore/.gitattributes para builds/credenciais e integridade vendorizada em checkout Windows. Inventário permitido e busca limitada por credenciais antes de empacotar fontes.
- Scripts de fonte/Linux e deployment Windows com gates de CTest, DLLs/plugins, smoke da distribuição e SHA-256; pacote Windows só existe após execução real em Windows. Código-fonte correspondente incluído nos binários.
- Workflow GCC/Clang, MSVC, sanitizers, licenças/integridade, fuzzing e artefatos/relatórios por 30 dias, actions por SHA, sem continue-on-error ou publicação automática de release.
- Documentos de build, instalação, release, segurança e limitações; notas locais e relatório de preparação pública. Os 17 testes existentes foram preservados.

Windows/desktop nativo permanecem sem validação local; pendências TSan widgets e LSan continuam explícitas. A primeira publicação depende de aprovação do mantenedor.


## 0.3.0 — 2026-11-03

Refinamento visual posterior: destaque de operandos por token, separadores de funções e indicadores XREF sem acrescentar linhas de instrução; classificação de símbolos definidos/indefinidos/importados; progresso recolhido ao terminar; tema Dark original completo. Layout, APIs e testes anteriores preservados. Novo teste workspace_visual: 17/17 em GCC, Clang e ASan/UBSan. Veja docs/visual-refinement.md.

- FunctionAnalyzer/Descriptor/Registry/Confidence/Model: sementes ELF/entry/calls diretos, delimitação conservadora, retornos, duplicatas, regiões compartilhadas/sobrepostas e navegação.
- CFGBuilder/Validator, ControlFlowGraph/Edge e BasicBlock compatível: blocos, arestas internas/terminais/desconhecidas, ciclos sem recursão, ordem determinística e limites globais por pedido.
- ReferenceType/Index/Model com índices de entrada/saída, evidência de operandos RIP-relative e navegação por função com histórico.
- Graph View própria em QGraphicsView/QGraphicsScene, zoom/pan, seleção/destaque, navegação, limites de exibição e exportação PNG/SVG.
- IAnalysisBackend e backend in-process usado pelo worker; stop_token conectado ao cancelamento durante parsing e análise. Contrato lógico v1 de mensagens/estados e interfaces abstratas Linux cgroups v2 / Windows Job Objects.
- 13 testes anteriores preservados; três novos testes. GCC/Clang/ASan+UBSan 16/16; TSan core/controlador 4/4, com falhas de widgets explicitamente documentadas e sem supressões.
- Documentação de auditoria, funções, CFG, referências, isolamento e validação, com capturas offscreen.

Limites: sem processo isolado, codec IPC, políticas de SO ativas, resolução de indiretos ou recuperação completa de funções. TSan widgets permanece pendente com Qt instrumentado. Windows e desktop nativo não validados.

## 0.2.1 — 2026-11-03

- Parser ELF e disassembly em worker dedicado C++20 com cancelamento cooperativo, mailbox limitada, join e transferência de resultados sem compartilhar streams com a GUI.
- Pedidos substituídos/cancelados não publicam resultados; preservação do projeto anterior, no máximo um worker ativo e um pedido pendente por controlador.
- Progresso visível na toolbar e painel, cancelamento e diálogo de limites de memória contabilizada, tempo e instruções.
- AnalysisControl compartilhado por parsing/engine; cota atômica global por projeto entre navegações, limite de trabalho e deduplicação de destinos para proteção contra ciclos.
- Parser mantém as validações anteriores e verifica cancelamento/prazo/memória ao percorrer cabeçalhos, nomes e símbolos. I/O recusa arquivos não regulares antes de abri-los.
- APIs anteriores preservadas; overloads controlados e abertura assíncrona adicionados. openFile continua síncrono para seu chamador, executando trabalho em worker.
- Nove testes anteriores intactos; quatro novos testes de limites/cancelamento/concorrência/fuzzing. libFuzzer opcional, com corpus de quatro classes/endianness e fixture de símbolos.
- CI com ASan/UBSan, TSan do núcleo/controlador e fuzzing inicial; documentação da auditoria e evidências locais.

Limites: orçamento de memória do núcleo não limita RSS; I/O bloqueado não é interrompido à força; publicação dos modelos ainda roda na GUI. TSan de widgets exige validação com Qt instrumentado. Sem novos backends, CFG ou decompilação.

## 0.2.0 — 2026-11-03

- Assembly x86-64 real com Zydis 4.1.1/Zycore 1.5.2 locais, fixados e com licenças MIT originais.
- IInstructionDecoder, X86InstructionDecoder, DisassemblyEngine, InstructionModel, AddressTranslator e SymbolResolver.
- Instruções com VA/offset/tamanho/bytes/mnemônico/operandos/fluxo/destino; calls, jumps condicionais/incondicionais, retornos, encodings inválidos e referências RIP-relative.
- Percurso recursivo limitado a partir de entry point/símbolos de função; distinção entre confirmação por metadados, hipóteses prováveis e bytes não analisados.
- Mapeamento ET_EXEC/ET_DYN por PT_LOAD, separação entre endereço ELF e runtime, bias explícito na API, validação de overflow e ambiguidades.
- Workspace com cinco docks à esquerda, duas abas centrais, quatro docks à direita e três inferiores; Assembly colorido, cópia, follow, histórico e informações por instrução/endereço.
- QSettings para geometria/layout/tema, Dark padrão e Light opcional.
- Preservados os cinco testes da v0.1.0; quatro novos testes cobrem decoder, engine, endereçamento e interação GUI/modelo.
- Documentação Linux/Windows, auditoria, evidências, captura e avisos de terceiros.

Limites: sem decompilação, CFG, PE, resolução completa de indiretos, strings ou MCP ativo; análise síncrona e limitada. Windows/Clang ainda sem execução local.

## 0.1.0

- Projeto C++20/Qt 6/CMake, inspeção ELF32/ELF64 little/big endian, seções/símbolos, hex paginado, nove painéis e temas.
- Parser com limites e testes de entradas malformadas/arquivos grandes; cinco testes automatizados.
- GPL-3.0-or-later e CI Linux/Windows. Sem desmontagem nesta versão inicial.

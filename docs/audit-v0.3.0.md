# Auditoria — REA Studio v0.3.0

Data: 2026-11-03. Escopo: fontes próprias em src/include, todos os testes/fuzz harnesses, CMake/CI, documentação anterior e integração das dependências locais. Não é auditoria formal de todos os internals gerados do Zydis; integridade das dependências conferida pelos hashes fixados.

## Estado de entrada e preservação

A branch main não possui commits. Na entrada já havia 135 arquivos no índice, vários com alterações adicionais, e os arquivos da v0.2.1 ainda não rastreados (AnalysisControl/worker/testes/documentação/fuzzing). Nenhum reset, checkout, commit ou alteração do índice foi feito. O estado anterior não foi confundido com alterações desta entrega. APIs de Loader, decoder/DisassemblyEngine, BasicBlock/CfgBackend, Window e os 13 testes existentes foram preservados. Reference ganhou campo adicional com default, mantendo os três campos iniciais e inicialização agregada antiga.

Foram lidos docs/validation-v0.2.1.md e docs/async-analysis.md, fontes próprias, testes, fixtures e harnesses. O primeiro build com a integração nova passou nos 13 testes existentes antes da inclusão dos três novos testes. Os arquivos dos testes antigos não foram editados nesta entrega. Dependências não foram modificadas.

## Achados e decisões

| Área | Evidência / decisão |
|---|---|
| ELF/I/O | Parser usa leituras limitadas, checagens de overflow/strides/contagens/strings e limites cooperativos. BinaryFile rejeita não regulares antes de abrir. Restam TOCTOU e ausência de snapshot; stat/open e I/O remoto podem bloquear. |
| Decoder | Zydis LONG_64/Intel, destinos relativos com overflow/underflow; RIP-relative conserva endereço do operando. Não resolver indiretos ou promover seu conteúdo a destino. |
| Código executável | AddressTranslator exige PT_LOAD file-backed e trata ambiguidades. O decoder verifica cada byte contra mapeamentos executáveis; a etapa de funções só consome instruções já validadas. |
| Funções | A lista anterior era uma heurística de UI. Nova análise tem sementes separadas de branches, membros conservadores, confiança, duplicatas e sobreposição explícitas. Metadados não são prova de execução. |
| CFG | Contrato anterior não possuía implementação. Nova construção tem limites globais, ordem determinística e Kahn sem recursão. Unknown conserva destino numérico conhecido, sem criar bloco ou resolver indireto. |
| Recursos | Novas estruturas cobram trabalho/reservas antes de inserir e compartilham deadline/cancelamento. Limite de memória é contabilização conservadora, não allocator exato/RSS. Ordenações e operações individuais de containers não são interrompíveis no meio. |
| Cancelamento | jthread já existia, mas não consumia stop_token. Agora token e cancelamento atômico são ligados por stop_callback, inclusive no parser. Checkpoints adicionados por instrução/aresta/bloco/ciclo nas novas etapas. |
| Propriedade | BinaryFile/Image/ProgramAnalysis são privados do worker durante construção; done release/acquire e join antecedem publicação. GUI recebe resultado por move. Referências/modelos não são compartilhados mutavelmente entre threads. |
| Qt/lifetime | Window, modelos, scene/items e exports são manipulados na GUI. Controlador emite sinais da GUI após polling; não há necessidade de conexão queued entre emissores/receptores na mesma thread. Destruição cancela/join antes de liberar estado. Duplo clique do grafo captura VA e termina o handler base antes de emitir navegação, permitindo reconstrução segura da cena. |
| Responsividade | Novas análises ficam no worker; novo grafo tem teto visual. Seções/símbolos/formatos do modelo Assembly ainda são publicados sincronicamente; arquivos extremos podem pausar a GUI nessa fase. Não há garantia rígida de latência para publicação/join. |
| Isolamento | Backend real in-process e contratos futuros separados. Nenhum processo, codec IPC ou política de SO foi simulado. |

## Revisão individual dos logs TSan anteriores

- `build-tsan/tsan-controller.log`: três testes aprovados; inclui cota/cancelamento e handoff do controlador com QCoreApplication. Sem supressões.
- `build-tsan/tsan-run.log`: analysis_worker falhou. Escritas malloc/realloc em QArrayData de libQt6Core, threads `Thread (pooled)` T10/T12, iniciadas quando o teste mostra a Window. As outras duas entradas daquela rodada passaram.
- `build-tsan/qt-only.log`: reprodução sem REA, QCoreApplication + QThread start/wait; delete no main versus new na QThread, frames em libQt6Core. Confirma que esse Qt instalado já emite relatório na reprodução mínima; não demonstra que todo relatório Qt seja falso positivo.
- O relatório textual v0.2.1 menciona uma falha interna anterior ThreadRegistry; sem stack adicional nos três logs acima, não foi tratada como corrida reproduzida do projeto.

A rodada atual separa núcleo/controlador de widgets e conserva as falhas. Não se pode concluir que a GUI esteja livre de corridas com Qt não instrumentado. Necessário reproduzir com Qt e bibliotecas relevantes instrumentados, símbolos completos e o mesmo fluxo de UI. Nenhuma supressão global/local foi criada. Veja [validação atual](validation-v0.3.0.md).

## Arquivos desta entrega

Criados:

- `include/rea/function_analysis.hpp`, `src/function_analysis.cpp`;
- `include/rea/analysis_backend.hpp`, `src/analysis_backend.cpp`;
- `include/rea/program_models.hpp`, `src/program_models.cpp`;
- `include/rea/graph_view.hpp`, `src/graph_view.cpp`;
- `tests/program_tests.cpp`, `tests/graph_tests.cpp`;
- `docs/function-analysis.md`, `docs/control-flow-graph.md`, `docs/cross-references.md`, `docs/isolation-design.md`, `docs/validation-v0.3.0.md`, este relatório;
- `docs/workspace-graph-v0.3.0.png`, `docs/workspace-references-v0.3.0.png`, `docs/graph-true-v0.3.0.svg`.

Atualizados: `CMakeLists.txt`, `.github/workflows/ci.yml`, `README.md`, `CHANGELOG.md`, `docs/architecture.md`, `docs/workspace-true.png`, `include/rea/analysis.hpp`, `include/rea/binary.hpp` (comentário), `include/rea/disassembly.hpp`, `include/rea/analysis_control.hpp`, `include/rea/analysis_worker.hpp`, `include/rea/window.hpp`, `src/analysis_worker.cpp`, `src/window.cpp`, `src/main.cpp`.

Logs/builds/capturas adicionais estão em diretórios build* ignorados. Arquivos que já estavam alterados na entrada permanecem no estado local; git diff contra o índice inclui também a v0.2.1 e não representa sozinho o delta desta entrega.

## Pendências

Processo separado/IPC, limites reais Linux/Windows, snapshot/abertura segura contra troca de arquivo, publicação em lotes, Qt instrumentado por TSan, desktop nativo e Windows. Funções são conservadoras e podem ficar incompletas; CFG não modela jump tables/exceções/indiretos nem interprocedural non-return. PE, ARM64, decompilação e integração MCP continuam ausentes.

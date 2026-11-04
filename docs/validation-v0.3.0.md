# Validação — REA Studio v0.3.0

Data: 2026-11-03. Ambiente local: Ubuntu, GCC 15.2.0, Clang 21.1.8, Qt 6.10.2, CMake 4.2.3. [Auditoria e lista de arquivos](audit-v0.3.0.md).

O refinamento visual posterior passou em **17/17 testes GCC, Clang e ASan/UBSan**; os resultados abaixo registram a entrega inicial. Comandos, logs e novas capturas estão em [refinamento visual](visual-refinement.md). TSan não foi repetido nessa etapa visual; suas pendências continuam abertas.

## Resultado da entrega inicial

| Configuração | Resultado | Escopo |
|---|---|---|
| GCC Debug | 16/16 aprovados | 13 anteriores + 3 novos |
| Clang Debug | 16/16 aprovados | Mesma suíte |
| ASan/UBSan | 16/16 aprovados | Código C/C++ próprio + Zydis/Zycore; Qt instalado não instrumentado |
| TSan core/controller | 4/4 aprovados | program_analysis, generate_fixture, analysis_control, analysis_worker_controller |
| TSan widgets adicional | 3 falhas, fixture aprovada | graph_navigation, analysis_worker e system_graph falharam em QArrayData/Qt |
| Qt offscreen funcional | Aprovado nas três primeiras configurações | Navegação real, modelos, cenas e exportações |
| Desktop nativo / Windows | Não executado | Sem alegação de validação |

Nenhum teste anterior foi desativado ou alterado. Nenhuma supressão TSan foi usada. ASAN_OPTIONS=detect_leaks=0 mantém a limitação anterior do LeakSanitizer neste ambiente sob ptrace; não há conclusão sobre vazamentos.

## Comandos executados

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure

cmake -S . -B build-clang -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_COMPILER=clang-21 -DCMAKE_CXX_COMPILER=clang++-21
cmake --build build-clang -j"$(nproc)"
ctest --test-dir build-clang --output-on-failure

cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' \
  -DCMAKE_CXX_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' \
  -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined'
cmake --build build-asan -j"$(nproc)"
ASAN_OPTIONS=detect_leaks=0 ctest --test-dir build-asan --output-on-failure

cmake -S . -B build-tsan -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_COMPILER=clang-21 -DCMAKE_CXX_COMPILER=clang++-21 \
  -DCMAKE_C_FLAGS='-fsanitize=thread -fno-omit-frame-pointer' \
  -DCMAKE_CXX_FLAGS='-fsanitize=thread -fno-omit-frame-pointer' \
  -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=thread'
cmake --build build-tsan -j"$(nproc)"
TSAN_OPTIONS=halt_on_error=1 ctest --test-dir build-tsan \
  -R '^(generate_fixture|analysis_control|analysis_worker_controller|program_analysis)$' --output-on-failure
TSAN_OPTIONS=halt_on_error=1 ctest --test-dir build-tsan \
  -R '^(analysis_worker|graph_navigation|system_graph)$' --output-on-failure
```

Saídas persistidas em `build*/configure-v030.log`, `build*/build-v030.log`, `build/test-v030.log`, `build-clang/test-v030.log`, `build-asan/test-v030.log`, `build-tsan/core-v030.log`, `build-tsan/widgets-v030.log`. CTest também mantém Testing/Temporary/LastTest.log (no TSan a última rodada contém widgets; core-v030.log preserva a rodada aprovada).

## Cobertura

| Teste novo | Verificações |
|---|---|
| program_analysis | Entry/símbolos/calls, confiança, duplicatas, limite ELF, região não executável, sobreposição de metadados/caudas, jumps não viram funções, destinos desconhecidos/externos, blocos/arestas/loops, xrefs tipadas e índices, nenhum destino inventado de call indireto, grafos malformados, limites de nós/arestas/memória, cancelamento após iniciar CFG, análises concorrentes determinísticas, tags/versão/estados do protocolo e stop_token |
| graph_navigation | Fixture ELF; modelos com QAbstractItemModelTester; duplo clique de referências, funções e blocos; histórico; seleção; zoom; limites de cena; PNG decodificável; SVG com XML/texto/arestas válidos; erros de exportação; capturas |
| system_graph | Mesmo fluxo de Graph View/referências sobre /usr/bin/true |

Os 13 testes anteriores continuam cobrindo ELF32/64/endian, 60 malformados, arquivo esparso de 5 GiB, Assembly/VA/offset, limites, cancelamento, cota compartilhada, lifetime/supersessão de workers, heartbeat GUI, navegação/cópia/temas/QSettings, /usr/bin/true e 5735 mutações/prefixos do fuzz smoke. Não foi repetida uma campanha libFuzzer nesta entrega; o resultado histórico de 10.000 execuções da v0.2.1 continua apenas histórico.

## Falhas observadas e classificação

Durante desenvolvimento houve erro de compilação no construtor QPainterPath (dois escalares em vez de QPointF), corrigido. A primeira execução dos testes novos falhou por sequência incompleta de clique no Qt e por assumir referência direta decodificada na função de entry de /usr/bin/true. O teste agora faz clique+duplo clique reais e navega para função com referência direta disponível; não removeu nenhuma asserção de navegação. As rodadas finais passaram nas configurações funcionais e ASan/UBSan.

TSan widgets permanece **reprovado**: nos três testes, malloc em QArrayData::allocate versus realloc em QArrayData::reallocateUnaligned, libQt6Core.so.6, threads `Thread (pooled)`, criadas durante Window::show. O núcleo novo concorrente e o controlador com QCoreApplication passaram. O padrão é semelhante ao relatório anterior e há reprodução mínima histórica sem código REA, revisada individualmente na auditoria. Isso indica uma limitação da análise com bibliotecas não instrumentadas, mas não permite descartar automaticamente corridas ou aprovar toda a GUI. Necessário Qt instrumentado e investigação com símbolos. Logs completos preservados, sem supressões.

A build emite avisos CMake de compatibilidade antiga nas dependências vendorizadas; não foram modificadas para silenciá-los. Builds finais não têm warnings C++ do projeto.

## Artefatos e inspeção visual

- `build/workspace-true.png`, `build/workspace-fixture.png`: Assembly/hex/workspace; cópia atual em docs/workspace-true.png.
- `build/graph-true-workspace.png`, `build/graph-fixture-workspace.png`: Graph View com nós, setas, bloco ativo e informação de instrução.
- `build/graph-true-references.png`, `build/graph-fixture-references.png`: aba de referências da função.
- `build/graph-{true,fixture}.png` e `.svg`: exportação da cena, diferente de captura da janela.
- Cópias permanentes em docs/workspace-graph-v0.3.0.png, docs/workspace-references-v0.3.0.png e docs/graph-true-v0.3.0.svg.

Capturas offscreen inspecionadas visualmente. Não equivalem a execução em desktop nativo. /usr/bin/true foi aberto/decodificado, sem executar o alvo: 255 instruções, 80 referências legadas, entry ELF 0x2db0/endbr64, 32 seções e 50 símbolos neste ambiente.

`git diff --check` passou. Os 107 hashes em `third_party/SHA256SUMS` conferem (`build/dependency-integrity-v030.txt`); dependências não foram alteradas. O índice Git foi preservado e a branch continua sem commits.

## Limites do resultado

Funções e CFG são conservadores; não há resolução de indiretos/jump tables/exceções. Os limites de análise e exibição são distintos. Etapa ProgramAnalysis interrompida é descartada integralmente, com Assembly parcial/diagnóstico preservado quando válido. Cancelamento continua cooperativo; syscall bloqueada e publicação síncrona de modelos não têm latência máxima garantida. Memória contabilizada não limita RSS. Backend é in-process; contrato de processo/OS não equivale a isolamento real. Windows, ARM64, PE e desktop nativo não são suportes validados nesta entrega.

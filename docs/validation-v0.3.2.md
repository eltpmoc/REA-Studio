# Validação da primeira etapa v0.3.2

Estado: implementação parcial; não é gate de release concluído.

| Configuração local | Resultado |
|---|---|
| GCC Debug | 26/26 |
| Clang Debug | 26/26 |
| ASan/UBSan | 26/26 |
| TSan núcleo/controlador | 9/9 |
| Windows nativo | Não executado nesta árvore |
| Linux Release/pacote desta versão | Pendente |
| Desktop Qt nativo | Pendente; testes usam offscreen |

Executado cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug, cmake --build build -j4 e ctest --test-dir build --output-on-failure. Demais configurações usam caches instrumentados documentados na v0.3.1; ASAN_OPTIONS=detect_leaks=0 mantém a limitação LSan explícita. TSan widgets permanece pendente, sem supressões globais.

Logs locais: build/v032-configure.log, build/v032-build.log, build/v032-tests.log; equivalentes em build-clang, build-asan e build-tsan. Build final sem warnings do projeto encontrados. CMake emitiu avisos de depreciação dos minimum_required vendorizados; dependências não foram alteradas para ocultá-los.

Novo teste control_flow_facts verifica diamante/joins, dominadores, pós-dominadores, laço natural, saída desconhecida, grafo malformado, cancelamento e orçamento. Os25 testes anteriores continuam passando; o ELF obrigatório continua sendo lido sem modificação. Fuzz smoke integra a suíte; nenhuma nova campanha libFuzzer foi executada nesta etapa.

Auditoria de licenças/inventário e hashes das107 dependências passou. git diff --check passou. Índice Git e fixture preservados. GitHub Actions consultado pertence à v0.3.0 e falhou; não foi anunciado como validação da v0.3.2.

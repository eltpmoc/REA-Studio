# Auditoria inicial v0.3.2-alpha

Raiz de desenvolvimento: REA-Studio; nenhum desenvolvimento em build/publication-review.

A raiz local estava em main sem commits, com índice de primeira importação e arquivos adicionais não rastreados. git log local não possuía HEAD. origin estava ausente; foi configurado para https://github.com/eltpmoc/REA-Studio.git e fetch executado com sucesso, sem checkout/reset dos arquivos. Backup do índice: build/audit-v032/original-index; hashes do índice e fixture em preserved.sha256.

O histórico remoto contém main e release/v0.3.0-alpha. A branch anterior contém o merge 9fd8269 que preserva o histórico GitHub e adota GPL. Nenhum histórico foi reescrito. A comparação simples git diff remoto não inclui arquivos locais untracked: esses arquivos não devem ser interpretados como exclusões intencionais.

GitHub autenticado via keyring fora do sandbox. A falha inicial de autenticação/rede no sandbox não indicava credencial realmente inválida. PR #1 da v0.3.0-alpha permanece aberto. Nenhuma release foi listada. Run 41767825769 falhou: Linux GCC/Clang em CTest, Windows em Configure, ASan/UBSan em testes; TSan passou. Esses resultados são anteriores à v0.3.2 e não validam esta árvore.

Publicação da v0.3.2 autorizada, mas ainda não realizada. Não substituir tags, não force push, não contornar proteções. Recuperação estruturada e validação Windows continuam gates pendentes.

## Publicação incremental em 2026-11-04

Branch `develop/v0.3.2-alpha` criada sobre `origin/release/v0.3.0-alpha` (9fd8269), pois main ainda contém somente a importação inicial. Commit 754039f publicado e confirmado via ls-remote; PR Draft #2: https://github.com/eltpmoc/REA-Studio/pull/2. A base temporária depende do PR #1. Índice anterior preservado em /tmp/rea-studio-index-before-publication-20261104. Não houve tag, release, exclusão ou reescrita de histórico.

Logs do run 41767825769:
- Linux GCC/Clang, Ubuntu 24.04: `ctest --preset linux-release --output-junit release-tests.xml`, saída 8. `system_graph`: `FAIL: navigable function reference`; `system_workspace`: `FAIL GUI: fixture must contain a mapped direct branch/call`.
- ASan/UBSan, Ubuntu 24.04: `ctest --test-dir build --output-on-failure --output-junit sanitizer-tests.xml`, mesmas duas falhas, saída 8. TSan passou.
- Windows Server 2025, imagem `windows-2025-vs2026`: `cmake --preset windows-x64`, saída 1: `Generator Visual Studio 17 2022 could not find any instance of Visual Studio.` O runner latest passou a fornecer VS 2026; preset e Qt usam MSVC 2022. Correção: runner windows-2022.

Os testes GUI completos continuam exigindo branches e navegação na fixture smoke.elf. Os testes system_graph/system_workspace passam a validar o binário real sem pressupor que /usr/bin/true tenha branches diretos alcançáveis: bytes, referências derivadas de instruções, CFG e renderização, navegação hex/histórico. Não se amplia o disassembler com varredura especulativa para satisfazer uma suposição do teste. TSan passa a incluir control_flow_facts, antes ausente do workflow.


Após 99adee5, run 41943653278: GCC/Clang passaram. Windows compilou e passou 20/20 testes, mas o pacote falhou com `Unreviewed Qt module: Qt6Network.dll`. O próprio log atribui a inclusão a `qtuiotouchplugin.dll` do tipo `generic`, embora as dependências diretas e transitivas da aplicação sejam Core/Gui/Widgets. O código oficial QtBase 6.8.3 (`src/tools/windeployqt/main.cpp`) oferece `--skip-plugin-types`; acrescentado `generic` para evitar incluir a dependência do plugin não distribuído. A lista permitida de DLLs e os testes do pacote permanecem obrigatórios. Versão CMake e nomes dos pacotes alinhados a 0.3.2-alpha; isso não cria release.

O mesmo run apresentou ASan function_navigation (`actual Assembly address selected`), não reproduzido nos testes ASan locais; causa ainda não comprovada. TSan process_memory falhou no runtime durante QProcess/clone (`tsan_rtl.cpp:253`, `!thr->slot`), não um diagnóstico de data race. Qt 6.4.2 usa vforkfd nesse caminho, conforme fonte oficial `src/corelib/io/qprocess_unix.cpp`; nenhuma supressão ou remoção de testes aplicada. Ambos permanecem gates obrigatórios.

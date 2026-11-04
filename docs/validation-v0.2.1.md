# Validação da versão 0.2.1

Data: 2026-11-03. Ubuntu, Qt 6.10.2, CMake 4.2.3, GCC 15.2.0 e Clang 21.1.8. Baseline auditada antes das alterações: **9/9 testes aprovados**. Veja [auditoria](audit-v0.2.1.md).

## Compilação e suíte funcional

Comandos solicitados executados:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure
```

Resultado GCC: **13/13 aprovados**. Build adicional com `-DCMAKE_C_COMPILER=clang-21 -DCMAKE_CXX_COMPILER=clang++-21` em build-clang: **13/13 aprovados**. Qt em modo offscreen para widgets. Os arquivos dos nove testes anteriores não foram alterados.

| Teste | Evidência |
|---|---|
| core | ELF32/64, duas ordens de bytes, 60 malformados, tabelas e arquivo esparso de 5 GiB |
| generate_fixture | Fixture válida com entry e instruções reais |
| disassembly | Calls/jumps/returns, inválidos, truncados, VA/offset, hipóteses e budgets |
| workspace_gui | Entry/Assembly/hex, duplo clique, histórico, cópia, temas/QSettings e rollback |
| gui_smoke | Janela com fixture ELF |
| system_elf | Parser com /usr/bin/true |
| system_disassembly | Assembly real do entry de /usr/bin/true |
| system_workspace | Interação GUI com /usr/bin/true |
| true_gui | Smoke anterior com /usr/bin/true |
| analysis_control | Cancelamento no parser/decoder, prazo, memória, trabalho, ciclos e cota concorrente 37/37 |
| analysis_worker | Progresso, heartbeat GUI, cancelamento, supersessão, concorrência, fechamento, limites e publicação |
| analysis_worker_controller | Mesmos cenários do controlador sem widgets, via QCoreApplication |
| elf_fuzz_smoke | 5735 mutações/prefixos, 1615 aceitos e 4120 recusados |

O worker/controller verifica substituição de pedidos ativos e pendentes, duas análises independentes disputando cota compartilhada (97/97), cancelamento isolado e join durante destruição. A GUI verifica que cancelamentos preservam resultados, pedido antigo não substitui o novo, barra chega a 100 no sucesso e navegar não reinicia a cota global. FIFO/diretório são recusados antes de abertura bloqueante (teste FIFO só no POSIX).

O binário alvo não foi executado:

```text
/usr/bin/true: ELF entry VA 0x2db0, offset 0x2db0: endbr64 ; 255 instructions; 80 references
  0x2db0 endbr64
  0x2db4 xor ebp, ebp
  0x2db6 mov r9, rdx
```

32 seções e 50 símbolos neste ambiente. A captura atual é gerada em build/workspace-true.png pelo teste e copiada para docs/workspace-true.png. Houve inspeção da captura offscreen; não equivale a interação em desktop nativo ou validação Windows.

## ASan + UBSan

```sh
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' \
  -DCMAKE_CXX_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' \
  -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined'
cmake --build build-asan -j"$(nproc)"
ASAN_OPTIONS=detect_leaks=0 ctest --test-dir build-asan --output-on-failure
```

**13/13 aprovados**, incluindo widgets, parser, engine, cancelamento/concorrência e mutações. C e C++ do projeto/Zydis/Zycore instrumentados. Qt instalado não foi recompilado. LeakSanitizer desativado por incompatibilidade com ptrace já observada no ambiente; não há alegação sobre vazamentos.

## ThreadSanitizer

```sh
cmake -S . -B build-tsan -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_COMPILER=clang-21 -DCMAKE_CXX_COMPILER=clang++-21 \
  -DCMAKE_C_FLAGS='-fsanitize=thread -fno-omit-frame-pointer' \
  -DCMAKE_CXX_FLAGS='-fsanitize=thread -fno-omit-frame-pointer' \
  -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=thread'
cmake --build build-tsan --target rea_analysis_tests rea_worker_tests rea_tests --parallel
TSAN_OPTIONS=halt_on_error=1 ctest --test-dir build-tsan \
  -R '^(generate_fixture|analysis_control|analysis_worker_controller)$' --output-on-failure
```

**3/3 aprovados**, sem supressões: parser controlado, núcleo concorrente e controlador real em std::jthread, com QCoreApplication. Isso valida o handoff/mutex/atomics/cancelamento do próprio projeto no escopo dos testes.

**Limitação explícita:** a execução TSan adicional de analysis_worker com QApplication/widgets **não passou**. Relatório final aponta malloc/realloc em QArrayData no pool `Thread (pooled)` de libQt6Core.so.6. Uma reprodução mínima separada, apenas QCoreApplication e QThread start/wait, sem código do REA, também emitiu relatório TSan no Qt instalado. Antes da adoção do worker dedicado também houve falha interna de ThreadRegistry. Isso não autoriza afirmar que toda a GUI está livre de corridas; será necessário executar com Qt instrumentado e investigar os relatórios desse ambiente. Nenhuma supressão foi aplicada para transformar a falha em aprovação. ASan/UBSan e testes funcionais de widgets passaram.

## Fuzzing guiado por cobertura

Clang 21.1.8 + libFuzzer/ASan/UBSan: **10.000 execuções**, corpus inicial com quatro combinações classe/endian e ELF com símbolos; sem crash reportado. Rodada final: cov 2025, ft 4766, corpus 59/36 KiB, 1 segundo. Veja [comandos, escopo e oráculos](fuzzing.md). Campanha curta não prova ausência de falhas.

## Artefatos e limites

- build/Testing/Temporary/LastTest.log;
- build-clang/Testing/Temporary/LastTest.log;
- build-asan/Testing/Temporary/LastTest.log;
- build-tsan/tsan-controller.log (aprovação do escopo concorrente);
- build-tsan/tsan-run.log e build-tsan/qt-only.log (relatórios Qt, sem aprovação de widgets);
- build-fuzz/fuzz-final.log e diretório de artefatos;
- build/workspace-true.png, build/workspace-fixture.png.

`cmake --install build --prefix /tmp/rea-studio-v0.2.1-install` passou, instalando executável e licenças. `git diff --check` passou; os nove testes anteriores têm diff vazio. Hashes dos 107 arquivos de terceiros conferidos sem alterações. Repositório segue sem commits, com índice anterior preservado. Windows e desktop nativo ainda dependem de validação externa; CI foi preparado, mas nenhum resultado remoto é alegado.

Memória é limite de reservas conservadoras do núcleo por pedido, **não de RSS**; prazo/cancelamento são cooperativos e não interrompem syscalls bloqueadas. Publicação de modelos ainda é feita na GUI. Esses limites estão descritos em [execução assíncrona](async-analysis.md).

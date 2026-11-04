# Evidências de validação — REA Studio 0.2.0

Execução local em 2026-11-03 no Ubuntu, com GCC 15.2.0, Qt 6.10.2 e CMake 4.2.3. A baseline v0.1.0 passou 5/5 antes das alterações. Não há repositório Git válido neste workspace: `.git` está vazio e `git status`/`git log` falham. Não foi criado commit.

## Comandos solicitados

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure
```

Configuração e compilação concluídas. Os arquivos originais do projeto compilam com `-Wall -Wextra -Wpedantic -Wconversion`, sem warnings na compilação final. Há avisos de depreciação do CMake mínimo dos fornecedores e ausência opcional de Doxygen; não impedem build/testes e os arquivos upstream foram preservados.

Resultado final:

```text
disassembly          Passed
generate_fixture     Passed
workspace_gui        Passed
core                 Passed
gui_smoke            Passed
system_disassembly   Passed
system_workspace     Passed
system_elf           Passed
true_gui             Passed
100% tests passed, 0 tests failed out of 9
Total Test time (real) = 0.91 sec
```

Os cinco nomes antigos foram preservados. A fixture gerada para smoke agora aponta o entry para um pequeno trecho x86-64 real, com calls/jumps/retornos, em vez de solicitar decodificação do cabeçalho ELF. As asserções originais do parser continuam presentes. São cinco testes portáveis mais quatro condicionais à existência de `/usr/bin/true`.

## Assembly real e navegação

`./build/rea_disassembly_tests /usr/bin/true`:

```text
/usr/bin/true: ELF entry VA 0x2db0, offset 0x2db0: endbr64 ; 255 instructions; 80 references
  0x2db0 endbr64
  0x2db4 xor ebp, ebp
  0x2db6 mov r9, rdx
  0x2db9 pop rsi
  0x2dba mov rdx, rsp
  0x2dbd and rsp, 0xFFFFFFFFFFFFFFF0
  0x2dc1 push rax
  0x2dc2 push rsp
PASS: x86-64 decoder, flows, translation, recursive analysis, confidence and budgets
```

Verificação independente com `objdump -d --start-address=0x2db0 --stop-address=0x2dd6 /usr/bin/true` confirmou bytes, endereços e operações desse trecho (objdump utiliza AT&T; a aplicação utiliza Intel). Objdump foi utilizado apenas para verificação, não é dependência do aplicativo. O binário alvo não foi executado.

Os testes `system_workspace` e `workspace_gui` criam a janela e verificam:

- Assembly central com entry selecionado e instrução válida; tabela de seções e bytes hex presentes; doze docks;
- shape/roles do modelo e invariantes via QAbstractItemModelTester;
- destino direto mapeado seguido por evento de duplo clique real no viewport;
- voltar para a origem e avançar para o destino pelo histórico;
- cópia de Assembly para clipboard e navegação por offset para Hex Viewer;
- rejeição de offsets/VAs inválidos e destinos no interior de instrução;
- carregamento de ELF malformado preservando projeto anterior e registrando erro;
- troca Dark/Light e persistência de tema/layout por QSettings em diretório temporário.

Capturas geradas: `build/workspace-true.png`, `build/workspace-fixture.png`. [Captura do workspace com /usr/bin/true](workspace-true.png) foi inspecionada visualmente. Esses testes usam **Qt offscreen**; não constituem validação em desktop nativo Wayland/X11/Windows. A mensagem `This plugin does not support propagateSizeHints()` é emitida pelo plugin offscreen.

## Sanitizadores

```sh
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' \
  -DCMAKE_CXX_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' \
  -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined'
cmake --build build-asan -j"$(nproc)"
ASAN_OPTIONS=detect_leaks=0 ctest --test-dir build-asan --output-on-failure
```

**9/9 aprovados**, 2.27 segundos. C e C++ instrumentados, incluindo Zydis/Zycore. LeakSanitizer foi desativado porque a baseline já demonstrou incompatibilidade com ptrace neste ambiente. Não há evidência de teste de vazamentos; Qt instalado não foi recompilado com sanitizadores.

## Dependências e instalação

- Zydis v4.1.1, commit `a2278f1d254e492f6a6b39f6cb5d1f5d515659dc`.
- Zycore 1.5.2, commit `0b2432ced0884fd152b471d97ecf0258ff4d859f`.
- 107 arquivos upstream incluídos sem alterações; licenças MIT preservadas, hashes em `third_party/SHA256SUMS`.
- `sha256sum -c third_party/SHA256SUMS`: sucesso.
- `cmake --install build --prefix /tmp/rea-studio-v0.2.0-install`: sucesso; executável e licenças GPL/Zydis/Zycore/avisos instalados. Qt permanece dependência do sistema/deployment.

Logs detalhados locais: `build/Testing/Temporary/LastTest.log`, `build-asan/Testing/Temporary/LastTest.log`. O workflow publica logs/capturas por compilador. Windows e Clang ainda não foram executados nesta entrega; nenhum resultado de CI remoto é alegado.

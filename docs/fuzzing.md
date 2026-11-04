# Fuzzing inicial do ELF — v0.2.1

Há dois caminhos que usam o mesmo harness de parser, sem executar o alvo:

1. `rea_elf_fuzz_smoke`: teste portável de mutações determinísticas, sempre disponível em BUILD_TESTING.
2. `rea_elf_fuzzer`: alvo opcional do **libFuzzer**, compilado e instrumentado com Clang/ASan/UBSan.

## Smoke portável

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build -R '^(generate_fixture|elf_fuzz_smoke)$' --output-on-failure
./build/rea_elf_fuzz_smoke build/smoke.elf
```

As quatro sementes mínimas cobrem ELF32/ELF64 × little/big endian. A fixture de símbolos da suíte anterior adiciona tabelas de seções, strings e símbolos. O runner testa prefixos truncados e 5000 mutações: flips de bits, valores extremos em registros, truncamentos, cabeçalhos e bytes aleatórios. Seed PRNG fixa `0x524541`. Resultado local: **5735 casos**, **1615 aceitos**, **4120 recusados**; as quatro sementes válidas são verificadas separadamente.

Isto é um smoke de mutações, não uma campanha exaustiva nem cobertura guiada por si só.

## libFuzzer com Clang

```sh
cmake -S . -B build-fuzz -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_COMPILER=clang-21 -DCMAKE_CXX_COMPILER=clang++-21 \
  -DBUILD_TESTING=OFF -DREA_BUILD_LIBFUZZER=ON
cmake --build build-fuzz --target rea_elf_fuzzer --parallel
./build/rea_elf_fuzz_smoke --write-corpus build-fuzz/corpus
cp build/smoke.elf build-fuzz/corpus/symbols.elf
mkdir -p build-fuzz/artifacts
ASAN_OPTIONS=detect_leaks=0 ./build-fuzz/rea_elf_fuzzer build-fuzz/corpus \
  -runs=10000 -seed=5391681 -max_len=65536 -timeout=2 \
  -rss_limit_mb=512 -artifact_prefix=build-fuzz/artifacts/
```

Use os nomes de Clang disponíveis no sistema (por exemplo clang/clang++ sem sufixo). O alvo compila diretamente BinaryFile, ElfLoader e AnalysisControl; não depende de Zydis ou da GUI em execução. A configuração do projeto ainda requer os pacotes Qt habituais. `-fsanitize=fuzzer,address,undefined` é aplicado a todo o código do alvo, não apenas à função de entrada. Não combine esse alvo com flags globais de ThreadSanitizer.

O corpus é ampliado por libFuzzer e crashes/timeouts produzem artefatos no diretório indicado. Para reproduzir: `ASAN_OPTIONS=detect_leaks=0 ./build-fuzz/rea_elf_fuzzer caminho/do/artefato`. Arquive/minimize os casos antes de aumentar o tempo de campanha. O CI roda 10.000 execuções iniciais com a mesma configuração e publica logs/artefatos.

## Harness e oráculos

Entradas de até 1 MiB são escritas em scratch exclusivo por thread/processo. Cada iteração recebe orçamento de 2 MiB contabilizados, 50 ms e 20.000 itens de trabalho. Falhas esperadas de validação/limites são recusas normais; bad_alloc, violações de invariantes e erros de sanitizadores não são silenciados.

Para arquivos aceitos, verificam-se ranges de seções com conteúdo, ranges de programas, overflow virtual e orçamento de memória contabilizada. As validações estruturais do parser continuam ativas em todas as iterações. Corpus mínimo e mutações não validam semântica completa de ABI nem equivalência a parsers externos.

## Evidência local

Clang 21.1.8, ASan/UBSan: **10.000 execuções concluídas**, sem crash reportado. A rodada final registrou `cov: 2025`, `ft: 4766`, corpus final 59 entradas / 36 KiB. São contadores do libFuzzer, não percentuais de cobertura de código. `-max_len=65536` é o teto configurado; a rodada curta terminou com limite evolutivo de 1024 bytes. Tempo aproximado: 1 segundo, RSS reportado: 274 MiB (inclui sanitizadores; não representa o orçamento lógico do parser).

LSan foi desativado por limitações do ambiente sob ptrace; vazamentos não foram avaliados. Não se afirma ausência de bugs após essa campanha inicial. Expanda o corpus com numeração estendida, NOBITS, símbolos dinâmicos, tabelas grandes e formatos menos comuns, e faça campanhas prolongadas com retenção de artefatos.

Referência do backend: [LLVM libFuzzer](https://llvm.org/docs/LibFuzzer.html).

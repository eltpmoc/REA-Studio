# Build Linux x86_64

Requisitos: Linux x86_64, compiladores C/C++ com C++20 e std::jthread, CMake >=3.21, Qt >=6.2 Widgets/Test, threads POSIX e Python >=3.10 para auditoria/empacotamento. Sem downloads de dependências no CMake: Zydis/Zycore são locais. Um compilador antigo sem std::jthread não atende ao requisito.

Ubuntu:

```sh
sudo apt-get update
sudo apt-get install build-essential cmake qt6-base-dev qt6-base-dev-tools python3
python3 scripts/release.py audit
cmake --preset linux-release
cmake --build --preset linux-release -j"$(nproc)"
ctest --preset linux-release --output-junit release-tests.xml
./build-release/rea-studio /usr/bin/true
```

O preset usa Unix Makefiles e build-release. Para Clang, configure outro diretório:

```sh
CC=clang CXX=clang++ cmake -S . -B build-clang -DCMAKE_BUILD_TYPE=Debug
cmake --build build-clang -j"$(nproc)"
ctest --test-dir build-clang --output-on-failure
```

Se o compilador já estiver fixado no cache, use um diretório novo ou mantenha o compilador existente; não misture runtimes/sanitizers em um build. Para Qt fora do prefixo do sistema, passe -DCMAKE_PREFIX_PATH=/caminho/Qt. O empacotador Linux desta alpha espera Qt compartilhado disponível pelo loader do sistema; para instalação de Qt privada, ajuste o deployment antes de distribuir, sem alegar compatibilidade do pacote atual.

ASan/UBSan:

```sh
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' \
  -DCMAKE_CXX_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' \
  -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined'
cmake --build build-asan -j"$(nproc)"
ASAN_OPTIONS=detect_leaks=0 ctest --test-dir build-asan --output-on-failure
```

LeakSanitizer está desativado apenas na configuração documentada do ambiente local sob ptrace; não se afirma ausência de vazamentos. Os relatórios TSan de widgets continuam pendentes; o workflow TSan exige os quatro testes de núcleo/controlador, sem supressões, e não aprova widgets.

Pacote com Qt do sistema:

```sh
python3 scripts/release.py linux --build build-release --output dist
(cd dist && sha256sum -c SHA256SUMS.txt)
```

O script exige Release, executa CTest, audita fontes/licenças, instala em staging exclusivo, inclui fontes, verifica ldd e executa smoke offscreen antes de criar o tar.gz. O ELF analisado nunca é executado. Não inclui Qt/glibc/libstdc++; a compatibilidade é com o ambiente reportado, não com toda distribuição Linux.

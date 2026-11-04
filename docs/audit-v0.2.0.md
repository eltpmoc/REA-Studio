# Auditoria anterior à implementação de v0.2.0

Data: 2026-11-03. Workspace existente, sem recriação do projeto.

## Baseline observado

CMake 4.2.3, GCC 15.2.0, Qt 6.10.2, Ubuntu. Antes de modificar o projeto:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure
```

Resultado: **5/5 aprovados**, nomes `core`, `generate_fixture`, `gui_smoke`, `system_elf`, `true_gui`.

## Achados e tratamento

| Área | Estado encontrado | Decisão na v0.2.0 |
|---|---|---|
| Git | `.git` é um diretório vazio; status/log retornam `fatal: not a git repository` | Não inicializar nem inventar histórico; alterações descritas por arquivo |
| Arquitetura | `rea_core` independente de Qt, `Loader`, modelo Image e contratos Assembly/CFG/MCP | Preservar loader/modelo e separar biblioteca `rea_ui`; implementar decoder/engine próprios |
| CMake | C++20, Qt Widgets, core, GUI e cinco testes | Ativar C para Zydis/Zycore, dependências locais fixadas, Qt Test apenas com BUILD_TESTING |
| ELF | Leitura campo a campo/endian explícito, checagem de ranges, contagens e strides; NOBITS e numeração estendida de headers | Preservar parser e testes; documentar ausência de SHN_XINDEX/relocações/verificador ABI completo |
| Mapeamento | `Image::offsetForAddress` soma offset e delta sem checagem própria; usa só sentido VA→offset | Proteger API antiga contra overflow; criar AddressTranslator com file size, sentido inverso, tipo ELF e bias explícito |
| Modelos Qt | Sections/Symbols em QTableWidget, hex paginado em QPlainTextEdit, sem modelo de instruções | Preservar os existentes; novo InstructionModel derivado de QAbstractTableModel e teste de invariantes |
| Interface | Nove docks e centro com mensagem; sem Assembly e sem persistência | Doze docks periféricos e duas abas centrais; tema e layout por QSettings |
| Testes | Quatro combinações ELF/endian, 60 malformados, símbolos, 5 GiB e smoke offscreen | Preservar os cinco nomes/asserções, adicionar decoder, engine, tradução, confiança e interação GUI |
| Licenças | Original GPL-3.0-or-later, nenhuma dependência de decoder | Zydis/Zycore MIT com textos originais, commits fixados e hashes; não alterar licenças |

## Limites remanescentes

Não é um parser isolado ou à prova de exaustão de todos os recursos: há limites por tabela/nome/instrução, mas sem orçamento global de memória nem cancelamento. Parser e construção das tabelas Qt continuam síncronos. Metadados ELF podem ser falsos e o termo Confirmed qualifica a evidência de metadados, não execução observada. CI preparado não equivale a Windows/Clang testados localmente.

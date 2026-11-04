# Preparação pública — REA Studio v0.3.0-alpha

Data: 2026-11-03. Esta é uma preparação local, sem push, tag, release remota ou disparo de GitHub Actions. O repositório de destino foi consultado em modo somente leitura.

## Git e remoto

Branch local main sem commits; índice pré-existente com 135 arquivos e alterações adicionais/untracked das entregas anteriores. Nenhum reset, checkout sobre a árvore original ou commit foi feito. Na entrada não havia origin configurado. A auditoria usa uma cópia bare ignorada em build/remote-publication-audit.git.

`git ls-remote --symref https://github.com/eltpmoc/REA-Studio.git HEAD refs/heads/main` retornou main / 5de48bf0877878a11aeeef40e2c97b17e0366493. Log remoto: um Initial commit. Árvore remota: apenas LICENSE MIT, copyright 2026 Elton Alves P. Conteúdo foi lido antes de preparar qualquer reconciliação; nada foi substituído no remoto. O acesso inicial sandbox falhou em DNS, e a consulta autorizada fora do sandbox funcionou.

A raiz local GPL é canônica e conserva SHA-256 3972dc9744f6499f0f9b2dbf76696f2ae7ad8af9b23dde66d6af86c9dfb36986. Não se alterou o texto legal só para atualizar a versão; COPYING.md e THIRD_PARTY_NOTICES.md atualizam os avisos e o contexto. Aprovação de titularidade/licença e preservação do commit remoto são gates de publicação. A cópia de revisão local baseada no remoto é descrita em release-process.md.

## Auditoria de publicação

107/107 hashes de third_party/SHA256SUMS conferem; conjunto real igual ao manifesto, sem extras. MIT Zydis/Zycore mantido. LGPL-3 integral adicionado para documentação/redistribuição de Qt, sem mudar as licenças dos componentes. Sem arquivos executáveis, objetos, libs, pacotes, caches ou credenciais no inventário permitido; screenshots PNG/SVG em docs são assets públicos intencionais.

.gitignore exclui builds/dist/staging, executáveis/DLLs/objetos, archives, caches, .env/chaves e configurações locais .aws/.codex/.agents. .gitattributes preserva bytes de third_party em Windows. scripts/release.py audit também verifica o índice, pois ignore não retira arquivo já staged. A busca por padrões conhecidos não é garantia absoluta de ausência de segredos; revisão manual do inventário/diff permanece necessária.

## Plataformas

Linux local: Ubuntu 26.04.1 LTS x86_64, GCC 15.2.0, Clang 21.1.8, Qt 6.10.2, CMake 4.2.3. Release GCC aprovado 17/17, Clang Debug e ASan/UBSan aprovados 17/17. TSan núcleo/controlador aprovado 4/4. Qt Widgets usa offscreen; desktop nativo não validado. LeakSanitizer continua desativado neste ambiente sob ptrace. Widgets TSan não foram reclassificados: falhas históricas QArrayData/pool Qt permanecem abertas, sem supressões.

Windows não disponível: sem MSVC, Windows host, PowerShell ou Wine. Cross-compile/Wine tampouco seria substituto de testes reais Windows. Ajustes estáticos: /utf-8 em todos os alvos, NOMINMAX/WIN32_LEAN_AND_MEAN, CRT /MD(d) e preset Visual Studio 17 2022 x64. API Windows do worker já usa path wide; FIFO é !\_WIN32. Zydis C local usa toolchain MSVC sem edição vendorizada. Preservados os 17 testes, dos quais 12 aplicáveis no Windows e cinco específicos de /usr/bin/true.

O workflow windows-latest deve compilar, testar, verificar licenças/fontes/DLLs, executar pacote com PATH sem Qt de desenvolvimento (offscreen e qwindows) e só então produzir ZIP. Nenhuma execução desse workflow é alegada. O critério de Windows real permanece bloqueado até seu resultado. Não foi criado ZIP Windows fictício.

## Comandos e logs locais

```sh
python3 scripts/release.py audit
cmake --list-presets
cmake --preset linux-release
cmake --build --preset linux-release -j"$(nproc)"
ctest --preset linux-release --output-junit release-tests.xml
cmake -S . -B build-clang -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=clang-21 -DCMAKE_CXX_COMPILER=clang++-21
cmake --build build-clang -j"$(nproc)"
ctest --test-dir build-clang --output-on-failure --output-junit publication-tests.xml
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' \
  -DCMAKE_CXX_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' \
  -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined'
cmake --build build-asan -j"$(nproc)"
ASAN_OPTIONS=detect_leaks=0 ctest --test-dir build-asan --output-on-failure --output-junit publication-tests.xml
cmake -S . -B build-tsan -DCMAKE_BUILD_TYPE=Debug
cmake --build build-tsan --target rea_analysis_tests rea_worker_tests rea_tests rea_program_tests -j"$(nproc)"
TSAN_OPTIONS=halt_on_error=1 ctest --test-dir build-tsan \
  -R '^(generate_fixture|analysis_control|analysis_worker_controller|program_analysis)$' --output-on-failure
python3 scripts/release.py linux --build build-release --output dist
(cd dist && sha256sum -c SHA256SUMS.txt)
git diff --check
```

TSan cache mantém Clang 21 e -fsanitize=thread, conforme validação anterior. Logs: build/{configure,build,test}-publication-release.log; build-clang e build-asan/{configure,build,test}-publication.log; build-tsan/core-publication.log. JUnit em build-release/release-tests.xml, build-clang/publication-tests.xml, build-asan/publication-tests.xml e build-tsan/publication-core-tests.xml. Integridade inicial em build/dependency-publication-check.txt.

## Artefatos candidatos

- dist/REA-Studio-v0.3.0-alpha-Linux-x86_64.tar.gz: bin/rea-studio e share/rea-studio com documentação, licenças, metadata/dependências e source archive correspondente. Qt e bibliotecas do SO não são empacotados.
- dist/REA-Studio-v0.3.0-alpha-Source.tar.gz: fonte completa por allowlist, com 107 arquivos vendorizados, scripts e testes, sem Git/builds/credenciais.
- dist/SHA256SUMS.txt, dist/SOURCE-MANIFEST.txt e dist/RELEASE-NOTES.md.
- REA-Studio-v0.3.0-alpha-Windows-x64.zip: ausente por falta de execução real Windows; não consta do manifesto de hashes.

O pacote Linux exige Qt/glibc/libstdc++ compatíveis com o host informado, não é universal. Staging exclusivo do empacotador usa cmake --install, ldd no binário próprio e --smoke offscreen. Verificação de pacote extraído é independente do diretório de build. Assinaturas, certificado e desktop nativo não são alegados.

## Verificações adicionais realizadas

O tar.gz Linux foi extraído em build/distribution-check e seu executável abriu /usr/bin/true em smoke offscreen fora do build original. readelf não mostrou RPATH/RUNPATH de desenvolvimento; dependências diretas são Qt6Widgets/Gui/Core, libstdc++, libgcc_s e libc. Não houve teste em distribuição/host diferente.

O Source.tar.gz foi comparado byte a byte com SOURCE-MANIFEST.txt (194 arquivos no candidato), sem .git/builds/credenciais. Foi extraído e recompilado em Release com BUILD_TESTING=OFF; o executável reconstruído passou no smoke /usr/bin/true. Isso verifica que o pacote de fontes não depende de arquivos locais omitidos. As verificações de fonte/Linux foram repetidas no candidato final após atualização deste relatório; o relatório não incorpora seu próprio hash para evitar auto-referência.

A fonte QtBase 6.8.3 foi baixada do arquivo oficial, SHA-256 56001b905601bb9023d399f3ba780d7fa940f3e4861e496a7c490331f49e0b80, e a extração de licenças/atribuições foi exercitada em Linux. Isso valida hash e extração documental, não build/deployment Windows. O download inicial bloqueado pelo sandbox foi repetido com autorização de rede. Os arquivos Qt baixados permanecem apenas em build/qt-source-audit e não entram no pacote Linux.

CMakePresets.json foi parseado por cmake --list-presets; YAML do workflow foi parseado localmente. Não houve execução do PowerShell, actionlint ou GitHub Actions; validação estática não comprova execução de CI. SHA de checkout v6, upload-artifact v7 e install-qt-action v4.3.0 foram consultados nas referências upstream e fixados no workflow. O job Windows usa QtBase 6.8.3 com fonte/hash fixados. O pacote não inclui Qt de outros módulos nem CRT DLLs de desenvolvimento.

`git diff --check` da árvore original passou. A checagem completa staged da cópia de revisão **não passou**: 23.622 ocorrências de whitespace/blank line em arquivos upstream de third_party, inclusive tabelas geradas. O log completo está em build/publication-review-whitespace.log. Nenhum arquivo vendorizado foi normalizado: seus 107 hashes são prioritários para preservar a fonte original. A checagem dos arquivos próprios (`git diff --cached --check -- . ':(exclude)third_party/**'`) passou, com log vazio em build/publication-review-own-whitespace.log. Não foi criada supressão global e a falha completa é mantida explícita. Índice original preservado, com checksum conferido antes/depois de montar a revisão. A cópia build/publication-review tem branch prepare/v0.3.0-alpha baseada no commit remoto inicial e fontes staged para revisão, sem novo commit/push. Sua origem aponta ao destino GitHub, mas nenhuma operação de escrita remota ocorreu.

## Bloqueios e aprovação

1. Confirmação e aprovação do licenciamento GPL proposto frente ao MIT inicial remoto.
2. Aprovação explícita de integração/push de código mantendo o histórico remoto; ainda não executados.
3. Execução real de CI Windows, revisão de seus relatórios e fontes/licenças Qt antes de incluir ZIP.
4. Aprovação explícita separada da tag/prerelease/assets finais. O workflow não contém publicação de release.

Consultar docs/release-process.md para a sequência e comandos apenas após aprovação. Pendências técnicas adicionais estão em known-limitations.md e security.md.

## Alteração posterior — 2026-11-04

O recurso Process Memory adicionou dois testes (19 Linux / 14 Windows aplicáveis). Sua validação está em process-memory.md. Os arquivos de dist e a árvore build/publication-review descritos neste relatório registram o candidato anterior e não incorporam automaticamente essa alteração. Antes de qualquer publicação, regenerar fontes/binários/hashes e a revisão a partir do código atual, repetir os gates e obter aprovação. Nenhum push/release foi autorizado ou executado por essa alteração.

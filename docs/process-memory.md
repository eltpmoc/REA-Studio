# Memória de processos — v0.3.0-alpha

Implementação de 2026-11-04. Visualização hexadecimal própria de memória virtual em execução, em Linux e com backend Windows preparado. Referência funcional solicitada: [RAM editor do HxD](https://mh-nexus.de/en/hxd/). Não há código, recursos gráficos ou dependência do HxD nesta implementação.

## Uso

File → Open process memory… abre a aba Process Memory, sem substituir o projeto ELF aberto. Refresh processes enumera processos visíveis na sessão/namespace do sistema. Filtro aceita PID, nome ou caminho. Processos sem permissão/caminho continuam listados quando o SO permite enumerá-los; não é possível listar processos ocultados pelo próprio SO/container.

Duplo clique abre uma sessão identificada por PID e identidade de criação. Regiões mapeadas mostram início/fim exclusivo, permissões r/w/x e nome/tipo. A primeira região legível executável é aberta automaticamente; se não houver, usa outra legível. Duplo clique numa região ou Go com endereço hexadecimal lê seus bytes de runtime. Previous/Next page e Refresh memory operam em até 4096 bytes por pedido, sem ultrapassar o fim da região. Hex/ASCII só mostram bytes realmente retornados. Ctrl+C copia a seleção. Refresh regions consulta novamente o mapa; ele é um snapshot, não uma promessa de que a próxima leitura terá sucesso.

Auto refresh (1 s) relê a página atual enquanto a aba estiver visível e não houver job ativo. O alvo continua executando. Cancel interrompe cooperativamente o pedido; fechar a janela interrompe timers, cancela e faz join. Não há edição de memória, escrita no processo, debugger attach, suspensão, injeção ou elevação automática de privilégios.

Os endereços são VAs de runtime do processo selecionado, separados de offsets de arquivo e VAs ELF usados no restante do workspace. Os bytes vêm do alvo vivo; não são lidos do executável em disco. Isso não é acesso físico a toda a RAM, dump integral nem snapshot atômico: páginas podem ser paginadas, alteradas/desmapeadas ou ficar indisponíveis entre mapa e leitura.

## Backend e identidade

ProcessDescriptor registra PID, identidade, nome/caminho. ProcessMemory conserva uma sessão RAII não copiável, com regiões e leituras limitadas. Identidade indisponível ou alterada exige atualizar a lista. A abertura falha com erro explícito quando o alvo termina ou o SO nega acesso; o alvo anterior fica identificado na view, sem fingir ter aberto o novo.

Linux enumera /proc, obtém identidade pelo starttime em stat e lê mapas. A sessão mantém descritor da pasta /proc/PID e de mem, aberto com O_RDONLY/CLOEXEC via openat. Esses descritores se vinculam ao alvo/mm original, evitando que reutilização numérica de PID redirecione uma leitura para outro processo. pread não muda offset compartilhado. Permissões ptrace/Yama/procfs continuam valendo; mesmo processos do mesmo usuário podem ser recusados. Exec/exit e mudanças de mapping podem invalidar a sessão/página: reabra/atualize após erro. Não há fallback para bypass de permissões.

Windows usa CreateToolhelp32Snapshot/Process32FirstW/NextW; consulta caminho/creation FILETIME quando permitido. Abre handle estável apenas com PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, verifica identidade e usa VirtualQueryEx/ReadProcessMemory. Exibe regiões MEM_COMMIT, respeita PAGE_GUARD/PAGE_NOACCESS e permissões de leitura. Não obtém SeDebugPrivilege. Caminhos Unicode são convertidos para UTF-8. Windows x64 ainda depende de build/testes reais no CI; implementação não equivale a validação.

Documentação das APIs: [proc_pid_mem](https://man7.org/linux/man-pages/man5/proc_pid_mem.5.html), [ReadProcessMemory](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-readprocessmemory) e [VirtualQueryEx](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualqueryex).

## Concorrência e limites

ProcessView possui no máximo um std::jthread e um pedido pendente. Supersessão pede stop e substitui o pendente, com ID monotônico; resultado antigo/cancelado não troca a sessão. Estado só é publicado por done release/acquire e consumido após join; widgets/modelos/timers pertencem à GUI. Handles ficam vivos via shared ownership até terminar o job. Os modelos são tabelas virtuais, sem criar itens Qt por célula para cada mapa.

Limites: 65.536 processos, 32 MiB de metadata contabilizada da lista, 131.072 regiões, 32 MiB de texto de maps Linux, linha de mapa até 16 KiB, metadata curta até 8 KiB e leitura até 4 KiB. Valores excessivos geram erro, não alegação de lista completa truncada. Estes limites são independentes do orçamento estático de instruções/CFG. Chamadas de sistema bloqueadas não têm interrupção forçada nem prazo rígido de join.

## Validação local

GCC Debug e Release **19/19**, Clang Debug **19/19**, ASan/UBSan **19/19**; os 17 testes anteriores não foram editados. TSan de núcleo/controlador **5/5**, incluindo process_memory. Qt/prebuilt não foi instrumentado; pendências TSan de widgets e limitação LSan anterior permanecem sem supressões. Nenhum resultado Windows ou desktop nativo é alegado.

- process_memory: enumeração/ordem/identidades, mapas reais, leitura de buffer próprio e de processo-filho controlado, alteração dos bytes no filho, término do filho, range inválido, destino ilegível, cancelamento prévio e duas sessões concorrentes.
- process_memory_gui: ação/aba, filtro, modelos, duplo clique real no processo de teste, bytes reais, atualização periódica, cópia, erro preservando alvo, supersessão/cancelamento, heartbeat e fechamento/join.
- Captura: docs/workspace-process-memory.png, Qt offscreen, com filtro no próprio processo de teste e uma página alinhada de 4096 bytes conhecidos. Não captura dados privados de aplicações de terceiros.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure
# Mesma configuração anterior de compilador/instrumentação nos caches separados:
cmake -S . -B build-clang -DCMAKE_BUILD_TYPE=Debug
cmake --build build-clang -j"$(nproc)"
ctest --test-dir build-clang --output-on-failure
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug
cmake --build build-asan -j"$(nproc)"
ASAN_OPTIONS=detect_leaks=0 ctest --test-dir build-asan --output-on-failure
cmake -S . -B build-tsan -DCMAKE_BUILD_TYPE=Debug
cmake --build build-tsan --target rea_analysis_tests rea_worker_tests rea_tests rea_program_tests rea_process_tests -j"$(nproc)"
TSAN_OPTIONS=halt_on_error=1 ctest --test-dir build-tsan \
  -R '^(generate_fixture|analysis_control|analysis_worker_controller|program_analysis|process_memory)$' --output-on-failure
```

Logs em build{,-clang,-asan}/{configure,build,test}-process.log e build-tsan/core-process.log; Release em build/{configure,build,test}-process-release.log. Durante implementação o primeiro build do teste GUI exigiu adicionar Q_OBJECT/registro AUTOMOC de ProcessView; corrigido antes de executar os testes finais. Builds finais sem warnings C++ próprios. git diff --check e os 107 hashes de terceiros conferem.

Arquivos novos: include/rea/process_memory.hpp, src/process_memory.cpp, include/rea/process_view.hpp, src/process_view.cpp, tests/process_tests.cpp, tests/process_gui_tests.cpp, este documento e a captura. CMake, Window, workflow e documentação foram atualizados; nenhum arquivo remoto foi publicado. Pacotes de release anteriores precisam ser regenerados antes de publicar o novo código.

## Atualização: escrita de memória — 2026-11-04

A seção anterior registra a primeira implementação de leitura. Agora há escrita explícita:

1. Abra o processo e marque **Enable writing**. A sessão é reaberta com acesso de leitura/escrita, verificando identidade; falha de permissão conserva a sessão anterior e o modo real.
2. Use Go/duplo clique para abrir uma região **r/w** no endereço desejado.
3. **Edit bytes…** mostra PID/endereço, bytes originais e campo hexadecimal. Apply grava exatamente o número de pares informado (até 4096, limitado aos bytes carregados), sem inserir/remover memória. O preview original acompanha esse comprimento. O hex principal continua protegido contra edição acidental; a gravação ocorre pelo diálogo.
4. O worker verifica novamente região atual e bytes esperados, escreve e relê. Status informa quantos bytes foram escritos e se o read-back foi verificado. Alterações concorrentes nos bytes originais recusam a escrita antes de iniciar. Isso **não é compare-and-swap atômico**: o alvo continua rodando e pode alterar mapping/bytes entre checagem, escrita e read-back.
5. **Undo last write** conserva só o último patch completamente verificado da sessão. Ele só restaura se os bytes atuais ainda coincidirem com os bytes escritos; não promete rollback de escrita parcial, de efeitos do alvo ou de patches anteriores. Reabrir/trocar sessão limpa esse registro.

O backend padrão continua read only para preservar APIs/comportamento anterior. Modo de escrita explícito usa /proc/PID/mem O_RDWR ou direitos PROCESS_VM_WRITE | PROCESS_VM_OPERATION e WriteProcessMemory em Windows. Rejeita regiões sem leitura/escrita e ranges que cruzem regiões; não chama mprotect/VirtualProtectEx nem contorna proteção RX/guard. Windows faz FlushInstructionCache em região executável gravada. Linux x86_64 tem a validação local; Windows não foi executado.

Cancelamento é aceito nos checkpoints anteriores à escrita. Depois de emitir um pedido de escrita, a GUI não o substitui/cancela silenciosamente: aguarda e publica o resultado real, inclusive ao fechar/join. A API completa a leitura de verificação mesmo se o stop_token for solicitado após a syscall de escrita. Uma chamada de sistema não é interrompida à força. Escrita parcial ou read-back divergente é erro explícito, sem rollback automático. Em mappings compartilhados, uma alteração pode afetar outros processos ou arquivos por semântica do SO; não há garantia de efeito restrito apenas ao alvo.

Testes ampliados: gravação real no processo-filho, read-back, conflito de bytes, restauração condicional, modo read only, cancelamento prévio e região não gravável. GUI usa Apply de verdade no diálogo sobre página de fixture conhecida, verifica bytes, undo e fechamento. A nova IR adicionou dois testes: suíte atual **21/21 GCC Debug/Release, Clang e ASan/UBSan; 6/6 TSan core**. Logs finais: build{,-clang,-asan}/{configure,build,test}-write-ir.log, build/{configure,build,test}-write-ir-release.log, build-tsan/core-write-ir.log. Um erro inicial de join duplicado ao fechar foi reproduzido pelo teste GUI e corrigido; a rodada final passa. Não foram desativadas as verificações anteriores.

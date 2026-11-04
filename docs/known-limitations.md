# Limitações conhecidas — v0.3.0-alpha

- Estado alpha, sem garantia de completude, estabilidade de ABI ou compatibilidade de formato de projeto. Não há persistência de banco de análise.
- Linux x86_64 validado localmente com Qt offscreen; desktop nativo e Windows real ainda precisam de evidências. Não existe ZIP Windows nesta preparação.
- Parser ELF32/64 little/big endian; disassembly somente x86-64 little endian ET_EXEC/ET_DYN. Sem PE, ARM64 disassembly, decompilação ou pseudocódigo.
- Funções e CFG conservadores; símbolos podem mentir. Não resolve indiretos, jump tables, exceções, relocações, GOT/PLT, DWARF ou convenções de chamada.
- Endereços ELF de link, sem ASLR/load bias inferido. Importados são candidatos por metadados, sem endereço runtime ou biblioteca inferidos.
- Análise em std::jthread no mesmo processo da GUI: sem isolamento de memória, cgroups ou Windows Job Objects ativos. Contratos de worker futuro não são sandbox.
- Limites de memória contabilizam estruturas do núcleo, não RSS. Cancelamento/prazo cooperativos não interrompem syscall bloqueada; stat/open tem risco TOCTOU e não há snapshot de arquivo.
- Publicação de modelos/símbolos/seções é síncrona na GUI; entradas extremas podem pausar a interface. Graph View possui limites próprios (256 blocos, 1024 arestas, 4096 instruções) e layout simples.
- TSan widgets falhou anteriormente em QArrayData/threads do pool de Qt não instrumentado. Núcleo/controlador passaram, mas toda a GUI não está aprovada. Sem supressões para ocultar falhas. Requer Qt instrumentado e nova investigação.
- ASan/UBSan não instrumenta o Qt do sistema. LeakSanitizer está desativado no ambiente local sob ptrace; não se afirma ausência de leaks.
- Pacote Linux usa Qt/glibc/libstdc++ do sistema e não é universal. Não há AppImage, Flatpak, MSI, auto-update, assinatura ou notarização.
- Licença raiz remota MIT inicial difere do código GPL local. Publicação exige aprovação da reconciliação e preservação do commit remoto. Veja COPYING.md.

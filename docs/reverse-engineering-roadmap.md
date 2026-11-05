# Matriz inicial de evolução do REA Studio

Pedido: recursos de engenharia reversa de alto nível tomando Ghidra e IDA Pro como referências. Prioridade confirmada: **decompilação/IR**. A lista abaixo é um inventário inicial do REA, não uma auditoria exaustiva de todas as versões/edições/plugins desses produtos. O baseline de versões/edições foi solicitado ao mantenedor. Não se promete fidelidade completa nem se copia código ou recursos proprietários.

Fontes públicas para organizar as categorias: [Ghidra oficial](https://github.com/NationalSecurityAgency/ghidra) e [documentação oficial Hex-Rays](https://docs.hex-rays.com/). Essas referências não são dependências instaladas do REA Studio. A presença de uma categoria no produto de referência não significa que esteja implementada aqui.

| Capacidade desejada | Estado real do REA | Trabalho faltante / gate |
|---|---|---|
| Disassembly | x86-64 ELF, Zydis, evidência/confiança | Outras arquiteturas/decoders, validação por ISA |
| Loaders | ELF32/64 metadata; código x86-64 ET_EXEC/DYN | PE/Mach-O, relocações, imports reais, validação/fuzzing |
| Funções/xrefs/CFG | Implementação conservadora, navegável e limitada | Jump tables, indiretos, exceções, caudas e análise interprocedural |
| IR | Operand-level tipada + subset GPR lowered | Semântica executável completa, efeitos de memória/flags e IR por blocos |
| SSA/dataflow | Ausente | Dominadores, phi, def-use, aliases e testes formais/diferenciais |
| Decompilação | Notação experimental de máquina, com opacos explícitos | AST, estruturação, variáveis, ABI, tipos e equivalência |
| Tipos/estruturas/protótipos | Ausente | Banco de tipos, parser, propagação, edição e persistência |
| Símbolos/debug info | Símbolos ELF e classificação de indefinidos | DWARF/PDB, versões, mangling, relocações e resolução de bibliotecas |
| Strings/busca/assinaturas | Ausente | Extração limitada, busca bytes/texto, assinaturas e proveniência |
| Navegação/Graph View | Histórico, CFG próprio, PNG/SVG, Dark/Light | Sincronização completa com IR/AST e grafos de chamadas |
| Processos/memória | Lista, mapas, leitura, edição verificada e último undo | Journal persistente, dumps/snapshots, diff e tratamento de mais casos de VM |
| Debugger | Ausente | Sessão por SO, stop reasons, threads, registros, breakpoints, stepping e lifecycle |
| Tracing/replay | Ausente | Coleta, armazenamento, sincronização, determinismo e limites |
| Emulação/symbolic execution | Ausente | Semântica executável, engine, estados/faults e validação |
| Anotações/rename/projetos | Sem banco persistente de análise | Modelo de projeto transacional/versionado e migrações |
| Scripting/plugins/headless | Interfaces internas; sem SDK público ou CLI de análise completa | API estável, isolamento de extensões, testes e documentação |
| Patching em disco/assembler | Ausente | Montagem, validação de tamanho/relocações, cópia segura e histórico |
| Colaboração | Ausente | Identidade, conflitos, protocolos e revisão de segurança |
| Isolamento do analisador | In-process com limites cooperativos | Processo separado real, IPC, cgroups/Job Objects implementados |
| Plataformas | Linux x86_64 testado; Windows preparado | Evidências Windows reais, desktop e outros hosts |

## Critério de conclusão de cada frente

Código funcional, testes positivos/negativos, limites/cancelamento, integração navegável, documentação de limitações e evidência por plataforma. Um contrato abstrato, um botão, um renderer ou um helper de pseudocódigo não contam como decompilador/debugger/arquitetura implementados. Comparações de paridade exigem versão, edição e corpus de teste definidos, incluindo casos sem símbolos e malformados.

Não há uma release com todos os recursos de Ghidra/IDA nesta entrega. Ela implementa escrita de memória e inicia a frente IR escolhida; os itens ausentes continuam pendências explícitas. Nenhuma publicação remota foi executada, e os candidatos de pacote anteriores precisam ser regenerados antes de release.

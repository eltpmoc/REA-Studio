# IR e pseudocódigo experimental — 2026-11-04

Prioridade escolhida pelo mantenedor: decompilação e representação intermediária. Esta entrega implementa uma primeira camada verificável; **não é um decompilador completo, SSA, recuperador de tipos ou equivalente ao Ghidra/Hex-Rays**.

## Implementação atual

X86IRLifter redecodifica os bytes de Instruction usando Zydis LONG_64. Verifica tamanho/faixa, extrai opcode, operandos visíveis/implícitos, acesso, largura, registradores, imediatos com sinal/relativos, memória (segmento/base/índice/escala/deslocamento), efeitos sobre flags e destinos relativos com checagem de overflow. Não infere semântica a partir de strings de Assembly. Instruções inválidas ficam opacas, sem destino de controle inventado.

IRInstruction conserva VA e bytes originais; IRFunction mantém sequência ordenada, listing e pseudocódigo derivado. O worker gera IR por função a partir dos membros de CFG, com checkpoints e reservas no mesmo AnalysisControl. A etapa não descobre novos endereços nem consome novas unidades da cota de descoberta de instruções; seu trabalho/memória/prazo são contabilizados. Resultado segue o handoff existente após join. Se apenas a etapa IR exceder orçamento, seus resultados ausentes são sinalizados e funções/CFG/referências já validados são preservados, com irStopReason e diagnóstico. IRs de funções já concluídas podem ser mantidas; a função em construção não é publicada parcialmente. Cancelamento continua descartando a publicação do pedido.

Subconjunto lowered: MOV, ADD, SUB, XOR, AND, OR, CMP e TEST com GPR de 32/64 bits e fonte GPR da mesma classe ou imediato. Escritas de r32 explicitam zero-extension para r64 pelo contrato write_gpr; truncN faz wrap ao tamanho do destino. Operações de flags aparecem como helpers distintos; CMP/TEST não geram escrita de destino. Imediatos negativos mantêm a extensão de sinal.

Memória, SSE/AVX, registradores parciais 8/16 bits, stack, chamadas, retornos e branches condicionais não têm lowering semântico nesta versão. Conservam operandos/metadados/bytes e viram opaque_x86_step no pseudocódigo. Jump direto pode atualizar PC explicitamente. Um dispatcher por PC evita inventar fallthrough para retorno, branch ou chamada opacos. Quando PC sai dos membros recuperados, o dispatcher sai da representação dessa função; isso não equivale a inferir um retorno ABI do binário.

## O que o pseudocódigo significa

A saída é **notação de máquina em estilo C**, com MachineState, read_gpr, write_gpr, truncN, flags_* e opaque_x86_step como contratos de notação. Não é C/C++ autossuficiente/compilável, não contém implementações executáveis desses helpers e não é um emulador. Flags_* marca o efeito x86 exigido e suas flags indefinidas; não afirma que exista execução de um modelo completo de flags. Opaque não é NOP: seus efeitos e PC permanecem desconhecidos e não podem ser ignorados por futuras otimizações.

Não se recuperam parâmetros, protótipos, ABI, variáveis locais, estruturas, ponteiros tipados, loops estruturados ou expressões de alto nível. Nenhum desses resultados é inventado para fazer a saída parecer C original. A representação atual é um ponto de inspeção/rastreabilidade para construir um decompilador, não uma alegação de equivalência semântica global.

## Interface

Selecionar uma instrução no Assembly escolhe sua função nas abas IR (experimental) e Pseudocode (experimental). Texto simples, cópia por Ctrl+C e origem no endereço/bytes. Vistas limitadas a funções de até 2048 instruções para publicação na GUI; o limite é explícito, sem truncar silenciosamente o cabeçalho. Função ausente ou etapa indisponível tem mensagem própria. O ELF de disco e a memória de runtime continuam separados; editar RAM não atualiza automaticamente a análise estática ou esta IR.

## Validação

intermediate_representation usa bytes x86 reais para MOV/ADD/memória/branch/RET, verifica larguras, imediato negativo, metadados, subset lowered versus opaco, dispatcher, determinismo e cancelamento. ir_workspace abre a fixture ELF pelo worker, verifica as vistas e copia pseudocódigo independentemente do Assembly. Captura real offscreen: workspace-pseudocode.png. Não houve execução Windows/desktop nativo nem prova de equivalência por emulação/diferential testing.

## Próximas etapas da prioridade escolhida

1. Especificar semântica bit-vector executável/testável por opcode, aliases de registradores e flags definidas/indefinidas; differential tests contra execução controlada. Um helper de notação só vira semântica implementada após esse gate.
2. Blocos IR com sucessores explícitos, dominadores, def-use, SSA e phi com tratamento de código irreducível. Testes de loops, joins, conflitos e cancelamento, sem excluir regiões desconhecidas.
3. Modelo de memória e aliases, stack/frame, ABI e chamadas; efeitos opacos conservadores para instruções não suportadas e destinos indiretos.
4. Propagação de constantes, simplificação bit-vector e eliminação de código morto com provas/testes; flags e faults observáveis não podem ser apagados indevidamente.
5. Recuperação de variáveis, tipos e protótipos com confiança explícita; integrar DWARF/PDB apenas com parsers e evidências correspondentes.
6. AST estruturada, if/loops/switch, emissão legível e navegação bidirecional pseudocódigo↔IR↔Assembly. Testar equivalência e limitações antes de chamar de decompilador completo.

Matriz inicial de capacidades e demais subsistemas: reverse-engineering-roadmap.md. As versões/edições exatas dos produtos de referência ainda precisam ser definidas para uma comparação exaustiva.

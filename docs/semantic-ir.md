# Semantic IR — v0.3.1

## Auditoria e arquitetura

O IR anterior decodifica operandos Zydis, tamanhos, ações e flags, mas só faz lowering MachineState de operações GPR32/64 reg/reg ou imediato. Memória, push/pop/ret eram opacos. Esse contrato e os testes anteriores foram mantidos. O novo SemanticResult em semantic_ir.hpp é uma camada independente, construída no worker depois do IR tipado; cLike é publicado junto ao resultado imutável.

SemanticValue é uma definição imutável de bitvector, com largura, operação e IDs de operandos anteriores. SemanticResult.uses contém as cadeias def-use. O mapa de registradores contém a definição corrente; cópias propagam IDs, constantes são avaliadas modularmente, extrações/concatenações adjacentes são simplificadas. A impressão percorre somente a expressão de retorno. É SSA local para uma sequência contígua: **não há phi, dominadores ou SSA entre blocos**. Fluxo não linear é recusado, não linearizado artificialmente.

## Semântica suportada e fronteiras

- MOV GPR reg/reg, imediato e memória local comprovada da pilha.
- ADD/SUB/XOR/AND/OR em bitvectors; CMP/TEST só são eliminados na projeção sem consumidores de flags. Branches, ADC, chamadas e consumidores não suportados recusam a recuperação inteira.
- GPRs 8/16/32/64, inclusive AH/BH/CH/DH: escrita parcial preserva os outros bits; escrita r32 zera os 32 bits superiores.
- Memória little endian em células de byte SSA: leitura, escrita, sobreposição e remontagem; leitura não inicializada ou alias não comprovado recusa a recuperação.
- PUSH de 64 bits avalia a origem antes de decrementar RSP e armazenar. POP para GPR64 lê antes de incrementar RSP; POP RSP/memória ainda é recusado. RET simples exige RSP balanceado, retorno original intocado e registradores callee-saved restaurados. RET imediato não suportado.
- LEA somente para endereço comprovado de pilha, destino64 e sem índice. LEA aritmético geral permanece pendente.
- Ajustes imediatos positivos limitados de RSP e cópias RSP/RBP são rastreados separadamente. Stores só são aceitos abaixo da posição de retorno, dentro de espaço local/red zone compatível com o RSP corrente. Slots podem conter argumentos ou temporários, sem inventar variáveis-fonte.

Flags definidos, lidos e indefinidos são preservados no IR original; flagsCleared/flagsSet distinguem constantes impostas. Flags não escritos/indefinidos são preservados pela instrução. A camada C-like não é um executor completo de flags. Ela só descarta flags na projeção de retorno inteiro System V, quando não há consumidor aceito. Não existe alegação de equivalência de MachineState completo.

A projeção considera execução normal, pilha válida e privada, sem observadores assíncronos. Faults, CET/shadow stack, unwind, sinais e instrumentação não estão modelados; ENDBR64/NOP são neutros somente nessa projeção. Prefixos LOCK/REP e endereçamento diferente de64 são recusados. A semântica detalhada original permanece disponível para qualquer recusa.

## ABI e tipos

Hipótese System V AMD64, confiança probable: entradas baixas de32 de RDI/RSI/RDX/RCX/R8/R9 e retorno EAX. Bits superiores de argumentos permanecem desconhecidos; uso não simplificável recusa o retorno. Não inferimos assinatura verdadeira apenas pelo uso de registradores. Argumentos genéricos até a maior posição usada mantêm as posições ABI. Inteiros usam uint32_t modular: signedness, tipo de retorno original e overflow C++ não são dedutíveis do ADD. Sem DWARF de tipos, a saída não afirma int32_t.

## Limites

Máximo8192 nós, profundidade64 e expressão32 KiB; checkpoints, trabalho e reserva de memória usam AnalysisControl. Sem isolamento de processo/RSS. O avaliador de testes opera sobre resultados construídos pelo motor; não é emulador nem API para grafos arbitrários não confiáveis. SSA global, memória externa, ponteiros genéricos, extensões MOVZX/MOVSX, ABI de64 bits, estruturas de controle e recuperação de tipos permanecem pendentes.

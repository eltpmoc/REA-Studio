# Referências cruzadas — v0.3.0

O contrato legado Reference mantém `from`, `to`, `flow`; o novo campo ReferenceType é adicional. O vetor legado no AnalysisResult permanece compatível. `ReferenceIndex` constrói seu próprio índice tipado a partir das instruções e não confia no campo type dos registros antigos.

| Tipo | Evidência |
|---|---|
| DirectCall | CALL com operando relativo resolvido pelo decoder |
| DirectJump | Branch condicional/incondicional com destino direto |
| CodeAddress | Operando RIP-relative cujo endereço coincide com instrução válida já decodificada |
| DataAddress | Outro endereço de operando RIP-relative |

CodeAddress comprova apenas a coincidência de endereço do operando com código conhecido. Não comprova que a instrução o executará. Em especial, `call [rip+disp]` referencia a posição de memória, não o ponteiro armazenado nem o destino da chamada. Não há resolução de indiretos, imports/GOT/PLT ou relocações. Invalid não produz referências. Destinos diretos não mapeados continuam no índice como evidência numérica, sem inventar código.

Ordenação por (from, to, type), deduplicação e índices incoming/outgoing por VA. Índices guardam posições no vetor interno; só devem ser usados enquanto o índice não for reconstruído. Objetos publicados pertencem à GUI e não são modificados pelo worker após join.

`ReferenceModel` apresenta origem, destino e tipo. A aba Cross References filtra referências com origem ou destino em uma instrução da função selecionada. Para instruções sem função identificada mostra o índice completo. Duplo clique na coluna From navega à origem; nas demais colunas ao destino. A mesma API de navegação valida VA/mapeamento/boundaries, pode solicitar decoding assíncrono de uma VA executável não analisada e registra o histórico Alt+Left/Right. Destino inválido é recusado e registrado no log.

Referências mostradas em Instruction Information e contagem histórica do Assembly continuam disponíveis. O projeto ainda não oferece banco incremental ou referências de dados derivadas de propagação de valores.

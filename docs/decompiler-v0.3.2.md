# v0.3.2 — desenvolvimento em andamento

Primeira etapa: ControlFlowFacts no núcleo, independente de Qt, consome o CFG existente validado. Mapas determinísticos registram predecessores, sucessores, alcançabilidade e dominadores. Dominadores são calculados por ponto fixo da interseção dos predecessores alcançáveis, mantendo a entrada como raiz.

Uma aresta n→h cujo destino domina a origem identifica um backedge natural. O corpo é obtido caminhando predecessores até o cabeçalho; múltiplos latches são agrupados. Isso não afirma que todo ciclo seja um while ou que o laço termine. Ciclos irreducíveis também são detectados pela ordenação de Kahn para impedir cálculo inadequado de pós-dominância.

Pós-dominadores são calculados apenas em grafos alcançáveis fechados e acíclicos. Cada saída inicia seu próprio conjunto; a interseção dos sucessores evita inventar um bloco de junção comum a retornos distintos. Grafos cíclicos ou com saídas desconhecidas não recebem prova de pós-dominância.

Os fatos aparecem na IR detalhada; não alteram o C-like existente. Há checkpoints e limites de nós/arestas/memória/trabalho. Conjuntos de dominadores têm custo quadrático; a reserva conservadora pode recusar grafos grandes.

Ainda faltam recuperação if/else/while/do, junções SSA/phi, multiplicação/divisão/resto no C-like, locais entre blocos, exportação e destaque de sintaxe. somar mantém a recuperação anterior; fatorial, eh_primo e classificar não foram declaradas recuperadas nesta etapa. O escopo completo da v0.3.2 não está concluído.

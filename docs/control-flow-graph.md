# Control Flow Graph — v0.3.0

`CFGBuilder` produz um CFG intraprocedural por FunctionDescriptor, usando apenas instruções membros previamente validadas. `BasicBlock` preserva o contrato antigo (address, instructions, successors); `ControlFlowGraph` acrescenta entry, blocos, arestas tipadas e indicador cyclic. O antigo CfgBackend continua disponível como interface de compatibilidade; não é o backend novo.

Entradas, destinos de branches internos, instruções após transferências e descontinuidades iniciam blocos. Calls podem encerrar um bloco e têm aresta de continuação; o destino do call pertence ao índice de referências, não ao CFG intraprocedural. Retornos e paradas possuem aresta terminal sem destino. Saltos condicionais têm destino e fallthrough; saltos incondicionais não têm fallthrough. Destino ausente ou fora dos membros da função gera Unknown, conservando a VA numérica quando conhecida. Não cria nó falso para destino indireto ou externo.

Blocos/instruções seguem ordem crescente de VA. Arestas seguem ordem de bloco, branch antes de fallthrough. Kahn detecta ciclos sem recursão; loops e self-loops não causam percurso infinito. `CFGValidator` verifica entradas, ordenação, duplicatas, instruções contíguas, overflow/tamanho, terminadores internos e endpoints das arestas conhecidas. É validação estrutural, não prova da semântica do binário ou de completude do grafo.

`GraphBudget` é compartilhado entre todos os CFGs de um pedido. `AnalysisLimits.maxCFGNodes/maxCFGEdges` têm padrões 4096/16384; limites são cobrados antes de inserir. Pontos de controle existem na indexação, seleção de líderes, cópia de instruções, arestas e algoritmo de ciclos. Falhas produzem AnalysisStopped tipado. Memória é contabilização conservadora, não RSS. Análises interrompidas não publicam ProgramAnalysis parcialmente construído.

## Graph View

Visualizador próprio QGraphicsView/QGraphicsScene. Layout vertical determinístico por VA, rotas ortogonais e pontas direcionadas; branch condicional em amarelo e outras arestas internas em verde. Unknown/external e return/stop são rótulos terminais. Não há recursos gráficos de disassemblers proprietários.

Roda do mouse controla zoom de 0,1× a 4×. Arrastar o fundo faz pan. Clique seleciona bloco; duplo clique navega para sua primeira instrução no Assembly. O bloco da instrução ativa recebe contorno amarelo. A função exibida acompanha a seleção do Assembly. O layout inicial não tenta minimizar cruzamentos e não é um layout hierárquico sofisticado.

A view recusa grafos malformados ou com mais de 256 blocos, 1024 arestas ou 4096 instruções. O limite visual é distinto do limite de análise. File → Export graph PNG/SVG exporta a cena da função atualmente exibida, independentemente da aba ativa. PNG recusa dimensões acima de 8192 ou mais de 32 milhões de pixels; exportação SVG usa QXmlStreamWriter para texto escapado e primitivas próprias, sem QtSvg. PNG e SVG retornam falha se o arquivo não puder ser escrito. SVG preserva topologia/texto/cores, com métricas tipográficas aproximadas; não é uma serialização do projeto.

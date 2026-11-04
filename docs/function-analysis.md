# Análise de funções — v0.3.0

`FunctionAnalyzer` recebe Image, tamanho do arquivo e AnalysisResult imutável. Não decodifica bytes adicionais. A descoberta usa STT_FUNC definidos com seção válida, entry point e destinos de calls diretos. Saltos não são sementes de função. Endereços precisam de PT_LOAD file-backed executável sem ambiguidades no AddressTranslator.

`FunctionRegistry` mantém descritores ordenados por entry. `at` procura uma entrada exata; `containing` procura uma instrução membro, sem tratar lacunas no envelope como código. `FunctionModel` oferece nome, VA, confiança, evidência e sobreposição à tabela Functions. Duplo clique usa a navegação e o histórico existentes.

| Confiança | Evidência |
|---|---|
| confirmed | Entry ou STT_FUNC definido, com instrução válida disponível |
| probable | Destino de call direto com instrução válida disponível |
| unknown | Semente em região executável, mas sem instrução válida disponível |

Confirmação é baseada em metadados, não em execução observada. Símbolos podem estar incorretos. Calls indiretos não produzem sementes inventadas. Destinos diretos fora de mapeamentos executáveis continuam nas referências; não viram funções.

Para cada semente o percurso segue instruções decodificadas: fallthrough de Next/Call/ConditionalJump, destino de Jump/ConditionalJump; Return/Stop/Invalid encerram o caminho. Não segue o destino de Call dentro da função chamadora. Outra entrada conhecida encerra o caminho; tamanho ELF não nulo limita a função. Retornos observados geram `hasReturn`. Não são inferidas convenções de chamada, epílogos, funções non-returning ou tail calls.

`instructions` é a lista ordenada de VAs membros. `end` é o maior fim de instrução observada; `declaredEnd` conserva o fim declarado por símbolo quando representável. Ambos são exclusivos. Membros podem ser descontínuos e compartilhar caudas. Instruções compartilhadas, envelopes e regiões declaradas que se intersectam marcam `overlap`; isso sinaliza ambiguidade, não prova que as funções estejam erradas. Entradas repetidas são consolidadas em um descritor com contador `duplicateEntries`; a evidência mais forte prevalece. Aliases não são todos exibidos nesse modelo; Symbols conserva a tabela original.

Visited impede ciclos. Trabalho, reservas conservadoras e prazo/cancelamento são compartilhados com parser/decoder/CFG. A construção pode consumir trabalho proporcional a funções × instruções; o limite de trabalho interrompe casos excessivos. Não há recuperação completa de funções stripped, DWARF, jump tables, exceções ou análise incremental. Se a etapa de programa for interrompida, seu resultado não é publicado parcialmente; o Assembly validado continua disponível com diagnóstico de interrupção.

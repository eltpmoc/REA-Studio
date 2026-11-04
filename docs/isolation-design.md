# Backend e isolamento futuro — v0.3.0

A implementação atual é **in-process**. std::jthread separa execução, não memória ou privilégios. Um defeito nativo no parser/decoder ainda pode comprometer o processo da GUI. Não existe executável rea-analysis-worker, transporte IPC, sandbox, cgroup ativo ou Job Object ativo nesta versão.

## Contrato executável atual

`IAnalysisBackend::analyze` recebe BinaryFile/Image exclusivos do pedido, AnalysisControl, stop_token e VA opcional. `InProcessAnalysisBackend` executa disassembly, funções, referências e CFG. `std::stop_callback` liga o token ao cancelamento atômico. O controlador também liga o token durante parsing; cancel, supersessão e destruição solicitam parada e fazem join. Nenhum worker toca modelos ou widgets. Results só passam para GUI após release/acquire e join.

O resultado BackendResult contém Assembly e ProgramAnalysis. Se o limite impedir a etapa de programa, Assembly permanece com stopReason/limited/diagnóstico; a etapa nova é descartada integralmente. Cancelamento descarta toda publicação na Window. Não há garantia de término de syscall bloqueada nem de join com latência máxima.

## Protocolo lógico v1

`rea::protocol` implementa Header, Payload/Message, validação de versão/tag/tamanho declarado, mensagens tipadas e máquina de estados. São contratos C++ testados; **não há codec ou transporte binário implementado**, e estruturas C++ nunca devem ser enviadas com memcpy. A validação atual não valida profundamente um resultado remoto: o futuro receptor deverá fazê-lo antes de publicar.

Header lógico: protocolVersion u32 (=1), kind enum (Hello, Analyze, Progress, Result, Cancel, Error), requestId u64 não zero, payloadBytes u64 até 16 MiB. IDs correlacionam mensagens e resultados; o receptor deverá rejeitar IDs antigos/desconhecidos. A GUI já aplica IDs monotônicos no controlador local.

| Mensagem | Payload tipado |
|---|---|
| Hello | supportedVersion |
| Analyze | path (1–4096 bytes, sem NUL), AnalysisLimits, requested VA opcional |
| Progress | AnalysisProgress (fase, percentual, instruções, bytes, tempo) |
| Result | AnalysisResult, ProgramAnalysis e StopReason |
| Cancel | Sem conteúdo adicional; ID no header |
| Error | Descrição até 4096 bytes e StopReason |

Idle → Analyze → Running. Running aceita Progress, Cancel → Cancelling, Result → Completed e Error → Failed. Cancelling aceita Result → Cancelled ou Error → Failed. Hello só é aceito em Idle. Estados terminais aceitam Analyze para novo pedido. Outras transições são recusadas por optional vazio. O controlador local continua usando seu lifecycle existente; não simula tráfego IPC.

O futuro codec deverá fixar endian, framing e representação de enums, limitar antes de alocar, conferir comprimento real contra payloadBytes e rejeitar enums/overflow/contagens/string inválidos. Resultado acima de 16 MiB deverá falhar ou ganhar protocolo paginado em nova versão; não se permite ignorar esse teto. Dados remotos precisam de validação de mapeamentos, instruções, funções, CFG, referências, IDs e limites acumulados. Progresso deve continuar como mailbox limitada. Timeout de cancelamento deverá matar/recolher o processo filho e invalidar seu resultado, preservando o projeto anterior. Crash/EOF/versão incompatível precisam de erro explícito. Essas são exigências de implementação futura, não garantias presentes.

## Interfaces específicas do sistema

`LinuxCgroupV2Policy::attachWorker(pid, ProcessLimits)` e `WindowsJobObjectPolicy::assignWorker(handle, ProcessLimits)` são interfaces abstratas. EnforcementResult exige sucesso real ou erro; não há implementação de sucesso fictício.

Linux: futura implementação deve verificar hierarquia/delegação cgroups v2, criar grupo exclusivo, aplicar memory.max e pids.max, anexar PID, ler de volta e tratar memory.events. CPU deverá distinguir quota cpu.max de tempo total consumido (monitoramento cpu.stat). Falta de permissão/controlador é erro explícito; não autoriza afirmar isolamento. O processo deve entrar nos limites antes de analisar dados não confiáveis. cgroup por si só não restringe acesso ao filesystem/rede; desenho de sandbox requer política adicional.

Windows: criar Job Object, configurar limites de memória/CPU/processos e KILL_ON_JOB_CLOSE, criar worker suspenso, atribuir e só então retomar. Tratar restrições de jobs aninhados e falhas de atribuição. Handles precisam de propriedade RAII e herança restrita. Nenhuma implementação Windows foi validada aqui.

Políticas do SO não substituem validação de mensagens, limites cooperativos nem autenticação/correlação do canal. A futura versão deverá ter testes de crash, payload truncado/excessivo, processo que ignora Cancel, esgotamento de memória/CPU e fechamento da GUI; nenhum desses testes de isolamento real é alegado nesta versão.

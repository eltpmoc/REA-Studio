# Segurança

REA Studio é um analisador estático e não executa os binários abertos nem comandos recebidos deles. Isso reduz superfície, mas não torna seguro analisar qualquer entrada no processo da GUI: parser, Zydis, Qt e bibliotecas nativas podem ter vulnerabilidades. Use uma conta sem privilégios e um ambiente descartável apropriado ao nível de confiança da amostra.

A v0.3.0-alpha tem limites de contagens, ranges, overflow, trabalho, prazo e reservas de memória, além de cancelamento cooperativo. Não fornece sandbox, processo separado, cap de RSS ou limite rígido de latência. Os detalhes e riscos de I/O estão em known-limitations.md e isolation-design.md. Não abra dispositivos/FIFOs; o programa recusa entradas não regulares antes de abrir, sem garantir snapshot/ausência de TOCTOU.

## Repositório e distribuição

Não versione credenciais, .env, .aws/.codex/.agents locais, chaves, dumps, caches, builds ou pacotes binários. O .gitignore não protege arquivos já no índice. scripts/release.py audit verifica também o índice, 107 hashes vendorizados, licenças e um inventário permitido de fontes. A busca por padrões de credenciais é uma verificação limitada, não prova de ausência de segredo; revise manualmente a diff e o inventário antes da aprovação.

Pacotes de fontes são construídos por allowlist, não por cópia de todo o workspace. Imagens PNG reais em docs são arquivos públicos intencionais; builds, clones de auditoria e credenciais não entram. Arquivos de release têm SHA-256; não são assinados. Nunca use ferramentas de dependências que possam executar a amostra (como ldd) em ELFs não confiáveis. Os scripts só chamam ldd no executável recém-compilado do próprio projeto.

Workflow com contents:read, checkout sem persistência de credenciais, actions fixadas por SHA e artefatos retidos por 30 dias. Não há passo de push, publicação de release ou uso de segredo de escrita. Falhas de build/testes/deployment encerram o job; não há continue-on-error. Artefatos de Actions são resultados temporários de CI, não uma release aprovada.

## Relato de vulnerabilidades

Não inclua amostras proprietárias, malware executável, credenciais ou dados pessoais em issues públicas. Use private vulnerability reporting do GitHub se o mantenedor o tiver habilitado; a preparação não altera configurações remotas nem afirma que esse canal já esteja ativo. Combine um canal privado com o mantenedor antes de transmitir detalhes sensíveis. Relatos devem identificar versão/commit, SO/toolchain, entrada mínima segura e comportamento observado. Sem SLA ou programa de recompensa anunciado.

## Leitura de memória em runtime

A aba Process Memory pode ler dados sensíveis do alvo quando o SO permite. A sessão inicial é somente leitura. Escrita exige Enable writing e Apply; não injeta código, suspende processos, obtém SeDebugPrivilege ou altera ptrace/Yama. Compara bytes/region e verifica read-back, sem garantia atômica ou rollback de escrita parcial. Mappings compartilhados podem propagar efeitos a outros processos/arquivos. Acesso negado permanece erro. Identidades e handles/descritores ancorados evitam redirecionar a sessão silenciosamente para outro PID reutilizado. A sessão não é um snapshot: o alvo pode mudar/desmapear páginas ou terminar entre operações. Não compartilhe capturas/dumps de memória de aplicações reais sem revisar seu conteúdo. A captura automatizada deste recurso usa apenas uma página conhecida de fixture preenchida com bytes de teste.

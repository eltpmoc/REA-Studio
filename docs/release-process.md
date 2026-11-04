# Processo de publicação — exige aprovação

Esta preparação é local. Não executa push, não cria tags remotas/releases, não dispara workflows no remoto e não substitui arquivos remotos. Artefatos em dist são candidatos, não releases publicadas.

## Auditoria e gates

1. Revisar docs/publication-readiness.md, código, capturas e inventário de fontes; executar scripts/release.py audit e git diff --check. Conferir fontes do pacote contra SOURCE-MANIFEST.txt. A diff staged completa da cópia baseada no remoto inclui whitespace original de terceiros; registrar essa falha separadamente e conferir os 107 hashes. Não normalizar os arquivos upstream nem ocultar a falha com configuração global. Arquivos próprios devem passar na checagem específica documentada em publication-readiness.md.
2. Resolver explicitamente o licenciamento. O remoto main inicial 5de48bf0877878a11aeeef40e2c97b17e0366493 contém só LICENSE MIT; código local pré-existente é GPL-3.0-or-later. O mantenedor precisa aprovar o LICENSE GPL proposto e confirmar os direitos. O texto canônico GPL permanece intacto, com termos e situação histórica em COPYING.md.
3. Reconsultar remoto antes de integrar. Se main mudou, ler os novos arquivos e integrar sem sobrescrever trabalho alheio. Preservar o commit inicial: nunca force-push, apagar/recriar repositório ou substituir main por uma história independente.
4. Obter aprovação explícita para publicar código-fonte. Não interpretar aprovação de build/testes como autorização de push/release.
5. Após o push aprovado, exigir jobs Linux GCC/Clang, Windows e sanitizers aprovados no SHA correspondente. Inspecionar relatórios JUnit, logs e deployment. Windows precisa de MSVC x64, seus 12 testes e smoke do pacote (offscreen e qwindows), não apenas cross-compile Linux.
6. Baixar artefatos de CI e verificar hashes. Reavaliar licenças/fontes Qt, dependências, ambiente mínimo e disponibilidade dos arquivos de fonte correspondentes. Publicar QtBase source dentro do ZIP Windows e source REA em todos os binários; não remover avisos.
7. Atualizar release notes apenas com resultados reais. Se Windows falhar, não incluir ZIP nem afirmar suporte validado. Se qualquer código mudar, refazer os testes/pacotes aplicáveis.
8. Obter aprovação explícita da release/tag/arquivos finais. Só então executar os comandos de publicação manual. O workflow desta entrega nunca publica releases.

## Integração preservando histórico

O workspace original está em main sem commits, com índice pré-existente que deve ser preservado. Não faça reset ou checkout sobre ele. A estratégia preparada é uma cópia de revisão em build/publication-review, baseada no commit remoto auditado, com fontes candidatas e diff local; nenhuma alteração é enviada ao GitHub. Confira seu status e diff antes de criar o primeiro commit de código.

Depois da aprovação da reconciliação/licença e do código, em uma árvore baseada no remoto:

```sh
# SOMENTE após aprovação explícita; comandos abaixo não foram executados.
git status --short
git diff --cached --stat
git diff --cached -- LICENSE COPYING.md
git commit -m "Prepare REA Studio v0.3.0-alpha"
git fetch origin main
git log --oneline --graph --all
# Se remoto mudou, examine seu conteúdo e faça merge normal, resolvendo conflitos.
# Se não mudou, o commit candidato já descende do main remoto.
git push origin HEAD:main
```

Não use --force ou --allow-unrelated-histories para contornar a divergência. A cópia de revisão já preserva a raiz remota. Refaça o candidato caso haja mudanças nas fontes após sua montagem.

## Build e artefatos locais

```sh
python3 scripts/release.py audit
cmake --preset linux-release
cmake --build --preset linux-release -j"$(nproc)"
python3 scripts/release.py linux --build build-release --output dist
(cd dist && sha256sum -c SHA256SUMS.txt)
```

Windows: docs/build-windows.md. Só o empacotador executado em Windows cria REA-Studio-v0.3.0-alpha-Windows-x64.zip. O script se recusa a substituir um ZIP já existente. Limpe/mova resultados antigos conscientemente antes de repetir a geração. Manifestos locais listam apenas artefatos existentes; ao reunir Linux e Windows, use scripts/release.py checksums --output dist para manifestar ambos, depois verifique cada arquivo.

## Publicação manual final

Após resultados CI reais e nova aprovação explícita para tag/release, preparar tag anotada v0.3.0-alpha apontando ao SHA testado, revisar release notes e lista exata de assets (Linux, Windows se validado, Source, SHA256SUMS e notas). Somente então enviar tag e criar uma GitHub prerelease. Não há comandos automáticos de release neste repositório. Não publique uma tag para commit diferente dos pacotes testados.

Antes de publicar, configure proteção de main, checks obrigatórios e canal de segurança no GitHub se desejado; são alterações remotas que dependem de autorização separada. Os SHA-256 não são assinaturas. Mantenha fontes/avisos disponíveis pelo prazo e condições das licenças aplicáveis.

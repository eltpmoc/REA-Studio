# Auditoria inicial v0.3.2-alpha

Raiz de desenvolvimento: REA-Studio; nenhum desenvolvimento em build/publication-review.

A raiz local estava em main sem commits, com índice de primeira importação e arquivos adicionais não rastreados. git log local não possuía HEAD. origin estava ausente; foi configurado para https://github.com/eltpmoc/REA-Studio.git e fetch executado com sucesso, sem checkout/reset dos arquivos. Backup do índice: build/audit-v032/original-index; hashes do índice e fixture em preserved.sha256.

O histórico remoto contém main e release/v0.3.0-alpha. A branch anterior contém o merge 9fd8269 que preserva o histórico GitHub e adota GPL. Nenhum histórico foi reescrito. A comparação simples git diff remoto não inclui arquivos locais untracked: esses arquivos não devem ser interpretados como exclusões intencionais.

GitHub autenticado via keyring fora do sandbox. A falha inicial de autenticação/rede no sandbox não indicava credencial realmente inválida. PR #1 da v0.3.0-alpha permanece aberto. Nenhuma release foi listada. Run 41767825769 falhou: Linux GCC/Clang em CTest, Windows em Configure, ASan/UBSan em testes; TSan passou. Esses resultados são anteriores à v0.3.2 e não validam esta árvore.

Publicação da v0.3.2 autorizada, mas ainda não realizada. Não substituir tags, não force push, não contornar proteções. Recuperação estruturada e validação Windows continuam gates pendentes.

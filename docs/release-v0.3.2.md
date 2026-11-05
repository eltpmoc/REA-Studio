# Estado de publicação v0.3.2-alpha

O desenvolvimento está publicado na branch `develop/v0.3.2-alpha`, pelo PR [#2](https://github.com/eltpmoc/REA-Studio/pull/2), baseado em `release/v0.3.0-alpha`. A recuperação estruturada completa continua em desenvolvimento; uma pré-release desta árvore representa um snapshot experimental, não a conclusão do decompilador.

Autorização de publicação recebida. O histórico e os PRs #1/#2 foram preservados. Em 2026-11-05, Windows x64 foi compilado com MSVC 19.44 e Qt 6.8.3: 21/21 testes, teste visual nativo e smoke do pacote com PATH restrito aprovados. ZIP, fontes correspondentes, licenças Qt e hashes ficam em `dist/windows-x64`. Veja [build Windows](build-windows.md).

O CI anterior reproduziu duas falhas de infraestrutura dos testes. A fixture ELF herdava a instrumentação ASan e incorporava seu runtime, ultrapassando o escopo das quatro pequenas funções que deveriam ser analisadas. Ela agora é compilada sem sanitizers; o analisador e os executáveis de teste continuam instrumentados. No TSan, o Qt 6.4 chamava `clone(CLONE_VM)` ao iniciar o filho e abortava dentro do runtime TSan; esse job usa Qt 6.8.3, cujo [QProcess detecta os interceptadores de sanitizers e usa fork](https://github.com/qt/qtbase/blob/v6.8.3/src/corelib/io/qprocess_unix.cpp#L595-L632). Os jobs normais continuam cobrindo o Qt do sistema.

Nenhum teste foi removido, nenhum timeout foi ampliado e nenhuma supressão de sanitizer foi adicionada. As correções dependem da execução do CI no commit publicado. A tag e os pacotes públicos devem apontar para o mesmo commit validado; builds Linux anteriores não são evidência desta versão. As limitações de TSan nos widgets Qt continuam distintas da suíte obrigatória de núcleo/controlador.

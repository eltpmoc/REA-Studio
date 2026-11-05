# Instalação da alpha

Verifique SHA256SUMS.txt antes de extrair. SHA-256 detecta corrupção/modificação relativa ao manifesto recebido; não substitui uma assinatura autenticada. Esta alpha não tem assinatura de código, notarização ou instalador automático.

## Linux x86_64 (Qt do sistema)

```sh
sha256sum -c SHA256SUMS.txt
tar -xzf REA-Studio-v0.3.0-alpha-Linux-x86_64.tar.gz
./REA-Studio-v0.3.0-alpha-Linux-x86_64/bin/rea-studio /usr/bin/true
```

A árvore pode ficar no diretório do usuário; não exige sudo nem instala serviços. Copie a árvore inteira. Para remover, apague apenas a árvore que você extraiu. Configurações Qt do usuário são independentes e não são removidas pelo pacote.

Qt6Widgets/Gui/Core compartilhados, libstdc++, glibc, libgcc e backend de janela Qt compatíveis são requisitos do sistema. No Ubuntu, instale qt6-base-dev ou os pacotes de runtime Qt correspondentes à versão da distribuição, incluindo o plugin de plataforma (ex.: qt6-qpa-plugins), bibliotecas X11/Wayland/OpenGL e fontes. O pacote local foi construído no Ubuntu 26.04.1, Qt 6.10.2 e GCC 15.2; não é prometida execução em Ubuntu mais antigo. Consulte share/rea-studio/REA_BUILD_METADATA.txt e DEPENDENCIES.txt. O CI Ubuntu 24.04 produzirá outro binário com dependências de seu próprio ambiente.

```sh
ldd REA-Studio-v0.3.0-alpha-Linux-x86_64/bin/rea-studio
QT_QPA_PLATFORM=offscreen REA-Studio-v0.3.0-alpha-Linux-x86_64/bin/rea-studio --smoke /usr/bin/true
```

Use ldd apenas no binário do próprio projeto de origem confiável; nunca em um ELF não confiável que pretende analisar. O smoke é um teste automatizado de Qt offscreen, não valida interação em desktop.

## Windows x64

O build local v0.3.2-alpha foi compilado com MSVC x64 e Qt 6.8.3 e passou os 21 testes aplicáveis. Use o ZIP produzido por `scripts/package-windows.ps1`, que exige também os smoke tests de deployment. Os comandos abaixo são executados no diretório que contém o ZIP (nesta máquina, `dist/windows-x64`); veja [build Windows e evidências](build-windows.md).

```powershell
Get-FileHash REA-Studio-v0.3.2-alpha-Windows-x64.zip -Algorithm SHA256
Expand-Archive REA-Studio-v0.3.2-alpha-Windows-x64.zip -DestinationPath .
./REA-Studio-v0.3.2-alpha-Windows-x64/bin/rea-studio.exe
```

Compare o hash com SHA256SUMS.txt. Instale o Visual C++ Redistributable oficial x64 compatível com o compilador. Preserve bin/platforms e qt.conf; não misture DLLs de outra instalação Qt. O programa Windows continua aceitando ELF, sem suporte PE. Plugins/fontes/licenças acompanham a árvore. Nenhum certificado de assinatura é alegado.

## Fontes

O arquivo Source.tar.gz correspondente à versão do pacote contém todo o código próprio e vendorizado necessário ao build, testes e scripts. Extraia e siga build-linux.md ou build-windows.md. Qt e toolchain são requisitos externos. Cada pacote binário inclui o mesmo source archive em share/rea-studio/sources. No Windows a fonte QtBase também acompanha o pacote.

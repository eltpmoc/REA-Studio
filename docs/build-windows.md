# Build Windows x64 — preparado, não validado localmente

Use Windows x64 real, Visual Studio 2022 com Desktop development with C++, Windows SDK, CMake >=3.21, Python >=3.10, PowerShell 7 e Qt 6.8.3 MSVC2022 x64 (qtbase, incluindo Widgets/Test e windeployqt). Não misture Qt MinGW, ARM64 ou x86 com MSVC x64.

No Developer PowerShell 7 para VS2022:

```powershell
$env:CMAKE_PREFIX_PATH = 'C:/Qt/6.8.3/msvc2022_64'
$env:PATH = 'C:/Qt/6.8.3/msvc2022_64/bin;' + $env:PATH
python scripts/release.py audit
cmake --preset windows-x64
cmake --build --preset windows-x64 --parallel
ctest --preset windows-x64 --output-junit windows-release-tests.xml
```

O preset fixa Visual Studio 17 2022 e arquitetura x64. UTF-8 se aplica a fontes e testes; NOMINMAX/WIN32_LEAN_AND_MEAN evitam macros da API Windows; CRT /MD ou /MDd acompanha o Qt DLL oficial. Zydis/Zycore são C estático local, configurado pelo mesmo toolchain, sem extensão GCC no código próprio. A única fixture FIFO está guardada por !\_WIN32. A abertura de arquivo do worker usa filesystem::path com QString::toStdWString no Windows. Não há backend PE, mesmo quando o aplicativo roda em Windows.

São 12 testes aplicáveis no Windows. Cinco dos 17 testes Linux usam /usr/bin/true e já eram condicionais no CMake; não foram desabilitados para conseguir aprovação. GUI usa offscreen nos testes. O teste de arquivo de 5 GiB pode precisar de espaço físico se o filesystem não criar arquivo esparso; reserve espaço antes de executar.

Deployment após build e testes reais:

```powershell
python scripts/fetch-qt-source.py build-windows/qt-source
./scripts/package-windows.ps1 `
  -QtRoot C:/Qt/6.8.3/msvc2022_64 `
  -QtSource build-windows/qt-source/qtbase-everywhere-src-6.8.3.tar.xz
```

O script exige metadata MSVC x64 / Qt 6.8.3, repete CTest, executa windeployqt, instala apenas plugins qwindows/qoffscreen do QtBase, verifica DLLs e gera relatório dumpbin. Depois executa o binário instalado com PATH restrito ao pacote e Windows, nos backends offscreen e windows. Qualquer falha impede o ZIP. Precisa de dumpbin no PATH do Developer PowerShell. Não executa binário alvo.

O ZIP inclui fontes completas de REA e QtBase, licenças/atribuições Qt extraídas do source archive verificado e qt.conf com plugins relativos. Só permite Qt6Core/Gui/Widgets; outro módulo requer revisão de licenças/fontes antes de alterar essa regra. Não inclui runtime CRT de desenvolvimento: usuário deve instalar o [Visual C++ Redistributable oficial x64](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist) compatível com os build tools. Ver [deployment oficial Qt 6.8](https://doc.qt.io/qt-6.8/windows-deployment.html).

.github/workflows/ci.yml contém job windows-latest. Nenhum workflow remoto foi disparado nesta preparação. Os passos acima e o PowerShell ainda dependem de execução real em Windows; não se afirma build/teste/deployment Windows aprovado.

# Build Windows x64

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

## Visual Studio 2026 com toolset MSVC 2022

O preset `windows-x64-ninja` usa o MSVC do Developer Shell e grava em `build-windows-x64`. Isso permite usar o toolset v143 instalado no Visual Studio 2026 sem alterar o preset VS2022 do CI. Abra PowerShell na raiz do projeto:

```powershell
$installer = "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer"
$env:PATH = "$installer;" + $env:PATH
$vs = & "$installer/vswhere.exe" -latest -products * `
  -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
Import-Module "$vs/Common7/Tools/Microsoft.VisualStudio.DevShell.dll"
Enter-VsDevShell -VsInstallPath $vs -SkipAutomaticLocation `
  -DevCmdArguments '-arch=x64 -host_arch=x64 -vcvars_ver=14.44'
$env:CMAKE_PREFIX_PATH = (Resolve-Path 'build-tools/Qt/6.8.3/msvc2022_64').Path
$env:PATH = "$env:CMAKE_PREFIX_PATH/bin;" + $env:PATH
$env:PYTHONUTF8 = '1'
python scripts/release.py audit
cmake --preset windows-x64-ninja
cmake --build --preset windows-x64-ninja --parallel 6
ctest --preset windows-x64-ninja --output-junit windows-release-tests.xml
```

O comando seleciona o MSVC 14.44; instale esse componente ou ajuste a versão conforme seu toolset. Se o Qt estiver em outro diretório, ajuste `CMAKE_PREFIX_PATH`. O [Qt 6.8 para Windows](https://doc.qt.io/qt-6.8/windows.html) oferece o kit MSVC2022 x86_64. Aqui x64/x86-64 significa aplicativo de 64 bits; não é um build Win32 de 32 bits.

Caso falte o Qt, a instalação local usada nesta máquina foi:

```powershell
python -m venv build-tools/venv
./build-tools/venv/Scripts/python.exe -m pip install aqtinstall==3.3.0
./build-tools/venv/Scripts/python.exe -m aqt install-qt windows desktop 6.8.3 `
  win64_msvc2022_64 --archives qtbase --outputdir build-tools/Qt
```

São 21 testes aplicáveis no Windows nesta árvore. Os testes Linux com `/usr/bin/true` e a fixture compilada ELF continuam condicionais. Nenhum teste Windows é desabilitado. O CMake fornece `SystemRoot/Fonts` ao backend offscreen para suas capturas terem fontes disponíveis. A fixture de 5 GiB usa `FSCTL_SET_SPARSE` no Windows e requer um filesystem com suporte a arquivos esparsos, como NTFS; a verificação do tamanho, do byte final e da leitura acima de 4 GiB permanece ativa. Ver [API de arquivos esparsos da Microsoft](https://learn.microsoft.com/windows/win32/api/winioctl/ni-winioctl-fsctl_set_sparse).

## Empacotamento

Deployment após build e testes reais:

```powershell
python scripts/fetch-qt-source.py build-windows/qt-source
./scripts/package-windows.ps1 `
  -Build build-windows `
  -QtRoot C:/Qt/6.8.3/msvc2022_64 `
  -QtSource build-windows/qt-source/qtbase-everywhere-src-6.8.3.tar.xz
```

Para o preset Ninja, use `-Build build-windows-x64` e `-QtRoot $env:CMAKE_PREFIX_PATH`. Use `-Output dist/windows-x64` para manter o pacote separado dos artefatos Linux anteriores. Execute o script no PowerShell 7, com o mesmo ambiente de desenvolvimento. Nesta máquina também há um PowerShell 7 portátil em `build-tools/pwsh/pwsh.exe`.

O script exige metadata MSVC x64 / Qt 6.8.3, repete CTest, executa windeployqt, instala apenas plugins qwindows/qoffscreen do QtBase, verifica DLLs e gera relatório dumpbin. Depois executa o binário instalado com PATH restrito ao pacote e Windows, nos backends offscreen e windows. Qualquer falha impede o ZIP. Precisa de dumpbin no PATH do Developer PowerShell. Não executa binário alvo.

O ZIP inclui fontes completas de REA e QtBase, licenças/atribuições Qt extraídas do source archive verificado e qt.conf com plugins relativos. Só permite Qt6Core/Gui/Widgets; outro módulo requer revisão de licenças/fontes antes de alterar essa regra. Não inclui runtime CRT de desenvolvimento: usuário deve instalar o [Visual C++ Redistributable oficial x64](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist) compatível com os build tools. Ver [deployment oficial Qt 6.8](https://doc.qt.io/qt-6.8/windows-deployment.html).

.github/workflows/ci.yml contém job `windows-2022`. Nenhum workflow remoto é necessário para o build local. O pacote deve ser considerado aprovado apenas após CTest e os dois smoke tests concluírem com sucesso; o script só cria o ZIP depois desses passos.

## Validação local em 2026-11-05

- REA Studio 0.3.2-alpha, Windows AMD64, ponteiros de 64 bits, Release.
- Visual Studio 2026 com MSVC 19.44.35229.0 (toolset 14.44), CMake 4.3.1-msvc1, Ninja e Qt 6.8.3 MSVC2022 x64.
- Build concluído; CTest: **21/21 aprovados**, zero testes ignorados, em 39,21 s após a correção da fixture esparsa.
- O teste `core` inicialmente excedeu o tempo ao alocar fisicamente 5 GiB. Foi corrigida a criação da fixture, mantendo todos os testes e o limite de tempo; a execução individual passou em 0,22 s.
- `rea_visual_tests.exe` também passou com `QT_QPA_PLATFORM=windows`; captura nativa inspecionada com textos legíveis. As capturas offscreen usam agora as fontes do Windows.
- Auditoria dos 107 arquivos vendorizados e licenças aprovada. Build com um aviso C4456 preexistente em `src/graph_view.cpp` (variável local sombreada), sem erros.
- Empacotamento aprovado: `windeployqt`, relatório `dumpbin`, 116 arquivos de licenças/atribuições Qt e smoke tests `offscreen`/`windows` com PATH restrito ao pacote e ao Windows. Log em `build-windows-x64/package-release.log`.

Relatórios locais em `build-windows-x64`: `REA_BUILD_METADATA.txt`, `build-release.log`, `windows-release-tests.xml`, `windows-initial-tests.xml`, `windows-core-tests.xml` e `native-ui/workspace-visual-fixture.png`. O script de pacote repete a suíte e grava a execução mais recente em `windows-release-tests.xml`; os tempos podem variar.

O pacote solicitado fica em `dist/windows-x64/REA-Studio-v0.3.2-alpha-Windows-x64.zip` após o script concluir. A máquina de desenvolvimento possui o runtime Visual C++; o smoke com PATH restrito verifica as dependências Qt do pacote, mas não substitui um teste em máquina limpa sem o runtime. Não foram gerados build Win32, assinatura de código ou publicação remota.

# Third-party components

REA Studio's original code is GPL-3.0-or-later. The bundled third-party source
files retain their original licenses and notices; they are not relicensed.

## Zydis 4.1.1

- Upstream: https://github.com/zyantific/zydis
- Tag: v4.1.1
- Commit: a2278f1d254e492f6a6b39f6cb5d1f5d515659dc
- License: MIT; full unchanged text: `third_party/zydis/LICENSE`.
- Copyright (c) 2014-2024 Florian Bernd and Joel Höner.

## Zycore 1.5.2

- Upstream: https://github.com/zyantific/zycore-c
- Commit pinned by the Zydis submodule: 0b2432ced0884fd152b471d97ecf0258ff4d859f
- License: MIT; full unchanged text:
  `third_party/zydis/dependencies/zycore/LICENSE`.
- Copyright (c) 2018-2024 Florian Bernd and Joel Höner.

`third_party/zydis` contains the unmodified CMake, source, headers, resources,
CMake helpers, README and license files needed for the build, plus the matching
Zycore files. Git metadata, examples, tools and upstream tests are not included.
The parent CMake disables tools, examples, tests, shared libraries and the encoder.
No downloads are needed during configuration or compilation. Integrity hashes
of included files are recorded in `third_party/SHA256SUMS` (paths from repo root).

The MIT permission and notice requirements are retained. MIT/Expat is listed as
GPL-compatible by GNU: https://www.gnu.org/licenses/license-list.html#Expat.

Qt 6 Widgets (and Qt Test for tests) is supplied by the build environment and
remains under its applicable Qt licenses. Binary distribution must include the
applicable Qt deployment files and license notices. This repository does not
vendor or modify Qt. No code or assets from Hopper/IDA are included.

## Binary packaging for v0.3.0-alpha

Linux archives use system-provided shared Qt and system libraries; they do not
bundle Qt, libc, libstdc++ or platform plugins. The installed DEPENDENCIES.txt and
REA_BUILD_METADATA.txt identify the build/runtime environment. Install matching
system packages; obtain their corresponding source from the OS distributor.

The proposed Windows archive uses only QtBase 6.8.3: Qt6Core, Qt6Gui, Qt6Widgets,
qwindows and qoffscreen. The package script verifies the official QtBase source
archive SHA-256 recorded in packaging/qtbase.json, includes that complete source
archive, and extracts its LICENSES, copyright/license notices and attribution
records into share/rea-studio/licenses/Qt. No other Qt module is accepted by that
script. Qt is dynamically linked. The original LGPL-3.0 text is also included as
licenses/LGPL-3.0.txt. Qt and its embedded third-party code retain their specific
licenses; distributing this application's GPL source does not relicense Qt's
third-party components. The upstream source includes their full notices.

Qt source reference and checksum metadata:
https://download.qt.io/archive/qt/6.8/6.8.3/submodules/qtbase-everywhere-src-6.8.3.tar.xz.mirrorlist
Qt deployment/licensing documentation:
https://doc.qt.io/qt-6.8/windows-deployment.html
https://doc.qt.io/qt-6.8/licensing.html

The Windows package requires the official Microsoft Visual C++ x64
Redistributable at least as recent as the build tools. Developer-machine CRT
DLLs are not bundled. No Microsoft installer is redistributed by this project.
https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist

Every proposed binary archive includes the matching REA Studio source archive,
including vendored MIT code, build/package scripts and tests. Preserve these
source archives and notices when distributing or modifying binaries. See
COPYING.md for the pending reconciliation of the initial remote MIT LICENSE and
the local GPL project. Windows packaging is prepared, not locally validated.

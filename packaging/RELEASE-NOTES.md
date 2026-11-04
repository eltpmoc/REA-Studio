# REA Studio v0.3.0-alpha — release notes (local candidate)

First proposed public alpha. No remote release has been published by this preparation.

## Features

- Static ELF32/ELF64 metadata inspection and bounded x86-64 little-endian ELF ET_EXEC/ET_DYN disassembly using Zydis.
- Conservative function discovery with explicit confidence, basic blocks, CFG cycles and unresolved destinations.
- Navigable cross-references, history, original Qt Graph View and PNG/SVG graph exports.
- Original Dark/Light workspace, syntax highlighting, function separators, XREF indicators and imported/undefined symbol classification.
- Dedicated std::jthread analysis, cooperative cancellation, resource accounting and versioned contracts for a future separate worker.

## Artifacts and status

- REA-Studio-v0.3.0-alpha-Linux-x86_64.tar.gz: system-Qt distribution; requires compatible Linux x86_64 Qt, glibc and libstdc++ versions, identified by the installed build metadata/dependency report. This is not a universal portable/AppImage build.
- REA-Studio-v0.3.0-alpha-Source.tar.gz: complete source candidate with tests, build/package scripts, documentation and vendored MIT dependencies.
- REA-Studio-v0.3.0-alpha-Windows-x64.zip: **not available from this local preparation**. Include only after actual Windows MSVC build, CTest and deployed-package smoke tests pass. The CI is prepared to generate it with QtBase source/notices included.
- SHA256SUMS.txt: hashes only artifacts actually produced.

The existing 17 Linux tests are retained. Windows has 12 platform-applicable tests; five tests require Linux /usr/bin/true. Windows and native desktop validation are pending until corresponding real reports exist. Functional Qt tests here run offscreen. See docs/publication-readiness.md for local results.

## Known limitations

No PE, ARM64 disassembly, decompilation, indirect target resolution, jump-table recovery or real out-of-process isolation. Analysis is conservative and can be incomplete. Resource memory accounting is not a process RSS cap. Blocking I/O and synchronous model publication can delay cancellation/responsiveness. TSan widget reports in the prebuilt Qt pool remain unresolved; passing core/controller tests does not approve the entire GUI. LeakSanitizer is not approved in the current ptrace environment.

## Licensing and publication gate

Original code: GPL-3.0-or-later. Vendored Zydis/Zycore: MIT. Qt/dependencies retain their licenses. The remote initial commit contains a MIT LICENSE while the local project uses GPL; the maintainer must approve that reconciliation and preserve remote history before publication. See COPYING.md and docs/release-process.md. No signatures or notarization are claimed.

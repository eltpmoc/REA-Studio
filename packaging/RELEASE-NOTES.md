# REA Studio v0.3.2-alpha — release notes (local candidate)

First proposed public alpha. No remote release has been published by this preparation.

## Features

- Static ELF32/ELF64 metadata inspection and bounded x86-64 little-endian ELF ET_EXEC/ET_DYN disassembly using Zydis.
- Conservative function discovery with explicit confidence, basic blocks, CFG cycles and unresolved destinations.
- Navigable cross-references, history, original Qt Graph View and PNG/SVG graph exports.
- Original Dark/Light workspace, syntax highlighting, function separators, XREF indicators and imported/undefined symbol classification.
- Dedicated std::jthread analysis, cooperative cancellation, resource accounting and versioned contracts for a future separate worker.

## Artifacts and status

- REA-Studio-v0.3.2-alpha-Windows-x64.zip: Release build using MSVC 19.44 and Qt 6.8.3. Generated only after all 21 Windows tests and deployed-package smoke tests pass. Includes QtBase sources/notices and the matching REA source archive.
- REA-Studio-v0.3.2-alpha-Source.tar.gz: complete source candidate with tests, build/package scripts, documentation and vendored MIT dependencies.
- The older v0.3.0-alpha Linux artifact is preserved separately; no new Linux binary is produced by this Windows build.
- SHA256SUMS.txt: hashes only artifacts actually produced.

Local Windows validation on 2026-11-05: 21/21 applicable CTest tests passed. The visual test also passed using the native Windows backend, with an inspected screenshot. The 5 GiB test fixture now explicitly enables Windows sparse-file support; large-file checks and test timeouts remain enabled. Offscreen tests receive the Windows font directory. See docs/build-windows.md for the toolchain, commands, reports and limits. No remote CI result is claimed.

## Known limitations

No PE, ARM64 disassembly, complete decompilation, indirect target resolution, jump-table recovery or real out-of-process isolation. IR and pseudocode remain experimental. The Windows application is x64, not Win32; it requires a compatible official Visual C++ Redistributable x64. Analysis is conservative and can be incomplete. Resource memory accounting is not a process RSS cap. Blocking I/O and synchronous model publication can delay cancellation/responsiveness. Historical Linux TSan widget reports remain unresolved. Native Windows smoke/visual checks do not constitute full manual UI validation or testing on a clean machine.

## Licensing and publication gate

Original code: GPL-3.0-or-later. Vendored Zydis/Zycore: MIT. Qt/dependencies retain their licenses. The remote initial commit contains a MIT LICENSE while the local project uses GPL; the maintainer must approve that reconciliation and preserve remote history before publication. See COPYING.md and docs/release-process.md. No signatures or notarization are claimed.

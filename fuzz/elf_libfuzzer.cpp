// SPDX-License-Identifier: GPL-3.0-or-later
#include "elf_harness.hpp"
#include "analysis_fixtures.hpp"
// One scratch directory per fuzzing thread/process, never a shared fixed /tmp filename.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data,size_t size) {
    thread_local rea::test::Scratch scratch;
    (void)rea::fuzz::exercise({data,size},scratch.directory/"input.elf");return 0;
}

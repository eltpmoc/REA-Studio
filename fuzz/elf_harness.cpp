// SPDX-License-Identifier: GPL-3.0-or-later
#include "elf_harness.hpp"
#include <fstream>
#include <stdexcept>
namespace rea::fuzz {
bool exercise(std::span<const uint8_t> input,const std::filesystem::path& scratch) {
    if(input.size()>1024*1024) return false;
    {
        std::ofstream out(scratch,std::ios::binary);
        if(!input.empty()) out.write(reinterpret_cast<const char*>(input.data()),static_cast<std::streamsize>(input.size()));
        if(!out) throw std::logic_error("Fuzz harness cannot write input");
    }
    try {
        AnalysisLimits limits;limits.maxMemoryBytes=2*1024*1024;limits.maxTime=std::chrono::milliseconds(50);limits.maxWorkItems=20000;
        AnalysisControl control(limits);BinaryFile file(scratch);
        auto image=ElfLoader{}.load(file,control);
        // Oracles run outside expected runtime_error paths: an invariant failure must crash a fuzz run.
        for(const auto& s:image.sections) if(s.type!=0 && s.type!=8)
            if(s.offset>input.size() || s.size>input.size()-s.offset) throw std::logic_error("Parser accepted out-of-file section");
        for(const auto& s:image.segments)
            if(s.offset>input.size() || s.fileSize>input.size()-s.offset || s.address>UINT64_MAX-s.memorySize)
                throw std::logic_error("Parser accepted invalid program range");
        if(control.reservedBytes()>limits.maxMemoryBytes) throw std::logic_error("Memory budget overshot");
        return true;
    }catch(const std::runtime_error&){return false;}
}
}

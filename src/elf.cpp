// SPDX-License-Identifier: GPL-3.0-or-later
#include "rea/binary.hpp"
#include "rea/symbol_names.hpp"
#include "rea/analysis_control.hpp"
#include <algorithm>
#include <stdexcept>
namespace rea {
namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
struct Fields {
    std::vector<uint8_t> data;
    bool little;
    uint64_t get(size_t offset, size_t width) const {
        require(offset <= data.size() && width <= data.size()-offset, "Truncated ELF record");
        uint64_t result=0;
        for (size_t i=0; i<width; ++i) result |= uint64_t(data[offset+i]) << (8*(little ? i : width-1-i));
        return result;
    }
};
void range(BinaryFile& f, uint64_t offset, uint64_t size) {
    require(offset <= f.size() && size <= f.size()-offset, "ELF range outside file");
}
void table(BinaryFile& f, uint64_t offset, uint64_t count, uint64_t stride, uint64_t minimum) {
    require(count <= 200000, "ELF table exceeds resource limit");
    if (!count) return;
    require(stride >= minimum && offset <= f.size() && count <= (f.size()-offset)/stride, "Invalid ELF table");
}
std::string nameAt(BinaryFile& f, const Section& strings, uint64_t index, AnalysisControl& control) {
    require(strings.type == 3 && index < strings.size, "Invalid ELF string table reference");
    std::string result;
    while (index < strings.size && result.size() < 4096) {
        control.work();
        auto bytes = f.read(strings.offset+index, std::min<uint64_t>(256, strings.size-index));
        for (auto c : bytes) {
            if (!c) return result;
            control.reserveMemory(2); // Conservatively account string capacity growth before allocating.
            result.push_back(static_cast<char>(c));
            require(result.size() <= 4096, "ELF string exceeds resource limit");
        }
        index += bytes.size();
    }
    throw std::runtime_error("Unterminated ELF string");
}
}
bool ElfLoader::accepts(std::span<const uint8_t> m) const {
    return m.size() >= 4 && m[0]==0x7f && m[1]=='E' && m[2]=='L' && m[3]=='F';
}
Image ElfLoader::load(BinaryFile& f) const {
    AnalysisControl control;
    return load(f,control);
}
Image ElfLoader::load(BinaryFile& f, AnalysisControl& control) const {
    control.checkpoint(); control.reserveMemory(8192); // Bounded record/string-read scratch.
    control.report(1,"ELF identification");
    auto ident=f.read(0,16);
    require(accepts(ident), "Unsupported format: expected ELF (PE is not implemented)");
    require((ident[4]==1 || ident[4]==2) && (ident[5]==1 || ident[5]==2) && ident[6]==1, "Invalid ELF identification");
    Image im;bool demangleScratchReserved=false;
    im.bits=ident[4]==1 ? 32U : 64U; im.littleEndian=ident[5]==1;
    bool wide=im.bits==64;
    Fields h{f.read(0,wide ? 64 : 52),im.littleEndian};
    im.type=static_cast<uint16_t>(h.get(16,2)); im.machine=static_cast<uint16_t>(h.get(18,2));
    require(h.get(20,4)==1,"Invalid ELF version");
    im.entry=h.get(24,wide ? 8 : 4);
    im.programOffset=h.get(wide ? 32 : 28,wide ? 8 : 4);
    im.sectionOffset=h.get(wide ? 40 : 32,wide ? 8 : 4);
    im.headerSize=h.get(wide ? 52 : 40,2);
    require(im.headerSize >= (wide ? 64U : 52U),"Invalid ELF header size");
    range(f,0,im.headerSize);
    auto phStride=h.get(wide ? 54 : 42,2), phCount=h.get(wide ? 56 : 44,2);
    auto shStride=h.get(wide ? 58 : 46,2), shCount=h.get(wide ? 60 : 48,2);
    auto names=h.get(wide ? 62 : 50,2);
    // Extended numbering resides in section zero.
    if (im.sectionOffset && (!shCount || names==0xffff || phCount==0xffff)) {
        table(f,im.sectionOffset,1,shStride,wide ? 64 : 40);
        Fields zero{f.read(im.sectionOffset,wide ? 64 : 40),im.littleEndian};
        require(zero.get(4,4)==0,"Invalid null section");
        if (!shCount) shCount=zero.get(wide ? 32 : 20,wide ? 8 : 4);
        if (names==0xffff) names=zero.get(wide ? 40 : 24,4);
        if (phCount==0xffff) phCount=zero.get(wide ? 44 : 28,4);
    }
    require((!shCount || im.sectionOffset) && (!phCount || im.programOffset),"Missing ELF table offset");
    table(f,im.programOffset,phCount,phStride,wide ? 56 : 32);
    control.reserveMemory(phCount*(2*sizeof(Segment)+64));
    im.segments.reserve(static_cast<size_t>(phCount));
    for (uint64_t i=0;i<phCount;++i) {
        control.work(); control.report(5,"ELF program headers");
        Fields p{f.read(im.programOffset+i*phStride,wide ? 56 : 32),im.littleEndian};
        Segment s;
        s.type=static_cast<uint32_t>(p.get(0,4)); s.flags=static_cast<uint32_t>(p.get(wide ? 4 : 24,4));
        s.offset=p.get(wide ? 8 : 4,wide ? 8 : 4); s.address=p.get(wide ? 16 : 8,wide ? 8 : 4);
        s.fileSize=p.get(wide ? 32 : 16,wide ? 8 : 4); s.memorySize=p.get(wide ? 40 : 20,wide ? 8 : 4);
        range(f,s.offset,s.fileSize);
        require(s.address <= UINT64_MAX-s.memorySize,"Segment address overflow");
        require(s.type!=1 || s.fileSize<=s.memorySize,"Invalid load segment size");
        im.segments.push_back(s);
    }
    table(f,im.sectionOffset,shCount,shStride,wide ? 64 : 40);
    control.reserveMemory(shCount*(2*sizeof(Section)+sizeof(uint32_t)+64));
    im.sections.reserve(static_cast<size_t>(shCount));
    std::vector<uint32_t> nameIndexes;
    nameIndexes.reserve(static_cast<size_t>(shCount));
    for (uint64_t i=0;i<shCount;++i) {
        control.work(); control.report(10,"ELF section headers");
        Fields s{f.read(im.sectionOffset+i*shStride,wide ? 64 : 40),im.littleEndian};
        Section sec;
        nameIndexes.push_back(static_cast<uint32_t>(s.get(0,4)));
        sec.type=static_cast<uint32_t>(s.get(4,4)); sec.flags=s.get(8,wide ? 8 : 4);
        sec.address=s.get(wide ? 16 : 12,wide ? 8 : 4); sec.offset=s.get(wide ? 24 : 16,wide ? 8 : 4);
        sec.size=s.get(wide ? 32 : 20,wide ? 8 : 4); sec.link=static_cast<uint32_t>(s.get(wide ? 40 : 24,4));
        sec.info=static_cast<uint32_t>(s.get(wide ? 44 : 28,4)); sec.entrySize=s.get(wide ? 56 : 36,wide ? 8 : 4);
        require(sec.address <= UINT64_MAX-sec.size,"Section address overflow");
        if (sec.type!=8 && sec.type!=0) range(f,sec.offset,sec.size);
        im.sections.push_back(sec);
    }
    require(!names || names<im.sections.size(),"Invalid section name table index");
    if (names) for (size_t i=0;i<im.sections.size();++i) {
        control.work(); control.report(20,"ELF section names");
        im.sections[i].name=nameAt(f,im.sections[static_cast<size_t>(names)],nameIndexes[i],control);
    }
    for (const auto& sec : im.sections) {
        control.work();
        if (sec.type!=2 && sec.type!=11) continue;
        require(sec.link<im.sections.size() && im.sections[sec.link].type==3,"Invalid symbol string table");
        require(sec.entrySize >= (wide ? 24U : 16U) && sec.size%sec.entrySize==0,"Invalid symbol entry size");
        auto count=sec.size/sec.entrySize;
        require(count<=200000 && im.symbols.size()+count<=200000,"Too many symbols");
        control.reserveMemory(count*(2*sizeof(Symbol)+64));
        im.symbols.reserve(im.symbols.size()+static_cast<size_t>(count));
        for (uint64_t i=0;i<count;++i) {
            control.work(); control.report(30,"ELF symbols");
            Fields s{f.read(sec.offset+i*sec.entrySize,wide ? 24 : 16),im.littleEndian};
            Symbol sym;
            sym.name=nameAt(f,im.sections[sec.link],s.get(0,4),control);
            sym.address=s.get(wide ? 8 : 4,wide ? 8 : 4); sym.size=s.get(wide ? 16 : 8,wide ? 8 : 4);
            sym.info=static_cast<uint8_t>(s.get(wide ? 4 : 12,1)); sym.visibility=static_cast<uint8_t>(s.get(wide ? 5 : 13,1));
            sym.section=static_cast<uint32_t>(s.get(wide ? 6 : 14,2)); sym.dynamic=sec.type==11;
            require(sym.section!=0xffff,"Extended symbol section indexes are not implemented");
            require(sym.section>=0xff00 || sym.section<im.sections.size(),"Invalid symbol section index");
            if(sym.name.starts_with("_Z") || sym.name.starts_with("?")) {
                if(!demangleScratchReserved){control.reserveMemory(65536);demangleScratchReserved=true;}
                control.work();auto demangled=demangleSymbol(sym.name);control.reserveMemory(128+2*demangled.size());sym.demangledName=std::move(demangled);control.checkpoint();
            }
            im.symbols.push_back(std::move(sym));
        }
    }
    control.checkpoint(); control.report(35,"ELF parsed",true);
    return im;
}
}

// SPDX-License-Identifier: GPL-3.0-or-later
#include "rea/binary.hpp"
#include "rea/analysis.hpp"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace rea;
namespace {
void check(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
struct Fixture {
    bool wide, little;
    std::vector<uint8_t> bytes=std::vector<uint8_t>(1024);
    void put(size_t o,uint64_t value,size_t width) {
        for(size_t i=0;i<width;++i) bytes.at(o+i)=static_cast<uint8_t>(value>>(8*(little ? i : width-1-i)));
    }
    Fixture(bool w,bool l):wide(w),little(l) {
        bytes[0]=0x7f; bytes[1]='E'; bytes[2]='L'; bytes[3]='F'; bytes[4]=w ? 2 : 1; bytes[5]=l ? 1 : 2; bytes[6]=1;
        put(16,2,2); put(18,w ? 62 : 3,2); put(20,1,4); put(24,0x400000,w ? 8 : 4);
        put(w ? 32 : 28,64,w ? 8 : 4); put(w ? 40 : 32,128,w ? 8 : 4);
        put(w ? 52 : 40,w ? 64 : 52,2); put(w ? 54 : 42,w ? 56 : 32,2); put(w ? 56 : 44,1,2);
        put(w ? 58 : 46,w ? 64 : 40,2); put(w ? 60 : 48,4,2); put(w ? 62 : 50,1,2);
        put(64,1,4); put(w ? 68 : 88,5,4); put(w ? 72 : 68,0,w ? 8 : 4); put(w ? 80 : 72,0x400000,w ? 8 : 4);
        put(w ? 96 : 80,1024,w ? 8 : 4); put(w ? 104 : 84,2048,w ? 8 : 4);
        const std::string names("\0.shstrtab\0.strtab\0.symtab\0",28);
        std::copy(names.begin(),names.end(),bytes.begin()+512);
        bytes[600]=0; bytes[601]='f'; bytes[602]='o'; bytes[603]='o'; bytes[604]=0;
        section(1,1,3,512,names.size()); section(2,11,3,600,5); section(3,19,2,640,w ? 48 : 32);
        size_t sh=128+3*(w ? 64 : 40); put(sh+(w ? 40 : 24),2,4); put(sh+(w ? 56 : 36),w ? 24 : 16,w ? 8 : 4);
        size_t sym=640+(w ? 24 : 16); put(sym,1,4); put(sym+(w ? 8 : 4),0x400010,w ? 8 : 4); put(sym+(w ? 16 : 8),4,w ? 8 : 4);
        put(sym+(w ? 6 : 14),2,2);
    }
    void section(size_t i,uint32_t name,uint32_t type,uint64_t offset,uint64_t size) {
        auto o=128+i*(wide ? 64 : 40); put(o,name,4); put(o+4,type,4);
        put(o+(wide ? 24 : 16),offset,wide ? 8 : 4); put(o+(wide ? 32 : 20),size,wide ? 8 : 4);
    }
};
void write(const std::filesystem::path& p,const std::vector<uint8_t>& bytes) {
    std::ofstream f(p,std::ios::binary); f.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size())); check(bool(f),"write fixture");
}
}
int main(int argc,char** argv) {
    if(argc==3 && std::string(argv[1])=="--fixture") {
        try {
            Fixture fixture(true,true); fixture.put(24,0x400300,8);
            const std::vector<uint8_t> code{0x31,0xed,0xe8,0x03,0x00,0x00,0x00,0x75,0x01,0xc3,0x90,0xc3};
            std::copy(code.begin(),code.end(),fixture.bytes.begin()+768);
            write(argv[2],fixture.bytes); return 0;
        }
        catch(const std::exception& e) {std::cerr<<e.what(); return 1;}
    }
    auto dir=std::filesystem::temp_directory_path()/("rea-tests-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        std::filesystem::create_directory(dir); auto path=dir/"fixture.elf";
        int rejected=0;
        auto bad=[&](const std::vector<uint8_t>& b){write(path,b); bool failed=false; try {BinaryFile f(path); (void)ElfLoader{}.load(f);} catch(const std::exception&){failed=true;} check(failed,"malformed ELF accepted"); ++rejected;};
        for(bool wide : {false,true}) for(bool little : {false,true}) {
            Fixture fixture(wide,little); write(path,fixture.bytes);
            {BinaryFile f(path); auto im=ElfLoader{}.load(f);
            check(im.bits==(wide ? 64U : 32U) && im.littleEndian==little,"class/endian");
            check(im.entry==0x400000 && im.sections.size()==4,"headers");
            check(im.sections[1].name==".shstrtab" && im.symbols.size()==2 && im.symbols[1].name=="foo","sections/symbols");
            check(im.offsetForAddress(0x400010)==16,"VA mapping"); check(!im.offsetForAddress(0x400400),"BSS mapping");
            check(!im.offsetForAddress(0x3fffff),"unmapped address");
            auto conflicting=im.segments.front(); conflicting.offset=1; im.segments.push_back(conflicting);
            check(!im.offsetForAddress(0x400010),"ambiguous mapping");
            check(hexRow(f,0).find("7f 45 4c 46")!=std::string::npos,"hex bytes");
            check(hexRow(f,1023,1).find("00000000000003ff")!=std::string::npos,"last byte");
            bool failed=false; try{f.read(UINT64_MAX,2);}catch(...){failed=true;} check(failed,"overflow read");}
            for(size_t length : {0U,4U,15U,31U,51U,63U,127U,200U,650U}) {auto b=fixture.bytes; b.resize(length); bad(b);}
            {auto x=fixture; x.put(wide ? 40 : 32,UINT64_MAX,wide ? 8 : 4); bad(x.bytes);}
            {auto x=fixture; x.put(128+(wide ? 64 : 40)+(wide ? 24 : 16),1023,wide ? 8 : 4); bad(x.bytes);}
            {auto x=fixture; x.put(128+3*(wide ? 64 : 40)+(wide ? 40 : 24),999,4); bad(x.bytes);}
            {auto x=fixture; x.put(128+3*(wide ? 64 : 40)+(wide ? 56 : 36),0,wide ? 8 : 4); bad(x.bytes);}
            {auto x=fixture; x.bytes[5]=0; bad(x.bytes);}
            {auto x=fixture; x.bytes[604]='x'; bad(x.bytes);}
            {auto x=fixture; x.put(wide ? 60 : 48,0,2); x.put(128+(wide ? 32 : 20),4,wide ? 8 : 4); write(path,x.bytes); BinaryFile f(path); check(ElfLoader{}.load(f).sections.size()==4,"extended count");}
            {auto x=fixture; x.put(128+3*(wide ? 64 : 40)+4,11,4); write(path,x.bytes); BinaryFile f(path); check(ElfLoader{}.load(f).symbols[1].dynamic,"dynamic symbols");}
        }
        auto large=dir/"large.bin";
        {std::ofstream out(large,std::ios::binary); out.seekp(static_cast<std::streamoff>((uint64_t{5}<<30)-1)); out.put('Z'); check(bool(out),"large sparse fixture");}
        {BinaryFile f(large); check(f.size()==(uint64_t{5}<<30),"64-bit file size"); check(hexRow(f,f.size()-1,1).find("|Z|")!=std::string::npos,"large file navigation");}
        if(argc==2) {BinaryFile f(argv[1]); auto im=ElfLoader{}.load(f); check(!im.sections.empty() && !im.segments.empty(),"system ELF"); std::cout<<argv[1]<<": "<<im.sections.size()<<" sections, "<<im.symbols.size()<<" symbols\n";}
        std::filesystem::remove_all(dir); std::cout<<"PASS: four class/endian combinations, "<<rejected<<" malformed fixtures, extended headers, static/dynamic symbols, sparse 5 GiB navigation\n"; return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\nFixtures: "<<dir<<'\n'; return 1;}
}

// SPDX-License-Identifier: GPL-3.0-or-later
#include "rea/disassembly.hpp"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace rea;
namespace {
void check(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
Instruction decode(std::initializer_list<uint8_t> code,uint64_t va=0x1000) {
    return X86InstructionDecoder{}.decode(std::span<const uint8_t>(code.begin(),code.size()),va,0x200);
}
Image image() {
    Image im; im.type=2; im.bits=64; im.machine=62; im.littleEndian=true; im.entry=0x1000;
    im.segments.push_back({1,5,0,0x1000,32,64}); im.sections.resize(2); return im;
}
const Instruction* find(const AnalysisResult& result,uint64_t va) {
    for(const auto& ins:result.instructions) if(ins.address==va) return &ins;
    return nullptr;
}
}
int main(int argc,char** argv) {
    auto path=std::filesystem::temp_directory_path()/("rea-code-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        auto nop=decode({0x90}); check(nop.mnemonic=="nop" && nop.size==1 && nop.offset==0x200 && nop.bytes==std::vector<uint8_t>{0x90},"nop metadata");
        check(decode({0x48,0x89,0xe5}).mnemonic=="mov","64-bit mov");
        auto call=decode({0xe8,0x05,0,0,0}); check(call.flow==FlowType::Call && call.directTarget==0x100a && call.size==5,"direct call rel32");
        check(decode({0xe8,0xfb,0xff,0xff,0xff}).directTarget==0x1000,"backward call");
        check(decode({0xff,0xd0}).flow==FlowType::Call && !decode({0xff,0xd0}).directTarget,"indirect call");
        check(decode({0x75,0xfc}).flow==FlowType::ConditionalJump && decode({0x75,0xfc}).directTarget==0xffe,"short conditional jump");
        check(decode({0x0f,0x85,0x10,0,0,0}).directTarget==0x1016,"near conditional jump");
        check(decode({0xe2,0xfe}).flow==FlowType::ConditionalJump && decode({0xe2,0xfe}).directTarget==0x1000,"loop");
        check(decode({0xeb,0xfe}).flow==FlowType::Jump && decode({0xeb,0xfe}).directTarget==0x1000,"unconditional jump");
        check(decode({0xff,0xe0}).flow==FlowType::Jump && !decode({0xff,0xe0}).directTarget,"indirect jump");
        check(decode({0xc3}).flow==FlowType::Return && decode({0xc2,0x10,0}).size==3,"returns");
        check(decode({0xf4}).flow==FlowType::Stop && decode({0x0f,0x0b}).flow==FlowType::Stop,"terminators");
        for(const auto& code:std::vector<std::vector<uint8_t>>{{0x0f},{0xe8,1},{0x48},{0x06},{0x0f,0x38,0xff}}) {
            auto ins=X86InstructionDecoder{}.decode(code,0x1000,0);
            check(ins.flow==FlowType::Invalid && ins.size==1 && !ins.directTarget,"invalid/truncated encoding");
        }
        check(X86InstructionDecoder{}.decode({},0,0).size==0,"empty input");
        check(!decode({0xeb,0xfd},0).directTarget,"relative underflow");
        check(!decode({0xeb,0x7f},UINT64_MAX-2).directTarget,"relative target overflow");
        check(decode({0x90},UINT64_MAX).flow==FlowType::Invalid,"next VA overflow");
        auto rip=decode({0x48,0x8b,0x05,0x10,0,0,0}); check(rip.dataReferences==std::vector<uint64_t>{0x1017},"RIP reference");
        auto im=image();
        AddressTranslator tr(im,32); check(tr.toFile(0x1000)==0 && tr.toFile(0x101f)==31 && tr.toVA(31)==0x101f,"EXEC translation");
        check(!tr.toFile(0x1020) && !tr.toFile(0x1040) && !tr.toVA(32) && !tr.toFile(0xfff),"mapping boundaries/BSS");
        check(tr.toRuntime(0x1000)==0x1000 && !AddressTranslator(im,32,1).toFile(0x1000),"EXEC bias");
        im.type=3; AddressTranslator dyn(im,32,0x70000000);
        check(dyn.toFile(0x1000)==0 && dyn.toRuntime(0x1000)==0x70001000 && dyn.fromRuntime(0x70001000)==0x1000,"DYN link/runtime distinction");
        check(!dyn.fromRuntime(1) && !dyn.toRuntime(UINT64_MAX),"runtime overflow");
        im.segments.push_back({1,5,1,0x1000,31,31}); check(!AddressTranslator(im,32).toFile(0x1000),"conflicting VA mapping");
        im=image(); im.segments.push_back({1,5,0,0x2000,32,32}); check(!AddressTranslator(im,32).toVA(0),"ambiguous reverse mapping");
        im=image(); im.segments[0].offset=UINT64_MAX; check(!AddressTranslator(im,32).toFile(0x1000),"offset overflow");
        im=image(); im.segments[0].address=UINT64_MAX-5; check(!AddressTranslator(im,32).toFile(UINT64_MAX-5),"segment overflow");
        im=image(); im.segments[0].fileSize=65; check(!AddressTranslator(im,100).toFile(0x1000),"file/memory size mismatch");
        im=image(); im.type=1; check(!AddressTranslator(im,32).toFile(0x1000),"ET_REL no invented VA");
        im=image(); im.symbols.push_back({"defined",0x1010,1,1,2,0,false}); im.symbols.push_back({"undefined",0x1000,0,0,2,0,true});
        SymbolResolver resolver(im); check(resolver.at(0x1010)=="defined" && resolver.at(0x1000).empty(),"defined symbol resolution");
        // call + conditional path + return; bytes after terminators must remain unanalyzed.
        std::vector<uint8_t> bytes(32,0x90);
        const std::vector<uint8_t> code{0xe8,0x0b,0,0,0,0x75,0x02,0xc3,0x90,0xeb,0x05};
        std::copy(code.begin(),code.end(),bytes.begin()); bytes[16]=0xc3;
        auto write=[&]{std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));check(bool(out),"fixture write");};
        X86InstructionDecoder decoder; DisassemblyEngine engine(decoder);
        write(); im=image();
        {
            BinaryFile file(path); auto result=engine.analyze(file,im);
            check(find(result,0x1000) && find(result,0x1000)->confidence==Confidence::Confirmed,"entry seed");
            check(find(result,0x1010) && find(result,0x1010)->flow==FlowType::Return && find(result,0x1010)->confidence==Confidence::Probable,"call target hypothesis");
            check(find(result,0x1009) && !find(result,0x1008) && !find(result,0x1011),"recursive traversal not linear sweep");
            check(result.references.size()==3,"flow references");
            auto limited=engine.analyze(file,im,{},2); check(limited.limited && limited.instructions.size()==2,"instruction budget");
            auto manual=engine.analyze(file,im,0x101f); check(find(manual,0x101f) && find(manual,0x101f)->confidence==Confidence::Probable,"manual probable seed");
        }
        im.symbols.push_back({"callee",0x1010,1,1,2,0,false});
        {BinaryFile file(path);check(find(engine.analyze(file,im),0x1010)->confidence==Confidence::Confirmed,"symbol evidence upgrade");}
        // An overlapping branch target is rejected instead of decoding both interpretations.
        bytes.assign(32,0x90); bytes[0]=0xeb; bytes[1]=0xff; write(); im=image();
        {BinaryFile file(path);auto result=engine.analyze(file,im);check(result.instructions.size()==1 && !result.diagnostics.empty(),"overlapping instruction boundary");}
        bytes.assign(32,0x90); bytes[31]=0xe8; write(); im=image(); im.entry=0x101f;
        {BinaryFile file(path);auto result=engine.analyze(file,im);check(result.instructions.size()==1 && result.instructions[0].flow==FlowType::Invalid && result.instructions[0].confidence==Confidence::Unanalyzed,"truncated instruction at segment end");}
        im.segments[0].flags=4;
        {BinaryFile file(path);check(engine.analyze(file,im).instructions.empty(),"non executable mapping rejected");}
        im=image(); im.machine=3;
        {BinaryFile file(path);check(engine.analyze(file,im).instructions.empty(),"unsupported architecture");}
        im=image(); im.segments[0].offset=32;
        {BinaryFile file(path);check(engine.analyze(file,im).instructions.empty(),"malformed mapping rejected");}
        if(argc==2) {
            BinaryFile file(argv[1]); auto elf=ElfLoader{}.load(file); auto result=engine.analyze(file,elf); auto* entry=find(result,elf.entry);
            check(entry && entry->flow!=FlowType::Invalid,"system entry real assembly");
            std::cout<<argv[1]<<": ELF entry VA 0x"<<std::hex<<elf.entry<<", offset 0x"<<entry->offset<<": "<<entry->mnemonic<<' '<<entry->operands<<std::dec<<"; "<<result.instructions.size()<<" instructions; "<<result.references.size()<<" references\n";
            size_t printed=0;for(const auto& ins:result.instructions) if(ins.address>=elf.entry && printed++<8) std::cout<<"  0x"<<std::hex<<ins.address<<" "<<ins.mnemonic<<" "<<ins.operands<<std::dec<<'\n';
        }
        std::filesystem::remove(path); std::cout<<"PASS: x86-64 decoder, flows, translation, recursive analysis, confidence and budgets\n";return 0;
    } catch(const std::exception& e) {std::cerr<<"FAIL: "<<e.what()<<"\nFixture: "<<path<<'\n';return 1;}
}

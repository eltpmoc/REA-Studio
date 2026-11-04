// SPDX-License-Identifier: GPL-3.0-or-later
#include "rea/analysis_backend.hpp"
#include "rea/symbol_names.hpp"
#include "analysis_fixtures.hpp"
#include <iostream>
using namespace rea;
using test::check;
class CancelDuringDemand final : public IInstructionDecoder {
public:
    AnalysisControl& control;mutable int calls{};explicit CancelDuringDemand(AnalysisControl& c):control(c){}
    Instruction decode(std::span<const uint8_t> bytes,uint64_t va,uint64_t offset) const override {auto instruction=X86InstructionDecoder{}.decode(bytes,va,offset);if(++calls==2)control.cancel();return instruction;}
};
int main(){try{
    auto demangled=demangleSymbol("_Z5somarii");
#ifndef _WIN32
    check(demangled=="somar(int, int)","Itanium demangling available on this Linux compiler");
#endif
    check(demangleSymbol("plain_name").empty() && demangleSymbol("_Zbad").empty() && demangleSymbol(std::string(600,'N')).empty(),"unmangled, malformed and complexity fallback");
    test::Scratch scratch;auto path=scratch.directory/"incremental.elf";auto bytes=test::elf(4096);bytes[512]=0xc3;
    const std::vector<uint8_t> hidden{0xe8,0xfb,0xfe,0xff,0xff,0xb8,0x2a,0,0,0,0xc3};std::copy(hidden.begin(),hidden.end(),bytes.begin()+768);test::write(path,bytes);
    BinaryFile file(path);auto image=ElfLoader{}.load(file);auto quota=std::make_shared<InstructionBudget>(32);AnalysisControl initial({},quota);
    auto base=InProcessAnalysisBackend{}.analyze(file,image,initial,{},{});check(base.assembly.instructions.size()==1 && quota->used()==1,"initial entry only");
    AnalysisControl demand({},quota);auto expanded=InProcessAnalysisBackend{}.analyzeIncremental(file,image,demand,{},0x400300,base.assembly);
    check(expanded.assembly.stopReason==StopReason::None && expanded.assembly.instructions.size()==4 && quota->used()==4,"only three newly decoded instructions charged");
    check(expanded.assembly.instructions.front().bytes==base.assembly.instructions.front().bytes && base.assembly.instructions.size()==1,"baseline preserved immutable");
    auto* f=expanded.program.registry.at(0x400300);check(f && f->origin==FunctionOrigin::UserRequested && f->confidence==FunctionConfidence::Probable,"on-demand executable entry registered with explicit confidence");
    check(expanded.program.graphs.contains(f->entry) && expanded.program.ir.contains(f->entry) && !expanded.program.references.incoming(image.entry).empty(),"CFG IR and incoming references rebuilt over combined result");
    const auto consumed=quota->used();AnalysisControl again({},quota);auto duplicate=InProcessAnalysisBackend{}.analyzeIncremental(file,image,again,{},0x400300,expanded.assembly);check(quota->used()==consumed && duplicate.assembly.instructions.size()==4,"existing address costs no new decoding");
    AnalysisControl midControl;CancelDuringDemand midDecoder(midControl);auto mid=DisassemblyEngine(midDecoder).analyze(file,image,midControl,0x400300,&base.assembly);check(mid.stopReason==StopReason::Cancelled && midDecoder.calls==2 && mid.instructions.front().address==image.entry && base.assembly.instructions.size()==1,"cooperative cancel during newly decoded instructions");
    auto tinyQuota=std::make_shared<InstructionBudget>(2);check(tinyQuota->claim(),"account baseline attempt");AnalysisControl limited({},tinyQuota);auto partial=InProcessAnalysisBackend{}.analyzeIncremental(file,image,limited,{},0x400300,base.assembly);check(tinyQuota->used()==2 && partial.assembly.stopReason==StopReason::InstructionLimit && partial.assembly.instructions.front().address==image.entry,"global instruction quota and baseline retention");
    AnalysisControl cancelled;cancelled.cancel();bool stopped=false;try{InProcessAnalysisBackend{}.analyzeIncremental(file,image,cancelled,{},0x400300,base.assembly);}catch(const AnalysisStopped& e){stopped=e.reason()==StopReason::Cancelled;}check(stopped && base.assembly.instructions.size()==1,"cancel before baseline copy preserves caller state");
    auto denied=image;denied.segments[0].flags=4;AnalysisControl nonexec;bool rejected=false;try{InProcessAnalysisBackend{}.analyzeIncremental(file,denied,nonexec,{},0x400300,base.assembly);}catch(const std::runtime_error&){rejected=true;}check(rejected,"non-executable request rejected");
    bytes[512]=0x90;test::write(path,bytes);BinaryFile changed(path);AnalysisControl modified;rejected=false;try{InProcessAnalysisBackend{}.analyzeIncremental(changed,image,modified,{},0x400300,base.assembly);}catch(const std::runtime_error&){rejected=true;}check(rejected,"changed baseline bytes refused");
    std::cout<<"PASS demangling, incremental decoding, combined derived views, immutable baseline, quota, cancellation, non-executable and file changes\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}

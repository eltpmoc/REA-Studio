// SPDX-License-Identifier: GPL-3.0-or-later
#include "rea/ir.hpp"
#include "rea/function_analysis.hpp"
#include <iostream>
#include <stdexcept>
using namespace rea;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(){try{
    X86InstructionDecoder decoder;X86IRLifter lifter;AnalysisControl control;
    const std::vector<uint8_t> bytes{0xb8,0x2a,0,0,0,0x83,0xc0,1,0x48,0x8b,0x03,0x75,0xf3,0xc3};
    std::vector<Instruction> code;size_t position=0;while(position<bytes.size()){auto i=decoder.decode(std::span(bytes).subspan(position),0x1000+position,position);check(i.size!=0,"decode progress");position+=i.size;code.push_back(std::move(i));}
    auto f=lifter.function(0x1000,code,control);check(f.instructions.size()==5 && f.loweredCount==2,"actual MOV/ADD lowered subset");
    check(f.instructions[0].operands[0].bits==32 && f.instructions[0].operands[1].immediate==42,"typed width and immediate");
    check(f.instructions[2].operands[1].kind==IROperandKind::Memory && f.instructions[2].operands[1].base=="rbx" && !f.instructions[2].lowered,"memory retained without invented lowering");
    check(f.instructions[3].directTarget==0x1000 && !f.instructions[3].lowered,"conditional branch evidence");
    check(f.pseudocode.find("write_gpr(cpu, \"eax\", 32")!=std::string::npos && f.pseudocode.find("flags_add")!=std::string::npos,"register write and flags explicit");
    check(f.pseudocode.find("opaque_x86_step")!=std::string::npos && f.pseudocode.find("dispatch_ir")!=std::string::npos,"opaque instructions dispatch by PC without fabricated fallthrough");
    const std::vector<uint8_t> negative{0x48,0x83,0xc0,0xff};auto signedIR=lifter.lift(decoder.decode(negative,0x2000,0),control);check(signedIR.operands[1].signedImmediate && signedIR.operands[1].immediate==UINT64_MAX,"sign-extended immediate retained");
    Image image;image.type=2;image.entry=0x1000;image.segments.push_back({1,5,0,0x1000,bytes.size(),bytes.size()});AnalysisResult assembly;assembly.instructions=code;
    AnalysisControl base;auto registry=FunctionAnalyzer{}.analyze(image,bytes.size(),assembly,base);ReferenceIndex refs;refs.build(assembly,base);GraphBudget graphBudget;
    for(const auto& descriptor:registry.functions)CFGBuilder{}.build(descriptor,assembly,graphBudget,base);
    AnalysisLimits limits;limits.maxMemoryBytes=base.reservedBytes()+1;AnalysisControl bounded(limits);auto partial=analyzeProgram(image,bytes.size(),assembly,bounded);
    check(partial.irStopReason==StopReason::MemoryLimit && !partial.registry.functions.empty() && !partial.graphs.empty(),"IR budget preserves prior validated functions and CFG");
    AnalysisControl repeat;check(lifter.function(0x1000,code,repeat).listing==f.listing,"deterministic listing");
    AnalysisControl cancelled;cancelled.cancel();bool stopped=false;try{lifter.function(0x1000,code,cancelled);}catch(const AnalysisStopped&){stopped=true;}check(stopped,"IR cancellation");
    std::cout<<"PASS typed IR, limited lowering, flags, opaque memory/control, determinism and cancellation\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

// SPDX-License-Identifier: GPL-3.0-or-later
#include "rea/disassembly.hpp"
#include "analysis_fixtures.hpp"
#include <future>
#include <iostream>
#include <thread>
#ifndef _WIN32
#include <sys/stat.h>
#endif
using namespace rea;
using rea::test::check;
namespace {
class CancellingDecoder final : public IInstructionDecoder {
public:
    AnalysisControl& control;
    mutable unsigned count{};
    explicit CancellingDecoder(AnalysisControl& c):control(c) {}
    Instruction decode(std::span<const uint8_t> bytes,uint64_t va,uint64_t offset) const override {
        if(++count==10) control.cancel();
        return X86InstructionDecoder{}.decode(bytes,va,offset);
    }
};
class SlowDecoder final : public IInstructionDecoder {
public:
    Instruction decode(std::span<const uint8_t> bytes,uint64_t va,uint64_t offset) const override {
        std::this_thread::sleep_for(std::chrono::milliseconds(15));
        return X86InstructionDecoder{}.decode(bytes,va,offset);
    }
};
}
int main(int argc,char** argv) {
    try {
        test::Scratch scratch;auto path=scratch.directory/"long.elf";auto bytes=test::elf(65536);test::write(path,bytes);
        bool directoryRejected=false;try{BinaryFile directory(scratch.directory);}catch(const std::runtime_error&){directoryRejected=true;}
        check(directoryRejected,"directories cannot be analyzed");
#ifndef _WIN32
        auto pipe=scratch.directory/"pipe";check(::mkfifo(pipe.c_str(),0600)==0,"FIFO fixture creation");
        bool pipeRejected=false;try{BinaryFile fifo(pipe);}catch(const std::runtime_error&){pipeRejected=true;}
        check(pipeRejected,"FIFO must be rejected before a blocking open");
#endif
        X86InstructionDecoder decoder;
        AnalysisLimits limits;limits.maxTime=std::chrono::seconds(5);
        {BinaryFile f(path);AnalysisControl c(limits);c.cancel();bool cancelled=false;
            try {(void)ElfLoader{}.load(f,c);}catch(const AnalysisStopped& e){cancelled=e.reason()==StopReason::Cancelled;}
            check(cancelled,"parser pre-cancellation");}
        // Cancel via parser progress without timing assumptions.
        {BinaryFile f(path);AnalysisControl* active=nullptr;
            AnalysisControl c(limits,{},[&](const AnalysisProgress& p){if(p.phase=="ELF program headers") active->cancel();});active=&c;
            bool cancelled=false;try{(void)ElfLoader{}.load(f,c);}catch(const AnalysisStopped& e){cancelled=e.reason()==StopReason::Cancelled;}
            check(cancelled,"parser cancellation at a checkpoint");}
        {BinaryFile f(path);auto im=ElfLoader{}.load(f);AnalysisControl c(limits);CancellingDecoder cancelling(c);
            auto result=DisassemblyEngine(cancelling).analyze(f,im,c);
            check(result.stopReason==StopReason::Cancelled && cancelling.count==10 && result.instructions.size()==9,"cancel during decoding");}
        {BinaryFile f(path);AnalysisLimits tiny=limits;tiny.maxMemoryBytes=1024;AnalysisControl c(tiny);bool stopped=false;
            try{(void)ElfLoader{}.load(f,c);}catch(const AnalysisStopped& e){stopped=e.reason()==StopReason::MemoryLimit;}
            check(stopped && c.reservedBytes()<=1024,"parser memory budget before allocation");}
        {BinaryFile f(path);auto im=ElfLoader{}.load(f);AnalysisLimits tiny=limits;tiny.maxMemoryBytes=20000;AnalysisControl c(tiny);
            auto result=DisassemblyEngine(decoder).analyze(f,im,c);
            check(result.stopReason==StopReason::MemoryLimit && result.limited && result.instructions.size()<5 && c.reservedBytes()<=tiny.maxMemoryBytes,"engine accounted memory limit");}
        {BinaryFile f(path);AnalysisLimits instant=limits;instant.maxTime=std::chrono::milliseconds(0);AnalysisControl c(instant);bool stopped=false;
            try{(void)ElfLoader{}.load(f,c);}catch(const AnalysisStopped& e){stopped=e.reason()==StopReason::TimeLimit;}
            check(stopped,"parser deadline");}
        {BinaryFile f(path);auto im=ElfLoader{}.load(f);AnalysisLimits instant=limits;instant.maxTime=std::chrono::milliseconds(5);AnalysisControl c(instant);SlowDecoder slow;
            auto result=DisassemblyEngine(slow).analyze(f,im,c);
            check(result.stopReason==StopReason::TimeLimit && result.instructions.empty(),"deadline after decoder returns");}
        {BinaryFile f(path);auto im=ElfLoader{}.load(f);AnalysisLimits work=limits;work.maxWorkItems=8;AnalysisControl c(work);
            auto result=DisassemblyEngine(decoder).analyze(f,im,c);check(result.stopReason==StopReason::WorkLimit,"work budget");}
        {BinaryFile f(path);auto im=ElfLoader{}.load(f);AnalysisLimits zero=limits;zero.maxInstructions=0;AnalysisControl c(zero);
            auto result=DisassemblyEngine(decoder).analyze(f,im,c);check(result.stopReason==StopReason::InstructionLimit && result.instructions.empty(),"zero instruction quota");}
        // Both recursive and conditional back edges must terminate without relying on a timeout.
        bytes[512]=0x75;bytes[513]=0xfe;bytes[514]=0xeb;bytes[515]=0xfc;test::write(path,bytes);
        {BinaryFile f(path);auto im=ElfLoader{}.load(f);AnalysisControl c(limits);
            auto result=DisassemblyEngine(decoder).analyze(f,im,c);check(result.stopReason==StopReason::None && result.instructions.size()==2,"back-edge deduplication");}
        bytes=test::elf(65536);test::write(path,bytes);
        // Global quota is atomic and shared between independently owned files/controls.
        auto quota=std::make_shared<InstructionBudget>(37);
        auto run=[&]{BinaryFile f(path);auto im=ElfLoader{}.load(f);AnalysisControl c(limits,quota);return DisassemblyEngine(decoder).analyze(f,im,c);};
        auto one=std::async(std::launch::async,run);auto two=std::async(std::launch::async,run);
        auto a=one.get();auto b=two.get();
        check(quota->used()==37 && a.instructions.size()+b.instructions.size()==37,"concurrent quota must never overshoot");
        check(a.stopReason==StopReason::InstructionLimit && b.stopReason==StopReason::InstructionLimit,"both workers stop on global quota");
        {auto result=run();check(result.instructions.empty() && quota->used()==37,"new request cannot reset project quota");}
        if(argc==2) { // Existing symbol-rich fixture exercises cancellation inside string/symbol parsing.
            BinaryFile f(argv[1]);AnalysisControl* active=nullptr;
            AnalysisControl c(limits,{},[&](const AnalysisProgress& p){if(p.phase=="ELF symbols") active->cancel();});active=&c;
            bool stopped=false;try{(void)ElfLoader{}.load(f,c);}catch(const AnalysisStopped& e){stopped=e.reason()==StopReason::Cancelled;}
            check(stopped,"symbol parsing cancellation");
        }
        std::cout<<"PASS: parser/decoder cancellation, deadlines, memory/work limits, loops and concurrent global quota (37/37)\n";return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}

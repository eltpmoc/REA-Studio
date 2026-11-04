// SPDX-License-Identifier: GPL-3.0-or-later
#include "rea/analysis_backend.hpp"
#include "analysis_fixtures.hpp"
#include <future>
#include <iostream>
using namespace rea;
using test::check;
namespace {
Image image() {Image im;im.type=2;im.bits=64;im.machine=62;im.littleEndian=true;im.entry=0x1000;im.sections.resize(2);im.segments.push_back({1,5,0,0x1000,256,256});return im;}
Instruction ins(uint64_t a,FlowType flow,std::optional<uint64_t> target={}) {Instruction i;i.address=a;i.size=1;i.mnemonic="test";i.flow=flow;i.directTarget=target;return i;}
AnalysisResult assembly() {
    AnalysisResult a;a.instructions={ins(0x1000,FlowType::Call,0x1010),ins(0x1001,FlowType::ConditionalJump,0x1004),ins(0x1002,FlowType::Next),ins(0x1003,FlowType::Return),ins(0x1004,FlowType::Jump,0x1001),ins(0x1010,FlowType::Call),ins(0x1011,FlowType::Return)};
    a.instructions[0].dataReferences={0x1004,0x2000};return a;
}
template<class F> void stopped(F run,StopReason reason) {bool caught=false;try{run();}catch(const AnalysisStopped& e){caught=e.reason()==reason;}check(caught,"expected typed analysis stop");}
}
int main() {
    try {
        auto im=image();auto a=assembly();AnalysisControl control;
        auto p=analyzeProgram(im,256,a,control);
        check(p.registry.functions.size()==2,"jumps do not seed functions");
        const auto* entry=p.registry.at(im.entry);const auto* callee=p.registry.at(0x1010);
        check(entry && entry->confidence==FunctionConfidence::Confirmed && entry->hasReturn,"entry confidence/return");
        check(callee && callee->confidence==FunctionConfidence::Probable && callee->instructions.size()==2,"direct call discovery and indirect call fallthrough");
        check(entry->instructions.size()==5 && p.registry.containing(0x1004)==entry,"conservative membership");
        const auto& cfg=p.graphs.at(0x1000);
        check(cfg.blocks.size()==4 && cfg.edges.size()==5 && cfg.cyclic,"partition and cycles");
        check(CFGValidator::validate(cfg).empty(),"valid CFG");
        bool conditional=false,fallthrough=false,ret=false,jump=false;
        for(auto e:cfg.edges) {conditional|=e.type==EdgeType::Conditional;fallthrough|=e.type==EdgeType::Fallthrough;ret|=e.type==EdgeType::Return;jump|=e.type==EdgeType::Jump;}
        check(conditional && fallthrough && ret && jump,"edge kinds");
        check(p.references.all().size()==5 && p.references.incoming(0x1004).size()==2 && p.references.outgoing(0x1000).size()==3,"incoming outgoing indices");
        check(p.references.incoming(123).empty(),"missing reference lookup");
        bool data=false,code=false;for(auto r:p.references.all()) {data|=r.type==ReferenceType::DataAddress;code|=r.type==ReferenceType::CodeAddress;}
        check(data && code,"address operand evidence classification");
        check(p.references.outgoing(0x1010).empty(),"no fabricated indirect call destination");
        auto repeated=assembly();repeated.instructions[0].directTarget=0x1050;AnalysisControl missingControl;
        auto missing=FunctionAnalyzer{}.analyze(im,256,repeated,missingControl);
        check(missing.at(0x1050) && missing.at(0x1050)->confidence==FunctionConfidence::Unknown,"undecoded call target unknown");
        im.symbols={{"symbol",0x1010,1,1,2,0,false},{"alias",0x1010,1,1,2,0,true}};
        AnalysisControl symbolsControl;auto symbols=FunctionAnalyzer{}.analyze(im,256,a,symbolsControl);
        check(symbols.at(0x1010)->confidence==FunctionConfidence::Confirmed && symbols.at(0x1010)->duplicateEntries==2,"symbol upgrade and duplicate entries");
        check(symbols.at(0x1010)->instructions.size()==1 && !symbols.at(0x1010)->hasReturn,"symbol size bounds traversal");
        im.symbols.push_back({"overlapping",0x1000,32,1,2,0,false});AnalysisControl regionOverlap;
        auto regions=FunctionAnalyzer{}.analyze(im,256,a,regionOverlap);check(regions.at(0x1000)->overlap && regions.at(0x1010)->overlap,"declared ELF regions overlap");
        im.segments.push_back({1,4,16,0x1010,16,16});AnalysisControl nx;
        check(!FunctionAnalyzer{}.analyze(im,256,a,nx).at(0x1010),"ambiguous/non executable region rejected");
        auto shared=assembly();shared.instructions[5]=ins(0x1010,FlowType::Jump,0x1002);shared.instructions.pop_back();AnalysisControl overlap;
        auto registry=FunctionAnalyzer{}.analyze(image(),256,shared,overlap);
        check(registry.at(0x1000)->overlap && registry.at(0x1010)->overlap,"shared tail overlap");
        auto indirect=assembly();indirect.instructions[4].directTarget.reset();AnalysisControl unknown;
        auto u=analyzeProgram(image(),256,indirect,unknown);bool unresolved=false;
        for(auto e:u.graphs.at(0x1000).edges) unresolved|=e.type==EdgeType::Unknown && !e.to;
        check(unresolved,"unresolved indirect jump edge");
        indirect.instructions[4].directTarget=0x5000;AnalysisControl external;auto ex=analyzeProgram(image(),256,indirect,external);
        check(ex.registry.functions.size()==2,"external jump not function");
        bool externalEdge=false;for(auto e:ex.graphs.at(0x1000).edges) externalEdge|=e.type==EdgeType::Unknown && e.to==0x5000;check(externalEdge,"external target evidence retained");
        auto malformed=cfg;malformed.blocks.push_back(malformed.blocks.front());check(!CFGValidator::validate(malformed).empty(),"duplicate blocks rejected");
        malformed=cfg;malformed.edges.push_back({0xdead,0xbeef,EdgeType::Jump});check(!CFGValidator::validate(malformed).empty(),"missing nodes rejected");
        malformed=cfg;malformed.blocks[0].instructions[0].size=0;check(!CFGValidator::validate(malformed).empty(),"zero length rejected");
        GraphBudget tiny;tiny.maxNodes=1;AnalysisControl nodes;stopped([&]{CFGBuilder{}.build(*entry,a,tiny,nodes);},StopReason::WorkLimit);
        tiny={};tiny.maxEdges=0;AnalysisControl edges;stopped([&]{CFGBuilder{}.build(*entry,a,tiny,edges);},StopReason::WorkLimit);
        AnalysisLimits low;low.maxMemoryBytes=1;AnalysisControl memory(low);stopped([&]{FunctionAnalyzer{}.analyze(image(),256,a,memory);},StopReason::MemoryLimit);
        AnalysisControl* active=nullptr;bool sawBlock=false;
        AnalysisControl cancelled({}, {},[&](const AnalysisProgress& progress){if(progress.phase=="CFG blocks") {sawBlock=true;active->cancel();}});active=&cancelled;
        GraphBudget cb;stopped([&]{CFGBuilder{}.build(*entry,a,cb,cancelled);},StopReason::Cancelled);check(sawBlock,"cancel after CFG work began");
        AnalysisControl pre;pre.cancel();stopped([&]{FunctionAnalyzer{}.analyze(image(),256,a,pre);},StopReason::Cancelled);
        auto run=[&]{AnalysisControl c;auto result=analyzeProgram(image(),256,a,c);return result.graphs.at(0x1000).edges.size();};
        auto first=std::async(std::launch::async,run),second=std::async(std::launch::async,run);check(first.get()==second.get(),"independent concurrent deterministic analyses");
        protocol::Header h{protocol::version,protocol::MessageKind::Analyze,1,10};check(protocol::valid(h),"protocol header");h.protocolVersion++;check(!protocol::valid(h),"unknown version rejected");h.protocolVersion=1;h.payloadBytes=protocol::maxPayloadBytes+1;check(!protocol::valid(h),"oversize message");
        protocol::Message message{{1,protocol::MessageKind::Cancel,1,0},protocol::CancelMessage{}};check(protocol::valid(message),"typed cancel message");
        message.header.kind=protocol::MessageKind::Result;check(!protocol::valid(message),"payload tag mismatch rejected");
        check(protocol::transition(protocol::State::Running,protocol::MessageKind::Cancel)==protocol::State::Cancelling,"cancellation state");
        check(protocol::transition(protocol::State::Cancelling,protocol::MessageKind::Result)==protocol::State::Cancelled,"late result cancelled");
        check(!protocol::transition(protocol::State::Idle,protocol::MessageKind::Result),"unexpected result rejected");
        test::Scratch scratch;auto path=scratch.directory/"input.elf";test::write(path,test::elf());BinaryFile file(path);auto elf=ElfLoader{}.load(file);AnalysisControl stoppedControl;std::stop_source source;source.request_stop();
        auto stoppedResult=InProcessAnalysisBackend{}.analyze(file,elf,stoppedControl,source.get_token(),{});check(stoppedResult.assembly.stopReason==StopReason::Cancelled,"std stop_token bridge");
        std::cout<<"PASS functions, CFG, xrefs, malformed graphs, budgets, cancellation, concurrency and protocol\n";return 0;
    } catch(const std::exception& e) {std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}

// SPDX-License-Identifier: GPL-3.0-or-later
#include "rea/control_flow_analysis.hpp"
#include "rea/analysis_backend.hpp"
#include "rea/semantic_ir.hpp"
#include "analysis_fixtures.hpp"
#include <iostream>
using namespace rea;
using test::check;
ControlFlowGraph fixture() {
    ControlFlowGraph g;g.entry=1;
    for(uint64_t address:{1,2,3,4}) {
        Instruction i;i.address=address;i.size=1;i.bytes={0x90};i.flow=FlowType::Next;
        g.blocks.push_back({address,{i},{}});
    }
    g.blocks[0].instructions.back().flow=FlowType::ConditionalJump;
    g.blocks[0].instructions.back().directTarget=3;
    g.edges={{1,3,EdgeType::Conditional},{1,2,EdgeType::Fallthrough},{2,4,EdgeType::Jump},{3,4,EdgeType::Jump},{4,{},EdgeType::Return}};
    return g;
}
int main(int argc,char** argv) {try {
    AnalysisControl c;auto g=fixture();auto regions=recoverControlFlowRegions(g,c);
    check(regions.size()==1 && regions[0].kind==RegionKind::IfElse && regions[0].exit==4,"diamond");
    g.blocks[0].instructions.back().directTarget=4;
    g.edges={{1,4,EdgeType::Conditional},{1,2,EdgeType::Fallthrough},{2,3,EdgeType::Jump},{3,1,EdgeType::Jump},{4,{},EdgeType::Return}};
    regions=recoverControlFlowRegions(g,c);
    check(regions.size()==1 && regions[0].kind==RegionKind::While && regions[0].body.size()==3,"pretest loop");
    auto side=g;side.edges.push_back({2,4,EdgeType::Jump});
    check(recoverControlFlowRegions(side,c).empty(),"side exit rejected");
    side=g;side.entry=4;side.edges={{4,1,EdgeType::Jump},{4,2,EdgeType::Jump},{1,4,EdgeType::Conditional},{1,2,EdgeType::Fallthrough},{2,3,EdgeType::Jump},{3,1,EdgeType::Jump}};
    check(recoverControlFlowRegions(side,c).empty(),"irreducible side entry rejected");
    g.blocks[0].instructions.back().flow=FlowType::Next;g.blocks[0].instructions.back().directTarget.reset();
    g.blocks[2].instructions.back().flow=FlowType::ConditionalJump;g.blocks[2].instructions.back().directTarget=1;
    g.edges={{1,2,EdgeType::Fallthrough},{2,3,EdgeType::Fallthrough},{3,1,EdgeType::Conditional},{3,4,EdgeType::Fallthrough},{4,{},EdgeType::Return}};
    regions=recoverControlFlowRegions(g,c);
    check(regions.size()==1 && regions[0].kind==RegionKind::DoWhile,"posttest loop");
    auto unknown=g;unknown.edges.push_back({2,{},EdgeType::Unknown});check(recoverControlFlowRegions(unknown,c).empty(),"unknown exit rejected");
    AnalysisControl cancelled;cancelled.cancel();bool stopped=false;
    try{recoverControlFlowRegions(g,cancelled);}catch(const AnalysisStopped&){stopped=true;}check(stopped,"cancellation");
    if(argc==2) {
        BinaryFile file(argv[1]);auto image=ElfLoader{}.load(file);
        AnalysisControl analysis;auto result=InProcessAnalysisBackend{}.analyze(file,image,analysis,{},{});
        check(result.assembly.stopReason==StopReason::None,"native analysis completed");
        for(const char* symbol:{"_Z5somarii","_Z8fatoriali","_Z8eh_primoi","_Z11classificari"}) {
            const Symbol* found=nullptr;for(const auto& s:image.symbols)if(s.name==symbol)found=&s;
            check(found,"real C++ function symbol");
            const auto& graph=result.program.graphs.at(found->address);
            const auto detected=recoverControlFlowRegions(graph,analysis);
            auto semantic=recoverIntegerFunction(result.program.ir.at(found->address),analysis);
            if(std::string(symbol)=="_Z5somarii") {
                const auto& instructions=result.program.ir.at(found->address).instructions;
                const bool nonlinear=std::any_of(instructions.begin(),instructions.end(),[](const auto& i){return i.flow==FlowType::ConditionalJump || i.flow==FlowType::Call;});
                if(nonlinear) {
                    check(!semantic.recovered && !semantic.reason.empty(),"instrumented sum conservatively refused");
                    continue;
                }
                check(semantic.recovered,"somar projection retained");
                for(uint32_t a:{0u,1u,0x7fffffffu,0xffffffffu})for(uint32_t b:{0u,2u,0x80000000u})
                    check(semantic.evaluate({a,b})==uint32_t(a+b),"somar bitvector values");
            }else check(!semantic.recovered,"unsupported nonlinear value semantics remain explicit");
            std::cout<<symbol<<": blocks="<<graph.blocks.size()<<", regions="<<detected.size()<<", semantic="<<semantic.recovered<<'\n';
        }
    }
    std::cout<<"PASS conservative regions, rejection and real-symbol coverage\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

// SPDX-License-Identifier: GPL-3.0-or-later
#include "rea/control_flow_analysis.hpp"
#include <iostream>
#include <stdexcept>
using namespace rea;
void check(bool b){if(!b)throw std::runtime_error("flow assertion");}
ControlFlowGraph graph(){ControlFlowGraph g;g.entry=1;for(uint64_t a:{1,2,3,4}){Instruction i;i.address=a;i.size=1;i.bytes={0x90};g.blocks.push_back({a,{i},{}});}g.edges={{1,2,EdgeType::Conditional},{1,3,EdgeType::Fallthrough},{2,4,EdgeType::Jump},{3,4,EdgeType::Jump},{4,{},EdgeType::Return}};return g;}
int main(){try{auto g=graph();AnalysisControl c;auto f=analyzeControlFlow(g,c);check(f.dominators.at(4)==std::set<uint64_t>({1,4}));check(f.postDominators.at(1)==std::set<uint64_t>({1,4}));check(f.predecessors.at(4).size()==2 && f.loops.empty());
 g.edges.push_back({4,1,EdgeType::Conditional});auto loop=analyzeControlFlow(g,c);check(loop.loops.size()==1 && loop.loops[0].body.size()==4 && loop.postDominators.empty());
 auto malformed=graph();malformed.edges.push_back({99,1,EdgeType::Jump});bool rejected=false;try{analyzeControlFlow(malformed,c);}catch(const std::invalid_argument&){rejected=true;}check(rejected);
 auto unknown=graph();unknown.edges.push_back({2,{},EdgeType::Unknown});check(!analyzeControlFlow(unknown,c).closed);
 AnalysisControl stop;stop.cancel();rejected=false;try{analyzeControlFlow(graph(),stop);}catch(const AnalysisStopped&){rejected=true;}check(rejected);
 AnalysisLimits limits;limits.maxMemoryBytes=1;AnalysisControl limited(limits);rejected=false;try{analyzeControlFlow(graph(),limited);}catch(const AnalysisStopped&){rejected=true;}check(rejected);
 std::cout<<"PASS dominators, postdominators, joins, natural loops, unknown flow, malformed, cancel and limits\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

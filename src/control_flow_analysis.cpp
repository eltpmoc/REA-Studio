// SPDX-License-Identifier: GPL-3.0-or-later
#include "rea/control_flow_analysis.hpp"
#include <algorithm>
#include <iterator>
#include <stdexcept>
namespace rea {
ControlFlowFacts analyzeControlFlow(const ControlFlowGraph& graph,AnalysisControl& control){
    control.checkpoint();
    if(!CFGValidator::validate(graph).empty())throw std::invalid_argument("Malformed CFG");
    ControlFlowFacts f;f.closed=true;std::set<uint64_t> nodes;
    for(const auto& b:graph.blocks){control.work();nodes.insert(b.address);f.predecessors[b.address];f.successors[b.address];}
    if(nodes.empty())return f;
    if(nodes.size()>control.limits().maxCFGNodes || graph.edges.size()>control.limits().maxCFGEdges)throw AnalysisStopped(StopReason::WorkLimit);
    control.reserveMemory(uint64_t(nodes.size())*uint64_t(nodes.size())*128+graph.edges.size()*128);
    std::set<uint64_t> unknown;
    for(const auto& e:graph.edges){control.work();if(e.type==EdgeType::Unknown || (e.to && !nodes.contains(*e.to)))unknown.insert(e.from);
        else if(e.to){f.successors[e.from].insert(*e.to);f.predecessors[*e.to].insert(e.from);}
        else if(e.type==EdgeType::Return || e.type==EdgeType::Stop)f.exits.insert(e.from);
        else unknown.insert(e.from);
    }
    std::vector<uint64_t> todo{graph.entry};while(!todo.empty()){control.work();auto v=todo.back();todo.pop_back();if(!f.reachable.insert(v).second)continue;for(auto s:f.successors.at(v))todo.push_back(s);}
    for(auto n:f.reachable){if(unknown.contains(n))f.closed=false;f.dominators[n]=n==graph.entry?std::set<uint64_t>{n}:f.reachable;}
    auto intersect=[](const auto& a,const auto& b){std::set<uint64_t> r;std::set_intersection(a.begin(),a.end(),b.begin(),b.end(),std::inserter(r,r.end()));return r;};
    bool changed=true;while(changed){changed=false;for(auto n:f.reachable){control.work();if(n==graph.entry)continue;auto result=f.reachable;for(auto p:f.predecessors[n])if(f.reachable.contains(p)){control.work();result=intersect(result,f.dominators.at(p));}result.insert(n);if(result!=f.dominators[n]){f.dominators[n]=std::move(result);changed=true;}}}
    std::map<uint64_t,NaturalLoop> loops;
    for(auto n:f.reachable)for(auto s:f.successors[n]){control.work();if(!f.dominators.at(n).contains(s))continue;auto& loop=loops[s];loop.header=s;loop.latches.insert(n);loop.body.insert(s);std::vector<uint64_t> stack{n};while(!stack.empty()){control.work();auto v=stack.back();stack.pop_back();if(!loop.body.insert(v).second)continue;for(auto p:f.predecessors[v])if(f.reachable.contains(p))stack.push_back(p);}}
    for(auto& [h,l]:loops){(void)h;f.loops.push_back(std::move(l));}
    // Kahn detects all cycles, including irreducible cycles without natural backedges.
    std::map<uint64_t,size_t> degree;todo.clear();for(auto n:f.reachable){for(auto p:f.predecessors[n])if(f.reachable.contains(p))++degree[n];if(!degree[n])todo.push_back(n);}
    size_t removed=0;while(!todo.empty()){control.work();auto n=todo.back();todo.pop_back();++removed;for(auto s:f.successors[n])if(--degree[s]==0)todo.push_back(s);}
    if(f.closed && removed==f.reachable.size()){
        for(auto n:f.reachable){if(f.successors[n].empty() && !f.exits.contains(n)){f.closed=false;break;}f.postDominators[n]=f.successors[n].empty()?std::set<uint64_t>{n}:f.reachable;}
        if(!f.closed){f.postDominators.clear();return f;}
        changed=true;while(changed){changed=false;for(auto n:f.reachable){control.work();if(f.successors[n].empty())continue;auto result=f.reachable;for(auto s:f.successors[n]){control.work();result=intersect(result,f.postDominators.at(s));}result.insert(n);if(result!=f.postDominators[n]){f.postDominators[n]=std::move(result);changed=true;}}}
    }
    return f;
}
}

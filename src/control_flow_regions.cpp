// SPDX-License-Identifier: GPL-3.0-or-later
#include "rea/control_flow_analysis.hpp"
#include <algorithm>
namespace rea {
std::vector<ControlFlowRegion> recoverControlFlowRegions(const ControlFlowGraph& graph,AnalysisControl& control) {
    auto facts=analyzeControlFlow(graph,control);
    std::vector<ControlFlowRegion> regions;
    if(!facts.closed)return regions;
    std::map<uint64_t,std::pair<uint64_t,uint64_t>> conditions;
    for(const auto& block:graph.blocks) {
        control.work();
        if(block.instructions.empty() || block.instructions.back().flow!=FlowType::ConditionalJump)continue;
        std::optional<uint64_t> taken,fallthrough;size_t count=0;
        for(const auto& edge:graph.edges)if(edge.from==block.address) {
            control.work();++count;
            if(edge.type==EdgeType::Conditional)taken=edge.to;
            if(edge.type==EdgeType::Fallthrough)fallthrough=edge.to;
        }
        const auto& last=block.instructions.back();
        if(count==2 && taken && fallthrough && taken!=fallthrough && last.directTarget==taken &&
           last.address<=UINT64_MAX-last.size && last.address+last.size==*fallthrough)
            conditions.emplace(block.address,std::pair{*taken,*fallthrough});
    }
    // Acyclic, single-entry linear arms ending at the nearest common
    // postdominator. Nested or shared arms are deliberately left unclassified.
    for(const auto& [header,targets]:conditions) {
        control.work();
        if(!facts.postDominators.contains(header))continue;
        std::optional<uint64_t> join;
        for(auto candidate:facts.postDominators.at(header)) {
            control.work();if(candidate==header)continue;
            bool nearest=true;
            for(auto other:facts.postDominators.at(header)) {
                control.work();if(other!=header && other!=candidate && !facts.postDominators.at(candidate).contains(other))nearest=false;
            }
            if(nearest){join=candidate;break;}
        }
        if(!join)continue;
        std::set<uint64_t> body{header};
        auto arm=[&](uint64_t node) {
            while(node!=*join) {
                control.work();
                if(!body.insert(node).second || facts.predecessors.at(node).size()!=1 ||
                   facts.successors.at(node).size()!=1 || facts.exits.contains(node))return false;
                node=*facts.successors.at(node).begin();
            }
            return true;
        };
        if(arm(targets.first) && arm(targets.second)) {
            control.reserveMemory(128+body.size()*64);
            regions.push_back({RegionKind::IfElse,header,header,targets.first,targets.second,*join,std::move(body)});
        }
    }
    // Natural single-latch loops, exactly one conditional exit, no side
    // entries or exits. Interior branching/early returns remain unsupported.
    for(const auto& loop:facts.loops) {
        control.work();if(loop.latches.size()!=1)continue;
        const auto latch=*loop.latches.begin();
        for(auto condition:{loop.header,latch}) {
            control.work();if(!conditions.contains(condition))continue;
            auto [taken,fallthrough]=conditions.at(condition);
            const bool takenInside=loop.body.contains(taken),fallInside=loop.body.contains(fallthrough);
            if(takenInside==fallInside)continue;
            const auto exit=takenInside?fallthrough:taken;
            const auto inside=takenInside?taken:fallthrough;
            const bool pretest=condition==loop.header && latch!=loop.header;
            if(!pretest && inside!=loop.header)continue;
            bool valid=true;
            for(auto node:loop.body) {
                control.work();
                for(auto pred:facts.predecessors.at(node)) {
                    control.work();if(node!=loop.header && !loop.body.contains(pred))valid=false;
                }
                if(facts.exits.contains(node))valid=false;
                const auto& successors=facts.successors.at(node);
                if(node!=condition && successors.size()!=1)valid=false;
                for(auto next:successors) {
                    control.work();if(!loop.body.contains(next) && (node!=condition || next!=exit))valid=false;
                }
            }
            if(valid) {
                control.reserveMemory(128+loop.body.size()*64);
                regions.push_back({pretest?RegionKind::While:RegionKind::DoWhile,loop.header,condition,taken,fallthrough,exit,loop.body});
                break;
            }
        }
    }
    control.checkpoint();return regions;
}
}

// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "rea/function_analysis.hpp"
namespace rea {
struct NaturalLoop {uint64_t header{};std::set<uint64_t> latches,body;};
struct ControlFlowFacts {
    std::map<uint64_t,std::set<uint64_t>> predecessors,successors,dominators,postDominators;
    std::vector<NaturalLoop> loops;
    std::set<uint64_t> reachable,exits;
    bool closed{};
};
// Postdominators are emitted only for closed, acyclic reachable graphs.
// Cycles require a termination policy; absence of a set is not a proof.
ControlFlowFacts analyzeControlFlow(const ControlFlowGraph&,AnalysisControl&);
}

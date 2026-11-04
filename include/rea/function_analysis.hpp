// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "rea/analysis.hpp"
#include <set>
namespace rea {
enum class FunctionConfidence { Confirmed, Probable, Unknown };
const char* functionConfidenceName(FunctionConfidence);
struct FunctionDescriptor {
    uint64_t entry{}, end{}, declaredEnd{};
    std::string name, evidence;
    FunctionConfidence confidence{FunctionConfidence::Unknown};
    std::vector<uint64_t> instructions;
    bool overlap{}, hasReturn{};
    size_t duplicateEntries{};
};
struct FunctionRegistry {
    std::vector<FunctionDescriptor> functions;
    const FunctionDescriptor* at(uint64_t entry) const;
    const FunctionDescriptor* containing(uint64_t address) const;
};
class FunctionAnalyzer {
public:
    FunctionRegistry analyze(const Image&,uint64_t fileSize,const AnalysisResult&,AnalysisControl&) const;
};
enum class EdgeType { Fallthrough, Conditional, Jump, Return, Unknown, Stop };
struct ControlFlowEdge { uint64_t from{}; std::optional<uint64_t> to; EdgeType type{}; };
struct ControlFlowGraph {
    uint64_t entry{};
    std::vector<BasicBlock> blocks;
    std::vector<ControlFlowEdge> edges;
    bool cyclic{};
};
struct GraphBudget { size_t nodes{}, edges{}; size_t maxNodes{4096}, maxEdges{16384}; };
class CFGBuilder {
public:
    ControlFlowGraph build(const FunctionDescriptor&,const AnalysisResult&,GraphBudget&,AnalysisControl&) const;
};
class CFGValidator {
public:
    static std::vector<std::string> validate(const ControlFlowGraph&);
};
class ReferenceIndex {
public:
    void build(const AnalysisResult&,AnalysisControl&);
    const std::vector<Reference>& all() const {return references_;}
    const std::vector<size_t>& incoming(uint64_t) const;
    const std::vector<size_t>& outgoing(uint64_t) const;
private:
    std::vector<Reference> references_;
    std::map<uint64_t,std::vector<size_t>> incoming_,outgoing_;
};
struct ProgramAnalysis {
    FunctionRegistry registry;
    std::map<uint64_t,ControlFlowGraph> graphs;
    ReferenceIndex references;
};
ProgramAnalysis analyzeProgram(const Image&,uint64_t,const AnalysisResult&,AnalysisControl&);
}

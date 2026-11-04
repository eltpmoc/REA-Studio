// SPDX-License-Identifier: GPL-3.0-or-later
#include "rea/function_analysis.hpp"
#include <algorithm>
#include <deque>
#include <limits>
namespace rea {
const char* functionConfidenceName(FunctionConfidence c) {
    switch(c) {case FunctionConfidence::Confirmed:return "confirmed";case FunctionConfidence::Probable:return "probable";default:return "unknown";}
}
const FunctionDescriptor* FunctionRegistry::at(uint64_t va) const {
    auto i=std::lower_bound(functions.begin(),functions.end(),va,[](const auto& f,uint64_t a){return f.entry<a;});
    return i!=functions.end() && i->entry==va ? &*i : nullptr;
}
const FunctionDescriptor* FunctionRegistry::containing(uint64_t va) const {
    for(const auto& f:functions) if(std::binary_search(f.instructions.begin(),f.instructions.end(),va)) return &f;
    return nullptr;
}
FunctionRegistry FunctionAnalyzer::analyze(const Image& image,uint64_t fileSize,const AnalysisResult& result,AnalysisControl& control) const {
    AddressTranslator translator(image,fileSize);
    std::map<uint64_t,const Instruction*> code;
    for(const auto& i:result.instructions) {control.work();control.reserveMemory(96);if(i.size && i.address<=UINT64_MAX-i.size && i.flow!=FlowType::Invalid) code.emplace(i.address,&i);}
    std::map<uint64_t,FunctionDescriptor> seeds;
    auto seed=[&](uint64_t va,std::string name,FunctionConfidence confidence,std::string evidence,uint64_t size=0){
        control.work();auto m=translator.mapping(va);if(!m || !m->executable) return;
        control.reserveMemory(512+name.size()+evidence.size());
        auto [it,inserted]=seeds.try_emplace(va);auto& f=it->second;
        if(!inserted) {++f.duplicateEntries;if(static_cast<int>(confidence)>=static_cast<int>(f.confidence)) return;}
        f.entry=va;f.name=std::move(name);f.evidence=std::move(evidence);f.confidence=confidence;
        f.end=size && size<=UINT64_MAX-va ? va+size : va;f.declaredEnd=f.end;
    };
    for(const auto& s:image.symbols) {control.work();if((s.info&15)==2 && s.section && s.section<image.sections.size()) seed(s.address,s.name,FunctionConfidence::Confirmed,"Defined ELF STT_FUNC",s.size);}
    seed(image.entry,"entry",FunctionConfidence::Confirmed,"ELF entry point");
    for(const auto& i:result.instructions) {control.work();if(i.flow==FlowType::Call && i.directTarget) seed(*i.directTarget,"sub_"+std::to_string(*i.directTarget),code.contains(*i.directTarget) ? FunctionConfidence::Probable : FunctionConfidence::Unknown,"Direct call target");}
    FunctionRegistry registry;
    std::map<uint64_t,size_t> owners;
    for(auto& [entry,f]:seeds) {
        control.work();const uint64_t declaredEnd=f.end;f.end=entry;
        if(!code.contains(entry)) f.confidence=FunctionConfidence::Unknown;
        std::deque<uint64_t> pending{entry};std::set<uint64_t> visited;
        while(!pending.empty()) {
            control.work();auto va=pending.front();pending.pop_front();
            if(visited.contains(va) || (va!=entry && seeds.contains(va)) || (declaredEnd>entry && (va<entry || va>=declaredEnd))) continue;
            auto it=code.find(va);if(it==code.end()) continue;
            auto m=translator.mapping(va);const auto& i=*it->second;
            if(!m || !m->executable || i.size>m->available || (declaredEnd>entry && i.size>declaredEnd-va)) continue;
            control.reserveMemory(256);visited.insert(va);f.instructions.push_back(va);f.end=std::max(f.end,va+i.size);
            if(auto owner=owners.find(va);owner!=owners.end()) {f.overlap=true;registry.functions[owner->second].overlap=true;}
            else owners.emplace(va,registry.functions.size());
            if(i.flow==FlowType::Return) f.hasReturn=true;
            if((i.flow==FlowType::Jump || i.flow==FlowType::ConditionalJump) && i.directTarget) pending.push_back(*i.directTarget);
            if(i.flow==FlowType::Next || i.flow==FlowType::Call || i.flow==FlowType::ConditionalJump) pending.push_back(va+i.size);
        }
        std::sort(f.instructions.begin(),f.instructions.end());registry.functions.push_back(std::move(f));
    }
    // Envelope intersections are evidence of overlapping regions, even with disjoint decoded membership.
    size_t furthest=0;
    for(size_t n=0;n<registry.functions.size();++n) {
        control.work();auto& f=registry.functions[n];
        if(n && f.entry<std::max(registry.functions[furthest].end,registry.functions[furthest].declaredEnd)) {f.overlap=true;registry.functions[furthest].overlap=true;}
        if(std::max(f.end,f.declaredEnd)>std::max(registry.functions[furthest].end,registry.functions[furthest].declaredEnd)) furthest=n;
    }
    return registry;
}
ControlFlowGraph CFGBuilder::build(const FunctionDescriptor& f,const AnalysisResult& result,GraphBudget& budget,AnalysisControl& control) const {
    ControlFlowGraph graph;graph.entry=f.entry;
    std::map<uint64_t,const Instruction*> code;
    for(const auto& i:result.instructions) {control.work();if(std::binary_search(f.instructions.begin(),f.instructions.end(),i.address)) {control.reserveMemory(96);code.emplace(i.address,&i);}}
    std::set<uint64_t> leaders{f.entry};
    for(const auto& [va,p]:code) {
        control.work();const auto& i=*p;
        if(i.flow==FlowType::Jump || i.flow==FlowType::ConditionalJump) {if(i.directTarget && code.contains(*i.directTarget)) leaders.insert(*i.directTarget);}
        if(i.flow!=FlowType::Next && code.contains(va+i.size)) leaders.insert(va+i.size);
    }
    std::map<uint64_t,uint64_t> blockFor;
    for(const auto& [va,p]:code) {
        control.work();
        if(graph.blocks.empty() || leaders.contains(va) || graph.blocks.back().instructions.back().address+graph.blocks.back().instructions.back().size!=va) {
            if(budget.nodes>=budget.maxNodes) throw AnalysisStopped(StopReason::WorkLimit);
            control.reserveMemory(512);++budget.nodes;graph.blocks.push_back({va,{},{}});
        }
        control.reserveMemory(2*sizeof(Instruction)+512+2*(p->mnemonic.size()+p->operands.size()+p->evidence.size()+p->bytes.size()+p->dataReferences.size()*sizeof(uint64_t)));
        graph.blocks.back().instructions.push_back(*p);blockFor[va]=graph.blocks.back().address;
        control.report(97,"CFG blocks");
    }
    auto edge=[&](BasicBlock& b,std::optional<uint64_t> target,EdgeType type){
        control.work();if(budget.edges>=budget.maxEdges) throw AnalysisStopped(StopReason::WorkLimit);
        control.reserveMemory(128);++budget.edges;
        if(target && blockFor.contains(*target)) {target=blockFor.at(*target);b.successors.push_back(*target);}
        else if(target) type=EdgeType::Unknown;
        graph.edges.push_back({b.address,target,type});
    };
    for(auto& b:graph.blocks) {
        control.work();const auto& i=b.instructions.back();
        if(i.flow==FlowType::Return) edge(b,{},EdgeType::Return);
        else if(i.flow==FlowType::Stop || i.flow==FlowType::Invalid) edge(b,{},EdgeType::Stop);
        else {
            if(i.flow==FlowType::Jump || i.flow==FlowType::ConditionalJump) edge(b,i.directTarget,i.directTarget ? (i.flow==FlowType::Jump ? EdgeType::Jump : EdgeType::Conditional) : EdgeType::Unknown);
            if(i.flow!=FlowType::Jump) edge(b,i.address+i.size,EdgeType::Fallthrough);
        }
    }
    // Kahn's algorithm avoids recursion on hostile deep graphs.
    std::map<uint64_t,size_t> indegree;std::map<uint64_t,std::vector<uint64_t>> adjacency;
    for(const auto& b:graph.blocks) indegree[b.address]=0;
    for(const auto& e:graph.edges) {control.work();if(e.to && indegree.contains(*e.to)) {++indegree[*e.to];adjacency[e.from].push_back(*e.to);}}
    std::deque<uint64_t> ready;for(auto [a,n]:indegree) if(!n) ready.push_back(a);
    size_t count=0;while(!ready.empty()) {control.work();auto a=ready.front();ready.pop_front();++count;for(auto t:adjacency[a]) if(--indegree[t]==0) ready.push_back(t);}
    graph.cyclic=count!=graph.blocks.size();return graph;
}
std::vector<std::string> CFGValidator::validate(const ControlFlowGraph& g) {
    std::vector<std::string> errors;std::set<uint64_t> nodes,instructions;
    uint64_t previous=0;bool first=true;
    for(const auto& b:g.blocks) {
        if(!nodes.insert(b.address).second || (!first && b.address<=previous)) errors.push_back("Duplicate or unordered block");
        previous=b.address;first=false;
        if(b.instructions.empty() || b.instructions.front().address!=b.address) errors.push_back("Invalid block entry");
        uint64_t next=b.address;
        for(const auto& i:b.instructions) {
            if(!i.size || i.address>UINT64_MAX-i.size || i.address!=next || !instructions.insert(i.address).second) errors.push_back("Invalid instruction range");
            next=i.address+i.size;
            if(&i!=&b.instructions.back() && i.flow!=FlowType::Next && i.flow!=FlowType::Call) errors.push_back("Terminator inside block");
        }
    }
    if(!g.blocks.empty() && !nodes.contains(g.entry)) errors.push_back("Missing entry");
    for(const auto& e:g.edges) {
        if(!nodes.contains(e.from)) errors.push_back("Missing edge source");
        if(e.type!=EdgeType::Unknown && e.to && !nodes.contains(*e.to)) errors.push_back("Missing edge destination");
        if((e.type==EdgeType::Fallthrough || e.type==EdgeType::Conditional || e.type==EdgeType::Jump) && !e.to) errors.push_back("Missing known target");
    }
    return errors;
}
void ReferenceIndex::build(const AnalysisResult& result,AnalysisControl& control) {
    references_.clear();incoming_.clear();outgoing_.clear();
    for(const auto& i:result.instructions) {
        control.work();if(i.flow==FlowType::Invalid) continue;
        auto add=[&](uint64_t to,ReferenceType type,bool flow){control.work();control.reserveMemory(256);references_.push_back({i.address,to,flow,type});};
        if(i.directTarget && (i.flow==FlowType::Call || i.flow==FlowType::Jump || i.flow==FlowType::ConditionalJump)) add(*i.directTarget,i.flow==FlowType::Call ? ReferenceType::DirectCall : ReferenceType::DirectJump,true);
        for(auto to:i.dataReferences) {
            auto target=std::lower_bound(result.instructions.begin(),result.instructions.end(),to,[](const Instruction& ins,uint64_t a){return ins.address<a;});
            add(to,target!=result.instructions.end() && target->address==to && target->flow!=FlowType::Invalid ? ReferenceType::CodeAddress : ReferenceType::DataAddress,false);
        }
    }
    std::sort(references_.begin(),references_.end(),[](auto a,auto b){return std::tie(a.from,a.to,a.type)<std::tie(b.from,b.to,b.type);});
    references_.erase(std::unique(references_.begin(),references_.end(),[](auto a,auto b){return a.from==b.from && a.to==b.to && a.type==b.type;}),references_.end());
    for(size_t n=0;n<references_.size();++n) {control.work();incoming_[references_[n].to].push_back(n);outgoing_[references_[n].from].push_back(n);}
}
const std::vector<size_t>& ReferenceIndex::incoming(uint64_t a) const {static const std::vector<size_t> empty;auto i=incoming_.find(a);return i==incoming_.end()?empty:i->second;}
const std::vector<size_t>& ReferenceIndex::outgoing(uint64_t a) const {static const std::vector<size_t> empty;auto i=outgoing_.find(a);return i==outgoing_.end()?empty:i->second;}
ProgramAnalysis analyzeProgram(const Image& image,uint64_t size,const AnalysisResult& assembly,AnalysisControl& control) {
    ProgramAnalysis p;control.report(95,"Function analysis",true);
    p.registry=FunctionAnalyzer{}.analyze(image,size,assembly,control);p.references.build(assembly,control);
    GraphBudget budget;budget.maxNodes=control.limits().maxCFGNodes;budget.maxEdges=control.limits().maxCFGEdges;
    for(const auto& f:p.registry.functions) {control.work();p.graphs.emplace(f.entry,CFGBuilder{}.build(f,assembly,budget,control));}
    control.checkpoint();return p;
}
}

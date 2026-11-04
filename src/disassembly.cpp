// SPDX-License-Identifier: GPL-3.0-or-later
#include "rea/disassembly.hpp"
#include <algorithm>
#include <deque>
#include <set>
namespace rea {
AnalysisResult DisassemblyEngine::analyze(BinaryFile& file,const Image& image,std::optional<uint64_t> requested,size_t maximum) const {
    AnalysisLimits limits; limits.maxInstructions=std::min<size_t>(maximum,20000);
    AnalysisControl control(limits);
    return analyze(file,image,control,requested);
}
AnalysisResult DisassemblyEngine::analyze(BinaryFile& file,const Image& image,AnalysisControl& control,std::optional<uint64_t> requested) const {
    AnalysisResult result;
    struct Seed { uint64_t va; Confidence confidence; std::string reason; };
    std::deque<Seed> pending;
    std::map<uint64_t,std::string> metadataSeeds;
    std::map<uint64_t,Instruction> decoded;
    std::set<uint64_t> visited,scheduled;
    size_t conflicts=0;
    try {
        control.checkpoint(); control.reserveMemory(8192);
        if(image.bits!=64 || image.machine!=62 || !image.littleEndian || (image.type!=2 && image.type!=3)) {
            result.diagnostics.push_back("Disassembly supports little-endian x86-64 ELF ET_EXEC/ET_DYN only."); return result;
        }
        if(image.segments.size()>256) {
            result.diagnostics.push_back("Disassembly refused: more than 256 program headers exceeds the analysis resource budget."); return result;
        }
        AddressTranslator translator(image,file.size());
        auto enqueue=[&](uint64_t va,Confidence confidence,const std::string& reason,bool front=false){
            control.work();
            if(scheduled.contains(va)) return;
            control.reserveMemory(512+2*static_cast<uint64_t>(reason.size()));
            scheduled.insert(va);
            if(front) pending.push_front({va,confidence,reason}); else pending.push_back({va,confidence,reason});
        };
        control.reserveMemory(256); metadataSeeds.emplace(image.entry,"ELF entry point");
        enqueue(image.entry,Confidence::Confirmed,"ELF entry point");
        for(const auto& s:image.symbols) {
            control.work();
            if((s.info&15)!=2 || !s.section || s.section>=image.sections.size() || metadataSeeds.contains(s.address)) continue;
            control.reserveMemory(512+4*static_cast<uint64_t>(s.name.size()));
            auto reason="Defined STT_FUNC symbol: "+s.name;
            metadataSeeds.emplace(s.address,reason); enqueue(s.address,Confidence::Confirmed,reason);
        }
        if(requested) {
            // Explicit requests take precedence even when they coincide with a metadata seed.
            control.reserveMemory(512);
            pending.push_front({*requested,Confidence::Probable,"User requested decode; code not confirmed"});
        }
        while(!pending.empty()) {
            control.work();
            auto seed=std::move(pending.front()); pending.pop_front();
            uint64_t va=seed.va;
            while(true) {
                control.work();
                if(visited.contains(va)) break;
                control.reserveMemory(128); visited.insert(va);
                auto mapping=translator.mapping(va);
                if(!mapping || !mapping->executable) break;
                auto upper=decoded.upper_bound(va);
                if(upper!=decoded.begin()) {
                    const auto& previous=std::prev(upper)->second;
                    if(va-previous.address<previous.size) {++conflicts;break;}
                }
                auto count=std::min<uint64_t>(15,mapping->available);
                uint64_t safe=0;
                for(;safe<count;++safe) {
                    control.checkpoint();
                    if(va>UINT64_MAX-safe) break;
                    auto m=translator.mapping(va+safe);
                    if(!m || !m->executable || m->offset!=mapping->offset+safe) break;
                }
                if(!safe) break;
                // Reservation includes map/vector nodes, final result, references and decoder scratch.
                control.reserveMemory(4096+2*static_cast<uint64_t>(seed.reason.size()));
                control.claimInstruction();
                auto bytes=file.read(mapping->offset,safe);
                auto ins=decoder_.decode(bytes,va,mapping->offset);
                control.checkpoint();
                if(!ins.size || ins.size>safe || ins.bytes.size()!=ins.size || va>UINT64_MAX-ins.size)
                    throw std::runtime_error("Decoder returned an invalid instruction range");
                control.reserveMemory(2*static_cast<uint64_t>(ins.operands.size()+ins.mnemonic.size()+ins.evidence.size()+ins.dataReferences.size()*sizeof(uint64_t)));
                auto next=decoded.lower_bound(va);
                if(next!=decoded.end() && next->first-va<ins.size) {++conflicts;break;}
                ins.confidence=va==seed.va ? seed.confidence : Confidence::Probable;
                ins.evidence=va==seed.va ? seed.reason : "Fallthrough from a decoded instruction";
                if(ins.flow==FlowType::Invalid) {ins.confidence=Confidence::Unanalyzed;ins.evidence="Invalid or truncated encoding; traversal stopped";}
                if(auto evidence=metadataSeeds.find(va);evidence!=metadataSeeds.end() && ins.flow!=FlowType::Invalid) {
                    ins.confidence=Confidence::Confirmed;ins.evidence=evidence->second;
                }
                auto flow=ins.flow; auto target=ins.directTarget; auto size=ins.size;
                // Store the instruction before enqueue can hit a resource boundary.
                decoded.emplace(va,std::move(ins));
                if(target) {
                    result.references.push_back({va,*target,true});
                    enqueue(*target,Confidence::Probable,flow==FlowType::Call ? "Direct call target" : "Direct branch target");
                }
                for(auto reference:decoded.at(va).dataReferences) result.references.push_back({va,reference,false});
                const auto maximum=std::max<uint64_t>(control.limits().maxInstructions,1);
                control.report(35+static_cast<int>(60*std::min(control.instructions(),maximum)/maximum),"Disassembly");
                if(flow==FlowType::Invalid || flow==FlowType::Return || flow==FlowType::Stop || flow==FlowType::Jump) break;
                va+=size;
            }
        }
        control.checkpoint();
    } catch(const AnalysisStopped& stopped) {
        result.stopReason=stopped.reason(); result.limited=stopped.reason()!=StopReason::Cancelled;
        result.diagnostics.emplace_back(stopReasonName(stopped.reason()));
    }
    if(conflicts) result.diagnostics.push_back("Conflicting instruction boundaries rejected: "+std::to_string(conflicts));
    // Memory for the final vector was reserved along with each map node.
    result.instructions.reserve(decoded.size());
    for(auto& [va,ins]:decoded) { (void)va; result.instructions.push_back(std::move(ins)); }
    return result;
}
}

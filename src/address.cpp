// SPDX-License-Identifier: GPL-3.0-or-later
#include "rea/disassembly.hpp"
#include <algorithm>
namespace rea {
AddressTranslator::AddressTranslator(const Image& im,uint64_t fileSize,uint64_t bias)
    : image_(im),fileSize_(fileSize),bias_(bias),valid_((im.type==2 || im.type==3) && (im.type==3 || bias==0)) {
    for(const auto& s:im.segments) if(s.type==1 &&
        (s.fileSize>s.memorySize || s.address>UINT64_MAX-s.memorySize || s.offset>fileSize || s.fileSize>fileSize-s.offset)) valid_=false;
}
std::optional<AddressMapping> AddressTranslator::mapping(uint64_t va) const {
    if(!valid_) return std::nullopt;
    std::optional<AddressMapping> result;
    for(const auto& s:image_.segments) {
        if(s.type!=1 || va<s.address || va-s.address>=s.fileSize) continue;
        auto delta=va-s.address;
        AddressMapping candidate{s.offset+delta,s.fileSize-delta,(s.flags&1)!=0};
        if(result) {
            if(result->offset!=candidate.offset) return std::nullopt;
            result->available=std::min(result->available,candidate.available);
            result->executable=result->executable && candidate.executable;
        } else result=candidate;
    }
    return result;
}
std::optional<uint64_t> AddressTranslator::toFile(uint64_t va) const {
    if(auto m=mapping(va)) return m->offset;
    return std::nullopt;
}
std::optional<uint64_t> AddressTranslator::toVA(uint64_t offset) const {
    if(!valid_ || offset>=fileSize_) return std::nullopt;
    std::optional<uint64_t> result;
    for(const auto& s:image_.segments) {
        if(s.type!=1 || offset<s.offset || offset-s.offset>=s.fileSize) continue;
        auto candidate=s.address+(offset-s.offset);
        if(result && *result!=candidate) return std::nullopt;
        result=candidate;
    }
    if(result && toFile(*result)!=offset) return std::nullopt;
    return result;
}
std::optional<uint64_t> AddressTranslator::toRuntime(uint64_t va) const {
    if(!valid_ || va>UINT64_MAX-bias_) return std::nullopt;
    return va+bias_;
}
std::optional<uint64_t> AddressTranslator::fromRuntime(uint64_t runtime) const {
    if(!valid_ || runtime<bias_) return std::nullopt;
    return runtime-bias_;
}
SymbolResolver::SymbolResolver(const Image& image) {
    for(const auto& s:image.symbols) {
        if(!s.name.empty() && s.section && s.section<image.sections.size()) {
            auto [it,inserted]=names_.emplace(s.address,s.name);
            if(!inserted && it->second!=s.name && it->second.size()+s.name.size()+3<=4096) it->second += " / "+s.name;
        }
    }
}
std::string SymbolResolver::at(uint64_t va) const {
    auto it=names_.find(va); return it==names_.end() ? std::string{} : it->second;
}
const char* flowName(FlowType flow) {
    switch(flow) {
    case FlowType::Next:return "Next"; case FlowType::Call:return "Call";
    case FlowType::ConditionalJump:return "Conditional jump"; case FlowType::Jump:return "Jump";
    case FlowType::Return:return "Return"; case FlowType::Stop:return "Stop"; default:return "Invalid";
    }
}
const char* confidenceName(Confidence c) {
    switch(c) {case Confidence::Confirmed:return "Confirmed (ELF metadata)";
    case Confidence::Probable:return "Probable (static hypothesis)"; default:return "Unanalyzed";}
}
}

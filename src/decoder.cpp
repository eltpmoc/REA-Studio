// SPDX-License-Identifier: GPL-3.0-or-later
#include "rea/disassembly.hpp"
#include <Zydis/Zydis.h>
#include <algorithm>
#include <stdexcept>
namespace rea {
namespace {
std::optional<uint64_t> relativeTarget(uint64_t va,uint8_t length,int64_t displacement) {
    if(va>UINT64_MAX-length) return std::nullopt;
    uint64_t next=va+length;
    if(displacement>=0) {
        auto d=static_cast<uint64_t>(displacement);
        if(next>UINT64_MAX-d) return std::nullopt;
        return next+d;
    }
    uint64_t magnitude=uint64_t(-(displacement+1))+1;
    if(next<magnitude) return std::nullopt;
    return next-magnitude;
}
}
Instruction X86InstructionDecoder::decode(std::span<const uint8_t> bytes,uint64_t va,uint64_t offset) const {
    Instruction out; out.address=va; out.offset=offset; out.mnemonic="invalid";
    if(bytes.empty()) return out;
    // On invalid/truncated input retain one byte for inspection; traversal stops.
    out.bytes={bytes.front()}; out.size=1;
    ZydisDecoder decoder;
    if(!ZYAN_SUCCESS(ZydisDecoderInit(&decoder,ZYDIS_MACHINE_MODE_LONG_64,ZYDIS_STACK_WIDTH_64)))
        throw std::runtime_error("Cannot initialize Zydis");
    ZydisDecodedInstruction ins{}; ZydisDecodedOperand ops[ZYDIS_MAX_OPERAND_COUNT]{};
    if(!ZYAN_SUCCESS(ZydisDecoderDecodeFull(&decoder,bytes.data(),std::min<size_t>(bytes.size(),15),&ins,ops))) return out;
    if(va>UINT64_MAX-ins.length || offset>UINT64_MAX-ins.length) return out;
    out.size=ins.length; out.bytes.assign(bytes.begin(),bytes.begin()+ins.length);
    out.mnemonic=ZydisMnemonicGetString(ins.mnemonic); out.flow=FlowType::Next;
    switch(ins.meta.category) {
    case ZYDIS_CATEGORY_CALL:out.flow=FlowType::Call;break;
    case ZYDIS_CATEGORY_COND_BR:out.flow=FlowType::ConditionalJump;break;
    case ZYDIS_CATEGORY_UNCOND_BR:out.flow=FlowType::Jump;break;
    case ZYDIS_CATEGORY_RET:out.flow=FlowType::Return;break;
    case ZYDIS_CATEGORY_INTERRUPT:case ZYDIS_CATEGORY_SYSCALL:case ZYDIS_CATEGORY_SYSRET:out.flow=FlowType::Stop;break;
    default:break;
    }
    if(ins.mnemonic==ZYDIS_MNEMONIC_HLT || ins.mnemonic==ZYDIS_MNEMONIC_UD2) out.flow=FlowType::Stop;
    ZydisFormatter formatter;
    if(!ZYAN_SUCCESS(ZydisFormatterInit(&formatter,ZYDIS_FORMATTER_STYLE_INTEL))) throw std::runtime_error("Cannot initialize formatter");
    for(uint8_t i=0;i<ins.operand_count_visible;++i) {
        char text[256]{};
        if(!ZYAN_SUCCESS(ZydisFormatterFormatOperand(&formatter,&ins,&ops[i],text,sizeof(text),va,nullptr)))
            throw std::runtime_error("Cannot format operand");
        if(i) out.operands+=", ";
        out.operands+=text;
        if(ops[i].type==ZYDIS_OPERAND_TYPE_IMMEDIATE && ops[i].imm.is_relative &&
            (out.flow==FlowType::Call || out.flow==FlowType::Jump || out.flow==FlowType::ConditionalJump))
            out.directTarget=relativeTarget(va,ins.length,ops[i].imm.value.s);
        if(ops[i].type==ZYDIS_OPERAND_TYPE_MEMORY && ops[i].mem.base==ZYDIS_REGISTER_RIP)
            if(auto target=relativeTarget(va,ins.length,ops[i].mem.disp.value)) out.dataReferences.push_back(*target);
    }
    return out;
}
}

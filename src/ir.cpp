// SPDX-License-Identifier: GPL-3.0-or-later
#include "rea/ir.hpp"
#include "rea/semantic_ir.hpp"
#include <Zydis/Zydis.h>
#include <sstream>
#include <algorithm>
namespace rea {
namespace {
std::string hex(uint64_t n){std::ostringstream out;out<<"0x"<<std::hex<<n;return out.str();}
std::string name(ZydisRegister reg){const auto* value=ZydisRegisterGetString(reg);return value?value:"none";}
std::string operand(const IROperand& o){
    switch(o.kind){case IROperandKind::Register:return o.reg+":"+std::to_string(o.bits);
    case IROperandKind::Immediate:return hex(o.immediate)+":"+std::to_string(o.bits)+(o.signedImmediate?":signed":"")+(o.relative?":relative":"");
    case IROperandKind::Memory:return "mem"+std::to_string(o.bits)+"["+o.segment+":"+o.base+"+"+o.index+"*"+std::to_string(o.scale)+"+"+std::to_string(o.displacement)+"]";
    default:return "unmodeled_operand";}
}
std::string value(const IROperand& o){return o.kind==IROperandKind::Register?"read_gpr(cpu, \""+o.reg+"\", "+std::to_string(o.bits)+")":hex(o.immediate)+"ULL";}
std::string writeReg(const IROperand& d,const std::string& expr){
    // Only 32/64-bit GPRs enter the lowered subset. x86-64 writes to r32 zero upper bits.
    return "write_gpr(cpu, \""+d.reg+"\", "+std::to_string(d.bits)+", trunc"+std::to_string(d.bits)+"("+expr+"));";
}
}
IRInstruction X86IRLifter::lift(const Instruction& source,AnalysisControl& control) const {
    control.work();control.reserveMemory(2048+source.bytes.size());IRInstruction out;
    out.address=source.address;out.size=source.size;out.flow=source.flow;out.directTarget=source.directTarget;out.bytes=source.bytes;out.mnemonic=source.mnemonic;
    ZydisDecoder decoder;ZydisDecodedInstruction decoded{};ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT]{};
    if(!ZYAN_SUCCESS(ZydisDecoderInit(&decoder,ZYDIS_MACHINE_MODE_LONG_64,ZYDIS_STACK_WIDTH_64)) || source.bytes.empty() || source.bytes.size()!=source.size || source.bytes.size()>15 || source.address>UINT64_MAX-source.size || !ZYAN_SUCCESS(ZydisDecoderDecodeFull(&decoder,source.bytes.data(),source.bytes.size(),&decoded,operands)) || decoded.length!=source.size) {out.flow=FlowType::Invalid;out.directTarget.reset();return out;}
    out.flow=FlowType::Next;out.directTarget.reset();
    switch(decoded.meta.category){case ZYDIS_CATEGORY_CALL:out.flow=FlowType::Call;break;case ZYDIS_CATEGORY_COND_BR:out.flow=FlowType::ConditionalJump;break;case ZYDIS_CATEGORY_UNCOND_BR:out.flow=FlowType::Jump;break;case ZYDIS_CATEGORY_RET:out.flow=FlowType::Return;break;case ZYDIS_CATEGORY_INTERRUPT:case ZYDIS_CATEGORY_SYSCALL:case ZYDIS_CATEGORY_SYSRET:out.flow=FlowType::Stop;break;default:break;}
    if(decoded.mnemonic==ZYDIS_MNEMONIC_HLT || decoded.mnemonic==ZYDIS_MNEMONIC_UD2)out.flow=FlowType::Stop;
    for(uint8_t n=0;n<decoded.operand_count_visible;++n) if(operands[n].type==ZYDIS_OPERAND_TYPE_IMMEDIATE && operands[n].imm.is_relative){
        const uint64_t next=source.address+source.size;const int64_t d=operands[n].imm.value.s;
        if(d>=0){if(static_cast<uint64_t>(d)<=UINT64_MAX-next)out.directTarget=next+static_cast<uint64_t>(d);}
        else {const auto magnitude=uint64_t(-(d+1))+1;if(magnitude<=next)out.directTarget=next-magnitude;}
    }
    out.mnemonic=ZydisMnemonicGetString(decoded.mnemonic);
    out.addressBits=decoded.address_width;out.semanticSafe=!(decoded.attributes & (ZYDIS_ATTRIB_HAS_LOCK | ZYDIS_ATTRIB_HAS_REP | ZYDIS_ATTRIB_HAS_REPE | ZYDIS_ATTRIB_HAS_REPNE));
    if(decoded.cpu_flags){out.flagsCleared=decoded.cpu_flags->set_0;out.flagsSet=decoded.cpu_flags->set_1;out.flagsRead=decoded.cpu_flags->tested;out.flagsWritten=decoded.cpu_flags->modified|decoded.cpu_flags->set_0|decoded.cpu_flags->set_1;out.flagsUndefined=decoded.cpu_flags->undefined;}
    for(uint8_t n=0;n<decoded.operand_count;++n){
        control.work();const auto& in=operands[n];IROperand o;o.bits=in.size;o.access=static_cast<uint8_t>(in.actions);
        switch(in.type){case ZYDIS_OPERAND_TYPE_REGISTER:o.kind=IROperandKind::Register;o.reg=name(in.reg.value);break;
        case ZYDIS_OPERAND_TYPE_IMMEDIATE:o.kind=IROperandKind::Immediate;o.immediate=in.imm.value.u;o.signedImmediate=in.imm.is_signed;o.relative=in.imm.is_relative;break;
        case ZYDIS_OPERAND_TYPE_MEMORY:o.kind=IROperandKind::Memory;o.segment=name(in.mem.segment);o.base=name(in.mem.base);o.index=name(in.mem.index);o.scale=in.mem.scale;o.displacement=in.mem.disp.value;break;
        default:break;}
        out.operands.push_back(std::move(o));
    }
    switch(decoded.mnemonic){case ZYDIS_MNEMONIC_MOV:out.op=IROp::Move;break;case ZYDIS_MNEMONIC_ADD:out.op=IROp::Add;break;case ZYDIS_MNEMONIC_SUB:out.op=IROp::Subtract;break;case ZYDIS_MNEMONIC_XOR:out.op=IROp::Xor;break;case ZYDIS_MNEMONIC_AND:out.op=IROp::And;break;case ZYDIS_MNEMONIC_OR:out.op=IROp::Or;break;case ZYDIS_MNEMONIC_CMP:out.op=IROp::Compare;break;case ZYDIS_MNEMONIC_TEST:out.op=IROp::Test;break;default:break;}
    if(out.flow==FlowType::Call) out.op=IROp::Call;
    if(out.flow==FlowType::Jump || out.flow==FlowType::ConditionalJump) out.op=IROp::Branch;
    if(out.flow==FlowType::Return) out.op=IROp::Return;
    if(decoded.operand_count_visible==2 && out.op>=IROp::Move && out.op<=IROp::Test){
        const auto& d=operands[0];const auto& s=operands[1];const auto cls=d.type==ZYDIS_OPERAND_TYPE_REGISTER?ZydisRegisterGetClass(d.reg.value):ZYDIS_REGCLASS_INVALID;
        const bool sourceGpr=s.type==ZYDIS_OPERAND_TYPE_REGISTER && ZydisRegisterGetClass(s.reg.value)==cls;
        out.lowered=(cls==ZYDIS_REGCLASS_GPR32 || cls==ZYDIS_REGCLASS_GPR64) && (sourceGpr || s.type==ZYDIS_OPERAND_TYPE_IMMEDIATE) && !(decoded.attributes&ZYDIS_ATTRIB_HAS_LOCK);
    }
    return out;
}
IRFunction X86IRLifter::function(uint64_t entry,std::span<const Instruction> instructions,AnalysisControl& control) const {
    IRFunction function;function.entry=entry;std::ostringstream listing,code;
    code<<"// Experimental register-level pseudocode derived from decoded bytes.\n// write_gpr(r32) zero-extends its enclosing r64; truncN wraps to N bits.\n// flags_* models x86 flags, including undefined flags. These are notation helpers.\n// opaque_x86 preserves unmodeled effects; no types/ABI/structured control flow recovered.\nvoid sub_"<<std::hex<<entry<<"(MachineState& cpu) {\n  uint64_t lhs, rhs, result;\ndispatch_ir:\n  switch (cpu.rip) {\n";
    for(const auto& i:instructions){control.work();code<<"    case "<<hex(i.address)<<": goto L_"<<std::hex<<i.address<<";\n";}
    code<<"    default: return; // leaves this recovered function\n  }\n";
    for(const auto& source:instructions){
        auto i=lift(source,control);control.reserveMemory(8192+i.operands.size()*512);
        listing<<hex(i.address)<<" "<<i.mnemonic<<" ["<<(i.lowered?"lowered":"opaque/control")<<"]";
        for(const auto& o:i.operands)listing<<" "<<operand(o)<<"{access="<<unsigned(o.access)<<"}";
        listing<<" flags(read="<<hex(i.flagsRead)<<",written="<<hex(i.flagsWritten)<<",undefined="<<hex(i.flagsUndefined)<<")\n";
        code<<"L_"<<std::hex<<i.address<<": // "<<i.mnemonic<<"\n  ";
        if(i.lowered){
            ++function.loweredCount;const auto& d=i.operands[0];const auto& s=i.operands[1];const auto lhs=value(d),rhs=value(s);
            if(i.op==IROp::Move)code<<writeReg(d,rhs);
            else {
                const char* op=i.op==IROp::Add?"+":i.op==IROp::Subtract || i.op==IROp::Compare?"-":i.op==IROp::Xor?"^":i.op==IROp::Or?"|":"&";
                code<<"lhs = "<<lhs<<"; rhs = trunc"<<std::dec<<d.bits<<"("<<rhs<<"); result = trunc"<<std::dec<<d.bits<<"(lhs "<<op<<" rhs);\n  "<<"flags_"<<i.mnemonic<<"(cpu, lhs, rhs, result, "<<d.bits<<");";
                if(i.op!=IROp::Compare && i.op!=IROp::Test)code<<"\n  "<<writeReg(d,"result");
            }
            code<<"\n  cpu.rip = "<<hex(i.address+i.size)<<"; goto dispatch_ir;";
        } else if(i.flow==FlowType::Jump && i.directTarget){
            code<<"cpu.rip = "<<hex(*i.directTarget)<<"; goto dispatch_ir;";
        } else {code<<"opaque_x86_step(cpu, "<<hex(i.address)<<"); goto dispatch_ir; // unmodeled semantics/PC; bytes:";for(auto byte:i.bytes)code<<" "<<hex(byte);}
        code<<"\n";function.instructions.push_back(std::move(i));
    }
    code<<"}\n";function.listing=listing.str();function.pseudocode=code.str();function.cLike=recoverIntegerFunction(function,control).code;control.checkpoint();return function;
}
}

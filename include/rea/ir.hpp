// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "rea/disassembly.hpp"
namespace rea {
enum class IROp { Move, Add, Subtract, Xor, And, Or, Compare, Test, Call, Branch, Return, Opaque };
enum class IROperandKind { Register, Immediate, Memory, Other };
struct IROperand {
    IROperandKind kind{IROperandKind::Other};uint16_t bits{};uint8_t access{};
    std::string reg,segment,base,index;uint8_t scale{};int64_t displacement{};
    uint64_t immediate{};bool signedImmediate{},relative{};
};
struct IRInstruction {
    uint64_t address{};uint8_t size{};IROp op{IROp::Opaque};std::string mnemonic;
    std::vector<IROperand> operands;std::vector<uint8_t> bytes;
    FlowType flow{FlowType::Invalid};std::optional<uint64_t> directTarget;
    uint32_t flagsRead{},flagsWritten{},flagsUndefined{};
    uint32_t flagsCleared{},flagsSet{};
    bool lowered{};
    bool semanticSafe{}; uint8_t addressBits{};
};
struct IRFunction {
    uint64_t entry{};std::vector<IRInstruction> instructions;
    std::string listing,pseudocode,cLike;size_t loweredCount{};
};
class X86IRLifter {
public:
    IRInstruction lift(const Instruction&,AnalysisControl&) const;
    IRFunction function(uint64_t,std::span<const Instruction>,AnalysisControl&) const;
};
}

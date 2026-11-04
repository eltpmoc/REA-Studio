// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "rea/binary.hpp"
#include "rea/analysis_control.hpp"
#include <map>
namespace rea {
enum class FlowType { Next, Call, ConditionalJump, Jump, Return, Stop, Invalid };
enum class Confidence { Confirmed, Probable, Unanalyzed };
const char* flowName(FlowType flow);
const char* confidenceName(Confidence confidence);
struct Instruction {
    uint64_t address{}, offset{};
    uint8_t size{};
    std::vector<uint8_t> bytes;
    std::string mnemonic, operands;
    FlowType flow{FlowType::Invalid};
    std::optional<uint64_t> directTarget;
    std::vector<uint64_t> dataReferences;
    Confidence confidence{Confidence::Unanalyzed};
    std::string evidence;
};
class IInstructionDecoder {
public:
    virtual ~IInstructionDecoder() = default;
    virtual Instruction decode(std::span<const uint8_t> bytes, uint64_t va, uint64_t offset) const = 0;
};
class X86InstructionDecoder final : public IInstructionDecoder {
public:
    Instruction decode(std::span<const uint8_t>, uint64_t va, uint64_t offset) const override;
};
struct AddressMapping { uint64_t offset{}, available{}; bool executable{}; };
// VA always means ELF link-time VA. Runtime addresses require explicit conversion.
class AddressTranslator {
public:
    AddressTranslator(const Image& image, uint64_t fileSize, uint64_t loadBias = 0);
    std::optional<AddressMapping> mapping(uint64_t va) const;
    std::optional<uint64_t> toFile(uint64_t va) const;
    std::optional<uint64_t> toVA(uint64_t offset) const;
    std::optional<uint64_t> toRuntime(uint64_t va) const;
    std::optional<uint64_t> fromRuntime(uint64_t runtime) const;
private:
    const Image& image_;
    uint64_t fileSize_, bias_;
    bool valid_{};
};
class SymbolResolver {
public:
    explicit SymbolResolver(const Image& image);
    std::string at(uint64_t va) const;
private:
    std::map<uint64_t,std::string> names_;
};
enum class ReferenceType { DirectCall, DirectJump, CodeAddress, DataAddress };
struct Reference { uint64_t from{}, to{}; bool flow{}; ReferenceType type{ReferenceType::DataAddress}; };
struct AnalysisResult {
    std::vector<Instruction> instructions;
    std::vector<Reference> references;
    std::vector<std::string> diagnostics;
    bool limited{};
    StopReason stopReason{StopReason::None};
};
class DisassemblyEngine {
public:
    explicit DisassemblyEngine(const IInstructionDecoder& decoder) : decoder_(decoder) {}
    // Recursive traversal only; undefined bytes are never swept as confirmed code.
    AnalysisResult analyze(BinaryFile&, const Image&, std::optional<uint64_t> requested = std::nullopt,
                           size_t maxInstructions = 20000) const;
    AnalysisResult analyze(BinaryFile&, const Image&, AnalysisControl&,
                           std::optional<uint64_t> requested = std::nullopt) const;
private:
    const IInstructionDecoder& decoder_;
};
}

// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "rea/disassembly.hpp"
namespace rea {
// Compatibility contracts. Concrete function/CFG APIs live in function_analysis.hpp.

class AssemblyBackend {
public:
    virtual ~AssemblyBackend() = default;
    virtual std::vector<Instruction> decode(const Image&, std::span<const uint8_t>, uint64_t address) const = 0;
};
struct BasicBlock { uint64_t address{}; std::vector<Instruction> instructions; std::vector<uint64_t> successors; };
class CfgBackend {
public:
    virtual ~CfgBackend() = default;
    virtual std::vector<BasicBlock> build(std::span<const Instruction>) const = 0;
};
class InspectionService {
public:
    virtual ~InspectionService() = default;
    // Future MCP adapters expose static metadata through this service.
    // No transport, command execution, or file mutation contract is provided.
    virtual const Image& image() const = 0;
};
}

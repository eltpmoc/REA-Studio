// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "rea/ir.hpp"
#include <array>
namespace rea {
// Immutable bitvector definitions; inputs precede users (single-block SSA).
struct SemanticValue {
    enum class Op { Constant, Argument, Unknown, Extract, Join, Add, Sub, Xor, And, Or };
    Op op{}; unsigned bits{}; uint64_t literal{}; size_t left{},right{};
};
struct SemanticResult {
    std::vector<SemanticValue> values;
    std::vector<std::vector<size_t>> uses;
    size_t returned{}; bool recovered{};
    std::string code,reason;
    uint64_t evaluate(std::array<uint64_t,6> arguments) const;
};
SemanticResult recoverIntegerFunction(const IRFunction&,AnalysisControl&);
}

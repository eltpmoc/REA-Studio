// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "rea/analysis_control.hpp"
#include "rea/binary.hpp"
#include <filesystem>
#include <span>
namespace rea::fuzz {
// Returns true for an accepted ELF. Expected parse/resource failures are normal rejections.
bool exercise(std::span<const uint8_t> input,const std::filesystem::path& scratch);
}

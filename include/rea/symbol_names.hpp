// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <string>
#include <string_view>
namespace rea {
// Empty means unsupported, refused by complexity guard, or not a mangled name.
std::string demangleSymbol(std::string_view);
}

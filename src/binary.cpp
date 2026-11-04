// SPDX-License-Identifier: GPL-3.0-or-later
#include "rea/binary.hpp"
#include <algorithm>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
namespace rea {
BinaryFile::BinaryFile(const std::filesystem::path& path) {
    std::error_code error;
    if(!std::filesystem::is_regular_file(path,error) || error) throw std::runtime_error("Expected a regular binary file");
    stream_.open(path,std::ios::binary);
    if (!stream_) throw std::runtime_error("Cannot open binary file");
    stream_.seekg(0, std::ios::end);
    auto end = stream_.tellg();
    if (end < 0) throw std::runtime_error("Cannot determine file size");
    size_ = static_cast<uint64_t>(end);
}
std::vector<uint8_t> BinaryFile::read(uint64_t offset, uint64_t count) {
    if (offset > size_ || count > size_ - offset || count > 16 * 1024 * 1024 ||
        offset > static_cast<uint64_t>(std::numeric_limits<std::streamoff>::max()))
        throw std::runtime_error("Invalid or excessive file range");
    std::vector<uint8_t> result(static_cast<size_t>(count));
    stream_.clear();
    stream_.seekg(static_cast<std::streamoff>(offset));
    if (count && !stream_.read(reinterpret_cast<char*>(result.data()), static_cast<std::streamsize>(count)))
        throw std::runtime_error("Short file read");
    return result;
}
std::string Image::architecture() const {
    switch(machine) {
    case 3: return "x86"; case 62: return "x86-64"; case 40: return "ARM";
    case 183: return "AArch64"; case 243: return "RISC-V"; case 8: return "MIPS";
    case 20: return "PowerPC"; case 21: return "PowerPC64";
    default: return "Unknown (" + std::to_string(machine) + ")";
    }
}
std::optional<uint64_t> Image::offsetForAddress(uint64_t address) const {
    std::optional<uint64_t> result;
    for (const auto& s : segments) {
        if (s.type != 1 || address < s.address || address - s.address >= s.fileSize) continue;
        auto delta = address - s.address;
        if (s.offset > UINT64_MAX-delta || s.address > UINT64_MAX-s.memorySize || s.fileSize > s.memorySize) return std::nullopt;
        auto candidate = s.offset + delta;
        if (result && *result != candidate) return std::nullopt;
        result = candidate;
    }
    return result;
}
std::string hexRow(BinaryFile& file, uint64_t offset, uint64_t count) {
    auto data = file.read(offset, std::min<uint64_t>(count, 16));
    std::ostringstream out;
    out << std::hex << std::setfill('0') << std::setw(16) << offset << "  ";
    for (size_t i=0; i<16; ++i) {
        if (i < data.size()) out << std::setw(2) << static_cast<unsigned>(data[i]) << ' ';
        else out << "   ";
    }
    out << " |";
    for (auto c : data) out << (c >= 32 && c <= 126 ? static_cast<char>(c) : '.');
    return out.str() + '|';
}
}

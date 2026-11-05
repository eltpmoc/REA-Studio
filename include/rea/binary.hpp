// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <span>
#include <string>
#include <vector>
namespace rea {
class AnalysisControl;
struct Section {
    std::string name;
    uint32_t type{};
    uint64_t flags{}, address{}, offset{}, size{}, entrySize{};
    uint32_t link{}, info{};
};
struct Symbol {
    std::string name;
    uint64_t address{}, size{};
    uint32_t section{};
    uint8_t info{}, visibility{};
    bool dynamic{};
    std::string demangledName;
};
struct Segment { uint32_t type{}, flags{}; uint64_t offset{}, address{}, fileSize{}, memorySize{}; };
struct Image {
    unsigned bits{};
    bool littleEndian{};
    uint16_t machine{}, type{};
    uint64_t entry{}, headerSize{}, programOffset{}, sectionOffset{};
    std::vector<Section> sections;
    std::vector<Symbol> symbols;
    std::vector<Segment> segments;
    std::string architecture() const;
    std::optional<uint64_t> offsetForAddress(uint64_t address) const;
};
class BinaryFile {
public:
    explicit BinaryFile(const std::filesystem::path& path);
    uint64_t size() const { return size_; }
    std::vector<uint8_t> read(uint64_t offset, uint64_t count);
private:
    std::ifstream stream_;
    uint64_t size_{};
};
class Loader {
public:
    virtual ~Loader() = default;
    virtual bool accepts(std::span<const uint8_t> magic) const = 0;
    virtual Image load(BinaryFile& file) const = 0;
};
class ElfLoader final : public Loader {
public:
    bool accepts(std::span<const uint8_t> magic) const override;
    Image load(BinaryFile& file) const override;
    Image load(BinaryFile& file, AnalysisControl& control) const;
};
// Future PE loaders implement Loader. See disassembly.hpp and function_analysis.hpp for analysis.
std::string hexRow(BinaryFile& file, uint64_t offset, uint64_t count = 16);
}

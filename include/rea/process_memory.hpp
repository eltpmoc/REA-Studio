// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
#include <memory>
#include <stop_token>
#include <span>
#include <string>
#include <vector>
namespace rea {
struct ProcessDescriptor {
    uint32_t pid{};
    uint64_t identity{}; // Linux start ticks / Windows creation FILETIME; zero = unavailable.
    std::string name, path;
};
struct MemoryRegion {
    uint64_t start{}, end{}; // Exclusive end, runtime addresses.
    bool readable{}, writable{}, executable{};
    std::string name;
};
struct MemoryPage {
    uint64_t address{};
    std::vector<uint8_t> bytes;
    std::string error; // A partial read retains only actual bytes, never zero-filled holes.
};
struct MemoryWriteResult {
    uint64_t address{};size_t written{};bool verified{};
    std::vector<uint8_t> before,after;std::string error;
};
uint32_t currentProcessId();
std::vector<ProcessDescriptor> enumerateProcesses(std::stop_token={});
class ProcessMemory final {
public:
    static constexpr size_t maxReadBytes=4096;
    explicit ProcessMemory(const ProcessDescriptor&,bool writable=false);
    ~ProcessMemory();
    ProcessMemory(const ProcessMemory&)=delete;
    ProcessMemory& operator=(const ProcessMemory&)=delete;
    std::vector<MemoryRegion> regions(std::stop_token={}) const;
    MemoryPage read(uint64_t address,size_t count,std::stop_token={}) const;
    MemoryWriteResult write(uint64_t,std::span<const uint8_t> expected,std::span<const uint8_t> replacement,std::stop_token={}) const;
    bool canWrite() const {return writable_;}
    const ProcessDescriptor& process() const {return process_;}
private:
    struct Native;
    std::unique_ptr<Native> native_;
    ProcessDescriptor process_;
    bool writable_{};
};
}

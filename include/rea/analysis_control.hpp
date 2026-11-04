// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
namespace rea {
enum class StopReason { None, Cancelled, TimeLimit, MemoryLimit, InstructionLimit, WorkLimit };
const char* stopReasonName(StopReason reason);
struct AnalysisLimits {
    uint64_t maxMemoryBytes{128ULL*1024*1024};
    std::chrono::milliseconds maxTime{10000};
    uint64_t maxInstructions{20000};
    uint64_t maxWorkItems{1000000};
    size_t maxCFGNodes{4096}, maxCFGEdges{16384};
};
class AnalysisStopped final : public std::runtime_error {
public:
    explicit AnalysisStopped(StopReason reason) : std::runtime_error(stopReasonName(reason)),reason_(reason) {}
    StopReason reason() const {return reason_;}
private:
    StopReason reason_;
};
// Shared across navigation jobs for a project; CAS prevents concurrent overshoot.
class InstructionBudget {
public:
    explicit InstructionBudget(uint64_t maximum) : maximum_(maximum) {}
    bool claim();
    uint64_t used() const {return used_.load(std::memory_order_relaxed);}
    uint64_t maximum() const {return maximum_;}
private:
    const uint64_t maximum_;
    std::atomic<uint64_t> used_{};
};
struct AnalysisProgress {
    int percent{};
    std::string phase;
    uint64_t instructions{}, reservedBytes{}, elapsedMs{};
};
class AnalysisControl {
public:
    using ProgressCallback=std::function<void(const AnalysisProgress&)>;
    explicit AnalysisControl(AnalysisLimits limits={},std::shared_ptr<InstructionBudget> budget={},ProgressCallback progress={});
    void cancel() noexcept {cancelled_.store(true,std::memory_order_relaxed);}
    bool cancelled() const noexcept {return cancelled_.load(std::memory_order_relaxed);}
    void checkpoint() const;
    void work();
    void reserveMemory(uint64_t bytes);
    void claimInstruction();
    void report(int percent,const std::string& phase,bool force=false);
    const AnalysisLimits& limits() const {return limits_;}
    uint64_t reservedBytes() const {return reserved_.load(std::memory_order_relaxed);}
    uint64_t instructions() const {return budget_->used();}
    uint64_t elapsedMs() const;
private:
    AnalysisLimits limits_;
    std::shared_ptr<InstructionBudget> budget_;
    ProgressCallback progress_;
    std::atomic<bool> cancelled_{};
    std::atomic<uint64_t> reserved_{};
    uint64_t workItems_{},localInstructions_{};
    std::chrono::steady_clock::time_point started_,lastReport_;
    int lastPercent_{};
    std::string lastPhase_;
};
}

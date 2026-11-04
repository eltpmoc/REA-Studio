// SPDX-License-Identifier: GPL-3.0-or-later
#include "rea/analysis_control.hpp"
#include <algorithm>
namespace rea {
const char* stopReasonName(StopReason r) {
    switch(r) {
    case StopReason::None:return "Completed"; case StopReason::Cancelled:return "Cancelled";
    case StopReason::TimeLimit:return "Time limit reached"; case StopReason::MemoryLimit:return "Memory budget reached";
    case StopReason::InstructionLimit:return "Global instruction budget reached"; default:return "Analysis work budget reached";
    }
}
bool InstructionBudget::claim() {
    auto current=used_.load(std::memory_order_relaxed);
    while(current<maximum_) if(used_.compare_exchange_weak(current,current+1,std::memory_order_relaxed)) return true;
    return false;
}
AnalysisControl::AnalysisControl(AnalysisLimits limits,std::shared_ptr<InstructionBudget> budget,ProgressCallback progress)
    : limits_(limits),budget_(budget ? std::move(budget) : std::make_shared<InstructionBudget>(limits.maxInstructions)),
      progress_(std::move(progress)),started_(std::chrono::steady_clock::now()),lastReport_(started_) {}
void AnalysisControl::checkpoint() const {
    if(cancelled()) throw AnalysisStopped(StopReason::Cancelled);
    if(std::chrono::steady_clock::now()-started_>=limits_.maxTime) throw AnalysisStopped(StopReason::TimeLimit);
}
void AnalysisControl::work() {
    checkpoint();
    if(workItems_>=limits_.maxWorkItems) throw AnalysisStopped(StopReason::WorkLimit);
    ++workItems_;
}
void AnalysisControl::reserveMemory(uint64_t bytes) {
    checkpoint();
    auto used=reserved_.load(std::memory_order_relaxed);
    if(used>limits_.maxMemoryBytes || bytes>limits_.maxMemoryBytes-used) throw AnalysisStopped(StopReason::MemoryLimit);
    reserved_.store(used+bytes,std::memory_order_relaxed);
}
void AnalysisControl::claimInstruction() {
    checkpoint();
    if(localInstructions_>=limits_.maxInstructions || !budget_->claim()) throw AnalysisStopped(StopReason::InstructionLimit);
    ++localInstructions_;
}
uint64_t AnalysisControl::elapsedMs() const {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-started_).count());
}
void AnalysisControl::report(int percent,const std::string& phase,bool force) {
    if(!progress_) return;
    auto now=std::chrono::steady_clock::now();
    if(!force && phase==lastPhase_ && now-lastReport_<std::chrono::milliseconds(40)) return;
    lastReport_=now;lastPhase_=phase;lastPercent_=std::max(lastPercent_,std::clamp(percent,0,100));
    progress_({lastPercent_,phase,instructions(),reservedBytes(),elapsedMs()});
}
}

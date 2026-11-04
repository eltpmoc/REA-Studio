// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "rea/function_analysis.hpp"
#include <stop_token>
#include <variant>
namespace rea {
// Typed contract only: no worker process or wire transport exists in v0.3.
namespace protocol {
inline constexpr uint32_t version=1;
inline constexpr uint64_t maxPayloadBytes=16*1024*1024;
enum class MessageKind { Hello, Analyze, Progress, Result, Cancel, Error };
enum class State { Idle, Running, Cancelling, Completed, Cancelled, Failed };
struct Header {uint32_t protocolVersion{version};MessageKind kind{};uint64_t requestId{},payloadBytes{};};
bool valid(const Header&);
std::optional<State> transition(State,MessageKind);
struct HelloMessage {uint32_t supportedVersion{version};};
struct AnalyzeMessage {std::string path;AnalysisLimits limits;std::optional<uint64_t> requested;};
struct CancelMessage {};
struct ResultMessage {AnalysisResult assembly;ProgramAnalysis program;StopReason reason{StopReason::None};};
struct ErrorMessage {std::string description;StopReason reason{StopReason::None};};
using Payload=std::variant<HelloMessage,AnalyzeMessage,AnalysisProgress,ResultMessage,CancelMessage,ErrorMessage>;
struct Message {Header header;Payload payload;};
bool valid(const Message&);
}
struct BackendResult {AnalysisResult assembly;ProgramAnalysis program;};
class IAnalysisBackend {
public:
    virtual ~IAnalysisBackend()=default;
    virtual BackendResult analyze(BinaryFile&,const Image&,AnalysisControl&,std::stop_token,std::optional<uint64_t>) const=0;
};
class InProcessAnalysisBackend final : public IAnalysisBackend {
public:
    BackendResult analyzeIncremental(BinaryFile&,const Image&,AnalysisControl&,std::stop_token,uint64_t,const AnalysisResult&) const;
    BackendResult analyze(BinaryFile&,const Image&,AnalysisControl&,std::stop_token,std::optional<uint64_t>) const override;
};
// Future platform implementations must report actual OS enforcement or an error.
struct ProcessLimits {uint64_t memoryBytes{},cpuMilliseconds{},processCount{1};};
struct EnforcementResult {bool enforced{};std::string error;};
class LinuxCgroupV2Policy {
public:
    virtual ~LinuxCgroupV2Policy()=default;
    virtual EnforcementResult attachWorker(int pid,const ProcessLimits&)=0;
};
class WindowsJobObjectPolicy {
public:
    virtual ~WindowsJobObjectPolicy()=default;
    virtual EnforcementResult assignWorker(uintptr_t processHandle,const ProcessLimits&)=0;
};
}

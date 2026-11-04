// SPDX-License-Identifier: GPL-3.0-or-later
#include "rea/analysis_backend.hpp"
namespace rea {
bool protocol::valid(const Header& h) {
    return h.protocolVersion==version && h.payloadBytes<=maxPayloadBytes && h.requestId!=0 &&
        static_cast<unsigned>(h.kind)<=static_cast<unsigned>(MessageKind::Error);
}
bool protocol::valid(const Message& m) {
    if(!valid(m.header) || m.payload.index()!=static_cast<size_t>(m.header.kind)) return false;
    if(const auto* hello=std::get_if<HelloMessage>(&m.payload)) return hello->supportedVersion==version;
    if(const auto* request=std::get_if<AnalyzeMessage>(&m.payload)) return !request->path.empty() && request->path.size()<=4096 && request->path.find(char(0))==std::string::npos;
    if(const auto* error=std::get_if<ErrorMessage>(&m.payload)) return error->description.size()<=4096;
    return true;
}
std::optional<protocol::State> protocol::transition(State s,MessageKind m) {
    if(m==MessageKind::Hello && s==State::Idle) return s;
    if(m==MessageKind::Analyze && (s==State::Idle || s==State::Completed || s==State::Cancelled || s==State::Failed)) return State::Running;
    if(m==MessageKind::Cancel && s==State::Running) return State::Cancelling;
    if(m==MessageKind::Progress && s==State::Running) return s;
    if(m==MessageKind::Result && s==State::Running) return State::Completed;
    if(m==MessageKind::Result && s==State::Cancelling) return State::Cancelled;
    if(m==MessageKind::Error && (s==State::Running || s==State::Cancelling)) return State::Failed;
    return {};
}
BackendResult InProcessAnalysisBackend::analyze(BinaryFile& file,const Image& image,AnalysisControl& control,std::stop_token token,std::optional<uint64_t> requested) const {
    std::stop_callback cancellation(token,[&control]{control.cancel();});
    X86InstructionDecoder decoder;BackendResult result;
    result.assembly=DisassemblyEngine(decoder).analyze(file,image,control,requested);
    if(result.assembly.stopReason==StopReason::None) {
        try {result.program=analyzeProgram(image,file.size(),result.assembly,control);}
        catch(const AnalysisStopped& e) {result.assembly.stopReason=e.reason();result.assembly.limited=e.reason()!=StopReason::Cancelled;result.assembly.diagnostics.emplace_back(stopReasonName(e.reason()));}
    }
    return result;
}
}

// SPDX-License-Identifier: GPL-3.0-or-later
#include "rea/analysis_worker.hpp"
#include <QThread>
#include <QTimer>
#include <mutex>
namespace rea {
struct AnalysisTaskState {
    std::mutex mutex;
    std::optional<AnalysisProgress> progress;
    std::atomic<bool> done{};
    quint64 id{};
    AnalysisOutcomePtr outcome{std::make_shared<AnalysisOutcome>()};
};
AnalysisController::AnalysisController(QObject* parent):QObject(parent),pollTimer_(new QTimer(this)) {
    qRegisterMetaType<AnalysisOutcomePtr>();pollTimer_->setInterval(15);
    connect(pollTimer_,&QTimer::timeout,this,&AnalysisController::poll);
}
AnalysisController::~AnalysisController() {
    destroying_=true; pending_.reset();pollTimer_->stop();
    if(control_) control_->cancel();
    if(worker_) {worker_->request_stop();worker_->join();worker_.reset();}
}
void AnalysisController::discardPending() {
    if(!pending_) return;
    auto id=pending_->id;pending_.reset();
    auto outcome=std::make_shared<AnalysisOutcome>();outcome->reason=StopReason::Cancelled;
    QTimer::singleShot(0,this,[this,id,outcome]{emit completed(id,outcome);});
}
quint64 AnalysisController::start(const QString& path,AnalysisLimits limits,std::optional<uint64_t> requested,std::shared_ptr<InstructionBudget> quota) {
    Q_ASSERT(QThread::currentThread()==thread());
    Request request{++latest_,path,limits,requested,std::move(quota)};
    if(worker_) {
        control_->cancel();worker_->request_stop();discardPending();pending_=std::move(request);
    } else launch(std::move(request));
    emit busyChanged(true);return latest_;
}
void AnalysisController::cancel() {
    Q_ASSERT(QThread::currentThread()==thread());
    if(control_) control_->cancel();
    if(worker_) worker_->request_stop();
    discardPending();
    if(!worker_) emit busyChanged(false);
}
void AnalysisController::launch(Request request) {
    state_=std::make_shared<AnalysisTaskState>();state_->id=request.id;
    auto state=state_;auto outcome=state->outcome;
    control_=std::make_shared<AnalysisControl>(request.limits,request.quota,[state](const AnalysisProgress& p){
        std::lock_guard lock(state->mutex);state->progress=p; // Single latest progress sample; bounded mailbox.
    });
    auto control=control_;
    worker_=std::make_unique<std::jthread>([request=std::move(request),control,outcome,state](std::stop_token token){
        std::stop_callback cancellation(token,[control]{control->cancel();});
        try {
            control->checkpoint();control->report(0,"Opening binary",true);
#ifdef _WIN32
            outcome->file=std::make_unique<BinaryFile>(std::filesystem::path(request.path.toStdWString()));
#else
            outcome->file=std::make_unique<BinaryFile>(std::filesystem::path(request.path.toUtf8().constData()));
#endif
            outcome->image=ElfLoader{}.load(*outcome->file,*control);outcome->hasImage=true;
            auto result=InProcessAnalysisBackend{}.analyze(*outcome->file,outcome->image,*control,token,request.requested);
            outcome->analysis=std::move(result.assembly);outcome->program=std::move(result.program);
            outcome->reason=outcome->analysis.stopReason;
            // A late cancel wins even after the final decoded instruction.
            if(control->cancelled()) outcome->reason=StopReason::Cancelled;
            control->report(outcome->reason==StopReason::None ? 100 : 95,stopReasonName(outcome->reason),true);
        } catch(const AnalysisStopped& stopped) {outcome->reason=stopped.reason();}
        catch(const std::exception& e) {outcome->error=QString::fromUtf8(e.what());}
        catch(...) {outcome->error="Unknown analysis failure";}
        outcome->instructions=control->instructions();outcome->reservedBytes=control->reservedBytes();outcome->elapsedMs=control->elapsedMs();
        state->done.store(true,std::memory_order_release);
    });
    pollTimer_->start();
}
void AnalysisController::poll() {
    if(!state_ || !worker_) return;
    const auto state=state_;
    std::optional<AnalysisProgress> sample;
    {std::lock_guard lock(state->mutex);sample=std::move(state->progress);state->progress.reset();}
    if(sample) emit progress(state->id,sample->percent,QString::fromStdString(sample->phase),sample->instructions,sample->reservedBytes,sample->elapsedMs);
    if(!state->done.load(std::memory_order_acquire)) return;
    worker_->join(); // All output is published only after acquire + join.
    worker_.reset();pollTimer_->stop();
    auto outcome=state->outcome;
    if(control_->cancelled()) outcome->reason=StopReason::Cancelled;
    control_.reset();state_.reset();
    auto next=std::move(pending_);pending_.reset();
    if(!destroying_) emit completed(state->id,outcome);
    if(!destroying_ && !worker_ && next && next->id==latest_) launch(std::move(*next));
    if(!destroying_) emit busyChanged(busy());
}
}

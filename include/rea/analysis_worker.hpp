// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "rea/analysis_backend.hpp"
#include <QObject>
#include <QString>
#include <optional>
#include <thread>
class QTimer;
namespace rea {
struct AnalysisTaskState;
struct AnalysisOutcome {
    std::unique_ptr<BinaryFile> file;
    Image image;
    ProgramAnalysis program;
    AnalysisResult analysis;
    QString error;
    StopReason reason{StopReason::None};
    uint64_t instructions{},reservedBytes{},elapsedMs{};
    bool hasImage{};
};
using AnalysisOutcomePtr=std::shared_ptr<AnalysisOutcome>;
// All public methods are called from the owning (GUI) thread. Only control.cancel is cross-thread.
class AnalysisController final : public QObject {
    Q_OBJECT
public:
    explicit AnalysisController(QObject* parent=nullptr);
    ~AnalysisController() override;
    quint64 start(const QString& path,AnalysisLimits limits={},std::optional<uint64_t> requested={},
                  std::shared_ptr<InstructionBudget> quota={},std::shared_ptr<const AnalysisResult> baseline={});
    void cancel();
    bool busy() const {return worker_!=nullptr || pending_.has_value();}
    quint64 latestId() const {return latest_;}
signals:
    void progress(quint64 id,int percent,QString phase,quint64 instructions,quint64 bytes,quint64 elapsedMs);
    void completed(quint64 id,rea::AnalysisOutcomePtr outcome);
    void busyChanged(bool busy);
private:
    struct Request {quint64 id;QString path;AnalysisLimits limits;std::optional<uint64_t> requested;std::shared_ptr<InstructionBudget> quota;std::shared_ptr<const AnalysisResult> baseline;};
    void launch(Request request);
    void discardPending();
    void poll();
    std::unique_ptr<std::jthread> worker_;
    QTimer* pollTimer_{};
    std::shared_ptr<AnalysisTaskState> state_;
    std::shared_ptr<AnalysisControl> control_;
    std::optional<Request> pending_;
    quint64 latest_{};
    bool destroying_{};
};
}
Q_DECLARE_METATYPE(rea::AnalysisOutcomePtr)

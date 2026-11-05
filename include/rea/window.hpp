// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "rea/instruction_model.hpp"
#include "rea/program_models.hpp"
#include "rea/graph_view.hpp"
#include "rea/analysis_worker.hpp"
#include <QMainWindow>
#include <memory>
class QPlainTextEdit;
class QTreeWidget;
class QTableWidget;
class QLabel;
class QTableView;
class QTabWidget;
class QListWidget;
class QProgressBar;
class QCloseEvent;
class QDockWidget;
class QAction;
namespace rea {
class ProcessView;
class Window final : public QMainWindow {
public:
    explicit Window(bool persistentSettings=true);
    ~Window() override;
    bool openFile(const QString& path, bool interactive = true);
    quint64 openFileAsync(const QString& path, bool interactive = true);
    void cancelAnalysis();
    bool analysisBusy() const;
    void setAnalysisLimits(AnalysisLimits limits) {limits_=limits;}
    AnalysisLimits analysisLimits() const {return limits_;}
    bool smokeValid() const;
    bool navigateToAddress(uint64_t va);
    bool navigateToFunction(uint64_t va);
    void goToFunction();
    const FunctionModel* functionModel() const {return functionModel_;}
    const SymbolModel* symbolModel() const {return symbolModel_;}
    const ProgramAnalysis& programAnalysis() const {return program_;}
    bool navigateToOffset(uint64_t offset);
    bool goBack();
    bool goForward();
    void followSelected();
    std::optional<uint64_t> selectedAddress() const;
    const InstructionModel* instructionModel() const {return instructions_;}
    void copySelection();
    void setDarkTheme(bool dark);
protected:
    void closeEvent(QCloseEvent*) override;
private:
    struct Location { bool address; uint64_t value; };
    bool go(Location location,bool record);
    void record(Location location);
    void navigate(bool address);
    void showHex(uint64_t offset);
    void log(const QString& message);
    void updateSelection();
    void updateFunctions();
    quint64 startJob(const QString& path,std::optional<uint64_t> navigation,bool interactive);
    void finishJob(quint64 id,AnalysisOutcomePtr outcome);
    bool commitOutcome(const QString& path,AnalysisOutcomePtr outcome);
    void configureLimits();
    QPlainTextEdit *irText_{},*pseudocode_{};
    QPlainTextEdit *hex_{}, *info_{}, *logs_{}, *console_{}, *addressInfo_{}, *instructionInfo_{};
    QTableWidget *sections_{}, *symbols_{};
    QTreeWidget *project_{};
    QTableView *functions_{}, *referencesView_{};
    FunctionModel* functionModel_{};
    SymbolModel* symbolModel_{};
    QAction* goToFunctionAction_{};
    ReferenceModel* referenceModel_{};
    GraphView* graph_{};
    ProgramAnalysis program_;
    std::optional<uint64_t> displayedFunction_;
    QLabel *hexPosition_{}, *analysisState_{};
    QTableView* assembly_{};
    InstructionModel* instructions_{};
    QTabWidget* centralTabs_{};
    ProcessView* processView_{};
    QListWidget* history_{};
    QProgressBar *progress_{}, *toolbarProgress_{};
    QLabel* progressDetails_{};
    QDockWidget* progressDock_{};
    QAction* toolbarProgressAction_{};
    AnalysisController* controller_{};
    AnalysisLimits limits_;
    std::shared_ptr<InstructionBudget> projectQuota_,requestQuota_;
    quint64 requestId_{};
    QString loadedPath_,requestPath_;
    std::optional<uint64_t> requestedNavigation_;
    bool requestInteractive_{},lastJobSuccess_{},applyingResult_{},closing_{};
    std::unique_ptr<BinaryFile> file_;
    Image image_;
    uint64_t page_{};
    std::vector<Location> locations_;
    int historyPosition_{-1};
    bool persistent_{}, dark_{true};
};
}

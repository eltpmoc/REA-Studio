// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "rea/process_memory.hpp"
#include <QAbstractTableModel>
#include <QWidget>
#include <optional>
#include <thread>
class QCheckBox;class QTableView;class QPlainTextEdit;class QLabel;class QLineEdit;class QPushButton;class QTimer;class QSortFilterProxyModel;
namespace rea {
class ProcessModel final : public QAbstractTableModel {
public:
    using QAbstractTableModel::QAbstractTableModel;
    int rowCount(const QModelIndex& p={}) const override {return p.isValid()?0:static_cast<int>(processes_.size());}
    int columnCount(const QModelIndex& p={}) const override {return p.isValid()?0:3;}
    QVariant data(const QModelIndex&,int role=Qt::DisplayRole) const override;
    QVariant headerData(int,Qt::Orientation,int) const override;
    void setProcesses(std::vector<ProcessDescriptor> p){beginResetModel();processes_=std::move(p);endResetModel();}
    const ProcessDescriptor* process(int row) const;
private:std::vector<ProcessDescriptor> processes_;
};
class MemoryRegionModel final : public QAbstractTableModel {
public:
    using QAbstractTableModel::QAbstractTableModel;
    int rowCount(const QModelIndex& p={}) const override {return p.isValid()?0:static_cast<int>(regions_.size());}
    int columnCount(const QModelIndex& p={}) const override {return p.isValid()?0:4;}
    QVariant data(const QModelIndex&,int role=Qt::DisplayRole) const override;
    QVariant headerData(int,Qt::Orientation,int) const override;
    void setRegions(std::vector<MemoryRegion> p){beginResetModel();regions_=std::move(p);endResetModel();}
    const MemoryRegion* region(int row) const;
    const MemoryRegion* containing(uint64_t address) const;
private:std::vector<MemoryRegion> regions_;
};
struct ProcessJobState;
class ProcessView final : public QWidget {
    Q_OBJECT
public:
    explicit ProcessView(QWidget* parent=nullptr);
    ~ProcessView() override;
    void refreshProcesses();
    void copySelection();
    void openProcess(const ProcessDescriptor&);
    bool readAddress(uint64_t);
    bool enableWriting(bool);
    bool writeMemory(uint64_t,std::vector<uint8_t> expected,std::vector<uint8_t> replacement);
    bool undoLastWrite();
    bool busy() const {return worker_!=nullptr || pending_.has_value();}
    void cancel();
    void stop();
    const ProcessModel* processModel() const {return processes_;}
    std::optional<uint32_t> openedPid() const {return session_?std::optional<uint32_t>(session_->process().pid):std::nullopt;}
protected:
    void showEvent(QShowEvent*) override;
private:
    enum class Job { Enumerate, Open, Regions, Read, Write };
    struct Request {Job kind{};uint64_t id{};ProcessDescriptor process;std::shared_ptr<ProcessMemory> session;uint64_t address{};size_t count{};bool writable{},undo{};std::vector<uint8_t> expected,replacement;};
    bool submit(Request);
    void editMemory();
    void launch(Request);
    void poll();
    void updateBusy();
    void showPage(const MemoryPage&);
    QTableView *processTable_{},*regionTable_{};
    ProcessModel* processes_{};MemoryRegionModel* regions_{};
    QSortFilterProxyModel* filter_{};
    QPlainTextEdit* hex_{};
    QLabel *status_{},*opened_{};
    QLineEdit* address_{};
    QPushButton *cancel_{},*edit_{},*undo_{};
    QCheckBox* writeMode_{};
    QTimer *pollTimer_{},*liveTimer_{};
    std::unique_ptr<std::jthread> worker_;
    std::shared_ptr<ProcessJobState> state_;
    std::optional<Request> pending_;
    std::shared_ptr<ProcessMemory> session_;
    uint64_t latest_{};
    std::optional<uint64_t> page_;
    MemoryPage lastPage_;
    std::optional<MemoryWriteResult> lastPatch_;
    bool initialized_{},stopping_{};
};
}

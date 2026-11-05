// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "rea/disassembly.hpp"
#include <QAbstractTableModel>
namespace rea {
struct FunctionRegistry;
class InstructionModel final : public QAbstractTableModel {
public:
    enum Role { AddressRole=Qt::UserRole+1, OffsetRole, FlowRole, ConfidenceRole, TargetRole, FunctionEntryRole, FunctionNameRole, IncomingReferencesRole, OutgoingReferencesRole, DarkRole };
    explicit InstructionModel(QObject* parent=nullptr) : QAbstractTableModel(parent) {}
    int rowCount(const QModelIndex& parent={}) const override;
    int columnCount(const QModelIndex& parent={}) const override;
    QVariant data(const QModelIndex&,int role=Qt::DisplayRole) const override;
    QVariant headerData(int section,Qt::Orientation,int role) const override;
    void setAnalysis(AnalysisResult result,const Image& image);
    void setDark(bool dark);
    void setFunctions(const FunctionRegistry&);
    const Instruction* instruction(int row) const;
    int rowForAddress(uint64_t va) const;
    const AnalysisResult& analysis() const {return *result_;}
    std::shared_ptr<const AnalysisResult> snapshot() const {return result_;}
private:
    std::shared_ptr<const AnalysisResult> result_{std::make_shared<AnalysisResult>()};
    std::vector<QString> comments_;
    std::map<uint64_t,QString> functions_;
    std::map<uint64_t,size_t> incoming_,outgoing_;
    bool dark_{true};
};
}

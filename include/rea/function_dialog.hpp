// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "rea/program_models.hpp"
#include <QDialog>
#include <QSortFilterProxyModel>
#include <functional>
class QLineEdit;class QTableView;class QLabel;
namespace rea {
class FunctionSearchModel final : public QAbstractTableModel {
public:
    FunctionSearchModel(FunctionModel*,SymbolModel*,QObject* parent=nullptr);
    int rowCount(const QModelIndex& p={}) const override {return p.isValid()?0:static_cast<int>(entries_.size());}
    int columnCount(const QModelIndex& p={}) const override {return p.isValid()?0:5;}
    QVariant data(const QModelIndex&,int role=Qt::DisplayRole) const override;
    QVariant headerData(int,Qt::Orientation,int) const override;
private:
    struct Entry {uint64_t address{},size{};QString name,search,origin,confidence;};
    void rebuild();FunctionModel* functions_;SymbolModel* symbols_;std::vector<Entry> entries_;
};
class FunctionSearchFilter final : public QSortFilterProxyModel {
public:
    using QSortFilterProxyModel::QSortFilterProxyModel;
    void setQuery(const QString&);
protected:bool filterAcceptsRow(int,const QModelIndex&) const override;
private:QStringList words_;
};
class GoToFunctionDialog final : public QDialog {
    Q_OBJECT
public:
    GoToFunctionDialog(FunctionModel*,SymbolModel*,std::function<bool(uint64_t)>,QWidget* parent=nullptr);
    static std::optional<uint64_t> parseAddress(QString);
private:
    void activate();QLineEdit* query_{};QTableView* view_{};QLabel* status_{};FunctionSearchFilter* filter_{};std::function<bool(uint64_t)> navigate_;
};
}

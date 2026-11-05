// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "rea/function_analysis.hpp"
#include <QAbstractTableModel>
namespace rea {
enum NavigationRole {NavAddressRole=Qt::UserRole,NavOriginalRole,NavDemangledRole,NavSizeRole,NavOriginRole,NavConfidenceRole,NavSearchRole,NavFunctionRole};
class SymbolModel final : public QAbstractTableModel {
public:
    using QAbstractTableModel::QAbstractTableModel;
    int rowCount(const QModelIndex& p={}) const override {return p.isValid()?0:static_cast<int>(symbols_.size());}
    int columnCount(const QModelIndex& p={}) const override {return p.isValid()?0:8;}
    QVariant data(const QModelIndex&,int role=Qt::DisplayRole) const override;
    QVariant headerData(int,Qt::Orientation,int) const override;
    void setImage(const Image&);
    const Symbol* symbol(int) const;
    bool isFunction(int) const;
private:std::vector<Symbol> symbols_;size_t sections_{};
};
class FunctionModel final : public QAbstractTableModel {
public:
    using QAbstractTableModel::QAbstractTableModel;
    int rowCount(const QModelIndex& p={}) const override {return p.isValid()?0:static_cast<int>(registry_.functions.size());}
    int columnCount(const QModelIndex& p={}) const override {return p.isValid()?0:5;}
    QVariant data(const QModelIndex&,int role=Qt::DisplayRole) const override;
    QVariant headerData(int,Qt::Orientation,int) const override;
    const FunctionDescriptor* function(int row) const;
    const FunctionRegistry& registry() const {return registry_;}
    void setRegistry(FunctionRegistry registry) {beginResetModel();registry_=std::move(registry);endResetModel();}
private: FunctionRegistry registry_;
};
class ReferenceModel final : public QAbstractTableModel {
public:
    using QAbstractTableModel::QAbstractTableModel;
    int rowCount(const QModelIndex& p={}) const override {return p.isValid()?0:static_cast<int>(references_.size());}
    int columnCount(const QModelIndex& p={}) const override {return p.isValid()?0:3;}
    QVariant data(const QModelIndex&,int role=Qt::DisplayRole) const override;
    QVariant headerData(int,Qt::Orientation,int) const override;
    void setReferences(std::vector<Reference> refs) {beginResetModel();references_=std::move(refs);endResetModel();}
    const Reference* reference(int row) const {return row>=0 && static_cast<size_t>(row)<references_.size()?&references_[static_cast<size_t>(row)]:nullptr;}
private: std::vector<Reference> references_;
};
}

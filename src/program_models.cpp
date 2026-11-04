// SPDX-License-Identifier: GPL-3.0-or-later
#include "rea/program_models.hpp"
namespace rea {
namespace {QString hex(uint64_t a){return "0x"+QString::number(a,16);}}
QVariant FunctionModel::data(const QModelIndex& i,int role) const {
    if(!i.isValid() || i.model()!=this || i.row()<0 || i.row()>=rowCount() || i.column()<0 || i.column()>=5) return {};
    const auto& f=registry_.functions[static_cast<size_t>(i.row())];
    if(role==Qt::UserRole) return QVariant::fromValue<qulonglong>(f.entry);
    if(role!=Qt::DisplayRole) return {};
    switch(i.column()){case 0:return QString::fromStdString(f.name);case 1:return hex(f.entry);case 2:return functionConfidenceName(f.confidence);case 3:return QString::fromStdString(f.evidence);default:return f.overlap?"overlap":"";}
}
QVariant FunctionModel::headerData(int s,Qt::Orientation o,int r) const {
    if(o!=Qt::Horizontal || r!=Qt::DisplayRole || s<0 || s>=5) return {};
    return QStringList{"Function","ELF VA","Confidence","Evidence","Regions"}[s];
}
QVariant ReferenceModel::data(const QModelIndex& i,int role) const {
    auto* ref=reference(i.row());if(!i.isValid() || i.model()!=this || !ref || i.column()<0 || i.column()>=3) return {};
    if(role==Qt::UserRole) return QVariant::fromValue<qulonglong>(i.column()==0?ref->from:ref->to);
    if(role!=Qt::DisplayRole) return {};
    if(i.column()==0) return hex(ref->from);
    if(i.column()==1) return hex(ref->to);
    switch(ref->type){case ReferenceType::DirectCall:return "Direct call";case ReferenceType::DirectJump:return "Direct jump";case ReferenceType::CodeAddress:return "Address operand";default:return "Data address";}
}
QVariant ReferenceModel::headerData(int s,Qt::Orientation o,int r) const {
    if(o!=Qt::Horizontal || r!=Qt::DisplayRole || s<0 || s>=3) return {};
    return QStringList{"From (double-click)","To (double-click)","Type"}[s];
}
}

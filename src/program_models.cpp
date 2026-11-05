// SPDX-License-Identifier: GPL-3.0-or-later
#include "rea/program_models.hpp"
namespace rea {
namespace {QString hex(uint64_t a){return "0x"+QString::number(a,16);}}
const Symbol* SymbolModel::symbol(int row) const {return row>=0 && static_cast<size_t>(row)<symbols_.size()?&symbols_[static_cast<size_t>(row)]:nullptr;}
bool SymbolModel::isFunction(int row) const {const auto* s=symbol(row);return s && (s->info&15)==2 && s->section && s->section<sections_;}
void SymbolModel::setImage(const Image& image){beginResetModel();symbols_=image.symbols;sections_=image.sections.size();endResetModel();}
QVariant SymbolModel::data(const QModelIndex& i,int role) const {
    const auto* s=symbol(i.row());if(!i.isValid() || i.model()!=this || !s || i.column()<0 || i.column()>=8)return {};
    const auto original=QString::fromStdString(s->name),demangled=QString::fromStdString(s->demangledName);const auto display=demangled.isEmpty()?original:demangled;
    const auto binding=s->info>>4;const bool imported=!s->section && s->dynamic && !s->name.empty() && (binding==1 || binding==2);
    const QString classification=imported?"Imported / undefined":!s->section?"Undefined":s->section==0xfff1?"Absolute":s->section==0xfff2?"Common":"Defined";
    switch(role){case NavAddressRole:return QVariant::fromValue<qulonglong>(s->address);case NavOriginalRole:return original;case NavDemangledRole:return demangled;case NavSizeRole:return QVariant::fromValue<qulonglong>(s->size);case NavFunctionRole:return isFunction(i.row());case NavOriginRole:return "ELF symbol";case NavConfidenceRole:return "unknown";case NavSearchRole:return display+" "+original+" "+hex(s->address)+" "+QString::number(s->address,16);case Qt::ToolTipRole:return QString("<qt>%1<br/>%2<br/>%3</qt>").arg(original.toHtmlEscaped(),display.toHtmlEscaped(),classification.toHtmlEscaped());default:break;}
    if(role!=Qt::DisplayRole)return {};
    switch(i.column()){case 0:return display;case 1:return s->section?hex(s->address):QString("—");case 2:return hex(s->size);case 3:return s->dynamic?"Dynamic":"Static";case 4:return s->section;case 5:return hex(s->info);case 6:return hex(s->visibility);default:return classification;}
}
QVariant SymbolModel::headerData(int s,Qt::Orientation o,int r) const {if(o!=Qt::Horizontal || r!=Qt::DisplayRole || s<0 || s>=8)return {};return QStringList{"Name","Address","Size","Table","Section","Info","Visibility","Classification"}[s];}
const FunctionDescriptor* FunctionModel::function(int row) const {return row>=0 && static_cast<size_t>(row)<registry_.functions.size()?&registry_.functions[static_cast<size_t>(row)]:nullptr;}
QVariant FunctionModel::data(const QModelIndex& i,int role) const {
    const auto* f=function(i.row());if(!i.isValid() || i.model()!=this || !f || i.column()<0 || i.column()>=5)return {};
    const auto original=QString::fromStdString(f->name),demangled=QString::fromStdString(f->demangledName);const auto display=demangled.isEmpty()?original:demangled;
    const uint64_t size=f->declaredEnd>f->entry?f->declaredEnd-f->entry:f->end>f->entry?f->end-f->entry:0;
    switch(role){case NavAddressRole:return QVariant::fromValue<qulonglong>(f->entry);case NavOriginalRole:return original;case NavDemangledRole:return demangled;case NavSizeRole:return QVariant::fromValue<qulonglong>(size);case NavOriginRole:return functionOriginName(f->origin);case NavConfidenceRole:return functionConfidenceName(f->confidence);case NavFunctionRole:return true;case NavSearchRole:return display+" "+original+" "+hex(f->entry)+" "+QString::number(f->entry,16)+" "+QString::fromStdString(f->evidence)+" "+functionOriginName(f->origin)+" "+functionConfidenceName(f->confidence);case Qt::ToolTipRole:return QString("<qt>%1<br/>%2<br/>%3%4</qt>").arg(original.toHtmlEscaped(),QString::fromStdString(f->evidence).toHtmlEscaped(),f->declaredEnd>f->entry?"Size: ELF declaration":"Size: observed envelope, may contain gaps",f->overlap?"; overlapping regions":"");default:break;}
    if(role!=Qt::DisplayRole)return {};
    switch(i.column()){case 0:return display;case 1:return hex(f->entry);case 2:return size?QVariant(QVariant::fromValue<qulonglong>(size)):QVariant("Unknown");case 3:return functionOriginName(f->origin);default:return functionConfidenceName(f->confidence);}
}
QVariant FunctionModel::headerData(int s,Qt::Orientation o,int r) const {if(o!=Qt::Horizontal || r!=Qt::DisplayRole || s<0 || s>=5)return {};return QStringList{"Function","ELF VA","Size (bytes)","Origin","Confidence"}[s];}
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

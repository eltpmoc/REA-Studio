// SPDX-License-Identifier: GPL-3.0-or-later
#include "rea/instruction_model.hpp"
#include "rea/function_analysis.hpp"
#include <QColor>
#include <QFontDatabase>
#include <algorithm>
namespace rea {
int InstructionModel::rowCount(const QModelIndex& parent) const {return parent.isValid() ? 0 : static_cast<int>(result_.instructions.size());}
int InstructionModel::columnCount(const QModelIndex& parent) const {return parent.isValid() ? 0 : 5;}
const Instruction* InstructionModel::instruction(int row) const {
    if(row<0 || static_cast<size_t>(row)>=result_.instructions.size()) return nullptr;
    return &result_.instructions[static_cast<size_t>(row)];
}
QVariant InstructionModel::headerData(int section,Qt::Orientation orientation,int role) const {
    if(role!=Qt::DisplayRole) return {};
    if(orientation==Qt::Vertical) return section+1;
    static const QStringList names{"Address (ELF VA)","Bytes","Mnemonic","Operands","Comments / Evidence"};
    return section>=0 && section<names.size() ? QVariant(names[section]) : QVariant{};
}
QVariant InstructionModel::data(const QModelIndex& index,int role) const {
    const auto* ins=instruction(index.row());
    if(!index.isValid() || index.model()!=this || !ins || index.column()<0 || index.column()>=5) return {};
    auto count=[](const auto& index,uint64_t address){auto it=index.find(address);return it==index.end()?qulonglong{0}:static_cast<qulonglong>(it->second);};
    switch(role) {
    case FunctionEntryRole:return functions_.contains(ins->address);
    case FunctionNameRole:{auto it=functions_.find(ins->address);return it==functions_.end()?QVariant{}:QVariant(it->second);}
    case IncomingReferencesRole:return count(incoming_,ins->address);
    case OutgoingReferencesRole:return count(outgoing_,ins->address);
    case DarkRole:return dark_;
    case Qt::BackgroundRole:if(functions_.contains(ins->address)) return QColor(dark_?"#1c293d":"#e8eff9");return {};
    case AddressRole:return QVariant::fromValue<qulonglong>(ins->address);
    case OffsetRole:return QVariant::fromValue<qulonglong>(ins->offset);
    case FlowRole:return static_cast<int>(ins->flow);
    case ConfidenceRole:return static_cast<int>(ins->confidence);
    case TargetRole:return ins->directTarget ? QVariant::fromValue<qulonglong>(*ins->directTarget) : QVariant{};
    case Qt::FontRole:{auto font=QFontDatabase::systemFont(QFontDatabase::FixedFont);if(index.column()==2 || (index.column()==0 && functions_.contains(ins->address))) font.setBold(true);return font;}
    case Qt::ToolTipRole:return QString("VA: 0x%1\nFile offset: 0x%2\nSize: %3 bytes\nFlow: %4\n%5\n%6")
        .arg(ins->address,0,16).arg(ins->offset,0,16).arg(ins->size).arg(flowName(ins->flow)).arg(confidenceName(ins->confidence)).arg(QString::fromStdString(ins->evidence))
        +QString("\nXREF: %1 incoming / %2 outgoing").arg(count(incoming_,ins->address)).arg(count(outgoing_,ins->address));
    case Qt::ForegroundRole:
        if(ins->flow==FlowType::Invalid) return QColor(dark_ ? "#ff8a87" : "#a22020");
        if(index.column()==4) return QColor(dark_ ? "#96ac98" : "#386140");
        if(index.column()==0) return QColor(dark_ ? "#8caad2" : "#25548b");
        if(index.column()==1) return QColor(dark_ ? "#a0a9b7" : "#566373");
        if(index.column()==2) {
            if(ins->flow==FlowType::Call) return QColor(dark_ ? "#f0c87c" : "#805000");
            if(ins->flow!=FlowType::Next) return QColor(dark_ ? "#c4a1f5" : "#763b9f");
            return QColor(dark_ ? "#78cbd0" : "#14646a");
        }
        return {};
    case Qt::DisplayRole:
        switch(index.column()) {
        case 0:return QString("%1").arg(ins->address,16,16,QChar('0'));
        case 1:{QString bytes; for(auto b:ins->bytes) bytes+=QString("%1 ").arg(b,2,16,QChar('0'));return bytes.trimmed();}
        case 2:return QString::fromStdString(ins->mnemonic);
        case 3:return QString::fromStdString(ins->operands);
        case 4:{QString prefix;auto f=functions_.find(ins->address);if(f!=functions_.end()) prefix="ƒ "+f->second+"  │  ";
            auto incoming=count(incoming_,ins->address),outgoing=count(outgoing_,ins->address);
            if(incoming || outgoing) prefix+=QString("XREF ↓%1 ↑%2  │  ").arg(incoming).arg(outgoing);
            return prefix+comments_[static_cast<size_t>(index.row())];}
        }
    }
    return {};
}
void InstructionModel::setAnalysis(AnalysisResult result,const Image& image) {
    beginResetModel(); result_=std::move(result); comments_.clear();
    functions_.clear();incoming_.clear();outgoing_.clear();
    functions_[image.entry]="entry";
    for(const auto& symbol:image.symbols) if((symbol.info&15)==2 && symbol.section && symbol.section<image.sections.size()) functions_[symbol.address]=QString::fromStdString(symbol.name);
    SymbolResolver symbols(image);
    for(const auto& ref:result_.references) {++incoming_[ref.to];++outgoing_[ref.from];}
    for(const auto& ins:result_.instructions) {
        QString text=QString::fromStdString(symbols.at(ins.address));
        if(!text.isEmpty()) text+=" — ";
        text+=QString::fromUtf8(confidenceName(ins.confidence))+": "+QString::fromStdString(ins.evidence);
        if(ins.directTarget) {
            text+=" → 0x"+QString::number(*ins.directTarget,16);
            auto name=symbols.at(*ins.directTarget); if(!name.empty()) text+=" <"+QString::fromStdString(name)+">";
        }

        comments_.push_back(text);
    }
    endResetModel();
}
void InstructionModel::setFunctions(const FunctionRegistry& registry) {
    for(const auto& f:registry.functions) functions_[f.entry]=QString::fromStdString(f.name)+" ["+functionConfidenceName(f.confidence)+"]";
    if(rowCount()) emit dataChanged(index(0,0),index(rowCount()-1,4));
}
void InstructionModel::setDark(bool dark) {
    dark_=dark; if(rowCount()) emit dataChanged(index(0,0),index(rowCount()-1,4),{Qt::ForegroundRole,Qt::BackgroundRole,DarkRole});
}
int InstructionModel::rowForAddress(uint64_t va) const {
    auto it=std::lower_bound(result_.instructions.begin(),result_.instructions.end(),va,[](const Instruction& i,uint64_t v){return i.address<v;});
    return it!=result_.instructions.end() && it->address==va ? static_cast<int>(it-result_.instructions.begin()) : -1;
}
}

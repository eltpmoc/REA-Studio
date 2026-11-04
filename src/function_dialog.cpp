// SPDX-License-Identifier: GPL-3.0-or-later
#include "rea/function_dialog.hpp"
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableView>
#include <QVBoxLayout>
#include <QRegularExpression>
#include <map>
namespace rea {
FunctionSearchModel::FunctionSearchModel(FunctionModel* f,SymbolModel* s,QObject* parent):QAbstractTableModel(parent),functions_(f),symbols_(s){
    connect(f,&QAbstractItemModel::modelReset,this,[this]{rebuild();});connect(s,&QAbstractItemModel::modelReset,this,[this]{rebuild();});connect(f,&QAbstractItemModel::dataChanged,this,[this]{rebuild();});rebuild();
}
void FunctionSearchModel::rebuild(){
    beginResetModel();entries_.clear();std::map<uint64_t,size_t> byAddress;
    for(int row=0;row<functions_->rowCount();++row){auto index=functions_->index(row,0);Entry e;e.address=index.data(NavAddressRole).toULongLong();e.size=index.data(NavSizeRole).toULongLong();e.name=index.data().toString();e.search=index.data(NavSearchRole).toString();e.origin=index.data(NavOriginRole).toString();e.confidence=index.data(NavConfidenceRole).toString();byAddress[e.address]=entries_.size();entries_.push_back(std::move(e));}
    for(int row=0;row<symbols_->rowCount();++row){if(!symbols_->isFunction(row))continue;auto index=symbols_->index(row,0);auto address=index.data(NavAddressRole).toULongLong();auto found=byAddress.find(address);
        if(found!=byAddress.end()){entries_[found->second].search+=" "+index.data(NavSearchRole).toString();continue;}
        Entry e;e.address=address;e.size=index.data(NavSizeRole).toULongLong();e.name=index.data().toString();e.search=index.data(NavSearchRole).toString();e.origin="ELF symbol (not analyzed)";e.confidence="unknown";byAddress[address]=entries_.size();entries_.push_back(std::move(e));
    }
    std::sort(entries_.begin(),entries_.end(),[](const auto& a,const auto& b){return a.address<b.address;});endResetModel();
}
QVariant FunctionSearchModel::data(const QModelIndex& i,int role) const {
    if(!i.isValid() || i.model()!=this || i.row()<0 || i.row()>=rowCount() || i.column()<0 || i.column()>=5)return {};
    const auto& e=entries_[static_cast<size_t>(i.row())];
    if(role==NavAddressRole)return QVariant::fromValue<qulonglong>(e.address);
    if(role==NavSearchRole)return e.search;
    if(role==Qt::ToolTipRole)return QString("<qt>%1</qt>").arg(e.search.toHtmlEscaped());
    if(role!=Qt::DisplayRole)return {};
    switch(i.column()){case 0:return e.name;case 1:return "0x"+QString::number(e.address,16);case 2:return e.size?QVariant::fromValue<qulonglong>(e.size):QVariant("Unknown");case 3:return e.origin;default:return e.confidence;}
}
QVariant FunctionSearchModel::headerData(int s,Qt::Orientation o,int r) const {if(o!=Qt::Horizontal || r!=Qt::DisplayRole || s<0 || s>=5)return {};return QStringList{"Function / symbol","ELF VA","Size (bytes)","Origin","Confidence"}[s];}
void FunctionSearchFilter::setQuery(const QString& query){
#if QT_VERSION >= QT_VERSION_CHECK(6,10,0)
    beginFilterChange();
#endif
    words_=query.split(QRegularExpression("\\s+"),Qt::SkipEmptyParts);
#if QT_VERSION >= QT_VERSION_CHECK(6,10,0)
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
#else
    invalidateFilter();
#endif
}
bool FunctionSearchFilter::filterAcceptsRow(int row,const QModelIndex& parent) const {const auto text=sourceModel()->index(row,0,parent).data(NavSearchRole).toString();for(const auto& word:words_)if(!text.contains(word,Qt::CaseInsensitive))return false;return true;}
std::optional<uint64_t> GoToFunctionDialog::parseAddress(QString text){text=text.trimmed();if(text.startsWith("0x",Qt::CaseInsensitive))text=text.mid(2);if(text.isEmpty() || text.size()>16)return {};for(auto c:text)if(!c.isDigit() && (c.toLower()<'a' || c.toLower()>'f'))return {};bool ok=false;auto va=text.toULongLong(&ok,16);return ok?std::optional<uint64_t>(va):std::nullopt;}
GoToFunctionDialog::GoToFunctionDialog(FunctionModel* functions,SymbolModel* symbols,std::function<bool(uint64_t)> navigate,QWidget* parent):QDialog(parent),navigate_(std::move(navigate)){
    setObjectName("goToFunctionDialog");setWindowTitle("Go to Function — ELF addresses");resize(940,480);auto* layout=new QVBoxLayout(this);
    query_=new QLineEdit;query_->setObjectName("functionSearch");query_->setMaxLength(4096);query_->setPlaceholderText("Function, demangled C++ name, original symbol or ELF VA (hex)");layout->addWidget(query_);
    filter_=new FunctionSearchFilter(this);filter_->setSourceModel(new FunctionSearchModel(functions,symbols,this));view_=new QTableView;view_->setObjectName("functionSearchResults");view_->setModel(filter_);view_->setSelectionBehavior(QAbstractItemView::SelectRows);view_->setSelectionMode(QAbstractItemView::SingleSelection);view_->setEditTriggers(QAbstractItemView::NoEditTriggers);view_->setAlternatingRowColors(true);view_->horizontalHeader()->setStretchLastSection(true);view_->setColumnWidth(0,340);view_->setColumnWidth(1,145);view_->setColumnWidth(2,110);view_->setColumnWidth(3,190);layout->addWidget(view_);
    status_=new QLabel;status_->setTextFormat(Qt::PlainText);status_->setWordWrap(true);layout->addWidget(status_);auto* buttons=new QDialogButtonBox(QDialogButtonBox::Open|QDialogButtonBox::Cancel);layout->addWidget(buttons);buttons->button(QDialogButtonBox::Open)->setDefault(true);
    auto* direct=new QPushButton("Analyze ELF address");direct->setObjectName("analyzeFunctionAddress");buttons->addButton(direct,QDialogButtonBox::ActionRole);
    connect(direct,&QPushButton::clicked,this,[this]{auto address=parseAddress(query_->text());if(address && navigate_(*address))accept();else status_->setText("Enter a valid executable ELF address (hex). No analysis was started for an invalid address.");});
    connect(buttons,&QDialogButtonBox::accepted,this,[this]{activate();});connect(buttons,&QDialogButtonBox::rejected,this,&QDialog::reject);connect(view_,&QTableView::doubleClicked,this,[this]{activate();});
    connect(query_,&QLineEdit::textChanged,this,[this](const QString& query){filter_->setQuery(query);if(filter_->rowCount())view_->setCurrentIndex(filter_->index(0,0));else view_->setCurrentIndex({});status_->setText(QString("%1 functions. Unanalyzed executable addresses are analyzed on demand; global quota is preserved.").arg(filter_->rowCount()));});
    connect(filter_,&QAbstractItemModel::modelReset,this,[this]{if(filter_->rowCount())view_->setCurrentIndex(filter_->index(0,0));else view_->setCurrentIndex({});status_->setText(QString("%1 functions — updated from current analysis").arg(filter_->rowCount()));});
    if(filter_->rowCount())view_->setCurrentIndex(filter_->index(0,0));
    status_->setText("Search functions or enter an executable ELF address. Runtime process addresses are a separate workspace.");query_->setFocus();
}
void GoToFunctionDialog::activate(){
    std::optional<uint64_t> target;
    if(view_->currentIndex().isValid())target=view_->currentIndex().data(NavAddressRole).toULongLong();
    else target=parseAddress(query_->text());
    if(!target){status_->setText("Select a function or enter a complete hexadecimal ELF address.");return;}
    if(navigate_(*target))accept();else status_->setText("Address is not an executable ELF instruction boundary, or analysis is unavailable. See Log for details.");
}
}

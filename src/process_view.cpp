// SPDX-License-Identifier: GPL-3.0-or-later
#include "rea/process_view.hpp"
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QSignalBlocker>
#include <QRegularExpression>
#include <QFontDatabase>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QShowEvent>
#include <QSortFilterProxyModel>
#include <QSplitter>
#include <QTableView>
#include <QTimer>
#include <QVBoxLayout>
#include <QDateTime>
#include <QThread>
#include <algorithm>
#include <atomic>
namespace rea {
namespace {
QString utf(const std::string& s){return QString::fromUtf8(s.data(),static_cast<qsizetype>(s.size()));}
QString hx(uint64_t a){return "0x"+QString::number(a,16);}
}
const ProcessDescriptor* ProcessModel::process(int row) const {return row>=0 && static_cast<size_t>(row)<processes_.size()?&processes_[static_cast<size_t>(row)]:nullptr;}
QVariant ProcessModel::data(const QModelIndex& i,int role) const {
    const auto* p=process(i.row());if(!i.isValid() || i.model()!=this || !p || i.column()<0 || i.column()>=3) return {};
    if(role==Qt::UserRole) return p->pid;
    if(role==Qt::ToolTipRole) return QString("<qt>%1</qt>").arg((p->identity?utf(p->path):QString("Identity unavailable: protected, inaccessible or terminated process")).toHtmlEscaped());
    if(role!=Qt::DisplayRole) return {};
    if(i.column()==0) return p->pid;
    if(i.column()==1) return utf(p->name);
    return p->path.empty()?QString("Unavailable"):utf(p->path);
}
QVariant ProcessModel::headerData(int s,Qt::Orientation o,int r) const {if(o!=Qt::Horizontal || r!=Qt::DisplayRole || s<0 || s>=3) return {};return QStringList{"PID","Process","Executable path"}[s];}
const MemoryRegion* MemoryRegionModel::region(int row) const {return row>=0 && static_cast<size_t>(row)<regions_.size()?&regions_[static_cast<size_t>(row)]:nullptr;}
const MemoryRegion* MemoryRegionModel::containing(uint64_t a) const {
    auto i=std::upper_bound(regions_.begin(),regions_.end(),a,[](uint64_t va,const auto& r){return va<r.start;});
    if(i==regions_.begin()) return nullptr;
    --i;return a<i->end?&*i:nullptr;
}
QVariant MemoryRegionModel::data(const QModelIndex& i,int role) const {
    const auto* r=region(i.row());if(!i.isValid() || i.model()!=this || !r || i.column()<0 || i.column()>=4) return {};
    if(role==Qt::UserRole) return QVariant::fromValue<qulonglong>(r->start);
    if(role==Qt::ToolTipRole) return QString("Runtime VA %1–%2 (end exclusive), %3 bytes").arg(hx(r->start),hx(r->end)).arg(r->end-r->start);
    if(role!=Qt::DisplayRole) return {};
    switch(i.column()){case 0:return hx(r->start);case 1:return hx(r->end);case 2:return QString(r->readable?"r":"-")+(r->writable?"w":"-")+(r->executable?"x":"-");default:return utf(r->name);}
}
QVariant MemoryRegionModel::headerData(int s,Qt::Orientation o,int r) const {if(o!=Qt::Horizontal || r!=Qt::DisplayRole || s<0 || s>=4) return {};return QStringList{"Runtime start","End (exclusive)","Access","Mapping"}[s];}
struct ProcessJobState {
    std::atomic<bool> done{};
    std::vector<ProcessDescriptor> processes;
    std::vector<MemoryRegion> regions;
    std::shared_ptr<ProcessMemory> session;
    MemoryPage page;MemoryWriteResult patch;bool undo{};
    std::string error;
    uint64_t id{};
    int kind{};
};
ProcessView::ProcessView(QWidget* parent):QWidget(parent) {
    setObjectName("processMemoryView");auto* layout=new QVBoxLayout(this);
    auto* tools=new QHBoxLayout;auto* refresh=new QPushButton("Refresh processes");refresh->setObjectName("refreshProcesses");
    auto* search=new QLineEdit;search->setObjectName("processFilter");search->setPlaceholderText("Filter by PID, name or path");
    cancel_=new QPushButton("Cancel");cancel_->setEnabled(false);tools->addWidget(refresh);tools->addWidget(search,1);tools->addWidget(cancel_);layout->addLayout(tools);
    auto* split=new QSplitter;layout->addWidget(split,1);
    processTable_=new QTableView;processTable_->setObjectName("processTable");processes_=new ProcessModel(this);filter_=new QSortFilterProxyModel(this);filter_->setSourceModel(processes_);filter_->setFilterKeyColumn(-1);filter_->setFilterCaseSensitivity(Qt::CaseInsensitive);processTable_->setModel(filter_);split->addWidget(processTable_);
    auto* right=new QWidget;auto* memoryLayout=new QVBoxLayout(right);split->addWidget(right);split->setStretchFactor(0,1);split->setStretchFactor(1,2);
    opened_=new QLabel("Double-click a process to inspect its memory. Access follows OS permissions.");opened_->setTextFormat(Qt::PlainText);opened_->setWordWrap(true);memoryLayout->addWidget(opened_);
    auto* memorySplit=new QSplitter(Qt::Vertical);memoryLayout->addWidget(memorySplit,1);
    regionTable_=new QTableView;regionTable_->setObjectName("memoryRegions");regions_=new MemoryRegionModel(this);regionTable_->setModel(regions_);memorySplit->addWidget(regionTable_);
    auto* page=new QWidget;auto* pageLayout=new QVBoxLayout(page);memorySplit->addWidget(page);
    auto* navigation=new QHBoxLayout;auto* previous=new QPushButton("Previous page");auto* next=new QPushButton("Next page");address_=new QLineEdit;address_->setObjectName("memoryAddress");address_->setPlaceholderText("Runtime address (hex)");auto* go=new QPushButton("Go");
    navigation->addWidget(previous);navigation->addWidget(next);navigation->addWidget(address_,1);navigation->addWidget(go);pageLayout->addLayout(navigation);
    auto* controls=new QHBoxLayout;auto* reread=new QPushButton("Refresh memory");auto* remap=new QPushButton("Refresh regions");auto* live=new QCheckBox("Auto refresh (1 s)");live->setObjectName("liveMemoryRefresh");controls->addWidget(reread);controls->addWidget(remap);controls->addWidget(live);controls->addStretch();pageLayout->addLayout(controls);
    auto* editing=new QHBoxLayout;writeMode_=new QCheckBox("Enable writing");writeMode_->setObjectName("enableMemoryWriting");edit_=new QPushButton("Edit bytes…");edit_->setObjectName("editMemoryBytes");undo_=new QPushButton("Undo last write");undo_->setObjectName("undoMemoryWrite");editing->addWidget(writeMode_);editing->addWidget(edit_);editing->addWidget(undo_);editing->addStretch();pageLayout->addLayout(editing);
    connect(writeMode_,&QCheckBox::toggled,this,[this](bool enabled){if(!enableWriting(enabled)){QSignalBlocker guard(writeMode_);writeMode_->setChecked(session_ && session_->canWrite());}});
    connect(edit_,&QPushButton::clicked,this,[this]{editMemory();});connect(undo_,&QPushButton::clicked,this,[this]{undoLastWrite();});
    hex_=new QPlainTextEdit;hex_->setObjectName("processMemoryHex");hex_->setReadOnly(true);hex_->setLineWrapMode(QPlainTextEdit::NoWrap);hex_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));hex_->setMaximumBlockCount(300);pageLayout->addWidget(hex_);
    status_=new QLabel("Runtime virtual memory • read only • target remains running");status_->setObjectName("processMemoryStatus");status_->setTextFormat(Qt::PlainText);status_->setWordWrap(true);layout->addWidget(status_);
    for(auto* table:{processTable_,regionTable_}) {table->setSelectionBehavior(QAbstractItemView::SelectRows);table->setSelectionMode(QAbstractItemView::SingleSelection);table->setEditTriggers(QAbstractItemView::NoEditTriggers);table->setAlternatingRowColors(true);table->horizontalHeader()->setStretchLastSection(true);table->verticalHeader()->hide();}
    processTable_->setColumnWidth(0,75);processTable_->setColumnWidth(1,180);regionTable_->setColumnWidth(0,155);regionTable_->setColumnWidth(1,155);regionTable_->setColumnWidth(2,70);
    pollTimer_=new QTimer(this);pollTimer_->setInterval(15);liveTimer_=new QTimer(this);liveTimer_->setInterval(1000);
    connect(pollTimer_,&QTimer::timeout,this,[this]{poll();});
    connect(refresh,&QPushButton::clicked,this,[this]{refreshProcesses();});connect(cancel_,&QPushButton::clicked,this,[this]{cancel();});
    connect(search,&QLineEdit::textChanged,filter_,&QSortFilterProxyModel::setFilterFixedString);
    connect(processTable_,&QTableView::doubleClicked,this,[this](const QModelIndex& i){const auto* p=processes_->process(filter_->mapToSource(i).row());if(p) openProcess(*p);});
    connect(regionTable_,&QTableView::doubleClicked,this,[this](const QModelIndex& i){const auto* r=regions_->region(i.row());if(r) readAddress(r->start);});
    auto navigate=[this]{bool valid=false;auto text=address_->text().trimmed();auto va=text.toULongLong(&valid,16);if(!valid) status_->setText("Invalid runtime address");else readAddress(va);};
    connect(go,&QPushButton::clicked,this,navigate);connect(address_,&QLineEdit::returnPressed,this,navigate);
    connect(previous,&QPushButton::clicked,this,[this]{if(page_ && *page_>=4096) {const auto* r=regions_->containing(*page_);if(r) readAddress(std::max(r->start,*page_-4096));}});
    connect(next,&QPushButton::clicked,this,[this]{if(page_ && *page_<=UINT64_MAX-4096) readAddress(*page_+4096);});
    connect(reread,&QPushButton::clicked,this,[this]{if(page_) readAddress(*page_);});
    connect(remap,&QPushButton::clicked,this,[this]{if(session_) {Request r;r.kind=Job::Regions;r.session=session_;submit(std::move(r));}});
    connect(live,&QCheckBox::toggled,this,[this](bool enabled){if(enabled) liveTimer_->start();else liveTimer_->stop();});
    updateBusy();
    connect(liveTimer_,&QTimer::timeout,this,[this]{if(isVisible() && session_ && page_ && !busy()) readAddress(*page_);});
}
ProcessView::~ProcessView(){stop();}
void ProcessView::stop(){stopping_=true;liveTimer_->stop();cancel();if(worker_) {worker_->join();poll();}pollTimer_->stop();state_.reset();}
void ProcessView::showEvent(QShowEvent* e){QWidget::showEvent(e);if(!initialized_) {initialized_=true;refreshProcesses();}}
void ProcessView::copySelection(){hex_->copy();}
void ProcessView::refreshProcesses(){Request r;r.kind=Job::Enumerate;submit(std::move(r));}
void ProcessView::openProcess(const ProcessDescriptor& p){Request r;r.kind=Job::Open;r.process=p;submit(std::move(r));}
bool ProcessView::readAddress(uint64_t va){
    if(!session_) {status_->setText("Open a process first");return false;}
    const auto* region=regions_->containing(va);if(!region || !region->readable){status_->setText("Address is not in a readable mapped region. Refresh regions if the map changed.");return false;}
    Request r;r.kind=Job::Read;r.session=session_;r.address=va;r.count=static_cast<size_t>(std::min<uint64_t>(4096,region->end-va));return submit(std::move(r));
}
void ProcessView::cancel(){if(worker_ && state_ && state_->kind==static_cast<int>(Job::Write)){status_->setText("Write in progress: waiting for actual write/read-back result");return;}++latest_;pending_.reset();if(worker_) worker_->request_stop();status_->setText("Process inspection cancelled");updateBusy();}
void ProcessView::updateBusy(){const bool writing=worker_ && state_ && state_->kind==static_cast<int>(Job::Write);cancel_->setEnabled(busy() && !writing);writeMode_->setEnabled(!busy() && session_!=nullptr);edit_->setEnabled(!busy() && session_ && session_->canWrite() && !lastPage_.bytes.empty());undo_->setEnabled(!busy() && lastPatch_.has_value());}
bool ProcessView::submit(Request r){
    if(worker_ && state_ && state_->kind==static_cast<int>(Job::Write)){status_->setText("Wait for the write/read-back result before another operation");return false;}
    if(stopping_) return false;
    Q_ASSERT(QThread::currentThread()==thread());r.id=++latest_;status_->setText(r.kind==Job::Enumerate?"Listing processes…":r.kind==Job::Write?"Writing process memory…":"Reading process memory…");
    if(worker_){worker_->request_stop();pending_=std::move(r);}else launch(std::move(r));updateBusy();return true;
}
void ProcessView::launch(Request request){
    state_=std::make_shared<ProcessJobState>();state_->id=request.id;state_->kind=static_cast<int>(request.kind);auto state=state_;
    worker_=std::make_unique<std::jthread>([request=std::move(request),state](std::stop_token token){
        try {
            if(token.stop_requested()) throw std::runtime_error("Cancelled");
            if(request.kind==Job::Enumerate) state->processes=enumerateProcesses(token);
            else {
                state->session=request.kind==Job::Open?std::make_shared<ProcessMemory>(request.process,request.writable):request.session;
                if(request.kind==Job::Write) {state->undo=request.undo;state->patch=state->session->write(request.address,request.expected,request.replacement,token);}
                else if(request.kind==Job::Read) state->page=state->session->read(request.address,request.count,token);
                else state->regions=state->session->regions(token);
            }
        }catch(const std::exception& e){state->error=e.what();}catch(...){state->error="Unknown process inspection failure";}
        state->done.store(true,std::memory_order_release);
    });pollTimer_->start();
}
void ProcessView::poll(){
    if(!state_ || !worker_ || !state_->done.load(std::memory_order_acquire)) return;
    if(worker_->joinable()) worker_->join();
    worker_.reset();pollTimer_->stop();auto result=std::move(state_);
    auto next=std::move(pending_);pending_.reset();
    if(result->id==latest_) {
        if(!result->error.empty()) {status_->setText(utf(result->error));QSignalBlocker guard(writeMode_);writeMode_->setChecked(session_ && session_->canWrite());}
        else if(result->kind==static_cast<int>(Job::Enumerate)) {processes_->setProcesses(std::move(result->processes));status_->setText(QString("%1 processes visible to this OS session. Double-click to inspect memory (read only).").arg(processes_->rowCount()));}
        else if(result->kind==static_cast<int>(Job::Write)) {
            showPage({result->patch.address,result->patch.after,result->patch.error});
            status_->setText(QString("PID %1: %2 bytes written; read-back %3%4").arg(session_->process().pid).arg(result->patch.written).arg(result->patch.verified?"verified":"NOT verified").arg(result->patch.error.empty()?"":" • "+utf(result->patch.error)));
            if(result->patch.verified){if(result->undo)lastPatch_.reset();else lastPatch_=std::move(result->patch);}
        }
        else if(result->kind==static_cast<int>(Job::Read)) showPage(result->page);
        else {
            session_=std::move(result->session);regions_->setRegions(std::move(result->regions));hex_->clear();page_.reset();lastPage_={};lastPatch_.reset();
            {QSignalBlocker guard(writeMode_);writeMode_->setChecked(session_->canWrite());}
            opened_->setText(QString("PID %1 — %2 • Runtime addresses • Read only").arg(session_->process().pid).arg(utf(session_->process().name)));
            opened_->setText(opened_->text().replace("Read only",session_->canWrite()?"Read / Write":"Read only"));
            const MemoryRegion* first=nullptr;
            for(int n=0;n<regions_->rowCount();++n){auto* r=regions_->region(n);if(r->readable && (!first || r->executable)){first=r;if(r->executable) break;}}
            status_->setText(QString("%1 mapped regions").arg(regions_->rowCount()));
            if(first && !stopping_) readAddress(first->start);else status_->setText("No readable region available; process may have terminated.");
        }
    }
    if(next && next->id==latest_ && !worker_) launch(std::move(*next));
    updateBusy();
}
void ProcessView::showPage(const MemoryPage& page){
    lastPage_=page;page_=page.address;address_->setText(hx(page.address));QString text;
    for(size_t n=0;n<page.bytes.size();n+=16){
        text+=QString("%1  ").arg(page.address+n,16,16,QChar('0'));QString ascii;
        for(size_t j=0;j<16;++j){if(n+j<page.bytes.size()){auto c=page.bytes[n+j];text+=QString("%1 ").arg(c,2,16,QChar('0'));ascii+=QChar(c>=32 && c<=126?c:'.');}else text+="   ";}
        text+=" |"+ascii+"|\n";
    }
    hex_->setPlainText(text);
    status_->setText(QString("PID %1 • %2 • %3 bytes read • %4%5").arg(session_->process().pid).arg(hx(page.address)).arg(page.bytes.size()).arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz")).arg(page.error.empty()?"":" • "+utf(page.error)));
}
}

namespace rea {
bool ProcessView::enableWriting(bool enabled){
    if(busy() || !session_) return false;
    Request r;r.kind=Job::Open;r.process=session_->process();r.writable=enabled;return submit(std::move(r));
}
bool ProcessView::writeMemory(uint64_t address,std::vector<uint8_t> expected,std::vector<uint8_t> replacement){
    if(busy() || !session_ || !session_->canWrite()) return false;
    Request r;r.kind=Job::Write;r.session=session_;r.address=address;r.expected=std::move(expected);r.replacement=std::move(replacement);return submit(std::move(r));
}
bool ProcessView::undoLastWrite(){
    if(busy() || !lastPatch_ || !session_) return false;
    Request r;r.kind=Job::Write;r.session=session_;r.address=lastPatch_->address;r.expected=lastPatch_->after;r.replacement=lastPatch_->before;r.undo=true;return submit(std::move(r));
}
void ProcessView::editMemory(){
    if(busy() || !session_ || !session_->canWrite() || lastPage_.bytes.empty()) return;
    QDialog dialog(this);dialog.setObjectName("memoryPatchDialog");dialog.setWindowTitle("Edit process memory");auto* form=new QFormLayout(&dialog);
    auto* target=new QLabel(QString("PID %1 — %2").arg(session_->process().pid).arg(hx(lastPage_.address)));target->setTextFormat(Qt::PlainText);form->addRow(target);
    auto* note=new QLabel("Target remains running. Comparison/write is not atomic. Only writable regions are accepted.");note->setWordWrap(true);form->addRow(note);
    const auto count=std::min<size_t>(lastPage_.bytes.size(),32);QString original;for(size_t n=0;n<count;++n)original+=QString("%1 ").arg(lastPage_.bytes[n],2,16,QChar('0'));
    auto* before=new QLineEdit(original.trimmed());before->setReadOnly(true);auto* after=new QLineEdit(original.trimmed());after->setObjectName("memoryReplacement");after->setMaxLength(3*4096);
    form->addRow("Original bytes",before);form->addRow("Replacement hex",after);auto* error=new QLabel;error->setTextFormat(Qt::PlainText);form->addRow(error);
    connect(after,&QLineEdit::textChanged,&dialog,[before,cached=lastPage_](QString text){text.remove(QRegularExpression("\\s"));QString preview;const auto n=std::min<size_t>(cached.bytes.size(),static_cast<size_t>(text.size()/2));for(size_t i=0;i<n;++i)preview+=QString("%1 ").arg(cached.bytes[i],2,16,QChar('0'));before->setText(preview.trimmed());});
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Apply|QDialogButtonBox::Cancel);form->addRow(buttons);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    const auto cached=lastPage_;connect(buttons->button(QDialogButtonBox::Apply),&QPushButton::clicked,&dialog,[&,cached]{
        QString hex=after->text();hex.remove(QRegularExpression("\\s"));
        static const QRegularExpression valid("^[0-9a-fA-F]+$");
        if(hex.isEmpty() || hex.size()%2 || !valid.match(hex).hasMatch() || hex.size()/2>static_cast<qsizetype>(cached.bytes.size())){error->setText("Enter complete hexadecimal byte pairs within the loaded page.");return;}
        auto data=QByteArray::fromHex(hex.toLatin1());std::vector<uint8_t> replacement(data.begin(),data.end());std::vector<uint8_t> expected(cached.bytes.begin(),cached.bytes.begin()+data.size());
        if(writeMemory(cached.address,std::move(expected),std::move(replacement))) dialog.accept();else error->setText("Session busy or writing disabled; refresh and retry.");
    });dialog.resize(660,220);const bool wasLive=liveTimer_->isActive();liveTimer_->stop();dialog.exec();if(wasLive && !stopping_)liveTimer_->start();
}
}

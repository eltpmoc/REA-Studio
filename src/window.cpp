// SPDX-License-Identifier: GPL-3.0-or-later
#include "rea/window.hpp"
#include "rea/function_dialog.hpp"
#include "rea/process_view.hpp"
#include "rea/assembly_delegate.hpp"
#include "rea/theme.hpp"
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QEventLoop>
#include <QFormLayout>
#include <QSpinBox>
#include <QDockWidget>
#include <QFileDialog>
#include <QFontDatabase>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSettings>
#include <QStatusBar>
#include <QTableView>
#include <QTableWidget>
#include <QTabWidget>
#include <QToolBar>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <algorithm>
#include <set>
namespace rea {
namespace {
QString hx(uint64_t v) {return "0x"+QString::number(v,16);}
QString utf(const std::string& s) {return QString::fromUtf8(s.data(),static_cast<qsizetype>(s.size()));}
QPlainTextEdit* textView(const char* name) {
    auto* t=new QPlainTextEdit; t->setObjectName(name); t->setReadOnly(true);
    t->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont)); t->setMaximumBlockCount(5000); return t;
}
QTableWidget* table(const char* name,QStringList headers) {
    auto* t=new QTableWidget(0,static_cast<int>(headers.size())); t->setObjectName(name); t->setHorizontalHeaderLabels(headers);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers); t->setSelectionBehavior(QAbstractItemView::SelectRows);
    t->setAlternatingRowColors(true); t->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    t->horizontalHeader()->setStretchLastSection(true); return t;
}
void row(QTableWidget* t,const QStringList& values) {
    int r=t->rowCount(); t->insertRow(r);
    for(int i=0;i<values.size();++i) t->setItem(r,i,new QTableWidgetItem(values[i]));
}
}
Window::Window(bool persistentSettings) : persistent_(persistentSettings) {
    setObjectName("reaWorkspace"); setWindowTitle(QString("REA Studio %1").arg(REA_VERSION)); resize(1600,980);
    setDockNestingEnabled(true); setDockOptions(AllowNestedDocks|AllowTabbedDocks|AnimatedDocks);
    setTabPosition(Qt::AllDockWidgetAreas,QTabWidget::North);
    auto* fileMenu=menuBar()->addMenu("&File");
    auto* open=fileMenu->addAction("&Open binary…",QKeySequence::Open,this,[this]{
        auto path=QFileDialog::getOpenFileName(this,"Open binary"); if(!path.isEmpty()) openFileAsync(path);
    });
    auto* processesAction=fileMenu->addAction("Open process memory…");processesAction->setObjectName("openProcessMemoryAction");
    fileMenu->addAction("Quit",QKeySequence::Quit,this,&QWidget::close);
    auto* view=menuBar()->addMenu("&View");
    auto* themes=menuBar()->addMenu("&Theme");
    themes->addAction("Dark",this,[this]{setDarkTheme(true);});
    themes->addAction("Light",this,[this]{setDarkTheme(false);});
    auto* nav=menuBar()->addMenu("&Navigate");
    goToFunctionAction_=nav->addAction("Go to Function…",QKeySequence("Ctrl+Shift+F"),this,[this]{goToFunction();});goToFunctionAction_->setObjectName("goToFunctionAction");goToFunctionAction_->setEnabled(false);
    auto* back=nav->addAction("Back",QKeySequence("Alt+Left"),this,[this]{goBack();}); back->setObjectName("backAction");
    auto* forward=nav->addAction("Forward",QKeySequence("Alt+Right"),this,[this]{goForward();}); forward->setObjectName("forwardAction");
    auto* entry=nav->addAction("Entry point",this,[this]{if(file_) navigateToAddress(image_.entry);});
    nav->addAction("File offset…",QKeySequence("Ctrl+G"),this,[this]{navigate(false);});
    nav->addAction("ELF virtual address…",QKeySequence("Ctrl+L"),this,[this]{navigate(true);});
    auto* follow=nav->addAction("Follow direct target",QKeySequence(Qt::Key_Return),this,[this]{followSelected();});
    auto* copy=nav->addAction("Copy selected Assembly",QKeySequence::Copy,this,[this]{copySelection();}); copy->setObjectName("copyAction");
    auto* analysisMenu=menuBar()->addMenu("&Analysis");
    auto* cancel=analysisMenu->addAction("Cancel analysis",this,[this]{cancelAnalysis();});
    cancel->setObjectName("cancelAnalysisAction");cancel->setEnabled(false);
    analysisMenu->addAction("Resource limits…",this,[this]{configureLimits();});
    auto* toolbar=addToolBar("Navigation"); toolbar->setObjectName("navigationToolbar");
    toolbar->addAction(open); toolbar->addSeparator(); toolbar->addAction(back); toolbar->addAction(forward); toolbar->addAction(entry); toolbar->addAction(follow); toolbar->addSeparator();toolbar->addAction(cancel);
    toolbarProgress_=new QProgressBar;toolbarProgress_->setObjectName("analysisToolbarProgress");
    toolbarProgress_->setFixedWidth(150);toolbarProgress_->setRange(0,100);toolbarProgress_->setValue(0);toolbarProgressAction_=toolbar->addWidget(toolbarProgress_);
    auto dock=[this,view](const QString& title,QWidget* widget,Qt::DockWidgetArea area){
        auto* d=new QDockWidget(title,this); d->setObjectName(title); d->setWidget(widget); addDockWidget(area,d);
        view->addAction(d->toggleViewAction()); return d;
    };
    centralTabs_=new QTabWidget; centralTabs_->setObjectName("centralTabs"); setCentralWidget(centralTabs_);
    auto* assemblyPage=new QWidget; auto* assemblyLayout=new QVBoxLayout(assemblyPage); assemblyLayout->setContentsMargins(0,0,0,0);
    analysisState_=new QLabel("No binary loaded. Bytes outside analysis remain unanalyzed."); analysisState_->setWordWrap(true);
    assemblyLayout->addWidget(analysisState_);
    assembly_=new QTableView; assembly_->setObjectName("assemblyView"); instructions_=new InstructionModel(this); assembly_->setModel(instructions_);
    assembly_->setItemDelegate(new AssemblyDelegate(assembly_));assembly_->setShowGrid(false);
    assembly_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont)); assembly_->setSelectionBehavior(QAbstractItemView::SelectRows);
    assembly_->setSelectionMode(QAbstractItemView::ExtendedSelection); assembly_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    assembly_->setAlternatingRowColors(true); assembly_->setWordWrap(false); assembly_->verticalHeader()->hide();
    assembly_->verticalHeader()->setDefaultSectionSize(24); assembly_->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    assembly_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel); assembly_->horizontalHeader()->setStretchLastSection(true);
    for(int c=0;c<5;++c) assembly_->setColumnWidth(c,c==0 ? 175 : c==1 ? 205 : c==2 ? 100 : c==3 ? 260 : 500);
    assemblyLayout->addWidget(assembly_); centralTabs_->addTab(assemblyPage,"Disassembly View");
    hex_=textView("hexView"); hex_->setLineWrapMode(QPlainTextEdit::NoWrap);
    auto* hexPage=new QWidget; auto* hexLayout=new QVBoxLayout(hexPage); auto* buttons=new QHBoxLayout;
    auto* prev=new QPushButton("Previous 4 KiB"); auto* next=new QPushButton("Next 4 KiB"); hexPosition_=new QLabel("No file loaded");
    buttons->addWidget(prev); buttons->addWidget(next); buttons->addWidget(hexPosition_); buttons->addStretch(); hexLayout->addLayout(buttons); hexLayout->addWidget(hex_);
    centralTabs_->addTab(hexPage,"Hex Viewer");
    connect(prev,&QPushButton::clicked,this,[this]{if(file_) navigateToOffset(page_>=4096 ? page_-4096 : 0);});
    connect(next,&QPushButton::clicked,this,[this]{if(file_ && file_->size()-page_>4096) navigateToOffset(page_+4096);});
    functions_=new QTableView; functions_->setObjectName("functionsView");
    functionModel_=new FunctionModel(this);functions_->setModel(functionModel_);functions_->setSelectionBehavior(QAbstractItemView::SelectRows);
    graph_=new GraphView;centralTabs_->addTab(graph_,"Graph View");
    referencesView_=new QTableView;referencesView_->setObjectName("referencesView");referenceModel_=new ReferenceModel(this);referencesView_->setModel(referenceModel_);
    referencesView_->horizontalHeader()->setStretchLastSection(true);centralTabs_->addTab(referencesView_,"Cross References");
    irText_=textView("irView");pseudocode_=textView("pseudocodeView");irText_->setMaximumBlockCount(0);pseudocode_->setMaximumBlockCount(0);centralTabs_->addTab(irText_,"IR (experimental)");centralTabs_->addTab(pseudocode_,"Pseudocode (experimental)");
    processView_=new ProcessView;centralTabs_->addTab(processView_,"Process Memory");
    connect(processesAction,&QAction::triggered,this,[this]{centralTabs_->setCurrentWidget(processView_);});
    fileMenu->addAction("Export graph PNG…",this,[this]{auto p=QFileDialog::getSaveFileName(this,"Export graph PNG",{},"PNG (*.png)");if(!p.isEmpty() && !graph_->exportPng(p)) log("Graph PNG export failed or exceeds pixel limit.");});
    fileMenu->addAction("Export graph SVG…",this,[this]{auto p=QFileDialog::getSaveFileName(this,"Export graph SVG",{},"SVG (*.svg)");if(!p.isEmpty() && !graph_->exportSvg(p)) log("Graph SVG export failed.");});
    connect(graph_,&GraphView::instructionActivated,this,[this](qulonglong a){navigateToAddress(a);});
    connect(referencesView_,&QTableView::doubleClicked,this,[this](const QModelIndex& i){auto* r=referenceModel_->reference(i.row());if(r) navigateToAddress(i.column()==0?r->from:r->to);});
    auto* functionsDock=dock("Functions",functions_,Qt::LeftDockWidgetArea);
    symbolModel_=new SymbolModel(this);
    symbols_=table("symbolsView",{"Name","Address","Size","Table","Section","Info","Visibility","Classification"});
    symbols_->horizontalHeader()->setSectionsMovable(true);symbols_->horizontalHeader()->moveSection(7,1);
    symbols_->setColumnWidth(0,150);symbols_->setColumnWidth(7,175);
    auto* symbolsDock=dock("Symbols",symbols_,Qt::LeftDockWidgetArea); tabifyDockWidget(functionsDock,symbolsDock);
    auto* strings=new QLabel("String extraction is not implemented."); strings->setMargin(12);
    auto* stringsDock=dock("Strings",strings,Qt::LeftDockWidgetArea); tabifyDockWidget(functionsDock,stringsDock);
    sections_=table("sectionsView",{"Name","Type","Virtual address","File offset","Size","Flags (W/A/X + raw)"});
    auto* sectionsDock=dock("Sections",sections_,Qt::LeftDockWidgetArea); tabifyDockWidget(functionsDock,sectionsDock);
    project_=new QTreeWidget; project_->setObjectName("projectView"); project_->setHeaderLabel("Project — no file loaded");
    auto* projectDock=dock("Project Explorer",project_,Qt::LeftDockWidgetArea); tabifyDockWidget(functionsDock,projectDock); functionsDock->raise();
    info_=textView("fileInformation"); info_->setPlainText("No executable loaded.");
    auto* fileDock=dock("File Information",info_,Qt::RightDockWidgetArea);
    addressInfo_=textView("addressInformation"); addressInfo_->setPlainText("Select an instruction or navigate to an address.");
    auto* addressDock=dock("Address Information",addressInfo_,Qt::RightDockWidgetArea);
    splitDockWidget(fileDock,addressDock,Qt::Vertical);
    instructionInfo_=textView("instructionInformation"); instructionInfo_->setPlainText("No instruction selected.");
    auto* instructionDock=dock("Instruction Information",instructionInfo_,Qt::RightDockWidgetArea); tabifyDockWidget(addressDock,instructionDock); instructionDock->raise();
    history_=new QListWidget; history_->setObjectName("navigationHistory");
    auto* historyDock=dock("Navigation History",history_,Qt::RightDockWidgetArea); splitDockWidget(addressDock,historyDock,Qt::Vertical);
    console_=textView("analysisConsole"); console_->setPlainText("Static analysis reports only. No commands or binaries are executed.");
    auto* consoleDock=dock("Analysis Console",console_,Qt::BottomDockWidgetArea);
    logs_=textView("logView"); auto* logsDock=dock("Log Viewer",logs_,Qt::BottomDockWidgetArea); tabifyDockWidget(consoleDock,logsDock);
    auto* progressWidget=new QWidget; auto* progressLayout=new QVBoxLayout(progressWidget);
    progressDetails_=new QLabel("Ready — configure limits in Analysis → Resource limits");
    progressDetails_->setObjectName("analysisProgressDetails");progressLayout->addWidget(progressDetails_);
    progress_=new QProgressBar; progress_->setObjectName("analysisProgress"); progress_->setRange(0,100); progress_->setValue(0); progressLayout->addWidget(progress_);
    progressDock_=dock("Progress",progressWidget,Qt::BottomDockWidgetArea); tabifyDockWidget(consoleDock,progressDock_); consoleDock->raise();
    connect(assembly_,&QTableView::doubleClicked,this,[this](const QModelIndex&){followSelected();});
    connect(assembly_->selectionModel(),&QItemSelectionModel::currentChanged,this,[this]{updateSelection();});
    assembly_->setContextMenuPolicy(Qt::ActionsContextMenu); assembly_->addAction(copy); assembly_->addAction(follow);
    connect(history_,&QListWidget::itemActivated,this,[this](QListWidgetItem* item){
        int position=history_->row(item);
        if(position>=0 && static_cast<size_t>(position)<locations_.size() && go(locations_[static_cast<size_t>(position)],false)) {
            historyPosition_=position; history_->setCurrentRow(position);
        }
    });
    connect(functions_,&QTableView::doubleClicked,this,[this](const QModelIndex& i){navigateToAddress(functionModel_->data(i,Qt::UserRole).toULongLong());});
    connect(symbols_,&QTableWidget::cellDoubleClicked,this,[this](int r,int){
        if(r<0 || static_cast<size_t>(r)>=image_.symbols.size()) return;
        const auto& symbol=image_.symbols[static_cast<size_t>(r)];
        if(!symbol.section || !navigateToAddress(symbol.address)) log("Symbol has no mapped definition.");
    });
    connect(sections_,&QTableWidget::cellDoubleClicked,this,[this](int r,int){
        if(r<0 || static_cast<size_t>(r)>=image_.sections.size()) return;
        const auto& s=image_.sections[static_cast<size_t>(r)];
        if(file_ && s.type!=8 && s.type!=0 && s.size && s.offset<file_->size()) navigateToOffset(s.offset);
        else log("Section has no file-backed bytes.");
    });
    controller_=new AnalysisController(this);controller_->setObjectName("analysisController");
    connect(controller_,&AnalysisController::busyChanged,this,[this,cancel](bool busy){
        cancel->setEnabled(busy);toolbarProgressAction_->setVisible(busy);toolbarProgress_->setVisible(busy);
        if(busy) progressDock_->show();else progressDock_->hide();
    });
    connect(controller_,&AnalysisController::progress,this,[this](quint64 id,int percent,const QString& phase,quint64 count,quint64 bytes,quint64 ms){
        if(id!=requestId_ || closing_) return;
        progress_->setValue(percent);toolbarProgress_->setValue(percent);
        progressDetails_->setText(QString("%1 | %2 instructions | %3 MiB reserved | %4 ms").arg(phase).arg(count).arg(double(bytes)/(1024*1024),0,'f',2).arg(ms));
        statusBar()->showMessage(phase);
    });
    connect(controller_,&AnalysisController::completed,this,&Window::finishJob);
    resizeDocks({functionsDock,fileDock},{320,330},Qt::Horizontal);
    resizeDocks({consoleDock},{150},Qt::Vertical);
    if(persistent_) {
        QSettings settings; restoreGeometry(settings.value("workspace/v2/geometry").toByteArray());
        restoreState(settings.value("workspace/v2/layout").toByteArray(),2);
        setDarkTheme(settings.value("workspace/v2/dark",true).toBool());
    } else setDarkTheme(true);
    progressDock_->hide();toolbarProgressAction_->setVisible(false);toolbarProgress_->hide();
    statusBar()->showMessage("Ready — x86-64 static disassembly / no decompilation");
}
void Window::setDarkTheme(bool dark) {
    dark_=dark;applyWorkspaceTheme(dark);
    instructions_->setDark(dark);
}
Window::~Window() {delete controller_;controller_=nullptr;}
void Window::closeEvent(QCloseEvent* event) {
    closing_=true;controller_->cancel();processView_->stop();
    if(persistent_) {
        QSettings settings; settings.setValue("workspace/v2/geometry",saveGeometry());
        settings.setValue("workspace/v2/layout",saveState(2)); settings.setValue("workspace/v2/dark",dark_);
    }
    QMainWindow::closeEvent(event);
}
void Window::log(const QString& message) {logs_->appendPlainText(QDateTime::currentDateTime().toString(Qt::ISODate)+"  "+message);}
bool Window::openFile(const QString& path,bool interactive) {
    // Compatibility API: work still runs on a dedicated worker; only the caller waits in a local event loop.
    QEventLoop loop;
    auto id=openFileAsync(path,interactive);
    bool success=false;
    auto connection=connect(controller_,&AnalysisController::completed,&loop,[&](quint64 completed,AnalysisOutcomePtr){
        if(completed==id) {success=requestId_==id && lastJobSuccess_;loop.quit();}
    });
    loop.exec(QEventLoop::ExcludeUserInputEvents);disconnect(connection);return success;
}
quint64 Window::openFileAsync(const QString& path,bool interactive) {return startJob(path,std::nullopt,interactive);}
bool Window::analysisBusy() const {return controller_->busy();}
void Window::cancelAnalysis() {controller_->cancel();statusBar()->showMessage("Cancellation requested…");}
quint64 Window::startJob(const QString& path,std::optional<uint64_t> navigation,bool interactive) {
    closing_=false;requestPath_=path;requestedNavigation_=navigation;requestInteractive_=interactive;lastJobSuccess_=false;
    requestQuota_=navigation ? projectQuota_ : std::make_shared<InstructionBudget>(limits_.maxInstructions);
    progress_->setFormat("%p%");toolbarProgress_->setFormat("%p%");
    progress_->setValue(0);toolbarProgress_->setValue(0);progressDetails_->setText("Analysis queued…");
    requestId_=controller_->start(path,limits_,navigation,requestQuota_,navigation?instructions_->snapshot():std::shared_ptr<const AnalysisResult>{});return requestId_;
}
void Window::finishJob(quint64 id,AnalysisOutcomePtr outcome) {
    if(id!=requestId_ || closing_) return;
    lastJobSuccess_=false;
    auto description=outcome->error.isEmpty() ? QString::fromUtf8(stopReasonName(outcome->reason)) : outcome->error;
    progressDetails_->setText(QString("%1 | %2 instructions globally | %3 MiB reserved | %4 ms")
        .arg(description).arg(outcome->instructions).arg(double(outcome->reservedBytes)/(1024*1024),0,'f',2).arg(outcome->elapsedMs));
    if(outcome->reason==StopReason::Cancelled) {
        progress_->setFormat("Cancelled");toolbarProgress_->setFormat("Cancelled");
        log("Analysis cancelled; previous project preserved.");statusBar()->showMessage("Analysis cancelled");return;
    }
    if(!outcome->error.isEmpty() || !outcome->hasImage) {
        log("Open failed: "+description);progress_->setFormat("Stopped");toolbarProgress_->setFormat("Stopped");
        statusBar()->showMessage("Analysis failed: "+description);
        if(requestInteractive_) QMessageBox::critical(this,"Cannot analyze binary",description);
        return;
    }
    if(requestedNavigation_) {
        const auto target=*requestedNavigation_;auto row=std::lower_bound(outcome->analysis.instructions.begin(),outcome->analysis.instructions.end(),target,[](const Instruction& i,uint64_t va){return i.address<va;});
        const auto* function=outcome->program.registry.containing(target);
        if(outcome->program.irStopReason!=StopReason::None || row==outcome->analysis.instructions.end() || row->address!=target || row->flow==FlowType::Invalid || !function || !outcome->program.graphs.contains(function->entry) || outcome->analysis.instructions.size()<static_cast<size_t>(instructions_->rowCount())){
            const QString why=outcome->reason!=StopReason::None?description:row==outcome->analysis.instructions.end() || row->address!=target?"No decoded instruction at requested address":row->flow==FlowType::Invalid?"Invalid encoding at requested address":"Derived function analysis unavailable";
            log(why+"; previous analysis preserved.");for(const auto& diagnostic:outcome->analysis.diagnostics)log(utf(diagnostic));
            statusBar()->showMessage("Go to Function stopped: "+why+" — previous analysis preserved");return;
        }
    }
    applyingResult_=true;
    lastJobSuccess_=commitOutcome(requestPath_,outcome);
    applyingResult_=false;
    progress_->setFormat("%p%");toolbarProgress_->setFormat("%p%");
    if(lastJobSuccess_) {
        loadedPath_=requestPath_;projectQuota_=requestQuota_;
        if(outcome->reason==StopReason::None) {progress_->setValue(100);toolbarProgress_->setValue(100);}
        else {
            log(description+"; partial analysis only. Remaining bytes are unanalyzed.");
            analysisState_->setText(analysisState_->text()+" | "+description);
            statusBar()->showMessage(description+" — partial analysis");
        }
    }
}
bool Window::commitOutcome(const QString& path,AnalysisOutcomePtr outcome) {
    try {
        file_=std::move(outcome->file);image_=std::move(outcome->image);
        program_=std::move(outcome->program);displayedFunction_.reset();
        symbolModel_->setImage(image_);goToFunctionAction_->setEnabled(true);
        instructions_->setAnalysis(std::move(outcome->analysis),image_);
        if(!requestedNavigation_) {locations_.clear(); historyPosition_=-1; history_->clear();}
        assembly_->clearSelection();
        sections_->setRowCount(0); symbols_->setRowCount(0); project_->clear();
        instructionInfo_->setPlainText("No instruction selected."); addressInfo_->clear();
        auto* root=new QTreeWidgetItem(project_,{path});
        new QTreeWidgetItem(root,{"ELF"+QString::number(image_.bits)+" / "+utf(image_.architecture())}); root->setExpanded(true);
        project_->setHeaderLabel("Project");
        for(const auto& s:image_.sections) {
            QString flags; if(s.flags&1) flags+='W'; if(s.flags&2) flags+='A'; if(s.flags&4) flags+='X';
            row(sections_,{utf(s.name),QString::number(s.type),hx(s.address),hx(s.offset),hx(s.size),flags+" "+hx(s.flags)});
        }
        for(int r=0;r<symbolModel_->rowCount();++r) {
            QStringList values;for(int c=0;c<symbolModel_->columnCount();++c) values<<symbolModel_->data(symbolModel_->index(r,c)).toString();row(symbols_,values);
            const auto* symbol=symbolModel_->symbol(r);
            for(int c=0;c<symbols_->columnCount();++c){auto* item=symbols_->item(r,c);item->setToolTip(symbolModel_->data(symbolModel_->index(r,c),Qt::ToolTipRole).toString());if(symbol && !symbol->section){auto font=item->font();font.setItalic(true);item->setFont(font);}}
        }
        info_->setPlainText(QString("File: %1\nSize: %2 bytes\nFormat: ELF%3\nArchitecture: %4\nEndian: %5\nType: %6\nEntry point (ELF VA): %7\nHeader size: %8\nProgram headers: %9 at %10\nSection headers: %11 at %12\nSymbols: %13")
            .arg(path).arg(file_->size()).arg(image_.bits).arg(utf(image_.architecture())).arg(image_.littleEndian ? "Little" : "Big").arg(image_.type==3 ? "ET_DYN (link-time VAs; runtime load bias unknown)" : image_.type==2 ? "ET_EXEC" : QString::number(image_.type)).arg(hx(image_.entry)).arg(image_.headerSize).arg(image_.segments.size()).arg(hx(image_.programOffset)).arg(image_.sections.size()).arg(hx(image_.sectionOffset)).arg(image_.symbols.size()));
        console_->setPlainText(QString("Entry ELF VA: %1\nDecoded: %2 instructions\nInitial references: %3\nConfirmed = valid instruction at an ELF metadata seed; this is not runtime proof.\nProbable = static traversal or user hypothesis. Other bytes remain unanalyzed.")
            .arg(hx(image_.entry)).arg(instructions_->rowCount()).arg(instructions_->analysis().references.size()));
        for(const auto& d:instructions_->analysis().diagnostics) console_->appendPlainText(utf(d));
        analysisState_->setText(QString("%1 instructions | %2 references | ELF VAs | Confirmed: metadata seeds; probable: hypotheses; other bytes: unanalyzed%3")
            .arg(instructions_->rowCount()).arg(instructions_->analysis().references.size()).arg(instructions_->analysis().limited ? " | Budget reached" : ""));
        updateFunctions(); showHex(0);
        if(requestedNavigation_) go({true,*requestedNavigation_},true);
        else if(instructions_->rowForAddress(image_.entry)>=0) navigateToAddress(image_.entry);
        else {centralTabs_->setCurrentIndex(0); log("Entry point has no decoded instruction in an executable file mapping.");}
        log("Loaded "+path+"; entry "+hx(image_.entry)+"; "+QString::number(instructions_->rowCount())+" decoded instructions");
        statusBar()->showMessage("Loaded — double-click Assembly to follow a direct branch/call; Alt+Left/Right for history"); return true;
    } catch(const std::exception& e) {
        progress_->setValue(0); log(QString("Open failed: ")+e.what());
        if(requestInteractive_) QMessageBox::critical(this,"Cannot open binary",QString::fromUtf8(e.what()));
        return false;
    }
}
void Window::configureLimits() {
    QDialog dialog(this);dialog.setWindowTitle("Analysis resource limits");QFormLayout form(&dialog);
    QSpinBox memory,time,instructions;
    memory.setRange(1,1024);memory.setSuffix(" MiB");memory.setValue(static_cast<int>(limits_.maxMemoryBytes/(1024*1024)));
    time.setRange(1,300000);time.setSuffix(" ms");time.setValue(static_cast<int>(limits_.maxTime.count()));
    instructions.setRange(1,1000000);instructions.setValue(static_cast<int>(limits_.maxInstructions));
    form.addRow("Accounted core memory:",&memory);form.addRow("Time per request:",&time);form.addRow("Global decoded instructions per project:",&instructions);
    QLabel note("Memory/time apply to new requests; global quota applies when opening a project.\nMemory is a conservative core allocation budget, not a process RSS cap.");note.setWordWrap(true);form.addRow(&note);
    QDialogButtonBox buttons(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);form.addRow(&buttons);
    connect(&buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(&buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()==QDialog::Accepted) {
        limits_.maxMemoryBytes=static_cast<uint64_t>(memory.value())*1024*1024;
        limits_.maxTime=std::chrono::milliseconds(time.value());limits_.maxInstructions=static_cast<uint64_t>(instructions.value());
    }
}
void Window::updateFunctions() {
    irText_->setPlainText("Select an analyzed function. IR is experimental.");pseudocode_->setPlainText("Select an analyzed function. No complete decompiler is available.");
    functionModel_->setRegistry(program_.registry);
    instructions_->setFunctions(program_.registry);
    referenceModel_->setReferences(program_.references.all());
    graph_->setGraph({});
}
void Window::showHex(uint64_t offset) {
    if(!file_ || offset>=file_->size()) return;
    QString text; auto count=std::min<uint64_t>(4096,file_->size()-offset);
    for(uint64_t i=0;i<count;i+=16) text+=QString::fromStdString(hexRow(*file_,offset+i,std::min<uint64_t>(16,count-i)))+'\n';
    page_=offset; hex_->setPlainText(text); hexPosition_->setText(hx(offset)+" / "+hx(file_->size()));
}
void Window::record(Location location) {
    if(historyPosition_>=0) {
        const auto& previous=locations_[static_cast<size_t>(historyPosition_)];
        if(previous.address==location.address && previous.value==location.value) return;
    }
    locations_.resize(static_cast<size_t>(historyPosition_+1)); locations_.push_back(location);
    if(locations_.size()>1000) locations_.erase(locations_.begin());
    history_->clear();
    for(const auto& item:locations_) history_->addItem(QString(item.address ? "VA  " : "Offset  ")+hx(item.value));
    historyPosition_=static_cast<int>(locations_.size())-1; history_->setCurrentRow(historyPosition_);
}
bool Window::go(Location location,bool addHistory) {
    if(!file_) return false;
    try {
        AddressTranslator translator(image_,file_->size());
        auto offset=location.address ? translator.toFile(location.value) : std::optional<uint64_t>(location.value);
        if(!offset || *offset>=file_->size()) {log("Invalid or ambiguous address / offset: "+hx(location.value)); return false;}
        int instructionRow=-1;
        if(location.address) {
            instructionRow=instructions_->rowForAddress(location.value);
            if(instructionRow<0) {
                // Do not reinterpret the middle of an existing instruction as a fresh boundary.
                const auto& existing=instructions_->analysis().instructions;
                auto next=std::upper_bound(existing.begin(),existing.end(),location.value,[](uint64_t va,const Instruction& ins){return va<ins.address;});
                if(next!=existing.begin()) {
                    const auto& previous=*std::prev(next);
                    if(location.value-previous.address<previous.size) {log("Target is inside an existing instruction; conflicting boundary rejected.");return false;}
                }
                auto m=translator.mapping(location.value);
                if(m && m->executable && image_.machine==62 && image_.bits==64 && image_.littleEndian) {
                    if(!applyingResult_) {
                        startJob(loadedPath_,location.value,false);
                        statusBar()->showMessage("Analyzing requested address asynchronously…");return true;
                    }
                }
            }
        }
        showHex(*offset);
        addressInfo_->setPlainText(QString("File offset: %1\nELF VA: %2\nAddress space: %3\nRuntime address: unknown (no process/load bias)")
            .arg(hx(*offset)).arg(location.address ? hx(location.value) : translator.toVA(*offset) ? hx(*translator.toVA(*offset)) : "Unmapped")
            .arg(image_.type==3 ? "ET_DYN — link-time VA" : image_.type==2 ? "ET_EXEC — ELF VA" : "File offsets only"));
        if(instructionRow>=0) {
            centralTabs_->setCurrentIndex(0); auto index=instructions_->index(instructionRow,0);
            assembly_->setCurrentIndex(index); assembly_->selectRow(instructionRow); assembly_->scrollTo(index,QAbstractItemView::PositionAtCenter); updateSelection();
        } else {centralTabs_->setCurrentIndex(1); instructionInfo_->setPlainText("No analyzed instruction at this location. Bytes are unanalyzed.");}
        if(addHistory) record(location);
        return true;
    } catch(const std::exception& e) {log(QString("Navigation failed: ")+e.what()); return false;}
}
void Window::goToFunction(){
    if(!file_){statusBar()->showMessage("Open an ELF file first");return;}
    GoToFunctionDialog dialog(functionModel_,symbolModel_,[this](uint64_t va){return navigateToFunction(va);},this);dialog.exec();
}
bool Window::navigateToFunction(uint64_t va){
    if(!file_)return false;
    AddressTranslator translator(image_,file_->size());auto mapping=translator.mapping(va);
    if(!mapping || !mapping->executable || image_.machine!=62 || image_.bits!=64 || !image_.littleEndian){log("Go to Function rejected: address is not executable x86-64 ELF code: "+hx(va));return false;}
    const auto* existing=instructions_->instruction(instructions_->rowForAddress(va));if(existing && existing->flow==FlowType::Invalid){log("Go to Function rejected: invalid encoding at "+hx(va));return false;}
    if(existing && !program_.registry.containing(va)){startJob(loadedPath_,va,false);statusBar()->showMessage("Building function views on demand…");return true;}
    return go({true,va},true);
}
bool Window::navigateToAddress(uint64_t va) {return go({true,va},true);}
bool Window::navigateToOffset(uint64_t offset) {return go({false,offset},true);}
bool Window::goBack() {
    if(historyPosition_<=0 || !go(locations_[static_cast<size_t>(historyPosition_-1)],false)) return false;
    --historyPosition_; history_->setCurrentRow(historyPosition_); return true;
}
bool Window::goForward() {
    if(static_cast<size_t>(historyPosition_+1)>=locations_.size() || !go(locations_[static_cast<size_t>(historyPosition_+1)],false)) return false;
    ++historyPosition_; history_->setCurrentRow(historyPosition_); return true;
}
void Window::followSelected() {
    if(centralTabs_->currentIndex()!=0) return;
    const auto* ins=instructions_->instruction(assembly_->currentIndex().row());
    if(ins && ins->directTarget) {
        const auto target=*ins->directTarget; record({true,ins->address}); navigateToAddress(target);
    }
    else log("Selected instruction has no direct control-flow destination.");
}
std::optional<uint64_t> Window::selectedAddress() const {
    if(centralTabs_->currentIndex()!=0) return std::nullopt;
    const auto* ins=instructions_->instruction(assembly_->currentIndex().row());
    return ins ? std::optional<uint64_t>(ins->address) : std::nullopt;
}
void Window::updateSelection() {
    const auto* ins=instructions_->instruction(assembly_->currentIndex().row()); if(!ins) return;
    if(const auto* f=program_.registry.containing(ins->address)) {
        if(displayedFunction_!=f->entry) {
            displayedFunction_=f->entry;
            const auto& entries=functionModel_->registry().functions;auto entry=std::lower_bound(entries.begin(),entries.end(),f->entry,[](const auto& candidate,uint64_t va){return candidate.entry<va;});
            if(entry!=entries.end() && entry->entry==f->entry){const int row=static_cast<int>(entry-entries.begin());functions_->setCurrentIndex(functionModel_->index(row,0));functions_->selectRow(row);functions_->scrollTo(functionModel_->index(row,0),QAbstractItemView::PositionAtCenter);}
            auto ir=program_.ir.find(f->entry);
            const bool displayIR=ir!=program_.ir.end() && ir->second.instructions.size()<=2048;
            irText_->setPlainText(displayIR?utf(ir->second.listing+"\n"+ir->second.pseudocode):"IR unavailable or exceeds display limit (2048 instructions)");
            pseudocode_->setPlainText(displayIR?utf(ir->second.cLike+"\n"+ir->second.pseudocode):"Pseudocode unavailable or exceeds display limit (2048 instructions)");
            auto graph=program_.graphs.find(f->entry);if(graph!=program_.graphs.end()) graph_->setGraph(graph->second);else graph_->setGraph({});
            std::vector<Reference> refs;for(const auto& r:program_.references.all())
                if(std::binary_search(f->instructions.begin(),f->instructions.end(),r.from) || std::binary_search(f->instructions.begin(),f->instructions.end(),r.to)) refs.push_back(r);
            referenceModel_->setReferences(std::move(refs));
        }
    }
    if(!program_.registry.containing(ins->address)) {displayedFunction_.reset();irText_->setPlainText("No identified function at this address");pseudocode_->setPlainText("No identified function at this address");graph_->setGraph({});referenceModel_->setReferences(program_.references.all());}
    graph_->highlight(ins->address);
    QString references;
    for(const auto& ref:instructions_->analysis().references) if(ref.to==ins->address)
        references+=QString("\n  %1 from %2").arg(ref.flow ? "Control-flow" : "Data").arg(hx(ref.from));
    instructionInfo_->setPlainText(QString("VA: %1\nOffset: %2\nSize: %3\nInstruction: %4 %5\nFlow: %6\nDirect target: %7\nConfidence: %8\nEvidence: %9\nIncoming references:%10")
        .arg(hx(ins->address)).arg(hx(ins->offset)).arg(ins->size).arg(utf(ins->mnemonic)).arg(utf(ins->operands)).arg(flowName(ins->flow))
        .arg(ins->directTarget ? hx(*ins->directTarget) : "None / indirect / overflow").arg(confidenceName(ins->confidence)).arg(utf(ins->evidence)).arg(references.isEmpty() ? " None" : references));
    addressInfo_->setPlainText(QString("ELF VA: %1\nFile offset: %2\n%3\nRuntime load bias: unknown").arg(hx(ins->address)).arg(hx(ins->offset)).arg(image_.type==3 ? "ET_DYN link-time address" : "ET_EXEC address"));
    if(file_) {
        try {showHex(ins->offset);} catch(const std::exception& e) {log(QString("Hex read failed: ")+e.what());}
    }
}
void Window::copySelection() {
    if(centralTabs_->currentWidget()==irText_) {irText_->copy();return;}
    if(centralTabs_->currentWidget()==pseudocode_) {pseudocode_->copy();return;}
    if(centralTabs_->currentWidget()==processView_) {processView_->copySelection();return;}
    if(centralTabs_->currentIndex()==1) {hex_->copy();return;}
    QString text; auto rows=assembly_->selectionModel()->selectedRows();
    std::sort(rows.begin(),rows.end(),[](const QModelIndex& a,const QModelIndex& b){return a.row()<b.row();});
    for(const auto& index:rows) {
        for(int c=0;c<5;++c) {if(c) text+='\t';text+=instructions_->data(instructions_->index(index.row(),c)).toString();}
        text+='\n';
    }
    if(!text.isEmpty()) QApplication::clipboard()->setText(text);
}
void Window::navigate(bool address) {
    if(!file_) return;
    bool accepted=false; auto text=QInputDialog::getText(this,"Navigate",address ? "ELF virtual address (hex, not runtime VA):" : "File offset (hex):",QLineEdit::Normal,"0x0",&accepted);
    if(!accepted) return;
    bool valid=false; auto value=text.trimmed().toULongLong(&valid,16);
    if(!valid || !(address ? navigateToAddress(value) : navigateToOffset(value)))
        QMessageBox::warning(this,"Navigation","Invalid, ambiguous or conflicting address / file offset.");
}
bool Window::smokeValid() const {
    if(!file_ || sections_->rowCount()!=static_cast<int>(image_.sections.size()) || hex_->toPlainText().isEmpty() || findChildren<QDockWidget*>().size()!=12) return false;
    if(image_.machine==62 && image_.bits==64 && image_.littleEndian) {
        int r=instructions_->rowForAddress(image_.entry); const auto* ins=instructions_->instruction(r);
        return ins && ins->flow!=FlowType::Invalid && selectedAddress()==image_.entry && centralTabs_->currentIndex()==0;
    }
    return true;
}
}

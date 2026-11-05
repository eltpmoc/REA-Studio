// SPDX-License-Identifier: GPL-3.0-or-later
#include "rea/window.hpp"
#include "rea/process_view.hpp"
#include <QApplication>
#include <QAbstractItemModelTester>
#include <QAction>
#include <QCheckBox>
#include <QClipboard>
#include <QElapsedTimer>
#include <QPushButton>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QTableView>
#include <QTabWidget>
#include <QTest>
#include <QTimer>
#include <array>
#include <iostream>
using namespace rea;
namespace {
void check(bool value,const char* m){if(!value)throw std::runtime_error(m);}
template<class F> void wait(F f){QElapsedTimer t;t.start();while(!f() && t.elapsed()<10000)QTest::qWait(1);check(f(),"GUI process job timed out");}
}
int main(int argc,char** argv){
    QApplication app(argc,argv);
    try{
        Window window(false);window.show();auto* action=window.findChild<QAction*>("openProcessMemoryAction");check(action,"menu action");action->trigger();
        auto* view=window.findChild<ProcessView*>("processMemoryView");auto* tabs=window.findChild<QTabWidget*>("centralTabs");check(view && tabs->currentWidget()==view,"process tab opened");
        int beats=0;QTimer heartbeat;QObject::connect(&heartbeat,&QTimer::timeout,[&]{++beats;});heartbeat.start(0);
        wait([&]{return !view->busy();});check(view->processModel()->rowCount()>0 && beats>0,"async listing and GUI heartbeat");heartbeat.stop();
        auto* table=window.findChild<QTableView*>("processTable");auto* search=window.findChild<QLineEdit*>("processFilter");search->setText(QString::number(currentProcessId()));QApplication::processEvents();
        int row=-1;for(int n=0;n<table->model()->rowCount();++n)if(table->model()->data(table->model()->index(n,0)).toUInt()==currentProcessId())row=n;check(row>=0,"self listed after filter");
        QAbstractItemModelTester tester(table->model(),QAbstractItemModelTester::FailureReportingMode::Fatal);
        auto index=table->model()->index(row,0);table->scrollTo(index);QApplication::processEvents();auto point=table->visualRect(index).center();
        QTest::mouseClick(table->viewport(),Qt::LeftButton,Qt::NoModifier,point);QTest::mouseDClick(table->viewport(),Qt::LeftButton,Qt::NoModifier,point);
        wait([&]{return !view->busy();});check(view->openedPid()==currentProcessId(),"double click opened real process");
        auto* hex=window.findChild<QPlainTextEdit*>("processMemoryHex");check(hex->isReadOnly() && !hex->toPlainText().isEmpty(),"read-only RAM hex displayed");
        auto* regions=window.findChild<QTableView*>("memoryRegions");QAbstractItemModelTester regionTester(regions->model(),QAbstractItemModelTester::FailureReportingMode::Fatal);check(regions->model()->rowCount()>0,"regions populated");
        alignas(4096) static std::array<char,4096> marker{};marker.fill('q');auto address=static_cast<uint64_t>(reinterpret_cast<uintptr_t>(marker.data()));check(view->readAddress(address),"queue runtime address");wait([&]{return !view->busy();});check(hex->toPlainText().contains("71 71 71 71"),"actual marker bytes");
        marker.fill('z');auto* live=window.findChild<QCheckBox*>("liveMemoryRefresh");live->setChecked(true);wait([&]{return hex->toPlainText().contains("7a 7a 7a 7a");});live->setChecked(false);
        check(view->enableWriting(true),"enable writing explicitly");wait([&]{return !view->busy();});
        check(view->readAddress(address),"reload writable fixture page");wait([&]{return !view->busy();});
        auto* edit=window.findChild<QPushButton*>("editMemoryBytes");check(edit && edit->isEnabled(),"edit enabled in RW session");
        QTimer::singleShot(0,&window,[&]{auto* dialog=window.findChild<QDialog*>("memoryPatchDialog");check(dialog,"patch dialog");dialog->findChild<QLineEdit*>("memoryReplacement")->setText("41 42 43 44");dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Apply)->click();});
        edit->click();view->cancel(); // An issued write must still report its real outcome.
        wait([&]{return !view->busy();});check(hex->toPlainText().contains("41 42 43 44"),"GUI patch writes and verifies real bytes");
        check(view->undoLastWrite(),"undo queued");wait([&]{return !view->busy();});check(hex->toPlainText().contains("7a 7a 7a 7a"),"undo restores only matching last patch");
        check(view->readAddress(address),"refresh full fixture");wait([&]{return !view->busy();});
        check(!view->readAddress(UINT64_MAX),"unmapped address rejected");
        const auto pid=view->openedPid();ProcessDescriptor bad{UINT32_MAX,1,"missing",{}};view->openProcess(bad);wait([&]{return !view->busy();});check(view->openedPid()==pid,"failed open preserves prior target");
        view->refreshProcesses();view->refreshProcesses();view->cancel();wait([&]{return !view->busy();});check(view->openedPid()==pid,"cancel/latest-request ownership");
        check(view->readAddress(address),"refresh after cancellation");wait([&]{return !view->busy();});
        hex->selectAll();window.copySelection();check(QApplication::clipboard()->text().contains("7a 7a 7a 7a"),"copy RAM hex");hex->moveCursor(QTextCursor::Start);
        if(argc==2)check(window.grab().save(argv[1]),"process workspace screenshot");
        view->refreshProcesses();window.close();check(!view->busy(),"close cancels and joins");
        std::cout<<"PASS process menu, filtering, real double click, models, memory, live refresh, error, cancellation and close\n";return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL GUI: "<<e.what()<<'\n';return 1;}
}

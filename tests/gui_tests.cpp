// SPDX-License-Identifier: GPL-3.0-or-later
#include "rea/window.hpp"
#include <QAbstractItemModelTester>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QDockWidget>
#include <QFile>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QSettings>
#include <QTableView>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTest>
#include <iostream>
#include <stdexcept>
using namespace rea;
namespace {void check(bool value,const char* message) {if(!value) throw std::runtime_error(message);}}
int main(int argc,char** argv) {
    QApplication app(argc,argv); app.setOrganizationName("REA Studio Tests"); app.setApplicationName("Workspace");
    QTemporaryDir temporary; if(!temporary.isValid() || argc<2) return 1;
    QSettings::setDefaultFormat(QSettings::IniFormat); QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,temporary.path());
    try {
        Window window(false); window.show(); QApplication::processEvents();
        check(window.openFile(QString::fromLocal8Bit(argv[1]),false),"open GUI fixture"); QApplication::processEvents();
        check(window.smokeValid(),"central assembly, entry selection, hex and 12 docks");
        auto* view=window.findChild<QTableView*>("assemblyView"); check(view,"assembly view missing");
        auto* model=const_cast<InstructionModel*>(window.instructionModel());
        QAbstractItemModelTester tester(model,QAbstractItemModelTester::FailureReportingMode::Fatal);
        const auto entry=window.selectedAddress(); check(entry.has_value(),"entry selected");
        const auto initialRows=model->rowCount(); check(initialRows>0 && model->columnCount()==5,"model shape");
        check(!model->data(QModelIndex()).isValid() && model->rowCount(model->index(0,0))==0,"model index validation");
        check(model->data(model->index(model->rowForAddress(*entry),0),InstructionModel::ConfidenceRole).toInt()==static_cast<int>(Confidence::Confirmed),"entry confidence role");
        check(!window.navigateToAddress(UINT64_MAX) && !window.navigateToOffset(UINT64_MAX),"invalid navigation");
        const auto* first=model->instruction(model->rowForAddress(*entry));
        if(first && first->size>1) check(!window.navigateToAddress(*entry+1),"mid-instruction navigation rejected");
        uint64_t origin=0,target=0; int branchRow=-1;
        for(int r=0;r<model->rowCount();++r) {
            auto* ins=model->instruction(r);
            if(ins->directTarget && model->rowForAddress(*ins->directTarget)>=0 && *ins->directTarget!=ins->address) {
                origin=ins->address;target=*ins->directTarget;branchRow=r;break;
            }
        }
        check(branchRow>=0,"fixture must contain a mapped direct branch/call");
        check(window.navigateToAddress(origin),"select branch origin"); QApplication::processEvents();
        auto index=model->index(branchRow,0); view->scrollTo(index,QAbstractItemView::PositionAtCenter); QApplication::processEvents();
        auto rect=view->visualRect(index); check(rect.isValid(),"branch visible");
        QTest::mouseClick(view->viewport(),Qt::LeftButton,Qt::NoModifier,rect.center());
        QTest::mouseDClick(view->viewport(),Qt::LeftButton,Qt::NoModifier,rect.center()); QApplication::processEvents();
        check(window.selectedAddress()==target,"double click follows actual decoded target");
        auto* back=window.findChild<QAction*>("backAction"); check(back,"back action"); back->trigger();
        check(window.selectedAddress()==origin,"history back to branch");
        check(window.goForward() && window.selectedAddress()==target,"history forward");
        view->selectRow(model->rowForAddress(target)); window.copySelection();
        check(QApplication::clipboard()->text().contains(QString::fromStdString(model->instruction(model->rowForAddress(target))->mnemonic)),"copy selected assembly");
        check(window.navigateToOffset(0),"offset navigation");
        auto* tabs=window.findChild<QTabWidget*>("centralTabs"); check(tabs && tabs->currentIndex()==1,"central Hex Viewer");
        auto* hex=window.findChild<QPlainTextEdit*>("hexView"); check(hex && hex->toPlainText().contains("7f 45 4c 46"),"ELF bytes in hex");
        check(window.goBack() && window.selectedAddress()==target,"history across hex and assembly");
        window.setDarkTheme(false); check(qApp->palette().color(QPalette::Base)==QColor("#ffffff"),"light theme"); window.setDarkTheme(true);
        auto* logs=window.findChild<QPlainTextEdit*>("logView");
        const auto selected=window.selectedAddress();
        QFile malformed(temporary.filePath("malformed.elf")); check(malformed.open(QIODevice::WriteOnly),"malformed fixture"); malformed.write("\x7f" "ELF",4); malformed.close();
        check(!window.openFile(malformed.fileName(),false),"malformed file rejected in GUI");
        check(model->rowCount()==initialRows && window.selectedAddress()==selected && logs->toPlainText().contains("Open failed"),"failed load preserves previous project and logs");
        check(window.navigateToAddress(*entry),"return to entry");
        if(argc>=3) check(window.grab().save(QString::fromLocal8Bit(argv[2])),"workspace screenshot");
        window.close();
        {
            Window saved(true); saved.show(); QApplication::processEvents(); saved.setDarkTheme(false);
            auto* project=saved.findChild<QDockWidget*>("Project Explorer"); check(project,"project dock"); project->hide(); saved.close();
        }
        {
            Window restored(true); restored.show(); QApplication::processEvents();
            check(qApp->palette().color(QPalette::Base)==QColor("#ffffff"),"QSettings theme restored");
            check(restored.findChild<QDockWidget*>("Project Explorer")->isHidden(),"QSettings layout restored"); restored.close();
        }
        std::cout<<"PASS GUI: entry + real Assembly, model invariants, double-click target, back/forward, copy, hex, errors, themes and QSettings\n";return 0;
    } catch(const std::exception& e) {std::cerr<<"FAIL GUI: "<<e.what()<<'\n';return 1;}
}

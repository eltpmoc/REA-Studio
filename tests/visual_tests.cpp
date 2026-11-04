// SPDX-License-Identifier: GPL-3.0-or-later
#include "rea/window.hpp"
#include "rea/assembly_delegate.hpp"
#include "analysis_fixtures.hpp"
#include <QApplication>
#include <QDockWidget>
#include <QTableWidget>
#include <QTableView>
#include <QProgressBar>
#include <QFile>
#include <QTest>
#include <QElapsedTimer>
#include <QAbstractItemModelTester>
#include <iostream>
using namespace rea;
using test::check;
int main(int argc,char** argv) {
    QApplication app(argc,argv);
    try {
        check(argc==3,"fixture/output arguments");
        InstructionModel model;AnalysisResult a;Image im;im.entry=0x1000;
        Instruction call;call.address=0x1000;call.size=5;call.flow=FlowType::Call;call.directTarget=0x1010;call.mnemonic="call";
        Instruction ret;ret.address=0x1010;ret.size=1;ret.flow=FlowType::Return;ret.mnemonic="ret";
        a.instructions={call,ret};a.references={{0x1000,0x1010,true}};model.setAnalysis(a,im);
        FunctionRegistry registry;FunctionDescriptor f;f.entry=0x1010;f.name="callee";f.confidence=FunctionConfidence::Probable;registry.functions.push_back(f);model.setFunctions(registry);
        QAbstractItemModelTester tester(&model,QAbstractItemModelTester::FailureReportingMode::Fatal);
        check(model.rowCount()==2 && model.columnCount()==5 && model.rowForAddress(0x1010)==1,"instruction rows unchanged by separators");
        check(model.data(model.index(0,0),InstructionModel::FunctionEntryRole).toBool() && model.data(model.index(1,0),InstructionModel::FunctionEntryRole).toBool(),"entry and discovered function separators");
        check(model.data(model.index(1,4)).toString().contains("callee [probable]"),"function label and confidence");
        check(model.data(model.index(1,0),InstructionModel::IncomingReferencesRole).toULongLong()==1 && model.data(model.index(0,0),InstructionModel::OutgoingReferencesRole).toULongLong()==1,"xref roles");
        check(model.data(model.index(1,4)).toString().contains("XREF ↓1 ↑0"),"xref indicator");
        const auto formats=AssemblyDelegate::operandFormats("rax, qword ptr [rip+0x1234]",true);
        check(formats.size()>=7 && formats.front().format.foreground().color()!=formats[formats.size()-2].format.foreground().color(),"register/number syntax colors");
        Window window(false);window.show();check(window.openFile(argv[1],false),"open fixture");QApplication::processEvents();
        auto* progress=window.findChild<QDockWidget*>("Progress");auto* toolbar=window.findChild<QProgressBar*>("analysisToolbarProgress");
        check(progress && progress->isHidden() && toolbar->isHidden() && toolbar->value()==100,"progress collapsed on completion with retained result");
        progress->show();check(!progress->isHidden(),"progress can be reopened");
        auto* assembly=window.findChild<QTableView*>("assemblyView");check(dynamic_cast<AssemblyDelegate*>(assembly->itemDelegate()),"syntax delegate installed");
        check(qApp->palette().color(QPalette::Base)==QColor("#101722") && qApp->palette().color(QPalette::Disabled,QPalette::Text)!=qApp->palette().color(QPalette::Active,QPalette::Text),"dark palette and disabled state");
        window.setDarkTheme(false);check(qApp->palette().color(QPalette::Base)==QColor("#ffffff"),"light preserved");window.setDarkTheme(true);QApplication::processEvents();check(toolbar->isHidden(),"theme preserves idle progress visibility");
        test::Scratch scratch;QFile fixture(argv[1]);check(fixture.open(QIODevice::ReadOnly),"read symbol fixture");const auto data=fixture.readAll();
        std::vector<uint8_t> bytes(data.begin(),data.end());
        // The existing ELF64 fixture has a symbol table at section 3 and foo at symbol 1.
        bytes.at(128+3*64+4)=11;bytes.at(664+4)=0x12;bytes.at(664+6)=0;bytes.at(664+7)=0;
        auto imported=scratch.directory/"imports.elf";test::write(imported,bytes);check(window.openFile(QString::fromStdString(imported.string()),false),"dynamic undefined fixture");
        auto* symbols=window.findChild<QTableWidget*>("symbolsView");
        check(symbols && symbols->item(1,7)->text()=="Imported / undefined" && symbols->item(1,1)->text()=="—","import candidate classification without fabricated address");
        check(symbols->item(0,7)->text()=="Undefined","null symbol is not an import");
        bytes.at(128+3*64+4)=2;auto undefined=scratch.directory/"undefined.elf";test::write(undefined,bytes);
        check(window.openFile(QString::fromStdString(undefined.string()),false) && symbols->item(1,7)->text()=="Undefined","static undefined symbol distinguished");
        auto longPath=scratch.directory/"long.elf";test::write(longPath,test::elf(2*1024*1024));window.openFileAsync(QString::fromStdString(longPath.string()),false);
        check(!progress->isHidden() && !toolbar->isHidden(),"progress restored for new request");window.cancelAnalysis();
        QElapsedTimer timer;timer.start();while(window.analysisBusy() && timer.elapsed()<10000) QTest::qWait(1);
        check(!window.analysisBusy() && progress->isHidden() && toolbar->isHidden(),"progress collapsed after cancellation");
        check(!window.openFile(QString::fromStdString((scratch.directory/"missing.elf").string()),false) && progress->isHidden() && toolbar->isHidden(),"failure collapses progress");
        check(window.openFile(argv[1],false),"restore fixture");
        check(window.grab().save(QString::fromLocal8Bit(argv[2])+"/workspace-visual-fixture.png"),"visual screenshot");
        std::cout<<"PASS syntax, separators, xrefs, symbol classification, theme and progress lifecycle\n";return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}

// SPDX-License-Identifier: GPL-3.0-or-later
#include "rea/window.hpp"
#include "rea/semantic_ir.hpp"
#include "rea/function_dialog.hpp"
#include "rea/graph_view.hpp"
#include "rea/symbol_names.hpp"
#include "analysis_fixtures.hpp"
#include <QAbstractItemModelTester>
#include <QAction>
#include <QApplication>
#include <QDialogButtonBox>
#include <QElapsedTimer>
#include <QGraphicsScene>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTableView>
#include <QTabWidget>
#include <QTest>
#include <QTimer>
#include <iostream>
#include <map>
using namespace rea;
using test::check;
namespace {
template<class F>void wait(F f){QElapsedTimer t;t.start();while(!f() && t.elapsed()<10000)QTest::qWait(1);check(f(),"navigation timed out");}
void drive(Window& window,const QString& query,uint64_t expected,const QString& screenshot={},bool shortcut=false,bool doubleClick=false){
    if(shortcut){window.activateWindow();window.setFocus();QApplication::processEvents();}
    std::string failure;
    QTimer::singleShot(0,&window,[&]{auto* dialog=window.findChild<GoToFunctionDialog*>("goToFunctionDialog");try{
        check(dialog,"Go to Function dialog opened");auto* input=dialog->findChild<QLineEdit*>("functionSearch");auto* results=dialog->findChild<QTableView*>("functionSearchResults");
        QAbstractItemModelTester tester(results->model(),QAbstractItemModelTester::FailureReportingMode::Fatal);
        input->setText(query);QApplication::processEvents();
        int row=-1;for(int n=0;n<results->model()->rowCount();++n)if(results->model()->data(results->model()->index(n,0),NavAddressRole).toULongLong()==expected)row=n;
        check(row>=0,"incremental search includes expected function");check(results->model()->columnCount()==5,"name/address/size/origin/confidence");
        results->setCurrentIndex(results->model()->index(row,0));results->selectRow(row);
        if(!screenshot.isEmpty())check(dialog->grab().save(screenshot),"dialog capture");
        if(doubleClick){results->scrollTo(results->currentIndex());QApplication::processEvents();auto point=results->visualRect(results->currentIndex()).center();QTest::mouseClick(results->viewport(),Qt::LeftButton,Qt::NoModifier,point);QTest::mouseDClick(results->viewport(),Qt::LeftButton,Qt::NoModifier,point);}
        else QTest::keyClick(input,Qt::Key_Return);
    }catch(const std::exception& e){failure=e.what();if(dialog)dialog->reject();}});
    if(shortcut){window.activateWindow();window.setFocus();QTest::keyClick(&window,Qt::Key_F,Qt::ControlModifier|Qt::ShiftModifier);QApplication::processEvents();}
    else window.findChild<QAction*>("goToFunctionAction")->trigger();
    check(failure.empty(),failure.c_str());wait([&]{return !window.analysisBusy();});check(window.selectedAddress()==expected,"actual Assembly address selected");
}
void verifyViews(Window& window,uint64_t address){
    auto* function=window.programAnalysis().registry.containing(address);check(function,"selected function has membership");
    check(window.programAnalysis().graphs.contains(function->entry) && window.programAnalysis().ir.contains(function->entry),"CFG and IR available");
    auto* functions=window.findChild<QTableView*>("functionsView");check(functions && functions->currentIndex().data(NavAddressRole).toULongLong()==function->entry,"Functions table follows selected entry");
    auto* graph=window.findChild<GraphView*>("graphView");check(graph && !graph->scene()->items().empty(),"Graph View scene populated");
    auto* ir=window.findChild<QPlainTextEdit*>("irView");auto* pseudo=window.findChild<QPlainTextEdit*>("pseudocodeView");
    check(ir->toPlainText().contains(QString::number(address,16)) && pseudo->toPlainText().contains(QString::number(address,16)),"IR and pseudocode follow selected function");
    auto* refs=window.findChild<QTableView*>("referencesView");check(refs && refs->model()->rowCount()>0,"cross-references available for real callers");
}
void modelTests(){
    Image image;image.sections.resize(2);Symbol s;s.name="_Z7computei";s.address=0x401000;s.size=12;s.section=1;s.info=2;s.demangledName=demangleSymbol(s.name);image.symbols.push_back(s);
    auto alias=s;alias.name="_Z5aliasi";alias.demangledName=demangleSymbol(alias.name);image.symbols.push_back(alias);
    auto undefined=s;undefined.section=0;undefined.address=0;image.symbols.push_back(undefined);
    SymbolModel symbols;symbols.setImage(image);QAbstractItemModelTester sm(&symbols,QAbstractItemModelTester::FailureReportingMode::Fatal);
    FunctionRegistry registry;FunctionDescriptor f;f.entry=0x401020;f.end=0x401028;f.name="sub_401020";f.origin=FunctionOrigin::DirectCall;f.confidence=FunctionConfidence::Probable;registry.functions.push_back(f);
    FunctionModel functions;functions.setRegistry(registry);QAbstractItemModelTester fm(&functions,QAbstractItemModelTester::FailureReportingMode::Fatal);
    FunctionSearchModel entries(&functions,&symbols);FunctionSearchFilter filter;filter.setSourceModel(&entries);QAbstractItemModelTester em(&entries,QAbstractItemModelTester::FailureReportingMode::Fatal);
    check(entries.rowCount()==2,"deduplicated symbol aliases plus heuristic function; undefined omitted");filter.setQuery("_Z5aliasi");check(filter.rowCount()==1 && filter.index(0,0).data(NavAddressRole).toULongLong()==s.address,"original alias search");
    filter.setQuery("401020");check(filter.rowCount()==1,"bare ELF address");filter.setQuery("sub_401020");check(filter.rowCount()==1,"heuristic name search");
    if(!s.demangledName.empty()){filter.setQuery("compute(int)");check(filter.rowCount()==1,"demangled search");}
    check(!GoToFunctionDialog::parseAddress("0xnothex") && !GoToFunctionDialog::parseAddress("10000000000000000") && GoToFunctionDialog::parseAddress("0x401000")==s.address,"address parser");
}

}
int main(int argc,char** argv){QApplication app(argc,argv);try{
    check(argc>=3,"fixture/output arguments");const QString output=QString::fromLocal8Bit(argv[2]);modelTests();
    // On-demand GUI path, including raw addresses that do not have a symbol/catalogue row.
    test::Scratch scratch;auto path=scratch.directory/"hidden.elf";auto bytes=test::elf(4096);bytes[512]=0xc3;const std::vector<uint8_t> hidden{0xe8,0xfb,0xfe,0xff,0xff,0xb8,0x2a,0,0,0,0xc3};std::copy(hidden.begin(),hidden.end(),bytes.begin()+768);bytes[1600]=0x90;bytes[1601]=0xc3;test::write(path,bytes);
    Window demand(false);demand.show();check(demand.openFile(QString::fromStdString(path.string()),false),"open entry-only ELF");auto before=demand.instructionModel()->snapshot();check(demand.instructionModel()->rowCount()==1,"no linear sweep");
    std::string failure;QTimer::singleShot(0,&demand,[&]{auto* d=demand.findChild<GoToFunctionDialog*>("goToFunctionDialog");try{check(d,"raw address dialog");d->findChild<QLineEdit*>("functionSearch")->setText("0x400300");d->findChild<QPushButton*>("analyzeFunctionAddress")->click();}catch(const std::exception& e){failure=e.what();if(d)d->reject();}});demand.findChild<QAction*>("goToFunctionAction")->trigger();check(failure.empty(),failure.c_str());wait([&]{return !demand.analysisBusy();});
    check(demand.selectedAddress()==0x400300 && demand.instructionModel()->rowCount()==4 && demand.instructionModel()->rowForAddress(0x400200)>=0 && before->instructions.size()==1,"real demand publication preserves original snapshot");verifyViews(demand,0x400300);drive(demand,"sub_",0x400300,output+"/go-to-heuristic.png");
    check(demand.goBack() && demand.selectedAddress()==0x400200,"back history");check(demand.goForward() && demand.selectedAddress()==0x400300,"forward history");auto stable=demand.instructionModel()->snapshot();const auto oldIR=demand.findChild<QPlainTextEdit*>("irView")->toPlainText(),oldPseudo=demand.findChild<QPlainTextEdit*>("pseudocodeView")->toPlainText();const auto oldRefs=demand.findChild<QTableView*>("referencesView")->model()->rowCount();const auto oldGraph=demand.findChild<GraphView*>("graphView")->scene()->items().size();check(!demand.navigateToFunction(0x400301) && !demand.navigateToFunction(UINT64_MAX),"invalid/mid boundary");
    check(demand.navigateToFunction(0x400640),"cancellable demand");demand.cancelAnalysis();wait([&]{return !demand.analysisBusy();});check(demand.instructionModel()->snapshot()==stable,"cancelled demand preserves existing code and derived models");
    auto limits=demand.analysisLimits();limits.maxMemoryBytes=1;demand.setAnalysisLimits(limits);check(demand.navigateToFunction(0x400640),"limited demand");wait([&]{return !demand.analysisBusy();});check(demand.instructionModel()->snapshot()==stable,"resource failure preserves exact old snapshot");check(demand.findChild<QPlainTextEdit*>("irView")->toPlainText()==oldIR && demand.findChild<QPlainTextEdit*>("pseudocodeView")->toPlainText()==oldPseudo && demand.findChild<QTableView*>("referencesView")->model()->rowCount()==oldRefs && demand.findChild<GraphView*>("graphView")->scene()->items().size()==oldGraph,"derived views also preserved on cancel/limit");
    auto nonexec=bytes;nonexec[68]=4;auto nx=scratch.directory/"nonexec.elf";test::write(nx,nonexec);Window denied(false);check(denied.openFile(QString::fromStdString(nx.string()),false),"non executable ELF metadata opens");check(!denied.navigateToFunction(0x400300),"non executable function denied");
    if(std::string(argv[1])!="-"){
        BinaryFile file(argv[1]);auto image=ElfLoader{}.load(file);Window window(false);window.show();check(window.openFile(QString::fromLocal8Bit(argv[1]),false),"open compiled O0/g ELF");
        auto* action=window.findChild<QAction*>("goToFunctionAction");check(action && action->shortcut()==QKeySequence("Ctrl+Shift+F"),"menu and keyboard shortcut");
        const std::map<std::string,uint64_t> required{{"_Z5somarii",0x11c9},{"_Z8fatoriali",0x11e1},{"_Z8eh_primoi",0x122a},{"_Z11classificari",0x1276}};
        size_t n=0;for(const auto& [original,requiredAddress]:required){auto s=std::find_if(image.symbols.begin(),image.symbols.end(),[&](const auto& sym){return sym.name==original && sym.section;});check(s!=image.symbols.end(),"C++ fixture symbol present");
            if(argc>=4 && std::string(argv[3])=="--required")check(s->address==requiredAddress,"mandatory executable symbol has required VA");
            check(!s->demangledName.empty(),"demangled C++ symbol from ELF worker");drive(window,QString::fromStdString(s->demangledName),s->address,n==0?output+"/go-to-function-dialog.png":QString{},n==0,n==1);verifyViews(window,s->address);
            if(original=="_Z5somarii"){AnalysisControl semanticControl;auto result=recoverIntegerFunction(window.programAnalysis().ir.at(s->address),semanticControl);const auto& lifted=window.programAnalysis().ir.at(s->address);const bool complexFlow=std::any_of(lifted.instructions.begin(),lifted.instructions.end(),[](const auto& i){return i.flow==FlowType::ConditionalJump || i.flow==FlowType::Call;});
                if(complexFlow && argc<4){check(!result.recovered && !result.reason.empty(),"instrumented fixture must retain unsupported control flow");std::cout<<"Instrumented fixture conservatively refused: "<<result.reason<<'\n';}
                else {check(result.recovered,"real ELF sum recovered");for(uint64_t a:{0ULL,1ULL,0xffffffffULL})for(uint64_t b:{0ULL,7ULL,0x80000000ULL})check(result.evaluate({a,b})==uint32_t(a+b),"real ELF addition semantics");std::cout<<result.code;auto* text=window.findChild<QPlainTextEdit*>("pseudocodeView");check(text->toPlainText().contains(QString::fromStdString(result.code)),"C-like actual UI");auto* tabs=window.findChild<QTabWidget*>("centralTabs");tabs->setCurrentWidget(text);QApplication::processEvents();check(window.grab().save(output+"/semantic-sum.png"),"semantic capture");tabs->setCurrentIndex(0);}}

            const auto* f=window.programAnalysis().registry.at(s->address);check(f && f->confidence==FunctionConfidence::Confirmed && f->origin==FunctionOrigin::ElfSymbol,"symbol evidence and confidence");
            drive(window,QString::fromStdString(original),s->address);drive(window,"0x"+QString::number(s->address,16),s->address);
            check(window.grab().save(output+"/function-"+QString::number(s->address,16)+".png"),"real function navigation capture");
            std::cout<<s->demangledName<<" -> 0x"<<std::hex<<s->address<<std::dec<<"; size="<<s->size<<"; GUI Assembly/CFG/xrefs/IR/pseudocode verified\n";++n;
        }
    }
    std::cout<<"PASS Go to Function search/navigation, typed models, real demand, history, quota/cancel preservation\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL navigation: "<<e.what()<<'\n';return 1;}}

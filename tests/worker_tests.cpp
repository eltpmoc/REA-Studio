// SPDX-License-Identifier: GPL-3.0-or-later
#include "rea/window.hpp"
#include "analysis_fixtures.hpp"
#include <QAction>
#include <QApplication>
#include <QElapsedTimer>
#include <QProgressBar>
#include <QThread>
#include <QTest>
#include <QTimer>
#include <iostream>
using namespace rea;
using rea::test::check;
namespace {
template<class Predicate> void waitFor(Predicate predicate) {
    QElapsedTimer timer;timer.start();
    while(!predicate() && timer.elapsed()<10000) QTest::qWait(1);
    check(predicate(),"worker test timed out");
}
}
int main(int argc,char** argv) {
    const bool controllerOnly=argc==3 && std::string(argv[2])=="--controller-only";
    std::unique_ptr<QCoreApplication> application;
    if(controllerOnly) application=std::make_unique<QCoreApplication>(argc,argv);
    else application=std::make_unique<QApplication>(argc,argv);
    auto& app=*application;
    if(argc<2) return 1;
    try {
        test::Scratch scratch;auto longPath=scratch.directory/"large.elf";test::write(longPath,test::elf(2*1024*1024));
        QString large=QString::fromStdString(longPath.string()),small=QString::fromLocal8Bit(argv[1]);
        AnalysisLimits limits;limits.maxMemoryBytes=1024ULL*1024*1024;limits.maxInstructions=1000000;limits.maxTime=std::chrono::seconds(5);
        AnalysisController controller;
        AnalysisOutcomePtr cancelled;
        bool sawProgress=false,onGuiThread=true;int heartbeats=0;
        QTimer heartbeat;QObject::connect(&heartbeat,&QTimer::timeout,[&]{++heartbeats;});heartbeat.start(0);
        QObject::connect(&controller,&AnalysisController::progress,&controller,[&](quint64,int,const QString& phase,quint64 count,quint64,quint64){
            onGuiThread=onGuiThread && QThread::currentThread()==app.thread();
            if(phase=="Disassembly" && count>0) {sawProgress=true;controller.cancel();}
        });
        QObject::connect(&controller,&AnalysisController::completed,&controller,[&](quint64,AnalysisOutcomePtr outcome){cancelled=std::move(outcome);});
        controller.start(large,limits);waitFor([&]{return cancelled && !controller.busy();});
        check(cancelled->reason==StopReason::Cancelled && sawProgress && onGuiThread && heartbeats>0,"in-flight worker cancellation / responsive GUI");
        heartbeat.stop();
        // Latest request wins; superseded active and pending jobs are cancelled.
        AnalysisController replacement;std::map<quint64,AnalysisOutcomePtr> results;
        QObject::connect(&replacement,&AnalysisController::completed,&replacement,[&](quint64 id,AnalysisOutcomePtr out){results.emplace(id,std::move(out));});
        auto first=replacement.start(large,limits);auto second=replacement.start(large,limits);auto last=replacement.start(small,limits);
        waitFor([&]{return results.contains(last) && results.contains(first) && results.contains(second) && !replacement.busy();});
        check(results[first]->reason==StopReason::Cancelled && results[second]->reason==StopReason::Cancelled,"active/pending supersession");
        check(results[last]->reason==StopReason::None && results[last]->hasImage,"latest request completed");
        AnalysisController parallelOne,parallelTwo;
        auto sharedQuota=std::make_shared<InstructionBudget>(97);
        AnalysisOutcomePtr parallelResultOne,parallelResultTwo;
        QObject::connect(&parallelOne,&AnalysisController::completed,&parallelOne,[&](quint64,AnalysisOutcomePtr out){parallelResultOne=std::move(out);});
        QObject::connect(&parallelTwo,&AnalysisController::completed,&parallelTwo,[&](quint64,AnalysisOutcomePtr out){parallelResultTwo=std::move(out);});
        parallelOne.start(large,limits,{},sharedQuota);parallelTwo.start(large,limits,{},sharedQuota);parallelOne.cancel();
        waitFor([&]{return parallelResultOne && parallelResultTwo && !parallelOne.busy() && !parallelTwo.busy();});
        check(parallelResultOne->reason==StopReason::Cancelled && parallelResultTwo->reason==StopReason::InstructionLimit && sharedQuota->used()==97,"independent controllers / shared quota / isolated cancellation");
        {AnalysisController destroyDuringRun;destroyDuringRun.start(large,limits);} // Destructor cancels and joins.
        if(controllerOnly) {
            std::cout<<"PASS: dedicated worker/controller cancellation, progress, concurrency, quota and joins (QCoreApplication)\n";return 0;
        }
        Window window(false);window.show();check(window.openFile(small,false),"initial GUI project");
        auto original=window.selectedAddress();int rows=window.instructionModel()->rowCount();
        window.setAnalysisLimits(limits);window.openFileAsync(large,false);window.cancelAnalysis();
        waitFor([&]{return !window.analysisBusy();});
        check(window.selectedAddress()==original && window.instructionModel()->rowCount()==rows,"cancel preserves published project");
        window.openFileAsync(large,false);window.openFileAsync(small,false);waitFor([&]{return !window.analysisBusy();});
        check(window.smokeValid() && window.selectedAddress()==original,"stale result cannot overwrite latest GUI project");
        auto* progress=window.findChild<QProgressBar*>("analysisToolbarProgress");check(progress && progress->value()==100,"visible progress reaches completion");
        AnalysisLimits smallQuota=limits;smallQuota.maxInstructions=3;window.setAnalysisLimits(smallQuota);
        check(window.openFile(small,false),"limited but valid ELF remains inspectable");
        check(window.instructionModel()->analysis().stopReason==StopReason::InstructionLimit && window.instructionModel()->rowCount()<=3,"GUI global instruction limit");
        std::optional<uint64_t> unknownTarget;
        for(const auto& ins:window.instructionModel()->analysis().instructions)
            if(ins.directTarget && window.instructionModel()->rowForAddress(*ins.directTarget)<0) {unknownTarget=ins.directTarget;break;}
        check(unknownTarget.has_value(),"limited fixture must contain an unanalyzed branch destination");
        const auto quotaRows=window.instructionModel()->rowCount();
        check(window.navigateToAddress(*unknownTarget),"queue navigation analysis");waitFor([&]{return !window.analysisBusy();});
        check(window.instructionModel()->rowCount()==quotaRows,"navigation must not reset the project instruction quota or erase the previous result");
        AnalysisLimits memory=limits;memory.maxMemoryBytes=1;window.setAnalysisLimits(memory);
        auto limitedRows=window.instructionModel()->rowCount();
        check(!window.openFile(small,false) && window.instructionModel()->rowCount()==limitedRows,"parser memory stop preserves GUI state");
        window.setAnalysisLimits(limits);window.openFileAsync(large,false);window.close();waitFor([&]{return !window.analysisBusy();});
        {Window destroyBusy(false);destroyBusy.setAnalysisLimits(limits);destroyBusy.openFileAsync(large,false);}
        std::cout<<"PASS: dedicated worker progress, cancellation, GUI heartbeat, latest-request wins, joins, stale results and partial limits\n";return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}

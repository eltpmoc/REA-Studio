// SPDX-License-Identifier: GPL-3.0-or-later
#include "rea/window.hpp"
#include "analysis_fixtures.hpp"
#include <QApplication>
#include <QAbstractItemModelTester>
#include <QGraphicsRectItem>
#include <QTabWidget>
#include <QTest>
#include <iostream>
using namespace rea;
using test::check;
// System executables may expose only an entry ending in an indirect libc call.
// Mandatory direct-edge interaction coverage lives in graph/workspace_gui,
// using smoke.elf whose branch topology is owned by this test suite.
int main(int argc,char** argv) {
    QApplication app(argc,argv);
    try {
        check(argc==4,"arguments");
        Window window(false);window.show();
        check(window.openFile(argv[1],false) && window.smokeValid(),"system workspace");
        const auto* model=window.instructionModel();
        QAbstractItemModelTester tester(const_cast<InstructionModel*>(model),QAbstractItemModelTester::FailureReportingMode::Fatal);
        auto entry=window.selectedAddress();check(entry.has_value(),"entry selected");
        BinaryFile file(argv[1]);
        for(const auto& i:model->analysis().instructions)
            check(file.read(i.offset,i.size)==i.bytes,"displayed instructions match file bytes");
        for(const auto& ref:window.programAnalysis().references.all()) {
            auto row=model->rowForAddress(ref.from);check(row>=0,"reference source decoded");
            const auto* i=model->instruction(row);
            if(ref.flow)check(i->directTarget && *i->directTarget==ref.to,"no fabricated direct reference");
        }
        if(std::string(argv[3])=="graph") {
            auto* graph=window.findChild<GraphView*>("graphView");
            auto* tabs=window.findChild<QTabWidget*>("centralTabs");
            check(graph && tabs,"graph widgets");tabs->setCurrentWidget(graph);QApplication::processEvents();
            const auto* function=window.programAnalysis().registry.containing(*entry);
            check(function,"entry function");
            const auto& cfg=window.programAnalysis().graphs.at(function->entry);
            check(CFGValidator::validate(cfg).empty(),"valid system CFG");
            size_t nodes=0;
            for(auto* item:graph->scene()->items())if(auto* node=qgraphicsitem_cast<QGraphicsRectItem*>(item)) {
                ++nodes;check(model->rowForAddress(node->data(0).toULongLong())>=0,"graph node is decoded");
            }
            check(nodes==cfg.blocks.size() && nodes>0,"all entry CFG blocks rendered");
            check(graph->exportPng(QString::fromLocal8Bit(argv[2])+"/graph-true.png"),"graph PNG");
            check(graph->exportSvg(QString::fromLocal8Bit(argv[2])+"/graph-true.svg"),"graph SVG");
        } else {
            check(window.navigateToOffset(0),"system ELF hex navigation");
            check(window.goBack() && window.selectedAddress()==entry,"system navigation history");
            check(!window.navigateToAddress(UINT64_MAX),"invalid address rejected");
            check(window.grab().save(QString::fromLocal8Bit(argv[2])+"/workspace-true.png"),"system screenshot");
        }
        std::cout<<"PASS system GUI with evidence-derived topology\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}

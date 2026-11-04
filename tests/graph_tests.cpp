// SPDX-License-Identifier: GPL-3.0-or-later
#include "rea/window.hpp"
#include "analysis_fixtures.hpp"
#include <QApplication>
#include <QAbstractItemModelTester>
#include <QGraphicsRectItem>
#include <QTableView>
#include <QTabWidget>
#include <QTest>
#include <QFile>
#include <QXmlStreamReader>
#include <QWheelEvent>
#include <iostream>
using namespace rea;
using test::check;
int main(int argc,char** argv) {
    QApplication app(argc,argv);
    try {
        check(argc>=3,"arguments");Window window(false);window.show();check(window.openFile(argv[1],false),"open");
        auto* graph=window.findChild<GraphView*>("graphView");auto* tabs=window.findChild<QTabWidget*>("centralTabs");
        auto* functions=window.findChild<QTableView*>("functionsView");auto* references=window.findChild<QTableView*>("referencesView");
        check(graph && functions && references && functions->model()->rowCount()>0,"new views populated");
        QAbstractItemModelTester ft(functions->model(),QAbstractItemModelTester::FailureReportingMode::Fatal);
        QAbstractItemModelTester rt(references->model(),QAbstractItemModelTester::FailureReportingMode::Fatal);
        auto entry=window.selectedAddress();check(entry.has_value(),"entry");
        for(const auto& instruction:window.instructionModel()->analysis().instructions)
            if(instruction.directTarget && window.instructionModel()->rowForAddress(*instruction.directTarget)>=0 && *instruction.directTarget!=instruction.address) {check(window.navigateToAddress(instruction.address),"choose function with direct references");entry=window.selectedAddress();break;}
        int refRow=-1;uint64_t origin=0,target=0;
        for(int r=0;r<references->model()->rowCount();++r) {
            auto from=references->model()->data(references->model()->index(r,0),Qt::UserRole).toULongLong();
            auto to=references->model()->data(references->model()->index(r,1),Qt::UserRole).toULongLong();
            if(to!=from && window.instructionModel()->rowForAddress(to)>=0) {refRow=r;origin=from;target=to;break;}
        }
        check(refRow>=0,"navigable function reference");tabs->setCurrentWidget(references);QApplication::processEvents();
        auto index=references->model()->index(refRow,1);references->scrollTo(index);QApplication::processEvents();
        QTest::mouseClick(references->viewport(),Qt::LeftButton,Qt::NoModifier,references->visualRect(index).center());
        QTest::mouseDClick(references->viewport(),Qt::LeftButton,Qt::NoModifier,references->visualRect(index).center());QApplication::processEvents();
        check(window.selectedAddress()==target,"xref double click target");
        check(window.goBack() && window.selectedAddress()==entry,"xref history back");check(window.goForward() && window.selectedAddress()==target,"xref history forward");
        check(window.navigateToAddress(origin),"select reference origin");
        tabs->setCurrentWidget(graph);QApplication::processEvents();
        QGraphicsRectItem* node=nullptr;for(auto* item:graph->scene()->items()) if(auto* r=qgraphicsitem_cast<QGraphicsRectItem*>(item)) {node=r;break;}
        check(node,"graph nodes");graph->ensureVisible(node);QApplication::processEvents();
        auto center=graph->mapFromScene(node->sceneBoundingRect().center());
        QTest::mouseClick(graph->viewport(),Qt::LeftButton,Qt::NoModifier,center);check(node->isSelected(),"block selected");
        auto block=node->data(0).toULongLong();QTest::mouseDClick(graph->viewport(),Qt::LeftButton,Qt::NoModifier,center);QApplication::processEvents();
        check(window.selectedAddress()==block,"graph double click navigates block entry");
        tabs->setCurrentWidget(graph);QApplication::processEvents();
        QWheelEvent wheel(QPointF(100,100),graph->mapToGlobal(QPoint(100,100)),{},QPoint(0,120),Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);
        QApplication::sendEvent(graph->viewport(),&wheel);check(graph->transform().m11()>1,"zoom");
        QString prefix=QString::fromLocal8Bit(argv[2])+"/graph-"+(QString::fromLocal8Bit(argv[1]).endsWith("true")?"true":"fixture");
        check(graph->exportPng(prefix+".png") && graph->exportSvg(prefix+".svg"),"PNG SVG export");
        QImage png(prefix+".png");check(!png.isNull() && png.width()>0,"PNG decode");
        QFile svg(prefix+".svg");check(svg.open(QIODevice::ReadOnly),"read SVG");QXmlStreamReader xml(&svg);bool path=false,text=false;while(!xml.atEnd()){xml.readNext();if(xml.isStartElement()){path|=xml.name()==u"path";text|=xml.name()==u"text";}}check(!xml.hasError() && text && path,"SVG valid XML with nodes and edges");
        check(window.grab().save(prefix+"-workspace.png"),"graph workspace screenshot");
        tabs->setCurrentWidget(references);QApplication::processEvents();check(window.grab().save(prefix+"-references.png"),"references screenshot");
        tabs->setCurrentIndex(0);QApplication::processEvents();
        auto functionIndex=functions->model()->index(0,0);functions->scrollTo(functionIndex);QApplication::processEvents();
        auto functionEntry=functions->model()->data(functionIndex,Qt::UserRole).toULongLong();
        QTest::mouseClick(functions->viewport(),Qt::LeftButton,Qt::NoModifier,functions->visualRect(functionIndex).center());
        QTest::mouseDClick(functions->viewport(),Qt::LeftButton,Qt::NoModifier,functions->visualRect(functionIndex).center());QApplication::processEvents();
        check(window.selectedAddress()==functionEntry,"function double click navigation");
        ControlFlowGraph huge;huge.blocks.resize(257);check(!graph->setGraph(huge),"large graph display limit");
        ControlFlowGraph bad;bad.blocks.push_back({1,{},{}});check(!graph->setGraph(bad),"malformed graph rejected by view");
        check(!graph->exportPng(QString::fromLocal8Bit(argv[2])+"/missing-dir/out.png"),"export error reported");
        std::cout<<"PASS function models, xref history, graph selection/navigation/zoom, limits, PNG and SVG\n";return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}

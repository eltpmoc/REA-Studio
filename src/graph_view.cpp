// SPDX-License-Identifier: GPL-3.0-or-later
#include "rea/graph_view.hpp"
#include <QGraphicsScene>
#include <QGraphicsRectItem>
#include <QGraphicsTextItem>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QPainter>
#include <QFile>
#include <QXmlStreamWriter>
#include <cmath>
namespace rea {
GraphView::GraphView(QWidget* parent):QGraphicsView(parent) {
    setScene(new QGraphicsScene(this));setObjectName("graphView");setDragMode(ScrollHandDrag);setRenderHint(QPainter::Antialiasing);setTransformationAnchor(AnchorUnderMouse);
}
bool GraphView::setGraph(const ControlFlowGraph& graph) {
    scene()->clear();nodes_.clear();blockFor_.clear();resetTransform();zoom_=1;
    if(graph.blocks.size()>256 || graph.edges.size()>1024 || !CFGValidator::validate(graph).empty()) {
        scene()->addText("Graph exceeds display limits (256 nodes / 1024 edges) or is malformed.");return false;
    }
    size_t row=0,total=0;
    for(const auto& b:graph.blocks) total+=b.instructions.size();
    if(total>4096) {scene()->addText("Graph exceeds display limit (4096 instructions).");return false;}
    for(const auto& b:graph.blocks) {
        QString text;
        for(const auto& i:b.instructions) {text+=QString::number(i.address,16)+"  "+QString::fromStdString(i.mnemonic+" "+i.operands).left(100)+'\n';blockFor_[i.address]=b.address;}
        auto* label=scene()->addText(text);label->setDefaultTextColor(QColor("#e5e9f0"));
        label->setFont(QFont("monospace",10));
        auto rect=label->boundingRect().adjusted(-8,-4,8,4);
        auto* node=scene()->addRect(rect,QPen(QColor("#7599bf"),2),QBrush(QColor("#202c3b")));
        node->setPos(80,static_cast<qreal>(row));node->setFlag(QGraphicsItem::ItemIsSelectable);node->setData(0,QVariant::fromValue<qulonglong>(b.address));
        label->setParentItem(node);label->setPos(0,0);nodes_[b.address]=node;
        row+=static_cast<size_t>(rect.height()+65);
    }
    size_t lane=0;
    for(const auto& e:graph.edges) {
        auto source=nodes_.find(e.from);if(source==nodes_.end()) continue;
        const auto a=source->second->sceneBoundingRect();
        if(!e.to || !nodes_.contains(*e.to) || e.type==EdgeType::Unknown) {
            auto* t=scene()->addText(e.type==EdgeType::Return?"return":e.type==EdgeType::Stop?"stop":"unknown / external");t->setDefaultTextColor(QColor("#c89752"));t->setPos(a.right()+12,a.center().y());continue;
        }
        auto b=nodes_.at(*e.to)->sceneBoundingRect();
        qreal x=30-static_cast<qreal>((lane++%12)*12);
        QPainterPath path(QPointF(a.left(),a.center().y()));path.lineTo(x,a.center().y());path.lineTo(x,b.top()-15);path.lineTo(b.center().x(),b.top()-15);path.lineTo(b.center().x(),b.top());
        QColor color=e.type==EdgeType::Conditional?QColor("#d6a853"):QColor("#53b9a3");
        scene()->addPath(path,QPen(color,2));
        QPolygonF arrow;arrow<<QPointF(b.center().x(),b.top())<<QPointF(b.center().x()-5,b.top()-9)<<QPointF(b.center().x()+5,b.top()-9);
        scene()->addPolygon(arrow,QPen(color),QBrush(color));
    }
    scene()->setSceneRect(scene()->itemsBoundingRect().adjusted(-25,-25,25,25));return true;
}
void GraphView::highlight(uint64_t va) {
    for(auto [address,node]:nodes_) node->setPen(QPen(blockFor_.contains(va) && blockFor_.at(va)==address ? QColor("#ffce54"):QColor("#7599bf"),2));
}
void GraphView::wheelEvent(QWheelEvent* e) {
    qreal factor=e->angleDelta().y()>0?1.15:1/1.15;
    if(zoom_*factor>=0.1 && zoom_*factor<=4) {scale(factor,factor);zoom_*=factor;}e->accept();
}
void GraphView::mouseDoubleClickEvent(QMouseEvent* e) {
    auto* item=itemAt(e->pos());while(item && !item->data(0).isValid()) item=item->parentItem();
    const auto address=item ? std::optional<qulonglong>(item->data(0).toULongLong()) : std::nullopt;
    QGraphicsView::mouseDoubleClickEvent(e);
    if(address) emit instructionActivated(*address);
}
bool GraphView::exportPng(const QString& path) {
    auto rect=scene()->sceneRect();if(rect.isEmpty() || rect.width()>8192 || rect.height()>8192 || rect.width()*rect.height()>32000000) return false;
    QImage output(rect.size().toSize(),QImage::Format_ARGB32);if(output.isNull()) return false;output.fill(QColor("#151920"));
    QPainter painter(&output);scene()->render(&painter);painter.end();return output.save(path,"PNG");
}
bool GraphView::exportSvg(const QString& path) {
    // Serialize our own scene primitives; no QtSvg dependency.
    QFile file(path);if(!file.open(QIODevice::WriteOnly)) return false;
    QXmlStreamWriter w(&file);w.writeStartDocument();w.writeStartElement("svg");w.writeDefaultNamespace("http://www.w3.org/2000/svg");
    auto rect=scene()->sceneRect();w.writeAttribute("viewBox",QString("%1 %2 %3 %4").arg(rect.x()).arg(rect.y()).arg(rect.width()).arg(rect.height()));
    for(auto* item:scene()->items(Qt::AscendingOrder)) {
        if(auto* r=qgraphicsitem_cast<QGraphicsRectItem*>(item)) {
            auto b=r->sceneBoundingRect();w.writeEmptyElement("rect");w.writeAttribute("x",QString::number(b.x()));w.writeAttribute("y",QString::number(b.y()));w.writeAttribute("width",QString::number(b.width()));w.writeAttribute("height",QString::number(b.height()));w.writeAttribute("fill",r->brush().color().name());w.writeAttribute("stroke",r->pen().color().name());
        } else if(auto* t=qgraphicsitem_cast<QGraphicsTextItem*>(item)) {
            auto pos=t->scenePos();int line=0;for(const auto& text:t->toPlainText().split('\n')) {w.writeStartElement("text");w.writeAttribute("x",QString::number(pos.x()));w.writeAttribute("y",QString::number(pos.y()+15+line++*16));w.writeAttribute("fill",t->defaultTextColor().name());w.writeAttribute("font-family","monospace");w.writeAttribute("font-size","12");w.writeCharacters(text);w.writeEndElement();}
        } else if(auto* p=qgraphicsitem_cast<QGraphicsPathItem*>(item)) {
            QString d;const auto pathData=p->path();for(int n=0;n<pathData.elementCount();++n) {auto e=pathData.elementAt(n);d+=QString(e.isMoveTo()?"M %1 %2 ":"L %1 %2 ").arg(e.x).arg(e.y);}
            w.writeEmptyElement("path");w.writeAttribute("d",d);w.writeAttribute("fill","none");w.writeAttribute("stroke",p->pen().color().name());
        } else if(auto* p=qgraphicsitem_cast<QGraphicsPolygonItem*>(item)) {
            QString points;for(auto pt:p->polygon()) points+=QString("%1,%2 ").arg(pt.x()).arg(pt.y());w.writeEmptyElement("polygon");w.writeAttribute("points",points);w.writeAttribute("fill",p->brush().color().name());
        }
    }
    w.writeEndElement();w.writeEndDocument();return !w.hasError() && file.flush();
}
}

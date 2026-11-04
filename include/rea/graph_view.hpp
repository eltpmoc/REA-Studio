// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "rea/function_analysis.hpp"
#include <QGraphicsView>
#include <map>
namespace rea {
class GraphView final : public QGraphicsView {
    Q_OBJECT
public:
    explicit GraphView(QWidget* parent=nullptr);
    bool setGraph(const ControlFlowGraph&);
    void highlight(uint64_t instruction);
    bool exportPng(const QString&);
    bool exportSvg(const QString&);
signals:
    void instructionActivated(qulonglong address);
protected:
    void wheelEvent(QWheelEvent*) override;
    void mouseDoubleClickEvent(QMouseEvent*) override;
private:
    std::map<uint64_t,QGraphicsRectItem*> nodes_;
    std::map<uint64_t,uint64_t> blockFor_;
    qreal zoom_{1};
};
}

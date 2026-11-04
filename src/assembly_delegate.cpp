// SPDX-License-Identifier: GPL-3.0-or-later
#include "rea/assembly_delegate.hpp"
#include "rea/instruction_model.hpp"
#include <QApplication>
#include <QPainter>
#include <QRegularExpression>
namespace rea {
QList<QTextLayout::FormatRange> AssemblyDelegate::operandFormats(const QString& text,bool dark,bool selected) {
    // Presentation only: these tokens do not infer any instruction semantics.
    static const QRegularExpression tokens(
        R"(\b(?:r(?:1[0-5]|[0-9])(?:d|w|b)?|[re]?(?:ax|bx|cx|dx|si|di|sp|bp|ip)|[abcd][lh]|[xyz]mm\d+|[cdefgs]s|[kbt]mm?\d+)\b|\b(?:0x[0-9a-f]+|\d+)\b|\b(?:byte|word|dword|qword|xmmword|ymmword|zmmword|ptr)\b|[\[\]+*\-:])",
        QRegularExpression::CaseInsensitiveOption);
    QList<QTextLayout::FormatRange> formats;auto matches=tokens.globalMatch(text);
    while(matches.hasNext()) {
        auto m=matches.next();auto token=m.captured().toLower();QColor color;
        if(token.startsWith("0x") || token.front().isDigit()) color=dark?QColor("#e9be7a"):QColor("#805000");
        else if(token=="ptr" || token.endsWith("word") || token=="byte") color=dark?QColor("#a6b0c2"):QColor("#5c6678");
        else if(token.size()==1 && !token.front().isLetter()) color=dark?QColor("#8997ae"):QColor("#566373");
        else color=dark?QColor("#8bd6c4"):QColor("#14646a");
        QTextCharFormat format;format.setForeground(selected?color.lighter(dark?115:100):color);
        formats.push_back({static_cast<int>(m.capturedStart()),static_cast<int>(m.capturedLength()),format});
    }
    return formats;
}
void AssemblyDelegate::paint(QPainter* painter,const QStyleOptionViewItem& option,const QModelIndex& index) const {
    QStyleOptionViewItem item(option);initStyleOption(&item,index);
    const bool dark=index.data(InstructionModel::DarkRole).toBool();
    const bool selected=item.state.testFlag(QStyle::State_Selected);
    if(index.column()==3) {
        const QString text=item.text;item.text.clear();
        auto* style=item.widget?item.widget->style():QApplication::style();
        style->drawControl(QStyle::CE_ItemViewItem,&item,painter,item.widget);
        const QRect rect=style->subElementRect(QStyle::SE_ItemViewItemText,&item,item.widget).adjusted(3,0,-3,0);
        painter->save();painter->setClipRect(rect);
        painter->setPen(selected?item.palette.color(QPalette::HighlightedText):item.palette.color(QPalette::Text));
        QTextLayout layout(text,item.font);layout.setFormats(operandFormats(text,dark,selected));
        layout.beginLayout();auto line=layout.createLine();if(line.isValid()) line.setLineWidth(100000);layout.endLayout();
        layout.draw(painter,QPointF(rect.left(),rect.top()+(rect.height()-layout.boundingRect().height())/2));painter->restore();
    } else QStyledItemDelegate::paint(painter,item,index);
    painter->save();painter->setClipRect(option.rect);
    if(index.data(InstructionModel::FunctionEntryRole).toBool()) {
        painter->setPen(QColor(dark?"#415470":"#a5b5cb"));painter->drawLine(option.rect.topLeft(),option.rect.topRight());
        if(index.column()==0) painter->fillRect(QRect(option.rect.left(),option.rect.top()+1,3,option.rect.height()-1),QColor(dark?"#779ef2":"#4169ad"));
    }
    if(index.column()==0 && index.data(InstructionModel::IncomingReferencesRole).toULongLong()) {
        painter->setPen(Qt::NoPen);painter->setBrush(QColor(dark?"#e9be7a":"#805000"));
        painter->drawEllipse(QPointF(option.rect.right()-7,option.rect.center().y()),2.5,2.5);
    }
    painter->restore();
}
}

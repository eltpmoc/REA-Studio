// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QStyledItemDelegate>
#include <QTextLayout>
namespace rea {
class AssemblyDelegate final : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter*,const QStyleOptionViewItem&,const QModelIndex&) const override;
    static QList<QTextLayout::FormatRange> operandFormats(const QString&,bool dark,bool selected=false);
};
}

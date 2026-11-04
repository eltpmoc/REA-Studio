// SPDX-License-Identifier: GPL-3.0-or-later
#include "rea/theme.hpp"
#include <QApplication>
#include <QPalette>
namespace rea {
void applyWorkspaceTheme(bool dark) {
    qApp->setStyle("Fusion");QPalette p;
    auto color=[dark](const char* night,const char* day){return QColor(dark?night:day);};
    p.setColor(QPalette::Window,color("#171e2a","#eef0f4"));
    p.setColor(QPalette::WindowText,color("#dce5f2","#20242c"));
    p.setColor(QPalette::Base,color("#101722","#ffffff"));
    p.setColor(QPalette::AlternateBase,color("#151f2d","#f0f3f8"));
    p.setColor(QPalette::Text,p.color(QPalette::WindowText));
    p.setColor(QPalette::Button,color("#222e40","#e5eaf2"));
    p.setColor(QPalette::ButtonText,p.color(QPalette::WindowText));
    p.setColor(QPalette::Highlight,color("#304f79","#c6dcf6"));
    p.setColor(QPalette::HighlightedText,color("#f4f8ff","#14243c"));
    p.setColor(QPalette::ToolTipBase,color("#263449","#fffdf4"));
    p.setColor(QPalette::ToolTipText,p.color(QPalette::WindowText));
    p.setColor(QPalette::Link,color("#96b7ff","#25548b"));
    p.setColor(QPalette::LinkVisited,color("#c5a5ed","#763b9f"));
    p.setColor(QPalette::BrightText,color("#ff9d9b","#a22020"));
    p.setColor(QPalette::Light,color("#41516b","#ffffff"));
    p.setColor(QPalette::Midlight,color("#2e3d53","#e4e9f1"));
    p.setColor(QPalette::Mid,color("#27364c","#bcc7d6"));
    p.setColor(QPalette::Dark,color("#0b111b","#9ca9bd"));
    p.setColor(QPalette::Shadow,color("#080d15","#718199"));
    p.setColor(QPalette::PlaceholderText,color("#8190a7","#6c7787"));
    for(auto role:{QPalette::WindowText,QPalette::Text,QPalette::ButtonText})
        p.setColor(QPalette::Disabled,role,color("#65748a","#8691a1"));
    p.setColor(QPalette::Inactive,QPalette::Highlight,color("#293e5a","#d7e4f4"));
    qApp->setPalette(p);
    // Original slate/ink palette, with restrained blue accents and amber/teal syntax.
    const QString border=color("#2a384e","#c4cddd").name();
    const QString raised=color("#1c2737","#e7ecf4").name();
    const QString hover=color("#293b55","#d5e2f3").name();
    const QString accent=color("#86aaf5","#4169ad").name();
    qApp->setStyleSheet(QString(R"(
QMainWindow::separator { background: %1; width: 3px; height: 3px; }
QMainWindow::separator:hover { background: %4; }
QDockWidget::title { background: %2; padding: 7px 8px; border-bottom: 1px solid %1; font-weight: 600; }
QMenuBar { padding: 3px; border-bottom: 1px solid %1; }
QMenuBar::item { padding: 5px 10px; background: transparent; }
QMenuBar::item:selected, QMenu::item:selected { background: %3; }
QMenu { border: 1px solid %1; padding: 5px; }
QMenu::item { padding: 6px 26px 6px 12px; }
QMenu::separator { height: 1px; background: %1; margin: 5px; }
QToolBar { border: 0; border-bottom: 1px solid %1; spacing: 5px; padding: 5px; }
QToolButton, QPushButton { border: 1px solid transparent; border-radius: 3px; padding: 5px 8px; }
QToolButton:hover, QPushButton:hover { background: %3; border-color: %1; }
QToolButton:pressed, QPushButton:pressed { background: %2; border-color: %4; }
QPushButton { background: %2; border-color: %1; }
QPushButton:default, QPushButton:focus { border-color: %4; }
QTabWidget::pane { border: 1px solid %1; }
QTabBar::tab { background: %2; padding: 7px 10px; border: 1px solid %1; border-bottom: 2px solid transparent; }
QTabBar::tab:selected { border-bottom-color: %4; }
QTabBar::tab:hover { background: %3; }
QAbstractItemView { border: 1px solid %1; outline: 0; }
QAbstractItemView::item { padding: 2px 3px; }
QAbstractItemView::item:hover { background: %3; }
QHeaderView::section { background: %2; border: 0; border-right: 1px solid %1; border-bottom: 1px solid %1; padding: 6px; font-weight: 600; }
QTableView { gridline-color: %1; }
QPlainTextEdit, QTextEdit, QLineEdit, QSpinBox, QComboBox { border: 1px solid %1; border-radius: 2px; padding: 4px; }
QLineEdit:focus, QSpinBox:focus, QComboBox:focus { border-color: %4; }
QComboBox QAbstractItemView { selection-background-color: %3; }
QProgressBar { border: 1px solid %1; border-radius: 3px; text-align: center; min-height: 14px; }
QProgressBar::chunk { background: %3; border-radius: 2px; }
QStatusBar { border-top: 1px solid %1; padding: 2px; }
QStatusBar::item { border: 0; }
QScrollBar:vertical { background: %2; width: 11px; margin: 0; }
QScrollBar:horizontal { background: %2; height: 11px; margin: 0; }
QScrollBar::handle { background: %1; border-radius: 4px; min-width: 22px; min-height: 22px; }
QScrollBar::handle:hover { background: %4; }
QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }
QToolTip { border: 1px solid %1; padding: 6px; }
)").arg(border,raised,hover,accent));
}
}

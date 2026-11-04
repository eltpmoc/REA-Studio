// SPDX-License-Identifier: GPL-3.0-or-later
#include "rea/window.hpp"
#include <QApplication>
#include <QPlainTextEdit>
#include <QTabWidget>
#include <QClipboard>
#include <iostream>
#include <stdexcept>
using namespace rea;
void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}
int main(int argc,char** argv){QApplication app(argc,argv);try{
    check(argc==3,"fixture and capture arguments");Window window(false);window.show();check(window.openFile(argv[1],false),"open actual ELF");
    auto* ir=window.findChild<QPlainTextEdit*>("irView");auto* pseudo=window.findChild<QPlainTextEdit*>("pseudocodeView");auto* tabs=window.findChild<QTabWidget*>("centralTabs");
    check(ir && pseudo && ir->isReadOnly() && pseudo->isReadOnly(),"IR views present");check(ir->toPlainText().contains("lowered") && pseudo->toPlainText().contains("dispatch_ir"),"actual function lifted in worker");
    check(pseudo->toPlainText().contains("Experimental") && pseudo->toPlainText().contains("opaque_x86_step"),"subset and unknown semantics visible");
    tabs->setCurrentWidget(pseudo);pseudo->selectAll();window.copySelection();check(QApplication::clipboard()->text()==pseudo->toPlainText(),"copy pseudocode independent from Assembly");pseudo->moveCursor(QTextCursor::Start);QApplication::processEvents();
    check(window.grab().save(argv[2]),"experimental pseudocode workspace capture");std::cout<<"PASS IR/pseudocode worker result, view and copy\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

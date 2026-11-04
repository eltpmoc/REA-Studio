// SPDX-License-Identifier: GPL-3.0-or-later
#include "rea/window.hpp"
#include <QApplication>
#include <QTimer>
int main(int argc,char** argv) {
    QApplication app(argc,argv); app.setApplicationName("REA Studio"); app.setApplicationVersion(REA_VERSION); app.setOrganizationName("REA Studio");
    const auto args=app.arguments();
    rea::Window window(!args.contains("--smoke")); window.show();
    if(args.size()==3 && args[1]=="--smoke") {
        bool ok=window.openFile(args[2],false) && window.smokeValid();
        QTimer::singleShot(100,&app,[&app,ok]{app.exit(ok ? 0 : 1);});
    } else if(args.size()==2) window.openFileAsync(args[1]);
    return app.exec();
}

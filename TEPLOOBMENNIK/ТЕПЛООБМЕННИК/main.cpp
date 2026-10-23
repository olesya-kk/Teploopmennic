#include "heatexchangerapp.h"
#include <QApplication>
#include <QMessageBox>
int main(int argc,char* argv[]) {
    QApplication app(argc,argv);
    QCoreApplication::setOrganizationName("StudyHeat");
    QCoreApplication::setApplicationName("HeatExchanger");
    try { HeatExchangerApp window; window.show(); return app.exec(); }
    catch(const std::exception& e) {QMessageBox::critical(nullptr,"Ошибка запуска",QString::fromUtf8(e.what()));return 1;}
}

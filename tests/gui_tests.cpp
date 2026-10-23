#include "heatexchangerapp.h"
#include <QApplication>
#include <QTemporaryDir>
#include <QTest>
#include <QElapsedTimer>
#include <QDialog>
#include <QTimer>
#include <iostream>
int main(int argc,char** argv) {
    QTemporaryDir temp;
    qputenv("XDG_DATA_HOME",temp.path().toUtf8());
    QApplication app(argc,argv);
    app.setOrganizationName("StudyHeatTest");app.setApplicationName("HeatTest");
    try {
        HeatExchangerApp window;window.show();
        auto find=[&](const QString& text)->QPushButton* {for(auto b:window.findChildren<QPushButton*>())if(b->text()==text)return b;return nullptr;};
        auto calculate=find("Рассчитать");if(!calculate)return 1;
        QTest::mouseClick(calculate,Qt::LeftButton);
        auto table=window.findChild<QTableWidget*>();QElapsedTimer timer;timer.start();
        while(table->rowCount()==0 && timer.elapsed()<15000) {app.processEvents();QTest::qWait(10);}
        if(table->rowCount()<25)return 2;
        if(table->item(0,1)->text().toDouble()!=250800)return 3;
        auto back=find("Назад");QTest::mouseClick(back,Qt::LeftButton);
        bool loaded=false;
        QTimer::singleShot(100,[&]{
            for(auto dialog:window.findChildren<QDialog*>()) {
                auto history=dialog->findChild<QTableWidget*>();
                if(history && history->rowCount()==1) {
                    for(auto button:dialog->findChildren<QPushButton*>()) if(button->text()=="Загрузить выбранный расчёт") {
                        loaded=true;QTest::mouseClick(button,Qt::LeftButton);return;
                    }
                }
            }
        });
        // Avoid a hung test if history fails to open.
        QTimer::singleShot(3000,[] {if(auto modal=QApplication::activeModalWidget())modal->close();});
        QTest::mouseClick(find("История и сравнение расчётов"),Qt::LeftButton);
        if(!loaded)return 4;
        if(!back->isVisible())return 5;
        window.grab().save("gui-preview.png");
        std::cout<<"GUI calculation, SQLite save and history reload passed\n";
    } catch(const std::exception& e) {std::cerr<<e.what();return 6;}
}

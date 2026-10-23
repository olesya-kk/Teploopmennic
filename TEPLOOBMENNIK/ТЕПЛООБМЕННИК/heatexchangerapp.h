#ifndef HEATEXCHANGERAPP_H
#define HEATEXCHANGERAPP_H

#include <QMainWindow>
#include <QStackedWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QRadioButton>
#include <QButtonGroup>
#include <QPushButton>
#include <QTableWidget>
#include <QProgressBar>
#include <QStatusBar>
#include <QMap>
#include <QFutureWatcher>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <memory>
#include "database.h"

class HeatExchangerApp : public QMainWindow
{
    Q_OBJECT

public:
    HeatExchangerApp(QWidget *parent = nullptr);
    ~HeatExchangerApp();

private slots:
    void updateTemperatureStyles();
    void togglePropsInputs(int streamIdx);
    void handleCalculation();
    void calculationFinished();
    void showHistory();
    void showCatalogs();
    void saveResults();

private:
    void initPage1();
    void initPage2();
    void flashButtonError(const QString &message);
    void populateResults();

    // Основной менеджер страниц
    QStackedWidget *m_stackedWidget;

    // Элементы Страницы 1 (Ввод)
    QRadioButton *m_radioHeat;
    QRadioButton *m_radioCool;
    QButtonGroup *m_buttonGroupMode;

    QLineEdit *m_inputT1n;
    QLineEdit *m_inputT1k;
    QLineEdit *m_inputT2n;
    QLineEdit *m_inputT2k;
    QComboBox *m_comboFlow;

    QLineEdit *m_inputDp1;
    QLineEdit *m_inputDp2;
    QLineEdit *m_inputG1;
    QLineEdit *m_inputG2;

    QComboBox *m_comboSubstance1;
    QComboBox *m_comboSubstance2;

    // Карты для быстрого доступа к полям свойств: Ключ - название ("rho", "mu" и т.д.)
    QMap<QString, QLineEdit*> m_propInputs1;
    QMap<QString, QLineEdit*> m_propInputs2;

    QPushButton *m_btnCalculate;

    // Элементы Страницы 2 (Результаты)
    QLineEdit *m_labelKValue;
    QTableWidget *m_tableResults;

    // Системные компоненты
    QProgressBar *m_progressBar;
    QStatusBar *m_statusBar;

    std::unique_ptr<Database> m_database;
    QFutureWatcher<CalcResult> m_watcher;
    InputData m_input;
    CalcResult m_result;
    QComboBox *m_material, *m_fouling1, *m_fouling2;
    QSpinBox *m_threads, *m_iterations;
    QDoubleSpinBox *m_tolerance, *m_margin;
    InputData collectInput();
    void restoreInput(const InputData& input);
};

#endif // HEATEXCHANGERAPP_H

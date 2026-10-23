#include "heatexchangerapp.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QMessageBox>
#include <QFileDialog>
#include <QTextStream>
#include <QHeaderView>
#include <QDoubleValidator>
#include <QApplication>
#include <QtConcurrent/QtConcurrentRun>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QDialog>
#include <QTabWidget>
#include <QFormLayout>
#include <QScrollArea>
#include <QThread>
#include <QCloseEvent>
#include <algorithm>
#include <cmath>
#include <stdexcept>

HeatExchangerApp::HeatExchangerApp(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("Проектный расчет теплообменника");
    resize(1024, 680);

    QString dir=QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if(!QDir().mkpath(dir)) throw std::runtime_error("Не удалось создать каталог базы");
    m_database=std::make_unique<Database>(dir+"/heat_exchanger.sqlite");
    connect(&m_watcher,&QFutureWatcher<CalcResult>::finished,this,&HeatExchangerApp::calculationFinished);
    m_stackedWidget = new QStackedWidget(this);
    setCentralWidget(m_stackedWidget);

    // Настройка строки статуса
    m_statusBar = new QStatusBar(this);
    setStatusBar(m_statusBar);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setMaximumWidth(200);
    m_progressBar->setVisible(false);
    m_statusBar->addPermanentWidget(m_progressBar);

    initPage1();
    initPage2();
}

HeatExchangerApp::~HeatExchangerApp() { m_watcher.waitForFinished(); }

void HeatExchangerApp::initPage1() {
    QWidget *page = new QWidget();
    QHBoxLayout *mainLayout = new QHBoxLayout(page);
    QVBoxLayout *leftPanel = new QVBoxLayout();

    // 1. Режим работы
    QGroupBox *modeGroup = new QGroupBox("Режим работы", page);
    QHBoxLayout *modeLayout = new QHBoxLayout(modeGroup);
    m_radioHeat = new QRadioButton("Нагрев", modeGroup);
    m_radioCool = new QRadioButton("Охлаждение", modeGroup);
    m_radioHeat->setChecked(true);
    modeLayout->addWidget(m_radioHeat);
    modeLayout->addWidget(m_radioCool);

    m_buttonGroupMode = new QButtonGroup(this);
    m_buttonGroupMode->addButton(m_radioHeat);
    m_buttonGroupMode->addButton(m_radioCool);
    connect(m_buttonGroupMode, &QButtonGroup::buttonToggled, this, &HeatExchangerApp::updateTemperatureStyles);
    leftPanel->addWidget(modeGroup);
    auto note=new QLabel("Горячий поток — в трубах; холодный — в кожухе.\nСвойства и типоразмеры — из таблиц ПАХТ. Допуски давления — в Па.",page);
    note->setWordWrap(true);leftPanel->addWidget(note);

    // 2. Температуры и течение
    QGroupBox *tempFlowGroup = new QGroupBox("Параметры потоков", page);
    QGridLayout *tfLayout = new QGridLayout(tempFlowGroup);
    QDoubleValidator *floatValidator = new QDoubleValidator(-1000000000.0, 1000000000.0, 8, this);

    tfLayout->addWidget(new QLabel("t1н (Начальная Т1), °C:"), 0, 0);
    m_inputT1n = new QLineEdit(tempFlowGroup);
    m_inputT1n->setValidator(floatValidator);
    tfLayout->addWidget(m_inputT1n, 0, 1);

    tfLayout->addWidget(new QLabel("t1к (Конечная Т1), °C:"), 1, 0);
    m_inputT1k = new QLineEdit(tempFlowGroup);
    m_inputT1k->setValidator(floatValidator);
    tfLayout->addWidget(m_inputT1k, 1, 1);

    tfLayout->addWidget(new QLabel("t2н (Начальная Т2), °C:"), 2, 0);
    m_inputT2n = new QLineEdit(tempFlowGroup);
    m_inputT2n->setValidator(floatValidator);
    tfLayout->addWidget(m_inputT2n, 2, 1);

    tfLayout->addWidget(new QLabel("t2к (Конечная Т2), °C:"), 3, 0);
    m_inputT2k = new QLineEdit(tempFlowGroup);
    m_inputT2k->setValidator(floatValidator);
    tfLayout->addWidget(m_inputT2k, 3, 1);

    tfLayout->addWidget(new QLabel("Тип течения:"), 4, 0);
    m_comboFlow = new QComboBox(tempFlowGroup);
    m_comboFlow->addItems({"Противоток", "Прямоток", "Многопроходный (1–3 хода кожуха)"});
    tfLayout->addWidget(m_comboFlow, 4, 1);
    leftPanel->addWidget(tempFlowGroup);

    // 3. Гидравлика и Расход
    QGroupBox *hydroGroup = new QGroupBox("Гидравлика и Расход", page);
    QGridLayout *hydroLayout = new QGridLayout(hydroGroup);

    hydroLayout->addWidget(new QLabel("Допустимое ΔP потока 1, Па:"), 0, 0);
    m_inputDp1 = new QLineEdit(hydroGroup);
    m_inputDp1->setPlaceholderText("Например, 100000");
    hydroLayout->addWidget(m_inputDp1, 0, 1);

    hydroLayout->addWidget(new QLabel("Допустимое ΔP потока 2, Па:"), 1, 0);
    m_inputDp2 = new QLineEdit(hydroGroup);
    m_inputDp2->setPlaceholderText("Например, 100000");
    hydroLayout->addWidget(m_inputDp2, 1, 1);

    hydroLayout->addWidget(new QLabel("Расход G1, кг/с:"), 2, 0);
    m_inputG1 = new QLineEdit(hydroGroup);
    m_inputG1->setValidator(floatValidator);
    hydroLayout->addWidget(m_inputG1, 2, 1);

    hydroLayout->addWidget(new QLabel("Расход G2, кг/с:"), 3, 0);
    m_inputG2 = new QLineEdit(hydroGroup);
    m_inputG2->setValidator(floatValidator);
    hydroLayout->addWidget(m_inputG2, 3, 1);
    leftPanel->addWidget(hydroGroup);

    mainLayout->addLayout(leftPanel, 1);

    // Правая панель: Теплоносители и свойства из вашей таблицы на эскизе
    QVBoxLayout *rightPanel = new QVBoxLayout();
    QGroupBox *propsGroup = new QGroupBox("Теплоносители и физико-химические свойства", page);
    QGridLayout *propsLayout = new QGridLayout(propsGroup);

    QStringList substances = {"Свойства вручную"};
    substances << m_database->fluids();

    propsLayout->addWidget(new QLabel("Теплоноситель 1:"), 0, 0);
    m_comboSubstance1 = new QComboBox(propsGroup);
    m_comboSubstance1->addItems(substances);
    connect(m_comboSubstance1, &QComboBox::currentIndexChanged, this, [this](){ togglePropsInputs(1); });
    propsLayout->addWidget(m_comboSubstance1, 0, 1);

    propsLayout->addWidget(new QLabel("Теплоноситель 2:"), 0, 2);
    m_comboSubstance2 = new QComboBox(propsGroup);
    m_comboSubstance2->addItems(substances);
    connect(m_comboSubstance2, &QComboBox::currentIndexChanged, this, [this](){ togglePropsInputs(2); });
    propsLayout->addWidget(m_comboSubstance2, 0, 3);

    // Массив для генерации строк таблицы свойств
    QList<QPair<QString, QString>> propsData = {
        {"Плотность, кг/м³", "rho"},
        {"Вязкость, Па·с", "mu"},
        {"Теплоемкость, Дж/(кг·К)", "cp"},
        {"Теплопроводность, Вт/(м·К)", "lambda"}
    };

    int rowIdx = 1;
    for (const auto &pair : propsData) {
        propsLayout->addWidget(new QLabel(pair.first), rowIdx, 0);

        QLineEdit *in1 = new QLineEdit(propsGroup);
        in1->setValidator(floatValidator);
        m_propInputs1[pair.second] = in1;
        propsLayout->addWidget(in1, rowIdx, 1);

        QLineEdit *in2 = new QLineEdit(propsGroup);
        in2->setValidator(floatValidator);
        m_propInputs2[pair.second] = in2;
        propsLayout->addWidget(in2, rowIdx, 3);
        rowIdx++;
    }
    rightPanel->addWidget(propsGroup);
    auto settings=new QGroupBox("Подбор и справочники SQLite",page);
    auto form=new QFormLayout(settings);
    m_material=new QComboBox(settings);
    for(const auto& mat:m_database->materials()) m_material->addItem(mat.name,mat.conductivity);
    m_material->setCurrentText("Сталь 12Х18Н10Т");
    form->addRow("Материал стенки",m_material);
    m_fouling1=new QComboBox(settings);m_fouling2=new QComboBox(settings);
    auto fouls=m_database->foulings();
    for(auto it=fouls.begin();it!=fouls.end();++it) {m_fouling1->addItem(it.key(),it.value());m_fouling2->addItem(it.key(),it.value());}
    m_fouling1->setCurrentText("Вода среднего качества");m_fouling2->setCurrentText("Вода среднего качества");
    form->addRow("Загрязнение потока 1",m_fouling1);form->addRow("Загрязнение потока 2",m_fouling2);
    m_threads=new QSpinBox(settings);m_threads->setRange(1,std::max(1,QThread::idealThreadCount()));m_threads->setValue(std::min(4,m_threads->maximum()));
    m_iterations=new QSpinBox(settings);m_iterations->setRange(2,1000);m_iterations->setValue(100);
    m_tolerance=new QDoubleSpinBox(settings);m_tolerance->setDecimals(8);m_tolerance->setRange(0.00000001,0.1);m_tolerance->setValue(0.00001);
    m_margin=new QDoubleSpinBox(settings);m_margin->setRange(0,100);m_margin->setValue(10);
    form->addRow("Потоков OpenMP",m_threads);form->addRow("Допуск по K (доля)",m_tolerance);
    form->addRow("Предел итераций",m_iterations);form->addRow("Минимальный запас площади, %",m_margin);
    rightPanel->addWidget(settings);
    auto history=new QPushButton("История и сравнение расчётов",page);
    connect(history,&QPushButton::clicked,this,&HeatExchangerApp::showHistory);
    rightPanel->addWidget(history);
    auto catalogs=new QPushButton("Справочники и источники ПАХТ",page);
    connect(catalogs,&QPushButton::clicked,this,&HeatExchangerApp::showCatalogs);rightPanel->addWidget(catalogs);

    // Кнопки управления нижнего правого угла
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();

    m_btnCalculate = new QPushButton("Рассчитать", page);
    m_btnCalculate->setMinimumSize(120, 40);
    connect(m_btnCalculate, &QPushButton::clicked, this, &HeatExchangerApp::handleCalculation);

    QPushButton *btnExit = new QPushButton("Выход", page);
    btnExit->setMinimumSize(100, 40);
    connect(btnExit, &QPushButton::clicked, qApp, &QApplication::quit);

    btnLayout->addWidget(m_btnCalculate);
    btnLayout->addWidget(btnExit);
    rightPanel->addLayout(btnLayout);

    mainLayout->addLayout(rightPanel, 1);
    auto scroll=new QScrollArea(this);scroll->setWidgetResizable(true);scroll->setWidget(page);
    m_stackedWidget->addWidget(scroll);
    m_inputT1n->setText("20");m_inputT1k->setText("50");m_inputT2n->setText("90");m_inputT2k->setText("60");
    m_inputG1->setText("2");m_inputG2->setText("2");m_inputDp1->setText("100000");m_inputDp2->setText("100000");
    for(auto* map:{&m_propInputs1,&m_propInputs2}) {
        (*map)["rho"]->setText("997");(*map)["mu"]->setText("0.001");(*map)["cp"]->setText("4180");(*map)["lambda"]->setText("0.6");
    }
    for(auto* field:{m_inputT1n,m_inputT1k}) connect(field,&QLineEdit::editingFinished,this,[this]{togglePropsInputs(1);});
    for(auto* field:{m_inputT2n,m_inputT2k}) connect(field,&QLineEdit::editingFinished,this,[this]{togglePropsInputs(2);});
    updateTemperatureStyles();
}

void HeatExchangerApp::initPage2()
{
    QWidget *page = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(page);

    QHBoxLayout *kLayout = new QHBoxLayout();
    kLayout->addWidget(new QLabel("<b>Коэффициент теплопередачи K (Вт/м²·К):</b>"));
    m_labelKValue = new QLineEdit(page);
    m_labelKValue->setReadOnly(true);
    m_labelKValue->setMaximumWidth(150);
    kLayout->addWidget(m_labelKValue);
    kLayout->addStretch();
    layout->addLayout(kLayout);

    m_tableResults = new QTableWidget(page);
    m_tableResults->setColumnCount(2);
    m_tableResults->setHorizontalHeaderLabels({"Параметр", "Значение"});
    m_tableResults->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_tableResults->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    layout->addWidget(m_tableResults);

    QHBoxLayout *btnLayout = new QHBoxLayout();
    QPushButton *btnBack = new QPushButton("Назад", page);
    btnBack->setMinimumSize(100, 40);
    connect(btnBack, &QPushButton::clicked, this, [this](){ m_stackedWidget->setCurrentIndex(0); });

    QPushButton *btnSave = new QPushButton("Сохранить...", page);
    btnSave->setMinimumSize(120, 40);
    connect(btnSave, &QPushButton::clicked, this, &HeatExchangerApp::saveResults);

    QPushButton *btnExit = new QPushButton("Выход", page);
    btnExit->setMinimumSize(100, 40);
    connect(btnExit, &QPushButton::clicked, qApp, &QApplication::quit);

    btnLayout->addWidget(btnBack);
    btnLayout->addStretch();
    btnLayout->addWidget(btnSave);
    btnLayout->addWidget(btnExit);
    layout->addLayout(btnLayout);

    m_stackedWidget->addWidget(page);
}

void HeatExchangerApp::updateTemperatureStyles()
{
    QString styleActiveBlue = "border: 2px solid #1E90FF; color: blue; font-weight: bold;";
    QString styleActiveRed  = "border: 2px solid #FF4500; color: red; font-weight: bold;";

    if (m_radioHeat->isChecked()) {
        m_inputT1n->setStyleSheet(styleActiveBlue);
        m_inputT1k->setStyleSheet(styleActiveBlue);
        m_inputT2n->setStyleSheet(styleActiveRed);
        m_inputT2k->setStyleSheet(styleActiveRed);
    } else {
        m_inputT1n->setStyleSheet(styleActiveRed);
        m_inputT1k->setStyleSheet(styleActiveRed);
        m_inputT2n->setStyleSheet(styleActiveBlue);
        m_inputT2k->setStyleSheet(styleActiveBlue);
    }
}


namespace {
double number(QLineEdit* field) {
    bool ok=false;double v=field->text().trimmed().replace(',','.').toDouble(&ok);
    if(!ok || !std::isfinite(v)) throw std::runtime_error("Проверьте числовые поля (включая давление и свойства)");
    return v;
}
void setProps(QMap<QString,QLineEdit*>& fields,const FluidProps& p) {
    fields["rho"]->setText(QString::number(p.density,'g',12)); fields["mu"]->setText(QString::number(p.viscosity,'g',12));
    fields["cp"]->setText(QString::number(p.heatCapacity,'g',12)); fields["lambda"]->setText(QString::number(p.conductivity,'g',12));
}
FluidProps readProps(const QMap<QString,QLineEdit*>& f) {return {number(f["rho"]),number(f["mu"]),number(f["cp"]),number(f["lambda"])};}
}
void HeatExchangerApp::togglePropsInputs(int streamIdx) {
    auto combo=streamIdx==1?m_comboSubstance1:m_comboSubstance2;
    auto &fields=streamIdx==1?m_propInputs1:m_propInputs2;
    bool manual=combo->currentIndex()==0;
    for(auto field:fields) field->setEnabled(manual);
    if(!manual) try {
        double mean=streamIdx==1?(number(m_inputT1n)+number(m_inputT1k))/2:(number(m_inputT2n)+number(m_inputT2k))/2;
        setProps(fields,propertiesAt(m_database->fluidTable(combo->currentText()),mean));
    } catch(const std::exception& e) {m_statusBar->showMessage(QString::fromUtf8(e.what()));}
}
InputData HeatExchangerApp::collectInput() {
    InputData in;in.geometryCatalog=m_database->geometryCatalog();
    double t1n=number(m_inputT1n),t1k=number(m_inputT1k),t2n=number(m_inputT2n),t2k=number(m_inputT2k);
    auto p1=readProps(m_propInputs1),p2=readProps(m_propInputs2);
    QVector<PropertyPoint> table1,table2;
    if(m_comboSubstance1->currentIndex()>0) {table1=m_database->fluidTable(m_comboSubstance1->currentText());p1=propertiesAt(table1,(t1n+t1k)/2);setProps(m_propInputs1,p1);}
    if(m_comboSubstance2->currentIndex()>0) {table2=m_database->fluidTable(m_comboSubstance2->currentText());p2=propertiesAt(table2,(t2n+t2k)/2);setProps(m_propInputs2,p2);}
    bool heat=m_radioHeat->isChecked();
    in.t_hot_in=heat?t2n:t1n; in.t_hot_out=heat?t2k:t1k;in.t_cold_in=heat?t1n:t2n;in.t_cold_out=heat?t1k:t2k;
    in.G_hot=number(heat?m_inputG2:m_inputG1);in.G_cold=number(heat?m_inputG1:m_inputG2);
    in.dp_max_hot=number(heat?m_inputDp2:m_inputDp1);in.dp_max_cold=number(heat?m_inputDp1:m_inputDp2);
    in.props_hot=heat?p2:p1;in.props_cold=heat?p1:p2;in.hotTable=heat?table2:table1;in.coldTable=heat?table1:table2;
    in.hotName=(heat?m_comboSubstance2:m_comboSubstance1)->currentText();in.coldName=(heat?m_comboSubstance1:m_comboSubstance2)->currentText();
    in.foulHot=(heat?m_fouling2:m_fouling1)->currentData().toDouble();in.foulCold=(heat?m_fouling1:m_fouling2)->currentData().toDouble();
    in.materialName=m_material->currentText();in.wallConductivity=m_material->currentData().toDouble();
    in.flowType=m_comboFlow->currentIndex();in.threads=m_threads->value();in.maxIterations=m_iterations->value();in.tolerance=m_tolerance->value();in.minimumMargin=m_margin->value();
    return in;
}
void HeatExchangerApp::handleCalculation() {
    try {
        m_input=collectInput();QString error=HeatCalculator::validate(m_input);
        if(!error.isEmpty()) throw std::runtime_error(error.toStdString());
        m_stackedWidget->widget(0)->setEnabled(false);
        m_progressBar->setRange(0,0);m_progressBar->show();m_statusBar->showMessage("Перебор геометрии и проверка гидравлики…");
        const auto input=m_input;
        m_watcher.setFuture(QtConcurrent::run([input]{return HeatCalculator::calculate(input);}));
    } catch(const std::exception& e) {flashButtonError(QString::fromUtf8(e.what()));}
}
void HeatExchangerApp::calculationFinished() {
    m_stackedWidget->widget(0)->setEnabled(true);m_progressBar->hide();
    try {
        m_result=m_watcher.result();
        if(!m_result.success) {m_database->save(m_input,m_result);flashButtonError(m_result.errorMsg);return;}
        populateResults();m_stackedWidget->setCurrentIndex(1);
        int id=m_database->save(m_input,m_result);
        m_statusBar->showMessage(QString("Сохранён №%1 · итераций %2 · невязка %3 · %4/%5 допустимых · %6 мс · потоков %7")
            .arg(id).arg(m_result.iterations).arg(m_result.residual,0,'g',3).arg(m_result.feasible).arg(m_result.candidates).arg(m_result.elapsedMs,0,'f',1).arg(m_result.threadsUsed));
    } catch(const std::exception& e) {flashButtonError(QString::fromUtf8(e.what()));}
}
void HeatExchangerApp::flashButtonError(const QString& message) {
    m_statusBar->showMessage("Ошибка: "+message);
    QMessageBox::warning(this,"Проверка расчёта",message);
}
void HeatExchangerApp::populateResults() {
    m_labelKValue->setText(QString::number(m_result.K,'f',3));
    auto rows=specification(m_result,m_input);m_tableResults->setRowCount(rows.size());
    m_tableResults->setEditTriggers(QAbstractItemView::NoEditTriggers);
    for(int i=0;i<rows.size();++i) {m_tableResults->setItem(i,0,new QTableWidgetItem(rows[i].first));m_tableResults->setItem(i,1,new QTableWidgetItem(rows[i].second));}
}
void HeatExchangerApp::saveResults() {
    QString path=QFileDialog::getSaveFileName(this,"Экспорт спецификации", "specification.txt","Текст (*.txt)");
    if(path.isEmpty()) return;
    QFile file(path);
    if(!file.open(QIODevice::WriteOnly|QIODevice::Text)) {flashButtonError(file.errorString());return;}
    QTextStream out(&file);out<<"СПЕЦИФИКАЦИЯ ТЕПЛООБМЕННИКА\n";
    for(const auto& row:specification(m_result,m_input)) out<<row.first<<": "<<row.second<<"\n";
    out.flush();if(file.error()!=QFile::NoError) flashButtonError(file.errorString());
}
void HeatExchangerApp::restoreInput(const InputData& in) {
    m_radioHeat->setChecked(true);
    auto set=[](QLineEdit* f,double v){f->setText(QString::number(v,'g',12));};
    set(m_inputT1n,in.t_cold_in);set(m_inputT1k,in.t_cold_out);set(m_inputT2n,in.t_hot_in);set(m_inputT2k,in.t_hot_out);
    set(m_inputG1,in.G_cold);set(m_inputG2,in.G_hot);set(m_inputDp1,in.dp_max_cold);set(m_inputDp2,in.dp_max_hot);
    m_comboSubstance1->setCurrentIndex(std::max(0,m_comboSubstance1->findText(in.coldName)));
    m_comboSubstance2->setCurrentIndex(std::max(0,m_comboSubstance2->findText(in.hotName)));
    setProps(m_propInputs1,in.props_cold);setProps(m_propInputs2,in.props_hot);
    m_comboFlow->setCurrentIndex(in.flowType);if(m_material->findText(in.materialName)<0)m_material->addItem(in.materialName,in.wallConductivity);m_material->setCurrentText(in.materialName);
    auto foul=[](QComboBox* c,double value){for(int i=0;i<c->count();++i) if(std::abs(c->itemData(i).toDouble()-value)<1e-12){c->setCurrentIndex(i);return;} c->addItem(QString("Из сохранённого расчёта: R=%1").arg(value,0,'g',10),value);c->setCurrentIndex(c->count()-1);};
    foul(m_fouling1,in.foulCold);foul(m_fouling2,in.foulHot);
    m_threads->setValue(in.threads);m_iterations->setValue(in.maxIterations);m_tolerance->setValue(in.tolerance);m_margin->setValue(in.minimumMargin);
}
void HeatExchangerApp::showHistory() {
    try {
        auto records=m_database->history();
        QDialog dialog(this);dialog.setWindowTitle("Сравнение сохранённых вариантов — двойной щелчок для загрузки");dialog.resize(1100,450);
        auto layout=new QVBoxLayout(&dialog);auto table=new QTableWidget(records.size(),10,&dialog);
        table->setHorizontalHeaderLabels({"№","Время UTC","Геометрия d×δ / n / L / ходы","Материал","K","Площадь, м²","Запас, %","ΔP труб, Па","ΔP кожуха, Па","Статус"});
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);layout->addWidget(table);
        for(int i=0;i<records.size();++i) {
            auto r=resultFromJson(records[i]["result"].toObject());auto g=r.geometry;
            QStringList row={QString::number(records[i]["id"].toInt()),records[i]["created"].toString(),
                QString("%1×%2 мм / %3 / %4 м / %5 труб, %6 кожух").arg(g.tubeOuterDiameter*1000).arg(g.tubeWallThickness*1000).arg(g.tubeCount).arg(g.tubeLength).arg(g.tubePasses).arg(g.shellPasses),g.materialName,
                QString::number(r.K),QString::number(g.heatTransferArea),QString::number(r.areaMargin),QString::number(r.dp_hot),QString::number(r.dp_cold),r.success?"Допустимый":r.errorMsg};
            for(int j=0;j<row.size();++j) table->setItem(i,j,new QTableWidgetItem(row[j]));
        }
        table->resizeColumnsToContents();
        table->setSelectionBehavior(QAbstractItemView::SelectRows);
        table->setSelectionMode(QAbstractItemView::SingleSelection);
        auto loadButton=new QPushButton("Загрузить выбранный расчёт",&dialog);layout->addWidget(loadButton);
        loadButton->setEnabled(!records.isEmpty());
        if(!records.isEmpty())table->selectRow(0);
        auto load=[&](int row,int){
            if(row<0||row>=records.size())return;
            m_input=inputFromJson(records[row]["input"].toObject());m_result=resultFromJson(records[row]["result"].toObject());
            restoreInput(m_input);if(m_result.success)populateResults();m_stackedWidget->setCurrentIndex(m_result.success?1:0);
            m_statusBar->showMessage(QString("Загружен расчёт №%1 · итераций %2 · невязка %3").arg(records[row]["id"].toInt()).arg(m_result.iterations).arg(m_result.residual));dialog.accept();
        };
        connect(table,&QTableWidget::cellDoubleClicked,&dialog,load);
        connect(loadButton,&QPushButton::clicked,&dialog,[&]{load(table->currentRow(),0);});
        dialog.exec();
    } catch(const std::exception& e) {flashButtonError(QString::fromUtf8(e.what()));}
}

void HeatExchangerApp::showCatalogs() {
    try {
        QDialog dialog(this);dialog.setWindowTitle("Справочники SQLite и происхождение данных");dialog.resize(1050,650);
        auto layout=new QVBoxLayout(&dialog);auto tabs=new QTabWidget(&dialog);layout->addWidget(tabs);
        auto table=[&](QString title,QStringList headers){auto t=new QTableWidget(0,headers.size(),tabs);t->setHorizontalHeaderLabels(headers);t->setEditTriggers(QAbstractItemView::NoEditTriggers);tabs->addTab(t,title);return t;};
        auto append=[](QTableWidget* t,QStringList cells){int row=t->rowCount();t->insertRow(row);for(int i=0;i<cells.size();++i)t->setItem(row,i,new QTableWidgetItem(cells[i]));};
        auto fluids=table("Среды",{"Среда","T, °C","ρ, кг/м³","μ, Па·с","cp, Дж/(кг·К)","λ, Вт/(м·К)"});
        for(auto name:m_database->fluids())for(auto point:m_database->fluidTable(name))append(fluids,{name,QString::number(point.temperature),QString::number(point.props.density),QString::number(point.props.viscosity),QString::number(point.props.heatCapacity),QString::number(point.props.conductivity)});
        auto mats=table("Материалы",{"Марка","λ, Вт/(м·К)","[σ] при 20 °C, МПа","ρ, кг/м³"});
        for(auto m:m_database->materials())append(mats,{m.name,QString::number(m.conductivity),QString::number(m.stress),QString::number(m.density)});
        auto foul=table("Загрязнения",{"Тип среды","R, м²·К/Вт"});auto fouls=m_database->foulings();
        for(auto it=fouls.begin();it!=fouls.end();++it)append(foul,{it.key(),QString::number(it.value(),'g',12)});
        auto geoms=table("Типоразмеры",{"D кожуха, м","d труб, м","δ, м","Число труб","Ходы труб","Длина, м"});
        for(auto value:m_database->geometryCatalog()) {auto o=value.toObject();QStringList row;for(auto key:{"shellDiameter","tubeOuterDiameter","tubeWallThickness","tubeCount","tubePasses","tubeLength"})row<<QString::number(o[key].toDouble());append(geoms,row);}
        auto sources=table("Источники",{"Раздел","Источник / условие"});auto metadata=m_database->metadata();
        for(auto it=metadata.begin();it!=metadata.end();++it)append(sources,{it.key(),it.value()});
        for(auto t:dialog.findChildren<QTableWidget*>())t->resizeColumnsToContents();dialog.exec();
    }catch(const std::exception& e){flashButtonError(QString::fromUtf8(e.what()));}
}

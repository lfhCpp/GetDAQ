#include "widgetplot2d.h"
#include "ui_widgetplot2d.h"
#include <QDebug>


WidgetPlot2D* WidgetPlot2D::fWidgetPlot2D = nullptr;//类内静态变量初始化
double WidgetPlot2D::Allkey = 0;
double WidgetPlot2D::key = 0;
//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo....

WidgetPlot2D::WidgetPlot2D(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::WidgetPlot2D),
    time(QTime::currentTime())
{


    ui->setupUi(this);
    this->fWidgetPlot2D=this;
    Busyness=false;//是否已经被调用-忙碌
    // QCP绘图初始化
    initQCP();
    // 初始化控件
    initWidget();
    // 上下分裂器
    ui->splitter->setStretchFactor(0, 20);
    ui->splitter->setStretchFactor(1, 1);
    // 左右分裂器
    ui->splitter1->setStretchFactor(0, 10);
    ui->splitter1->setStretchFactor(1, 3);
    // 滚动条设置
    ui->horizontalScrollBar->setRange(0, 0);
    // 拖动水平滚动条，图幅跟随变化
    connect(ui->horizontalScrollBar, SIGNAL(valueChanged(int)), this, SLOT(horzScrollBarChanged(int)));
    // 默认坐标范围
    ui->customPlot->yAxis->setRange(-20, 20);
    ui->customPlot->replot();

    // widgetPlot2D为通过窗口提升得到的控件
    fWidgetPlot2D->initGraphName(QStringList() <<"ch0"<<"ch1"<<"ch2"<<"ch3"<<"ch4"<<"ch5"<<"ch6"<<"ch7"<<"ch8");


}

WidgetPlot2D::~WidgetPlot2D(){


    if(ui!=nullptr){
        delete ui;
        ui=nullptr;
    }
    if(item!=nullptr){
        delete item;
        item=nullptr;
    }
    if(isShowCheckBox!=nullptr){
        delete isShowCheckBox;
        isShowCheckBox=nullptr;
    }
    if(valueLabel!=nullptr){
        delete valueLabel;
        valueLabel=nullptr;
    }
    if(GraphColorPushButton!=nullptr){
        delete GraphColorPushButton;
        GraphColorPushButton=nullptr;
    }
    if(fQCPAxisTickerTime!=nullptr){
        delete fQCPAxisTickerTime;
        fQCPAxisTickerTime=nullptr;
    }
    if(fWidgetPlot2D!=nullptr){
        delete fWidgetPlot2D;
        fWidgetPlot2D=nullptr;
    }


}

WidgetPlot2D* WidgetPlot2D::GetInstance(){
    if (fWidgetPlot2D==nullptr) {
      fWidgetPlot2D = new WidgetPlot2D();
    }
    return fWidgetPlot2D;
}


/* 初始化曲线名称 */
void WidgetPlot2D::initGraphName(QStringList name)
{
//----------------------------------------------------------------------------------------//
    // 设置表头
    ui->treeWidget->setHeaderLabels(QStringList() << "名称" << "值" << "颜色");
    ui->treeWidget->header()->setVisible(true);
    // 表头文字中间对齐
    QTreeWidgetItem* header = ui->treeWidget->headerItem();
    header->setTextAlignment(0, Qt::AlignLeft);
    header->setTextAlignment(1, Qt::AlignLeft);
    header->setTextAlignment(2, Qt::AlignLeft);
    // 设置列宽
    ui->treeWidget->setColumnWidth(0, 100);
    ui->treeWidget->setColumnWidth(1, 90);
    ui->treeWidget->setColumnWidth(2, 30);
    // 表头顺序是否可以拖动改变
    ui->treeWidget->header()->setCascadingSectionResizes(true);
    // 被选部分是否高亮显示
    ui->treeWidget->header()->setHighlightSections(false);
    // 最后一个区域是否占满表格余下的所有部分
    ui->treeWidget->header()->setStretchLastSection(true);
    // 列表的列数
    ui->treeWidget->setColumnCount(3);
//----------------------------------------------------------------------------------------//
    for (int i = 0; i < name.length(); i++) {
        // 添加曲线列表项
        item = new QTreeWidgetItem;
        ui->treeWidget->addTopLevelItem(item);
//----------------------------------------------------------------------------------------//
        // 向曲线列表项第一列添加曲线是否可见复选框
        isShowCheckBox = new QCheckBox(name.at(i));
        isShowCheckBox->setChecked(true);
        connect(isShowCheckBox, SIGNAL(stateChanged(int)), this, SLOT(changeGraphVisible()));
        ui->treeWidget->setItemWidget(item, 0, isShowCheckBox);
        isShowCheckBoxVector.append(isShowCheckBox);
//----------------------------------------------------------------------------------------//
        // 向曲线列表项第二列添加曲线数值标签
        valueLabel = new QLabel("0");
        QFont font("Courier");
        valueLabel->setFont(font);
        //valueLabel->setAlignment(Qt::AlignHCenter);
        ui->treeWidget->setItemWidget(item, 1, valueLabel);
        valueLabelVector.append(valueLabel);
//----------------------------------------------------------------------------------------//
        // 向曲线列表项第三列添加曲线颜色按钮，点击该按钮弹出颜色选择框，选择曲线颜色
        GraphColorPushButton = new QPushButton("");
        // 随机分配初始按钮背景色
        static int r = 0, g = 0, b = 0;
        r += 150; g += 50; b += 100;
        if (r > 255) r -= 255;
        if (g > 255) g -= 255;
        if (b > 255) b -= 255;
        QPalette pal;
        pal.setColor(QPalette::Button, QColor(r, g, b));
        GraphColorPushButton->setPalette(pal);
        GraphColorPushButton->setAutoFillBackground(true);  // 该句不能缺少，否则背景颜色无法改变
        GraphColorPushButton->setFlat(true);
        // 固定颜色按钮大小
        GraphColorPushButton->setMinimumHeight(18);  // 固定高度
        GraphColorPushButton->setMaximumHeight(18);
        GraphColorPushButton->setMinimumWidth(30);   // 固定宽度
        GraphColorPushButton->setMaximumWidth(30);
        connect(GraphColorPushButton, SIGNAL(clicked()), this, SLOT(changeGraphColor()));
        ui->treeWidget->setItemWidget(item, 2, GraphColorPushButton);
        GraphColorPushButtonVector.append(GraphColorPushButton);

//----------------------------------------------------------------------------------------//
        // 添加曲线
        ui->customPlot->addGraph();
        ui->customPlot->graph(ui->customPlot->graphCount()-1)->setPen(QPen(QColor(r, g, b)));
        ui->customPlot->graph(ui->customPlot->graphCount()-1)->setVisible(true);
        // 将曲线名称与曲线序号一一对应，之后添加数据就可以一一对应
        nameToGraphMap[name.at(i)] = i;
        // 初始化数据容器的值
        for (int t = 0; t < name.length(); t++) {
            valueVector.append(0);
        }
    }
}

/* 添加数据 */
void WidgetPlot2D::addData(QString name, double value)
{
    // 如果点击了“暂停”按钮，则不绘制图形
    if (ui->pausePBtn->text() == "开始") return;
    // 系统当前时间 = 系统运行初始时间 + 系统运行时间
    static double start0 = time.hour()*60*60 + time.minute()*60 + time.second() + time.msec()/1000.0;
    double key0 = start0 + time.elapsed()/1000.0;
//    double start = 0;
//    static int key = 0;
//    key++;
//    qDebug() << "start" << start;
//    qDebug() << "key" << key;
    // 子网格显示
    ui->customPlot->xAxis->grid()->setSubGridVisible(ui->subGridCheck->isChecked());
    ui->customPlot->yAxis->grid()->setSubGridVisible(ui->subGridCheck->isChecked());
    // 自适应量程
    if (ui->autoRangeCheck->isChecked()) {
        ui->customPlot->rescaleAxes();
    }
    // 设置时间轴
    int timeAxis = ui->timeAxisSpin->value();
    ui->customPlot->xAxis->setRange(key0, timeAxis, Qt::AlignRight);
    // x轴和y轴全程显示
    if (ui->fullShowCheck->isChecked()) {
        ui->customPlot->rescaleAxes();
    }
    // 刷新绘图水平滚动条
    ui->horizontalScrollBar->setRange(int(start0), int(key0));  // 刷新滚动条的范围
    ui->horizontalScrollBar->setPageStep(1);                  // 设置翻页步长为 1s 的宽度
    ui->horizontalScrollBar->setValue(int(key0));              // 调整滑块位置到最右边
    // 更新曲线绘图
    ui->customPlot->graph(nameToGraphMap[name])->addData(key0, value);
    //ui->customPlot->replot();
    // 使用rpQueuedReplot参数可以加快绘图速度，避免不必要的重复绘制
    ui->customPlot->replot(QCustomPlot::rpQueuedReplot);
    // 存储曲线的当前值
    valueVector[nameToGraphMap[name]] = value;
//----------------------------------------------------------------------------------------//
    // 计算每秒的帧率
    static double lastFpsKey = key0;
    static int frameCount = 0;
    frameCount++;
    // 每2秒显示一次帧率
    if (key0-lastFpsKey > 0.5) {
        uint64_t sum = 0;
        for (int i = 0; i < ui->customPlot->plottableCount(); i++) {
            sum += uint64_t(ui->customPlot->graph(i)->data()->size());
        }
        ui->statusLabel->setText(QString("%1 FPS, Total Data points: %2").arg(frameCount/(key0-lastFpsKey), 0, 'f', 0).arg(sum));
        lastFpsKey = key0;
        frameCount = 0;
        // 更新数据标签
        for (int t = 0; t < ui->customPlot->plottableCount(); t++) {
            valueLabelVector[t]->setText(QString::number(valueVector[t]));
        }
    }
//----------------------------------------------------------------------------------------//
}

/* 添加数据点 */
void WidgetPlot2D::addDataPoint(QString name, int value)
{
    // 如果点击了“暂停”按钮，则不绘制图形
    if (ui->pausePBtn->text() == "开始") return;

    this->key=key+0.02;//us 50MHz采样频率：20ns
    this->Allkey=Allkey+0.02;

    if(key<=19.98){//1000个采样点后刷新一次
        //更新数据
        ui->customPlot->graph(nameToGraphMap[name])->addData(Allkey, value);
        valueVector[nameToGraphMap[name]] = value;
    }
    else{
        this->key=0;
        // 子网格显示
        ui->customPlot->xAxis->grid()->setSubGridVisible(ui->subGridCheck->isChecked());
        ui->customPlot->yAxis->grid()->setSubGridVisible(ui->subGridCheck->isChecked());
        // 自适应量程
        if (ui->autoRangeCheck->isChecked()) {
            ui->customPlot->rescaleAxes();
        }
        // 设置时间轴
        int timeAxis = ui->timeAxisSpin->value();
        ui->customPlot->xAxis->setRange(key, timeAxis, Qt::AlignRight);
        // x轴和y轴全程显示
        if (ui->fullShowCheck->isChecked()) {
            ui->customPlot->rescaleAxes();
        }
        // 刷新绘图水平滚动条
        ui->horizontalScrollBar->setRange(int(Allkey-20), int(Allkey));  // 刷新滚动条的范围
        ui->horizontalScrollBar->setPageStep(1);                         // 设置翻页步长为 1us 的宽度
        ui->horizontalScrollBar->setValue(int(Allkey));                  // 调整滑块位置到最右边
        // 更新曲线绘图
        ui->customPlot->graph(nameToGraphMap[name])->addData(Allkey, value);
        //ui->customPlot->replot();
        // 使用rpQueuedReplot参数可以加快绘图速度，避免不必要的重复绘制
        ui->customPlot->replot(QCustomPlot::rpQueuedReplot);
        // 存储曲线的当前值
        valueVector[nameToGraphMap[name]] = value;
        ui->statusLabel->setText(QString("Total Data Events: %1").arg(Allkey/20));
        //计数
        if((Allkey/20)>3000) {//累计3000个事件后清零
            for (int i = 0; i < ui->customPlot->graphCount(); i++) {
                //时间轴清零
                this->Allkey = 0;
                this->key = 0;
                // 先获得每条曲线的数据指针，然后删除数据
                ui->customPlot->graph(i)->data()->clear();
                ui->customPlot->replot();
                time = QTime::currentTime();
            }
        }
    }//else if(key==20)

 }//function end

/* 曲线是否显示 */
void WidgetPlot2D::changeGraphVisible()
{
    for (int i = 0; i < isShowCheckBoxVector.length(); i++) {
        ui->customPlot->graph(i)->setVisible(isShowCheckBoxVector[i]->isChecked());
    }
}

/* 改变曲线颜色 */
void WidgetPlot2D::changeGraphColor()
{
    /* 在槽（SLOT)中sender()函数会返回一个指向 QObject 的指针来指向信号的发送者。
     * 然后通过 C++ RTTI(Run-Time Type Identification)机制提供的dynamic_cast运算符，
     * 在执行的时候检查sender()返回的对象是否是QPushButton类，如果是则将sender()返回的QObject指针转换为QPushButton指针，
     * 然后if中的语句就会执行。如果sender()返回的对象不是QPushButton类型的指针，则dynamic_cast就会返回0，if中的语句就不会执行了。
     * */
    if (QPushButton* btn = dynamic_cast<QPushButton*>(sender())) {
        QColor color = QColorDialog::getColor(Qt::white, this);
        // 用户取消颜色对话框
        if (color.isValid() == false)
            return;
        // 设置按钮背景色
        QPalette pal;// = btn->palette();
        pal.setColor(QPalette::Button, color);
        btn->setPalette(pal);
        btn->setAutoFillBackground(true);  // 该句不能缺少，否则背景颜色无法改变
        btn->setFlat(true);                // 该句不能缺少，否则背景颜色无法改变
    }
    // 改变对应的曲线颜色
    for (int i = 0; i < GraphColorPushButtonVector.length(); i++) {
        // 获取按钮的颜色
        QPalette pal = GraphColorPushButtonVector[i]->palette();
        QColor color = pal.color(QPalette::Button);
        // 曲线颜色对应按钮颜色
        ui->customPlot->graph(i)->setPen(QPen(color));
    }
    ui->customPlot->replot();
}

/* QCP绘图初始化 */
void WidgetPlot2D::initQCP()
{
    // 刻度显示
    ui->customPlot->xAxis->setTicks(true);
    ui->customPlot->yAxis->setTicks(true);
    // 刻度值显示
    ui->customPlot->xAxis->setTickLabels(true);
    ui->customPlot->yAxis->setTickLabels(true);
    // 网格显示
    ui->customPlot->xAxis->grid()->setVisible(true);
    ui->customPlot->yAxis->grid()->setVisible(true);
    // 子网格显示
    ui->customPlot->xAxis->grid()->setSubGridVisible(false);
    ui->customPlot->yAxis->grid()->setSubGridVisible(false);
    // 右和上坐标轴、刻度值显示
    ui->customPlot->xAxis2->setVisible(true);
    ui->customPlot->yAxis2->setVisible(true);
    ui->customPlot->yAxis2->setTicks(true);
    ui->customPlot->yAxis2->setTickLabels(true);
    // make top right axes clones of bottom left axes. Looks prettier:
//    ui->customPlot->axisRect()->setupFullAxesBox();
    // make left and bottom axes always transfer their ranges to right and top axes:
    connect(ui->customPlot->xAxis, SIGNAL(rangeChanged(QCPRange)), ui->customPlot->xAxis2, SLOT(setRange(QCPRange)));
    connect(ui->customPlot->yAxis, SIGNAL(rangeChanged(QCPRange)), ui->customPlot->yAxis2, SLOT(setRange(QCPRange)));
    // 暗色主题
    //setPlotTheme(Qt::white, Qt::black);
    // 亮色主题
    setTheme(Qt::black, Qt::white);
    // 可放大缩小和移动
    ui->customPlot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    // x轴以时间形式显示
//    fQCPAxisTickerTime=new QCPAxisTickerTime;
//    QSharedPointer<QCPAxisTickerTime> timeTicker(fQCPAxisTickerTime);
//    timeTicker->setTimeFormat("%h:%m:%s");
//    ui->customPlot->xAxis->setTicker(timeTicker);
    ui->customPlot->axisRect()->setupFullAxesBox();
    ui->customPlot->yAxis->setLabel("Amplitude");
    ui->customPlot->replot();
}

/* 设置绘图主题 */
void WidgetPlot2D::setTheme(QColor axis, QColor background)
{
    // 坐标颜色按钮
    QPalette pal = ui->axisColorPBtn->palette();
    pal.setColor(QPalette::Button, axis);
    ui->axisColorPBtn->setPalette(pal);
    ui->axisColorPBtn->setAutoFillBackground(true);  // 该句不能缺少，否则背景颜色无法改变
    ui->axisColorPBtn->setFlat(true);                // 该句不能缺少，否则背景颜色无法改变
    // 背景颜色按钮
    pal = ui->backgroundColorPBtn->palette();
    pal.setColor(QPalette::Button, background);
    ui->backgroundColorPBtn->setPalette(pal);
    ui->backgroundColorPBtn->setAutoFillBackground(true);  // 该句不能缺少，否则背景颜色无法改变
    ui->backgroundColorPBtn->setFlat(true);                // 该句不能缺少，否则背景颜色无法改变
//----------------------------------------------------------------------------------------//
    // 坐标标注颜色
    ui->customPlot->xAxis->setLabelColor(axis);
    ui->customPlot->yAxis->setLabelColor(axis);
    // 坐标刻度值颜色
    ui->customPlot->xAxis->setTickLabelColor(axis);
    ui->customPlot->yAxis->setTickLabelColor(axis);
    // 坐标基线颜色和宽度
    ui->customPlot->xAxis->setBasePen(QPen(axis, 1));
    ui->customPlot->yAxis->setBasePen(QPen(axis, 1));
    // 坐标主刻度颜色和宽度
    ui->customPlot->xAxis->setTickPen(QPen(axis, 1));
    ui->customPlot->yAxis->setTickPen(QPen(axis, 1));
    // 坐标子刻度颜色和宽度
    ui->customPlot->xAxis->setSubTickPen(QPen(axis, 1));
    ui->customPlot->yAxis->setSubTickPen(QPen(axis, 1));
    // 坐标标注颜色
    ui->customPlot->xAxis2->setLabelColor(axis);
    ui->customPlot->yAxis2->setLabelColor(axis);
    // 坐标刻度值颜色
    ui->customPlot->xAxis2->setTickLabelColor(axis);
    ui->customPlot->yAxis2->setTickLabelColor(axis);
    // 坐标基线颜色和宽度
    ui->customPlot->xAxis2->setBasePen(QPen(axis, 1));
    ui->customPlot->yAxis2->setBasePen(QPen(axis, 1));
    // 坐标主刻度颜色和宽度
    ui->customPlot->xAxis2->setTickPen(QPen(axis, 1));
    ui->customPlot->yAxis2->setTickPen(QPen(axis, 1));
    // 坐标子刻度颜色和宽度
    ui->customPlot->xAxis2->setSubTickPen(QPen(axis, 1));
    ui->customPlot->yAxis2->setSubTickPen(QPen(axis, 1));
    // 整个画布背景色
    ui->customPlot->setBackground(background);
    // 绘图区域背景色
    ui->customPlot->axisRect()->setBackground(background);
    // 刷新绘图
    ui->customPlot->replot();
}

/* 初始化控件 */
void WidgetPlot2D::initWidget()
{

    // 时间轴默认时间宽度
    ui->timeAxisSpin->setValue(20);
    // 主题
    QStringList theme;
    theme << "亮色" << "暗色" << "自定义";
    ui->themeCombo->addItems(theme);
    connect(ui->themeCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(changePlotTheme()));
//----------------------------------------------------------------------------------------//
    // 坐标颜色按钮
    QPalette pal = ui->axisColorPBtn->palette();
    pal.setColor(QPalette::Button, Qt::black);
    ui->axisColorPBtn->setPalette(pal);
    ui->axisColorPBtn->setAutoFillBackground(true);  // 该句不能缺少，否则背景颜色无法改变
    ui->axisColorPBtn->setFlat(true);                // 该句不能缺少，否则背景颜色无法改变
    connect(ui->axisColorPBtn, SIGNAL(clicked()), this, SLOT(changePlotTheme()));
    // 背景颜色按钮
    pal = ui->backgroundColorPBtn->palette();
    pal.setColor(QPalette::Button, Qt::white);
    ui->backgroundColorPBtn->setPalette(pal);
    ui->backgroundColorPBtn->setAutoFillBackground(true);  // 该句不能缺少，否则背景颜色无法改变
    ui->backgroundColorPBtn->setFlat(true);                // 该句不能缺少，否则背景颜色无法改变
    connect(ui->backgroundColorPBtn, SIGNAL(clicked()), this, SLOT(changePlotTheme()));
    // 操作按钮
    connect(ui->clearPBtn,    SIGNAL(clicked()), this, SLOT(plotOperation()));
    connect(ui->fullShowPBtn, SIGNAL(clicked()), this, SLOT(plotOperation()));
    connect(ui->savePBtn,     SIGNAL(clicked()), this, SLOT(plotOperation()));
    connect(ui->pausePBtn,    SIGNAL(clicked()), this, SLOT(plotOperation()));
//----------------------------------------------------------------------------------------//
}

/* 改变画图主题 */
void WidgetPlot2D::changePlotTheme()
{
    // 主题选择
    if (QComboBox* combo = dynamic_cast<QComboBox*>(sender()))
    {
        if (combo->currentText() == "亮色") {
            setTheme(Qt::black, Qt::white);  // 亮色主题

        }
        else if (combo->currentText() == "暗色") {
            setTheme(Qt::white, Qt::black);  // 暗色主题

        }
        else if (combo->currentText() == "自定义") {
            // 绘图坐标颜色
            QPalette axisPal = ui->axisColorPBtn->palette();
            QColor axisColor = axisPal.color(QPalette::Button);
            // 绘图背景颜色
            QPalette backgroundPal = ui->backgroundColorPBtn->palette();
            QColor backgroundColor = backgroundPal.color(QPalette::Button);

            setTheme(axisColor, backgroundColor);
        }
    }

    // 用户自定义主题
    if (QPushButton* btn = dynamic_cast<QPushButton*>(sender())) {
        QColor color = QColorDialog::getColor(Qt::white, this);

        // 用户取消颜色对话框
        if (color.isValid() == false)
            return;
        // 设置按钮背景色
        QPalette pal = btn->palette();
        pal.setColor(QPalette::Button, color);
        btn->setPalette(pal);
        btn->setAutoFillBackground(true);  // 该句不能缺少，否则背景颜色无法改变
        btn->setFlat(true);                // 该句不能缺少，否则背景颜色无法改变
        // 绘图坐标颜色
        QPalette axisPal = ui->axisColorPBtn->palette();
        QColor axisColor = axisPal.color(QPalette::Button);
        // 绘图背景颜色
        QPalette backgroundPal = ui->backgroundColorPBtn->palette();
        QColor backgroundColor = backgroundPal.color(QPalette::Button);
        // 改变主题颜色
        setTheme(axisColor, backgroundColor);
        ui->themeCombo->setCurrentText("自定义");
    }
}

/* 绘图操作 */
void WidgetPlot2D::plotOperation()
{
    if (QPushButton* btn = dynamic_cast<QPushButton*>(sender())) {
        if (btn->text() == "清除") {
            for (int i = 0; i < ui->customPlot->graphCount(); i++) {
                //时间轴清零
                this->Allkey = 0;
                this->key = 0;
                // 先获得每条曲线的数据指针，然后删除数据
                ui->customPlot->graph(i)->data()->clear();
                ui->customPlot->replot();
                time = QTime::currentTime();
            }
        }
        if (btn->text() == "整图") {
            ui->customPlot->rescaleAxes();
            ui->customPlot->replot();
        }
        if (btn->text() == "保存") {
            savePlotPng();
        }
        if (btn->text() == "暂停") {
            btn->setText("开始");
            QIcon icon(":/image/player.png");
            btn->setIcon(icon);
        } else if (btn->text() == "开始") {
            btn->setText("暂停");
            QIcon icon(":/image/pause.png");
            btn->setIcon(icon);
        }
    }
}

/* 判断路径是否存在，不存在则新建，只能创建一级子目录，必须保证上级目录存在 */
bool WidgetPlot2D::isDirExist(QString fullPath)
{
    QDir dir(fullPath);
    if(dir.exists()) {
        return true;
    } else {
        // 创建一级子目录，必须保证上级目录存在
        bool ok = dir.mkdir(fullPath);
        return ok;
    }
}

/* 保存绘图成图片 */
void WidgetPlot2D::savePlotPng()
{
    // 获取程序运行路径
    QString savePath = QCoreApplication::applicationDirPath() + "/output";
    // 判断文件路径
    if (isDirExist(savePath)) {
    } else {
        qDebug() << "保存文件目录未找到!";
        QMessageBox::warning(this,
                             "warning",
                             "保存文件目录未找到!",
                             QMessageBox::Yes,
                             QMessageBox::Yes);
    }
    QString fileName = QFileDialog::getSaveFileName(this,
                                                    "保存波形数据",                    // 对话框的标题
                                                    savePath,                        // 保存的默认路径为程序运行路径
                                                    "Save Picture (*.png *jpg)");    // 打开文件的类型，;隔开
    // 如果用户点击了“取消”按钮
    if (fileName.isNull())
        return;
    // 保存图片
    ui->customPlot->savePng(fileName, 1280, 800, 1.0, -1, 255);
}

/* 水平滚动条移动 */
void WidgetPlot2D::horzScrollBarChanged(int value)
{
    // 若全程显示，则不移动绘图坐标
    if (ui->fullShowCheck->isChecked())
        return;
    int timeAxis = ui->timeAxisSpin->value();
    ui->customPlot->xAxis->setRange(value, timeAxis, Qt::AlignRight);
    ui->customPlot->replot();
}


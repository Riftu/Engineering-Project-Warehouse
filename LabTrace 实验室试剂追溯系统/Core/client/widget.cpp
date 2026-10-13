#include "widget.h"
#include "ui_widget.h"
#include <QFileDialog>
#include <QStandardPaths>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSplitter>
#include <QHeaderView>

Widget::Widget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Widget)
{
    ui->setupUi(this);

    setupRealtimeChart();
    setupResponsiveLayout();

    // 设置表格列
    ui->tableTelemetry->setColumnCount(9);
    ui->tableTelemetry->setHorizontalHeaderLabels({
        "设备", "操作员", "试剂",
        "领用前(g)", "领用后(g)", "实际取用量(g)",
        "温度(°C)", "湿度(%)", "时间"
    });

    // 窗口变宽时列可以手动扩展；窗口较窄时保留横向滚动条查看完整字段。
    ui->tableTelemetry->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    ui->tableTelemetry->horizontalHeader()->setStretchLastSection(true);
    ui->tableTelemetry->horizontalHeader()->setMinimumSectionSize(90);
    ui->tableTelemetry->setColumnWidth(0, 125);
    ui->tableTelemetry->setColumnWidth(1, 125);
    ui->tableTelemetry->setColumnWidth(2, 125);
    ui->tableTelemetry->setColumnWidth(3, 105);
    ui->tableTelemetry->setColumnWidth(4, 105);
    ui->tableTelemetry->setColumnWidth(5, 125);
    ui->tableTelemetry->setColumnWidth(6, 100);
    ui->tableTelemetry->setColumnWidth(7, 100);
    ui->tableTelemetry->setColumnWidth(8, 170);
    ui->tableTelemetry->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    ui->tableTelemetry->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableTelemetry->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tableTelemetry->setRowCount(0);

    // 命令预设
    ui->commandCombo->addItem("查询历史-今天",        "query_today");
    ui->commandCombo->addItem("查询历史-最近1小时",   "query_1h");
    ui->commandCombo->addItem("远程开门",             "open_door");
    ui->commandCombo->addItem("远程关门",             "close_door");
    //设备
    ui->deviceCombo->addItem("stm32_001","stm32_001");
    ui->deviceCombo->addItem("stm32_002","stm32_002");

    //选择某个设备
    connect(ui->deviceCombo, &QComboBox::currentTextChanged,this, [this](const QString &dev)
    {
        // 只提示，不清数据
        ui->textBrowser->append(QString("[切换] 下次查询目标: %1").arg(dev));
    });

    m_netThread = new QThread;
    m_tcpClient = new TcpClient;
    m_tcpClient->moveToThread(m_netThread);
    connect(m_netThread, &QThread::finished, m_tcpClient, &QObject::deleteLater);
    m_netThread->start();

    connect(this,&Widget::sigConnectServer,m_tcpClient,&TcpClient::slotConnect);
    connect(this,&Widget::sigSendJson,m_tcpClient,&TcpClient::slotSendJson);

    connect(m_tcpClient,&TcpClient::sigConnected,this,&Widget::onTcpConnected);
    connect(m_tcpClient,&TcpClient::sigRealData,this,&Widget::onRealData);
    connect(m_tcpClient,&TcpClient::sigNetError,this,&Widget::onNetErr);
    connect(m_tcpClient,&TcpClient::sigDisconnected,this,&Widget::onTcpDisconnect);

    // 历史查询结果由网络线程传给主界面
    connect(m_tcpClient,&TcpClient::sigHistoryResult,this,&Widget::onQueryHistoryResult);
    connect(m_tcpClient, &TcpClient::sigBatchReport, this, &Widget::onBatchReport);
    connect(m_tcpClient, &TcpClient::sigDeviceHeartbeat, this,
            [this](const QJsonObject &heartbeat)
    {
        const QString deviceId = heartbeat.value("device_id").toString();
        const quint32 failedMask = static_cast<quint32>(
            heartbeat.value("failed_task_mask").toVariant().toULongLong());
        if (failedMask == 0U)
        {
            ui->connStatusLabel->setText("● 已连接 / 设备在线");
            ui->connStatusLabel->setStyleSheet("color: green; font-weight: bold;");
        }
        else
        {
            ui->textBrowser->append(QString("[故障] %1 任务心跳异常 mask=0x%2")
                .arg(deviceId)
                .arg(failedMask, 8, 16, QLatin1Char('0')));
        }
    });
    connect(m_tcpClient, &TcpClient::sigCommandResult, this,
            [this](const QJsonObject &result)
    {
        const bool success = result.value("success").toBool();
        ui->textBrowser->append(success
            ? QStringLiteral("[服务端] MQTT命令发布成功，等待设备执行回执")
            : QStringLiteral("[服务端] MQTT命令发布失败"));
    });
    connect(m_tcpClient, &TcpClient::sigCommandAck, this,
            [this](const QJsonObject &ack)
    {
        const QString deviceId = ack.value("device_id").toString();
        const QString command = ack.value("command").toString();
        const bool success = ack.value("success").toBool();
        const QString action = command == QStringLiteral("open_door")
            ? QStringLiteral("远程开门") : QStringLiteral("远程关门");
        ui->textBrowser->append(QString("[设备回执] %1 %2%3")
            .arg(deviceId, action, success ? QStringLiteral("执行成功")
                                           : QStringLiteral("执行失败")));
    });
    m_taskThread = new QThread;
    m_taskWorker = new TaskWorker;
    m_taskWorker->moveToThread(m_taskThread);
    connect(m_taskThread, &QThread::finished, m_taskWorker, &QObject::deleteLater);
    m_taskThread->start();

    //主线程 -> 任务线程：下发分析/导出任务
    connect(this,&Widget::sigStartAnalyse,m_taskWorker,&TaskWorker::slotAnalyseData);
    connect(this,&Widget::sigExportCsv,m_taskWorker,&TaskWorker::slotExportCsv);

    //任务线程 -> 主线程：返回计算结果
    connect(m_taskWorker,&TaskWorker::sigAnalyseDone,this,&Widget::onAnalyseFinish);
    connect(m_taskWorker,&TaskWorker::sigExportDone,this,&Widget::onExportFinish);

    // 自动连接本机服务端的 TCP 6666 端口
    emit sigConnectServer(QStringLiteral("127.0.0.1"), 6666);
}

void Widget::setupRealtimeChart()
{
    m_temperatureSeries = new QLineSeries(this);
    m_humiditySeries = new QLineSeries(this);
    m_temperatureSeries->setName(QStringLiteral("温度 (°C)"));
    m_humiditySeries->setName(QStringLiteral("湿度 (%)"));

    m_chart = new QChart;
    m_chart->setTitle(QStringLiteral("实时环境波形（最近60条）"));
    m_chart->legend()->setVisible(true);
    m_chart->addSeries(m_temperatureSeries);
    m_chart->addSeries(m_humiditySeries);

    m_sampleAxis = new QValueAxis(this);
    m_temperatureAxis = new QValueAxis(this);
    m_humidityAxis = new QValueAxis(this);
    m_sampleAxis->setTitleText(QStringLiteral("采样序号"));
    m_sampleAxis->setRange(0, 59);
    m_sampleAxis->setLabelFormat("%.0f");
    m_temperatureAxis->setTitleText(QStringLiteral("温度 °C"));
    m_temperatureAxis->setRange(0, 50);
    m_temperatureAxis->setLabelFormat("%.0f");
    m_humidityAxis->setTitleText(QStringLiteral("湿度 %"));
    m_humidityAxis->setRange(0, 100);
    m_humidityAxis->setLabelFormat("%.0f");

    m_chart->addAxis(m_sampleAxis, Qt::AlignBottom);
    m_chart->addAxis(m_temperatureAxis, Qt::AlignLeft);
    m_chart->addAxis(m_humidityAxis, Qt::AlignRight);
    m_temperatureSeries->attachAxis(m_sampleAxis);
    m_temperatureSeries->attachAxis(m_temperatureAxis);
    m_humiditySeries->attachAxis(m_sampleAxis);
    m_humiditySeries->attachAxis(m_humidityAxis);

    m_chartView = new QChartView(m_chart, this);
    m_chartView->setRenderHint(QPainter::Antialiasing);
    m_chartView->setMinimumHeight(220);
}

void Widget::setupResponsiveLayout()
{
    setWindowTitle(QStringLiteral("实验室试剂追溯客户端"));
    setMinimumSize(1000, 700);
    resize(1280, 820);

    // 原输入框未参与当前协议，隐藏后避免与命令区重复。
    ui->lineEdit->hide();

    QHBoxLayout *headerLayout = new QHBoxLayout;
    headerLayout->addWidget(ui->connStatusLabel);
    headerLayout->addStretch();
    headerLayout->addWidget(ui->label);
    headerLayout->addStretch();

    QSplitter *dataSplitter = new QSplitter(Qt::Horizontal, this);
    dataSplitter->addWidget(ui->tableTelemetry);
    dataSplitter->addWidget(ui->textBrowser);
    dataSplitter->setStretchFactor(0, 3);
    dataSplitter->setStretchFactor(1, 2);

    QSplitter *mainSplitter = new QSplitter(Qt::Vertical, this);
    mainSplitter->addWidget(dataSplitter);
    mainSplitter->addWidget(m_chartView);
    mainSplitter->setStretchFactor(0, 3);
    mainSplitter->setStretchFactor(1, 2);

    QHBoxLayout *controlLayout = new QHBoxLayout;
    controlLayout->addWidget(ui->deviceCombo);
    controlLayout->addWidget(ui->commandCombo);
    controlLayout->addWidget(ui->pushButton);
    controlLayout->addStretch();
    controlLayout->addWidget(ui->analyseBtn);
    controlLayout->addWidget(ui->exportBtn);

    QVBoxLayout *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(12, 12, 12, 12);
    rootLayout->addLayout(headerLayout);
    rootLayout->addWidget(mainSplitter, 1);
    rootLayout->addLayout(controlLayout);
}

void Widget::appendChartPoint(const QVariantMap &data)
{
    const qint64 x = m_chartSampleIndex++;
    m_temperatureSeries->append(x, data.value("temperature").toDouble());
    m_humiditySeries->append(x, data.value("humidity").toDouble());

    const int excess = m_temperatureSeries->count() - 60;
    if (excess > 0)
    {
        m_temperatureSeries->removePoints(0, excess);
        m_humiditySeries->removePoints(0, excess);
    }
    m_sampleAxis->setRange(qMax<qint64>(0, x - 59), qMax<qint64>(59, x));
}

void Widget::rebuildChart(const QVariantList &list)
{
    m_temperatureSeries->clear();
    m_humiditySeries->clear();
    m_chartSampleIndex = 0;
    const int start = qMax(0, list.size() - 60);
    for (int i = start; i < list.size(); ++i)
        appendChartPoint(list.at(i).toMap());
}
Widget::~Widget()
{
    // 先断开所有信号，防止线程还在给已析构对象发信号
    disconnect(m_tcpClient, nullptr, this, nullptr);
    disconnect(m_taskWorker, nullptr, this, nullptr);

    //安全停止网络子线程
    m_netThread->quit();
    m_netThread->wait(); // 等待线程安全退出
    delete m_netThread;

    //安全释放任务线程
    m_taskThread->quit();
    m_taskThread->wait();
    delete m_taskThread;

    delete ui;
}

// 追加一行到表格
void Widget::appendRowToTable(const QVariantMap &data)
{
    QString deviceId     = data.value("device_id").toString();
    QString operatorName = data.value("operator_name").toString();
    QString reagentName  = data.value("reagent_name").toString();
    if(operatorName.isEmpty())
        operatorName = data.value("operator_uid").toString();
    if(reagentName.isEmpty())
        reagentName = data.value("reagent_uid").toString();

    double  weightBefore = data.value("weight_before").toDouble();
    double  weightAfter  = data.value("weight_after").toDouble();
    double  weightDelta  = data.value("weight_delta").toDouble();
    double  temp         = data.value("temperature").toDouble();
    double  humi         = data.value("humidity").toDouble();
    qint64  ts           = data.value("timestamp").toLongLong();

    QString timeStr = ts > 0
        ? QDateTime::fromMSecsSinceEpoch(ts).toString("yyyy-MM-dd HH:mm:ss")
        : "-";

    int row = ui->tableTelemetry->rowCount();
    ui->tableTelemetry->insertRow(row);

    ui->tableTelemetry->setItem(row, 0, new QTableWidgetItem(deviceId));
    ui->tableTelemetry->setItem(row, 1, new QTableWidgetItem(operatorName));
    ui->tableTelemetry->setItem(row, 2, new QTableWidgetItem(reagentName));
    ui->tableTelemetry->setItem(row, 3, new QTableWidgetItem(QString::number(weightBefore, 'f', 1)));
    ui->tableTelemetry->setItem(row, 4, new QTableWidgetItem(QString::number(weightAfter, 'f', 1)));
    ui->tableTelemetry->setItem(row, 5, new QTableWidgetItem(QString::number(weightDelta, 'f', 1)));
    ui->tableTelemetry->setItem(row, 6, new QTableWidgetItem(QString::number(temp, 'f', 1)));
    ui->tableTelemetry->setItem(row, 7, new QTableWidgetItem(QString::number(humi, 'f', 1)));
    ui->tableTelemetry->setItem(row, 8, new QTableWidgetItem(timeStr));
}

// 表格大小限制，填表
void Widget::fillTableFromList(const QVariantList &list)
{
    ui->tableTelemetry->setRowCount(0);

    for(const QVariant &item : list)
    {
        appendRowToTable(item.toMap());
    }

    if(ui->tableTelemetry->rowCount() > 500)
    {
        ui->tableTelemetry->removeRow(0);
    }
}

// TCP真正连接成功之后，再发送登录指令
void Widget::onTcpConnected()
{
    ui->connStatusLabel->setText("● 已连接");
    ui->connStatusLabel->setStyleSheet("color: green; font-weight: bold;");
    ui->textBrowser->append("[系统] ✅ TCP连接成功");
}

void Widget::onTcpDisconnect()
{
    ui->connStatusLabel->setText("● 已断开，正在重连...");
    ui->connStatusLabel->setStyleSheet("color: red; font-weight: bold;");
}

void Widget::onNetErr(QString err)
{
    ui->textBrowser->append("[系统] 网络错误: " + err);
}

// 实时数据
 void Widget::onRealData(QVariantMap data)
 {

     QString deviceId = data.value("device_id").toString();
     qint64  ts       = data.value("timestamp").toLongLong();

     // 去重
     QString key = QString("%1_%2").arg(deviceId).arg(ts);

     if(m_shownKeys.contains(key))
     {
         qDebug() << "重复，跳过显示:" << key;
         return;
     }
     m_shownKeys.insert(key);
     if(m_shownKeys.size() > 2000)
         m_shownKeys.clear();

     const int alarmFlags = data.value("alarm_flags").toInt();
     if(alarmFlags != 0)
     {
         QStringList alarms;
         if(alarmFlags & 0x0001) alarms << "低库存";
         if(alarmFlags & 0x0002) alarms << "温度过低";
         if(alarmFlags & 0x0004) alarms << "温度过高";
         if(alarmFlags & 0x0008) alarms << "湿度过低";
         if(alarmFlags & 0x0010) alarms << "湿度过高";
         if(alarmFlags & 0x0020) alarms << "任务心跳异常";
         if(alarmFlags & 0x0040) alarms << "IWDG复位";
         ui->textBrowser->append(QString("[报警] %1: %2")
             .arg(deviceId, alarms.join("、")));
     }

     // 加到 m_currentData
     m_currentData.append(data);
     if(m_currentData.size() > 500)
     m_currentData.removeFirst();   //  删最旧的

     // 填表格
     appendRowToTable(data);
     appendChartPoint(data);
     //超过 500 行，删第一行（最旧的）
     if(ui->tableTelemetry->rowCount() > 500)
             ui->tableTelemetry->removeRow(0);
}

//  历史查询结果
void Widget::onQueryHistoryResult(QVariantList list)
{
    m_historyList = list;                  // 保存供导出
    m_currentData = list;

    fillTableFromList(list);
    rebuildChart(list);

    ui->textBrowser->append(QString("历史查询结果：共 %1 条").arg(list.size()));
}

void Widget::onExportFinish(bool ok, QString info)
{
    ui->textBrowser->append(ok ? "[导出] ✅ " + info : "[导出] ⚠️ " + info);
}

//  日报
void Widget::onBatchReport(const QJsonObject &packet)
{
    QVariantList list = packet.value("data").toArray().toVariantList();
    QString date = packet.value("date").toString();

    if(list.isEmpty())
    {
        ui->textBrowser->append(QString("[日报] %1 无数据").arg(date));
        return;
    }

    // 保存一份，供后续导出
    m_historyList = list;
    m_currentData = list;
    fillTableFromList(list);
    rebuildChart(list);

    ui->textBrowser->append(QString("[日报] %1 收到 %2 条，可点击\"导出CSV\"保存")
                     .arg(date).arg(list.size()));
}

//任务线程回调--分析结果
void Widget::onAnalyseFinish(QVariantMap result)
{
    if(result.value("status").toString() != "ok")
    {
        ui->textBrowser->append("[分析] 无数据");
        return;
    }

    ui->textBrowser->append(QString("[分析] 共 %1 条")
        .arg(result.value("count").toInt()));
    ui->textBrowser->append(QString("  温度: %1 ~ %2 °C, 平均 %3 °C")
        .arg(result.value("temp_min").toDouble(), 0, 'f', 1)
        .arg(result.value("temp_max").toDouble(), 0, 'f', 1)
        .arg(result.value("temp_avg").toDouble(), 0, 'f', 1));
    ui->textBrowser->append(QString("  湿度: %1 ~ %2 %%, 平均 %3 %%")
        .arg(result.value("humi_min").toDouble(), 0, 'f', 1)
        .arg(result.value("humi_max").toDouble(), 0, 'f', 1)
        .arg(result.value("humi_avg").toDouble(), 0, 'f', 1));
    ui->textBrowser->append(QString("  总取用量: %1 g")
        .arg(result.value("weight_total").toDouble(), 0, 'f', 2));
}

//  发送命令
void Widget::on_pushButton_clicked()
{
    QString devId = ui->deviceCombo->currentData().toString();
    if(devId.isEmpty())
    {
        QMessageBox::warning(this, "提示", "请先选择设备");
        return;
    }

    QString cmd = ui->commandCombo->currentData().toString();
    QJsonObject obj;
    obj["device_id"] = devId;

    if(cmd == "query_today")
    {
        qint64 today0 = QDateTime(QDate::currentDate(), QTime(0,0)).toMSecsSinceEpoch();
        qint64 now    = QDateTime::currentMSecsSinceEpoch();

        obj["type"]       = "history_query";
        obj["start_time"] = today0;
        obj["end_time"]   = now;
    }
    else if(cmd == "query_1h")
    {
       qint64 now   = QDateTime::currentMSecsSinceEpoch();
       qint64 hour0 = now - 3600 * 1000;

       obj["type"]       = "history_query";
       obj["start_time"] = hour0;
       obj["end_time"]   = now;
    }

    else if(cmd == "open_door" || cmd == "close_door")
    {
        obj["type"]    = "command";
        obj["command"] = cmd;
    }
    else
    {
        QMessageBox::warning(this, "提示", "未知命令");
        return;
    }

    emit sigSendJson(obj);
    ui->textBrowser->append(QString("[发送] %1: %2")
                    .arg(devId).arg(ui->commandCombo->currentText()));
}

//导出
void Widget::on_exportBtn_clicked()
{
    if(m_currentData.isEmpty())
    {
        QMessageBox::warning(this, "提示", "没有数据可导出，请先查询历史");
        return;
    }

    QString path = QFileDialog::getSaveFileName(this, "保存CSV",
        QStandardPaths::writableLocation(QStandardPaths::DesktopLocation)
            + "/试剂取用记录_"
            + QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss")
            + ".csv",
        "CSV (*.csv)");

    if(path.isEmpty()) return;

    emit sigExportCsv(m_currentData, path);
    ui->textBrowser->append(QString("[导出] 正在导出 %1 条...").arg(m_currentData.size()));
}

//分析
void Widget::on_analyseBtn_clicked()
{
   if(m_currentData.isEmpty())
   {
        QMessageBox::warning(this, "提示", "没有数据可分析");
        return;
   }
   emit sigStartAnalyse(m_currentData);
}



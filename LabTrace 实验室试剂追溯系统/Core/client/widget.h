#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include <QThread>
#include <QDebug>
#include <QMessageBox>
#include <QStandardItemModel>
#include <QDateTime>
#include <QVariant>
#include <QSet>
#include <QtCharts/QChart>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <QtCharts/QChartView>
#include "tcpclient.h"
#include "taskworker.h"

QT_CHARTS_USE_NAMESPACE

QT_BEGIN_NAMESPACE
namespace Ui { class Widget; }
QT_END_NAMESPACE

class Widget : public QWidget
{
    Q_OBJECT

public:
    Widget(QWidget *parent = nullptr);
    ~Widget();

signals:
    // 给TcpClient子线程发送指令
    void sigConnectServer(QString ip,quint16 port);
    void sigSendJson(QJsonObject obj);

    // 给TaskWorker任务线程下发任务
    void sigStartAnalyse(QVariantList rawData);
    void sigExportCsv(QVariantList rawData, QString filePath);
    void sigQueryHistoryReq(QString st, QString et);

public slots:

    // 接收网络线程返回的信号，主线程执行，刷新UI
    void onRealData(QVariantMap data);
    void onNetErr(QString err);
    void onTcpConnected(); //TCP连接成功回调
    void onTcpDisconnect();

    //任务线程回调
    void onAnalyseFinish(QVariantMap result);
    void onExportFinish(bool ok, QString info);
    void onQueryHistoryResult(QVariantList list);

private slots:
    void on_pushButton_clicked();

    void onBatchReport(const QJsonObject &packet);

    void on_exportBtn_clicked();

    void on_analyseBtn_clicked();

    void fillTableFromList(const QVariantList &list);

    void appendRowToTable(const QVariantMap &data);
private:
    void setupResponsiveLayout();
    void setupRealtimeChart();
    void appendChartPoint(const QVariantMap &data);
    void rebuildChart(const QVariantList &list);

    Ui::Widget *ui;

    //网络线程
    QThread* m_netThread;
    TcpClient* m_tcpClient;

    //任务线程
    QThread* m_taskThread;
    TaskWorker* m_taskWorker;

    QString m_loginUser;
    QString m_loginPwd;

    QVariantList m_historyList;
    QVariantList m_currentData;
    QSet<QString> m_shownKeys;

    QChart *m_chart = nullptr;
    QChartView *m_chartView = nullptr;
    QLineSeries *m_temperatureSeries = nullptr;
    QLineSeries *m_humiditySeries = nullptr;
    QValueAxis *m_sampleAxis = nullptr;
    QValueAxis *m_temperatureAxis = nullptr;
    QValueAxis *m_humidityAxis = nullptr;
    qint64 m_chartSampleIndex = 0;

};
#endif // WIDGET_H

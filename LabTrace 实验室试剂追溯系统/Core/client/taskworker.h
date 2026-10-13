#ifndef TASKWORKER_H
#define TASKWORKER_H
#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <QDateTime>

class TaskWorker : public QObject
{
    Q_OBJECT
public:
    explicit TaskWorker(QObject *parent = nullptr);

signals:
    // 数据分析完成回调
    void sigAnalyseDone(QVariantMap result);
    // CSV导出完成回调
    void sigExportDone(bool ok, QString info);

public slots:
    // 数据分析
    void slotAnalyseData(QVariantList rawData);
    // CSV导出
    void slotExportCsv(QVariantList rawData, QString filePath);
    // 历史数据库查询
    void slotQueryHistory(QString startTime, QString endTime);
};

#endif // TASKWORKER_H

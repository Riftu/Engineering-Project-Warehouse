#include "taskworker.h"
#include <QDebug>
#include <QFile>
#include <QTextStream>

TaskWorker::TaskWorker(QObject *parent)
    : QObject(parent)
{

}

void TaskWorker::slotAnalyseData(QVariantList rawData)
{
    QVariantMap res;
    if(rawData.isEmpty())
    {
        res["status"] = "empty";
        res["count"]  = 0;
        emit sigAnalyseDone(res);
        return;
    }

    //  统计温湿度
    double tMin = 1e9, tMax = -1e9, tSum = 0;
    double hMin = 1e9, hMax = -1e9, hSum = 0;
    // 重量
    double wSum  = 0;   // 总实际取用量
    double wbSum = 0;   // 总领用前重量
    double waSum = 0;   // 总领用后重量

    int validCount = 0;

    for(const QVariant &var : rawData)
    {
        QVariantMap row = var.toMap();

        double temp = row.value("temperature").toDouble();
        double humi = row.value("humidity").toDouble();
        double weightBefore = row.value("weight_before").toDouble();
        double weightAfter  = row.value("weight_after").toDouble();
        double weightDelta  = row.value("weight_delta").toDouble();

        // 温湿度范围
        if(temp < tMin) tMin = temp;
        if(temp > tMax) tMax = temp;
        tSum += temp;

        if(humi < hMin) hMin = humi;
        if(humi > hMax) hMax = humi;
        hSum += humi;

        // 重量累加
        wSum  += weightDelta;
        wbSum += weightBefore;
        waSum += weightAfter;

        validCount++;
    }

    res["status"]      = "ok";
    res["count"]       = validCount;

    // 温度
    res["temp_min"]    = tMin;
    res["temp_max"]    = tMax;
    res["temp_avg"]    = tSum / validCount;

    // 湿度
    res["humi_min"]    = hMin;
    res["humi_max"]    = hMax;
    res["humi_avg"]    = hSum / validCount;

    // 重量
    res["weight_total"]        = wSum;    // 总实际取用量
    res["weight_before_total"] = wbSum;   // 总领用前重量
    res["weight_after_total"]  = waSum;   // 总领用后重量

    qDebug() << "数据分析完成:"
             << "条数" << validCount
             << "温度" << tMin << "~" << tMax
             << "湿度" << hMin << "~" << hMax
             << "总取用量" << wSum;

    emit sigAnalyseDone(res);
}

//  导出 CSV
void TaskWorker::slotExportCsv(QVariantList rawData, QString filePath)
{
    //   打开文件
    QFile file(filePath);
    if(!file.open(QIODevice::WriteOnly))
    {
        emit sigExportDone(false, "无法打开文件: " + filePath);
        return;
    }

    //   中文用 GBK（Windows Excel 兼容）
    auto toLocal = [](const QString &s) -> QByteArray {
        return s.toLocal8Bit();
    };

    //   CSV 转义（含逗号/引号/换行的字段）
    auto escapeCsv = [](const QString &s) -> QString
    {
        if(s.contains(',') || s.contains('"') || s.contains('\n'))
        {
            QString tmp = s;
            tmp.replace("\"", "\"\"");
            return "\"" + tmp + "\"";
        }
        return s;
    };

    //  表头
    file.write(toLocal(
        "设备,操作员,试剂,领用前(g),领用后(g),实际取用量(g),温度(°C),湿度(%),时间\n"));

    //   数据行
    for(const QVariant &var : rawData)
    {
        QVariantMap row = var.toMap();

        // 基本字段
        QString deviceId     = row.value("device_id").toString();
        QString operatorName = row.value("operator_name").toString();
        QString reagentName  = row.value("reagent_name").toString();

        //  名字兜底：空则用 uid
        if(operatorName.isEmpty())
            operatorName = row.value("operator_uid").toString();
        if(reagentName.isEmpty())
            reagentName = row.value("reagent_uid").toString();

        //  三个重量字段
        double weightBefore = row.value("weight_before").toDouble();
        double weightAfter  = row.value("weight_after").toDouble();
        double weightDelta  = row.value("weight_delta").toDouble();

        //  环境
        double temperature  = row.value("temperature").toDouble();
        double humidity     = row.value("humidity").toDouble();

        //  时间戳 → 可读字符串
        qint64 ts = row.value("timestamp").toLongLong();
        QString timeStr = ts > 0
            ? QDateTime::fromMSecsSinceEpoch(ts).toString("yyyy-MM-dd HH:mm:ss")
            : "";

        // 拼成一行 CSV
        QString line =
            escapeCsv(deviceId)     + "," +
            escapeCsv(operatorName) + "," +
            escapeCsv(reagentName)  + "," +
            QString::number(weightBefore, 'f', 1) + "," +
            QString::number(weightAfter,  'f', 1) + "," +
            QString::number(weightDelta,  'f', 1) + "," +
            QString::number(temperature,  'f', 1) + "," +
            QString::number(humidity,     'f', 1) + "," +
            timeStr + "\n";

        file.write(toLocal(line));
    }

    file.close();
    emit sigExportDone(true, "导出完成: " + filePath);
}

void TaskWorker::slotQueryHistory(QString startTime, QString endTime)
{
    Q_UNUSED(startTime)
    Q_UNUSED(endTime)

}

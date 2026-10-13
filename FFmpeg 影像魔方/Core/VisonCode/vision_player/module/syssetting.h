#ifndef _SYSSETTING_H_
#define _SYSSETTING_H_

#include <QFile>
#include "common.h"

class SysSetting
{
public:
    static SysSetting * GetInstance();
    static void DestoryInstance();

    int InitLogSys();
    int WriteLogToFile(QString strInfo);
private:
    SysSetting();
    ~SysSetting();

    int IsWriteLog();

    static SysSetting *m_pInstance;

    int m_iLogRowCount;
    int m_iLogFileCount;
    QFile m_file;
};

void LogMessageOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg);

#endif //_SYSSETTING_H_

#include <QDir>
#include <QDebug>
#include "syssetting.h"

SysSetting * SysSetting::m_pInstance = NULL;

SysSetting *SysSetting::GetInstance()
{
    //FUNCTION_ENTER;
    if (m_pInstance == NULL)
    {
        m_pInstance = new SysSetting();
    }
    //FUNCTION_EXIT;
    return m_pInstance;
}

void SysSetting::DestoryInstance()
{
    FUNCTION_ENTER;
    if (m_pInstance != NULL)
    {
        delete m_pInstance;
    }
    m_pInstance = NULL;
    FUNCTION_EXIT;
}

int SysSetting::InitLogSys()
{
    //FUNCTION_ENTER;
    if (IsWriteLog() != LOG_OK)
    {
        return LOG_ERR;
    }

    QString strDir = USER_DATA_PATH + "/tmp/log";
    QDir dir(strDir);
    if (!dir.exists())
    {
        if (!dir.mkdir(strDir))
        {
            return LOG_ERR;
        }
    }
    QString strFile = USER_DATA_PATH + "/tmp/log/vplay_log_" + QString::number(m_iLogFileCount)+".txt";
    m_file.setFileName(strFile);
    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Truncate))
    {
        return LOG_ERR;
    }
    //FUNCTION_EXIT;
    return LOG_OK;
}

SysSetting::SysSetting()
{
    FUNCTION_ENTER;
    m_iLogRowCount = 0;
    m_iLogFileCount = 0;
    FUNCTION_EXIT;
}

SysSetting::~SysSetting()
{
    FUNCTION_ENTER;
    m_iLogRowCount = 0;
    m_iLogFileCount = 0;
    FUNCTION_EXIT;
}

int SysSetting::IsWriteLog()
{
    //FUNCTION_ENTER;
    QString strFile = USER_DATA_PATH + "/tmp/config.ini";
    QFile file(strFile);
    if (!file.open(QIODevice::ReadOnly))
    {
        //qDebug() << "config.ini open error";
        return LOG_ERR;
    }
    char buf[512];
    memset(buf, 0, 512);
    qint64 len = file.readLine(buf, 512);
    if (len > 0)
    {
        QString str(buf);
        str.remove('\n');
        str.remove('\r');
        QStringList list = str.split("=");
        if (list.length()!= 2)
        {
            return LOG_ERR;
        }
        if (list.at(0) != "debug" ||  list.at(1) != "1")
        {
            return LOG_ERR;
        }
    }

    qInstallMessageHandler(LogMessageOutput);
    //FUNCTION_EXIT;
    return LOG_OK;
}

int SysSetting::WriteLogToFile(QString strInfo)
{
    if (strInfo.length() <= 0)
    {
        return LOG_ERR;
    }
    if (strInfo.at(strInfo.length()-1) != '\n')
    {
        strInfo.append('\n');
    }

    if (m_iLogRowCount == LOG_FILE_ROW_MAX)
    {
        m_file.close();
        m_iLogFileCount++;
        if (m_iLogFileCount == LOG_FILE_MAX)
        {
           m_iLogFileCount = 0;
        }

        QString strFile = USER_DATA_PATH + "/tmp/log/vplay_log_" + QString::number(m_iLogFileCount)+".txt";
        m_file.setFileName(strFile);
        if (!m_file.open(QIODevice::Append | QIODevice::Truncate))
        {
            return LOG_ERR;
        }
        m_iLogRowCount = 0;
    }
    if (m_file.isOpen())
    {
        if (m_file.write(strInfo.toStdString().data()) < 0)
        {
            return LOG_ERR;
        }
        m_iLogRowCount++;
        m_file.flush();
        return LOG_OK;
    }
    return LOG_ERR;
}

void LogMessageOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    Q_UNUSED(type);
    Q_UNUSED(context);
    //SysSetting *pLog = SysSetting::GetInstance();
    //pLog->WriteLogToFile(msg);
}

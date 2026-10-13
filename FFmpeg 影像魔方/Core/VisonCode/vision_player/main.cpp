#include "widget.h"
#include "syssetting.h"

#include <QApplication>
#include <QDir>


int main(int argc, char *argv[])
{
    //qInstallMessageHandler(LogMessageOutput);
    QApplication a(argc, argv);
    QString strDir = USER_DATA_PATH;
    QDir dir(strDir);
    if (!dir.exists())
    {
        if (!dir.mkdir(strDir))
        {
            return -1;
        }
    }
    //SysSetting::GetInstance()->InitLogSys();

    if (STAVControl::GetInstance() == NULL)
    {
        return -1;
    }
    if(STAvNet::GetInstance() == NULL)
    {
        return -1;
    }

    STAVControl::GetInstance()->InitControl();
    STAvNet::GetInstance()->NetInit();

    Widget w;
    w.InitUI();
    w.show();
    return a.exec();
}

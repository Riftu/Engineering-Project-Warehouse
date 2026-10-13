#include <QDebug>
#include <QJsonValue>
#include <QJsonArray>
#include <QFile>
#include <QFileInfo>
#include <QStringList>
#include <QProcess>
#include <QDir>
#include <QNetworkReply>
#include "sidebar.h"
#include "av_control.h"
#include "ui_sidebar.h"
#include "common.h"

Sidebar::Sidebar(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Sidebar)
{
    ui->setupUi(this);
    m_AccessManager = NULL;
    m_pCurrentDownLoadItem = NULL;
    ui->treeWidget->setHeaderHidden(true);
    ui->treeWidget->setColumnCount(3);
    ui->treeWidget->setColumnWidth(0, 200);
    ui->treeWidget->setColumnWidth(1, 50);
    ui->treeWidget->setColumnWidth(2, 50);
    ui->treeWidget->setSelectionBehavior(QTreeWidget::SelectRows);
    ui->vlist->setText(SIDEBAR_PLAY_LIST);

    ui->sidebarhide->setProperty("status", "show");
    connect(ui->treeWidget,SIGNAL(itemDoubleClicked(QTreeWidgetItem*,int)),this,SLOT(OnItemDoubleClicked(QTreeWidgetItem *,int)));

    m_AccessManager = new QNetworkAccessManager(this);
    m_pReplay = NULL;
}

Sidebar::~Sidebar()
{
    delete ui;
}


void Sidebar::ShowAvList(QJsonObject &obj)
{
    FUNCTION_ENTER;
    if (obj.contains("videolist"))
    {
        QJsonArray arr = obj.value("videolist").toArray();
        qDebug() << arr.count();

        int i = 0;
        for (i = 0; i < arr.count(); i++)
        {
            QJsonObject val = arr.at(i).toObject();
            QString strResType = val.value("res_type").toString();
            QTreeWidgetItem *topItem = new QTreeWidgetItem();
            topItem->setText(0,strResType);

            ui->treeWidget->addTopLevelItem(topItem);
            QJsonArray arrFile = val.value("res_file").toArray();

            if (arrFile.count() <= 0)
            {
                continue;
            }
            int j = 0;
            for (j = 0; j < arrFile.count(); j++)
            {
                QJsonObject file = arrFile.at(j).toObject();
                QString strName = file.value("res_note").toString();

                AvFileInfo avinfo;
                avinfo.m_strUrl = file.value("res_url").toString();
                avinfo.m_iVDurationTs = file.value("v_duration_ts").toInt();
                avinfo.m_iVFrames = file.value("v_frames").toInt();
                avinfo.m_iADurationTs = file.value("a_duration_ts").toInt();
                avinfo.m_iAFrames = file.value("a_frames").toInt();
                //file exist?
                QStringList strList = avinfo.m_strUrl.split("/");
                if (strList.length() == 0)
                {
                 continue;
                }
                QTreeWidgetItem *item = new QTreeWidgetItem(topItem);
                item->setText(0,strName);
                QVariant v=QVariant::fromValue(avinfo);
                item->setData(0,Qt::UserRole, v);
                //qDebug() << strList.length();
                if (!CheckFileExist(strList.at(strList.length()-1)))
                {
                    //file isn't exist,display download button
                    QPushButton *pDownBtn = new QPushButton(AV_FILE_NOT_DOWNLOAD);
                    pDownBtn->setStyleSheet("QPushButton{color:red;background-color:transparent;font-weight: bold;}");
                    ui->treeWidget->setItemWidget(item, 2, pDownBtn);
                    pDownBtn->hide();
                    connect(pDownBtn,SIGNAL(clicked()),this,SLOT(OnDownFile()));
                }
                else
                {
                    QPushButton *pDownBtn = new QPushButton(AV_FILE_DOWNLOAD);
                    pDownBtn->setStyleSheet("QPushButton{color:green;background-color:transparent;font-weight: bold; }");
                    ui->treeWidget->setItemWidget(item, 2, pDownBtn);
                    pDownBtn->hide();
                }
            }
        }
    }
    FUNCTION_EXIT;
}

AvFileInfo Sidebar::GetDispalyItem(bool &bNext, bool &bPre)
{
    FUNCTION_ENTER;
    AvFileInfo info = GetItemPlayFile();
    GetItemNextAndPre(bNext, bPre);
    FUNCTION_EXIT;
    return info;
}

AvFileInfo Sidebar::GetDisplayNextItem(bool &bNext, bool &bPre)
{
    FUNCTION_ENTER;
    QString str = "";
    AvFileInfo info;
    QTreeWidgetItem *item = ui->treeWidget->currentItem();
    if (NULL == item)
    {
        return info;
    }
    if (NULL == item->parent())
    {
        return info;
    }
    int index = ui->treeWidget->currentIndex().row();
    int count = item->parent()->childCount();
    if (index >= count-1)
    {
        return info;
    }
    index++;
    qDebug() << index;
    QTreeWidgetItem *pItem = item->parent()->child(index);
    if (NULL == pItem)
    {
        return info;
    }
    ui->treeWidget->setCurrentItem(pItem);
    info = GetItemPlayFile();
    GetItemNextAndPre(bNext, bPre);
    FUNCTION_EXIT;
    return info;
}

AvFileInfo Sidebar::GetDisplayPreItem(bool &bNext, bool &bPre)
{
    FUNCTION_ENTER;
    QString str = "";
    AvFileInfo info;
    QTreeWidgetItem *item = ui->treeWidget->currentItem();
    if (NULL == item)
    {
        return info;
    }
    if (NULL == item->parent())
    {
        return info;
    }
    int index = ui->treeWidget->currentIndex().row();

    if (index == 0)
    {
        return info;
    }
    index--;
    qDebug() << index;
    QTreeWidgetItem *pItem = item->parent()->child(index);
    if (NULL == pItem)
    {
        return info;
    }
    ui->treeWidget->setCurrentItem(pItem);
    info = GetItemPlayFile();
    GetItemNextAndPre(bNext, bPre);
    FUNCTION_EXIT;
    return info;
}

void Sidebar::ClearDisplayList()
{
    FUNCTION_ENTER;
    int iTopCount = ui->treeWidget->topLevelItemCount();
    int i = 0;
    for(i = 0; i < iTopCount; i++)
    {
        QTreeWidgetItem *pTop = ui->treeWidget->takeTopLevelItem(0);
        if (NULL == pTop) continue;
        int iChild = pTop->childCount();
        int j = 0;
        for(j = 0; j < iChild; j++)
        {
             QTreeWidgetItem *item = pTop->takeChild(0);
             if (NULL != item)
             {
                 delete item;
             }
        }
        delete pTop;
    }
    if (m_pReplay != NULL)
    {
        disconnect(m_pReplay, 0, 0, 0);
        m_pReplay->abort();
        m_pReplay->deleteLater();
        m_pReplay = NULL;
        m_pCurrentDownLoadItem = NULL;
    }
    FUNCTION_EXIT;
}

void Sidebar::ShowSidebar(bool bShow)
{
    FUNCTION_ENTER;
    if (bShow)
    {
        ui->sidebarhide->setProperty("status", "show");
        ui->sidebarhide->style()->polish(ui->sidebarhide);
    }
    else
    {
        ui->sidebarhide->setProperty("status", "hide");
        ui->sidebarhide->style()->polish(ui->sidebarhide);
    }
    FUNCTION_EXIT;
}

void Sidebar::OnDownFile()
{
    FUNCTION_ENTER;
    if (NULL != m_pCurrentDownLoadItem || NULL != m_pReplay)
    {
        //show messagebox, downloading file, wait
        return ;
    }
    QPushButton* button = qobject_cast<QPushButton*>(sender());
    m_pCurrentDownLoadItem = ui->treeWidget->itemAt(button->pos());
    if (NULL == m_pCurrentDownLoadItem)
    {
        return;
    }
    QVariant v = m_pCurrentDownLoadItem->data(0,Qt::UserRole);
    AvFileInfo info = v.value<AvFileInfo>();
    QString strDownUrl = info.m_strUrl;
    qDebug() << strDownUrl;
    QString strUrl = QString(USER_DOMAIN) + strDownUrl;
    qDebug() << strUrl;
    QUrl url(strUrl);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    m_pReplay = m_AccessManager->get(request);
    if (NULL != m_pReplay)
    {
        connect(m_pReplay, SIGNAL(downloadProgress(qint64,qint64)), this, SLOT(OnDownloadProgress(qint64, qint64)));
        connect(m_pReplay, SIGNAL(finished()), this, SLOT(OnDownFileFinish()));
    }
    else
    {
        m_pCurrentDownLoadItem = NULL;
    }

    FUNCTION_EXIT;
}

void Sidebar::OnDownloadProgress(qint64 recv, qint64 total)
{
    FUNCTION_ENTER;
    qDebug() << recv << " " << total;
    int percentage = static_cast<int>((recv * 100) / total);
    m_pCurrentDownLoadItem->setText(1,QString::number(percentage) + "%");
    FUNCTION_EXIT;
}

void Sidebar::OnDownFileFinish()
{
    FUNCTION_ENTER;
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (NULL == reply)
    {
        m_pCurrentDownLoadItem = NULL;
        if (NULL != m_pReplay)
        {
            disconnect(m_pReplay, 0, 0, 0);
            m_pReplay->abort();
            m_pReplay->deleteLater();
            m_pReplay = NULL;
        }
        return;
    }
    if (reply != m_pReplay)
    {
        reply->deleteLater();
        reply = NULL;
        if (NULL != m_pReplay)
        {
            disconnect(m_pReplay, 0, 0, 0);
            m_pReplay->abort();
            m_pReplay->deleteLater();
            m_pReplay = NULL;
        }
        m_pCurrentDownLoadItem = NULL;
        return ;
    }
    QVariant v = m_pCurrentDownLoadItem->data(0,Qt::UserRole);
    AvFileInfo info = v.value<AvFileInfo>();
    QString strDownUrl = info.m_strUrl;
    QStringList strList = strDownUrl.split("/");
    if (strList.length() == 0)
    {
        m_pCurrentDownLoadItem = NULL;
        m_pReplay->deleteLater();
        return;
    }
    QString strDir = USER_DATA_PATH+ "/data";
    QDir dir(strDir);
    if (!dir.exists())
    {
        if (!dir.mkdir(strDir))
        {
            m_pCurrentDownLoadItem = NULL;
            disconnect(m_pReplay, 0, 0, 0);
            m_pReplay->abort();
            m_pReplay->deleteLater();
            m_pReplay = NULL;
            return;
        }
    }
    QString strPath = USER_DATA_PATH+ "/data/" + strList.at(strList.length()-1);
    QByteArray data = m_pReplay->readAll();
    QFile file(strPath);
    if (file.open(QIODevice::WriteOnly))
    {
        file.write(data);
        file.close();
        qDebug() << "data write to file ok";
        //change button style
        QPushButton *pBt = qobject_cast<QPushButton*>(ui->treeWidget->itemWidget(m_pCurrentDownLoadItem, 2));
        if (pBt != NULL)
        {
            pBt->setStyleSheet("QPushButton{color:green;background-color:transparent;font-weight: bold; }");
            pBt->setText(AV_FILE_DOWNLOAD);
            disconnect(pBt,SIGNAL(clicked()),this,SLOT(OnDownFile()));
        }
        m_pCurrentDownLoadItem->setText(1,"");
    }
    m_pCurrentDownLoadItem = NULL;
    disconnect(m_pReplay, 0, 0, 0);
    m_pReplay->abort();
    m_pReplay->deleteLater();
    m_pReplay = NULL;
    FUNCTION_EXIT;
}

void Sidebar::OnItemDoubleClicked(QTreeWidgetItem *item, int column)
{
    FUNCTION_ENTER;
    if (column != 0)
    {
        return;
    }
    QVariant v = item->data(0,Qt::UserRole);
    AvFileInfo info = v.value<AvFileInfo>();
    QString strDownUrl = info.m_strUrl;
    QStringList strList = strDownUrl.split("/");
    if (strList.length() == 0)
    {
        return;
    }
    if (CheckFileExist(strList.at(strList.length()-1)))
    {
        //play file
        QString strPath = USER_DATA_PATH+ "/data/" + strList.at(strList.length()-1);
        //STAVControl::GetInstance()->OpenAVFile(strPath);
        QVariant ret;
        ret.setValue(info);
        emit SideBarSignal(SIDEBAR_DOUBLE_ITEM, ret);
    }
    FUNCTION_EXIT;
}

bool Sidebar::CheckFileExist(QString strFile)
{
    FUNCTION_ENTER;
    QString strPath = USER_DATA_PATH+ "/data/" + strFile;
    QFileInfo file(strPath);
    if (file.exists())
    {
        FUNCTION_EXIT;
        return true;
    }
    FUNCTION_EXIT;
    return false;

}

AvFileInfo Sidebar::GetItemPlayFile()
{
    FUNCTION_ENTER;
    QString str = "";
    AvFileInfo info;
    QTreeWidgetItem *item = ui->treeWidget->currentItem();
    if (NULL == item)
    {
        return info;
    }
    QVariant v= item->data(0,Qt::UserRole);
    info = v.value<AvFileInfo>();
    QString strDownUrl = info.m_strUrl;
    qDebug() << strDownUrl;
    if (strDownUrl.length() <= 0)
    {
        return info;
    }
    QStringList strList = strDownUrl.split("/");
    if (strList.length() == 0)
    {
        return info;
    }
    if (!CheckFileExist(strList.at(strList.length()-1)))
    {
        return info;
    }
    info.m_strUrl = USER_DATA_PATH+ "/data/" + strList.at(strList.length()-1);
    FUNCTION_EXIT;
    return info;
}

bool Sidebar::GetItemNextAndPre(bool &bNext, bool &bPre)
{
    FUNCTION_ENTER;
    bNext = false;
    bPre = false;
    QTreeWidgetItem *item = ui->treeWidget->currentItem();
    if (NULL == item)
    {
        return false;
    }
    if (NULL == item->parent())
    {
        return false;
    }
    int index = ui->treeWidget->currentIndex().row();
    int count = item->parent()->childCount();
    if (count == 1)
    {
        bNext = false;
        bPre = false;
        FUNCTION_EXIT;
        return true;
    }
    if (index == 0)
    {
        bPre = false;
        bNext = true;
    }
    else if (index == count-1)
    {
        bNext = false;
        bPre = true;
    }
    else
    {
        bPre = true;
        bNext = true;
    }
    FUNCTION_EXIT;
    return true;
}

void Sidebar::on_refreshbt_clicked()
{
    FUNCTION_ENTER;
    if (m_pReplay != NULL)
    {
        disconnect(m_pReplay, 0, 0, 0);
        m_pReplay->deleteLater();
        m_pReplay = NULL;
        m_pCurrentDownLoadItem = NULL;
    }
    QVariant ret;
    ret.setValue(true);
    SideBarSignal(SIDEBAR_REFRESH, ret);
    FUNCTION_EXIT;
}


void Sidebar::on_sidebarhide_clicked()
{
    FUNCTION_ENTER;
    QVariant ret;
    ret.setValue(true);
    SideBarSignal(SIDEBAR_SHOW_HIDE, ret);
    FUNCTION_EXIT;
}


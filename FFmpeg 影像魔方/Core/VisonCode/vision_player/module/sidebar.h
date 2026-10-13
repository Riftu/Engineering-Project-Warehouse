#ifndef SIDEBAR_H
#define SIDEBAR_H

#include <QObject>
#include <QVariant>
#include <QWidget>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTreeWidgetItem>
#include <QJsonObject>
#include <QStandardItemModel>
#include "common.h"
namespace Ui {
class Sidebar;
}

class Sidebar : public QWidget
{
    Q_OBJECT

public:
    explicit Sidebar(QWidget *parent = nullptr);
    ~Sidebar();

    void ShowAvList(QJsonObject &obj);
    AvFileInfo GetDispalyItem(bool &bNext, bool &bPre);
    AvFileInfo GetDisplayNextItem(bool &bNext, bool &bPre);
    AvFileInfo GetDisplayPreItem(bool &bNext, bool &bPre);
    void ClearDisplayList();
    void ShowSidebar(bool bShow);

signals:
    void SideBarSignal(int iType, QVariant data);

private slots:
    void OnDownFile();
    void OnDownloadProgress(qint64 recv, qint64 total);
    void OnDownFileFinish();
    void OnItemDoubleClicked(QTreeWidgetItem *item,int column);
    void on_refreshbt_clicked();
    void on_sidebarhide_clicked();

private:
    bool CheckFileExist(QString strFile);
    AvFileInfo GetItemPlayFile();
    bool GetItemNextAndPre(bool &bNext, bool &bPre);

private:
    Ui::Sidebar *ui;
    QNetworkAccessManager *m_AccessManager;
    QTreeWidgetItem *m_pCurrentDownLoadItem;
    QNetworkReply  *m_pReplay;
};

#endif // SIDEBAR_H

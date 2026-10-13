#ifndef WIDGET_H
#define WIDGET_H

#include <QVariant>
#include <QWidget>
#include <QTimer>
#include <qpropertyanimation.h>
#include "common.h"
#include "widgetbase.h"
#include "av_control.h"
#include "av_net.h"
#include "userlogin.h"
#include "syssetting.h"
#include <QElapsedTimer>
QT_BEGIN_NAMESPACE
namespace Ui { class Widget; }
QT_END_NAMESPACE

class Widget : public VWidgetBase
{
    Q_OBJECT

public:
    Widget(QWidget *parent = nullptr);
    ~Widget();

    void InitUI();
    void LoadStyle();

private slots:
    void OnStartPlay(int iType,quint64 playtime,quint32 index,int avType);
    void OnTimeOut();
    void OnTimeOutAutoLogin();
    void OnAvNetResult(int iType, QVariant data);
    void OnControlBarSignal(int iType, QVariant data);
    void OnSidebarBarSignal(int iType, QVariant data);
    void OnTitleBarSignal(int iType, QVariant data);
    void OnTimeOutCheckProcessList();
    void OnLogoutSignal();

protected:
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void WindowLayout();
    void OnShowSetUp();
    void OnShowSidebar();
    void OnDisplayAVFile(AvFileInfo info);
    void ChangeUserLabelPos();

private:
    Ui::Widget *ui;
    STAVControl *m_pAVCtl;
    QTimer *m_pTimer;
    QTimer *m_pTimerInit;  //auto login, get avlist
    QTimer *m_ProcessList; //check capture desktop process

    QPropertyAnimation* m_paControlBar;
    QPropertyAnimation* m_paSidebar;
    QPropertyAnimation* m_paVideoLabel;
    UserLogin *m_pUserLogin;
    //SysSetting *m_pSetting;

    bool m_bSidebarShow;
    QElapsedTimer m_timer;//从开始计时到停止所经过的毫秒数
    quint64 m_timeval;//时间累加值 每播放一帧图片就进行累加
    QList<quint64> m_playtimelist;

};
#endif // WIDGET_H

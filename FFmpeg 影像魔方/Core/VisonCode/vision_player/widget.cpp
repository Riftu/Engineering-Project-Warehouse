#include <QFile>
#include <QDebug>
#include <QTextStream>
#include <QVariant>
#include <QJsonDocument>

#include <QJsonObject>
#include <QJsonArray>
#include <QProcess>
#include <QMessageBox>
#include <QDir>
#include <QTextCodec>
#include <QMouseEvent>

#include "widget.h"
#include "ui_widget.h"

Widget::Widget(QWidget *parent)
    : VWidgetBase(parent)
    , ui(new Ui::Widget)
{
    ui->setupUi(this);

    m_pAVCtl = NULL;
    m_pTimer = NULL;
    m_pTimerInit = NULL;
    m_ProcessList = NULL;
    m_paVideoLabel = NULL;

    m_paSidebar = NULL;
    m_paControlBar = NULL;
    m_pUserLogin = NULL;
    //m_pSetting = NULL;
    m_bSidebarShow = true;
}

Widget::~Widget()
{
    m_pAVCtl->StopPlay();
    //QTest::qSleep(100);
    QThread::msleep(100);
    m_pTimer->stop();
    m_pAVCtl->ResetAVControl();
    m_pAVCtl->DestroyINstance();
    STAvNet::DestroyINstance();

    m_pTimerInit->stop();
    m_ProcessList->stop();

    delete ui;
}

void Widget::InitUI()
{
    FUNCTION_ENTER;                  // qt版本>=5.3可设置背景透明
    QString userDir = QDir::homePath();
    qDebug() << userDir;
    LoadStyle();
    this->setTitleBar(ui->titlebar->getBackground());
    connect(this, &VWidgetBase::windowStateChanged, ui->titlebar, &TitleBar::on_windowStateChanged);
    if (NULL == m_pAVCtl)
    {
        m_pAVCtl = STAVControl::GetInstance();
    }

    if (m_pAVCtl->InitControl() != CTL_OK)
    {
        qDebug() << "init error\r\n";
        return;
    }
    STAvNet * ptmp = STAvNet::GetInstance();
    connect(ptmp, SIGNAL(AVNetResult(int, QVariant)), this, SLOT(OnAvNetResult(int, QVariant)), Qt::QueuedConnection);
    connect(ui->controlBar, SIGNAL(ControlBarSignal(int, QVariant)), this, SLOT(OnControlBarSignal(int, QVariant)), Qt::QueuedConnection);
    connect(ui->sidebar, SIGNAL(SideBarSignal(int, QVariant)), this, SLOT(OnSidebarBarSignal(int, QVariant)), Qt::QueuedConnection);
    connect(ui->titlebar, SIGNAL(TitleBarSignal(int, QVariant)), this, SLOT(OnTitleBarSignal(int, QVariant)), Qt::QueuedConnection);

    connect(m_pAVCtl, SIGNAL(StartPlay(int,quint64,quint32,int)), this, SLOT(OnStartPlay(int,quint64,quint32,int)), Qt::QueuedConnection);
    m_pTimer = new QTimer(this);
    connect(m_pTimer, SIGNAL(timeout()), this, SLOT(OnTimeOut()), Qt::QueuedConnection);

    m_paVideoLabel = new QPropertyAnimation(ui->video_label, "size");
    m_paControlBar = new QPropertyAnimation(ui->controlBar, "pos");
    m_paSidebar = new QPropertyAnimation(ui->sidebar, "pos");

    m_paControlBar->setDuration(500);
    m_paSidebar->setDuration(500);
    m_paVideoLabel->setDuration(500);

    m_bSidebarShow = true;

    ui->video_label->installEventFilter(this);

    m_pTimerInit = new QTimer(this);
    connect(m_pTimerInit, SIGNAL(timeout()), this, SLOT(OnTimeOutAutoLogin()));
    m_pTimerInit->start(1000);

    m_ProcessList = new QTimer(this);
    connect(m_ProcessList, SIGNAL(timeout()), this, SLOT(OnTimeOutCheckProcessList()));
    m_ProcessList->start(2000);

    m_pUserLogin = new UserLogin(this);
    connect(m_pUserLogin, SIGNAL(LogoutSignal()), this, SLOT(OnLogoutSignal()));

    //m_pSetting = new SysSetting(this);
    //ui->video_label->setProperty("bg", "init");

    QImage img(":/Style/image/st_bg.png");
    QPixmap pixmap = QPixmap::fromImage(img);
    ui->video_label->setPixmap(pixmap);
    ui->userlabel->setText(USER_NOLOGIN);
    FUNCTION_EXIT;
}

void Widget::LoadStyle()
{
    FUNCTION_ENTER;
    QFile file(":/Style/main.css");
    if (file.open(QFile::ReadOnly))
    {
        QStringList list;
        QTextStream in(&file);
        while (!in.atEnd()) {
            QString line;
            in >> line;
            list << line;
        }

        file.close();
        QString qss = list.join("\n");
        this->setStyleSheet("");
        qApp->setStyleSheet(qss);
    }
    FUNCTION_EXIT;
}

void Widget::resizeEvent(QResizeEvent *event)
{
    Q_UNUSED(event);
    WindowLayout();
}

void Widget::WindowLayout()
{
    int b_width = ui->body->width();
    int b_height = ui->body->height();
    //qDebug() << "width " << b_width << " height " << b_height;

    QSize c_size = ui->controlBar->size();
    c_size.setWidth(b_width );
    ui->controlBar->resize(c_size);
    int y = b_height - ui->controlBar->height();
    ui->controlBar->move(0, y);

    if (m_bSidebarShow == true)
    {
        QSize s_size = ui->sidebar->size();
        s_size.setHeight(b_height- ui->controlBar->height());
        ui->sidebar->resize(s_size);
        int s_x = b_width - ui->sidebar->width();
        ui->sidebar->move(s_x, 0);

        QSize v_size = ui->video_label->size();
        v_size.setWidth(b_width - ui->sidebar->width());
        v_size.setHeight(b_height - ui->controlBar->height());
        ui->video_label->resize(v_size);
    }
    else
    {
        QSize s_size = ui->sidebar->size();
        s_size.setHeight(b_height- ui->controlBar->height());
        ui->sidebar->resize(s_size);
        int s_x = b_width - 20;
        ui->sidebar->move(s_x, 0);

        QSize v_size = ui->video_label->size();
        v_size.setWidth(b_width - 20);
        v_size.setHeight(b_height - ui->controlBar->height());
        ui->video_label->resize(v_size);
    }

}
                                                       //索引
void Widget::OnStartPlay(int iType,quint64 playtime,quint32 index,int avType)
{
    FUNCTION_ENTER;
    qDebug() << "start play signal";
    if(avType == AV_S_PAUSE)
    {
        m_pTimer->stop();
        m_timer.invalidate();//使之无效
        return;

    }
    if(avType == AV_S_CONTINUE)
    {
        m_timer.restart();
        m_pTimer->start(1);//取下一帧时间
        return;
    }
    //刚启动
    if(index == 0 && playtime == 0)
    {
        m_timer.restart();
        m_timeval = m_timer.elapsed();//时间开始流逝,记录经过了多少毫秒，作为外部时间基准
    }
    //视频类型
    if(iType==AV_TYPE_VIDEO)
    {
        qDebug()<< "AV_TYPE_VIDEO playtime"<<playtime<<" "<<m_timeval <<"index"<<index;
        if(index == 0)//第一帧
        {             //播放时间,假设50ms   读到第一帧时间,假设3ms
            int len = playtime - m_timer.elapsed();//播放时间减去经过的时间
            if(len>0){
                m_pTimer->start(len);//设置超时
                m_playtimelist.clear();//第一张图片的播放时间不入队？
            }
            else {
               m_pTimer->start(1);
            }
        }
        else {
            m_playtimelist.push_back(playtime);
        }
    }
    else {
        qDebug()<< "AV_TYPE_AUDIO playtime"<<playtime<<" "<<m_timeval <<"index"<<index;

    }
    /*
    AVData *tmp = m_pAVCtl->GetData();
    m_pTimer->stop();
    if (NULL == tmp)
    {
        return;
    }

    unsigned int len = tmp->m_iNextTime - tmp->m_iPlayTime;
    if (len == 0)
    {
        len = 1;
    }

    m_pTimer->start(len);
    if (tmp->m_iType == AV_TYPE_VIDEO)
    {
        QPixmap pixmap = QPixmap::fromImage(*tmp->m_image);
        ui->video_label->setPixmap(pixmap);
        ui->video_label->setScaledContents(true);
    }
    */
    FUNCTION_EXIT;
}

void Widget::OnTimeOut()
{
    FUNCTION_ENTER;
    //保证widget队列中，始终保存avcontrol队头的下一帧时间
    //如果没收到数据，等100毫秒
    if(m_playtimelist.length()==0)
    {
        m_pTimer->start(100);
        return;
    }

    AVData *tmp = m_pAVCtl->GetData();//显示图片
    m_pTimer->stop();
    if (NULL == tmp)
    {
        qDebug() << "get data is null";
        m_pTimer->start(100);
        return;
    }
    int playtime =0;
    int len = 0;
    //计算下一帧播放时间
    if(m_playtimelist.length()>0)
    {
        playtime = m_playtimelist.front();
        int elapsed  = m_timer.elapsed();
        qDebug()<<"base time"<<m_timeval << " offset" << elapsed;
        int offset = 0;

        m_timeval += elapsed;
        m_timer.restart();
        offset = tmp->m_iPlayTime-m_timeval;
        len = playtime - tmp->m_iPlayTime+offset;
        if(qAbs(len)>500){
            m_timeval = playtime;//更新时间基准
            len = 0;
        }
        m_playtimelist.pop_front();
    }
    if(len <=0)
    {
        m_pTimer->stop();
        m_pTimer->start(1);
    }

    //下一帧播放时间的超时
     m_pTimer->start(len);
     //显示当前帧图片
     if (tmp->m_iType == AV_TYPE_VIDEO)
     {

         QPixmap pixmap = QPixmap::fromImage(*tmp->m_image);
         ui->video_label->setPixmap(pixmap);
         ui->video_label->setScaledContents(true);
          ui->controlBar->SetPlayTime(tmp->m_iPlayTime);
     }
    /*
    unsigned int len = tmp->m_iNextTime - tmp->m_iPlayTime;
    qDebug() << "time len : " << len;
    if (len == 0)
    {
        len = 1;
    }

    if (tmp->m_iType == AV_TYPE_VIDEO)
    {
        m_pTimer->start(40);
        QPixmap pixmap = QPixmap::fromImage(*tmp->m_image);
        ui->video_label->setPixmap(pixmap);
        ui->video_label->setScaledContents(true);
    }
    ui->controlBar->SetPlayTime(tmp->m_iPlayTime);
    */
    FUNCTION_EXIT;
}

void Widget::OnShowSetUp()
{
    FUNCTION_ENTER;

    //m_pSetting->show();
    FUNCTION_EXIT;
}

void Widget::OnTimeOutAutoLogin()
{
    FUNCTION_ENTER;
    m_pTimerInit->stop();

    bool bRet = STAvNet::GetInstance()->AutoLogin();
    if (bRet)
    {
        this->m_pUserLogin->SetLoginType(UserLogin::LOGIN_AUTO, "");
    }
    FUNCTION_EXIT;
}

void Widget::OnAvNetResult(int iType, QVariant data)
{
    FUNCTION_ENTER;
    if (iType == AV_NET_LOGIN_SUCCESS)
    {
        //get av list
        AVLoginStatus st = data.value<AVLoginStatus>();
        this->m_pUserLogin->SetLoginType(UserLogin::LOGIN_OK, st.m_strUser);
        ui->titlebar->SetLoginUser(st.m_strUser);
        ui->userlabel->setText(st.m_strUser);
        STAvNet::GetInstance()->GetAvList();
    }
    else if (iType == AV_NET_LOGIN_ERROR)
    {
        //login failed, show message box
        this->m_pUserLogin->SetLoginType(UserLogin::LOGIN_NO, "");
        ui->titlebar->SetLoginUser(USER_NOLOGIN);
        ui->userlabel->setText(USER_NOLOGIN);
        QMessageBox box(QMessageBox::Warning, USER_LOGIN_ERROR_TITLE, USER_LOGIN_ERROR, QMessageBox::Ok, this);
        box.setStyleSheet("QLabel{font-size: 16px;color: black;}"
                          "QPushButton{font-size: 16px;color: black;}");
        box.exec();
    }
    else if (AV_NET_PWDRESET == iType)
    {
        bool bRet = data.toBool();
        if (bRet == true)
        {
            //relogin
            OnLogoutSignal();
        }
        else
        {
            QMessageBox box(QMessageBox::Warning, USER_LOGIN_NAME_RESETPWD_ERROR_TITLE, USER_LOGIN_NAME_RESETPWD_ERROR, QMessageBox::Ok, this);
            box.setStyleSheet("QLabel{font-size: 16px;color: black;}"
                              "QPushButton{font-size: 16px;color: black;}");
            box.exec();
        }
    }
    else if (AV_NET_AVLIST == iType)
    {
        //invoke sidebar function , add av list to ui
        QJsonParseError jsonError;
        QJsonObject obj;
        QJsonDocument doc;
        QByteArray arr = data.toByteArray();
        doc = QJsonDocument::fromJson(arr, &jsonError);

        if( jsonError.error != QJsonParseError::NoError || !doc.isObject() )
        {
            qDebug("avlist json error");
            return;
        }
        obj = doc.object();
        if (obj.contains("status"))
        {
            int status = obj.value("status").toInt();
            if (status != 1)
            {
                qDebug("avlist status error");
                return;
            }
            ui->sidebar->ShowAvList(obj);
        }
    }
    FUNCTION_EXIT;
}

void Widget::OnShowSidebar()
{
    FUNCTION_ENTER;
    int b_width = ui->body->width();

    int s_x = ui->sidebar->x();
    qDebug() << s_x << " " << b_width;
    if (b_width-s_x > 50)
    {
        //hide
        m_paSidebar->setStartValue(QPoint(b_width-ui->sidebar->width(), 0));
        m_paSidebar->setEndValue(QPoint(b_width-20, 0));
        m_paSidebar->setEasingCurve(QEasingCurve::OutQuad);
        m_paSidebar->start();
        m_bSidebarShow = false;

        m_paVideoLabel->setStartValue(ui->video_label->size());
        m_paVideoLabel->setEndValue(QSize(b_width - 20, ui->video_label->height()));
        m_paVideoLabel->setEasingCurve(QEasingCurve::OutQuad);
        m_paVideoLabel->start();
        ui->sidebar->ShowSidebar(false);
    }
    else
    {
        //show
        m_paSidebar->setStartValue(QPoint(b_width-20, 0));
        m_paSidebar->setEndValue(QPoint(b_width-ui->sidebar->width(), 0));
        m_paSidebar->setEasingCurve(QEasingCurve::OutQuad);
        m_paSidebar->start();
        m_bSidebarShow = true;

        m_paVideoLabel->setStartValue(ui->video_label->size());
        m_paVideoLabel->setEndValue(QSize(b_width - ui->sidebar->width() , ui->video_label->height()));
        m_paVideoLabel->setEasingCurve(QEasingCurve::OutQuad);
        m_paVideoLabel->start();
        ui->sidebar->ShowSidebar(true);
    }
    FUNCTION_EXIT;
}

void Widget::OnDisplayAVFile(AvFileInfo info)
{
    FUNCTION_ENTER;
    m_pTimer->stop();
    //display default img
    QImage img(":/Style/image/st_bg.png");
    QPixmap pixmap = QPixmap::fromImage(img);
    ui->video_label->setPixmap(pixmap);
    ui->video_label->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    ui->video_label->setScaledContents(false);

    m_pAVCtl->StopPlay();
    //QTest::qSleep(100);
    QThread::msleep(100);
    m_pAVCtl->ResetAVControl();
    int iRet = m_pAVCtl->OpenAVFile(info);
    if (iRet != CTL_OK)
    {
        qDebug() << "open error\r\n";
        return;
    }
    ui->controlBar->SetPlayTotalTime(m_pAVCtl->GetTotalMs());
    FUNCTION_EXIT;
}

void Widget::ChangeUserLabelPos()
{
    int x = ui->userlabel->x();
    int y = ui->userlabel->y();
    int w = ui->video_label->width();
    int h = ui->video_label->height();

    static int dir = 1;  //1 右下， 2 左上， 3 左下，4 右上
    if (dir == 1)
    {
        //right
        if (x < w-125 && y < h - 30)
        {
            int step = qrand() % 10;
            ui->userlabel->move(x+10, y+ step);
        }
        else
        {
            dir = 2;
        }
    }
    if (dir == 2)
    {
        if (x > 50 && y > 30)
        {
            int step = qrand() % 10;
            ui->userlabel->move(x-step, y - 10);
        }
        else
        {
            dir = 3;
        }
    }
    if (dir == 3)
    {
        if (x > 50 && y < h - 30)
        {
            int step = qrand() % 10;
            ui->userlabel->move(x-step, y + 10);
        }
        else
        {
            dir = 4;
        }
    }
    if (dir == 4)
    {
        if (x < w-125 && y > 30)
        {
            int step = qrand() % 10;
            ui->userlabel->move(x+step, y - 10);
        }
        else
        {
            dir = 1;
        }
    }
    //qDebug() << x << " " << y << " " << w << " " << h << " " << dir;
}

void Widget::OnControlBarSignal(int iType, QVariant data)
{
    Q_UNUSED(data);
    FUNCTION_ENTER;
    if (iType == CONTROL_BAR_SHOW_SETUP)
    {
        OnShowSetUp();
    }
    else if (iType == CONTROL_BAR_PLAY)
    {
        bool bNext = false;
        bool bPre = false;
        AvFileInfo info = ui->sidebar->GetDispalyItem(bNext, bPre);
        if (info.m_strUrl.length() > 0)
        {
            OnDisplayAVFile(info);
        }
    }
    else if (iType == CONTROL_BAR_NEXT_PLAY)
    {
        bool bNext = false;
        bool bPre = false;
        AvFileInfo info = ui->sidebar->GetDisplayNextItem(bNext, bPre);
        if (info.m_strUrl.length() > 0)
        {
            OnDisplayAVFile(info);
        }
    }
    else if (iType == CONTROL_BAR_PRE_PLAY)
    {
        bool bNext = false;
        bool bPre = false;
        AvFileInfo info = ui->sidebar->GetDisplayPreItem(bNext, bPre);
        if (info.m_strUrl.length() > 0)
        {
            OnDisplayAVFile(info);
        }
    }
    else if (iType == CONTROL_BAR_PLAY_END)
    {
        m_pTimer->stop();
        //display default img
        QImage img(":/Style/image/st_bg.png");
        QPixmap pixmap = QPixmap::fromImage(img);
        ui->video_label->setPixmap(pixmap);
        ui->video_label->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
        ui->video_label->setScaledContents(false);

        m_pAVCtl->StopPlay();
        //QTest::qSleep(100);
        QThread::msleep(100);

        m_pAVCtl->ResetAVControl();
    }
    FUNCTION_EXIT;
}

void Widget::OnSidebarBarSignal(int iType, QVariant data)
{
    FUNCTION_ENTER;
    Q_UNUSED(data);
    if (iType == SIDEBAR_SHOW_HIDE)
    {
        OnShowSidebar();
    }
    else if (iType == SIDEBAR_DOUBLE_ITEM)
    {
        bool bNext = false;
        bool bPre = false;
        AvFileInfo info = ui->sidebar->GetDispalyItem(bNext, bPre);
        if (info.m_strUrl.length() > 0)
        {
            OnDisplayAVFile(info);
        }
    }
    else if (iType == SIDEBAR_REFRESH)
    {
        m_pAVCtl->StopPlay();
        //QTest::qSleep(100);
        QThread::msleep(100);
        m_pTimer->stop();
        m_pAVCtl->ResetAVControl();

        //display default img
        QImage img(":/Style/image/st_bg.png");
        QPixmap pixmap = QPixmap::fromImage(img);
        ui->video_label->setPixmap(pixmap);
        ui->video_label->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
        ui->video_label->setScaledContents(false);
        //clear play list
        ui->sidebar->ClearDisplayList();
        STAvNet::GetInstance()->GetAvList();
        //reset controlbar
        ui->controlBar->ControlBarReset();

    }
    FUNCTION_EXIT;
}

void Widget::OnTitleBarSignal(int iType, QVariant data)
{
    FUNCTION_ENTER;
    Q_UNUSED(data);
    if (iType == TITLE_BAR_SHOW_LOGIN)
    {
        m_pUserLogin->InitUi();
        m_pUserLogin->show();
    }
    FUNCTION_EXIT;
}

void Widget::OnTimeOutCheckProcessList()
{
    //FUNCTION_ENTER;
    QProcess process;
    QStringList capture = {"EVCapture.exe", "bdcam.exe", "screen_recorder.exe", "FSRecorder.exe"
                          ,"ApowerREC.exe", "QNApp.exe", "ScreenRecorder.exe", "屏录专家.exe"
                          ,"ScreenPlayback.exe", "YeCongRecorder.exe", "CamtasiaStudio.exe", "NNScreen.exe"
                          ,"Ludaka.exe", "CefViewWing.exe", "JiSuRecorder.exe", "ScreenRecord.exe"
                          ,"ScreenRecorderPlus.Client", "OBSRecorder.exe", "KeanRecord.exe", "RecorderPro.exe"
                          ,"JZRecord.exe", "HiRecMaster.exe", "recorder.exe"};
    QStringList processName;
    processName.clear();
    process.start("tasklist");
    process.waitForFinished();
    QTextCodec *pCode= QTextCodec::codecForLocale();
    QString output = pCode->toUnicode(process.readAllStandardOutput());
    QStringList processes = output.split("\n", QString::SkipEmptyParts);
    for (int i = 2; i < processes.size(); i++) {
        QString pName = processes[i].split(" ").first();
        //qDebug() << pName ;
        processName.append(pName);
    }
    for (int i = 0; i < capture.length(); i++)
    {
        int index = processName.indexOf(capture.at(i));
        if (index != -1)
        {
            m_pAVCtl->StopPlay();
            //QTest::qSleep(100);
            QThread::msleep(100);
            m_pTimer->stop();
            //QTest::qSleep(100);
            QThread::msleep(100);

            QImage img(":/Style/image/st_bg.png");
            QPixmap pixmap = QPixmap::fromImage(img);
            ui->video_label->setPixmap(pixmap);
            ui->video_label->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
            ui->video_label->setScaledContents(false);

            m_pAVCtl->ResetAVControl();
            STAvNet::GetInstance()->ResetStNet();

            qDebug() << processName.at(index);
            QMessageBox box(QMessageBox::Warning, CAPTURE_PROCESS_TITLE, CAPTURE_PROCESS, QMessageBox::Ok, this);
            box.setStyleSheet("QLabel{font-size: 16px;color: black;}"
                              "QPushButton{font-size: 16px;color: black;}");
            box.exec();
            this->close();
        }
    }
    //change user_label position
    ChangeUserLabelPos();
    //FUNCTION_EXIT;
}

void Widget::OnLogoutSignal()
{
    FUNCTION_ENTER;
    m_pAVCtl->StopPlay();
    //QTest::qSleep(100);
    QThread::msleep(100);
    m_pTimer->stop();
    //QTest::qSleep(100);
    QThread::msleep(100);
    //display default img
    //ui->video_label->setProperty("bg", "init");
    //ui->video_label->style()->polish(ui->video_label);
    QImage img(":/Style/image/st_bg.png");
    QPixmap pixmap = QPixmap::fromImage(img);
    ui->video_label->setPixmap(pixmap);
    ui->video_label->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    ui->video_label->setScaledContents(false);

    m_pAVCtl->ResetAVControl();
    STAvNet::GetInstance()->ResetStNet();
    //delete auto config file
    STAvNet::GetInstance()->RemoveAutoLoginFile();
    m_pUserLogin->SetLoginType(UserLogin::LOGIN_NO, "");
    ui->titlebar->SetLoginUser(USER_NOLOGIN);
    ui->userlabel->setText(USER_NOLOGIN);
    //clear play list
    ui->sidebar->ClearDisplayList();
    //reset controlbar
    ui->controlBar->ControlBarReset();

    FUNCTION_EXIT;
}

bool Widget::eventFilter(QObject *watched, QEvent *event)
{

    if(watched != ui->video_label || event->type() != QEvent::MouseButtonDblClick)
    {
        return false;
    }
    QMouseEvent* e = static_cast<QMouseEvent*>(event);
    if (e->button() != Qt::LeftButton)
    {
        return false;
    }
    FUNCTION_ENTER;
    qDebug() << "video label left double clicked";


    if(this->isFullScreen())
    {
        qDebug() << "full screen";
        ui->titlebar->show();
        this->showNormal();
    }
    else
    {
        qDebug() << "not full screen";
        ui->titlebar->hide();
        this->showFullScreen();
        int b_width = ui->body->width();
        int b_height = ui->body->height();
        qDebug() << "width " << b_width << " height " << b_height;

        QSize s_size = ui->sidebar->size();
        s_size.setHeight(b_height- ui->controlBar->height());
        ui->sidebar->resize(s_size);

        int s_x = b_width - ui->sidebar->width();
        ui->sidebar->move(s_x, 0);

        m_paSidebar->setStartValue(QPoint(b_width - ui->sidebar->width(), 0));
        m_paSidebar->setEndValue(QPoint(b_width-20, 0));
        m_paSidebar->setEasingCurve(QEasingCurve::OutQuad);
        m_paSidebar->start();

        QSize c_size = ui->controlBar->size();
        c_size.setWidth(b_width );
        ui->controlBar->resize(c_size);
        int y = b_height - ui->controlBar->height();
        ui->controlBar->move(0, y);

        m_paControlBar->setStartValue(QPoint(0, b_height - ui->controlBar->height()));
        m_paControlBar->setEndValue(QPoint(0, b_height));
        m_paControlBar->setEasingCurve(QEasingCurve::OutQuad);
        m_paControlBar->start();

        m_paVideoLabel->setStartValue(ui->video_label->size());
        m_paVideoLabel->setEndValue(QSize(b_width, b_height));
        m_paVideoLabel->setEasingCurve(QEasingCurve::OutQuad);
        m_paVideoLabel->start();
    }
    m_bSidebarShow = true;
    FUNCTION_EXIT;
    return true;
}






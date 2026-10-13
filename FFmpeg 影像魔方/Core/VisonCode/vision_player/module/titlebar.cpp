#include <QStyle>
#include "titlebar.h"
#include "ui_titlebar.h"
#include "userlogin.h"

TitleBar::TitleBar(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::TitleBar)
{
    ui->setupUi(this);

    m_parentWidget = this->parentWidget();   // 获取标题栏父窗口
    ui->label_title->setText(PLAYER_TITLE);
    ui->loginuser->setText(USER_NOLOGIN);
}

TitleBar::~TitleBar()
{
    delete ui;
}


QWidget *TitleBar::getBackground() const
{
    return ui->background;
}

void TitleBar::SetLoginUser(QString strName)
{
    if (strName.length() > 0)
    {
        ui->loginuser->setText(strName);
    }
    else
    {
        ui->loginuser->setText(USER_NOLOGIN);
    }
}


void TitleBar::setWindowTitle(const QString& title)
{
    QWidget::setWindowTitle(title);
    ui->label_title->setText(title);
}

void TitleBar::on_windowStateChanged(Qt::WindowStates windowStates)
{
    switch (windowStates)
    {
    case Qt::WindowMaximized:
        ui->bt_max->setProperty("Max", true);
        break;
    case Qt::WindowNoState:
        ui->bt_max->setProperty("Max", false);
        break;
    default:break;
    }
    ui->bt_max->style()->polish(ui->bt_max);
}


void TitleBar::on_bt_min_clicked()
{
    if(!m_parentWidget)return;
    m_parentWidget->showMinimized();
}


void TitleBar::on_bt_max_clicked()
{
    if(!m_parentWidget)return;
    if(m_parentWidget->isMaximized())
    {
        m_parentWidget->showNormal();
    }
    else
    {
        m_parentWidget->showMaximized();
    }
}

void TitleBar::on_bt_close_clicked()
{
    if(!m_parentWidget)return;
    m_parentWidget->close();
}


void TitleBar::on_loginbt_clicked()
{
    QVariant ret;
    emit TitleBarSignal(TITLE_BAR_SHOW_LOGIN, ret);
}


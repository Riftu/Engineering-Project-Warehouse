#include <QMessageBox>
#include <QCryptographicHash>
#include "userlogin.h"
#include "ui_userlogin.h"
#include "av_net.h"

UserLogin::UserLogin(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::UserLogin)
{
    FUNCTION_ENTER;
    ui->setupUi(this);
    this->setWindowFlags(this->windowFlags()&~Qt::WindowMinMaxButtonsHint);
    this->setWindowModality(Qt::ApplicationModal);
    this->setWindowFlags(this->windowFlags() | Qt::Dialog);
    ui->userpwd->setEchoMode(QLineEdit::EchoMode::Password);
    ui->originpwd->setEchoMode(QLineEdit::EchoMode::Password);
    ui->newpwd->setEchoMode(QLineEdit::EchoMode::Password);
    this->setStyleSheet("") ;
    m_iStatus = LOGIN_NO;
    m_strUser = "";
    FUNCTION_EXIT;
}

UserLogin::~UserLogin()
{
    FUNCTION_ENTER;
    delete ui;
    FUNCTION_EXIT;
}

void UserLogin::SetLoginType(int iType, QString strUser)
{
    m_iStatus = iType;
    m_strUser = strUser;
}

void UserLogin::on_login_bt_clicked()
{
    FUNCTION_ENTER;
    QString strUserName = ui->username->text();
    QString strUserPwd = ui->userpwd->text();
    if (strUserName.length() <= 0 || strUserPwd.length() <= 0)
    {
        QMessageBox box(QMessageBox::Warning, USER_LOGIN_NAME_PWD_ERROR_TITLE, USER_LOGIN_NAME_PWD_NULL_ERROR, QMessageBox::Ok, this);
        box.setStyleSheet("QLabel{font-size: 16px;color: black;}"
                          "QPushButton{font-size: 16px;color: black;}");
        box.exec();
        return;
    }
    QByteArray md5_hash;
    md5_hash = QCryptographicHash::hash(strUserPwd.toUtf8(), QCryptographicHash::Md5);
    QString md5hash_string = md5_hash.toHex();
    qDebug() << md5hash_string;
    bool bAuto = ui->checkBox->isChecked();
    STAvNet::GetInstance()->LoginUser(strUserName, md5hash_string, bAuto);
    this->hide();
    FUNCTION_EXIT;
}

void UserLogin::InitUi()
{
    FUNCTION_ENTER;
    qDebug() << "login status : " << m_iStatus;
    if (m_iStatus == LOGIN_AUTO)
    {
        ui->loginoutwidget->hide();
        ui->username->setEnabled(false);
        ui->userpwd->setEnabled(false);
        ui->login_bt->setEnabled(false);
        ui->checkBox->setEnabled(false);
        ui->loginwidget->show();

        ui->u_name->setEnabled(false);
        ui->originpwd->setEnabled(false);
        ui->newpwd->setEnabled(false);
        ui->resetbt->setEnabled(false);
        ui->u_name->setText("");
        ui->originpwd->setText("");
        ui->newpwd->setText("");
    }
    else if (m_iStatus == LOGIN_NO)
    {
        ui->loginoutwidget->hide();
        ui->username->setEnabled(true);
        ui->userpwd->setEnabled(true);
        ui->login_bt->setEnabled(true);
        ui->checkBox->setEnabled(true);
        ui->loginwidget->show();

        ui->u_name->setEnabled(false);
        ui->originpwd->setEnabled(false);
        ui->newpwd->setEnabled(false);
        ui->resetbt->setEnabled(false);
        ui->u_name->setText("");
        ui->originpwd->setText("");
        ui->newpwd->setText("");
    }
    else if (m_iStatus == LOGIN_OK)
    {
        ui->loginwidget->hide();
        ui->loginuser->setEnabled(false);
        ui->loginuser->setText(m_strUser);
        ui->loginoutwidget->show();

        ui->u_name->setEnabled(false);
        ui->originpwd->setEnabled(true);
        ui->newpwd->setEnabled(true);
        ui->resetbt->setEnabled(true);
        ui->u_name->setText(m_strUser);
        ui->originpwd->setText("");
        ui->newpwd->setText("");
    }
    this->hide();
    FUNCTION_EXIT;
}


void UserLogin::on_loginout_bt_clicked()
{
    FUNCTION_ENTER;
    emit LogoutSignal();
    this->hide();
    FUNCTION_EXIT;
}


void UserLogin::on_resetbt_clicked()
{
    FUNCTION_ENTER;
    QString strUserName = ui->u_name->text();
    QString strUserPwd = ui->originpwd->text();
    QString strUserPwdNew = ui->newpwd->text();
    if (strUserName.length() <= 0 || strUserPwd.length() <= 0 || strUserPwdNew.length() <= 0)
    {
        QMessageBox box(QMessageBox::Warning, USER_LOGIN_NAME_RESETPWD_ERROR_TITLE, USER_LOGIN_NAME_PWD_NULL_ERROR, QMessageBox::Ok, this);
        box.setStyleSheet("QLabel{font-size: 16px;color: black;}"
                          "QPushButton{font-size: 16px;color: black;}");
        box.exec();
        return;
    }
    if (strUserPwd == strUserPwdNew)
    {
        QMessageBox box(QMessageBox::Warning, USER_LOGIN_NAME_RESETPWD_ERROR_TITLE, USER_LOGIN_NAME_RESETPWD_NULL_ERROR, QMessageBox::Ok, this);
        box.setStyleSheet("QLabel{font-size: 16px;color: black;}"
                          "QPushButton{font-size: 16px;color: black;}");
        box.exec();
        return;
    }
    QByteArray md5_hash;
    md5_hash = QCryptographicHash::hash(strUserPwd.toUtf8(), QCryptographicHash::Md5);
    QString md5hash_string = md5_hash.toHex();

    QByteArray md5_hash_new = QCryptographicHash::hash(strUserPwdNew.toUtf8(), QCryptographicHash::Md5);
    QString md5hash_string_new = md5_hash_new.toHex();

    STAvNet::GetInstance()->UserResetPwd(strUserName, md5hash_string, md5hash_string_new);
    this->hide();
    FUNCTION_EXIT;
}


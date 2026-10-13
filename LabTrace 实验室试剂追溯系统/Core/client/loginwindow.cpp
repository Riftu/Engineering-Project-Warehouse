#include "loginwindow.h"
#include "ui_login.h"

LoginWindow::LoginWindow(QWidget *parent) :
    QDialog(parent),
    loginUI(new Ui::Login)
{
    loginUI->setupUi(this);
    loginUI->editPwd->setEchoMode(QLineEdit::Password);
}

LoginWindow::~LoginWindow()
{
    delete loginUI;
}

void LoginWindow::on_loginBtn_clicked()
{
    QString userName = loginUI->editUser->text().trimmed();
    QString userPwd = loginUI->editPwd->text().trimmed();
    if(userName.isEmpty() || userPwd.isEmpty())
    {
        QMessageBox::warning(this,"提示","用户名密码不能为空");
        return;
    }
    //只发射信号，账号密码交给Widget处理
    emit sigSendLoginInfo(userName, userPwd);
}

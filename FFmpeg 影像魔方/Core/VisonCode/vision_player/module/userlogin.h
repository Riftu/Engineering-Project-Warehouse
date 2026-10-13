#ifndef USERLOGIN_H
#define USERLOGIN_H

#include <QWidget>
#include "common.h"

namespace Ui {
class UserLogin;
}

class UserLogin : public QWidget
{
    Q_OBJECT
public:
    enum LOGIN_STATUS
    {
        LOGIN_NO = 0,
        LOGIN_AUTO ,
        LOGIN_OK
    };
public:
    explicit UserLogin(QWidget *parent = nullptr);
    ~UserLogin();
    void SetLoginType(int iType, QString strUser);
    void InitUi();

signals:
    void LogoutSignal();
private slots:
    void on_login_bt_clicked();

    void on_loginout_bt_clicked();

    void on_resetbt_clicked();

private:


private:
    Ui::UserLogin *ui;
    int m_iStatus;
    QString m_strUser;

};

#endif // USERLOGIN_H

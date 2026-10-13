#ifndef _AV_NET_H_
#define _AV_NET_H_

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include "common.h"

class STAvNet : public QObject
{
    Q_OBJECT
public:
    static STAvNet *GetInstance();
    static void DestroyINstance();
    void NetInit();
    void LoginUser(QString strName, QString strPwd, bool bAuto);
    void UserResetPwd(QString strName, QString strPwd, QString strNewPwd);
    bool AutoLogin();
    void GetAvList();
    bool RemoveAutoLoginFile();
    void ResetStNet();


signals:
    void AVNetResult(int iType, QVariant data);

private:
    STAvNet();
    ~STAvNet();

    QString GetDiskSerialNumber();
    QString GetCpuID();
    QString EncryptUserData(QString strUserInfo, QString strKey);
    QString DecryptUserData(QString strUserInfo, QString strKey);

private slots:
    void LoginDataResult();
    void AVListDataResult();
    void UserResetPwdResult();

private:
    QNetworkAccessManager *m_AccessManager;
    QString m_strToken;
    QString m_strUserName;
    QString m_strPwd;
    bool m_bAuto;
    int m_iStatus;
    QNetworkReply  *m_pReplay;
    static STAvNet *m_pInstance;
};

#endif //_AV_NET_H_

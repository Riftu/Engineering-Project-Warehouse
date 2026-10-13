#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QProcess>
#include <windows.h>
#include <QCryptographicHash>
#include <QCoreApplication>
#include <QDir>

#include "common.h"
#include "av_net.h"
#include "qaesencryption.h"

STAvNet* STAvNet::m_pInstance = NULL;

STAvNet::STAvNet()
{
    FUNCTION_ENTER;
    m_AccessManager = NULL;
    m_bAuto = false;
    m_iStatus = AV_NET_INIT;
    m_strPwd = "";
    m_strToken = "";
    m_strUserName = "";
    m_pReplay = NULL;
    FUNCTION_EXIT;
}

STAvNet::~STAvNet()
{
    FUNCTION_ENTER;
    if (NULL != m_AccessManager)
    {
        delete m_AccessManager;
        m_AccessManager = NULL;
    }
    FUNCTION_EXIT;
}

QString STAvNet::GetDiskSerialNumber()
{
    FUNCTION_ENTER;
    QString cmd="wmic diskdrive where index=0 get serialnumber";
    QProcess p;
    p.start(cmd);
    p.waitForFinished();
    QString result = QString::fromLocal8Bit(p.readAllStandardOutput());
    QStringList list = cmd.split(" ");
    result = result.remove(list.last(), Qt::CaseInsensitive);
    result = result.replace("\r", "");
    result = result.replace("\n", "");
    result = result.simplified();
    qDebug() << result;
    FUNCTION_EXIT;
    return result;
}

QString STAvNet::GetCpuID()
{
    FUNCTION_ENTER;
    QString cmd="wmic cpu get processorid";
    QProcess p;
    p.start(cmd);
    p.waitForFinished();
    QString result = QString::fromLocal8Bit(p.readAllStandardOutput());
    QStringList list = cmd.split(" ");
    result = result.remove(list.last(), Qt::CaseInsensitive);
    result = result.replace("\r", "");
    result = result.replace("\n", "");
    result = result.simplified();
    qDebug() << result;
    FUNCTION_EXIT;
    return result;
}

QString STAvNet::EncryptUserData(QString strUserInfo, QString strKey)
{
    FUNCTION_ENTER;
    QAESEncryption encryption(QAESEncryption::AES_256, QAESEncryption::ECB);
    QByteArray arr = encryption.encode(strUserInfo.toLocal8Bit(), strKey.toLocal8Bit());
    QString encodeStr = arr.toBase64();
    qDebug()<<"encode："<< encodeStr;
    FUNCTION_EXIT;
    return encodeStr;
}

QString STAvNet::DecryptUserData(QString strUserInfo, QString strKey)
{
    FUNCTION_ENTER;
    QAESEncryption encryption(QAESEncryption::AES_256, QAESEncryption::ECB);
    QByteArray arr1 = strUserInfo.toLocal8Bit();
    QByteArray arr2 = QByteArray::fromBase64(arr1);;

    //qDebug() <<"arr2" << arr2;
    QByteArray arr3 = encryption.decode(arr2, strKey.toLocal8Bit());
    QByteArray decodedText = encryption.removePadding(arr3);
    QString decrypt_str = QString(decodedText);
    //qDebug()<<"dncode:"<<decrypt_str;
    FUNCTION_EXIT;
    return decrypt_str;
}

STAvNet *STAvNet::GetInstance()
{
    FUNCTION_ENTER;
    if (NULL == m_pInstance)
    {
        m_pInstance = new STAvNet();
    }
    FUNCTION_EXIT;
    return m_pInstance;
}

void STAvNet::DestroyINstance()
{
    FUNCTION_ENTER;
    if (NULL != m_pInstance)
    {
        delete m_pInstance;
        m_pInstance = NULL;
    }
    FUNCTION_EXIT;
}

void STAvNet::NetInit()
{
    FUNCTION_ENTER;
    if (NULL == m_AccessManager)
    {
        m_AccessManager = new QNetworkAccessManager(this);
    }
    FUNCTION_EXIT;
}

void STAvNet::LoginUser(QString strName, QString strPwd, bool bAuto)
{
    FUNCTION_ENTER;

    if (AV_NET_REQUESTING == m_iStatus)
    {
        return;
    }
    m_iStatus = AV_NET_REQUESTING;
    QString strInfo = GetDiskSerialNumber();
    strInfo += "-" + GetCpuID();
    QByteArray md5_hash;
    md5_hash = QCryptographicHash::hash(strInfo.toUtf8(), QCryptographicHash::Md5);
    QString md5hash_string = md5_hash.toHex();
    qDebug() << md5hash_string;
    qDebug() << strInfo;

    QNetworkRequest request;
    QString strUrl = QString(USER_DOMAIN) + QString(USER_LOGIN_URL);
    request.setUrl(QUrl(strUrl));
    request.setRawHeader("Content-Type", "application/json;charset=utf-8");
    request.setRawHeader("User-Agent", "API Explorer");

    QJsonDocument jsonDoc;
    QJsonObject domain;
    domain.insert("username", strName);
    domain.insert("userpwd", strPwd);

    domain.insert("userinfo", md5hash_string);
    jsonDoc.setObject(domain);
    QByteArray byteArray = jsonDoc.toJson(QJsonDocument::Compact);
    m_pReplay = m_AccessManager->post(request, byteArray);
    connect(m_pReplay, SIGNAL(finished()), this, SLOT(LoginDataResult()));

    m_strUserName = strName;
    m_strPwd = strPwd;
    m_bAuto = bAuto;
    FUNCTION_EXIT;
}

void STAvNet::UserResetPwd(QString strName, QString strPwd, QString strNewPwd)
{
    FUNCTION_ENTER;
    Q_UNUSED(strName);
    if (strPwd == strNewPwd)
    {
        return;
    }
    if (AV_NET_REQUESTING == m_iStatus)
    {
        return;
    }
    m_iStatus = AV_NET_REQUESTING;
    QNetworkRequest request;
    QString strUrl = QString(USER_DOMAIN) + QString(USER_RESETPWD);
    request.setUrl(QUrl(strUrl));
    request.setRawHeader("Content-Type", "application/json;charset=utf-8");
    request.setRawHeader("User-Agent", "API Explorer");
    QJsonDocument jsonDoc;
    QJsonObject domain;
    domain.insert("token", m_strToken);
    domain.insert("newpwd", strNewPwd);
    jsonDoc.setObject(domain);
    QByteArray byteArray = jsonDoc.toJson(QJsonDocument::Compact);

    m_pReplay = m_AccessManager->post(request, byteArray);
    connect(m_pReplay, SIGNAL(finished()), this, SLOT(UserResetPwdResult()));
    FUNCTION_EXIT;
}

bool STAvNet::AutoLogin()
{
    FUNCTION_ENTER;
    QString strFile = USER_DATA_PATH + "/tmp/conifg.txt";
    QFile file(strFile);
    if (!file.open(QIODevice::ReadOnly))
    {
        qDebug() << "open error " << file.errorString();
        return false;
    }
    QByteArray arr = file.readAll();
    QString str = DecryptUserData(arr, AES_KEY);
    QStringList strList = str.split(" ");
    if (strList.length() == 2)
    {
        LoginUser(strList[0], strList[1], true);
    }
    else
    {
        qDebug() << "auto login error,  please login";
        return false;
    }
    FUNCTION_EXIT;
    return true;
}

void STAvNet::GetAvList()
{
    FUNCTION_ENTER;
    if (AV_NET_REQUESTING == m_iStatus)
    {
        return;
    }
    m_iStatus = AV_NET_REQUESTING;
    if (m_strToken.length() <= 0)
    {
        return;
    }
    QNetworkRequest request;
    QString strUrl = QString(USER_DOMAIN) + QString(USER_GETAVLIST_URL);
    request.setUrl(QUrl(strUrl));
    request.setRawHeader("Content-Type", "application/json;charset=utf-8");
    request.setRawHeader("User-Agent", "API Explorer");
    QJsonDocument jsonDoc;
    QJsonObject domain;
    domain.insert("token", m_strToken);
    jsonDoc.setObject(domain);
    QByteArray byteArray = jsonDoc.toJson(QJsonDocument::Compact);
    m_pReplay = m_AccessManager->post(request, byteArray);
    connect(m_pReplay, SIGNAL(finished()), this, SLOT(AVListDataResult()));
    FUNCTION_EXIT;
}

bool STAvNet::RemoveAutoLoginFile()
{
    FUNCTION_ENTER;
    bool bRet = false;
    QString strFile = USER_DATA_PATH + "/tmp/conifg.txt";
    QFile file(strFile);
    if (file.remove())
    {
        bRet = true;
    }
    FUNCTION_EXIT;
    return bRet;
}

void STAvNet::ResetStNet()
{
    FUNCTION_ENTER;
    m_bAuto = false;
    m_iStatus = AV_NET_INIT;
    m_strPwd = "";
    m_strToken = "";
    m_strUserName = "";
    if (NULL != m_pReplay)
    {
        disconnect(m_pReplay, 0, 0, 0);
        m_pReplay->abort();
        m_pReplay->deleteLater();
    }
    m_pReplay = NULL;

    FUNCTION_EXIT;
}

void STAvNet::LoginDataResult()
{
    FUNCTION_ENTER;
    QVariant ret;
    AVLoginStatus retval;
    QByteArray arr;
    QJsonParseError jsonError;
    QJsonObject obj;
    QJsonDocument doc;

    m_iStatus = AV_NET_FREE;

    QNetworkReply  *replay = (QNetworkReply *)sender();
    if (m_pReplay != replay)
    {
        if (NULL != replay )
        {
            replay->deleteLater();
        }
        replay = NULL;
        goto LOGIN_ERROR;
    }
    if (NULL == m_pReplay) return;

    arr = m_pReplay->readAll();
    qDebug() << arr;

    doc = QJsonDocument::fromJson(arr, &jsonError);

    if( jsonError.error != QJsonParseError::NoError || !doc.isObject() )
    {
        qDebug("json error");
        goto LOGIN_ERROR;
        //return;
    }
    obj = doc.object();
    if (obj.contains("status"))
    {
        int status = obj.value("status").toInt();
        if (status == 1)
        {
            m_strToken = obj.value("token").toString();
            if (m_strToken.length() == 0)
            {
                qDebug("token error");
                goto LOGIN_ERROR;
            }
            if (m_bAuto)
            {
                //write auto login flie, config.txt
                QString strUser = m_strUserName + " " + m_strPwd;
                QString strEn = EncryptUserData(strUser, AES_KEY);
                qDebug() << strUser << "  " << strEn;

                //save user info
                QString strDir = USER_DATA_PATH+ "/tmp";
                QDir dir(strDir);
                if (!dir.exists())
                {
                    if (!dir.mkdir(strDir))
                    {
                        goto LOGIN_ERROR;
                    }
                }
                QString strFile = USER_DATA_PATH + "/tmp/conifg.txt";
                QFile file(strFile);
                if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
                {
                    qDebug() << "open error " << file.errorString();
                    goto LOGIN_ERROR;
                }

                file.write(strEn.toStdString().data());
                file.close();
            }
        }
        else
        {
            goto LOGIN_ERROR;
        }
    }
    else
    {
        goto LOGIN_ERROR;
    }

    m_pReplay->deleteLater();
    m_pReplay = NULL;

    //m_iStatus = AV_NET_LOGIN_SUCCESS;
    retval.m_iStatus = AV_NET_LOGIN_SUCCESS;
    retval.m_strUser = m_strUserName;
    ret.setValue(retval);
    emit AVNetResult(AV_NET_LOGIN_SUCCESS, ret);
    FUNCTION_EXIT;
    return;

LOGIN_ERROR:

    if (NULL != m_pReplay)
    {
        m_pReplay->deleteLater();
        m_pReplay = NULL;
    }
    //m_iStatus = AV_NET_LOGIN_ERROR;
    retval.m_iStatus = AV_NET_LOGIN_ERROR;
    retval.m_strUser = m_strUserName;
    ret.setValue(retval);
    emit AVNetResult(AV_NET_LOGIN_ERROR, ret);
}

void STAvNet::AVListDataResult()
{
    FUNCTION_ENTER;
    QVariant ret;
    QByteArray arr;
    m_iStatus = AV_NET_FREE;

    QNetworkReply  *replay = (QNetworkReply *)sender();
    if (m_pReplay != replay)
    {
        if (NULL != replay )
        {
            replay->deleteLater();
        }
        replay = NULL;
        return;
    }
    if (NULL == m_pReplay) return;

    arr = m_pReplay->readAll();
    m_pReplay->deleteLater();
    m_pReplay = NULL;
    qDebug() << arr;

    ret.setValue(arr);
    emit AVNetResult(AV_NET_AVLIST, ret);
    FUNCTION_EXIT;
}

void STAvNet::UserResetPwdResult()
{
    FUNCTION_ENTER;
    QVariant ret;
    QByteArray arr;
    m_iStatus = AV_NET_FREE;
    QJsonParseError jsonError;
    QJsonObject obj;
    QJsonDocument doc;

    QNetworkReply  *replay = (QNetworkReply *)sender();
    if (m_pReplay != replay)
    {
        if (NULL != replay )
        {
            replay->deleteLater();
        }
        replay = NULL;
        return;
    }
    if (NULL == m_pReplay) return;

    arr = m_pReplay->readAll();
    m_pReplay->deleteLater();
    m_pReplay = NULL;
    qDebug() << arr;

    doc = QJsonDocument::fromJson(arr, &jsonError);

    if( jsonError.error != QJsonParseError::NoError || !doc.isObject() )
    {
        qDebug("json error");
        goto RESET_ERROR;
        //return;
    }
    obj = doc.object();
    if (obj.contains("status"))
    {
        int status = obj.value("status").toInt();
        if (status == 1)
        {
            m_strToken = "";
            m_strUserName = "";
            m_strPwd = "";
            ret.setValue(true);
            emit AVNetResult(AV_NET_PWDRESET, ret);
        }
        else
        {
            goto RESET_ERROR;
        }
    }
    else
    {
        goto RESET_ERROR;
    }
    FUNCTION_EXIT;
    return;

RESET_ERROR:
    ret.setValue(false);
    emit AVNetResult(AV_NET_PWDRESET, ret);
}



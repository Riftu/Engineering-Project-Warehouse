#ifndef TCPCLIENT_H
#define TCPCLIENT_H
#include <QObject>
#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QJsonArray>
#include <QVariantList>

class TcpClient : public QObject
{
    Q_OBJECT
public:
    explicit TcpClient(QObject *parent = nullptr);
    ~TcpClient();

signals:
    void sigConnected();          //TCP连接成功信号
    void sigDisconnected();       //连接断开
    void sigNetError(QString err);

    void sigRealData(QVariantMap data);        // 实时推送采集数据 push_data
    void sigLoginResult(bool ok, QString msg); // 登录返回结果

    // 将服务端返回的历史数据发送给主界面
    void sigHistoryResult(QVariantList list);
    void sigBatchReport(const QJsonObject &packet);
    void sigDeviceHeartbeat(const QJsonObject &heartbeat);
    void sigCommandResult(const QJsonObject &result);
    void sigCommandAck(const QJsonObject &ack);

public slots:
    void slotConnect(QString ip, quint16 port);
    void slotSendJson(QJsonObject obj);
    void slotTryReConnect();        //重连
    void slotStopReconnect();
    void slotReadyRead();

private slots:
    void OnConnected();
    void OnDisconnected();
    void slotSocketError(QTcpSocket::SocketError err);

private:
    QTcpSocket *m_socket = nullptr;
    QByteArray m_recvBuf;  // 粘包缓冲区
    QTimer *m_reconnectTimer;
    QString m_ip;
    quint16 m_port;
    bool m_bManualClose = false;
};
#endif // TCPCLIENT_H

#include "tcpclient.h"
#include <QVariantMap>

TcpClient::TcpClient(QObject *parent)
    : QObject(parent)
{
    // 断线后 2 秒自动重连
    m_reconnectTimer = new QTimer(this);
    m_reconnectTimer->setInterval(2000);
    connect(m_reconnectTimer,&QTimer::timeout,this,&TcpClient::slotTryReConnect);
}

TcpClient::~TcpClient()
{
    m_bManualClose = true;  // 主动关闭，避免重连
    if(m_socket)
    {
        m_socket->disconnectFromHost();
        m_socket->deleteLater();
    }
}

//  连接
void TcpClient::slotConnect(QString ip, quint16 port)
{
    m_ip = ip;
    m_port = port;
    m_bManualClose = false;

    // 释放旧 socket，防止残留信号
    if(m_socket)
    {
        m_socket->disconnectFromHost();
        m_socket->deleteLater();
    }

    m_socket = new QTcpSocket(this);
    connect(m_socket,&QTcpSocket::readyRead,this,&TcpClient::slotReadyRead);
    connect(m_socket, &QTcpSocket::connected, this,&TcpClient::OnConnected);
    connect(m_socket,&QTcpSocket::disconnected,this,&TcpClient::OnDisconnected);
    connect(m_socket,QOverload<QTcpSocket::SocketError>::of(&QTcpSocket::error),
            this,&TcpClient::slotSocketError);

    m_socket->connectToHost(m_ip,m_port);
}

//  重连
void TcpClient::slotTryReConnect()
{
    if(m_bManualClose)
    {
        m_reconnectTimer->stop();
        return;
    }

    // 已连接或正在连接中，跳过
    if(m_socket && (m_socket->state() == QTcpSocket::ConnectedState
            || m_socket->state() == QTcpSocket::ConnectingState))
    {
        return;
    }

    qDebug()<<"尝试重连 TCP:"<<m_ip<<m_port;
    slotConnect(m_ip,m_port);
}

void TcpClient::slotStopReconnect()
{
    m_bManualClose = true;
    m_reconnectTimer->stop();
}

//  连接状态回调
void TcpClient::OnConnected()
{
    m_reconnectTimer->stop();   // 连上了，停止重连
    qDebug()<<"TCP连接成功";
    emit sigConnected(); //TCP连上成功，发射信号
}

void TcpClient::OnDisconnected()
{
    qDebug()<<"TCP连接断开，开启自动重连";
    emit sigDisconnected();
    if(!m_bManualClose)
    {
        m_reconnectTimer->start();
    }
}

//  错误处理
void TcpClient::slotSocketError(QTcpSocket::SocketError err)
{
    Q_UNUSED(err)
    emit sigNetError(m_socket->errorString());
    if(!m_bManualClose && !m_reconnectTimer->isActive())
            m_reconnectTimer->start();
}

//  发送
void TcpClient::slotSendJson(QJsonObject obj)
{
    if(!m_socket || m_socket->state() != QAbstractSocket::ConnectedState)
    {
        qDebug() << "未连接，发送失败";
        emit sigNetError("未连接");
        return;
    }

    QByteArray data = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    data.append('\n');
    m_socket->write(data);
    qDebug() << "已发送:" << data;
}
//  接收 / 粘包处理
void TcpClient::slotReadyRead()
{
    // 累积到缓冲，按 '\n' 切分
    m_recvBuf += m_socket->readAll();

    // 防止缓冲无限增长
    if(m_recvBuf.size() > 4 * 1024 * 1024)
    {
        qWarning() << "接收缓冲超限，清空";
        m_recvBuf.clear();
        return;
    }

    // 服务端使用换行符表示一条消息结束
    while (m_recvBuf.contains('\n'))
    {
        int index = m_recvBuf.indexOf('\n');

        // 取出一条完整消息（不含 '\n'），并从缓冲中移除
        QByteArray packetData =m_recvBuf.left(index);
        m_recvBuf.remove(0, index + 1);

        packetData = packetData.trimmed();
        if (packetData.isEmpty())
        {
            continue;
        }

        // 解析 JSON 数据
        QJsonParseError error;
        QJsonDocument document =QJsonDocument::fromJson(packetData, &error);

        if (error.error != QJsonParseError::NoError ||
            !document.isObject())
        {
            qDebug() << "收到的不是有效 JSON:"
                     << packetData;
            continue;
        }

        QJsonObject packet =document.object();

        QString type =packet.value("type").toString();

        // 处理服务端转发的设备采集数据
        if (type == QStringLiteral("telemetry"))
        {
            QVariantMap data =
                packet.value("data")
                      .toObject()
                      .toVariantMap();

            emit sigRealData(data);
        }

        // 处理以后控制命令的返回结果
        else if (type == QStringLiteral("command_result"))
        {
            qDebug() << "控制命令结果:"
                     << packet;
            emit sigCommandResult(packet);
        }

        // 处理服务端返回的历史查询结果
        else if (type == QStringLiteral("history_result"))
        {
            bool success =packet.value("success").toBool();

            if (!success)
            {
                qDebug() << "历史数据查询失败";
                continue;
            }

            // 将 JSON 数组转换成 QVariantList
            QVariantList historyList =
                packet.value("data")
                      .toArray()
                      .toVariantList();

            qDebug() << "客户端收到历史数据:"
                     << historyList.size()
                     << "条";

            // 把结果交给主界面显示
            emit sigHistoryResult(historyList);
        }

        // ---------- 日报 ----------
        else if(type == "batch_report")
        {
            emit sigBatchReport(packet);
        }
        else if(type == "device_heartbeat")
        {
            emit sigDeviceHeartbeat(packet.value("data").toObject());
        }
        else if(type == "command_ack")
        {
            emit sigCommandAck(packet.value("data").toObject());
        }
    }
}


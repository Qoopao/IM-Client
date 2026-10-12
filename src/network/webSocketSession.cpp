#include "webSocketSession.h"
#include "../proto/sdkws.pb.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QSslSocket>
#include <QUuid>
#include <QUrl>
#include <utility>

namespace network {

WebSocketSession::WebSocketSession(QObject* parent)
    : QObject(parent)
{
    loadTrustAnchor();
    m_heartbeatTimer.setInterval(30000);
    connect(&m_heartbeatTimer, &QTimer::timeout, this, [this] {
        if (m_waitingPong) {
            failConnection(QStringLiteral("心跳超时，服务器连接已断开"));
            return;
        }
        m_waitingPong = sendRequest(0, {});
    });
}

WebSocketSession::~WebSocketSession()
{
    close();
}

void WebSocketSession::open(uint64_t account)
{
    close();
    m_account = account;
    m_socket = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);
    m_socket->setSslConfiguration(QSslConfiguration::defaultConfiguration());

    connect(m_socket, &QWebSocket::connected, this, &WebSocketSession::connected);
    connect(m_socket, &QWebSocket::disconnected, this, &WebSocketSession::onDisconnected);
    connect(m_socket, &QWebSocket::binaryMessageReceived, this, &WebSocketSession::onFrameReceived);
    connect(m_socket, &QWebSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        const auto error = m_socket->errorString();
        failConnection(error);
    });
    connect(m_socket, QOverload<const QList<QSslError>&>::of(&QWebSocket::sslErrors), this,
            [this](const QList<QSslError>& errors) {
                if (!errors.isEmpty())
                    failConnection(QStringLiteral("SSL握手失败：") + errors.first().errorString());
            });

    m_socket->open(QUrl(QString::fromUtf8(kWsUrl)));
}

void WebSocketSession::close()
{
    m_heartbeatTimer.stop();
    m_authenticated = false;
    m_waitingPong = false;
    m_account = 0;
    m_token.clear();

    auto* socket = std::exchange(m_socket, nullptr);
    if (!socket)
        return;

    socket->disconnect(this);
    socket->abort();
    socket->deleteLater();
}

void WebSocketSession::setAuthenticated(const QString& token)
{
    m_token = token;
    m_authenticated = true;
    m_heartbeatTimer.start();
}

bool WebSocketSession::isOnline() const
{
    return m_authenticated && m_socket && m_socket->state() == QAbstractSocket::ConnectedState;
}

bool WebSocketSession::sendRequest(int type, const QByteArray& data, const QString& requestId)
{
    if (!m_socket || m_socket->state() != QAbstractSocket::ConnectedState)
        return false;
    if (!m_authenticated && type != 101)
        return false;

    sdkws::SdkWSReq req;
    req.set_type(type);
    req.set_account(m_account);
    req.set_token(m_token.toStdString());
    req.set_requestid((requestId.isEmpty() ? QUuid::createUuid().toString(QUuid::WithoutBraces) : requestId).toStdString());
    req.set_data(data.constData(), data.size());
    return m_socket->sendBinaryMessage(QByteArray::fromStdString(req.SerializeAsString())) >= 0;
}

void WebSocketSession::onDisconnected()
{
    close();
    emit connectionLost();
}

void WebSocketSession::onFrameReceived(const QByteArray& frame)
{
    sdkws::SdkWSResp resp;
    if (!resp.ParseFromArray(frame.constData(), frame.size()))
        return;

    if (m_authenticated && resp.type() == 0 && resp.errorcode() == "0") {
        m_waitingPong = false;
        return;
    }
    emit responseReceived(frame);
}

void WebSocketSession::failConnection(const QString& error)
{
    const bool wasOnline = m_authenticated;
    close();
    emit connectionError(error);
    if (wasOnline)
        emit connectionLost();
}

void WebSocketSession::loadTrustAnchor()
{
    const QString certPath = findCertFile();
    if (certPath.isEmpty()) {
        qWarning() << "WebSocketSession: 未找到 ca.crt";
        return;
    }

    const auto certs = QSslCertificate::fromPath(certPath, QSsl::Pem);
    if (certs.isEmpty()) {
        qWarning() << "WebSocketSession: 无法解析证书:" << certPath;
        return;
    }

    auto ssl = QSslConfiguration::defaultConfiguration();
    ssl.setCaCertificates(certs);
    ssl.setPeerVerifyMode(QSslSocket::VerifyPeer);
    QSslConfiguration::setDefaultConfiguration(ssl);
}

QString WebSocketSession::findCertFile() const
{
    const auto appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        appDir + QStringLiteral("/tools/certs/ca.crt"),
        appDir + QStringLiteral("/../../tools/certs/ca.crt"),
        appDir + QStringLiteral("/../tools/certs/ca.crt"),
        QDir::currentPath() + QStringLiteral("/tools/certs/ca.crt"),
    };
    for (const auto& path : candidates) {
        if (QFileInfo::exists(path))
            return path;
    }
    return {};
}

}

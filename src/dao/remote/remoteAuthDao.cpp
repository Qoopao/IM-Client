#include "remoteAuthDao.h"
#include <QUuid>
#include <utility>

namespace dao {

RemoteAuthDao::RemoteAuthDao(network::WebSocketSession& connection, QObject* parent)
    : QObject(parent)
    , m_connection(connection)
{
    m_timeoutTimer.setSingleShot(true);
    connect(&m_timeoutTimer, &QTimer::timeout, this, [this] { failAuthentication(QStringLiteral("登录超时，请稍后重试")); });
    connect(&m_connection, &network::WebSocketSession::connected, this, &RemoteAuthDao::onConnected);
    connect(&m_connection, &network::WebSocketSession::responseReceived, this, &RemoteAuthDao::onResponseReceived);
    connect(&m_connection, &network::WebSocketSession::connectionError, this, &RemoteAuthDao::failAuthentication);
    connect(&m_connection, &network::WebSocketSession::connectionLost, this, [this] { failAuthentication(QStringLiteral("服务器连接失败")); });
}

RemoteAuthDao::~RemoteAuthDao()
{
    cancel();
}

void RemoteAuthDao::sendAuth(uint64_t account, const QString& password, Callback callback)
{
    if (m_callback || !callback)
        return;

    cancel();
    m_callback = std::move(callback);
    m_pendingRequestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_password = password.toUtf8();
    m_timeoutTimer.start(kTimeoutMs);
    m_connection.open(account);
}

void RemoteAuthDao::cancel()
{
    m_timeoutTimer.stop();
    m_callback = nullptr;
    m_pendingRequestId.clear();
    m_password.clear();
    m_connection.close();
}

void RemoteAuthDao::onConnected()
{
    if (!m_callback)
        return;

    const bool sent = m_connection.sendRequest(101, m_password, m_pendingRequestId);
    m_password.clear();
    if (!sent)
        failAuthentication(QStringLiteral("登录请求发送失败"));
}

void RemoteAuthDao::onResponseReceived(const QByteArray& frame)
{
    if (!m_callback)
        return;

    sdkws::SdkWSResp resp;
    if (!resp.ParseFromArray(frame.constData(), frame.size()) || QString::fromStdString(resp.requestid()) != m_pendingRequestId)
        return;

    const auto result = parseResp(resp);
    if (result.error == model::LoginError::None)
        m_connection.setAuthenticated(result.token);
    else
        m_connection.close();

    finishAuthentication(result);
}

void RemoteAuthDao::failAuthentication(const QString& error)
{
    if (!m_callback)
        return;

    m_connection.close();
    model::LoginResult result;
    result.error = model::LoginError::NetworkError;
    result.message = error;
    finishAuthentication(result);
}

void RemoteAuthDao::finishAuthentication(const model::LoginResult& result)
{
    m_timeoutTimer.stop();
    m_pendingRequestId.clear();
    m_password.clear();
    auto callback = std::move(m_callback);
    m_callback = nullptr;
    if (callback)
        callback(result);
}

model::LoginResult RemoteAuthDao::parseResp(const sdkws::SdkWSResp& resp) const
{
    model::LoginResult result;
    const auto code = QString::fromStdString(resp.errorcode());
    const auto error = QString::fromStdString(resp.errormsg());

    if ((code.isEmpty() || code == "0") && error.isEmpty()) {
        result.token = QString::fromStdString(resp.token());
        result.error = model::LoginError::None;
    } else if (code == "INVALID_PARAM" || code == "USER_NOT_FOUND" || code == "WRONG_PASSWORD") {
        result.error = model::LoginError::InvalidCredentials;
        result.message = error;
    } else {
        result.error = model::LoginError::ServerRejected;
        result.message = error;
    }
    return result;
}

}

#include "remoteFriendDao.h"
#include "../../proto/sdkws.pb.h"
#include <QDateTime>
#include <QUuid>

namespace dao {

RemoteFriendDao::RemoteFriendDao(network::WebSocketSession& connection, QObject* parent)
    : QObject(parent)
    , m_connection(connection)
{
    connect(&connection, &network::WebSocketSession::responseReceived, this, &RemoteFriendDao::receive);
    connect(&connection, &network::WebSocketSession::connectionLost, this,
            [this] { failAll(QStringLiteral("好友请求中断，请重新登录后刷新")); });
    m_timer.setInterval(1000);
    connect(&m_timer, &QTimer::timeout, this, [this] {
        QList<Callback> expired;
        const auto now = QDateTime::currentMSecsSinceEpoch();
        for (auto it = m_pending.begin(); it != m_pending.end();) {
            if (it->deadline > now) {
                ++it;
                continue;
            }
            expired.append(it->callback);
            it = m_pending.erase(it);
        }
        friendpb::Reply reply;
        reply.set_errorcode("TIMEOUT");
        reply.set_errormessage("Friend request timed out; refresh to confirm the result");
        for (const auto& callback : expired)
            callback(reply);
    });
    m_timer.start();
}

void RemoteFriendDao::execute(const friendpb::Command& command, Callback callback)
{
    const auto id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const int type = 220 + static_cast<int>(command.operation());
    m_pending.insert(id, {type, QDateTime::currentMSecsSinceEpoch() + 12000, std::move(callback)});
    if (!m_connection.sendRequest(type, QByteArray::fromStdString(command.SerializeAsString()), id)) {
        const auto pending = m_pending.take(id);
        friendpb::Reply reply;
        reply.set_errorcode("OFFLINE");
        reply.set_errormessage("Server connection is unavailable");
        pending.callback(reply);
    }
}

void RemoteFriendDao::cancel()
{
    m_pending.clear();
}

void RemoteFriendDao::failAll(const QString& message)
{
    const auto pending = m_pending;
    m_pending.clear();
    friendpb::Reply reply;
    reply.set_errorcode("OFFLINE");
    reply.set_errormessage(message.toStdString());
    for (const auto& request : pending)
        request.callback(reply);
}

void RemoteFriendDao::receive(const QByteArray& frame)
{
    sdkws::SdkWSResp response;
    if (!response.ParseFromArray(frame.constData(), frame.size()))
        return;
    if (response.type() == 230) {
        emit refreshRequested();
        return;
    }
    const auto id = QString::fromStdString(response.requestid());
    const auto it = m_pending.constFind(id);
    if (it == m_pending.cend() || it->type != response.type())
        return;
    const auto pending = m_pending.take(id);
    friendpb::Reply reply;
    if (!reply.ParseFromString(response.data())) {
        reply.set_errorcode("INVALID_RESPONSE");
        reply.set_errormessage("Invalid friend response");
    } else if (!response.errorcode().empty() && response.errorcode() != "0") {
        reply.set_errorcode(response.errorcode());
        reply.set_errormessage(response.errormsg());
    }
    pending.callback(reply);
}

}

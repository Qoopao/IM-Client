#include "chatService.h"
#include "../proto/sdkws.pb.h"
#include "../proto/msg.pb.h"
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QUuid>
#include <algorithm>

namespace service {
ChatService::ChatService(network::WebSocketSession& session, dao::WcdbChatDao& storage, QObject* parent)
    : QObject(parent)
    , m_session(session)
    , m_storage(storage)
{
    connect(&session, &network::WebSocketSession::responseReceived, this, &ChatService::receive);
    connect(&session, &network::WebSocketSession::connectionLost, this, [this] {
        if (m_account.isEmpty())
            return;
        m_status = QStringLiteral("连接已断开，请退出登录后重新登录");
        m_historyCursors.clear();
        failPending(QStringLiteral("连接断开，发送结果未确认"));
        emit changed();
    });
    m_timer.setInterval(1000);
    m_conversationTimer.setSingleShot(true);
    m_conversationTimer.setInterval(200);
    connect(&m_conversationTimer, &QTimer::timeout, this, &ChatService::requestConversations);
    connect(&m_timer, &QTimer::timeout, this, [this] {
        const auto now = QDateTime::currentMSecsSinceEpoch();
        bool updated = false;
        for (auto it = m_pending.begin(); it != m_pending.end();) {
            if (it->deadline > now) {
                ++it;
                continue;
            }
            const auto pending = it.value();
            it = m_pending.erase(it);
            for (auto m : std::as_const(m_messages))
                if (m.id == pending.id && m.outgoing) {
                    m.status = 3;
                    persist(m);
                    updated = true;
                    break;
                }
        }
        if (updated) {
            emit errorOccurred(QStringLiteral("发送超时，服务端是否接收尚未确认"));
            emit changed();
        }
    });
}

QString ChatService::conversation(const QString& peer) const
{
    const auto owner = m_account.toULongLong();
    const auto other = peer.toULongLong();
    return QString::number(std::min(owner, other)) + "_" + QString::number(std::max(owner, other));
}

void ChatService::start(const QString& account)
{
    stop();
    m_account = account;
    m_status = QStringLiteral("已连接，正在同步离线消息…");
    QString error;
    if (!m_storage.load(account, m_messages, error))
        emit errorOccurred(error);
    const auto saved = m_messages;
    for (auto m : saved)
        if (m.status == 1) {
            m.status = 3;
            persist(m);
        }
    m_timer.start();        // 启动超时定时器
    m_conversationTimer.start();
    emit changed();
}
void ChatService::stop()
{
    failPending(QString());
    m_timer.stop();
    m_conversationTimer.stop();
    m_pending.clear();
    m_messages.clear();
    m_clearedSeq.clear();
    m_lastReadSent.clear();
    m_conversations.clear();
    m_historyCursors.clear();
    m_account.clear();
    m_peer.clear();
    m_status.clear();
    m_viewing = false;
}
bool ChatService::openPeer(const QString& account)
{
    const auto input = account.trimmed();
    bool ok = false;
    const auto number = input.toULongLong(&ok);
    if (!ok || number == 0 || !QRegularExpression(QStringLiteral("^[0-9]+$")).match(input).hasMatch() ||
        QString::number(number) == m_account) {
        emit errorOccurred(QStringLiteral("请输入有效的对方账号，不能与当前账号相同"));
        return false;
    }
    m_peer = QString::number(number);
    if (!m_historyCursors.contains(m_peer)) {
        m_historyCursors.insert(m_peer, 0);
        requestHistory(m_peer, 0);
    }
    markCurrentRead();
    emit changed();
    return true;
}
void ChatService::clearConversation(const QString& peer, qint64 deletedSeq)
{
    if (m_account.isEmpty() || deletedSeq <= m_clearedSeq.value(peer))
        return;
    QString error;
    if (!m_storage.clearConversation(m_account, peer, deletedSeq, error)) {
        emit errorOccurred(error);
        return;
    }
    m_clearedSeq.insert(peer, deletedSeq);
    m_conversations.remove(conversation(peer));
    m_lastReadSent.remove(peer);
    m_historyCursors.remove(peer);
    for (auto it = m_messages.begin(); it != m_messages.end();) {
        if (it->peer == peer && (it->seq <= deletedSeq || it->seq == 0)) {
            m_pending.remove(it->id);
            it = m_messages.erase(it);
        } else {
            ++it;
        }
    }
    if (m_peer == peer && std::none_of(m_messages.cbegin(), m_messages.cend(),
                                      [&](const auto& message) { return message.peer == peer; }))
        m_peer.clear();
    emit changed();
}
void ChatService::setViewing(bool viewing)
{
    if (m_viewing == viewing)
        return;

    m_viewing = viewing;
    if (viewing) {
        markCurrentRead();
        emit changed();
    }
}
void ChatService::markCurrentRead()
{
    if (!m_viewing || m_peer.isEmpty() || m_account.isEmpty())
        return;
    QString error;
    if (!m_storage.markRead(m_account, conversation(m_peer), error)) {
        emit errorOccurred(error);
        return;
    }
    for (auto& m : m_messages)
        if (m.peer == m_peer)
            m.unread = false;
    qint64 highest = 0;
    const auto snapshot = m_conversations.constFind(conversation(m_peer));
    // Cached history can precede the server conversation snapshot or outlive it.
    // Only submit positions acknowledged by the authoritative conversation list.
    if (snapshot == m_conversations.cend())
        return;
    for (const auto& m : std::as_const(m_messages))
        if (m.peer == m_peer && m.status == 2 && m.seq <= snapshot->maxseq())
            highest = std::max(highest, m.seq);
    if (highest > std::max(m_lastReadSent.value(m_peer), static_cast<qint64>(snapshot->readseq()))) {
        conversation::MarkConversationReadReq request;
        request.set_conversationid(snapshot->conversationid());
        request.set_readseq(highest);
        if (m_session.sendRequest(241, QByteArray::fromStdString(request.SerializeAsString())))
            m_lastReadSent.insert(m_peer, highest);
    }
}
void ChatService::requestConversations()
{
    if (!m_account.isEmpty() && m_session.isOnline())
        m_session.sendRequest(240, QByteArray());
}
void ChatService::requestHistory(const QString& peer, qint64 afterSeq)
{
    if (m_account.isEmpty() || !m_session.isOnline()) {
        m_historyCursors.remove(peer);
        return;
    }
    msg::GetSingleChatHistoryReq request;
    request.set_peeruserid(peer.toULongLong());
    request.set_afterseq(afterSeq);
    request.set_limit(100);
    if (!m_session.sendRequest(244, QByteArray::fromStdString(request.SerializeAsString())))
        m_historyCursors.remove(peer);
}
bool ChatService::persist(const model::ChatMessage& message)
{
    QString error;
    if (!m_storage.save(m_account, message, error)) {
        emit errorOccurred(error);
        return false;
    }
    for (auto& existing : m_messages) {
        if (existing.id == message.id && existing.peer == message.peer && existing.outgoing == message.outgoing) {
            existing = message;
            return true;
        }
    }
    m_messages.append(message);
    return true;
}
bool ChatService::sendText(const QString& text)
{
    if (m_peer.isEmpty() || text.trimmed().isEmpty())
        return false;
    if (!m_session.isOnline()) {
        emit errorOccurred(QStringLiteral("连接已断开，请重新登录"));
        return false;
    }
    if (text.toUtf8().size() > 64 * 1024) {
        emit errorOccurred(QStringLiteral("消息内容过长"));
        return false;
    }
    model::ChatMessage m;
    m.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m.peer = m_peer;
    m.conversation = conversation(m_peer);
    m.text = text;
    m.outgoing = true;
    m.time = QDateTime::currentMSecsSinceEpoch();
    m.status = 1;
    if (!persist(m))
        return false;
    sdkws::SendMessageReq req;
    auto* msg = req.add_msgs();
    msg->set_sendid(m_account.toULongLong());
    msg->set_recvid(m_peer.toULongLong());
    msg->set_convid(m.conversation.toStdString());
    msg->set_clientmsgid(m.id.toStdString());
    msg->set_sessiontype(1);
    msg->set_contenttype(101);
    msg->set_msgfrom(100);
    msg->set_senderplatformid(3);
    msg->set_sendtime(m.time);
    msg->set_status(1);
    msg->set_content(QJsonDocument(QJsonObject{{"content", text}}).toJson(QJsonDocument::Compact).toStdString());
    m_pending.insert(m.id, {m.id, m.time + 15000});
    if (!m_session.sendRequest(101, QByteArray::fromStdString(req.SerializeAsString()), m.id)) {
        m_pending.remove(m.id);
        m.status = 3;
        persist(m);
        emit errorOccurred(QStringLiteral("消息发送失败"));
        emit changed();
        return false;
    }
    emit changed();
    return true;
}
void ChatService::failPending(const QString& reason)
{
    const auto pending = m_pending;
    m_pending.clear();
    for (const auto& p : pending)
        for (auto m : std::as_const(m_messages))
            if (m.id == p.id && m.outgoing) {
                m.status = 3;
                persist(m);
                break;
            }
    if (!pending.isEmpty() && !reason.isEmpty())
        emit errorOccurred(reason);
}
void ChatService::receive(const QByteArray& frame)
{
    if (m_account.isEmpty())
        return;
    sdkws::SdkWSResp resp;
    if (!resp.ParseFromArray(frame.constData(), frame.size()))
        return;
    const auto code = QString::fromStdString(resp.errorcode());
    if (resp.type() == 209) {
        if (code.isEmpty() || code == "0")
            m_status = QStringLiteral("已连接，离线消息同步完成");
        else
            m_status = QStringLiteral("离线消息同步失败");
        emit changed();
        m_conversationTimer.start();
        return;
    }
    if (resp.type() == 240) {
        if (!code.isEmpty() && code != "0") {
            emit errorOccurred(QString::fromStdString(resp.errormsg()));
            return;
        }
        conversation::GetAllConversationsResp list;
        if (!list.ParseFromString(resp.data())) {
            emit errorOccurred(QStringLiteral("会话列表响应无效"));
            return;
        }
        m_conversations.clear();
        for (const auto& item : list.conversations()) {
            const auto peer = QString::number(item.userid());
            if (item.conversationtype() == 1 && item.owneruserid() == m_account.toULongLong() &&
                (m_clearedSeq.value(peer) == 0 || item.maxseq() > m_clearedSeq.value(peer)))
                m_conversations.insert(QString::fromStdString(item.conversationid()), item);
        }
        markCurrentRead();
        emit changed();
        return;
    }
    if (resp.type() == 241) {
        if (!code.isEmpty() && code != "0") {
            m_lastReadSent.clear();
            emit errorOccurred(QString::fromStdString(resp.errormsg()));
        }
        m_conversationTimer.start();
        return;
    }
    if (resp.type() == 244) {
        if (!code.isEmpty() && code != "0") {
            m_historyCursors.clear();
            emit errorOccurred(QString::fromStdString(resp.errormsg()));
            return;
        }
        msg::GetSingleChatHistoryResp page;
        if (!page.ParseFromString(resp.data())) {
            m_historyCursors.clear();
            emit errorOccurred(QStringLiteral("聊天记录响应无效"));
            return;
        }
        const auto peer = QString::number(page.peeruserid());
        if (!m_historyCursors.contains(peer))
            return;
        const auto previous = m_historyCursors.value(peer);
        bool updated = false;
        for (const auto& source : page.messages()) {
            if (source.seq() <= m_clearedSeq.value(peer) ||
                (source.sendid() != m_account.toULongLong() && source.sendid() != page.peeruserid()) ||
                (source.recvid() != m_account.toULongLong() && source.recvid() != page.peeruserid()))
                continue;
            model::ChatMessage message;
            message.id = source.clientmsgid().empty() ? QString::fromStdString(source.servermsgid())
                                                     : QString::fromStdString(source.clientmsgid());
            message.serverId = QString::fromStdString(source.servermsgid());
            message.peer = peer;
            message.conversation = conversation(peer);
            message.outgoing = source.sendid() == m_account.toULongLong();
            message.time = static_cast<qint64>(source.sendtime());
            message.seq = source.seq();
            message.status = 2;
            message.payload = QString::fromLatin1(QByteArray::fromStdString(source.SerializeAsString()).toBase64());
            const auto raw = QByteArray::fromStdString(source.content());
            if (source.contenttype() == 101) {
                const auto doc = QJsonDocument::fromJson(raw);
                message.text = doc.isObject() && doc.object().value("content").isString()
                                   ? doc.object().value("content").toString() : QString::fromUtf8(raw);
            } else {
                message.text = QStringLiteral("[暂不支持的消息类型 %1]").arg(source.contenttype());
            }
            const auto snapshot = m_conversations.value(message.conversation);
            message.unread = !message.outgoing && message.seq > snapshot.readseq() &&
                             !(m_viewing && m_peer == peer);
            auto existing = std::find_if(m_messages.begin(), m_messages.end(), [&](const auto& item) {
                return (item.serverId == message.serverId && !message.serverId.isEmpty()) ||
                       (item.id == message.id && item.peer == peer && item.outgoing == message.outgoing);
            });
            if (existing != m_messages.end() && existing->serverId == message.serverId && existing->status == 2)
                continue;
            if (persist(message))
                updated = true;
        }
        if (page.hasmore() && page.nextseq() > previous) {
            m_historyCursors.insert(peer, page.nextseq());
            requestHistory(peer, page.nextseq());
        } else {
            m_historyCursors.remove(peer);
            if (m_viewing && m_peer == peer)
                markCurrentRead();
        }
        if (updated)
            emit changed();
        return;
    }
    if (resp.type() == 104 || resp.type() == 200) {
        if (!code.isEmpty() && code != "0")
            return;
        sdkws::MsgData msg;
        if (!msg.ParseFromString(resp.data()) || msg.servermsgid().empty()) {
            emit errorOccurred(QStringLiteral("收到无法解析的消息，未发送确认"));
            return;
        }
        if (msg.sessiontype() != 1 || msg.recvid() != m_account.toULongLong() || !msg.sendid())
            return;
        model::ChatMessage m;
        m.serverId = QString::fromStdString(msg.servermsgid());
        m.id = msg.clientmsgid().empty() ? m.serverId : QString::fromStdString(msg.clientmsgid());
        m.peer = QString::number(msg.sendid());
        m.conversation = conversation(m.peer);
        m.time = static_cast<qint64>(msg.sendtime());
        m.seq = msg.seq();
        m.status = 2;
        m.payload = QString::fromLatin1(QByteArray::fromStdString(msg.SerializeAsString()).toBase64());
        m.unread = !(m_viewing && m_peer == m.peer);
        if (msg.contenttype() == 101) {
            const auto raw = QByteArray::fromStdString(msg.content());
            const auto doc = QJsonDocument::fromJson(raw);
            m.text = doc.isObject() && doc.object().value("content").isString()
                         ? doc.object().value("content").toString()
                         : QString::fromUtf8(raw);
        } else {
            m.text = QStringLiteral("[暂不支持的消息类型 %1]").arg(msg.contenttype());
        }
        bool duplicate = m_clearedSeq.value(m.peer) > 0 && m.seq <= m_clearedSeq.value(m.peer);
        for (const auto& old : m_messages)
            if (old.serverId == m.serverId || (old.id == m.id && old.peer == m.peer && !old.outgoing)) {
                duplicate = true;
                break;
            }
        if (!duplicate && !persist(m))
            return;
        sdkws::AckReq ack;
        ack.set_userid(m_account.toULongLong());
        ack.add_servermsgids(msg.servermsgid());
        ack.set_acktype(resp.type() == 200 ? 2 : 1);
        if (!m_session.sendRequest(106, QByteArray::fromStdString(ack.SerializeAsString())))
            emit errorOccurred(QStringLiteral("消息已保存，但确认发送失败；重新登录后会去重处理"));
        emit changed();
        m_conversationTimer.start();
        return;
    }
    if (resp.type() != 101)
        return;
    const auto requestId = QString::fromStdString(resp.requestid());
    auto found = std::find_if(m_messages.begin(), m_messages.end(),
                              [&](const auto& m) { return m.outgoing && m.id == requestId; });
    if (found == m_messages.end())
        return;
    auto m = *found;
    sdkws::SendMessageResp result;
    QString error;
    if ((!code.isEmpty() && code != "0") || !result.ParseFromString(resp.data()) || result.infos_size() != 1) {
        error = QString::fromStdString(resp.errormsg());
        if (error.isEmpty())
            error = QStringLiteral("发送响应无效");
    } else {
        const auto& info = result.infos(0);
        if (info.errorcode() == "PENDING" && info.has_msg() &&
            info.msg().clientmsgid() == m.id.toStdString() && !info.msg().servermsgid().empty()) {
            if (m.status == 2)
                return;
            m.serverId = QString::fromStdString(info.msg().servermsgid());
            m.status = 1;
            persist(m);
            m_pending.insert(m.id, {m.id, QDateTime::currentMSecsSinceEpoch() + 60000});
            emit changed();
            return;
        }
        if (!info.errorcode().empty() && info.errorcode() != "0")
            error = QString::fromStdString(info.errormsg());
        if ((!info.errorcode().empty() && info.errorcode() != "0") && error.isEmpty())
            error = QStringLiteral("服务端拒绝发送");
        if (error.isEmpty() &&
            (!info.has_msg() || info.msg().servermsgid().empty() || info.msg().clientmsgid() != m.id.toStdString()))
            error = QStringLiteral("服务端发送回执缺少消息标识");
        if (error.isEmpty()) {
            m.serverId = QString::fromStdString(info.msg().servermsgid());
            m.seq = info.msg().seq();
            m.conversation = QString::fromStdString(info.msg().convid());
            m.time = static_cast<qint64>(info.msg().sendtime());
        }
    }
    if (m.status == 2 && !error.isEmpty())
        return;
    m_pending.remove(requestId);
    m.status = error.isEmpty() ? 2 : 3;
    persist(m);
    if (!error.isEmpty())
        emit errorOccurred(error);
    emit changed();
    m_conversationTimer.start();
}
}

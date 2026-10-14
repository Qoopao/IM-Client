#include "wcdbChatDao.h"
#include "../../../storage/wcdb/localDatabase.h"
#include "../../../storage/wcdb/chatMessageRow.h"
namespace dao {
bool WcdbChatDao::initialize(QString& error)
{
    if (!m_database.initialize(error))
        return false;
    if (m_initialized)
        return true;
    m_initialized = m_database.database().createTable<storage::ChatMessageRow>(storage::chatMessageTable);
    if (!m_initialized)
        error = QStringLiteral("无法初始化聊天记录数据库");
    return m_initialized;
}
bool WcdbChatDao::load(const QString& owner, QList<model::ChatMessage>& messages, QString& error)
{
    if (!initialize(error))
        return false;

    using Row = storage::ChatMessageRow;
    const auto rows = m_database.database().getAllObjects<Row>(
        storage::chatMessageTable, WCDB_FIELD(Row::owner) == owner.toStdString(),
        {WCDB_FIELD(Row::time).asOrder(WCDB::Order::ASC), WCDB_FIELD(Row::seq).asOrder(WCDB::Order::ASC)});

    if (!rows.hasValue()) {
        error = QStringLiteral("无法读取聊天记录");
        return false;
    }

    messages.clear();
    for (const auto& r : rows.value()) {
        model::ChatMessage m;
        m.id = QString::fromStdString(r.clientId);
        m.serverId = QString::fromStdString(r.serverId);
        m.conversation = QString::fromStdString(r.conversation);
        m.peer = QString::fromStdString(r.peer);
        m.text = QString::fromStdString(r.text);
        m.payload = QString::fromStdString(r.payload);
        m.time = r.time;
        m.seq = r.seq;
        m.status = r.status;
        m.outgoing = r.outgoing;
        m.unread = r.unread;
        messages.append(m);
    }
    return true;
}
bool WcdbChatDao::save(const QString& owner, const model::ChatMessage& m, QString& error)
{
    if (!initialize(error))
        return false;
    storage::ChatMessageRow r;
    r.key = (owner + ":" + m.peer + (m.outgoing ? ":out:" : ":in:") + m.id).toStdString();
    r.owner = owner.toStdString();
    r.clientId = m.id.toStdString();
    r.serverId = m.serverId.toStdString();
    r.conversation = m.conversation.toStdString();
    r.peer = m.peer.toStdString();
    r.text = m.text.toStdString();
    r.payload = m.payload.toStdString();
    r.time = m.time;
    r.seq = m.seq;
    r.status = m.status;
    r.outgoing = m.outgoing;
    r.unread = m.unread;
    if (!m_database.database().insertOrReplaceObject(r, storage::chatMessageTable)) {
        error = QStringLiteral("无法保存聊天记录，消息尚未确认");
        return false;
    }
    return true;
}
bool WcdbChatDao::clearConversation(const QString& owner, const QString& peer, qint64 deletedSeq, QString& error)
{
    if (!initialize(error))
        return false;
    using Row = storage::ChatMessageRow;
    if (!m_database.database().deleteObjects(storage::chatMessageTable,
        (WCDB_FIELD(Row::owner) == owner.toStdString()) && (WCDB_FIELD(Row::peer) == peer.toStdString()) &&
        ((WCDB_FIELD(Row::seq) <= deletedSeq) || (WCDB_FIELD(Row::seq) == 0)))) {
        error = QStringLiteral("无法清理已删除好友的聊天记录");
        return false;
    }
    return true;
}

bool WcdbChatDao::markRead(const QString& owner, const QString& conversation, QString& error)
{
    if (!initialize(error))
        return false;
    using Row = storage::ChatMessageRow;
    Row r;
    r.unread = 0;
    if (!m_database.database().updateObject(r, {WCDB_FIELD(Row::unread)}, storage::chatMessageTable,
                                            (WCDB_FIELD(Row::owner) == owner.toStdString()) &&
                                                (WCDB_FIELD(Row::conversation) == conversation.toStdString()))) {
        error = QStringLiteral("无法保存已读状态");
        return false;
    }
    return true;
}
}

#pragma once
#include "../../../model/chatMessage.h"
namespace storage {

class LocalDatabase;
}
namespace dao {

class WcdbChatDao
{
public:
    explicit WcdbChatDao(storage::LocalDatabase& database)
        : m_database(database)
    {
    }
    bool load(const QString& owner, QList<model::ChatMessage>& messages, QString& error);       // 加载本地聊天记录
    bool save(const QString& owner, const model::ChatMessage& message, QString& error);            // 保存聊天记录到本地
    bool markRead(const QString& owner, const QString& conversation, QString& error);               // 数据库标记消息已读

private:
    bool initialize(QString& error);
    storage::LocalDatabase& m_database;
    bool m_initialized = false;
};

}

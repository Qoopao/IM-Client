#pragma once
#include <QObject>
#include <QMap>
#include <QSet>
#include <QTimer>
#include "../model/chatMessage.h"
#include "../network/webSocketSession.h"
#include "../dao/local/wcdb/wcdbChatDao.h"
#include "../proto/conversation.pb.h"
namespace service {

class ChatService : public QObject
{
    Q_OBJECT
public:
    ChatService(network::WebSocketSession& session, dao::WcdbChatDao& storage, QObject* parent = nullptr);
    void start(const QString& account);     // 初始化当前账号，加载本地聊天记录，启动超时检查
    void stop();
    bool openPeer(const QString& account);
    bool sendText(const QString& text);
    void setViewing(bool viewing);
    void clearConversation(const QString& peer, qint64 deletedSeq);
    const QMap<QString, conversation::Conversation>& conversations() const
    {
        return m_conversations;
    }
    const QList<model::ChatMessage>& messages() const
    {
        return m_messages;
    }
    QString currentPeer() const
    {
        return m_peer;
    }
    QString account() const
    {
        return m_account;
    }
    QString status() const
    {
        return m_status;
    }
signals:
    void changed();
    void errorOccurred(const QString& error);

private:
    void receive(const QByteArray& frame);
    bool persist(const model::ChatMessage& message);
    void failPending(const QString& reason);
    void markCurrentRead();
    void requestConversations();
    void requestHistory(const QString& peer, qint64 afterSeq);
    QString conversation(const QString& peer) const;
    network::WebSocketSession& m_session;
    dao::WcdbChatDao& m_storage;
    QList<model::ChatMessage> m_messages;
    struct Pending
    {
        QString id;
        qint64 deadline;
    };
    QMap<QString, Pending> m_pending;
    QMap<QString, qint64> m_clearedSeq;
    QMap<QString, qint64> m_lastReadSent;
    QMap<QString, conversation::Conversation> m_conversations;
    QMap<QString, qint64> m_historyCursors;
    QTimer m_conversationTimer;
    QTimer m_timer;
    QString m_account, m_peer, m_status;
    bool m_viewing = false;
};

}

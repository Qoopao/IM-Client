#pragma once
#include <QObject>
#include <QMap>
#include <QTimer>
#include "../model/chatMessage.h"
#include "../network/webSocketSession.h"
#include "../dao/local/wcdb/wcdbChatDao.h"
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
    QTimer m_timer;
    QString m_account, m_peer, m_status;
    bool m_viewing = false;
};

}

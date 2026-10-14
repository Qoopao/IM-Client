#pragma once
#include <QObject>
#include <QMap>
#include <QTimer>
#include <QList>
#include "../network/webSocketSession.h"
#include "../dao/local/wcdb/wcdbGroupDao.h"
#include "../proto/groupchat.pb.h"
namespace service {
class GroupService : public QObject {
    Q_OBJECT
public:
    GroupService(network::WebSocketSession& session,dao::WcdbGroupDao& storage,QObject* parent=nullptr);
    void start(const QString& account);
    void stop();
    void refresh();
    void open(const QString& id);
    void setViewing(bool viewing);
    bool command(groupchat::Command cmd);
    bool sendText(const QString& text);
    QString account() const { return m_account; }
    QString current() const { return m_current; }
    const QMap<QString,groupchat::LocalState>& groups() const { return m_groups; }
    QList<groupchat::Message> messages(const QString& id) const;
    QList<groupchat::Command> pendingMessages(const QString& id) const;
    bool busy(groupchat::Operation operation,const QString& group={}) const;
signals:
    void changed();
    void result(int operation,const groupchat::Reply& reply);
    void errorOccurred(const QString& error);
    void operationFailed(int operation,const QString& group,const QString& error);
private:
    struct Pending { groupchat::Command command; qint64 deadline=0; bool durable=false; groupchat::Reply pages; };
    bool request(groupchat::Command cmd,bool durable=false);
    void receive(const QByteArray& frame);
    void sync(const QString& id);
    bool saveState(const QString& id);
    bool storeMessage(const groupchat::Message& message);
    bool advance(const QString& id);
    void acknowledge(const QString& id);
    void markRead();
    network::WebSocketSession& m_session;
    dao::WcdbGroupDao& m_storage;
    QMap<QString,groupchat::LocalState> m_groups;
    QMap<QString,QMap<quint64,groupchat::Message>> m_messages;
    QMap<QString,Pending> m_pending;
    QTimer m_timer;
    QString m_account,m_current;
    bool m_viewing=false;
    qint64 m_nextRefresh=0;
    bool m_refreshAgain=false;
};
}

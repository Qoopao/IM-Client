#ifndef REMOTE_FRIEND_DAO_H
#define REMOTE_FRIEND_DAO_H

#include <QObject>
#include <QMap>
#include <QTimer>
#include <functional>
#include "../../network/webSocketSession.h"
#include "../../proto/friend.pb.h"

namespace dao {

class RemoteFriendDao : public QObject
{
    Q_OBJECT

public:
    using Callback = std::function<void(const friendpb::Reply&)>;
    explicit RemoteFriendDao(network::WebSocketSession& connection, QObject* parent = nullptr);
    void execute(const friendpb::Command& command, Callback callback);
    void cancel();

signals:
    void refreshRequested();

private:
    struct Pending
    {
        int type = 0;
        qint64 deadline = 0;
        Callback callback;
    };
    void receive(const QByteArray& frame);
    void failAll(const QString& message);

    network::WebSocketSession& m_connection;
    QMap<QString, Pending> m_pending;
    QTimer m_timer;
};

}

#endif

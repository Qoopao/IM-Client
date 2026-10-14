#ifndef FRIEND_SERVICE_H
#define FRIEND_SERVICE_H

#include <QObject>
#include <QMap>
#include "../model/friendModel.h"
#include "../dao/remote/remoteFriendDao.h"
#include "../dao/local/wcdb/wcdbFriendDao.h"

namespace service {

class FriendService : public QObject
{
    Q_OBJECT

public:
    FriendService(dao::RemoteFriendDao& remote, dao::WcdbFriendDao& local, QObject* parent = nullptr);
    const model::FriendModel& state() const;
    void start(const QString& account);
    void stop();
    void refresh();
    QString nickname(const QString& account);
    void queryUser(const QString& account);
    void apply(const QString& message);
    void handleApplication(quint64 id, bool accept);
    void deleteFriend(const QString& account);

signals:
    void changed();
    void operationFinished(int operation);
    void errorOccurred(const QString& error);
    void conversationCleared(const QString& peer, qint64 deletedSeq);

private:
    void loadState(const friendpb::Reply& snapshot);
    void pullPage(friendpb::Operation operation, quint64 cursor = 0, quint64 snapshot = 0);
    bool successful(const friendpb::Reply& reply);
    void finishSync(bool success);

    dao::RemoteFriendDao& m_remote;
    dao::WcdbFriendDao& m_local;
    model::FriendModel m_state;
    friendpb::Reply m_snapshot;
    QMap<QString, QString> m_nicknames;
    QSet<QString> m_requestedNicknames;
    bool m_refreshAgain = false;
};

}

#endif

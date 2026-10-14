#ifndef FRIEND_CONTROLLER_H
#define FRIEND_CONTROLLER_H

#include "../service/friendService.h"

namespace controller {

class FriendController : public QObject
{
    Q_OBJECT

public:
    explicit FriendController(service::FriendService& service, QObject* parent = nullptr);
    const model::FriendModel& state() const;
    void refresh();
    QString nickname(const QString& account);
    void queryUser(const QString& account);
    void apply(const QString& message);
    void handleApplication(quint64 id, bool accept);
    void deleteFriend(const QString& account);

signals:
    void changed();
    void operationFinished(int operation);

private:
    service::FriendService& m_service;
};

}

#endif

#ifndef FRIEND_MODEL_H
#define FRIEND_MODEL_H

#include <QString>
#include <QList>
#include <QSet>

namespace model {

enum class FriendApplicationStatus { Pending, Accepted, Rejected };

struct FriendUser
{
    QString account;
    QString nickname;
    QString faceUrl;
};

struct FriendEntry
{
    FriendUser user;
    QString remark;
    qint64 createdTime = 0;
};

struct FriendApplication
{
    quint64 id = 0;
    FriendUser sender;
    FriendUser recipient;
    QString message;
    FriendApplicationStatus status = FriendApplicationStatus::Pending;
    qint64 createdTime = 0;
    qint64 handledTime = 0;
};

struct FriendModel
{
    QString account;
    QList<FriendEntry> friends;
    QList<FriendApplication> applications;
    FriendUser searchResult;
    bool hasSearchResult = false;
    bool querying = false;
    bool applying = false;
    bool syncing = false;
    QSet<quint64> handling;
    QSet<QString> deleting;
};

}

#endif

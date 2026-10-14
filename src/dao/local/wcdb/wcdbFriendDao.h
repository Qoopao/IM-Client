#ifndef WCDB_FRIEND_DAO_H
#define WCDB_FRIEND_DAO_H

#include <QString>
#include "../../../proto/friend.pb.h"

namespace storage {
class LocalDatabase;
}

namespace dao {

class WcdbFriendDao
{
public:
    explicit WcdbFriendDao(storage::LocalDatabase& database);
    bool load(const QString& owner, friendpb::Reply& snapshot, QString& error);
    bool save(const QString& owner, const friendpb::Reply& snapshot, QString& error);

private:
    bool initialize(QString& error);
    storage::LocalDatabase& m_database;
    bool m_initialized = false;
};

}

#endif

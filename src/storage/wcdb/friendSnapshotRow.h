#ifndef FRIEND_SNAPSHOT_ROW_H
#define FRIEND_SNAPSHOT_ROW_H

#include "wcdbHeaders.h"
#include <string>

namespace storage {

inline constexpr const char* friendSnapshotTable = "friend_snapshots";

struct FriendSnapshotRow
{
    std::string owner;
    std::string payload;
    WCDB_CPP_ORM_DECLARATION(FriendSnapshotRow)
};

}

#endif

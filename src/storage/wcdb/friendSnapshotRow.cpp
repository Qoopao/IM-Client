#include "friendSnapshotRow.h"

namespace storage {

WCDB_CPP_ORM_IMPLEMENTATION_BEGIN(FriendSnapshotRow)
WCDB_CPP_SYNTHESIZE(owner)
WCDB_CPP_SYNTHESIZE(payload)
WCDB_CPP_PRIMARY(owner)
WCDB_CPP_NOT_NULL(owner)
WCDB_CPP_ORM_IMPLEMENTATION_END

}

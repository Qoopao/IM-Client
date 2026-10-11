#include "accountHistoryRow.h"

namespace storage {

WCDB_CPP_ORM_IMPLEMENTATION_BEGIN(AccountHistoryRow)
WCDB_CPP_SYNTHESIZE(account)
WCDB_CPP_SYNTHESIZE(lastLoginTime)
WCDB_CPP_PRIMARY(account)
WCDB_CPP_NOT_NULL(account)
WCDB_CPP_NOT_NULL(lastLoginTime)
WCDB_CPP_ORM_IMPLEMENTATION_END
}

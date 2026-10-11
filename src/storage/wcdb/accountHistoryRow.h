#ifndef ACCOUNT_HISTORY_ROW_H
#define ACCOUNT_HISTORY_ROW_H

#include "wcdbHeaders.h"
#include <string>
#include <cstdint>

namespace storage {

inline constexpr const char* accountHistoryTable = "account_history";
struct AccountHistoryRow
{
    std::string account;
    int64_t lastLoginTime = 0;

    WCDB_CPP_ORM_DECLARATION(AccountHistoryRow)
};

}

#endif // ACCOUNT_HISTORY_ROW_H

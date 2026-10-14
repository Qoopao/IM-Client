#pragma once
#include "wcdbHeaders.h"
#include <string>
namespace storage {
inline constexpr const char* groupTable = "group_cache";
struct GroupRow {
    std::string key, owner, kind, payload;
    WCDB_CPP_ORM_DECLARATION(GroupRow)
};
}

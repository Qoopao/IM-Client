#pragma once
#include "wcdbHeaders.h"
#include <string>
#include <cstdint>
namespace storage {

inline constexpr const char* chatMessageTable = "chat_messages";
struct ChatMessageRow
{
    std::string key, owner, clientId, serverId, conversation, peer, text, payload;
    int64_t time = 0, seq = 0;
    int status = 0, outgoing = 0, unread = 0;
    WCDB_CPP_ORM_DECLARATION(ChatMessageRow)
};

}

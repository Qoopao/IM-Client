#pragma once
#include <QString>
#include <QList>

namespace model {

struct ChatMessage
{
    QString id, serverId, conversation, peer, text, payload;
    qint64 time = 0;
    qint64 seq = 0;
    int status = 0;                  // 0 接收中, 1 发送中，等待服务端回执, 2 自己发送的消息已被服务端接收，或对方消息已接收并保存, 3 发送失败，或因超时、断线等原因无法确认发送结果
    bool outgoing = false;
    bool unread = false;
};

}

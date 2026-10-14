# 单聊会话同步

服务端的 `ConversationService` 使用 MySQL `ape_auth.im_conversations` 保存每个用户自己的会话状态和最新消息摘要，SQL 操作统一位于 `MySQLHandler`。消息正文仍在 MongoDB `IM-System.msg`。

`im_conversations` 的联合主键为 `(owner_account, conversation_id)`。消息服务先在好友校验事务中持久化发送任务；后台保存 MongoDB 正文后，在重新校验好友关系的 MySQL 事务中更新双方会话并提交任务阶段。最新摘要仅随更大的 `maxSeq` 更新。列表直接读取摘要和未读计数，不再逐个会话查询 MongoDB。摘要正文限制为 512 个 UTF-8 字节，完整正文由消息服务读取。排序索引覆盖账号、隐藏状态、置顶、最新时间和会话 ID；分页由 SQL `LIMIT/OFFSET` 完成，查询单个会话直接命中联合主键。只读查询使用连接池 1，事务写入使用连接池 0。

`readSeq` 表示已读位置；`im_conversation_unread` 仅保存尚未读取的接收消息序号，联合主键 `(owner_account, conversation_id, seq)` 阻止重复计数。新消息通过事务增加 `unreadCount`，标记已读时锁定对应会话行、删除不超过读位置的未读条目，再扣除实际删除条数。新到达的消息保持未读，已读位置不会倒退；Snowflake 序号不连续，因此未读数不能通过两个序号相减得到。

删除会话入口使用隐藏状态，保留水位以阻止重复消息恢复入口；新的消息可使会话重新出现。删除好友则在好友关系事务中删除双方会话，外键级联清理未读序号，同时提交清理任务。后台按删除序号清理 MongoDB 正文和 Redis 缓存，失败后重试，具体见 [跨存储任务与恢复](transaction-outbox.md)。

客户端经 WebSocket 网关使用以下类型：

| 类型 | 请求/响应 | 用途 |
| --- | --- | --- |
| 240 | 空请求 / `GetAllConversationsResp` | 登录后拉取完整会话列表 |
| 241 | `MarkConversationReadReq` / 空响应 | 查看会话后提交已读位置 |
| 242 | `UpdateConversationSettingsReq` / 空响应 | 更新置顶或免打扰 |
| 243 | `DeleteConversationReq` / 空响应 | 删除当前用户的会话入口 |
| 244 | `GetSingleChatHistoryReq` / `GetSingleChatHistoryResp` | 打开会话时按序号分页读取消息正文 |

网关使用已认证的账号覆盖请求里的 `ownerUserID`。历史读取由 `MsgService` 处理并检查好友关系。好友删除会清除双方的消息正文和会话记录；服务端保留删除水位，避免旧消息在重新加好友后出现。

单聊 ACK 先提交 MySQL 投递回执和后台清理任务，再更新 MongoDB 接收状态并清理 Redis 实时正文缓存。不再使用 Redis 离线 ZSET 或 `last_seq` 水位；单聊按逐条持久化回执去重，避免较大序号的 ACK 跳过较小序号的未确认消息。投递回执与 `Conversation.readSeq` 分离；收到消息不等于用户已读。当前客户端打开单聊时从服务端补齐历史，消息正文仍写入本地 WCDB。群聊会话接口暂未接入这条同步流程。

## 旧数据迁移

新服务启动时创建 MySQL 会话表，不会自动导入旧 MongoDB 会话。已有会话需要在停止所有服务端写入后迁移；建议在首次启动新服务之前完成。

服务端脚本 `scripts/migrate_conversations.py` 需要 Python 依赖 `pymongo` 和 `mysql-connector-python`。通过 `APE_MONGO_URI` 设置 MongoDB 连接串，通过 `APE_MYSQL_PASSWORD` 设置 MySQL 密码；未设置 MySQL 密码时脚本会交互输入。默认 MySQL 账号为 `ape_user`，数据库为 `ape_auth`。

```sh
python3 scripts/migrate_conversations.py --server-stopped
```

脚本按每个用户的会话提交事务，批量导入未读序号并重算计数。已有 MySQL 会话不会被覆盖，可中断后再次执行；旧 MongoDB 文档保留。只有当前仍有双向好友关系的单聊会话会被导入，好友删除水位之前的消息会被排除。旧数据没有已读位置时使用 0，首次导入后需要打开会话提交已读位置。

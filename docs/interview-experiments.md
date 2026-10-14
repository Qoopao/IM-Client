# 项目理解与面试实验手册

建议按顺序做：先走通正常流程，再检查数据与协议，最后注入故障。每个实验留下操作记录、关键日志、数据变化和自己的解释。不要把本手册的预期当成已经验证过的结论：不一致的结果也要记录，可能是缺陷、旧数据或运行版本不同。

## 准备与记录方式

- 准备 A、B、C 三个测试账号，允许双客户端同时运行。记录实际完整账号，简称只用于笔记。
- 记录客户端和服务端版本、编译配置、依赖地址及启动方式。先在独立测试环境做删除、断网、停依赖等实验。
- 正常业务用客户端操作；重放请求、修改 actor、乱序、200/201 人切换等场景用测试程序或临时协议工具。UI 没有入口的功能不要当作已经完成的客户端功能。
- 客户端记录界面状态、Qt 日志、WCDB；服务端记录请求、业务日志、任务状态及存储变化。WSS 的业务内容需在应用层记录或断点查看，普通抓包只能直接观察连接/TLS 等信息。
- 日志只保留必要字段：账号、requestId、clientMsgID、serverMsgID、conversationID、seq、群内部 ID、群号、epoch、错误码。不要记录密码或完整 token，也不要把完整消息正文带到面试材料里。
- 故障注入前写清影响范围和恢复步骤；完成后恢复依赖并验证最终状态。

每次复制这个模板：

```text
实验编号 / 时间 / 客户端和服务端版本：
要验证的问题与预测：
初始状态：在线用户、关系、群成员、缓存、依赖状态
操作步骤与故障注入位置：
调用链：UI → Controller/Service → DAO/WS → 网关 → RPC → Handler
关联标识：requestId / clientMsgID / serverMsgID / conversationID / seq / epoch
关键日志及发生顺序：
各存储操作前后变化：
实际结果：成功 / 失败 / 未确认
与预测不同之处：
原因解释及代码位置：
恢复方法与恢复结果：
面试时用 90 秒怎么讲：
```

## 实验顺序

| 编号 | 实验 | 操作与观察 | 最后要能回答的问题 |
| --- | --- | --- | --- |
| 01 | 构建与协议 | 从 CMake 找源文件、AUTOMOC/AUTOUIC、Qt/vcpkg 依赖；对照两端 proto 和生成文件；查看 deploy 脚本 | `.ui` 如何变成 C++？为什么 protoc 与运行库要匹配？编译产物与部署目录有什么区别？ |
| 02 | 服务端启动和退出 | 正常启动 main，按日志记录连接池、建表、Backbon、各服务、网关、消费者及 worker；正常退出再启动 | 初始化有什么依赖顺序？哪些是线程/协程，哪些是独立进程？后台任务如何随服务关闭？ |
| 03 | 注册 | 正常注册，缺字段、重复手机号等按实际校验做负例；再研究注册集成与故障测试 | 账号如何分配？密码如何保存？重复请求与事务提交结果不确定如何处理？ |
| 04 | 登录与 TLS | 成功登录、错误密码、服务器不可达；在测试配置下验证不可信证书被拒绝 | TLS 建连与业务认证有什么区别？登录后谁保存 token？失败如何回到 UI？ |
| 05 | 身份和会话生命周期 | 用协议工具尝试未登录请求、篡改 actor/owner；关闭客户端后重开；同账号双开观察实际策略 | 真实用户身份取自哪里？连接、业务会话与本地数据库分别由谁管理？多端策略是否完整？ |
| 06 | 心跳与断线 | 空闲观察心跳；测试环境中断连接，等待状态变化，再重新登录 | 30 秒心跳如何判断失联？超时与主动断开有何区别？当前是否自动重连？ |
| 07 | 查找与好友申请 | A 查询 B，发申请，B 拒绝；再次申请并接受；重复提交和重复处理 | 好友申请状态机是什么？双向关系怎样原子建立？通知丢失为何还能恢复？ |
| 08 | 分页和缓存 | 检查好友/申请分页；用工具构造多页数据，在中途让一页失败，再恢复同步 | 为什么完整拉取后才替换快照？游标/快照边界解决什么问题？旧缓存何时保留？ |
| 09 | 在线单聊全链路 | A/B 在线，A 发唯一编号文本，逐阶段检查任务、正文、索引、消费者、客户端保存与 ACK | 发送成功、推送成功、ACK、已读是同一件事吗？每一阶段的责任是什么？ |
| 10 | 接收与已读分离 | B 登录但停留在其他会话，A 连发三条；B 打开会话，再切到其他程序；重启 B | 为什么 ACK 不清除未读？窗口可见与激活如何影响已读？为什么单聊序号差不能当未读数？ |
| 11 | 离线与历史 | B 退出，A 连发三条，B 登录；再重启 B 比较本地历史与服务端补齐；检查账号切换隔离 | 离线队列与历史查询有何区别？何时发 ACK？为何先落盘？缓存按什么隔离？ |
| 12 | 请求与投递重复 | 工具重放同一个 clientMsgID；再用相同 ID 改正文；重复投递同一 serverMsgID | 幂等键是什么？客户端和服务端各在哪里去重？requestId 与消息 ID 有什么区别？ |
| 13 | 发送故障恢复 | 分别让 MongoDB、Redis、Kafka 暂时不可用，在不同阶段发送；恢复依赖并等待重试 | 跨存储为何不能靠普通事务回滚？Outbox 保证什么、不保证什么？PENDING 如何恢复？ |
| 14 | 接收故障恢复 | 测试工具丢弃 ACK；使客户端保存失败；在消费者处理完成但 offset 提交前模拟退出 | 重复来自哪些窗口？ACK 失败怎样处理？为什么最大 ACK 序号不能跳过所有较小消息？ |
| 15 | 删除好友与重新添加 | A/B 有历史及待处理消息时删除好友，立即尝试发送/拉历史，再重新添加并发新消息 | 逻辑删除与物理清理有何区别？删除水位如何防止旧任务删掉新消息？ |
| 16 | 会话列表与旧缓存 | 比较聊天列表摘要与正文；检查 maxSeq/readSeq/未读条目；用测试验证缺失会话、旧缓存及已读重开 | 为什么会话摘要放 MySQL？服务端序号边界为何能避免已读错误？旧 Mongo 会话是否自动迁移？ |
| 17 | 建群与审核 | A 建群；B 按 10 位群号搜索申请；A 拒绝/同意；C 直接请求发送；检查独立群资料窗口 | 群号与内部 ID 为何分开？群主权限在哪校验？未入群为何不能发消息？ |
| 18 | 群历史与成员代次 | A 先发 M1，B 加入后发 M2；B 退出，A 发 M3，B 重新申请后发 M4；重放 B 旧 epoch 请求 | joinSeq 与 epoch 各解决什么？保留本地旧历史与禁止拉入群前历史如何同时成立？ |
| 19 | 群顺序与 ACK | A/B/C 并发发送；工具模拟重复和乱序正文，观察补齐、连续 cursor、ACK/readSeq | 群连续序号与单聊 Snowflake 有何区别？收到 seq=5 为什么不一定能 ACK 到 5？ |
| 20 | 大小群切换 | 用集成测试创建 200/201 人群，发消息，再减少人数；观察正文推送与提示拉取 | 大群减少的是正文扇出还是所有通知？切换时为什么无需重置成员进度？ |
| 21 | 群管理与竞态 | 同名/空白/超长改名，确认与取消；群主尝试退出；踢人和发送并发；解散后发送/拉取 | UI 校验为什么还要配服务端校验？群行锁保护什么？权限改变与在途请求怎么交互？ |
| 22 | 注册发现与 RPC | 查看 etcd 注册/租约、Backbon 调用、gRPC 客户端选择；测试环境中使发现服务不可用 | 服务发现与请求转发是什么关系？同进程为什么还使用 RPC？注册不等于可用，如何判断？ |
| 23 | 并发、性能与背压 | 小规模压测起步：同一好友对与多对用户分别发送；观察延迟、吞吐、线程、池等待和 Kafka 积压 | 账号对锁、群行锁和连接池分别限制什么？协程为何不等于无限并发？瓶颈在哪？ |
| 24 | 可观测性与设计复盘 | 找一条成功请求和一条失败请求，串联 trace/log/任务记录；画架构图和时序图 | 能否用证据解释一次失败？哪些能力只是协议声明，哪些已经实际实现？有哪些部署边界？ |

## 几个关键实验的具体做法

### 09：一条在线单聊消息

1. A/B 已是好友且在线。用带实验编号的文本发送，记录发送者状态变化。
2. 从客户端 `ChatService::sendText` 跟到 WS 请求，找到网关处理分支与 `MsgService::SendMessages`。
3. 检查 MySQL 发送任务：消息标识、state、stage、dispatch_done。沿 OutboxWorker 找到 MongoDB 写入和双方会话更新的位置。
4. 检查 Mongo 正文及 committed 状态、Redis 缓存/离线索引、Kafka 消费及 PushService 调用。用实际顺序判断哪些操作位于同一 MySQL 事务中。
5. B 收到后检查 WCDB 保存、ACK 请求、服务端回执与后台清理任务。
6. B 打开消息后再检查会话 readSeq 与未读条目。画出“接受请求 → 服务端保存 → 分发 → 客户端保存 → ACK → 已读”的时间线。

快速读取任务状态，注意不要直接 SELECT 出正文载荷：

```sql
SELECT id, operation, state, stage, dispatch_done, attempts,
       available_at, lease_until, last_error
FROM im_outbox ORDER BY id DESC LIMIT 30;

SELECT owner_account, conversation_id, peer_account,
       max_seq, read_seq, unread_count
FROM im_conversations
WHERE owner_account IN (<A账号>, <B账号>);

SELECT owner_account, conversation_id, seq
FROM im_conversation_unread
WHERE owner_account = <B账号> ORDER BY seq;
```

尖括号是占位符，替换成实际数字账号后才能执行。任务完成不等于客户端已读，dispatch_done 也不等于客户端已 ACK。

### 13：跨存储故障

每次只故障一个依赖，按“正常基线 → 注入 → 发送 → 观察 → 恢复 → 最终核对”执行。

- Mongo 不可用：观察正文阶段是否停留、错误与退避、客户端失败/待确认的实际结果。
- Redis 或 Kafka 不可用：检查正文和会话是否已经提交，任务为何仍未完成；恢复后检查重复执行是否增加正文或未读计数。
- 在外部操作成功、MySQL 任务状态尚未提交时退出进程：需要断点或故障注入工具精确控制；重新启动，观察租约和重试。粗暴停进程不保证命中这个窗口。
- 发请求后让响应丢失：保持原 clientMsgID 重试，核对是否返回同一条消息。

必须区分“当前阶段无法推进”和“整个请求已被取消”。当前实现是至少一次处理与幂等收敛，不是跨库原子事务或端到端恰好一次。

### 15：删除水位与异步清理

1. 留下已确认的旧消息，并在测试环境制造一个待处理任务。
2. 删除好友，核对双向关系、会话、删除水位和清理任务。
3. 暂不恢复故障依赖，验证发送和历史读取已被阻止：这验证逻辑删除不依赖物理清理先完成。
4. 重新添加好友并发送新消息，再恢复依赖。
5. 检查旧正文清理完成、新消息保留、两端本地旧消息按清理记录同步删除。不要仅凭列表消失判断存储已经清空。

### 18：群成员代次

记录 B 每次加入时的 epoch、joinSeq、ackSeq、readSeq 和本地 cursor。

- M1：B 初次入群前发送，B 不应从服务器拉到。
- M2：B 在群内收到，落盘后 ACK；退出后本地记录应保留。
- M3：B 退出期间发送，重新加入后不应补拉。
- M4：B 重新加入后发送，应正常收到。
- 用旧 epoch 拉取、ACK、已读，检查服务端拒绝及错误码。

判断历史是否越权时，用新客户端缓存或检查实际服务端响应；不能把保留的本地旧消息误认为服务端重新下发。

### 20：200/201 人边界

先阅读服务端 `test/group_integration.cpp`，再在临时数据库运行 `scripts/run_group_tests.sh`；它会构建并启动测试数据库，不是纯单元测试。现有集成测试覆盖 200/201 切换、分页、并发顺序、踢人与发送竞态等。

结合断点或日志观察 GroupDeliveryBatch 的选择：小群 notice 可携带正文，大群 notice 提示最新序号后客户端 Pull。人数降低后再次发送，检查同一群序号和成员 cursor 连续。测试通过证明特定场景的正确性，不代表已做过生产规模压测。

### 23：性能实验

按低负载开始逐步增加并发，每次只改变一个变量。记录发送端确认延迟、接收端落盘延迟的 p50/p95/p99、错误率、单位时间成功消息数及队列积压。至少比较：同一好友对、多好友对、同一个群、小群与大群、在线与离线。

源码中的 stress_test 需要先检查认证、协议与当前实现是否一致，再作为压力发生器使用。不要直接把旧测试的成功率当作新业务的结果。记录机器配置、依赖配置和数据量，才能解释数字。

## 推荐的源码阅读入口

| 流程 | 客户端入口 | 服务端入口（相对 ape-server 根目录） |
| --- | --- | --- |
| 生命周期 | `main.cpp`、`src/application/applicationController.cpp`、`clientSession.cpp` | `src/main.cpp` |
| 登录/连接 | `authService.cpp`、`remoteAuthDao.cpp`、`network/webSocketSession.cpp` | `src/gateway/ws_session.cpp`、`src/services/auth_service/server.cpp` |
| 注册 | `src/ui/reg.cpp`，沿调用继续定位 | `auth_service/server.cpp`、`src/storage/mysqlhandler.cpp`、注册集成测试 |
| 好友 | `friendController.cpp`、`friendService.cpp`、`remoteFriendDao.cpp` | `friend_service/server.cpp`、`mysqlhandler.cpp` |
| 单聊 | `chatService.cpp`、`wcdbChatDao.cpp` | `msg_service/server.cpp`、`msg_service/outbox_worker.cpp`、`mysqlhandler.cpp`、`mongohandler.cpp`、`redishandler.cpp` |
| 分发 | `webSocketSession.cpp` 的响应入口 | `src/messagequeue/kafkaconsumer.cpp`、`push_service`、`gateway_push_service`、`ws_session_manager.cpp` |
| 会话/已读 | `chatService.cpp` | `conversation_service/server.cpp`、`mysqlhandler.cpp` |
| 群聊 | `groupService.cpp`、`groupPanel.cpp/.ui`、`mainIM.cpp/.ui` | `group_service/server.cpp`、`mysqlhandler.cpp` |
| 存储/线程 | `storage/wcdb` 与各本地 DAO | `src/storage/*connector.cpp`、`*handler.cpp`、`src/util/ioc_pool.*` |
| 注册发现/观测 | requestId 与 Qt 日志 | `backbon_service`、各 Service 基类、`src/om` |

客户端表位于 WCDB：`chat_messages`、`friend_snapshots`、`group_cache`；通常在 `%LOCALAPPDATA%\IM-client\IM-client\storage\client.db`。服务端单聊正文位于 MongoDB `IM-System.msg`，关系、会话、Outbox、回执、群消息及进度位于 MySQL。Redis 键和 Kafka topic 以实际运行代码为准，沿 handler/producer 找到后记入自己的数据地图。

## 现有测试怎样辅助理解

- 客户端 `chat-read-test`：服务端缺失会话、旧缓存超界、重开会话、可见性与重启已读。
- 客户端 `group-cache-test`：账号隔离、持久化恢复、独立群窗口、名称校验、确认/取消、群主退出限制。
- 服务端 `group_integration`：群号补齐、RPC 链路、审核与权限、历史边界、epoch、大小群、并发与解散。
- 服务端 `offline_login_test`：Redis EVAL 命令构造、旧 BSON 数值读取、离线收件人校验。
- 服务端注册集成/故障测试：先阅读对应 fixture、代理及启动条件，再运行，重点关注提交不确定与重试。

测试是控制特定时序的工具；UI 手动实验是验证实际部署链路的工具，两者都需要。

## 完成后应该产出的面试材料

1. 一张架构图：客户端分层、WSS 网关、gRPC 服务、消息队列、各存储与观测组件，标明当前部署进程边界。
2. 五张时序图：登录、在线单聊、离线恢复、好友删除、群聊发送/入群。
3. 一张数据地图：每张关键表/集合/Redis 键保存什么，唯一键、权威数据和可重建派生数据在哪里。
4. 三份故障报告：现象、定位证据、根因、恢复、重复执行窗口与改进方案。
5. 一份性能报告：测试条件、延迟分位数、吞吐、错误率、瓶颈证据。
6. 一页真实边界：客户端未提供的协议入口、同进程服务、多网关通知限制、重连/多端现状、数据保留策略和大历史加载问题。

面试回答按“具体场景 → 实际调用链 → 数据与事务边界 → 失败窗口 → 实验结果 → 取舍”组织。不要只背类名，也不要承诺实验没有证明的可靠性或性能。

配套设计说明：[好友](friend-feature.md)、[单聊会话](conversation-service.md)、[Outbox 与恢复](transaction-outbox.md)、[群聊](group-feature.md)。

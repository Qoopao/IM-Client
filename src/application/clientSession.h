#ifndef CLIENT_SESSION_H
#define CLIENT_SESSION_H

#include "../network/webSocketSession.h"
#include "../dao/remote/remoteAuthDao.h"
#include "../dao/local/wcdb/wcdbChatDao.h"
#include "../service/authService.h"
#include "../service/chatService.h"
#include "../controller/authController.h"

namespace application {

class ClientSession
{
public:
    explicit ClientSession(storage::LocalDatabase& database);
    ~ClientSession();

    controller::AuthController& authController();       // 向上
    service::ChatService& chatService();
    void activate(const QString& account);
    void shutdown();

private:
    network::WebSocketSession m_connection;         // 管理socket连接状态

    dao::RemoteAuthDao m_authDao;
    service::AuthService m_authService;
    controller::AuthController m_authController;

    dao::WcdbChatDao m_chatDao;
    service::ChatService m_chatService;
};

}

#endif // CLIENT_SESSION_H

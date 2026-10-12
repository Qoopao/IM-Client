#include "clientSession.h"

namespace application {

ClientSession::ClientSession(storage::LocalDatabase& database)
    : m_authDao(m_connection)
    , m_authService(m_authDao)
    , m_authController(m_authService)
    , m_chatDao(database)
    , m_chatService(m_connection, m_chatDao)
{
}

ClientSession::~ClientSession()
{
    shutdown();
}

controller::AuthController& ClientSession::authController()
{
    return m_authController;
}

service::ChatService& ClientSession::chatService()
{
    return m_chatService;
}

void ClientSession::activate(const QString& account)
{
    m_chatService.start(account);
}

void ClientSession::shutdown()
{
    m_chatService.stop();
    m_authController.logout();
    m_connection.close();
}

}

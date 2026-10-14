#include "clientSession.h"

namespace application {

ClientSession::ClientSession(storage::LocalDatabase& database)
    : m_authDao(m_connection)
    , m_authService(m_authDao)
    , m_authController(m_authService)
    , m_chatDao(database)
    , m_chatService(m_connection, m_chatDao)
    , m_groupDao(database)
    , m_groupService(m_connection, m_groupDao)
    , m_friendRemote(m_connection)
    , m_friendLocal(database)
    , m_friendService(m_friendRemote, m_friendLocal)
    , m_friendController(m_friendService)
{
    QObject::connect(&m_friendService, &service::FriendService::conversationCleared,
            &m_chatService, &service::ChatService::clearConversation);
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

controller::FriendController& ClientSession::friendController()
{
    return m_friendController;
}

void ClientSession::activate(const QString& account)
{
    m_chatService.start(account);
    m_groupService.start(account);
    m_friendService.start(account);
}

void ClientSession::shutdown()
{
    m_friendService.stop();
    m_groupService.stop();
    m_chatService.stop();
    m_authController.logout();
    m_connection.close();
}

}

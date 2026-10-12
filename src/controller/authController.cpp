#include "authController.h"

namespace controller {

AuthController::AuthController(service::AuthService& service, QObject* parent)
    : QObject(parent)
    , m_service(service)
{
    connect(&m_service, &service::AuthService::stateChanged, this, &AuthController::stateChanged);
}

void AuthController::login(const QString& account, const QString& password)
{
    m_service.login(account, password);
}

void AuthController::logout()
{
    m_service.logout();
}

}

#include "authController.h"

namespace controller {

AuthController::AuthController(QObject* parent) : QObject(parent)
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

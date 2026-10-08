#include "regController.h"

namespace controller {

RegController::RegController(QObject* parent) : QObject(parent)
{
    connect(&m_service, &service::RegService::stateChanged, this, &RegController::stateChanged);
}

bool RegController::validPhone(const QString& value) const { return service::RegService::validPhone(value); }

bool RegController::validNickname(const QString& value) const { return service::RegService::validNickname(value); }

bool RegController::validPassword(const QString& value) const { return service::RegService::validPassword(value); }

void RegController::registerAccount(const QString& phone, const QString& nickname, const QString& password)
{
    m_service.registerAccount(phone, nickname, password);
}

void RegController::cancel() { m_service.cancel(); }

}

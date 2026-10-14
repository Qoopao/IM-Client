#include "friendController.h"

namespace controller {

FriendController::FriendController(service::FriendService& service, QObject* parent)
    : QObject(parent)
    , m_service(service)
{
    connect(&service, &service::FriendService::changed, this, &FriendController::changed);
    connect(&service, &service::FriendService::operationFinished, this, &FriendController::operationFinished);
}

const model::FriendModel& FriendController::state() const
{
    return m_service.state();
}

void FriendController::refresh()
{
    m_service.refresh();
}

QString FriendController::nickname(const QString& account)
{
    return m_service.nickname(account);
}

void FriendController::queryUser(const QString& account)
{
    m_service.queryUser(account);
}

void FriendController::apply(const QString& message)
{
    m_service.apply(message);
}

void FriendController::handleApplication(quint64 id, bool accept)
{
    m_service.handleApplication(id, accept);
}

void FriendController::deleteFriend(const QString& account)
{
    m_service.deleteFriend(account);
}

}

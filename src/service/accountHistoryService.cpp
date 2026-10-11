#include "accountHistoryService.h"
#include <QDateTime>

namespace service {

bool AccountHistoryService::loadAccounts(QList<model::AccountHistory>& accounts, QString& error)
{
    return m_dao.loadAccounts(accounts, error);
}

bool AccountHistoryService::recordSuccessfulLogin(const QString& account, QString& error)
{
    return m_dao.saveSuccessfulLogin({account, QDateTime::currentMSecsSinceEpoch()}, error);
}

bool AccountHistoryService::removeAccount(const QString& account, QString& error)
{
    return m_dao.removeAccount(account, error);
}

}
